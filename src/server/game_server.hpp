// UPSTREAM: OpenRA.Game/Network/GameServer.cs @e180e3225 L19-166,196-265
//          (服务器侧面全文:GameClient 的 Session.Client 投影 + GameServer
//          (Server) ctor 的 13 字段装配 + ToPOSTData 的 SerializeFields
//          序列化 + LAN 附加四字段与 Clients 节;MiniYaml(yaml) 的解析
//          ctor 属主服务器浏览器客户端面,随 Phase 6/7 客户端批落地;
//          WebServices/GeoIP 的 Location 字段为空承载)
//          The whole of the server-side face of GameServer.cs (GameClient's
//          Session.Client projection + the GameServer(Server) ctor's
//          13-field assembly + ToPOSTData's SerializeFields serialization +
//          the LAN's four extra fields and the Clients node; the MiniYaml
//          parse ctor belongs to the master-server browser's client face and
//          rides the Phase 6/7 client batch; the WebServices/GeoIP Location
//          field stays empty).
#pragma once
import std;

#include "core/color.hpp"
#include "net/session.hpp"


namespace ora::server {

class Server;

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

  /// GameServer(Server)(L196-219)
  GameServerData(const Server& server);

  /// ToPOSTData(L222-254;lanGame 的四附加字段 + Clients 节 + ToLines 连接)
  /// ToPOSTData (L222-254; lanGame's four extra fields + the Clients node +
  /// the ToLines join).
  std::string ToPOSTData(bool b_lan_game) const;
};

}  // namespace ora::server
