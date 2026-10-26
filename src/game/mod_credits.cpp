// UPSTREAM: OpenRA.Mods.Common/ModCredits.cs @1f594e320(值面装载)。
import std;

#include "game/mod_credits.hpp"

namespace ora::game {

ModCredits ModCreditsFromYaml(const yaml::MiniYaml* ptr_node) {
  ModCredits credits_result{};
  if (ptr_node == nullptr)
    return credits_result;

  for (const yaml::MiniYamlNode& node : ptr_node->Nodes) {
    if (node.Key == nullptr)
      continue;
    const std::string_view sv_key = *node.Key;
    const std::string str_value =
        node.Value.Value != nullptr ? *node.Value.Value : std::string{};
    if (sv_key == "ModTabTitle")
      credits_result.str_mod_tab_title = str_value;
    else if (sv_key == "ModCreditsFile")
      credits_result.str_mod_credits_file = str_value;
    else if (sv_key == "EngineCreditsFile")
      credits_result.str_engine_credits_file = str_value;
  }
  return credits_result;
}

}  // namespace ora::game
