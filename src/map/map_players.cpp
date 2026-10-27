import std;

#include "map/map_players.hpp"

#include "meta/field_loader.hpp"
#include "meta/parse.hpp"

namespace ora::map {

namespace {

std::string_view ValueOf(const yaml::MiniYamlNode& node) {
  return node.Value.Value != nullptr ? std::string_view{*node.Value.Value}
                                     : std::string_view{};
}

}

PlayerReference::PlayerReference(const yaml::MiniYaml& my,
                                 core::Color color_default_fallback)
    : Color{color_default_fallback} {

  if (const auto* node = my.NodeWithKeyOrDefault("Name")) {
    if (node->Value.Value != nullptr)
      Name = *node->Value.Value;
  }
  if (const auto* node = my.NodeWithKeyOrDefault("Palette")) {
    if (node->Value.Value != nullptr)
      Palette = *node->Value.Value;
  }
  if (const auto* node = my.NodeWithKeyOrDefault("Bot")) {
    if (node->Value.Value != nullptr)
      Bot = *node->Value.Value;
  }
  if (const auto* node = my.NodeWithKeyOrDefault("StartingUnitsClass")) {
    if (node->Value.Value != nullptr)
      StartingUnitsClass = *node->Value.Value;
  }
  if (const auto* node = my.NodeWithKeyOrDefault("AllowBots"))
    AllowBots = meta::GetBoolValue("AllowBots", ValueOf(*node));
  if (const auto* node = my.NodeWithKeyOrDefault("Playable"))
    Playable = meta::GetBoolValue("Playable", ValueOf(*node));
  if (const auto* node = my.NodeWithKeyOrDefault("Required"))
    Required = meta::GetBoolValue("Required", ValueOf(*node));
  if (const auto* node = my.NodeWithKeyOrDefault("OwnsWorld"))
    OwnsWorld = meta::GetBoolValue("OwnsWorld", ValueOf(*node));
  if (const auto* node = my.NodeWithKeyOrDefault("Spectating"))
    Spectating = meta::GetBoolValue("Spectating", ValueOf(*node));
  if (const auto* node = my.NodeWithKeyOrDefault("NonCombatant"))
    NonCombatant = meta::GetBoolValue("NonCombatant", ValueOf(*node));
  if (const auto* node = my.NodeWithKeyOrDefault("LockFaction"))
    LockFaction = meta::GetBoolValue("LockFaction", ValueOf(*node));
  if (const auto* node = my.NodeWithKeyOrDefault("Faction")) {
    if (node->Value.Value != nullptr)
      Faction = *node->Value.Value;
  }
  if (const auto* node = my.NodeWithKeyOrDefault("LockColor"))
    LockColor = meta::GetBoolValue("LockColor", ValueOf(*node));
  if (const auto* node = my.NodeWithKeyOrDefault("Color"))
    Color = meta::GetColorValue("Color", ValueOf(*node));
  if (const auto* node = my.NodeWithKeyOrDefault("HomeLocation")) {

    const int2 uv = meta::GetSizeValue("HomeLocation", ValueOf(*node));
    HomeLocation = CPos{uv.X, uv.Y};
  }
  if (const auto* node = my.NodeWithKeyOrDefault("LockSpawn"))
    LockSpawn = meta::GetBoolValue("LockSpawn", ValueOf(*node));
  if (const auto* node = my.NodeWithKeyOrDefault("Spawn"))
    Spawn = meta::GetInt32Value("Spawn", ValueOf(*node));
  if (const auto* node = my.NodeWithKeyOrDefault("LockTeam"))
    LockTeam = meta::GetBoolValue("LockTeam", ValueOf(*node));
  if (const auto* node = my.NodeWithKeyOrDefault("Team"))
    Team = meta::GetInt32Value("Team", ValueOf(*node));
  if (const auto* node = my.NodeWithKeyOrDefault("LockHandicap"))
    LockHandicap = meta::GetBoolValue("LockHandicap", ValueOf(*node));
  if (const auto* node = my.NodeWithKeyOrDefault("Handicap"))
    Handicap = meta::GetInt32Value("Handicap", ValueOf(*node));
  if (const auto* node = my.NodeWithKeyOrDefault("Allies")) {
    if (node->Value.Value != nullptr)
      Allies = meta::GetStringArrayValue("Allies", ValueOf(*node));
  }
  if (const auto* node = my.NodeWithKeyOrDefault("Enemies")) {
    if (node->Value.Value != nullptr)
      Enemies = meta::GetStringArrayValue("Enemies", ValueOf(*node));
  }
}

MapPlayers::MapPlayers(std::span<const yaml::MiniYamlNode> player_definitions,
                       core::Color color_default_fallback) {

  for (const yaml::MiniYamlNode& node : player_definitions) {
    yaml::MiniYaml value_yaml;
    value_yaml.Nodes = node.Value.Nodes;
    PlayerReference pr{value_yaml, color_default_fallback};

    if (Find(pr.Name) != nullptr)
      throw std::runtime_error(
          "An item with the same key has already been added. Key: " + pr.Name);
    Players.emplace_back(pr.Name, std::move(pr));
  }
}

const PlayerReference* MapPlayers::Find(std::string_view name) const {
  for (const auto& [key, pr] : Players)
    if (key == name)
      return &pr;
  return nullptr;
}

}
