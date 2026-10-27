#pragma once
import std;

#include "map/map_cache.hpp"
#include "net/session.hpp"
#include "server/server_traits.hpp"

namespace ora::server {

class ServerConnection;

class LobbyCommands final : public ServerTrait,
                            public IInterpretCommand,
                            public INotifyServerStart,
                            public INotifyServerEmpty,
                            public IClientJoined,
                            public ITick {
 public:
  LobbyCommands() = default;

  bool InterpretCommand(Server& server, ServerConnection& conn,
                        net::SessionClient* p_client,
                        std::string_view str_cmd) override;
  void ServerStarted(Server& server) override;
  void ServerEmpty(Server& server) override;
  void ClientJoined(Server& server, ServerConnection& conn) override;
  void Tick(Server& server) override;

  static bool ValidateCommand(Server& server, ServerConnection& conn,
                              const net::SessionClient& client,
                              std::string_view str_command);

  static std::optional<net::SessionSlot> MakeSlotFromPlayerReference(
      const map::PlayerReference& pr);

  static void LoadMapSettings(Server& server, net::SessionGlobalSettings& gs,
                              const map::MapPreview& map);

  static core::Color SanitizePlayerColor(
      Server& server, core::Color asked_color, int int4_player_index,
      ServerConnection* p_connection_to_echo = nullptr);

  static std::string SanitizePlayerFaction(
      Server& server, std::string_view str_asked_faction,
      std::span<const std::string> vec_valid_factions);

  static const map::PlayerReference* PlayerReferenceForSlot(
      Server& server, const net::SessionSlot* p_slot);

  static std::vector<int> AvailableSpawnPoints(int int4_spawn_points,
                                               const net::Session& lobby_info);

  static bool InsufficientEnabledSpawnPoints(const map::MapPreview& map,
                                             const net::Session& lobby_info);

 private:

  using Handler = std::function<bool(Server&, ServerConnection&,
                                     net::SessionClient*, std::string_view)>;

  static const std::vector<std::pair<std::string_view, Handler>>&
  CommandHandlers();

  static bool ValidateSlotCommand(Server& server, ServerConnection& conn,
                                  const net::SessionClient& client,
                                  std::string_view str_arg,
                                  bool b_requires_host);

  static void CheckAutoStart(Server& server);

  static bool State(Server&, ServerConnection&, net::SessionClient*,
                    std::string_view);
  static bool StartGame(Server&, ServerConnection&, net::SessionClient*,
                        std::string_view);
  static bool Slot(Server&, ServerConnection&, net::SessionClient*,
                   std::string_view);
  static bool AllowSpectators(Server&, ServerConnection&, net::SessionClient*,
                              std::string_view);
  static bool Spectate(Server&, ServerConnection&, net::SessionClient*,
                       std::string_view);
  static bool SlotClose(Server&, ServerConnection&, net::SessionClient*,
                        std::string_view);
  static bool SlotOpen(Server&, ServerConnection&, net::SessionClient*,
                       std::string_view);
  static bool SlotBot(Server&, ServerConnection&, net::SessionClient*,
                      std::string_view);
  static bool Map(Server&, ServerConnection&, net::SessionClient*,
                  std::string_view);
  static bool Option(Server&, ServerConnection&, net::SessionClient*,
                     std::string_view);
  static bool ResetOptions(Server&, ServerConnection&, net::SessionClient*,
                           std::string_view);
  static bool AssignTeams(Server&, ServerConnection&, net::SessionClient*,
                          std::string_view);
  static bool Kick(Server&, ServerConnection&, net::SessionClient*,
                   std::string_view);
  static bool VoteKick(Server&, ServerConnection&, net::SessionClient*,
                       std::string_view);
  static bool MakeAdmin(Server&, ServerConnection&, net::SessionClient*,
                        std::string_view);
  static bool MakeSpectator(Server&, ServerConnection&, net::SessionClient*,
                            std::string_view);
  static bool Name(Server&, ServerConnection&, net::SessionClient*,
                   std::string_view);
  static bool Faction(Server&, ServerConnection&, net::SessionClient*,
                      std::string_view);
  static bool Team(Server&, ServerConnection&, net::SessionClient*,
                   std::string_view);
  static bool Handicap(Server&, ServerConnection&, net::SessionClient*,
                       std::string_view);
  static bool Spawn(Server&, ServerConnection&, net::SessionClient*,
                    std::string_view);
  static bool ClearPlayerSpawn(Server&, ServerConnection&, net::SessionClient*,
                               std::string_view);
  static bool PlayerColor(Server&, ServerConnection&, net::SessionClient*,
                          std::string_view);
  static bool SyncLobby(Server&, ServerConnection&, net::SessionClient*,
                        std::string_view);

  static bool TryParseClientCommand(Server& server, ServerConnection& conn,
                                    std::string_view str_command,
                                    std::string_view str_s,
                                    std::vector<std::string>& vec_parts_out,
                                    net::SessionClient*& p_target_out);

  static void InitializeMapPool(Server& server);
  static std::string ChooseInitialMap(Server& server);
};

}
