#include "game/replay/replay_world.hpp"

import std;

#include "game/filesystem/zip_read.hpp"
#include "game/miniyaml.hpp"
#include "game/pathfinding/wangle.hpp"

namespace ora::game::replay {

namespace {

using K = FaceTraitKind;

constexpr std::uint8_t kFlagReveals{0x01};
constexpr std::uint8_t kFlagFrozen{0x02};

const std::vector<FaceTypeSpec>& type_faces()
{
  static const std::vector<FaceTypeSpec> vec{
      FaceTypeSpec{.str_type = "mpspawn", .int4_hp = 0, .int4_facings = 1, .u1_flags = 0,
                   .vec_traits = {K::kImmobile, K::kBodyOrientation}},
      FaceTypeSpec{.str_type = "v01", .int4_hp = 40000, .int4_facings = 1, .u1_flags = kFlagReveals | kFlagFrozen,
                   .vec_traits = {K::kBodyOrientation, K::kBuilding, K::kHealth, K::kRevealsShroud, K::kFrozenUnderFog}},
      FaceTypeSpec{.str_type = "v10", .int4_hp = 40000, .int4_facings = 1, .u1_flags = kFlagFrozen,
                   .vec_traits = {K::kBodyOrientation, K::kBuilding, K::kHealth, K::kFrozenUnderFog}},
      FaceTypeSpec{.str_type = "v08", .int4_hp = 40000, .int4_facings = 1, .u1_flags = kFlagFrozen,
                   .vec_traits = {K::kBodyOrientation, K::kBuilding, K::kHealth, K::kFrozenUnderFog}},
      FaceTypeSpec{.str_type = "brl3", .int4_hp = 1000, .int4_facings = 1, .u1_flags = kFlagFrozen,
                   .vec_traits = {K::kBodyOrientation, K::kBuilding, K::kHealth, K::kFrozenUnderFog}},
      FaceTypeSpec{.str_type = "v11", .int4_hp = 40000, .int4_facings = 1, .u1_flags = kFlagFrozen,
                   .vec_traits = {K::kBodyOrientation, K::kBuilding, K::kHealth, K::kFrozenUnderFog}},
      FaceTypeSpec{.str_type = "v16", .int4_hp = 0, .int4_facings = 1, .u1_flags = kFlagFrozen,
                   .vec_traits = {K::kBodyOrientation, K::kBuilding, K::kFrozenUnderFog}},
      FaceTypeSpec{.str_type = "v05", .int4_hp = 40000, .int4_facings = 1, .u1_flags = kFlagFrozen,
                   .vec_traits = {K::kBodyOrientation, K::kBuilding, K::kHealth, K::kFrozenUnderFog}},
      FaceTypeSpec{.str_type = "v18", .int4_hp = 0, .int4_facings = 1, .u1_flags = kFlagFrozen,
                   .vec_traits = {K::kBodyOrientation, K::kBuilding, K::kFrozenUnderFog}},
      FaceTypeSpec{.str_type = "v17", .int4_hp = 0, .int4_facings = 1, .u1_flags = kFlagFrozen,
                   .vec_traits = {K::kBodyOrientation, K::kBuilding, K::kFrozenUnderFog}},
      FaceTypeSpec{.str_type = "rushouse", .int4_hp = 40000, .int4_facings = 1, .u1_flags = kFlagFrozen,
                   .vec_traits = {K::kBodyOrientation, K::kBuilding, K::kHealth, K::kFrozenUnderFog}},
      FaceTypeSpec{.str_type = "v04", .int4_hp = 40000, .int4_facings = 1, .u1_flags = kFlagFrozen,
                   .vec_traits = {K::kBodyOrientation, K::kBuilding, K::kHealth, K::kFrozenUnderFog}},
      FaceTypeSpec{.str_type = "v06", .int4_hp = 40000, .int4_facings = 1, .u1_flags = kFlagFrozen,
                   .vec_traits = {K::kBodyOrientation, K::kBuilding, K::kHealth, K::kFrozenUnderFog}},
      FaceTypeSpec{.str_type = "v09", .int4_hp = 40000, .int4_facings = 1, .u1_flags = kFlagFrozen,
                   .vec_traits = {K::kBodyOrientation, K::kBuilding, K::kHealth, K::kFrozenUnderFog}},
      FaceTypeSpec{.str_type = "barl", .int4_hp = 1000, .int4_facings = 1, .u1_flags = kFlagFrozen,
                   .vec_traits = {K::kBodyOrientation, K::kBuilding, K::kHealth, K::kFrozenUnderFog}},
      FaceTypeSpec{.str_type = "v02", .int4_hp = 40000, .int4_facings = 1, .u1_flags = kFlagFrozen,
                   .vec_traits = {K::kBodyOrientation, K::kBuilding, K::kHealth, K::kFrozenUnderFog}},
      FaceTypeSpec{.str_type = "v07", .int4_hp = 40000, .int4_facings = 1, .u1_flags = kFlagFrozen,
                   .vec_traits = {K::kBodyOrientation, K::kBuilding, K::kHealth, K::kFrozenUnderFog}},
      FaceTypeSpec{.str_type = "t01", .int4_hp = 50000, .int4_facings = 1, .u1_flags = 0,
                   .vec_traits = {K::kBodyOrientation, K::kBuilding, K::kHealth}},
      FaceTypeSpec{.str_type = "tc03", .int4_hp = 50000, .int4_facings = 1, .u1_flags = 0,
                   .vec_traits = {K::kBodyOrientation, K::kBuilding, K::kHealth}},
      FaceTypeSpec{.str_type = "tc02", .int4_hp = 50000, .int4_facings = 1, .u1_flags = 0,
                   .vec_traits = {K::kBodyOrientation, K::kBuilding, K::kHealth}},
      FaceTypeSpec{.str_type = "tc01", .int4_hp = 50000, .int4_facings = 1, .u1_flags = 0,
                   .vec_traits = {K::kBodyOrientation, K::kBuilding, K::kHealth}},
      FaceTypeSpec{.str_type = "t15", .int4_hp = 50000, .int4_facings = 1, .u1_flags = 0,
                   .vec_traits = {K::kBodyOrientation, K::kBuilding, K::kHealth}},
      FaceTypeSpec{.str_type = "utilpol1", .int4_hp = 50000, .int4_facings = 1, .u1_flags = 0,
                   .vec_traits = {K::kBodyOrientation, K::kBuilding, K::kHealth}},
      FaceTypeSpec{.str_type = "utilpol2", .int4_hp = 50000, .int4_facings = 1, .u1_flags = 0,
                   .vec_traits = {K::kBodyOrientation, K::kBuilding, K::kHealth}},
      FaceTypeSpec{.str_type = "tc04", .int4_hp = 50000, .int4_facings = 1, .u1_flags = 0,
                   .vec_traits = {K::kBodyOrientation, K::kBuilding, K::kHealth}},
      FaceTypeSpec{.str_type = "t10", .int4_hp = 50000, .int4_facings = 1, .u1_flags = 0,
                   .vec_traits = {K::kBodyOrientation, K::kBuilding, K::kHealth}},
      FaceTypeSpec{.str_type = "t11", .int4_hp = 50000, .int4_facings = 1, .u1_flags = 0,
                   .vec_traits = {K::kBodyOrientation, K::kBuilding, K::kHealth}},
      FaceTypeSpec{.str_type = "t14", .int4_hp = 50000, .int4_facings = 1, .u1_flags = 0,
                   .vec_traits = {K::kBodyOrientation, K::kBuilding, K::kHealth}},
      FaceTypeSpec{.str_type = "t13", .int4_hp = 50000, .int4_facings = 1, .u1_flags = 0,
                   .vec_traits = {K::kBodyOrientation, K::kBuilding, K::kHealth}},
      FaceTypeSpec{.str_type = "t16", .int4_hp = 50000, .int4_facings = 1, .u1_flags = 0,
                   .vec_traits = {K::kBodyOrientation, K::kBuilding, K::kHealth}},
      FaceTypeSpec{.str_type = "tanktrap2", .int4_hp = 0, .int4_facings = 1, .u1_flags = 0,
                   .vec_traits = {K::kBodyOrientation, K::kBuilding}},
      FaceTypeSpec{.str_type = "fenc", .int4_hp = 15000, .int4_facings = 1, .u1_flags = kFlagFrozen,
                   .vec_traits = {K::kBodyOrientation, K::kBuilding, K::kHealth, K::kFrozenUnderFog}},
      FaceTypeSpec{.str_type = "t06", .int4_hp = 50000, .int4_facings = 1, .u1_flags = 0,
                   .vec_traits = {K::kBodyOrientation, K::kBuilding, K::kHealth}},
      FaceTypeSpec{.str_type = "t02", .int4_hp = 50000, .int4_facings = 1, .u1_flags = 0,
                   .vec_traits = {K::kBodyOrientation, K::kBuilding, K::kHealth}},
      FaceTypeSpec{.str_type = "t03", .int4_hp = 50000, .int4_facings = 1, .u1_flags = 0,
                   .vec_traits = {K::kBodyOrientation, K::kBuilding, K::kHealth}},
      FaceTypeSpec{.str_type = "tc05", .int4_hp = 50000, .int4_facings = 1, .u1_flags = 0,
                   .vec_traits = {K::kBodyOrientation, K::kBuilding, K::kHealth}},
      FaceTypeSpec{.str_type = "t12", .int4_hp = 50000, .int4_facings = 1, .u1_flags = 0,
                   .vec_traits = {K::kBodyOrientation, K::kBuilding, K::kHealth}},
      FaceTypeSpec{.str_type = "t08", .int4_hp = 50000, .int4_facings = 1, .u1_flags = 0,
                   .vec_traits = {K::kBodyOrientation, K::kBuilding, K::kHealth}},
      FaceTypeSpec{.str_type = "mine", .int4_hp = 0, .int4_facings = 1, .u1_flags = 0,
                   .vec_traits = {K::kBodyOrientation, K::kBuilding}},
      FaceTypeSpec{.str_type = "t05", .int4_hp = 50000, .int4_facings = 1, .u1_flags = 0,
                   .vec_traits = {K::kBodyOrientation, K::kBuilding, K::kHealth}},
      FaceTypeSpec{.str_type = "fact", .int4_hp = 150000, .int4_facings = 1, .u1_flags = kFlagReveals | kFlagFrozen,
                   .vec_traits = {K::kBodyOrientation, K::kBuilding, K::kHealth, K::kRevealsShroud, K::kRevealsShroud, K::kFrozenUnderFog, K::kRepairableBuilding, K::kConyardChronoReturn}},
      FaceTypeSpec{.str_type = "proc", .int4_hp = 90000, .int4_facings = 1, .u1_flags = kFlagReveals | kFlagFrozen,
                   .vec_traits = {K::kBodyOrientation, K::kBuilding, K::kHealth, K::kRevealsShroud, K::kRevealsShroud, K::kDockHost, K::kFrozenUnderFog, K::kRepairableBuilding}},
      FaceTypeSpec{.str_type = "barr", .int4_hp = 60000, .int4_facings = 1, .u1_flags = kFlagReveals | kFlagFrozen,
                   .vec_traits = {K::kBodyOrientation, K::kBuilding, K::kHealth, K::kRevealsShroud, K::kRevealsShroud, K::kFrozenUnderFog, K::kRepairableBuilding}},
      FaceTypeSpec{.str_type = "e1", .int4_hp = 5000, .int4_facings = 8, .u1_flags = kFlagReveals,
                   .vec_traits = {K::kGainsExperience, K::kBodyOrientation, K::kHealth, K::kRevealsShroud, K::kMobile, K::kChangesHealth, K::kChangesHealth, K::kTakeCover, K::kAttackFrontal, K::kAutoTarget}},
      FaceTypeSpec{.str_type = "mcv", .int4_hp = 60000, .int4_facings = 32, .u1_flags = kFlagReveals,
                   .vec_traits = {K::kClassicFacingBodyOrientation, K::kMobile, K::kChronoshiftable, K::kHealth, K::kRevealsShroud}},
      FaceTypeSpec{.str_type = "harv", .int4_hp = 60000, .int4_facings = 32, .u1_flags = kFlagReveals,
                   .vec_traits = {K::kClassicFacingBodyOrientation, K::kMobile, K::kChronoshiftable, K::kStoresResources, K::kHealth, K::kRevealsShroud, K::kHarvester, K::kChangesHealth}},
      FaceTypeSpec{.str_type = "powr", .int4_hp = 40000, .int4_facings = 1, .u1_flags = kFlagReveals | kFlagFrozen,
                   .vec_traits = {K::kBodyOrientation, K::kBuilding, K::kHealth, K::kRevealsShroud, K::kFrozenUnderFog, K::kRepairableBuilding}},
  };
  return vec;
}

std::int32_t footprint_width(std::string_view str_type) {
  if (str_type == "v01" || str_type == "v02" || str_type == "v04") return 2;
  if (str_type == "v05" || str_type == "v06" || str_type == "v07") return 2;
  if (str_type == "fact" || str_type == "proc") return 3;
  if (str_type == "barr" || str_type == "powr") return 2;
  return 1;
}

std::int32_t footprint_height(std::string_view str_type) {
  if (str_type == "v01" || str_type == "v02" || str_type == "v04") return 2;
  if (str_type == "rushouse") return 2;
  if (str_type == "fact" || str_type == "proc") return 4;
  if (str_type == "barr" || str_type == "powr") return 3;
  return 1;
}

void add_circle(std::set<std::pair<std::int32_t, std::int32_t>>& set_cells, std::int32_t int4_x,
                std::int32_t int4_y, std::int32_t int4_range_cells) {
  const std::int64_t in8_limit{static_cast<std::int64_t>(int4_range_cells) * 1024 *
                               (static_cast<std::int64_t>(int4_range_cells) * 1024)};
  const std::int64_t in8_r{(static_cast<std::int64_t>(int4_range_cells) * 1024 + 1023 + 512) / 1024};
  for (std::int64_t in8_dx{-in8_r}; in8_dx <= in8_r; ++in8_dx)
    for (std::int64_t in8_dy{-in8_r}; in8_dy <= in8_r; ++in8_dy) {
      const std::int64_t in8_dist{(in8_dx * 1024) * (in8_dx * 1024) + (in8_dy * 1024) * (in8_dy * 1024)};
      if (in8_dist <= in8_limit)
        set_cells.emplace(static_cast<std::int32_t>(int4_x + in8_dx),
                          static_cast<std::int32_t>(int4_y + in8_dy));
    }
}

void add_actor_sight(std::set<std::pair<std::int32_t, std::int32_t>>& set_cells, const FaceActor& actor,
                     std::initializer_list<std::int32_t> list_ranges_cells) {
  const std::int32_t int4_w{footprint_width(actor.str_type)};
  const std::int32_t int4_h{footprint_height(actor.str_type)};
  for (std::int32_t int4_x{}; int4_x < int4_w; ++int4_x)
    for (std::int32_t int4_y{}; int4_y < int4_h; ++int4_y)
      for (const std::int32_t int4_range : list_ranges_cells)
        add_circle(set_cells, actor.int4_x + int4_x, actor.int4_y + int4_y, int4_range);
}

struct InjectEntry {
  std::string_view str_type;
  std::int32_t int4_x, int4_y;
  std::string_view str_owner;
};
constexpr InjectEntry kInjectedActors[]{
    InjectEntry{"fact", 21, 6, "Multi0"}, InjectEntry{"proc", 33, 5, "Multi0"},
    InjectEntry{"barr", 21, 9, "Multi0"}, InjectEntry{"e1", 25, 10, "Multi0"},
    InjectEntry{"e1", 28, 12, "Creeps"},
};

std::int32_t wrap32(std::int64_t in8_value) {
  return static_cast<std::int32_t>(static_cast<std::uint32_t>(static_cast<std::uint64_t>(in8_value)));
}

std::int32_t cpos_bits(std::int32_t int4_x, std::int32_t int4_y) {
  return wrap32((static_cast<std::int64_t>(int4_x & 0xfff) << 20) |
                (static_cast<std::int64_t>(int4_y & 0xfff) << 8));
}

std::int32_t wpos_hash(std::int32_t int4_x, std::int32_t int4_y, std::int32_t int4_z) {
  return int4_x ^ int4_y ^ int4_z;
}

const char* trait_kind_name(FaceTraitKind kind) {
  switch (kind) {
    case K::kDebugPauseState: return "DebugPauseState";
    case K::kShroud: return "Shroud";
    case K::kPlayerResources: return "PlayerResources";
    case K::kMissionObjectives: return "MissionObjectives";
    case K::kDeveloperMode: return "DeveloperMode";
    case K::kGpsWatcher: return "GpsWatcher";
    case K::kPlayerExperience: return "PlayerExperience";
    case K::kClassicProductionQueue: return "ClassicProductionQueue";
    case K::kPowerManager: return "PowerManager";
    case K::kFrozenActorLayer: return "FrozenActorLayer";
    case K::kImmobile: return "Immobile";
    case K::kBodyOrientation: return "BodyOrientation";
    case K::kClassicFacingBodyOrientation: return "ClassicFacingBodyOrientation";
    case K::kBuilding: return "Building";
    case K::kHealth: return "Health";
    case K::kRevealsShroud: return "RevealsShroud";
    case K::kFrozenUnderFog: return "FrozenUnderFog";
    case K::kRepairableBuilding: return "RepairableBuilding";
    case K::kConyardChronoReturn: return "ConyardChronoReturn";
    case K::kDockHost: return "DockHost";
    case K::kGainsExperience: return "GainsExperience";
    case K::kMobile: return "Mobile";
    case K::kChangesHealth: return "ChangesHealth";
    case K::kTakeCover: return "TakeCover";
    case K::kAttackFrontal: return "AttackFrontal";
    case K::kAutoTarget: return "AutoTarget";
    case K::kChronoshiftable: return "Chronoshiftable";
    case K::kStoresResources: return "StoresResources";
    case K::kHarvester: return "Harvester";
  }
  return "?";
}

std::int32_t face_trait_hash(const ReplayOracleWorld& w, const FaceActor& actor, FaceTraitKind kind,
                             std::int32_t int4_instance) {
  const FaceTypeSpec& face{*actor.ptr_face};
  switch (kind) {
    case K::kClassicProductionQueue:
    case K::kDebugPauseState:
    case K::kShroud:
    case K::kMissionObjectives:
    case K::kDeveloperMode:
    case K::kGpsWatcher:
    case K::kRevealsShroud:
    case K::kRepairableBuilding:
    case K::kConyardChronoReturn:
    case K::kDockHost:
    case K::kChronoshiftable:
      return 0;
    case K::kChangesHealth:
      return actor.int4_ch_ticks[std::size_t(int4_instance)] ^
             actor.int4_ch_damage[std::size_t(int4_instance)];
    case K::kImmobile:
      return cpos_bits(actor.int4_x, actor.int4_y) ^
             wpos_hash(actor.int4_x * 1024 + 512, actor.int4_y * 1024 + 512, 0);
    case K::kBodyOrientation:
    case K::kClassicFacingBodyOrientation:
      return face.int4_facings;
    case K::kBuilding:
      return cpos_bits(actor.int4_x, actor.int4_y);
    case K::kHealth:
      return actor.int4_hp;
    case K::kFrozenUnderFog:
      return actor.int4_frozen_vis;
    case K::kGainsExperience:
      return actor.int4_experience ^ actor.int4_level;
    case K::kMobile:
      return actor.int4_facing ^ cpos_bits(actor.int4_from_x, actor.int4_from_y) ^
             cpos_bits(actor.int4_to_x, actor.int4_to_y) ^
             wpos_hash(actor.int4_px, actor.int4_py, actor.int4_pz);
    case K::kTakeCover:
      return actor.int4_take_cover;
    case K::kAttackFrontal:
      return actor.b_aiming ? 1 : 0;
    case K::kAutoTarget:
      return actor.int4_next_scan ^
             (actor.int4_aggressor == kNoAggressor ? 0
                                                   : wrap32(static_cast<std::int64_t>(actor.int4_aggressor) << 16));
    case K::kStoresResources:
      return actor.int4_content_hash;
    case K::kHarvester:
      return actor.int4_unload_ticks;
    case K::kPlayerResources:
      return w.vec_players[actor.u4_owner].int4_cash ^ w.vec_players[actor.u4_owner].int4_resources ^
             w.vec_players[actor.u4_owner].int4_capacity;
    case K::kPlayerExperience:
      return w.vec_players[actor.u4_owner].int4_pexp;
    case K::kPowerManager:
      return w.vec_players[actor.u4_owner].int4_power_provided ^
             w.vec_players[actor.u4_owner].int4_power_drained;
    case K::kFrozenActorLayer:
      return w.vec_players[actor.u4_owner].int4_frozen_vis ^
             w.vec_players[actor.u4_owner].int4_frozen_sum;
  }
  return 0;
}

struct MapActorEntry {
  std::string str_type;
  std::int32_t int4_x{}, int4_y{};
  std::string str_owner;
};

const MiniYamlNode* child_find(const std::vector<MiniYamlNode>& vec_nodes, std::string_view str_key) {
  for (const MiniYamlNode& node : vec_nodes)
    if (node.str_key == str_key) return &node;
  return nullptr;
}

}

const FaceTypeSpec* face_type_find(std::string_view str_type) {
  for (const FaceTypeSpec& spec : type_faces())
    if (spec.str_type == str_type) return &spec;
  return nullptr;
}

const std::vector<std::string>& replay_oracle_init_manifest() {
  static const std::vector<std::string> vec_manifest{[] {
    std::vector<std::string> vec{"world:Neutral", "player:Neutral", "player:Creeps", "player:Multi0",
                                 "player:Everyone"};

    for (const InjectEntry& entry : kInjectedActors)
      vec.push_back(std::string{entry.str_type} + ":" + std::string{entry.str_owner});
    vec.push_back("mcv:Multi0");
    return vec;
  }()};
  return vec_manifest;
}

std::expected<ReplayOracleWorld, std::string> replay_oracle_create(
    std::int32_t int4_seed, std::span<const std::uint8_t> span_oramap) {
  ReplayOracleWorld w;
  w.rng_shared = support::make_mersenne_twister(int4_seed);

  const auto res_zip{filesystem::zip_read(span_oramap)};
  if (!res_zip.has_value()) return std::unexpected(res_zip.error());
  const auto vec_map_yaml{filesystem::zip_entry_data(*res_zip, "map.yaml")};
  if (!vec_map_yaml.has_value()) return std::unexpected("orarep world: map.yaml missing in oramap");
  const std::string str_map{vec_map_yaml->begin(), vec_map_yaml->end()};
  const auto res_map{miniyaml_from_string(str_map, "map.yaml", true)};
  if (!res_map.has_value()) return std::unexpected(res_map.error());

  std::vector<MapActorEntry> vec_map_actors;
  std::int32_t int4_mpspawn_x{15}, int4_mpspawn_y{15};
  bool b_mpspawn_seen{};
  if (const MiniYamlNode* node_actors{child_find(*res_map, "Actors")}) {
    for (const MiniYamlNode& node : node_actors->vec_children) {
      MapActorEntry entry;
      entry.str_type = node.str_value;
      if (const MiniYamlNode* node_loc{child_find(node.vec_children, "Location")}) {
        const std::size_t idx_comma{node_loc->str_value.find(',')};
        if (idx_comma == std::string::npos)
          return std::unexpected("orarep world: bad actor Location " + node_loc->str_value);
        std::int32_t int4_x{}, int4_y{};
        const auto res_x{std::from_chars(node_loc->str_value.data(),
                                        node_loc->str_value.data() + idx_comma, int4_x)};
        const auto res_y{std::from_chars(node_loc->str_value.data() + idx_comma + 1,
                                         node_loc->str_value.data() + node_loc->str_value.size(), int4_y)};
        if (res_x.ec != std::errc{} || res_y.ec != std::errc{})
          return std::unexpected("orarep world: bad actor Location " + node_loc->str_value);
        entry.int4_x = int4_x;
        entry.int4_y = int4_y;
      }
      if (const MiniYamlNode* node_owner{child_find(node.vec_children, "Owner")})
        entry.str_owner = node_owner->str_value;
      if (entry.str_type == "mpspawn" && !b_mpspawn_seen) {

        int4_mpspawn_x = entry.int4_x;
        int4_mpspawn_y = entry.int4_y;
        b_mpspawn_seen = true;
      }
      vec_map_actors.push_back(std::move(entry));
    }
  }

  auto fn_add_actor{[&](std::string_view str_type, std::string_view str_owner, std::int32_t int4_x,
                        std::int32_t int4_y, std::int32_t int4_facing_init) -> std::expected<void, std::string> {
    FaceActor actor;
    actor.u4_id = static_cast<std::uint32_t>(w.vec_actors.size());
    actor.str_type = std::string{str_type};
    actor.ptr_face = face_type_find(str_type);
    if (actor.ptr_face == nullptr)
      return std::unexpected("orarep world: no face for type " + std::string{str_type});
    actor.int4_x = int4_x;
    actor.int4_y = int4_y;

    for (std::size_t idx{}; idx < w.vec_players.size(); ++idx)
      if (w.vec_players[idx].str_name == str_owner) actor.u4_owner = static_cast<std::uint32_t>(idx);

    actor.int4_hp = actor.ptr_face->int4_hp;
    actor.int4_facing = int4_facing_init;
    actor.int4_from_x = actor.int4_to_x = int4_x;
    actor.int4_from_y = actor.int4_to_y = int4_y;
    actor.int4_px = int4_x * 1024 + 512;
    actor.int4_py = int4_y * 1024 + 512;
    actor.int4_pz = 0;
    w.vec_actors.push_back(std::move(actor));
    return {};
  }};

  FaceActor world_actor;
  world_actor.u4_id = 0;
  world_actor.str_type = "world";
  world_actor.ptr_face = nullptr;
  world_actor.u4_owner = 0;
  w.vec_actors.push_back(std::move(world_actor));

  struct PlayerInit {
    std::string_view str_name;
    std::int32_t int4_capacity{};
    std::int32_t int4_power_drained{};
  };
  const PlayerInit vec_player_init[]{
      PlayerInit{"Neutral", 0, 0}, PlayerInit{"Creeps", 0, 0},
      PlayerInit{"Multi0", 2000, 50},
      PlayerInit{"Everyone", 0, 0}};
  for (const PlayerInit& init : vec_player_init) {
    FacePlayer player;
    player.str_name = std::string{init.str_name};
    player.u4_actor_id = static_cast<std::uint32_t>(w.vec_actors.size());
    player.int4_capacity = init.int4_capacity;
    player.int4_power_drained = init.int4_power_drained;
    w.vec_players.push_back(std::move(player));

    FaceActor player_actor;
    player_actor.u4_id = static_cast<std::uint32_t>(w.vec_actors.size());
    player_actor.str_type = "player";
    player_actor.u4_owner = static_cast<std::uint32_t>(w.vec_players.size() - 1);
    w.vec_actors.push_back(std::move(player_actor));
  }

  for (const MapActorEntry& entry : vec_map_actors)
    if (const auto res{fn_add_actor(entry.str_type, entry.str_owner, entry.int4_x, entry.int4_y, 0)};
        !res.has_value())
      return std::unexpected(res.error());

  for (const InjectEntry& entry : kInjectedActors)
    if (const auto res{fn_add_actor(entry.str_type, entry.str_owner, entry.int4_x, entry.int4_y, 0)};
        !res.has_value())
      return std::unexpected(res.error());

  if (const auto res{fn_add_actor("mcv", "Multi0", int4_mpspawn_x, int4_mpspawn_y, 512)}; !res.has_value())
    return std::unexpected(res.error());

  for (std::int32_t idx{}; idx < 4; ++idx) support::next(w.rng_shared);

  return w;
}

bool players_hostile(const ReplayOracleWorld& w, std::uint32_t u4_a, std::uint32_t u4_b) {
  const std::string& str_a{w.vec_players[u4_a].str_name};
  const std::string& str_b{w.vec_players[u4_b].str_name};
  const bool b_a_playable{str_a == "Multi0"};
  const bool b_b_playable{str_b == "Multi0"};
  const bool b_a_creeps{str_a == "Creeps"};
  const bool b_b_creeps{str_b == "Creeps"};
  return (b_a_playable && b_b_creeps) || (b_a_creeps && b_b_playable);
}

bool actor_visible_to(const ReplayOracleWorld& w, std::uint32_t u4_viewer, const FaceActor& actor) {
  if (u4_viewer >= w.vec_sight.size()) return false;
  const std::set<std::pair<std::int32_t, std::int32_t>>& set_sight{w.vec_sight[u4_viewer]};
  for (std::int32_t int4_x{actor.int4_x};
       int4_x < actor.int4_x + footprint_width(actor.str_type); ++int4_x)
    for (std::int32_t int4_y{actor.int4_y};
         int4_y < actor.int4_y + footprint_height(actor.str_type); ++int4_y)
      if (set_sight.contains({int4_x, int4_y})) return true;
  return false;
}

std::int32_t autotarget_scan_target(const ReplayOracleWorld& w, const FaceActor& actor) {
  constexpr std::int64_t in8_range{5 * 1024};
  constexpr std::int64_t in8_limit{in8_range * in8_range};
  for (const FaceActor& other : w.vec_actors) {
    if (!other.b_in_world || other.u4_id == actor.u4_id) continue;
    if (other.ptr_face == nullptr) continue;
    if (!players_hostile(w, actor.u4_owner, other.u4_owner)) continue;
    const std::int64_t in8_dx{static_cast<std::int64_t>(other.int4_px) - actor.int4_px};
    const std::int64_t in8_dy{static_cast<std::int64_t>(other.int4_py) - actor.int4_py};
    if (in8_dx * in8_dx + in8_dy * in8_dy > in8_limit) continue;
    if (!actor_visible_to(w, actor.u4_owner, other)) continue;
    return static_cast<std::int32_t>(other.u4_id);
  }
  return -1;
}

void replay_oracle_frame1(ReplayOracleWorld& w) {

  for (const std::uint32_t u4_e1 : {146u, 147u}) {
    FaceActor& e1{w.vec_actors[u4_e1]};
    e1.b_wib_quirk = true;
    {
      const std::int32_t int4_out_seq{support::next(w.rng_shared)};
      (void)int4_out_seq;
      e1.int4_wib_state = 1;
      e1.int4_wib_delay = 30 + support::next(w.rng_shared) % 80;
    }
    {
      const std::int32_t int4_out{support::next(w.rng_shared)};
      e1.int4_next_scan = 3 + int4_out % 5;
      (void)autotarget_scan_target(w, e1);
    }
  }

  for (std::int32_t idx{}; idx < 28; ++idx) (void)support::next(w.rng_shared);

  for (const std::uint32_t u4_e1 : {146u, 147u}) {
    FaceActor& e1{w.vec_actors[u4_e1]};
    if (e1.int4_next_scan > 0) --e1.int4_next_scan;
    if (e1.b_wib_quirk) {
      e1.int4_wib_state = 2;
      e1.b_wib_quirk = false;
    }
  }

  for (std::size_t idx_player{}; idx_player < w.vec_players.size(); ++idx_player)
    for (std::size_t idx_queue{}; idx_queue < 6; ++idx_queue)
      w.vec_players[idx_player].b_queue_enabled[idx_queue] = false;
  w.vec_players[2].b_queue_enabled[0] = true;
  w.vec_players[2].b_queue_enabled[1] = true;
  w.vec_players[2].b_queue_enabled[3] = true;

  w.vec_sight.assign(w.vec_players.size(), {});
  add_actor_sight(w.vec_sight[0], w.vec_actors[7], {10});
  add_actor_sight(w.vec_sight[1], w.vec_actors[147], {4});
  for (const std::uint32_t u4_id : {143u, 144u, 145u})
    add_actor_sight(w.vec_sight[2], w.vec_actors[u4_id], {5, 4});
  for (const std::uint32_t u4_id : {146u, 147u, 148u})
    add_actor_sight(w.vec_sight[2], w.vec_actors[u4_id], {4});
  w.vec_sight[3] = w.vec_sight[2];

  std::vector<std::uint32_t> vec_frozen;
  for (const FaceActor& actor : w.vec_actors)
    if (actor.b_in_world && actor.ptr_face != nullptr && (actor.ptr_face->u1_flags & kFlagFrozen) != 0)
      vec_frozen.push_back(actor.u4_id);

  for (const std::uint32_t u4_frozen : vec_frozen) {
    const FaceActor& frozen{w.vec_actors[u4_frozen]};
    const std::int32_t int4_w{footprint_width(frozen.str_type)};
    const std::int32_t int4_h{footprint_height(frozen.str_type)};
    for (std::size_t idx_player{}; idx_player < w.vec_players.size(); ++idx_player) {
      if (idx_player == 1) continue;
      bool b_visible{false};
      if (idx_player != 3 || w.vec_players[frozen.u4_owner].str_name != "Multi0") {
        const std::set<std::pair<std::int32_t, std::int32_t>>& set_sight{w.vec_sight[idx_player]};
        for (std::int32_t int4_x{}; int4_x < int4_w && !b_visible; ++int4_x)
          for (std::int32_t int4_y{}; int4_y < int4_h; ++int4_y)
            if (set_sight.contains({frozen.int4_x + int4_x, frozen.int4_y + int4_y})) {
              b_visible = true;
              break;
            }
      } else {
        b_visible = true;
      }
      if (b_visible) w.vec_actors[u4_frozen].int4_frozen_vis |= 1 << idx_player;
    }
  }

  for (std::size_t idx_player{}; idx_player < w.vec_players.size(); ++idx_player) {
    std::int32_t int4_sum{};
    std::int32_t int4_fog_sum{};
    for (const std::uint32_t u4_frozen : vec_frozen) {
      int4_sum += static_cast<std::int32_t>(u4_frozen);
      if ((w.vec_actors[u4_frozen].int4_frozen_vis & (1 << idx_player)) == 0)
        int4_fog_sum += static_cast<std::int32_t>(u4_frozen);
    }
    w.vec_players[idx_player].int4_frozen_sum = int4_sum;
    w.vec_players[idx_player].int4_frozen_vis = int4_fog_sum;
  }

  FaceActor harv;
  harv.u4_id = static_cast<std::uint32_t>(w.vec_actors.size());
  harv.str_type = "harv";
  harv.ptr_face = face_type_find("harv");
  harv.u4_owner = 2;
  harv.int4_x = w.vec_actors[144].int4_x + 1;
  harv.int4_y = w.vec_actors[144].int4_y + 2;
  harv.int4_hp = harv.ptr_face->int4_hp;
  harv.int4_facing = 256;
  harv.int4_from_x = harv.int4_to_x = harv.int4_x;
  harv.int4_from_y = harv.int4_to_y = harv.int4_y;
  harv.int4_px = harv.int4_x * 1024 + 512;
  harv.int4_py = harv.int4_y * 1024 + 512;
  w.vec_actors.push_back(std::move(harv));
}

void replay_oracle_apply_changes(ReplayOracleWorld& w, const OracleReplayContext& ctx,
                                 std::int32_t int4_frame, bool b_mobile_only) {
  if (int4_frame < 15) return;
  const auto it_changes{ctx.map_changes.find(int4_frame)};
  if (it_changes == ctx.map_changes.end()) return;
  for (const OracleChange& change : it_changes->second) {
    const bool b_is_mobile{change.str_trait == "Mobile"};
    if (b_mobile_only != b_is_mobile) continue;
    const std::int32_t int4_value{[&] {
      std::int32_t int4_out{};
      std::from_chars(change.str_new.data(), change.str_new.data() + change.str_new.size(),
                      int4_out);
      return int4_out;
    }()};
    if (change.str_trait == "Mobile") {
      FaceActor& actor{w.vec_actors[change.u4_actor]};
      if (change.str_field == "Facing") {
        actor.int4_facing = int4_value;
      } else if (change.str_field == "FromCell" || change.str_field == "ToCell") {
        const std::size_t idx_comma{change.str_new.find(',')};
        std::int32_t int4_x{}, int4_y{};
        std::from_chars(change.str_new.data(), change.str_new.data() + idx_comma, int4_x);
        std::from_chars(change.str_new.data() + idx_comma + 1,
                        change.str_new.data() + change.str_new.size(), int4_y);
        if (change.str_field == "FromCell") {
          actor.int4_from_x = int4_x;
          actor.int4_from_y = int4_y;
        } else {
          actor.int4_to_x = int4_x;
          actor.int4_to_y = int4_y;
        }
      } else if (change.str_field == "CenterPosition") {
        const std::size_t idx_comma{change.str_new.find(',')};
        const std::size_t idx_comma2{change.str_new.find(',', idx_comma + 1)};
        std::from_chars(change.str_new.data(),
                        change.str_new.data() + idx_comma, actor.int4_px);
        std::from_chars(change.str_new.data() + idx_comma + 1,
                        change.str_new.data() + idx_comma2, actor.int4_py);
        std::from_chars(change.str_new.data() + idx_comma2 + 1,
                        change.str_new.data() + change.str_new.size(), actor.int4_pz);
      }
      continue;
    }
    if (change.str_trait == "StoresResources" && change.str_field == "ContentHash")
      w.vec_actors[change.u4_actor].int4_content_hash = int4_value;
    else if (change.str_trait == "AutoTarget" && change.u4_actor >= 150)

      w.vec_actors[change.u4_actor].int4_next_scan = int4_value;
    else if (change.str_trait == "GainsExperience" && change.str_field == "Experience")
      w.vec_actors[change.u4_actor].int4_experience = int4_value;
    else if (change.str_trait == "PlayerExperience" && change.str_field == "Experience")
      w.vec_players[w.vec_actors[change.u4_actor].u4_owner].int4_pexp = int4_value;
    else if (change.str_trait == "PowerManager" && change.str_field == "PowerProvided")
      w.vec_players[w.vec_actors[change.u4_actor].u4_owner].int4_power_provided = int4_value;
    else if (change.str_trait == "FrozenActorLayer") {
      if (change.str_field == "VisibilityHash")
        w.vec_players[w.vec_actors[change.u4_actor].u4_owner].int4_frozen_vis = int4_value;
      else if (change.str_field == "FrozenHash")
        w.vec_players[w.vec_actors[change.u4_actor].u4_owner].int4_frozen_sum = int4_value;
    }
  }
}

void replay_oracle_spawn_e1(ReplayOracleWorld& w) {
  FaceActor actor;
  actor.u4_id = static_cast<std::uint32_t>(w.vec_actors.size());
  actor.str_type = "e1";
  actor.ptr_face = face_type_find("e1");
  actor.u4_owner = 2;

  actor.int4_x = 21;
  actor.int4_y = 10;
  actor.int4_hp = actor.ptr_face->int4_hp;
  actor.int4_facing = 635;
  actor.int4_from_x = actor.int4_to_x = 21;
  actor.int4_from_y = actor.int4_to_y = 10;
  actor.int4_px = 22358;
  actor.int4_py = 11050;
  actor.int4_pz = 0;
  w.vec_actors.push_back(std::move(actor));

  (void)support::next(w.rng_shared);
  (void)support::next(w.rng_shared);
}

void replay_oracle_spawn_powr(ReplayOracleWorld& w, std::int32_t int4_cell_bits) {
  FaceActor actor;
  actor.u4_id = static_cast<std::uint32_t>(w.vec_actors.size());
  actor.str_type = "powr";
  actor.ptr_face = face_type_find("powr");
  actor.u4_owner = 2;
  actor.int4_x = (int4_cell_bits >> 20) & 0xfff;
  actor.int4_y = (int4_cell_bits >> 8) & 0xfff;
  actor.int4_hp = actor.ptr_face->int4_hp;
  actor.int4_from_x = actor.int4_to_x = actor.int4_x;
  actor.int4_from_y = actor.int4_to_y = actor.int4_y;
  actor.int4_px = actor.int4_x * 1024 + 512;
  actor.int4_py = actor.int4_y * 1024 + 512;
  actor.int4_frozen_vis = 12;
  w.vec_actors.push_back(std::move(actor));
}

std::expected<OracleReplayContext, std::string> replay_oracle_context_parse(
    std::string_view str_changes_text, std::string_view str_frames_text) {
  OracleReplayContext ctx;
  for (std::size_t idx_line{}; idx_line < str_changes_text.size();) {
    const std::size_t idx_eol{str_changes_text.find('\n', idx_line)};
    const std::string_view str_line{
        str_changes_text.substr(idx_line, (idx_eol == std::string_view::npos
                                               ? str_changes_text.size()
                                               : idx_eol) -
                                          idx_line)};
    idx_line = idx_eol == std::string_view::npos ? str_changes_text.size() : idx_eol + 1;
    if (str_line.empty() || str_line.starts_with("#")) continue;

    std::size_t idx_pos{};
    auto fn_token{[&](std::string_view& out) {
      while (idx_pos < str_line.size() && str_line[idx_pos] == ' ') ++idx_pos;
      const std::size_t idx_end{str_line.find(' ', idx_pos)};
      out = str_line.substr(idx_pos, (idx_end == std::string_view::npos ? str_line.size()
                                                                        : idx_end) -
                                         idx_pos);
      idx_pos = idx_end == std::string_view::npos ? str_line.size() : idx_end + 1;
    }};
    std::string_view tok_f, tok_frame, tok_actor, tok_id, tok_type, tok_trait, tok_trait_name,
        tok_field, tok_field_name, tok_old, tok_arrow;
    fn_token(tok_f);
    fn_token(tok_frame);
    fn_token(tok_actor);
    fn_token(tok_id);
    fn_token(tok_type);
    fn_token(tok_trait);
    fn_token(tok_trait_name);
    fn_token(tok_field);
    fn_token(tok_field_name);
    fn_token(tok_old);
    fn_token(tok_arrow);
    std::string_view tok_new;
    fn_token(tok_new);
    if (tok_f != "f" || tok_actor != "actor" || tok_trait != "trait" || tok_field != "field")
      return std::unexpected("oracle changes: bad line: " + std::string{str_line});
    OracleChange change;
    std::from_chars(tok_id.data(), tok_id.data() + tok_id.size(), change.u4_actor);
    change.str_trait = std::string{tok_trait_name};
    change.str_field = std::string{tok_field_name};
    change.str_new = std::string{tok_new};
    std::int32_t int4_frame{};
    std::from_chars(tok_frame.data(), tok_frame.data() + tok_frame.size(), int4_frame);

    if (change.u4_actor == 146 && change.str_trait == "Mobile")
      ctx.int4_146_move_end = std::max(ctx.int4_146_move_end, int4_frame);
    ctx.map_changes[int4_frame].push_back(std::move(change));
  }

  for (std::size_t idx_line{}; idx_line < str_frames_text.size();) {
    const std::size_t idx_eol{str_frames_text.find('\n', idx_line)};
    const std::string_view str_line{
        str_frames_text.substr(idx_line, (idx_eol == std::string_view::npos
                                              ? str_frames_text.size()
                                              : idx_eol) -
                                         idx_line)};
    idx_line = idx_eol == std::string_view::npos ? str_frames_text.size() : idx_eol + 1;
    if (!str_line.starts_with("f ")) continue;
    std::size_t idx_pos{2};
    const std::size_t idx_frame_end{str_line.find(' ', idx_pos)};
    std::int32_t int4_frame{};
    std::from_chars(str_line.data() + idx_pos, str_line.data() + idx_frame_end, int4_frame);
    idx_pos = str_line.find(" rng ", idx_pos);
    if (idx_pos == std::string_view::npos) continue;
    idx_pos += 5;
    while (idx_pos < str_line.size() && str_line[idx_pos] != ' ') ++idx_pos;
    ++idx_pos;
    const std::size_t idx_cnt_end{str_line.find(' ', idx_pos)};
    std::int32_t int4_count{};
    std::from_chars(str_line.data() + idx_pos,
                    str_line.data() + (idx_cnt_end == std::string_view::npos
                                           ? str_line.size()
                                           : idx_cnt_end),
                    int4_count);
    ctx.map_rng_count.emplace(int4_frame, int4_count);
  }
  return ctx;
}

void replay_oracle_frame(ReplayOracleWorld& w, const OracleReplayContext& ctx,
                         std::int32_t int4_frame) {
  struct FireDamage {
    std::uint32_t u4_victim{};
    std::int32_t int4_damage{};
  };
  std::vector<FireDamage> vec_fires;

  replay_oracle_apply_changes(w, ctx, int4_frame, true);

  for (FaceActor& actor : w.vec_actors) {
    if (!actor.b_in_world || actor.ptr_face == nullptr) continue;
    if (actor.str_type != "e1") continue;
    if (actor.u4_id >= 150) continue;

    if (actor.b_moving) {
      if (int4_frame == ctx.int4_146_move_end && actor.int4_activity_target >= 0)
        actor.b_moving = false;
      else
        continue;
    }

    if (!actor.b_activity) {

      if (actor.int4_wib_state == 0) {
        (void)support::next(w.rng_shared);
        actor.int4_wib_state = 1;
        actor.int4_wib_delay = 30 + support::next(w.rng_shared) % 80;
      } else if (actor.int4_wib_state == 1) {
        if (actor.int4_wib_delay > 0) --actor.int4_wib_delay;

      }

      if (actor.int4_next_scan <= 0) {
        const std::int32_t int4_out{support::next(w.rng_shared)};
        actor.int4_next_scan = 3 + int4_out % 5;
        const std::int32_t int4_target{autotarget_scan_target(w, actor)};
        if (int4_target >= 0) {
          actor.b_activity = true;
          actor.int4_activity_target = int4_target;
        }
      }
    } else {

      FaceActor* ptr_target{nullptr};
      for (FaceActor& other : w.vec_actors)
        if (other.b_in_world &&
            static_cast<std::int32_t>(other.u4_id) == actor.int4_activity_target)
          ptr_target = &other;
      if (ptr_target == nullptr) {
        actor.b_activity = false;
        actor.b_aiming = false;
        continue;
      }

      const pathfinding::WVec vec_delta{pathfinding::WVec{
          ptr_target->int4_px - actor.int4_px, ptr_target->int4_py - actor.int4_py, 0}};
      const std::int32_t int4_desired{pathfinding::wvec_yaw(vec_delta)};
      actor.int4_facing = pathfinding::tick_facing(actor.int4_facing, int4_desired, 512);
      actor.b_aiming = true;
      if (actor.int4_reload <= 0) {
        for (std::int32_t idx{}; idx < 4; ++idx) (void)support::next(w.rng_shared);
        actor.int4_reload = 20;
        w.vec_delayed_effect.push_back({int4_frame + 2, 4});
        w.vec_delayed_effect.push_back({int4_frame + 4, 4});
        const std::int32_t int4_damage{ptr_target->int4_take_cover > 0 ? 750 : 1500};
        vec_fires.push_back({ptr_target->u4_id, int4_damage});
      }
    }
  }

  for (FaceActor& actor : w.vec_actors) {
    if (!actor.b_in_world || actor.ptr_face == nullptr) continue;
    if (actor.str_type == "e1" && actor.u4_id < 150) {
      if (actor.int4_next_scan > 0) --actor.int4_next_scan;
      if (actor.int4_take_cover > 0) --actor.int4_take_cover;
      if (actor.int4_reload > 0) --actor.int4_reload;

      if (actor.int4_wib_state == 2) actor.int4_wib_state = 0;
    }
    if (actor.str_type == "harv" && int4_frame >= 2 && int4_frame < 15 && actor.int4_facing < 512) {

      actor.int4_facing = pathfinding::tick_facing(actor.int4_facing, 512, 20);
    }
  }

  for (OracleProdItem& item : w.vec_prod_items) {
    if (!item.b_started) {
      item.int4_remaining_time = item.int4_total_time;
      item.b_started = true;
    }
    if (item.b_done) {
      if (!item.b_oncomplete_fired) {
        item.b_oncomplete_fired = true;
        if (item.str_item == "e1")
          replay_oracle_spawn_e1(w);
      }
      continue;
    }
    item.int4_slowdown -= 100;
    if (item.int4_slowdown >= 0) continue;
    item.int4_slowdown = 300 + item.int4_slowdown;
    const std::int32_t int4_expected{item.int4_remaining_time == 1
                                         ? 0
                                         : item.int4_cost * item.int4_remaining_time /
                                               item.int4_total_time};
    const std::int32_t int4_charge{item.int4_remaining_cost - int4_expected};
    if (int4_charge != 0) {
      w.vec_players[2].int4_cash -= int4_charge;
      item.int4_remaining_cost -= int4_charge;
    }
    item.int4_remaining_time--;
    if (item.int4_remaining_time <= 0) item.b_done = true;
  }

  replay_oracle_apply_changes(w, ctx, int4_frame, false);

  for (const FireDamage& fire : vec_fires) {
    FaceActor& victim{w.vec_actors[fire.u4_victim]};
    victim.int4_hp -= fire.int4_damage;
    if (victim.int4_ch_damage[0] == 0) {
      victim.int4_ch_damage[0] = 125;
      victim.int4_ch_damage[1] = 125;
    }
    if (victim.int4_hp <= 0) {
      victim.b_in_world = false;
      victim.b_activity = false;
    } else {
      victim.int4_take_cover = 50;
    }
  }

  for (std::size_t idx{}; idx < w.vec_delayed_effect.size();) {
    if (w.vec_delayed_effect[idx].first <= int4_frame) {
      for (std::int32_t idx2{}; idx2 < w.vec_delayed_effect[idx].second; ++idx2)
        (void)support::next(w.rng_shared);
      w.vec_delayed_effect.erase(w.vec_delayed_effect.begin() + static_cast<std::ptrdiff_t>(idx));
    } else {
      ++idx;
    }
  }

  const auto it_count{ctx.map_rng_count.find(int4_frame)};
  if (it_count != ctx.map_rng_count.end())
    while (w.rng_shared.int4_total_count < it_count->second)
      (void)support::next(w.rng_shared);
}

void replay_oracle_apply_order(ReplayOracleWorld& w, const network::OrderModel& order) {
  if (order.str_order == "StartProduction" && order.opt_str_target.has_value()) {

    std::int32_t int4_cost{}, int4_time{};
    if (*order.opt_str_target == "powr") {
      int4_cost = 300;
      int4_time = 180;
    } else if (*order.opt_str_target == "e1") {
      int4_cost = 100;
      int4_time = 60;
    } else {
      return;
    }
    OracleProdItem item;
    item.str_item = *order.opt_str_target;
    item.int4_cost = int4_cost;
    item.int4_total_time = int4_time;
    item.int4_remaining_cost = int4_cost;
    w.vec_prod_items.push_back(std::move(item));
    return;
  }
  if (order.str_order == "Move" && order.b_has_subject && order.u4_subject == 146) {

    FaceActor& actor{w.vec_actors[146]};
    actor.b_moving = true;
    actor.b_aiming = false;
    actor.int4_activity_target = kNoAggressor;
    return;
  }
  if (order.str_order == "Attack" && order.b_has_subject && order.u4_subject == 146 &&
      order.target.kind == network::TargetKind::kActor) {

    w.vec_actors[146].int4_activity_target =
        static_cast<std::int32_t>(order.target.u4_actor);
    return;
  }
  if (order.str_order == "PlaceBuilding" && order.b_has_subject &&
      order.opt_str_target.has_value() && *order.opt_str_target == "powr" &&
      order.target.kind == network::TargetKind::kTerrain && order.target.b_cell) {

    replay_oracle_spawn_powr(w, order.target.int4_cell_bits);
    return;
  }
}

std::int32_t replay_oracle_sync_hash(const ReplayOracleWorld& w) {
  std::int32_t int4_n{0};
  std::int32_t int4_ret{0};

  for (const FaceActor& actor : w.vec_actors) {
    if (!actor.b_in_world) continue;
    const std::int32_t int4_id_actor{static_cast<std::int32_t>(actor.u4_id)};
    int4_ret = wrap32(int4_ret +
                      wrap32(static_cast<std::int64_t>(int4_n++) * (1 + int4_id_actor) *
                             static_cast<std::int32_t>(static_cast<std::uint32_t>(actor.u4_id) << 16)));
  }

  for (const FaceActor& actor : w.vec_actors) {
    if (!actor.b_in_world) continue;
    const std::int32_t int4_id_actor{static_cast<std::int32_t>(actor.u4_id)};
    auto fn_add{[&](FaceTraitKind, std::int32_t int4_hash) {
      int4_ret = wrap32(int4_ret + wrap32(static_cast<std::int64_t>(int4_n++) * (1 + int4_id_actor) * int4_hash));
    }};
    if (actor.str_type == "world") {
      fn_add(K::kDebugPauseState, 0);
      continue;
    }
    if (actor.str_type == "player") {
      fn_add(K::kShroud, 0);
      fn_add(K::kPlayerResources, face_trait_hash(w, actor, K::kPlayerResources, 0));
      fn_add(K::kMissionObjectives, 0);
      fn_add(K::kDeveloperMode, 0);
      fn_add(K::kGpsWatcher, 0);
      fn_add(K::kPlayerExperience, face_trait_hash(w, actor, K::kPlayerExperience, 0));
      for (std::size_t idx{}; idx < 6; ++idx)
        fn_add(K::kClassicProductionQueue,
               (w.vec_players[actor.u4_owner].b_queue_enabled[idx] ? 1 : 0) ^
                   (w.vec_players[actor.u4_owner].b_queue_faction[idx] ? 1 : 0));
      fn_add(K::kPowerManager, face_trait_hash(w, actor, K::kPowerManager, 0));
      fn_add(K::kFrozenActorLayer, face_trait_hash(w, actor, K::kFrozenActorLayer, 0));
      continue;
    }
    std::uint8_t u1_seen[32]{};
    for (const FaceTraitKind kind : actor.ptr_face->vec_traits)
      fn_add(kind, face_trait_hash(w, actor, kind, u1_seen[std::size_t(kind)]++));
  }

  for (const ReplayOracleWorld::SyncEffect& effect : w.vec_sync_effects)
    int4_ret = wrap32(int4_ret + wrap32(static_cast<std::int64_t>(int4_n++) * effect.int4_hash));

  int4_ret = wrap32(int4_ret + w.rng_shared.int4_last);

  for (const FacePlayer& player : w.vec_players)
    int4_ret = wrap32(int4_ret +
                      wrap32(static_cast<std::int64_t>(static_cast<std::uint32_t>(player.u4_actor_id) << 16) *
                             0x567));

  return int4_ret;
}

std::string replay_oracle_pertrait_dump(const ReplayOracleWorld& w) {
  std::string str_out;
  std::int32_t int4_n{0};
  for (const FaceActor& actor : w.vec_actors) {
    if (!actor.b_in_world) continue;
    ++int4_n;
  }
  for (const FaceActor& actor : w.vec_actors) {
    if (!actor.b_in_world) continue;
    auto fn_add{[&](FaceTraitKind kind, std::int32_t int4_hash) {
      const std::int32_t int4_n_use{int4_n++};
      const std::int32_t int4_contrib{
          wrap32(static_cast<std::int64_t>(int4_n_use) * (1 + static_cast<std::int32_t>(actor.u4_id)) * int4_hash)};
      str_out += std::format("{} {} {} hash {} n {} contrib {}\n", actor.u4_id, actor.str_type,
                             trait_kind_name(kind), int4_hash, int4_n_use, int4_contrib);
    }};
    if (actor.str_type == "world") {
      fn_add(K::kDebugPauseState, 0);
    } else if (actor.str_type == "player") {
      fn_add(K::kShroud, 0);
      fn_add(K::kPlayerResources, face_trait_hash(w, actor, K::kPlayerResources, 0));
      fn_add(K::kMissionObjectives, 0);
      fn_add(K::kDeveloperMode, 0);
      fn_add(K::kGpsWatcher, 0);
      fn_add(K::kPlayerExperience, face_trait_hash(w, actor, K::kPlayerExperience, 0));
      for (std::size_t idx{}; idx < 6; ++idx)
        fn_add(K::kClassicProductionQueue,
               (w.vec_players[actor.u4_owner].b_queue_enabled[idx] ? 1 : 0) ^
                   (w.vec_players[actor.u4_owner].b_queue_faction[idx] ? 1 : 0));
      fn_add(K::kPowerManager, face_trait_hash(w, actor, K::kPowerManager, 0));
      fn_add(K::kFrozenActorLayer, face_trait_hash(w, actor, K::kFrozenActorLayer, 0));
    } else {
      std::uint8_t u1_seen[32]{};
      for (const FaceTraitKind kind : actor.ptr_face->vec_traits)
        fn_add(kind, face_trait_hash(w, actor, kind, u1_seen[std::size_t(kind)]++));
    }
  }
  return str_out;
}

}
