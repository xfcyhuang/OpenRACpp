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
//  - 玩家创建链消费面(Phase 5 第二批):Slots(PlayerReference/LockFaction)、
//    ClientInSlot、Client 的 Bot(类型串)/Handicap/Fingerprint/Color/
//    IsAdmin 面(C# nullable 串 → 空串承载);完整大厅协议 Phase 7
//    The player-creation-chain consumption faces (Phase 5 batch 2):
//    Slots (PlayerReference/LockFaction), ClientInSlot, the Client's
//    Bot (type string)/Handicap/Fingerprint/Color/IsAdmin faces (C#
//    nullable strings carried as ""); the full lobby protocol lands in
//    Phase 7.
#pragma once
import std;

#include "core/color.hpp"

namespace ora::net {

/// Session.ClientState(上游同名枚举子集)
enum class ClientState { Invalid, Joining, Connected, Waiting, Ready, Disconnected };

/// Session.Client(最小面;Color/PreferredColor 等 HSLColor 面随 Phase 7
/// 大厅批补齐)
/// Session.Client (the minimal face; the Color/PreferredColor HSLColor
/// faces land with the Phase 7 lobby batch).
struct SessionClient {
  int Index = 0;
  std::string Name;
  std::string IpAddress;
  std::string Faction;    // Game.JoinLocal 装配面 | the Game.JoinLocal face
  int SpawnPoint = 0;     // 同上 | ditto
  int Team = 0;
  bool IsBot = false;
  bool IsObserver = false;
  bool IsAdmin = false;   // Session.Client.IsAdmin | (the same).
  std::string Bot;        // null 承载为空串(bot 类型名)| null as "" (the
                          // bot type name).
  std::string Slot;       // 客户端所在槽键(空 = 无槽)| the client's slot
                          // key (empty = no slot).
  int Handicap = 0;       // Session.Client.Handicap | (the same).
  std::string Fingerprint;  // 同上 | ditto.
  core::Color Color{core::Color::FromArgbRaw(0xFF4B4B4B)};  // 大厅色面
                            // (JoinLocal 装配覆盖)| the lobby color face.
  ClientState State = ClientState::Invalid;
};

/// Session.Slot(玩家创建链消费面;完整 Slot 字段随 Phase 7 大厅批)
/// Session.Slot (the player-creation-chain consumption face; the full Slot
/// fields land with the Phase 7 lobby batch).
struct SessionSlot {
  std::string PlayerReference;  // 上游槽的 pr 名 | the slot's pr name.
  bool Closed = false;
  bool Locked = false;
  bool AllowBots = true;
  bool LockFaction = false;
  bool LockColor = false;
  bool LockSpawn = false;
  bool LockTeam = false;
  bool Required = false;
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

/// Session(最小面容器) | Session (the minimal container).
struct Session {
  std::vector<SessionClient> vec_clients;
  /// Slots:插入序字典(Slot 键为 yaml 键)| Slots: the insertion-ordered
  /// dictionary (the slot key is the yaml key).
  std::vector<std::pair<std::string, SessionSlot>> vec_slots;
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

  /// ClientInSlot(slot)(Session.cs:Clients.FirstOrDefault(c => c.Slot ==
  /// slot)):缺客户端 → null | ClientInSlot(slot): no client → null.
  SessionClient* ClientInSlot(const std::string& str_slot) {
    for (auto& c : vec_clients)
      if (c.Slot == str_slot)
        return &c;
    return nullptr;
  }
};

}  // namespace ora::net
