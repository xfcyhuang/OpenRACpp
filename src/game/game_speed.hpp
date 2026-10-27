#pragma once
import std;

#include "yaml/mini_yaml.hpp"

namespace ora::game {

class GameSpeed final {
 public:

  std::string Name;
  std::int32_t Timestep{0};
  std::int32_t OrderLatency{0};
};

class GameSpeeds final {
 public:
  GameSpeeds() = default;

  explicit GameSpeeds(const yaml::MiniYaml& y);

  std::string DefaultSpeed;
  std::vector<std::pair<std::string, GameSpeed>> Speeds{};

  const GameSpeed* Find(std::string_view str_name) const;
};

}
