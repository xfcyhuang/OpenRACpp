#include "net/unit_orders.hpp"

#include "map/map_cache.hpp"
#include "net/fluent_message.hpp"
#include "net/handshake.hpp"
#include "net/order_manager.hpp"
#include "net/sanitize.hpp"
#include "net/session.hpp"
#include "net/text_notifications.hpp"
#include "sim/player.hpp"
#include "sim/trait_interfaces.hpp"
#include "sim/world.hpp"
#include "yaml/mini_yaml.hpp"

namespace ora::net {

namespace {

constexpr std::string_view kJoined = "notification-joined";
constexpr std::string_view kLeft = "notification-lobby-disconnected";
constexpr std::string_view kGameStartedKey = "notification-game-has-started";
constexpr std::string_view kGamePausedKey = "notification-game-paused";
constexpr std::string_view kGameUnpausedKey = "notification-game-unpaused";

std::optional<int> opt_kick_vote_target;

std::function<void(OrderManager&, const std::string&)>& StartGameHandler() {
  static std::function<void(OrderManager&, const std::string&)> fn_start_game;
  return fn_start_game;
}

std::function<void()>& LobbyInfoChangedHandler() {
  static std::function<void()> fn_changed;
  return fn_changed;
}

std::function<std::int64_t()>& RuntimeMsFn() {
  static std::function<std::int64_t()> fn_runtime = [] {
    return std::int64_t{0};
  };
  return fn_runtime;
}

ClientJoinFace& JoinFace() {
  static ClientJoinFace face;
  return face;
}

std::function<ora::map::MapPreview*(const std::string&)>& MapCacheAtFn() {
  static std::function<ora::map::MapPreview*(const std::string&)> fn_at;
  return fn_at;
}

void SyncLobbyInfo() {
  if (LobbyInfoChangedHandler())
    LobbyInfoChangedHandler()();
}

std::string_view NodeText(const yaml::MiniYamlNode& node) {
  if (node.Value.Value != nullptr)
    return std::string_view{*node.Value.Value};
  return {};
}

std::string_view NodeKeyText(const yaml::MiniYamlNode& node) {
  return std::string_view{*node.Key};
}

sim::Player* FindPlayerByClient(sim::World& world,
                                const SessionClient& client) {
  for (auto* p_player : world.Players())
    if (p_player->ClientIndex() == client.Index && p_player->Playable())
      return p_player;
  return nullptr;
}

bool OrderNotFromServerOrWorldIsReplay(int int4_client_id,
                                       sim::World* p_world) {
  return int4_client_id != 0 ||
         (p_world != nullptr && p_world->IsReplay());
}

void ResolveOrder(const Order& order, sim::World& world,
                  OrderManager& order_manager, int client_id) {
  if (order.p_subject == nullptr || order.p_subject->IsDead())
    return;

  for (auto* vo :
       world.WorldActor() ? world.WorldActor()->TraitsImplementing<sim::IValidateOrder>()
                          : std::vector<sim::IValidateOrder*>{})
    if (!vo->OrderValidation(order_manager, world, client_id, order))
      return;

  order.p_subject->ResolveOrder(order);
}

std::optional<int> ParseInt32Text(std::string_view sv) {
  int out = 0;
  auto [ptr, ec] = std::from_chars(sv.data(), sv.data() + sv.size(), out, 10);
  if (ec != std::errc{} || ptr != sv.data() + sv.size())
    return std::nullopt;
  return out;
}

bool LoadMapGenerationArgs(const yaml::MiniYaml& yaml_v,
                           map::MapGenerationArgs& out) {
  for (const auto& node : yaml_v.Nodes) {
    const auto key = NodeKeyText(node);
    const auto text = NodeText(node);
    if (key == "Uid") {
      out.str_uid = std::string{text};
    } else if (key == "Generator") {
      out.str_generator = std::string{text};
    } else if (key == "Tileset") {
      out.str_tileset = std::string{text};
    } else if (key == "Size") {

      const auto comma = text.find(',');
      if (comma == std::string_view::npos)
        return false;
      const auto width = ParseInt32Text(text.substr(0, comma));
      const auto height = ParseInt32Text(text.substr(comma + 1));
      if (!width.has_value() || !height.has_value())
        return false;
      out.size_ = Size{*width, *height};
    } else if (key == "Title") {
      out.str_title = std::string{text};
    } else if (key == "Author") {
      out.str_author = std::string{text};
    } else if (key == "PreviewVisibility") {
      if (text == "None")
        out.kind_preview_visibility =
            map::MapGenerationArgs::PreviewVisibilityFlags::None;
      else if (text == "Lobby")
        out.kind_preview_visibility =
            map::MapGenerationArgs::PreviewVisibilityFlags::Lobby;
      else if (text == "MapChooser")
        out.kind_preview_visibility =
            map::MapGenerationArgs::PreviewVisibilityFlags::MapChooser;
      else if (text == "All")
        out.kind_preview_visibility =
            map::MapGenerationArgs::PreviewVisibilityFlags::All;
      else
        return false;
    } else if (key == "Options") {
      for (const auto& option_node : node.Value.Nodes)
        out.vec_options.emplace_back(std::string{NodeKeyText(option_node)},
                                     std::string{NodeText(option_node)});
    }
  }
  return true;
}

std::set<std::string> ParseStringSet(std::string_view str_value) {
  auto set_out = std::set<std::string>{};
  std::size_t pos_start = 0;
  while (pos_start <= str_value.size()) {
    const auto comma = str_value.find(',', pos_start);
    const auto part = str_value.substr(
        pos_start, comma == std::string_view::npos ? std::string_view::npos
                                                   : comma - pos_start);

    const auto first = part.find_first_not_of(" \t\r\n");
    if (first != std::string_view::npos) {
      const auto last = part.find_last_not_of(" \t\r\n");
      set_out.emplace(part.substr(first, last - first + 1));
    }
    if (comma == std::string_view::npos)
      break;
    pos_start = comma + 1;
  }
  return set_out;
}

ConnectionQuality QualityFromText(std::string_view sv, bool& b_ok) {
  b_ok = true;
  if (sv == "Good")
    return ConnectionQuality::Good;
  if (sv == "Moderate")
    return ConnectionQuality::Moderate;
  if (sv == "Poor")
    return ConnectionQuality::Poor;
  b_ok = false;
  return ConnectionQuality::Good;
}

}

std::optional<int> KickVoteTarget() { return opt_kick_vote_target; }

void SetKickVoteTarget(std::optional<int> int4_target) {
  opt_kick_vote_target = int4_target;
}

void ClearUnitOrdersStatics() { opt_kick_vote_target.reset(); }

void SetStartGameHandler(
    std::function<void(OrderManager&, const std::string&)> fn_start_game) {
  StartGameHandler() = std::move(fn_start_game);
}

void ClearStartGameHandler() { StartGameHandler() = nullptr; }

void SetLobbyInfoChangedHandler(std::function<void()> fn_changed) {
  LobbyInfoChangedHandler() = std::move(fn_changed);
}

void ClearLobbyInfoChangedHandler() { LobbyInfoChangedHandler() = nullptr; }

void SetRuntimeMsFn(std::function<std::int64_t()> fn_runtime_ms) {
  RuntimeMsFn() = std::move(fn_runtime_ms);
}

void SetClientJoinFace(ClientJoinFace face_join) {
  JoinFace() = std::move(face_join);
}

const ClientJoinFace& ClientJoinFaceOf() { return JoinFace(); }

void SetMapCacheAtFn(
    std::function<ora::map::MapPreview*(const std::string& str_uid)> fn_at) {
  MapCacheAtFn() = std::move(fn_at);
}

void ProcessOrder(OrderManager& order_manager, sim::World* world,
                  int client_id, const Order& order) {
  auto& tnm = TextNotificationsManager::Instance();
  const std::string& name = order.str_order_string;

  if (name == "Message") {
    tnm.AddSystemLine(order.str_target_string.value_or(""));
    return;
  }

  if (name == "FluentMessage") {
    const auto& str_target = order.str_target_string.value_or("");
    if (str_target.empty())
      return;

    const auto vec_nodes =
        yaml::MiniYaml::FromString(str_target, order.str_order_string);
    for (const auto& node : vec_nodes) {
      const auto message = FluentMessage::FromYaml(node.Value);
      if (message.Key() == kJoined)
        tnm.AddPlayerJoinedLine(message.Key(), message.Arguments());
      else if (message.Key() == kLeft)
        tnm.AddPlayerLeftLine(message.Key(), message.Arguments());
      else
        tnm.AddSystemLine(message.Key(), message.Arguments());
    }
    return;
  }

  if (name == "DisableChatEntry") {
    if (OrderNotFromServerOrWorldIsReplay(client_id, world))
      return;

    if (order.uint4_extra_data == std::numeric_limits<std::uint32_t>::max())
      tnm.SetChatDisabledUntil(
          std::numeric_limits<std::int64_t>::max());
    else
      tnm.SetChatDisabledUntil(RuntimeMsFn()() +
                               static_cast<std::int64_t>(
                                   order.uint4_extra_data));
    return;
  }

  if (name == "StartKickVote") {
    if (OrderNotFromServerOrWorldIsReplay(client_id, world))
      return;

    opt_kick_vote_target =
        static_cast<int>(order.uint4_extra_data);
    return;
  }

  if (name == "EndKickVote") {
    if (OrderNotFromServerOrWorldIsReplay(client_id, world))
      return;

    if (opt_kick_vote_target.has_value() &&
        *opt_kick_vote_target == static_cast<int>(order.uint4_extra_data))
      opt_kick_vote_target.reset();
    return;
  }

  if (name == "Chat") {
    auto* p_client = order_manager.LobbyInfo().ClientWithIndex(client_id);
    if (p_client == nullptr)
      return;

    auto str_message = order.str_target_string.value_or("");
    if (str_message.size() > static_cast<std::size_t>(kChatMessageMaxLength))
      str_message = str_message.substr(0, kChatMessageMaxLength);

    if (order.uint4_extra_data == 0) {
      auto* p_player =
          world != nullptr ? FindPlayerByClient(*world, *p_client) : nullptr;
      auto str_suffix =
          (p_player != nullptr &&
           p_player->PlayerWinState() == sim::WinState::Lost)
              ? std::string{" (Dead)"}
              : std::string{};
      if (p_client->IsObserver())
        str_suffix = " (Spectator)";

      auto* p_local = order_manager.LocalClient();
      if (p_local != nullptr && p_client != p_local && p_client->Team > 0 &&
          p_client->Team == p_local->Team)
        str_suffix += " (Ally)";

      tnm.AddChatLine(client_id, p_client->Name + str_suffix, str_message,
                      p_client->Color);
      return;
    }

    if (world == nullptr) {
      const auto str_prefix =
          order.uint4_extra_data ==
                  std::numeric_limits<std::uint32_t>::max()
              ? "[Spectators] "
              : "[Team] ";
      auto* p_local = order_manager.LocalClient();
      if (p_local != nullptr && p_client->Team == p_local->Team)
        tnm.AddChatLine(client_id, str_prefix + p_client->Name,
                        str_message, p_client->Color);
      return;
    }

    auto* p_player = FindPlayerByClient(*world, *p_client);
    auto* p_local = order_manager.LocalClient();
    const bool b_local_observer =
        world->IsReplay() ||
        (p_local != nullptr && p_local->IsObserver()) ||
        (world->LocalPlayer() != nullptr &&
         world->LocalPlayer()->PlayerWinState() != sim::WinState::Undefined);

    if (order.uint4_extra_data ==
            std::numeric_limits<std::uint32_t>::max() &&
        b_local_observer) {

      if (p_client->IsObserver() ||
          (p_player != nullptr &&
           p_player->PlayerWinState() != sim::WinState::Undefined))
        tnm.AddChatLine(client_id, "[Spectators] " + p_client->Name,
                        str_message, p_client->Color);
      return;
    }

    const bool b_valid =
        static_cast<std::int64_t>(p_client->Team) ==
            static_cast<std::int64_t>(order.uint4_extra_data) &&
        p_player != nullptr &&
        p_player->PlayerWinState() == sim::WinState::Undefined;
    const bool b_same_team =
        p_local != nullptr &&
        static_cast<std::int64_t>(order.uint4_extra_data) ==
            static_cast<std::int64_t>(p_local->Team) &&
        world->LocalPlayer() != nullptr &&
        world->LocalPlayer()->PlayerWinState() == sim::WinState::Undefined;

    if (b_valid && (b_same_team || world->IsReplay())) {
      const auto str_team_suffix =
          world->IsReplay()
              ? std::format(" {}", order.uint4_extra_data)
              : std::string{};
      tnm.AddChatLine(client_id,
                      "[Team" + str_team_suffix + "] " + p_client->Name,
                      str_message, p_client->Color);
    }
    return;
  }

  if (name == "StartGame") {
    if (order.str_target_string.has_value() &&
        !order.str_target_string->empty()) {
      const auto vec_data = yaml::MiniYaml::FromString(
          *order.str_target_string, order.str_order_string);
      for (const auto& node : vec_data) {
        const auto text = NodeText(node);
        if (*node.Key == "SaveLastOrdersFrame") {
          if (const auto parsed = ParseInt32Text(text))
            order_manager.SetGameSaveLastFrame(*parsed);
        } else if (*node.Key == "SaveSyncFrame") {
          if (const auto parsed = ParseInt32Text(text))
            order_manager.SetGameSaveLastSyncFrame(*parsed);
        }
      }
    } else {
      tnm.AddSystemLine(kGameStartedKey);
    }

    if (StartGameHandler())
      StartGameHandler()(order_manager,
                         order_manager.LobbyInfo().global_settings.Map);
    return;
  }

  if (name == "SaveTraitData") {
    const auto vec_data = yaml::MiniYaml::FromString(
        order.str_target_string.value_or(""), order.str_order_string);
    if (vec_data.empty())
      return;
    const auto trait_index = ParseInt32Text(NodeKeyText(vec_data.front()));
    if (world != nullptr && trait_index.has_value())
      world->AddGameSaveTraitData(*trait_index, vec_data.front().Value);
    return;
  }

  if (name == "GameSaved") {
    auto* p_world = order_manager.World();
    if (p_world == nullptr || p_world->WorldActor() == nullptr)
      return;
    for (auto* nsr : p_world->WorldActor()
                         ->TraitsImplementing<sim::INotifyGameSaved>())
      nsr->GameSaved(*p_world, order.uint4_extra_data != 0);
    return;
  }

  if (name == "PauseGame") {
    auto* p_client = order_manager.LobbyInfo().ClientWithIndex(client_id);
    if (p_client == nullptr)
      return;

    const bool b_pause =
        order.str_target_string.value_or("") == "Pause";

    auto* p_om_world = order_manager.World();
    if (p_om_world == nullptr)
      return;
    if (p_om_world->IsGameOver() && !b_pause)
      return;

    if (p_om_world->Paused() != b_pause && world != nullptr &&
        world->LobbyInfo().NonBotClients().size() > 1)
      tnm.AddSystemLine(b_pause ? kGamePausedKey : kGameUnpausedKey);

    p_om_world->SetPaused(b_pause);
    p_om_world->SetPredictedPaused(b_pause);
    return;
  }

  if (name == "HandshakeRequest") {
    const auto& face = JoinFace();
    if (face.str_mod_id.empty())
      return;

    const auto request = HandshakeRequest::Deserialize(
        order.str_target_string.value_or(""), order.str_order_string);
    if (!request.has_value())
      return;

    auto info = SessionClient{};
    info.Name = SanitizedPlayerName(face.str_player_name, "Commander");
    info.PreferredColor = face.color_player;
    info.Color = face.color_player;
    info.Faction = "Random";
    info.SpawnPoint = 0;
    info.Team = 0;
    info.State = ClientState::Invalid;

    auto response = HandshakeResponse{};
    response.Client = info;
    response.Mod = face.str_mod_id;
    response.Version = face.str_mod_version;
    response.Password = face.str_password;
    response.Fingerprint = face.str_fingerprint;
    response.OrdersProtocol = 21;

    if (!request->AuthToken.empty() && !response.Fingerprint.empty() &&
        face.fn_sign)
      response.AuthSignature = face.fn_sign(request->AuthToken);

    auto order_response = Order("HandshakeResponse", nullptr, false);
    order_response.type = OrderType::Handshake;
    order_response.b_is_immediate = true;
    order_response.str_target_string = response.Serialize();
    order_manager.IssueOrder(order_response);
    return;
  }

  if (name == "ServerError") {
    order_manager.SetServerError(
        std::string{order.str_target_string.value_or("")});
    order_manager.SetAuthenticationFailed(false);
    return;
  }

  if (name == "AuthenticationError") {

    order_manager.SetServerError(
        std::string{order.str_target_string.value_or("")});
    order_manager.SetAuthenticationFailed(true);
    return;
  }

  if (name == "SyncInfo") {
    order_manager.LobbyInfo() = Session::Deserialize(
        order.str_target_string.value_or(""), order.str_order_string);
    SyncLobbyInfo();
    return;
  }

  if (name == "SyncLobbyClients") {
    auto vec_clients = std::vector<SessionClient>{};
    const auto vec_nodes = yaml::MiniYaml::FromString(
        order.str_target_string.value_or(""), order.str_order_string);
    for (const auto& node : vec_nodes) {
      const auto& key = *node.Key;
      const auto at = key.find('@');
      if (at != std::string::npos && key.substr(0, at) == "Client") {
        SessionClient client{};
        if (LoadSessionClient(node.Value, client))
          vec_clients.push_back(std::move(client));
      }
    }

    order_manager.LobbyInfo().vec_clients = std::move(vec_clients);
    SyncLobbyInfo();
    return;
  }

  if (name == "SyncLobbySlots") {
    auto vec_slots =
        std::vector<std::pair<std::string, SessionSlot>>{};
    const auto vec_nodes = yaml::MiniYaml::FromString(
        order.str_target_string.value_or(""), order.str_order_string);
    for (const auto& node : vec_nodes) {
      const auto& key = *node.Key;
      const auto at = key.find('@');
      if (at == std::string::npos || key.substr(0, at) != "Slot")
        continue;
      SessionSlot slot{};
      if (!LoadSessionSlot(node.Value, slot))
        continue;

      const auto str_reference = slot.PlayerReference;
      bool b_duplicate = false;
      for (const auto& [key_existing, slot_existing] : vec_slots)
        static_cast<void>(slot_existing),
            b_duplicate |= key_existing == str_reference;
      if (!b_duplicate)
        vec_slots.emplace_back(str_reference, std::move(slot));
    }

    order_manager.LobbyInfo().vec_slots = std::move(vec_slots);
    SyncLobbyInfo();
    return;
  }

  if (name == "SyncLobbyGlobalSettings") {
    const auto vec_nodes = yaml::MiniYaml::FromString(
        order.str_target_string.value_or(""), order.str_order_string);
    for (const auto& node : vec_nodes) {
      const auto& key = *node.Key;
      const auto at = key.find('@');
      if (at != std::string::npos &&
          key.substr(0, at) == "GlobalSettings") {
        SessionGlobalSettings global{};
        if (LoadSessionGlobalSettings(node.Value, global))
          order_manager.LobbyInfo().global_settings = std::move(global);
      }
    }

    SyncLobbyInfo();
    return;
  }

  if (name == "SyncConnectionQuality") {
    const auto vec_nodes = yaml::MiniYaml::FromString(
        order.str_target_string.value_or(""), order.str_order_string);
    for (const auto& node : vec_nodes) {
      const auto& key = *node.Key;
      const auto at = key.find('@');
      if (at == std::string::npos ||
          key.substr(0, at) != "ConnectionQuality")
        continue;
      const auto index = ParseInt32Text(key.substr(at + 1));
      if (!index.has_value())
        continue;
      auto* p_client = order_manager.LobbyInfo().ClientWithIndex(*index);
      if (p_client == nullptr)
        continue;
      bool b_parsed = false;
      const auto quality = QualityFromText(NodeText(node), b_parsed);
      if (b_parsed)
        p_client->ConnectionQualityKind = quality;
    }
    return;
  }

  if (name == "SyncMapPool") {
    order_manager.SetServerMapPool(
        ParseStringSet(order.str_target_string.value_or("")));
    return;
  }

  if (name == "GenerateMap") {
    const auto vec_nodes = yaml::MiniYaml::FromString(
        order.str_target_string.value_or(""), order.str_order_string);
    auto args = map::MapGenerationArgs{};
    if (!LoadMapGenerationArgs(
            yaml::MiniYaml{nullptr, vec_nodes}, args))
      return;

    if (!MapCacheAtFn())
      return;
    auto* p_preview = MapCacheAtFn()(args.str_uid);
    if (p_preview == nullptr)
      return;
    if (p_preview->Status() != map::MapStatus::Available &&
        p_preview->Class() != map::MapClassification::Generated) {
      p_preview->UpdateFromGenerationArgs(args);
      p_preview->Generate();
    }
    return;
  }

  if (world == nullptr)
    return;

  if (!order.vec_grouped_actors.has_value()) {
    ResolveOrder(order, *world, order_manager, client_id);
  } else {
    for (auto* subject : *order.vec_grouped_actors)
      ResolveOrder(Order::FromGroupedOrder(order, subject), *world,
                   order_manager, client_id);
  }
}

}
