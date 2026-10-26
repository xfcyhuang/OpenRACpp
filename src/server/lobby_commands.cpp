// UPSTREAM: OpenRA.Mods.Common/ServerTraits/LobbyCommands.cs @1f594e320
//          L29-1492 + LobbyUtils.cs L289-302(实现体;Dictionary 命令表 →
//          静态 string_view/function 表,LINQ → 消费时物化)
//          The implementation body (LobbyCommands.cs + LobbyUtils.cs
//          L289-302; the Dictionary command table → a static
//          string_view/function table, LINQ → consumption-time
//          materialization).
#include "server/lobby_commands.hpp"

#include <cstdio>

#include "core/color.hpp"
#include "map/map_players.hpp"
#include "meta/parse.hpp"
#include "net/fluent_message.hpp"
#include "server/server.hpp"
#include "server/server_connection.hpp"
#include "sim/player.hpp"
#include "yaml/mini_yaml.hpp"

namespace ora::server {

using ora::net::FluentArgument;
using ora::net::FluentArgumentType;
using ora::net::FluentMessage;

namespace {

constexpr std::string_view kCustomRules = "notification-custom-rules";
constexpr std::string_view kOnlyHostStartGame = "notification-admin-start-game";
constexpr std::string_view kNoStartUntilRequiredSlotsFull =
    "notification-no-start-until-required-slots-full";
constexpr std::string_view kNoStartWithoutPlayers =
    "notification-no-start-without-players";
constexpr std::string_view kTwoHumansRequired = "notification-two-humans-required";
constexpr std::string_view kInsufficientEnabledSpawnPoints =
    "notification-insufficient-enabled-spawn-points";
constexpr std::string_view kMalformedCommand = "notification-malformed-command";
constexpr std::string_view kKickNone = "notification-kick-none";
constexpr std::string_view kNoKickSelf = "notification-kick-self";
constexpr std::string_view kNoKickGameStarted = "notification-no-kick-game-started";
constexpr std::string_view kAdminKicked = "notification-admin-kicked";
constexpr std::string_view kKicked = "notification-kicked";
constexpr std::string_view kTempBan = "notification-temp-ban";
constexpr std::string_view kNoTransferAdmin = "notification-admin-transfer-admin";
constexpr std::string_view kEmptySlot = "notification-empty-slot";
constexpr std::string_view kMoveSpectators = "notification-move-spectators";
constexpr std::string_view kNick = "notification-nick-changed";
constexpr std::string_view kStateUnchangedReady = "notification-state-unchanged-ready";
constexpr std::string_view kStateUnchangedGameStarted =
    "notification-state-unchanged-game-started";
constexpr std::string_view kInvalidFactionSelected =
    "notification-invalid-faction-selected";
constexpr std::string_view kRequiresHost = "notification-requires-host";
constexpr std::string_view kInvalidBotSlot = "notification-invalid-bot-slot";
constexpr std::string_view kInvalidBotType = "notification-invalid-bot-type";
constexpr std::string_view kHostChangeMap = "notification-admin-change-map";
constexpr std::string_view kUnknownMap = "notification-unknown-map";
constexpr std::string_view kSearchingMap = "notification-searching-map";
constexpr std::string_view kNotAdmin = "notification-admin-change-configuration";
constexpr std::string_view kInvalidConfigurationCommand =
    "notification-invalid-configuration-command";
constexpr std::string_view kChangedMap = "notification-changed-map";
constexpr std::string_view kHiddenMapHasBeenPreviewed =
    "notification-hidden-map-has-been-previewed";
constexpr std::string_view kMapBotsDisabled = "notification-map-bots-disabled";
constexpr std::string_view kNoMoveSpectators = "notification-admin-move-spectators";
constexpr std::string_view kAdminOption = "notification-admin-option";
constexpr std::string_view kNumberTeams = "notification-error-number-teams";
constexpr std::string_view kAdminClearSpawn = "notification-admin-clear-spawn";
constexpr std::string_view kSpawnOccupied = "notification-spawn-occupied";
constexpr std::string_view kSpawnLocked = "notification-spawn-locked";
constexpr std::string_view kAdminLobbyInfo = "notification-admin-lobby-info";
constexpr std::string_view kInvalidLobbyInfo = "notification-invalid-lobby-info";
constexpr std::string_view kAdminKick = "notification-admin-kick";
constexpr std::string_view kSlotClosed = "notification-slot-closed";
constexpr std::string_view kNewAdmin = "notification-new-admin";
constexpr std::string_view kYouWereKicked = "notification-you-were-kicked";
constexpr std::string_view kVoteKickDisabled = "notification-vote-kick-disabled";

std::vector<std::string> SplitSpace(std::string_view sv) {
  auto vec_out = std::vector<std::string>{};
  std::size_t sz_pos = 0;
  while (sz_pos <= sv.size()) {
    const auto sz_next = sv.find(' ', sz_pos);
    const auto sz_end = sz_next == std::string_view::npos ? sv.size() : sz_next;
    vec_out.emplace_back(sv.substr(sz_pos, sz_end - sz_pos));
    if (sz_next == std::string_view::npos)
      break;
    sz_pos = sz_next + 1;
  }
  return vec_out;
}

std::string JoinWith(std::span<const std::string> vec, std::string_view str_sep) {
  auto str_out = std::string{};
  for (std::size_t sz_i = 0; sz_i < vec.size(); sz_i++) {
    if (sz_i > 0)
      str_out += str_sep;
    str_out += vec[sz_i];
  }
  return str_out;
}

/// bool.TryParse(空白容忍 + 大小写不敏感)
/// bool.TryParse (whitespace-tolerant + case-insensitive).
bool TryParseBool(std::string_view sv, bool& b_out) {
  const auto sz_begin = sv.find_first_not_of(" \t\r\n");
  const auto sz_end = sv.find_last_not_of(" \t\r\n");
  if (sz_begin == std::string_view::npos) {
    b_out = false;
    return false;
  }
  const auto str_trimmed =
      std::string{sv.substr(sz_begin, sz_end - sz_begin + 1)};
  if (str_trimmed == "True" || str_trimmed == "true") {
    b_out = true;
    return true;
  }
  if (str_trimmed == "False" || str_trimmed == "false") {
    b_out = false;
    return true;
  }
  b_out = false;
  return false;
}

/// Enum.TryParse<ClientState>(大小写敏感名匹配)
/// Enum.TryParse<ClientState> (the case-sensitive name match).
bool TryParseClientState(std::string_view sv, net::ClientState& state_out) {
  if (sv == "NotReady") {
    state_out = net::ClientState::NotReady;
    return true;
  }
  if (sv == "Invalid") {
    state_out = net::ClientState::Invalid;
    return true;
  }
  if (sv == "Ready") {
    state_out = net::ClientState::Ready;
    return true;
  }
  if (sv == "Disconnected") {
    state_out = net::ClientState::Disconnected;
    return true;
  }
  return false;
}

std::string_view ClientStateName(net::ClientState state) {
  switch (state) {
    case net::ClientState::NotReady:
      return "NotReady";
    case net::ClientState::Invalid:
      return "Invalid";
    case net::ClientState::Ready:
      return "Ready";
    case net::ClientState::Disconnected:
      return "Disconnected";
  }
  return "";
}

/// WorldActorInfo.TraitInfos<FactionInfo>() 的可选面汇聚
/// The selectable gathering of WorldActorInfo.TraitInfos<FactionInfo>().
std::vector<std::string> SelectableFactionsOf(const map::MapPreview& map) {
  auto vec_out = std::vector<std::string>{};
  const auto ptr_world = map.WorldActorInfo();
  if (!ptr_world)
    return vec_out;
  for (const meta::RecordObject* rec : ptr_world->TraitInfosByInterface(
           "OpenRA.Traits.ITraitInfoInterface")) {
    if (rec->record_desc().str_name != "FactionInfo")
      continue;
    const auto faction = sim::FactionInfoData::Parse(*rec);
    if (faction.Selectable)
      vec_out.push_back(faction.InternalName);
  }
  return vec_out;
}

std::int64_t SteadyMs() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::steady_clock::now().time_since_epoch())
      .count();
}

}  // namespace

const std::vector<std::pair<std::string_view, LobbyCommands::Handler>>&
LobbyCommands::CommandHandlers() {
  static const std::vector<std::pair<std::string_view, Handler>> vec_table{
      {"state", State},
      {"startgame", StartGame},
      {"slot", Slot},
      {"allow_spectators", AllowSpectators},
      {"spectate", Spectate},
      {"slot_close", SlotClose},
      {"slot_open", SlotOpen},
      {"slot_bot", SlotBot},
      {"map", Map},
      {"option", Option},
      {"reset_options", ResetOptions},
      {"assignteams", AssignTeams},
      {"kick", Kick},
      {"vote_kick", VoteKick},
      {"make_admin", MakeAdmin},
      {"make_spectator", MakeSpectator},
      {"name", Name},
      {"faction", Faction},
      {"team", Team},
      {"handicap", Handicap},
      {"spawn", Spawn},
      {"clear_spawn", ClearPlayerSpawn},
      {"color", PlayerColor},
      {"sync_lobby", SyncLobby},
  };
  return vec_table;
}

bool LobbyCommands::ValidateSlotCommand(Server& server, ServerConnection& conn,
                                        const net::SessionClient& client,
                                        std::string_view str_arg,
                                        bool b_requires_host) {
  std::lock_guard lock{server.mutex_lobby};
  if (server.LobbyInfo.FindSlot(std::string{str_arg}) == nullptr) {
    server.ServerLog(std::format("Invalid slot: {}", str_arg));
    return false;
  }

  if (b_requires_host && !client.IsAdmin) {
    server.SendFluentMessageTo(conn, kRequiresHost);
    return false;
  }

  return true;
}

bool LobbyCommands::ValidateCommand(Server& server, ServerConnection& conn,
                                    const net::SessionClient& client,
                                    std::string_view str_command) {
  std::lock_guard lock{server.mutex_lobby};

  if (str_command.starts_with("kick ") ||
      str_command.starts_with("vote_kick "))
    return true;

  if (server.State() == ServerState::GameStarted) {
    server.SendFluentMessageTo(
        conn, kStateUnchangedGameStarted,
        std::vector<FluentArgument>{{"command", std::string{str_command}}});
    return false;
  } else if (client.State == net::ClientState::Ready &&
             !(str_command.starts_with("state") || str_command == "startgame")) {
    server.SendFluentMessageTo(conn, kStateUnchangedReady);
    return false;
  }

  return true;
}

bool LobbyCommands::InterpretCommand(Server& server, ServerConnection& conn,
                                     net::SessionClient* p_client,
                                     std::string_view str_cmd) {
  if (p_client == nullptr || !ValidateCommand(server, conn, *p_client, str_cmd))
    return false;

  const auto vec_split = SplitSpace(str_cmd);
  const auto& str_cmd_name = vec_split.front();
  const auto str_cmd_value = JoinWith(
      std::span{vec_split}.subspan(1), " ");

  for (const auto& [str_name, fn] : CommandHandlers()) {
    if (str_name == str_cmd_name)
      return fn(server, conn, p_client, str_cmd_value);
  }

  return false;
}

void LobbyCommands::CheckAutoStart(Server& server) {
  std::lock_guard lock{server.mutex_lobby};
  const auto vec_non_bot = server.LobbyInfo.NonBotPlayers();

  const net::SessionClient* p_admin = nullptr;
  for (const auto& c : server.LobbyInfo.vec_clients)
    if (c.IsAdmin) {
      p_admin = &c;
      break;
    }
  if (p_admin == nullptr)
    return;

  bool b_all_ready = true;
  for (const auto* c : vec_non_bot)
    if (c->State != net::ClientState::Ready) {
      b_all_ready = false;
      break;
    }
  if (!b_all_ready || p_admin->State != net::ClientState::Ready)
    return;

  if (!server.LobbyInfo.global_settings.EnableSingleplayer &&
      vec_non_bot.size() < 2)
    return;

  for (const auto& [str_key, slot] : server.LobbyInfo.vec_slots)
    if (slot.Required &&
        server.LobbyInfo.ClientInSlot(str_key) == nullptr)
      return;

  bool b_any_slotted = false;
  for (const auto& [str_key, slot] : server.LobbyInfo.vec_slots)
    if (server.LobbyInfo.ClientInSlot(str_key) != nullptr) {
      b_any_slotted = true;
      break;
    }
  if (!b_any_slotted)
    return;

  if (server.Type != ServerType::Dedicated) {
    const auto* p_host_map =
        server.Deps().fn_map_cache_at
            ? server.Deps().fn_map_cache_at(server.MapUid())
            : nullptr;
    const auto kind_status = p_host_map != nullptr
                                 ? p_host_map->Status()
                                 : map::MapStatus::Unavailable;
    if (kind_status != map::MapStatus::Available) {
      net::SessionClient* p_host = nullptr;
      for (auto& c : server.LobbyInfo.vec_clients)
        if (c.Index == 0) {
          p_host = &c;
          break;
        }
      if (p_host != nullptr) {
        p_host->State = net::ClientState::NotReady;
      } else {
        for (auto& c : server.LobbyInfo.vec_clients)
          c.State = net::ClientState::NotReady;
      }

      server.SyncLobbyClients();
      return;
    }
  }

  if (server.Map != nullptr &&
      InsufficientEnabledSpawnPoints(*server.Map, server.LobbyInfo))
    return;

  server.StartGame();
}

bool LobbyCommands::State(Server& server, ServerConnection& conn,
                          net::SessionClient* p_client, std::string_view str_s) {
  std::lock_guard lock{server.mutex_lobby};

  auto state = net::ClientState::Invalid;
  if (!TryParseClientState(str_s, state)) {
    server.SendFluentMessageTo(
        conn, kMalformedCommand,
        std::vector<FluentArgument>{{"command", "state"}});
    return true;
  }

  p_client->State = state;
  server.ServerLog(std::format("Player @{} is {}", conn.str_end_point,
                               ClientStateName(p_client->State)));

  server.SyncLobbyClients();
  CheckAutoStart(server);

  return true;
}

bool LobbyCommands::StartGame(Server& server, ServerConnection& conn,
                              net::SessionClient* p_client, std::string_view) {
  std::lock_guard lock{server.mutex_lobby};

  if (!p_client->IsAdmin) {
    server.SendFluentMessageTo(conn, kOnlyHostStartGame);
    return true;
  }

  for (const auto& [str_key, slot] : server.LobbyInfo.vec_slots)
    if (slot.Required && server.LobbyInfo.ClientInSlot(str_key) == nullptr) {
      server.SendFluentMessageTo(conn, kNoStartUntilRequiredSlotsFull);
      return true;
    }

  bool b_any_valid = false;
  for (const auto& [str_key, slot] : server.LobbyInfo.vec_slots) {
    const auto* p_occupant = server.LobbyInfo.ClientInSlot(str_key);
    if (p_occupant != nullptr &&
        p_occupant->State != net::ClientState::Invalid) {
      b_any_valid = true;
      break;
    }
  }
  if (!b_any_valid) {
    server.SendFluentMessageTo(conn, kNoStartWithoutPlayers);
    return true;
  }

  if (!server.LobbyInfo.global_settings.EnableSingleplayer &&
      server.LobbyInfo.NonBotPlayers().size() < 2) {
    server.SendFluentMessageTo(conn, kTwoHumansRequired);
    return true;
  }

  if (server.Map != nullptr &&
      InsufficientEnabledSpawnPoints(*server.Map, server.LobbyInfo)) {
    server.SendFluentMessageTo(conn, kInsufficientEnabledSpawnPoints);
    return true;
  }

  server.StartGame();

  return true;
}

bool LobbyCommands::Slot(Server& server, ServerConnection& conn,
                         net::SessionClient* p_client, std::string_view str_s) {
  std::lock_guard lock{server.mutex_lobby};

  const auto str_slot = std::string{str_s};
  const auto* p_slot = server.LobbyInfo.FindSlot(str_slot);
  if (p_slot == nullptr) {
    server.ServerLog(std::format("Invalid slot: {}", str_slot));
    return false;
  }

  if (p_slot->Closed || server.LobbyInfo.ClientInSlot(str_slot) != nullptr)
    return false;

  if (!p_client->Slot.empty()) {
    const auto* p_old_slot = server.LobbyInfo.FindSlot(p_client->Slot);
    if (p_old_slot != nullptr && p_old_slot->LockSpawn)
      p_client->SpawnPoint = 0;
  }

  p_client->Slot = str_slot;
  Server::SyncClientToPlayerReference(
      *p_client, PlayerReferenceForSlot(server, p_slot));

  if (!p_slot->LockColor)
    p_client->PreferredColor = p_client->Color =
        SanitizePlayerColor(server, p_client->Color, p_client->Index, &conn);

  server.SyncLobbyClients();
  CheckAutoStart(server);

  return true;
}

bool LobbyCommands::AllowSpectators(Server& server, ServerConnection& conn,
                                    net::SessionClient*,
                                    std::string_view str_s) {
  std::lock_guard lock{server.mutex_lobby};

  bool b_value = false;
  if (TryParseBool(str_s, b_value)) {
    server.LobbyInfo.global_settings.AllowSpectators = b_value;
    server.SyncLobbyGlobalSettings();
    return true;
  }

  server.SendFluentMessageTo(
      conn, kMalformedCommand,
      std::vector<FluentArgument>{{"command", "allow_spectate"}});

  return true;
}

bool LobbyCommands::Spectate(Server& server, ServerConnection&,
                             net::SessionClient* p_client, std::string_view) {
  std::lock_guard lock{server.mutex_lobby};

  if (server.LobbyInfo.global_settings.AllowSpectators || p_client->IsAdmin) {
    p_client->Slot.clear();
    p_client->SpawnPoint = 0;
    p_client->Team = 0;
    p_client->Handicap = 0;
    p_client->Color = core::Color::FromArgb(0xFFFFFFFF);
    server.SyncLobbyClients();
    CheckAutoStart(server);
    return true;
  }

  return false;
}

bool LobbyCommands::SlotClose(Server& server, ServerConnection& conn,
                              net::SessionClient* p_client,
                              std::string_view str_s) {
  std::lock_guard lock{server.mutex_lobby};

  if (!ValidateSlotCommand(server, conn, *p_client, str_s, true))
    return false;

  const auto str_slot = std::string{str_s};
  auto* p_occupant = server.LobbyInfo.ClientInSlot(str_slot);
  if (p_occupant != nullptr) {
    if (p_occupant->IsBot()) {
      auto& vec = server.LobbyInfo.vec_clients;
      for (auto it = vec.begin(); it != vec.end(); ++it)
        if (&*it == p_occupant) {
          vec.erase(it);
          break;
        }
      server.SyncLobbyClients();
    } else {
      std::shared_ptr<ServerConnection> ptr_occupant_conn;
      for (const auto& c : server.vec_conns)
        if (c->int4_player_index == p_occupant->Index) {
          ptr_occupant_conn = c;
          break;
        }
      if (ptr_occupant_conn) {
        server.SendOrderTo(conn, "ServerError", kSlotClosed);
        server.DropClient(*ptr_occupant_conn);
      }
    }
  }

  if (auto* p_slot = server.LobbyInfo.FindSlot(str_slot))
    p_slot->Closed = true;
  server.SyncLobbySlots();

  return true;
}

bool LobbyCommands::SlotOpen(Server& server, ServerConnection& conn,
                             net::SessionClient* p_client,
                             std::string_view str_s) {
  std::lock_guard lock{server.mutex_lobby};

  if (!ValidateSlotCommand(server, conn, *p_client, str_s, true))
    return false;

  const auto str_slot = std::string{str_s};
  if (auto* p_slot = server.LobbyInfo.FindSlot(str_slot))
    p_slot->Closed = false;
  server.SyncLobbySlots();

  auto* p_occupant = server.LobbyInfo.ClientInSlot(str_slot);
  if (p_occupant != nullptr && p_occupant->IsBot()) {
    auto& vec = server.LobbyInfo.vec_clients;
    for (auto it = vec.begin(); it != vec.end(); ++it)
      if (&*it == p_occupant) {
        vec.erase(it);
        break;
      }
  }

  server.SyncLobbyClients();

  return true;
}

bool LobbyCommands::SlotBot(Server& server, ServerConnection& conn,
                            net::SessionClient* p_client, std::string_view str_s) {
  std::lock_guard lock{server.mutex_lobby};

  const auto vec_parts = SplitSpace(str_s);
  if (vec_parts.size() < 3) {
    server.SendFluentMessageTo(
        conn, kMalformedCommand,
        std::vector<FluentArgument>{{"command", "slot_bot"}});
    return true;
  }

  if (!ValidateSlotCommand(server, conn, *p_client, vec_parts[0], true))
    return false;

  auto* p_slot = server.LobbyInfo.FindSlot(vec_parts[0]);
  auto* p_bot = server.LobbyInfo.ClientInSlot(vec_parts[0]);
  int int4_controller = 0;
  if (!meta::TryParseInt32Invariant(vec_parts[1], int4_controller)) {
    server.ServerLog(
        std::format("Invalid bot controller client index: {}", vec_parts[1]));
    return false;
  }

  if (p_bot != nullptr && !p_bot->IsBot()) {
    server.SendFluentMessageTo(conn, kInvalidBotSlot);
    return true;
  }

  const auto& str_bot_type = vec_parts[2];
  const std::string* p_bot_info = nullptr;
  if (server.Map != nullptr && server.Deps().fn_bot_infos) {
    for (const auto& [str_type, str_name] :
         server.Deps().fn_bot_infos(*server.Map))
      if (str_type == str_bot_type) {
        p_bot_info = &str_name;
        break;
      }
  }

  if (p_bot_info == nullptr) {
    server.SendFluentMessageTo(conn, kInvalidBotType);
    return true;
  }

  if (p_slot != nullptr)
    p_slot->Closed = false;
  if (p_bot == nullptr) {
    auto bot = net::SessionClient{};
    bot.Index = server.ChooseFreePlayerIndex();
    bot.Name = *p_bot_info;
    bot.Bot = str_bot_type;
    bot.Slot = vec_parts[0];
    bot.Faction = "Random";
    bot.SpawnPoint = 0;
    bot.Team = 0;
    bot.Handicap = 0;
    bot.State = net::ClientState::NotReady;
    bot.BotControllerClientIndex = int4_controller;

    std::vector<core::Color> vec_terrain;
    if (server.Map != nullptr && server.Deps().fn_terrain_restricted_colors)
      vec_terrain =
          server.Deps().fn_terrain_restricted_colors(server.Map->TileSet());
    auto vec_player_colors = std::vector<core::Color>{};
    for (const auto& c : server.LobbyInfo.vec_clients)
      vec_player_colors.push_back(c.Color);
    if (server.Map != nullptr)
      for (const auto& [str_key, pr] : server.Map->Players().Players)
        vec_player_colors.push_back(pr.Color);

    static const ColorPickerManagerData data_default{};
    const ColorPickerManagerData* p_data =
        server.Deps().fn_color_manager
            ? server.Deps().fn_color_manager()
            : nullptr;
    if (p_data == nullptr)
      p_data = &data_default;

    bot.Color = bot.PreferredColor =
        ColorRandomPresetColor(*p_data, server.Random, vec_terrain,
                               vec_player_colors);

    server.LobbyInfo.vec_clients.push_back(std::move(bot));
    p_bot = &server.LobbyInfo.vec_clients.back();
  } else {
    p_bot->Name = *p_bot_info;
    p_bot->Bot = str_bot_type;
  }

  Server::SyncClientToPlayerReference(
      *p_bot, PlayerReferenceForSlot(server, p_slot));
  server.SyncLobbyClients();
  server.SyncLobbySlots();

  return true;
}

bool LobbyCommands::Map(Server& server, ServerConnection& conn,
                        net::SessionClient* p_client, std::string_view str_s) {
  std::lock_guard lock{server.mutex_lobby};

  if (!p_client->IsAdmin) {
    server.SendFluentMessageTo(conn, kHostChangeMap);
    return true;
  }

  const auto str_uid = std::string{str_s};
  const auto fn_query_failed = [&server, &conn] {
    server.SendFluentMessageTo(conn, kUnknownMap);
  };

  if (server.opt_map_pool.has_value() &&
      std::find(server.opt_map_pool->begin(), server.opt_map_pool->end(),
                str_uid) == server.opt_map_pool->end()) {
    fn_query_failed();
    return true;
  }

  const auto str_last_map = server.LobbyInfo.global_settings.Map;
  const auto int4_commander_index = p_client->Index;
  const auto fn_select_map = [&server, &conn, int4_commander_index,
                              str_last_map](const map::MapPreview& map) {
    std::lock_guard lock_inner{server.mutex_lobby};

    // 远程回调晚到时命令者可能已离场 —— 上游引用常青,C++ 以 Index 复
    // 解析(null 即放弃)
    // The commander may have left by the time the remote callback runs —
    // upstream's reference stays evergreen, C++ re-resolves by Index (a
    // null abandons the change).
    auto* p_commander =
        server.LobbyInfo.ClientWithIndex(int4_commander_index);
    if (p_commander == nullptr)
      return;

    if (server.LobbyInfo.global_settings.Map != str_last_map)
      return;

    server.LobbyInfo.global_settings.Map = map.Uid();

    std::vector<std::string> vec_old_slots;
    for (const auto& [str_key, slot] : server.LobbyInfo.vec_slots)
      vec_old_slots.push_back(str_key);

    server.SetMap(&map);
    server.LobbyInfo.global_settings.MapStatusFlags =
        server.MapStatusCacheInstance().StatusOf(map);

    auto vec_slots_new = std::vector<std::pair<std::string, net::SessionSlot>>{};
    for (const auto& [str_key, pr] : map.Players().Players)
      if (auto opt_slot = MakeSlotFromPlayerReference(pr))
        vec_slots_new.emplace_back(pr.Name, std::move(*opt_slot));
    server.LobbyInfo.vec_slots = std::move(vec_slots_new);

    LoadMapSettings(server, server.LobbyInfo.global_settings, map);

    const auto vec_selectable = SelectableFactionsOf(map);

    for (auto& c : server.LobbyInfo.vec_clients) {
      c.Faction = SanitizePlayerFaction(server, c.Faction, vec_selectable);
      c.State = net::ClientState::Invalid;
    }

    std::vector<std::string> vec_bot_types;
    if (server.Deps().fn_bot_infos)
      for (const auto& [str_type, str_name] : server.Deps().fn_bot_infos(map))
        vec_bot_types.push_back(str_type);

    std::vector<std::string> vec_new_slot_keys;
    for (const auto& [str_key, slot] : server.LobbyInfo.vec_slots)
      vec_new_slot_keys.push_back(str_key);

    std::size_t sz_i = 0;
    for (const auto& str_os : vec_old_slots) {
      auto* c = server.LobbyInfo.ClientInSlot(str_os);
      if (c == nullptr)
        continue;

      c->SpawnPoint = 0;
      c->Slot = sz_i < vec_new_slot_keys.size()
                    ? vec_new_slot_keys[sz_i++]
                    : std::string{};
      if (!c->Slot.empty()) {
        const auto* p_pr = map.Players().Find(c->Slot);
        if (c->IsBot() &&
            (p_pr == nullptr || !p_pr->AllowBots ||
             std::find(vec_bot_types.begin(), vec_bot_types.end(), c->Bot) ==
                 vec_bot_types.end())) {
          auto& vec = server.LobbyInfo.vec_clients;
          for (auto it = vec.begin(); it != vec.end(); ++it)
            if (&*it == c) {
              vec.erase(it);
              break;
            }
          continue;
        }
        Server::SyncClientToPlayerReference(*c, p_pr);
      } else if (c->IsBot()) {
        auto& vec = server.LobbyInfo.vec_clients;
        for (auto it = vec.begin(); it != vec.end(); ++it)
          if (&*it == c) {
            vec.erase(it);
            break;
          }
        continue;
      } else {
        c->Color = core::Color::FromArgb(0xFFFFFFFF);
      }
    }

    for (auto& c : server.LobbyInfo.vec_clients) {
      const auto* p_slot =
          c.Slot.empty() ? nullptr : server.LobbyInfo.FindSlot(c.Slot);
      if (!c.Slot.empty() && p_slot != nullptr && !p_slot->LockColor)
        c.Color = c.PreferredColor =
            SanitizePlayerColor(server, c.Color, c.Index, &conn);
    }

    server.LobbyInfo.vec_disabled_spawn_points.clear();

    server.SendFluentMessage(
        kChangedMap,
        std::vector<FluentArgument>{{"player", p_commander->Name},
                                    {"map", map.Title()}});

    auto kind_visibility =
        map::MapGenerationArgs::PreviewVisibilityFlags::All;
    if (const auto& args = map.GenerationArgs())
      kind_visibility = args->kind_preview_visibility;
    const auto b_hidden_needs_disclaimer =
        (static_cast<std::int32_t>(kind_visibility) &
         static_cast<std::int32_t>(
             map::MapGenerationArgs::PreviewVisibilityFlags::Lobby)) == 0 &&
        (static_cast<std::int32_t>(kind_visibility) &
         static_cast<std::int32_t>(
             map::MapGenerationArgs::PreviewVisibilityFlags::MapChooser)) != 0;
    if (b_hidden_needs_disclaimer)
      server.SendFluentMessage(kHiddenMapHasBeenPreviewed);

    server.SyncLobbyInfo();

    if (net::HasMapStatusFlag(
            server.LobbyInfo.global_settings.MapStatusFlags,
            net::SessionMapStatus::UnsafeCustomRules))
      server.SendFluentMessage(kCustomRules);

    if (!server.LobbyInfo.global_settings.EnableSingleplayer) {
      server.SendFluentMessage(kTwoHumansRequired);
    } else {
      bool b_all_no_bots = true;
      bool b_any_playable = false;
      for (const auto& [str_key, pr] : map.Players().Players) {
        if (!pr.Playable)
          continue;
        b_any_playable = true;
        if (pr.AllowBots) {
          b_all_no_bots = false;
          break;
        }
      }
      if (b_any_playable && b_all_no_bots)
        server.SendFluentMessage(kMapBotsDisabled);
    }
  };

  const auto* p_preview =
      server.Deps().fn_map_cache_at ? server.Deps().fn_map_cache_at(str_uid)
                                    : nullptr;
  const auto kind_status = p_preview != nullptr
                               ? p_preview->Status()
                               : map::MapStatus::Unavailable;
  if (p_preview != nullptr &&
      (kind_status == map::MapStatus::Available ||
       kind_status == map::MapStatus::DownloadAvailable ||
       kind_status == map::MapStatus::Generatable ||
       kind_status == map::MapStatus::Generating)) {
    fn_select_map(*p_preview);
  } else if (server.Settings.QueryMapRepository) {
    server.SendFluentMessageTo(conn, kSearchingMap);
    auto b_reported = std::make_shared<bool>(false);
    const auto vec_uids = std::vector<std::string>{str_uid};
    if (server.Deps().fn_query_remote_map) {
      server.Deps().fn_query_remote_map(
          vec_uids,
          [fn_select_map, p_preview, b_reported, fn_query_failed](
              const std::string&) {
            if (p_preview != nullptr)
              fn_select_map(*p_preview);
            else if (!*b_reported)
              fn_query_failed();
            *b_reported = true;
          },
          [b_reported, fn_query_failed] {
            if (!*b_reported)
              fn_query_failed();
            *b_reported = true;
          });
    } else {
      fn_query_failed();
    }
  } else {
    fn_query_failed();
  }

  return true;
}

bool LobbyCommands::Option(Server& server, ServerConnection& conn,
                           net::SessionClient* p_client, std::string_view str_s) {
  std::lock_guard lock{server.mutex_lobby};

  if (!p_client->IsAdmin) {
    server.SendFluentMessageTo(conn, kNotAdmin);
    return true;
  }

  auto map_options = std::vector<LobbyOptionData>{};
  if (server.Map != nullptr && server.Deps().fn_lobby_options)
    map_options = server.Deps().fn_lobby_options(*server.Map);

  auto map_by_id = std::map<std::string, const LobbyOptionData*>{};
  for (const auto& o : map_options)
    map_by_id[o.Id] = &o;

  const auto vec_split = SplitSpace(str_s);
  const LobbyOptionData* p_option = nullptr;
  if (vec_split.size() >= 2) {
    const auto it = map_by_id.find(vec_split[0]);
    if (it != map_by_id.end())
      p_option = it->second;
  }
  if (vec_split.size() < 2 || p_option == nullptr || p_option->IsLocked ||
      !p_option->HasValue(vec_split[1])) {
    server.SendFluentMessageTo(conn, kInvalidConfigurationCommand);
    return true;
  }

  auto& oo = server.LobbyInfo.global_settings.map_lobby_options[p_option->Id];
  if (oo.Value == vec_split[1])
    return true;

  if (!p_option->HasValue(vec_split[1])) {
    server.SendFluentMessageTo(conn, kInvalidConfigurationCommand);
    return true;
  }

  oo.Value = oo.PreferredValue = vec_split[1];

  server.SyncLobbyGlobalSettings();
  for (auto& c : server.LobbyInfo.vec_clients)
    c.State = net::ClientState::NotReady;

  server.SyncLobbyClients();

  return true;
}

bool LobbyCommands::ResetOptions(Server& server, ServerConnection& conn,
                                 net::SessionClient* p_client,
                                 std::string_view) {
  std::lock_guard lock{server.mutex_lobby};

  if (!p_client->IsAdmin) {
    server.SendFluentMessageTo(conn, kNotAdmin);
    return true;
  }

  auto map_options_new =
      std::map<std::string, net::SessionLobbyOption>{};
  if (server.Map != nullptr && server.Deps().fn_lobby_options) {
    for (const auto& o : server.Deps().fn_lobby_options(*server.Map)) {
      net::SessionLobbyOption state{};
      state.IsLocked = o.IsLocked;
      state.Value = o.DefaultValue;
      state.PreferredValue = o.DefaultValue;
      map_options_new.emplace(o.Id, std::move(state));
    }
  }
  server.LobbyInfo.global_settings.map_lobby_options =
      std::move(map_options_new);

  server.SyncLobbyGlobalSettings();

  for (auto& c : server.LobbyInfo.vec_clients)
    c.State = net::ClientState::NotReady;

  server.SyncLobbyClients();

  return true;
}

bool LobbyCommands::AssignTeams(Server& server, ServerConnection& conn,
                                net::SessionClient* p_client,
                                std::string_view str_raw) {
  std::lock_guard lock{server.mutex_lobby};

  if (!p_client->IsAdmin) {
    server.SendFluentMessageTo(conn, kAdminOption);
    return true;
  }

  int int4_team_count = 0;
  if (!meta::TryParseInt32Invariant(str_raw, int4_team_count)) {
    server.SendFluentMessageTo(
        conn, kNumberTeams,
        std::vector<FluentArgument>{{"raw", std::string{str_raw}}});
    return true;
  }

  int int4_slotted = 0;
  for (const auto& c : server.LobbyInfo.vec_clients)
    if (!c.Slot.empty())
      int4_slotted++;
  const auto int4_max_teams = (int4_slotted + 1) / 2;
  int4_team_count = std::clamp(int4_team_count, 0, int4_max_teams);

  auto vec_clients = std::vector<net::SessionClient*>{};
  for (const auto& [str_key, slot] : server.LobbyInfo.vec_slots) {
    auto* c = server.LobbyInfo.ClientInSlot(str_key);
    if (c == nullptr)
      continue;
    const auto* p_slot = server.LobbyInfo.FindSlot(c->Slot);
    if (p_slot != nullptr && !p_slot->LockTeam)
      vec_clients.push_back(c);
  }

  auto int4_assigned = 0;
  const auto int4_client_count = static_cast<int>(vec_clients.size());
  for (auto* player : vec_clients) {
    if (int4_team_count == 0)
      player->Team = 0;
    else if (int4_team_count == 1)
      player->Team = player->IsBot() ? 2 : 1;
    else
      player->Team =
          int4_assigned++ * int4_team_count / int4_client_count + 1;
  }

  server.SyncLobbyClients();

  return true;
}

namespace {

/// SingleOrDefault(Conns, GetClient(c)?.Index == id) 的查找面
/// The SingleOrDefault lookup over Conns by GetClient(c)?.Index == id.
std::shared_ptr<ServerConnection> FindConnByClientIndex(
    Server& server, int int4_client_id) {
  std::shared_ptr<ServerConnection> ptr_found;
  for (const auto& c : server.vec_conns) {
    auto* client = server.GetClient(*c);
    if (client != nullptr && client->Index == int4_client_id) {
      if (ptr_found)
        return nullptr;
      ptr_found = c;
    }
  }
  return ptr_found;
}

}  // namespace

bool LobbyCommands::Kick(Server& server, ServerConnection& conn,
                         net::SessionClient* p_client, std::string_view str_s) {
  std::lock_guard lock{server.mutex_lobby};

  if (!p_client->IsAdmin) {
    server.SendFluentMessageTo(conn, kAdminKick);
    return true;
  }

  const auto vec_split = SplitSpace(str_s);
  if (vec_split.size() < 2) {
    server.SendFluentMessageTo(
        conn, kMalformedCommand,
        std::vector<FluentArgument>{{"command", "kick"}});
    return true;
  }

  int int4_kick_id = 0;
  auto ptr_kick_conn =
      meta::TryParseInt32Invariant(vec_split[0], int4_kick_id)
          ? FindConnByClientIndex(server, int4_kick_id)
          : nullptr;

  if (!ptr_kick_conn) {
    server.SendFluentMessageTo(conn, kKickNone);
    return true;
  }

  auto* p_kick_client = server.GetClient(*ptr_kick_conn);
  if (p_kick_client == p_client) {
    server.SendFluentMessageTo(conn, kNoKickSelf);
    return true;
  }

  if (server.State() == ServerState::GameStarted &&
      !p_kick_client->IsObserver() &&
      !server.HasClientWonOrLost(*p_kick_client)) {
    server.SendFluentMessageTo(conn, kNoKickGameStarted);
    return true;
  }

  // DropClient 会从 LobbyInfo 移除该 client —— 引用语义的字段先快照
  // DropClient removes the client from LobbyInfo — snapshot the
  // reference-semantics fields first.
  const auto str_kick_name = p_kick_client->Name;
  const auto str_kick_ip = p_kick_client->IPAddress;

  server.ServerLog(std::format("Kicking client {}.", int4_kick_id));
  server.SendFluentMessage(
      kAdminKicked,
      std::vector<FluentArgument>{{"admin", p_client->Name},
                                  {"player", str_kick_name}});
  server.SendOrderTo(*ptr_kick_conn, "ServerError", kYouWereKicked);
  server.DropClient(*ptr_kick_conn);

  bool b_temp_ban = false;
  if (TryParseBool(vec_split[1], b_temp_ban) && b_temp_ban) {
    server.ServerLog(std::format("Temporarily banning client {} ({}).",
                                 int4_kick_id, str_kick_ip));
    server.SendFluentMessage(
        kTempBan,
        std::vector<FluentArgument>{{"admin", p_client->Name},
                                    {"player", str_kick_name}});
    server.vec_temp_bans.push_back(str_kick_ip);
  }

  server.SyncLobbyClients();
  server.SyncLobbySlots();

  return true;
}

bool LobbyCommands::VoteKick(Server& server, ServerConnection& conn,
                             net::SessionClient* p_client, std::string_view str_s) {
  std::lock_guard lock{server.mutex_lobby};

  const auto vec_split = SplitSpace(str_s);
  if (vec_split.size() != 2) {
    server.SendFluentMessageTo(
        conn, kMalformedCommand,
        std::vector<FluentArgument>{{"command", "vote_kick"}});
    return true;
  }

  if (!server.Settings.EnableVoteKick) {
    server.SendFluentMessageTo(conn, kVoteKickDisabled);
    return true;
  }

  int int4_kick_id = 0;
  auto ptr_kick_conn =
      meta::TryParseInt32Invariant(vec_split[0], int4_kick_id)
          ? FindConnByClientIndex(server, int4_kick_id)
          : nullptr;

  if (!ptr_kick_conn) {
    server.SendFluentMessageTo(conn, kKickNone);
    return true;
  }

  auto* p_kick_client = server.GetClient(*ptr_kick_conn);
  if (p_kick_client == p_client) {
    server.SendFluentMessageTo(conn, kNoKickSelf);
    return true;
  }

  bool b_vote = false;
  if (!TryParseBool(vec_split[1], b_vote)) {
    server.SendFluentMessageTo(
        conn, kMalformedCommand,
        std::vector<FluentArgument>{{"command", "vote_kick"}});
    return true;
  }

  if (server.GetVoteKickTracker().VoteKick(conn, *p_client, *ptr_kick_conn,
                                           *p_kick_client, int4_kick_id,
                                           b_vote)) {
    const auto str_kick_name = p_kick_client->Name;
    server.ServerLog(std::format("Kicking client {}.", int4_kick_id));
    server.SendFluentMessage(
        kKicked,
        std::vector<FluentArgument>{{"player", str_kick_name}});
    server.SendOrderTo(*ptr_kick_conn, "ServerError", kYouWereKicked);
    server.DropClient(*ptr_kick_conn);

    server.SyncLobbyClients();
    server.SyncLobbySlots();
  }

  return true;
}

void LobbyCommands::Tick(Server& server) {
  server.GetVoteKickTracker().Tick();
}

bool LobbyCommands::MakeAdmin(Server& server, ServerConnection& conn,
                              net::SessionClient* p_client,
                              std::string_view str_s) {
  std::lock_guard lock{server.mutex_lobby};

  if (!p_client->IsAdmin) {
    server.SendFluentMessageTo(conn, kNoTransferAdmin);
    return true;
  }

  int int4_new_admin_id = 0;
  auto ptr_new_admin =
      meta::TryParseInt32Invariant(str_s, int4_new_admin_id)
          ? FindConnByClientIndex(server, int4_new_admin_id)
          : nullptr;

  if (!ptr_new_admin) {
    server.SendFluentMessageTo(conn, kEmptySlot);
    return true;
  }

  auto* p_new_admin = server.GetClient(*ptr_new_admin);
  p_client->IsAdmin = false;
  p_new_admin->IsAdmin = true;

  for (const auto& [str_key, slot] : server.LobbyInfo.vec_slots) {
    auto* c = server.LobbyInfo.ClientInSlot(str_key);
    if (c != nullptr && c->IsBot())
      c->BotControllerClientIndex = int4_new_admin_id;
  }

  server.SendFluentMessage(
      kNewAdmin,
      std::vector<FluentArgument>{{"player", p_new_admin->Name}});
  server.ServerLog(std::format("{} is now the admin.", p_new_admin->Name));
  server.SyncLobbyClients();

  return true;
}

bool LobbyCommands::MakeSpectator(Server& server, ServerConnection& conn,
                                  net::SessionClient* p_client,
                                  std::string_view str_s) {
  std::lock_guard lock{server.mutex_lobby};

  if (!p_client->IsAdmin) {
    server.SendFluentMessageTo(conn, kNoMoveSpectators);
    return true;
  }

  int int4_target_id = 0;
  auto ptr_target =
      meta::TryParseInt32Invariant(str_s, int4_target_id)
          ? FindConnByClientIndex(server, int4_target_id)
          : nullptr;

  if (!ptr_target) {
    server.SendFluentMessageTo(conn, kEmptySlot);
    return true;
  }

  auto* p_target = server.GetClient(*ptr_target);
  p_target->Slot.clear();
  p_target->SpawnPoint = 0;
  p_target->Team = 0;
  p_target->Handicap = 0;
  p_target->Color = core::Color::FromArgb(0xFFFFFFFF);
  p_target->State = net::ClientState::NotReady;
  server.SendFluentMessage(
      kMoveSpectators,
      std::vector<FluentArgument>{{"admin", p_client->Name},
                                  {"player", p_target->Name}});
  server.ServerLog(
      std::format("{} moved {} to spectators.", p_client->Name, p_target->Name));
  server.SyncLobbyClients();
  CheckAutoStart(server);

  return true;
}

bool LobbyCommands::Name(Server& server, ServerConnection& conn,
                         net::SessionClient* p_client, std::string_view str_s) {
  std::lock_guard lock{server.mutex_lobby};

  const auto str_sanitized =
      SanitizedPlayerName(str_s, server.Deps().str_player_name);
  if (str_sanitized == p_client->Name)
    return true;

  server.ServerLog(std::format("Player@{} is now known as {}.",
                               conn.str_end_point, str_sanitized));
  server.SendFluentMessage(
      kNick,
      std::vector<FluentArgument>{{"player", p_client->Name},
                                  {"name", str_sanitized}});
  p_client->Name = str_sanitized;
  server.SyncLobbyClients();

  return true;
}

bool LobbyCommands::TryParseClientCommand(
    Server& server, ServerConnection& conn, std::string_view str_command,
    std::string_view str_s, std::vector<std::string>& vec_parts_out,
    net::SessionClient*& p_target_out) {
  vec_parts_out = SplitSpace(str_s);
  p_target_out = nullptr;
  int int4_target_id = 0;
  if (vec_parts_out.size() != 2 ||
      !meta::TryParseInt32Invariant(vec_parts_out[0], int4_target_id)) {
    server.SendFluentMessageTo(
        conn, kMalformedCommand,
        std::vector<FluentArgument>{{"command", std::string{str_command}}});
    return false;
  }

  p_target_out = server.LobbyInfo.ClientWithIndex(int4_target_id);
  if (p_target_out == nullptr) {
    server.SendFluentMessageTo(
        conn, kMalformedCommand,
        std::vector<FluentArgument>{{"command", std::string{str_command}}});
    return false;
  }

  return true;
}

bool LobbyCommands::Faction(Server& server, ServerConnection& conn,
                            net::SessionClient* p_client, std::string_view str_s) {
  std::lock_guard lock{server.mutex_lobby};

  auto vec_parts = std::vector<std::string>{};
  auto* p_target = static_cast<net::SessionClient*>(nullptr);
  if (!TryParseClientCommand(server, conn, "faction", str_s, vec_parts,
                             p_target) ||
      p_target->Slot.empty())
    return true;

  if (p_target->Index != p_client->Index && !p_client->IsAdmin)
    return true;

  const auto* p_slot = server.LobbyInfo.FindSlot(p_target->Slot);
  if (p_slot != nullptr && p_slot->LockFaction)
    return true;

  const auto& str_faction = vec_parts[1];
  auto b_valid = false;
  if (server.Map != nullptr) {
    const auto vec_selectable = SelectableFactionsOf(*server.Map);
    b_valid = std::find(vec_selectable.begin(), vec_selectable.end(),
                        str_faction) != vec_selectable.end();
  }

  if (!b_valid) {
    server.SendFluentMessageTo(
        conn, kInvalidFactionSelected,
        std::vector<FluentArgument>{{"faction", str_faction}});
    return true;
  }

  p_target->Faction = str_faction;
  server.SyncLobbyClients();

  return true;
}

bool LobbyCommands::Team(Server& server, ServerConnection& conn,
                         net::SessionClient* p_client, std::string_view str_s) {
  std::lock_guard lock{server.mutex_lobby};

  auto vec_parts = std::vector<std::string>{};
  auto* p_target = static_cast<net::SessionClient*>(nullptr);
  if (!TryParseClientCommand(server, conn, "team", str_s, vec_parts,
                             p_target) ||
      p_target->Slot.empty())
    return true;

  if (p_target->Index != p_client->Index && !p_client->IsAdmin)
    return true;

  const auto* p_slot = server.LobbyInfo.FindSlot(p_target->Slot);
  if (p_slot != nullptr && p_slot->LockTeam)
    return true;

  int int4_team = 0;
  if (!meta::TryParseInt32Invariant(vec_parts[1], int4_team)) {
    server.ServerLog(std::format("Invalid team: {}", str_s));
    return false;
  }

  p_target->Team = int4_team;
  server.SyncLobbyClients();

  return true;
}

bool LobbyCommands::Handicap(Server& server, ServerConnection& conn,
                             net::SessionClient* p_client,
                             std::string_view str_s) {
  std::lock_guard lock{server.mutex_lobby};

  auto vec_parts = std::vector<std::string>{};
  auto* p_target = static_cast<net::SessionClient*>(nullptr);
  if (!TryParseClientCommand(server, conn, "handicap", str_s, vec_parts,
                             p_target) ||
      p_target->Slot.empty())
    return true;

  if (p_target->Index != p_client->Index && !p_client->IsAdmin)
    return true;

  const auto* p_slot = server.LobbyInfo.FindSlot(p_target->Slot);
  if (p_slot != nullptr && p_slot->LockHandicap)
    return true;

  int int4_handicap = 0;
  if (!meta::TryParseInt32Invariant(vec_parts[1], int4_handicap)) {
    server.ServerLog(std::format("Invalid handicap: {}", str_s));
    return false;
  }

  if (int4_handicap < 0 || int4_handicap > 95 || int4_handicap % 5 != 0) {
    server.ServerLog(std::format("Invalid handicap: {}", str_s));
    return false;
  }

  p_target->Handicap = int4_handicap;
  server.SyncLobbyClients();

  return true;
}

bool LobbyCommands::ClearPlayerSpawn(Server& server, ServerConnection& conn,
                                     net::SessionClient* p_client,
                                     std::string_view str_s) {
  int int4_spawn_point = 0;
  if (!meta::TryParseInt32Invariant(str_s, int4_spawn_point)) {
    server.SendFluentMessageTo(
        conn, kMalformedCommand,
        std::vector<FluentArgument>{{"command", "clear_spawn"}});
    return true;
  }

  if (int4_spawn_point == 0)
    return true;

  net::SessionClient* p_existing = nullptr;
  for (auto& cc : server.LobbyInfo.vec_clients)
    if (cc.SpawnPoint == int4_spawn_point) {
      p_existing = &cc;
      break;
    }
  if (p_client != p_existing && !p_client->IsAdmin) {
    server.SendFluentMessageTo(conn, kAdminClearSpawn);
    return true;
  }

  if (p_existing != nullptr) {
    if (!p_existing->Slot.empty()) {
      const auto* p_slot = server.LobbyInfo.FindSlot(p_existing->Slot);
      if (p_slot != nullptr && p_slot->LockSpawn)
        return true;
    }

    p_existing->SpawnPoint = 0;
    if (p_existing->State == net::ClientState::Ready)
      p_existing->State = net::ClientState::NotReady;

    server.SyncLobbyClients();
    return true;
  }

  auto& vec_disabled = server.LobbyInfo.vec_disabled_spawn_points;
  const auto it = std::find(vec_disabled.begin(), vec_disabled.end(),
                            int4_spawn_point);
  if (it != vec_disabled.end())
    vec_disabled.erase(it);
  else
    vec_disabled.push_back(int4_spawn_point);

  server.SyncLobbyInfo();
  return true;
}

bool LobbyCommands::Spawn(Server& server, ServerConnection& conn,
                          net::SessionClient* p_client, std::string_view str_s) {
  std::lock_guard lock{server.mutex_lobby};

  auto vec_parts = std::vector<std::string>{};
  auto* p_target = static_cast<net::SessionClient*>(nullptr);
  if (!TryParseClientCommand(server, conn, "spawn", str_s, vec_parts,
                             p_target) ||
      p_target->Slot.empty())
    return true;

  if (p_target->Index != p_client->Index && !p_client->IsAdmin)
    return true;

  if (p_target->Slot.empty())
    return true;

  const auto* p_slot = server.LobbyInfo.FindSlot(p_target->Slot);
  if (p_slot != nullptr && p_slot->LockSpawn)
    return true;

  int int4_spawn_point = 0;
  const auto int4_spawn_count =
      server.Map != nullptr
          ? static_cast<int>(server.Map->SpawnPoints().size())
          : 0;
  if (!meta::TryParseInt32Invariant(vec_parts[1], int4_spawn_point) ||
      int4_spawn_point < 0 || int4_spawn_point > int4_spawn_count) {
    server.ServerLog(std::format("Invalid spawn point: {}", vec_parts[1]));
    return true;
  }

  for (const auto& cc : server.LobbyInfo.vec_clients)
    if (&cc != p_client && cc.SpawnPoint == int4_spawn_point &&
        cc.SpawnPoint != 0) {
      server.SendFluentMessageTo(conn, kSpawnOccupied);
      return true;
    }

  if (int4_spawn_point > 0) {
    auto b_locked_by_other = false;
    for (const auto& [str_key, slot] : server.LobbyInfo.vec_slots) {
      if (!slot.LockSpawn)
        continue;
      const auto* p_pr = PlayerReferenceForSlot(server, &slot);
      if (p_pr != nullptr && p_pr->Spawn == int4_spawn_point) {
        b_locked_by_other = true;
        break;
      }
    }

    if (b_locked_by_other) {
      server.SendFluentMessageTo(conn, kSpawnLocked);
      return true;
    }
  }

  p_target->SpawnPoint = int4_spawn_point;
  server.SyncLobbyClients();

  return true;
}

bool LobbyCommands::PlayerColor(Server& server, ServerConnection& conn,
                                net::SessionClient* p_client,
                                std::string_view str_s) {
  std::lock_guard lock{server.mutex_lobby};

  auto vec_parts = std::vector<std::string>{};
  auto* p_target = static_cast<net::SessionClient*>(nullptr);
  if (!TryParseClientCommand(server, conn, "color", str_s, vec_parts,
                             p_target))
    return true;

  if (p_target->Index != p_client->Index && !p_client->IsAdmin)
    return true;

  const auto* p_slot = p_target->Slot.empty()
                           ? nullptr
                           : server.LobbyInfo.FindSlot(p_target->Slot);
  if (p_target->Slot.empty() || (p_slot != nullptr && p_slot->LockColor))
    return true;

  auto color_new = core::Color{};
  if (!core::Color::TryParse(vec_parts[1], color_new)) {
    server.SendFluentMessageTo(
        conn, kMalformedCommand,
        std::vector<FluentArgument>{{"command", "color"}});
    return true;
  }

  p_target->Color = SanitizePlayerColor(server, color_new, p_target->Index, &conn);

  if (color_new == p_target->Color)
    p_target->PreferredColor = p_target->Color;

  server.SyncLobbyClients();

  return true;
}

bool LobbyCommands::SyncLobby(Server& server, ServerConnection& conn,
                              net::SessionClient* p_client,
                              std::string_view str_s) {
  std::lock_guard lock{server.mutex_lobby};

  if (!p_client->IsAdmin) {
    server.SendFluentMessageTo(conn, kAdminLobbyInfo);
    return true;
  }

  try {
    server.LobbyInfo = net::Session::Deserialize(str_s, "SyncLobby");
    server.SyncLobbyInfo();
  } catch (const std::exception&) {
    server.SendFluentMessageTo(conn, kInvalidLobbyInfo);
  }

  return true;
}

void LobbyCommands::InitializeMapPool(Server& server) {
  if (server.Type != ServerType::Dedicated)
    return;

  if (!server.Settings.vec_map_pool.empty())
    server.opt_map_pool = server.Settings.vec_map_pool;
  else if (!server.Settings.QueryMapRepository) {
    server.opt_map_pool =
        server.Deps().fn_available_lobby_maps
            ? server.Deps().fn_available_lobby_maps()
            : std::vector<std::string>{};
  } else
    return;

  auto vec_unknown = std::vector<std::string>{};
  for (const auto& str_uid : *server.opt_map_pool)
    if (server.MapIsUnknown(str_uid))
      vec_unknown.push_back(str_uid);
  if (vec_unknown.empty())
    return;

  if (server.Settings.QueryMapRepository) {
    server.ServerLog(std::format(
        "Querying Resource Center for information on {} maps...",
        vec_unknown.size()));

    if (server.Deps().fn_query_remote_map) {
      auto vec_uids = std::vector<std::string>{vec_unknown};
      server.Deps().fn_query_remote_map(
          vec_uids, [](const std::string&) {}, [] {});
    }

    const auto time_start = std::chrono::steady_clock::now();
    while (true) {
      bool b_any_searching = false;
      for (const auto& str_uid : vec_unknown) {
        const auto* p_map =
            server.Deps().fn_map_cache_at
                ? server.Deps().fn_map_cache_at(str_uid)
                : nullptr;
        if (p_map != nullptr &&
            p_map->Status() == map::MapStatus::Searching) {
          b_any_searching = true;
          break;
        }
      }
      if (!b_any_searching ||
          std::chrono::duration_cast<std::chrono::milliseconds>(
              std::chrono::steady_clock::now() - time_start)
                  .count() >= 10000)
        break;
      std::this_thread::sleep_for(std::chrono::milliseconds{100});
    }
  }

  auto vec_still_unknown = std::vector<std::string>{};
  for (const auto& str_uid : *server.opt_map_pool)
    if (server.MapIsUnknown(str_uid))
      vec_still_unknown.push_back(str_uid);
  if (!vec_still_unknown.empty())
    server.ServerLog(std::format("Failed to resolve maps: {}",
                                 JoinWith(vec_still_unknown, ", ")));
}

std::string LobbyCommands::ChooseInitialMap(Server& server) {
  if (server.MapIsKnown(server.Settings.Map))
    return server.Settings.Map;

  if (!server.opt_map_pool.has_value()) {
    // MapCache.ChooseInitialMap(settings.Map, new MersenneTwister()) 的
    // 注入面;缺省 = 直返(空表回落;D226)
    // MapCache.ChooseInitialMap's injection face; the default returns the
    // input as-is (the empty-table fallback; D226).
    return server.Deps().fn_choose_initial_map
               ? server.Deps().fn_choose_initial_map(server.Settings.Map)
               : server.Settings.Map;
  }

  auto vec_known = std::vector<std::string>{};
  for (const auto& str_uid : *server.opt_map_pool)
    if (server.MapIsKnown(str_uid))
      vec_known.push_back(str_uid);

  if (vec_known.empty())
    return std::string{};

  auto random = MersenneTwister{static_cast<std::int32_t>(
      static_cast<std::uint32_t>(SteadyMs() & 0x7FFFFFFF))};
  return vec_known[static_cast<std::size_t>(
      random.Next(static_cast<int>(vec_known.size())))];
}

void LobbyCommands::ServerStarted(Server& server) {
  std::lock_guard lock{server.mutex_lobby};

  InitializeMapPool(server);

  const auto str_uid = ChooseInitialMap(server);
  if (str_uid.empty()) {
    // 上游 throw InvalidOperationException("Unable to resolve a valid
    // initial map")的 no-throw 承载(D226)
    // The no-throw carrier of upstream's throw (D226).
    server.ServerLog("Unable to resolve a valid initial map");
    return;
  }

  server.LobbyInfo.global_settings.Map = server.Settings.Map = str_uid;
  const auto* p_map = server.Deps().fn_map_cache_at
                          ? server.Deps().fn_map_cache_at(str_uid)
                          : nullptr;
  server.SetMap(p_map);
  if (server.Map != nullptr) {
    server.LobbyInfo.global_settings.MapStatusFlags =
        server.MapStatusCacheInstance().StatusOf(*server.Map);

    auto vec_slots_new =
        std::vector<std::pair<std::string, net::SessionSlot>>{};
    for (const auto& [str_key, pr] : server.Map->Players().Players)
      if (auto opt_slot = MakeSlotFromPlayerReference(pr))
        vec_slots_new.emplace_back(pr.Name, std::move(*opt_slot));
    server.LobbyInfo.vec_slots = std::move(vec_slots_new);

    LoadMapSettings(server, server.LobbyInfo.global_settings, *server.Map);
  }
}

std::optional<net::SessionSlot> LobbyCommands::MakeSlotFromPlayerReference(
    const map::PlayerReference& pr) {
  if (!pr.Playable)
    return std::nullopt;

  auto slot = net::SessionSlot{};
  slot.PlayerReference = pr.Name;
  slot.Closed = false;
  slot.AllowBots = pr.AllowBots;
  slot.LockFaction = pr.LockFaction;
  slot.LockColor = pr.LockColor;
  slot.LockTeam = pr.LockTeam;
  slot.LockHandicap = pr.LockHandicap;
  slot.LockSpawn = pr.LockSpawn;
  slot.Required = pr.Required;
  return slot;
}

void LobbyCommands::LoadMapSettings(Server& server,
                                    net::SessionGlobalSettings& gs,
                                    const map::MapPreview& map) {
  std::lock_guard lock{server.mutex_lobby};

  auto vec_options = std::vector<LobbyOptionData>{};
  if (server.Deps().fn_lobby_options)
    vec_options = server.Deps().fn_lobby_options(map);

  for (const auto& o : vec_options) {
    auto str_value = o.DefaultValue;
    auto str_preferred = o.DefaultValue;
    const auto it = gs.map_lobby_options.find(o.Id);
    if (it != gs.map_lobby_options.end()) {
      if (!o.IsLocked) {
        if (o.HasValue(it->second.PreferredValue))
          str_value = it->second.PreferredValue;
        else if (o.HasValue(it->second.Value))
          str_value = it->second.Value;
      }

      str_preferred = it->second.PreferredValue;
    }

    auto& state = it != gs.map_lobby_options.end()
                      ? it->second
                      : gs.map_lobby_options[o.Id];
    state.IsLocked = o.IsLocked;
    state.Value = str_value;
    state.PreferredValue = str_preferred;
    gs.map_lobby_options[o.Id] = state;
  }
}

core::Color LobbyCommands::SanitizePlayerColor(
    Server& server, core::Color asked_color, int int4_player_index,
    ServerConnection* p_connection_to_echo) {
  std::lock_guard lock{server.mutex_lobby};

  static const ColorPickerManagerData data_default{};
  const ColorPickerManagerData* p_data =
      server.Deps().fn_color_manager ? server.Deps().fn_color_manager()
                                     : nullptr;
  if (p_data == nullptr)
    p_data = &data_default;

  std::vector<core::Color> vec_terrain;
  if (server.Map != nullptr && server.Deps().fn_terrain_restricted_colors)
    vec_terrain =
        server.Deps().fn_terrain_restricted_colors(server.Map->TileSet());

  auto vec_player_colors = std::vector<core::Color>{};
  for (const auto& c : server.LobbyInfo.vec_clients)
    if (c.Index != int4_player_index)
      vec_player_colors.push_back(c.Color);
  if (server.Map != nullptr)
    for (const auto& [str_key, pr] : server.Map->Players().Players)
      vec_player_colors.push_back(pr.Color);

  return ColorMakeValid(
      *p_data, asked_color, server.Random, vec_terrain, vec_player_colors,
      [p_connection_to_echo, &server](std::string_view str_message) {
        if (p_connection_to_echo != nullptr && !str_message.empty())
          server.SendFluentMessageTo(*p_connection_to_echo, str_message);
      });
}

std::string LobbyCommands::SanitizePlayerFaction(
    Server&, std::string_view str_asked_faction,
    std::span<const std::string> vec_valid_factions) {
  return std::find(vec_valid_factions.begin(), vec_valid_factions.end(),
                   str_asked_faction) == vec_valid_factions.end()
             ? std::string{"Random"}
             : std::string{str_asked_faction};
}

const map::PlayerReference* LobbyCommands::PlayerReferenceForSlot(
    Server& server, const net::SessionSlot* p_slot) {
  if (p_slot == nullptr)
    return nullptr;

  return server.Map != nullptr
             ? server.Map->Players().Find(p_slot->PlayerReference)
             : nullptr;
}

void LobbyCommands::ClientJoined(Server& server, ServerConnection& conn) {
  std::lock_guard lock{server.mutex_lobby};

  if (server.opt_map_pool.has_value()) {
    auto str_pool = std::string{};
    for (std::size_t sz_i = 0; sz_i < server.opt_map_pool->size(); sz_i++) {
      if (sz_i > 0)
        str_pool += ", ";
      str_pool += (*server.opt_map_pool)[sz_i];
    }
    server.SendOrderTo(conn, "SyncMapPool", str_pool);
  }

  auto* p_client = server.GetClient(conn);

  if (p_client != nullptr && !p_client->Slot.empty()) {
    const auto* p_slot = server.LobbyInfo.FindSlot(p_client->Slot);
    if (p_slot != nullptr && !p_slot->LockColor)
      p_client->Color =
          SanitizePlayerColor(server, p_client->Color, p_client->Index);
  }
}

void LobbyCommands::ServerEmpty(Server& server) {
  std::lock_guard lock{server.mutex_lobby};

  server.vec_temp_bans.clear();

  server.LobbyInfo.global_settings.AllowSpectators = true;

  if (server.Map != nullptr) {
    auto vec_slots_new =
        std::vector<std::pair<std::string, net::SessionSlot>>{};
    for (const auto& [str_key, pr] : server.Map->Players().Players)
      if (auto opt_slot = MakeSlotFromPlayerReference(pr))
        vec_slots_new.emplace_back(pr.Name, std::move(*opt_slot));
    server.LobbyInfo.vec_slots = std::move(vec_slots_new);
  }
}

std::vector<int> LobbyCommands::AvailableSpawnPoints(
    int int4_spawn_points, const net::Session& lobby_info) {
  auto vec_out = std::vector<int>{};
  for (int int4_i = 1; int4_i <= int4_spawn_points; int4_i++)
    if (std::find(lobby_info.vec_disabled_spawn_points.begin(),
                  lobby_info.vec_disabled_spawn_points.end(),
                  int4_i) == lobby_info.vec_disabled_spawn_points.end())
      vec_out.push_back(int4_i);
  return vec_out;
}

bool LobbyCommands::InsufficientEnabledSpawnPoints(
    const map::MapPreview& map, const net::Session& lobby_info) {
  const auto int4_spawn_points = static_cast<int>(map.SpawnPoints().size());
  if (int4_spawn_points == 0)
    return false;

  int int4_non_observers = 0;
  for (const auto& c : lobby_info.vec_clients)
    if (!c.IsObserver())
      int4_non_observers++;

  return static_cast<int>(AvailableSpawnPoints(int4_spawn_points, lobby_info)
                              .size()) < int4_non_observers;
}

}  // namespace ora::server
