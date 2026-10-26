// UPSTREAM: OpenRA.Mods.Common/ServerTraits/LobbyCommands.cs @1f594e320
//          L29-1492 全文(24 命令分发字典 + ValidateCommand 双前置门 +
//          CheckAutoStart 的六段自动开局 + State/StartGame/Slot/
//          AllowSpectators/Spectate/SlotClose/SlotOpen/SlotBot/Map/Option/
//          ResetOptions/AssignTeams/Kick/VoteKick/MakeAdmin/MakeSpectator/
//          Name/Faction/Team/Handicap/Spawn/ClearPlayerSpawn/PlayerColor/
//          SyncLobby 二十命令全文 + InitializeMapPool/ChooseInitialMap/
//          ServerStarted 的地图装配 + LoadMapSettings 的选项状态迁移 +
//          SanitizePlayerColor/SanitizePlayerFaction + ClientJoined 的
//          SyncMapPool 与色彩消毒 + ServerEmpty 的槽位重置;
//          LobbyUtils.InsufficientEnabledSpawnPoints(LobbyUtils.cs L289-302)
//          一并移植;ModData 大厅面(MapCache/色彩管理器/ILobbyOptions/
//          IBotInfo/远程查询)经 ServerDeps 注入族承载 —— 逐 trait 的
//          LobbyOptions(map) 装配随其宿主 trait 批,AI 的 IBotInfo 面
//          随 Phase 8)
//          The whole of LobbyCommands.cs L29-1492 (the 24-command dispatch
//          dictionary + ValidateCommand's two preamble gates +
//          CheckAutoStart's six-stage auto-start + the full twenty-command
//          bodies State/StartGame/Slot/AllowSpectators/Spectate/SlotClose/
//          SlotOpen/SlotBot/Map/Option/ResetOptions/AssignTeams/Kick/
//          VoteKick/MakeAdmin/MakeSpectator/Name/Faction/Team/Handicap/
//          Spawn/ClearPlayerSpawn/PlayerColor/SyncLobby + InitializeMapPool/
//          ChooseInitialMap/ServerStarted's map assembly + LoadMapSettings'
//          option-state migration + SanitizePlayerColor/SanitizePlayerFaction
//          + ClientJoined's SyncMapPool and color sanitization +
//          ServerEmpty's slot reset; LobbyUtils.InsufficientEnabledSpawnPoints
//          (LobbyUtils.cs L289-302) ported along; the ModData lobby faces
//          (MapCache/the color manager/ILobbyOptions/IBotInfo/the remote
//          query) ride the ServerDeps injection family — the per-trait
//          LobbyOptions(map) assembly rides its host trait's batch, and the
//          AI's IBotInfo face rides Phase 8).
#pragma once
import std;

#include "map/map_cache.hpp"
#include "net/session.hpp"
#include "server/server_traits.hpp"


namespace ora::server {

class ServerConnection;

/// LobbyCommands(LobbyCommands.cs L29)
/// LobbyCommands (LobbyCommands.cs L29).
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

  // ———— 公开静态面(其余 trait/装配侧消费)————
  // ———— The public static faces (consumed by the other traits/assembly
  //      sides) ————

  /// ValidateCommand(L212-233)
  static bool ValidateCommand(Server& server, ServerConnection& conn,
                              const net::SessionClient& client,
                              std::string_view str_command);

  /// MakeSlotFromPlayerReference(L1370-1387)
  static std::optional<net::SessionSlot> MakeSlotFromPlayerReference(
      const map::PlayerReference& pr);

  /// LoadMapSettings(L1389-1423)
  static void LoadMapSettings(Server& server, net::SessionGlobalSettings& gs,
                              const map::MapPreview& map);

  /// SanitizePlayerColor(L1425-1444)
  static core::Color SanitizePlayerColor(
      Server& server, core::Color asked_color, int int4_player_index,
      ServerConnection* p_connection_to_echo = nullptr);

  /// SanitizePlayerFaction(L1446-1449)
  static std::string SanitizePlayerFaction(
      Server& server, std::string_view str_asked_faction,
      std::span<const std::string> vec_valid_factions);

  /// PlayerReferenceForSlot(L1484-1490)
  static const map::PlayerReference* PlayerReferenceForSlot(
      Server& server, const net::SessionSlot* p_slot);

  /// LobbyUtils.AvailableSpawnPoints(LobbyUtils.cs L289-292)
  static std::vector<int> AvailableSpawnPoints(int int4_spawn_points,
                                               const net::Session& lobby_info);

  /// LobbyUtils.InsufficientEnabledSpawnPoints(LobbyUtils.cs L294-302)
  static bool InsufficientEnabledSpawnPoints(const map::MapPreview& map,
                                             const net::Session& lobby_info);

 private:
  /// 命令处理器类型(上游 Func<S, Connection, Client, string, bool> 的
  /// std::function 封装器形态)
  /// The command-handler type (upstream's Func<S, Connection, Client,
  /// string, bool> as the std::function wrapper form).
  using Handler = std::function<bool(Server&, ServerConnection&,
                                     net::SessionClient*, std::string_view)>;

  /// commandHandlers(L163-190)的静态表
  /// The static table of commandHandlers (L163-190).
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

  /// TryParseClientCommand(L1246-1265)
  static bool TryParseClientCommand(Server& server, ServerConnection& conn,
                                    std::string_view str_command,
                                    std::string_view str_s,
                                    std::vector<std::string>& vec_parts_out,
                                    net::SessionClient*& p_target_out);

  static void InitializeMapPool(Server& server);
  static std::string ChooseInitialMap(Server& server);
};

}  // namespace ora::server
