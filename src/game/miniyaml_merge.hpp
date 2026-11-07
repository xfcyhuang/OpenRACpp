#pragma once

import std;

#include "game/miniyaml.hpp"

namespace ora::game {

[[nodiscard]] std::expected<std::vector<MiniYamlNode>, std::string> miniyaml_merge(
    const std::vector<std::vector<MiniYamlNode>>& vec_sources);

}
