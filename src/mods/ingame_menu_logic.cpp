// UPSTREAM: OpenRA.Mods.Common/Widgets/Logic/Ingame/IngameMenuLogic.cs
//          @e180e3225 L23-647 全文(实现部分)。
#include "mods/ingame_menu_logic.hpp"

#include "fluent/fluent_provider.hpp"
#include "game/actor_info.hpp"
#include "game/mod_data.hpp"
#include "gfx/world_renderer.hpp"
#include "meta/generic_record.hpp"
#include "sim/trait_registry.hpp"
#include "mods/confirmation_dialogs.hpp"
#include "mods/mission_objectives.hpp"
#include "net/order.hpp"
#include "net/session.hpp"
#include "net/text_notifications.hpp"
#include "sim/player.hpp"
#include "sim/world.hpp"
#include "ui/ui.hpp"
#include "ui/widgets/button_widget.hpp"

namespace ora::mods {

namespace {

std::string Fluent(std::string_view str_key) {
  return fluent::FluentProvider::Instance().GetMessage(str_key);
}

const meta::RecordObject* FindTraitInfoNamed(const sim::World& world,
                                             const char* str_actor,
                                             std::string_view str_info_name) {
  const game::ActorInfo* ptr_actor =
      const_cast<sim::World&>(world).Map().Rules().FindActor(str_actor);
  if (ptr_actor == nullptr)
    return nullptr;
  for (const meta::RecordObject* rec : ptr_actor->TraitsInConstructOrder())
    if (rec->record_desc().str_name == str_info_name)
      return rec;
  return nullptr;
}

std::optional<int2> ParseInt2(std::string_view str_value) {
  const std::size_t pos_comma = str_value.find(',');
  if (pos_comma == std::string_view::npos)
    return std::nullopt;
  int2 out{};
  const auto fn_int = [](std::string_view sv_part, int& out_value) {
    std::string str_part{sv_part};
    const std::size_t pos_begin = str_part.find_first_not_of(" \t");
    const std::size_t pos_end = str_part.find_last_not_of(" \t");
    if (pos_begin == std::string::npos)
      return false;
    str_part = str_part.substr(pos_begin, pos_end - pos_begin + 1);
    int int4_value{};
    if (std::from_chars(str_part.data(), str_part.data() + str_part.size(),
                        int4_value)
            .ec != std::errc{})
      return false;
    out_value = int4_value;
    return true;
  };
  if (!fn_int(str_value.substr(0, pos_comma), out.X))
    return std::nullopt;
  if (!fn_int(str_value.substr(pos_comma + 1), out.Y))
    return std::nullopt;
  return out;
}

std::vector<std::string> ParseStringArray(std::string_view str_value) {
  std::vector<std::string> vec_items;
  std::size_t pos_start = 0;
  while (pos_start <= str_value.size()) {
    const std::size_t pos_comma = str_value.find(',', pos_start);
    const std::string_view str_part =
        str_value.substr(pos_start, pos_comma == std::string_view::npos
                                        ? std::string_view::npos
                                        : pos_comma - pos_start);
    const std::size_t pos_begin = str_part.find_first_not_of(" \t");
    const std::size_t pos_end = str_part.find_last_not_of(" \t");
    if (pos_begin != std::string_view::npos)
      vec_items.emplace_back(
          str_part.substr(pos_begin, pos_end - pos_begin + 1));
    if (pos_comma == std::string_view::npos)
      break;
    pos_start = pos_comma + 1;
  }
  return vec_items;
}

bool IsLoadPanelEnabled(const game::ModData& mod_data) {
  const std::string str_base_save_path =
      std::string{"Saves"} + "/" + mod_data.ManifestRef().Id() + "/" +
      mod_data.ManifestRef().Metadata().str_version;
  std::error_code ec{};
  if (!std::filesystem::exists(str_base_save_path, ec))
    return false;

  for (const auto& entry :
       std::filesystem::directory_iterator{str_base_save_path, ec})
    if (entry.is_regular_file() && entry.path().extension() == ".orasav")
      return true;
  return false;
}

}  // namespace

bool IngameMenuLogic::b_last_game_editor_ = false;

IngameMenuLogic::IngameMenuLogic(ora::ui::Widget& widget,
                                 game::ModData& mod_data, sim::World& world,
                                 std::function<void()> fn_on_exit,
                                 gfx::WorldRenderer* ptr_world_renderer,
                                 IngameInfoPanel initial_panel,
                                 const YamlDictionary* ptr_logic_args)
    : ptr_mod_data_{&mod_data},
      ptr_world_{&world},
      ptr_world_renderer_{ptr_world_renderer},
      fn_on_exit_{std::move(fn_on_exit)} {
  const std::vector<std::pair<std::string,
                              void (IngameMenuLogic::*)()>> vec_button_handlers{
      {"ABORT_MISSION", &IngameMenuLogic::CreateAbortMissionButton},
      {"BACK_TO_EDITOR", &IngameMenuLogic::CreateBackToEditorButton},
      {"RESTART", &IngameMenuLogic::CreateRestartButton},
      {"SURRENDER", &IngameMenuLogic::CreateSurrenderButton},
      {"LOAD_GAME", &IngameMenuLogic::CreateLoadGameButton},
      {"SAVE_GAME", &IngameMenuLogic::CreateSaveGameButton},
      {"MUSIC", &IngameMenuLogic::CreateMusicButton},
      {"SETTINGS", &IngameMenuLogic::CreateSettingsButton},
      {"RESUME", &IngameMenuLogic::CreateResumeButton},
      {"SAVE_MAP", &IngameMenuLogic::CreateSaveMapButton},
      {"PLAY_MAP", &IngameMenuLogic::CreatePlayMapButton},
      {"EXIT_EDITOR", &IngameMenuLogic::CreateExitEditorButton},
  };

  b_is_single_player_ = !world.LobbyInfo().global_settings.Dedicated &&
                        world.LobbyInfo().NonBotClients().size() == 1;

  ptr_menu_ = widget.Get("INGAME_MENU");
  ptr_button_container_ = ptr_menu_->Get("MENU_BUTTONS");
  ora::ui::Widget* ptr_button_template =
      ptr_button_container_->Get<ora::ui::ButtonWidget>("BUTTON_TEMPLATE");
  up_button_template_ = ptr_button_container_->RemoveChild(*ptr_button_template);
  ptr_button_container_->fn_is_visible = [this] { return !b_hide_menu_; };

  if (ptr_logic_args != nullptr)
    for (const auto& [str_key, yaml_value] : *ptr_logic_args) {
      if (str_key == "ButtonStride" && yaml_value != nullptr &&
          yaml_value->Value != nullptr) {
        if (const auto int2_stride = ParseInt2(*yaml_value->Value))
          int2_button_stride_ = *int2_stride;
      }
    }

  b_has_error_ = false;

  if (ptr_logic_args != nullptr)
    for (const auto& [str_key, yaml_value] : *ptr_logic_args) {
      if (str_key != "Buttons" || yaml_value == nullptr)
        continue;
      const std::vector<std::string> vec_button_ids =
          yaml_value->Value != nullptr
              ? ParseStringArray(*yaml_value->Value)
              : std::vector<std::string>{};
      for (const std::string& str_button : vec_button_ids)
        for (const auto& [str_id, fn_create] : vec_button_handlers)
          if (str_button == str_id)
            (this->*fn_create)();
    }

  if (!vec_buttons_.empty()) {
    const int2 int2_expand{(static_cast<int>(vec_buttons_.size()) - 1) *
                                int2_button_stride_.X,
                            (static_cast<int>(vec_buttons_.size()) - 1) *
                                int2_button_stride_.Y};
    ptr_button_container_->bounds_widget.int4_x -= int2_expand.X / 2;
    ptr_button_container_->bounds_widget.int4_y -= int2_expand.Y / 2;
    ptr_button_container_->bounds_widget.int4_width += int2_expand.X;
    ptr_button_container_->bounds_widget.int4_height += int2_expand.Y;
  }

  ora::ui::Widget* ptr_panel_root = widget.GetOrNull("PANEL_ROOT");
  if (ptr_panel_root != nullptr && world.Type() != sim::WorldType::Editor) {
    ora::ui::WidgetArgs args_panel;
    args_panel = args_panel.With(
        "initialPanel", static_cast<std::int32_t>(initial_panel));
    args_panel = args_panel.With(
        "hideMenu",
        std::function<void(bool)>{[this](bool b_hide) {
          b_hide_menu_ = b_hide;
        }});
    args_panel = args_panel.With(
        "closeMenu",
        std::function<void()>{[this] { CloseMenu(); }});
    args_panel = args_panel.With("world", &world);
    if (ptr_world_renderer != nullptr)
      args_panel = args_panel.With("worldRenderer", ptr_world_renderer);

    ora::ui::Widget* ptr_game_info_panel = ora::ui::Ui::Instance().LoadWidget(
        "GAME_INFO_PANEL", ptr_panel_root, std::move(args_panel));
    ptr_game_info_panel->fn_is_visible = [this] { return !b_hide_menu_; };
  }

  ora::ui::Ui::Instance().SetWidgetsVisible(true);
  ora::ui::Ui::Instance().SetHideCursor(false);
}

void IngameMenuLogic::OnQuit(sim::World& world) {
  if (world.Type() == sim::WorldType::Regular) {
    if (const meta::RecordObject* rec_moi = FindTraitInfoNamed(
            world, "player", "MissionObjectivesInfo")) {
      std::string str_leave_notification;
      std::string str_leave_text_notification;
      if (const auto v = sim::RecordFieldString(*rec_moi, "LeaveNotification"))
        str_leave_notification = std::string{*v};
      if (const auto v =
              sim::RecordFieldString(*rec_moi, "LeaveTextNotification"))
        str_leave_text_notification = std::string{*v};

      ora::ui::Ui::Instance().PlayNotification(
          &world.Map().Rules(), str_leave_notification);
      net::TextNotificationsManager::Instance().AddTransientLine(
          nullptr, str_leave_text_notification);
    }
  }

  int int4_exit_delay = 0;

  if (!ora::ui::Ui::Instance().IsCurrentWorldDeps(&world)) {
    ora::ui::Ui::Instance().DisconnectDeps();
    ora::ui::Ui::Instance().ResetAll();
    ora::ui::Ui::Instance().LoadShellMapDeps();
    return;
  }

  b_last_game_editor_ = false;
  ora::ui::Ui::Instance().RunAfterDelayDefer(
      int4_exit_delay, [ptr_world = &world] {
        if (!ora::ui::Ui::Instance().IsCurrentWorldDeps(ptr_world))
          return;

        ora::ui::Ui::Instance().DisconnectDeps();
        ora::ui::Ui::Instance().ResetAll();
        ora::ui::Ui::Instance().LoadShellMapDeps();
      });
}

void IngameMenuLogic::ShowMenu() {
  b_hide_menu_ = false;
}

void IngameMenuLogic::CloseMenu() {
  ora::ui::Ui::Instance().CloseWindow();
  if (fn_on_exit_)
    fn_on_exit_();
  ora::ui::Ui::Instance().ResetTooltips();
}

ora::ui::ButtonWidget* IngameMenuLogic::AddButton(const std::string& str_id,
                                                  const std::string& str_label) {
  std::unique_ptr<ora::ui::Widget> up_button = up_button_template_->Clone();
  auto* ptr_button =
      dynamic_cast<ora::ui::ButtonWidget*>(up_button.get());
  if (!vec_buttons_.empty()) {
    ptr_button->bounds_widget.int4_x =
        vec_buttons_.back()->bounds_widget.int4_x + int2_button_stride_.X;
    ptr_button->bounds_widget.int4_y =
        vec_buttons_.back()->bounds_widget.int4_y + int2_button_stride_.Y;
  }

  ptr_button->str_id = str_id;
  ptr_button->fn_is_disabled = [this] { return b_leaving_; };
  ptr_button->fn_get_text = [str_label] { return str_label; };
  ptr_button_container_->AddChild(std::move(up_button));
  vec_buttons_.push_back(ptr_button);

  return ptr_button;
}

void IngameMenuLogic::CreateAbortMissionButton() {
  if (ptr_world_->Type() != sim::WorldType::Regular)
    return;

  auto* ptr_button = AddButton(
      "ABORT_MISSION", Fluent(ptr_world_->IsGameOver()
                                  ? std::string_view{"menu-ingame.leave"}
                                  : std::string_view{"menu-ingame.abort"}));

  ptr_button->fn_on_click = [this] {
    b_hide_menu_ = true;

    ButtonPrompt(
        "dialog-leave-mission.title", "dialog-leave-mission.prompt",
        [this] {
          OnQuit(*ptr_world_);
          b_leaving_ = true;
        },
        "dialog-leave-mission.confirm",
        [this] { ShowMenu(); }, "dialog-leave-mission.cancel");
  };
}

void IngameMenuLogic::CreateRestartButton() {
  if (ptr_world_->Type() != sim::WorldType::Regular || !b_is_single_player_)
    return;

  int int4_exit_delay = 0;

  auto fn_on_restart = std::make_shared<std::function<void()>>();
  *fn_on_restart = [this, int4_exit_delay]() mutable {
    ora::ui::Ui::Instance().CloseWindow();
    ora::ui::Ui::Instance().RunAfterDelayDefer(int4_exit_delay, [] {
      ora::ui::Ui::Instance().RestartGameDeps();
    });
  };

  auto* ptr_button = AddButton("RESTART", Fluent("menu-ingame.restart"));
  ptr_button->fn_is_disabled = [this] { return b_leaving_; };
  ptr_button->fn_on_click = [this, fn_on_restart] {
    b_hide_menu_ = true;
    ButtonPrompt("dialog-restart-mission.title",
                 "dialog-restart-mission.prompt", *fn_on_restart,
                 "dialog-restart-mission.confirm",
                 [this] { ShowMenu(); }, "dialog-restart-mission.cancel");
  };
}

void IngameMenuLogic::CreateSurrenderButton() {
  if (ptr_world_->Type() != sim::WorldType::Regular || b_is_single_player_ ||
      ptr_world_->LocalPlayer() == nullptr)
    return;

  auto fn_on_surrender = std::make_shared<std::function<void()>>();
  *fn_on_surrender = [this] {
    ptr_world_->IssueOrder(
        new net::Order{"Surrender",
                       ptr_world_->LocalPlayer()->PlayerActor(), false});
    CloseMenu();
  };

  auto* ptr_button = AddButton("SURRENDER", Fluent("menu-ingame.surrender"));
  ptr_button->fn_is_disabled = [this] {
    return ptr_world_->LocalPlayer() == nullptr ||
           ptr_world_->LocalPlayer()->PlayerWinState() !=
               sim::WinState::Undefined ||
           b_has_error_ || b_leaving_;
  };
  ptr_button->fn_on_click = [this, fn_on_surrender] {
    b_hide_menu_ = true;
    ButtonPrompt("dialog-surrender.title", "dialog-surrender.prompt",
                 *fn_on_surrender, "dialog-surrender.confirm",
                 [this] { ShowMenu(); }, "dialog-surrender.cancel");
  };
}

void IngameMenuLogic::CreateLoadGameButton() {
  if (ptr_world_->Type() != sim::WorldType::Regular ||
      !ptr_world_->LobbyInfo().global_settings.EnableGameSaves ||
      ptr_world_->IsReplay())
    return;

  auto* ptr_button = AddButton("LOAD_GAME", Fluent("menu-ingame.load-game"));
  ptr_button->fn_is_disabled = [this] {
    return b_leaving_ || !IsLoadPanelEnabled(*ptr_mod_data_);
  };
  ptr_button->fn_on_click = [this] {
    b_hide_menu_ = true;
    ora::ui::WidgetArgs args_panel;
    args_panel = args_panel.With(
        "onExit", std::function<void()>{[this] { b_hide_menu_ = false; }});
    args_panel = args_panel.With(
        "onStart", std::function<void()>{[this] { CloseMenu(); }});
    ora::ui::Ui::Instance().OpenWindow("LOAD_GAME_BROWSER_PANEL",
                                       std::move(args_panel));
  };
}

void IngameMenuLogic::CreateSaveGameButton() {
  if (ptr_world_->Type() != sim::WorldType::Regular ||
      !ptr_world_->LobbyInfo().global_settings.EnableGameSaves ||
      ptr_world_->IsReplay())
    return;

  auto* ptr_button = AddButton("SAVE_GAME", Fluent("menu-ingame.save-game"));
  ptr_button->fn_is_disabled = [this] {
    if (b_has_error_ || b_leaving_)
      return true;
    for (const sim::Player* player : ptr_world_->Players())
      if (player->Playable() &&
          player->PlayerWinState() == sim::WinState::Undefined)
        return false;
    return true;
  };
  ptr_button->fn_on_click = [this] {
    b_hide_menu_ = true;
    ora::ui::WidgetArgs args_panel;
    args_panel = args_panel.With(
        "onExit", std::function<void()>{[this] { b_hide_menu_ = false; }});
    args_panel = args_panel.With("onStart", std::function<void()>{[] {}});
    args_panel = args_panel.With("world", ptr_world_);
    ora::ui::Ui::Instance().OpenWindow("GAMESAVE_BROWSER_PANEL",
                                       std::move(args_panel));
  };
}

void IngameMenuLogic::CreateMusicButton() {
  auto* ptr_button = AddButton("MUSIC", Fluent("menu-ingame.music"));
  ptr_button->fn_on_click = [this] {
    b_hide_menu_ = true;
    ora::ui::WidgetArgs args_panel;
    args_panel = args_panel.With(
        "onExit", std::function<void()>{[this] { b_hide_menu_ = false; }});
    args_panel = args_panel.With("world", ptr_world_);
    ora::ui::Ui::Instance().OpenWindow("MUSIC_PANEL", std::move(args_panel));
  };
}

void IngameMenuLogic::CreateSettingsButton() {
  auto* ptr_button = AddButton("SETTINGS", Fluent("menu-ingame.settings"));
  ptr_button->fn_on_click = [this] {
    b_hide_menu_ = true;
    ora::ui::WidgetArgs args_panel;
    args_panel = args_panel.With("world", ptr_world_);
    if (ptr_world_renderer_ != nullptr)
      args_panel = args_panel.With("worldRenderer", ptr_world_renderer_);
    args_panel = args_panel.With(
        "onExit", std::function<void()>{[this] { b_hide_menu_ = false; }});
    ora::ui::Ui::Instance().OpenWindow("SETTINGS_PANEL",
                                       std::move(args_panel));
  };
}

void IngameMenuLogic::CreateResumeButton() {
  auto* ptr_button = AddButton(
      "RESUME", Fluent(ptr_world_->IsGameOver()
                           ? std::string_view{"menu-ingame.return-to-map"}
                           : std::string_view{"menu-ingame.resume"}));
  ptr_button->key_hotkey = ora::ui::Ui::Instance().Hotkey("escape");
  ptr_button->fn_on_click = [this] { CloseMenu(); };
}

void IngameMenuLogic::CreateSaveMapButton() {
  if (ptr_world_->Type() != sim::WorldType::Editor)
    return;
}

void IngameMenuLogic::CreatePlayMapButton() {
  if (ptr_world_->Type() != sim::WorldType::Editor)
    return;
}

void IngameMenuLogic::CreateBackToEditorButton() {
  if (ptr_world_->Type() != sim::WorldType::Regular || !b_last_game_editor_)
    return;
}

void IngameMenuLogic::CreateExitEditorButton() {
  if (ptr_world_->Type() != sim::WorldType::Editor)
    return;
}

void IngameMenuLogic::ExitEditor(bool b_has_unsaved_items,
                                 std::function<void()> fn_on_success) {
  if (b_has_unsaved_items) {
    b_hide_menu_ = true;
    ButtonPrompt("dialog-exit-map-editor.title",
                 "dialog-exit-map-editor.prompt-unsaved",
                 [this, fn_on_success] {
                   fn_on_success();
                   b_leaving_ = true;
                 },
                 "dialog-exit-map-editor.confirm",
                 [this] { ShowMenu(); });
  } else {
    fn_on_success();
    b_leaving_ = true;
  }
}

void RegisterIngameMenuLogic() {
  ora::ui::ChromeLogicRegistry::Instance().Register(
      "IngameMenuLogic",
      [](ora::ui::WidgetArgs& args_args)
          -> std::unique_ptr<ora::ui::ChromeLogic> {
        const YamlDictionary* ptr_logic_args = nullptr;
        if (const ora::ui::WidgetArgValue* ptr_value =
                args_args.TryGet("logicArgs"))
          if (const auto* ptr_dict =
                  std::get_if<const YamlDictionary*>(ptr_value))
            ptr_logic_args = *ptr_dict;
        std::function<void()> fn_on_exit;
        if (const ora::ui::WidgetArgValue* ptr_value =
                args_args.TryGet("onExit"))
          if (const auto* ptr_fn = std::get_if<std::function<void()>>(ptr_value))
            fn_on_exit = *ptr_fn;
        IngameInfoPanel initial_panel = IngameInfoPanel::AutoSelect;
        if (const ora::ui::WidgetArgValue* ptr_value =
                args_args.TryGet("initialPanel"))
          if (const auto* ptr_int = std::get_if<std::int32_t>(ptr_value))
            initial_panel = static_cast<IngameInfoPanel>(*ptr_int);
        gfx::WorldRenderer* ptr_world_renderer = nullptr;
        if (const ora::ui::WidgetArgValue* ptr_value =
                args_args.TryGet("worldRenderer"))
          if (const auto* ptr_wr =
                  std::get_if<gfx::WorldRenderer*>(ptr_value))
            ptr_world_renderer = *ptr_wr;
        return std::make_unique<IngameMenuLogic>(
            *std::get<ora::ui::Widget*>(args_args.At("widget")),
            *std::get<game::ModData*>(args_args.At("modData")),
            *std::get<sim::World*>(args_args.At("world")),
            std::move(fn_on_exit), ptr_world_renderer, initial_panel,
            ptr_logic_args);
      });
}

}  // namespace ora::mods
