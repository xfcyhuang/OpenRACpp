module ora.game.ui;

import std;
import ora.game.miniyaml;
import ora.game.render;

namespace ora::game::ui {

namespace {

constexpr bool is_space(char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }

std::string_view trim(std::string_view str_value) {
  while (!str_value.empty() && is_space(str_value.front())) str_value.remove_prefix(1);
  while (!str_value.empty() && is_space(str_value.back())) str_value.remove_suffix(1);
  return str_value;
}

std::vector<std::string_view> split_comma(std::string_view str_value) {
  std::vector<std::string_view> vec_out;
  std::size_t sz_begin{0};
  while (sz_begin <= str_value.size()) {
    const std::size_t sz_comma{str_value.find(',', sz_begin)};
    std::string_view str_part{str_value.substr(
        sz_begin, sz_comma == std::string_view::npos ? std::string_view::npos : sz_comma - sz_begin)};
    str_part = trim(str_part);
    if (!str_part.empty()) vec_out.push_back(str_part);
    if (sz_comma == std::string_view::npos) break;
    sz_begin = sz_comma + 1;
  }
  return vec_out;
}

std::expected<int, std::string> parse_int_value(std::string_view str_part, std::string_view str_key) {
  int int4_value{};
  const auto res{std::from_chars(str_part.data(), str_part.data() + str_part.size(), int4_value)};
  if (res.ec != std::errc{} || res.ptr != str_part.data() + str_part.size())
    return std::unexpected(std::format(
        "FieldLoader: Cannot parse `{}` into field `{}` of type `Int32`", str_part, str_key));
  return int4_value;
}

std::expected<ChromeRect, std::string> parse_rect(std::string_view str_value, std::string_view str_key) {
  const std::vector<std::string_view> vec_parts{split_comma(str_value)};
  if (vec_parts.size() != 4)
    return std::unexpected(std::format(
        "FieldLoader: Cannot parse `{}` into field `{}` of type `Rectangle`", trim(str_value), str_key));
  const auto res_x{parse_int_value(vec_parts[0], str_key)};
  if (!res_x) return std::unexpected(res_x.error());
  const auto res_y{parse_int_value(vec_parts[1], str_key)};
  if (!res_y) return std::unexpected(res_y.error());
  const auto res_w{parse_int_value(vec_parts[2], str_key)};
  if (!res_w) return std::unexpected(res_w.error());
  const auto res_h{parse_int_value(vec_parts[3], str_key)};
  if (!res_h) return std::unexpected(res_h.error());
  return ChromeRect{*res_x, *res_y, *res_w, *res_h};
}

struct PanelSideName {
  std::string_view str_name;
  std::uint32_t u4_value;
};

constexpr PanelSideName kPanelSides[]{
    {"Left", 0x1}, {"Top", 0x2}, {"Right", 0x4}, {"Bottom", 0x8}, {"Center", 0x10},
    {"Edges", 0xF}, {"All", 0x1F},
};

char to_lower_ascii(char c) { return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c; }

bool name_equals(std::string_view str_part, std::string_view str_name) {
  if (str_part.size() != str_name.size()) return false;
  for (std::size_t sz_i{0}; sz_i < str_part.size(); ++sz_i)
    if (to_lower_ascii(str_part[sz_i]) != to_lower_ascii(str_name[sz_i])) return false;
  return true;
}

std::expected<std::uint32_t, std::string> parse_panel_sides(std::string_view str_value) {
  std::uint32_t u4_sides{};
  for (const std::string_view str_part : split_comma(str_value)) {
    bool b_matched{false};
    for (const PanelSideName& side : kPanelSides)
      if (name_equals(str_part, side.str_name)) {
        u4_sides |= side.u4_value;
        b_matched = true;
        break;
      }
    if (b_matched) continue;
    int int4_numeric{};
    const auto res{std::from_chars(str_part.data(), str_part.data() + str_part.size(), int4_numeric)};
    if (res.ec == std::errc{} && res.ptr == str_part.data() + str_part.size() && int4_numeric >= 0) {
      u4_sides |= static_cast<std::uint32_t>(int4_numeric);
      continue;
    }
    return std::unexpected(std::format(
        "FieldLoader: Cannot parse `{}` into field `PanelSides` of type `PanelSides`", trim(str_value)));
  }
  return u4_sides;
}

std::expected<ChromeCollection, std::string> load_collection(const MiniYamlNode& node) {
  ChromeCollection collection{};
  for (const MiniYamlNode& field : node.vec_children) {
    if (field.str_key == "Image") {
      collection.str_image = field.str_value;
      continue;
    }
    if (field.str_key == "Image2x") {
      collection.str_image2x = field.str_value;
      continue;
    }
    if (field.str_key == "Image3x") {
      collection.str_image3x = field.str_value;
      continue;
    }
    if (field.str_key == "PanelRegion") {
      for (const std::string_view str_part : split_comma(field.str_value)) {
        const auto res{parse_int_value(str_part, "PanelRegion")};
        if (!res) return std::unexpected(res.error());
        collection.vec_panel_region.push_back(*res);
      }
      continue;
    }
    if (field.str_key == "PanelSides") {
      const auto res{parse_panel_sides(field.str_value)};
      if (!res) return std::unexpected(res.error());
      collection.u4_panel_sides = *res;
      continue;
    }
    if (field.str_key == "Regions") {
      for (const MiniYamlNode& region : field.vec_children) {
        const auto res{parse_rect(region.str_value, region.str_key)};
        if (!res) return std::unexpected(res.error());
        collection.map_regions.emplace(region.str_key, *res);
      }
      continue;
    }
    return std::unexpected(std::format(
        "FieldLoader: Missing field `{}` on `ChromeProvider+Collection`", field.str_key));
  }
  return collection;
}

}  // namespace

std::expected<std::map<std::string, std::string>, std::string> chrome_metrics_initialize(
    const std::vector<std::vector<MiniYamlNode>>& vec_sources) {
  const auto res_merged{miniyaml_merge(vec_sources)};
  if (!res_merged) return std::unexpected(res_merged.error());
  std::map<std::string, std::string> map_out;
  for (const MiniYamlNode& top : *res_merged)
    for (const MiniYamlNode& child : top.vec_children) map_out[child.str_key] = child.str_value;
  return map_out;
}

std::expected<std::map<std::string, ChromeCollection>, std::string>
chrome_provider_initialize(const std::vector<std::vector<MiniYamlNode>>& vec_sources) {
  const auto res_merged{miniyaml_merge(vec_sources)};
  if (!res_merged) return std::unexpected(res_merged.error());
  std::map<std::string, ChromeCollection> map_out;
  for (const MiniYamlNode& top : *res_merged) {
    if (!top.str_key.empty() && top.str_key[0] == '^') continue;
    auto res_collection{load_collection(top)};
    if (!res_collection) return std::unexpected(res_collection.error());
    map_out.emplace(top.str_key, std::move(*res_collection));
  }
  return map_out;
}

std::optional<ChromeRect> chrome_try_get_image(
    const std::map<std::string, ChromeCollection>& map_collections, std::string_view str_collection,
    std::string_view str_image) {
  if (str_collection.empty()) return std::nullopt;
  const auto it{map_collections.find(std::string{str_collection})};
  if (it == map_collections.end()) return std::nullopt;
  const auto it_region{it->second.map_regions.find(std::string{str_image})};
  if (it_region == it->second.map_regions.end()) return std::nullopt;
  return it_region->second;
}

std::vector<std::optional<ChromeRect>> chrome_get_panel_images(
    const std::map<std::string, ChromeCollection>& map_collections, std::string_view str_collection) {
  const auto it{map_collections.find(std::string{str_collection})};
  if (it == map_collections.end()) return {};
  const ChromeCollection& collection{it->second};
  if (collection.vec_panel_region.size() != 8) {
    if (collection.map_regions.empty()) return {};
    return std::vector<std::optional<ChromeRect>>{
        chrome_try_get_image(map_collections, str_collection, "corner-tl"),
        chrome_try_get_image(map_collections, str_collection, "border-t"),
        chrome_try_get_image(map_collections, str_collection, "corner-tr"),
        chrome_try_get_image(map_collections, str_collection, "border-l"),
        chrome_try_get_image(map_collections, str_collection, "background"),
        chrome_try_get_image(map_collections, str_collection, "border-r"),
        chrome_try_get_image(map_collections, str_collection, "corner-bl"),
        chrome_try_get_image(map_collections, str_collection, "border-b"),
        chrome_try_get_image(map_collections, str_collection, "corner-br")};
  }
  const int int4_x{collection.vec_panel_region[0]};
  const int int4_y{collection.vec_panel_region[1]};
  const int int4_left_w{collection.vec_panel_region[2]};
  const int int4_top_h{collection.vec_panel_region[3]};
  const int int4_center_w{collection.vec_panel_region[4]};
  const int int4_center_h{collection.vec_panel_region[5]};
  const int int4_right_w{collection.vec_panel_region[6]};
  const int int4_bottom_h{collection.vec_panel_region[7]};
  const std::uint32_t u4_sides{collection.u4_panel_sides};
  const auto has_side{[u4_sides](std::uint32_t u4_mask) { return (u4_sides & u4_mask) == u4_mask; }};

  const std::pair<std::uint32_t, ChromeRect> vec_slots[]{
      {0x2 | 0x1, ChromeRect{int4_x, int4_y, int4_left_w, int4_top_h}},
      {0x2, ChromeRect{int4_x + int4_left_w, int4_y, int4_center_w, int4_top_h}},
      {0x2 | 0x4, ChromeRect{int4_x + int4_left_w + int4_center_w, int4_y, int4_right_w, int4_top_h}},
      {0x1, ChromeRect{int4_x, int4_y + int4_top_h, int4_left_w, int4_center_h}},
      {0x10, ChromeRect{int4_x + int4_left_w, int4_y + int4_top_h, int4_center_w, int4_center_h}},
      {0x4, ChromeRect{int4_x + int4_left_w + int4_center_w, int4_y + int4_top_h, int4_right_w,
                       int4_center_h}},
      {0x8 | 0x1, ChromeRect{int4_x, int4_y + int4_top_h + int4_center_h, int4_left_w, int4_bottom_h}},
      {0x8, ChromeRect{int4_x + int4_left_w, int4_y + int4_top_h + int4_center_h, int4_center_w,
                       int4_bottom_h}},
      {0x8 | 0x4, ChromeRect{int4_x + int4_left_w + int4_center_w,
                             int4_y + int4_top_h + int4_center_h, int4_right_w, int4_bottom_h}},
  };

  std::vector<std::optional<ChromeRect>> vec_out;
  vec_out.reserve(9);
  for (const auto& [u4_mask, rect] : vec_slots)
    vec_out.push_back(has_side(u4_mask) ? std::optional<ChromeRect>{rect} : std::nullopt);
  return vec_out;
}

std::pair<int, int> chrome_get_minimum_panel_size(
    const std::map<std::string, ChromeCollection>& map_collections, std::string_view str_collection) {
  if (str_collection.empty()) return {0, 0};
  const auto it{map_collections.find(std::string{str_collection})};
  if (it == map_collections.end()) return {0, 0};
  if (it->second.vec_panel_region.size() != 8) return {0, 0};
  return {it->second.vec_panel_region[2] + it->second.vec_panel_region[6],
          it->second.vec_panel_region[3] + it->second.vec_panel_region[7]};
}

std::expected<std::uint32_t, std::string> chrome_sheet_load(
    ChromeSheetSet& set, std::string_view str_image_name, std::span<const std::uint8_t> span1u_png) {
  const auto it{set.map_sheets.find(std::string{str_image_name})};
  if (it != set.map_sheets.end()) return it->second.u4_id;
  auto res_image{render::png_decode(span1u_png)};
  if (!res_image) return std::unexpected(res_image.error());
  const std::uint32_t u4_id{set.u4_next_id++};
  ChromeSheet sheet;
  sheet.u4_id = u4_id;
  sheet.int4_width = res_image->int4_width;
  sheet.int4_height = res_image->int4_height;
  sheet.vec_u4_argb = std::move(res_image->vec_u4_argb);
  set.map_sheets.emplace(std::string{str_image_name}, std::move(sheet));
  return u4_id;
}

const ChromeSheet* chrome_sheet_for_collection(
    const ChromeSheetSet& set, const std::map<std::string, ChromeCollection>& map_collections,
    std::string_view str_collection) {
  const auto it_col{map_collections.find(std::string{str_collection})};
  if (it_col == map_collections.end()) return nullptr;
  const auto it_sheet{set.map_sheets.find(it_col->second.str_image)};
  if (it_sheet == set.map_sheets.end()) return nullptr;
  return &it_sheet->second;
}

}  // namespace ora::game::ui
