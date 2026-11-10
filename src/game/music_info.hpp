#pragma once

import std;

#include "game/miniyaml.hpp"

namespace ora::game {

struct music_info {
  std::string str_filename;
  std::string str_title;
  bool b_hidden{false};
  float f4_volume_modifier{1.0f};
  std::int32_t int4_length{0};
  bool b_exists{false};
};

[[nodiscard]] std::expected<music_info, std::string> music_info_from_yaml(
    std::string_view str_key, const MiniYamlNode& value);

[[nodiscard]] std::expected<std::vector<std::pair<std::string, music_info>>, std::string>
load_music_infos(std::string_view str_path);

void music_infos_probe_exists(std::vector<std::pair<std::string, music_info>>& vec_tracks,
                              std::string_view str_dir);

}
