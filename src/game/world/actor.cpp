#include "game/world/actor.hpp"

#include "game/world/world.hpp"

namespace ora::game::world {

const ActorModel* world_actor_by_id(const WorldModel& w, std::uint32_t u4_id) {
  if (u4_id >= w.vec_actors_all.size()) return nullptr;
  const ActorModel& actor{w.vec_actors_all[u4_id]};
  return actor.b_in_world ? &actor : nullptr;
}

ActorModel* world_actor_by_id(WorldModel& w, std::uint32_t u4_id) {
  return const_cast<ActorModel*>(world_actor_by_id(static_cast<const WorldModel&>(w), u4_id));
}

}
