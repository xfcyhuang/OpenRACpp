#pragma once

import std;

namespace ora::game::world {

struct TraitContainerModel {
  std::vector<std::uint32_t> vec_actor_ids;
  std::vector<std::uint32_t> vec_trait_handles;
};

void container_add(TraitContainerModel& container, std::uint32_t u4_actor_id,
                   std::uint32_t u4_trait_handle);

void container_remove_actor(TraitContainerModel& container, std::uint32_t u4_actor_id);

}  // namespace ora::game::world
