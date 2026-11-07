#pragma once

import std;

#include "game/input/input_events.hpp"
#include "game/input/screen_map.hpp"
#include "game/input/selection.hpp"
#include "game/world/world.hpp"

namespace ora::game::world {
struct WorldModel;
}

namespace ora::game::input {

constexpr std::int32_t kSelectionDeadzone{24};

// 上游 SelectionPriorityModifiers（TraitsInterfaces.cs:479-484）。
enum class SelectionPriorityModifiers : std::uint8_t {
  kNone = 0,
  kCtrl = 1,
  kAlt = 2,
};

[[nodiscard]] constexpr SelectionPriorityModifiers operator&(
    SelectionPriorityModifiers a, SelectionPriorityModifiers b) {
  return static_cast<SelectionPriorityModifiers>(static_cast<std::uint8_t>(a) &
                                                 static_cast<std::uint8_t>(b));
}

[[nodiscard]] constexpr SelectionPriorityModifiers operator|(
    SelectionPriorityModifiers a, SelectionPriorityModifiers b) {
  return static_cast<SelectionPriorityModifiers>(static_cast<std::uint8_t>(a) |
                                                 static_cast<std::uint8_t>(b));
}

enum class SelectionRelationship {
  kSelf,
  kNeutral,
  kEnemy,
};

[[nodiscard]] SelectionRelationship selection_relationship(const world::WorldModel& w,
                                                           std::uint32_t u4_viewer_player,
                                                           std::uint32_t u4_owner_player);

[[nodiscard]] std::int32_t selection_priority(const world::WorldModel& w,
                                              const world::ActorModel& actor,
                                              std::uint32_t u4_viewer_player,
                                              ModifierKeys mods);

// 上游 Selectable.Class（Selectable.cs:43-53）：yaml Class，空则 actor 类型名（小写）。
[[nodiscard]] std::string selectable_class(const world::WorldModel& w,
                                           const world::ActorModel& actor);

// 上游 ISelectableInfo.PriorityModifiers（Selectable trait yaml 字段）。
[[nodiscard]] SelectionPriorityModifiers selectable_priority_modifiers(
    const world::WorldModel& w, const world::ActorModel& actor);

[[nodiscard]] std::int32_t pixel_distance(std::int32_t int4_dx, std::int32_t int4_dy);

[[nodiscard]] std::optional<std::uint32_t> select_highest_priority_at_point(
    const world::WorldModel& w, std::uint32_t u4_viewer_player, std::int32_t int4_x,
    std::int32_t int4_y, ModifierKeys mods);

[[nodiscard]] std::vector<std::uint32_t> select_actors_in_box(
    const world::WorldModel& w, std::uint32_t u4_viewer_player, std::int32_t int4_ax,
    std::int32_t int4_ay, std::int32_t int4_bx, std::int32_t int4_by, ModifierKeys mods);

// 上游 SelectActorsOnScreen（SelectionUtils.cs:23-28）：视口（world px）框内
// Selectable actor 按 owner 集 + selectionClass 集过滤；classes 为空 = 全选。
[[nodiscard]] std::vector<std::uint32_t> select_actors_on_screen(
    const world::WorldModel& w, std::span<const std::uint32_t> vec_eligible_players,
    std::span<const std::string> vec_selection_classes, const PxRect& rect_viewport);

// 上游 SelectActorsInWorld（SelectionUtils.cs:30-33）：全图 in-world Selectable actor
// 按 owner 集 + selectionClass 集过滤。
[[nodiscard]] std::vector<std::uint32_t> select_actors_in_world(
    const world::WorldModel& w, std::span<const std::uint32_t> vec_eligible_players,
    std::span<const std::string> vec_selection_classes);

// 上游 SubsetWithHighestSelectionPriority（SelectableExts.cs:92-99）：
// GroupBy(priority) 取最高组（保输入序）。
[[nodiscard]] std::vector<std::uint32_t> subset_with_highest_selection_priority(
    const world::WorldModel& w, std::uint32_t u4_viewer_player,
    std::span<const std::uint32_t> vec_ids, ModifierKeys mods);

// 上游 GetPlayersToIncludeInSelection（SelectionUtils.cs:76-84）：viewer，
// 观战（NonCombatant+Spectating）→ 全体玩家。
[[nodiscard]] std::vector<std::uint32_t> players_to_include_in_selection(
    const world::WorldModel& w, std::uint32_t u4_viewer_player);

// 双击选同类（WIC :148-168）：tap>=2 时鼠标下最高优先 actor 的 Class →
// 视口内同 Class 且 eligible 玩家的 actor 集合（调用方 Combine(isCombine=true,isClick=false)）；
// 无命中/owner 不 eligible → 空集（不改选区）。
[[nodiscard]] std::vector<std::uint32_t> select_by_type_double_click(
    const world::WorldModel& w, std::uint32_t u4_viewer_player, std::int32_t int4_x,
    std::int32_t int4_y, ModifierKeys mods, const PxRect& rect_viewport);

struct DragState {
  bool b_dragging{};
  std::int32_t int4_start_x{};
  std::int32_t int4_start_y{};
  std::int32_t int4_mouse_x{};
  std::int32_t int4_mouse_y{};
};

[[nodiscard]] std::int32_t drag_length(const DragState& drag);

[[nodiscard]] bool drag_is_valid_box(const DragState& drag);

void selection_handle_mouse(const world::WorldModel& w, std::uint32_t u4_viewer_player,
                            SelectionModel& selection, DragState& drag, const MouseInput& mi);

// 带 world px 视口框的完整 WIC 左键面（双击选同类走视口内 SelectActorsOnScreen）。
// 旧签名等价于"视口 = 全图包围盒"。
void selection_handle_mouse_in_view(const world::WorldModel& w, std::uint32_t u4_viewer_player,
                                    SelectionModel& selection, DragState& drag,
                                    const MouseInput& mi, const PxRect& rect_viewport);

}
