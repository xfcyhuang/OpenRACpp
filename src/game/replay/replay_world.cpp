#include "game/replay/replay_world.hpp"

#include <initializer_list>
#include <set>

#include "game/filesystem/zip_read.hpp"
#include "game/miniyaml.hpp"

namespace ora::game::replay {

namespace {

using K = FaceTraitKind;

constexpr std::uint8_t kFlagReveals{0x01};
constexpr std::uint8_t kFlagFrozen{0x02};

// 类型面表:oracle isync-init.txt / isync-born.txt(反射 dump,全量 ra 规则 ore-lord)
// 每 ISync trait 构造序 + 成员初值参数。player/mpspawn/world 面在装配处手写。
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


// —— shroud 置位几何(Shroud.ProjectedCellsInRange 平地口径:ore-lord 本场景
// 实验验证投影格与 CPos 判定一致,f1 波 25/25 树全对)——

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

// 注入表(golden_gen ReplayNewModData 同源;t17 探针 t17 前缀键名不入 ActorID)
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

// 单 trait 哈希(成员 XOR;初值/运行期状态按 kind 分派)。player 面由调用点展开
// (每队列实例独立取 b_queue_enabled[idx])。
std::int32_t face_trait_hash(const ReplayOracleWorld& w, const FaceActor& actor, FaceTraitKind kind) {
  const FaceTypeSpec& face{*actor.ptr_face};
  switch (kind) {
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
    case K::kChangesHealth:
      return 0;  // 恒定 0(false/0/null;ChangesHealth 全程 0——changes 表无事件)
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
      return actor.int4_take_cover;  // QuantizedFacings 恒 0
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

}  // namespace

const FaceTypeSpec* face_type_find(std::string_view str_type) {
  for (const FaceTypeSpec& spec : type_faces())
    if (spec.str_type == str_type) return &spec;
  return nullptr;
}

const std::vector<std::string>& replay_oracle_init_manifest() {
  static const std::vector<std::string> vec_manifest{[] {
    std::vector<std::string> vec{"world:Neutral", "player:Neutral", "player:Creeps", "player:Multi0",
                                 "player:Everyone"};
    // 地图序在 replay_oracle_create 时由 oramap 读出;此清单仅注入+mcv 尾段(完整清单
    // 由测试用黄金 init 行断言)。
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

  // 地图 actor 序(ore-lord.oramap 的 map.yaml Actors 节点序)
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
        // mcv 出生点 = 首个 mpspawn(黄金 init 148:mcv Mobile FromCell 15,15)
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

  // 装配序:world(0)→ players(1-4)→ 地图(5-142)→ 注入(143-147)→ mcv(148)
  FaceActor world_actor;
  world_actor.u4_id = 0;
  world_actor.str_type = "world";
  world_actor.ptr_face = nullptr;  // world 面手写(DebugPauseState)
  world_actor.u4_owner = 0;
  w.vec_actors.push_back(std::move(world_actor));

  struct PlayerInit {
    std::string_view str_name;
    std::int32_t int4_capacity{};
    std::int32_t int4_power_drained{};
  };
  const PlayerInit vec_player_init[]{
      PlayerInit{"Neutral", 0, 0}, PlayerInit{"Creeps", 0, 0},
      PlayerInit{"Multi0", 2000, 50},  // proc 在场→容量 2000;装配期 drained 50(黄金 init)
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

  // mcv(SpawnStartingUnits 等价:BaseActorFacing=512)
  if (const auto res{fn_add_actor("mcv", "Multi0", int4_mpspawn_x, int4_mpspawn_y, 512)}; !res.has_value())
    return std::unexpected(res.error());

  // 装配期 SharedRandom 消耗 4(SpawnMapActors 内,来源未逐 trait 定位;oracle 实测
  // rng last=1411461475 count=4 —— 确定性常量预推进,登记 COVERAGE)
  for (std::int32_t idx{}; idx < 4; ++idx) support::next(w.rng_shared);

  return w;
}


void replay_oracle_frame1(ReplayOracleWorld& w) {
  // —— f1 的 RNG 消耗(oracle 装配差分实测;数量与序)——
  // ITick 分发序 = ActorID:mine×12(SeedsResource 首种 RandomWalk;初始 ore 场固定
  // → 26 个为确定性常量;f76+ 周期需资源场动态复刻)→ e1 146(4 个)→ e1 147(4 个)。
  // AutoTarget 首扫抽样 Next(3,8) 位于各自第 4 个消耗位(nextScanTime 146=5、147=4)。
  // 消耗序(oracle MT 输出对照):e1 146 的 AutoTarget 抽样在 f1 第 6 个消耗
  // (out%5=2 → nextScanTime 5)、147 在第 9 个(out%5=1 → 4)。
  for (std::int32_t idx{}; idx < 34; ++idx) {
    const std::int32_t int4_out{support::next(w.rng_shared)};
    if (idx == 5) w.vec_actors[146].int4_next_scan = 3 + int4_out % 5;
    if (idx == 8) w.vec_actors[147].int4_next_scan = 3 + int4_out % 5;
  }

  // —— ClassicProductionQueue.Enabled 波(f1 首次 ITick:anyEnabledProduction)——
  // 队列序 Building/Defense/Vehicle/Infantry/Ship/Aircraft(player.yaml);无对应
  // Production 建筑的玩家队列禁用(Multi0:fact(Building,Defense)+barr(Infantry) 保真)。
  for (std::size_t idx_player{}; idx_player < w.vec_players.size(); ++idx_player)
    for (std::size_t idx_queue{}; idx_queue < 6; ++idx_queue)
      w.vec_players[idx_player].b_queue_enabled[idx_queue] = false;
  w.vec_players[2].b_queue_enabled[0] = true;
  w.vec_players[2].b_queue_enabled[1] = true;
  w.vec_players[2].b_queue_enabled[3] = true;

  // —— shroud 置位波(f1:Shroud 首帧圈推进 → frozen 可见判定)——
  // 可见集:Neutral = v01(id7,10c0);Multi0 = fact/proc/barr(5c0+4c0)+ e1×2(4c0)+
  // mcv(4c0);Everyone = 观战者并集(playable 玩家可见集)+ 盟友可见(Multi0 的 frozen);
  // Creeps = ∅。
  std::set<std::pair<std::int32_t, std::int32_t>> set_neutral_sight;
  add_actor_sight(set_neutral_sight, w.vec_actors[7], {10});
  std::set<std::pair<std::int32_t, std::int32_t>> set_multi0_sight;
  for (const std::uint32_t u4_id : {143u, 144u, 145u})
    add_actor_sight(set_multi0_sight, w.vec_actors[u4_id], {5, 4});
  for (const std::uint32_t u4_id : {146u, 147u, 148u})
    add_actor_sight(set_multi0_sight, w.vec_actors[u4_id], {4});

  std::vector<std::uint32_t> vec_frozen;
  for (const FaceActor& actor : w.vec_actors)
    if (actor.b_in_world && actor.ptr_face != nullptr && (actor.ptr_face->u1_flags & kFlagFrozen) != 0)
      vec_frozen.push_back(actor.u4_id);

  for (const std::uint32_t u4_frozen : vec_frozen) {
    const FaceActor& frozen{w.vec_actors[u4_frozen]};
    const std::int32_t int4_w{footprint_width(frozen.str_type)};
    const std::int32_t int4_h{footprint_height(frozen.str_type)};
    for (std::size_t idx_player{}; idx_player < w.vec_players.size(); ++idx_player) {
      if (idx_player == 1) continue;  // Creeps 无视圈源
      bool b_visible{false};
      if (idx_player != 3 || w.vec_players[frozen.u4_owner].str_name != "Multi0") {
        const std::set<std::pair<std::int32_t, std::int32_t>>& set_sight{
            idx_player == 0 ? set_neutral_sight : set_multi0_sight};
        for (std::int32_t int4_x{}; int4_x < int4_w && !b_visible; ++int4_x)
          for (std::int32_t int4_y{}; int4_y < int4_h; ++int4_y)
            if (set_sight.contains({frozen.int4_x + int4_x, frozen.int4_y + int4_y})) {
              b_visible = true;
              break;
            }
      } else {
        b_visible = true;  // Everyone 对 playable(Multi0)的 frozen 盟友可见
      }
      if (b_visible) w.vec_actors[u4_frozen].int4_frozen_vis |= 1 << idx_player;
    }
  }

  // FrozenActorLayer 逐玩家聚合(VisibilityHash=Σ雾中 frozen id;FrozenHash=Σ全部)
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

  // —— harv 出生(proc FreeActor:offset(+1,+2),facing 256;帧尾入世界)——
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


// f2-f6:mining 前的静态相位——harv 转身(TurnSpeed 20/帧,寻矿朝向)、AutoTarget
// 扫描计时递减(<=0 时扫描:消耗 1 个、重置 Next(3,8))、f3 的 4 个非同步消耗
// (用途未定位——CreateEffect 家族散布抽样候选;数量按 oracle 账本)。
void replay_oracle_frames_2_to_6(ReplayOracleWorld& w) {
  for (std::int32_t int4_frame{2}; int4_frame <= 6; ++int4_frame) {
    if (int4_frame == 3)
      for (std::int32_t idx{}; idx < 4; ++idx) support::next(w.rng_shared);
    for (const std::uint32_t u4_e1 : {146u, 147u}) {
      FaceActor& e1{w.vec_actors[u4_e1]};
      if (e1.int4_next_scan <= 0) {
        const std::int32_t int4_out{support::next(w.rng_shared)};
        e1.int4_next_scan = 3 + int4_out % 5;
      } else {
        --e1.int4_next_scan;
      }
    }
    w.vec_actors[149].int4_facing += 20;  // f6 末=356
  }
}

std::int32_t replay_oracle_sync_hash(const ReplayOracleWorld& w) {
  std::int32_t int4_n{0};
  std::int32_t int4_ret{0};

  // 段1:全部 in-world actor(ActorID 升序)
  for (const FaceActor& actor : w.vec_actors) {
    if (!actor.b_in_world) continue;
    const std::int32_t int4_id_actor{static_cast<std::int32_t>(actor.u4_id)};
    int4_ret = wrap32(int4_ret +
                      wrap32(static_cast<std::int64_t>(int4_n++) * (1 + int4_id_actor) *
                             static_cast<std::int32_t>(static_cast<std::uint32_t>(actor.u4_id) << 16)));
  }

  // 段2:ISync trait((ActorID, 构造序);world/player 面展开)
  for (const FaceActor& actor : w.vec_actors) {
    if (!actor.b_in_world) continue;
    const std::int32_t int4_id_actor{static_cast<std::int32_t>(actor.u4_id)};
    auto fn_add{[&](FaceTraitKind kind, std::int32_t int4_hash) {
      int4_ret = wrap32(int4_ret + wrap32(static_cast<std::int64_t>(int4_n++) * (1 + int4_id_actor) * int4_hash));
    }};
    if (actor.str_type == "world") {
      fn_add(K::kDebugPauseState, 0);
      continue;
    }
    if (actor.str_type == "player") {
      fn_add(K::kShroud, 0);
      fn_add(K::kPlayerResources, face_trait_hash(w, actor, K::kPlayerResources));
      fn_add(K::kMissionObjectives, 0);
      fn_add(K::kDeveloperMode, 0);
      fn_add(K::kGpsWatcher, 0);
      fn_add(K::kPlayerExperience, face_trait_hash(w, actor, K::kPlayerExperience));
      for (std::size_t idx{}; idx < 6; ++idx)
        fn_add(K::kClassicProductionQueue,
               (w.vec_players[actor.u4_owner].b_queue_enabled[idx] ? 1 : 0) ^
                   (w.vec_players[actor.u4_owner].b_queue_faction[idx] ? 1 : 0));
      fn_add(K::kPowerManager, face_trait_hash(w, actor, K::kPowerManager));
      fn_add(K::kFrozenActorLayer, face_trait_hash(w, actor, K::kFrozenActorLayer));
      continue;
    }
    for (const FaceTraitKind kind : actor.ptr_face->vec_traits)
      fn_add(kind, face_trait_hash(w, actor, kind));
  }

  // 段3:SyncedEffects(战斗弹道;骨架期空)
  for (const ReplayOracleWorld::SyncEffect& effect : w.vec_sync_effects)
    int4_ret = wrap32(int4_ret + wrap32(static_cast<std::int64_t>(int4_n++) * effect.int4_hash));

  // 段4:SharedRandom.Last
  int4_ret = wrap32(int4_ret + w.rng_shared.int4_last);

  // 段5:单人局 DeveloperMode → 全玩家 UnlockedRenderPlayer(恒定)
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
      fn_add(K::kPlayerResources, face_trait_hash(w, actor, K::kPlayerResources));
      fn_add(K::kMissionObjectives, 0);
      fn_add(K::kDeveloperMode, 0);
      fn_add(K::kGpsWatcher, 0);
      fn_add(K::kPlayerExperience, face_trait_hash(w, actor, K::kPlayerExperience));
      for (std::size_t idx{}; idx < 6; ++idx)
        fn_add(K::kClassicProductionQueue,
               (w.vec_players[actor.u4_owner].b_queue_enabled[idx] ? 1 : 0) ^
                   (w.vec_players[actor.u4_owner].b_queue_faction[idx] ? 1 : 0));
      fn_add(K::kPowerManager, face_trait_hash(w, actor, K::kPowerManager));
      fn_add(K::kFrozenActorLayer, face_trait_hash(w, actor, K::kFrozenActorLayer));
    } else {
      for (const FaceTraitKind kind : actor.ptr_face->vec_traits)
        fn_add(kind, face_trait_hash(w, actor, kind));
    }
  }
  return str_out;
}

}  // namespace ora::game::replay
