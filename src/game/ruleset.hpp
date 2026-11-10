#pragma once

import std;

#include "game/miniyaml.hpp"
#include "game/miniyaml_merge.hpp"

namespace ora::game {

[[nodiscard]] std::expected<std::vector<std::string>, std::string> manifest_rules_files(
    std::string_view str_mod_yaml_path);

struct ActorRules {
  std::string str_name;
  std::vector<std::string> vec_trait_keys;
};

struct RulesetModel {
  std::vector<ActorRules> vec_actors;
};

[[nodiscard]] std::expected<RulesetModel, std::string> ruleset_load_minimal(
    const std::vector<std::pair<std::string, std::string>>& vec_manifest_entries);

}
