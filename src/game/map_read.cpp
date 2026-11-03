#include "game/map_read.hpp"

#include "game/miniyaml.hpp"
#include "game/support/sha1.hpp"

namespace ora::game {

namespace {

[[nodiscard]] const MiniYamlNode* node_with_key(const std::vector<MiniYamlNode>& vec_nodes,
    std::string_view str_key) {
  const auto it_hit{std::find_if(vec_nodes.begin(), vec_nodes.end(),
      [&](const MiniYamlNode& node) { return node.str_key == str_key; })};
  return it_hit == vec_nodes.end() ? nullptr : &*it_hit;
}

[[nodiscard]] std::expected<std::string, std::string> require_value(const std::vector<MiniYamlNode>& vec_nodes,
    std::string_view str_key) {
  const MiniYamlNode* node{node_with_key(vec_nodes, str_key)};
  if (node == nullptr) return std::unexpected(std::format("Required field `{}` not found in map.yaml", str_key));
  return node->str_value;
}

[[nodiscard]] std::expected<int, std::string> parse_int_field(const std::vector<MiniYamlNode>& vec_nodes,
    std::string_view str_key) {
  const auto res_text{require_value(vec_nodes, str_key)};
  if (!res_text) return std::unexpected(res_text.error());
  int int4_value{};
  const auto res_parse{std::from_chars(res_text->data(), res_text->data() + res_text->size(), int4_value)};
  if (res_parse.ec != std::errc{} || res_parse.ptr != res_text->data() + res_text->size())
    return std::unexpected(std::format("FieldLoader: Invalid value for `{}`: `{}", str_key, *res_text));
  return int4_value;
}

[[nodiscard]] std::expected<int, std::string> parse_int_value(std::string_view str_text) {
  int int4_value{};
  const auto res_parse{std::from_chars(str_text.data(), str_text.data() + str_text.size(), int4_value)};
  if (res_parse.ec != std::errc{} || res_parse.ptr != str_text.data() + str_text.size())
    return std::unexpected(std::format("FieldLoader: Invalid integer value `{}", str_text));
  return int4_value;
}

[[nodiscard]] std::vector<std::string> split_comma_trimmed(std::string_view str_text) {
  std::vector<std::string> vec_str_out;
  std::size_t sz_pos{0};
  while (sz_pos <= str_text.size()) {
    const std::size_t idx_comma{str_text.find(',', sz_pos)};
    std::string_view str_part{idx_comma == std::string_view::npos
        ? str_text.substr(sz_pos)
        : str_text.substr(sz_pos, idx_comma - sz_pos)};
    while (!str_part.empty() && (str_part.front() == ' ' || str_part.front() == '\t')) str_part.remove_prefix(1);
    while (!str_part.empty() && (str_part.back() == ' ' || str_part.back() == '\t')) str_part.remove_suffix(1);
    if (!str_part.empty()) vec_str_out.emplace_back(str_part);
    if (idx_comma == std::string_view::npos) break;
    sz_pos = idx_comma + 1;
  }
  return vec_str_out;
}

[[nodiscard]] std::expected<std::uint32_t, std::string> parse_visibility(std::string_view str_text) {
  constexpr std::array<std::pair<std::string_view, std::uint32_t>, 3> kFlagNames{{
      {"Lobby", 1u},
      {"Shellmap", 2u},
      {"MissionSelector", 4u},
  }};
  std::uint32_t u4_value{0};
  for (const std::string& str_name : split_comma_trimmed(str_text)) {
    bool b_hit{false};
    for (const auto& [str_flag, u4_bits] : kFlagNames)
      if (str_name == str_flag) {
        u4_value |= u4_bits;
        b_hit = true;
        break;
      }
    if (!b_hit) return std::unexpected(std::format("未知 MapVisibility 值 '{}'", str_name));
  }
  return u4_value;
}

[[nodiscard]] std::string visibility_to_string(std::uint32_t u4_value) {
  constexpr std::array<std::pair<std::string_view, std::uint32_t>, 3> kFlagNames{{
      {"Lobby", 1u},
      {"Shellmap", 2u},
      {"MissionSelector", 4u},
  }};
  if (u4_value == 0u) return "0";
  std::string str_out;
  std::uint32_t u4_rest{u4_value};
  for (const auto& [str_flag, u4_bits] : kFlagNames)
    if ((u4_value & u4_bits) != 0u) {
      if (!str_out.empty()) str_out += ", ";
      str_out += str_flag;
      u4_rest &= ~u4_bits;
    }
  if (u4_rest != 0u) {
    if (!str_out.empty()) str_out += ", ";
    str_out += std::to_string(u4_rest);
  }
  return str_out;
}

[[nodiscard]] bool line_matches_map_format(std::string_view str_line, int& int4_out) {
  if (!str_line.starts_with("MapFormat:")) return false;
  std::string_view str_rest{str_line.substr(10)};
  while (!str_rest.empty() && (str_rest.front() == ' ' || str_rest.front() == '\t')) str_rest.remove_prefix(1);
  while (!str_rest.empty() && (str_rest.back() == ' ' || str_rest.back() == '\t')) str_rest.remove_suffix(1);
  if (str_rest.empty()) {
    int4_out = 0;
    return true;
  }
  const auto res_parse{std::from_chars(str_rest.data(), str_rest.data() + str_rest.size(), int4_out)};
  return res_parse.ec == std::errc{} && res_parse.ptr == str_rest.data() + str_rest.size();
}

[[nodiscard]] std::string to_lower_ascii(std::string_view str_text) {
  std::string str_out;
  str_out.reserve(str_text.size());
  for (const char ch : str_text)
    str_out.push_back(ch >= 'A' && ch <= 'Z' ? static_cast<char>(ch - 'A' + 'a') : ch);
  return str_out;
}

[[nodiscard]] std::string join_strings(const std::vector<std::string>& vec_str_parts,
    std::string_view str_sep) {
  std::string str_out;
  for (std::size_t sz_i{0}; sz_i < vec_str_parts.size(); ++sz_i) {
    if (sz_i > 0) str_out += str_sep;
    str_out += vec_str_parts[sz_i];
  }
  return str_out;
}

}

std::expected<int, std::string> map_scan_format(
    std::span<const std::uint8_t> span1u_yaml_bytes) {
  std::string_view str_text{reinterpret_cast<const char*>(span1u_yaml_bytes.data()), span1u_yaml_bytes.size()};
  std::size_t sz_pos{0};
  while (sz_pos < str_text.size()) {
    const std::size_t idx_nl{str_text.find('\n', sz_pos)};
    std::string_view str_line{idx_nl == std::string_view::npos
        ? str_text.substr(sz_pos)
        : str_text.substr(sz_pos, idx_nl - sz_pos)};
    if (!str_line.empty() && str_line.back() == '\r') str_line.remove_suffix(1);
    int int4_format{0};
    if (line_matches_map_format(str_line, int4_format)) return int4_format;
    if (idx_nl == std::string_view::npos) break;
    sz_pos = idx_nl + 1;
  }
  return std::unexpected(std::string{"MapFormat is not defined"});
}

std::expected<MapHeader, std::string> map_header_parse(
    std::span<const std::uint8_t> span1u_yaml_bytes, std::string_view str_source_name) {
  const auto res_nodes{miniyaml_from_string(
      std::string_view{reinterpret_cast<const char*>(span1u_yaml_bytes.data()), span1u_yaml_bytes.size()},
      str_source_name, true)};
  if (!res_nodes) return std::unexpected(res_nodes.error());

  MapHeader hdr_out;
  const auto res_format{parse_int_field(*res_nodes, "MapFormat")};
  if (!res_format) return std::unexpected(res_format.error());
  hdr_out.int4_format = *res_format;

  const auto res_requires{require_value(*res_nodes, "RequiresMod")};
  if (!res_requires) return std::unexpected(res_requires.error());
  hdr_out.str_requires_mod = *res_requires;

  const auto res_title{require_value(*res_nodes, "Title")};
  if (!res_title) return std::unexpected(res_title.error());
  hdr_out.str_title = *res_title;

  const auto res_author{require_value(*res_nodes, "Author")};
  if (!res_author) return std::unexpected(res_author.error());
  hdr_out.str_author = *res_author;

  const auto res_tileset{require_value(*res_nodes, "Tileset")};
  if (!res_tileset) return std::unexpected(res_tileset.error());
  hdr_out.str_tileset = *res_tileset;

  const auto res_size{require_value(*res_nodes, "MapSize")};
  if (!res_size) return std::unexpected(res_size.error());
  const std::vector<std::string> vec_str_size{split_comma_trimmed(*res_size)};
  if (vec_str_size.size() != 2) return std::unexpected(std::format("MapSize '{}' 非宽,高对", *res_size));
  const auto res_w{parse_int_value(vec_str_size[0])};
  if (!res_w) return std::unexpected(res_w.error());
  const auto res_h{parse_int_value(vec_str_size[1])};
  if (!res_h) return std::unexpected(res_h.error());
  hdr_out.int4_size_x = *res_w;
  hdr_out.int4_size_y = *res_h;

  const auto res_bounds{require_value(*res_nodes, "Bounds")};
  if (!res_bounds) return std::unexpected(res_bounds.error());
  const std::vector<std::string> vec_str_bounds{split_comma_trimmed(*res_bounds)};
  if (vec_str_bounds.size() != 4) return std::unexpected(std::format("Bounds '{}' 非四元组", *res_bounds));
  const auto res_bx{parse_int_value(vec_str_bounds[0])};
  if (!res_bx) return std::unexpected(res_bx.error());
  const auto res_by{parse_int_value(vec_str_bounds[1])};
  if (!res_by) return std::unexpected(res_by.error());
  const auto res_bw{parse_int_value(vec_str_bounds[2])};
  if (!res_bw) return std::unexpected(res_bw.error());
  const auto res_bh{parse_int_value(vec_str_bounds[3])};
  if (!res_bh) return std::unexpected(res_bh.error());
  hdr_out.int4_bounds_x = *res_bx;
  hdr_out.int4_bounds_y = *res_by;
  hdr_out.int4_bounds_w = *res_bw;
  hdr_out.int4_bounds_h = *res_bh;

  const auto res_visibility{require_value(*res_nodes, "Visibility")};
  if (!res_visibility) return std::unexpected(res_visibility.error());
  const auto res_flags{parse_visibility(*res_visibility)};
  if (!res_flags) return std::unexpected(res_flags.error());
  hdr_out.u4_visibility = *res_flags;

  const MiniYamlNode* node_categories{node_with_key(*res_nodes, "Categories")};
  if (node_categories != nullptr)
    hdr_out.vec_str_categories = split_comma_trimmed(node_categories->str_value);
  if (hdr_out.vec_str_categories.empty()) hdr_out.vec_str_categories.emplace_back("Conquest");

  const MiniYamlNode* node_players{node_with_key(*res_nodes, "Players")};
  if (node_players != nullptr) hdr_out.sz_players = node_players->vec_children.size();

  const MiniYamlNode* node_actors{node_with_key(*res_nodes, "Actors")};
  if (node_actors != nullptr) hdr_out.sz_actors = node_actors->vec_children.size();

  return hdr_out;
}

std::expected<std::string, std::string> map_uid(const filesystem::ZipArchive& archive_zip) {
  bool b_has_yaml{false};
  bool b_has_bin{false};
  for (const filesystem::ZipEntry& entry : archive_zip.vec_entries) {
    if (entry.str_name == "map.yaml") b_has_yaml = true;
    if (entry.str_name == "map.bin") b_has_bin = true;
  }
  if (!b_has_yaml) return std::unexpected(std::string{"Required file map.yaml not present in this map"});
  if (!b_has_bin) return std::unexpected(std::string{"Required file map.bin not present in this map"});

  const auto res_yaml{filesystem::zip_entry_data(archive_zip, "map.yaml")};
  if (!res_yaml) return std::unexpected(res_yaml.error());
  const auto res_format{map_scan_format(*res_yaml)};
  if (!res_format) return std::unexpected(res_format.error());

  std::string str_merged;
  for (const filesystem::ZipEntry& entry : archive_zip.vec_entries) {
    const std::string_view str_name{entry.str_name};
    if (str_name.ends_with(".yaml") || str_name.ends_with(".bin") || str_name.ends_with(".lua")
        || (*res_format >= 12 && str_name == "map.png")) {
      const auto res_data{filesystem::zip_entry_data(archive_zip, str_name)};
      if (!res_data) return std::unexpected(res_data.error());
      str_merged.append(reinterpret_cast<const char*>(res_data->data()), res_data->size());
    }
  }

  return support::sha1_hex(str_merged);
}

std::expected<std::string, std::string> map_info_dump(
    std::span<const std::uint8_t> span1u_oramap_bytes, std::string_view str_tileset_dir) {
  const auto res_zip{filesystem::zip_read(span1u_oramap_bytes)};
  if (!res_zip) return std::unexpected(res_zip.error());

  const auto res_yaml{filesystem::zip_entry_data(*res_zip, "map.yaml")};
  if (!res_yaml) return std::unexpected(std::format("Not a valid map\n File: map.yaml"));
  const auto res_bin{filesystem::zip_entry_data(*res_zip, "map.bin")};
  if (!res_bin) return std::unexpected(std::format("Not a valid map\n File: map.bin"));

  const auto res_uid{map_uid(*res_zip)};
  if (!res_uid) return std::unexpected(res_uid.error());

  const auto res_header{map_header_parse(*res_yaml, "map.yaml")};
  if (!res_header) return std::unexpected(res_header.error());

  const auto str_tileset_path{std::format("{}/{}.yaml", str_tileset_dir,
      to_lower_ascii(res_header->str_tileset))};
  std::ifstream stream_tileset{std::filesystem::path{str_tileset_path}, std::ios::binary};
  if (!stream_tileset) return std::unexpected(std::format("tileset 文件缺失：{}", str_tileset_path));
  std::string str_tileset_bytes;
  char buf1_chunk[8192];
  while (stream_tileset.read(buf1_chunk, static_cast<std::streamsize>(sizeof buf1_chunk))
      || stream_tileset.gcount() > 0)
    str_tileset_bytes.append(buf1_chunk, static_cast<std::size_t>(stream_tileset.gcount()));
  const auto res_tileset{tileset_load(str_tileset_bytes, str_tileset_path)};
  if (!res_tileset) return std::unexpected(res_tileset.error());

  auto res_mapbin{map_bin_parse(*res_bin, res_header->int4_size_x, res_header->int4_size_y)};
  if (!res_mapbin) return std::unexpected(res_mapbin.error());

  std::size_t sz_replaced{0};
  const TerrainTile tile_default{res_tileset->default_terrain_tile()};
  for (TerrainTile& tile : res_mapbin->vec_tiles) {
    if (!res_tileset->tile_is_valid(tile)) {
      ++sz_replaced;
      tile = tile_default;
    }
  }

  std::map<std::pair<std::uint16_t, std::uint8_t>, std::size_t> map_hist_tiles;
  std::map<std::pair<std::uint8_t, std::uint8_t>, std::size_t> map_hist_resources;
  for (int int4_i{0}; int4_i < res_mapbin->int4_width; ++int4_i)
    for (int int4_j{0}; int4_j < res_mapbin->int4_height; ++int4_j) {
      const TerrainTile& tile{res_mapbin->tile_at(int4_i, int4_j)};
      ++map_hist_tiles[{tile.u2_type, tile.u1_index}];
      const ResourceTile& res{res_mapbin->resource_at(int4_i, int4_j)};
      ++map_hist_resources[{res.u1_type, res.u1_density}];
    }

  std::string str_out;
  str_out += std::format("uid {}\n", *res_uid);
  str_out += std::format("mapformat {}\n", res_header->int4_format);
  str_out += std::format("requiresmod {}\n", res_header->str_requires_mod);
  str_out += std::format("title {}\n", res_header->str_title);
  str_out += std::format("author {}\n", res_header->str_author);
  str_out += std::format("tileset {}\n", res_header->str_tileset);
  str_out += std::format("mapsize {},{}\n", res_header->int4_size_x, res_header->int4_size_y);
  str_out += std::format("bounds {},{},{},{}\n", res_header->int4_bounds_x, res_header->int4_bounds_y,
      res_header->int4_bounds_w, res_header->int4_bounds_h);
  str_out += std::format("visibility {}\n", visibility_to_string(res_header->u4_visibility));
  str_out += std::format("categories {}\n", join_strings(res_header->vec_str_categories, ","));
  str_out += std::format("players {}\n", res_header->sz_players);
  str_out += std::format("actors {}\n", res_header->sz_actors);
  str_out += std::format("invalidcustomrules {}\n", "False");
  str_out += std::format("replacedinvalidtiles {}\n", sz_replaced);

  str_out += std::format("tilerecords {}\n", map_hist_tiles.size());
  for (const auto& [pair_key, sz_count] : map_hist_tiles)
    str_out += std::format("tile {},{} {}\n", pair_key.first, pair_key.second, sz_count);

  str_out += std::format("resourcerecords {}\n", map_hist_resources.size());
  for (const auto& [pair_key, sz_count] : map_hist_resources)
    str_out += std::format("resource {},{} {}\n", pair_key.first, pair_key.second, sz_count);

  str_out += std::format("terraininfo id={} name={} tilesize={},{} terraintypes={}\n",
      res_tileset->str_id, res_tileset->str_name, res_tileset->int4_tile_size_x,
      res_tileset->int4_tile_size_y, res_tileset->vec_str_types.size());
  str_out += std::format("defaulttile {},{}\n", tile_default.u2_type, tile_default.u1_index);

  std::string str_idx;
  std::size_t sz_n{0};
  for (const auto& [pair_key, sz_count] : map_hist_tiles) {
    if (sz_n++ == 5) break;
    str_idx += std::format("{},{}->{} ", pair_key.first, pair_key.second,
        res_tileset->terrain_index_of_tile(TerrainTile{pair_key.first, pair_key.second}));
  }
  while (!str_idx.empty() && str_idx.back() == ' ') str_idx.pop_back();
  str_out += std::format("terrainidx {}\n", str_idx);

  return str_out;
}

}
