import std;

#include <cstdio>

#include "game/combat/combat_sim.hpp"
#include "game/combat/projectile.hpp"
#include "game/filesystem/zip_read.hpp"
#include "game/map_bin.hpp"
#include "game/map_read.hpp"
#include "game/tileset.hpp"
#include "game/pathfinding/move_sim.hpp"
#include "game/pathfinding/path_graph.hpp"
#include "game/combat/health.hpp"
#include "game/combat/warhead_damage.hpp"
#include "game/combat/weapon.hpp"
#include "game/miniyaml.hpp"
#include "game/miniyaml_merge.hpp"

#ifndef ORA_COMBAT_GOLDEN_DIR
#define ORA_COMBAT_GOLDEN_DIR "tests/golden/combat"
#endif

namespace {

int int4_failures{0};
int int4_checks{0};
int int4_reports{0};

constexpr int kMaxReports{20};

void check(bool b_ok, std::string_view str_what) {
  ++int4_checks;
  if (!b_ok) {
    ++int4_failures;
    if (int4_reports < kMaxReports) {
      ++int4_reports;
      std::println("combat: {}", str_what);
    }
  }
}

std::optional<std::string> read_file_bytes(std::string_view str_path) {
  std::ifstream stream_in{std::filesystem::path{std::string(str_path)}, std::ios::binary};
  if (!stream_in) return std::nullopt;
  std::string str_bytes;
  char buf1_chunk[8192];
  while (stream_in.read(buf1_chunk, static_cast<std::streamsize>(sizeof buf1_chunk)) ||
         stream_in.gcount() > 0)
    str_bytes.append(buf1_chunk, static_cast<std::size_t>(stream_in.gcount()));
  return str_bytes;
}

std::vector<std::string> read_lines(const std::string& str_text) {
  std::vector<std::string> vec_lines;
  std::size_t sz_pos{0};
  while (sz_pos <= str_text.size()) {
    const std::size_t sz_end{str_text.find('\n', sz_pos)};
    if (sz_end == std::string::npos) {
      if (sz_pos < str_text.size()) vec_lines.push_back(str_text.substr(sz_pos));
      break;
    }
    std::string str_line{str_text.substr(sz_pos, sz_end - sz_pos)};
    if (!str_line.empty() && str_line.back() == '\r') str_line.pop_back();
    vec_lines.push_back(std::move(str_line));
    sz_pos = sz_end + 1;
  }
  return vec_lines;
}

std::optional<ora::game::combat::WeaponTable> load_weapon_table() {
  const auto str_manifest{
      read_file_bytes(std::format("{}/weapons-manifest.txt", ORA_COMBAT_GOLDEN_DIR))};
  if (!str_manifest.has_value()) return std::nullopt;

  std::vector<std::string> vec_texts;
  std::vector<std::string> vec_names;
  for (const std::string& str_entry : read_lines(*str_manifest)) {
    if (str_entry.empty()) continue;

    std::string str_rel{str_entry};
    const std::size_t idx_bar{str_rel.find('|')};
    if (idx_bar != std::string::npos) str_rel[idx_bar] = '_';
    const auto str_text{
        read_file_bytes(std::format("{}/weapons/{}", ORA_COMBAT_GOLDEN_DIR, str_rel))};
    if (!str_text.has_value()) return std::nullopt;
    vec_texts.push_back(*str_text);
    vec_names.push_back(str_entry);
  }

  auto res{ora::game::combat::weapon_table_load(vec_texts, vec_names)};
  if (!res.has_value()) {
    std::println("combat: weapon_table_load failed: {}", res.error());
    return std::nullopt;
  }
  return std::move(*res);
}

void test_weapon_table() {
  const auto opt_table{load_weapon_table()};
  check(opt_table.has_value(), "weapon table loads from golden");
  if (!opt_table.has_value()) return;
  const ora::game::combat::WeaponTable& table{*opt_table};

  check(table.vec_weapons.size() == 89,
        std::format("weapons loaded 89 (got {})", table.vec_weapons.size()));

  const ora::game::combat::WeaponDef* wh_m1{table.find("m1carbine")};
  const ora::game::combat::WeaponDef* wh_25{table.find("25mm")};
  const ora::game::combat::WeaponDef* wh_155{table.find("155mm")};
  check(wh_m1 != nullptr && wh_25 != nullptr && wh_155 != nullptr, "three weapons found");

  if (wh_m1 != nullptr) {
    check(wh_m1->int4_range == 5120, std::format("m1carbine range 5120 (got {})", wh_m1->int4_range));
    check(wh_m1->int4_reload_delay == 20, "m1carbine reload 20");
    check(wh_m1->int4_burst == 1, "m1carbine burst 1");
    check(wh_m1->projectile.kind == ora::game::combat::ProjectileKind::kInstantHit,
          "m1carbine projectile InstantHit");
    check(wh_m1->vec_warheads.size() == 7,
          std::format("m1carbine 7 warheads (got {})", wh_m1->vec_warheads.size()));
    if (wh_m1->vec_warheads.size() == 7) {

      const auto& wh1{wh_m1->vec_warheads[0]};
      check(wh1.kind == ora::game::combat::WarheadKind::kSpreadDamage, "m1 wh1 SpreadDamage");
      check(wh1.int4_damage == 1000, "m1 wh1 damage 1000");
      const std::vector<std::pair<std::string, int>> vec_expect{
          {"None", 150}, {"Wood", 30}, {"Light", 40}, {"Heavy", 10}, {"Concrete", 10}};
      check(wh1.vec_versus == vec_expect, "m1 wh1 versus order/values");

      const std::vector<int> vec_delays{0, 0, 2, 2, 4, 4};
      for (std::size_t idx{0}; idx < 6; ++idx) {
        const auto& whc{wh_m1->vec_warheads[idx + 1]};
        check(whc.kind == ora::game::combat::WarheadKind::kCreateEffect &&
                  whc.int4_delay == vec_delays[idx] && whc.int4_inaccuracy == 171,
              std::format("m1 wh{} CreateEffect delay {} inacc 171 (got kind {} delay {} inacc {})",
                          idx + 2, vec_delays[idx], static_cast<int>(whc.kind), whc.int4_delay,
                          whc.int4_inaccuracy));
      }
    }
  }

  if (wh_25 != nullptr) {
    check(wh_25->int4_range == 4864, "25mm range 4864");
    check(wh_25->int4_reload_delay == 21, "25mm reload 21");
    check(wh_25->projectile.kind == ora::game::combat::ProjectileKind::kBullet,
          "25mm projectile Bullet");
    check(wh_25->projectile.int4_speed == 853, "25mm speed 853");
    check(wh_25->vec_warheads.size() == 3, "25mm 3 warheads");
  }

  if (wh_155 != nullptr) {
    check(wh_155->int4_range == 12288, "155mm range 12288");
    check(wh_155->int4_reload_delay == 85, "155mm reload 85");
    check(wh_155->int4_min_range == 4096, "155mm minrange 4096");
    check(wh_155->b_target_actor_center, "155mm TargetActorCenter");
    check(wh_155->projectile.int4_speed == 170, "155mm speed 170");
    check(wh_155->projectile.vec_launch_angle.size() == 1 &&
              wh_155->projectile.vec_launch_angle[0] == 62,
          "155mm LaunchAngle 62");
    check(wh_155->projectile.int4_inaccuracy == 1162, "155mm Inaccuracy 1c138=1162");
    check(wh_155->vec_warheads.size() == 4, "155mm 4 warheads");
    if (wh_155->vec_warheads.size() == 4) {
      const auto& wh1{wh_155->vec_warheads[0]};
      check(wh1.vec_falloff == std::vector<int>{100, 55, 20, 5}, "155mm falloff 100,55,20,5");
      check(wh1.int4_spread == 426, "155mm spread 426");
      check(wh_155->vec_warheads[1].kind == ora::game::combat::WarheadKind::kLeaveSmudge,
            "155mm wh2 LeaveSmudge");
    }
  }
}

void test_damage_chains() {
  using ora::game::combat::DamagePacket;
  using ora::game::combat::HealthState;
  using ora::game::combat::apply_percentage_modifiers;
  using ora::game::combat::get_damage_falloff;

  check(apply_percentage_modifiers(100, {150}) == 150, "chain1 versus 100*150%");
  check(apply_percentage_modifiers(1000, {100, 150}) == 1500, "chain2 1000*100%*150%");

  check(apply_percentage_modifiers(2500, {100, 32}) == 800, "chain2 25mm 800");

  check(apply_percentage_modifiers(23000, {70, 60}) == 9660, "chain2 155mm 9660");

  check(apply_percentage_modifiers(10, {33}) == 3, "10*33% -> 3 (truncate)");
  check(apply_percentage_modifiers(10, {33, 33}) == 1, "10*33%*33% -> 1 (chain, no per-step)");

  check(apply_percentage_modifiers(100, {7}) == 7, "100*7% -> 7");

  check(apply_percentage_modifiers(100, {33, 33}) == 10, "chain1 versus 100*33%*33% -> 10");

  {
    HealthState hp;
    hp.int4_hp = 1000;
    hp.int4_max_hp = 1000;
    const DamagePacket pkt{1000};
    const int int4_applied{health_inflict(hp, pkt, std::vector<std::int32_t>{50, 50})};
    check(int4_applied == 250, "health victim chain 1000*50%*50% -> 250");
    check(hp.int4_hp == 750, "health hp 750 after 250");
  }

  {
    HealthState hp;
    hp.int4_hp = 100;
    hp.int4_max_hp = 5000;
    const DamagePacket pkt{23000};
    const int int4_applied{health_inflict(hp, pkt, {})};
    check(int4_applied == 100, "overkill clamps applied to hp");
    check(hp.int4_hp == 0, "hp floor 0");
    check(hp.b_dead, "dead flag set at hp 0");
  }

  {
    HealthState hp;
    hp.int4_hp = 0;
    hp.int4_max_hp = 5000;
    hp.b_dead = true;
    check(health_inflict(hp, DamagePacket{1500}, {}) == 0, "no damage after death");
    check(!hp.b_died_this_call, "kill not re-triggered after death");
  }

  check(get_damage_falloff(std::vector<int>{100, 55, 20, 5}, 426, 0) == 100, "falloff d=0 = 100");
  check(get_damage_falloff(std::vector<int>{100, 55, 20, 5}, 426, 128) == 87, "falloff d=128 lerp");
  check(get_damage_falloff(std::vector<int>{100, 55, 20, 5}, 426, 426) == 55, "falloff d=426 = 55");
  check(get_damage_falloff(std::vector<int>{100, 55, 20, 5}, 426, 853) == 20, "falloff d=853 = 20");
  check(get_damage_falloff(std::vector<int>{100, 55, 20, 5}, 426, 1279) == 0, "falloff beyond 0");
}

namespace ogf = ora::game::filesystem;
namespace ogm = ora::game;
namespace pfg = ora::game::pathfinding;

struct OreLordFootMap {
  ogm::MapBin bin_map;
  ogm::Tileset tileset;
  pfg::LocomotorMap locomotor;
};

std::optional<OreLordFootMap> load_ore_lord_foot() {
  const auto str_oramap{
      read_file_bytes(std::format("{}/ore-lord.oramap", ORA_MAPS_GOLDEN_DIR))};
  const auto str_tileset{
      read_file_bytes(std::format("{}/tilesets/temperat.yaml", ORA_MAPS_GOLDEN_DIR))};
  if (!str_oramap.has_value() || !str_tileset.has_value()) return std::nullopt;

  const auto res_zip{ogf::zip_read(std::vector<std::uint8_t>{str_oramap->begin(), str_oramap->end()})};
  if (!res_zip.has_value()) return std::nullopt;
  const auto vec_yaml{ogf::zip_entry_data(*res_zip, "map.yaml")};
  const auto vec_bin{ogf::zip_entry_data(*res_zip, "map.bin")};
  if (!vec_yaml.has_value() || !vec_bin.has_value()) return std::nullopt;

  const auto res_header{ogm::map_header_parse(*vec_yaml, "map.yaml")};
  if (!res_header.has_value()) return std::nullopt;

  OreLordFootMap ore;
  const auto res_tileset{ogm::tileset_load(*str_tileset, "temperat.yaml")};
  if (!res_tileset.has_value()) return std::nullopt;
  ore.tileset = std::move(*res_tileset);

  const auto res_bin{
      ogm::map_bin_parse(*vec_bin, res_header->int4_size_x, res_header->int4_size_y)};
  if (!res_bin.has_value()) return std::nullopt;
  ore.bin_map = std::move(*res_bin);

  constexpr std::string_view kFootYaml = R"yaml(world:
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
)yaml";
  const auto res_nodes{ogm::miniyaml_from_string(kFootYaml, "t12-foot-rules", true)};
  if (!res_nodes.has_value()) return std::nullopt;
  const ogm::MiniYamlNode* node_world{nullptr};
  for (const ogm::MiniYamlNode& node : *res_nodes)
    if (node.str_key == "world") node_world = &node;
  if (node_world == nullptr) return std::nullopt;
  const ogm::MiniYamlNode* node_locomotor{nullptr};
  for (const ogm::MiniYamlNode& node : node_world->vec_children)
    if (node.str_key == "Locomotor@FOOT") node_locomotor = &node;
  if (node_locomotor == nullptr) return std::nullopt;
  const auto res_def{pfg::locomotor_def_from_node(*node_locomotor)};
  if (!res_def.has_value()) return std::nullopt;

  ore.locomotor = pfg::locomotor_map_build(
      ore.bin_map, ore.tileset, *res_def, res_header->int4_bounds_x, res_header->int4_bounds_y,
      res_header->int4_bounds_x + res_header->int4_bounds_w,
      res_header->int4_bounds_y + res_header->int4_bounds_h);
  return ore;
}

struct DuelSpec {
  std::string str_name;
  std::string str_type_a;
  ora::game::pathfinding::CPos cell_a;
  std::string str_type_b;
  ora::game::pathfinding::CPos cell_b;
  bool b_mutual;
  int int4_ticks;
};

std::vector<std::string> run_duel(const ora::game::combat::WeaponTable& table,
                                  const DuelSpec& spec) {
  namespace cbt = ora::game::combat;

  cbt::Arena arena;
  arena.ptr_weapons = &table;
  arena.rng_shared = ora::game::support::make_mersenne_twister(0);
  static const std::optional<OreLordFootMap> opt_ore{load_ore_lord_foot()};
  if (opt_ore.has_value()) arena.ptr_locomotor_foot = &opt_ore->locomotor;

  auto res_a{cbt::arena_spawn(arena, spec.str_type_a, 0, spec.cell_a)};
  auto res_b{cbt::arena_spawn(arena, spec.str_type_b, 1, spec.cell_b)};
  if (!res_a.has_value() || !res_b.has_value()) return {"SPAWN FAILED"};

  cbt::CombatUnit& unit_a{*cbt::arena_unit(arena, *res_a)};
  cbt::CombatUnit& unit_b{*cbt::arena_unit(arena, *res_b)};

  std::vector<std::string> vec_out;
  vec_out.push_back(std::format("spawn a id {} type {} maxhp {}", unit_a.u4_id, unit_a.str_type,
                                unit_a.health.int4_max_hp));
  vec_out.push_back(std::format("spawn b id {} type {} maxhp {}", unit_b.u4_id, unit_b.str_type,
                                unit_b.health.int4_max_hp));

  cbt::arena_attack_order(arena, unit_a.u4_id, unit_b.u4_id);
  if (spec.b_mutual) cbt::arena_attack_order(arena, unit_b.u4_id, unit_a.u4_id);

  int int4_prev_hp_a{unit_a.health.int4_max_hp};
  int int4_prev_hp_b{unit_b.health.int4_max_hp};
  bool b_prev_in_world_a{true};
  bool b_prev_in_world_b{true};

  for (int idx{0}; idx < spec.int4_ticks; ++idx) {
    cbt::arena_tick(arena);

    const int int4_hp_a{unit_a.b_in_world ? unit_a.health.int4_hp : 0};
    const int int4_hp_b{unit_b.b_in_world ? unit_b.health.int4_hp : 0};

    if (int4_hp_a != int4_prev_hp_a) {
      vec_out.push_back(
          std::format("hit tick {} victim a hp {}->{}", arena.int4_world_tick, int4_prev_hp_a,
                      int4_hp_a));
      int4_prev_hp_a = int4_hp_a;
    }
    if (int4_hp_b != int4_prev_hp_b) {
      vec_out.push_back(
          std::format("hit tick {} victim b hp {}->{}", arena.int4_world_tick, int4_prev_hp_b,
                      int4_hp_b));
      int4_prev_hp_b = int4_hp_b;
    }
    if (int4_hp_a == 0 && b_prev_in_world_a) {
      vec_out.push_back(std::format("death tick {} actor a", arena.int4_world_tick));
      b_prev_in_world_a = false;
    }
    if (int4_hp_b == 0 && b_prev_in_world_b) {
      vec_out.push_back(std::format("death tick {} actor b", arena.int4_world_tick));
      b_prev_in_world_b = false;
    }
    if (unit_a.b_in_world != b_prev_in_world_a) {
      vec_out.push_back(std::format("removed tick {} actor a", arena.int4_world_tick));
      b_prev_in_world_a = unit_a.b_in_world;
    }
    if (unit_b.b_in_world != b_prev_in_world_b) {
      vec_out.push_back(std::format("removed tick {} actor b", arena.int4_world_tick));
      b_prev_in_world_b = unit_b.b_in_world;
    }

    vec_out.push_back(cbt::arena_dump_row(arena, unit_a.u4_id, unit_b.u4_id));

    for (const cbt::Effect& eff : arena.vec_effects) {
      if (eff.kind != cbt::Effect::Kind::kBullet) continue;
      vec_out.push_back(std::format("proj tick {} pos {},{},{} src {},{},{} tgt {},{},{}",
                                    arena.int4_world_tick, eff.pos.int4_x, eff.pos.int4_y,
                                    eff.pos.int4_z, eff.source.int4_x, eff.source.int4_y,
                                    eff.source.int4_z, eff.target.int4_x, eff.target.int4_y,
                                    eff.target.int4_z));
    }

    if (!unit_a.b_in_world && !unit_b.b_in_world && arena.vec_effects.empty()) break;
  }

  return vec_out;
}

void test_duels() {
  namespace pfg = ora::game::pathfinding;
  const auto str_golden{read_file_bytes(std::format("{}/duels.txt", ORA_COMBAT_GOLDEN_DIR))};
  check(str_golden.has_value(), "duels.txt golden readable");
  if (!str_golden.has_value()) return;

  const auto opt_table{load_weapon_table()};
  check(opt_table.has_value(), "weapon table for duels");
  if (!opt_table.has_value()) return;

  const std::vector<DuelSpec> vec_specs{
      DuelSpec{"duelA-instant-mutual", "rifleman", pfg::CPos{30, 30}, "rifleman",
               pfg::CPos{30, 33}, true, 90},
      DuelSpec{"duelB-bullet-turret", "tank", pfg::CPos{30, 30}, "rifleman", pfg::CPos{30, 33},
               false, 220},
      DuelSpec{"duelC-arc-spread", "arty", pfg::CPos{30, 30}, "rifleman", pfg::CPos{30, 38},
               false, 280},
      DuelSpec{"duelD-approach", "rifleman", pfg::CPos{20, 20}, "rifleman", pfg::CPos{20, 26},
               false, 600},
  };

  std::map<std::string, std::vector<std::string>> map_golden;
  std::string str_current;
  for (const std::string& str_line : read_lines(*str_golden)) {
    if (str_line.starts_with("scenario ")) {
      str_current = str_line.substr(9);
      map_golden[str_current];
    } else if (!str_current.empty() && (str_line.starts_with("spawn ") ||
                                        str_line.starts_with("hit ") ||
                                        str_line.starts_with("death ") ||
                                        str_line.starts_with("removed ") ||
                                        str_line.starts_with("tick ") ||
                                        str_line.starts_with("proj "))) {
      map_golden[str_current].push_back(str_line);
    }
  }

  for (const DuelSpec& spec : vec_specs) {

    const std::vector<std::string> vec_run1{run_duel(*opt_table, spec)};
    const std::vector<std::string> vec_run2{run_duel(*opt_table, spec)};
    check(vec_run1 == vec_run2, std::format("{}: double-run identical", spec.str_name));

    const auto it_gold{map_golden.find(spec.str_name)};
    check(it_gold != map_golden.end(), std::format("{}: golden section found", spec.str_name));
    if (it_gold == map_golden.end()) continue;
    const std::vector<std::string>& vec_gold{it_gold->second};

    std::size_t idx_gold{0};
    std::int32_t int4_mismatches{0};
    for (std::size_t idx{0}; idx < vec_run1.size(); ++idx) {
      if (idx_gold >= vec_gold.size()) {
        if (int4_mismatches < 6)
          check(false, std::format("{}: sim produced more lines than golden at {}", spec.str_name,
                                   idx));
        ++int4_mismatches;
        continue;
      }
      if (vec_run1[idx] != vec_gold[idx_gold]) {
        if (int4_mismatches < 6)
          check(false, std::format("{} line {} mismatch: SIM[{}] GOLD[{}]",
                                   spec.str_name, idx, vec_run1[idx], vec_gold[idx_gold]));
        ++int4_mismatches;
      }
      ++idx_gold;
    }
    if (idx_gold != vec_gold.size() && int4_mismatches == 0)
      check(false, std::format("{}: consumed {} of {} golden lines", spec.str_name, idx_gold,
                               vec_gold.size()));
    if (int4_mismatches == 0 && idx_gold == vec_gold.size())
      check(true, std::format("{}: {} lines match golden", spec.str_name, idx_gold));
  }
}
}

int main() {
  test_weapon_table();
  test_damage_chains();
  test_duels();

  std::fflush(nullptr);
  if (int4_failures != 0) {
    std::println("combat: {} of {} checks failed", int4_failures, int4_checks);
    return 1;
  }
  std::println("combat: all {} checks passed", int4_checks);
  return 0;
}
