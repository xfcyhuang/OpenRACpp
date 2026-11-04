#pragma once

import std;

#include "game/miniyaml.hpp"
#include "game/production/production.hpp"
#include "game/support/mersenne_twister.hpp"
#include "game/world/actor.hpp"
#include "game/world/trait.hpp"
#include "game/world/trait_container.hpp"

namespace ora::game::world {

struct MapPlayerRef {
  std::string str_name;
  bool b_owns_world{};
  bool b_non_combatant{};
  bool b_playable{};
  std::string str_faction;
};

struct MapActorRef {
  std::string str_type;
  std::string str_owner;
  std::int32_t int4_pos_x{};
  std::int32_t int4_pos_y{};
};

struct SyntheticMap {
  std::string str_title;
  std::vector<std::string> vec_rule_texts;
  std::string str_map_text;
  std::vector<MapPlayerRef> vec_player_refs;
  std::vector<MapActorRef> vec_actor_defs;
};

struct FrameEndTask {
  std::uint32_t u4_kind{};
  std::uint32_t u4_actor{};
  std::uint32_t u4_player{};
  std::uint32_t u4_value{};
  std::string str_item;
};

struct PlayerModel {
  std::string str_internal_name;
  std::string str_faction;
  std::uint32_t u4_actor_id{};
  bool b_owns_world{};
  bool b_non_combatant{};
  bool b_spectating{};
  bool b_playable{};
};

struct ActorTypeDef {
  std::string str_name_lower;
  std::vector<TraitTemplate> vec_templates;
};

struct WorldModel {
  std::vector<ActorTypeDef> vec_actor_types;
  std::vector<ActorModel> vec_actors_all;
  std::vector<std::uint32_t> vec_in_world;
  std::vector<TraitModel> vec_traits;
  TraitContainerModel container_tick;
  TraitContainerModel container_sync;
  std::vector<PlayerModel> vec_players;
  production::ProductionWorldState prod;
  production::LobbyModel lobby;
  support::MersenneTwister rng_shared;
  support::MersenneTwister rng_player;
  std::int32_t int4_world_tick{};
  bool b_paused{};
  std::uint32_t u4_net_frame{1};
  std::deque<FrameEndTask> deque_frame_end;
  std::uint32_t u4_next_aid{};
  std::uint32_t u4_world_owner_player{};
};

[[nodiscard]] std::expected<SyntheticMap, std::string> synthetic_map_from_yaml(
    std::string_view str_rules_text, std::string_view str_map_text);

[[nodiscard]] std::expected<WorldModel, std::string> world_create(
    const SyntheticMap& map_syn, std::int32_t int4_seed);

[[nodiscard]] std::expected<WorldModel, std::string> world_create(
    const SyntheticMap& map_syn, std::int32_t int4_seed,
    const production::LobbyModel& lobby);

[[nodiscard]] std::expected<std::uint32_t, std::string> create_actor(
    WorldModel& w, std::string_view str_type, std::uint32_t u4_owner,
    std::int32_t int4_pos_x, std::int32_t int4_pos_y, bool b_add_to_world);

void dispose_actor(WorldModel& w, std::uint32_t u4_actor_id);

[[nodiscard]] std::int32_t process_orders(WorldModel& w);

void world_tick(WorldModel& w);

}  // namespace ora::game::world
