#pragma once
import std;

#include "core/color.hpp"
#include "net/session.hpp"

namespace ora::yaml {
class MiniYaml;
}

namespace ora::server {

class Server;

struct GameServerClientDeps {
  std::function<bool(const std::string&, const std::string&)>
      fn_external_mod_compatible =
          [](const std::string&, const std::string&) { return false; };
  std::function<std::optional<std::string>(const std::string&, const std::string&)>
      fn_external_mod_id =
          [](const std::string&, const std::string&) {
            return std::optional<std::string>{};
          };
  std::function<std::optional<std::string>(const std::string&)> fn_mod_title =
      [](const std::string&) { return std::optional<std::string>{}; };
  std::function<bool()> fn_allow_downloading = [] { return true; };
  std::function<bool(const std::string&)> fn_map_available =
      [](const std::string&) { return false; };
};

GameServerClientDeps& GameServerClientDepsRef();

/// GameClient(GameServer.cs L21-37)
/// GameClient (GameServer.cs L21-37).
struct GameClientData {
  std::string Name;
  std::string Fingerprint;
  core::Color Color{};
  std::string Faction;
  int Team = 0;
  int SpawnPoint = 0;
  bool IsAdmin = false;
  bool IsSpectator = false;
  bool IsBot = false;

  GameClientData() = default;
  explicit GameClientData(const net::SessionClient& c);
  static std::optional<GameClientData> TryFromYaml(const yaml::MiniYaml& yaml);
};

/// GameServer 的服务器侧装配面(GameServer.cs L75-166 的子集;解析 ctor
/// 随客户端批)
/// The server-side assembly face of GameServer (a subset of L75-166; the
/// parse ctor rides the client batch).
struct GameServerData {
  static constexpr int kProtocolVersion = 2;  // L72

  int Id = -1;                        // L78(LAN = GUID 串注入)
  std::string Name;                   // L81
  std::string Address;                // L84
  int Port = 0;                       // L87
  int State = 0;                      // L90
  int MaxPlayers = 0;                 // L93
  std::string Map;                    // L96
  std::string Mod;                    // L99
  std::string Version;                // L102
  std::string ModTitle;               // L105
  std::string ModWebsite;             // L108
  std::string ModIcon32;              // L111
  std::string Location;               // L114
  bool Protected = false;             // L117
  bool Authentication = false;        // L120
  std::vector<GameClientData> vec_clients;  // L142
  std::vector<int> vec_disabled_spawn_points;  // L145
  /// Platform.SessionGUID.ToString()(LAN ToPOSTData 的 Id 字段)
  /// Platform.SessionGUID.ToString() (LAN ToPOSTData's Id field).
  std::string SessionGuid;

  std::string Started;                // L123(仅 yaml 解析域)
  int Players = 0;                    // L126
  int Bots = 0;                       // L129
  int Spectators = 0;                 // L132
  int PlayTime = -1;                  // L135
  bool IsCompatible = false;          // L139
  bool IsJoinable = false;            // L141
  bool b_address_null = false;

  static std::optional<GameServerData> TryFromYaml(const yaml::MiniYaml& yaml);

  GameServerData() = default;

  /// ModLabel => $"{ModTitle} ({Version})"(L167)
  /// ModLabel (L167).
  std::string ModLabel() const {
    return ModTitle + " (" + Version + ")";
  }

  /// GameServer(Server)(L196-219)
  GameServerData(const Server& server);

  /// ToPOSTData(L222-254;lanGame 的四附加字段 + Clients 节 + ToLines 连接)
  /// ToPOSTData (L222-254; lanGame's four extra fields + the Clients node +
  /// the ToLines join).
  std::string ToPOSTData(bool b_lan_game) const;
};

}  // namespace ora::server
