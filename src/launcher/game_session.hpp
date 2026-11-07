#pragma once

import std;

#include "game/combat/armament.hpp"
#include "game/combat/combat_sim.hpp"
#include "game/combat/health.hpp"
#include "game/combat/weapon.hpp"
#include "game/input/production_panel.hpp"
#include "game/network/order_manager.hpp"
#include "game/pathfinding/move_sim.hpp"
#include "game/world/sync_hash.hpp"
#include "game/world/world.hpp"
#include "launcher/sandbox_recipe.hpp"

namespace ora::launcher {

struct SessionEnv {
  std::int32_t int4_seed{0};
  std::string str_map_path;

  std::string str_tileset_path{"data/temperat.yaml"};
};

struct MoverState {
  std::uint32_t u4_actor{};
  game::pathfinding::MoveSimState move;
  std::int32_t int4_speed{113};
  std::int32_t int4_turn_speed{512};
};

struct FighterState {
  game::combat::CombatUnit unit;
  game::combat::ArmamentState arm;
  game::combat::HealthState hp;
  std::string str_weapon;
  std::uint32_t u4_target{};
};

struct GameSession {
  game::world::WorldModel w;
  game::network::OrderManagerModel om;
  game::combat::WeaponTable weapons;
  std::vector<MoverState> vec_movers;
  std::vector<FighterState> vec_fighters;
  std::vector<std::pair<std::uint32_t, game::network::OrderModel>> vec_bridge_pending;
  std::uint32_t u4_viewer{2};
  std::vector<std::string> vec_events;
};

[[nodiscard]] std::expected<GameSession, std::string> session_setup(const SessionEnv& env);

void session_issue_order(GameSession& s, const game::network::OrderModel& order);

void session_tick(GameSession& s);

[[nodiscard]] std::string session_digest(const GameSession& s);

[[nodiscard]] std::string session_hud_line(const GameSession& s);

[[nodiscard]] std::vector<game::input::ProductionQueueView> session_queue_views(
    const GameSession& s);

}
