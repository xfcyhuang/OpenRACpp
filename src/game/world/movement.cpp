module ora.game.world;

import std;
import ora.game;
import ora.game.miniyaml;
import ora.game.pathfinding;
import ora.game.support;

namespace ora::game::world {

namespace {

namespace pfg = ora::game::pathfinding;

// economy.cpp find_world_template 同款：actor 类型模板查找。
const TraitTemplate* find_type_template(const WorldModel& w, std::string_view str_type,
                                        std::string_view str_kind) {
  for (const ActorTypeDef& def : w.vec_actor_types) {
    if (def.str_name_lower != str_type) continue;
    for (const TraitTemplate& tpl : def.vec_templates)
      if (tpl.str_kind == str_kind) return &tpl;
    return nullptr;
  }
  return nullptr;
}

std::optional<std::int32_t> template_kv_int(const TraitTemplate& tpl,
                                            std::string_view str_key) {
  for (const auto& [str_k, str_v] : tpl.vec_kv)
    if (str_k == str_key) {
      std::int32_t int4_out{};
      const auto res{std::from_chars(str_v.data(), str_v.data() + str_v.size(), int4_out)};
      if (res.ec == std::errc{} && res.ptr == str_v.data() + str_v.size()) return int4_out;
    }
  return std::nullopt;
}

// Map.Clamp（Map.cs:1212-1215 → MPos.cs:81-86 PPos.Clamp）：格坐标钳到
// [left, right-1] x [top, bottom-1]（半开 bounds 转闭格区间）。
pfg::CPos clamp_cell(const pfg::LocomotorMap& map, pfg::CPos cell) {
  return pfg::CPos{std::min(map.int4_right - 1, std::max(cell.int4_x, map.int4_left)),
                   std::min(map.int4_bottom - 1, std::max(cell.int4_y, map.int4_top))};
}

// 无阻塞面可走判定（BlockedByActor.None：纯地形，Locomotor.cs CanMoveFreelyInto）。
bool cell_enterable(const pfg::LocomotorMap& map, pfg::CPos cell) {
  return pfg::map_contains(map, cell) &&
         pfg::movement_cost_for_cell(map, cell) != pfg::kCostUnreachable;
}

// Mobile.NearestMoveableCell（Mobile.cs:768-788）无阻塞面：目标可走即目标；
// 否则 FindTilesInAnnulus(target, 1, 10) 首个可走格；找不到原样返回（上游同）。
pfg::CPos nearest_moveable_cell(const pfg::LocomotorMap& map, pfg::CPos target) {
  if (cell_enterable(map, target)) return target;
  for (const pfg::CPos cell : pfg::find_tiles_in_annulus(map, target, 1, 10))
    if (cell_enterable(map, cell)) return cell;
  return target;
}

// Actor.QueueActivity（Actor.cs）最小等价：queued 挂 next 链尾；否则替换当前活动并
// 丢弃 next 链（上游非 queued 直接覆写 CurrentActivity，旧链整体作废）。
void queue_activity(MobileState& m, pfg::CPos cell, const bool b_queued) {
  if (b_queued && m.move.b_has_activity) {
    m.vec_queued_destinations.push_back(cell);
    return;
  }
  m.vec_queued_destinations.clear();
  pfg::move_queue(m.move, cell);
}

// 订单 target 格位（order_generator.cpp cpos_bits 同源编码：x<<20 | y<<8）。
pfg::CPos cell_of_bits(const std::int32_t int4_bits) {
  return pfg::CPos{static_cast<std::int32_t>(static_cast<std::uint32_t>(int4_bits) >> 20),
                   (static_cast<std::int16_t>(int4_bits >> 4)) >> 4};
}

void create_noop(const TraitCreateCtx&, TraitModel&) {}

// Mobile.IResolveOrder.ResolveOrder 的 "Move"（Mobile.cs:937-951）无阻塞面：
// Clamp → evaluateNearestMovableCell（Move.OnFirstRun 预处理）→ QueueActivity。
void resolve_mobile_order(const TraitResolveCtx& ctx, TraitModel&) {
  const network::OrderModel& order{ctx.ref_order};
  if (order.str_order != "Move") return;
  // Target.IsValidFor 简化面（D033 口径）：Terrain/cell 目标。
  if (order.target.kind != network::TargetKind::kTerrain || !order.target.b_cell) return;
  MobileState* ptr_mobile{movement_state_of(ctx.ref_world, ctx.ref_actor.u4_id)};
  // 非 Mobile 活动面单位（harv 归 economy FDA；无 Mobile trait）不受理。
  if (ptr_mobile == nullptr) return;
  const pfg::LocomotorMap& map{ctx.ref_world.econ.map_move};
  if (map.int4_width <= 0 || map.int4_height <= 0) return;
  const pfg::CPos cell_clamped{clamp_cell(map, cell_of_bits(order.target.int4_cell_bits))};
  queue_activity(*ptr_mobile, nearest_moveable_cell(map, cell_clamped), order.b_queued);
}

// AttackMove.IResolveOrder.ResolveOrder 的 "AttackMove"/"AssaultMove"（AttackMove.cs:66-89）
// 无攻击面：AttackMoveActivity 在无 AttackMove/AutoTarget trait 时退化为纯移动
// （AttackMoveActivity.cs:31-34 OnFirstRun 直 QueueChild(getMove())）。
void resolve_attack_move_order(const TraitResolveCtx& ctx, TraitModel&) {
  const network::OrderModel& order{ctx.ref_order};
  if (order.str_order != "AttackMove" && order.str_order != "AssaultMove") return;
  if (order.target.kind != network::TargetKind::kTerrain || !order.target.b_cell) return;
  MobileState* ptr_mobile{movement_state_of(ctx.ref_world, ctx.ref_actor.u4_id)};
  if (ptr_mobile == nullptr) return;
  const pfg::LocomotorMap& map{ctx.ref_world.econ.map_move};
  if (map.int4_width <= 0 || map.int4_height <= 0) return;
  const pfg::CPos cell_clamped{clamp_cell(map, cell_of_bits(order.target.int4_cell_bits))};
  // resolve 期先取 targetLocation（上游 AttackMove.cs:80），getMove() 内不再 evaluateNearest。
  queue_activity(*ptr_mobile, nearest_moveable_cell(map, cell_clamped), order.b_queued);
}

}  // namespace

MobileState* movement_state_of(WorldModel& w, const std::uint32_t u4_actor) {
  for (MobileState& mobile : w.vec_mobiles)
    if (mobile.u4_actor == u4_actor) return &mobile;
  return nullptr;
}

const MobileState* movement_state_of(const WorldModel& w, const std::uint32_t u4_actor) {
  for (const MobileState& mobile : w.vec_mobiles)
    if (mobile.u4_actor == u4_actor) return &mobile;
  return nullptr;
}

void movement_on_actor_added(WorldModel& w, const std::uint32_t u4_actor_id) {
  if (u4_actor_id >= w.vec_actors_all.size()) return;
  if (movement_state_of(w, u4_actor_id) != nullptr) return;
  const ActorModel& actor{w.vec_actors_all[u4_actor_id]};

  // Mobile trait 面且非 Harvester（harv 移动归 economy FDA 自治，D044 口径延续）。
  const TraitTemplate* ptr_mobile{find_type_template(w, actor.str_type, "Mobile")};
  if (ptr_mobile == nullptr) return;
  if (find_type_template(w, actor.str_type, "Harvester") != nullptr) return;

  MobileState mobile;
  mobile.u4_actor = u4_actor_id;
  mobile.int4_speed = template_kv_int(*ptr_mobile, "Speed").value_or(113);
  mobile.int4_turn_speed = template_kv_int(*ptr_mobile, "TurnSpeed").value_or(512);
  mobile.move.cell_from = pfg::CPos{actor.int4_pos_x, actor.int4_pos_y};
  mobile.move.cell_to = mobile.move.cell_from;
  mobile.move.cell_destination = mobile.move.cell_from;
  mobile.move.b_has_destination = false;
  mobile.move.pos_center = pfg::center_of_cell(mobile.move.cell_from);
  w.vec_mobiles.push_back(std::move(mobile));
}

void movement_on_actor_removed(WorldModel& w, const std::uint32_t u4_actor_id) {
  for (std::size_t idx{0}; idx < w.vec_mobiles.size(); ++idx)
    if (w.vec_mobiles[idx].u4_actor == u4_actor_id) {
      w.vec_mobiles.erase(w.vec_mobiles.begin() + static_cast<std::ptrdiff_t>(idx));
      return;
    }
}

void movement_on_tick(WorldModel& w) {
  const pfg::LocomotorMap& map{w.econ.map_move};
  if (map.int4_width <= 0 || map.int4_height <= 0) return;

  for (MobileState& mobile : w.vec_mobiles) {
    if (mobile.u4_actor >= w.vec_actors_all.size()) continue;
    ActorModel& actor{w.vec_actors_all[mobile.u4_actor]};
    if (!actor.b_in_world || actor.b_disposed) continue;

    pfg::MoveSimConfig cfg;
    cfg.ptr_map = &map;
    cfg.int4_speed = mobile.int4_speed;
    cfg.int4_speed_den = 100;
    cfg.int4_turn_speed = mobile.int4_turn_speed;
    pfg::move_sim_tick(mobile.move, cfg, w.int4_world_tick);

    // 活动完成 → next 链依序起并同帧接续（ActivityUtils.RunActivity 的 while 语义）。
    if (!mobile.move.b_has_activity && !mobile.vec_queued_destinations.empty()) {
      const pfg::CPos cell_next{mobile.vec_queued_destinations.front()};
      mobile.vec_queued_destinations.erase(mobile.vec_queued_destinations.begin());
      pfg::move_queue(mobile.move, cell_next);
      pfg::move_sim_tick(mobile.move, cfg, w.int4_world_tick);
    }

    // 位置同步（Actor.CenterPosition 面；桥承载期行为保持，选择/攻击桥/渲染消费）。
    actor.int4_pos_x = mobile.move.pos_center.int4_x / 1024;
    actor.int4_pos_y = mobile.move.pos_center.int4_y / 1024;
  }
}

void movement_queue_move(WorldModel& w, const std::uint32_t u4_actor, const pfg::CPos cell) {
  MobileState* ptr_mobile{movement_state_of(w, u4_actor)};
  if (ptr_mobile == nullptr) return;
  pfg::move_queue(ptr_mobile->move, cell);
}

const std::vector<TraitSpec>& movement_trait_specs() {
  static const std::vector<TraitSpec> vec_specs{
      TraitSpec{.str_kind = "Mobile",
                .str_str_param_key = "Locomotor",
                .fn_create = create_noop,
                .fn_resolve = resolve_mobile_order},
      TraitSpec{.str_kind = "AttackMove", .fn_create = create_noop,
                .fn_resolve = resolve_attack_move_order},
  };
  return vec_specs;
}

}  // namespace ora::game::world
