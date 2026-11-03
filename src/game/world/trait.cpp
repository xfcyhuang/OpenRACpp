#include "game/world/trait.hpp"

#include "game/world/world.hpp"

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

std::int32_t sync_probe_hash(const TraitModel& t) {
  return t.int4_counter ^ (t.b_sync_flag ? 1 : 0);
}

}  // namespace

const std::vector<TraitSpec>& trait_registry() {
  static const std::vector<TraitSpec> vec_registry{
      TraitSpec{"TickCounter", true, false, "Start", "", create_tick_counter, tick_counter,
                nullptr},
      TraitSpec{"RngProbe", true, false, "", "", create_noop, tick_rng_probe, nullptr},
      TraitSpec{"SyncProbe", true, true, "", "", create_noop, tick_sync_probe, sync_probe_hash},
      TraitSpec{"SelfDestructProbe", true, false, "At", "", create_noop, tick_self_destruct,
                nullptr},
      TraitSpec{"SpawnerProbe", true, false, "Interval", "Type", create_noop, tick_spawner,
                nullptr},
      TraitSpec{"FactionInfo", false, false, "", "", create_noop, nullptr, nullptr},
  };
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

}  // namespace ora::game::world
