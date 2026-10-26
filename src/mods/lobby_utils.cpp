// UPSTREAM: OpenRA.Mods.Common/Widgets/Logic/Lobby/LobbyUtils.cs
//          @1f594e320(实现;头注见 lobby_utils.hpp)
//          Implementation; the header notes live in lobby_utils.hpp.
#include "mods/lobby_utils.hpp"

#include "fluent/fluent_provider.hpp"
#include "game/actor_info.hpp"
#include "game/mod_data.hpp"
#include "gfx/chrome_provider.hpp"
#include "gfx/sprite.hpp"
#include "map/map_cache.hpp"
#include "mods/browser_logics.hpp"
#include "net/order.hpp"
#include "net/order_manager.hpp"
#include "net/sanitize.hpp"
#include "net/session.hpp"
#include "sim/trait_registry.hpp"
#include "ui/ui.hpp"
#include "ui/widget_utils.hpp"
#include "ui/widgets/checkbox_widget.hpp"
#include "ui/widgets/client_tooltip_region_widget.hpp"
#include "ui/widgets/color_block_widget.hpp"
#include "ui/widgets/drop_down_button_widget.hpp"
#include "ui/widgets/image_widget.hpp"
#include "ui/widgets/label_widget.hpp"
#include "ui/widgets/map_preview_widget.hpp"
#include "ui/widgets/scroll_item_widget.hpp"
#include "ui/widgets/text_field_widget.hpp"

namespace ora::mods {
namespace {

fluent::FluentProvider& Fluent() {
  return fluent::FluentProvider::Instance();
}

std::string Msg(std::string_view str_key) {
  return Fluent().GetMessage(str_key);
}

struct SlotDropDownOption {
  std::string str_title;
  std::string str_order;
  std::function<bool()> fn_selected;
};

struct LobbyDropDownOption {
  std::string str_title;
  std::function<bool()> fn_is_selected = [] { return false; };
  std::function<void()> fn_on_click;
};

std::string TrimWhitespace(const std::string& str_input) {
  std::size_t sz_begin = 0;
  std::size_t sz_end = str_input.size();
  while (sz_begin < sz_end &&
         (str_input[sz_begin] == ' ' || str_input[sz_begin] == '\t' ||
          str_input[sz_begin] == '\r' || str_input[sz_begin] == '\n'))
    ++sz_begin;
  while (sz_end > sz_begin &&
         (str_input[sz_end - 1] == ' ' || str_input[sz_end - 1] == '\t' ||
          str_input[sz_end - 1] == '\r' || str_input[sz_end - 1] == '\n'))
    --sz_end;
  return str_input.substr(sz_begin, sz_end - sz_begin);
}

std::string SpawnLabel(int int4_spawn) {
  return int4_spawn == 0 ? std::string{"-"}
                         : std::string(1, static_cast<char>('A' - 1 +
                                                             int4_spawn));
}

bool DisabledSpawnContains(const net::Session& session_lobby,
                           int int4_spawn) {
  for (int int4_disabled : session_lobby.vec_disabled_spawn_points)
    if (int4_disabled == int4_spawn)
      return true;
  return false;
}

void SetSpawnPoint(net::OrderManager& order_manager,
                   net::SessionClient* ptr_player_to_move,
                   int int4_selected_spawn_point) {
  bool b_owned = false;
  for (const auto& client : order_manager.LobbyInfo().vec_clients)
    if (client.SpawnPoint == int4_selected_spawn_point) {
      b_owned = true;
      break;
    }
  if (!b_owned)
    b_owned = DisabledSpawnContains(order_manager.LobbyInfo(),
                                    int4_selected_spawn_point);

  if (int4_selected_spawn_point == 0 || !b_owned) {
    const int int4_index = ptr_player_to_move != nullptr
                               ? ptr_player_to_move->Index
                               : order_manager.LocalClient()->Index;
    order_manager.IssueOrder(net::Order::Command(
        std::format("spawn {} {}", int4_index, int4_selected_spawn_point)));
  }
}

int DetermineSelectedSpawnPoint(ora::ui::MapPreviewWidget& map_preview,
                                ora::map::MapPreview& preview,
                                const MouseInput& mi_input) {
  if (preview.HideSpawnPreviews())
    return 0;

  gfx::ChromeProvider* ptr_chrome = ora::ui::ChromeProviderOrNull();
  if (ptr_chrome == nullptr)
    return 0;

  const gfx::Sprite& sprite_spawn =
      ptr_chrome->GetImage("lobby-bits", "spawn-unclaimed");
  const core::Vector2 vec_spawn_size{sprite_spawn.vec_size.X,
                                     sprite_spawn.vec_size.Y};

  const auto& vec_spawn_points = preview.SpawnPoints();
  for (std::size_t sz_index = 0; sz_index < vec_spawn_points.size();
       ++sz_index) {
    const int2 point_preview = map_preview.ConvertToPreview(
        vec_spawn_points[sz_index],
        preview.GridType().value_or(MapGridType::Rectangular));
    const float fp_dx = static_cast<float>(point_preview.X -
                                           mi_input.Location.X) *
                        2.0f / vec_spawn_size.X;
    const float fp_dy = static_cast<float>(point_preview.Y -
                                           mi_input.Location.Y) *
                        2.0f / vec_spawn_size.Y;
    if (fp_dx * fp_dx + fp_dy * fp_dy <= 1.0f)
      return static_cast<int>(sz_index) + 1;
  }

  return 0;
}

void SelectPlayerSpawnPoint(net::OrderManager& order_manager,
                            ora::ui::MapPreviewWidget& map_preview,
                            ora::map::MapPreview& preview,
                            const MouseInput& mi_input) {
  const int int4_selected_spawn =
      DetermineSelectedSpawnPoint(map_preview, preview, mi_input);

  const net::SessionClient* ptr_local = order_manager.LocalClient();
  const bool b_is_host = ora::ui::Ui::Instance().IsHostDeps();
  net::SessionClient* ptr_player_to_move = nullptr;
  for (auto& client : order_manager.LobbyInfo().vec_clients) {
    const bool b_local = ptr_local != nullptr &&
                         (client.Index == ptr_local->Index ||
                          (b_is_host && !client.Bot.empty()));
    if (!b_local)
      continue;

    if (((int4_selected_spawn == 0) != (client.SpawnPoint == 0)) &&
        !client.IsObserver()) {
      ptr_player_to_move = &client;
      break;
    }
  }

  SetSpawnPoint(order_manager, ptr_player_to_move, int4_selected_spawn);
}

void ClearPlayerSpawnPoint(net::OrderManager& order_manager,
                           ora::ui::MapPreviewWidget& map_preview,
                           ora::map::MapPreview& preview,
                           const MouseInput& mi_input) {
  const int int4_selected_spawn =
      DetermineSelectedSpawnPoint(map_preview, preview, mi_input);

  net::SessionClient* ptr_occupant = nullptr;
  for (auto& client : order_manager.LobbyInfo().vec_clients)
    if (client.SpawnPoint == int4_selected_spawn) {
      ptr_occupant = &client;
      break;
    }

  if (ora::ui::Ui::Instance().IsHostDeps() ||
      ptr_occupant == order_manager.LocalClient())
    order_manager.IssueOrder(net::Order::Command(
        std::format("clear_spawn {}", int4_selected_spawn)));
}

}  // namespace

void ShowSlotDropDown(ora::ui::DropDownButtonWidget& dropdown,
                      const ora::net::SessionSlot& slot,
                      const ora::net::SessionClient* ptr_client,
                      ora::net::OrderManager& order_manager,
                      const ora::map::MapPreview& map_preview,
                      ora::game::ModData& mod_data) {
  const std::string str_open = Msg("options-lobby-slot.open");
  const std::string str_closed = Msg("options-lobby-slot.closed");

  std::vector<SlotDropDownOption> vec_slot_options;
  vec_slot_options.push_back(SlotDropDownOption{
      str_open, "slot_open " + slot.PlayerReference,
      [ptr_client, &slot] { return !slot.Closed && ptr_client == nullptr; }});
  vec_slot_options.push_back(SlotDropDownOption{
      str_closed, "slot_close " + slot.PlayerReference,
      [&slot] { return slot.Closed; }});

  std::vector<std::pair<std::string, std::vector<SlotDropDownOption>>>
      vec_groups;
  vec_groups.emplace_back(Msg("options-lobby-slot.slot"),
                          std::move(vec_slot_options));

  std::vector<SlotDropDownOption> vec_bots;
  if (slot.AllowBots) {
    if (const auto ptr_player_actor_info = map_preview.PlayerActorInfo())
      for (const meta::RecordObject* rec_bot :
           ptr_player_actor_info->TraitInfosByInterface("IBotInfo")) {
        const auto sv_name = sim::RecordFieldString(*rec_bot, "Name");
        const auto sv_type = sim::RecordFieldString(*rec_bot, "Type");
        if (!sv_name || !sv_type)
          continue;

        const net::SessionClient* ptr_bot_controller = nullptr;
        for (const auto& client_admin :
             order_manager.LobbyInfo().vec_clients)
          if (client_admin.IsAdmin) {
            ptr_bot_controller =
                order_manager.LobbyInfo().ClientWithIndex(client_admin.Index);
            break;
          }

        const int int4_controller_index =
            ptr_bot_controller != nullptr ? ptr_bot_controller->Index : 0;
        const std::string str_type{*sv_type};
        vec_bots.push_back(SlotDropDownOption{
            map_preview.GetMessage(*sv_name),
            std::format("slot_bot {} {} {}", slot.PlayerReference,
                        int4_controller_index, str_type),
            [ptr_client, str_type] {
              return ptr_client != nullptr && ptr_client->Bot == str_type;
            }});
      }
  }

  vec_groups.emplace_back(
      !vec_bots.empty() ? Msg("options-lobby-slot.bots")
                        : Msg("options-lobby-slot.bots-disabled"),
      std::move(vec_bots));

  dropdown.ShowDropDown<SlotDropDownOption>(
      "LABEL_DROPDOWN_TEMPLATE", 180, vec_groups,
      [&order_manager](const SlotDropDownOption& option,
                       ora::ui::ScrollItemWidget& item_template) {
        auto up_item = ora::ui::ScrollItemWidget::Setup(
            item_template, option.fn_selected,
            [&order_manager, str_order = option.str_order] {
              order_manager.IssueOrder(
                  net::Order::Command(str_order));
            });
        up_item->Get<ora::ui::LabelWidget>("LABEL")->fn_get_text =
            [str_title = option.str_title] { return str_title; };
        return up_item;
      });
  (void)mod_data;
}

void ShowPlayerActionDropDown(ora::ui::DropDownButtonWidget& dropdown,
                              ora::net::SessionClient& client,
                              ora::net::OrderManager& order_manager,
                              ora::ui::Widget& lobby,
                              std::function<void()> fn_before,
                              std::function<void()> fn_after) {
  auto fn_ok_pressed =
      [&order_manager, &client,
       fn_after](bool b_temp_ban) {
        order_manager.IssueOrder(net::Order::Command(
            std::format("kick {} {}", client.Index, b_temp_ban ? "True" : "False")));
        fn_after();
      };

  std::vector<LobbyDropDownOption> vec_options;
  vec_options.push_back(LobbyDropDownOption{
      "Kick",
      [] { return false; },
      [&lobby, &client, fn_before, fn_ok_pressed,
       fn_after = std::move(fn_after)]() mutable {
        fn_before();

        ora::ui::WidgetArgs args_dialog;
        args_dialog.Add("clientName", client.Name);
        args_dialog.Add("okPressed", std::function<void(bool)>{fn_ok_pressed});
        args_dialog.Add("cancelPressed", fn_after);
        ora::ui::Ui::Instance().LoadWidget(
            "KICK_CLIENT_DIALOG", lobby.Get("TOP_PANELS_ROOT"),
            std::move(args_dialog));
      }});

  if (order_manager.LobbyInfo().global_settings.Dedicated) {
    vec_options.push_back(LobbyDropDownOption{
        "Transfer Admin",
        [] { return false; },
        [&order_manager, &client] {
          order_manager.IssueOrder(net::Order::Command(
              std::format("make_admin {}", client.Index)));
        }});
  }

  if (!client.IsObserver() &&
      order_manager.LobbyInfo().global_settings.AllowSpectators) {
    vec_options.push_back(LobbyDropDownOption{
        "Move to Spectator",
        [] { return false; },
        [&order_manager, &client] {
          order_manager.IssueOrder(net::Order::Command(
              std::format("make_spectator {}", client.Index)));
        }});
  }

  dropdown.ShowDropDown<LobbyDropDownOption>(
      "PLAYERACTION_DROPDOWN_TEMPLATE", 167, vec_options,
      [](const LobbyDropDownOption& option,
         ora::ui::ScrollItemWidget& item_template) {
        auto up_item = ora::ui::ScrollItemWidget::Setup(
            item_template, option.fn_is_selected, option.fn_on_click);
        up_item->Get<ora::ui::LabelWidget>("LABEL")->fn_get_text =
            [str_title = option.str_title] { return str_title; };
        return up_item;
      });
}

void ShowTeamDropDown(ora::ui::DropDownButtonWidget& dropdown,
                      ora::net::SessionClient& client,
                      ora::net::OrderManager& order_manager,
                      int int4_team_count) {
  std::vector<int> vec_options;
  for (int int4_team = 0; int4_team <= int4_team_count; ++int4_team)
    vec_options.push_back(int4_team);

  dropdown.ShowDropDown<int>(
      "TEAM_DROPDOWN_TEMPLATE", 150, vec_options,
      [&order_manager,
       &client](int int4_team, ora::ui::ScrollItemWidget& item_template) {
        auto up_item = ora::ui::ScrollItemWidget::Setup(
            item_template,
            [&client, int4_team] { return client.Team == int4_team; },
            [&order_manager, &client, int4_team] {
              order_manager.IssueOrder(net::Order::Command(
                  std::format("team {} {}", client.Index, int4_team)));
            });
        up_item->Get<ora::ui::LabelWidget>("LABEL")->fn_get_text =
            [int4_team] {
              return int4_team == 0 ? std::string{"-"}
                                    : std::to_string(int4_team);
            };
        return up_item;
      });
}

void ShowHandicapDropDown(ora::ui::DropDownButtonWidget& dropdown,
                          ora::net::SessionClient& client,
                          ora::net::OrderManager& order_manager) {
  std::vector<int> vec_options;
  for (int int4_step = 0; int4_step < 20; ++int4_step)
    vec_options.push_back(5 * int4_step);

  dropdown.ShowDropDown<int>(
      "TEAM_DROPDOWN_TEMPLATE", 150, vec_options,
      [&order_manager,
       &client](int int4_handicap, ora::ui::ScrollItemWidget& item_template) {
        auto up_item = ora::ui::ScrollItemWidget::Setup(
            item_template,
            [&client, int4_handicap] {
              return client.Handicap == int4_handicap;
            },
            [&order_manager, &client, int4_handicap] {
              order_manager.IssueOrder(net::Order::Command(
                  std::format("handicap {} {}", client.Index, int4_handicap)));
            });
        up_item->Get<ora::ui::LabelWidget>("LABEL")->fn_get_text =
            [int4_handicap] { return std::format("{}%", int4_handicap); };
        return up_item;
      });
}

void ShowSpawnDropDown(ora::ui::DropDownButtonWidget& dropdown,
                       ora::net::SessionClient& client,
                       ora::net::OrderManager& order_manager,
                       const std::vector<int>& vec_spawn_points) {
  dropdown.ShowDropDown<int>(
      "SPAWN_DROPDOWN_TEMPLATE", 150, vec_spawn_points,
      [&order_manager,
       &client](int int4_spawn, ora::ui::ScrollItemWidget& item_template) {
        auto up_item = ora::ui::ScrollItemWidget::Setup(
            item_template,
            [&client, int4_spawn] { return client.SpawnPoint == int4_spawn; },
            [&order_manager, &client, int4_spawn] {
              SetSpawnPoint(order_manager, &client, int4_spawn);
            });
        up_item->Get<ora::ui::LabelWidget>("LABEL")->fn_get_text =
            [int4_spawn] { return SpawnLabel(int4_spawn); };
        return up_item;
      });
}

const LobbyFaction* FindLobbyFaction(
    const std::vector<std::pair<std::string, LobbyFaction>>& vec_factions,
    std::string_view str_id) {
  for (const auto& [str_key, faction] : vec_factions)
    if (str_key == str_id)
      return &faction;
  return nullptr;
}

void ShowFactionDropDown(ora::ui::DropDownButtonWidget& dropdown,
                         ora::net::SessionClient& client,
                         ora::net::OrderManager& order_manager,
                         const std::vector<std::pair<std::string, LobbyFaction>>&
                             vec_factions) {
  std::vector<std::pair<std::string, std::vector<std::string>>> vec_groups;
  for (const auto& [str_faction_id, faction] : vec_factions) {
    if (!faction.Selectable)
      continue;

    const std::string str_group =
        !faction.Side.empty() ? Msg(faction.Side) : std::string{};
    bool b_found = false;
    for (auto& [str_key, vec_members] : vec_groups)
      if (str_key == str_group) {
        vec_members.push_back(Msg(str_faction_id));
        b_found = true;
        break;
      }
    if (!b_found)
      vec_groups.emplace_back(str_group,
                              std::vector<std::string>{Msg(str_faction_id)});
  }

  dropdown.ShowDropDown<std::string>(
      "FACTION_DROPDOWN_TEMPLATE", 154, vec_groups,
      [&order_manager, &client,
       &vec_factions](const std::string& str_faction_id,
                      ora::ui::ScrollItemWidget& item_template) {
        auto up_item = ora::ui::ScrollItemWidget::Setup(
            item_template,
            [&client, str_faction_id] {
              return client.Faction == str_faction_id;
            },
            [&order_manager, &client, str_faction_id] {
              order_manager.IssueOrder(net::Order::Command(
                  std::format("faction {} {}", client.Index,
                              str_faction_id)));
            });

        const LobbyFaction* ptr_faction =
            FindLobbyFaction(vec_factions, str_faction_id);
        const std::string str_faction_name =
            ptr_faction != nullptr ? Msg(ptr_faction->Name) : std::string{};

        auto* ptr_label = up_item->GetOrNull<ora::ui::LabelWidget>("LABEL");
        const std::string str_label_text = ora::ui::TruncateText(
            str_faction_name, ptr_label->bounds_widget.int4_width,
            ora::ui::RequireWidgetFont(ptr_label->str_font));
        ptr_label->fn_get_text = [str_label_text] { return str_label_text; };

        auto* ptr_flag = up_item->Get<ora::ui::ImageWidget>("FLAG");
        ptr_flag->fn_get_image_collection = [] { return "flags"; };
        ptr_flag->fn_get_image_name =
            [str_faction_id] { return str_faction_id; };

        const std::string str_description =
            ptr_faction != nullptr && !ptr_faction->Description.empty()
                ? Msg(ptr_faction->Description)
                : std::string{};
        const auto [opt_text, opt_desc] = SplitOnFirstToken(str_description);
        up_item->fn_get_tooltip_text = [opt_text] {
          return opt_text.value_or(std::string{});
        };
        up_item->fn_get_tooltip_desc = [opt_desc] {
          return opt_desc.value_or(std::string{});
        };

        return up_item;
      });
}

void SelectSpawnPoint(ora::net::OrderManager& order_manager,
                      ora::ui::MapPreviewWidget& map_preview,
                      ora::map::MapPreview& preview,
                      const MouseInput& mi_input) {
  if (order_manager.LocalClient()->State == net::ClientState::Ready)
    return;

  if (mi_input.Button == MouseButton::Left)
    SelectPlayerSpawnPoint(order_manager, map_preview, preview, mi_input);

  if (mi_input.Button == MouseButton::Right)
    ClearPlayerSpawnPoint(order_manager, map_preview, preview, mi_input);
}

std::vector<int> AvailableSpawnPoints(int int4_spawn_points,
                                      const ora::net::Session& lobby_info) {
  std::vector<int> vec_out;
  for (int int4_spawn = 1; int4_spawn <= int4_spawn_points; ++int4_spawn)
    if (!DisabledSpawnContains(lobby_info, int4_spawn))
      vec_out.push_back(int4_spawn);
  return vec_out;
}

bool InsufficientEnabledSpawnPoints(
    const ora::map::MapPreview& map_preview,
    const ora::net::Session& lobby_info) {
  const std::size_t sz_spawn_points = map_preview.SpawnPoints().size();
  if (sz_spawn_points == 0)
    return false;

  std::size_t sz_players = 0;
  for (const auto& client : lobby_info.vec_clients)
    if (!client.IsObserver())
      ++sz_players;

  return AvailableSpawnPoints(static_cast<int>(sz_spawn_points), lobby_info)
             .size() < sz_players;
}

core::Color LatencyColor(const ora::net::SessionClient* ptr_client) {
  if (ptr_client == nullptr)
    return core::Color::FromArgb(255, 128, 128, 128);

  switch (ptr_client->ConnectionQualityKind) {
    case ora::net::ConnectionQuality::Good:
      return core::Color::FromArgb(255, 50, 205, 50);
    case ora::net::ConnectionQuality::Moderate:
      return core::Color::FromArgb(255, 255, 165, 0);
    case ora::net::ConnectionQuality::Poor:
      return core::Color::FromArgb(255, 255, 0, 0);
    default:
      return core::Color::FromArgb(255, 128, 128, 128);
  }
}

std::string LatencyDescription(const ora::net::SessionClient* ptr_client) {
  if (ptr_client == nullptr)
    return "Unknown";

  switch (ptr_client->ConnectionQualityKind) {
    case ora::net::ConnectionQuality::Good:
      return "Good";
    case ora::net::ConnectionQuality::Moderate:
      return "Moderate";
    case ora::net::ConnectionQuality::Poor:
      return "Poor";
    default:
      return "Unknown";
  }
}

void SetupLatencyWidget(ora::ui::Widget& parent,
                        ora::net::SessionClient* ptr_client,
                        ora::net::OrderManager& order_manager) {
  const bool b_visible = ptr_client != nullptr && ptr_client->Bot.empty();
  if (auto* ptr_block = parent.GetOrNull("LATENCY")) {
    ptr_block->fn_is_visible = [b_visible] { return b_visible; };

    if (b_visible)
      parent.Get<ora::ui::ColorBlockWidget>("LATENCY_COLOR")
          ->fn_get_color = [ptr_client] { return LatencyColor(ptr_client); };
  }

  auto& tooltip_region =
       *parent.Get<ora::ui::ClientTooltipRegionWidget>("LATENCY_REGION");
  tooltip_region.fn_is_visible = [b_visible] { return b_visible; };
  if (b_visible)
    tooltip_region.Bind(&order_manager, nullptr, ptr_client);
}

void SetupProfileWidget(ora::ui::Widget& widget_parent,
                        ora::net::SessionClient* ptr_client,
                        ora::net::OrderManager* ptr_order_manager,
                        ora::gfx::WorldRenderer* ptr_world_renderer) {
  if (auto* ptr_profile =
          widget_parent.GetOrNull<ora::ui::ImageWidget>("PROFILE")) {
    std::string str_image_name = ptr_client->IsBot()
                                     ? std::string{"bot"}
                                     : ptr_client->IsAdmin
                                           ? std::string{"admin-"}
                                           : std::string{"player-"};

    if (!ptr_client->IsBot())
      str_image_name += !ptr_client->Fingerprint.empty() ? "registered"
                                                         : "anonymous";

    ptr_profile->fn_get_image_name = [str_image_name] {
      return str_image_name;
    };
    ptr_profile->fn_is_visible = [] { return true; };
  }

  if (auto* ptr_profile_tooltip =
          widget_parent.GetOrNull<ora::ui::ClientTooltipRegionWidget>(
              "PROFILE_TOOLTIP")) {
    if (!ptr_client->Fingerprint.empty())
      ptr_profile_tooltip->str_template = "REGISTERED_PLAYER_TOOLTIP";

    if (ptr_client->IsBot())
      ptr_profile_tooltip->str_template = "BOT_TOOLTIP";

    ptr_profile_tooltip->Bind(ptr_order_manager, ptr_world_renderer,
                              ptr_client);

    ptr_profile_tooltip->fn_is_visible = [] { return true; };
  }
}

void SetupEditableNameWidget(ora::ui::Widget& parent,
                             ora::net::SessionClient& client,
                             ora::net::OrderManager& order_manager,
                             ora::gfx::WorldRenderer* ptr_world_renderer) {
  auto& field_name = *parent.Get<ora::ui::TextFieldWidget>("NAME");
  field_name.fn_is_visible = [] { return true; };
  field_name.fn_is_disabled = [&order_manager] {
    return order_manager.LocalClient()->IsReady();
  };

  field_name.SetText(client.Name);
  auto ptr_esc_pressed = std::make_shared<bool>(false);
  field_name.fn_on_lose_focus =
      [&field_name, &client, &order_manager, ptr_esc_pressed]() mutable {
        if (*ptr_esc_pressed) {
          *ptr_esc_pressed = false;
          return;
        }

        std::string str_text = TrimWhitespace(field_name.Text());
        if (str_text.empty())
          field_name.SetText(client.Name);
        else if (str_text != client.Name) {
          str_text = net::SanitizedPlayerName(
              str_text, ora::ui::Ui::Instance().PlayerNameDeps());
          field_name.SetText(str_text);
          order_manager.IssueOrder(
              net::Order::Command("name " + str_text));
          ora::ui::Ui::Instance().PlayerNameSetDeps(str_text);
          ora::ui::Ui::Instance().SettingsSaveDeps();
        } else {
          field_name.SetText(str_text);
        }
      };

  field_name.fn_on_enter_key = [&field_name](const KeyInput&) {
    field_name.YieldKeyboardFocus();
    return true;
  };
  field_name.fn_on_esc_key = [&field_name, &client,
                              ptr_esc_pressed](const KeyInput&) {
    field_name.SetText(client.Name);
    *ptr_esc_pressed = true;
    field_name.YieldKeyboardFocus();
    return true;
  };

  SetupProfileWidget(field_name, &client, &order_manager,
                     ptr_world_renderer);

  HideChildWidget(parent, "SLOT_OPTIONS");
}

void SetupNameWidget(ora::ui::Widget& parent,
                     ora::net::SessionClient& client,
                     ora::net::OrderManager& order_manager,
                     ora::gfx::WorldRenderer* ptr_world_renderer,
                     const ora::map::MapPreview& map_preview) {
  auto& label_name = *parent.Get<ora::ui::LabelWidget>("NAME");
  label_name.fn_is_visible = [] { return true; };
  ora::gfx::SpriteFont& font_name =
      ora::ui::RequireWidgetFont(label_name.str_font);

  auto transform_client_name =
      core::CachedTransform<int, std::string>(
          [&client, &map_preview, &label_name,
           &font_name](int int4_map_status) {
            (void)int4_map_status;
            std::string str_name = client.Name;
            if (client.IsBot() &&
                !map_preview.TryGetMessage(client.Name, str_name))
              str_name = Msg("label-bot-player");

            return ora::ui::TruncateText(
                str_name, label_name.bounds_widget.int4_width, font_name);
          });

  label_name.fn_get_text = [&transform_client_name, &map_preview] {
    return transform_client_name.Update(
        static_cast<int>(map_preview.Status()));
  };

  SetupProfileWidget(parent, &client, &order_manager, ptr_world_renderer);
}

void SetupEditableSlotWidget(ora::ui::Widget& parent,
                             const ora::net::SessionSlot& slot,
                             const ora::net::SessionClient* ptr_client,
                             ora::net::OrderManager& order_manager,
                             const ora::map::MapPreview& map_preview,
                             ora::game::ModData& mod_data) {
  auto& dropdown_slot = *parent.Get<ora::ui::DropDownButtonWidget>(
      "SLOT_OPTIONS");
  dropdown_slot.fn_is_visible = [] { return true; };
  dropdown_slot.fn_is_disabled = [&order_manager] {
    return order_manager.LocalClient()->IsReady();
  };

  auto transform_truncated = core::CachedTransform<std::string, std::string>(
      [&dropdown_slot](const std::string& str_name) {
        return ora::ui::TruncateText(
            str_name,
            dropdown_slot.bounds_widget.int4_width -
                dropdown_slot.bounds_widget.int4_height -
                dropdown_slot.int4_left_margin -
                dropdown_slot.int4_right_margin,
            ora::ui::RequireWidgetFont(dropdown_slot.str_font));
      });

  const std::string str_closed = Msg("options-lobby-slot.closed");
  const std::string str_open = Msg("options-lobby-slot.open");

  auto transform_client_name = core::CachedTransform<int, std::string>(
      [ptr_client, &map_preview](int int4_map_status) {
        (void)int4_map_status;
        if (ptr_client != nullptr && ptr_client->IsBot()) {
          std::string str_message;
          if (map_preview.TryGetMessage(ptr_client->Name, str_message))
            return str_message;
          return Msg("label-bot-player");
        }

        return ptr_client != nullptr ? ptr_client->Name : std::string{};
      });

  dropdown_slot.fn_get_text = [&transform_truncated, &transform_client_name,
                               &map_preview, ptr_client, &slot, str_closed,
                               str_open] {
    return transform_truncated.Update(
        ptr_client != nullptr
            ? transform_client_name.Update(
                  static_cast<int>(map_preview.Status()))
            : (slot.Closed ? str_closed : str_open));
  };

  dropdown_slot.fn_on_mouse_down =
      [&dropdown_slot, &slot, ptr_client, &order_manager, &map_preview,
       &mod_data](const MouseInput&) {
        ShowSlotDropDown(dropdown_slot, slot, ptr_client, order_manager,
                         map_preview, mod_data);
      };

  HideChildWidget(parent, "NAME");
}

void SetupSlotWidget(ora::ui::Widget& parent,
                     ora::game::ModData& mod_data,
                     const ora::net::SessionSlot& slot,
                     const ora::net::SessionClient* ptr_client) {
  auto& label_name = *parent.Get<ora::ui::LabelWidget>("NAME");
  label_name.fn_is_visible = [] { return true; };
  label_name.fn_get_text = [ptr_client, &slot] {
    if (ptr_client != nullptr)
      return ptr_client->Name;
    return slot.Closed ? Msg("options-lobby-slot.closed")
                       : Msg("options-lobby-slot.open");
  };

  HideChildWidget(parent, "SLOT_OPTIONS");
  (void)mod_data;
}

void SetupPlayerActionWidget(ora::ui::Widget& parent,
                             ora::net::SessionClient& client,
                             ora::net::OrderManager& order_manager,
                             ora::gfx::WorldRenderer* ptr_world_renderer,
                             ora::ui::Widget& lobby,
                             std::function<void()> fn_before,
                             std::function<void()> fn_after) {
  auto& dropdown_slot =
       *parent.Get<ora::ui::DropDownButtonWidget>("PLAYER_ACTION");
  dropdown_slot.fn_is_visible = [&order_manager, &client] {
    return ora::ui::Ui::Instance().IsHostDeps() &&
           client.Index != order_manager.LocalClient()->Index;
  };
  dropdown_slot.fn_is_disabled = [&order_manager] {
    return order_manager.LocalClient()->IsReady();
  };

  auto transform_truncated = core::CachedTransform<std::string, std::string>(
      [&dropdown_slot](const std::string& str_name) {
        return ora::ui::TruncateText(
            str_name,
            dropdown_slot.bounds_widget.int4_width -
                dropdown_slot.bounds_widget.int4_height -
                dropdown_slot.int4_left_margin -
                dropdown_slot.int4_right_margin,
            ora::ui::RequireWidgetFont(dropdown_slot.str_font));
      });

  dropdown_slot.fn_get_text = [&transform_truncated, &client] {
    return transform_truncated.Update(client.Name);
  };
  dropdown_slot.fn_on_mouse_down =
      [&dropdown_slot, &client, &order_manager, &lobby, fn_before,
       fn_after](const MouseInput&) {
        ShowPlayerActionDropDown(dropdown_slot, client, order_manager, lobby,
                                 fn_before, fn_after);
      };

  SetupProfileWidget(dropdown_slot, &client, &order_manager,
                     ptr_world_renderer);

  HideChildWidget(parent, "NAME");
}

void SetupKickSpectatorsWidget(ora::ui::Widget& parent,
                               ora::net::OrderManager& order_manager,
                               ora::ui::Widget& lobby,
                               std::function<void()> fn_before,
                               std::function<void()> fn_after,
                               bool b_skirmish_mode) {
  auto& checkbox_toggle =
       *parent.Get<ora::ui::CheckboxWidget>("TOGGLE_SPECTATORS");
  checkbox_toggle.fn_is_checked = [&order_manager] {
    return order_manager.LobbyInfo().global_settings.AllowSpectators;
  };
  checkbox_toggle.fn_is_visible = [&order_manager, b_skirmish_mode] {
    return order_manager.LocalClient()->IsAdmin && !b_skirmish_mode;
  };
  checkbox_toggle.fn_is_disabled = [] { return false; };

  auto fn_ok_pressed = [&order_manager, fn_after]() {
    order_manager.IssueOrder(net::Order::Command(std::format(
        "allow_spectators {}",
        order_manager.LobbyInfo().global_settings.AllowSpectators ? "False"
                                                                  : "True")));
    std::vector<net::Order> vec_kick_orders;
    for (const auto& client : order_manager.LobbyInfo().vec_clients)
      if (client.IsObserver() && !client.IsAdmin)
        vec_kick_orders.push_back(net::Order::Command(
            std::format("kick {} {}", client.Index, client.Name)));
    order_manager.IssueOrders(vec_kick_orders);

    fn_after();
  };

  checkbox_toggle.fn_on_click =
      [&order_manager, &lobby, fn_before, fn_after,
       fn_ok_pressed]() mutable {
        fn_before();

        int int4_spectator_count = 0;
        for (const auto& client : order_manager.LobbyInfo().vec_clients)
          if (client.IsObserver())
            ++int4_spectator_count;

        if (int4_spectator_count > 0) {
          ora::ui::WidgetArgs args_dialog;
          args_dialog.Add("clientCount", int4_spectator_count);
          args_dialog.Add("okPressed",
                          std::function<void()>{fn_ok_pressed});
          args_dialog.Add("cancelPressed", fn_after);
          ora::ui::Ui::Instance().LoadWidget(
              "KICK_SPECTATORS_DIALOG", lobby.Get("TOP_PANELS_ROOT"),
              std::move(args_dialog));
        } else {
          order_manager.IssueOrder(net::Order::Command(std::format(
              "allow_spectators {}",
              order_manager.LobbyInfo().global_settings.AllowSpectators
                  ? "False"
                  : "True")));
          fn_after();
        }
      };
}

void SetupEditableColorWidget(ora::ui::Widget& parent,
                              const ora::net::SessionSlot* ptr_slot,
                              ora::net::SessionClient& client,
                              ora::net::OrderManager& order_manager,
                              ora::gfx::WorldRenderer* ptr_world_renderer) {
  auto& dropdown_color =
       *parent.Get<ora::ui::DropDownButtonWidget>("COLOR");
  dropdown_color.fn_is_disabled = [ptr_slot, &order_manager] {
    return (ptr_slot != nullptr && ptr_slot->LockColor) ||
           order_manager.LocalClient()->IsReady();
  };
  dropdown_color.fn_on_mouse_down =
      [&dropdown_color, &client, &order_manager,
       ptr_world_renderer](const MouseInput&) {
        ora::ui::Ui::Instance().ShowColorDropDownDeps(
            dropdown_color, client.Color, &client.Faction,
            ptr_world_renderer,
            [&order_manager, &client](core::Color color_new) {
              if (&client == order_manager.LocalClient()) {
                ora::ui::Ui::Instance().PlayerColorSetDeps(color_new);
                ora::ui::Ui::Instance().SettingsSaveDeps();
              }

              order_manager.IssueOrder(net::Order::Command(std::format(
                  "color {} {}", client.Index, color_new.ToString())));
            });
      };

  SetupColorWidget(dropdown_color, client);
}

void SetupColorWidget(ora::ui::Widget& parent,
                      ora::net::SessionClient& client) {
  parent.Get<ora::ui::ColorBlockWidget>("COLORBLOCK")->fn_get_color =
      [&client] { return client.Color; };
}

void SetupEditableFactionWidget(
    ora::ui::Widget& parent,
    const ora::net::SessionSlot& slot,
    ora::net::SessionClient& client,
    ora::net::OrderManager& order_manager,
    const std::vector<std::pair<std::string, LobbyFaction>>& vec_factions) {
  auto& dropdown_faction =
       *parent.Get<ora::ui::DropDownButtonWidget>("FACTION");
  dropdown_faction.fn_is_disabled = [&slot, &order_manager] {
    return slot.LockFaction || order_manager.LocalClient()->IsReady();
  };
  dropdown_faction.fn_on_mouse_down =
      [&dropdown_faction, &client, &order_manager,
       &vec_factions](const MouseInput&) {
        ShowFactionDropDown(dropdown_faction, client, order_manager,
                            vec_factions);
      };

  const LobbyFaction* ptr_faction =
      FindLobbyFaction(vec_factions, client.Faction);
  const std::string str_description =
      ptr_faction != nullptr && !ptr_faction->Description.empty()
          ? Msg(ptr_faction->Description)
          : std::string{};
  const auto [opt_text, opt_desc] = SplitOnFirstToken(str_description);
  dropdown_faction.fn_get_tooltip_text = [opt_text] {
    return opt_text.value_or(std::string{});
  };
  dropdown_faction.fn_get_tooltip_desc = [opt_desc] {
    return opt_desc.value_or(std::string{});
  };

  SetupFactionWidget(dropdown_faction, client, vec_factions);
}

void SetupFactionWidget(
    ora::ui::Widget& parent,
    ora::net::SessionClient& client,
    const std::vector<std::pair<std::string, LobbyFaction>>& vec_factions) {
  auto& label_faction_name =
       *parent.Get<ora::ui::LabelWidget>("FACTIONNAME");
  ora::gfx::SpriteFont& font_faction =
      ora::ui::RequireWidgetFont(label_faction_name.str_font);
  auto transform_truncated = core::CachedTransform<std::string, std::string>(
      [&vec_factions, &label_faction_name, &font_faction](
          const std::string& str_client_faction) {
        const LobbyFaction* ptr_faction =
            FindLobbyFaction(vec_factions, str_client_faction);
        return ora::ui::TruncateText(
            ptr_faction != nullptr ? Msg(ptr_faction->Name) : std::string{},
            label_faction_name.bounds_widget.int4_width, font_faction);
      });
  label_faction_name.fn_get_text = [&transform_truncated, &client] {
    return transform_truncated.Update(client.Faction);
  };

  auto& flag_faction = *parent.Get<ora::ui::ImageWidget>("FACTIONFLAG");
  flag_faction.fn_get_image_name = [&client] { return client.Faction; };
  flag_faction.fn_get_image_collection = [] { return "flags"; };
}

void SetupEditableTeamWidget(ora::ui::Widget& parent,
                             const ora::net::SessionSlot& slot,
                             ora::net::SessionClient& client,
                             ora::net::OrderManager& order_manager,
                             const ora::map::MapPreview& map_preview) {
  auto& dropdown_team =
       *parent.Get<ora::ui::DropDownButtonWidget>("TEAM_DROPDOWN");
  dropdown_team.fn_is_visible = [] { return true; };
  dropdown_team.fn_is_disabled = [&slot, &order_manager] {
    return slot.LockTeam || order_manager.LocalClient()->IsReady();
  };
  dropdown_team.fn_on_mouse_down =
      [&dropdown_team, &client, &order_manager,
       &map_preview](const MouseInput&) {
        ShowTeamDropDown(dropdown_team, client, order_manager,
                         map_preview.PlayerCount());
      };
  dropdown_team.fn_get_text = [&client] {
    return client.Team == 0 ? std::string{"-"} : std::to_string(client.Team);
  };

  HideChildWidget(parent, "TEAM");
}

void SetupTeamWidget(ora::ui::Widget& parent,
                     ora::net::SessionClient& client) {
  auto& label_team = *parent.Get<ora::ui::LabelWidget>("TEAM");
  label_team.fn_is_visible = [] { return true; };
  label_team.fn_get_text = [&client] {
    return client.Team == 0 ? std::string{"-"} : std::to_string(client.Team);
  };
  HideChildWidget(parent, "TEAM_DROPDOWN");
}

void SetupEditableHandicapWidget(ora::ui::Widget& parent,
                                 const ora::net::SessionSlot& slot,
                                 ora::net::SessionClient& client,
                                 ora::net::OrderManager& order_manager) {
  auto& dropdown_handicap =
       *parent.Get<ora::ui::DropDownButtonWidget>("HANDICAP_DROPDOWN");
  dropdown_handicap.fn_is_visible = [] { return true; };
  dropdown_handicap.fn_is_disabled = [&slot, &order_manager] {
    return slot.LockHandicap || order_manager.LocalClient()->IsReady();
  };
  dropdown_handicap.fn_on_mouse_down =
      [&dropdown_handicap, &client,
       &order_manager](const MouseInput&) {
        ShowHandicapDropDown(dropdown_handicap, client, order_manager);
      };

  auto transform_handicap = core::CachedTransform<int, std::string>(
      [](int int4_handicap) { return std::format("{}%", int4_handicap); });
  dropdown_handicap.fn_get_text = [&transform_handicap, &client] {
    return transform_handicap.Update(client.Handicap);
  };

  HideChildWidget(parent, "HANDICAP");
}

void SetupHandicapWidget(ora::ui::Widget& parent,
                         ora::net::SessionClient& client) {
  auto& label_handicap = *parent.Get<ora::ui::LabelWidget>("HANDICAP");
  label_handicap.fn_is_visible = [] { return true; };

  auto transform_handicap = core::CachedTransform<int, std::string>(
      [](int int4_handicap) { return std::format("{}%", int4_handicap); });
  label_handicap.fn_get_text = [&transform_handicap, &client] {
    return transform_handicap.Update(client.Handicap);
  };
  HideChildWidget(parent, "HANDICAP_DROPDOWN");
}

void SetupEditableSpawnWidget(ora::ui::Widget& parent,
                              const ora::net::SessionSlot& slot,
                              ora::net::SessionClient& client,
                              ora::net::OrderManager& order_manager,
                              const ora::map::MapPreview& map_preview) {
  auto& dropdown_spawn =
       *parent.Get<ora::ui::DropDownButtonWidget>("SPAWN_DROPDOWN");
  dropdown_spawn.fn_is_visible = [] { return true; };
  dropdown_spawn.fn_is_disabled = [&slot, &order_manager] {
    return slot.LockSpawn || order_manager.LocalClient()->IsReady();
  };
  dropdown_spawn.fn_on_mouse_down =
      [&dropdown_spawn, &client, &order_manager,
       &map_preview](const MouseInput&) {
        std::vector<int> vec_taken;
        for (const auto& client_other :
             order_manager.LobbyInfo().vec_clients)
          if (&client_other != &client && client_other.SpawnPoint != 0)
            vec_taken.push_back(client_other.SpawnPoint);

        std::vector<int> vec_spawn_points;
        for (std::size_t sz_index = 0;
             sz_index <= map_preview.SpawnPoints().size(); ++sz_index) {
          const int int4_spawn = static_cast<int>(sz_index);
          const bool b_taken = [&] {
            for (int int4_point : vec_taken)
              if (int4_point == int4_spawn)
                return true;
            return false;
          }();
          if (b_taken)
            continue;
          if (DisabledSpawnContains(order_manager.LobbyInfo(), int4_spawn))
            continue;
          vec_spawn_points.push_back(int4_spawn);
        }

        ShowSpawnDropDown(dropdown_spawn, client, order_manager,
                          vec_spawn_points);
      };
  dropdown_spawn.fn_get_text = [&client] {
    return SpawnLabel(client.SpawnPoint);
  };

  HideChildWidget(parent, "SPAWN");
}

void SetupSpawnWidget(ora::ui::Widget& parent,
                      ora::net::SessionClient& client) {
  auto& label_spawn = *parent.Get<ora::ui::LabelWidget>("SPAWN");
  label_spawn.fn_is_visible = [] { return true; };
  label_spawn.fn_get_text = [&client] { return SpawnLabel(client.SpawnPoint); };
  HideChildWidget(parent, "SPAWN_DROPDOWN");
}

void SetupEditableReadyWidget(ora::ui::Widget& parent,
                              ora::net::SessionClient& client,
                              ora::net::OrderManager& order_manager,
                              const ora::map::MapPreview& map_preview,
                              bool b_is_enabled) {
  auto& checkbox_status =
       *parent.Get<ora::ui::CheckboxWidget>("STATUS_CHECKBOX");
  checkbox_status.fn_is_visible = [] { return true; };
  checkbox_status.fn_is_disabled = [&client, &map_preview, b_is_enabled] {
    return !client.Bot.empty() ||
           map_preview.Status() != ora::map::MapStatus::Available ||
           !b_is_enabled;
  };

  if (client.Bot.empty()) {
    auto transform_is_checked =
        std::make_shared<core::PredictedCachedTransform<
            const ora::net::SessionClient*, bool>>(
            [](const ora::net::SessionClient* ptr_client) {
              return ptr_client->IsReady();
            });
    checkbox_status.fn_is_checked = [transform_is_checked, &client] {
      return transform_is_checked->Update(&client);
    };
    checkbox_status.fn_on_click =
        [transform_is_checked, &client, &order_manager]() mutable {
          const bool b_state_next = !transform_is_checked->Update(&client);
          order_manager.IssueOrder(net::Order::Command(std::format(
              "state {}",
              b_state_next ? static_cast<int>(net::ClientState::Ready)
                           : static_cast<int>(net::ClientState::NotReady))));
          transform_is_checked->Predict(!client.IsReady());
        };
  } else {
    checkbox_status.fn_is_checked = [] { return true; };
  }
}

void SetupReadyWidget(ora::ui::Widget& parent,
                      ora::net::SessionClient& client) {
  parent.Get<ora::ui::ImageWidget>("STATUS_IMAGE")->fn_is_visible =
      [&client] { return client.IsReady() || !client.Bot.empty(); };
}

void HideReadyWidgets(ora::ui::Widget& parent) {
  HideChildWidget(parent, "STATUS_CHECKBOX");
  HideChildWidget(parent, "STATUS_IMAGE");
}

void HideChildWidget(ora::ui::Widget& parent, std::string_view str_widget_id) {
  if (auto* ptr_widget = parent.GetOrNull(std::string(str_widget_id)))
    ptr_widget->fn_is_visible = [] { return false; };
}

}  // namespace ora::mods
