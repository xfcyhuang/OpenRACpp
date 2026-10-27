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

enum class ServerState {
  WaitingPlayers = 1,
  GameStarted = 2,
  ShuttingDown = 3,
};

enum class ServerType {
  Local = 0,
  Skirmish = 1,
  Multiplayer = 2,
  Dedicated = 3,
};

struct ServerDeps {

  ServerConnection::ClockFn fn_clock;

  std::function<std::string()> fn_new_game_uid;

  net::ReplayRecorder::ChooseFilename fn_timestamped_filename;

  std::string str_mod_id;
  std::string str_mod_version;

  std::string str_player_name = "Commander";

  std::function<const map::PlayerReference*(
      std::string_view str_slot)> fn_player_reference;

  std::function<void(net::Session& lobby_info,
                     std::vector<std::optional<GameInformationPlayer>>&
                         vec_world_players_out,
                     MersenneTwister& player_random)>
      fn_create_server_players;

  std::function<const game::GameSpeeds*()> fn_game_speeds;

  std::function<std::string(std::string_view str_key)> fn_fluent_get;

  std::function<void(std::string_view str_line)> fn_server_log;

  std::function<bool(std::string_view str_fingerprint,
                     std::string_view str_signature,
                     std::string_view str_auth_token,
                     std::string& str_profile_id_out)>
      fn_verify_auth;

  std::function<map::MapPreview*(const std::string&)> fn_map_cache_at;

  std::function<std::vector<std::string>()> fn_available_lobby_maps;

  std::function<std::vector<LobbyOptionData>(const map::MapPreview&)>
      fn_lobby_options;

  std::function<std::vector<std::pair<std::string, std::string>>(
      const map::MapPreview&)>
      fn_bot_infos;

  std::function<const ColorPickerManagerData*()> fn_color_manager;

  std::function<std::vector<core::Color>(std::string_view)>
      fn_terrain_restricted_colors;

  std::function<void(std::span<const std::string>,
                     std::function<void(const std::string&)>,
                     std::function<void()>)>
      fn_query_remote_map;

  std::function<std::string(const std::string&)> fn_choose_initial_map;

  std::string str_mod_title;
  std::string str_mod_website;
  std::string str_mod_icon32;

  std::string str_session_guid;

  std::vector<LintServerMapPassFn> vec_lint_passes;
};

class Server final {
 public:
  MersenneTwister Random;
  const ServerType Type;
  bool IsMultiplayer() const {
    return Type == ServerType::Dedicated || Type == ServerType::Multiplayer;
  }

  std::vector<std::shared_ptr<ServerConnection>> vec_conns;

  std::recursive_mutex mutex_lobby;
  net::Session LobbyInfo{};
  ServerSettings Settings;
  std::vector<std::string> vec_temp_bans;
  std::string GeneratedMapData;

  std::string str_map_uid;
  std::string str_map_title;

  const map::MapPreview* Map = nullptr;

  std::optional<std::vector<std::string>> opt_map_pool;

  void SetMap(const map::MapPreview* p_map) {
    Map = p_map;
    str_map_uid = p_map ? p_map->Uid() : std::string{};
    str_map_title = p_map ? p_map->Title() : std::string{};
  }

  int OrderLatency = 1;

  ServerState State() const {
    return state_internal_.load(std::memory_order_relaxed);
  }
  void SetState(ServerState s) {
    state_internal_.store(s, std::memory_order_relaxed);
  }

  static void SyncClientToPlayerReference(net::SessionClient& c,
                                          const map::PlayerReference* p_pr);

  void Shutdown();

  void EndGame();

  void RecordFakeHandshake();

  Server(std::vector<std::pair<std::string, int>> vec_endpoints,
         ServerSettings settings, ServerDeps deps, ServerType type);
  ~Server();

  Server(const Server&) = delete;
  Server& operator=(const Server&) = delete;

  bool IsValid() const { return b_listening_; }

  int ChooseFreePlayerIndex() {
    return int4_next_player_index_++;
  }

  void OnConnectionPacket(std::shared_ptr<ServerConnection> conn, int frame,
                          std::vector<std::uint8_t> vec_data);
  void OnConnectionPing(std::shared_ptr<ServerConnection> conn,
                        std::vector<int> vec_ping_history,
                        std::uint8_t u1_queue_length);
  void OnConnectionDisconnect(std::shared_ptr<ServerConnection> conn);

  void NotifyConnectionAlive();
  void NotifyConnectionGone();

  void DispatchOrdersToClients(ServerConnection& conn, int frame,
                               const std::vector<std::uint8_t>& vec_data);

  void DispatchServerOrdersToClients(const std::vector<std::uint8_t>& vec_data,
                                     int frame = 0);
  void DispatchServerOrdersToClients(
      std::span<const std::shared_ptr<ServerConnection>> vec_conns,
      const std::vector<std::uint8_t>& vec_data, int frame = 0);

  void ReceiveOrders(ServerConnection& conn, int frame,
                     const std::vector<std::uint8_t>& vec_data);

  void SendOrderTo(ServerConnection& conn, std::string_view str_order,
                   std::string_view str_data);

  void SendFluentMessage(std::string_view str_key,
                         std::span<const FluentArgument> vec_args = {});
  void SendFluentMessage(
      std::span<const std::shared_ptr<ServerConnection>> vec_conns,
      std::string_view str_key, std::span<const FluentArgument> vec_args = {});
  void SendFluentMessageTo(ServerConnection& conn, std::string_view str_key,
                           std::span<const FluentArgument> vec_args = {});

  void ReceivePing(ServerConnection& conn,
                   const std::vector<int>& vec_ping_history);

  net::SessionClient* GetClient(ServerConnection& conn);

  bool HasClientWonOrLost(const net::SessionClient& client);

  void DropClient(ServerConnection& to_drop);

  void SyncLobbyInfo();

  void SyncLobbyClients();

  void SyncLobbySlots();

  void SyncLobbyGlobalSettings();

  void StartGame();

  bool InterpretCommand(std::string_view str_command, ServerConnection& conn);

  net::ConnectionTarget GetEndpointForLocalConnection();

  void InterpretServerOrders(ServerConnection& conn,
                             const std::vector<std::uint8_t>& vec_data);

  void AddServerTrait(std::unique_ptr<ServerTrait> up_trait);

  void PostEvent(std::function<void()> fn);

  net::ReplayRecorder* Recorder() { return up_recorder_.get(); }
  OrderBuffer& GetOrderBuffer() { return order_buffer_; }
  const GameInformation* GameInfo() const {
    return up_game_info_ ? &*up_game_info_ : nullptr;
  }
  GameInformation* GameInfo() { return up_game_info_ ? &*up_game_info_ : nullptr; }

  MapStatusCache& MapStatusCacheInstance() {
    return *up_map_status_cache_;
  }

  VoteKickTracker& GetVoteKickTracker() { return *up_vote_kick_tracker_; }

  std::int64_t RunTimeMs() const { return deps_.fn_clock ? deps_.fn_clock() : 0; }

  void ServerLog(std::string_view str_line) const;

  const ServerDeps& Deps() const { return deps_; }

  const std::string& MapUid() const { return str_map_uid; }
  const std::string& ModId() const { return deps_.str_mod_id; }
  const std::string& ModVersion() const { return deps_.str_mod_version; }
  const std::string& ModTitle() const { return deps_.str_mod_title; }
  const std::string& ModWebsite() const { return deps_.str_mod_website; }
  const std::string& ModIcon32() const { return deps_.str_mod_icon32; }
  const std::string& SessionGuid() const { return deps_.str_session_guid; }

  bool MapIsUnknown(const std::string& str_uid) const;
  bool MapIsKnown(const std::string& str_uid) const;

 private:

  void AcceptConnection(platform::TcpSocket socket);

  void ValidateClient(std::shared_ptr<ServerConnection> up_new_conn,
                      std::string_view str_data);

  void CompleteConnection(std::shared_ptr<ServerConnection> up_new_conn,
                          net::SessionClient client);

  static std::vector<std::uint8_t> CreateFrame(
      int client, int frame, const std::vector<std::uint8_t>& vec_data);

  static std::vector<std::uint8_t> CreateAckFrame(
      int frame, std::uint8_t count);

  static std::vector<std::uint8_t> CreateTickScaleFrame(
      float scale);

  void DispatchOrdersToClient(ServerConnection& c, int client, int frame,
                              const std::vector<std::uint8_t>& vec_data);

  void DispatchFrameToClient(ServerConnection& c, int client,
                             std::vector<std::uint8_t> vec_frame_data);

  bool AnyUndefinedWinStates();

  void SetPlayerDefeat(int int4_player_index);

  void OutOfSync(int frame);

  void HandleSyncOrder(int frame, const std::vector<std::uint8_t>& vec_packet);

  void RecordOrder(int frame, const std::vector<std::uint8_t>& vec_data,
                   int from);

  void InterpretServerOrder(ServerConnection& conn, const net::Order& o);

  void WriteLineWithTimeStamp(std::string_view str_line);

  void ServerThreadBody();

  ServerDeps deps_;
  int int4_next_player_index_ = 0;
  int int4_random_seed_ = 0;
  std::vector<platform::TcpSocket> vec_listeners_;
  std::vector<std::thread> vec_listener_threads_;
  std::vector<std::pair<std::string, int>> vec_local_endpoints_;
  bool b_listening_ = false;

  std::vector<std::unique_ptr<ServerTrait>> vec_traits_;
  std::vector<IInterpretCommand*> vec_iface_interpret_command_;
  std::vector<INotifySyncLobbyInfo*> vec_iface_sync_lobby_info_;
  std::vector<INotifyServerStart*> vec_iface_server_start_;
  std::vector<INotifyServerEmpty*> vec_iface_server_empty_;
  std::vector<INotifyServerShutdown*> vec_iface_server_shutdown_;
  std::vector<IStartGame*> vec_iface_start_game_;
  std::vector<IClientJoined*> vec_iface_client_joined_;
  std::vector<IEndGame*> vec_iface_end_game_;
  std::vector<ITick*> vec_iface_tick_;

  OrderBuffer order_buffer_;

  std::atomic<ServerState> state_internal_{ServerState::WaitingPlayers};

  struct EventQueue {
    std::mutex mutex;
    std::condition_variable cv;
    std::deque<std::function<void()>> deque_events;
    bool b_completed = false;

    void Add(std::function<void()> fn);
    bool TryTake(std::function<void()>& fn_out, int int4_timeout_ms);
  };
  EventQueue queue_events_;

  std::unique_ptr<net::ReplayRecorder> up_recorder_;
  std::unique_ptr<net::ReplayMetadata> up_replay_metadata_;
  std::optional<GameInformation> up_game_info_;
  std::vector<std::optional<GameInformationPlayer>> vec_world_players_;
  std::chrono::steady_clock::time_point time_ping_updated_{
      std::chrono::steady_clock::now()};

  std::thread thread_server_;
  std::atomic<int> int4_live_connections_{0};
  std::mutex mutex_connections_gone_;
  std::condition_variable cv_connections_gone_;

  std::map<int, std::vector<std::uint8_t>> map_sync_for_frame_;
  int int4_last_defeat_state_frame_ = 0;
  std::uint64_t uint8_last_defeat_state_ = 0;

  friend class PlayerMessageTracker;
  std::unique_ptr<class PlayerMessageTracker> up_player_message_tracker_;

  std::unique_ptr<MapStatusCache> up_map_status_cache_;
  std::unique_ptr<VoteKickTracker> up_vote_kick_tracker_;
};

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

  void DisableChatUI(ServerConnection& conn, int int4_time);

  bool IsPlayerAtFloodLimit(ServerConnection& conn);

 private:
  Server* p_server_;
  std::function<void(ServerConnection&, int, int,
                     const std::vector<std::uint8_t>&)>
      fn_dispatch_orders_to_client_;
  std::function<void(ServerConnection&, std::string_view,
                     std::span<const FluentArgument>)>
      fn_send_fluent_message_to_;
  std::map<int, std::vector<long>> map_message_tracker_;
};

}
