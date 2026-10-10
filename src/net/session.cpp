import std;

#include "net/session.hpp"

#include "yaml/mini_yaml.hpp"

namespace ora::net {

namespace {

yaml::StringPool& SessionPool() {
  static yaml::StringPool pool_session;
  return pool_session;
}

[[noreturn]] void ThrowInvalidSession(std::string_view str_data) {
  throw yaml::YamlException(
      std::format("Session deserialized invalid MiniYaml:\n{}", str_data));
}

std::optional<int> ParseInt32(std::string_view sv) {
  int out = 0;
  auto [ptr, ec] = std::from_chars(sv.data(), sv.data() + sv.size(), out, 10);
  if (ec != std::errc{} || ptr != sv.data() + sv.size())
    return std::nullopt;
  return out;
}

bool ParseBool(std::string_view sv, bool& out) {
  if (sv == "True") {
    out = true;
    return true;
  }
  if (sv == "False") {
    out = false;
    return true;
  }
  return false;
}

std::optional<core::Color> ParseColorHex(std::string_view sv) {
  core::Color color{};
  if (!core::Color::TryParse(sv, color))
    return std::nullopt;
  return color;
}

std::string FormatBool(bool b) { return b ? "True" : "False"; }

std::string FormatColor(const core::Color& c) { return c.ToString(); }

std::string FormatClientState(ClientState s) {
  switch (s) {
    case ClientState::NotReady: return "NotReady";
    case ClientState::Invalid: return "Invalid";
    case ClientState::Ready: return "Ready";
    case ClientState::Disconnected: return "Disconnected";
  }
  return "Invalid";
}

bool ParseClientState(std::string_view sv, ClientState& out) {
  if (sv == "NotReady") { out = ClientState::NotReady; return true; }
  if (sv == "Invalid") { out = ClientState::Invalid; return true; }
  if (sv == "Ready") { out = ClientState::Ready; return true; }
  if (sv == "Disconnected") { out = ClientState::Disconnected; return true; }
  return false;
}

std::string FormatConnectionQuality(ConnectionQuality q) {
  switch (q) {
    case ConnectionQuality::Good: return "Good";
    case ConnectionQuality::Moderate: return "Moderate";
    case ConnectionQuality::Poor: return "Poor";
  }
  return "Good";
}

bool ParseConnectionQuality(std::string_view sv, ConnectionQuality& out) {
  if (sv == "Good") { out = ConnectionQuality::Good; return true; }
  if (sv == "Moderate") { out = ConnectionQuality::Moderate; return true; }
  if (sv == "Poor") { out = ConnectionQuality::Poor; return true; }
  return false;
}

std::string FormatMapStatus(int flags) {
  if (flags == 0)
    return "Unknown";
  std::string out;
  const auto append = [&out](std::string_view name) {
    if (!out.empty())
      out += ", ";
    out += name;
  };
  if ((flags & 1) != 0) append("Validating");
  if ((flags & 2) != 0) append("Playable");
  if ((flags & 4) != 0) append("Incompatible");
  if ((flags & 8) != 0) append("UnsafeCustomRules");
  return out;
}

bool ParseMapStatus(std::string_view sv, int& out) {
  out = 0;
  std::size_t pos = 0;
  while (pos <= sv.size()) {
    const std::size_t comma = sv.find(',', pos);
    const std::string_view part = sv.substr(
        pos, comma == std::string_view::npos ? std::string_view::npos : comma - pos);
    std::string_view trimmed = part;
    while (!trimmed.empty() && (trimmed.front() == ' ' || trimmed.front() == '\t'))
      trimmed.remove_prefix(1);
    while (!trimmed.empty() && (trimmed.back() == ' ' || trimmed.back() == '\t'))
      trimmed.remove_suffix(1);
    if (trimmed == "Unknown") {
    } else if (trimmed == "Validating") {
      out |= 1;
    } else if (trimmed == "Playable") {
      out |= 2;
    } else if (trimmed == "Incompatible") {
      out |= 4;
    } else if (trimmed == "UnsafeCustomRules") {
      out |= 8;
    } else {
      return false;
    }
    if (comma == std::string_view::npos)
      break;
    pos = comma + 1;
  }
  return true;
}

bool LoadClient(const yaml::MiniYaml& yaml_v, SessionClient& out) {
  for (const auto& node : yaml_v.Nodes) {
    const std::string_view key{*node.Key};
    const std::string text{node.Value.Value != nullptr ? *node.Value.Value : std::string{}};
    if (key == "Index") {
      const auto parsed = ParseInt32(text);
      if (!parsed) return false;
      out.Index = *parsed;
    } else if (key == "PreferredColor") {
      const auto parsed = ParseColorHex(text);
      if (!parsed) return false;
      out.PreferredColor = *parsed;
    } else if (key == "Color") {
      const auto parsed = ParseColorHex(text);
      if (!parsed) return false;
      out.Color = *parsed;
    } else if (key == "Faction") {
      out.Faction = text;
    } else if (key == "SpawnPoint") {
      const auto parsed = ParseInt32(text);
      if (!parsed) return false;
      out.SpawnPoint = *parsed;
    } else if (key == "Name") {
      out.Name = text;
    } else if (key == "AnonymizedIPAddress") {
      out.AnonymizedIPAddress = text;
    } else if (key == "Location") {
      out.Location = text;
    } else if (key == "ConnectionQuality") {
      if (!ParseConnectionQuality(text, out.ConnectionQualityKind)) return false;
    } else if (key == "State") {
      if (!ParseClientState(text, out.State)) return false;
    } else if (key == "Team") {
      const auto parsed = ParseInt32(text);
      if (!parsed) return false;
      out.Team = *parsed;
    } else if (key == "Handicap") {
      const auto parsed = ParseInt32(text);
      if (!parsed) return false;
      out.Handicap = *parsed;
    } else if (key == "Slot") {
      out.Slot = text;
    } else if (key == "Bot") {
      out.Bot = text;
    } else if (key == "BotControllerClientIndex") {
      const auto parsed = ParseInt32(text);
      if (!parsed) return false;
      out.BotControllerClientIndex = *parsed;
    } else if (key == "IsAdmin") {
      if (!ParseBool(text, out.IsAdmin)) return false;
    } else if (key == "Fingerprint") {
      out.Fingerprint = text;
    }
  }
  return true;
}

bool LoadSlot(const yaml::MiniYaml& yaml_v, SessionSlot& out) {
  for (const auto& node : yaml_v.Nodes) {
    const std::string_view key{*node.Key};
    const std::string text{node.Value.Value != nullptr ? *node.Value.Value : std::string{}};
    if (key == "PlayerReference") {
      out.PlayerReference = text;
    } else if (key == "Closed") {
      if (!ParseBool(text, out.Closed)) return false;
    } else if (key == "AllowBots") {
      if (!ParseBool(text, out.AllowBots)) return false;
    } else if (key == "LockFaction") {
      if (!ParseBool(text, out.LockFaction)) return false;
    } else if (key == "LockColor") {
      if (!ParseBool(text, out.LockColor)) return false;
    } else if (key == "LockTeam") {
      if (!ParseBool(text, out.LockTeam)) return false;
    } else if (key == "LockHandicap") {
      if (!ParseBool(text, out.LockHandicap)) return false;
    } else if (key == "LockSpawn") {
      if (!ParseBool(text, out.LockSpawn)) return false;
    } else if (key == "Required") {
      if (!ParseBool(text, out.Required)) return false;
    }
  }
  return true;
}

bool LoadLobbyOption(const yaml::MiniYaml& yaml_v, SessionLobbyOption& out) {
  for (const auto& node : yaml_v.Nodes) {
    const std::string_view key{*node.Key};
    const std::string text{node.Value.Value != nullptr ? *node.Value.Value : std::string{}};
    if (key == "Value") {
      out.Value = text;
    } else if (key == "PreferredValue") {
      out.PreferredValue = text;
    } else if (key == "IsLocked") {
      if (!ParseBool(text, out.IsLocked)) return false;
    }
  }
  return true;
}

bool LoadGlobal(const yaml::MiniYaml& yaml_v, SessionGlobalSettings& out) {
  for (const auto& node : yaml_v.Nodes) {
    const std::string_view key{*node.Key};
    const std::string text{node.Value.Value != nullptr ? *node.Value.Value : std::string{}};
    if (key == "ServerName") {
      out.ServerName = text;
    } else if (key == "Map") {
      out.Map = text;
    } else if (key == "MapStatus") {
      if (!ParseMapStatus(text, out.MapStatusFlags)) return false;
    } else if (key == "RandomSeed") {
      const auto parsed = ParseInt32(text);
      if (!parsed) return false;
      out.RandomSeed = *parsed;
    } else if (key == "AllowSpectators") {
      if (!ParseBool(text, out.AllowSpectators)) return false;
    } else if (key == "GameUid") {
      out.GameUid = text;
    } else if (key == "EnableSingleplayer") {
      if (!ParseBool(text, out.EnableSingleplayer)) return false;
    } else if (key == "EnableMapGeneration") {
      if (!ParseBool(text, out.EnableMapGeneration)) return false;
    } else if (key == "EnableGameSaves") {
      if (!ParseBool(text, out.EnableGameSaves)) return false;
    } else if (key == "EnableSyncReports") {
      if (!ParseBool(text, out.EnableSyncReports)) return false;
    } else if (key == "Dedicated") {
      if (!ParseBool(text, out.Dedicated)) return false;
    } else if (key == "NetFrameInterval") {
      const auto parsed = ParseInt32(text);
      if (!parsed) return false;
      out.NetFrameInterval = *parsed;
    } else if (key == "GameTimestep") {
      const auto parsed = ParseInt32(text);
      if (!parsed) return false;
      out.GameTimestep = *parsed;
    } else if (key == "Options") {
      for (const auto& option_node : node.Value.Nodes) {
        SessionLobbyOption option{};
        if (!LoadLobbyOption(option_node.Value, option))
          return false;
        out.map_lobby_options[*option_node.Key] = option;
      }
    }
  }
  return true;
}

yaml::MiniYamlNode MakeLeaf(yaml::StringPool& pool, std::string_view key,
                            std::string_view text) {
  return yaml::MiniYamlNode{pool.Intern(key), yaml::MiniYaml{pool.Intern(text)}};
}

yaml::MiniYamlNode SerializeClient(yaml::StringPool& pool, const SessionClient& c) {
  std::vector<yaml::MiniYamlNode> nodes;
  nodes.push_back(MakeLeaf(pool, "Index", std::to_string(c.Index)));
  nodes.push_back(MakeLeaf(pool, "PreferredColor", FormatColor(c.PreferredColor)));
  nodes.push_back(MakeLeaf(pool, "Color", FormatColor(c.Color)));
  nodes.push_back(MakeLeaf(pool, "Faction", c.Faction));
  nodes.push_back(MakeLeaf(pool, "SpawnPoint", std::to_string(c.SpawnPoint)));
  nodes.push_back(MakeLeaf(pool, "Name", c.Name));
  nodes.push_back(MakeLeaf(pool, "AnonymizedIPAddress", c.AnonymizedIPAddress));
  nodes.push_back(MakeLeaf(pool, "Location", c.Location));
  nodes.push_back(MakeLeaf(pool, "ConnectionQuality", FormatConnectionQuality(c.ConnectionQualityKind)));
  nodes.push_back(MakeLeaf(pool, "State", FormatClientState(c.State)));
  nodes.push_back(MakeLeaf(pool, "Team", std::to_string(c.Team)));
  nodes.push_back(MakeLeaf(pool, "Handicap", std::to_string(c.Handicap)));
  nodes.push_back(MakeLeaf(pool, "Slot", c.Slot));
  nodes.push_back(MakeLeaf(pool, "Bot", c.Bot));
  nodes.push_back(MakeLeaf(pool, "BotControllerClientIndex", std::to_string(c.BotControllerClientIndex)));
  nodes.push_back(MakeLeaf(pool, "IsAdmin", FormatBool(c.IsAdmin)));
  nodes.push_back(MakeLeaf(pool, "Fingerprint", c.Fingerprint));
  return yaml::MiniYamlNode{
      pool.Intern(std::format("Client@{}", c.Index)),
      yaml::MiniYaml{nullptr, std::move(nodes)}};
}

yaml::MiniYamlNode SerializeSlot(yaml::StringPool& pool, const SessionSlot& s) {
  std::vector<yaml::MiniYamlNode> nodes;
  nodes.push_back(MakeLeaf(pool, "PlayerReference", s.PlayerReference));
  nodes.push_back(MakeLeaf(pool, "Closed", FormatBool(s.Closed)));
  nodes.push_back(MakeLeaf(pool, "AllowBots", FormatBool(s.AllowBots)));
  nodes.push_back(MakeLeaf(pool, "LockFaction", FormatBool(s.LockFaction)));
  nodes.push_back(MakeLeaf(pool, "LockColor", FormatBool(s.LockColor)));
  nodes.push_back(MakeLeaf(pool, "LockTeam", FormatBool(s.LockTeam)));
  nodes.push_back(MakeLeaf(pool, "LockHandicap", FormatBool(s.LockHandicap)));
  nodes.push_back(MakeLeaf(pool, "LockSpawn", FormatBool(s.LockSpawn)));
  nodes.push_back(MakeLeaf(pool, "Required", FormatBool(s.Required)));
  return yaml::MiniYamlNode{
      pool.Intern("Slot@" + s.PlayerReference),
      yaml::MiniYaml{nullptr, std::move(nodes)}};
}

yaml::MiniYamlNode SerializeGlobal(yaml::StringPool& pool,
                                   const SessionGlobalSettings& g) {
  std::vector<yaml::MiniYamlNode> nodes;
  nodes.push_back(MakeLeaf(pool, "ServerName", g.ServerName));
  nodes.push_back(MakeLeaf(pool, "Map", g.Map));
  nodes.push_back(MakeLeaf(pool, "MapStatus", FormatMapStatus(g.MapStatusFlags)));
  nodes.push_back(MakeLeaf(pool, "RandomSeed", std::to_string(g.RandomSeed)));
  nodes.push_back(MakeLeaf(pool, "AllowSpectators", FormatBool(g.AllowSpectators)));
  nodes.push_back(MakeLeaf(pool, "GameUid", g.GameUid));
  nodes.push_back(MakeLeaf(pool, "EnableSingleplayer", FormatBool(g.EnableSingleplayer)));
  nodes.push_back(MakeLeaf(pool, "EnableMapGeneration", FormatBool(g.EnableMapGeneration)));
  nodes.push_back(MakeLeaf(pool, "EnableGameSaves", FormatBool(g.EnableGameSaves)));
  nodes.push_back(MakeLeaf(pool, "EnableSyncReports", FormatBool(g.EnableSyncReports)));
  nodes.push_back(MakeLeaf(pool, "Dedicated", FormatBool(g.Dedicated)));
  nodes.push_back(MakeLeaf(pool, "NetFrameInterval", std::to_string(g.NetFrameInterval)));
  nodes.push_back(MakeLeaf(pool, "GameTimestep", std::to_string(g.GameTimestep)));
  if (!g.map_lobby_options.empty()) {
    std::vector<yaml::MiniYamlNode> option_nodes;
    for (const auto& [name, option] : g.map_lobby_options) {
      std::vector<yaml::MiniYamlNode> fields;
      fields.push_back(MakeLeaf(pool, "Value", option.Value));
      fields.push_back(MakeLeaf(pool, "PreferredValue", option.PreferredValue));
      fields.push_back(MakeLeaf(pool, "IsLocked", FormatBool(option.IsLocked)));
      option_nodes.emplace_back(pool.Intern(name),
                                yaml::MiniYaml{nullptr, std::move(fields)});
    }
    nodes.emplace_back(pool.Intern("Options"),
                       yaml::MiniYaml{nullptr, std::move(option_nodes)});
  }
  return yaml::MiniYamlNode{pool.Intern("GlobalSettings"),
                            yaml::MiniYaml{nullptr, std::move(nodes)}};
}

}  // namespace

Session Session::Deserialize(std::string_view str_data,
                             std::string_view str_name) {
  const std::vector<yaml::MiniYamlNode> vec_nodes =
      yaml::MiniYaml::FromString(str_data, str_name);

  Session session;
  for (const auto& node : vec_nodes) {
    const std::string_view key{*node.Key};
    const std::size_t at = key.find('@');
    const std::string_view head =
        at == std::string_view::npos ? key : key.substr(0, at);
    if (head == "Client") {
      SessionClient client{};
      if (!LoadClient(node.Value, client))
        ThrowInvalidSession(str_data);
      session.vec_clients.push_back(std::move(client));
    } else if (head == "GlobalSettings") {
      SessionGlobalSettings global{};
      if (!LoadGlobal(node.Value, global))
        ThrowInvalidSession(str_data);
      session.global_settings = std::move(global);
    } else if (head == "Slot") {
      SessionSlot slot{};
      if (!LoadSlot(node.Value, slot))
        ThrowInvalidSession(str_data);
      session.vec_slots.emplace_back(slot.PlayerReference, std::move(slot));
    } else if (key == "DisabledSpawnPoints") {
      const std::string text{
          node.Value.Value != nullptr ? *node.Value.Value : std::string{}};
      std::size_t pos = 0;
      while (pos <= text.size()) {
        const std::size_t comma = text.find(',', pos);
        const std::string part = text.substr(
            pos, comma == std::string::npos ? std::string::npos : comma - pos);
        if (!part.empty()) {
          const auto parsed = ParseInt32(part);
          if (!parsed)
            ThrowInvalidSession(str_data);
          session.vec_disabled_spawn_points.push_back(*parsed);
        }
        if (comma == std::string::npos)
          break;
        pos = comma + 1;
      }
    }
  }

  return session;
}

std::string Session::Serialize() const {
  yaml::StringPool& pool = SessionPool();
  std::vector<yaml::MiniYamlNode> vec_nodes;

  std::string str_disabled;
  for (std::size_t i = 0; i < vec_disabled_spawn_points.size(); ++i) {
    if (i != 0)
      str_disabled += ", ";
    str_disabled += std::to_string(vec_disabled_spawn_points[i]);
  }
  vec_nodes.push_back(
      MakeLeaf(pool, "DisabledSpawnPoints", str_disabled));

  for (const auto& client : vec_clients)
    vec_nodes.push_back(SerializeClient(pool, client));

  for (const auto& [name, slot] : vec_slots)
    vec_nodes.push_back(SerializeSlot(pool, slot));

  vec_nodes.push_back(SerializeGlobal(pool, global_settings));

  return yaml::WriteToString(vec_nodes);
}

}  // namespace ora::net
