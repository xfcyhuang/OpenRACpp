import std;

import ora.game;
import ora.game.input;
import ora.game.miniyaml;
import ora.game.pathfinding;
import ora.game.support;
import ora.game.world;
import ora.test.util;
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
    "\t\tBounds: 512,512\n"
    "probe.bunker:\n"
    "\tSelectable:\n"
    "\t\tPriority: 1\n"
    "\t\tBounds: 1024,1024\n"
    "\t\tPriorityModifiers: Ctrl\n"
    "probe.tank2:\n"
    "\tSelectable:\n"
    "\t\tBounds: 512,512\n"
    "\t\tClass: armor\n"
    "probe.altunit:\n"
    "\tSelectable:\n"
    "\t\tPriority: 2\n"
    "\t\tBounds: 512,512\n"
    "\t\tPriorityModifiers: Alt\n"
    "probe.ectrl:\n"
    "\tSelectable:\n"
    "\t\tPriority: 1\n"
    "\t\tBounds: 512,512\n"
    "\t\tPriorityModifiers: Ctrl\n"
    "probe.am:\n"
    "\tSelectable:\n"
    "\t\tBounds: 512,512\n"
    "\tAttackMoveProbe:\n"};

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
    "\t\tOwner: Neutral\n"
    "\tbunker0: probe.bunker\n"
    "\t\tLocation: 10,10\n"
    "\t\tOwner: Neutral\n"
    "\ttank2: probe.tank2\n"
    "\t\tLocation: 10,16\n"
    "\t\tOwner: Neutral\n"
    "\talt0: probe.altunit\n"
    "\t\tLocation: 25,15\n"
    "\t\tOwner: Neutral\n"
    "\tectrl0: probe.ectrl\n"
    "\t\tLocation: 25,18\n"
    "\t\tOwner: P1\n"
    "\ttankc: probe.tank\n"
    "\t\tLocation: 35,16\n"
    "\t\tOwner: Neutral\n"
    "\tam0: probe.am\n"
    "\t\tLocation: 35,10\n"
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
constexpr std::uint32_t kBunker{13};
constexpr std::uint32_t kTank2{14};
constexpr std::uint32_t kAltUnit{15};
constexpr std::uint32_t kEnemyCtrl{16};
constexpr std::uint32_t kTankC{17};
constexpr std::uint32_t kAttackMoveUnit{18};
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
                        wig::ModifierKeys mods = wig::ModifierKeys::kNone,
                        std::int32_t int4_tap_count = 1) {
  return wig::MouseInput{.event = event,
                         .button = wig::MouseButton::kLeft,
                         .int4_x = int4_x,
                         .int4_y = int4_y,
                         .mods = mods,
                         .int4_tap_count = int4_tap_count};
}

void test_screen_map() {
  auto res_world{make_world()};
  check(res_world.has_value(), "screen_map: world creates");
  if (!res_world.has_value()) {
    std::println("world error: {}", res_world.error());
    return;
  }
  const ww::WorldModel& w{*res_world};

  check(w.vec_actors_all.size() == 19 && w.vec_actors_all[kTankA].str_type == "probe.tank" &&
            w.vec_actors_all[kTankB].str_type == "probe.tank" &&
            w.vec_actors_all[kTruck].str_type == "probe.truck" &&
            w.vec_actors_all[kEnemyTank].u4_owner == 2 &&
            w.vec_actors_all[kNeutralWall].u4_owner == 1 &&
            w.vec_actors_all[kHeavy].str_type == "probe.heavy" &&
            w.vec_actors_all[kDeco].str_type == "probe.deco" &&
            w.vec_actors_all[kDeco].vec_trait_handles.size() == 1 &&
            w.vec_actors_all[kBunker].str_type == "probe.bunker" &&
            w.vec_actors_all[kAttackMoveUnit].str_type == "probe.am",
        "screen_map: actor layout ids 5..18 by map yaml order");

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
    check(vec_hits.size() == 3 && vec_hits[0].u4_actor_id == kTankA &&
              vec_hits[1].u4_actor_id == kHeavy && vec_hits[2].u4_actor_id == kBunker,
        "screen_map: point at tanka center also inside heavy and bunker bounds");
  }
  {
    const std::vector<wig::ActorBoundsPair> vec_hits{wig::actors_at_mouse(w, 100, 100)};
    check(vec_hits.empty(), "screen_map: point in empty terrain hits nothing");
  }
  {
    const std::vector<wig::ActorBoundsPair> vec_hits{wig::actors_in_box(
        w, wig::PxRect{240, 240, 258, 24})};
    check(vec_hits.size() == 4 && vec_hits[0].u4_actor_id == kTankA &&
              vec_hits[1].u4_actor_id == kTankB && vec_hits[2].u4_actor_id == kHeavy &&
              vec_hits[3].u4_actor_id == kBunker,
        "screen_map: box over tanka/tankb/heavy/bunker intersects all four in actor id order");
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

// —— 用例 3：tap_history 三槽状态机（注入时钟黄金序列，MultiTapDetection.cs:45-83） ——
void test_tap_history_state_machine() {
  wig::TapHistoryState st;
  wig::tap_history_init(st, 1000);

  // 首击（init 槽在 now-1000 → 必判不近）。
  check(wig::tap_history_detect(st, 1000, 100, 100) == 1, "tap: first tap counts 1");
  // 249ms 后同点 → 2；再 249ms → 3。
  check(wig::tap_history_detect(st, 1249, 100, 100) == 2, "tap: +249ms same spot counts 2");
  check(wig::tap_history_detect(st, 1498, 100, 100) == 3, "tap: +249ms again counts 3");
  // 相邻对链式语义：第三对超窗只重置为 1，下一击与上一击成对判 2。
  check(wig::tap_history_detect(st, 1800, 100, 100) == 1,
        "tap: 302ms gap resets to 1 (adjacent-pair chain, not since-first)");
  check(wig::tap_history_detect(st, 1900, 100, 100) == 2, "tap: next tap pairs with previous");

  // 250ms 边界：< 250 才算近，恰 250 不算。
  wig::tap_history_init(st, 0);
  check(wig::tap_history_detect(st, 0, 50, 50) == 1, "tap: seed first tap");
  check(wig::tap_history_detect(st, 250, 50, 50) == 1, "tap: exactly 250ms is not close");
  wig::tap_history_init(st, 0);
  check(wig::tap_history_detect(st, 0, 50, 50) == 1, "tap: seed first tap (time)");
  check(wig::tap_history_detect(st, 249, 50, 50) == 2, "tap: 249ms is close");

  // 位移边界：欧氏长度 <4（dx²+dy²<16）。(0,4)=16 → 不近；(2,2)=8 → 近；(3,3)=18 → 不近。
  wig::tap_history_init(st, 0);
  check(wig::tap_history_detect(st, 0, 50, 50) == 1, "tap: seed first tap (disp 4)");
  check(wig::tap_history_detect(st, 10, 50, 54) == 1, "tap: 4px displacement is not close");
  wig::tap_history_init(st, 0);
  check(wig::tap_history_detect(st, 0, 50, 50) == 1, "tap: seed first tap (disp 2.83)");
  check(wig::tap_history_detect(st, 10, 52, 52) == 2, "tap: 2.83px displacement is close");
  wig::tap_history_init(st, 0);
  check(wig::tap_history_detect(st, 0, 50, 50) == 1, "tap: seed first tap (disp 4.24)");
  check(wig::tap_history_detect(st, 10, 53, 53) == 1, "tap: 4.24px displacement is not close");

  // Down/Up 相位：Up 走 Info（不移位、回放上次 Down 计数）。
  wig::tap_history_init(st, 0);
  check(wig::tap_history_detect(st, 0, 10, 10) == 1, "tap: down 1");
  check(wig::tap_history_info(st) == 1, "tap: up reads last down count (1)");
  check(wig::tap_history_detect(st, 10, 10, 10) == 2, "tap: down 2");
  check(wig::tap_history_info(st) == 2, "tap: up reads 2 without shifting history");
  check(wig::tap_history_info(st) == 2, "tap: repeated info stays 2 (no double shift)");

  // 分桶：鼠标按按钮、键盘按 (keycode, mods)。
  wig::TapHistoryMouse taps_mouse;
  check(wig::tap_detect_mouse(taps_mouse, 1, 0, 5, 5) == 1, "tap: mouse left down 1");
  check(wig::tap_detect_mouse(taps_mouse, 3, 10, 5, 5) == 1, "tap: right button own bucket 1");
  check(wig::tap_detect_mouse(taps_mouse, 1, 20, 5, 5) == 2, "tap: left again counts 2");
  check(wig::tap_info_mouse(taps_mouse, 3) == 1, "tap: right bucket info unaffected");

  wig::TapHistoryKeyboard taps_key;
  check(wig::tap_detect_keyboard(taps_key, wig::Keycode::kNumber1, wig::ModifierKeys::kNone, 0) ==
            1,
        "tap: keyboard bucket NUMBER_1 None 1");
  check(wig::tap_detect_keyboard(taps_key, wig::Keycode::kNumber1,
                                 wig::ModifierKeys::kNone, 10) == 2,
        "tap: same bucket double counts 2");
  check(wig::tap_detect_keyboard(taps_key, wig::Keycode::kNumber1, wig::ModifierKeys::kCtrl, 20) ==
            1,
        "tap: Ctrl+1 separate bucket counts 1 (no cross-bucket bleed)");
  check(wig::tap_detect_keyboard(taps_key, wig::Keycode::kNumber2, wig::ModifierKeys::kNone, 30) ==
            1,
        "tap: different key separate bucket counts 1");
}

// —— 用例 1/2：双击选同类（WIC :148-168） ——
void test_double_click_select_by_type() {
  auto res_world{make_world()};
  check(res_world.has_value(), "dblclick: world creates");
  if (!res_world.has_value()) return;
  const ww::WorldModel& w{*res_world};
  wig::SelectionModel sel;
  wig::DragState drag;

  // 单击选中（tap=1 不触发同类选择）。
  wig::selection_handle_mouse(w, kViewerPlayer, sel, drag, mi_left(252, 252));
  wig::selection_handle_mouse(w, kViewerPlayer, sel, drag,
                              mi_left(252, 252, wig::MouseInputEvent::kUp));
  check(sel.vec_actor_ids == std::vector<std::uint32_t>{kTankA}, "dblclick: single tap selects one");

  // 双击 tanka → 视口（全图）内同 Class 同阵营：tanka/tankb/tankc（e1 是敌方被
  // eligible 过滤，truck/wall/am 类不同）。
  wig::selection_handle_mouse(w, kViewerPlayer, sel, drag, mi_left(252, 252));
  wig::selection_handle_mouse(w, kViewerPlayer, sel, drag,
                              mi_left(252, 252, wig::MouseInputEvent::kUp,
                                      wig::ModifierKeys::kNone, 2));
  check(sel.vec_actor_ids == std::vector<std::uint32_t>({kTankA, kTankB, kTankC}),
        "dblclick: double tap on tank selects same-class same-owner actors on screen");

  // tap=3 同 tap=2 路径（>=2）。
  wig::selection_clear(sel);
  wig::selection_handle_mouse(w, kViewerPlayer, sel, drag, mi_left(252, 252));
  wig::selection_handle_mouse(w, kViewerPlayer, sel, drag,
                              mi_left(252, 252, wig::MouseInputEvent::kUp,
                                      wig::ModifierKeys::kNone, 3));
  check(sel.vec_actor_ids == std::vector<std::uint32_t>({kTankA, kTankB, kTankC}),
        "dblclick: tap=3 takes the same multi-click path");

  // Shift 与双击并存：isCombine 硬编码 true（不切换）。
  wig::selection_clear(sel);
  wig::selection_handle_mouse(w, kViewerPlayer, sel, drag,
                              mi_left(252, 252, wig::MouseInputEvent::kDown,
                                      wig::ModifierKeys::kShift));
  wig::selection_handle_mouse(w, kViewerPlayer, sel, drag,
                              mi_left(252, 252, wig::MouseInputEvent::kUp,
                                      wig::ModifierKeys::kShift, 2));
  check(sel.vec_actor_ids == std::vector<std::uint32_t>({kTankA, kTankB, kTankC}),
        "dblclick: shift-double-click still combines (isCombine hardcoded true)");

  // 自定义 Class：双击 tank2（Class: armor）→ armor 集只有 tank2。
  wig::selection_clear(sel);
  wig::selection_handle_mouse(w, kViewerPlayer, sel, drag, mi_left(252, 396));
  wig::selection_handle_mouse(w, kViewerPlayer, sel, drag,
                              mi_left(252, 396, wig::MouseInputEvent::kUp,
                                      wig::ModifierKeys::kNone, 2));
  check(sel.vec_actor_ids == std::vector<std::uint32_t>{kTank2},
        "dblclick: custom Class groups by declared class (armor has one member)");

  // 视口限制：viewport 只盖左半 → (35,16) 的 tankc 排除。
  wig::selection_clear(sel);
  wig::selection_handle_mouse_in_view(w, kViewerPlayer, sel, drag, mi_left(252, 252),
                                      wig::PxRect{0, 0, 600, 400});
  wig::selection_handle_mouse_in_view(w, kViewerPlayer, sel, drag,
                                      mi_left(252, 252, wig::MouseInputEvent::kUp,
                                              wig::ModifierKeys::kNone, 2),
                                      wig::PxRect{0, 0, 600, 400});
  check(sel.vec_actor_ids == std::vector<std::uint32_t>({kTankA, kTankB}),
        "dblclick: viewport-limited double click excludes off-screen same-class actor");

  // 观战者：eligible = 全体 → 敌方同类也被框入（选中不含己方限制）。
  constexpr std::uint32_t kSpectator{3};
  check(w.vec_players[kSpectator].b_spectating && w.vec_players[kSpectator].b_non_combatant,
        "dblclick: Everyone spectator player present");
  wig::SelectionModel sel_spec;
  wig::selection_handle_mouse_in_view(w, kSpectator, sel_spec, drag, mi_left(252, 252),
                                      wig::PxRect{0, 0, 10000, 10000});
  wig::selection_handle_mouse_in_view(w, kSpectator, sel_spec, drag,
                                      mi_left(252, 252, wig::MouseInputEvent::kUp,
                                              wig::ModifierKeys::kNone, 2),
                                      wig::PxRect{0, 0, 10000, 10000});
  check(sel_spec.vec_actor_ids == std::vector<std::uint32_t>({kTankA, kTankB, kEnemyTank, kTankC}),
        "dblclick: spectator double click includes enemy same-class actor");

  // 空地双击：无命中 → 选区不变。
  wig::selection_combine(sel, std::vector<std::uint32_t>{kTankA}, false, false);
  wig::selection_handle_mouse(w, kViewerPlayer, sel, drag, mi_left(100, 100));
  wig::selection_handle_mouse(w, kViewerPlayer, sel, drag,
                              mi_left(100, 100, wig::MouseInputEvent::kUp,
                                      wig::ModifierKeys::kNone, 2));
  check(sel.vec_actor_ids == std::vector<std::uint32_t>{kTankA},
        "dblclick: double click on empty ground keeps selection");
}

// —— 用例 4/5/6：Ctrl/Alt 越权（SelectableExts.cs:50-61 互斥 + 关系扣减叠加） ——
void test_priority_modifiers() {
  auto res_world{make_world()};
  check(res_world.has_value(), "mods: world creates");
  if (!res_world.has_value()) return;
  const ww::WorldModel& w{*res_world};

  // 越权在基础值上置 MaxValue，关系扣减仍在。
  check(wig::selection_priority(w, w.vec_actors_all[kBunker], kViewerPlayer,
                                wig::ModifierKeys::kCtrl) == std::numeric_limits<std::int32_t>::max(),
        "mods: own ctrl-flag actor gets int.MaxValue under Ctrl");
  check(wig::selection_priority(w, w.vec_actors_all[kEnemyCtrl], kViewerPlayer,
                                wig::ModifierKeys::kCtrl) ==
            std::numeric_limits<std::int32_t>::max() - 90,
        "mods: enemy ctrl-flag actor gets MaxValue-90 (deduction still applies)");
  check(wig::selection_priority(w, w.vec_actors_all[kBunker], kViewerPlayer,
                                wig::ModifierKeys::kCtrl | wig::ModifierKeys::kAlt) == 1,
        "mods: Ctrl+Alt together grants no override (mutual exclusion)");
  check(wig::selection_priority(w, w.vec_actors_all[kBunker], kViewerPlayer,
                                wig::ModifierKeys::kAlt) == 1,
        "mods: Alt does not override an actor that only declares Ctrl");
  check(wig::selection_priority(w, w.vec_actors_all[kAltUnit], kViewerPlayer,
                                wig::ModifierKeys::kAlt) == std::numeric_limits<std::int32_t>::max(),
        "mods: alt-flag actor overrides under Alt");

  // 点选：裸点 tanka 胜（10 vs bunker 1）；Ctrl 点 bunker 胜；Ctrl+Alt 点回 tanka。
  check(wig::select_highest_priority_at_point(w, kViewerPlayer, 252, 252,
                                              wig::ModifierKeys::kNone) == kTankA,
        "mods: bare click picks normal tank over priority-1 bunker");
  check(wig::select_highest_priority_at_point(w, kViewerPlayer, 252, 252,
                                              wig::ModifierKeys::kCtrl) == kBunker,
        "mods: Ctrl click picks the ctrl-flag bunker");
  check(wig::select_highest_priority_at_point(w, kViewerPlayer, 252, 252,
                                              wig::ModifierKeys::kCtrl |
                                                  wig::ModifierKeys::kAlt) == kTankA,
        "mods: Ctrl+Alt click picks normal tank (no override)");

  // 框选：只取最高优先组。alt0(612,372) 与 tankb(492,252) 同框：
  // 裸框 → 兵组（10）；Alt 框 → altunit 组（MaxValue）。
  {
    const std::vector<std::uint32_t> vec_bare{wig::select_actors_in_box(
        w, kViewerPlayer, 460, 240, 620, 380, wig::ModifierKeys::kNone)};
    check(vec_bare == std::vector<std::uint32_t>{kTankB},
          "mods: bare box selects own normal group");
    const std::vector<std::uint32_t> vec_alt{wig::select_actors_in_box(
        w, kViewerPlayer, 460, 240, 620, 380, wig::ModifierKeys::kAlt)};
    check(vec_alt == std::vector<std::uint32_t>{kAltUnit},
          "mods: Alt box selects the alt-flag group only");
  }

  // 敌方越权（MaxValue-90）仍胜于己方普通（10）——越权作用在基础值。
  {
    const std::vector<std::uint32_t> vec_enemy{wig::select_actors_in_box(
        w, kViewerPlayer, 600, 430, 624, 454, wig::ModifierKeys::kCtrl)};
    check(vec_enemy == std::vector<std::uint32_t>{kEnemyCtrl},
          "mods: enemy ctrl-override (MaxValue-90) still beats own normal (10) in Ctrl box");
  }
  // 但己方越权（MaxValue）胜于敌方越权（MaxValue-90）。
  {
    const std::vector<std::uint32_t> vec_both{wig::select_actors_in_box(
        w, kViewerPlayer, 240, 240, 624, 454, wig::ModifierKeys::kCtrl)};
    check(vec_both == std::vector<std::uint32_t>{kBunker},
          "mods: own ctrl-override (MaxValue) beats enemy ctrl-override (MaxValue-90)");
  }
}

// —— 用例 7/8/9：订单面 force 修饰（UOG :169-174 + AttackBase force 变体） ——
void test_force_modifier_orders() {
  auto res_world{make_world()};
  check(res_world.has_value(), "force: world creates");
  if (!res_world.has_value()) return;
  const ww::WorldModel& w{*res_world};
  wig::SelectionModel sel;
  wig::selection_combine(sel, std::vector<std::uint32_t>{kTankA}, false, false);

  const auto make_mi{[](std::int32_t int4_x, std::int32_t int4_y, wig::ModifierKeys mods) {
    return wig::MouseInput{.event = wig::MouseInputEvent::kUp,
                           .button = wig::MouseButton::kRight,
                           .int4_x = int4_x,
                           .int4_y = int4_y,
                           .mods = mods};
  }};

  // Ctrl+右键空地 → ForceAttack(cell)。
  {
    const std::vector<net::OrderModel> vec_orders{wig::unit_order_generator_order(
        w, kViewerPlayer, sel, 29, 29, 700, 700, make_mi(700, 700, wig::ModifierKeys::kCtrl))};
    check(vec_orders.size() == 2 && vec_orders[0].str_order == "CreateGroup" &&
              vec_orders[1].str_order == "ForceAttack" &&
              vec_orders[1].target.kind == net::TargetKind::kTerrain &&
              vec_orders[1].target.b_cell &&
              vec_orders[1].target.int4_cell_bits == wig::cpos_bits(29, 29, 0) &&
              !vec_orders[1].b_queued,
          "force: ctrl right-click ground yields ForceAttack with cell target");
    check(to_hex(net::serialize_order(vec_orders[1])) ==
              "ff0b466f72636541747461636bc1000500000002001dd00100",
          "force: ForceAttack cell hex matches T08 serialize");
  }

  // Ctrl+右键敌单位 → ForceAttack(FromActor)。
  {
    const std::vector<net::OrderModel> vec_orders{wig::unit_order_generator_order(
        w, kViewerPlayer, sel, 30, 10, 732, 252, make_mi(732, 252, wig::ModifierKeys::kCtrl))};
    check(vec_orders.size() == 2 && vec_orders[1].str_order == "ForceAttack" &&
              vec_orders[1].target.kind == net::TargetKind::kActor &&
              vec_orders[1].target.u4_actor == kEnemyTank &&
              vec_orders[1].target.int4_generation == 0,
          "force: ctrl right-click enemy actor yields ForceAttack with actor target");
    check(to_hex(net::serialize_order(vec_orders[1])) ==
              "ff0b466f72636541747461636b810005000000010800000000000000",
          "force: ForceAttack actor hex matches T08 serialize");
  }

  // Alt+右键敌单位 → ForceMove 令 Attack targeter 拒绝 → 第二遍 cell → Move（敌脚下）。
  {
    const std::vector<net::OrderModel> vec_orders{wig::unit_order_generator_order(
        w, kViewerPlayer, sel, 30, 10, 732, 252, make_mi(732, 252, wig::ModifierKeys::kAlt))};
    check(vec_orders.size() == 2 && vec_orders[1].str_order == "Move" &&
              vec_orders[1].target.kind == net::TargetKind::kTerrain &&
              vec_orders[1].target.int4_cell_bits == wig::cpos_bits(30, 10, 0) &&
              !vec_orders[1].b_queued,
          "force: alt right-click enemy unit moves to its cell (force-move bypass)");
  }

  // Ctrl+Shift+右键敌单位 → ForceAttack + queued 位。
  {
    const std::vector<net::OrderModel> vec_orders{wig::unit_order_generator_order(
        w, kViewerPlayer, sel, 30, 10, 732, 252,
        make_mi(732, 252, wig::ModifierKeys::kCtrl | wig::ModifierKeys::kShift))};
    check(vec_orders.size() == 2 && vec_orders[1].str_order == "ForceAttack" &&
              vec_orders[1].b_queued,
          "force: ctrl+shift right-click yields queued ForceAttack");
    check(to_hex(net::serialize_order(vec_orders[1])) ==
              "ff0b466f72636541747461636b890005000000010800000000000000",
          "force: queued ForceAttack hex has queued flag bit");
  }

  // Ctrl+Alt+右键空地：两 force 互相否决（Attack 拒 force-move）→ Move。
  {
    const std::vector<net::OrderModel> vec_orders{wig::unit_order_generator_order(
        w, kViewerPlayer, sel, 29, 29, 700, 700,
        make_mi(700, 700, wig::ModifierKeys::kCtrl | wig::ModifierKeys::kAlt))};
    check(vec_orders.size() == 2 && vec_orders[1].str_order == "Move",
          "force: ctrl+alt right-click ground falls through to Move");
  }

  // Shift+右键空地（无 Ctrl）→ 普通 Move queued（既有语义复钉）。
  {
    const std::vector<net::OrderModel> vec_orders{wig::unit_order_generator_order(
        w, kViewerPlayer, sel, 29, 29, 700, 700, make_mi(700, 700, wig::ModifierKeys::kShift))};
    check(vec_orders.size() == 2 && vec_orders[1].str_order == "Move" && vec_orders[1].b_queued,
          "force: shift right-click ground keeps queued Move");
  }

  // AttackMove 生成器：Ctrl 变体 AssaultMove、非 Ctrl AttackMove、subject 空、grouped=subjects。
  {
    wig::SelectionModel sel_am;
    wig::selection_combine(sel_am, std::vector<std::uint32_t>{kAttackMoveUnit, kTankA}, false,
                           false);
    const std::vector<net::OrderModel> vec_am{wig::attack_move_generator_order(
        w, kViewerPlayer, sel_am, 20, 20, make_mi(492, 492, wig::ModifierKeys::kNone))};
    check(vec_am.size() == 1 && vec_am[0].str_order == "AttackMove" && !vec_am[0].b_has_subject &&
              vec_am[0].target.b_cell &&
              vec_am[0].target.int4_cell_bits == wig::cpos_bits(20, 20, 0) &&
              vec_am[0].b_has_grouped && vec_am[0].vec_grouped == std::vector<std::uint32_t>{kAttackMoveUnit},
          "force: attack-move generator issues AttackMove for AttackMove units only");
    const std::vector<net::OrderModel> vec_assault{wig::attack_move_generator_order(
        w, kViewerPlayer, sel_am, 20, 20, make_mi(492, 492, wig::ModifierKeys::kCtrl))};
    check(vec_assault.size() == 1 && vec_assault[0].str_order == "AssaultMove",
          "force: ctrl variant issues AssaultMove");
    const std::vector<net::OrderModel> vec_queued{wig::attack_move_generator_order(
        w, kViewerPlayer, sel_am, 20, 20, make_mi(492, 492, wig::ModifierKeys::kShift))};
    check(vec_queued.size() == 1 && vec_queued[0].b_queued,
          "force: shift keeps attack-move queued");
  }
}

// —— 用例 13：输入层（双击/控制组键/视口键/force 单）不进 SyncHash 边界 ——
void test_hotkey_layer_sync_boundary() {
  auto res_world{make_world()};
  check(res_world.has_value(), "synCB: world creates");
  if (!res_world.has_value()) return;
  ww::WorldModel w{std::move(*res_world)};

  net::OrderManagerModel om;
  const std::int32_t arr_clients[]{1};
  net::order_manager_start_game(om, w, arr_clients);
  check(net::run_frame(om, w), "syncB: first frame ticks");

  const std::int32_t int4_hash0{ww::sync_hash_upstream(w)};
  const std::string str_digest0{ww::state_digest(w)};

  // 合成注册表（内联 yaml —— hotkey_registry_load 的非黄金路径）。
  constexpr std::string_view kHkYaml{
      "ControlGroupSelect01: NUMBER_1\n"
      "\tTypes: ControlGroups\n"
      "\tContexts: player\n"
      "ControlGroupCreate01: NUMBER_1 Ctrl\n"
      "\tTypes: ControlGroups\n"
      "\tContexts: player\n"
      "MapScrollUp: UP\n"
      "\tTypes: Viewport\n"
      "\tContexts: player, spectator\n"
      "ZoomIn: RIGHTBRACKET\n"
      "\tTypes: Viewport\n"
      "\tContexts: player, spectator\n"};
  auto res_nodes{ora::game::miniyaml_from_string(kHkYaml, "hk-inline", true)};
  check(res_nodes.has_value(), "syncB: inline hotkey yaml parses");
  if (!res_nodes.has_value()) return;
  std::vector<std::vector<ora::game::MiniYamlNode>> vec_files{std::move(*res_nodes)};
  auto res_reg{wig::hotkey_registry_load(vec_files)};
  check(res_reg.has_value(), "syncB: inline registry loads");
  if (!res_reg.has_value()) return;
  const wig::HotkeyRegistryModel reg{std::move(*res_reg)};

  wig::SelectionModel sel;
  wig::DragState drag;
  wig::selection_handle_mouse(w, kViewerPlayer, sel, drag, mi_left(252, 252));
  wig::selection_handle_mouse(w, kViewerPlayer, sel, drag,
                              mi_left(252, 252, wig::MouseInputEvent::kUp));
  wig::selection_handle_mouse(w, kViewerPlayer, sel, drag, mi_left(252, 252));
  wig::selection_handle_mouse(w, kViewerPlayer, sel, drag,
                              mi_left(252, 252, wig::MouseInputEvent::kUp,
                                      wig::ModifierKeys::kNone, 2));
  check(sel.vec_actor_ids.size() == 3, "syncB: double click selected same-class set");

  const auto res_group{wig::control_groups_handle_key(reg, sel, w, kViewerPlayer,
                                                      wig::KeyInput{.event = wig::KeyInputEvent::kDown,
                                                                    .key = wig::Keycode::kNumber1,
                                                                    .mods = wig::ModifierKeys::kCtrl})};
  check(res_group.b_handled, "syncB: control group create via registry");

  wig::ViewportState viewport;
  check(wig::viewport_handle_key(viewport, reg,
                                 wig::KeyInput{.event = wig::KeyInputEvent::kDown,
                                               .key = wig::Keycode::kUp,
                                               .mods = wig::ModifierKeys::kNone},
                                 960, 960) &&
            wig::has_scroll_direction(viewport.scroll_directions, wig::ScrollDirection::kUp),
        "syncB: MapScrollUp sets direction bit");
  check(wig::viewport_handle_key(viewport, reg,
                                 wig::KeyInput{.event = wig::KeyInputEvent::kDown,
                                               .key = wig::Keycode::kRightbracket,
                                               .mods = wig::ModifierKeys::kNone},
                                 960, 960) && viewport.f64_zoom > 1.0,
        "syncB: ZoomIn raises zoom");

  // 产出 force 单但不应用（issue 后、apply 前世界零变化）。
  const std::vector<net::OrderModel> vec_orders{wig::order_generator_handle_mouse(
      w, kViewerPlayer, sel, drag,
      wig::MouseInput{.event = wig::MouseInputEvent::kUp,
                      .button = wig::MouseButton::kRight,
                      .int4_x = 700,
                      .int4_y = 700,
                      .mods = wig::ModifierKeys::kCtrl})};
  check(vec_orders.size() == 4 && vec_orders[1].str_order == "ForceAttack" &&
              vec_orders[2].str_order == "ForceAttack" && vec_orders[3].str_order == "ForceAttack",
        "syncB: force attack produced for all three selected tanks");
  for (const net::OrderModel& order : vec_orders) net::issue_order(om, order);

  check(ww::sync_hash_upstream(w) == int4_hash0,
        "syncB: selection/double-click/control-group/viewport/tap state stays out of sync hash");
  check(ww::state_digest(w) == str_digest0,
        "syncB: input layer invisible in state digest before apply frame");

  check(net::run_frame(om, w) && net::run_frame(om, w), "syncB: two frames tick");
  check(ww::state_digest(w) != str_digest0,
        "syncB: applied force orders visible in digest");
  check(om.str_fatal.empty() && !om.b_out_of_sync, "syncB: pipeline healthy");
}

}

int main() {
  test_screen_map();
  test_selection_logic_point();
  test_selection_combine();
  test_selection_handle_mouse();
  test_tap_history_state_machine();
  test_double_click_select_by_type();
  test_priority_modifiers();
  test_force_modifier_orders();
  test_control_groups();
  test_order_generator();
  test_production_panel_layout();
  test_production_panel_mouse();
  test_input_pipeline_and_sync();
  test_hotkey_layer_sync_boundary();

  return ora::test::test_exit_code("input");
}
