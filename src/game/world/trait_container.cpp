module ora.game.world;

import std;

namespace ora::game::world {

namespace {

std::size_t binary_search_many(const std::vector<std::uint32_t>& vec_ids,
                               std::uint32_t u4_search_for) {
  std::size_t idx_start{0};
  std::size_t idx_end{vec_ids.size()};
  while (idx_start != idx_end) {
    const std::size_t idx_mid{(idx_start + idx_end) / 2};
    if (vec_ids[idx_mid] < u4_search_for)
      idx_start = idx_mid + 1;
    else
      idx_end = idx_mid;
  }
  return idx_start;
}

}

void container_add(TraitContainerModel& container, std::uint32_t u4_actor_id,
                   std::uint32_t u4_trait_handle) {
  const std::size_t idx_insert{binary_search_many(container.vec_actor_ids, u4_actor_id + 1)};
  container.vec_actor_ids.insert(container.vec_actor_ids.begin() +
                                     static_cast<std::ptrdiff_t>(idx_insert),
                                 u4_actor_id);
  container.vec_trait_handles.insert(container.vec_trait_handles.begin() +
                                         static_cast<std::ptrdiff_t>(idx_insert),
                                     u4_trait_handle);
}

void container_remove_actor(TraitContainerModel& container, std::uint32_t u4_actor_id) {
  if (container.vec_actor_ids.empty()) return;
  const std::size_t idx_start{binary_search_many(container.vec_actor_ids, u4_actor_id)};
  if (idx_start >= container.vec_actor_ids.size() ||
      container.vec_actor_ids[idx_start] != u4_actor_id)
    return;

  std::size_t idx_end{idx_start + 1};
  while (idx_end < container.vec_actor_ids.size() &&
         container.vec_actor_ids[idx_end] == u4_actor_id)
    ++idx_end;

  container.vec_actor_ids.erase(container.vec_actor_ids.begin() +
                                    static_cast<std::ptrdiff_t>(idx_start),
                                container.vec_actor_ids.begin() +
                                    static_cast<std::ptrdiff_t>(idx_end));
  container.vec_trait_handles.erase(container.vec_trait_handles.begin() +
                                        static_cast<std::ptrdiff_t>(idx_start),
                                    container.vec_trait_handles.begin() +
                                        static_cast<std::ptrdiff_t>(idx_end));
}

}
