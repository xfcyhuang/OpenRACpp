#include "server/game_server.hpp"

#include "meta/parse.hpp"
#include "server/server.hpp"
#include "yaml/mini_yaml.hpp"

namespace ora::server {

GameServerClientDeps& GameServerClientDepsRef() {
  static GameServerClientDeps deps{};
  return deps;
}

GameClientData::GameClientData(const net::SessionClient& c) {
  Name = c.Name;
  Fingerprint = c.Fingerprint;
  Color = c.Color;
  Faction = c.Faction;
  Team = c.Team;
  SpawnPoint = c.SpawnPoint;
  IsAdmin = c.IsAdmin;
  IsSpectator = c.Slot.empty() && c.Bot.empty();
  IsBot = c.IsBot();
}

namespace {

std::string FormatBool(bool b_v) {
  return b_v ? "True" : "False";
}

std::string FormatIntCollection(const std::vector<int>& vec) {
  auto str_out = std::string{};
  for (std::size_t sz_i = 0; sz_i < vec.size(); ++sz_i) {
    if (sz_i > 0)
      str_out += ", ";
    str_out += std::to_string(vec[sz_i]);
  }
  return str_out;
}

yaml::MiniYamlNode MakeLeaf(yaml::StringPool& pool, std::string_view key,
                            std::string_view text) {
  return yaml::MiniYamlNode{pool.Intern(key),
                            yaml::MiniYaml{pool.Intern(text)}};
}

bool IsClientKey(std::string_view sv_key) {
  constexpr std::string_view kPrefix = "Client@";
  if (!sv_key.starts_with(kPrefix))
    return false;
  const std::string_view sv_digits = sv_key.substr(kPrefix.size());
  if (sv_digits.empty())
    return false;
  for (const char c : sv_digits)
    if (c < '0' || c > '9')
      return false;
  return true;
}

std::optional<std::int64_t> TryParseUtcSeconds(std::string_view sv) {
  auto digits = [](std::string_view sv_part, int int4_count) {
    if (sv_part.size() < static_cast<std::size_t>(int4_count))
      return -1;
    int int4_value = 0;
    for (int int4_i = 0; int4_i < int4_count; int4_i++) {
      const char c = sv_part[int4_i];
      if (c < '0' || c > '9')
        return -1;
      int4_value = int4_value * 10 + (c - '0');
    }
    return int4_value;
  };

  if (sv.size() < 19)
    return std::nullopt;
  const int int4_year = digits(sv.substr(0, 4), 4);
  if (sv[4] != '-' || int4_year < 0)
    return std::nullopt;
  const int int4_month = digits(sv.substr(5, 2), 2);
  if (sv[7] != '-' || int4_month <= 0)
    return std::nullopt;
  const int int4_day = digits(sv.substr(8, 2), 2);
  if ((sv[10] != 'T' && sv[10] != ' ') || int4_day <= 0)
    return std::nullopt;
  const int int4_hour = digits(sv.substr(11, 2), 2);
  if (sv[13] != ':' || int4_hour < 0)
    return std::nullopt;
  const int int4_minute = digits(sv.substr(14, 2), 2);
  if (sv[16] != ':' || int4_minute < 0)
    return std::nullopt;
  const int int4_second = digits(sv.substr(17, 2), 2);
  if (int4_second < 0)
    return std::nullopt;

  std::size_t sz_i = 19;
  if (sz_i < sv.size() && sv[sz_i] == '.') {
    sz_i++;
    while (sz_i < sv.size() && sv[sz_i] >= '0' && sv[sz_i] <= '9')
      sz_i++;
  }
  if (sz_i < sv.size() && (sv[sz_i] == 'Z' || sv[sz_i] == 'z'))
    sz_i++;
  if (sz_i != sv.size())
    return std::nullopt;

  const std::int64_t i8_y = int4_year - (int4_month <= 2 ? 1 : 0);
  const std::int64_t i8_era =
      (i8_y >= 0 ? i8_y : i8_y - 399) / 400;
  const std::int64_t i8_yoe = i8_y - i8_era * 400;
  const std::int64_t i8_doy =
      (153 * (int4_month + (int4_month > 2 ? -3 : 9)) + 2) / 5 +
      int4_day - 1;
  const std::int64_t i8_doe = i8_yoe * 365 + i8_yoe / 4 - i8_yoe / 100 +
                              i8_doy;
  const std::int64_t i8_days = i8_era * 146097 + i8_doe - 719468;
  return i8_days * 86400 + int4_hour * 3600 + int4_minute * 60 +
         int4_second;
}

std::int64_t UnixNowSeconds() {
  return std::chrono::duration_cast<std::chrono::seconds>(
             std::chrono::system_clock::now().time_since_epoch())
      .count();
}

}  // namespace

std::optional<GameClientData> GameClientData::TryFromYaml(
    const yaml::MiniYaml& yaml) {
  GameClientData out{};
  for (const auto& node : yaml.Nodes) {
    const std::string_view sv_key = *node.Key;
    const std::string_view sv_text =
        node.Value.Value != nullptr ? std::string_view{*node.Value.Value}
                                    : std::string_view{};
    if (sv_key == "Name")
      out.Name = std::string{sv_text};
    else if (sv_key == "Fingerprint")
      out.Fingerprint = std::string{sv_text};
    else if (sv_key == "Color") {
      if (!core::Color::TryParse(sv_text, out.Color))
        return std::nullopt;
    } else if (sv_key == "Faction")
      out.Faction = std::string{sv_text};
    else if (sv_key == "Team") {
      if (!meta::TryParseInt32Invariant(sv_text, out.Team))
        return std::nullopt;
    } else if (sv_key == "SpawnPoint") {
      if (!meta::TryParseInt32Invariant(sv_text, out.SpawnPoint))
        return std::nullopt;
    } else if (sv_key == "IsAdmin") {
      if (!meta::TryParseBoolNet(sv_text, out.IsAdmin))
        return std::nullopt;
    } else if (sv_key == "IsSpectator") {
      if (!meta::TryParseBoolNet(sv_text, out.IsSpectator))
        return std::nullopt;
    } else if (sv_key == "IsBot") {
      if (!meta::TryParseBoolNet(sv_text, out.IsBot))
        return std::nullopt;
    }
  }
  return out;
}

std::optional<GameServerData> GameServerData::TryFromYaml(
    const yaml::MiniYaml& yaml) {
  const GameServerClientDeps& deps = GameServerClientDepsRef();

  GameServerData out{};
  bool b_address_null = false;
  bool b_mod_null = false;
  bool b_version_null = false;

  for (const auto& node : yaml.Nodes) {
    const std::string_view sv_key = *node.Key;
    const bool b_value_null = node.Value.Value == nullptr;
    const std::string_view sv_text =
        b_value_null ? std::string_view{}
                     : std::string_view{*node.Value.Value};

    const auto parse_int = [&](int& int4_field) {
      return meta::TryParseInt32Invariant(sv_text, int4_field);
    };

    if (sv_key == "Id") {
      if (!parse_int(out.Id))
        return std::nullopt;
    } else if (sv_key == "Name")
      out.Name = std::string{sv_text};
    else if (sv_key == "Address") {
      b_address_null = b_value_null;
      out.Address = std::string{sv_text};
    } else if (sv_key == "Port") {
      if (!parse_int(out.Port))
        return std::nullopt;
    } else if (sv_key == "State") {
      if (!parse_int(out.State))
        return std::nullopt;
    } else if (sv_key == "MaxPlayers") {
      if (!parse_int(out.MaxPlayers))
        return std::nullopt;
    } else if (sv_key == "Map")
      out.Map = std::string{sv_text};
    else if (sv_key == "Mod") {
      b_mod_null = b_value_null;
      out.Mod = std::string{sv_text};
    } else if (sv_key == "Version") {
      b_version_null = b_value_null;
      out.Version = std::string{sv_text};
    } else if (sv_key == "ModTitle")
      out.ModTitle = std::string{sv_text};
    else if (sv_key == "ModWebsite")
      out.ModWebsite = std::string{sv_text};
    else if (sv_key == "ModIcon32")
      out.ModIcon32 = std::string{sv_text};
    else if (sv_key == "Location")
      out.Location = std::string{sv_text};
    else if (sv_key == "Protected") {
      if (!meta::TryParseBoolNet(sv_text, out.Protected))
        return std::nullopt;
    } else if (sv_key == "Authentication") {
      if (!meta::TryParseBoolNet(sv_text, out.Authentication))
        return std::nullopt;
    } else if (sv_key == "Started")
      out.Started = std::string{sv_text};
    else if (sv_key == "Players") {
      if (!parse_int(out.Players))
        return std::nullopt;
    } else if (sv_key == "Bots") {
      if (!parse_int(out.Bots))
        return std::nullopt;
    } else if (sv_key == "Spectators") {
      if (!parse_int(out.Spectators))
        return std::nullopt;
    } else if (sv_key == "PlayTime") {
      if (!parse_int(out.PlayTime))
        return std::nullopt;
    } else if (sv_key == "DisabledSpawnPoints") {
      out.vec_disabled_spawn_points.clear();
      for (const std::string_view sv_part : meta::SplitCommaTrimmed(sv_text)) {
        std::int32_t int4_value = 0;
        if (!meta::TryParseInt32Invariant(sv_part, int4_value))
          return std::nullopt;
        out.vec_disabled_spawn_points.push_back(int4_value);
      }
    } else if (sv_key == "Clients") {
      for (const auto& client : node.Value.Nodes)
        if (IsClientKey(*client.Key)) {
          auto opt_client = GameClientData::TryFromYaml(client.Value);
          if (!opt_client.has_value())
            return std::nullopt;
          out.vec_clients.push_back(std::move(*opt_client));
        }
    }
  }

  if (b_mod_null || b_version_null) {
    const auto* ptr_mods = yaml.NodeWithKeyOrDefault("Mods");
    if (ptr_mods != nullptr && ptr_mods->Value.Value != nullptr) {
      const std::string_view sv_mods = *ptr_mods->Value.Value;
      const std::size_t sz_at = sv_mods.find('@');
      if (sz_at != std::string_view::npos) {
        out.Mod = std::string{sv_mods.substr(0, sz_at)};
        out.Version = std::string{sv_mods.substr(sz_at + 1)};
      }
    }
  }

  if (out.State == 2 && out.PlayTime < 0) {
    if (const auto i8_start = TryParseUtcSeconds(out.Started))
      out.PlayTime =
          static_cast<int>(UnixNowSeconds() - *i8_start);
  }

  if (deps.fn_external_mod_compatible(out.Mod, out.Version))
    out.IsCompatible = true;

  if (out.ModTitle.empty()) {
    if (const auto str_external =
            deps.fn_external_mod_id(out.Mod, out.Version))
      out.ModTitle = *str_external;
    else if (const auto str_title = deps.fn_mod_title(out.Mod))
      out.ModTitle = *str_title;
    else
      out.ModTitle = std::format("Unknown mod: {}", out.Mod);
  }

  const bool b_map_available =
      deps.fn_allow_downloading() || deps.fn_map_available(out.Map);
  out.IsJoinable = out.IsCompatible && out.State == 1 && b_map_available;

  out.b_address_null = b_address_null;
  return out;
}

GameServerData::GameServerData(const Server& server) {
  Name = server.Settings.Name;

  Address = std::format("0.0.0.0:{}", server.Settings.ListenPort);
  State = static_cast<int>(server.State());

  int int4_open_slots = 0;
  for (const auto& [str_key, slot] : server.LobbyInfo.vec_slots)
    if (!slot.Closed)
      int4_open_slots++;
  int int4_bots = 0;
  for (const auto& c : server.LobbyInfo.vec_clients)
    if (c.IsBot())
      int4_bots++;
  MaxPlayers = int4_open_slots - int4_bots;

  Map = server.MapUid();
  Mod = server.ModId();
  Version = server.ModVersion();
  ModTitle = server.ModTitle();
  ModWebsite = server.ModWebsite();
  ModIcon32 = server.ModIcon32();
  Protected = !server.Settings.Password.empty();
  Authentication = server.Settings.RequireAuthentication ||
                   !server.Settings.vec_profile_id_whitelist.empty();

  vec_clients.reserve(server.LobbyInfo.vec_clients.size());
  for (const auto& c : server.LobbyInfo.vec_clients)
    vec_clients.emplace_back(c);
  vec_disabled_spawn_points = server.LobbyInfo.vec_disabled_spawn_points;
  SessionGuid = server.SessionGuid();
}

std::string GameServerData::ToPOSTData(bool b_lan_game) const {
  auto pool = yaml::StringPool{};
  auto root = std::vector<yaml::MiniYamlNode>{};
  root.push_back(MakeLeaf(pool, "Protocol", std::to_string(kProtocolVersion)));

  const auto push = [&](std::string_view key, std::string_view text) {
    root.push_back(MakeLeaf(pool, key, text));
  };

  push("Name", Name);
  push("Address", Address);
  push("Mod", Mod);
  push("Version", Version);
  push("ModTitle", ModTitle);
  push("ModWebsite", ModWebsite);
  push("ModIcon32", ModIcon32);
  push("Map", Map);
  push("State", std::to_string(State));
  push("MaxPlayers", std::to_string(MaxPlayers));
  push("Protected", FormatBool(Protected));
  push("Authentication", FormatBool(Authentication));
  push("DisabledSpawnPoints",
       FormatIntCollection(vec_disabled_spawn_points));

  if (b_lan_game) {
    int int4_players = 0;
    int int4_spectators = 0;
    int int4_bots = 0;
    for (const auto& c : vec_clients) {
      if (c.IsBot)
        int4_bots++;
      else if (c.IsSpectator)
        int4_spectators++;
      else
        int4_players++;
    }

    // LAN 以 GUID 串占用 Id 字段(Platform.SessionGUID.ToString())
    // LAN overloads Id with the GUID string (Platform.SessionGUID.
    // ToString()).
    root.push_back(MakeLeaf(pool, "Id", SessionGuid));
    root.push_back(
        MakeLeaf(pool, "Players", std::to_string(int4_players)));
    root.push_back(
        MakeLeaf(pool, "Spectators", std::to_string(int4_spectators)));
    root.push_back(MakeLeaf(pool, "Bots", std::to_string(int4_bots)));
    root.push_back(MakeLeaf(pool, "Mods", Mod + "@" + Version));
  }

  auto vec_client_nodes = std::vector<yaml::MiniYamlNode>{};
  for (std::size_t sz_i = 0; sz_i < vec_clients.size(); sz_i++) {
    const auto& c = vec_clients[sz_i];
    auto fields = std::vector<yaml::MiniYamlNode>{};
    fields.push_back(MakeLeaf(pool, "Name", c.Name));
    fields.push_back(MakeLeaf(pool, "Fingerprint", c.Fingerprint));
    fields.push_back(MakeLeaf(pool, "Color", c.Color.ToString()));
    fields.push_back(MakeLeaf(pool, "Faction", c.Faction));
    fields.push_back(MakeLeaf(pool, "Team", std::to_string(c.Team)));
    fields.push_back(
        MakeLeaf(pool, "SpawnPoint", std::to_string(c.SpawnPoint)));
    fields.push_back(MakeLeaf(pool, "IsAdmin", FormatBool(c.IsAdmin)));
    fields.push_back(
        MakeLeaf(pool, "IsSpectator", FormatBool(c.IsSpectator)));
    fields.push_back(MakeLeaf(pool, "IsBot", FormatBool(c.IsBot)));
    vec_client_nodes.emplace_back(
        pool.Intern(std::format("Client@{}", sz_i)),
        yaml::MiniYaml{pool.Intern(std::string{}), std::move(fields)});
  }

  root.emplace_back(
      pool.Intern("Clients"),
      yaml::MiniYaml{pool.Intern(std::string{}), std::move(vec_client_nodes)});

  const auto yaml_root =
      yaml::MiniYaml{pool.Intern(std::string{}), std::move(root)};
  auto vec_lines = std::vector<std::string>{};
  const std::string str_key = "Game";
  yaml_root.ToLines(vec_lines, &str_key, nullptr);

  auto str_out = std::string{};
  for (std::size_t sz_i = 0; sz_i < vec_lines.size(); sz_i++) {
    if (sz_i > 0)
      str_out += '\n';
    str_out += vec_lines[sz_i];
  }
  return str_out;
}

}  // namespace ora::server
