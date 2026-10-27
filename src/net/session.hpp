#pragma once
import std;

#include "core/color.hpp"

namespace ora::yaml {
struct MiniYamlNode;
class MiniYaml;
class StringPool;
}

namespace ora::net {

enum class ClientState {
  NotReady = 0,
  Invalid = 1,
  Ready = 2,
  Disconnected = 1000,
};

enum class ConnectionQuality { Good, Moderate, Poor };

enum class SessionMapStatus : int {
  Unknown = 0,
  Validating = 1,
  Playable = 2,
  Incompatible = 4,
  UnsafeCustomRules = 8,
};

constexpr bool HasMapStatusFlag(int int4_flags, SessionMapStatus flag) {
  return (int4_flags & static_cast<int>(flag)) == static_cast<int>(flag);
}

struct SessionClient {
  int Index = 0;
  std::string Name;
  core::Color PreferredColor{};
  core::Color Color{core::Color::FromArgbRaw(0xFF4B4B4B)};
  std::string Faction;
  int SpawnPoint = 0;
  std::string AnonymizedIPAddress;
  std::string Location;

  std::string IPAddress;
  ConnectionQuality ConnectionQualityKind = ConnectionQuality::Good;
  ClientState State = ClientState::Invalid;
  int Team = 0;
  int Handicap = 0;
  std::string Slot;
  std::string Bot;
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
  int MapStatusFlags = 0;
  int RandomSeed = 0;
  bool AllowSpectators = true;
  std::string GameUid;
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

  const SessionClient* ClientWithIndex(int index) const {
    for (const auto& c : vec_clients)
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

  const SessionClient* ClientInSlot(const std::string& str_slot) const {
    for (const auto& c : vec_clients)
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

  std::vector<SessionClient*> NonBotPlayers() {
    std::vector<SessionClient*> out;
    for (auto& c : vec_clients)
      if (!c.IsBot() && !c.Slot.empty())
        out.push_back(&c);
    return out;
  }

  SessionSlot* FindSlot(const std::string& str_key) {
    for (auto& [k, s] : vec_slots)
      if (k == str_key)
        return &s;
    return nullptr;
  }
  const SessionSlot* FindSlot(const std::string& str_key) const {
    for (const auto& [k, s] : vec_slots)
      if (k == str_key)
        return &s;
    return nullptr;
  }

  std::string FirstEmptySlot() const;

  std::string FirstEmptyBotSlot() const;

  static Session Deserialize(std::string_view str_data, std::string_view str_name);

  std::string Serialize() const;
};

yaml::MiniYamlNode SerializeSessionClientFields(yaml::StringPool& pool,
                                                const SessionClient& c);

yaml::MiniYamlNode SerializeSessionClient(yaml::StringPool& pool,
                                          const SessionClient& c);

yaml::MiniYamlNode SerializeSessionSlot(yaml::StringPool& pool,
                                        const SessionSlot& s);

yaml::MiniYamlNode SerializeSessionGlobalSettings(
    yaml::StringPool& pool, const SessionGlobalSettings& g);

bool LoadSessionClient(const yaml::MiniYaml& yaml_v, SessionClient& out);

bool LoadSessionSlot(const yaml::MiniYaml& yaml_v, SessionSlot& out);
bool LoadSessionGlobalSettings(const yaml::MiniYaml& yaml_v,
                               SessionGlobalSettings& out);

}
