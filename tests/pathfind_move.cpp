import std;

#include <cstdio>

#include "game/filesystem/zip_read.hpp"
#include "game/map_bin.hpp"
#include "game/map_read.hpp"
#include "game/miniyaml.hpp"
#include "game/pathfinding/move_sim.hpp"
#include "game/pathfinding/path_graph.hpp"
#include "game/tileset.hpp"
#include "test_util.hpp"
using ora::test::check;

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

struct TickRow {
  std::int32_t int4_tick{};
  pfg::CPos cell_from{};
  pfg::CPos cell_to{};
  std::int32_t int4_facing{};
  pfg::WPos pos{};
  std::int32_t int4_rng_last{};
  std::int32_t int4_rng_count{};
  bool b_idle{};
};

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

std::optional<std::int32_t> parse_dec(std::string_view str_token) {
  std::int32_t int4_value{};
  const auto res{std::from_chars(str_token.data(), str_token.data() + str_token.size(), int4_value)};
  if (res.ec != std::errc{} || res.ptr != str_token.data() + str_token.size())
    return std::nullopt;
  return int4_value;
}

std::optional<std::array<std::int32_t, 3>> parse_triple(std::string_view str_token) {
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
  if (vec_values.size() != 3) return std::nullopt;
  return std::array<std::int32_t, 3>{vec_values[0], vec_values[1], vec_values[2]};
}

std::optional<TickRow> parse_tick_row(std::string_view str_line) {
  const std::vector<std::string_view> vec_tokens{split_tokens(str_line)};
  if (vec_tokens.size() != 16) return std::nullopt;
  TickRow row;
  const auto opt_tick{parse_dec(vec_tokens[1])};
  const auto opt_from{parse_cell(vec_tokens[3])};
  const auto opt_to{parse_cell(vec_tokens[5])};
  const auto opt_facing{parse_dec(vec_tokens[7])};
  const auto opt_pos{parse_triple(vec_tokens[9])};
  const auto opt_rng{parse_dec(vec_tokens[11])};
  const auto opt_cnt{parse_dec(vec_tokens[13])};
  if (!opt_tick.has_value() || !opt_from.has_value() || !opt_to.has_value() ||
      !opt_facing.has_value() || !opt_pos.has_value() ||
      !opt_rng.has_value() || !opt_cnt.has_value())
    return std::nullopt;
  row.int4_tick = *opt_tick;
  row.cell_from = pfg::CPos{opt_from->first, opt_from->second};
  row.cell_to = pfg::CPos{opt_to->first, opt_to->second};
  row.int4_facing = *opt_facing;
  row.pos = pfg::WPos{(*opt_pos)[0], (*opt_pos)[1], (*opt_pos)[2]};
  row.int4_rng_last = *opt_rng;
  row.int4_rng_count = *opt_cnt;
  row.b_idle = vec_tokens[15] == "True";
  return row;
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
  for (const ogm::MiniYamlNode& node : *res_nodes)
    if (node.str_key == "world") node_world = &node;
  if (node_world == nullptr) return std::nullopt;
  const ogm::MiniYamlNode* node_locomotor{nullptr};
  for (const ogm::MiniYamlNode& node : node_world->vec_children)
    if (node.str_key == "Locomotor@TRACKED") node_locomotor = &node;
  if (node_locomotor == nullptr) return std::nullopt;

  const auto res_def{pfg::locomotor_def_from_node(*node_locomotor)};
  if (!res_def.has_value()) return std::nullopt;
  return pfg::locomotor_map_build(ore.bin_map, ore.tileset, *res_def, ore.int4_left, ore.int4_top,
                                  ore.int4_right, ore.int4_bottom);
}

std::string format_row(const TickRow& row) {
  return std::format("tick {} from {},{} to {},{} facing {} pos {},{},{} rng {} cnt {} idle {}",
                     row.int4_tick, row.cell_from.int4_x, row.cell_from.int4_y,
                     row.cell_to.int4_x, row.cell_to.int4_y, row.int4_facing, row.pos.int4_x,
                     row.pos.int4_y, row.pos.int4_z, row.int4_rng_last, row.int4_rng_count,
                     row.b_idle);
}

TickRow snapshot_row(const pfg::MoveSimState& st, std::int32_t int4_tick) {
  TickRow row;
  row.int4_tick = int4_tick;
  row.cell_from = st.cell_from;
  row.cell_to = st.cell_to;
  row.int4_facing = st.int4_facing;
  row.pos = st.pos_center;
  row.b_idle = !st.b_has_activity;
  return row;
}

}

int main() {
  const auto str_golden{read_file_bytes(std::format("{}/ore-lord.move.txt", ORA_PATHFINDING_GOLDEN_DIR))};
  if (!str_golden.has_value()) {
    std::println("pathfind_move: golden move file missing");
    return 1;
  }

  std::vector<TickRow> vec_gold_tick;
  std::vector<TickRow> vec_gold_tick2;
  for (const std::string& str_line : read_lines(*str_golden)) {
    if (str_line.empty() || str_line[0] == '#') continue;
    if (str_line.starts_with("tick2 ")) {
      if (const auto opt_row{parse_tick_row(str_line)}) vec_gold_tick2.push_back(*opt_row);
    } else if (str_line.starts_with("tick ")) {
      if (const auto opt_row{parse_tick_row(str_line)}) vec_gold_tick.push_back(*opt_row);
    }
  }
  check(!vec_gold_tick.empty() && !vec_gold_tick2.empty(), "move: golden rows parsed");
  check(vec_gold_tick.size() == 228, std::format("move: golden tick rows {}", vec_gold_tick.size()));
  check(vec_gold_tick2.size() == 88,
        std::format("move: golden tick2 rows {}", vec_gold_tick2.size()));

  for (const TickRow& row : vec_gold_tick)
    check(row.int4_rng_last == 0 && row.int4_rng_count == 0,
          std::format("move: golden rng invariant at tick {}", row.int4_tick));
  for (const TickRow& row : vec_gold_tick2)
    check(row.int4_rng_last == 0 && row.int4_rng_count == 0,
          std::format("move: golden rng invariant at tick2 {}", row.int4_tick));

  const auto opt_ore{load_ore_lord()};
  if (!opt_ore.has_value()) {
    std::println("pathfind_move: ore-lord.oramap load failed");
    return 1;
  }
  const auto opt_map{build_tracked_map(*opt_ore)};
  if (!opt_map.has_value()) {
    std::println("pathfind_move: tracked locomotor map build failed");
    return 1;
  }

  pfg::MoveSimConfig cfg;
  cfg.ptr_map = &*opt_map;
  cfg.int4_speed = 113;
  cfg.int4_turn_speed = 512;
  cfg.int4_heuristic_weight = 125;

  pfg::MoveSimState st;
  st.cell_from = pfg::CPos{10, 17};
  st.cell_to = pfg::CPos{10, 17};
  st.int4_facing = 0;
  st.pos_center = pfg::center_of_cell(st.cell_to);

  pfg::move_queue(st, pfg::CPos{35, 17});

  std::size_t idx_gold{0};
  std::int32_t int4_tick{0};
  while (st.b_has_activity && int4_tick < 400) {
    ++int4_tick;
    pfg::move_sim_tick(st, cfg, int4_tick);
    if (idx_gold >= vec_gold_tick.size()) {
      check(false, "move: sim produced more ticks than golden");
      break;
    }
    const TickRow row_sim{snapshot_row(st, int4_tick)};
    const TickRow& row_gold{vec_gold_tick[idx_gold]};
    const bool b_match{row_sim.int4_tick == row_gold.int4_tick &&
                       row_sim.cell_from.int4_x == row_gold.cell_from.int4_x &&
                       row_sim.cell_from.int4_y == row_gold.cell_from.int4_y &&
                       row_sim.cell_to.int4_x == row_gold.cell_to.int4_x &&
                       row_sim.cell_to.int4_y == row_gold.cell_to.int4_y &&
                       row_sim.int4_facing == row_gold.int4_facing &&
                       row_sim.pos.int4_x == row_gold.pos.int4_x &&
                       row_sim.pos.int4_y == row_gold.pos.int4_y &&
                       row_sim.pos.int4_z == row_gold.pos.int4_z && row_sim.b_idle == row_gold.b_idle};
    check(b_match, std::format("move tick mismatch:\n  sim  {}\n  gold {}",
                               format_row(row_sim), format_row(row_gold)));
    ++idx_gold;
  }
  check(idx_gold == vec_gold_tick.size(),
        std::format("move: consumed {} of {} golden tick rows", idx_gold, vec_gold_tick.size()));

  pfg::move_queue(st, pfg::CPos{28, 10});

  std::size_t idx_gold2{0};
  while (st.b_has_activity && int4_tick < 800) {
    ++int4_tick;
    pfg::move_sim_tick(st, cfg, int4_tick);
    if (idx_gold2 >= vec_gold_tick2.size()) {
      check(false, "move: sim produced more tick2 rows than golden");
      break;
    }
    const TickRow row_sim{snapshot_row(st, int4_tick)};
    const TickRow& row_gold{vec_gold_tick2[idx_gold2]};
    const bool b_match{row_sim.int4_tick == row_gold.int4_tick &&
                       row_sim.cell_from.int4_x == row_gold.cell_from.int4_x &&
                       row_sim.cell_from.int4_y == row_gold.cell_from.int4_y &&
                       row_sim.cell_to.int4_x == row_gold.cell_to.int4_x &&
                       row_sim.cell_to.int4_y == row_gold.cell_to.int4_y &&
                       row_sim.int4_facing == row_gold.int4_facing &&
                       row_sim.pos.int4_x == row_gold.pos.int4_x &&
                       row_sim.pos.int4_y == row_gold.pos.int4_y &&
                       row_sim.pos.int4_z == row_gold.pos.int4_z && row_sim.b_idle == row_gold.b_idle};
    check(b_match, std::format("move tick2 mismatch:\n  sim  {}\n  gold {}",
                               format_row(row_sim), format_row(row_gold)));
    ++idx_gold2;
  }
  check(idx_gold2 == vec_gold_tick2.size(),
        std::format("move: consumed {} of {} golden tick2 rows", idx_gold2, vec_gold_tick2.size()));

  return ora::test::test_exit_code("pathfind_move");
}
