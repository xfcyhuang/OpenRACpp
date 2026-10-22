// UPSTREAM: OpenRA.Mods.Common/Widgets/Logic/Ingame/GameInfoLogic.cs
//          @e180e3225(实现;头注见 game_info_logics.hpp)
//          Implementation; the header notes live in game_info_logics.hpp.
#include "mods/game_info_logics.hpp"

#include "fluent/fluent_provider.hpp"
#include "game/actor_info.hpp"
#include "gfx/sprite_font.hpp"
#include "game/mod_data.hpp"
#include "map/map_cache.hpp"
#include "mods/confirmation_dialogs.hpp"
#include "mods/lobby_utils.hpp"
#include "mods/mission_objectives.hpp"
#include "mods/player_statistics.hpp"
#include "net/order_manager.hpp"
#include "net/session.hpp"
#include "net/text_notifications.hpp"
#include "net/unit_orders.hpp"
#include "sim/actor.hpp"
#include "sim/player.hpp"
#include "sim/trait_registry.hpp"
#include "sim/world.hpp"
#include "ui/ui.hpp"
#include "ui/widget_utils.hpp"
#include "ui/widgets/button_widget.hpp"
#include "ui/widgets/checkbox_widget.hpp"
#include "ui/widgets/image_widget.hpp"
#include "ui/widgets/label_widget.hpp"
#include "ui/widgets/label_with_tooltip_widget.hpp"
#include "ui/widgets/map_preview_widget.hpp"
#include "ui/widgets/scroll_item_widget.hpp"
#include "ui/widgets/scroll_panel_widget.hpp"
#include "ui/widgets/text_field_widget.hpp"

#ifdef GetMessage
#undef GetMessage
#endif

namespace ora::mods {
namespace {

fluent::FluentProvider& Fluent() {
  return fluent::FluentProvider::Instance();
}

std::string Msg(std::string_view str_key) {
  return Fluent().GetMessage(str_key);
}

std::string Msg1(std::string_view str_key, std::string_view str_arg_name,
                 fluent::FluentValue val_value) {
  return Fluent().GetMessage(
      str_key,
      std::vector<std::pair<std::string, fluent::FluentValue>>{
          {std::string{str_arg_name}, std::move(val_value)}});
}

const meta::RecordObject* FirstTraitInfo(const game::ActorInfo& info_actor,
                                         std::string_view str_name) {
  for (const auto& rec_trait : info_actor.Traits())
    if (rec_trait->record_desc().str_name == str_name)
      return rec_trait.get();
  return nullptr;
}

std::pair<std::string_view, std::string_view> PanelOf(IngameInfoPanel panel) {
  switch (panel) {
    case IngameInfoPanel::Objectives:
      return {"OBJECTIVES_PANEL", "menu-game-info.objectives"};
    case IngameInfoPanel::Map:
      return {"MAP_PANEL", "menu-game-info.briefing"};
    case IngameInfoPanel::LobbyOptions:
      return {"LOBBY_OPTIONS_PANEL", "menu-game-info.options"};
    case IngameInfoPanel::Debug:
      return {"DEBUG_PANEL", "menu-game-info.debug"};
    case IngameInfoPanel::Chat:
      return {"CHAT_PANEL", "menu-game-info.chat"};
    default:
      return {"", ""};
  }
}

}  // namespace

GameInfoLogic::GameInfoLogic(ora::ui::Widget& widget, game::ModData& mod_data,
                             sim::World& world, IngameInfoPanel initial_panel,
                             std::function<void(bool)> fn_hide_menu,
                             std::function<void()> fn_close_menu)
    : ptr_world_(&world),
      ptr_mod_data_(&mod_data),
      fn_hide_menu_(std::move(fn_hide_menu)),
      fn_close_menu_(std::move(fn_close_menu)) {
  active_panel_ = initial_panel;

  static_cast<void>(&mod_data);

  const game::ActorInfo* ptr_world_actor_info =
      world.WorldActor() != nullptr ? world.WorldActor()->Info() : nullptr;

  b_has_error_ = false;

  if (ptr_world_actor_info != nullptr) {
    if (const meta::RecordObject* rec_panel =
            FirstTraitInfo(*ptr_world_actor_info, "ObjectivesPanelInfo"))
      if (const auto sv_panel = sim::RecordFieldString(*rec_panel, "PanelName"))
        str_objectives_panel_ = std::string{*sv_panel};
  }

  if (b_has_error_ || !str_objectives_panel_.empty())
    vec_visible_panels_.push_back(IngameInfoPanel::Objectives);

  std::string str_briefing;
  if (ptr_world_actor_info != nullptr) {
    if (const meta::RecordObject* rec_mission =
            FirstTraitInfo(*ptr_world_actor_info, "MissionDataInfo"))
      if (const auto sv_briefing =
              sim::RecordFieldString(*rec_mission, "Briefing"))
        str_briefing = std::string{*sv_briefing};
  }
  if (!str_briefing.empty())
    vec_visible_panels_.push_back(IngameInfoPanel::Map);

  vec_visible_panels_.push_back(IngameInfoPanel::LobbyOptions);

  bool b_developer_enabled = false;
  if (world.MapPtr() != nullptr) {
    if (const game::ActorInfo* ptr_player_actor =
            world.MapPtr()->Rules().FindActor("player"))
      for (const meta::RecordObject* rec :
           ptr_player_actor->TraitsInConstructOrder()) {
        if (rec->record_desc().str_name !=
            std::string_view{"DeveloperModeInfo"})
          continue;
        if (const auto* gv = sim::RecordFieldValue(*rec, "CheckboxEnabled"))
          if (const auto* b = std::get_if<bool>(&gv->val))
            b_developer_enabled = *b;
      }
  }
  b_developer_enabled = world.LobbyInfo().global_settings.OptionOrDefault(
      "cheats", b_developer_enabled);
  if (world.LocalPlayer() != nullptr && b_developer_enabled)
    vec_visible_panels_.push_back(IngameInfoPanel::Debug);

  if (world.LobbyInfo().NonBotClients().size() > 1)
    vec_visible_panels_.push_back(IngameInfoPanel::Chat);

  const int int4_num_tabs = static_cast<int>(vec_visible_panels_.size());
  ora::ui::Widget* ptr_tab_container =
      !b_has_error_
          ? widget.GetOrNull<ora::ui::Widget>(
                std::format("TAB_CONTAINER_{}", int4_num_tabs))
          : nullptr;
  if (ptr_tab_container != nullptr)
    ptr_tab_container->fn_is_visible = [] { return true; };

  ora::ui::Widget* ptr_chat_panel =
      widget.Get<ora::ui::Widget>("CHAT_PANEL");

  for (int int4_i = 0; int4_i < int4_num_tabs; ++int4_i) {
    const IngameInfoPanel type_panel = vec_visible_panels_[int4_i];
    const auto [sv_panel, sv_label] = PanelOf(type_panel);
    const std::string str_panel_id{sv_panel};
    const std::string str_label_key{sv_label};

    ora::ui::ButtonWidget* ptr_tab_button =
        ptr_tab_container != nullptr
            ? ptr_tab_container->GetOrNull<ora::ui::ButtonWidget>(
                  std::format("BUTTON{}", int4_i + 1))
            : nullptr;

    if (ptr_tab_button != nullptr) {
      const std::string str_tab_text = Msg(str_label_key);
      ptr_tab_button->fn_get_text = [str_tab_text] { return str_tab_text; };
      ptr_tab_button->fn_on_click = [this, type_panel, ptr_chat_panel] {
        if (active_panel_ == IngameInfoPanel::Chat)
          LeaveChatPanelForTest(*ptr_chat_panel);
        active_panel_ = type_panel;
      };
      ptr_tab_button->fn_is_highlighted = [this, type_panel] {
        return active_panel_ == type_panel;
      };
    }

    ora::ui::Widget* ptr_panel_container =
        widget.Get<ora::ui::Widget>(str_panel_id);
    ptr_panel_container->fn_is_visible = [this, type_panel] {
      return active_panel_ == type_panel;
    };

    switch (type_panel) {
      case IngameInfoPanel::Objectives:
        SetupObjectivesPanel(ptr_tab_button, *ptr_panel_container);
        break;
      case IngameInfoPanel::Map:
        SetupMapPanel(ptr_tab_button, *ptr_panel_container);
        break;
      case IngameInfoPanel::LobbyOptions:
        SetupLobbyOptionsPanel(ptr_tab_button, *ptr_panel_container);
        break;
      case IngameInfoPanel::Debug:
        SetupDebugPanel(ptr_tab_button, *ptr_panel_container);
        break;
      case IngameInfoPanel::Chat:
        SetupChatPanel(ptr_tab_button, *ptr_panel_container);
        break;
      default:
        break;
    }

    if (active_panel_ == IngameInfoPanel::AutoSelect)
      active_panel_ = type_panel;
  }

  auto* ptr_title = widget.GetOrNull<ora::ui::LabelWidget>("TITLE");

  str_map_title_ = world.MapPtr() != nullptr ? world.MapPtr()->Title()
                                            : std::string{};
  if (world.MapPtr() != nullptr &&
      !world.MapPtr()->Categories().empty())
    str_map_title_ = world.MapPtr()->Categories().front() + ": " +
                     str_map_title_;

  if (ptr_title != nullptr)
    ptr_title->fn_get_text = [this] { return str_map_title_; };
}

void GameInfoLogic::SetupObjectivesPanel(
    ora::ui::ButtonWidget* ptr_tab_button,
    ora::ui::Widget& widget_panel_container) {
  static_cast<void>(ptr_tab_button);
  const std::string str_panel = b_has_error_ ? std::string{"SCRIPT_ERROR_PANEL"}
                                             : str_objectives_panel_;
  ora::ui::WidgetArgs args_args;
  args_args.Add("hideMenu", fn_hide_menu_);
  args_args.Add("closeMenu", fn_close_menu_);
  ora::ui::Ui::Instance().LoadWidget(str_panel, &widget_panel_container,
                                     std::move(args_args));
}

void GameInfoLogic::SetupMapPanel(ora::ui::ButtonWidget* ptr_tab_button,
                                  ora::ui::Widget& widget_panel_container) {
  static_cast<void>(ptr_tab_button);
  ora::ui::Ui::Instance().LoadWidget("MAP_PANEL", &widget_panel_container,
                                     ora::ui::WidgetArgs{});
}

void GameInfoLogic::SetupLobbyOptionsPanel(
    ora::ui::ButtonWidget* ptr_tab_button,
    ora::ui::Widget& widget_panel_container) {
  static_cast<void>(ptr_tab_button);
  ora::ui::WidgetArgs args_args;
  args_args.Add(
      "getMap",
      std::function<map::MapPreview*()>{[world = ptr_world_]() {
        map::MapCache* ptr_cache = ora::ui::Ui::Instance().MapCacheDeps();
        if (ptr_cache == nullptr || world->MapPtr() == nullptr)
          return static_cast<map::MapPreview*>(nullptr);
        return &ptr_cache->At(world->MapPtr()->Uid());
      }});
  args_args.Add("configurationDisabled",
                std::function<bool()>{[] { return true; }});
  ora::ui::Ui::Instance().LoadWidget("LOBBY_OPTIONS_PANEL",
                                     &widget_panel_container,
                                     std::move(args_args));
}

void GameInfoLogic::SetupDebugPanel(ora::ui::ButtonWidget* ptr_tab_button,
                                    ora::ui::Widget& widget_panel_container) {
  if (ptr_tab_button != nullptr)
    ptr_tab_button->fn_is_disabled = [world = ptr_world_] {
      return world->IsGameOver();
    };

  ora::ui::Ui::Instance().LoadWidget("DEBUG_PANEL", &widget_panel_container,
                                     ora::ui::WidgetArgs{});

  if (active_panel_ == IngameInfoPanel::AutoSelect)
    active_panel_ = IngameInfoPanel::Debug;
}

void GameInfoLogic::SetupChatPanel(ora::ui::ButtonWidget* ptr_tab_button,
                                   ora::ui::Widget& widget_panel_container) {
  if (ptr_tab_button != nullptr) {
    auto fn_last_on_click = ptr_tab_button->fn_on_click;
    ptr_tab_button->fn_on_click =
        [fn_last_on_click, ptr_tab_button, &widget_panel_container] {
          if (fn_last_on_click)
            fn_last_on_click();
          widget_panel_container.Get<ora::ui::TextFieldWidget>(
                                  "CHAT_TEXTFIELD")
              ->TakeKeyboardFocus();
          static_cast<void>(ptr_tab_button);
        };
  }

  ora::ui::WidgetArgs args_args;
  args_args.Add("isMenuChat", true);
  ora::ui::Ui::Instance().LoadWidget("CHAT_CONTAINER", &widget_panel_container,
                                     std::move(args_args));
}

void GameInfoLogic::LeaveChatPanelForTest(
    ora::ui::Widget& widget_chat_panel) {
  widget_chat_panel.Get<ora::ui::TextFieldWidget>("CHAT_TEXTFIELD")
      ->YieldKeyboardFocus();
}

GameInfoBriefingLogic::GameInfoBriefingLogic(ora::ui::Widget& widget,
                                             game::ModData& mod_data,
                                             sim::World& world) {
  static_cast<void>(&mod_data);
  auto* ptr_preview_widget =
      widget.Get<ora::ui::MapPreviewWidget>("MAP_PREVIEW");
  const std::string str_uid =
      world.MapPtr() != nullptr ? world.MapPtr()->Uid() : std::string{};
  ptr_preview_widget->fn_preview = [str_uid]() -> map::MapPreview* {
    map::MapCache* ptr_cache = ora::ui::Ui::Instance().MapCacheDeps();
    if (ptr_cache == nullptr)
      return nullptr;
    return &ptr_cache->At(str_uid);
  };

  auto* ptr_description_panel =
      widget.Get<ora::ui::ScrollPanelWidget>("MAP_DESCRIPTION_PANEL");
  auto* ptr_description =
      widget.Get<ora::ui::LabelWidget>("MAP_DESCRIPTION");
  ora::gfx::SpriteFont& font_map =
      ora::ui::RequireWidgetFont(ptr_description->str_font);

  std::string str_briefing;
  if (world.MapPtr() != nullptr) {
    if (const game::ActorInfo* ptr_world_actor =
            world.MapPtr()->Rules().FindActor("world"))
      if (const meta::RecordObject* rec_mission =
              FirstTraitInfo(*ptr_world_actor, "MissionDataInfo"))
        if (const auto sv_briefing =
                sim::RecordFieldString(*rec_mission, "Briefing"))
          str_briefing = std::string{*sv_briefing};
  }

  if (!str_briefing.empty()) {
    std::string str_text = str_briefing;
    if (map::MapPreview* ptr_preview = ptr_preview_widget->fn_preview()) {
      std::string str_translation;
      if (ptr_preview->TryGetMessage(str_briefing, str_translation))
        str_text = str_translation;
    }

    str_text_ = ora::ui::WrapText(str_text,
                                  ptr_description->bounds_widget.int4_width,
                                  font_map);
    ptr_description->fn_get_text = [this] { return str_text_; };
    ptr_description->bounds_widget.int4_height =
        font_map.Measure(str_text_).Y;
    ptr_description_panel->ScrollToTop();
    if (ptr_description_panel->ptr_layout != nullptr)
      ptr_description_panel->ptr_layout->AdjustChildren();
  }
}

GameInfoObjectivesLogic::GameInfoObjectivesLogic(ora::ui::Widget& widget,
                                                 sim::World& world) {
  ptr_player_ = world.RenderPlayer() != nullptr ? world.RenderPlayer()
                                                : world.LocalPlayer();

  ptr_objectives_panel_ =
      widget.Get<ora::ui::ScrollPanelWidget>("OBJECTIVES_PANEL");
  up_template_ = ptr_objectives_panel_->RemoveChild(
      *ptr_objectives_panel_->Get<ora::ui::Widget>("OBJECTIVE_TEMPLATE"));

  if (ptr_player_ == nullptr) {
    ptr_objectives_panel_->RemoveChildren();
    return;
  }

  if (ptr_player_->PlayerActor() != nullptr)
    ptr_mission_objectives_ =
        ptr_player_->PlayerActor()->TraitOrDefault<MissionObjectives>();
  if (ptr_mission_objectives_ == nullptr) {
    ptr_objectives_panel_->RemoveChildren();
    return;
  }

  const std::string str_in_progress = Msg("label-mission-in-progress");
  const std::string str_accomplished = Msg("label-mission-accomplished");
  const std::string str_failed = Msg("label-mission-failed");

  auto* ptr_mission_status =
      widget.Get<ora::ui::LabelWidget>("MISSION_STATUS");
  ptr_mission_status->fn_get_text = [this, str_in_progress, str_accomplished,
                                     str_failed] {
    switch (ptr_player_->PlayerWinState()) {
      case sim::WinState::Won:
        return str_accomplished;
      case sim::WinState::Lost:
        return str_failed;
      default:
        return str_in_progress;
    }
  };
  ptr_mission_status->fn_get_color = [this] {
    switch (ptr_player_->PlayerWinState()) {
      case sim::WinState::Won:
        return core::Color{0xFF32CD32};
      case sim::WinState::Lost:
        return core::Color{0xFFFF0000};
      default:
        return core::Color{0xFFFFFFFF};
    }
  };

  PopulateObjectivesList();
  str_mission_status_ =
      widget.Get<ora::ui::LabelWidget>("MISSION_STATUS")->fn_get_text();

  n_objective_added_token_ = ptr_mission_objectives_->AddObjectiveAddedHandler(
      [this](sim::Player& player_updated, bool) {
        if (&player_updated == ptr_player_)
          PopulateObjectivesList();
      });
}

GameInfoObjectivesLogic::~GameInfoObjectivesLogic() {
  if (ptr_mission_objectives_ != nullptr && n_objective_added_token_ != 0)
    ptr_mission_objectives_->RemoveObjectiveAddedHandler(
        n_objective_added_token_);
}

void GameInfoObjectivesLogic::PopulateObjectivesList() {
  ptr_objectives_panel_->RemoveChildren();

  std::vector<const MissionObjective*> vec_objectives;
  for (const MissionObjective& objective : ptr_mission_objectives_->Objectives())
    vec_objectives.push_back(&objective);
  std::stable_sort(vec_objectives.begin(), vec_objectives.end(),
                   [](const MissionObjective* a, const MissionObjective* b) {
                     return a->str_type < b->str_type;
                   });

  for (const MissionObjective* ptr_objective : vec_objectives) {
    auto up_widget = up_template_->Clone();
    up_widget->Get<ora::ui::LabelWidget>("OBJECTIVE_TYPE")->fn_get_text =
        [ptr_objective] { return ptr_objective->str_type; };

    auto* ptr_checkbox =
        up_widget->Get<ora::ui::CheckboxWidget>("OBJECTIVE_STATUS");
    ptr_checkbox->fn_is_checked = [ptr_objective] {
      return ptr_objective->state != ObjectiveState::Incomplete;
    };
    ptr_checkbox->fn_get_checkmark = [ptr_objective] {
      return ptr_objective->state == ObjectiveState::Completed
                 ? std::string{"tick"}
                 : std::string{"cross"};
    };
    ptr_checkbox->fn_get_text = [ptr_objective] {
      return ptr_objective->str_description;
    };

    ptr_objectives_panel_->AddChild(std::move(up_widget));
  }
}

GameInfoStatsLogic::GameInfoStatsLogic(
    ora::ui::Widget& widget, game::ModData& mod_data, sim::World& world,
    net::OrderManager& order_manager, gfx::WorldRenderer* ptr_world_renderer,
    std::function<void(bool)> fn_hide_menu,
    std::function<void()> fn_close_menu)
    : ptr_world_(&world),
      ptr_mod_data_(&mod_data),
      ptr_order_manager_(&order_manager),
      fn_hide_menu_(std::move(fn_hide_menu)),
      fn_close_menu_(std::move(fn_close_menu)) {
  sim::Player* ptr_local_player = world.LocalPlayer();
  auto* ptr_player_panel =
      widget.Get<ora::ui::ScrollPanelWidget>("PLAYER_LIST");
  ora::ui::Widget* ptr_stats_header = widget.Get<ora::ui::Widget>(
      "STATS_HEADERS");

  if (ptr_local_player != nullptr && !ptr_local_player->NonCombatant()) {
    auto* ptr_checkbox =
        widget.Get<ora::ui::CheckboxWidget>("STATS_CHECKBOX");
    auto* ptr_status_label = widget.Get<ora::ui::LabelWidget>("STATS_STATUS");

    ptr_checkbox->fn_is_checked = [ptr_local_player] {
      return ptr_local_player->PlayerWinState() != sim::WinState::Undefined;
    };
    ptr_checkbox->fn_get_checkmark = [ptr_local_player] {
      return ptr_local_player->PlayerWinState() == sim::WinState::Won
                 ? std::string{"tick"}
                 : std::string{"cross"};
    };

    if (ptr_local_player->HasObjectives()) {
      MissionObjectives* ptr_mo =
          ptr_local_player->PlayerActor() != nullptr
              ? ptr_local_player->PlayerActor()->TraitOrDefault<MissionObjectives>()
              : nullptr;
      if (ptr_mo != nullptr && !ptr_mo->Objectives().empty())
        ptr_checkbox->fn_get_text = [ptr_mo] {
          return ptr_mo->Objectives().front().str_description;
        };
    }

    const std::string str_failed = Msg("label-mission-failed");
    const std::string str_in_progress = Msg("label-mission-in-progress");
    const std::string str_accomplished = Msg("label-mission-accomplished");
    ptr_status_label->fn_get_text =
        [ptr_local_player, str_accomplished, str_failed, str_in_progress] {
          switch (ptr_local_player->PlayerWinState()) {
            case sim::WinState::Won:
              return str_accomplished;
            case sim::WinState::Lost:
              return str_failed;
            default:
              return str_in_progress;
          }
        };
    str_status_text_ = ptr_status_label->fn_get_text();
    ptr_status_label->fn_get_color = [ptr_local_player] {
      switch (ptr_local_player->PlayerWinState()) {
        case sim::WinState::Won:
          return core::Color{0xFF32CD32};
        case sim::WinState::Lost:
          return core::Color{0xFFFF0000};
        default:
          return core::Color{0xFFFFFFFF};
      }
    };
  } else {
    ora::ui::Widget* ptr_objective_group =
        widget.Get<ora::ui::Widget>("OBJECTIVE");

    ptr_objective_group->b_visible = false;
    ptr_stats_header->bounds_widget.int4_y -=
        ptr_objective_group->bounds_widget.int4_height;
    ptr_player_panel->bounds_widget.int4_y -=
        ptr_objective_group->bounds_widget.int4_height;
    ptr_player_panel->bounds_widget.int4_height +=
        ptr_objective_group->bounds_widget.int4_height;
  }

  bool b_has_other_clients = false;
  for (const net::SessionClient& client : order_manager.LobbyInfo().vec_clients)
    if (!client.IsBot() &&
        client.Index != (order_manager.LocalClient() != nullptr
                             ? order_manager.LocalClient()->Index
                             : -1) &&
        client.State != net::ClientState::Disconnected) {
      b_has_other_clients = true;
      break;
    }
  if (!b_has_other_clients)
    ptr_stats_header->Get<ora::ui::LabelWidget>("ACTIONS")->b_visible = false;

  {
    std::unique_ptr<ora::ui::Widget> up_team_taken =
        ptr_player_panel->RemoveChild(
            *ptr_player_panel->Get<ora::ui::Widget>("TEAM_TEMPLATE"));
    up_team_template_.reset(
        static_cast<ora::ui::ScrollItemWidget*>(up_team_taken.release()));
    up_player_template_ = ptr_player_panel->RemoveChild(
        *ptr_player_panel->Get<ora::ui::Widget>("PLAYER_TEMPLATE"));
    up_spectator_template_ = ptr_player_panel->RemoveChild(
        *ptr_player_panel->Get<ora::ui::Widget>("SPECTATOR_TEMPLATE"));
  }
  const std::string str_unmute_tooltip = Msg("label-unmute-player");
  const std::string str_mute_tooltip = Msg("label-mute-player");
  const std::string str_kick_tooltip = Msg("button-kick-player");
  const std::string str_vote_kick_tooltip = Msg("button-vote-kick-player");
  ptr_player_panel->RemoveChildren();

  struct StatsEntry {
    sim::Player* ptr_player;
    PlayerStatistics* ptr_statistics;
    int int4_experience;
  };
  std::vector<StatsEntry> vec_entries;
  for (sim::Player* ptr_player : world.Players()) {
    if (ptr_player->NonCombatant() || !ptr_player->Playable())
      continue;
    PlayerStatistics* ptr_statistics =
        ptr_player->PlayerActor() != nullptr
            ? ptr_player->PlayerActor()->TraitOrDefault<PlayerStatistics>()
            : nullptr;
    vec_entries.push_back(
        {ptr_player, ptr_statistics,
         ptr_statistics != nullptr ? ptr_statistics->GetExperience() : 0});
  }
  std::stable_sort(vec_entries.begin(), vec_entries.end(),
                   [](const StatsEntry& a, const StatsEntry& b) {
                     return a.int4_experience > b.int4_experience;
                   });

  struct TeamGroup {
    int int4_team;
    int int4_total_experience;
    std::vector<StatsEntry> vec_members;
  };
  std::vector<TeamGroup> vec_teams;
  for (const StatsEntry& entry : vec_entries) {
    const net::SessionClient* ptr_client =
        world.LobbyInfo().ClientWithIndex(entry.ptr_player->ClientIndex());
    const int int4_team = ptr_client != nullptr ? ptr_client->Team : 0;
    auto it_team = std::find_if(vec_teams.begin(), vec_teams.end(),
                                [int4_team](const TeamGroup& group) {
                                  return group.int4_team == int4_team;
                                });
    if (it_team == vec_teams.end()) {
      vec_teams.push_back(TeamGroup{int4_team, 0, {}});
      it_team = std::end(vec_teams) - 1;
    }
    it_team->int4_total_experience += entry.int4_experience;
    it_team->vec_members.push_back(entry);
  }
  std::stable_sort(vec_teams.begin(), vec_teams.end(),
                   [](const TeamGroup& a, const TeamGroup& b) {
                     return a.int4_total_experience >
                            b.int4_total_experience;
                   });

  const net::SessionClient* ptr_local_client = order_manager.LocalClient();
  sim::Player* ptr_local_player_by_client = nullptr;
  if (ptr_local_client != nullptr)
    for (sim::Player* ptr_player : world.Players())
      if (ptr_player->ClientIndex() == ptr_local_client->Index) {
        ptr_local_player_by_client = ptr_player;
        break;
      }
  const bool b_is_host = ora::ui::Ui::Instance().IsHostDeps();

  for (const TeamGroup& group : vec_teams) {
    if (vec_teams.size() > 1) {
      auto up_team_header = ora::ui::ScrollItemWidget::Setup(
          *up_team_template_, [] { return false; }, [] {});
      const std::string str_team =
          group.int4_team > 0
              ? Msg1("label-team-name", "team",
                     fluent::FluentValue{
                         static_cast<std::int64_t>(group.int4_team)})
              : Msg("label-no-team");
      up_team_header->Get<ora::ui::LabelWidget>("TEAM")->fn_get_text =
          [str_team] { return str_team; };
      vec_team_headers_.push_back(str_team);

      auto* ptr_team_rating =
          up_team_header->Get<ora::ui::LabelWidget>("TEAM_SCORE");
      auto sp_score_cache =
          std::make_shared<core::CachedTransform<int, std::string>>(
              [](const int& int4_score) { return std::to_string(int4_score); });
      const int int4_total = group.int4_total_experience;
      ptr_team_rating->fn_get_text = [int4_total,
                                      cache_score = std::move(sp_score_cache)] {
        return cache_score->Update(int4_total);
      };

      ptr_player_panel->AddChild(std::move(up_team_header));
    }

    for (const StatsEntry& entry : group.vec_members) {
      sim::Player* ptr_player = entry.ptr_player;
      net::SessionClient* ptr_client =
          world.LobbyInfo().ClientWithIndex(ptr_player->ClientIndex());
      auto up_item = up_player_template_->Clone();
      SetupProfileWidget(*up_item, ptr_client, ptr_order_manager_,
                         ptr_world_renderer);

      auto* ptr_name_label =
          up_item->Get<ora::ui::LabelWithTooltipWidget>("NAME");
      ora::ui::BindPlayerNameAndStatus(*ptr_name_label, *ptr_player);
      vec_player_rows_.push_back(ptr_player->ResolvedPlayerName());
      ptr_name_label->fn_get_color = [ptr_player] {
        return ptr_player->Color();
      };

      auto* ptr_flag = up_item->Get<ora::ui::ImageWidget>("FACTIONFLAG");
      ptr_flag->fn_get_image_collection = [] { return std::string{"flags"}; };

      std::string str_faction_name = ptr_player->DisplayFaction().Name;
      if (ptr_local_player == nullptr ||
          ptr_local_player->RelationshipWith(ptr_player) ==
              sim::PlayerRelationship::Ally ||
          ptr_local_player->PlayerWinState() != sim::WinState::Undefined) {
        ptr_flag->fn_get_image_name = [ptr_player] {
          return ptr_player->Faction().InternalName;
        };
        str_faction_name =
            ptr_player->Faction().Name != str_faction_name
                ? std::format("{} ({})", Msg(str_faction_name),
                              Msg(ptr_player->Faction().Name))
                : Msg(ptr_player->Faction().Name);
      } else {
        ptr_flag->fn_get_image_name = [ptr_player] {
          return ptr_player->DisplayFaction().InternalName;
        };
        str_faction_name = Msg(str_faction_name);
      }

      ora::ui::TruncateLabelToTooltip(
          *up_item->Get<ora::ui::LabelWithTooltipWidget>("FACTION"),
          str_faction_name);

      auto sp_score_cache =
          std::make_shared<core::CachedTransform<int, std::string>>(
              [](const int& int4_score) { return std::to_string(int4_score); });
      const int int4_score = entry.int4_experience;
      up_item->Get<ora::ui::LabelWidget>("SCORE")->fn_get_text =
          [int4_score, cache_score = std::move(sp_score_cache)] {
            return cache_score->Update(int4_score);
          };

      auto* ptr_mute_checkbox = up_item->Get<ora::ui::CheckboxWidget>("MUTE");
      const int int4_mute_index = ptr_player->ClientIndex();
      ptr_mute_checkbox->fn_is_checked = [int4_mute_index] {
        const auto& map_muted =
            net::TextNotificationsManager::Instance().MutedPlayers();
        const auto it_muted = map_muted.find(int4_mute_index);
        return it_muted != map_muted.end() && it_muted->second;
      };
      ptr_mute_checkbox->fn_on_click = [int4_mute_index] {
        auto& map_muted =
            net::TextNotificationsManager::Instance().MutedPlayers();
        map_muted[int4_mute_index] = !map_muted[int4_mute_index];
      };
      ptr_mute_checkbox->fn_is_visible = [this, ptr_player, ptr_client,
                                          int4_mute_index] {
        return !ptr_player->IsBot() &&
               ptr_client != nullptr &&
               ptr_client->State != net::ClientState::Disconnected &&
               int4_mute_index !=
                   (ptr_order_manager_->LocalClient() != nullptr
                        ? ptr_order_manager_->LocalClient()->Index
                        : -1);
      };
      ptr_mute_checkbox->fn_get_tooltip_text =
          [ptr_mute_checkbox, str_unmute_tooltip, str_mute_tooltip] {
            return ptr_mute_checkbox->fn_is_checked() ? str_unmute_tooltip
                                                      : str_mute_tooltip;
          };

      auto* ptr_kick_button = up_item->Get<ora::ui::ButtonWidget>("KICK");
      const bool b_local_can_kick =
          ptr_local_client != nullptr &&
          (b_is_host ||
           (!ptr_local_client->IsObserver() &&
            ptr_local_player_by_client != nullptr &&
            ptr_local_player_by_client->PlayerWinState() ==
                sim::WinState::Undefined));
      ptr_kick_button->fn_is_visible =
          [this, ptr_player, ptr_client, b_local_can_kick] {
            const auto fn_is_vote_kick = [this, ptr_player] {
              return !ora::ui::Ui::Instance().IsHostDeps() ||
                     ptr_player->PlayerWinState() == sim::WinState::Undefined;
            };
            const bool b_client_kickable =
                ptr_client != nullptr &&
                ptr_client->Index !=
                    (ptr_order_manager_->LocalClient() != nullptr
                         ? ptr_order_manager_->LocalClient()->Index
                         : -1) &&
                ptr_client->State != net::ClientState::Disconnected &&
                (!ptr_client->IsAdmin ||
                 ptr_order_manager_->LobbyInfo().global_settings
                     .Dedicated) &&
                (!fn_is_vote_kick() ||
                 !net::KickVoteTarget().has_value() ||
                 net::KickVoteTarget() == ptr_client->Index);
            return !ptr_player->IsBot() && b_local_can_kick &&
                   b_client_kickable;
          };
      if (ptr_client != nullptr)
        ptr_kick_button->fn_on_click =
            [this, ptr_client,
             ptr_player] {
              const auto fn_is_vote_kick = [ptr_player] {
                return !ora::ui::Ui::Instance().IsHostDeps() ||
                       ptr_player->PlayerWinState() ==
                           sim::WinState::Undefined;
              };
              KickAction(*ptr_client, fn_is_vote_kick);
            };
      ptr_kick_button->fn_get_tooltip_text =
          [str_kick_tooltip, str_vote_kick_tooltip, ptr_player] {
            const bool b_vote = !ora::ui::Ui::Instance().IsHostDeps() ||
                                ptr_player->PlayerWinState() ==
                                    sim::WinState::Undefined;
            return b_vote ? str_vote_kick_tooltip : str_kick_tooltip;
          };

      ptr_player_panel->AddChild(std::move(up_item));
    }
  }

  std::vector<net::SessionClient*> vec_spectators;
  for (net::SessionClient& client : order_manager.LobbyInfo().vec_clients)
    if (client.IsObserver())
      vec_spectators.push_back(&client);
  if (!vec_spectators.empty()) {
    auto up_spectator_header = ora::ui::ScrollItemWidget::Setup(
        *up_team_template_, [] { return false; }, [] {});
    const std::string str_spectator_team = Msg("label-spectators");
    up_spectator_header->Get<ora::ui::LabelWidget>("TEAM")->fn_get_text =
        [str_spectator_team] { return str_spectator_team; };
    vec_team_headers_.push_back(str_spectator_team);

    ptr_player_panel->AddChild(std::move(up_spectator_header));

    for (net::SessionClient* ptr_client : vec_spectators) {
      auto up_item = up_spectator_template_->Clone();
      SetupProfileWidget(*up_item, ptr_client, ptr_order_manager_,
                         ptr_world_renderer);

      auto* ptr_name_label =
          up_item->Get<ora::ui::LabelWithTooltipWidget>("NAME");
      ora::gfx::SpriteFont& font_name =
          ora::ui::RequireWidgetFont(ptr_name_label->str_font);

      const std::string str_name = ora::ui::WithSuffix(
          ptr_client->Name, sim::WinState::Undefined, ptr_client->State);
      const std::string str_truncated = ora::ui::TruncateText(
          str_name, ptr_name_label->bounds_widget.int4_width, font_name);
      if (str_name != str_truncated)
        ptr_name_label->fn_get_tooltip_text = [str_name] { return str_name; };
      else
        ptr_name_label->fn_get_tooltip_text = nullptr;
      ptr_name_label->fn_get_text = [str_truncated] { return str_truncated; };
      vec_spectator_rows_.push_back(ptr_client->Name);

      auto* ptr_kick_button = up_item->Get<ora::ui::ButtonWidget>("KICK");
      const bool b_local_can_kick =
          ptr_local_client != nullptr &&
          (b_is_host ||
           (!ptr_local_client->IsObserver() &&
            ptr_local_player_by_client != nullptr &&
            ptr_local_player_by_client->PlayerWinState() ==
                sim::WinState::Undefined));
      ptr_kick_button->fn_is_visible =
          [this, ptr_client, b_local_can_kick] {
            const bool b_client_kickable =
                ptr_client->Index !=
                    (ptr_order_manager_->LocalClient() != nullptr
                         ? ptr_order_manager_->LocalClient()->Index
                         : -1) &&
                ptr_client->State != net::ClientState::Disconnected &&
                (!ptr_client->IsAdmin ||
                 ptr_order_manager_->LobbyInfo().global_settings.Dedicated) &&
                (!ora::ui::Ui::Instance().IsHostDeps() ||
                 !net::KickVoteTarget().has_value() ||
                 net::KickVoteTarget() == ptr_client->Index);
            return b_local_can_kick && b_client_kickable;
          };
      ptr_kick_button->fn_on_click = [this, ptr_client] {
        KickAction(*ptr_client, [] {
          return !ora::ui::Ui::Instance().IsHostDeps();
        });
      };
      ptr_kick_button->fn_get_tooltip_text =
          [str_kick_tooltip, str_vote_kick_tooltip] {
            const bool b_vote = !ora::ui::Ui::Instance().IsHostDeps();
            return b_vote ? str_vote_kick_tooltip : str_kick_tooltip;
          };

      auto* ptr_mute_checkbox = up_item->Get<ora::ui::CheckboxWidget>("MUTE");
      ptr_mute_checkbox->fn_is_checked = [ptr_client] {
        const auto& map_muted =
            net::TextNotificationsManager::Instance().MutedPlayers();
        const auto it_muted = map_muted.find(ptr_client->Index);
        return it_muted != map_muted.end() && it_muted->second;
      };
      ptr_mute_checkbox->fn_on_click = [ptr_client] {
        auto& map_muted =
            net::TextNotificationsManager::Instance().MutedPlayers();
        map_muted[ptr_client->Index] = !map_muted[ptr_client->Index];
      };
      ptr_mute_checkbox->fn_is_visible = [this, ptr_client] {
        return !ptr_client->IsBot() &&
               ptr_client->State != net::ClientState::Disconnected &&
               ptr_client->Index !=
                   (ptr_order_manager_->LocalClient() != nullptr
                        ? ptr_order_manager_->LocalClient()->Index
                        : -1);
      };
      ptr_mute_checkbox->fn_get_tooltip_text =
          [ptr_mute_checkbox, str_unmute_tooltip, str_mute_tooltip] {
            return ptr_mute_checkbox->fn_is_checked() ? str_unmute_tooltip
                                                      : str_mute_tooltip;
          };

      ptr_player_panel->AddChild(std::move(up_item));
    }
  }
}

void GameInfoStatsLogic::KickAction(
    net::SessionClient& client, std::function<bool()> fn_is_vote_kick) {
  if (fn_hide_menu_)
    fn_hide_menu_(true);
  if (fn_is_vote_kick()) {
    int int4_bots_count = 0;
    if (client.IsAdmin)
      for (const sim::Player* ptr_player : ptr_world_->Players())
        if (ptr_player->IsBot() &&
            ptr_player->PlayerWinState() == sim::WinState::Undefined)
          ++int4_bots_count;

    const std::vector<std::pair<std::string, fluent::FluentValue>>
        vec_title_arguments{{std::string{"player"},
                             fluent::FluentValue{client.Name}}};
    const std::vector<std::pair<std::string, fluent::FluentValue>>
        vec_text_arguments{{std::string{"bots"},
                            fluent::FluentValue{
                                static_cast<std::int64_t>(int4_bots_count)}}};

    const std::string str_text_key = int4_bots_count > 0
                                         ? "dialog-vote-kick.prompt-break-bots"
                                         : "dialog-vote-kick.prompt";

    if (!net::KickVoteTarget().has_value()) {
      ButtonPrompt(
          "dialog-vote-kick.title", str_text_key,
          [this, int4_index = client.Index] {
            ptr_order_manager_->IssueOrder(net::Order::Command(std::format(
                "vote_kick {} {}", int4_index, true)));
            if (fn_hide_menu_)
              fn_hide_menu_(false);
            if (fn_close_menu_)
              fn_close_menu_();
          },
          "dialog-vote-kick.vote-start",
          [this] {
            if (fn_hide_menu_)
              fn_hide_menu_(false);
          },
          {}, nullptr, {}, vec_text_arguments, vec_title_arguments);
      return;
    }

    ButtonPrompt(
        "dialog-vote-kick.title", str_text_key,
        [this, int4_index = client.Index] {
          ptr_order_manager_->IssueOrder(net::Order::Command(std::format(
              "vote_kick {} {}", int4_index, true)));
          if (fn_hide_menu_)
            fn_hide_menu_(false);
          if (fn_close_menu_)
            fn_close_menu_();
        },
        "dialog-vote-kick.vote-for",
        [this] {
          if (fn_hide_menu_)
            fn_hide_menu_(false);
        },
        "dialog-vote-kick.vote-cancel",
        [this, int4_index = client.Index] {
          ora::ui::Ui::Instance().CloseWindow();
          ptr_order_manager_->IssueOrder(net::Order::Command(std::format(
              "vote_kick {} {}", int4_index, false)));
          if (fn_hide_menu_)
            fn_hide_menu_(false);
          if (fn_close_menu_)
            fn_close_menu_();
        },
        "dialog-vote-kick.vote-against", vec_text_arguments,
        vec_title_arguments);
  } else {
    ButtonPrompt(
        "dialog-kick.title", "dialog-kick.prompt",
        [this, int4_index = client.Index] {
          ptr_order_manager_->IssueOrder(net::Order::Command(std::format(
              "kick {} {}", int4_index, false)));
          if (fn_hide_menu_)
            fn_hide_menu_(false);
        },
        "dialog-kick.confirm",
        [this] {
          if (fn_hide_menu_)
            fn_hide_menu_(false);
        },
        {}, nullptr, {},
        {{std::string{"player"}, fluent::FluentValue{client.Name}}});
  }
}

void RegisterGameInfoLogics() {
  auto& registry_logics = ora::ui::ChromeLogicRegistry::Instance();

  registry_logics.Register(
      "GameInfoLogic",
      [](ora::ui::WidgetArgs& args_args)
          -> std::unique_ptr<ora::ui::ChromeLogic> {
        IngameInfoPanel initial_panel = IngameInfoPanel::AutoSelect;
        if (const auto* ptr_value = std::get_if<std::int32_t>(
                &args_args.At("initialPanel")))
          initial_panel = static_cast<IngameInfoPanel>(*ptr_value);

        std::function<void(bool)> fn_hide_menu;
        if (const auto* ptr_value =
                std::get_if<std::function<void(bool)>>(
                    &args_args.At("hideMenu")))
          fn_hide_menu = *ptr_value;

        std::function<void()> fn_close_menu;
        if (const auto* ptr_value =
                std::get_if<std::function<void()>>(&args_args.At("closeMenu")))
          fn_close_menu = *ptr_value;

        return std::make_unique<GameInfoLogic>(
            *std::get<ora::ui::Widget*>(args_args.At("widget")),
            *std::get<game::ModData*>(args_args.At("modData")),
            *std::get<sim::World*>(args_args.At("world")), initial_panel,
            std::move(fn_hide_menu), std::move(fn_close_menu));
      });

  registry_logics.Register(
      "GameInfoBriefingLogic",
      [](ora::ui::WidgetArgs& args_args)
          -> std::unique_ptr<ora::ui::ChromeLogic> {
        return std::make_unique<GameInfoBriefingLogic>(
            *std::get<ora::ui::Widget*>(args_args.At("widget")),
            *std::get<game::ModData*>(args_args.At("modData")),
            *std::get<sim::World*>(args_args.At("world")));
      });

  registry_logics.Register(
      "GameInfoObjectivesLogic",
      [](ora::ui::WidgetArgs& args_args)
          -> std::unique_ptr<ora::ui::ChromeLogic> {
        return std::make_unique<GameInfoObjectivesLogic>(
            *std::get<ora::ui::Widget*>(args_args.At("widget")),
            *std::get<sim::World*>(args_args.At("world")));
      });

  registry_logics.Register(
      "GameInfoStatsLogic",
      [](ora::ui::WidgetArgs& args_args)
          -> std::unique_ptr<ora::ui::ChromeLogic> {
        std::function<void(bool)> fn_hide_menu;
        if (const auto* ptr_value =
                std::get_if<std::function<void(bool)>>(
                    &args_args.At("hideMenu")))
          fn_hide_menu = *ptr_value;

        std::function<void()> fn_close_menu;
        if (const auto* ptr_value =
                std::get_if<std::function<void()>>(&args_args.At("closeMenu")))
          fn_close_menu = *ptr_value;

        gfx::WorldRenderer* ptr_world_renderer = nullptr;
        if (const auto* ptr_value = std::get_if<gfx::WorldRenderer*>(
                &args_args.At("worldRenderer")))
          ptr_world_renderer = *ptr_value;

        return std::make_unique<GameInfoStatsLogic>(
            *std::get<ora::ui::Widget*>(args_args.At("widget")),
            *std::get<game::ModData*>(args_args.At("modData")),
            *std::get<sim::World*>(args_args.At("world")),
            *std::get<net::OrderManager*>(args_args.At("orderManager")),
            ptr_world_renderer, std::move(fn_hide_menu),
            std::move(fn_close_menu));
      });
}

}  // namespace ora::mods
