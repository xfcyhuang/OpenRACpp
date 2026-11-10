import std;

import ora.game;
import ora.game.miniyaml;
import ora.game.pathfinding;
import ora.game.support;
import ora.game.world;
import ora.test.util;
using ora::test::check;

namespace {


constexpr std::int32_t kSeed{0};

constexpr std::string_view kRulesBase{
    "^probe.base:\n"
    "\tTickCounter:\n"
    "\t\tStart: 10\n"};

constexpr std::string_view kRulesMain{
    "world:\n"
    "\tInherits: ^probe.base\n"
    "\tFactionInfo@allies:\n"
    "\tFactionInfo@soviet:\n"
    "\tRngProbe:\n"
    "player:\n"
    "\tTickCounter:\n"
    "probe.rng2:\n"
    "\tRngProbe@first:\n"
    "\tRngProbe@second:\n"
    "probe.sync:\n"
    "\tSyncProbe:\n"
    "\tTickCounter@second:\n"
    "probe.spawn:\n"
    "\tSpawnerProbe:\n"
    "\t\tInterval: 13\n"
    "\t\tType: probe.child\n"
    "probe.child:\n"
    "\tTickCounter:\n"
    "\tSelfDestructProbe:\n"
    "\t\tAt: 50\n"};

constexpr std::string_view kMapYaml{
    "Title: synthetic-01\n"
    "Players:\n"
    "\tPlayerReference@Neutral:\n"
    "\t\tName: Neutral\n"
    "\t\tOwnsWorld: True\n"
    "\t\tNonCombatant: True\n"
    "\t\tFaction: allies\n"
    "\tPlayerReference@Creeps:\n"
    "\t\tName: Creeps\n"
    "\t\tNonCombatant: True\n"
    "\t\tFaction: soviet\n"
    "Actors:\n"
    "\trng0: probe.rng2\n"
    "\t\tLocation: 10,20\n"
    "\t\tOwner: Neutral\n"
    "\tsync0: probe.sync\n"
    "\t\tLocation: 30,40\n"
    "\t\tOwner: Creeps\n"
    "\tghost0: probe.child\n"
    "\t\tLocation: 50,60\n"
    "\t\tOwner: Nobody\n"
    "\tspawn0: probe.spawn\n"
    "\t\tLocation: 70,80\n"
    "\t\tOwner: Neutral\n"};

std::expected<ora::game::world::SyntheticMap, std::string> make_synthetic_map() {
  return ora::game::world::synthetic_map_from_yaml(
      std::string(kRulesBase) + "\n" + std::string(kRulesMain), kMapYaml);
}

std::expected<ora::game::world::WorldModel, std::string> make_world(std::int32_t int4_seed) {
  auto res_map{make_synthetic_map()};
  if (!res_map.has_value()) return std::unexpected(res_map.error());
  return ora::game::world::world_create(*res_map, int4_seed);
}

const ora::game::world::TraitModel& trait_of(const ora::game::world::WorldModel& w,
                                             std::uint32_t u4_actor_id, std::size_t idx_trait) {
  return w.vec_traits[w.vec_actors_all[u4_actor_id].vec_trait_handles[idx_trait]];
}

}

int main() {
  {
    auto res_map{make_synthetic_map()};
    check(res_map.has_value(), "synthetic map parses");
    if (res_map.has_value()) {
      const auto& map{*res_map};
      check(map.str_title == "synthetic-01", "map title parsed");
      check(map.vec_player_refs.size() == 2, "two map player refs");
      check(map.vec_player_refs.size() == 2 && map.vec_player_refs[0].str_name == "Neutral" &&
                map.vec_player_refs[0].b_owns_world && map.vec_player_refs[0].b_non_combatant &&
                map.vec_player_refs[0].str_faction == "allies",
            "Neutral ref fields");
      check(map.vec_player_refs.size() == 2 && map.vec_player_refs[1].str_name == "Creeps" &&
                !map.vec_player_refs[1].b_owns_world && map.vec_player_refs[1].b_non_combatant,
            "Creeps ref fields");
      check(map.vec_actor_defs.size() == 4, "four map actor defs");
      check(map.vec_actor_defs.size() == 4 && map.vec_actor_defs[0].str_type == "probe.rng2" &&
                map.vec_actor_defs[0].str_owner == "Neutral" &&
                map.vec_actor_defs[0].int4_pos_x == 10 && map.vec_actor_defs[0].int4_pos_y == 20,
            "actor def 0 fields");
      check(map.vec_actor_defs.size() == 4 && map.vec_actor_defs[2].str_owner == "Nobody",
            "actor def 2 keeps unknown owner name for fallback");
    }
  }

  {
    auto res_world{make_world(kSeed)};
    check(res_world.has_value(), "world_create succeeds");
    if (res_world.has_value()) {
      const auto& w{*res_world};
      check(w.vec_players.size() == 3, "three players (map non-playable + Everyone)");
      check(w.vec_players.size() == 3 && w.vec_players[0].str_internal_name == "Neutral" &&
                w.vec_players[1].str_internal_name == "Creeps" &&
                w.vec_players[2].str_internal_name == "Everyone" &&
                w.vec_players[2].b_spectating && w.vec_players[2].b_non_combatant,
            "player creation order and Everyone appended last");
      check(w.u4_world_owner_player == 0, "world owner is Neutral");
      check(w.vec_actors_all.size() == 8 && w.u4_next_aid == 8,
            "8 actors created (world + 3 player + 4 map)");
      check(w.vec_actors_all[0].str_type == "world" && w.vec_actors_all[0].u4_id == 0 &&
                w.vec_actors_all[0].u4_owner == 0,
            "world actor id 0 owned by world owner");
      check(w.vec_players[0].u4_actor_id == 1 && w.vec_players[1].u4_actor_id == 2 &&
                w.vec_players[2].u4_actor_id == 3,
            "player actor ids 1..3 in player order");
      check(w.vec_actors_all[4].str_type == "probe.rng2" && w.vec_actors_all[5].str_type == "probe.sync" &&
                w.vec_actors_all[6].str_type == "probe.child" && w.vec_actors_all[7].str_type == "probe.spawn",
            "map actor ids 4..7 in map yaml order");
      check(w.vec_actors_all[5].u4_owner == 1, "sync0 owned by Creeps");
      check(w.vec_actors_all[6].u4_owner == 0, "ghost0 unknown owner falls back to world owner");
      check(w.vec_in_world.size() == 8, "all 8 actors in world");
      check(w.rng_player.int4_total_count == 1,
            "playerRandom consumed once (Everyone Random faction)");
      check(w.rng_shared.int4_total_count == 0, "shared random untouched at assembly");

      check(w.vec_actors_all[0].vec_trait_handles.size() == 4, "world actor has 4 traits");
      check(trait_of(w, 0, 0).str_kind == "TickCounter" && trait_of(w, 0, 0).int4_counter == 10,
            "world trait 0 = inherited TickCounter with Start 10 (yaml/inherit order)");
      check(trait_of(w, 0, 1).str_kind == "FactionInfo" && trait_of(w, 0, 2).str_kind == "FactionInfo",
            "world traits 1-2 = FactionInfo instances");
      check(trait_of(w, 0, 3).str_kind == "RngProbe", "world trait 3 = RngProbe");

      check(w.vec_actors_all[4].vec_trait_handles.size() == 2 &&
                trait_of(w, 4, 0).str_kind == "RngProbe" &&
                trait_of(w, 4, 0).str_instance == "first" &&
                trait_of(w, 4, 1).str_kind == "RngProbe" && trait_of(w, 4, 1).str_instance == "second",
            "rng0 has RngProbe@first then RngProbe@second (construct order = yaml order)");

      check(w.vec_actors_all[5].vec_trait_handles.size() == 2 &&
                trait_of(w, 5, 0).str_kind == "SyncProbe" &&
                trait_of(w, 5, 1).str_kind == "TickCounter" &&
                trait_of(w, 5, 1).str_instance == "second",
            "sync0 has SyncProbe then TickCounter@second");

      check(w.vec_actors_all[6].vec_trait_handles.size() == 2 &&
                trait_of(w, 6, 0).str_kind == "TickCounter" &&
                trait_of(w, 6, 1).str_kind == "SelfDestructProbe" &&
                trait_of(w, 6, 1).int4_param == 50,
            "ghost0 TickCounter then SelfDestructProbe(At=50)");

      check(w.vec_actors_all[7].vec_trait_handles.size() == 1 &&
                trait_of(w, 7, 0).str_kind == "SpawnerProbe" &&
                trait_of(w, 7, 0).int4_param == 13 &&
                trait_of(w, 7, 0).str_param_type == "probe.child",
            "spawn0 SpawnerProbe interval 13 type probe.child");

      check(w.container_tick.vec_actor_ids.size() == 12,
            "tick container holds 12 ITick trait instances");
      check(w.container_tick.vec_actor_ids.size() == w.container_tick.vec_trait_handles.size(),
            "tick container parallel vectors aligned");
      check(w.container_sync.vec_actor_ids.size() == 1 && w.container_sync.vec_actor_ids[0] == 5,
            "sync container holds only sync0's SyncProbe");
      check(w.int4_world_tick == 0 && w.u4_net_frame == 1,
            "world starts at tick 0 net frame 1 (StartGame)");
    }
  }

  {
    auto res_world{make_world(kSeed)};
    check(res_world.has_value(), "world for error cases");
    if (res_world.has_value()) {
      auto res_bad{ora::game::world::create_actor(*res_world, "nosuch", 0, 1, 1, true)};
      check(!res_bad.has_value() && res_bad.error() == "No rules definition for unit nosuch",
            "unknown actor type rejected with upstream message");
    }
  }

  {
    auto res_map{make_synthetic_map()};
    check(res_map.has_value(), "map for missing world owner case");
    if (res_map.has_value()) {
      auto map_bad{*res_map};
      for (auto& ref : map_bad.vec_player_refs) ref.b_owns_world = false;
      auto res_world{ora::game::world::world_create(map_bad, kSeed)};
      check(!res_world.has_value() &&
                res_world.error() ==
                    "Map synthetic-01 does not define a player actor owning the world.",
            "missing world owner rejected with upstream message");
    }
  }

  {
    auto res_world{ora::game::world::world_create(
        ora::game::world::SyntheticMap{
            "", {"probe.x:\n\tNotATrait:\n"}, "", {}, {}}, kSeed)};
    check(!res_world.has_value() &&
              res_world.error() == "Cannot locate type: NotATraitInfo",
          "unknown trait kind rejected at rules load");
  }

  {
    auto res_world{make_world(kSeed)};
    check(res_world.has_value(), "world for tick order case");
    if (res_world.has_value()) {
      auto& w{*res_world};
      ora::game::support::MersenneTwister rng_ref{ora::game::support::make_mersenne_twister(kSeed)};
      const std::int32_t int4_draw1{ora::game::support::next(rng_ref)};
      const std::int32_t int4_draw2{ora::game::support::next(rng_ref)};
      const std::int32_t int4_draw3{ora::game::support::next(rng_ref)};

      const std::int32_t int4_hash_pre{ora::game::world::process_orders(w)};
      ora::game::world::world_tick(w);
      check(w.int4_world_tick == 1 && w.u4_net_frame == 2,
            "one frame advances world tick and net frame");
      check(trait_of(w, 0, 3).int4_counter == int4_draw1,
            "world RngProbe gets draw 1 (actor id 0 dispatches first)");
      check(trait_of(w, 4, 0).int4_counter == int4_draw2,
            "rng0 RngProbe@first gets draw 2 (same-actor construct order)");
      check(trait_of(w, 4, 1).int4_counter == int4_draw3,
            "rng0 RngProbe@second gets draw 3");
      check(trait_of(w, 0, 0).int4_counter == 11, "world TickCounter 10 to 11");
      check(trait_of(w, 1, 0).int4_counter == 1 && trait_of(w, 3, 0).int4_counter == 1,
            "player actor TickCounters advanced");
      check(trait_of(w, 5, 0).int4_counter == 1 && trait_of(w, 5, 0).b_sync_flag,
            "SyncProbe counter 1 flag alternated");
      const std::int32_t int4_hash_post{ora::game::world::process_orders(w)};
      check(int4_hash_pre != int4_hash_post,
            "sync hash sample point precedes tick (state advanced between samples)");
    }
  }

  {
    auto res_world{make_world(kSeed)};
    check(res_world.has_value(), "world for spawner case");
    if (res_world.has_value()) {
      auto& w{*res_world};
      for (std::int32_t int4_tick{0}; int4_tick < 12; ++int4_tick) {
        static_cast<void>(ora::game::world::process_orders(w));
        ora::game::world::world_tick(w);
      }
      check(w.u4_next_aid == 8 && w.vec_in_world.size() == 8,
            "no spawns before interval tick 13");
      for (std::int32_t int4_tick{0}; int4_tick < 1; ++int4_tick) {
        static_cast<void>(ora::game::world::process_orders(w));
        ora::game::world::world_tick(w);
      }
      check(w.u4_next_aid == 9 && w.vec_in_world.size() == 9,
            "first spawn at tick 13 grows world");
      check(w.vec_actors_all.size() == 9 && w.vec_actors_all[8].str_type == "probe.child" &&
                w.vec_actors_all[8].u4_owner == 0 && w.vec_actors_all[8].int4_pos_x == 71 &&
                w.vec_actors_all[8].int4_pos_y == 80,
            "spawned child fields (owner inherited, position offset)");
      check(trait_of(w, 8, 0).int4_counter == 0,
            "actor created mid-ITick is not ticked in its spawn tick (snapshot dispatch)");
      static_cast<void>(ora::game::world::process_orders(w));
      ora::game::world::world_tick(w);
      check(trait_of(w, 8, 0).int4_counter == 1, "spawned child ticks from the next tick");
    }
  }

  {
    auto res_world{make_world(kSeed)};
    check(res_world.has_value(), "world for dispose case");
    if (res_world.has_value()) {
      auto& w{*res_world};
      for (std::int32_t int4_tick{0}; int4_tick < 49; ++int4_tick) {
        static_cast<void>(ora::game::world::process_orders(w));
        ora::game::world::world_tick(w);
      }
      check(w.vec_actors_all[6].b_in_world && !w.vec_actors_all[6].b_will_dispose,
            "ghost0 alive and not disposing at tick 49");
      static_cast<void>(ora::game::world::process_orders(w));
      ora::game::world::world_tick(w);
      check(trait_of(w, 6, 1).int4_counter == 50,
            "ghost0 received ITick on its disposal tick");
      check(!w.vec_actors_all[6].b_in_world && w.vec_actors_all[6].b_disposed,
            "ghost0 removed at frame end of tick 50 (deferred dispose)");
      check(w.u4_next_aid == 11, "next aid unchanged by dispose (ids never reused)");
      bool b_in_container{false};
      for (const std::uint32_t u4_id : w.container_tick.vec_actor_ids)
        if (u4_id == 6) b_in_container = true;
      check(!b_in_container, "disposed actor removed from tick container");
      check(w.vec_in_world.size() == 10,
            "in-world count at tick 50: 8 initial - ghost0 + 3 spawned children");
      ora::game::world::dispose_actor(w, 6);
      ora::game::world::world_tick(w);
      check(w.vec_in_world.size() == 10, "double dispose is a no-op");
    }
  }

  {
    auto res_a{make_world(kSeed)};
    auto res_b{make_world(kSeed)};
    auto res_c{make_world(kSeed + 1)};
    check(res_a.has_value() && res_b.has_value() && res_c.has_value(), "worlds for determinism");
    if (res_a.has_value() && res_b.has_value() && res_c.has_value()) {
      constexpr std::int32_t kTicks{1000};
      bool b_digest_equal{true};
      bool b_hash_equal{true};
      std::string str_digest_a;
      std::string str_digest_c;
      for (std::int32_t int4_tick{0}; int4_tick < kTicks; ++int4_tick) {
        const std::int32_t int4_hash_a{ora::game::world::process_orders(*res_a)};
        const std::int32_t int4_hash_b{ora::game::world::process_orders(*res_b)};
        static_cast<void>(ora::game::world::process_orders(*res_c));
        ora::game::world::world_tick(*res_a);
        ora::game::world::world_tick(*res_b);
        ora::game::world::world_tick(*res_c);
        str_digest_a = ora::game::world::state_digest(*res_a);
        if (str_digest_a != ora::game::world::state_digest(*res_b)) b_digest_equal = false;
        if (int4_hash_a != int4_hash_b) b_hash_equal = false;
        if (int4_tick == 0) str_digest_c = ora::game::world::state_digest(*res_c);
      }
      check(b_digest_equal, "same seed two worlds: per-tick state digest identical over 1000 ticks");
      check(b_hash_equal, "same seed two worlds: per-tick upstream sync hash identical");
      check(str_digest_a != str_digest_c, "different seed produces a different digest");
      check(res_a->u4_next_aid == 84,
            "total actors after 1000 ticks: 8 initial + 76 spawns (every 13th tick)");
      check(res_a->vec_in_world.size() == 10,
            "in-world at tick 1000: 8 - ghost0 - 73 expired children + 76 children");
      check(res_a->rng_shared.int4_total_count == 3000,
            "shared random consumed 3 draws per tick over 1000 ticks");
    }
  }

  {
    auto res_map{make_synthetic_map()};
    check(res_map.has_value(), "map for zero-sync hash case");
    if (res_map.has_value()) {
      auto map_nosync{*res_map};
      map_nosync.vec_actor_defs.erase(map_nosync.vec_actor_defs.begin() + 1);
      auto res_world{ora::game::world::world_create(map_nosync, kSeed)};
      check(res_world.has_value(), "zero-sync world created");
      if (res_world.has_value()) {
        auto& w{*res_world};
        std::int32_t int4_expected{0};
        for (std::uint32_t u4_id{0}; u4_id < 7; ++u4_id) {
          const std::int32_t int4_n{static_cast<std::int32_t>(u4_id)};
          int4_expected +=
              int4_n * (int4_n + 1) * static_cast<std::int32_t>(u4_id << 16);
        }
        check(ora::game::world::sync_hash_upstream(w) == int4_expected,
              "zero-ISync sync hash equals segment 1 sum only");
        ora::game::support::MersenneTwister rng_ref{ora::game::support::make_mersenne_twister(kSeed)};
        for (std::int32_t int4_i{0}; int4_i < 3; ++int4_i) ora::game::support::next(rng_ref);
        static_cast<void>(ora::game::world::process_orders(w));
        ora::game::world::world_tick(w);
        check(ora::game::world::sync_hash_upstream(w) == int4_expected + rng_ref.int4_last,
              "after one tick sync hash adds shared random last value");
      }
    }
  }

  {
    auto res_world{make_world(kSeed)};
    check(res_world.has_value(), "world for five-segment hash case");
    if (res_world.has_value()) {
      auto& w{*res_world};
      for (std::int32_t int4_tick{0}; int4_tick < 5; ++int4_tick) {
        static_cast<void>(ora::game::world::process_orders(w));
        ora::game::world::world_tick(w);
      }
      std::int32_t int4_expected{0};
      for (std::uint32_t u4_id{0}; u4_id < 8; ++u4_id) {
        const std::int32_t int4_n{static_cast<std::int32_t>(u4_id)};
        int4_expected +=
            int4_n * (int4_n + 1) * static_cast<std::int32_t>(u4_id << 16);
      }
      const std::int32_t int4_trait_hash{5 ^ 1};
      int4_expected += 8 * 6 * int4_trait_hash;
      ora::game::support::MersenneTwister rng_ref{ora::game::support::make_mersenne_twister(kSeed)};
      for (std::int32_t int4_i{0}; int4_i < 15; ++int4_i) ora::game::support::next(rng_ref);
      int4_expected += rng_ref.int4_last;
      check(ora::game::world::sync_hash_upstream(w) == int4_expected,
            "five-segment hash at tick 5 matches spec worked example");
    }
  }

  return ora::test::test_exit_code("world_tick");
}
