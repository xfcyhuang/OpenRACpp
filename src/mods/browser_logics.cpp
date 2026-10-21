// UPSTREAM: OpenRA.Mods.Common/Widgets/Logic/ReplayBrowserLogic.cs +
//          ReplayUtils.cs + GameSaveUtils.cs(see browser_logics.hpp)。
#include "mods/browser_logics.hpp"

#include "fluent/fluent_provider.hpp"
#include "game/mod_data.hpp"
#include "game/web_services.hpp"
#include "map/map_cache.hpp"
#include "mods/confirmation_dialogs.hpp"
#include "net/game_save.hpp"
#include "net/replay_recorder.hpp"
#include "server/game_information.hpp"
#include "sim/player.hpp"
#include "ui/ui.hpp"
#include "ui/widget_utils.hpp"
#include "ui/widgets/drop_down_button_widget.hpp"
#include "ui/widgets/image_widget.hpp"
#include "ui/widgets/label_widget.hpp"
#include "ui/widgets/label_with_tooltip_widget.hpp"
#include "ui/widgets/scroll_item_widget.hpp"
#include "ui/widgets/scroll_panel_widget.hpp"

namespace ora::mods {
namespace {

fluent::FluentProvider& Fluent() {
  return fluent::FluentProvider::Instance();
}

std::string Msg(std::string_view str_key) {
  return Fluent().GetMessage(str_key);
}

std::string Msg1(std::string_view str_key, std::string_view str_arg_name,
                 fluent::FluentValue val_arg) {
  return Fluent().GetMessage(
      str_key, std::vector<std::pair<std::string, fluent::FluentValue>>{
                   {std::string{str_arg_name}, std::move(val_arg)}});
}

std::string Msg2(std::string_view str_key, std::string_view str_arg_a,
                 const fluent::FluentValue& val_a,
                 std::string_view str_arg_b,
                 const fluent::FluentValue& val_b) {
  return Fluent().GetMessage(
      str_key, std::vector<std::pair<std::string, fluent::FluentValue>>{
                   {std::string{str_arg_a}, val_a},
                   {std::string{str_arg_b}, val_b}});
}

char FoldChar(char c) {
  return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c;
}

bool EqualsIgnoreCase(std::string_view a, std::string_view b) {
  if (a.size() != b.size())
    return false;
  for (std::size_t i = 0; i < a.size(); ++i)
    if (FoldChar(a[i]) != FoldChar(b[i]))
      return false;
  return true;
}

bool LessIgnoreCase(std::string_view a, std::string_view b) {
  const std::size_t n = std::min(a.size(), b.size());
  for (std::size_t i = 0; i < n; ++i) {
    const char ca = FoldChar(a[i]);
    const char cb = FoldChar(b[i]);
    if (ca != cb)
      return ca < cb;
  }
  return a.size() < b.size();
}

FluentArgs MakeArgs(
    std::initializer_list<std::pair<std::string_view, std::string>> vec_pairs) {
  FluentArgs vec_out;
  for (const auto& [str_key, str_value] : vec_pairs)
    vec_out.emplace_back(
        std::string{str_key},
        fluent::FluentValue{std::string{str_value}});
  return vec_out;
}

std::string ButtonText(const ora::ui::ButtonWidget& button) {
  if (button.fn_get_text)
    return button.fn_get_text();
  return button.str_text;
}

std::string BaseDirectory(const char* str_kind,
                          const game::ModData& mod_data) {
  return std::string{str_kind} + "/" + mod_data.ManifestRef().Id() + "/" +
         mod_data.ManifestRef().Metadata().str_version;
}

std::vector<std::string> FilesByExtension(const std::string& str_dir,
                                          std::string_view str_extension) {
  std::vector<std::string> vec_files;
  std::error_code ec{};
  for (auto it = std::filesystem::recursive_directory_iterator{str_dir, ec};
       it != std::filesystem::recursive_directory_iterator{};) {
    if (ec != std::errc{})
      break;
    if (it->is_regular_file(ec) &&
        it->path().extension().generic_string().ends_with(str_extension))
      vec_files.push_back(it->path().generic_string());
    it.increment(ec);
  }
  return vec_files;
}

std::string FilenameWithoutExtension(const std::string& str_path) {
  return std::filesystem::path{str_path}.stem().generic_string();
}

}  // namespace

map::MapPreview& GameInfoMapPreview(const server::GameInformation& info,
                                     game::ModData& /*mod_data*/) {
  map::MapCache* ptr_cache = ora::ui::Ui::Instance().MapCacheDeps();
  if (ptr_cache == nullptr)
    return map::MapCache::UnknownMap();
  return ptr_cache->At(info.MapUid);
}

std::string GameInfoResolvedPlayerName(
    const server::GameInformation& info,
    const server::GameInformationPlayer& player) {
  if (player.IsBot) {
    std::int64_t n_number = 1;
    for (const auto& p : info.vec_players) {
      if (p.BotType != player.BotType)
        continue;
      if (&p == &player)
        break;
      ++n_number;
    }
    return Msg2("enumerated-bot-name", "name",
                fluent::FluentValue{Msg(player.Name)}, "number",
                fluent::FluentValue{n_number});
  }
  return player.Name;
}

bool PromptConfirmReplayCompatibility(const net::ReplayMetadata& meta,
                                      game::ModData& mod_data,
                                      std::function<void()> fn_on_cancel) {
  const auto incompatible =
      [&](const std::string& str_text, const FluentArgs& vec_args = {}) {
        ButtonPrompt("dialog-incompatible-replay.title", str_text, nullptr,
                     "dialog-incompatible-replay.confirm",
                     std::move(fn_on_cancel),
                     "dialog-incompatible-replay.confirm", nullptr, "",
                     vec_args);
        return false;
      };

  const std::optional<server::GameInformation> info =
      server::GameInformation::Deserialize(meta.GameInfoData(),
                                           meta.FilePath());
  if (!info.has_value())
    return incompatible("dialog-incompatible-replay.prompt");
  if (info->Version.empty())
    return incompatible("dialog-incompatible-replay.prompt-unknown-version");
  if (info->Mod.empty())
    return incompatible("dialog-incompatible-replay.prompt-unknown-mod");

  // Game.Mods 已安装模子表 = 当前模子(ExternalMods 面随其批;COVERAGE)
  if (!EqualsIgnoreCase(info->Mod, mod_data.ManifestRef().Id()))
    return incompatible("dialog-incompatible-replay.prompt-unavailable-mod",
                        MakeArgs({{"mod", info->Mod}}));
  if (info->Version != mod_data.ManifestRef().Metadata().str_version)
    return incompatible(
        "dialog-incompatible-replay.prompt-incompatible-version",
        MakeArgs({{"version", info->Version}}));

  if (GameInfoMapPreview(*info, mod_data).Status() != map::MapStatus::Available)
    return incompatible("dialog-incompatible-replay.prompt-unavailable-map",
                        MakeArgs({{"map", info->MapUid}}));

  return true;
}

std::optional<double> GetGameDurationSeconds(const net::GameSave* save) {
  if (save == nullptr || !save->opt_global_settings.has_value() ||
      save->opt_global_settings->GameTimestep <= 0 ||
      save->LastOrdersFrame < 0)
    return std::nullopt;
  return static_cast<double>(
             static_cast<std::int64_t>(save->LastOrdersFrame) *
             save->opt_global_settings->GameTimestep) /
         1000.0;
}

std::string FormatGameDuration(std::optional<double> fp8_seconds) {
  if (!fp8_seconds.has_value())
    return "?";
  const std::int64_t t8_ms = static_cast<std::int64_t>(*fp8_seconds * 1000.0);
  return std::format("{:02}:{:02}:{:02}", t8_ms / 3600000,
                     (t8_ms / 60000) % 60, (t8_ms / 1000) % 60);
}

bool IsValidNewSaveName(const std::string& str_new_name,
                        const std::string& str_initial_name,
                        const std::string& str_directory) {
  if (str_new_name == str_initial_name)
    return false;

  bool b_has_content = false;
  for (const char c : str_new_name)
    if (c != ' ' && c != '\t' && c != '\r' && c != '\n') {
      b_has_content = true;
      break;
    }
  if (!b_has_content)
    return false;

  const auto is_invalid = [](char c) {
    return c == '<' || c == '>' || c == ':' || c == '"' || c == '/' ||
           c == '\\' || c == '|' || c == '?' || c == '*' || c == '\0' ||
           (static_cast<unsigned char>(c) < 0x20);
  };
  for (const char c : str_new_name)
    if (is_invalid(c))
      return false;

  std::error_code ec{};
  if (std::filesystem::exists(std::filesystem::path{str_directory} /
                                  str_new_name,
                              ec))
    return false;

  return true;
}

// ———— ReplayBrowserLogic ————

ReplayBrowserLogic::Filter ReplayBrowserLogic::filter_{};

ReplayBrowserLogic::ReplayBrowserLogic(
    ora::ui::Widget& widget, game::ModData& mod_data,
    std::function<void()> fn_on_exit, std::function<void()> fn_on_start)
    : fn_on_start_{std::move(fn_on_start)}, ptr_mod_data_{&mod_data} {
  ptr_map_ = &map::MapCache::UnknownMap();
  ptr_panel_ = &widget;
  ptr_alive_ = std::make_shared<bool>(true);

  // MAP_PREVIEW 子树装载(MapPreviewLogic)随大厅批;COVERAGE 登记

  fn_unsubscribe_game_start_ = ora::ui::Ui::Instance().SubscribeBeforeGameStart(
      [this] {
        ora::ui::Ui::Instance().CloseWindow();
        fn_on_start_();
      });

  ptr_player_list_ = widget.Get<ora::ui::ScrollPanelWidget>("PLAYER_LIST");
  up_player_header_ = ptr_player_list_->RemoveChild(
      *ptr_player_list_->Get<ora::ui::ScrollItemWidget>("HEADER"));
  up_player_template_ = ptr_player_list_->RemoveChild(
      *ptr_player_list_->Get<ora::ui::ScrollItemWidget>("TEMPLATE"));
  ptr_player_list_->RemoveChildren();

  widget.Get<ora::ui::ButtonWidget>("CANCEL_BUTTON")->fn_on_click =
      [this, fn_on_exit = std::move(fn_on_exit)]() mutable {
        b_cancel_loading_ = true;
        ora::ui::Ui::Instance().CloseWindow();
        fn_on_exit();
      };

  ptr_replay_list_ = widget.Get<ora::ui::ScrollPanelWidget>("REPLAY_LIST");
  up_replay_template_ = ptr_replay_list_->RemoveChild(
      *ptr_replay_list_->Get<ora::ui::ScrollItemWidget>("REPLAY_TEMPLATE"));

  const std::string str_dir = BaseDirectory("Replays", mod_data);
  std::error_code ec{};
  if (std::filesystem::exists(str_dir, ec))
    LoadReplaysAsync(str_dir);

  auto* ptr_watch = widget.Get<ora::ui::ButtonWidget>("WATCH_BUTTON");
  ptr_watch->fn_is_disabled = [this] {
    return sz_selected_ == static_cast<std::size_t>(-1) ||
           ptr_map_->Status() != map::MapStatus::Available;
  };
  ptr_watch->fn_on_click = [this] { WatchReplay(); };

  widget.Get("MAP_PREVIEW_ROOT")->fn_is_visible = [this] {
    return sz_selected_ != static_cast<std::size_t>(-1);
  };
  widget.Get("REPLAY_INFO")->fn_is_visible = [this] {
    return sz_selected_ != static_cast<std::size_t>(-1);
  };

  auto transform_duration = std::make_shared<
      core::CachedTransform<std::string, std::string>>(
      [](const std::string&) { return std::string{}; });
  widget.Get<ora::ui::LabelWidget>("DURATION")->fn_get_text =
      [this, transform_duration]() {
        if (sz_selected_ == static_cast<std::size_t>(-1) ||
            !vec_replays_[sz_selected_].info_game.has_value())
          return transform_duration->Update({});
        const auto& info = *vec_replays_[sz_selected_].info_game;
        return transform_duration->Update(Msg1(
            "label-duration", "time",
            fluent::FluentValue{static_cast<std::int64_t>(
                info.DurationSeconds())}));
      };

  SetupFilters();
  SetupManagement();
}

ReplayBrowserLogic::~ReplayBrowserLogic() {
  *ptr_alive_ = false;
  if (fn_unsubscribe_game_start_) {
    fn_unsubscribe_game_start_();
    fn_unsubscribe_game_start_ = nullptr;
  }
}

void ReplayBrowserLogic::LoadReplaysAsync(const std::string& str_dir) {
  std::thread([this, str_dir]() {
    struct Loaded {
      std::unique_ptr<net::ReplayMetadata> up_metadata;
      std::optional<server::GameInformation> info;
    };
    std::vector<Loaded> vec_loaded;
    for (const std::string& str_file : FilesByExtension(str_dir, ".orarep")) {
      if (b_cancel_loading_.load(std::memory_order_relaxed))
        return;
      auto up_meta = net::ReplayMetadata::ReadFile(str_file);
      if (up_meta == nullptr)
        continue;
      const std::string str_payload = up_meta->GameInfoData();
      const std::string str_path = up_meta->FilePath();
      vec_loaded.push_back(
          Loaded{std::move(up_meta),
                 server::GameInformation::Deserialize(str_payload,
                                                      str_path)});
    }

    if (b_cancel_loading_.load(std::memory_order_relaxed))
      return;

    std::stable_sort(
        vec_loaded.begin(), vec_loaded.end(),
        [](const Loaded& a, const Loaded& b) {
          const std::int64_t t_a =
              a.info.has_value() ? a.info->StartTimeUtc.ToUnixSeconds() : 0;
          const std::int64_t t_b =
              b.info.has_value() ? b.info->StartTimeUtc.ToUnixSeconds() : 0;
          return t_a > t_b;
        });

    const auto ptr_loaded =
        std::make_shared<std::vector<Loaded>>(std::move(vec_loaded));
    const std::weak_ptr<bool> weak_alive = ptr_alive_;
    ora::ui::Ui::Instance().RunAfterTickDefer([this, ptr_loaded,
                                               weak_alive]() {
      const auto ptr_alive = weak_alive.lock();
      if (ptr_alive == nullptr || !*ptr_alive)
        return;
      ptr_replay_list_->RemoveChildren();
      vec_files_for_test_.clear();
      for (auto& loaded : *ptr_loaded) {
        vec_files_for_test_.push_back(loaded.up_metadata->FilePath());
        AddReplay(std::move(loaded.up_metadata), std::move(loaded.info));
      }
      SetupReplayDependentFilters();
      ApplyFilter();
    });
  }).detach();
}

void ReplayBrowserLogic::SetupFilters() {
  {
    auto* ptr_ddb =
        ptr_panel_->GetOrNull<ora::ui::DropDownButtonWidget>(
            "FLT_GAMETYPE_DROPDOWNBUTTON");
    if (ptr_ddb != nullptr) {
      const std::vector<std::pair<GameType, std::string>> vec_options{
          {GameType::Any, ButtonText(*ptr_ddb)},
          {GameType::Singleplayer, Msg("options-replay-type.singleplayer")},
          {GameType::Multiplayer, Msg("options-replay-type.multiplayer")}};
      ptr_ddb->fn_get_text = [vec_options] {
        for (const auto& [kind, str_text] : vec_options)
          if (kind == filter_.kind_type)
            return str_text;
        return std::string{};
      };
      ptr_ddb->fn_on_mouse_down =
          [ptr_ddb, vec_options](const MouseInput&) {
            ptr_ddb->ShowDropDown<std::pair<GameType, std::string>>(
                "LABEL_DROPDOWN_TEMPLATE", 330, vec_options,
                [](const std::pair<GameType, std::string>& option,
                   ora::ui::ScrollItemWidget& tpl) {
                  auto up_item = ora::ui::ScrollItemWidget::Setup(
                      tpl, [kind = option.first] {
                        return filter_.kind_type == kind;
                      },
                      [kind = option.first] {
                        filter_.kind_type = kind;
                      });
                  up_item->Get<ora::ui::LabelWidget>("LABEL")->fn_get_text =
                      [str_text = option.second] { return str_text; };
                  return up_item;
                });
          };
    }
  }

  {
    auto* ptr_ddb =
        ptr_panel_->GetOrNull<ora::ui::DropDownButtonWidget>(
            "FLT_DATE_DROPDOWNBUTTON");
    if (ptr_ddb != nullptr) {
      const std::vector<std::pair<DateType, std::string>> vec_options{
          {DateType::Any, ButtonText(*ptr_ddb)},
          {DateType::Today, Msg("options-replay-date.today")},
          {DateType::LastWeek, Msg("options-replay-date.last-week")},
          {DateType::LastFortnight, Msg("options-replay-date.last-fortnight")},
          {DateType::LastMonth, Msg("options-replay-date.last-month")}};
      ptr_ddb->fn_get_text = [vec_options] {
        for (const auto& [kind, str_text] : vec_options)
          if (kind == filter_.kind_date)
            return str_text;
        return std::string{};
      };
      ptr_ddb->fn_on_mouse_down =
          [ptr_ddb, vec_options](const MouseInput&) {
            ptr_ddb->ShowDropDown<std::pair<DateType, std::string>>(
                "LABEL_DROPDOWN_TEMPLATE", 330, vec_options,
                [](const std::pair<DateType, std::string>& option,
                   ora::ui::ScrollItemWidget& tpl) {
                  auto up_item = ora::ui::ScrollItemWidget::Setup(
                      tpl, [kind = option.first] {
                        return filter_.kind_date == kind;
                      },
                      [kind = option.first] { filter_.kind_date = kind; });
                  up_item->Get<ora::ui::LabelWidget>("LABEL")->fn_get_text =
                      [str_text = option.second] { return str_text; };
                  return up_item;
                });
          };
    }
  }

  {
    auto* ptr_ddb =
        ptr_panel_->GetOrNull<ora::ui::DropDownButtonWidget>(
            "FLT_DURATION_DROPDOWNBUTTON");
    if (ptr_ddb != nullptr) {
      const std::vector<std::pair<DurationType, std::string>> vec_options{
          {DurationType::Any, ButtonText(*ptr_ddb)},
          {DurationType::VeryShort,
           Msg("options-replay-duration.very-short")},
          {DurationType::Short, Msg("options-replay-duration.short")},
          {DurationType::Medium, Msg("options-replay-duration.medium")},
          {DurationType::Long, Msg("options-replay-duration.long")}};
      ptr_ddb->fn_get_text = [vec_options] {
        for (const auto& [kind, str_text] : vec_options)
          if (kind == filter_.kind_duration)
            return str_text;
        return std::string{};
      };
      ptr_ddb->fn_on_mouse_down =
          [ptr_ddb, vec_options](const MouseInput&) {
            ptr_ddb->ShowDropDown<std::pair<DurationType, std::string>>(
                "LABEL_DROPDOWN_TEMPLATE", 330, vec_options,
                [](const std::pair<DurationType, std::string>& option,
                   ora::ui::ScrollItemWidget& tpl) {
                  auto up_item = ora::ui::ScrollItemWidget::Setup(
                      tpl, [kind = option.first] {
                        return filter_.kind_duration == kind;
                      },
                      [kind = option.first] {
                        filter_.kind_duration = kind;
                      });
                  up_item->Get<ora::ui::LabelWidget>("LABEL")->fn_get_text =
                      [str_text = option.second] { return str_text; };
                  return up_item;
                });
          };
    }
  }

  {
    auto* ptr_ddb =
        ptr_panel_->GetOrNull<ora::ui::DropDownButtonWidget>(
            "FLT_OUTCOME_DROPDOWNBUTTON");
    if (ptr_ddb != nullptr) {
      ptr_ddb->fn_is_disabled = [] { return filter_.str_player_name.empty(); };
      const std::vector<std::pair<server::WinState, std::string>>
          vec_options{
              {server::WinState::Undefined, ButtonText(*ptr_ddb)},
              {server::WinState::Lost, Msg("options-winstate.defeat")},
              {server::WinState::Won, Msg("options-winstate.victory")}};
      ptr_ddb->fn_get_text = [vec_options] {
        for (const auto& [kind, str_text] : vec_options)
          if (kind == filter_.kind_outcome)
            return str_text;
        return std::string{};
      };
      ptr_ddb->fn_on_mouse_down =
          [ptr_ddb, vec_options](const MouseInput&) {
            ptr_ddb->ShowDropDown<std::pair<server::WinState, std::string>>(
                "LABEL_DROPDOWN_TEMPLATE", 330, vec_options,
                [](const std::pair<server::WinState, std::string>& option,
                   ora::ui::ScrollItemWidget& tpl) {
                  auto up_item = ora::ui::ScrollItemWidget::Setup(
                      tpl, [kind = option.first] {
                        return filter_.kind_outcome == kind;
                      },
                      [kind = option.first] {
                        filter_.kind_outcome = kind;
                      });
                  up_item->Get<ora::ui::LabelWidget>("LABEL")->fn_get_text =
                      [str_text = option.second] { return str_text; };
                  return up_item;
                });
          };
    }
  }

  {
    auto* ptr_button = ptr_panel_->Get<ora::ui::ButtonWidget>(
        "FLT_RESET_BUTTON");
    ptr_button->fn_is_disabled = [] { return filter_.IsEmpty(); };
    ptr_button->fn_on_click = [] {
      filter_ = Filter{};
    };
  }
}

void ReplayBrowserLogic::SetupReplayDependentFilters() {
  {
    auto* ptr_ddb =
        ptr_panel_->GetOrNull<ora::ui::DropDownButtonWidget>(
            "FLT_MAPNAME_DROPDOWNBUTTON");
    if (ptr_ddb != nullptr) {
      std::vector<std::string> vec_options;
      for (const auto& state : vec_replays_)
        if (state.info_game.has_value()) {
          bool b_found = false;
          for (const std::string& str_existing : vec_options)
            if (EqualsIgnoreCase(str_existing,
                                 state.info_game->MapTitle)) {
              b_found = true;
              break;
            }
          if (!b_found)
            vec_options.push_back(state.info_game->MapTitle);
        }
      std::sort(vec_options.begin(), vec_options.end(),
                LessIgnoreCase);
      vec_options.insert(vec_options.begin(), std::string{});

      const std::string str_any_text = ButtonText(*ptr_ddb);
      ptr_ddb->fn_get_text = [str_any_text] {
        return filter_.str_map_name.empty() ? str_any_text
                                            : filter_.str_map_name;
      };
      ptr_ddb->fn_on_mouse_down =
          [ptr_ddb, vec_options, str_any_text](const MouseInput&) {
            ptr_ddb->ShowDropDown<std::string>(
                "LABEL_DROPDOWN_TEMPLATE", 330, vec_options,
                [str_any_text](const std::string& str_option,
                               ora::ui::ScrollItemWidget& tpl) {
                  auto up_item = ora::ui::ScrollItemWidget::Setup(
                      tpl,
                      [str_option] {
                        return EqualsIgnoreCase(filter_.str_map_name,
                                                str_option);
                      },
                      [str_option] { filter_.str_map_name = str_option; });
                  up_item->Get<ora::ui::LabelWidget>("LABEL")->fn_get_text =
                      [str_option, str_any_text] {
                        return str_option.empty() ? str_any_text
                                                  : str_option;
                      };
                  return up_item;
                });
          };
    }
  }

  {
    auto* ptr_ddb =
        ptr_panel_->GetOrNull<ora::ui::DropDownButtonWidget>(
            "FLT_PLAYER_DROPDOWNBUTTON");
    if (ptr_ddb != nullptr) {
      std::vector<std::string> vec_options;
      for (const auto& state : vec_replays_)
        if (state.info_game.has_value())
          for (const auto& p : state.info_game->vec_players) {
            const std::string str_name = GameInfoResolvedPlayerName(
                *state.info_game, p);
            bool b_found = false;
            for (const std::string& str_existing : vec_options)
              if (EqualsIgnoreCase(str_existing, str_name)) {
                b_found = true;
                break;
              }
            if (!b_found)
              vec_options.push_back(str_name);
          }
      std::sort(vec_options.begin(), vec_options.end(), LessIgnoreCase);
      vec_options.insert(vec_options.begin(), std::string{});

      const std::string str_any_text = ButtonText(*ptr_ddb);
      ptr_ddb->fn_get_text = [str_any_text] {
        return filter_.str_player_name.empty() ? str_any_text
                                               : filter_.str_player_name;
      };
      ptr_ddb->fn_on_mouse_down =
          [ptr_ddb, vec_options, str_any_text](const MouseInput&) {
            ptr_ddb->ShowDropDown<std::string>(
                "LABEL_DROPDOWN_TEMPLATE", 330, vec_options,
                [str_any_text](const std::string& str_option,
                               ora::ui::ScrollItemWidget& tpl) {
                  auto up_item = ora::ui::ScrollItemWidget::Setup(
                      tpl,
                      [str_option] {
                        return EqualsIgnoreCase(filter_.str_player_name,
                                                str_option);
                      },
                      [str_option] {
                        filter_.str_player_name = str_option;
                      });
                  up_item->Get<ora::ui::LabelWidget>("LABEL")->fn_get_text =
                      [str_option, str_any_text] {
                        return str_option.empty() ? str_any_text
                                                  : str_option;
                      };
                  return up_item;
                });
          };
    }
  }

  {
    auto* ptr_ddb =
        ptr_panel_->GetOrNull<ora::ui::DropDownButtonWidget>(
            "FLT_FACTION_DROPDOWNBUTTON");
    if (ptr_ddb != nullptr) {
      ptr_ddb->fn_is_disabled = [] { return filter_.str_player_name.empty(); };
      std::vector<std::string> vec_options;
      for (const auto& state : vec_replays_)
        if (state.info_game.has_value())
          for (const auto& p : state.info_game->vec_players) {
            if (p.FactionName.empty())
              continue;
            bool b_found = false;
            for (const std::string& str_existing : vec_options)
              if (EqualsIgnoreCase(str_existing, p.FactionName)) {
                b_found = true;
                break;
              }
            if (!b_found)
              vec_options.push_back(p.FactionName);
          }
      std::sort(vec_options.begin(), vec_options.end(), LessIgnoreCase);
      vec_options.insert(vec_options.begin(), std::string{});

      const std::string str_any_text = ButtonText(*ptr_ddb);
      ptr_ddb->fn_get_text = [str_any_text] {
        return filter_.str_faction.empty()
                   ? str_any_text
                   : Msg(filter_.str_faction);
      };
      ptr_ddb->fn_on_mouse_down =
          [ptr_ddb, vec_options, str_any_text](const MouseInput&) {
            ptr_ddb->ShowDropDown<std::string>(
                "LABEL_DROPDOWN_TEMPLATE", 330, vec_options,
                [str_any_text](const std::string& str_option,
                               ora::ui::ScrollItemWidget& tpl) {
                  auto up_item = ora::ui::ScrollItemWidget::Setup(
                      tpl,
                      [str_option] {
                        return EqualsIgnoreCase(filter_.str_faction,
                                                str_option);
                      },
                      [str_option] { filter_.str_faction = str_option; });
                  up_item->Get<ora::ui::LabelWidget>("LABEL")->fn_get_text =
                      [str_option, str_any_text] {
                        return str_option.empty()
                                   ? str_any_text
                                   : Msg(str_option);
                      };
                  return up_item;
                });
          };
    }
  }
}

void ReplayBrowserLogic::SetupManagement() {
  auto* ptr_rename =
      ptr_panel_->Get<ora::ui::ButtonWidget>("MNG_RENSEL_BUTTON");
  ptr_rename->fn_is_disabled = [this] {
    return sz_selected_ == static_cast<std::size_t>(-1);
  };
  ptr_rename->fn_on_click = [this] {
    if (sz_selected_ == static_cast<std::size_t>(-1))
      return;
    net::ReplayMetadata& meta = *vec_replays_[sz_selected_].up_metadata;
    const std::string str_initial_name =
        FilenameWithoutExtension(meta.FilePath());
    const std::string str_directory =
        std::filesystem::path{meta.FilePath()}
            .parent_path()
            .generic_string();

    TextInputPrompt(
        "dialog-rename-replay.title", "dialog-rename-replay.prompt",
        str_initial_name,
        [this, &meta](const std::string& str_new_name) {
          ora::ui::ScrollItemWidget* ptr_item =
              vec_replays_[sz_selected_].ptr_item;
          if (meta.RenameFile(str_new_name)) {
            ptr_item->fn_get_text = [str_new_name] { return str_new_name; };
            if (auto* ptr_label =
                    ptr_item->GetOrNull<ora::ui::LabelWithTooltipWidget>(
                        "TITLE"))
              ptr_label->fn_get_text = [str_new_name] {
                return str_new_name;
              };
          }
        },
        nullptr, "dialog-rename-replay.confirm", "",
        [str_initial_name, str_directory](const std::string& str_new_name) {
          if (str_new_name == str_initial_name)
            return false;
          if (!IsValidNewSaveName(str_new_name, "", str_directory))
            return false;
          return true;
        });
  };

  auto* ptr_delete =
      ptr_panel_->Get<ora::ui::ButtonWidget>("MNG_DELSEL_BUTTON");
  ptr_delete->fn_is_disabled = [this] {
    return sz_selected_ == static_cast<std::size_t>(-1);
  };
  ptr_delete->fn_on_click = [this] {
    if (sz_selected_ == static_cast<std::size_t>(-1))
      return;
    const std::size_t sz_target = sz_selected_;
    ButtonPrompt(
        "dialog-delete-replay.title", "dialog-delete-replay.prompt",
        [this, sz_target] {
          std::error_code ec{};
          std::filesystem::remove(
              vec_replays_[sz_target].up_metadata->FilePath(), ec);
          if (sz_target == sz_selected_)
            SelectReplay(static_cast<std::size_t>(-1));
          ptr_replay_list_->RemoveChild(
              *vec_replays_[sz_target].ptr_item);
          vec_replays_.erase(vec_replays_.begin() +
                             static_cast<std::ptrdiff_t>(sz_target));
          if (sz_selected_ == static_cast<std::size_t>(-1))
            SelectFirstVisibleReplay();
        },
        "dialog-delete-replay.confirm", nullptr, "",
        nullptr, "",
        MakeArgs({{"replay", FilenameWithoutExtension(
                                vec_replays_[sz_target]
                                    .up_metadata->FilePath())}}));
  };

  auto* ptr_delete_all =
      ptr_panel_->Get<ora::ui::ButtonWidget>("MNG_DELALL_BUTTON");
  ptr_delete_all->fn_is_disabled = [this] {
    for (const auto& state : vec_replays_)
      if (state.b_visible)
        return false;
    return true;
  };
  ptr_delete_all->fn_on_click = [this] {
    std::vector<std::size_t> vec_list;
    for (std::size_t i = 0; i < vec_replays_.size(); ++i)
      if (vec_replays_[i].b_visible)
        vec_list.push_back(i);
    if (vec_list.empty())
      return;

    const auto delete_all = [this, vec_list] {
      for (std::size_t i = vec_list.size(); i-- > 0;) {
        std::error_code ec{};
        std::filesystem::remove(
            vec_replays_[vec_list[i]].up_metadata->FilePath(), ec);
        ptr_replay_list_->RemoveChild(*vec_replays_[vec_list[i]].ptr_item);
        vec_replays_.erase(vec_replays_.begin() +
                           static_cast<std::ptrdiff_t>(vec_list[i]));
      }
      sz_selected_ = static_cast<std::size_t>(-1);
      SelectFirstVisibleReplay();
    };

    if (vec_list.size() == 1) {
      const std::size_t sz_target = vec_list[0];
      ButtonPrompt(
          "dialog-delete-replay.title", "dialog-delete-replay.prompt",
          [this, sz_target, delete_all] {
            std::error_code ec{};
            std::filesystem::remove(
                vec_replays_[sz_target].up_metadata->FilePath(), ec);
            ptr_replay_list_->RemoveChild(
                *vec_replays_[sz_target].ptr_item);
            vec_replays_.erase(vec_replays_.begin() +
                               static_cast<std::ptrdiff_t>(sz_target));
            sz_selected_ = static_cast<std::size_t>(-1);
            SelectFirstVisibleReplay();
          },
          "dialog-delete-replay.confirm", nullptr, "", nullptr, "",
          MakeArgs({{"replay", FilenameWithoutExtension(
                                  vec_replays_[sz_target]
                                      .up_metadata->FilePath())}}));
      return;
    }

    ButtonPrompt(
        "dialog-delete-all-replays.title",
        "dialog-delete-all-replays.prompt", delete_all,
        "dialog-delete-all-replays.confirm", nullptr, "", nullptr, "",
        MakeArgs({{"count", std::to_string(vec_list.size())}}));
  };
}

void ReplayBrowserLogic::SelectFirstVisibleReplay() {
  for (std::size_t i = 0; i < vec_replays_.size(); ++i)
    if (vec_replays_[i].b_visible) {
      SelectReplay(i);
      return;
    }
  SelectReplay(static_cast<std::size_t>(-1));
}

void ReplayBrowserLogic::SelectReplay(std::size_t sz_index) {
  sz_selected_ = sz_index;
  ptr_map_ = sz_index != static_cast<std::size_t>(-1) &&
                     vec_replays_[sz_index].info_game.has_value()
                 ? &GameInfoMapPreview(*vec_replays_[sz_index].info_game,
                                       *ptr_mod_data_)
                 : &map::MapCache::UnknownMap();
  if (sz_index == static_cast<std::size_t>(-1))
    return;

  map::MapCache* ptr_cache = ora::ui::Ui::Instance().MapCacheDeps();
  if (ptr_map_->Status() == map::MapStatus::Unavailable &&
      ptr_cache != nullptr && ptr_cache->AllowDownloading())
    ptr_cache->QueryRemoteMapDetails(
        game::WebServices{}.MapRepository, {ptr_map_->Uid()});

  const server::GameInformation& info =
      *vec_replays_[sz_index].info_game;

  std::vector<std::pair<int, std::vector<const server::GameInformationPlayer*>>>
      vec_players_by_team;
  for (const auto& p : info.vec_players) {
    bool b_found = false;
    for (auto& [team, vec_members] : vec_players_by_team)
      if (team == p.Team) {
        vec_members.push_back(&p);
        b_found = true;
        break;
      }
    if (!b_found)
      vec_players_by_team.emplace_back(
          p.Team, std::vector<const server::GameInformationPlayer*>{&p});
  }
  std::sort(vec_players_by_team.begin(), vec_players_by_team.end(),
            [](const auto& a, const auto& b) { return a.first < b.first; });

  const bool b_no_teams = vec_players_by_team.size() == 1;
  ptr_player_list_->RemoveChildren();
  for (const auto& [int4_team, vec_members] : vec_players_by_team) {
    const std::string str_label =
        b_no_teams
            ? Msg("label-players")
            : (int4_team > 0
                   ? Msg1("label-team-name", "team",
                          fluent::FluentValue{
                              static_cast<std::int64_t>(int4_team)})
                   : Msg("label-no-team"));
    if (!str_label.empty()) {
      auto up_header = ora::ui::ScrollItemWidget::Setup(
          *static_cast<ora::ui::ScrollItemWidget*>(up_player_header_.get()),
          [] { return false; }, [] {});
      up_header->Get<ora::ui::LabelWidget>("LABEL")->fn_get_text =
          [str_label] { return str_label; };
      ptr_player_list_->AddChild(std::move(up_header));
    }

    for (const server::GameInformationPlayer* ptr_player : vec_members) {
      const core::Color color = ptr_player->Color;
      const std::string str_name =
          GameInfoResolvedPlayerName(info, *ptr_player);
      auto up_item = ora::ui::ScrollItemWidget::Setup(
          *static_cast<ora::ui::ScrollItemWidget*>(up_player_template_.get()),
          [] { return false; }, [] {});
      up_item->Get<ora::ui::LabelWidget>("LABEL")->fn_get_text =
          [str_name] { return str_name; };
      up_item->Get<ora::ui::LabelWidget>("LABEL")->fn_get_color =
          [color] { return color; };
      auto* ptr_flag = up_item->Get<ora::ui::ImageWidget>("FLAG");
      ptr_flag->fn_get_image_collection = [] { return "flags"; };
      ptr_flag->fn_get_image_name =
          [faction_id = ptr_player->FactionId] { return faction_id; };
      ptr_player_list_->AddChild(std::move(up_item));
    }
  }
}

void ReplayBrowserLogic::WatchReplay() {
  if (sz_selected_ != static_cast<std::size_t>(-1) &&
      PromptConfirmReplayCompatibility(
          *vec_replays_[sz_selected_].up_metadata, *ptr_mod_data_)) {
    b_cancel_loading_ = true;
    ora::ui::Ui::Instance().JoinReplayDeps(
        vec_replays_[sz_selected_].up_metadata->FilePath());
  }
}

void ReplayBrowserLogic::AddReplay(
    std::unique_ptr<net::ReplayMetadata> up_meta,
    std::optional<server::GameInformation> info) {
  ReplayState state{};
  state.up_metadata = std::move(up_meta);
  state.info_game = std::move(info);

  const std::string str_item_text =
      FilenameWithoutExtension(state.up_metadata->FilePath());
  const std::size_t sz_index = vec_replays_.size();
  auto up_item = ora::ui::ScrollItemWidget::Setup(
      *static_cast<ora::ui::ScrollItemWidget*>(up_replay_template_.get()),
      [this, sz_index] { return sz_selected_ == sz_index; },
      [this, sz_index] { SelectReplay(sz_index); },
      [this] { WatchReplay(); });
  up_item->fn_get_text = [str_item_text] { return str_item_text; };
  if (auto* ptr_label =
          up_item->GetOrNull<ora::ui::LabelWithTooltipWidget>("TITLE"))
    ptr_label->fn_get_text = [str_item_text] { return str_item_text; };

  state.ptr_item = up_item.get();
  up_item->fn_is_visible = [this, sz_index] {
    return vec_replays_[sz_index].b_visible;
  };
  ptr_replay_list_->AddChild(std::move(up_item));
  vec_replays_.push_back(std::move(state));
}

void ReplayBrowserLogic::ApplyFilter() {
  for (auto& state : vec_replays_)
    state.b_visible = !state.info_game.has_value() ||
                      EvaluateReplayVisibility(*state.info_game);

  if (sz_selected_ == static_cast<std::size_t>(-1) ||
      !vec_replays_[sz_selected_].b_visible)
    SelectFirstVisibleReplay();

  if (ptr_replay_list_->ptr_layout != nullptr)
    ptr_replay_list_->ptr_layout->AdjustChildren();
  ptr_replay_list_->ScrollToSelectedItem();
}

bool ReplayBrowserLogic::EvaluateReplayVisibility(
    const server::GameInformation& info) {
  if ((filter_.kind_type == GameType::Multiplayer && info.IsSinglePlayer()) ||
      (filter_.kind_type == GameType::Singleplayer &&
       !info.IsSinglePlayer()))
    return false;

  if (filter_.kind_date != DateType::Any) {
    double fp_days = 30.0;
    switch (filter_.kind_date) {
      case DateType::Today:
        fp_days = 1.0;
        break;
      case DateType::LastWeek:
        fp_days = 7.0;
        break;
      case DateType::LastFortnight:
        fp_days = 14.0;
        break;
      case DateType::LastMonth:
      case DateType::Any:
        fp_days = 30.0;
        break;
    }
    const std::int64_t t8_cutoff =
        server::GameDateTime::UtcNow().ToUnixSeconds() -
        static_cast<std::int64_t>(fp_days * 86400.0);
    if (info.StartTimeUtc.ToUnixSeconds() < t8_cutoff)
      return false;
  }

  if (filter_.kind_duration != DurationType::Any) {
    const double fp_minutes = info.DurationSeconds() / 60.0;
    switch (filter_.kind_duration) {
      case DurationType::VeryShort:
        if (fp_minutes >= 5)
          return false;
        break;
      case DurationType::Short:
        if (fp_minutes < 5 || fp_minutes >= 20)
          return false;
        break;
      case DurationType::Medium:
        if (fp_minutes < 20 || fp_minutes >= 60)
          return false;
        break;
      case DurationType::Long:
        if (fp_minutes < 60)
          return false;
        break;
      case DurationType::Any:
        break;
    }
  }

  if (!filter_.str_map_name.empty() &&
      !EqualsIgnoreCase(filter_.str_map_name, info.MapTitle))
    return false;

  if (!filter_.str_player_name.empty()) {
    const server::GameInformationPlayer* ptr_player = nullptr;
    for (const auto& p : info.vec_players)
      if (EqualsIgnoreCase(filter_.str_player_name,
                           GameInfoResolvedPlayerName(info, p))) {
        ptr_player = &p;
        break;
      }
    if (ptr_player == nullptr)
      return false;

    if (filter_.kind_outcome != server::WinState::Undefined &&
        filter_.kind_outcome != ptr_player->Outcome)
      return false;

    if (!filter_.str_faction.empty() &&
        !EqualsIgnoreCase(filter_.str_faction, ptr_player->FactionName))
      return false;
  }

  return true;
}

void RegisterBrowserLogics() {
  auto& registry_logics = ora::ui::ChromeLogicRegistry::Instance();
  registry_logics.Register(
      "ReplayBrowserLogic",
      [](ora::ui::WidgetArgs& args_args)
          -> std::unique_ptr<ora::ui::ChromeLogic> {
        return std::make_unique<ReplayBrowserLogic>(
            *std::get<ora::ui::Widget*>(args_args.At("widget")),
            *std::get<game::ModData*>(args_args.At("modData")),
            std::get<std::function<void()>>(args_args.At("onExit")),
            std::get<std::function<void()>>(args_args.At("onStart")));
      });
}

}  // namespace ora::mods
