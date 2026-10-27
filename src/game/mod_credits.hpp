#pragma once
import std;

#include "yaml/mini_yaml.hpp"

namespace ora::game {

struct ModCredits {
  std::string str_mod_tab_title = "Game";
  std::string str_mod_credits_file;
  std::string str_engine_credits_file = "^EngineDir|AUTHORS";

  bool HasModCreditsFile() const { return !str_mod_credits_file.empty(); }
};

ModCredits ModCreditsFromYaml(const yaml::MiniYaml* ptr_node);

}
