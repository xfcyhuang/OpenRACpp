module ora.launcher;

import std;
import ora.game;
import ora.game.filesystem;
import ora.game.miniyaml;
import ora.game.pathfinding;
import ora.game.support;
import ora.game.world;

namespace ora::launcher {

namespace {

constexpr std::string_view kRules{
    R"yaml(world:
	FactionInfo@allies:
		Name: Allies
		InternalName: allies
		Side: Allies
	FactionInfo@soviet:
		Name: Soviet
		InternalName: soviet
		Side: Soviet
	Locomotor@HEAVYWHEELED:
		Name: heavywheeled
		TerrainSpeeds:
			Clear: 100
			Rough: 50
			Road: 125
			Bridge: 125
			Ore: 88
			Gems: 88
			Beach: 50
	Locomotor@FOOT:
		Name: foot
		SharesCell: True
		TerrainSpeeds:
			Clear: 100
			Rough: 89
			Road: 111
			Bridge: 111
			Ore: 89
			Gems: 89
			Beach: 89
	PathFinder:
	SpawnMapActors:
	ResourceClaimLayer:
	ResourceLayer:
		RecalculateResourceDensity: true
		ResourceTypes:
			Ore:
				ResourceIndex: 1
				TerrainType: Ore
				AllowedTerrainTypes: Clear, Road
				MaxDensity: 12
			Gems:
				ResourceIndex: 2
				TerrainType: Gems
				AllowedTerrainTypes: Clear, Road
				MaxDensity: 3
	BuildingInfluence:
player:
	Shroud:
	DeveloperMode:
	PlayerResources:
		ResourceValues:
			Ore: 25
			Gems: 50
	TechTree:
	ClassicProductionQueue@Building:
		Type: Building
		LowPowerModifier: 300
		ReadyAudio: UnitReady
		BlockedAudio: NoBuild
	ClassicProductionQueue@Defense:
		Type: Defense
		LowPowerModifier: 300
	ClassicProductionQueue@Vehicle:
		Type: Vehicle
		LowPowerModifier: 300
	ClassicProductionQueue@Infantry:
		Type: Infantry
		LowPowerModifier: 300
		SpeedUp: true
	ClassicProductionQueue@Ship:
		Type: Ship
		LowPowerModifier: 300
	ClassicProductionQueue@Aircraft:
		Type: Aircraft
		LowPowerModifier: 300
	PlaceBuilding:
	PowerManager:
	ProvidesTechPrerequisite@unrestricted:
		Id: unrestricted
		Prerequisites: techlevel.infonly, techlevel.low, techlevel.medium, techlevel.high, techlevel.unrestricted
mpspawn:
fact:
	Selectable:
		Bounds: 2048,2048
	Targetable:
		TargetTypes: GroundActor, Structure
	Building:
		Footprint: xxX xxx XxX ===
		Dimensions: 3,4
		LocalCenterOffset: 0,-512,0
	BaseProvider:
		Range: 16c0
	Production:
		Produces: Building, Defense
	GivesBuildableArea:
	Power:
		Amount: 0
	ProvidesPrerequisite@soviet:
		Factions: soviet, russia, ukraine
		Prerequisite: structures.soviet
powr:
	Selectable:
		Bounds: 2048,2048
	Targetable:
		TargetTypes: GroundActor, Structure
	Valued:
		Cost: 300
	Buildable:
		Queue: Building
		Prerequisites: ~techlevel.infonly
	Power:
		Amount: 100
	Building:
		Footprint: xx xx ==
		Dimensions: 2,3
		LocalCenterOffset: 0,-512,0
		RequiresBaseProvider: True
		BuildSounds: placedown1.aud,placedown2.aud
	GivesBuildableArea:
	ProvidesPrerequisite:
		Prerequisite: anypower
barr:
	Selectable:
		Bounds: 2048,2048
	Targetable:
		TargetTypes: GroundActor, Structure
	Valued:
		Cost: 500
	Buildable:
		Queue: Building
		Prerequisites: anypower, ~structures.soviet, ~techlevel.infonly
	Power:
		Amount: -20
	Building:
		Footprint: xx xx ==
		Dimensions: 2,3
		LocalCenterOffset: 0,-512,0
		RequiresBaseProvider: True
	GivesBuildableArea:
	Production:
		Produces: Infantry, Soldier
	Exit@1:
		ExitCell: 1,2
		ProductionTypes: Soldier, Infantry
	Exit@2:
		ExitCell: 0,2
		ProductionTypes: Soldier, Infantry
	ProvidesPrerequisite:
		Prerequisite: barracks
	ProvidesPrerequisite@buildingname:
ftur:
	Selectable:
		Bounds: 2048,2048
	Targetable:
		TargetTypes: GroundActor, Structure
	Valued:
		Cost: 500
	Buildable:
		Queue: Defense
		Prerequisites: anypower, ~structures.soviet, ~techlevel.infonly
	Power:
		Amount: -20
	Building:
		Footprint: xx xx ==
		Dimensions: 2,3
		LocalCenterOffset: 0,-512,0
		RequiresBaseProvider: True
	GivesBuildableArea:
	ProvidesPrerequisite:
		Prerequisite: defense
silo:
	Selectable:
		Bounds: 1024,1024
	Targetable:
		TargetTypes: GroundActor, Structure
	Building:
		Footprint: x
		Dimensions: 1,1
	Power:
		Amount: -10
proc:
	Selectable:
		Bounds: 2048,2048
	RenderSprites:
	WithSpriteBody:
	Targetable:
		TargetTypes: GroundActor, Structure
	Building:
		Footprint: _X_ xxx X== ===
		Dimensions: 3,4
		LocalCenterOffset: 0,-512,0
	DockHost:
		Type: Unload
		DockAngle: 256
		DockOffset: 0,1c0,0
	Refinery:
		ShowTicks: false
	StoresPlayerResources:
		Capacity: 2000
	FreeActor:
		Actor: harv
		SpawnOffset: 1,2
		Facing: 256
harv:
	Selectable:
		Bounds: 1024,1024
	RenderSprites:
	WithSpriteBody:
	Targetable:
		TargetTypes: GroundActor, Vehicle
	HitShape:
	BodyOrientation:
		QuantizedFacings: 32
	Mobile:
		Locomotor: heavywheeled
		Speed: 72
		TurnSpeed: 20
	StoresResources:
		Capacity: 20
		Resources: Ore,Gems
	Harvester:
		Resources: Ore,Gems
		BaleUnloadDelay: 1
		SearchFromProcRadius: 15
		SearchFromHarvesterRadius: 8
		HarvestFacings: 8
	DockClientManager:
	WithDockingAnimation:
e1:
	Selectable:
		Bounds: 512,512
	Targetable:
		TargetTypes: GroundActor, Infantry
	HitShape:
	BodyOrientation:
		QuantizedFacings: 8
	Mobile:
		Locomotor: foot
		Speed: 113
		TurnSpeed: 512
	MobileProbe:
	AttackProbe:
	Valued:
		Cost: 100
	Buildable:
		Queue: Infantry
		BuildAtProductionType: Soldier
		Prerequisites: ~barracks, ~techlevel.infonly
	WithInfantryBody:
		IdleSequences: idle1, idle2
)yaml"};

constexpr std::string_view kMap{
    "Title: ora-sandbox\n"
    "Players:\n"
    "\tPlayerReference@Neutral:\n"
    "\t\tName: Neutral\n"
    "\t\tOwnsWorld: True\n"
    "\t\tNonCombatant: True\n"
    "\t\tFaction: allies\n"
    "\tPlayerReference@Multi0:\n"
    "\t\tName: Multi0\n"
    "\t\tPlayable: True\n"
    "\t\tFaction: soviet\n"
    "\tPlayerReference@Creeps:\n"
    "\t\tName: Creeps\n"
    "\t\tNonCombatant: True\n"
    "\t\tFaction: allies\n"
    "Actors:\n"
    "\tfact0: fact\n"
    "\t\tLocation: 17,20\n"
    "\t\tOwner: Multi0\n"
    "\tproc0: proc\n"
    "\t\tLocation: 21,20\n"
    "\t\tOwner: Multi0\n"
    "\te1own: e1\n"
    "\t\tLocation: 18,24\n"
    "\t\tOwner: Multi0\n"
    "\te1foe: e1\n"
    "\t\tLocation: 28,24\n"
    "\t\tOwner: Creeps\n"};

constexpr std::string_view kWeapons{
    "carbine:\n"
    "\tReloadDelay: 20\n"
    "\tRange: 5c0\n"
    "\tReport: gun11.aud\n"
    "\tProjectile: InstantHit\n"
    "\tWarhead@Damage: SpreadDamage\n"
    "\t\tDamage: 40\n"
    "\t\tValidTargets: Ground, GroundActor\n"
    "\tWarhead@Effect: CreateEffect\n"
    "\t\tExplosions: piff\n"
    "\t\tImpactSounds: kaboom12.aud\n"
    "\t\tValidTargets: Ground, GroundActor\n"};

constexpr std::int32_t kSandboxSize{40};

game::Tileset sandbox_tileset() {
  game::Tileset tileset;
  tileset.str_id = "sandbox";
  tileset.str_name = "Sandbox";
  tileset.int4_tile_size_x = 24;
  tileset.int4_tile_size_y = 24;
  tileset.vec_str_types = {"Beach", "Bridge", "Clear", "Gems", "Ore", "Road", "Rough"};
  game::TerrainTemplate tpl;
  tpl.u2_id = 1;
  tpl.int4_size_x = 1;
  tpl.int4_size_y = 1;
  const std::size_t idx_clear{
      static_cast<std::size_t>(tileset.terrain_index_of_type("Clear").value_or(2))};
  tpl.vec_opt1u_frame_terrain.push_back(static_cast<std::uint8_t>(idx_clear));
  tileset.vec_templates.push_back(std::move(tpl));
  tileset.u1_default_walkable = static_cast<std::uint8_t>(idx_clear);
  return tileset;
}

}

std::string_view sandbox_rules_text() { return kRules; }

std::string_view sandbox_map_text() { return kMap; }

std::string_view sandbox_weapons_text() { return kWeapons; }

SandboxMapData sandbox_map_data() {
  SandboxMapData data;
  data.tileset = sandbox_tileset();
  data.bin_map.int4_width = kSandboxSize;
  data.bin_map.int4_height = kSandboxSize;
  data.bin_map.vec_tiles.assign(
      static_cast<std::size_t>(kSandboxSize) * static_cast<std::size_t>(kSandboxSize),
      game::TerrainTile{1, 0});
  data.bin_map.vec_resources.assign(
      static_cast<std::size_t>(kSandboxSize) * static_cast<std::size_t>(kSandboxSize),
      game::ResourceTile{});

  for (std::int32_t int4_x{20}; int4_x < 28; ++int4_x)
    for (std::int32_t int4_y{24}; int4_y < 27; ++int4_y)
      data.bin_map.vec_resources[static_cast<std::size_t>(int4_x) *
                                     static_cast<std::size_t>(kSandboxSize) +
                                 static_cast<std::size_t>(int4_y)] = game::ResourceTile{1, 2};
  return data;
}

std::expected<LoadedMap, std::string> load_builtin_sandbox() {
  LoadedMap out;
  const SandboxMapData data{sandbox_map_data()};
  auto res_map{game::world::synthetic_map_from_yaml(kRules, kMap)};
  if (!res_map.has_value()) return std::unexpected(res_map.error());
  out.map = std::move(*res_map);
  out.bin_map = data.bin_map;
  out.tileset = data.tileset;
  out.int4_right = kSandboxSize;
  out.int4_bottom = kSandboxSize;
  out.str_title = "ora-sandbox";
  return out;
}

std::expected<LoadedMap, std::string> load_oramap(std::string_view str_path,
                                                  std::string_view str_tileset_path) {
  std::ifstream stream_in{std::filesystem::path{std::string(str_path)}, std::ios::binary};
  if (!stream_in) return std::unexpected(std::format("map not found: {}", str_path));
  std::string str_bytes;
  char buf1_chunk[8192];
  while (stream_in.read(buf1_chunk, static_cast<std::streamsize>(sizeof buf1_chunk)) ||
         stream_in.gcount() > 0)
    str_bytes.append(buf1_chunk, static_cast<std::size_t>(stream_in.gcount()));

  const std::vector<std::uint8_t> vec1u_bytes{str_bytes.begin(), str_bytes.end()};
  const auto res_zip{game::filesystem::zip_read(vec1u_bytes)};
  if (!res_zip.has_value()) return std::unexpected(res_zip.error());
  const auto vec_yaml{game::filesystem::zip_entry_data(*res_zip, "map.yaml")};
  const auto vec_bin{game::filesystem::zip_entry_data(*res_zip, "map.bin")};
  if (!vec_yaml.has_value() || !vec_bin.has_value())
    return std::unexpected("oramap lacks map.yaml/map.bin");

  const std::string str_yaml{vec_yaml->begin(), vec_yaml->end()};
  const auto res_header{game::map_header_parse(*vec_yaml, "map.yaml")};
  if (!res_header.has_value()) return std::unexpected(res_header.error());

  LoadedMap out;
  out.str_title = res_header->str_title;
  out.int4_left = res_header->int4_bounds_x;
  out.int4_top = res_header->int4_bounds_y;
  out.int4_right = res_header->int4_bounds_x + res_header->int4_bounds_w;
  out.int4_bottom = res_header->int4_bounds_y + res_header->int4_bounds_h;

  const auto res_bin{game::map_bin_parse(*vec_bin, res_header->int4_size_x,
                                         res_header->int4_size_y)};
  if (!res_bin.has_value()) return std::unexpected(res_bin.error());
  out.bin_map = std::move(*res_bin);

  std::ifstream stream_tileset{std::filesystem::path{std::string{str_tileset_path}},
                               std::ios::binary};
  if (!stream_tileset)
    return std::unexpected(std::format("tileset {} not found (run from executable dir or "
                                       "pass absolute path)",
                                       str_tileset_path));
  std::string str_tileset;
  while (stream_tileset.read(buf1_chunk, static_cast<std::streamsize>(sizeof buf1_chunk)) ||
         stream_tileset.gcount() > 0)
    str_tileset.append(buf1_chunk, static_cast<std::size_t>(stream_tileset.gcount()));
  const auto res_tileset{game::tileset_load(str_tileset, "temperat.yaml")};
  if (!res_tileset.has_value()) return std::unexpected(res_tileset.error());
  out.tileset = std::move(*res_tileset);

  auto res_map{game::world::synthetic_map_from_yaml(sandbox_rules_text(), str_yaml)};
  if (!res_map.has_value()) return std::unexpected(res_map.error());
  out.map = std::move(*res_map);

  std::set<std::string> set_known_types;
  {
    auto res_rules{game::miniyaml_from_string(sandbox_rules_text(), "sandbox-rules", true)};
    if (!res_rules.has_value()) return std::unexpected(res_rules.error());
    for (const game::MiniYamlNode& node : *res_rules) {
      std::string str_lower{node.str_key};
      for (char& ch : str_lower)
        if (ch >= 'A' && ch <= 'Z') ch = static_cast<char>(ch - 'A' + 'a');
      set_known_types.insert(str_lower);
    }
  }

  std::vector<game::world::MapActorRef> vec_actors_kept;
  std::int32_t int4_dropped{0};
  for (const game::world::MapActorRef& ref : out.map.vec_actor_defs) {
    std::string str_lower{ref.str_type};
    for (char& ch : str_lower)
      if (ch >= 'A' && ch <= 'Z') ch = static_cast<char>(ch - 'A' + 'a');
    if (set_known_types.contains(str_lower)) {
      vec_actors_kept.push_back(ref);
    } else {
      ++int4_dropped;
    }
  }
  out.map.vec_actor_defs = std::move(vec_actors_kept);
  out.str_title = std::format("{} (smoke: {} unknown actors dropped)", out.str_title,
                              int4_dropped);
  return out;
}

}
