// UPSTREAM: OpenRA.Game/Server/Server.cs @e180e3225 L33-1601(服务器实体
//          全文:监听线程群 + 单线程事件循环 + AcceptConnection 的令牌
//          握手 + ValidateClient 的六门校验 + CompleteConnection 的槽位
//          装配 + ReceiveOrders 的 OrderLatency 前投影与 Ack 回执 +
//          HandleSyncOrder 的服务器侧 desync 检测与 defeat 状态跟踪 +
//          StartGame 的空帧注入(L1444-1464 注释语义)+ DropClient 的
//          管理员重指派 + SyncLobby 族 + ReceivePing 的连接质量分级;
//          BlockingCollection<IServerEvent> → mutex/cv 闭包队列,Lock →
//          std::mutex;Map/PlayerDatabase/GeoIP/Nat/HTTP 鉴权/GameSave/
//          FluentProvider 为注入面 —— 见 docs/COVERAGE.md D216~)
//          The whole of the server entity, Server.cs L33-1601 (the
//          listener threads + the single-threaded event loop +
//          AcceptConnection's token handshake + ValidateClient's six
//          gates + CompleteConnection's slot assembly + ReceiveOrders'
//          OrderLatency forward projection with the Ack receipt +
//          HandleSyncOrder's server-side desync detection and defeat-state
//          tracking + StartGame's empty-frame injection (the L1444-1464
//          comment semantics) + DropClient's admin reassignment + the
//          SyncLobby family + ReceivePing's connection-quality grading;
//          BlockingCollection<IServerEvent> → the mutex/cv closure queue,
//          Lock → std::mutex; Map/PlayerDatabase/GeoIP/Nat/HTTP-auth/
//          GameSave/FluentProvider ride injection faces — see
//          docs/COVERAGE.md D216-on).
#pragma once
import std;

#include "core/mersenne_twister.hpp"
#include "map/map_players.hpp"
#include "net/connection_target.hpp"
#include "net/order.hpp"
#include "net/order_io.hpp"
#include "net/replay_recorder.hpp"
#include "net/session.hpp"
#include "server/color_picker_manager.hpp"
#include "net/fluent_message.hpp"
#include "server/game_information.hpp"
#include "net/handshake.hpp"
#include "server/lobby_option.hpp"
#include "server/map_status_cache.hpp"
#include "server/order_buffer.hpp"
#include "server/protocol_version.hpp"
#include "server/server_connection.hpp"
#include "server/server_settings.hpp"
#include "server/server_traits.hpp"
#include "server/vote_kick_tracker.hpp"

namespace ora::server {

using ora::net::FluentArgument;
using ora::net::FluentArgumentType;
using ora::net::HandshakeRequest;
using ora::net::HandshakeResponse;
using ora::net::FluentMessage;

enum class ServerState {  // Server.cs L33-38
  WaitingPlayers = 1,
  GameStarted = 2,
  ShuttingDown = 3,
};

enum class ServerType {  // Server.cs L40-46
  Local = 0,
  Skirmish = 1,
  Multiplayer = 2,
  Dedicated = 3,
};

/// 上游隐式环境依赖的注入面(本批承载 Local/Skirmish 全语义 + Dedicated
/// 的非鉴权路径;MP 在线面随其批)
/// The injection face of upstream's implicit environment dependencies
/// (this batch carries the full Local/Skirmish semantics + Dedicated's
/// unauthenticated path; the online MP faces ride their batch).
struct ServerDeps {
  /// Game.RunTime(单调毫秒;缺省 = 进程级 steady 时钟)
  /// Game.RunTime (monotonic ms; default = the process-wide steady clock).
  ServerConnection::ClockFn fn_clock;

  /// Guid.NewGuid().ToString()(缺省 = 随机 v4 形态十六进制)
  /// Guid.NewGuid().ToString() (default = random v4-shaped hex).
  std::function<std::string()> fn_new_game_uid;

  /// Game.TimestampedFilename(extra: "-Server")(服务器录像的文件名)
  /// Game.TimestampedFilename(extra: "-Server") (the server replay's
  /// filename).
  net::ReplayRecorder::ChooseFilename fn_timestamped_filename;

  /// ModData.Manifest.Id / Metadata.Version
  std::string str_mod_id;
  std::string str_mod_version;

  /// Game.Settings.Player.Name(SanitizedServerName 的回落名)
  /// Game.Settings.Player.Name (SanitizedServerName's fallback name).
  std::string str_player_name = "Commander";

  /// Map.Players.Players[slot](槽位 PlayerReference 查询;观察者路径不
  /// 触达)
  /// Map.Players.Players[slot] (the slot's PlayerReference lookup; the
  /// observer path never touches it).
  std::function<const map::PlayerReference*(
      std::string_view str_slot)> fn_player_reference;

  /// Map.WorldActorInfo.TraitInfos<ICreatePlayersInfo>() 的汇流循环
  /// (worldPlayers 空位表 = optional 槽)
  /// The fan-in loop over Map.WorldActorInfo.TraitInfos<
  /// ICreatePlayersInfo>() (the worldPlayers null-slot table = optional
  /// slots).
  std::function<void(net::Session& lobby_info,
                     std::vector<std::optional<GameInformationPlayer>>&
                         vec_world_players_out,
                     MersenneTwister& player_random)>
      fn_create_server_players;

  /// ModData.GetOrCreate<GameSpeeds>()
  std::function<const game::GameSpeeds*()> fn_game_speeds;

  /// FluentProvider.GetMessage(key, args)(专用服控制台回显;缺省 = key)
  /// FluentProvider.GetMessage(key, args) (the dedicated-server console
  /// echo; default = the key).
  std::function<std::string(std::string_view str_key)> fn_fluent_get;

  /// Log.Write("server", line)(缺省 = 丢弃)
  /// Log.Write("server", line) (default = dropped).
  std::function<void(std::string_view str_line)> fn_server_log;

  /// 玩家档案在线鉴权(MP;返回 = 允许入座)
  /// The online player-profile authentication (MP; the return = admitted).
  std::function<bool(std::string_view str_fingerprint,
                     std::string_view str_signature,
                     std::string_view str_auth_token,
                     std::string& str_profile_id_out)>
      fn_verify_auth;

  // ———— 第十五批:ModData 大厅面(ModData.MapCache/DefaultRules 色
  //      管理器/DefaultTerrainInfo/ILobbyOptions·IBotInfo 汇流/
  //      QueryRemoteMapDetails/ILintServerMapPass)的注入族 ————
  // ———— Batch 15: the injection family of the ModData lobby faces
  //      (ModData.MapCache / the DefaultRules color manager /
  //      DefaultTerrainInfo / the ILobbyOptions·IBotInfo fan-in /
  //      QueryRemoteMapDetails / ILintServerMapPass) ————

  /// ModData.MapCache[uid](null = UnknownMap 语义)
  /// ModData.MapCache[uid] (null = the UnknownMap semantics).
  std::function<map::MapPreview*(const std::string&)> fn_map_cache_at;

  /// MapCache.Where(Available && Visibility 含 Lobby).Select(Uid)
  /// (InitializeMapPool 的空 MapPool 池面)
  /// MapCache.Where(Available && Lobby-visible).Select(Uid) (the empty-
  /// MapPool pool face of InitializeMapPool).
  std::function<std::vector<std::string>()> fn_available_lobby_maps;

  /// PlayerActorInfo+WorldActorInfo 的 ILobbyOptions 汇流(逐 trait 装配
  /// 面随其宿主 trait 批;缺省 = 空集)
  /// The PlayerActorInfo+WorldActorInfo ILobbyOptions fan-in (the per-trait
  /// assembly rides the hosts' batches; default = the empty set).
  std::function<std::vector<LobbyOptionData>(const map::MapPreview&)>
      fn_lobby_options;

  /// PlayerActorInfo.TraitInfos<IBotInfo>() 的 {Type, Name} 汇流(AI 面
  /// 随 Phase 8;缺省 = 空集)
  /// The {Type, Name} fan-in of PlayerActorInfo.TraitInfos<IBotInfo>()
  /// (the AI face rides Phase 8; default = the empty set).
  std::function<std::vector<std::pair<std::string, std::string>>(
      const map::MapPreview&)>
      fn_bot_infos;

  /// DefaultRules.Actors[World].TraitInfo<IColorPickerManagerInfo>()
  /// (缺省 = ColorPickerManagerInfo 默认值面)
  /// DefaultRules.Actors[World].TraitInfo<IColorPickerManagerInfo>()
  /// (default = the ColorPickerManagerInfo default-value face).
  std::function<const ColorPickerManagerData*()> fn_color_manager;

  /// DefaultTerrainInfo[tileset].RestrictedPlayerColors(缺省 = 空集)
  /// DefaultTerrainInfo[tileset].RestrictedPlayerColors (default = the
  /// empty set).
  std::function<std::vector<core::Color>(std::string_view)>
      fn_terrain_restricted_colors;

  /// MapCache.QueryRemoteMapDetails(repo, uids, success(uid), failure)
  /// (缺省 = 立即 failure —— 无资源中心同 observable)
  /// MapCache.QueryRemoteMapDetails(repo, uids, success(uid), failure)
  /// (default = an immediate failure — the same observable as no
  /// repository).
  std::function<void(std::span<const std::string>,
                     std::function<void(const std::string&)>,
                     std::function<void()>)>
      fn_query_remote_map;

  /// MapCache.ChooseInitialMap(initial, MersenneTwister)(缺省 = 直返
  /// initial —— 无缓存同 observable 的回落面)
  /// MapCache.ChooseInitialMap(initial, MersenneTwister) (default = the
  /// initial returned as-is — the no-cache observable fallback).
  std::function<std::string(const std::string&)> fn_choose_initial_map;

  /// Manifest.Metadata 的 Title/Website/WebIcon32(GameServer 装配)
  /// Manifest.Metadata's Title/Website/WebIcon32 (the GameServer assembly).
  std::string str_mod_title;
  std::string str_mod_website;
  std::string str_mod_icon32;

  /// Platform.SessionGUID.ToString()(LAN 广告的 Id 字段)
  /// Platform.SessionGUID.ToString() (the LAN advertisement's Id field).
  std::string str_session_guid;

  /// ILintServerMapPass 类型表(MapStatusCache;缺省 = 空集)
  /// The ILintServerMapPass type table (MapStatusCache; default = the
  /// empty set).
  std::vector<LintServerMapPassFn> vec_lint_passes;
};

/// Server(Server.cs L49-1601)
/// Server (Server.cs L49-1601).
class Server final {
 public:
  MersenneTwister Random;  // L117(new() = 时钟种子)
  const ServerType Type;   // L118
  bool IsMultiplayer() const {  // L119
    return Type == ServerType::Dedicated || Type == ServerType::Multiplayer;
  }

  std::vector<std::shared_ptr<ServerConnection>> vec_conns;  // L121

  /// LobbyInfoLock(L123;C# Lock 可重入 → recursive_mutex)
  /// LobbyInfoLock (L123; C#'s Lock is reentrant → recursive_mutex).
  std::recursive_mutex mutex_lobby;
  net::Session LobbyInfo{};     // L124
  ServerSettings Settings;  // L125
  std::vector<std::string> vec_temp_bans;  // L127
  std::string GeneratedMapData;             // L128

  /// Map 的注入承载(L131;PlayerReference 经 Deps,Uid/Title 供
  /// GameInformation)
  /// Map's injected carrier (L131; the PlayerReference via Deps, the
  /// Uid/Title for GameInformation).
  std::string str_map_uid;
  std::string str_map_title;

  /// Map(L131 的 C++ 承载:MapCache 所有的 MapPreview 借用指针;
  /// LobbyCommands.ServerStarted/Map 命令装配;null = 未装配地图)
  /// Map (L131's C++ carrier: a borrowed MapPreview pointer owned by the
  /// MapCache; assembled by LobbyCommands.ServerStarted/the Map command;
  /// null = no map assembled).
  const map::MapPreview* Map = nullptr;

  /// MapPool(L134 的 FrozenSet 承载;nullopt = 无池)
  /// MapPool (L134's FrozenSet carrier; nullopt = no pool).
  std::optional<std::vector<std::string>> opt_map_pool;

  /// SetMap:LobbyCommands 的装配面(uid/title 同步供 GameInformation)
  /// SetMap: LobbyCommands' assembly face (uid/title kept in sync for
  /// GameInformation).
  void SetMap(const map::MapPreview* p_map) {
    Map = p_map;
    str_map_uid = p_map ? p_map->Uid() : std::string{};
    str_map_title = p_map ? p_map->Title() : std::string{};
  }

  int OrderLatency = 1;  // L137

  ServerState State() const {  // L158-162(volatile → relaxed)
    return state_internal_.load(std::memory_order_relaxed);
  }
  void SetState(ServerState s) {
    state_internal_.store(s, std::memory_order_relaxed);
  }

  /// SyncClientToPlayerReference(L164-179)
  static void SyncClientToPlayerReference(net::SessionClient& c,
                                          const map::PlayerReference* p_pr);

  void Shutdown();  // L181-184

  /// EndGame(L186-193)
  void EndGame();

  /// RecordFakeHandshake(L197-226)
  void RecordFakeHandshake();

  /// ctor(L239-403):监听线程群 + 服务器事件线程;无可用监听 = IsValid
  /// false(上游 throw 的 bool 承载)
  /// The ctor (L239-403): the listener threads + the server event thread;
  /// no usable listener = IsValid false (the bool carrier of upstream's
  /// throw).
  Server(std::vector<std::pair<std::string, int>> vec_endpoints,
         ServerSettings settings, ServerDeps deps, ServerType type);
  ~Server();

  Server(const Server&) = delete;
  Server& operator=(const Server&) = delete;

  bool IsValid() const { return b_listening_; }

  int ChooseFreePlayerIndex() {  // L406-409
    return int4_next_player_index_++;
  }

  // ———— 读线程的入队面(L411-424)————
  // ———— The reader threads' enqueue faces (L411-424) ————
  void OnConnectionPacket(std::shared_ptr<ServerConnection> conn, int frame,
                          std::vector<std::uint8_t> vec_data);
  void OnConnectionPing(std::shared_ptr<ServerConnection> conn,
                        std::vector<int> vec_ping_history,
                        std::uint8_t u1_queue_length);
  void OnConnectionDisconnect(std::shared_ptr<ServerConnection> conn);

  void NotifyConnectionAlive();
  void NotifyConnectionGone();

  /// DispatchOrdersToClients(L853-862):非 SyncHash 转发 + 记账
  /// DispatchOrdersToClients (L853-862).
  void DispatchOrdersToClients(ServerConnection& conn, int frame,
                               const std::vector<std::uint8_t>& vec_data);

  /// DispatchServerOrdersToClients(L877-902)三 overload 的合并面
  /// The merged face of DispatchServerOrdersToClients' three overloads
  /// (L877-902).
  void DispatchServerOrdersToClients(const std::vector<std::uint8_t>& vec_data,
                                     int frame = 0);
  void DispatchServerOrdersToClients(
      std::span<const std::shared_ptr<ServerConnection>> vec_conns,
      const std::vector<std::uint8_t>& vec_data, int frame = 0);

  /// ReceiveOrders(L904-944):前投影 + Ack + SyncHash 记账
  /// ReceiveOrders (L904-944): the forward projection + the Ack + the
  /// SyncHash accounting.
  void ReceiveOrders(ServerConnection& conn, int frame,
                     const std::vector<std::uint8_t>& vec_data);

  /// SendOrderTo(L964-967)
  void SendOrderTo(ServerConnection& conn, std::string_view str_order,
                   std::string_view str_data);

  /// SendFluentMessage(L969-988)族
  /// The SendFluentMessage family (L969-988).
  void SendFluentMessage(std::string_view str_key,
                         std::span<const FluentArgument> vec_args = {});
  void SendFluentMessage(
      std::span<const std::shared_ptr<ServerConnection>> vec_conns,
      std::string_view str_key, std::span<const FluentArgument> vec_args = {});
  void SendFluentMessageTo(ServerConnection& conn, std::string_view str_key,
                           std::span<const FluentArgument> vec_args = {});

  /// ReceivePing(L1173-1201)
  void ReceivePing(ServerConnection& conn,
                   const std::vector<int>& vec_ping_history);

  net::SessionClient* GetClient(ServerConnection& conn);  // L1203-1209

  bool HasClientWonOrLost(const net::SessionClient& client);  // L1211-1212

  /// DropClient(L1214-1281)
  void DropClient(ServerConnection& to_drop);

  /// SyncLobbyInfo(L1283-1293)
  void SyncLobbyInfo();

  /// SyncLobbyClients(L1295-1314)
  void SyncLobbyClients();

  /// SyncLobbySlots(L1316-1331)
  void SyncLobbySlots();

  /// SyncLobbyGlobalSettings(L1333-1347)
  void SyncLobbyGlobalSettings();

  /// StartGame(L1349-1466)
  void StartGame();

  /// InterpretCommand(L1468-1475)
  bool InterpretCommand(std::string_view str_command, ServerConnection& conn);

  /// GetEndpointForLocalConnection(L1477-1492)
  net::ConnectionTarget GetEndpointForLocalConnection();

  /// InterpretServerOrder(L995-1171)的 Chat/Command 承载(GameSave/
  /// GenerateMap 面随其批)
  /// The Chat/Command carriers of InterpretServerOrder (L995-1171) (the
  /// GameSave/GenerateMap faces ride their batch).
  void InterpretServerOrders(ServerConnection& conn,
                             const std::vector<std::uint8_t>& vec_data);

  /// trait 注册(Manifest.ServerTraits 的工厂面;LobbyCommands 批接真
  /// 实现)
  /// The trait registration (Manifest.ServerTraits' factory face; the
  /// LobbyCommands batch supplies the real implementations).
  void AddServerTrait(std::unique_ptr<ServerTrait> up_trait);

  /// CallbackEvent(L1587-1600)的公开入队面(装配/测试注入,服务器线
  /// 程上执行)
  /// The public enqueue face of CallbackEvent (L1587-1600) (an assembly/
  /// test injection, executed on the server thread).
  void PostEvent(std::function<void()> fn);

  net::ReplayRecorder* Recorder() { return up_recorder_.get(); }
  OrderBuffer& GetOrderBuffer() { return order_buffer_; }
  const GameInformation* GameInfo() const {
    return up_game_info_ ? &*up_game_info_ : nullptr;
  }
  GameInformation* GameInfo() { return up_game_info_ ? &*up_game_info_ : nullptr; }

  /// ———— 第十五批的 trait 消费面 ————
  /// ———— Batch 15's trait-consumption faces ————

  /// MapStatusCache(L132;null = 未装配 —— Dedicated+lint 或测试注入)
  /// MapStatusCache (L132; null = unassembled — Dedicated+lint or a test
  /// injection).
  MapStatusCache& MapStatusCacheInstance() {
    return *up_map_status_cache_;
  }

  /// VoteKickTracker(L155)
  VoteKickTracker& GetVoteKickTracker() { return *up_vote_kick_tracker_; }

  /// Game.RunTime(trait 消费;deps.fn_clock 同源)
  /// Game.RunTime (the trait consumption; the same source as deps.
  /// fn_clock).
  std::int64_t RunTimeMs() const { return deps_.fn_clock ? deps_.fn_clock() : 0; }

  /// Log.Write("server", line)(trait 消费)
  /// Log.Write("server", line) (the trait consumption).
  void ServerLog(std::string_view str_line) const;

  /// Deps 只读面(trait 的注入族消费)
  /// The deps' read-only face (the traits' injection-family consumption).
  const ServerDeps& Deps() const { return deps_; }

  /// GameServer 装配的 Manifest.Metadata/SessionGUID 面
  /// The Manifest.Metadata/SessionGUID faces of the GameServer assembly.
  const std::string& MapUid() const { return str_map_uid; }
  const std::string& ModId() const { return deps_.str_mod_id; }
  const std::string& ModVersion() const { return deps_.str_mod_version; }
  const std::string& ModTitle() const { return deps_.str_mod_title; }
  const std::string& ModWebsite() const { return deps_.str_mod_website; }
  const std::string& ModIcon32() const { return deps_.str_mod_icon32; }
  const std::string& SessionGuid() const { return deps_.str_session_guid; }

  /// MapIsUnknown/MapIsKnown(L1494-1512)
  bool MapIsUnknown(const std::string& str_uid) const;
  bool MapIsKnown(const std::string& str_uid) const;

 private:
  /// AcceptConnection(L426-465)
  void AcceptConnection(platform::TcpSocket socket);

  /// ValidateClient(L467-707):本批全六门 + Local 直通/MP 注入鉴权
  /// ValidateClient (L467-707): all six gates this batch + Local's
  /// direct pass / MP's injected authentication.
  void ValidateClient(std::shared_ptr<ServerConnection> up_new_conn,
                      std::string_view str_data);

  /// CompleteConnection(L545-607)
  void CompleteConnection(std::shared_ptr<ServerConnection> up_new_conn,
                          net::SessionClient client);

  static std::vector<std::uint8_t> CreateFrame(  // L709-717
      int client, int frame, const std::vector<std::uint8_t>& vec_data);

  static std::vector<std::uint8_t> CreateAckFrame(  // L719-728
      int frame, std::uint8_t count);

  static std::vector<std::uint8_t> CreateTickScaleFrame(  // L730-739
      float scale);

  void DispatchOrdersToClient(ServerConnection& c, int client, int frame,  // L741-744
                              const std::vector<std::uint8_t>& vec_data);

  void DispatchFrameToClient(ServerConnection& c, int client,  // L746-753
                             std::vector<std::uint8_t> vec_frame_data);

  bool AnyUndefinedWinStates();  // L755-768

  void SetPlayerDefeat(int int4_player_index);  // L770-790

  /// OutOfSync(L792-807):服务器侧 desync —— 录像作废
  /// OutOfSync (L792-807): the server-side desync — the recording is
  /// invalidated.
  void OutOfSync(int frame);

  /// HandleSyncOrder(L813-851)
  void HandleSyncOrder(int frame, const std::vector<std::uint8_t>& vec_packet);

  void RecordOrder(int frame, const std::vector<std::uint8_t>& vec_data,  // L864-875
                   int from);

  void InterpretServerOrder(ServerConnection& conn, const net::Order& o);

  void WriteLineWithTimeStamp(std::string_view str_line);  // L990-993

  /// 事件线程体(L348-402)
  /// The event-thread body (L348-402).
  void ServerThreadBody();

  ServerDeps deps_;
  int int4_next_player_index_ = 0;  // L405
  int int4_random_seed_ = 0;        // L139
  std::vector<platform::TcpSocket> vec_listeners_;       // L140
  std::vector<std::thread> vec_listener_threads_;
  std::vector<std::pair<std::string, int>> vec_local_endpoints_;  // GetEndpointForLocalConnection 的缓存
  bool b_listening_ = false;

  std::vector<std::unique_ptr<ServerTrait>> vec_traits_;  // L141
  std::vector<IInterpretCommand*> vec_iface_interpret_command_;
  std::vector<INotifySyncLobbyInfo*> vec_iface_sync_lobby_info_;
  std::vector<INotifyServerStart*> vec_iface_server_start_;
  std::vector<INotifyServerEmpty*> vec_iface_server_empty_;
  std::vector<INotifyServerShutdown*> vec_iface_server_shutdown_;
  std::vector<IStartGame*> vec_iface_start_game_;
  std::vector<IClientJoined*> vec_iface_client_joined_;
  std::vector<IEndGame*> vec_iface_end_game_;
  std::vector<ITick*> vec_iface_tick_;

  OrderBuffer order_buffer_;  // L144

  std::atomic<ServerState> state_internal_{ServerState::WaitingPlayers};  // L146

  /// BlockingCollection<IServerEvent> → mutex/cv 闭包队列(五事件类的
  /// 调用面等价闭包)
  /// BlockingCollection<IServerEvent> → the mutex/cv closure queue (the
  /// five event classes' invocation-equivalent closures).
  struct EventQueue {
    std::mutex mutex;
    std::condition_variable cv;
    std::deque<std::function<void()>> deque_events;
    bool b_completed = false;

    void Add(std::function<void()> fn);
    bool TryTake(std::function<void()>& fn_out, int int4_timeout_ms);
  };
  EventQueue queue_events_;

  std::unique_ptr<net::ReplayRecorder> up_recorder_;  // L150
  std::unique_ptr<net::ReplayMetadata> up_replay_metadata_;  // recorder.Metadata 的所有权
  std::optional<GameInformation> up_game_info_;  // L151
  std::vector<std::optional<GameInformationPlayer>> vec_world_players_;  // L152
  std::chrono::steady_clock::time_point time_ping_updated_{
      std::chrono::steady_clock::now()};  // L153(pingUpdated)

  std::thread thread_server_;
  std::atomic<int> int4_live_connections_{0};
  std::mutex mutex_connections_gone_;
  std::condition_variable cv_connections_gone_;

  std::map<int, std::vector<std::uint8_t>> map_sync_for_frame_;  // L809
  int int4_last_defeat_state_frame_ = 0;                        // L810
  std::uint64_t uint8_last_defeat_state_ = 0;                   // L811

  friend class PlayerMessageTracker;
  std::unique_ptr<class PlayerMessageTracker> up_player_message_tracker_;

  std::unique_ptr<MapStatusCache> up_map_status_cache_;  // L132
  std::unique_ptr<VoteKickTracker> up_vote_kick_tracker_;  // L155
};

/// PlayerMessageTracker(PlayerMessageTracker.cs L17-85):加入冷却与洪水
/// 限制的聊天闸
/// PlayerMessageTracker (PlayerMessageTracker.cs L17-85): the chat gate
/// of the join cooldown and the flood limit.
class PlayerMessageTracker {
 public:
  PlayerMessageTracker(
      Server& server,
      std::function<void(ServerConnection&, int, int,
                         const std::vector<std::uint8_t>&)>
          fn_dispatch_orders_to_client,
      std::function<void(ServerConnection&, std::string_view,
                         std::span<const FluentArgument>)>
          fn_send_fluent_message_to);

  /// DisableChatUI(L37-39)
  void DisableChatUI(ServerConnection& conn, int int4_time);

  /// IsPlayerAtFloodLimit(L41-84)
  bool IsPlayerAtFloodLimit(ServerConnection& conn);

 private:
  Server* p_server_;
  std::function<void(ServerConnection&, int, int,
                     const std::vector<std::uint8_t>&)>
      fn_dispatch_orders_to_client_;
  std::function<void(ServerConnection&, std::string_view,
                     std::span<const FluentArgument>)>
      fn_send_fluent_message_to_;
  std::map<int, std::vector<long>> map_message_tracker_;  // L22
};

}  // namespace ora::server
