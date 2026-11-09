#pragma once

import std;

#include "game/miniyaml.hpp"
#include "game/render/png_decode.hpp"

namespace ora::game::ui {

[[nodiscard]] std::expected<std::map<std::string, std::string>, std::string> chrome_metrics_initialize(
    const std::vector<std::vector<MiniYamlNode>>& vec_sources);

struct ChromeRect {
  int int4_x{};
  int int4_y{};
  int int4_width{};
  int int4_height{};
};

struct ChromeCollection {
  std::string str_image;
  std::string str_image2x;
  std::string str_image3x;
  std::vector<int> vec_panel_region;
  std::uint32_t u4_panel_sides{0x1F};
  std::map<std::string, ChromeRect> map_regions;
};

[[nodiscard]] std::expected<std::map<std::string, ChromeCollection>, std::string>
chrome_provider_initialize(const std::vector<std::vector<MiniYamlNode>>& vec_sources);

[[nodiscard]] std::optional<ChromeRect> chrome_try_get_image(
    const std::map<std::string, ChromeCollection>& map_collections, std::string_view str_collection,
    std::string_view str_image);

[[nodiscard]] std::vector<std::optional<ChromeRect>> chrome_get_panel_images(
    const std::map<std::string, ChromeCollection>& map_collections, std::string_view str_collection);

[[nodiscard]] std::pair<int, int> chrome_get_minimum_panel_size(
    const std::map<std::string, ChromeCollection>& map_collections, std::string_view str_collection);

struct ChromeSheet {
  std::uint32_t u4_id{};
  int int4_width{};
  int int4_height{};
  std::vector<std::uint32_t> vec_u4_argb;
};

struct ChromeSheetSet {
  std::map<std::string, ChromeSheet> map_sheets;
  std::uint32_t u4_next_id{1};
};

[[nodiscard]] std::expected<std::uint32_t, std::string> chrome_sheet_load(
    ChromeSheetSet& set, std::string_view str_image_name, std::span<const std::uint8_t> span1u_png);

[[nodiscard]] const ChromeSheet* chrome_sheet_for_collection(
    const ChromeSheetSet& set, const std::map<std::string, ChromeCollection>& map_collections,
    std::string_view str_collection);

}
