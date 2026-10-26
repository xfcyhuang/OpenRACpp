// UPSTREAM: OpenRA.Mods.Common/Widgets/Logic/Lobby/LobbyUtils.cs
//          @1f594e320 全文(SetupProfileWidget 先于本批承载;本批补齐
//          其余:下拉族/出生点选择/Setup* widget 族/延迟色面)
//          The whole file (SetupProfileWidget landed before this batch;
//          this batch completes the rest: the dropdown family / spawn
//          selection / the Setup* widget family / the latency colors).
#pragma once
import std;

#include "core/cached_transform.hpp"
#include "core/color.hpp"
#include "ui/widget.hpp"

namespace ora {
struct MouseInput;
}

namespace ora::game {
class ModData;
}
namespace ora::gfx {
class WorldRenderer;
}
namespace ora::net {
class OrderManager;
struct SessionClient;
struct SessionSlot;
struct Session;
}  // namespace ora::net
namespace ora::map {
class MapPreview;
}
namespace ora::ui {
class CheckboxWidget;
class ColorBlockWidget;
class DropDownButtonWidget;
class ImageWidget;
class LabelWidget;
class MapPreviewWidget;
class TextFieldWidget;
}  // namespace ora::ui

namespace ora::mods {

struct LobbyFaction {
  bool Selectable = true;
  std::string Name;
  std::string Description;  // 空 = C# null | empty = C# null
  std::string Side;         // 同上 | ditto
};

void ShowSlotDropDown(ora::ui::DropDownButtonWidget& dropdown,
                      const ora::net::SessionSlot& slot,
                      const ora::net::SessionClient* ptr_client,
                      ora::net::OrderManager& order_manager,
                      const ora::map::MapPreview& map_preview,
                      ora::game::ModData& mod_data);

void ShowPlayerActionDropDown(ora::ui::DropDownButtonWidget& dropdown,
                              ora::net::SessionClient& client,
                              ora::net::OrderManager& order_manager,
                              ora::ui::Widget& lobby,
                              std::function<void()> fn_before,
                              std::function<void()> fn_after);

void ShowTeamDropDown(ora::ui::DropDownButtonWidget& dropdown,
                      ora::net::SessionClient& client,
                      ora::net::OrderManager& order_manager,
                      int int4_team_count);

void ShowHandicapDropDown(ora::ui::DropDownButtonWidget& dropdown,
                          ora::net::SessionClient& client,
                          ora::net::OrderManager& order_manager);

void ShowSpawnDropDown(ora::ui::DropDownButtonWidget& dropdown,
                       ora::net::SessionClient& client,
                       ora::net::OrderManager& order_manager,
                       const std::vector<int>& vec_spawn_points);

void ShowFactionDropDown(ora::ui::DropDownButtonWidget& dropdown,
                         ora::net::SessionClient& client,
                         ora::net::OrderManager& order_manager,
                         const std::vector<std::pair<std::string, LobbyFaction>>&
                             vec_factions);

const LobbyFaction* FindLobbyFaction(
    const std::vector<std::pair<std::string, LobbyFaction>>& vec_factions,
    std::string_view str_id);

void SelectSpawnPoint(ora::net::OrderManager& order_manager,
                      ora::ui::MapPreviewWidget& map_preview,
                      ora::map::MapPreview& preview,
                      const ora::MouseInput& mi_input);

std::vector<int> AvailableSpawnPoints(int int4_spawn_points,
                                      const ora::net::Session& lobby_info);

bool InsufficientEnabledSpawnPoints(const ora::map::MapPreview& map_preview,
                                    const ora::net::Session& lobby_info);

core::Color LatencyColor(const ora::net::SessionClient* ptr_client);

std::string LatencyDescription(const ora::net::SessionClient* ptr_client);

void SetupLatencyWidget(ora::ui::Widget& parent,
                        ora::net::SessionClient* ptr_client,
                        ora::net::OrderManager& order_manager);

void SetupProfileWidget(ora::ui::Widget& widget_parent,
                        ora::net::SessionClient* ptr_client,
                        ora::net::OrderManager* ptr_order_manager,
                        ora::gfx::WorldRenderer* ptr_world_renderer);

void SetupEditableNameWidget(ora::ui::Widget& parent,
                             ora::net::SessionClient& client,
                             ora::net::OrderManager& order_manager,
                             ora::gfx::WorldRenderer* ptr_world_renderer);

void SetupNameWidget(ora::ui::Widget& parent,
                     ora::net::SessionClient& client,
                     ora::net::OrderManager& order_manager,
                     ora::gfx::WorldRenderer* ptr_world_renderer,
                     const ora::map::MapPreview& map_preview);

void SetupEditableSlotWidget(ora::ui::Widget& parent,
                             const ora::net::SessionSlot& slot,
                             const ora::net::SessionClient* ptr_client,
                             ora::net::OrderManager& order_manager,
                             const ora::map::MapPreview& map_preview,
                             ora::game::ModData& mod_data);

void SetupSlotWidget(ora::ui::Widget& parent,
                     ora::game::ModData& mod_data,
                     const ora::net::SessionSlot& slot,
                     const ora::net::SessionClient* ptr_client);

void SetupPlayerActionWidget(ora::ui::Widget& parent,
                             ora::net::SessionClient& client,
                             ora::net::OrderManager& order_manager,
                             ora::gfx::WorldRenderer* ptr_world_renderer,
                             ora::ui::Widget& lobby,
                             std::function<void()> fn_before,
                             std::function<void()> fn_after);

void SetupKickSpectatorsWidget(ora::ui::Widget& parent,
                               ora::net::OrderManager& order_manager,
                               ora::ui::Widget& lobby,
                               std::function<void()> fn_before,
                               std::function<void()> fn_after,
                               bool b_skirmish_mode);

void SetupEditableColorWidget(ora::ui::Widget& parent,
                              const ora::net::SessionSlot* ptr_slot,
                              ora::net::SessionClient& client,
                              ora::net::OrderManager& order_manager,
                              ora::gfx::WorldRenderer* ptr_world_renderer);

void SetupColorWidget(ora::ui::Widget& parent,
                      ora::net::SessionClient& client);

void SetupEditableFactionWidget(ora::ui::Widget& parent,
                                const ora::net::SessionSlot& slot,
                                ora::net::SessionClient& client,
                                ora::net::OrderManager& order_manager,
                                const std::vector<std::pair<std::string, LobbyFaction>>&
                                    vec_factions);

void SetupFactionWidget(ora::ui::Widget& parent,
                        ora::net::SessionClient& client,
                        const std::vector<std::pair<std::string, LobbyFaction>>&
                            vec_factions);

void SetupEditableTeamWidget(ora::ui::Widget& parent,
                             const ora::net::SessionSlot& slot,
                             ora::net::SessionClient& client,
                             ora::net::OrderManager& order_manager,
                             const ora::map::MapPreview& map_preview);

void SetupTeamWidget(ora::ui::Widget& parent, ora::net::SessionClient& client);

void SetupEditableHandicapWidget(ora::ui::Widget& parent,
                                 const ora::net::SessionSlot& slot,
                                 ora::net::SessionClient& client,
                                 ora::net::OrderManager& order_manager);

void SetupHandicapWidget(ora::ui::Widget& parent,
                         ora::net::SessionClient& client);

void SetupEditableSpawnWidget(ora::ui::Widget& parent,
                              const ora::net::SessionSlot& slot,
                              ora::net::SessionClient& client,
                              ora::net::OrderManager& order_manager,
                              const ora::map::MapPreview& map_preview);

void SetupSpawnWidget(ora::ui::Widget& parent, ora::net::SessionClient& client);

void SetupEditableReadyWidget(ora::ui::Widget& parent,
                              ora::net::SessionClient& client,
                              ora::net::OrderManager& order_manager,
                              const ora::map::MapPreview& map_preview,
                              bool b_is_enabled);

void SetupReadyWidget(ora::ui::Widget& parent, ora::net::SessionClient& client);

void HideReadyWidgets(ora::ui::Widget& parent);

void HideChildWidget(ora::ui::Widget& parent, std::string_view str_widget_id);

}  // namespace ora::mods
