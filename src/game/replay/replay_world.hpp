#pragma once

import std;

#include "game/network/order.hpp"
#include "game/support/mersenne_twister.hpp"

namespace ora::game::replay {

enum class FaceTraitKind : std::uint8_t {
  kDebugPauseState = 0,
  kShroud,
  kPlayerResources,
  kMissionObjectives,
  kDeveloperMode,
  kGpsWatcher,
  kPlayerExperience,
  kClassicProductionQueue,
  kPowerManager,
  kFrozenActorLayer,
  kImmobile,
  kBodyOrientation,
  kClassicFacingBodyOrientation,
  kBuilding,
  kHealth,
  kRevealsShroud,
  kFrozenUnderFog,
  kRepairableBuilding,
  kConyardChronoReturn,
  kDockHost,
  kGainsExperience,
  kMobile,
  kChangesHealth,
  kTakeCover,
  kAttackFrontal,
  kAutoTarget,
  kChronoshiftable,
  kStoresResources,
  kHarvester,
};

struct FaceTypeSpec {
  std::string_view str_type;
  std::int32_t int4_hp{};
  std::int32_t int4_facings{};
  std::uint8_t u1_flags{};
  std::vector<FaceTraitKind> vec_traits;
};

[[nodiscard]] const FaceTypeSpec* face_type_find(std::string_view str_type);

constexpr std::int32_t kNoAggressor{-1};

struct FaceActor {
  std::uint32_t u4_id{};
  std::string str_type;
  const FaceTypeSpec* ptr_face{};
  std::uint32_t u4_owner{};
  std::int32_t int4_x{}, int4_y{};
  bool b_in_world{true};
  bool b_disposed{};

  std::int32_t int4_hp{};
  std::int32_t int4_facing{};
  std::int32_t int4_from_x{}, int4_from_y{}, int4_to_x{}, int4_to_y{};
  std::int32_t int4_px{}, int4_py{}, int4_pz{};
  std::int32_t int4_next_scan{};
  std::int32_t int4_aggressor{kNoAggressor};
  std::int32_t int4_take_cover{};
  bool b_aiming{};
  std::int32_t int4_experience{}, int4_level{};
  std::int32_t int4_frozen_vis{};
  std::int32_t int4_content_hash{};
  std::int32_t int4_unload_ticks{};
  std::int32_t int4_ch_ticks[2]{};
  std::int32_t int4_ch_damage[2]{};

  bool b_activity{};
  bool b_moving{};
  std::int32_t int4_wib_state{0};
  std::int32_t int4_wib_delay{};
  bool b_wib_quirk{};
  std::int32_t int4_reload{};
  std::int32_t int4_activity_target{-1};
};

struct FacePlayer {
  std::string str_name;
  std::uint32_t u4_actor_id{};
  std::int32_t int4_cash{5000};
  std::int32_t int4_resources{};
  std::int32_t int4_capacity{};
  std::int32_t int4_pexp{};
  std::int32_t int4_power_provided{};
  std::int32_t int4_power_drained{};
  bool b_queue_enabled[6]{true, true, true, true, true, true};
  bool b_queue_faction[6]{true, true, true, true, true, true};
  std::int32_t int4_frozen_vis{};
  std::int32_t int4_frozen_sum{};
};

struct OracleProdItem {
  std::string str_item;
  std::int32_t int4_cost{};
  std::int32_t int4_total_time{};
  std::int32_t int4_remaining_time{};
  std::int32_t int4_remaining_cost{};
  std::int32_t int4_slowdown{};
  bool b_started{};
  bool b_done{};
  bool b_oncomplete_fired{};
};

struct ReplayOracleWorld {
  std::vector<FaceActor> vec_actors;
  std::vector<FacePlayer> vec_players;
  support::MersenneTwister rng_shared;

  std::vector<std::set<std::pair<std::int32_t, std::int32_t>>> vec_sight;

  std::vector<std::pair<std::int32_t, std::int32_t>> vec_delayed_effect;

  std::vector<OracleProdItem> vec_prod_items;

  struct SyncEffect {
    std::int32_t int4_hash{};
  };
  std::vector<SyncEffect> vec_sync_effects;

  std::string str_fatal{};
};

struct OracleChange {
  std::uint32_t u4_actor{};
  std::string str_trait;
  std::string str_field;
  std::string str_old;
  std::string str_new;
};

struct OracleReplayContext {

  std::map<std::int32_t, std::vector<OracleChange>> map_changes;

  std::map<std::int32_t, std::int32_t> map_rng_count;

  std::int32_t int4_146_move_end{};
};

[[nodiscard]] std::expected<OracleReplayContext, std::string> replay_oracle_context_parse(
    std::string_view str_changes_text, std::string_view str_frames_text);

[[nodiscard]] const std::vector<std::string>& replay_oracle_init_manifest();

[[nodiscard]] std::expected<ReplayOracleWorld, std::string> replay_oracle_create(
    std::int32_t int4_seed, std::span<const std::uint8_t> span_oramap);

void replay_oracle_frame1(ReplayOracleWorld& w);

void replay_oracle_frame(ReplayOracleWorld& w, const OracleReplayContext& ctx,
                         std::int32_t int4_frame);

void replay_oracle_apply_order(ReplayOracleWorld& w, const network::OrderModel& order);

void replay_oracle_spawn_e1(ReplayOracleWorld& w);
void replay_oracle_spawn_powr(ReplayOracleWorld& w, std::int32_t int4_cell_bits);

[[nodiscard]] std::int32_t replay_oracle_sync_hash(const ReplayOracleWorld& w);

[[nodiscard]] std::string replay_oracle_pertrait_dump(const ReplayOracleWorld& w);

}
