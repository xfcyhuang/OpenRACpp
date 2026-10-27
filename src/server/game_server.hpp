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

struct GameServerData {
  static constexpr int kProtocolVersion = 2;

  int Id = -1;
  std::string Name;
  std::string Address;
  int Port = 0;
  int State = 0;
  int MaxPlayers = 0;
  std::string Map;
  std::string Mod;
  std::string Version;
  std::string ModTitle;
  std::string ModWebsite;
  std::string ModIcon32;
  std::string Location;
  bool Protected = false;
  bool Authentication = false;
  std::vector<GameClientData> vec_clients;
  std::vector<int> vec_disabled_spawn_points;

  std::string SessionGuid;

  std::string Started;
  int Players = 0;
  int Bots = 0;
  int Spectators = 0;
  int PlayTime = -1;
  bool IsCompatible = false;
  bool IsJoinable = false;
  bool b_address_null = false;

  static std::optional<GameServerData> TryFromYaml(const yaml::MiniYaml& yaml);

  GameServerData() = default;

  std::string ModLabel() const {
    return ModTitle + " (" + Version + ")";
  }

  GameServerData(const Server& server);

  std::string ToPOSTData(bool b_lan_game) const;
};

}
