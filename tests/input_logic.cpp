import std;

#include "game/input/input_events.hpp"
#include "game/input/order_generator.hpp"
#include "game/input/production_panel.hpp"
#include "game/network/order_manager.hpp"
#include "game/input/screen_map.hpp"
#include "game/input/selection.hpp"
#include "game/input/selection_logic.hpp"
#include "game/world/sync_hash.hpp"
#include "game/world/world.hpp"
#include "test_util.hpp"
using ora::test::check;

namespace {

namespace wig = ora::game::input;
namespace net = ora::game::network;
namespace ww = ora::game::world;


constexpr std::string_view kRules{
    "player:\n"
    "\tTickCounter:\n"
    "world:\n"
    "\tFactionInfo@allies:\n"
    "\tFactionInfo@soviet:\n"
    "probe.tank:\n"
    "\tSelectable:\n"
    "\t\tBounds: 512,512\n"
    "\tTargetable:\n"
    "\tMobileProbe:\n"
    "\tAttackProbe:\n"
    "\tOrderCounterProbe:\n"
    "probe.truck:\n"
    "\tSelectable:\n"
    "\t\tPriority: 5\n"
    "\t\tBounds: 512,512\n"
    "\tTargetable:\n"
    "\tMobileProbe:\n"
    "probe.wall:\n"
    "\tSelectable:\n"
    "\t\tBounds: 1024,1024\n"
    "\tTargetable:\n"
    "probe.turret:\n"
    "\tSelectable:\n"
    "\t\tBounds: 512,512\n"
    "\tTargetable:\n"
    "\tAttackProbe:\n"
    "probe.heavy:\n"
    "\tSelectable:\n"
    "\t\tPriority: 11\n"
    "\t\tBounds: 2048,2048\n"
    "\tTargetable:\n"
    "probe.deco:\n"
    "\tSelectable:\n"
    "\t\tBounds: 512,512\n"};

constexpr std::string_view kMap{
    "Title: input-map\n"
    "Players:\n"
    "\tPlayerReference@Neutral:\n"
    "\t\tName: Neutral\n"
    "\t\tOwnsWorld: True\n"
    "\t\tNonCombatant: True\n"
    "\t\tFaction: allies\n"
    "\tPlayerReference@Creeps:\n"
    "\t\tName: Creeps\n"
    "\t\tNonCombatant: True\n"
    "\t\tFaction: soviet\n"
    "\tPlayerReference@P1:\n"
    "\t\tName: P1\n"
    "\t\tFaction: soviet\n"
    "Actors:\n"
    "\ttanka: probe.tank\n"
    "\t\tLocation: 10,10\n"
    "\t\tOwner: Neutral\n"
    "\ttankb: probe.tank\n"
    "\t\tLocation: 20,10\n"
    "\t\tOwner: Neutral\n"
    "\ttruck0: probe.truck\n"
    "\t\tLocation: 10,13\n"
    "\t\tOwner: Neutral\n"
    "\te1: probe.tank\n"
    "\t\tLocation: 30,10\n"
    "\t\tOwner: P1\n"
    "\tn1: probe.wall\n"
    "\t\tLocation: 30,14\n"
    "\t\tOwner: Creeps\n"
    "\tturret0: probe.turret\n"
    "\t\tLocation: 10,20\n"
    "\t\tOwner: Neutral\n"
    "\theavy0: probe.heavy\n"
    "\t\tLocation: 11,11\n"
    "\t\tOwner: Neutral\n"
    "\tdeco0: probe.deco\n"
    "\t\tLocation: 40,10\n"
    "\t\tOwner: Neutral\n"};

constexpr std::int32_t kSeed{0};
constexpr std::uint32_t kViewerPlayer{0};

std::expected<ww::WorldModel, std::string> make_world() {
  auto res_map{ww::synthetic_map_from_yaml(kRules, kMap)};
  if (!res_map.has_value()) return std::unexpected(res_map.error());
  return ww::world_create(*res_map, kSeed);
}

constexpr std::uint32_t kTankA{5};
constexpr std::uint32_t kTankB{6};
constexpr std::uint32_t kTruck{7};
constexpr std::uint32_t kEnemyTank{8};
constexpr std::uint32_t kNeutralWall{9};
constexpr std::uint32_t kTurret{10};
constexpr std::uint32_t kHeavy{11};
constexpr std::uint32_t kDeco{12};
constexpr std::uint32_t kViewerPlayerActor{1};

std::string to_hex(std::span<const std::uint8_t> span_bytes) {
  std::string str_out;
  str_out.reserve(span_bytes.size() * 2);
  for (const std::uint8_t u1_byte : span_bytes)
    str_out.append(std::format("{:02x}", u1_byte));
  return str_out;
}

wig::MouseInput mi_left(std::int32_t int4_x, std::int32_t int4_y,
                        wig::MouseInputEvent event = wig::MouseInputEvent::kDown,
                        wig::ModifierKeys mods = wig::ModifierKeys::kNone) {
  return wig::MouseInput{.event = event,
                         .button = wig::MouseButton::kLeft,
                         .int4_x = int4_x,
                         .int4_y = int4_y,
                         .mods = mods};
}

void test_screen_map() {
  auto res_world{make_world()};
  check(res_world.has_value(), "screen_map: world creates");
  if (!res_world.has_value()) {
    std::println("world error: {}", res_world.error());
    return;
  }
  const ww::WorldModel& w{*res_world};

  check(w.vec_actors_all.size() == 13 && w.vec_actors_all[kTankA].str_type == "probe.tank" &&
            w.vec_actors_all[kTankB].str_type == "probe.tank" &&
            w.vec_actors_all[kTruck].str_type == "probe.truck" &&
            w.vec_actors_all[kEnemyTank].u4_owner == 2 &&
            w.vec_actors_all[kNeutralWall].u4_owner == 1 &&
            w.vec_actors_all[kHeavy].str_type == "probe.heavy" &&
            w.vec_actors_all[kDeco].str_type == "probe.deco" &&
            w.vec_actors_all[kDeco].vec_trait_handles.size() == 1,
        "screen_map: actor layout ids 5..12 by map yaml order");

  check(wig::view_to_world_px(123) == 123, "screen_map: zoom 1 view px equals world px");
  check(wig::world_px_to_cell(251) == 10 && wig::world_px_to_cell(252) == 10 &&
            wig::world_px_to_cell(275) == 11,
        "screen_map: world px to cell via floor by 24");
  check(wig::actor_center_px(10) == 252, "screen_map: cell center world px 24c+12");

  const std::optional<wig::PxRect> rect_tanka{wig::actor_mouse_bounds(w, w.vec_actors_all[kTankA])};
  check(rect_tanka.has_value() && *rect_tanka == wig::PxRect{246, 246, 12, 12},
        "screen_map: tanka bounds 12x12 centered on cell (512 WDist)");

  const std::optional<wig::PxRect> rect_wall{
      wig::actor_mouse_bounds(w, w.vec_actors_all[kNeutralWall])};
  check(rect_wall.has_value() && *rect_wall == wig::PxRect{720, 336, 24, 24},
        "screen_map: wall bounds 24x24 (1024 WDist)");

  const std::optional<wig::PxRect> rect_heavy{wig::actor_mouse_bounds(w, w.vec_actors_all[kHeavy])};
  check(rect_heavy.has_value() && *rect_heavy == wig::PxRect{252, 252, 48, 48},
        "screen_map: heavy bounds 48x48 (2048 WDist) overlaps tanka bounds corner");

  {
    const std::vector<wig::ActorBoundsPair> vec_hits{wig::actors_at_mouse(w, 252, 252)};
    check(vec_hits.size() == 2 && vec_hits[0].u4_actor_id == kTankA &&
              vec_hits[1].u4_actor_id == kHeavy,
        "screen_map: point at tanka center also inside heavy bounds");
  }
  {
    const std::vector<wig::ActorBoundsPair> vec_hits{wig::actors_at_mouse(w, 100, 100)};
    check(vec_hits.empty(), "screen_map: point in empty terrain hits nothing");
  }
  {
    const std::vector<wig::ActorBoundsPair> vec_hits{wig::actors_in_box(
        w, wig::PxRect{240, 240, 258, 24})};
    check(vec_hits.size() == 3 && vec_hits[0].u4_actor_id == kTankA &&
              vec_hits[1].u4_actor_id == kTankB && vec_hits[2].u4_actor_id == kHeavy,
        "screen_map: box over tanka/tankb/heavy intersects all three in actor id order");
  }
}

void test_selection_logic_point() {
  auto res_world{make_world()};
  check(res_world.has_value(), "point: world creates");
  if (!res_world.has_value()) return;
  const ww::WorldModel& w{*res_world};

  check(wig::selection_priority(w, w.vec_actors_all[kTankA], kViewerPlayer,
                                wig::ModifierKeys::kNone) == 10,
        "point: own unit priority is Selectable priority");
  check(wig::selection_priority(w, w.vec_actors_all[kTruck], kViewerPlayer,
                                wig::ModifierKeys::kNone) == 5,
        "point: explicit Priority 5 parsed");
  check(wig::selection_priority(w, w.vec_actors_all[kNeutralWall], kViewerPlayer,
                                wig::ModifierKeys::kNone) == 10 - 2 * 30,
        "point: neutral owner priority base-60");
  check(wig::selection_priority(w, w.vec_actors_all[kEnemyTank], kViewerPlayer,
                                wig::ModifierKeys::kNone) == 10 - 3 * 30,
        "point: enemy owner priority base-90");

  check(wig::select_highest_priority_at_point(w, kViewerPlayer, 252, 252,
                                              wig::ModifierKeys::kNone) == kTankA,
        "point: tanka wins at own center despite heavy priority 11 (distance 33 vs 0)");
  check(wig::select_highest_priority_at_point(w, kViewerPlayer, 255, 255,
                                              wig::ModifierKeys::kNone) == kTankA,
        "point: nearer low-priority tanka beats farther heavy inside overlap");
  check(wig::select_highest_priority_at_point(w, kViewerPlayer, 280, 280,
                                              wig::ModifierKeys::kNone) == kHeavy,
        "point: heavy wins near its center outside tanka bounds");
  check(!wig::select_highest_priority_at_point(w, kViewerPlayer, 100, 100,
                                               wig::ModifierKeys::kNone)
             .has_value(),
        "point: empty terrain selects nothing");

  {
    const std::vector<std::uint32_t> vec_picked{wig::select_actors_in_box(
        w, kViewerPlayer, 252, 252, 276, 252, wig::ModifierKeys::kNone)};
    check(vec_picked.size() == 1 && vec_picked[0] == kTankA,
          "point: drag within 24px deadzone collapses to click at drag start");
  }
  {
    const std::vector<std::uint32_t> vec_picked{wig::select_actors_in_box(
        w, kViewerPlayer, 252, 252, 253, 277, wig::ModifierKeys::kNone)};
    check(vec_picked.size() == 1 && vec_picked[0] == kHeavy,
          "point: drag beyond deadzone becomes box select taking priority group");
  }
}

void test_selection_combine() {
  wig::SelectionModel sel;

  wig::selection_combine(sel, std::vector<std::uint32_t>{kTankA, kTankB}, false, false);
  check(sel.vec_actor_ids == std::vector<std::uint32_t>{kTankA, kTankB},
        "combine: replace with new set");

  wig::selection_combine(sel, std::vector<std::uint32_t>{kTruck}, false, false);
  check(sel.vec_actor_ids == std::vector<std::uint32_t>{kTruck},
        "combine: replace clears previous");

  wig::selection_combine(sel, std::vector<std::uint32_t>{kTankA}, true, false);
  check(sel.vec_actor_ids == std::vector<std::uint32_t>{kTankA, kTruck},
        "combine: shift box adds to selection");

  wig::selection_combine(sel, std::vector<std::uint32_t>{kTruck}, true, true);
  check(sel.vec_actor_ids == std::vector<std::uint32_t>{kTankA},
        "combine: shift click toggles selected actor out");

  wig::selection_combine(sel, std::vector<std::uint32_t>{kTruck}, true, true);
  check(sel.vec_actor_ids == std::vector<std::uint32_t>{kTankA, kTruck},
        "combine: shift click toggles absent actor in");

  wig::selection_combine(sel, std::vector<std::uint32_t>{kTankA, kTankB}, true, true);
  check(sel.vec_actor_ids == std::vector<std::uint32_t>{kTruck},
        "combine: click path takes first candidate only before symmetric except");

  wig::selection_combine(sel, std::vector<std::uint32_t>{}, false, false);
  check(sel.vec_actor_ids.empty(), "combine: empty ground click clears selection");
}

void test_selection_handle_mouse() {
  auto res_world{make_world()};
  check(res_world.has_value(), "wic: world creates");
  if (!res_world.has_value()) return;
  const ww::WorldModel& w{*res_world};
  wig::SelectionModel sel;
  wig::DragState drag;

  wig::selection_handle_mouse(w, kViewerPlayer, sel, drag, mi_left(252, 252));
  check(drag.b_dragging, "wic: left down starts drag");
  check(drag.int4_start_x == 252 && drag.int4_start_y == 252, "wic: drag start recorded");

  wig::selection_handle_mouse(w, kViewerPlayer, sel, drag,
                              mi_left(252, 252, wig::MouseInputEvent::kUp));
  check(sel.vec_actor_ids == std::vector<std::uint32_t>{kTankA}, "wic: click selects tanka");
  check(!drag.b_dragging, "wic: left up ends drag");

  wig::selection_handle_mouse(w, kViewerPlayer, sel, drag, mi_left(700, 700));
  wig::selection_handle_mouse(w, kViewerPlayer, sel, drag,
                              mi_left(700, 700, wig::MouseInputEvent::kUp));
  check(sel.vec_actor_ids.empty(), "wic: ground click clears selection");

  wig::selection_handle_mouse(w, kViewerPlayer, sel, drag,
                              mi_left(492, 252, wig::MouseInputEvent::kDown,
                                      wig::ModifierKeys::kShift));
  wig::selection_handle_mouse(w, kViewerPlayer, sel, drag,
                              mi_left(492, 252, wig::MouseInputEvent::kUp,
                                      wig::ModifierKeys::kShift));
  check(sel.vec_actor_ids == std::vector<std::uint32_t>{kTankB},
        "wic: shift click on unselected actor selects it");

  wig::selection_handle_mouse(w, kViewerPlayer, sel, drag,
                              mi_left(492, 252, wig::MouseInputEvent::kDown,
                                      wig::ModifierKeys::kShift));
  wig::selection_handle_mouse(w, kViewerPlayer, sel, drag,
                              mi_left(492, 252, wig::MouseInputEvent::kUp,
                                      wig::ModifierKeys::kShift));
  check(sel.vec_actor_ids.empty(), "wic: shift click on selected actor deselects it");

  wig::selection_handle_mouse(w, kViewerPlayer, sel, drag, mi_left(200, 200));
  wig::selection_handle_mouse(w, kViewerPlayer, sel, drag,
                              wig::MouseInput{.event = wig::MouseInputEvent::kMove,
                                              .button = wig::MouseButton::kNone,
                                              .int4_x = 600,
                                              .int4_y = 400});
  check(drag.b_dragging && drag.int4_mouse_x == 600 && drag.int4_mouse_y == 400,
        "wic: move event updates drag mouse position");
  check(wig::drag_length(drag) > wig::kSelectionDeadzone && wig::drag_is_valid_box(drag),
        "wic: 500px drag exceeds deadzone and is a valid box");
  wig::selection_handle_mouse(w, kViewerPlayer, sel, drag,
                              mi_left(600, 400, wig::MouseInputEvent::kUp));
  check(sel.vec_actor_ids == std::vector<std::uint32_t>{kHeavy},
        "wic: big box over tanka/tankb/truck/heavy keeps only priority-11 group");
}

void test_control_groups() {
  auto res_world{make_world()};
  check(res_world.has_value(), "groups: world creates");
  if (!res_world.has_value()) return;
  const ww::WorldModel& w{*res_world};
  wig::SelectionModel sel;
  wig::DragState drag;

  wig::selection_handle_mouse(w, kViewerPlayer, sel, drag, mi_left(240, 240));
  wig::selection_handle_mouse(w, kViewerPlayer, sel, drag,
                              mi_left(502, 264, wig::MouseInputEvent::kUp));
  check(sel.vec_actor_ids == std::vector<std::uint32_t>{kHeavy},
        "groups: box over all four own units selects priority-11 group only");

  wig::selection_handle_mouse(w, kViewerPlayer, sel, drag,
                              mi_left(240, 240, wig::MouseInputEvent::kDown,
                                      wig::ModifierKeys::kShift));
  wig::selection_handle_mouse(w, kViewerPlayer, sel, drag,
                              mi_left(252, 264, wig::MouseInputEvent::kUp,
                                      wig::ModifierKeys::kShift));
  check(sel.vec_actor_ids == std::vector<std::uint32_t>{kTankA, kHeavy},
        "groups: shift box unions groups across priorities");

  wig::selection_handle_mouse(w, kViewerPlayer, sel, drag, mi_left(400, 240));
  wig::selection_handle_mouse(w, kViewerPlayer, sel, drag,
                              mi_left(510, 264, wig::MouseInputEvent::kUp));
  check(sel.vec_actor_ids == std::vector<std::uint32_t>{kTankB},
        "groups: box over tankb only avoids heavy and tanka");

  wig::control_group_create(sel, w, kViewerPlayer, 2);
  check(sel.arr_groups[2] == std::vector<std::uint32_t>{kTankB},
        "groups: create puts own selected into group");
  sel.vec_actor_ids.clear();
  wig::control_group_select(sel, 2);
  check(sel.vec_actor_ids == std::vector<std::uint32_t>{kTankB},
        "groups: select restores group as replacement combine");

  wig::selection_combine(sel, std::vector<std::uint32_t>{kEnemyTank}, false, false);
  check(sel.vec_actor_ids == std::vector<std::uint32_t>{kEnemyTank},
        "groups: enemy unit can be selected (view only)");
  wig::control_group_create(sel, w, kViewerPlayer, 3);
  check(sel.arr_groups[3].empty(),
        "groups: create filters out non-own selected actors");

  wig::selection_combine(sel, std::vector<std::uint32_t>{kTankB}, false, false);
  wig::control_group_add_to(sel, w, kViewerPlayer, 3);
  check(sel.arr_groups[3] == std::vector<std::uint32_t>{kTankB},
        "groups: add-to merges own selected into group");

  wig::selection_combine(sel, std::vector<std::uint32_t>{kTankA, kTankB}, false, false);
  wig::control_group_create(sel, w, kViewerPlayer, 2);
  check(sel.arr_groups[2] == std::vector<std::uint32_t>{kTankA, kTankB} &&
            sel.arr_groups[3].empty(),
        "groups: re-create clears group and removes members from every other group");
}

void test_order_generator() {
  auto res_world{make_world()};
  check(res_world.has_value(), "orders: world creates");
  if (!res_world.has_value()) return;
  const ww::WorldModel& w{*res_world};
  wig::SelectionModel sel;
  wig::DragState drag;

  {
    wig::selection_combine(sel, std::vector<std::uint32_t>{kTankA, kTankB}, false, false);
    const std::vector<net::OrderModel> vec_orders{wig::unit_order_generator_order(
        w, kViewerPlayer, sel, 29, 29, 700, 700,
        wig::MouseInput{.event = wig::MouseInputEvent::kUp,
                        .button = wig::MouseButton::kRight,
                        .int4_x = 700,
                        .int4_y = 700})};
    check(vec_orders.size() == 3, "orders: two units right-click ground yield 3 orders");
    if (vec_orders.size() == 3) {
      check(vec_orders[0].str_order == "CreateGroup" && vec_orders[0].b_has_subject &&
                vec_orders[0].u4_subject == kViewerPlayerActor && vec_orders[0].b_has_grouped &&
                vec_orders[0].vec_grouped == std::vector<std::uint32_t>({kTankA, kTankB}) &&
                !vec_orders[0].b_queued,
            "orders: CreateGroup leads with player actor subject and grouped ids");
      check(vec_orders[1].str_order == "Move" && vec_orders[1].u4_subject == kTankA &&
                vec_orders[1].target.kind == net::TargetKind::kTerrain &&
                vec_orders[1].target.b_cell &&
                vec_orders[1].target.int4_cell_bits == wig::cpos_bits(29, 29, 0) &&
                vec_orders[1].target.u1_subcell == 0 && !vec_orders[1].b_queued,
            "orders: tanka gets Move with cell target");
      check(vec_orders[2].str_order == "Move" && vec_orders[2].u4_subject == kTankB,
            "orders: tankb gets Move too");
      check(to_hex(net::serialize_order(vec_orders[0])) ==
                "ff0b43726561746547726f7570800101000000020000000500000006000000",
            "orders: CreateGroup hex matches T08 serialize");
      check(to_hex(net::serialize_order(vec_orders[1])) ==
                "ff044d6f7665c1000500000002001dd00100",
            "orders: Move hex matches T08 serialize");
    }
  }

  {
    const std::vector<net::OrderModel> vec_orders{wig::unit_order_generator_order(
        w, kViewerPlayer, sel, 29, 29, 700, 700,
        wig::MouseInput{.event = wig::MouseInputEvent::kUp,
                        .button = wig::MouseButton::kRight,
                        .int4_x = 700,
                        .int4_y = 700,
                        .mods = wig::ModifierKeys::kShift})};
    check(vec_orders.size() == 3 && vec_orders[1].b_queued && vec_orders[2].b_queued &&
              !vec_orders[0].b_queued,
        "orders: shift right click sets queued flag on unit orders only");
    check(to_hex(net::serialize_order(vec_orders[1])) == "ff044d6f7665c9000500000002001dd00100",
          "orders: queued Move hex has queued flag bit");
  }

  {
    const std::vector<net::OrderModel> vec_orders{wig::unit_order_generator_order(
        w, kViewerPlayer, sel, 30, 10, 732, 252,
        wig::MouseInput{.event = wig::MouseInputEvent::kUp,
                        .button = wig::MouseButton::kRight,
                        .int4_x = 732,
                        .int4_y = 252})};
    check(vec_orders.size() == 3 && vec_orders[1].str_order == "Attack" &&
              vec_orders[1].target.kind == net::TargetKind::kActor &&
              vec_orders[1].target.u4_actor == kEnemyTank &&
              vec_orders[1].target.int4_generation == 0 &&
              vec_orders[2].str_order == "Attack" && vec_orders[2].u4_subject == kTankB,
          "orders: right click enemy tank yields Attack with actor target");
    check(to_hex(net::serialize_order(vec_orders[1])) ==
              "ff0641747461636b810005000000010800000000000000",
          "orders: Attack hex matches T08 serialize");
  }

  {
    wig::SelectionModel sel_mixed;
    wig::selection_combine(sel_mixed, std::vector<std::uint32_t>{kTruck, kTurret}, false, false);
    const std::vector<net::OrderModel> vec_orders{wig::unit_order_generator_order(
        w, kViewerPlayer, sel_mixed, 30, 10, 732, 252,
        wig::MouseInput{.event = wig::MouseInputEvent::kUp,
                        .button = wig::MouseButton::kRight,
                        .int4_x = 732,
                        .int4_y = 252})};
    check(vec_orders.size() == 3 && vec_orders[1].str_order == "Move" &&
              vec_orders[1].u4_subject == kTruck && vec_orders[2].str_order == "Attack" &&
              vec_orders[2].u4_subject == kTurret,
          "orders: mixed selection two-pass arbitration per unit");
  }

  {
    wig::SelectionModel sel_enemy;
    wig::selection_combine(sel_enemy, std::vector<std::uint32_t>{kTankA, kEnemyTank}, false,
                           false);
    const std::vector<net::OrderModel> vec_orders{wig::unit_order_generator_order(
        w, kViewerPlayer, sel_enemy, 29, 29, 700, 700,
        wig::MouseInput{.event = wig::MouseInputEvent::kUp,
                        .button = wig::MouseButton::kRight,
                        .int4_x = 700,
                        .int4_y = 700})};
    check(vec_orders.size() == 2 && vec_orders[0].vec_grouped == std::vector<std::uint32_t>({kTankA}) &&
              vec_orders[1].u4_subject == kTankA,
          "orders: enemy selected unit receives no order");
  }

  {
    wig::SelectionModel sel_one;
    wig::selection_combine(sel_one, std::vector<std::uint32_t>{kTankA}, false, false);
    const std::vector<net::OrderModel> vec_orders{wig::unit_order_generator_order(
        w, kViewerPlayer, sel_one, 40, 10, 972, 252,
        wig::MouseInput{.event = wig::MouseInputEvent::kUp,
                        .button = wig::MouseButton::kRight,
                        .int4_x = 972,
                        .int4_y = 252})};
    check(vec_orders.size() == 2 && vec_orders[1].str_order == "Move" &&
              vec_orders[1].target.b_cell && vec_orders[1].target.int4_cell_bits == wig::cpos_bits(40, 10, 0),
          "orders: non-targetable deco under cursor falls through to cell move");
  }

  {
    wig::SelectionModel sel_one;
    wig::selection_combine(sel_one, std::vector<std::uint32_t>{kTankA}, false, false);
    check(wig::unit_order_generator_order(
              w, kViewerPlayer, sel_one, 29, 29, 700, 700,
              wig::MouseInput{.event = wig::MouseInputEvent::kUp,
                              .button = wig::MouseButton::kLeft,
                              .int4_x = 700,
                              .int4_y = 700})
              .empty(),
          "orders: non-action button yields no orders");

    wig::DragState drag_box;
    drag_box.b_dragging = true;
    drag_box.int4_start_x = 100;
    drag_box.int4_start_y = 100;
    drag_box.int4_mouse_x = 600;
    drag_box.int4_mouse_y = 400;
    check(wig::order_generator_handle_mouse(
              w, kViewerPlayer, sel_one, drag_box,
              wig::MouseInput{.event = wig::MouseInputEvent::kUp,
                              .button = wig::MouseButton::kRight,
                              .int4_x = 600,
                              .int4_y = 400})
              .empty(),
          "orders: right click during valid drag box is ignored");

    const std::vector<net::OrderModel> vec_via_handle{
        wig::order_generator_handle_mouse(
            w, kViewerPlayer, sel_one, drag,
            wig::MouseInput{.event = wig::MouseInputEvent::kUp,
                            .button = wig::MouseButton::kRight,
                            .int4_x = 700,
                            .int4_y = 700})};
    check(vec_via_handle.size() == 2 && vec_via_handle[0].str_order == "CreateGroup" &&
              vec_via_handle[1].str_order == "Move" &&
              vec_via_handle[1].target.int4_cell_bits == wig::cpos_bits(29, 29, 0),
          "orders: handle_mouse entry derives cell from view px (zoom 1)");
  }
}

wig::MouseInput mi_panel(std::int32_t int4_x, std::int32_t int4_y, wig::MouseButton button,
                         wig::MouseInputEvent event = wig::MouseInputEvent::kDown,
                         wig::ModifierKeys mods = wig::ModifierKeys::kNone) {
  return wig::MouseInput{.event = event, .button = button, .int4_x = int4_x, .int4_y = int4_y,
                         .mods = mods};
}

wig::ProductionQueueView make_vehicle_queue() {
  wig::ProductionQueueView queue;
  queue.str_type = "Vehicle";
  queue.b_enabled = true;
  queue.vec_items = {"jeep", "1tnk", "2tnk"};
  return queue;
}

void test_production_panel_layout() {
  constexpr wig::PanelGeometry kGeo{960, 600};

  check(wig::panel_rect(kGeo) == wig::PxRect{722, 0, 238, 600},
        "panel: right column 238 wide");
  check(wig::panel_tab_rect(kGeo, 0) == wig::PxRect{726, 8, 28, 28}, "panel: tab 0 rect");
  check(wig::panel_tab_rect(kGeo, 5) == wig::PxRect{726, 158, 28, 28}, "panel: tab 5 rect");
  check(wig::panel_icon_rect(kGeo, 0) == wig::PxRect{764, 8, 62, 46}, "panel: icon 0 rect");
  check(wig::panel_icon_rect(kGeo, 1) == wig::PxRect{828, 8, 62, 46},
        "panel: icon 1 second column");
  check(wig::panel_icon_rect(kGeo, 3) == wig::PxRect{764, 56, 62, 46},
        "panel: icon 3 second row first column");
  check(wig::panel_contains(kGeo, 900, 300) && !wig::panel_contains(kGeo, 700, 300),
        "panel: containment splits at sidebar left edge");
}

void test_production_panel_mouse() {
  constexpr wig::PanelGeometry kGeo{960, 600};
  constexpr std::uint32_t kQueueActor{1};

  wig::ProductionPanelModel panel;
  wig::ProductionQueueView queue_vehicle{make_vehicle_queue()};
  wig::ProductionQueueView queue_building;
  queue_building.str_type = "Building";
  queue_building.b_enabled = false;
  queue_building.vec_items = {"fact"};
  const std::vector<wig::ProductionQueueView> vec_queues{queue_building, queue_vehicle};

  {
    const auto res{wig::panel_handle_mouse(panel, kGeo, vec_queues, kQueueActor,
                                           mi_panel(700, 300, wig::MouseButton::kLeft))};
    check(!res.b_handled && res.vec_orders.empty(),
          "panel: click outside panel not handled");
  }

  {
    const auto res{wig::panel_handle_mouse(panel, kGeo, vec_queues, kQueueActor,
                                           mi_panel(740, 20, wig::MouseButton::kLeft))};
    check(res.b_handled && res.vec_orders.empty() && panel.str_current_type.empty(),
          "panel: disabled tab click handled but does not switch");
  }

  {
    const auto res{wig::panel_handle_mouse(panel, kGeo, vec_queues, kQueueActor,
                                           mi_panel(740, 112, wig::MouseButton::kLeft))};
    check(res.b_handled && panel.str_current_type == "Vehicle",
          "panel: enabled tab click switches current type");
  }

  {
    const auto res{wig::panel_handle_mouse(panel, kGeo, vec_queues, kQueueActor,
                                           mi_panel(770, 20, wig::MouseButton::kLeft))};
    check(res.b_handled && res.vec_orders.size() == 1 &&
              res.vec_orders[0].str_order == "StartProduction" &&
              res.vec_orders[0].u4_subject == kQueueActor &&
              res.vec_orders[0].opt_str_target == "jeep" &&
              res.vec_orders[0].u4_extra_data == 1 && res.vec_orders[0].b_queued,
          "panel: left down queues StartProduction count 1 queued");
    if (res.vec_orders.size() == 1)
      check(to_hex(net::serialize_order(res.vec_orders[0])) ==
                "ff0f537461727450726f64756374696f6eac0001000000046a65657001000000",
            "panel: StartProduction hex matches T08 serialize");
  }

  {
    const auto res{wig::panel_handle_mouse(panel, kGeo, vec_queues, kQueueActor,
                                           mi_panel(770, 20, wig::MouseButton::kLeft,
                                                    wig::MouseInputEvent::kDown,
                                                    wig::ModifierKeys::kShift))};
    check(res.vec_orders.size() == 1 && res.vec_orders[0].u4_extra_data == 5,
          "panel: shift left queues count 5");
  }

  {
    const auto res{wig::panel_handle_mouse(panel, kGeo, vec_queues, kQueueActor,
                                           mi_panel(770, 20, wig::MouseButton::kLeft,
                                                    wig::MouseInputEvent::kDown,
                                                    wig::ModifierKeys::kCtrl))};
    check(res.vec_orders.size() == 1 && !res.vec_orders[0].b_queued,
          "panel: ctrl left inserts at front (queued flag clear)");
  }

  {
    const auto res{wig::panel_handle_mouse(panel, kGeo, vec_queues, kQueueActor,
                                           mi_panel(770, 20, wig::MouseButton::kRight))};
    check(res.b_handled && res.vec_orders.empty(),
          "panel: right click icon without queued item is no-op");
  }

  {
    wig::ProductionQueueView queue{make_vehicle_queue()};
    queue.vec_queue.push_back(wig::ProductionItemView{.str_item = "jeep",
                                                      .int4_total_time = 60,
                                                      .int4_remaining_time = 30,
                                                      .int4_total_cost = 500,
                                                      .int4_remaining_cost = 250,
                                                      .b_started = true});
    const std::vector<wig::ProductionQueueView> vec_one{queue};
    const auto res{wig::panel_handle_mouse(panel, kGeo, vec_one, kQueueActor,
                                           mi_panel(770, 20, wig::MouseButton::kRight))};
    check(res.vec_orders.size() == 1 && res.vec_orders[0].str_order == "PauseProduction" &&
              res.vec_orders[0].opt_str_target == "jeep" && res.vec_orders[0].u4_extra_data == 1,
          "panel: right click in-progress item pauses");
    check(wig::panel_item_progress_percent(queue.vec_queue[0]) == 50,
          "panel: progress percent half done");
  }

  {
    wig::ProductionQueueView queue{make_vehicle_queue()};
    queue.vec_queue.push_back(wig::ProductionItemView{.str_item = "jeep",
                                                      .int4_total_time = 60,
                                                      .int4_remaining_time = 30,
                                                      .int4_total_cost = 500,
                                                      .int4_remaining_cost = 250,
                                                      .b_started = true,
                                                      .b_paused = true});
    const std::vector<wig::ProductionQueueView> vec_one{queue};
    const auto res{wig::panel_handle_mouse(panel, kGeo, vec_one, kQueueActor,
                                           mi_panel(770, 20, wig::MouseButton::kLeft))};
    check(res.vec_orders.size() == 1 && res.vec_orders[0].str_order == "PauseProduction" &&
              res.vec_orders[0].u4_extra_data == 0,
          "panel: left click paused item resumes");
  }

  {
    wig::ProductionQueueView queue{make_vehicle_queue()};
    queue.vec_queue.push_back(
        wig::ProductionItemView{.str_item = "jeep", .int4_total_time = 60, .int4_remaining_time = 0,
                                .int4_total_cost = 500, .int4_remaining_cost = 0, .b_started = true,
                                .b_done = true});
    const std::vector<wig::ProductionQueueView> vec_one{queue};
    const auto res{wig::panel_handle_mouse(panel, kGeo, vec_one, kQueueActor,
                                           mi_panel(770, 20, wig::MouseButton::kRight))};
    check(res.vec_orders.size() == 1 && res.vec_orders[0].str_order == "CancelProduction" &&
              res.vec_orders[0].u4_extra_data == 1,
          "panel: right click done item cancels");
    check(wig::panel_item_progress_percent(queue.vec_queue[0]) == 100,
          "panel: done item progress 100");
  }

  {
    wig::ProductionQueueView queue{make_vehicle_queue()};
    queue.vec_queue.push_back(
        wig::ProductionItemView{.str_item = "jeep", .int4_total_time = 1, .int4_remaining_time = 1,
                                .int4_total_cost = 500, .int4_remaining_cost = 500});
    queue.vec_queue.push_back(
        wig::ProductionItemView{.str_item = "1tnk", .int4_total_time = 1, .int4_remaining_time = 1,
                                .int4_total_cost = 700, .int4_remaining_cost = 700});
    const std::vector<wig::ProductionQueueView> vec_two{queue};
    const auto res{wig::panel_handle_mouse(panel, kGeo, vec_two, kQueueActor,
                                           mi_panel(770, 20, wig::MouseButton::kRight,
                                                    wig::MouseInputEvent::kDown,
                                                    wig::ModifierKeys::kCtrl))};
    check(res.vec_orders.size() == 1 && res.vec_orders[0].str_order == "CancelProduction" &&
              res.vec_orders[0].u4_extra_data == 2,
          "panel: ctrl right cancels with whole queue length");
  }

  {
    wig::ProductionQueueView queue{make_vehicle_queue()};
    queue.vec_items = {"fact"};
    queue.vec_queue.push_back(wig::ProductionItemView{.str_item = "fact", .b_done = true,
                                                      .b_building = true});
    const std::vector<wig::ProductionQueueView> vec_one{queue};
    const auto res{wig::panel_handle_mouse(panel, kGeo, vec_one, kQueueActor,
                                           mi_panel(770, 20, wig::MouseButton::kLeft))};
    check(res.vec_orders.empty(),
          "panel: done building left click is a no-op (place mode out of scope)");
  }

  {
    const auto res{wig::panel_handle_mouse(panel, kGeo, vec_queues, kQueueActor,
                                           mi_panel(770, 20, wig::MouseButton::kLeft,
                                                    wig::MouseInputEvent::kUp))};
    check(res.b_handled && res.vec_orders.empty(),
          "panel: up events are swallowed without action");
  }
}

std::int32_t order_counter(const ww::WorldModel& w, std::uint32_t u4_actor) {
  for (const std::uint32_t u4_handle : w.vec_actors_all[u4_actor].vec_trait_handles)
    if (w.vec_traits[u4_handle].str_kind == "OrderCounterProbe")
      return w.vec_traits[u4_handle].int4_counter;
  return -1;
}

void test_input_pipeline_and_sync() {
  auto res_world{make_world()};
  check(res_world.has_value(), "pipeline: world creates");
  if (!res_world.has_value()) return;
  ww::WorldModel w{std::move(*res_world)};

  net::OrderManagerModel om;
  const std::int32_t arr_clients[]{1};
  net::order_manager_start_game(om, w, arr_clients);
  check(net::run_frame(om, w), "pipeline: first frame ticks");

  const std::int32_t int4_hash0{ww::sync_hash_upstream(w)};
  const std::string str_digest0{ww::state_digest(w)};

  wig::SelectionModel sel;
  wig::DragState drag;
  wig::selection_handle_mouse(w, kViewerPlayer, sel, drag, mi_left(252, 252));
  wig::selection_handle_mouse(w, kViewerPlayer, sel, drag,
                              mi_left(252, 252, wig::MouseInputEvent::kUp));
  check(sel.vec_actor_ids == std::vector<std::uint32_t>{kTankA},
        "pipeline: click select via state machine");
  wig::control_group_create(sel, w, kViewerPlayer, 0);

  drag.b_dragging = true;
  drag.int4_start_x = 100;
  drag.int4_start_y = 100;
  drag.int4_mouse_x = 500;
  drag.int4_mouse_y = 400;

  wig::ProductionPanelModel panel;
  wig::ProductionQueueView queue{make_vehicle_queue()};
  const std::vector<wig::ProductionQueueView> vec_queues{queue};
  const auto res_tab{wig::panel_handle_mouse(panel, wig::PanelGeometry{960, 600}, vec_queues,
                                             kViewerPlayerActor,
                                             mi_panel(740, 112, wig::MouseButton::kLeft))};

  check(ww::sync_hash_upstream(w) == int4_hash0,
        "pipeline: selection/control group/drag/panel state stays out of sync hash");
  check(ww::state_digest(w) == str_digest0,
        "pipeline: local input state invisible in state digest");
  check(res_tab.b_handled && panel.str_current_type == "Vehicle",
        "pipeline: panel tab switched in same block");

  drag = wig::DragState{};

  wig::selection_combine(sel, std::vector<std::uint32_t>{kTankA, kTankB}, false, false);
  const std::vector<net::OrderModel> vec_orders{wig::order_generator_handle_mouse(
      w, kViewerPlayer, sel, drag,
      wig::MouseInput{.event = wig::MouseInputEvent::kUp,
                      .button = wig::MouseButton::kRight,
                      .int4_x = 700,
                      .int4_y = 700})};
  check(vec_orders.size() == 3, "pipeline: right click produces three orders");
  for (const net::OrderModel& order : vec_orders) net::issue_order(om, order);

  check(ww::sync_hash_upstream(w) == int4_hash0 && ww::state_digest(w) == str_digest0,
        "pipeline: issued orders change nothing before their applied frame");
  check(order_counter(w, kTankA) == 0 && order_counter(w, kTankB) == 0,
        "pipeline: no order applied in issue frame");

  check(net::run_frame(om, w), "pipeline: send frame ticks");
  check(order_counter(w, kTankA) == 0 && order_counter(w, kTankB) == 0,
        "pipeline: orders projected to next frame stay unapplied");

  check(net::run_frame(om, w), "pipeline: apply frame ticks");
  check(order_counter(w, kTankA) == 1 && order_counter(w, kTankB) == 1,
        "pipeline: Move orders applied on next frame (N issues, N+1 applies)");
  check(om.str_fatal.empty() && !om.b_out_of_sync,
        "pipeline: CreateGroup without handler passes through silently");
  check(ww::state_digest(w) != str_digest0,
        "pipeline: applied orders are visible in state digest");

  {
    wig::SelectionModel sel_one;
    wig::selection_combine(sel_one, std::vector<std::uint32_t>{kTankA}, false, false);
    const std::vector<net::OrderModel> vec_attack{wig::order_generator_handle_mouse(
        w, kViewerPlayer, sel_one, drag,
        wig::MouseInput{.event = wig::MouseInputEvent::kUp,
                        .button = wig::MouseButton::kRight,
                        .int4_x = 732,
                        .int4_y = 252})};
    check(vec_attack.size() == 2 && vec_attack[1].str_order == "Attack",
          "pipeline: attack order produced");
    for (const net::OrderModel& order : vec_attack) net::issue_order(om, order);
    net::run_frame(om, w);
    net::run_frame(om, w);
    check(order_counter(w, kTankA) == 1,
          "pipeline: Attack order not consumed by Move counter probe");
    check(om.str_fatal.empty(), "pipeline: attack pipeline stays healthy");
  }
}

}

int main() {
  test_screen_map();
  test_selection_logic_point();
  test_selection_combine();
  test_selection_handle_mouse();
  test_control_groups();
  test_order_generator();
  test_production_panel_layout();
  test_production_panel_mouse();
  test_input_pipeline_and_sync();

  return ora::test::test_exit_code("input");
}
