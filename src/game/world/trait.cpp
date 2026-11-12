module ora.game.world;

import std;
import ora.game;
import ora.game.miniyaml;
import ora.game.pathfinding;
import ora.game.support;

namespace ora::game::world {

namespace {

void create_noop(const TraitCreateCtx&, TraitModel&) {}

void create_tick_counter(const TraitCreateCtx&, TraitModel& t) { t.int4_counter = t.int4_param; }

void tick_counter(const TraitTickCtx&, TraitModel& t) { ++t.int4_counter; }

void tick_rng_probe(const TraitTickCtx& ctx, TraitModel& t) {
  t.int4_counter = support::next(ctx.ref_world.rng_shared);
}

void tick_sync_probe(const TraitTickCtx&, TraitModel& t) {
  ++t.int4_counter;
  t.b_sync_flag = !t.b_sync_flag;
}

void tick_self_destruct(const TraitTickCtx& ctx, TraitModel& t) {
  ++t.int4_counter;
  if (t.int4_param > 0 && t.int4_counter == t.int4_param)
    dispose_actor(ctx.ref_world, ctx.ref_actor.u4_id);
}

void tick_spawner(const TraitTickCtx& ctx, TraitModel& t) {
  ++t.int4_counter;
  if (t.int4_param > 0 && t.int4_counter % t.int4_param == 0) {
    auto res_spawn{create_actor(ctx.ref_world, t.str_param_type, ctx.ref_actor.u4_owner,
                                ctx.ref_actor.int4_pos_x + t.int4_counter / t.int4_param,
                                ctx.ref_actor.int4_pos_y, true)};
    if (!res_spawn.has_value()) {
      std::println("world: spawner actor creation failed: {}", res_spawn.error());
      std::abort();
    }
  }
}

std::int32_t sync_probe_hash(const WorldModel&, std::uint32_t, const TraitModel& t) {
  return t.int4_counter ^ (t.b_sync_flag ? 1 : 0);
}

void resolve_order_counter(const TraitResolveCtx& ctx, TraitModel& t) {
  if (ctx.ref_order.str_order == "Move") ++t.int4_counter;
}

void create_selectable(const TraitCreateCtx&, TraitModel& t) {
  if (t.int4_param == 0) t.int4_param = 10;
}

}

const std::vector<TraitSpec>& trait_registry() {
  static const std::vector<TraitSpec> vec_registry{
      [] {
        std::vector<TraitSpec> vec{TraitSpec{.str_kind = "TickCounter",
                                             .b_implements_tick = true,
                                             .str_int_param_key = "Start",
                                             .fn_create = create_tick_counter,
                                             .fn_tick = tick_counter},
                                   TraitSpec{.str_kind = "RngProbe",
                                             .b_implements_tick = true,
                                             .fn_create = create_noop,
                                             .fn_tick = tick_rng_probe},
                                   TraitSpec{.str_kind = "SyncProbe",
                                             .b_implements_tick = true,
                                             .b_implements_sync = true,
                                             .fn_create = create_noop,
                                             .fn_tick = tick_sync_probe,
                                             .fn_sync_hash = sync_probe_hash},
                                   TraitSpec{.str_kind = "SelfDestructProbe",
                                             .b_implements_tick = true,
                                             .str_int_param_key = "At",
                                             .fn_create = create_noop,
                                             .fn_tick = tick_self_destruct},
                                   TraitSpec{.str_kind = "SpawnerProbe",
                                             .b_implements_tick = true,
                                             .str_int_param_key = "Interval",
                                             .str_str_param_key = "Type",
                                             .fn_create = create_noop,
                                             .fn_tick = tick_spawner},
                                   TraitSpec{.str_kind = "OrderCounterProbe",
                                             .b_implements_resolve = true,
                                             .fn_create = create_noop,
                                             .fn_resolve = resolve_order_counter},
                                   TraitSpec{.str_kind = "FactionInfo", .fn_create = create_noop},
                                   TraitSpec{.str_kind = "Selectable",
                                             .str_int_param_key = "Priority",
                                             .str_str_param_key = "Bounds",
                                             .str_str_param2_key = "Class",
                                             .str_str_param3_key = "PriorityModifiers",
                                             .fn_create = create_selectable},
                                   TraitSpec{.str_kind = "Targetable", .fn_create = create_noop},
                                   TraitSpec{.str_kind = "AttackMoveProbe", .fn_create = create_noop},
                                   TraitSpec{.str_kind = "MobileProbe",
                                             .str_order_id = "Move",
                                             .int4_order_priority = 4,
                                             .b_order_terrain_target = true,
                                             .fn_create = create_noop},
                                   TraitSpec{.str_kind = "AttackProbe",
                                             .str_order_id = "Attack",
                                             .int4_order_priority = 6,
                                             .b_order_actor_target = true,
                                             .fn_create = create_noop}};
        for (const TraitSpec& spec : production::production_trait_specs()) vec.push_back(spec);
        for (const TraitSpec& spec : economy::economy_trait_specs()) vec.push_back(spec);
        for (const TraitSpec& spec : movement_trait_specs()) vec.push_back(spec);
        for (const TraitSpec& spec : bot_trait_specs()) vec.push_back(spec);
        return vec;
      }()};
  return vec_registry;
}

const TraitSpec* trait_spec_find(std::string_view str_kind) {
  for (const TraitSpec& spec : trait_registry())
    if (spec.str_kind == str_kind) return &spec;
  return nullptr;
}

std::pair<std::string_view, std::string_view> trait_key_split(std::string_view str_key) {
  const std::size_t idx_at{str_key.find('@')};
  if (idx_at == std::string_view::npos) return {str_key, std::string_view{}};
  return {str_key.substr(0, idx_at), str_key.substr(idx_at + 1)};
}

}
