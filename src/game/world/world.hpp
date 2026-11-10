#pragma once

import std;

#include "game/economy/economy.hpp"
#include "game/miniyaml.hpp"
#include "game/network/order.hpp"
#include "game/production/production.hpp"
#include "game/support/mersenne_twister.hpp"
#include "game/world/actor.hpp"
#include "game/world/bot.hpp"
#include "game/world/trait.hpp"
#include "game/world/trait_container.hpp"

namespace ora::game {
struct sfx_context;
}

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
  std::string str_item{};
};

struct PlayerModel {
  std::string str_internal_name;
  std::string str_faction;
  std::uint32_t u4_actor_id{};
  bool b_owns_world{};
  bool b_non_combatant{};
  bool b_spectating{};
  bool b_playable{};
  // Player.IsBot = BotType != null（Player.cs:202）的载体：空串 = 非 bot。
  std::string str_bot_type;
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
  economy::EconomyWorldState econ;
  production::LobbyModel lobby;
  std::vector<BotModel> vec_bots;
  // bot_tick 出单暂存面（上游 world.IssueOrder 的等价落点，T31 改投订单管道）。
  std::vector<network::OrderModel> vec_bot_issued_orders;
  support::MersenneTwister rng_shared;
  support::MersenneTwister rng_player;
  // World.LocalRandom（World.cs:200，无种子墙钟 MT）的等价载体：bot 决策随机
  // 走此流，不进同步面（种子派生口径见 COVERAGE 登记）。
  support::MersenneTwister rng_local;
  std::int32_t int4_world_tick{};
  bool b_paused{};
  // 回放重演世界位（ModularBot.Activate 的 IsReplay 早退）。
  bool b_is_replay{};
  std::uint32_t u4_net_frame{1};
  std::deque<FrameEndTask> deque_frame_end;
  std::uint32_t u4_next_aid{};
  std::uint32_t u4_world_owner_player{};
  ora::game::sfx_context* ptr_sfx{nullptr};
};

[[nodiscard]] std::expected<SyntheticMap, std::string> synthetic_map_from_yaml(
    std::string_view str_rules_text, std::string_view str_map_text);

[[nodiscard]] std::expected<WorldModel, std::string> world_create(
    const SyntheticMap& map_syn, std::int32_t int4_seed);

[[nodiscard]] std::expected<WorldModel, std::string> world_create(
    const SyntheticMap& map_syn, std::int32_t int4_seed,
    const production::LobbyModel& lobby, bool b_is_replay = false);

[[nodiscard]] std::expected<std::uint32_t, std::string> create_actor(
    WorldModel& w, std::string_view str_type, std::uint32_t u4_owner,
    std::int32_t int4_pos_x, std::int32_t int4_pos_y, bool b_add_to_world);

void dispose_actor(WorldModel& w, std::uint32_t u4_actor_id);

[[nodiscard]] std::int32_t process_orders(WorldModel& w);

void world_tick(WorldModel& w);

}
