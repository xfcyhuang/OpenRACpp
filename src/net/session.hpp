// UPSTREAM: OpenRA.Game/Network/Session.cs @b6fc03f L1-286(锁步/大厅面;
//          反序列化随第十一批回放驱动链落地 —— FieldLoader.Load 的
//          Client/Slot/Global/LobbyOptionState 按名装载手写面,序列化面
//          对称承载)
//          The lockstep/lobby face of Session.cs; the deserialization lands
//          with the batch-11 replay-driver chain (a hand-written by-name
//          loading face of FieldLoader.Load for Client/Slot/Global/
//          LobbyOptionState), with the symmetric serialization face.
//
// 机制对照 / Mechanism mapping:
//  - OrderManager/UnitOrders 消费面:GlobalSettings(RandomSeed/
//    NetFrameInterval/OptionOrDefault/EnableSyncReports)、Clients(Index/
//    IsBot/State/Team)、ClientWithIndex/NonBotClients
//    The OrderManager/UnitOrders consumption surface.
//  - 玩家创建链消费面(Phase 5 第二批):Slots(PlayerReference/LockFaction)、
//    ClientInSlot、Client 的 Bot(类型串)/Handicap/Fingerprint/Color/
//    IsAdmin 面(C# nullable 串 → 空串承载)
//    The player-creation-chain consumption faces (Phase 5 batch 2).
//  - Deserialize 的任何字段/结构失败统一抛 YamlException("Session
//    deserialized invalid MiniYaml:\n…")(上游两 catch 包装的等价文本)
//    Any field/structure failure in Deserialize throws the unified
//    YamlException ("Session deserialized invalid MiniYaml:\n…" — the
//    equivalent text of upstream's two catch wrappers).
//  - Serialize 的 FieldSaver.Save 反射 → 按名字段的双向手写表
//    Serialize's reflected FieldSaver.Save becomes the by-name hand-written
//    two-way tables.
#pragma once
import std;

#include "core/color.hpp"

namespace ora::yaml {
struct MiniYamlNode;
}

namespace ora::net {

enum class ClientState {
  NotReady = 0,
  Invalid = 1,
  Ready = 2,
  Disconnected = 1000,
};

enum class ConnectionQuality { Good, Moderate, Poor };

struct SessionClient {
  int Index = 0;
  std::string Name;
  core::Color PreferredColor{};
  core::Color Color{core::Color::FromArgbRaw(0xFF4B4B4B)};
  std::string Faction;
  int SpawnPoint = 0;
  std::string AnonymizedIPAddress;  // null 承载为空串 | null as ""
  std::string Location;             // 同上 | ditto
  ConnectionQuality ConnectionQualityKind = ConnectionQuality::Good;
  ClientState State = ClientState::Invalid;
  int Team = 0;
  int Handicap = 0;
  std::string Slot;  // 空 = 无槽(观察者) | empty = no slot (observer)
  std::string Bot;   // null 承载为空串 | null as ""
  int BotControllerClientIndex = 0;
  bool IsAdmin = false;
  std::string Fingerprint;

  bool IsReady() const { return State == ClientState::Ready; }
  bool IsObserver() const { return Slot.empty(); }
  bool IsBot() const { return !Bot.empty(); }
};

struct SessionSlot {
  std::string PlayerReference;
  bool Closed = false;
  bool AllowBots = false;
  bool LockFaction = false;
  bool LockColor = false;
  bool LockTeam = false;
  bool LockHandicap = false;
  bool LockSpawn = false;
  bool Required = false;
};

struct SessionLobbyOption {
  std::string Value;
  std::string PreferredValue;
  bool IsLocked = false;

  bool IsEnabled() const { return Value == "True"; }
};

struct SessionGlobalSettings {
  std::string ServerName;
  std::string Map;
  int MapStatusFlags = 0;  // MapStatus 位组合 | the MapStatus bit union
  int RandomSeed = 0;
  bool AllowSpectators = true;
  std::string GameUid;  // null 承载为空串 | null as ""
  bool EnableSingleplayer = false;
  bool EnableMapGeneration = false;
  bool EnableGameSaves = false;
  bool EnableSyncReports = false;
  bool Dedicated = false;
  int NetFrameInterval = 3;
  int GameTimestep = 0;
  std::map<std::string, SessionLobbyOption> map_lobby_options;

  std::string OptionOrDefault(std::string_view key,
                              std::string_view fallback) const {
    auto it = map_lobby_options.find(std::string(key));
    if (it != map_lobby_options.end())
      return it->second.Value;
    return std::string(fallback);
  }

  bool OptionOrDefault(std::string_view key, bool fallback) const {
    auto it = map_lobby_options.find(std::string(key));
    if (it != map_lobby_options.end())
      return it->second.IsEnabled();
    return fallback;
  }
};

struct Session {
  std::vector<SessionClient> vec_clients;
  std::vector<std::pair<std::string, SessionSlot>> vec_slots;
  std::vector<int> vec_disabled_spawn_points;
  SessionGlobalSettings global_settings;

  SessionClient* ClientWithIndex(int index) {
    for (auto& c : vec_clients)
      if (c.Index == index)
        return &c;
    return nullptr;
  }

  SessionClient* ClientInSlot(const std::string& str_slot) {
    for (auto& c : vec_clients)
      if (c.Slot == str_slot)
        return &c;
    return nullptr;
  }

  std::vector<SessionClient*> NonBotClients() {
    std::vector<SessionClient*> out;
    for (auto& c : vec_clients)
      if (!c.IsBot())
        out.push_back(&c);
    return out;
  }

  static Session Deserialize(std::string_view str_data, std::string_view str_name);

  std::string Serialize() const;
};

}  // namespace ora::net
