import std;


#include "game/economy/economy.hpp"
#include "game/filesystem/zip_read.hpp"
#include "game/map_bin.hpp"
#include "game/map_read.hpp"
#include "game/miniyaml.hpp"
#include "game/network/order.hpp"
#include "game/network/order_manager.hpp"
#include "game/tileset.hpp"
#include "game/world/sync_hash.hpp"
#include "game/world/world.hpp"

// T13（issue #14）：资源采集与经济最小面。
// 剧本 A（调研笔记 t13-economy.md §3）：ore-lord 原图资源区 + 最小经济规则集 +
// 注入 PROC@(11,13)/BARR@(15,14)（剔除全部 12 个 MINE → 全链零 SeedsResource RNG）+
// PROC FreeActor 附送 HARV + startingcash=0 + t460 下单 E1。
// 完整闭环：t167 首 bale → t350 满 20 → t581 到 dock → t600 起 1 bale/tick 变现
// → t659 E1 出厂（rng 恰 +1，Exit shuffle）→ 终态 ore = 20×25 − 100 = 400。

#ifndef ORA_ECONOMY_GOLDEN_DIR
#define ORA_ECONOMY_GOLDEN_DIR "tests/golden/economy"
#endif

#ifndef ORA_MAPS_GOLDEN_DIR
#define ORA_MAPS_GOLDEN_DIR "tests/golden/maps"
#endif

namespace {

namespace eco = ora::game::economy;
namespace ogf = ora::game::filesystem;
namespace ogm = ora::game;
namespace pfg = ora::game::pathfinding;

int g_failed{};
int g_total{};
constexpr int kMaxReports{20};

void check(bool b_ok, std::string_view str_what) {
  ++g_total;
  if (b_ok) return;
  ++g_failed;
  if (g_failed <= kMaxReports) std::println("FAIL: {}", str_what);
}

constexpr std::int32_t kSeed{0};
constexpr std::int32_t kOrderTick{460};
constexpr std::int32_t kMaxTicks{1200};

std::optional<std::string> read_file_bytes(std::string_view str_path) {
  std::ifstream stream_in{std::filesystem::path{std::string(str_path)}, std::ios::binary};
  if (!stream_in) return std::nullopt;
  std::string str_bytes;
  char buf1_chunk[8192];
  while (stream_in.read(buf1_chunk, static_cast<std::streamsize>(sizeof buf1_chunk))
         || stream_in.gcount() > 0)
    str_bytes.append(buf1_chunk, static_cast<std::size_t>(stream_in.gcount()));
  return str_bytes;
}

std::vector<std::uint8_t> to_bytes(const std::string& str_text) {
  return std::vector<std::uint8_t>{str_text.begin(), str_text.end()};
}

std::string read_golden(std::string_view str_name) {
  const auto str_bytes{read_file_bytes(std::format("{}/{}", ORA_ECONOMY_GOLDEN_DIR, str_name))};
  if (!str_bytes.has_value()) return {};
  std::string str_out;
  std::size_t sz_pos{0};
  while (sz_pos < str_bytes->size()) {
    const std::size_t sz_end{str_bytes->find('\n', sz_pos)};
    const std::size_t sz_stop{sz_end == std::string::npos ? str_bytes->size() : sz_end};
    std::string_view str_line{std::string_view{*str_bytes}.substr(sz_pos, sz_stop - sz_pos)};
    if (!str_line.empty() && str_line.front() != '#') {
      str_out += str_line;
      str_out += '\n';
    }
    if (sz_end == std::string::npos) break;
    sz_pos = sz_end + 1;
  }
  return str_out;
}

struct OreLordMap {
  ogm::MapBin bin_map;
  ogm::Tileset tileset;
  std::int32_t int4_left{};
  std::int32_t int4_top{};
  std::int32_t int4_right{};
  std::int32_t int4_bottom{};
  std::string str_uid;
};

std::optional<OreLordMap> load_ore_lord() {
  const auto str_oramap{read_file_bytes(std::format("{}/ore-lord.oramap", ORA_MAPS_GOLDEN_DIR))};
  const auto str_tileset{read_file_bytes(std::format("{}/tilesets/temperat.yaml", ORA_MAPS_GOLDEN_DIR))};
  if (!str_oramap.has_value() || !str_tileset.has_value()) return std::nullopt;

  const auto res_zip{ogf::zip_read(to_bytes(*str_oramap))};
  if (!res_zip.has_value()) return std::nullopt;
  const auto vec_yaml{ogf::zip_entry_data(*res_zip, "map.yaml")};
  const auto vec_bin{ogf::zip_entry_data(*res_zip, "map.bin")};
  if (!vec_yaml.has_value() || !vec_bin.has_value()) return std::nullopt;

  const auto res_header{ogm::map_header_parse(*vec_yaml, "map.yaml")};
  if (!res_header.has_value()) return std::nullopt;

  OreLordMap ore;
  const auto res_uid{ogm::map_uid(*res_zip)};
  if (!res_uid.has_value()) return std::nullopt;
  ore.str_uid = *res_uid;

  const auto res_tileset{ogm::tileset_load(*str_tileset, "temperat.yaml")};
  if (!res_tileset.has_value()) return std::nullopt;
  ore.tileset = std::move(*res_tileset);

  const auto res_bin{ogm::map_bin_parse(*vec_bin, res_header->int4_size_x, res_header->int4_size_y)};
  if (!res_bin.has_value()) return std::nullopt;
  ore.bin_map = std::move(*res_bin);
  ore.int4_left = res_header->int4_bounds_x;
  ore.int4_top = res_header->int4_bounds_y;
  ore.int4_right = res_header->int4_bounds_x + res_header->int4_bounds_w;
  ore.int4_bottom = res_header->int4_bounds_y + res_header->int4_bounds_h;
  return ore;
}

// 最小经济规则集（oracle golden_gen economy 模式逐字段同源；Faction@→FactionInfo@
// 为 C++ trait 注册表适配，faction 表内容一致：allies/soviet）。
constexpr std::string_view kRulesEconomy{R"yaml(world:
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
	ClassicProductionQueue@Infantry:
		Type: Infantry
		LowPowerModifier: 300
		SpeedUp: true
proc:
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
barr:
	RenderSprites:
	WithSpriteBody:
	Targetable:
		TargetTypes: GroundActor, Structure
	Building:
		Footprint: xx xx ==
		Dimensions: 2,3
		LocalCenterOffset: 0,-512,0
	Production:
		Produces: Infantry, Soldier
	ProvidesPrerequisite:
		Prerequisite: barracks
	Exit@1:
		ExitCell: 1,2
		ProductionTypes: Soldier, Infantry
	Exit@2:
		ExitCell: 0,2
		ProductionTypes: Soldier, Infantry
harv:
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
	Targetable:
		TargetTypes: GroundActor, Infantry
	HitShape:
	BodyOrientation:
		QuantizedFacings: 8
	Mobile:
		Locomotor: foot
		Speed: 113
		TurnSpeed: 512
	Valued:
		Cost: 100
	Buildable:
		Queue: Infantry
		Prerequisites: ~barracks
		BuildDurationModifier: 60
)yaml"};

constexpr std::string_view kMapEconomy{
    "Title: t13-economy\n"
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
    "Actors:\n"
    "\tproc0: proc\n"
    "\t\tLocation: 11,13\n"
    "\t\tOwner: Multi0\n"
    "\tbarr0: barr\n"
    "\t\tLocation: 15,14\n"
    "\t\tOwner: Multi0\n"};

struct EconRun {
  std::string str_observations;
  ora::game::world::WorldModel w;
  bool b_ok{false};
  std::string str_error;
};

std::int32_t econ_view_id_of(const ora::game::world::WorldModel& w, std::uint32_t u4_actor) {
  if (u4_actor >= w.vec_actors_all.size()) return -1;
  const ora::game::world::ActorModel& actor{w.vec_actors_all[u4_actor]};
  std::int32_t int4_next{1};
  for (const auto& player : w.vec_players) {
    if (player.u4_actor_id == u4_actor) return player.b_spectating ? -1 : int4_next;
    if (!player.b_spectating) ++int4_next;
  }
  if (actor.str_type == "world") return 0;
  if (actor.str_type == "player") return -1;
  for (std::size_t idx{0}; idx < w.vec_actors_all.size(); ++idx) {
    const std::string& str_type{w.vec_actors_all[idx].str_type};
    if (str_type == "world" || str_type == "player") continue;
    if (static_cast<std::uint32_t>(idx) == u4_actor) return int4_next;
    ++int4_next;
  }
  return -1;
}

std::string init_actors_line(const ora::game::world::WorldModel& w) {
  std::string str_out;
  for (std::size_t idx{0}; idx < w.vec_actors_all.size(); ++idx) {
    const std::int32_t int4_view{econ_view_id_of(w, static_cast<std::uint32_t>(idx))};
    if (int4_view < 0) continue;
    if (!str_out.empty()) str_out += ' ';
    const auto& actor{w.vec_actors_all[idx]};
    const std::string str_owner{actor.u4_owner < w.vec_players.size()
                                    ? w.vec_players[actor.u4_owner].str_internal_name
                                    : "-"};
    str_out += std::format("{}:{}:{}", int4_view, actor.str_type, str_owner);
  }
  return str_out;
}

EconRun run_economy(const OreLordMap& ore) {
  EconRun run;
  auto res_map{ora::game::world::synthetic_map_from_yaml(kRulesEconomy, kMapEconomy)};
  if (!res_map.has_value()) {
    run.str_error = res_map.error();
    return run;
  }
  ora::game::production::LobbyModel lobby;
  lobby.vec_playable_factions.emplace_back("Multi0", "soviet");
  lobby.int4_starting_cash = 0;
  lobby.str_techlevel = "unrestricted";
  auto res_world{ora::game::world::world_create(*res_map, kSeed, lobby)};
  if (!res_world.has_value()) {
    run.str_error = res_world.error();
    return run;
  }
  run.w = std::move(*res_world);

  auto res_setup{eco::economy_setup(run.w, ore.bin_map, ore.tileset, kRulesEconomy, ore.int4_left,
                                    ore.int4_top, ore.int4_right, ore.int4_bottom)};
  if (!res_setup.has_value()) {
    run.str_error = res_setup.error();
    return run;
  }

  std::string& str_obs{run.str_observations};
  str_obs += std::format("map uid {}\n", ore.str_uid);
  str_obs += "world created actors=1\n";
  std::int32_t int4_players{0};
  for (const auto& player : run.w.vec_players)
    if (!player.b_spectating) ++int4_players;
  str_obs += std::format("players {}\n", int4_players);

  const auto& res_multi0{run.w.prod.vec_players[1].res};
  str_obs += std::format("init startcash {} ore-cap {} rescells {} rng {} cnt {}\n",
                         lobby.int4_starting_cash, res_multi0.int4_capacity,
                         run.w.econ.layer.int4_res_cells, run.w.rng_shared.int4_last,
                         run.w.rng_shared.int4_total_count);
  str_obs += std::format("init actors {}\n", init_actors_line(run.w));

  {
    std::int32_t int4_cells{};
    std::int32_t int4_sum{};
    std::vector<std::string> vec_type_order;
    std::map<std::string, std::array<std::int32_t, 3>> map_stats;
    for (const auto& cell : run.w.econ.layer.vec_cells) {
      if (cell.str_type.empty()) continue;
      ++int4_cells;
      int4_sum += cell.u1_density;
      auto it{map_stats.find(cell.str_type)};
      if (it == map_stats.end()) {
        vec_type_order.push_back(cell.str_type);
        it = map_stats.emplace(cell.str_type,
                               std::array<std::int32_t, 3>{0, cell.u1_density, cell.u1_density})
                 .first;
      }
      it->second[0] += 1;
      it->second[1] = std::min(it->second[1], static_cast<std::int32_t>(cell.u1_density));
      it->second[2] = std::max(it->second[2], static_cast<std::int32_t>(cell.u1_density));
    }
    str_obs += std::format("init field cells {} sumdensity {}\n", int4_cells, int4_sum);
    for (const std::string& str_type : vec_type_order) {
      const auto& arr_stats{map_stats[str_type]};
      std::int32_t int4_type_sum{};
      for (const auto& cell : run.w.econ.layer.vec_cells)
        if (cell.str_type == str_type) int4_type_sum += cell.u1_density;
      str_obs += std::format("init field {} n {} sum {} min {} max {}\n", str_type, arr_stats[0],
                             int4_type_sum, arr_stats[1], arr_stats[2]);
    }
  }

  ora::game::network::OrderManagerModel om;
  const std::vector<std::int32_t> vec_clients{1};
  ora::game::network::order_manager_start_game(om, run.w, vec_clients);

  bool b_ordered{false};
  std::int32_t int4_e1_tick{-1};
  std::string str_last_actor_line;
  for (;;) {
    ora::game::network::run_frame(om, run.w);
    str_obs += eco::economy_observation(run.w, 1);

    const std::string str_actor_line{eco::economy_actor_line(run.w)};
    if (str_actor_line != str_last_actor_line) {
      str_obs += std::format("A {} {}\n", run.w.int4_world_tick, str_actor_line);
      str_last_actor_line = str_actor_line;
    }

    if (int4_e1_tick < 0) {
      for (const std::uint32_t u4_id : run.w.vec_in_world) {
        if (run.w.vec_actors_all[u4_id].str_type != "e1") continue;
        int4_e1_tick = run.w.int4_world_tick;
        const auto& actor_e1{run.w.vec_actors_all[u4_id]};
        str_obs += std::format("E {} e1 spawned id {} at {},{}\n", run.w.int4_world_tick,
                               econ_view_id_of(run.w, u4_id), actor_e1.int4_pos_x,
                               actor_e1.int4_pos_y);
        break;
      }
    }

    if (!b_ordered && run.w.int4_world_tick == kOrderTick) {
      ora::game::network::OrderModel order;
      order.str_order = "StartProduction";
      order.b_has_subject = true;
      order.u4_subject = run.w.vec_players[1].u4_actor_id;
      order.opt_str_target = "e1";
      order.u4_extra_data = 1;
      ora::game::network::issue_order(om, order);
      str_obs += std::format("E {} order StartProduction e1 issued\n", run.w.int4_world_tick);
      b_ordered = true;
    }

    if (int4_e1_tick >= 0 && run.w.int4_world_tick >= int4_e1_tick + 20) break;
    if (run.w.int4_world_tick >= kMaxTicks) break;
  }

  const auto& res_final{run.w.prod.vec_players[1].res};
  str_obs += std::format("final tick {} cash {} ore {} rescells {} rng {} cnt {}\n",
                         run.w.int4_world_tick, res_final.int4_cash, res_final.int4_resources,
                         run.w.econ.layer.int4_res_cells, run.w.rng_shared.int4_last,
                         run.w.rng_shared.int4_total_count);
  run.b_ok = true;
  return run;
}

std::string observation_field(const std::string& str_obs, std::string_view str_line_prefix,
                              std::string_view str_key) {
  // 逐行扫描（行数 ~2700，简单直白优于嵌套 find 推进）。
  std::size_t sz_pos{0};
  while (sz_pos < str_obs.size()) {
    const std::size_t sz_end{str_obs.find('\n', sz_pos)};
    const std::size_t sz_stop{sz_end == std::string::npos ? str_obs.size() : sz_end};
    if (sz_stop >= sz_pos + str_line_prefix.size() &&
        std::string_view{str_obs}.substr(sz_pos, str_line_prefix.size()) == str_line_prefix) {
      const std::string_view str_line{std::string_view{str_obs}.substr(sz_pos, sz_stop - sz_pos)};
      const std::size_t idx_key{str_line.find(str_key, str_line_prefix.size())};
      if (idx_key != std::string_view::npos) {
        const std::size_t idx_value_start{idx_key + str_key.size()};
        const std::size_t idx_value_end{str_line.find(' ', idx_value_start)};
        return std::string{str_line.substr(
            idx_value_start,
            (idx_value_end == std::string_view::npos ? str_line.size() : idx_value_end) -
                idx_value_start)};
      }
    }
    if (sz_end == std::string::npos) break;
    sz_pos = sz_end + 1;
  }
  return {};
}

std::int32_t e_line_field(const std::string& str_obs, std::int32_t int4_tick,
                          std::string_view str_key) {
  const std::string str_prefix{std::format("E {} ", int4_tick)};
  const std::string str_value{
      observation_field(str_obs, str_prefix, str_key)};
  std::int32_t int4_out{-1};
  if (!str_value.empty())
    std::from_chars(str_value.data(), str_value.data() + str_value.size(), int4_out);
  return int4_out;
}

// —— 金额公式单元（Refinery.AcceptResources 逐行：value=count×ResourceValues，
//    UseStorage 仓限制 while 减 bale，溢出截断；矿石进 Resources 不进 Cash）——

void test_refinery_accept() {
  ora::game::world::WorldModel w;
  w.econ.layer.map_types.emplace("Ore", eco::ResourceTypeSpec{.str_name = "Ore",
                                                              .u1_index = 1, .str_terrain = "Ore",
                                                              .int4_max_density = 12,
                                                              .int4_value = 25});
  w.econ.layer.map_types.emplace("Gems", eco::ResourceTypeSpec{.str_name = "Gems",
                                                               .u1_index = 2,
                                                               .str_terrain = "Gems",
                                                               .int4_max_density = 3,
                                                               .int4_value = 50});
  w.prod.vec_players.resize(1);
  auto& res{w.prod.vec_players[0].res};

  res = {.int4_cash = 0, .int4_resources = 0, .int4_capacity = 2000};
  check(eco::refinery_accept(w, 0, "Ore", 1) == 1, "accept: 1 ore bale accepted");
  check(res.int4_resources == 25, "accept: 1 bale x 25 enters Resources (not Cash)");
  check(res.int4_cash == 0, "accept: Cash untouched when UseStorage");

  check(eco::refinery_accept(w, 0, "Gems", 2) == 2, "accept: 2 gem bales accepted");
  check(res.int4_resources == 125, "accept: gems value 2x50 added");

  // 仓限制减 bale：余量 5 装不下 25 → count 递减到 0（剩余留在车里）。
  res = {.int4_cash = 0, .int4_resources = 1995, .int4_capacity = 2000};
  check(eco::refinery_accept(w, 0, "Ore", 1) == 0, "accept: storage limit leaves bale in harv");
  check(res.int4_resources == 1995, "accept: nothing stored when 1 bale does not fit");
  // 余量 30：25 装得下（1 bale），2 bale 50 装不下 → 只接受 1。
  res.int4_resources = 1970;
  check(eco::refinery_accept(w, 0, "Ore", 2) == 1, "accept: while-loop drops to fitting bales");
  check(res.int4_resources == 1995, "accept: stored exactly the fitting bale value");

  // GiveResources 溢出截断（减 bale 循环已保证不溢出；直接边界不触发）。
  res = {.int4_cash = 0, .int4_resources = 0, .int4_capacity = 2000};
  check(eco::refinery_accept(w, 0, "Ore", 80) == 80, "accept: full load 80 bales accepted");
  check(res.int4_resources == 2000, "accept: 80x25=2000 exactly fills capacity");

  // DiscardExcessResources：value 截到 limit、bale 数不减。
  w.econ.cfg.b_discard_excess = true;
  res = {.int4_cash = 0, .int4_resources = 1990, .int4_capacity = 2000};
  check(eco::refinery_accept(w, 0, "Ore", 1) == 1, "accept: discard branch returns full count");
  check(res.int4_resources == 2000, "accept: discard truncates value to storage limit");
  w.econ.cfg.b_discard_excess = false;

  // UseStorage=false → 变现直接进 Cash。
  w.econ.cfg.b_use_storage = false;
  res = {.int4_cash = 100, .int4_resources = 0, .int4_capacity = 2000};
  check(eco::refinery_accept(w, 0, "Ore", 2) == 2, "accept: no-storage branch accepts");
  check(res.int4_cash == 150 && res.int4_resources == 0, "accept: no-storage pays into Cash");
  w.econ.cfg.b_use_storage = true;

  check(eco::refinery_accept(w, 0, "Unknown", 1) == 0, "accept: unknown resource type rejected");
}

// —— ResourceLayer.RemoveResource：密度 -1、归零清格（resCells--）——

eco::EconomyWorldState make_layer_fixture(std::int32_t int4_w, std::int32_t int4_h) {
  eco::EconomyWorldState econ;
  econ.map_move.int4_width = int4_w;
  econ.map_move.int4_height = int4_h;
  econ.map_move.int4_left = 0;
  econ.map_move.int4_top = 0;
  econ.map_move.int4_right = int4_w;
  econ.map_move.int4_bottom = int4_h;
  econ.map_move.int4_min_cost = 100;
  econ.map_move.vec2_cost.assign(
      static_cast<std::size_t>(int4_w) * static_cast<std::size_t>(int4_h), 100);
  econ.map_move.vec4_speed.assign(
      static_cast<std::size_t>(int4_w) * static_cast<std::size_t>(int4_h), 72);
  econ.layer.int4_width = int4_w;
  econ.layer.int4_height = int4_h;
  econ.layer.vec_cells.assign(static_cast<std::size_t>(int4_w) * static_cast<std::size_t>(int4_h),
                              eco::ResourceCellState{});
  return econ;
}

void test_resource_remove() {
  auto econ{make_layer_fixture(4, 4)};
  auto& layer{econ.layer};
  // 列主序（与 ResourceLayer 一致）：idx = x * height + y。
  layer.vec_cells[0 * 4 + 0] = eco::ResourceCellState{.str_type = "Ore", .u1_density = 3};
  layer.vec_cells[1 * 4 + 0] = eco::ResourceCellState{.str_type = "Ore", .u1_density = 1};
  layer.vec_cells[2 * 4 + 0] = eco::ResourceCellState{.str_type = "Gems", .u1_density = 2};
  layer.int4_res_cells = 3;

  check(eco::resource_remove(layer, econ, "Ore", {0, 0}) == 1,
        "remove: one unit harvested from density 3");
  check(layer.vec_cells[0].u1_density == 2, "remove: density decremented");
  check(layer.int4_res_cells == 3, "remove: cell not cleared above zero density");

  check(eco::resource_remove(layer, econ, "Ore", {1, 0}) == 1,
        "remove: last unit harvested from density 1");
  check(layer.vec_cells[1].str_type.empty() && layer.vec_cells[1].u1_density == 0,
        "remove: cell cleared at zero density");
  check(layer.int4_res_cells == 2, "remove: resCells decremented on clear");

  check(eco::resource_remove(layer, econ, "Ore", {2, 0}) == 0,
        "remove: wrong type rejected (Gems cell, Ore request)");
  check(eco::resource_remove(layer, econ, "Ore", {3, 3}) == 0,
        "remove: empty cell returns zero");
  check(econ.map_move.vec2_cost[0] == 100, "remove: CustomTerrain restored to base cost");
}

void test_cargo() {
  eco::HarvCargoState cargo{.int4_capacity = 20};
  cargo.vec_contents.emplace_back("Ore", 0);
  cargo.vec_contents.emplace_back("Gems", 0);

  eco::HarvState harv;
  harv.cargo = cargo;
  eco::EconomyWorldState econ;
  econ.cfg.int4_harv_capacity = 20;

  check(eco::harv_cargo_sum(cargo) == 0 && eco::harv_is_empty(harv), "cargo: starts empty");
  eco::cargo_add(cargo, "Ore", 1);
  check(eco::harv_cargo_sum(cargo) == 1 && cargo.vec_contents[0].second == 1,
        "cargo: add increments existing type slot");
  eco::cargo_add(cargo, "Ore", 19);
  harv.cargo = cargo;
  check(eco::harv_is_full(econ, harv), "cargo: full at Capacity 20");
  check(eco::harv_fullness(harv) == 100, "cargo: fullness 100 at full load");
  eco::cargo_add(cargo, "Gems", 10);
  check(eco::harv_fullness(harv) == 100, "cargo: fullness integer division ContentsSum*100/Cap");

  eco::HarvCargoState partial{.int4_capacity = 20};
  partial.vec_contents.emplace_back("Ore", 9);
  eco::HarvState harv2;
  harv2.cargo = partial;
  check(eco::harv_fullness(harv2) == 45, "cargo: 9/20 fullness 45");
}

// —— rng 账本断言：采集→变现全链 0 耗；E1 出厂恰 +1 ——

void test_rng_ledger(const EconRun& run) {
  check(e_line_field(run.str_observations, 1, "cnt ") == 0, "rng: count 0 at first tick");
  check(e_line_field(run.str_observations, 167, "cnt ") == 0, "rng: 0 through first bale");
  check(e_line_field(run.str_observations, 350, "cnt ") == 0, "rng: 0 through full load");
  check(e_line_field(run.str_observations, 581, "cnt ") == 0, "rng: 0 through dock arrival");
  check(e_line_field(run.str_observations, 600, "cnt ") == 0, "rng: 0 through unload start");
  check(e_line_field(run.str_observations, 658, "cnt ") == 0, "rng: 0 through e1 done tick");
  check(e_line_field(run.str_observations, 659, "cnt ") == 1, "rng: exactly +1 on exit frame");
  check(run.w.rng_shared.int4_total_count == 1, "rng: final shared count 1");
}

// —— 剧本锚点断言（黄金语义抽查；逐字节对拍见 test_golden_compare）——

void test_scenario_anchors(const EconRun& run) {
  check(run.w.econ.layer.int4_res_cells == 413, "anchors: final rescells 413");
  check(run.w.prod.vec_players[1].res.int4_resources == 400,
        "anchors: final ore 400 = 20 bales x25 - 100 e1");
  check(run.w.prod.vec_players[1].res.int4_cash == 0, "anchors: cash stays 0");
  check(e_line_field(run.str_observations, 167, "rescells ") == 417,
        "anchors: rescells 417 before first bale");
  check(e_line_field(run.str_observations, 350, "rescells ") == 413,
        "anchors: rescells 413 after clearing four cells");
  const std::string str_e1{observation_field(run.str_observations, "E 659 e1 ", "at ")};
  check(str_e1 == "15,16", "anchors: e1 spawns at barracks exit (15,16)");
  check(run.w.vec_actors_all.size() == 8 && run.w.u4_next_aid == 8,
        "anchors: 8 actors (world+3 player+proc+barr+harv+e1)");
}

void test_golden_compare(const OreLordMap& ore) {
  const std::string str_golden{read_golden("ore-lord.econ.txt")};
  check(!str_golden.empty(), "golden: ore-lord.econ.txt present and non-empty");
  if (str_golden.empty()) return;

  const EconRun run{run_economy(ore)};
  check(run.b_ok, std::format("golden: scenario runs ({})", run.str_error));
  if (!run.b_ok) return;

  if (run.str_observations == str_golden) {
    check(true, "golden: economy observation stream matches oracle byte-for-byte");
    return;
  }

  const std::size_t sz_max{std::min(run.str_observations.size(), str_golden.size())};
  std::size_t sz_diff{0};
  while (sz_diff < sz_max && run.str_observations[sz_diff] == str_golden[sz_diff]) ++sz_diff;
  const std::size_t sz_line_start{run.str_observations.rfind('\n', sz_diff) + 1};
  const std::size_t sz_gold_line_start{str_golden.rfind('\n', sz_diff) + 1};
  std::println("  first diff near offset {}:\n  cpp  {}\n  gold {}",
               sz_diff,
               run.str_observations.substr(sz_line_start, 200),
               str_golden.substr(std::min(sz_gold_line_start, str_golden.size()), 200));
  check(false, "golden: economy observation stream matches oracle byte-for-byte");
}

void test_determinism(const OreLordMap& ore) {
  const EconRun run_a{run_economy(ore)};
  const EconRun run_b{run_economy(ore)};
  check(run_a.b_ok && run_b.b_ok, "determinism: both runs succeed");
  check(run_a.str_observations == run_b.str_observations,
        "determinism: observation streams identical across runs");
  check(ora::game::world::state_digest(run_a.w) == ora::game::world::state_digest(run_b.w),
        "determinism: state digests identical across runs");
}

}  // namespace

int main() {
  const auto opt_ore{load_ore_lord()};
  check(opt_ore.has_value(), "ore-lord.oramap + temperat tileset load");
  if (opt_ore.has_value()) {
    check(opt_ore->str_uid == "b0a8d5bb32ba1eb4fa36b16a198db8b831b37f93",
          "ore-lord map uid golden anchor");

    test_refinery_accept();
    test_resource_remove();
    test_cargo();
    test_golden_compare(*opt_ore);

    const EconRun run{run_economy(*opt_ore)};
    if (run.b_ok) {
      test_rng_ledger(run);
      test_scenario_anchors(run);
      test_determinism(*opt_ore);
    } else {
      check(false, std::format("anchors: scenario runs ({})", run.str_error));
    }
  }

  std::println("economy: {} checks, {} failed", g_total, g_failed);
  return g_failed == 0 ? 0 : 1;
}
