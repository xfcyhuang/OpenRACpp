module ora.game;

import std;
import ora.game.miniyaml;

namespace ora::game {

namespace {

[[nodiscard]] const MiniYamlNode* node_with_key(const std::vector<MiniYamlNode>& vec_nodes,
    std::string_view str_key) {
  const auto it_hit{std::find_if(vec_nodes.begin(), vec_nodes.end(),
      [&](const MiniYamlNode& node) { return node.str_key == str_key; })};
  return it_hit == vec_nodes.end() ? nullptr : &*it_hit;
}

[[nodiscard]] std::expected<int, std::string> parse_int(std::string_view str_text, std::string_view str_field,
    std::string_view str_source_name) {
  int int4_value{};
  const auto res_parse{std::from_chars(str_text.data(), str_text.data() + str_text.size(), int4_value)};
  if (res_parse.ec != std::errc{} || res_parse.ptr != str_text.data() + str_text.size())
    return std::unexpected(std::format("{}: 字段 {} 值 '{}' 非整数", str_source_name, str_field, str_text));
  return int4_value;
}

[[nodiscard]] std::expected<std::pair<int, int>, std::string> parse_int_pair(std::string_view str_text,
    std::string_view str_field, std::string_view str_source_name) {
  const std::size_t idx_comma{str_text.find(',')};
  if (idx_comma == std::string_view::npos)
    return std::unexpected(std::format("{}: 字段 {} 值 '{}' 非 x,y 对", str_source_name, str_field, str_text));
  const auto res_x{parse_int(str_text.substr(0, idx_comma), str_field, str_source_name)};
  if (!res_x) return std::unexpected(res_x.error());
  const auto res_y{parse_int(str_text.substr(idx_comma + 1), str_field, str_source_name)};
  if (!res_y) return std::unexpected(res_y.error());
  return std::pair{*res_x, *res_y};
}

[[nodiscard]] std::expected<bool, std::string> parse_bool(std::string_view str_text, std::string_view str_field,
    std::string_view str_source_name) {
  if (str_text == "True") return true;
  if (str_text == "False") return false;
  return std::unexpected(std::format("{}: 字段 {} 值 '{}' 非布尔", str_source_name, str_field, str_text));
}

}

std::expected<Tileset, std::string> tileset_load(
    std::string_view str_yaml_text, std::string_view str_source_name) {
  const auto res_nodes{miniyaml_from_string(str_yaml_text, str_source_name, true)};
  if (!res_nodes) return std::unexpected(res_nodes.error());

  const MiniYamlNode* node_general{node_with_key(*res_nodes, "General")};
  if (node_general == nullptr) return std::unexpected(std::format("{}: 缺 General 块", str_source_name));

  Tileset ts_out;
  if (const MiniYamlNode* node_id{node_with_key(node_general->vec_children, "Id")})
    ts_out.str_id = node_id->str_value;
  if (const MiniYamlNode* node_name{node_with_key(node_general->vec_children, "Name")})
    ts_out.str_name = node_name->str_value;
  if (const MiniYamlNode* node_size{node_with_key(node_general->vec_children, "TileSize")}) {
    const auto res_pair{parse_int_pair(node_size->str_value, "TileSize", str_source_name)};
    if (!res_pair) return std::unexpected(res_pair.error());
    ts_out.int4_tile_size_x = res_pair->first;
    ts_out.int4_tile_size_y = res_pair->second;
  }

  const MiniYamlNode* node_terrain{node_with_key(*res_nodes, "Terrain")};
  if (node_terrain == nullptr) return std::unexpected(std::format("{}: 缺 Terrain 块", str_source_name));
  for (const MiniYamlNode& node_type : node_terrain->vec_children) {
    const MiniYamlNode* node_name{node_with_key(node_type.vec_children, "Type")};
    if (node_name == nullptr)
      return std::unexpected(std::format("{}: 地形类型 {} 缺 Type 字段", str_source_name, node_type.str_key));
    ts_out.vec_str_types.push_back(node_name->str_value);
  }
  std::sort(ts_out.vec_str_types.begin(), ts_out.vec_str_types.end());
  if (ts_out.vec_str_types.size() >= 255)
    return std::unexpected(std::format("{}: Too many terrain types.", str_source_name));
  for (std::size_t sz_i{1}; sz_i < ts_out.vec_str_types.size(); ++sz_i)
    if (ts_out.vec_str_types[sz_i] == ts_out.vec_str_types[sz_i - 1])
      return std::unexpected(std::format("{}: Duplicate terrain type '{}' in '{}'.",
          str_source_name, ts_out.vec_str_types[sz_i], str_source_name));

  const auto res_walkable{ts_out.terrain_index_of_type("Clear")};
  if (!res_walkable) return std::unexpected(res_walkable.error());
  ts_out.u1_default_walkable = *res_walkable;

  const MiniYamlNode* node_templates{node_with_key(*res_nodes, "Templates")};
  if (node_templates == nullptr) return std::unexpected(std::format("{}: 缺 Templates 块", str_source_name));
  for (const MiniYamlNode& node_tpl : node_templates->vec_children) {
    TerrainTemplate tpl_next{};
    if (const MiniYamlNode* node_id{node_with_key(node_tpl.vec_children, "Id")}) {
      const auto res_id{parse_int(node_id->str_value, "Id", str_source_name)};
      if (!res_id) return std::unexpected(res_id.error());
      if (*res_id < 0 || *res_id > 65535)
        return std::unexpected(std::format("{}: 模板 Id {} 越界", str_source_name, *res_id));
      tpl_next.u2_id = static_cast<std::uint16_t>(*res_id);
    }
    int int4_frame_slots{1};
    if (const MiniYamlNode* node_size{node_with_key(node_tpl.vec_children, "Size")}) {
      const auto res_pair{parse_int_pair(node_size->str_value, "Size", str_source_name)};
      if (!res_pair) return std::unexpected(res_pair.error());
      tpl_next.int4_size_x = res_pair->first;
      tpl_next.int4_size_y = res_pair->second;
      int4_frame_slots = tpl_next.int4_size_x * tpl_next.int4_size_y;
      if (int4_frame_slots < 0)
        return std::unexpected(std::format("{}: 模板 {} Size 乘积为负", str_source_name, tpl_next.u2_id));
    }
    if (const MiniYamlNode* node_images{node_with_key(node_tpl.vec_children, "Images")}) {
      std::string_view str_rest{node_images->str_value};
      while (!str_rest.empty()) {
        const std::size_t sz_comma{str_rest.find(',')};
        const std::string_view str_one{sz_comma == std::string_view::npos ? str_rest
                                                                          : str_rest.substr(0, sz_comma)};
        std::size_t sz_begin{0};
        std::size_t sz_end{str_one.size()};
        while (sz_begin < sz_end && (str_one[sz_begin] == ' ' || str_one[sz_begin] == '	'))
          ++sz_begin;
        while (sz_end > sz_begin && (str_one[sz_end - 1] == ' ' || str_one[sz_end - 1] == '	'))
          --sz_end;
        if (sz_end > sz_begin)
          tpl_next.vec_str_images.emplace_back(str_one.substr(sz_begin, sz_end - sz_begin));
        if (sz_comma == std::string_view::npos) break;
        str_rest = str_rest.substr(sz_comma + 1);
      }
      if (tpl_next.vec_str_images.empty())
        return std::unexpected(std::format("{}: 模板 {} Images 为空", str_source_name, tpl_next.u2_id));
    }
    if (const MiniYamlNode* node_pick{node_with_key(node_tpl.vec_children, "PickAny")}) {
      const auto res_bool{parse_bool(node_pick->str_value, "PickAny", str_source_name)};
      if (!res_bool) return std::unexpected(res_bool.error());
      tpl_next.b_pickany = *res_bool;
    }

    const MiniYamlNode* node_tiles{node_with_key(node_tpl.vec_children, "Tiles")};
    if (node_tiles == nullptr)
      return std::unexpected(std::format("{}: 模板 {} 缺 Tiles 块", str_source_name, tpl_next.u2_id));

    if (tpl_next.b_pickany) {
      tpl_next.vec_opt1u_frame_terrain.reserve(node_tiles->vec_children.size());
      for (std::size_t sz_frame{0}; sz_frame < node_tiles->vec_children.size(); ++sz_frame) {
        const MiniYamlNode& node_frame{node_tiles->vec_children[sz_frame]};
        const auto res_frame{parse_int(node_frame.str_key, "Tiles 帧", str_source_name)};
        if (!res_frame) return std::unexpected(res_frame.error());
        if (*res_frame != static_cast<int>(sz_frame))
          return std::unexpected(std::format("{}: 模板 {} 缺帧 {} 定义（PickAny 要求 0 起连续）",
              str_source_name, tpl_next.u2_id, sz_frame));
        const auto res_terrain{ts_out.terrain_index_of_type(node_frame.str_value)};
        if (!res_terrain) return std::unexpected(res_terrain.error());
        tpl_next.vec_opt1u_frame_terrain.push_back(*res_terrain);
      }
    } else {
      tpl_next.vec_opt1u_frame_terrain.assign(static_cast<std::size_t>(int4_frame_slots), std::nullopt);
      for (const MiniYamlNode& node_frame : node_tiles->vec_children) {
        const auto res_frame{parse_int(node_frame.str_key, "Tiles 帧", str_source_name)};
        if (!res_frame) return std::unexpected(res_frame.error());
        if (*res_frame < 0 || *res_frame >= int4_frame_slots)
          return std::unexpected(std::format("{}: tileset `{}` template `{}` references frame {}, "
              "but only [0..{}] are valid for a {}x{} Size template.",
              str_source_name, ts_out.str_id, tpl_next.u2_id, *res_frame, int4_frame_slots - 1,
              tpl_next.int4_size_x, tpl_next.int4_size_y));
        const auto res_terrain{ts_out.terrain_index_of_type(node_frame.str_value)};
        if (!res_terrain) return std::unexpected(res_terrain.error());
        tpl_next.vec_opt1u_frame_terrain[static_cast<std::size_t>(*res_frame)] = *res_terrain;
      }
    }

    ts_out.vec_templates.push_back(std::move(tpl_next));
  }

  return ts_out;
}

}
