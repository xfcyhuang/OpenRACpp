#pragma once

import std;

#include "game/filesystem/zip_read.hpp"
#include "game/map_bin.hpp"
#include "game/tileset.hpp"

namespace ora::game {

struct MapHeader {
  int int4_format{};
  std::string str_requires_mod;
  std::string str_title;
  std::string str_author;
  std::string str_tileset;
  int int4_size_x{};
  int int4_size_y{};
  int int4_bounds_x{};
  int int4_bounds_y{};
  int int4_bounds_w{};
  int int4_bounds_h{};
  std::uint32_t u4_visibility{};
  std::vector<std::string> vec_str_categories;
  std::size_t sz_players{};
  std::size_t sz_actors{};
};

[[nodiscard]] std::expected<MapHeader, std::string> map_header_parse(
    std::span<const std::uint8_t> span1u_yaml_bytes, std::string_view str_source_name);

[[nodiscard]] std::expected<int, std::string> map_scan_format(
    std::span<const std::uint8_t> span1u_yaml_bytes);

[[nodiscard]] std::expected<std::string, std::string> map_uid(const filesystem::ZipArchive& archive_zip);

[[nodiscard]] std::expected<std::string, std::string> map_info_dump(
    std::span<const std::uint8_t> span1u_oramap_bytes, std::string_view str_tileset_dir);

}
