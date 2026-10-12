// UPSTREAM: OpenRA.Game/Network/GameServer.cs @e180e3225 L19-37,75-166,
//          196-265(实现体;FieldSaver.SaveField/Save → 手写按名字段表,
//          输出逐字等价)
//          The implementation body (FieldSaver.SaveField/Save → hand-written
//          by-name field tables; the output verbatim-equal).
#include "server/game_server.hpp"

#include "server/server.hpp"
#include "yaml/mini_yaml.hpp"

namespace ora::server {

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
  for (std::size_t sz_i = 0; sz_i < vec.size(); sz_i++) {
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

}  // namespace

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
