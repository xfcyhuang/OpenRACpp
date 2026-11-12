module ora.game.world;

import std;
import ora.game;
import ora.game.filesystem;
import ora.game.miniyaml;
import ora.game.pathfinding;
import ora.game.support;

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
                   .u1_frozen_init = 0b10100,
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
      // —— bot 剧本（T34）面：weap/dome/tsla/sam/apwr/e3/e2（trait 序=上游装配序，
      // bot-diag f2 pertrait 对拍定标） ——
      FaceTypeSpec{.str_type = "weap", .int4_hp = 150000, .int4_facings = 1, .u1_flags = kFlagReveals | kFlagFrozen,
                   .vec_traits = {K::kBodyOrientation, K::kBuilding, K::kHealth, K::kRevealsShroud, K::kRevealsShroud, K::kFrozenUnderFog, K::kRepairableBuilding}},
      FaceTypeSpec{.str_type = "dome", .int4_hp = 100000, .int4_facings = 1,
                   .u1_flags = kFlagReveals | kFlagFrozen, .u1_reveals_init = 0b010,
                   .vec_traits = {K::kBodyOrientation, K::kBuilding, K::kToggleConditionOnOrder, K::kHealth, K::kRevealsShroud, K::kRevealsShroud, K::kRevealsShroud, K::kFrozenUnderFog, K::kRepairableBuilding}},
      FaceTypeSpec{.str_type = "tsla", .int4_hp = 40000, .int4_facings = 1, .u1_flags = kFlagReveals | kFlagFrozen,
                   .vec_traits = {K::kBodyOrientation, K::kBuilding, K::kToggleConditionOnOrder, K::kHealth, K::kRevealsShroud, K::kRevealsShroud, K::kAttackTesla, K::kFrozenUnderFog, K::kRepairableBuilding, K::kAutoTarget}},
      FaceTypeSpec{.str_type = "sam", .int4_hp = 40000, .int4_facings = 1, .u1_flags = kFlagReveals | kFlagFrozen,
                   .u1_frozen_init = 0b10100, .int4_turret_facings = 32,
                   .vec_traits = {K::kBuilding, K::kToggleConditionOnOrder, K::kHealth, K::kRevealsShroud, K::kRevealsShroud, K::kClassicFacingBodyOrientation, K::kFrozenUnderFog, K::kRepairableBuilding, K::kTurreted, K::kAttackTurreted, K::kAutoTarget}},
      FaceTypeSpec{.str_type = "apwr", .int4_hp = 70000, .int4_facings = 1, .u1_flags = kFlagReveals | kFlagFrozen,
                   .u1_frozen_init = 0b10100,
                   .vec_traits = {K::kBodyOrientation, K::kBuilding, K::kHealth, K::kRevealsShroud, K::kFrozenUnderFog, K::kRepairableBuilding}},
      FaceTypeSpec{.str_type = "e3", .int4_hp = 4500, .int4_facings = 8, .u1_flags = kFlagReveals,
                   .vec_traits = {K::kGainsExperience, K::kBodyOrientation, K::kHealth, K::kRevealsShroud, K::kMobile, K::kChangesHealth, K::kChangesHealth, K::kTakeCover, K::kAttackFrontal, K::kAutoTarget}},
      FaceTypeSpec{.str_type = "e2", .int4_hp = 5000, .int4_facings = 8, .u1_flags = kFlagReveals,
                   .vec_traits = {K::kGainsExperience, K::kBodyOrientation, K::kHealth, K::kRevealsShroud, K::kMobile, K::kChangesHealth, K::kChangesHealth, K::kTakeCover, K::kAttackFrontal, K::kAutoTarget}},
  };
  return vec;
}

std::int32_t footprint_width(std::string_view str_type) {
  if (str_type == "v01" || str_type == "v02" || str_type == "v04") return 2;
  if (str_type == "v05" || str_type == "v06" || str_type == "v07") return 2;
  if (str_type == "fact" || str_type == "proc") return 3;
  if (str_type == "barr" || str_type == "powr" || str_type == "weap" || str_type == "apwr") return 2;
  if (str_type == "dome") return 2;
  return 1;
}

std::int32_t footprint_height(std::string_view str_type) {
  if (str_type == "v01" || str_type == "v02" || str_type == "v04") return 2;
  if (str_type == "rushouse") return 2;
  if (str_type == "fact" || str_type == "proc") return 4;
  if (str_type == "barr" || str_type == "powr" || str_type == "weap") return 3;
  if (str_type == "dome" || str_type == "apwr") return 2;
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
    case K::kAttackTesla: return "AttackTesla";
    case K::kToggleConditionOnOrder: return "ToggleConditionOnOrder";
    case K::kHarvester: return "Harvester";
    case K::kTurreted: return "Turreted";
    case K::kAttackTurreted: return "AttackTurreted";
  }
  return "?";
}

std::int32_t face_trait_hash(const ReplayOracleWorld& w, const FaceActor& actor, FaceTraitKind kind,
                             std::int32_t int4_instance) {
  // face 指针延迟解引用：player/world actor 无 face 表项（ptr_face 为 null），仅
  // 类型面分支（kBodyOrientation/kTurreted——均 face 表 actor）触达。
  const FaceTypeSpec* ptr_face{actor.ptr_face};
  switch (kind) {
    case K::kClassicProductionQueue:
    case K::kDebugPauseState:
    case K::kShroud:
    case K::kMissionObjectives:
    case K::kDeveloperMode:
    case K::kGpsWatcher:
    case K::kConyardChronoReturn:
    case K::kDockHost:
    case K::kChronoshiftable:
      return 0;
    case K::kRepairableBuilding:
      // RepairersHash（[VerifySync]——BuildingRepair 出单后非 0；恒 0 时与旧 return 0
      // 面等价，ore-lord/skirmish 零影响）。
      return actor.int4_repairers;
    case K::kRevealsShroud:
      // CachedTraitDisabled（[VerifySync] bool，bot 剧本垫底面——dome 三实例振荡）
      return actor.b_reveals_disabled[std::size_t(std::clamp(int4_instance, 0, 2))] ? 1 : 0;
    case K::kChangesHealth:
      return actor.int4_ch_ticks[std::size_t(int4_instance)] ^
             actor.int4_ch_damage[std::size_t(int4_instance)];
    case K::kImmobile:
      return cpos_bits(actor.int4_x, actor.int4_y) ^
             wpos_hash(actor.int4_x * 1024 + 512, actor.int4_y * 1024 + 512, 0);
    case K::kBodyOrientation:
    case K::kClassicFacingBodyOrientation:
      return ptr_face->int4_facings;
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
    case K::kAttackTesla:
      return actor.int4_ts_charges ^ actor.int4_ts_recharge;
    case K::kToggleConditionOnOrder:
      return actor.b_powerdown ? 1 : 0;
    case K::kStoresResources:
      return actor.int4_content_hash;
    case K::kHarvester:
      return actor.int4_unload_ticks;
    case K::kTurreted:
      return ptr_face->int4_turret_facings;
    case K::kAttackTurreted:
      return 0;
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

namespace {

// create 公共主体（T32 参数化）：注入表由剧本给定（ore-lord 生产链剧本 / skirmish
// 互射剧本），装配流程/玩家装配/地图 actor/ore 资源场共享。
std::expected<ReplayOracleWorld, std::string> oracle_create_common(
    std::int32_t int4_seed, std::span<const std::uint8_t> span_oramap,
    std::span<const std::uint8_t> span1u_tileset_yaml,
    std::span<const InjectEntry> span_injects, const bool b_bot_script = false) {
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
  std::int32_t int4_mpspawn2_x{-1}, int4_mpspawn2_y{-1};
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
      if (entry.str_type == "mpspawn") {
        if (!b_mpspawn_seen) {
          int4_mpspawn_x = entry.int4_x;
          int4_mpspawn_y = entry.int4_y;
          b_mpspawn_seen = true;
        } else if (int4_mpspawn2_x < 0) {
          int4_mpspawn2_x = entry.int4_x;
          int4_mpspawn2_y = entry.int4_y;
        }
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

    // WithInfantryBody.Created（WithInfantryBody.cs:104）：每实例 1 次
    // SharedRandom.Next(30, 110)（idleDelay 初值，不进 ISync）。e1 规则带两个实例
    // （主体 + @PARACHUTE——ConditionalTrait 的 Created 无条件执行），故每只 e1
    // 出生消耗 2 次。D054① 装配期常量 4 的真语义（差分实验：撤注入 e1 后装配期 0 耗）。
    if (str_type == "e1") {
      (void)support::next(w.rng_shared);
      (void)support::next(w.rng_shared);
    }
    // RevealsShroud 实例的 CachedTraitDisabled 装配初值（RequiresCondition: disabled
    // 的实例装配期即禁用——dome@Offline）
    actor.b_reveals_disabled[0] = (actor.ptr_face->u1_reveals_init & 1) != 0;
    actor.b_reveals_disabled[1] = (actor.ptr_face->u1_reveals_init & 2) != 0;
    actor.b_reveals_disabled[2] = (actor.ptr_face->u1_reveals_init & 4) != 0;
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
    std::int32_t int4_power_provided{};
  };
  // bot 剧本（T34）：t34 注入集的电力/矿仓贡献——proc(-30)+weap(-30)+barr(-20)+
  // dome(-40)+tsla(-80) 耗 200（fact Amount 0 不计；PowerManager hash 172=100^200
  // bot-diag f2 定标）、powr(+100)；proc 矿仓 2000。Multi1 human 对照位（无建筑）。
  // ore-lord 面不变。
  const PlayerInit vec_player_init_orelord[]{
      PlayerInit{"Neutral", 0, 0, 0}, PlayerInit{"Creeps", 0, 0, 0},
      PlayerInit{"Multi0", 2000, 50, 0}, PlayerInit{"Everyone", 0, 0, 0}};
  const PlayerInit vec_player_init_bot[]{
      PlayerInit{"Neutral", 0, 0, 0}, PlayerInit{"Creeps", 0, 0, 0},
      PlayerInit{"Multi0", 2000, 200, 100}, PlayerInit{"Multi1", 0, 0, 0},
      PlayerInit{"Everyone", 0, 0, 0}};
  const std::span<const PlayerInit> vec_player_init{
      b_bot_script ? std::span<const PlayerInit>{vec_player_init_bot}
                   : std::span<const PlayerInit>{vec_player_init_orelord}};
  for (const PlayerInit& init : vec_player_init) {
    FacePlayer player;
    player.str_name = std::string{init.str_name};
    player.u4_actor_id = static_cast<std::uint32_t>(w.vec_actors.size());
    player.int4_capacity = init.int4_capacity;
    player.int4_power_drained = init.int4_power_drained;
    player.int4_power_provided = init.int4_power_provided;
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

  for (const InjectEntry& entry : span_injects)
    if (const auto res{fn_add_actor(entry.str_type, entry.str_owner, entry.int4_x, entry.int4_y, 0)};
        !res.has_value())
      return std::unexpected(res.error());

  if (const auto res{fn_add_actor("mcv", "Multi0", int4_mpspawn_x, int4_mpspawn_y, 512)}; !res.has_value())
    return std::unexpected(res.error());
  if (b_bot_script && int4_mpspawn2_x >= 0)
    if (const auto res{fn_add_actor("mcv", "Multi1", int4_mpspawn2_x, int4_mpspawn2_y, 512)};
        !res.has_value())
      return std::unexpected(res.error());

  // —— SeedsResource 首种资源场（T32，D054② 退役） ——
  // 上游 ResourceLayer.AllowResourceAt/CanAddResource 谓词的等价数据面：
  // 图内 + tileset 地形 ∈ {Clear, Road}（斜坡 tile 的 terrain type 为 Ramp，天然排除）
  // + 无建筑 footprint（BuildingInfo.TerrainTypes 默认空集 → 建筑格恒不允许）。
  {
    const auto vec_map_bin{filesystem::zip_entry_data(*res_zip, "map.bin")};
    if (!vec_map_bin.has_value()) return std::unexpected("orarep world: map.bin missing in oramap");
    const auto res_header{map_header_parse(*vec_map_yaml, "map.yaml")};
    if (!res_header.has_value()) return std::unexpected(res_header.error());
    const auto res_tileset{tileset_load(
        std::string_view{reinterpret_cast<const char*>(span1u_tileset_yaml.data()),
                         span1u_tileset_yaml.size()},
        "temperat.yaml")};
    if (!res_tileset.has_value()) return std::unexpected(res_tileset.error());
    const auto res_bin{map_bin_parse(*vec_map_bin, res_header->int4_size_x, res_header->int4_size_y)};
    if (!res_bin.has_value()) return std::unexpected(res_bin.error());

    w.ore.int4_width = res_bin->int4_width;
    w.ore.int4_height = res_bin->int4_height;
    w.ore.vec_b_allow.assign(static_cast<std::size_t>(res_bin->int4_width) * res_bin->int4_height,
                             false);
    for (std::int32_t int4_x{}; int4_x < res_bin->int4_width; ++int4_x)
      for (std::int32_t int4_y{}; int4_y < res_bin->int4_height; ++int4_y) {
        const std::string str_terrain{
            res_tileset->vec_str_types[res_tileset->terrain_index_of_tile(
                res_bin->tile_at(int4_x, int4_y))]};
        w.ore.vec_b_allow[static_cast<std::size_t>(int4_x) * res_bin->int4_height
                          + static_cast<std::size_t>(int4_y)] =
            str_terrain == "Clear" || str_terrain == "Road";
      }

    // 已入世界建筑的 footprint 否决（AllowResourceAt 的 BuildingInfluence 面）
    for (const FaceActor& actor : w.vec_actors) {
      if (!actor.ptr_face) continue;
      bool b_building{false};
      for (const FaceTraitKind kind : actor.ptr_face->vec_traits)
        if (kind == K::kBuilding) b_building = true;
      if (!b_building) continue;
      for (std::int32_t int4_x{actor.int4_x}; int4_x < actor.int4_x + footprint_width(actor.str_type);
           ++int4_x)
        for (std::int32_t int4_y{actor.int4_y};
             int4_y < actor.int4_y + footprint_height(actor.str_type); ++int4_y) {
          if (int4_x < 0 || int4_x >= w.ore.int4_width || int4_y < 0 || int4_y >= w.ore.int4_height)
            continue;
          w.ore.vec_b_allow[static_cast<std::size_t>(int4_x) * w.ore.int4_height
                            + static_cast<std::size_t>(int4_y)] = false;
        }
    }
  }

  return w;
}

}  // namespace

std::expected<ReplayOracleWorld, std::string> replay_oracle_create(
    std::int32_t int4_seed, std::span<const std::uint8_t> span_oramap,
    std::span<const std::uint8_t> span1u_tileset_yaml) {
  return oracle_create_common(int4_seed, span_oramap, span1u_tileset_yaml, kInjectedActors);
}

std::expected<ReplayOracleWorld, std::string> replay_oracle_create_bot(
    std::int32_t int4_seed, std::span<const std::uint8_t> span_oramap,
    std::span<const std::uint8_t> span1u_tileset_yaml) {
  // T34 bot 剧本注入（golden_gen BotGroup 的 BotActors 同表）：bot 生产基地 +
  // 低电欠额（powr 100 vs 耗 235）+ harv 采矿链 + Creeps e1 攻击源；双 mpspawn
  // 的 mcv（Multi0 bot / Multi1 human）由公共装配尾段注入。
  constexpr InjectEntry kBotInjectedActors[]{
      InjectEntry{"fact", 21, 6, "Multi0"},  InjectEntry{"proc", 33, 5, "Multi0"},
      InjectEntry{"barr", 21, 9, "Multi0"},  InjectEntry{"weap", 24, 9, "Multi0"},
      InjectEntry{"powr", 19, 6, "Multi0"},  InjectEntry{"dome", 19, 8, "Multi0"},
      InjectEntry{"tsla", 27, 8, "Multi0"},  InjectEntry{"harv", 30, 7, "Multi0"},
      InjectEntry{"harv", 31, 9, "Multi0"},  InjectEntry{"e1", 28, 11, "Creeps"},
      InjectEntry{"e1", 29, 12, "Creeps"},
  };
  auto res{oracle_create_common(int4_seed, span_oramap, span1u_tileset_yaml,
                                kBotInjectedActors, true)};
  if (!res.has_value()) return res;
  // tsla 的 AttackTesla 初值（AttackTesla.cs:55：charges=MaxCharges、recharge 0）。
  for (FaceActor& actor : res->vec_actors)
    if (actor.str_type == "tsla") {
      actor.int4_ts_charges = 3;
      actor.int4_ts_recharge = 0;
    }
  return res;
}

std::expected<ReplayOracleWorld, std::string> replay_oracle_create_skirmish(
    std::int32_t int4_seed, std::span<const std::uint8_t> span_oramap,
    std::span<const std::uint8_t> span1u_tileset_yaml) {
  // T32 互射剧本注入（golden_gen SkirmishGroup 同表）：2v2 敌对 e1 + 远角 idle 单位；
  // mpspawn 的 mcv 由公共装配尾段注入（与 ore-lord 同源）。
  constexpr InjectEntry kSkirmishActors[]{
      InjectEntry{"e1", 25, 10, "Multi0"}, InjectEntry{"e1", 24, 10, "Multi0"},
      InjectEntry{"e1", 28, 12, "Creeps"}, InjectEntry{"e1", 28, 13, "Creeps"},
      InjectEntry{"e1", 50, 48, "Multi0"},
  };
  auto res{oracle_create_common(int4_seed, span_oramap, span1u_tileset_yaml, kSkirmishActors)};
  if (!res.has_value()) return res;
  // 玩家资源/电力初值面（T32 差分定位）：ore-lord 硬编码的 Multi0 capacity 2000
  // （proc 矿仓）与 power_drained 50（fact/proc/barr 建筑耗电）是其注入建筑的贡献，
  // skirmish 无建筑——上游 PlayerResources.Capacity 与 PowerManager.DrainedLoad 均 0
  // （黄金 f1 无 PlayerResources/PowerManager 行 = 初值恒定，sync f1 锚定）。
  res->vec_players[2].int4_capacity = 0;
  res->vec_players[2].int4_power_drained = 0;
  return res;
}

namespace {

// 上游 SeedsResource.Seed（SeedsResource.cs:44-53）+ Util.RandomWalk（Util.cs:213-226）
// + ResourceLayer.CanAddResource/AddResource 的等价：每步 dx/dy 各一次 SharedRandom
// 抽样（(0,0) 重抽不计步）；SkipWhile 停在首个非（Ore 且 !CanAdd）格；Take(100)
// 上限；候选格 CanAdd 才 AddResource（density+1，clamp 至 MaxDensity=12）。
void seeds_resource_seed_one(ReplayOracleWorld& w, const FaceActor& mine) {
  std::int32_t int4_x{mine.int4_x}, int4_y{mine.int4_y};
  const auto fn_in_map{[&](std::int32_t int4_cx, std::int32_t int4_cy) {
    return int4_cx >= 0 && int4_cx < w.ore.int4_width && int4_cy >= 0 && int4_cy < w.ore.int4_height;
  }};
  const auto fn_can_add{[&](std::int32_t int4_cx, std::int32_t int4_cy) -> bool {
    if (!fn_in_map(int4_cx, int4_cy)) return false;
    const auto it_hit{w.ore.map_density.find({int4_cx, int4_cy})};
    if (it_hit == w.ore.map_density.end())
      return static_cast<bool>(
          w.ore.vec_b_allow[static_cast<std::size_t>(int4_cx) * w.ore.int4_height
                            + static_cast<std::size_t>(int4_cy)]);
    return it_hit->second + 1 <= 12;
  }};

  bool b_added{false};
  for (std::int32_t int4_steps{}; int4_steps < 100 && !b_added;) {
    const std::int32_t int4_dx{support::next(w.rng_shared, -1, 2)};
    const std::int32_t int4_dy{support::next(w.rng_shared, -1, 2)};
    if (int4_dx == 0 && int4_dy == 0) continue;
    ++int4_steps;
    int4_x += int4_dx;
    int4_y += int4_dy;

    // SkipWhile 谓词：Ore 且 !CanAdd（满密度）→ 跳过继续走
    const auto it_hit{w.ore.map_density.find({int4_x, int4_y})};
    const bool b_is_ore{it_hit != w.ore.map_density.end()};
    if (b_is_ore && it_hit->second + 1 > 12) continue;

    // 首个候选：CanAdd 才种（density+1）
    if (fn_can_add(int4_x, int4_y)) {
      if (it_hit == w.ore.map_density.end())
        w.ore.map_density.emplace(std::pair{int4_x, int4_y}, 1);
      else
        ++it_hit->second;
    }
    b_added = true;
  }
}

// SeedsResource.ITick（SeedsResource.cs:38-45）：--ticks<=0 → Seed + ticks=Interval(75)。
// 初始 ticks=0 → 首个 tick 即首种。ore-lord 剧本仅 f1 调用一次（首种语义化）；周期种
// （f76+）依赖 harv 采掘后的资源场动态（D055 数据面未移植），由帧末 rng 账本补齐
// 承载（D054② 部分退役口径）。skirmish 剧本无 harv，资源场演化封闭，每帧驱动。
void seeds_resource_tick(ReplayOracleWorld& w) {
  for (FaceActor& mine : w.vec_actors) {
    if (!mine.b_in_world || mine.ptr_face == nullptr || mine.str_type != "mine") continue;
    if (--mine.int4_seed_ticks <= 0) {
      seeds_resource_seed_one(w, mine);
      mine.int4_seed_ticks = 75;
    }
  }
}

}  // namespace

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

namespace {

// WithInfantryBody idle 面（WithInfantryBody.cs TickIdle/ITick 的 rng 等价，T32 探针
// 全量锚定）：状态机 0=Waiting/1=Idle/2=Moving(瞬态)/3=Attacking/4=IdleAnimating。
// TickIdle（段1）：prone（TakeCover.IsModifyingSequence）期 AllowIdleAnimation 直接
// return（不抽不递减）；Waiting→Idle 抽 2 次（IdleSequences.Random + Next(30,110)=
// idleDelay——差分实验 C：清 IdleSequences 后 f1/f3 的每 e1 2 耗消失）；Idle 递减
// 到 0 → IdleAnimating。攻击/移动（!IsIdle）期间 ITick 版 PlayStandAnimation 把
// Idle/IdleAnimating 打回 Waiting（wib_frame_end 的活动打回）；无头宿主
// Animation.Tick 正常推进（skirmish 探针推翻「Attacking 卡住」旧口径）——shoot
// （8 帧 Tick 40）与 idle（16 帧 Tick 120=48 世界帧）播完 after → PlayStand →
// Waiting，次帧 TickIdle 再抽。@PARACHUTE 第二实例 IdleSequences 空/shoot 序列缺，
// 恒 Waiting 不抽——rng 面与单实例等价。
void with_infantry_body_idle(ReplayOracleWorld& w, FaceActor& actor,
                             std::int32_t int4_frame) {
  // prone gate：TakeCover.IsModifyingSequence → AllowIdleAnimation false。
  if (actor.int4_take_cover > 0) return;
  if (actor.int4_wib_state == 0) {
    (void)support::next(w.rng_shared);
    actor.int4_wib_state = 1;
    actor.int4_wib_delay = 30 + support::next(w.rng_shared) % 80;
  } else if (actor.int4_wib_state == 1 && actor.int4_wib_delay > 0) {
    // 上游 --idleDelay==0 即转 IdleAnimating（PlayThen frame=0，Tick 120：首推进
    // +3 帧、16 次推进后 after——after=转入帧+47（段2），探针定标）。
    if (--actor.int4_wib_delay == 0) {
      actor.int4_wib_state = 4;
      actor.int4_wib_timer = int4_frame + 47;
    }
  }
}

// AutoTarget idle 面（AutoTarget.cs ScanForTarget:203-208）：nextScanTime<=0 时
// SharedRandom.Next(3,8)（=3+out%5）+ ChooseTarget（FindActorsInCircle+可见性），
// 发现敌对目标即进入攻击活动。f1 时 vec_sight 尚未建立（Shroud 首帧未就绪）——
// 上游 f1 的扫描恒扑空（probe dump：f1 末 activity=null，f6/f7 起才发现）。
void auto_target_idle(ReplayOracleWorld& w, FaceActor& actor) {
  if (actor.int4_next_scan <= 0) {
    const std::int32_t int4_out{support::next(w.rng_shared)};
    actor.int4_next_scan = 3 + int4_out % 5;
    const std::int32_t int4_target{autotarget_scan_target(w, actor)};
    if (int4_target >= 0) {
      actor.b_activity = true;
      actor.int4_activity_target = int4_target;
    }
  }
}

// —— e1 战斗面共享段（ore-lord 混合驱动与 skirmish 机制面全语义共用，T32） ——

struct FireDamage {
  std::uint32_t u4_victim{};
  std::uint32_t u4_attacker{};
  std::int32_t int4_damage{};
  // 该发的 Weapon.Fire 消耗序（4 次，ore-lord 黄金锚定的总账）——消耗时点在
  // 本帧帧尾的命中链（伤害+INotifyDamage.Damaged 反击扫）之后（skirmish 黄金
  // f5 锚定：反击扫抽值先于 fire 耗）。
  std::int32_t int4_pending_cost{};
};

// 攻击活动 tick（AttackActivity/AttackFrontal 链）：追击朝向（FacingTolerance 0 的
// 转向在 tick_facing 承载）+ 开火（每发 4 次 SharedRandom——Weapon.Fire 消耗序，
// ore-lord 黄金锚定）+ ReloadDelay 20 + 延迟特效（+2/+4 帧各 4 耗）。
// 伤害 = M1Carbine 基伤 1500（prone 50% 减伤 750，TakeCover.DamageModifiers）×
// FirepowerMultiplier（^GainsExperience: rank1-3/elite=105/110/120/130%）×
// DamageMultiplier（受方 95/90/85/75%）——等级面数值实现（ore-lord/skirmish 均
// level 0 无差，等级修正黄金未触达，登记 D093）。
std::int32_t veterancy_firepower(std::int32_t int4_level) {
  switch (int4_level) {
    case 1: return 105;
    case 2: return 110;
    case 3: return 120;
    case 4: return 130;
  }
  return 100;
}

std::int32_t veterancy_damage_mod(std::int32_t int4_level) {
  switch (int4_level) {
    case 1: return 95;
    case 2: return 90;
    case 3: return 85;
    case 4: return 75;
  }
  return 100;
}

void e1_attack_tick(ReplayOracleWorld& w, FaceActor& actor, std::int32_t int4_frame,
                    std::vector<FireDamage>& vec_fires) {
  FaceActor* ptr_target{nullptr};
  for (FaceActor& other : w.vec_actors)
    if (other.b_in_world && static_cast<std::int32_t>(other.u4_id) == actor.int4_activity_target)
      ptr_target = &other;
  if (ptr_target == nullptr) {
    actor.b_activity = false;
    actor.b_aiming = false;
    return;
  }

  const pathfinding::WVec vec_delta{pathfinding::WVec{
      ptr_target->int4_px - actor.int4_px, ptr_target->int4_py - actor.int4_py, 0}};
  const std::int32_t int4_desired{pathfinding::wvec_yaw(vec_delta)};
  // ra 步兵 TurnSpeed 一步到位（skirmish 黄金 Mobile Facing 0→692/672/160/128 单帧）；
  // ore-lord 中 facing 由数据面先行写位，此步长为 no-op（T32 语义化）。
  actor.int4_facing = pathfinding::tick_facing(actor.int4_facing, int4_desired, 1024);
  // Attack.Tick 的 useLastVisibleTarget 面（Attack.cs:150-161，T32）：目标经
  // Target.Recalculate 对攻击者不可见（shroud 收缩）且在射程内（lastVisibleTarget
  // IsInRange → return true）即活动 Done——skirmish 黄金：145 f46 死、Creeps 视野
  // f47 段2 收缩、146 的活动 f48 断（IsAiming False+次帧重扫）。不可见且超程的
  // 上游分支是 QueueChild(move) 追击——两剧本单位静止不触达（D093 登记项）。
  if (!actor_visible_to(w, actor.u4_owner, *ptr_target)) {
    actor.b_activity = false;
    actor.b_aiming = false;
    return;
  }
  actor.b_aiming = true;
  if (actor.int4_reload <= 0) {
    actor.int4_reload = 20;
    // INotifyAttack.PreparingAttack → 帧尾 FrameEndTask → Attacking（PlayThen
    // shoot：8 帧 Tick 40，after=fire+8 段2 转 Waiting；受击 dirty 打回会中断——
    // skirmish f7 a143 探针：Attacking frame=1 即被 prone 沿打回）。
    actor.int4_wib_state = 3;
    actor.int4_wib_timer = int4_frame + 8;
    w.vec_delayed_effect.push_back({int4_frame + 2, 4});
    w.vec_delayed_effect.push_back({int4_frame + 4, 4});
    const std::int32_t int4_base{ptr_target->int4_take_cover > 0 ? 750 : 1500};
    const std::int64_t in8_damage{
        static_cast<std::int64_t>(int4_base) * veterancy_firepower(actor.int4_level) / 100
        * veterancy_damage_mod(ptr_target->int4_level) / 100};
    vec_fires.push_back({ptr_target->u4_id, actor.u4_id,
                         static_cast<std::int32_t>(in8_damage), 4});
  }
}

// 帧尾结算：TakeCover prone（受击 remainingDuration=50，帧尾伤害段）+ 击杀移除 +
// GivesExperience 经验账本（同 ore-lord 段，含 PlayerExperience）。
void e1_damage_settle(ReplayOracleWorld& w, const std::vector<FireDamage>& vec_fires,
                      std::int32_t int4_frame) {
  (void)int4_frame;
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
      // shroud 收缩入队（T32）：次帧段2 从 owner/Everyone 的 sight 移除 victim 圈
      // （上游 Shroud 滞后一拍更新；ore-lord 不消费此队列）。
      w.vec_sight_shrink_pending.push_back(victim.u4_id);
      // （kill+3/kill+8 各 2 耗的旧「死亡动画面占位」已删——探针正名：那是击杀者
      // 们 Attacking 播完 after → Waiting → 断活动后首个 TickIdle 的 WIB 抽样
      //（skirmish f49 a146=fire@29+8 的 after 链、f54 a144=fire@45+8），由
      // wib 状态机自然产生，D093 注记随改。）

      // GivesExperience.Killed（GivesExperience.cs:60-79，T32 机制面接管）：victim 的
      // 经验值 = Valued.Cost（e1=100）；killer.GiveExperience(cost × ActorExperienceModifier
      // 10000% = 10000)；killer owner 的 PlayerExperience += cost × PlayerExperienceModifier
      // 1% = 1（ra ^ExistsInWorld:GivesExperience{PlayerExperienceModifier:1}）。
      // 升级阈值 = ^GainsExperience Conditions 200/400/800/1600 × cost（20000/40000/
      // 80000/160000）。
      FaceActor& killer{w.vec_actors[fire.u4_attacker]};
      if (killer.b_in_world && killer.ptr_face != nullptr && killer.str_type == "e1") {
        constexpr std::int32_t int4_exp_cost{100};
        killer.int4_experience =
            std::min(killer.int4_experience + int4_exp_cost * 10000 / 100, 160000);
        while (killer.int4_level < 4
               && killer.int4_experience >= int4_exp_cost * 200 << killer.int4_level)
          ++killer.int4_level;
        if (killer.u4_owner < w.vec_players.size())
          w.vec_players[killer.u4_owner].int4_pexp += int4_exp_cost * 1 / 100;
      }
    } else {
      victim.int4_take_cover = 50;
      // AutoTarget.Damaged 反击面（AutoTarget.cs Damged 段，T32 语义化——skirmish
      // 黄金 f5 锚定）：受击结算在 World.Tick 的 effects 段（②ITck 之后），受击者
      // idle（无活动/无移动）且 Stance>=ReturnFire（Defend/AttackAnything 均满足）
      // 即反击。AttackAnything/Defend > ReturnFire 走 ScanForTarget（ignore=false：
      // nextScanTime<=0 时抽 Next(3,8) 并写——条件用 ② 后值），选中目标否则回退
      // 原攻击者，随后无条件建立攻击活动（次帧 ① 首跑 facing+fire）。
      if (!victim.b_activity && !victim.b_moving && victim.int4_next_scan <= 0) {
        const std::int32_t int4_out{support::next(w.rng_shared)};
        victim.int4_next_scan = 3 + int4_out % 5;
        const std::int32_t int4_target{autotarget_scan_target(w, victim)};
        victim.b_activity = true;
        victim.int4_activity_target = int4_target >= 0
                                          ? int4_target
                                          : static_cast<std::int32_t>(fire.u4_attacker);
        // AutoTarget.Aggressor（[VerifySync] Actor 字段——不进 isync dump 值域但进
        // SyncHash）：Damaged 链置扫中者（TickIdle 扫攻击不置——ore-lord 场景恒
        // null 的原因）。
        victim.int4_aggressor = victim.int4_activity_target;
      }
    }

    // 该发的 Weapon.Fire 消耗序（帧尾命中链后，skirmish 黄金 f5 rng 序锚定）。
    for (std::int32_t idx{}; idx < fire.int4_pending_cost; ++idx)
      (void)support::next(w.rng_shared);
  }
}

// 段2 帧尾（ITick 递减面 + WithInfantryBody Moving 瞬态转 Waiting）。
// WithInfantryBody 段2 面（wib_frame_end 先行）：TakeCover 递减在本函数，故 prone
// 快照沿在递减前判（上游主体 WIB.Tick 先于 TakeCover.Tick）。
void e1_frame_end(FaceActor& actor) {
  if (actor.str_type != "e1") return;
  if (actor.int4_next_scan > 0) --actor.int4_next_scan;
  if (actor.int4_take_cover > 0) --actor.int4_take_cover;
  if (actor.int4_reload > 0) --actor.int4_reload;
  if (actor.int4_wib_state == 2) actor.int4_wib_state = 0;
}

// WithInfantryBody.ITick 段2 等价（T32 语义化，探针锚定）。三个打回面：
// ①dirty：TakeCover IsModifyingSequence 沿（受击转 prone / prone 到期）→ 无条件
//   PlayStand → Waiting（含 Attacking 中断——skirmish f7 a143 from=Attacking、
//   f97 a143 from=Idle 双抽实证）；
// ②after：Attacking（fire+8）/IdleAnimating（转入+47）播完 → PlayStand → Waiting；
// ③活动/移动（!IsIdle）：Idle/IdleAnimating → Waiting（skirmish f4 a144/f5 a143
//   扫描建活动同帧打回实证）。
// 判定用递减前的 take_cover（上游 TakeCover.Tick 在主体 WIB.Tick 之后）。
void wib_frame_end(FaceActor& actor, std::int32_t int4_frame) {
  if (actor.str_type != "e1" || actor.b_dataplane) return;
  const bool b_prone_now{actor.int4_take_cover > 0};
  if (actor.b_wib_prone != b_prone_now)
    actor.int4_wib_state = 0;  // ①dirty（PlayStand 无条件）
  if ((actor.int4_wib_state == 3 || actor.int4_wib_state == 4)
      && int4_frame >= actor.int4_wib_timer)
    actor.int4_wib_state = 0;  // ②after
  if ((actor.int4_wib_state == 1 || actor.int4_wib_state == 4)
      && (actor.b_activity || actor.b_moving))
    actor.int4_wib_state = 0;  // ③!IsIdle 打回
  actor.b_wib_prone = b_prone_now;
}

// delayed 特效消耗段（warhead 效果创建在命中 +2/+4 帧，各 4 次 SharedRandom——
// SpriteEffect 家族的消耗序占位，结果弃用；D036③/U051 口径）。
void delayed_effect_tick(ReplayOracleWorld& w, std::int32_t int4_frame) {
  for (std::size_t idx{}; idx < w.vec_delayed_effect.size();) {
    if (w.vec_delayed_effect[idx].first <= int4_frame) {
      for (std::int32_t idx2{}; idx2 < w.vec_delayed_effect[idx].second; ++idx2)
        (void)support::next(w.rng_shared);
      w.vec_delayed_effect.erase(w.vec_delayed_effect.begin() + static_cast<std::ptrdiff_t>(idx));
    } else {
      ++idx;
    }
  }
}

// shroud 收缩消费（T32，skirmish 段2 头部=上游 world actor 的 Shroud ITick 位）：
// 从 victim owner 与 Everyone 的 sight 集减其出生格 RevealsShroud 圈（e1@4c）。
// 集合差自动保留与其它单位圈重叠的格（上游可见集=存活单位圈的并）。
void sight_shrink_pending_consume(ReplayOracleWorld& w) {
  for (const std::uint32_t u4_victim : w.vec_sight_shrink_pending) {
    if (u4_victim >= w.vec_actors.size()) continue;
    const FaceActor& victim{w.vec_actors[u4_victim]};
    if (victim.u4_owner >= w.vec_players.size()) continue;
    std::set<std::pair<std::int32_t, std::int32_t>> set_circle;
    add_actor_sight(set_circle, victim, {4});
    for (const auto& cell : set_circle) {
      w.vec_sight[victim.u4_owner].erase(cell);
      w.vec_sight[3].erase(cell);
    }
  }
  w.vec_sight_shrink_pending.clear();
}

// f1 冻结层重建（T32 提取共享：ore-lord/skirmish 剧本同构）：kFlagFrozen actor 对
// 各玩家的可见位（Everyone 对 Multi0 自有 actor 恒可见；Creeps 玩家无冻结视野）+
// FrozenActorLayer 的 VisibilityHash/FrozenHash 聚合。
void frozen_layer_rebuild(ReplayOracleWorld& w) {
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
}

}  // namespace

void replay_oracle_frame1(ReplayOracleWorld& w) {

  // f1 段1（World.Tick 段1：actor 序 TickIdle，WithInfantryBody 先于 AutoTarget——
  // 上游 trait 构造序，D054④ 首扫抽样位 146@6/147@9(0-based) 的自然来源）
  for (const std::uint32_t u4_e1 : {146u, 147u}) {
    FaceActor& e1{w.vec_actors[u4_e1]};
    with_infantry_body_idle(w, e1, 1);
    auto_target_idle(w, e1);
  }

  // f1 段2（ITick）：mine SeedsResource 首种（D054② 退役——26/28 常量的真语义；
  // 差分实验：无 mine 时 f1 少 28 耗）
  seeds_resource_tick(w);

  for (const std::uint32_t u4_e1 : {146u, 147u}) {
    FaceActor& e1{w.vec_actors[u4_e1]};
    // f1 段2：WithInfantryBody ITick 的 Moving 瞬态（f1 末 CurrentMovementTypes
    // 短暂 Horizontal，probe dump：f1 末 state=Moving）→ f2 TickIdle 不抽、
    // f2 段2 转 Waiting、f3 再抽 2（D054③"f3 每 e1 2 耗"的真语义）
    e1.int4_wib_state = 2;
    // AutoTarget.ITick：nextScanTime>0 → --
    if (e1.int4_next_scan > 0) --e1.int4_next_scan;
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

  frozen_layer_rebuild(w);

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

// —— skirmish 剧本帧驱动（T32/issue #51）：与 ore-lord frame1 同构、无 harv/无
// 建筑队列（全 false）、注入面 sight（143/144/147+mcv 为 Multi0、145/146 为 Creeps，
// 圈半径 4c——143 的圈含 145 格而 144 的圈不含：f4 144 命中/f48 后 143 只扫描不
// 攻击的黄金可见性根源）。f1 段1 扫描恒扑空（vec_sight 尚未建立，Shroud 首帧未
// 就绪），f1 末 WithInfantryBody 的 Moving 瞬态同 ore-lord。
void replay_oracle_frame1_skirmish(ReplayOracleWorld& w) {
  for (std::uint32_t u4_id{143}; u4_id <= 147; ++u4_id) {
    FaceActor& e1{w.vec_actors[u4_id]};
    with_infantry_body_idle(w, e1, 1);
    auto_target_idle(w, e1);
  }

  seeds_resource_tick(w);

  for (std::uint32_t u4_id{143}; u4_id <= 147; ++u4_id) {
    FaceActor& e1{w.vec_actors[u4_id]};
    e1.int4_wib_state = 2;
    if (e1.int4_next_scan > 0) --e1.int4_next_scan;
  }

  for (std::size_t idx_player{}; idx_player < w.vec_players.size(); ++idx_player)
    for (std::size_t idx_queue{}; idx_queue < 6; ++idx_queue)
      w.vec_players[idx_player].b_queue_enabled[idx_queue] = false;

  w.vec_sight.assign(w.vec_players.size(), {});
  add_actor_sight(w.vec_sight[0], w.vec_actors[7], {10});
  for (const std::uint32_t u4_id : {145u, 146u})
    add_actor_sight(w.vec_sight[1], w.vec_actors[u4_id], {4});
  for (const std::uint32_t u4_id : {143u, 144u, 147u, 148u})
    add_actor_sight(w.vec_sight[2], w.vec_actors[u4_id], {4});
  w.vec_sight[3] = w.vec_sight[2];

  frozen_layer_rebuild(w);
}

// skirmish 帧序（上游 World.Tick 逐 actor 的 activities→ITick 交错在 rng 消耗序
// 上与两段全局等价：ITick 面零 rng 消耗，activities/TickIdle 面按 ActorID 序）：
// 段1=活动（e1_attack_tick：一步 facing+fire 4 耗+ReloadDelay 20+延迟特效）或
// TickIdle（WithInfantryBody idle 状态机+AutoTarget 扫描 Next(3,8)）→ 段2=ITick
// （Shroud 收缩消费→mine SeedsResource 周期种 75→e1 递减面）→ 帧尾=伤害结算
// （prone 750/站姿 1500、击杀经验 10000/PlayerExperience 1、TakeCover
// remainingDuration=50）+ delayed 特效消耗。无 rng 账本补齐——消耗序错则 rng
// total/sync 即分叉。
void replay_oracle_frame_skirmish(ReplayOracleWorld& w, std::int32_t int4_frame) {
  std::vector<FireDamage> vec_fires;

  for (FaceActor& actor : w.vec_actors) {
    if (!actor.b_in_world || actor.ptr_face == nullptr) continue;
    if (actor.str_type != "e1") continue;
    if (actor.b_activity) {
      e1_attack_tick(w, actor, int4_frame, vec_fires);
    } else {
      with_infantry_body_idle(w, actor, int4_frame);
      auto_target_idle(w, actor);
    }
  }

  sight_shrink_pending_consume(w);
  seeds_resource_tick(w);

  for (FaceActor& actor : w.vec_actors) {
    if (!actor.b_in_world || actor.ptr_face == nullptr) continue;
    if (actor.str_type != "e1") continue;
    wib_frame_end(actor, int4_frame);
    e1_frame_end(actor);
  }

  e1_damage_settle(w, vec_fires, int4_frame);
  delayed_effect_tick(w, int4_frame);
}

void replay_oracle_apply_changes(ReplayOracleWorld& w, const OracleReplayContext& ctx,
                                 std::int32_t int4_frame, bool b_mobile_only) {
  if (int4_frame < 15) return;
  const auto it_changes{ctx.map_changes.find(int4_frame)};
  if (it_changes == ctx.map_changes.end()) return;
  if (!w.str_fatal.empty()) return;
  std::size_t sz_applied{};
  std::size_t sz_skipped{};
  const auto fn_fatal{[&](const OracleChange& change, const std::string& str_what) {
    w.str_fatal = std::format("isync-changes f{} actor{} {} {}: {}", int4_frame,
                              change.u4_actor, change.str_trait, change.str_field, str_what);
  }};
  const auto fn_parse_i32{[&](std::string_view str_text, std::int32_t& int4_out) {
    const auto res{std::from_chars(str_text.data(), str_text.data() + str_text.size(), int4_out)};
    return res.ec == std::errc{} && res.ptr == str_text.data() + str_text.size();
  }};

  for (const OracleChange& change : it_changes->second) {
    const bool b_is_mobile{change.str_trait == "Mobile"};
    if (b_mobile_only != b_is_mobile) {
      ++sz_skipped;
      continue;
    }
    if (change.u4_actor >= w.vec_actors.size()) {
      fn_fatal(change, std::format("actor 越界 (vec_actors={})", w.vec_actors.size()));
      return;
    }
    FaceActor& actor{w.vec_actors[change.u4_actor]};
    if (actor.u4_owner >= w.vec_players.size()) {
      fn_fatal(change, std::format("owner {} 越界 (vec_players={})", actor.u4_owner,
                                   w.vec_players.size()));
      return;
    }

    // 机制面接管字段显式跳过表（跳过 + 对账计入，禁止静默吞行）：
    // ClassicProductionQueue/PlayerResources=生产真机制、Health/AttackFrontal/
    // ChangesHealth=攻击链真机制、FrozenUnderFog=冻结层真机制。AutoTarget/TakeCover/
    // GainsExperience/PlayerExperience 已随 T32 撤列（黄金生成侧 IsyncMechanized 排除，
    // 机制面全接管：扫描序/prone/击杀经验账本）。
    // 机制面真机制跳过（ore-lord/skirmish 的 e1 互射面——HP/aiming/damageTicks 由
    // e1_attack_tick 链承载）；bot 剧本的非 e1 actor（tsla/harv/e3 等数据垫底模式）
    // 的同名字段不跳过——走下方的数据应用分支。
    const bool b_is_e1{actor.str_type == "e1"};
    const bool b_skip{(change.str_trait == "PlayerResources" && change.str_field == "Cash")
                      || (change.str_trait == "FrozenUnderFog" && change.str_field == "VisibilityHash")
                      || (change.str_trait == "ClassicProductionQueue" && change.str_field == "Enabled")
                      || (change.str_trait == "Health" && change.str_field == "HP" && b_is_e1)
                      || (change.str_trait == "AttackFrontal" && change.str_field == "IsAiming" && b_is_e1)
                      || (change.str_trait == "ChangesHealth" && change.str_field == "damageTicks" && b_is_e1)
                      // hash 恒 0 的 ISync（face_trait_hash 返回 0 面）：数据行对拍账
                      // 计（黄金生成侧 IsyncMechanized 未排除者）但不应用。
                      || (change.str_trait == "RepairableBuilding")
                      || (change.str_trait == "RevealsShroud")};
    if (b_skip) {
      ++sz_skipped;
      continue;
    }

    // 当前值镜像（与 golden 同字段语义），用于 old 校验与新值应用
    std::int32_t* ptr_current{nullptr};
    std::int32_t int4_cur_x{}, int4_cur_y{}, int4_cur_z{};
    bool b_tuple{};
    if (change.str_trait == "Mobile") {
      if (change.str_field == "Facing") {
        ptr_current = &actor.int4_facing;
      } else if (change.str_field == "FromCell" || change.str_field == "ToCell") {
        b_tuple = true;
        int4_cur_x = change.str_field == "FromCell" ? actor.int4_from_x : actor.int4_to_x;
        int4_cur_y = change.str_field == "FromCell" ? actor.int4_from_y : actor.int4_to_y;
      } else if (change.str_field == "CenterPosition") {
        b_tuple = true;
        int4_cur_x = actor.int4_px;
        int4_cur_y = actor.int4_py;
        int4_cur_z = actor.int4_pz;
      }
    } else if (change.str_trait == "StoresResources" && change.str_field == "ContentHash") {
      ptr_current = &actor.int4_content_hash;
    } else if (change.str_trait == "AutoTarget" && change.str_field == "nextScanTime") {
      ptr_current = &actor.int4_next_scan;
    } else if (change.str_trait == "AttackTesla" && change.str_field == "charges") {
      ptr_current = &actor.int4_ts_charges;
    } else if (change.str_trait == "AttackTesla" && change.str_field == "timeToRecharge") {
      ptr_current = &actor.int4_ts_recharge;
    } else if (change.str_trait == "Health" && change.str_field == "HP") {
      // bot 剧本非 e1 面（tsla 受击/维修回血、harv 被击）：数据垫底。
      ptr_current = &actor.int4_hp;
    } else if (change.str_trait == "AttackFrontal" && change.str_field == "IsAiming") {
      actor.b_aiming = change.str_new == "True";
      ++sz_applied;
      continue;
    } else if (change.str_trait == "ChangesHealth" && change.str_field == "damageTicks") {
      ptr_current = &actor.int4_ch_ticks[0];
    } else if (change.str_trait == "PlayerResources" && change.str_field == "Ore") {
      // player actor 的 Ore（bot 剧本采矿收入/生产扣款面，owner=actor 所有者）。
      if (actor.u4_owner < w.vec_players.size())
        ptr_current = &w.vec_players[actor.u4_owner].int4_resources;
    } else if (change.str_trait == "ToggleConditionOnOrder" && change.str_field == "enabled") {
      actor.b_powerdown = change.str_new == "True";
      ++sz_applied;
      continue;
    } else if (change.str_trait == "PowerManager" && change.str_field == "PowerProvided") {
      ptr_current = &w.vec_players[actor.u4_owner].int4_power_provided;
    } else if (change.str_trait == "FrozenActorLayer" && change.str_field == "VisibilityHash") {
      ptr_current = &w.vec_players[actor.u4_owner].int4_frozen_vis;
    } else if (change.str_trait == "FrozenActorLayer" && change.str_field == "FrozenHash") {
      ptr_current = &w.vec_players[actor.u4_owner].int4_frozen_sum;
    }
    if (ptr_current == nullptr && !b_tuple) {
      fn_fatal(change, "未知 (trait,field) 组合");
      return;
    }

    if (b_tuple) {
      const auto fn_parse_tuple{[&](std::string_view str_text, std::int32_t& int4_a,
                                    std::int32_t& int4_b, std::int32_t* ptr_c) {
        const std::size_t idx_comma{str_text.find(',')};
        if (idx_comma == std::string_view::npos) return false;
        const std::size_t idx_comma2{str_text.find(',', idx_comma + 1)};
        if (ptr_c != nullptr && idx_comma2 == std::string_view::npos) return false;
        if (!fn_parse_i32(str_text.substr(0, idx_comma), int4_a)) return false;
        const std::size_t idx_tail{ptr_c != nullptr ? idx_comma2 + 1 : idx_comma + 1};
        if (!fn_parse_i32(str_text.substr(idx_comma + 1,
                                          (ptr_c != nullptr ? idx_comma2 : str_text.size())
                                              - idx_comma - 1),
                          int4_b))
          return false;
        if (ptr_c != nullptr
            && !fn_parse_i32(str_text.substr(idx_tail, str_text.size() - idx_tail), *ptr_c))
          return false;
        return true;
      }};
      if (change.str_field == "CenterPosition") {
        std::int32_t int4_old_x{}, int4_old_y{}, int4_old_z{};
        if (!fn_parse_tuple(change.str_old, int4_old_x, int4_old_y, &int4_old_z)
            || int4_old_x != int4_cur_x || int4_old_y != int4_cur_y
            || int4_old_z != int4_cur_z) {
          fn_fatal(change, std::format("old 校验失败: golden old={} 当前={},{},{}",
                                       change.str_old, int4_cur_x, int4_cur_y, int4_cur_z));
          return;
        }
        if (!fn_parse_tuple(change.str_new, actor.int4_px, actor.int4_py, &actor.int4_pz)) {
          fn_fatal(change, std::format("new 值解析失败: {}", change.str_new));
          return;
        }
      } else {
        std::int32_t int4_old_x{}, int4_old_y{};
        if (!fn_parse_tuple(change.str_old, int4_old_x, int4_old_y, nullptr)
            || int4_old_x != int4_cur_x || int4_old_y != int4_cur_y) {
          fn_fatal(change, std::format("old 校验失败: golden old={} 当前={},{}", change.str_old,
                                       int4_cur_x, int4_cur_y));
          return;
        }
        std::int32_t& ref_x{change.str_field == "FromCell" ? actor.int4_from_x : actor.int4_to_x};
        std::int32_t& ref_y{change.str_field == "FromCell" ? actor.int4_from_y : actor.int4_to_y};
        if (!fn_parse_tuple(change.str_new, ref_x, ref_y, nullptr)) {
          fn_fatal(change, std::format("new 值解析失败: {}", change.str_new));
          return;
        }
      }
      ++sz_applied;
      continue;
    }

    std::int32_t int4_old{};
    if (!fn_parse_i32(change.str_old, int4_old) || int4_old != *ptr_current) {
      fn_fatal(change, std::format("old 校验失败: golden old={} 当前={}", change.str_old,
                                   *ptr_current));
      return;
    }
    std::int32_t int4_value{};
    if (!fn_parse_i32(change.str_new, int4_value)) {
      fn_fatal(change, std::format("new 值解析失败: {}", change.str_new));
      return;
    }
    *ptr_current = int4_value;
    ++sz_applied;
  }
  if (sz_applied + sz_skipped != it_changes->second.size()) {
    w.str_fatal = std::format("isync-changes f{}: 对账失败 applied={} skipped={} total={}",
                              int4_frame, sz_applied, sz_skipped, it_changes->second.size());
    return;
  }
}

void replay_oracle_spawn_e1(ReplayOracleWorld& w) {
  FaceActor actor;
  actor.u4_id = static_cast<std::uint32_t>(w.vec_actors.size());
  actor.str_type = "e1";
  actor.ptr_face = face_type_find("e1");
  actor.u4_owner = 2;
  // 生产出生的 e1（id≥150）在 ore-lord 剧本由 isync 黄金垫底（D055 数据面：
  // AutoTarget/TakeCover/移动面），不走机制面（b_dataplane 门控）。
  actor.b_dataplane = true;

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

    // effect 行（bot 剧本，T34 打回③）：`e {f} effect {Type} h {hash}`——SendSync 时刻
    // 的 SyncedEffects 全量投影（帧内 List 枚举序）。
    if (tok_actor == "effect") {
      std::string_view tok_etype, tok_h, tok_hash;
      fn_token(tok_etype);
      fn_token(tok_h);
      fn_token(tok_hash);
      std::int32_t int4_eff_frame{};
      if (tok_f != "e" || tok_h != "h"
          || std::from_chars(tok_frame.data(), tok_frame.data() + tok_frame.size(),
                             int4_eff_frame)
                 .ec != std::errc{})
        return std::unexpected("oracle changes: bad effect line: " + std::string{str_line});
      OracleEffect effect;
      effect.str_type = std::string{tok_etype};
      if (std::from_chars(tok_hash.data(), tok_hash.data() + tok_hash.size(),
                          effect.int4_hash)
              .ec != std::errc{})
        return std::unexpected("oracle changes: bad effect hash: " + std::string{str_line});
      ctx.map_effects[int4_eff_frame].push_back(std::move(effect));
      continue;
    }

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
    if ((tok_f != "f" && tok_f != "o" && tok_f != "t") || tok_actor != "actor"
        || tok_trait != "trait" || tok_field != "field" || tok_arrow != "->")
      return std::unexpected("oracle changes: bad line: " + std::string{str_line});
    OracleChange change;
    if (std::from_chars(tok_id.data(), tok_id.data() + tok_id.size(), change.u4_actor).ec
        != std::errc{})
      return std::unexpected("oracle changes: bad actor id: " + std::string{str_line});
    if (const std::size_t idx_hash{tok_trait_name.find('#')};
        idx_hash != std::string_view::npos) {
      change.str_trait = std::string{tok_trait_name.substr(0, idx_hash)};
      if (std::from_chars(tok_trait_name.data() + idx_hash + 1,
                          tok_trait_name.data() + tok_trait_name.size(),
                          change.int4_instance)
              .ec != std::errc{})
        return std::unexpected("oracle changes: bad trait instance: "
                               + std::string{str_line});
    } else {
      change.str_trait = std::string{tok_trait_name};
    }
    change.str_field = std::string{tok_field_name};
    change.str_old = std::string{tok_old};
    change.str_new = std::string{tok_new};
    std::int32_t int4_frame{};
    if (std::from_chars(tok_frame.data(), tok_frame.data() + tok_frame.size(), int4_frame).ec
        != std::errc{})
      return std::unexpected("oracle changes: bad frame: " + std::string{str_line});

    if (change.u4_actor == 146 && change.str_trait == "Mobile")
      ctx.int4_146_move_end = std::max(ctx.int4_146_move_end, int4_frame);
    // 行前缀路由（T34 打回③）：f=ore-lord/skirmish 全量合并行；o/t=bot 产物按
    // SendSync 拍拆段（orders 面/tick 面）。
    (tok_f == "o" ? ctx.map_changes_orders
                  : tok_f == "t" ? ctx.map_changes_tick : ctx.map_changes)[int4_frame]
        .push_back(std::move(change));
  }

  for (std::size_t idx_line{}; idx_line < str_frames_text.size();) {
    const std::size_t idx_eol{str_frames_text.find('\n', idx_line)};
    const std::string_view str_line{
        str_frames_text.substr(idx_line, (idx_eol == std::string_view::npos
                                              ? str_frames_text.size()
                                              : idx_eol) -
                                         idx_line)};
    idx_line = idx_eol == std::string_view::npos ? str_frames_text.size() : idx_eol + 1;

    // 出生/死亡行（bot 剧本驱动，T34 打回）：`actor+ f{n} {id}:{type}` / `actor- f{n}
    // {id}:{type}`——SyncHash 段1（在世集合）与 fnv 的结构驱动源。
    if (str_line.starts_with("actor+ ") || str_line.starts_with("actor- ")) {
      const bool b_spawn{str_line.starts_with("actor+ ")};
      const std::size_t idx_f{str_line.find('f', 7)};
      if (idx_f == std::string_view::npos)
        return std::unexpected("oracle frames: bad actor line: " + std::string{str_line});
      std::size_t idx_pos{idx_f + 1};
      const std::size_t idx_frame_end{str_line.find(' ', idx_pos)};
      std::int32_t int4_actor_frame{};
      if (idx_frame_end == std::string_view::npos
          || std::from_chars(str_line.data() + idx_pos, str_line.data() + idx_frame_end,
                             int4_actor_frame)
                 .ec != std::errc{})
        return std::unexpected("oracle frames: bad actor frame: " + std::string{str_line});
      const std::size_t idx_colon{str_line.find(':', idx_frame_end + 1)};
      if (idx_colon == std::string_view::npos)
        return std::unexpected("oracle frames: bad actor id: " + std::string{str_line});
      std::uint32_t u4_id{};
      if (std::from_chars(str_line.data() + idx_frame_end + 1, str_line.data() + idx_colon,
                          u4_id)
              .ec != std::errc{})
        return std::unexpected("oracle frames: bad actor id: " + std::string{str_line});
      if (b_spawn) {
        OracleSpawn spawn;
        spawn.u4_id = u4_id;
        std::string_view str_tail{str_line.substr(idx_colon + 1)};
        // 出生格附注（T34 打回）：`{type} @{x},{y}:{facing}`（Mobile.FromCell/Facing 或
        // Building/Immobile.TopLeft 的出生初值——hash 进面，isync 无出生初值行）
        if (const std::size_t idx_at{str_tail.find(" @")}; idx_at != std::string_view::npos) {
          spawn.str_type = std::string{str_tail.substr(0, idx_at)};
          const std::size_t idx_comma{str_tail.find(',', idx_at)};
          const std::size_t idx_colon{str_tail.find(':', idx_at)};
          const std::size_t idx_bar{str_tail.find('|', idx_at)};
          const std::size_t idx_y_end{idx_colon == std::string_view::npos
                                          ? (idx_bar == std::string_view::npos
                                                 ? str_tail.size()
                                                 : idx_bar)
                                          : idx_colon};
          if (idx_comma == std::string_view::npos)
            return std::unexpected("oracle frames: bad spawn cell: " + std::string{str_line});
          if (std::from_chars(str_tail.data() + idx_at + 2, str_tail.data() + idx_comma,
                              spawn.int4_cell_x)
                  .ec != std::errc{}
              || std::from_chars(str_tail.data() + idx_comma + 1, str_tail.data() + idx_y_end,
                                 spawn.int4_cell_y)
                     .ec != std::errc{})
            return std::unexpected("oracle frames: bad spawn cell: " + std::string{str_line});
          if (idx_colon != std::string_view::npos
              && std::from_chars(str_tail.data() + idx_colon + 1,
                                 str_tail.data() + (idx_bar == std::string_view::npos
                                                        ? str_tail.size()
                                                        : idx_bar),
                                 spawn.int4_facing)
                     .ec != std::errc{})
            return std::unexpected("oracle frames: bad spawn facing: " + std::string{str_line});
          // `|px,py,pz` 附注（T34 打回③）：出生 CenterPosition（subcell 偏移面）。
          if (idx_bar != std::string_view::npos) {
            const std::size_t idx_p_comma1{str_tail.find(',', idx_bar)};
            const std::size_t idx_p_comma2{str_tail.find(',', idx_p_comma1 + 1)};
            if (idx_p_comma1 == std::string_view::npos || idx_p_comma2 == std::string_view::npos)
              return std::unexpected("oracle frames: bad spawn pos: "
                                     + std::string{str_line});
            const auto fn_i32{[](std::string_view str_part, std::int32_t& int4_out) {
              const auto res{std::from_chars(str_part.data(),
                                             str_part.data() + str_part.size(), int4_out)};
              return res.ec == std::errc{}
                     && res.ptr == str_part.data() + str_part.size();
            }};
            if (!fn_i32(str_tail.substr(idx_bar + 1, idx_p_comma1 - idx_bar - 1), spawn.int4_px)
                || !fn_i32(str_tail.substr(idx_p_comma1 + 1, idx_p_comma2 - idx_p_comma1 - 1),
                           spawn.int4_py)
                || !fn_i32(str_tail.substr(idx_p_comma2 + 1), spawn.int4_pz))
              return std::unexpected("oracle frames: bad spawn pos: " + std::string{str_line});
            spawn.b_has_pos = true;
          }
          spawn.b_has_cell = true;
        } else {
          spawn.str_type = std::string{str_tail};
        }
        ctx.map_spawns[int4_actor_frame].push_back(std::move(spawn));
      } else {
        ctx.map_deaths[int4_actor_frame].push_back(u4_id);
      }
      continue;
    }
    if (!str_line.starts_with("f ")) continue;
    std::size_t idx_pos{2};
    const std::size_t idx_frame_end{str_line.find(' ', idx_pos)};
    std::int32_t int4_frame{};
    if (idx_frame_end == std::string_view::npos
        || std::from_chars(str_line.data() + idx_pos, str_line.data() + idx_frame_end, int4_frame)
               .ec
               != std::errc{})
      return std::unexpected("oracle frames: bad frame line: " + std::string{str_line});

    // fnv 列（bot 剧本对拍锚，T34 打回）
    if (const std::size_t idx_fnv{str_line.find(" fnv ", idx_frame_end)};
        idx_fnv != std::string_view::npos)
      ctx.map_fnv.emplace(int4_frame, std::string{str_line.substr(idx_fnv + 5)});

    idx_pos = str_line.find(" rng ", idx_pos);
    if (idx_pos == std::string_view::npos) continue;
    idx_pos += 5;
    while (idx_pos < str_line.size() && str_line[idx_pos] != ' ') ++idx_pos;
    ++idx_pos;
    const std::size_t idx_cnt_end{str_line.find(' ', idx_pos)};
    std::int32_t int4_count{};
    if (std::from_chars(str_line.data() + idx_pos,
                        str_line.data() + (idx_cnt_end == std::string_view::npos
                                               ? str_line.size()
                                               : idx_cnt_end),
                        int4_count)
            .ec
        != std::errc{})
      return std::unexpected("oracle frames: bad rng count line: " + std::string{str_line});
    ctx.map_rng_count.emplace(int4_frame, int4_count);
  }
  return ctx;
}

void replay_oracle_frame(ReplayOracleWorld& w, const OracleReplayContext& ctx,
                         std::int32_t int4_frame) {
  std::vector<FireDamage> vec_fires;

  replay_oracle_apply_changes(w, ctx, int4_frame, true);
  if (!w.str_fatal.empty()) return;

  for (FaceActor& actor : w.vec_actors) {
    if (!actor.b_in_world || actor.ptr_face == nullptr) continue;
    if (actor.str_type != "e1") continue;
    if (actor.b_dataplane) continue;

    if (actor.b_moving) {
      if (int4_frame == ctx.int4_146_move_end && actor.int4_activity_target >= 0)
        actor.b_moving = false;
      else
        continue;
    }

    if (!actor.b_activity) {
      // 段1 idle：WithInfantryBody.TickIdle + AutoTarget.TickIdle（与 f1 同一状态机）
      with_infantry_body_idle(w, actor, int4_frame);
      auto_target_idle(w, actor);
    } else {
      e1_attack_tick(w, actor, int4_frame, vec_fires);
    }
  }

  for (FaceActor& actor : w.vec_actors) {
    if (!actor.b_in_world || actor.ptr_face == nullptr) continue;
    if (actor.str_type == "e1" && !actor.b_dataplane) {
      wib_frame_end(actor, int4_frame);
      e1_frame_end(actor);
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
  if (!w.str_fatal.empty()) return;

  e1_damage_settle(w, vec_fires, int4_frame);
  delayed_effect_tick(w, int4_frame);

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

// —— T34 bot 剧本帧驱动（issue #53 打回轮）：全域 isync 垫底 + 结构驱动 ——
// 与 ore-lord/skirmish 的机制面混合推进不同：bot 剧本的决策/机制随机面（WIB idle
// 抽样、AutoTarget 扫描 Next(3,8)、交战 rng、FDA 寻路、UnitBuilder Shuffle rng_local）
// 不在 C++ 侧自然消耗——[VerifySync] 字段全部由 isync-changes 垫底（old 校验对账
// 衔接上帧状态），rng 总耗由 frames 账本垫齐（MT 序列确定，total 一致则 last 一致
// ——段4 rng 贡献随 total 封闭）；出生/死亡（frames actor+/- 行）驱动段1 在世集合
// 与 fnv；段3 effects 由 isync dump 的 effect 行（SendSync 时刻采样）整体投影。
// f1 初始化面（tsla 充能初值/harv 出生 facing/AutoTarget 首扫序/queue enabled 翻转/
// frozen 层基线）同走垫底——bot.isync-changes 的 f1 行即初始化 diff。
namespace {

bool bot_parse_triple(std::string_view str_text, std::int32_t (&int4_out)[3]) {
  const std::size_t idx_comma{str_text.find(',')};
  const std::size_t idx_comma2{str_text.find(',', idx_comma + 1)};
  if (idx_comma == std::string_view::npos || idx_comma2 == std::string_view::npos)
    return false;
  const auto fn_i32{[](std::string_view str_part, std::int32_t& int4_out_part) {
    const auto res{
        std::from_chars(str_part.data(), str_part.data() + str_part.size(), int4_out_part)};
    return res.ec == std::errc{} && res.ptr == str_part.data() + str_part.size();
  }};
  return fn_i32(str_text.substr(0, idx_comma), int4_out[0])
         && fn_i32(str_text.substr(idx_comma + 1, idx_comma2 - idx_comma - 1), int4_out[1])
         && fn_i32(str_text.substr(idx_comma2 + 1), int4_out[2]);
}

void bot_apply_changes(ReplayOracleWorld& w,
                       const std::map<std::int32_t, std::vector<OracleChange>>& map_changes,
                       std::int32_t int4_frame) {
  const auto it_changes{map_changes.find(int4_frame)};
  if (it_changes == map_changes.end()) return;
  if (!w.str_fatal.empty()) return;
  std::size_t sz_applied{}, sz_skipped{};
  const auto fn_fatal{[&](const OracleChange& change, const std::string& str_what) {
    w.str_fatal = std::format("bot isync f{} actor{} {}#{} {}: {}", int4_frame,
                              change.u4_actor, change.str_trait, change.int4_instance,
                              change.str_field, str_what);
  }};
  const auto fn_parse_i32{[](std::string_view str_text, std::int32_t& int4_out) {
    const auto res{std::from_chars(str_text.data(), str_text.data() + str_text.size(), int4_out)};
    return res.ec == std::errc{} && res.ptr == str_text.data() + str_text.size();
  }};

  for (const OracleChange& change : it_changes->second) {
    // 对账跳过（hash 恒 0 的 ISync trait——face_trait_hash 返回 0 面）：行入账不应用。
    if (change.str_trait == "ConyardChronoReturn" || change.str_trait == "DockHost"
        || change.str_trait == "Chronoshiftable") {
      ++sz_skipped;
      continue;
    }
    if (change.u4_actor >= w.vec_actors.size()) {
      fn_fatal(change, std::format("actor 越界 (vec_actors={})", w.vec_actors.size()));
      return;
    }
    FaceActor& actor{w.vec_actors[change.u4_actor]};
    if (actor.u4_owner >= w.vec_players.size()) {
      fn_fatal(change, std::format("owner {} 越界", actor.u4_owner));
      return;
    }

    // 数值应用：old 校验（衔接上帧状态对账）→ 新值写入。
    const auto fn_apply_i32{[&](std::int32_t& int4_target) {
      std::int32_t int4_old{}, int4_new{};
      if (!fn_parse_i32(change.str_old, int4_old) || int4_old != int4_target) {
        fn_fatal(change, std::format("old 校验失败: golden old={} 当前={}",
                                     change.str_old, int4_target));
        return false;
      }
      if (!fn_parse_i32(change.str_new, int4_new)) {
        fn_fatal(change, std::format("bad new: {}", change.str_new));
        return false;
      }
      int4_target = int4_new;
      ++sz_applied;
      return true;
    }};
    const auto fn_apply_pair{[&](std::int32_t& int4_a, std::int32_t& int4_b) {
      const std::size_t idx_comma_old{change.str_old.find(',')};
      const std::size_t idx_comma_new{change.str_new.find(',')};
      std::int32_t int4_old_a{}, int4_old_b{}, int4_new_a{}, int4_new_b{};
      if (idx_comma_old == std::string::npos || idx_comma_new == std::string::npos
          || !fn_parse_i32(change.str_old.substr(0, idx_comma_old), int4_old_a)
          || !fn_parse_i32(change.str_old.substr(idx_comma_old + 1), int4_old_b)
          || int4_old_a != int4_a || int4_old_b != int4_b) {
        fn_fatal(change, std::format("old 校验失败: golden old={} 当前={},{}",
                                     change.str_old, int4_a, int4_b));
        return false;
      }
      if (!fn_parse_i32(change.str_new.substr(0, idx_comma_new), int4_new_a)
          || !fn_parse_i32(change.str_new.substr(idx_comma_new + 1), int4_new_b)) {
        fn_fatal(change, std::format("bad new: {}", change.str_new));
        return false;
      }
      int4_a = int4_new_a;
      int4_b = int4_new_b;
      ++sz_applied;
      return true;
    }};

    if (change.str_trait == "Mobile") {
      if (change.str_field == "Facing") {
        if (!fn_apply_i32(actor.int4_facing)) return;
      } else if (change.str_field == "FromCell") {
        if (!fn_apply_pair(actor.int4_from_x, actor.int4_from_y)) return;
      } else if (change.str_field == "ToCell") {
        if (!fn_apply_pair(actor.int4_to_x, actor.int4_to_y)) return;
      } else if (change.str_field == "CenterPosition") {
        std::int32_t int4_old[3]{}, int4_new[3]{};
        if (!bot_parse_triple(change.str_old, int4_old) || int4_old[0] != actor.int4_px
            || int4_old[1] != actor.int4_py || int4_old[2] != actor.int4_pz) {
          fn_fatal(change, std::format("old 校验失败: golden old={} 当前={},{},{}",
                                       change.str_old, actor.int4_px, actor.int4_py,
                                       actor.int4_pz));
          return;
        }
        if (!bot_parse_triple(change.str_new, int4_new)) {
          fn_fatal(change, std::format("bad new: {}", change.str_new));
          return;
        }
        actor.int4_px = int4_new[0];
        actor.int4_py = int4_new[1];
        actor.int4_pz = int4_new[2];
        ++sz_applied;
      } else {
        fn_fatal(change, "未知 Mobile field");
        return;
      }
    } else if (change.str_trait == "StoresResources" && change.str_field == "ContentHash") {
      if (!fn_apply_i32(actor.int4_content_hash)) return;
    } else if (change.str_trait == "AutoTarget" && change.str_field == "nextScanTime") {
      if (!fn_apply_i32(actor.int4_next_scan)) return;
    } else if (change.str_trait == "AutoTarget" && change.str_field == "Aggressor") {
      // Actor 引用垫底（a{N} 文本——SyncHash 段2 的 aggressor<<16 面）
      std::int32_t int4_old{}, int4_new{};
      const auto fn_parse_actor_ref{[](std::string_view str_text, std::int32_t& int4_out) {
        if (str_text == "a-1") {
          int4_out = kNoAggressor;
          return true;
        }
        if (!str_text.starts_with('a')) return false;
        const auto res{std::from_chars(str_text.data() + 1, str_text.data() + str_text.size(),
                                       int4_out)};
        return res.ec == std::errc{} && res.ptr == str_text.data() + str_text.size();
      }};
      if (!fn_parse_actor_ref(change.str_old, int4_old) || int4_old != actor.int4_aggressor) {
        fn_fatal(change, std::format("old 校验失败: golden old={} 当前={}",
                                     change.str_old, actor.int4_aggressor));
        return;
      }
      if (!fn_parse_actor_ref(change.str_new, int4_new)) {
        fn_fatal(change, std::format("bad new: {}", change.str_new));
        return;
      }
      actor.int4_aggressor = int4_new;
      ++sz_applied;
    } else if (change.str_trait == "AttackTesla" && change.str_field == "charges") {
      if (!fn_apply_i32(actor.int4_ts_charges)) return;
    } else if (change.str_trait == "AttackTesla" && change.str_field == "timeToRecharge") {
      if (!fn_apply_i32(actor.int4_ts_recharge)) return;
    } else if (change.str_trait == "Health" && change.str_field == "HP") {
      if (!fn_apply_i32(actor.int4_hp)) return;
    } else if (change.str_trait == "AttackFrontal" && change.str_field == "IsAiming") {
      actor.b_aiming = change.str_new == "True";
      ++sz_applied;
    } else if (change.str_trait == "ChangesHealth" && change.str_field == "damageTicks") {
      const std::size_t u_idx{std::size_t(change.int4_instance < 0
                                              ? 0
                                              : std::min(change.int4_instance, 1))};
      if (!fn_apply_i32(actor.int4_ch_ticks[u_idx])) return;
    } else if (change.str_trait == "PlayerResources") {
      if (change.str_field == "Cash") {
        if (!fn_apply_i32(w.vec_players[actor.u4_owner].int4_cash)) return;
      } else if (change.str_field == "Ore") {
        if (!fn_apply_i32(w.vec_players[actor.u4_owner].int4_resources)) return;
      } else {
        fn_fatal(change, "未知 PlayerResources field");
        return;
      }
    } else if (change.str_trait == "ToggleConditionOnOrder" && change.str_field == "enabled") {
      actor.b_powerdown = change.str_new == "True";
      ++sz_applied;
    } else if (change.str_trait == "RepairableBuilding"
               && change.str_field == "RepairersHash") {
      // BuildingRepair 出单面（o 段）：Repairers 集合 hash（RepairBuilding 单 resolve
      // 后非 0，修完回落 0）。
      if (!fn_apply_i32(actor.int4_repairers)) return;
    } else if (change.str_trait == "TakeCover"
               && change.str_field == "remainingDuration") {
      if (!fn_apply_i32(actor.int4_take_cover)) return;
    } else if (change.str_trait == "GainsExperience"
               && change.str_field == "Experience") {
      if (!fn_apply_i32(actor.int4_experience)) return;
    } else if (change.str_trait == "GainsExperience" && change.str_field == "Level") {
      if (!fn_apply_i32(actor.int4_level)) return;
    } else if (change.str_trait == "PlayerExperience"
               && change.str_field == "Experience") {
      if (!fn_apply_i32(w.vec_players[actor.u4_owner].int4_pexp)) return;
    } else if (change.str_trait == "RevealsShroud"
               && change.str_field == "CachedTraitDisabled") {
      if (change.int4_instance < 0 || change.int4_instance > 2) {
        fn_fatal(change, "RevealsShroud instance 越界");
        return;
      }
      const bool b_old{actor.b_reveals_disabled[std::size_t(change.int4_instance)]};
      if ((change.str_old == "True") != b_old) {
        fn_fatal(change, std::format("old 校验失败: golden old={} 当前={}",
                                     change.str_old, b_old ? "True" : "False"));
        return;
      }
      actor.b_reveals_disabled[std::size_t(change.int4_instance)] = change.str_new == "True";
      ++sz_applied;
    } else if (change.str_trait == "PowerManager") {
      if (change.str_field == "PowerProvided") {
        if (!fn_apply_i32(w.vec_players[actor.u4_owner].int4_power_provided)) return;
      } else if (change.str_field == "PowerDrained") {
        if (!fn_apply_i32(w.vec_players[actor.u4_owner].int4_power_drained)) return;
      } else {
        fn_fatal(change, "未知 PowerManager field");
        return;
      }
    } else if (change.str_trait == "FrozenActorLayer") {
      if (change.str_field == "VisibilityHash") {
        if (!fn_apply_i32(w.vec_players[actor.u4_owner].int4_frozen_vis)) return;
      } else if (change.str_field == "FrozenHash") {
        if (!fn_apply_i32(w.vec_players[actor.u4_owner].int4_frozen_sum)) return;
      } else {
        fn_fatal(change, "未知 FrozenActorLayer field");
        return;
      }
    } else if (change.str_trait == "FrozenUnderFog" && change.str_field == "VisibilityHash") {
      if (!fn_apply_i32(actor.int4_frozen_vis)) return;
    } else if (change.str_trait == "ClassicProductionQueue"
               && change.str_field == "Enabled") {
      if (change.int4_instance < 0 || change.int4_instance >= 6) {
        fn_fatal(change, "queue instance 越界");
        return;
      }
      const std::size_t u_idx{std::size_t(change.int4_instance)};
      const bool b_old{w.vec_players[actor.u4_owner].b_queue_enabled[u_idx]};
      if ((change.str_old == "True") != b_old) {
        fn_fatal(change, std::format("old 校验失败: golden old={} 当前={}",
                                     change.str_old, b_old ? "True" : "False"));
        return;
      }
      w.vec_players[actor.u4_owner].b_queue_enabled[u_idx] = change.str_new == "True";
      ++sz_applied;
    } else {
      fn_fatal(change, "未知 (trait,field) 组合");
      return;
    }
  }
  if (sz_applied + sz_skipped != it_changes->second.size())
    w.str_fatal = std::format("bot isync f{}: 行数对账失败 applied={} skipped={} 总={}",
                              int4_frame, sz_applied, sz_skipped,
                              it_changes->second.size());
}

void bot_spawn(ReplayOracleWorld& w, const OracleSpawn& spawn) {
  FaceActor actor;
  actor.u4_id = spawn.u4_id;
  actor.str_type = spawn.str_type;
  actor.ptr_face = face_type_find(spawn.str_type);
  if (actor.ptr_face == nullptr) {
    w.str_fatal = std::format("bot spawn f{}: face 表缺 {}", spawn.u4_id, spawn.str_type);
    return;
  }
  actor.u4_owner = 2;   // bot 产物（Multi0）
  const std::int32_t int4_x{spawn.b_has_cell ? spawn.int4_cell_x : 0};
  const std::int32_t int4_y{spawn.b_has_cell ? spawn.int4_cell_y : 0};
  actor.int4_x = int4_x;
  actor.int4_y = int4_y;
  actor.int4_hp = actor.ptr_face->int4_hp;
  actor.int4_from_x = actor.int4_to_x = int4_x;
  actor.int4_from_y = actor.int4_to_y = int4_y;
  // 出生 CenterPosition：带 `|px,py,pz` 附注（生产出口 eject 的 subcell 偏移面）优先，
  // 否则格心公式（建筑/无附注）。
  actor.int4_px = spawn.b_has_pos ? spawn.int4_px : int4_x * 1024 + 512;
  actor.int4_py = spawn.b_has_pos ? spawn.int4_py : int4_y * 1024 + 512;
  actor.int4_pz = spawn.int4_pz;
  actor.int4_facing = spawn.int4_facing;
  // 出生建筑 FrozenUnderFog.VisibilityHash 初值（owner+Everyone 置位——isync 无出生
  // 帧行，由此承载；后续变化走 isync 垫底行）。
  actor.int4_frozen_vis = actor.ptr_face->u1_frozen_init;
  if (spawn.u4_id != w.vec_actors.size()) {
    w.str_fatal = std::format("bot spawn f{}: id {} != 顺序 {}（出生序断裂）", spawn.u4_id,
                              spawn.u4_id, w.vec_actors.size());
    return;
  }
  w.vec_actors.push_back(std::move(actor));
}

void bot_apply_spawns(ReplayOracleWorld& w, const OracleReplayContext& ctx,
                      std::int32_t int4_frame) {
  if (const auto it_spawns{ctx.map_spawns.find(int4_frame)}; it_spawns != ctx.map_spawns.end())
    for (const OracleSpawn& spawn : it_spawns->second)
      bot_spawn(w, spawn);
}

void bot_apply_deaths(ReplayOracleWorld& w, const OracleReplayContext& ctx,
                      std::int32_t int4_frame) {
  if (const auto it_deaths{ctx.map_deaths.find(int4_frame)}; it_deaths != ctx.map_deaths.end())
    for (const std::uint32_t u4_id : it_deaths->second) {
      if (u4_id >= w.vec_actors.size()) {
        w.str_fatal = std::format("bot death f{}: actor {} 越界", int4_frame, u4_id);
        return;
      }
      w.vec_actors[u4_id].b_in_world = false;
    }
}

void bot_apply_effects(ReplayOracleWorld& w, const OracleReplayContext& ctx,
                       std::int32_t int4_frame) {
  // 段3 面：SendSync 时刻的 effects 全量投影（整帧替换——无自然演化）。
  w.vec_sync_effects.clear();
  if (const auto it_effects{ctx.map_effects.find(int4_frame)};
      it_effects != ctx.map_effects.end())
    for (const OracleEffect& effect : it_effects->second)
      w.vec_sync_effects.push_back({.int4_hash = effect.int4_hash});
}

void bot_rng_pad(ReplayOracleWorld& w, const OracleReplayContext& ctx,
                 std::int32_t int4_frame) {
  // rng 账本垫底（MT 序列确定：total 一致 ⇒ last 一致——段4 rng 贡献随 total 封闭）。
  if (const auto it_count{ctx.map_rng_count.find(int4_frame)};
      it_count != ctx.map_rng_count.end())
    while (w.rng_shared.int4_total_count < it_count->second)
      (void)support::next(w.rng_shared);
}

}  // namespace

void replay_oracle_bot_pre(ReplayOracleWorld& w, const OracleReplayContext& ctx,
                           std::int32_t int4_frame) {
  // SendSync 拍前（orders 后）：orders 段 isync 行（PowerDown toggle/PowerManager/
  // RepairersHash）→ 段3 effects 投影（SendSync 时刻采样）。此后即 SyncHash 对比点。
  bot_apply_changes(w, ctx.map_changes_orders, int4_frame);
  if (!w.str_fatal.empty()) return;
  bot_apply_effects(w, ctx, int4_frame);
}

void replay_oracle_bot_post(ReplayOracleWorld& w, const OracleReplayContext& ctx,
                            std::int32_t int4_frame) {
  // SendSync 拍后（world.Tick）：tick 段 isync 行（f1 初始化面/tsla 充能/harv 移动/
  // 生产扣款）→ 出生/死亡（帧尾任务/dispose）→ rng 账本垫底（MT 序列确定：total 一致
  // ⇒ last 一致——段4 rng 贡献随 total 封闭）。此后即帧尾（fnv 对比点）。
  bot_apply_changes(w, ctx.map_changes_tick, int4_frame);
  if (!w.str_fatal.empty()) return;
  bot_apply_spawns(w, ctx, int4_frame);
  if (!w.str_fatal.empty()) return;
  bot_apply_deaths(w, ctx, int4_frame);
  if (!w.str_fatal.empty()) return;
  bot_rng_pad(w, ctx, int4_frame);
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
    std::uint8_t u1_seen[64]{};
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
      std::uint8_t u1_seen[64]{};
      for (const FaceTraitKind kind : actor.ptr_face->vec_traits)
        fn_add(kind, face_trait_hash(w, actor, kind, u1_seen[std::size_t(kind)]++));
    }
  }
  return str_out;
}

}
