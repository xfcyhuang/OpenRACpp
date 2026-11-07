import std;

#include "game/filesystem/zip_read.hpp"
#include "game/map_bin.hpp"
#include "game/map_read.hpp"
#include "game/miniyaml.hpp"
#include "game/pathfinding/astar.hpp"
#include "game/pathfinding/path_graph.hpp"
#include "game/pathfinding/priority_queue.hpp"
#include "game/tileset.hpp"

#ifndef ORA_PATHFINDING_GOLDEN_DIR
#define ORA_PATHFINDING_GOLDEN_DIR "tests/golden/pathfinding"
#endif

#ifndef ORA_MAPS_GOLDEN_DIR
#define ORA_MAPS_GOLDEN_DIR "tests/golden/maps"
#endif

namespace {

namespace pfg = ora::game::pathfinding;
namespace ogf = ora::game::filesystem;
namespace ogm = ora::game;

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
      std::println("pathfind: {}", str_what);
    }
  }
}

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

struct GoldenPathCase {
  std::string str_tag;
  std::vector<pfg::CPos> vec_cells;
};

struct GoldenPaths {
  std::string str_uid;
  std::int32_t int4_width{};
  std::int32_t int4_height{};
  std::int32_t int4_left{};
  std::int32_t int4_top{};
  std::int32_t int4_right{};
  std::int32_t int4_bottom{};
  std::vector<std::string> vec_costmap;
  std::vector<GoldenPathCase> vec_astar;
  std::vector<GoldenPathCase> vec_hpf;
};

std::vector<std::string_view> split_tokens(std::string_view str_text) {
  std::vector<std::string_view> vec_tokens;
  std::size_t sz_pos{0};
  while (sz_pos < str_text.size()) {
    std::size_t sz_next{str_text.find(' ', sz_pos)};
    if (sz_next == std::string_view::npos) sz_next = str_text.size();
    if (sz_next > sz_pos) vec_tokens.push_back(str_text.substr(sz_pos, sz_next - sz_pos));
    sz_pos = sz_next + 1;
  }
  return vec_tokens;
}

std::optional<std::pair<std::int32_t, std::int32_t>> parse_cell(std::string_view str_token) {
  const std::size_t idx_comma{str_token.find(',')};
  if (idx_comma == std::string_view::npos) return std::nullopt;
  std::int32_t int4_x{};
  std::int32_t int4_y{};
  const auto res_x{std::from_chars(str_token.data(), str_token.data() + idx_comma, int4_x)};
  const auto res_y{std::from_chars(str_token.data() + idx_comma + 1, str_token.data() + str_token.size(), int4_y)};
  if (res_x.ec != std::errc{} || res_y.ec != std::errc{} || res_y.ptr != str_token.data() + str_token.size())
    return std::nullopt;
  return std::pair{int4_x, int4_y};
}

std::vector<std::int32_t> parse_quadruple(std::string_view str_token) {
  std::vector<std::int32_t> vec_values;
  std::size_t sz_pos{0};
  while (sz_pos <= str_token.size()) {
    const std::size_t sz_comma{str_token.find(',', sz_pos)};
    const std::size_t sz_end{sz_comma == std::string_view::npos ? str_token.size() : sz_comma};
    std::int32_t int4_value{};
    const auto res{std::from_chars(str_token.data() + sz_pos, str_token.data() + sz_end, int4_value)};
    if (res.ec != std::errc{} || res.ptr != str_token.data() + sz_end) break;
    vec_values.push_back(int4_value);
    if (sz_comma == std::string_view::npos) break;
    sz_pos = sz_comma + 1;
  }
  return vec_values;
}

std::optional<GoldenPaths> parse_golden_paths(const std::string& str_text) {
  GoldenPaths gold;
  bool b_in_costmap{false};
  for (const std::string& str_line : read_lines(str_text)) {
    if (str_line.empty()) continue;
    const std::vector<std::string_view> vec_tokens{split_tokens(str_line)};

    if (str_line[0] == '#' && vec_tokens.size() > 1) continue;

    if (b_in_costmap) {
      if (!vec_tokens.empty() && vec_tokens[0] == "astar") {
        b_in_costmap = false;
      } else if (!vec_tokens.empty() && vec_tokens[0] == "hpf") {
        b_in_costmap = false;
      } else if (!str_line.empty() && str_line.find(' ') == std::string::npos) {
        gold.vec_costmap.push_back(str_line);
        continue;
      } else {
        b_in_costmap = false;
      }
    }

    if (vec_tokens.empty()) continue;
    if (vec_tokens[0] == "map" && vec_tokens.size() >= 2 && vec_tokens[1] == "uid") {
      gold.str_uid = std::string(vec_tokens[2]);
    } else if (vec_tokens[0] == "map" && vec_tokens[1] == "size") {
      const auto opt_size{parse_cell(vec_tokens[2])};
      const std::vector<std::int32_t> vec_bounds{parse_quadruple(vec_tokens[4])};
      if (!opt_size.has_value() || vec_bounds.size() != 4)
        return std::nullopt;
      gold.int4_width = opt_size->first;
      gold.int4_height = opt_size->second;
      gold.int4_left = vec_bounds[0];
      gold.int4_top = vec_bounds[1];
      gold.int4_right = vec_bounds[2];
      gold.int4_bottom = vec_bounds[3];
    } else if (vec_tokens[0] == "costmap") {
      b_in_costmap = true;
    } else if (vec_tokens[0] == "astar" && vec_tokens.size() >= 4) {
      GoldenPathCase cs;
      cs.str_tag = std::string(vec_tokens[1]) + " " + std::string(vec_tokens[2]);
      for (std::size_t idx{4}; idx < vec_tokens.size(); ++idx) {
        const auto opt_cell{parse_cell(vec_tokens[idx])};
        if (!opt_cell.has_value()) return std::nullopt;
        cs.vec_cells.push_back(pfg::CPos{opt_cell->first, opt_cell->second});
      }
      gold.vec_astar.push_back(std::move(cs));
    } else if (vec_tokens[0] == "hpf" && vec_tokens.size() >= 4) {
      GoldenPathCase cs;
      cs.str_tag = std::string(vec_tokens[1]) + " " + std::string(vec_tokens[2]);
      for (std::size_t idx{4}; idx < vec_tokens.size(); ++idx) {
        const auto opt_cell{parse_cell(vec_tokens[idx])};
        if (!opt_cell.has_value()) return std::nullopt;
        cs.vec_cells.push_back(pfg::CPos{opt_cell->first, opt_cell->second});
      }
      gold.vec_hpf.push_back(std::move(cs));
    }
  }
  return gold;
}

void run_queue_unit_tests() {
  {
    pfg::LevelOrderQueue q;
    check(pfg::queue_empty(q), "queue: fresh queue empty");
    pfg::queue_add(q, {pfg::CPos{1, 1}, 5});
    check(!pfg::queue_empty(q), "queue: non-empty after add");
    const pfg::GraphConnection item{pfg::queue_pop(q)};
    check(item.int4_cost == 5 && item.cell.int4_x == 1, "queue: single add/pop roundtrip");
    check(pfg::queue_empty(q), "queue: empty after pop");
  }

  {
    pfg::LevelOrderQueue q;
    const int vec_costs[]{3, 1, 2};
    for (const int int4_cost : vec_costs)
      pfg::queue_add(q, {pfg::CPos{int4_cost, 0}, int4_cost});
    const int vec_expected[]{1, 2, 3};
    for (const int int4_expect : vec_expected) {
      const pfg::GraphConnection item{pfg::queue_pop(q)};
      check(item.int4_cost == int4_expect, "queue: min-heap order for 3,1,2");
    }
    check(pfg::queue_empty(q), "queue: drained after 3,1,2");
  }

  {
    pfg::LevelOrderQueue q;
    pfg::queue_add(q, {pfg::CPos{0, 0}, 5});
    pfg::queue_add(q, {pfg::CPos{1, 0}, 5});
    pfg::queue_add(q, {pfg::CPos{2, 0}, 4});
    const pfg::GraphConnection item1{pfg::queue_pop(q)};
    check(item1.int4_cost == 4 && item1.cell.int4_x == 2, "queue: equal-cost pop 1 is the 4");
    const pfg::GraphConnection item2{pfg::queue_pop(q)};
    check(item2.int4_cost == 5 && item2.cell.int4_x == 0, "queue: equal-cost pop 2 is first-added 5");
    const pfg::GraphConnection item3{pfg::queue_pop(q)};
    check(item3.int4_cost == 5 && item3.cell.int4_x == 1, "queue: equal-cost pop 3 is second-added 5");
    check(pfg::queue_empty(q), "queue: drained after equal-cost triple");
  }

  {
    pfg::LevelOrderQueue q;
    pfg::queue_add(q, {pfg::CPos{7, 0}, 7});
    pfg::queue_add(q, {pfg::CPos{2, 0}, 2});
    check(pfg::queue_pop(q).int4_cost == 2, "queue: interleaved pop 2");
    pfg::queue_add(q, {pfg::CPos{9, 0}, 9});
    pfg::queue_add(q, {pfg::CPos{1, 0}, 1});
    check(pfg::queue_pop(q).int4_cost == 1, "queue: interleaved pop 1");
    check(pfg::queue_pop(q).int4_cost == 7, "queue: interleaved pop 7");
    check(pfg::queue_pop(q).int4_cost == 9, "queue: interleaved pop 9");
    check(pfg::queue_empty(q), "queue: drained interleaved");
  }
}

struct OreLordMap {
  ogm::MapBin bin_map;
  ogm::Tileset tileset;
  std::int32_t int4_left{};
  std::int32_t int4_top{};
  std::int32_t int4_right{};
  std::int32_t int4_bottom{};
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

constexpr std::string_view kRulesYaml = R"yaml(world:
    Locomotor@TRACKED:
        Name: tracked
        TerrainSpeeds:
            Clear: 100
            Rough: 88
            Road: 125
            Bridge: 125
            Ore: 88
            Gems: 88
            Beach: 88
)yaml";

std::optional<pfg::LocomotorMap> build_tracked_map(const OreLordMap& ore) {
  const auto res_nodes{ogm::miniyaml_from_string(kRulesYaml, "t11-rules", true)};
  if (!res_nodes.has_value()) return std::nullopt;
  const ogm::MiniYamlNode* node_world{nullptr};
  const ogm::MiniYamlNode* node_locomotor{nullptr};
  for (const ogm::MiniYamlNode& node : *res_nodes) {
    if (node.str_key == "world") node_world = &node;
  }
  if (node_world == nullptr) return std::nullopt;
  for (const ogm::MiniYamlNode& node : node_world->vec_children)
    if (node.str_key == "Locomotor@TRACKED") node_locomotor = &node;
  if (node_locomotor == nullptr) return std::nullopt;

  const auto res_def{pfg::locomotor_def_from_node(*node_locomotor)};
  if (!res_def.has_value()) return std::nullopt;
  return pfg::locomotor_map_build(ore.bin_map, ore.tileset, *res_def, ore.int4_left, ore.int4_top,
                                  ore.int4_right, ore.int4_bottom);
}

std::string format_cell_list(const std::vector<pfg::CPos>& vec_cells) {
  std::string str_out;
  for (const pfg::CPos& cell : vec_cells) {
    if (!str_out.empty()) str_out += ' ';
    str_out += std::format("{},{}", cell.int4_x, cell.int4_y);
  }
  return str_out;
}

void run_golden_paths_check(const pfg::LocomotorMap& map, const GoldenPaths& gold) {
  std::vector<std::string> vec_cost_rows;
  for (std::int32_t int4_y{map.int4_top}; int4_y < map.int4_bottom; ++int4_y) {
    std::string str_row;
    for (std::int32_t int4_x{map.int4_left}; int4_x < map.int4_right; ++int4_x) {
      const std::int16_t int2_cost{pfg::movement_cost_for_cell(map, pfg::CPos{int4_x, int4_y})};
      const char ch_cost{int2_cost == pfg::kCostUnreachable
                             ? '#'
                             : int2_cost == 100 ? '.'
                             : int2_cost == 88 ? 'r'
                             : int2_cost == 80 ? 'o'
                             : int2_cost == 113 ? 'R'
                             : '?'};
      str_row.push_back(ch_cost);
    }
    vec_cost_rows.push_back(std::move(str_row));
  }
  check(vec_cost_rows.size() == gold.vec_costmap.size(),
        std::format("costmap: row count {} vs golden {}", vec_cost_rows.size(),
                    gold.vec_costmap.size()));
  for (std::size_t idx{0}; idx < vec_cost_rows.size() && idx < gold.vec_costmap.size(); ++idx)
    check(vec_cost_rows[idx] == gold.vec_costmap[idx],
          std::format("costmap: row {} differs | got {} | want {}", idx,
                      vec_cost_rows[idx], gold.vec_costmap[idx]));

  struct PairSpec {
    pfg::CPos cell_from;
    pfg::CPos cell_to;
    std::string_view str_tag;
  };
  const PairSpec vec_pairs[]{
      PairSpec{pfg::CPos{10, 17}, pfg::CPos{35, 17}, "straight"},
      PairSpec{pfg::CPos{10, 17}, pfg::CPos{35, 25}, "diagonal"},
      PairSpec{pfg::CPos{55, 8}, pfg::CPos{8, 55}, "cross"},
      PairSpec{pfg::CPos{10, 17}, pfg::CPos{50, 45}, "longdiag"},
      PairSpec{pfg::CPos{30, 3}, pfg::CPos{30, 58}, "vertical"},
      PairSpec{pfg::CPos{10, 17}, pfg::CPos{11, 18}, "adjacent"},
      PairSpec{pfg::CPos{10, 17}, pfg::CPos{10, 17}, "samecell"},
      PairSpec{pfg::CPos{10, 17}, pfg::CPos{0, 0}, "outofbounds"},
      PairSpec{pfg::CPos{10, 17}, pfg::CPos{5, 3}, "targetimpassable"},
      PairSpec{pfg::CPos{5, 3}, pfg::CPos{10, 10}, "sourceimpassable"},
      PairSpec{pfg::CPos{55, 8}, pfg::CPos{50, 13}, "nearpair"},
      PairSpec{pfg::CPos{35, 17}, pfg::CPos{12, 12}, "movepair"},
      PairSpec{pfg::CPos{35, 17}, pfg::CPos{28, 10}, "moveturn"},
  };

  std::map<std::string, std::vector<pfg::CPos>> map_gold_astar;
  for (const GoldenPathCase& cs : gold.vec_astar) map_gold_astar[cs.str_tag] = cs.vec_cells;
  check(map_gold_astar.size() == gold.vec_astar.size(), "astar: golden tags unique");
  check(gold.vec_astar.size() == 27, std::format("astar: golden case count {}", gold.vec_astar.size()));

  for (const PairSpec& spec : vec_pairs) {
    for (const std::int32_t int4_weight : {100, 125}) {
      const std::string str_tag{std::format("{} w{}", spec.str_tag, int4_weight)};
      const auto it_gold{map_gold_astar.find(str_tag)};
      if (it_gold == map_gold_astar.end()) {
        check(false, std::format("astar: golden lacks case {}", str_tag));
        continue;
      }
      const std::vector<pfg::CPos> vec_path{pfg::astar_to_target_cell(
          map, spec.cell_from, spec.cell_to, int4_weight, true)};
      check(vec_path.size() == it_gold->second.size() &&
                format_cell_list(vec_path) == format_cell_list(it_gold->second),
            std::format("astar {} path mismatch: got [{}] want [{}]", str_tag,
                        format_cell_list(vec_path), format_cell_list(it_gold->second)));
    }
  }

  {
    const auto it_gold{map_gold_astar.find("straight-nolane w100")};
    if (it_gold == map_gold_astar.end()) {
      check(false, "astar: golden lacks straight-nolane w100");
    } else {
      const std::vector<pfg::CPos> vec_path{pfg::astar_to_target_cell(
          map, pfg::CPos{10, 17}, pfg::CPos{35, 17}, 100, false)};
      check(format_cell_list(vec_path) == format_cell_list(it_gold->second),
            std::format("astar straight-nolane w100 mismatch: got [{}] want [{}]",
                        format_cell_list(vec_path), format_cell_list(it_gold->second)));
    }
  }
}

void run_hpf_cross_check(const GoldenPaths& gold) {
  std::map<std::string, std::vector<pfg::CPos>> map_gold;
  for (const GoldenPathCase& cs : gold.vec_hpf) map_gold[cs.str_tag] = cs.vec_cells;

  struct CrossSpec {
    std::string_view str_hpf;
    std::string_view str_astar;
  };
  const CrossSpec vec_cross[]{
      CrossSpec{"straight none", "straight w125"},
      CrossSpec{"straight all", "straight w125"},
      CrossSpec{"cross none", "cross w125"},
      CrossSpec{"cross all", "cross w125"},
      CrossSpec{"longdiag none", "longdiag w125"},
      CrossSpec{"longdiag all", "longdiag w125"},
      CrossSpec{"vertical none", "vertical w125"},
      CrossSpec{"vertical all", "vertical w125"},
      CrossSpec{"adjacent none", "adjacent w100"},
      CrossSpec{"adjacent all", "adjacent w100"},
      CrossSpec{"outofbounds none", "outofbounds w100"},
      CrossSpec{"outofbounds all", "outofbounds w100"},
      CrossSpec{"targetimpassable none", "targetimpassable w100"},
      CrossSpec{"targetimpassable all", "targetimpassable w100"},
      CrossSpec{"unreachable none", "sourceimpassable w100"},
      CrossSpec{"unreachable all", "sourceimpassable w100"},
      CrossSpec{"nearpair none", "nearpair w100"},
      CrossSpec{"nearpair all", "nearpair w100"},
      CrossSpec{"moveturn none", "moveturn w125"},
      CrossSpec{"moveturn all", "moveturn w125"},

  };

  std::map<std::string, std::vector<pfg::CPos>> map_gold_astar;
  for (const GoldenPathCase& cs : gold.vec_astar) map_gold_astar[cs.str_tag] = cs.vec_cells;

  for (const CrossSpec& spec : vec_cross) {
    const auto it_hpf{map_gold.find(std::string(spec.str_hpf))};
    const auto it_astar{map_gold_astar.find(std::string(spec.str_astar))};
    if (it_hpf == map_gold.end() || it_astar == map_gold_astar.end()) {
      check(false, std::format("hpf cross: golden lacks {} or {}", spec.str_hpf, spec.str_astar));
      continue;
    }
    check(format_cell_list(it_hpf->second) == format_cell_list(it_astar->second),
          std::format("hpf cross: {} != {} (oracle HPF layer degenerates to local A* on this "
                      "actor-free map)",
                      spec.str_hpf, spec.str_astar));
  }
}

}

int main() {
  run_queue_unit_tests();

  const auto str_golden{read_file_bytes(std::format("{}/ore-lord.paths.txt", ORA_PATHFINDING_GOLDEN_DIR))};
  if (!str_golden.has_value()) {
    std::println("pathfind: golden paths file missing");
    return 1;
  }
  const auto opt_gold{parse_golden_paths(*str_golden)};
  if (!opt_gold.has_value()) {
    std::println("pathfind: golden paths file parse error");
    return 1;
  }
  check(opt_gold->vec_costmap.size() == 61, "costmap: golden row count 61");
  check(opt_gold->vec_hpf.size() == 24, std::format("hpf: golden case count {}", opt_gold->vec_hpf.size()));

  const auto opt_ore{load_ore_lord()};
  if (!opt_ore.has_value()) {
    std::println("pathfind: ore-lord.oramap load failed");
    return 1;
  }
  const auto opt_map{build_tracked_map(*opt_ore)};
  if (!opt_map.has_value()) {
    std::println("pathfind: tracked locomotor map build failed");
    return 1;
  }

  run_golden_paths_check(*opt_map, *opt_gold);
  run_hpf_cross_check(*opt_gold);

  if (int4_failures != 0) {
    std::println("pathfind: {} of {} checks failed", int4_failures, int4_checks);
    return 1;
  }
  std::println("pathfind: all {} checks passed", int4_checks);
  return 0;
}
