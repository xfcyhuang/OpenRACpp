#include "game/world/sync_hash.hpp"

namespace ora::game::world {

namespace {

std::int32_t hash_actor(const ActorModel& actor) {
  return static_cast<std::int32_t>(actor.u4_id << 16);
}

std::int32_t trait_sync_hash(const TraitModel& trait) {
  const TraitSpec* ptr_spec{trait_spec_find(trait.str_kind)};
  if (ptr_spec == nullptr || !ptr_spec->fn_sync_hash) return 0;
  return ptr_spec->fn_sync_hash(trait);
}

}  // namespace

std::int32_t sync_hash_upstream(const WorldModel& w) {
  std::int32_t int4_n{0};
  std::int32_t int4_ret{0};

  for (const std::uint32_t u4_id : w.vec_in_world) {
    const ActorModel& actor{w.vec_actors_all[u4_id]};
    int4_ret += int4_n * static_cast<std::int32_t>(1u + actor.u4_id) * hash_actor(actor);
    ++int4_n;
  }

  for (std::size_t idx{0}; idx < w.container_sync.vec_actor_ids.size(); ++idx) {
    const std::uint32_t u4_id{w.container_sync.vec_actor_ids[idx]};
    const TraitModel& trait{w.vec_traits[w.container_sync.vec_trait_handles[idx]]};
    int4_ret += int4_n * static_cast<std::int32_t>(1u + u4_id) * trait_sync_hash(trait);
    ++int4_n;
  }

  int4_ret += w.rng_shared.int4_last;

  return int4_ret;
}

std::string state_digest(const WorldModel& w) {
  std::string str_out;
  str_out += std::format("worldtick {}\n", w.int4_world_tick);
  str_out += std::format("netframe {}\n", w.u4_net_frame);
  str_out += std::format("nextaid {}\n", w.u4_next_aid);
  str_out += std::format("rngshared {} {}\n", w.rng_shared.int4_last,
                         w.rng_shared.int4_total_count);
  str_out += std::format("rngplayer {} {}\n", w.rng_player.int4_last,
                         w.rng_player.int4_total_count);
  str_out += std::format("worldowner {}\n", w.u4_world_owner_player);
  str_out += "players";
  for (const PlayerModel& player : w.vec_players) str_out += " " + player.str_internal_name;
  str_out += "\n";
  str_out += std::format("frameend {}\n", w.deque_frame_end.size());

  for (const std::uint32_t u4_id : w.vec_in_world) {
    const ActorModel& actor{w.vec_actors_all[u4_id]};
    str_out += std::format("actor {} {} owner {} pos {} {} willdispose {}\n", actor.u4_id,
                           actor.str_type, actor.u4_owner, actor.int4_pos_x, actor.int4_pos_y,
                           actor.b_will_dispose ? 1 : 0);
    for (const std::uint32_t u4_handle : actor.vec_trait_handles) {
      const TraitModel& trait{w.vec_traits[u4_handle]};
      str_out += std::format(" trait {}@{} counter {} param {} stype {} flag {}\n",
                             trait.str_kind, trait.str_instance, trait.int4_counter,
                             trait.int4_param, trait.str_param_type, trait.b_sync_flag ? 1 : 0);
    }
  }

  str_out += "tickcontainer";
  for (std::size_t idx{0}; idx < w.container_tick.vec_actor_ids.size(); ++idx)
    str_out += std::format(" {}:{}", w.container_tick.vec_actor_ids[idx],
                           w.container_tick.vec_trait_handles[idx]);
  str_out += "\n";
  str_out += "synccontainer";
  for (std::size_t idx{0}; idx < w.container_sync.vec_actor_ids.size(); ++idx)
    str_out += std::format(" {}:{}", w.container_sync.vec_actor_ids[idx],
                           w.container_sync.vec_trait_handles[idx]);
  str_out += "\n";
  return str_out;
}

}  // namespace ora::game::world
