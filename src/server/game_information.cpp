// UPSTREAM: OpenRA.Game/GameInformation.cs @e180e3225 L71-121(双节 yaml
//          序列化全文;FieldLoader/FieldSaver → 按名手写表)
//          GameInformation.cs L71-121 (the whole two-node yaml
//          serialization; FieldLoader/FieldSaver → the by-name hand-written
//          tables).
import std;

#include "server/game_information.hpp"

#include "yaml/mini_yaml.hpp"

namespace ora::server {

namespace {

yaml::MiniYamlNode MakeInfoLeaf(yaml::StringPool& pool,
                                std::string_view str_key,
                                std::string_view str_text) {
  return yaml::MiniYamlNode{pool.Intern(str_key),
                            yaml::MiniYaml{pool.Intern(str_text)}};
}

std::string FormatWinState(WinState s) {
  switch (s) {
    case WinState::Undefined: return "Undefined";
    case WinState::Won: return "Won";
    case WinState::Lost: return "Lost";
  }
  return "Undefined";
}

std::optional<WinState> ParseWinState(std::string_view str_text) {
  if (str_text == "Undefined")
    return WinState::Undefined;
  if (str_text == "Won")
    return WinState::Won;
  if (str_text == "Lost")
    return WinState::Lost;
  return std::nullopt;
}

std::vector<yaml::MiniYamlNode> SerializePlayerFields(
    yaml::StringPool& pool, const GameInformationPlayer& p) {
  auto vec_fields = std::vector<yaml::MiniYamlNode>{};
  vec_fields.push_back(
      MakeInfoLeaf(pool, "ClientIndex", std::to_string(p.ClientIndex)));
  vec_fields.push_back(MakeInfoLeaf(pool, "Name", p.Name));
  vec_fields.push_back(MakeInfoLeaf(pool, "IsHuman", p.IsHuman ? "True" : "False"));
  vec_fields.push_back(MakeInfoLeaf(pool, "IsBot", p.IsBot ? "True" : "False"));
  vec_fields.push_back(MakeInfoLeaf(pool, "BotType", p.BotType));
  vec_fields.push_back(MakeInfoLeaf(pool, "FactionName", p.FactionName));
  vec_fields.push_back(MakeInfoLeaf(pool, "FactionId", p.FactionId));
  vec_fields.push_back(MakeInfoLeaf(pool, "Color", p.Color.ToString()));
  vec_fields.push_back(
      MakeInfoLeaf(pool, "DisplayFactionName", p.DisplayFactionName));
  vec_fields.push_back(
      MakeInfoLeaf(pool, "DisplayFactionId", p.DisplayFactionId));
  vec_fields.push_back(MakeInfoLeaf(pool, "Team", std::to_string(p.Team)));
  vec_fields.push_back(
      MakeInfoLeaf(pool, "SpawnPoint", std::to_string(p.SpawnPoint)));
  vec_fields.push_back(
      MakeInfoLeaf(pool, "Handicap", std::to_string(p.Handicap)));
  vec_fields.push_back(MakeInfoLeaf(
      pool, "IsRandomFaction", p.IsRandomFaction ? "True" : "False"));
  vec_fields.push_back(MakeInfoLeaf(
      pool, "IsRandomSpawnPoint", p.IsRandomSpawnPoint ? "True" : "False"));
  vec_fields.push_back(MakeInfoLeaf(pool, "Fingerprint", p.Fingerprint));
  vec_fields.push_back(MakeInfoLeaf(pool, "Outcome", FormatWinState(p.Outcome)));
  vec_fields.push_back(
      MakeInfoLeaf(pool, "OutcomeTimestampUtc", p.OutcomeTimestampUtc.str_text));
  vec_fields.push_back(
      MakeInfoLeaf(pool, "DisconnectFrame", std::to_string(p.DisconnectFrame)));
  return vec_fields;
}

bool ParseIntField(std::string_view str_text, int& int4_out) {
  const auto parsed =
      std::from_chars(str_text.data(), str_text.data() + str_text.size(), int4_out);
  return parsed.ec == std::errc{};
}

std::optional<GameInformationPlayer> DeserializePlayer(
    const yaml::MiniYaml& yaml_v) {
  auto p = GameInformationPlayer{};
  for (const auto& node : yaml_v.Nodes) {
    const std::string_view key{*node.Key};
    const std::string text{
        node.Value.Value != nullptr ? *node.Value.Value : std::string{}};
    if (key == "ClientIndex") {
      if (!ParseIntField(text, p.ClientIndex))
        return std::nullopt;
    } else if (key == "Name")
      p.Name = text;
    else if (key == "IsHuman")
      p.IsHuman = text == "True";
    else if (key == "IsBot")
      p.IsBot = text == "True";
    else if (key == "BotType")
      p.BotType = text;
    else if (key == "FactionName")
      p.FactionName = text;
    else if (key == "FactionId")
      p.FactionId = text;
    else if (key == "Color") {
      core::Color color{};
      if (!core::Color::TryParse(text, color))
        return std::nullopt;
      p.Color = color;
    }
    else if (key == "DisplayFactionName")
      p.DisplayFactionName = text;
    else if (key == "DisplayFactionId")
      p.DisplayFactionId = text;
    else if (key == "Team") {
      if (!ParseIntField(text, p.Team))
        return std::nullopt;
    } else if (key == "SpawnPoint") {
      if (!ParseIntField(text, p.SpawnPoint))
        return std::nullopt;
    } else if (key == "Handicap") {
      if (!ParseIntField(text, p.Handicap))
        return std::nullopt;
    } else if (key == "IsRandomFaction")
      p.IsRandomFaction = text == "True";
    else if (key == "IsRandomSpawnPoint")
      p.IsRandomSpawnPoint = text == "True";
    else if (key == "Fingerprint")
      p.Fingerprint = text;
    else if (key == "Outcome") {
      const auto parsed = ParseWinState(text);
      if (!parsed)
        return std::nullopt;
      p.Outcome = *parsed;
    } else if (key == "OutcomeTimestampUtc")
      p.OutcomeTimestampUtc.str_text = text;
    else if (key == "DisconnectFrame") {
      if (!ParseIntField(text, p.DisconnectFrame))
        return std::nullopt;
    }
  }
  return p;
}

}  // namespace

GameDateTime GameDateTime::UtcNow() {
  const auto now = std::chrono::system_clock::now();
  const auto days = std::chrono::floor<std::chrono::days>(now);
  const auto ymd = std::chrono::year_month_day{days};
  const auto hms = std::chrono::hh_mm_ss{
      std::chrono::floor<std::chrono::seconds>(now - days)};

  auto dt = GameDateTime{};
  dt.str_text = std::format("{:04}-{:02}-{:02} {:02}-{:02}-{:02}",
                            static_cast<int>(ymd.year()),
                            static_cast<unsigned>(ymd.month()),
                            static_cast<unsigned>(ymd.day()),
                            hms.hours().count(), hms.minutes().count(),
                            hms.seconds().count());
  return dt;
}

std::string GameInformation::Serialize() const {
  auto pool = yaml::StringPool{};

  auto str_disabled = std::string{};
  for (std::size_t i = 0; i < vec_disabled_spawn_points.size(); ++i) {
    if (i != 0)
      str_disabled += ", ";
    str_disabled += std::to_string(vec_disabled_spawn_points[i]);
  }

  auto vec_root_fields = std::vector<yaml::MiniYamlNode>{};
  vec_root_fields.push_back(MakeInfoLeaf(pool, "Mod", Mod));
  vec_root_fields.push_back(MakeInfoLeaf(pool, "Version", Version));
  vec_root_fields.push_back(MakeInfoLeaf(pool, "MapUid", MapUid));
  vec_root_fields.push_back(MakeInfoLeaf(pool, "MapTitle", MapTitle));
  vec_root_fields.push_back(
      MakeInfoLeaf(pool, "FinalGameTick", std::to_string(FinalGameTick)));
  vec_root_fields.push_back(
      MakeInfoLeaf(pool, "StartTimeUtc", StartTimeUtc.str_text));
  vec_root_fields.push_back(
      MakeInfoLeaf(pool, "EndTimeUtc", EndTimeUtc.str_text));
  vec_root_fields.push_back(
      MakeInfoLeaf(pool, "DisabledSpawnPoints", str_disabled));

  auto vec_root = std::vector<yaml::MiniYamlNode>{};
  vec_root.push_back(yaml::MiniYamlNode{
      pool.Intern("Root"), yaml::MiniYaml{nullptr, std::move(vec_root_fields)}});

  for (std::size_t i = 0; i < vec_players.size(); ++i)
    vec_root.push_back(yaml::MiniYamlNode{
        pool.Intern(std::format("Player@{}", i)),
        yaml::MiniYaml{nullptr, SerializePlayerFields(pool, vec_players[i])}});

  return yaml::WriteToString(vec_root);
}

std::optional<GameInformation> GameInformation::Deserialize(
    std::string_view str_data, std::string_view str_path) {
  const auto vec_nodes = yaml::MiniYaml::FromString(str_data, str_path);

  auto info = GameInformation{};
  for (const auto& node : vec_nodes) {
    const std::string_view key{*node.Key};
    const auto at = key.find('@');
    const std::string_view head =
        at == std::string_view::npos ? key : key.substr(0, at);

    if (head == "Root") {
      for (const auto& field : node.Value.Nodes) {
        const std::string_view field_key{*field.Key};
        const std::string text{field.Value.Value != nullptr
                                   ? *field.Value.Value
                                   : std::string{}};
        if (field_key == "Mod")
          info.Mod = text;
        else if (field_key == "Version")
          info.Version = text;
        else if (field_key == "MapUid")
          info.MapUid = text;
        else if (field_key == "MapTitle")
          info.MapTitle = text;
        else if (field_key == "FinalGameTick") {
          if (!ParseIntField(text, info.FinalGameTick))
            return std::nullopt;
        } else if (field_key == "StartTimeUtc")
          info.StartTimeUtc.str_text = text;
        else if (field_key == "EndTimeUtc")
          info.EndTimeUtc.str_text = text;
        else if (field_key == "DisabledSpawnPoints") {
          info.vec_disabled_spawn_points.clear();
          auto pos = std::size_t{0};
          while (pos < text.size()) {
            const auto end = std::min(text.find(',', pos), text.size());
            auto start = pos;
            while (start < end && text[start] == ' ')
              ++start;
            auto stop = end;
            while (stop > start && text[stop - 1] == ' ')
              --stop;
            if (stop > start) {
              int value = 0;
              if (!ParseIntField(
                      std::string_view{text}.substr(start, stop - start), value))
                return std::nullopt;
              info.vec_disabled_spawn_points.push_back(value);
            }
            pos = end + 1;
          }
        }
      }
    } else if (head == "Player") {
      auto player = DeserializePlayer(node.Value);
      if (!player)
        return std::nullopt;
      info.vec_players.push_back(std::move(*player));
    }
  }

  return info;
}

}  // namespace ora::server
