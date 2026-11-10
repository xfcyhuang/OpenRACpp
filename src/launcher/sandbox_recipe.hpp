#pragma once

import std;

#include "game/map_bin.hpp"
#include "game/tileset.hpp"
#include "game/world/world.hpp"

namespace ora::launcher {

struct SandboxMapData {
  game::MapBin bin_map;
  game::Tileset tileset;
};

[[nodiscard]] std::string_view sandbox_rules_text();

[[nodiscard]] std::string_view sandbox_map_text();

[[nodiscard]] std::string_view sandbox_weapons_text();

[[nodiscard]] SandboxMapData sandbox_map_data();

struct LoadedMap {
  game::world::SyntheticMap map;
  game::MapBin bin_map;
  game::Tileset tileset;
  std::int32_t int4_left{};
  std::int32_t int4_top{};
  std::int32_t int4_right{};
  std::int32_t int4_bottom{};
  std::string str_title;
};

[[nodiscard]] std::expected<LoadedMap, std::string> load_builtin_sandbox();

[[nodiscard]] std::expected<LoadedMap, std::string> load_oramap(
    std::string_view str_path, std::string_view str_tileset_path = "data/temperat.yaml");

}
