// UPSTREAM: OpenRA.Game/Network/Session.cs @b6fc03f L1-286(锁步/大厅最小面;完整
//          序列化与大厅协议 Phase 7 落地)
//          The lockstep/lobby minimal surface of Session.cs (the full
//          Session serialization and lobby protocol land in Phase 7).
//
// 机制对照 / Mechanism mapping:
//  - OrderManager/UnitOrders 消费面:GlobalSettings(RandomSeed/
//    NetFrameInterval/OptionOrDefault/EnableSyncReports)、Clients(Index/
//    IsBot/State/Team)、ClientWithIndex/NonBotClients
//    The OrderManager/UnitOrders consumption surface: GlobalSettings
//    (RandomSeed/NetFrameInterval/OptionOrDefault/EnableSyncReports),
//    Clients (Index/IsBot/State/Team), ClientWithIndex/NonBotClients.
#pragma once
import std;

namespace ora::net {

/// Session.ClientState(上游同名枚举子集)
enum class ClientState { Invalid, Joining, Connected, Waiting, Ready, Disconnected };

/// Session.Client(最小面)
struct SessionClient {
  int Index = 0;
  std::string Name;
  std::string IpAddress;
  int Team = 0;
  bool IsBot = false;
  bool IsObserver = false;
  ClientState State = ClientState::Invalid;
};

/// Session.GlobalSettings(最小面)
struct SessionGlobalSettings {
  std::string ServerName;
  std::string Map;
  int RandomSeed = 0;
  int NetFrameInterval = 3;   // ProtocolVersion.NetFrameInterval 默认
  bool EnableSyncReports = false;
  std::map<std::string, std::string> map_options;

  /// Session.GlobalSettings.OptionOrDefault(key, fallback)
  std::string OptionOrDefault(std::string_view key,
                              std::string_view fallback) const {
    auto it = map_options.find(std::string(key));
    return it != map_options.end() ? it->second : std::string(fallback);
  }
};

/// Session(最小面容器)
struct Session {
  std::vector<SessionClient> vec_clients;
  SessionGlobalSettings global_settings;

  SessionClient* ClientWithIndex(int index) {
    for (auto& c : vec_clients)
      if (c.Index == index)
        return &c;
    return nullptr;
  }

  std::vector<SessionClient*> NonBotClients() {
    std::vector<SessionClient*> out;
    for (auto& c : vec_clients)
      if (!c.IsBot)
        out.push_back(&c);
    return out;
  }
};

}  // namespace ora::net
