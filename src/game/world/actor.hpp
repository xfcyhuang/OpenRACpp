#pragma once

import std;

namespace ora::game::world {

struct WorldModel;

constexpr std::uint32_t kOwnerNone{0xffffffffu};

struct ActorModel {
  std::uint32_t u4_id{};
  std::string str_type;
  std::uint32_t u4_owner{kOwnerNone};
  std::int32_t int4_pos_x{};
  std::int32_t int4_pos_y{};
  bool b_in_world{};
  bool b_will_dispose{};
  bool b_disposed{};
  std::vector<std::uint32_t> vec_trait_handles;
};

[[nodiscard]] const ActorModel* world_actor_by_id(const WorldModel& w, std::uint32_t u4_id);

[[nodiscard]] ActorModel* world_actor_by_id(WorldModel& w, std::uint32_t u4_id);

}  // namespace ora::game::world
