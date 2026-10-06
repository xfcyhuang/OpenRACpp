// UPSTREAM: OpenRA.Game/GameSpeed.cs @b6fc03f(game_speed.hpp 的实现)
//          The implementation of game_speed.hpp.
import std;

#include "game/game_speed.hpp"

#include "meta/field_loader.hpp"
#include "meta/parse.hpp"

namespace ora::game {

namespace {

std::string_view ValueOf(const yaml::MiniYamlNode& node) {
  return node.Value.Value != nullptr ? std::string_view{*node.Value.Value}
                                     : std::string_view{};
}

GameSpeed LoadGameSpeed(const yaml::MiniYaml& y) {
  // FieldLoader.Load<GameSpeed>(y):Name/Timestep/OrderLatency 全 Required
  GameSpeed speed;
  const yaml::MiniYamlNode* name_node = y.NodeWithKeyOrDefault("Name");
  const yaml::MiniYamlNode* timestep_node = y.NodeWithKeyOrDefault("Timestep");
  const yaml::MiniYamlNode* latency_node =
      y.NodeWithKeyOrDefault("OrderLatency");

  std::vector<std::string> vec_missing;
  if (name_node == nullptr)
    vec_missing.push_back("Name");
  if (timestep_node == nullptr)
    vec_missing.push_back("Timestep");
  if (latency_node == nullptr)
    vec_missing.push_back("OrderLatency");
  if (!vec_missing.empty())
    throw meta::MissingFieldsException(std::move(vec_missing));

  speed.Name = std::string{ValueOf(*name_node)};
  speed.Timestep = meta::GetInt32Value("Timestep", ValueOf(*timestep_node));
  speed.OrderLatency =
      meta::GetInt32Value("OrderLatency", ValueOf(*latency_node));
  return speed;
}

}  // namespace

GameSpeeds::GameSpeeds(const yaml::MiniYaml& y) {
  // [FieldLoader.Require] DefaultSpeed(L33)先行;缺字段 MissingFieldsException
  if (const yaml::MiniYamlNode* node = y.NodeWithKeyOrDefault("DefaultSpeed")) {
    DefaultSpeed = std::string{ValueOf(*node)};
  } else {
    throw meta::MissingFieldsException({"DefaultSpeed"});
  }

  // LoadSpeeds(L39-60):Speeds 节点缺失 → YamlException 同文本;逐项
  // try/catch 包装(单缺失/多缺失头面)
  const yaml::MiniYamlNode* speeds_node = y.NodeWithKeyOrDefault("Speeds");
  if (speeds_node == nullptr)
    throw yaml::YamlException("Error parsing GameSpeeds: Missing Speeds node!");

  for (const yaml::MiniYamlNode& node : speeds_node->Value.Nodes) {
    try {
      GameSpeed speed = LoadGameSpeed(node.Value);
      // 字典键 = yaml 节点键(ret.Add(node.Key, ...),L50)—— 非 Name 字段
      // The dictionary key = the yaml node key (ret.Add(node.Key, ...), L50)
      // — not the Name field.
      const std::string str_key =
          node.Key != nullptr ? std::string{*node.Key} : std::string{};
      if (Find(str_key) != nullptr)
        throw std::runtime_error(
            "An item with the same key has already been added. Key: " +
            str_key);
      Speeds.emplace_back(str_key, std::move(speed));
    } catch (meta::MissingFieldsException& e) {
      // L52-56:单/多缺失头部 | L52-56: the single/multiple-missing header.
      const std::string_view label =
          e.Missing().size() > 1 ? "Required properties missing"
                                 : "Required property missing";
      throw yaml::YamlException(std::format(
          "Error parsing GameSpeed {}: {}: {}", node.Key != nullptr
                                                   ? std::string_view{*node.Key}
                                                   : std::string_view{},
          label, e.what()));
    }
  }
}

const GameSpeed* GameSpeeds::Find(std::string_view str_name) const {
  for (const auto& [key, speed] : Speeds)
    if (key == str_name)
      return &speed;
  return nullptr;
}

}  // namespace ora::game
