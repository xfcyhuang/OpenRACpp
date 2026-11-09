#include "game/network/lobby_session.hpp"

namespace ora::game::network {

namespace {

MiniYamlNode make_node(std::string_view str_key, std::string str_value = "") {
  MiniYamlNode node;
  node.str_key = std::string{str_key};
  node.str_value = std::move(str_value);
  return node;
}

std::string_view trim(std::string_view str_in) {
  while (!str_in.empty() && (str_in.front() == ' ' || str_in.front() == '\t'))
    str_in.remove_prefix(1);
  while (!str_in.empty() && (str_in.back() == ' ' || str_in.back() == '\t'))
    str_in.remove_suffix(1);
  return str_in;
}

std::vector<std::string_view> split_spaces(std::string_view str_in) {
  std::vector<std::string_view> vec_out;
  std::size_t sz_begin{0};
  while (sz_begin < str_in.size()) {
    while (sz_begin < str_in.size() && str_in[sz_begin] == ' ') ++sz_begin;
    if (sz_begin >= str_in.size()) break;
    const std::size_t sz_word{str_in.find(' ', sz_begin)};
    vec_out.push_back(str_in.substr(
        sz_begin, sz_word == std::string_view::npos ? std::string_view::npos : sz_word - sz_begin));
    sz_begin = sz_word == std::string_view::npos ? str_in.size() : sz_word;
  }
  return vec_out;
}

bool parse_bool_yn(std::string_view str_value, bool& b_out) {
  std::string str_core{trim(str_value)};
  for (char& ch : str_core)
    if (ch >= 'A' && ch <= 'Z') ch = static_cast<char>(ch - 'A' + 'a');
  if (str_core == "true") {
    b_out = true;
    return true;
  }
  if (str_core == "false") {
    b_out = false;
    return true;
  }
  return false;
}

std::optional<std::int32_t> parse_int(std::string_view str_value) {
  std::int32_t int4_out{};
  const auto res{std::from_chars(str_value.data(), str_value.data() + str_value.size(), int4_out)};
  if (res.ec != std::errc{} || res.ptr != str_value.data() + str_value.size()) return std::nullopt;
  return int4_out;
}

std::string format_bool(bool b_value) { return b_value ? "True" : "False"; }

LobbySlot make_slot_from_player_ref(const LobbyPlayerRef& ref) {
  LobbySlot slot;
  slot.str_player_reference = ref.str_name;
  slot.b_closed = false;
  slot.b_allow_bots = ref.b_allow_bots;
  slot.b_lock_faction = ref.b_lock_faction;
  slot.b_lock_color = ref.b_lock_color;
  slot.b_lock_team = ref.b_lock_team;
  slot.b_lock_handicap = ref.b_lock_handicap;
  slot.b_lock_spawn = ref.b_lock_spawn;
  slot.b_required = ref.b_required;
  return slot;
}

std::vector<std::pair<std::string, LobbySlot>> make_slots_from_players(
    const std::vector<LobbyPlayerRef>& vec_players) {
  std::vector<std::pair<std::string, LobbySlot>> vec_out;
  for (const LobbyPlayerRef& ref : vec_players) {
    if (!ref.b_playable) continue;
    vec_out.emplace_back(ref.str_name, make_slot_from_player_ref(ref));
  }
  return vec_out;
}

void sync_client_to_player_reference(LobbyClient& client, const LobbyPlayerRef& ref) {
  if (ref.b_lock_faction) client.str_faction = ref.str_faction;
  if (ref.b_lock_spawn) client.int4_spawn_point = 0;
  if (ref.b_lock_team) client.int4_team = ref.int4_team;
  if (ref.b_lock_handicap) client.int4_handicap = 0;
  client.str_color = ref.b_lock_color ? client.str_color : client.str_preferred_color;
}

const LobbyPlayerRef* player_ref_by_name(const LobbyMapInfo& map, std::string_view str_name) {
  for (const LobbyPlayerRef& ref : map.vec_players)
    if (ref.str_name == str_name) return &ref;
  return nullptr;
}

}

std::string_view lobby_client_state_name(LobbyClientState state) {
  switch (state) {
    case LobbyClientState::kNotReady: return "NotReady";
    case LobbyClientState::kInvalid: return "Invalid";
    case LobbyClientState::kReady: return "Ready";
    case LobbyClientState::kDisconnected: return "Disconnected";
  }
  return "Invalid";
}

const LobbyClient* LobbySession::client_with_index(std::int32_t int4_index) const {
  for (const LobbyClient& client : vec_clients)
    if (client.int4_index == int4_index) return &client;
  return nullptr;
}

LobbyClient* LobbySession::client_with_index(std::int32_t int4_index) {
  for (LobbyClient& client : vec_clients)
    if (client.int4_index == int4_index) return &client;
  return nullptr;
}

const LobbyClient* LobbySession::client_in_slot(std::string_view str_slot) const {
  for (const LobbyClient& client : vec_clients)
    if (client.b_has_slot && client.str_slot == str_slot) return &client;
  return nullptr;
}

LobbyClient* LobbySession::client_in_slot(std::string_view str_slot) {
  for (LobbyClient& client : vec_clients)
    if (client.b_has_slot && client.str_slot == str_slot) return &client;
  return nullptr;
}

const LobbySlot* LobbySession::slot(std::string_view str_player_reference) const {
  for (const auto& [str_key, slot] : vec_slots)
    if (str_key == str_player_reference) return &slot;
  return nullptr;
}

const std::string* LobbySession::first_empty_slot() const {
  for (const auto& [str_key, slot] : vec_slots) {
    if (slot.b_closed) continue;
    if (client_in_slot(str_key) != nullptr) continue;
    return &str_key;
  }
  return nullptr;
}

std::string LobbySession::serialize() const {
  std::vector<MiniYamlNode> vec_nodes;

  {
    std::string str_value;
    for (std::size_t sz_i{0}; sz_i < vec_disabled_spawn_points.size(); ++sz_i) {
      if (sz_i != 0) str_value += ", ";
      str_value += std::to_string(vec_disabled_spawn_points[sz_i]);
    }
    vec_nodes.push_back(make_node("DisabledSpawnPoints", std::move(str_value)));
  }

  for (const LobbyClient& client : vec_clients) {
    MiniYamlNode node_client{make_node(std::format("Client@{}", client.int4_index))};
    auto add = [&node_client](std::string_view str_key, std::string_view str_value) {
      node_client.vec_children.push_back(make_node(str_key, std::string{str_value}));
    };
    add("Index", std::to_string(client.int4_index));
    add("PreferredColor", client.str_preferred_color);
    add("Color", client.str_color);
    add("Faction", client.str_faction);
    add("SpawnPoint", std::to_string(client.int4_spawn_point));
    add("Name", client.str_name);
    add("AnonymizedIPAddress", client.b_has_anonymized_ip ? client.str_anonymized_ip_address : "");
    add("Location", client.b_has_location ? client.str_location : "");
    add("ConnectionQuality", client.str_connection_quality);
    add("State", std::string{lobby_client_state_name(client.state)});
    add("Team", std::to_string(client.int4_team));
    add("Handicap", std::to_string(client.int4_handicap));
    add("Slot", client.b_has_slot ? client.str_slot : "");
    add("Bot", client.b_is_bot ? client.str_bot : "");
    add("BotControllerClientIndex", std::to_string(client.int4_bot_controller_client_index));
    add("IsAdmin", format_bool(client.b_is_admin));
    add("Fingerprint", client.b_has_fingerprint ? client.str_fingerprint : "");
    vec_nodes.push_back(std::move(node_client));
  }

  for (const auto& [str_key, slot] : vec_slots) {
    MiniYamlNode node_slot{make_node(std::format("Slot@{}", str_key))};
    auto add = [&node_slot](std::string_view str_key_field, std::string_view str_value) {
      node_slot.vec_children.push_back(make_node(str_key_field, std::string{str_value}));
    };
    add("PlayerReference", slot.str_player_reference);
    add("Closed", format_bool(slot.b_closed));
    add("AllowBots", format_bool(slot.b_allow_bots));
    add("LockFaction", format_bool(slot.b_lock_faction));
    add("LockColor", format_bool(slot.b_lock_color));
    add("LockTeam", format_bool(slot.b_lock_team));
    add("LockHandicap", format_bool(slot.b_lock_handicap));
    add("LockSpawn", format_bool(slot.b_lock_spawn));
    add("Required", format_bool(slot.b_required));
    vec_nodes.push_back(std::move(node_slot));
  }

  {
    MiniYamlNode node_global{make_node("GlobalSettings")};
    auto add = [&node_global](std::string_view str_key, std::string_view str_value) {
      node_global.vec_children.push_back(make_node(str_key, std::string{str_value}));
    };
    add("ServerName", global.str_server_name);
    add("Map", global.str_map);
    add("MapStatus", global.str_map_status);
    add("RandomSeed", std::to_string(global.int4_random_seed));
    add("AllowSpectators", format_bool(global.b_allow_spectators));
    add("GameUid", global.str_game_uid);
    add("EnableSingleplayer", format_bool(global.b_enable_singleplayer));
    add("EnableMapGeneration", format_bool(global.b_enable_map_generation));
    add("EnableGameSaves", format_bool(global.b_enable_game_saves));
    add("EnableSyncReports", format_bool(global.b_enable_sync_reports));
    add("Dedicated", format_bool(global.b_dedicated));
    add("NetFrameInterval", std::to_string(global.int4_net_frame_interval));
    add("GameTimestep", std::to_string(global.int4_game_timestep));
    MiniYamlNode node_options{make_node("Options")};
    for (const auto& [str_key, option] : global.vec_options) {
      MiniYamlNode node_option{make_node(str_key)};
      node_option.vec_children.push_back(make_node("Value", option.str_value));
      node_option.vec_children.push_back(
          make_node("PreferredValue", option.str_preferred_value));
      node_option.vec_children.push_back(make_node("IsLocked", format_bool(option.b_is_locked)));
      node_options.vec_children.push_back(std::move(node_option));
    }
    node_global.vec_children.push_back(std::move(node_options));
    vec_nodes.push_back(std::move(node_global));
  }

  return miniyaml_write_to_string(vec_nodes);
}

std::expected<std::vector<LobbyPlayerRef>, std::string> lobby_player_refs_from_map_yaml(
    std::string_view str_yaml, std::string_view str_source_name) {
  auto res_yaml{miniyaml_from_string(str_yaml, str_source_name)};
  if (!res_yaml.has_value()) return std::unexpected(res_yaml.error());
  const MiniYamlNode* node_players{nullptr};
  for (const MiniYamlNode& node : *res_yaml)
    if (node.str_key == "Players") node_players = &node;
  if (node_players == nullptr)
    return std::unexpected(std::format("{}: map yaml lacks Players", str_source_name));

  std::vector<LobbyPlayerRef> vec_out;
  for (const MiniYamlNode& node_ref : node_players->vec_children) {
    const std::size_t sz_at{node_ref.str_key.find('@')};
    const std::string str_type{sz_at == std::string::npos
                                   ? node_ref.str_key
                                   : node_ref.str_key.substr(0, sz_at)};
    if (str_type != "PlayerReference") continue;
    LobbyPlayerRef ref;
    for (const MiniYamlNode& field : node_ref.vec_children) {
      const std::string_view str_key{field.str_key};
      const std::string_view str_value{trim(field.str_value)};
      if (str_key == "Name") ref.str_name = str_value;
      else if (str_key == "Playable") {
        if (!parse_bool_yn(str_value, ref.b_playable))
          return std::unexpected(std::format(
              "FieldLoader: Cannot parse `{}` into field `Playable` of type `Boolean`",
              field.str_value));
      } else if (str_key == "AllowBots") {
        if (!parse_bool_yn(str_value, ref.b_allow_bots))
          return std::unexpected(std::format(
              "FieldLoader: Cannot parse `{}` into field `AllowBots` of type `Boolean`",
              field.str_value));
      } else if (str_key == "LockFaction") {
        parse_bool_yn(str_value, ref.b_lock_faction);
      } else if (str_key == "LockColor") {
        parse_bool_yn(str_value, ref.b_lock_color);
      } else if (str_key == "LockTeam") {
        parse_bool_yn(str_value, ref.b_lock_team);
      } else if (str_key == "LockHandicap") {
        parse_bool_yn(str_value, ref.b_lock_handicap);
      } else if (str_key == "LockSpawn") {
        parse_bool_yn(str_value, ref.b_lock_spawn);
      } else if (str_key == "Required") {
        parse_bool_yn(str_value, ref.b_required);
      } else if (str_key == "Faction") {
        ref.str_faction = str_value;
      } else if (str_key == "Team") {
        const auto opt{parse_int(str_value)};
        if (opt.has_value()) ref.int4_team = *opt;
      } else if (str_key == "Bot") {
        ref.str_bot = str_value;
      }
    }
    vec_out.push_back(std::move(ref));
  }
  return vec_out;
}

std::vector<std::string> ra_selectable_factions() {
  return {"england", "france", "germany", "russia", "ukraine", "Random", "RandomAllies",
          "RandomSoviet"};
}

void LobbyServerModel::server_start(std::vector<LobbyMapInfo> vec_maps_in,
                                    std::size_t sz_initial_index) {
  vec_maps = std::move(vec_maps_in);
  map = vec_maps[sz_initial_index];
  lobby_info = LobbySession{};
  lobby_info.global.str_server_name = "";
  lobby_info.global.int4_random_seed = 0;
  lobby_info.global.str_game_uid = "00000000-0000-0000-0000-000000000000";
  lobby_info.global.str_map = map.str_uid;
  lobby_info.global.str_map_status = "Playable";
  lobby_info.global.b_allow_spectators = true;
  lobby_info.global.b_enable_singleplayer = true;
  lobby_info.global.b_enable_map_generation = true;
  lobby_info.global.b_enable_sync_reports = false;
  lobby_info.global.b_dedicated = false;
  lobby_info.global.int4_net_frame_interval = 3;
  lobby_info.global.int4_game_timestep = 0;
  lobby_info.vec_slots = make_slots_from_players(map.vec_players);
}

void LobbyServerModel::join_local_client(std::string_view str_name, std::string_view str_color_hex) {
  LobbyClient client;
  client.int4_index = static_cast<std::int32_t>(lobby_info.vec_clients.size());
  client.str_name = std::string{str_name};
  client.str_preferred_color = std::string{str_color_hex};
  client.str_color = std::string{str_color_hex};
  client.str_faction = "Random";
  client.int4_spawn_point = 0;
  client.int4_team = 0;
  client.int4_handicap = 0;
  client.state = LobbyClientState::kInvalid;
  client.str_connection_quality = "Good";
  client.b_is_admin = true;
  for (const LobbyClient& other : lobby_info.vec_clients)
    if (other.b_is_admin) client.b_is_admin = false;
  const std::string* str_slot{lobby_info.first_empty_slot()};
  if (str_slot != nullptr) {
    client.b_has_slot = true;
    client.str_slot = *str_slot;
    if (const LobbyPlayerRef* ref{player_ref_by_name(map, *str_slot)})
      sync_client_to_player_reference(client, *ref);
  } else {
    client.str_color = "FFFFFF";
  }
  lobby_info.vec_clients.push_back(std::move(client));
}

bool LobbyServerModel::select_map(std::string_view str_map_name) {
  const LobbyMapInfo* ptr_new{nullptr};
  for (const LobbyMapInfo& info : vec_maps)
    if (info.str_name == str_map_name) ptr_new = &info;
  if (ptr_new == nullptr) return false;

  std::vector<std::string> vec_old_slots;
  for (const auto& [str_key, slot] : lobby_info.vec_slots) vec_old_slots.push_back(str_key);

  lobby_info.global.str_map = ptr_new->str_uid;
  map = *ptr_new;
  lobby_info.vec_slots = make_slots_from_players(ptr_new->vec_players);

  for (LobbyClient& client : lobby_info.vec_clients) {
    if (std::find(ptr_new->vec_selectable_factions.begin(), ptr_new->vec_selectable_factions.end(),
                  client.str_faction) ==
        ptr_new->vec_selectable_factions.end())
      client.str_faction = "Random";
    client.state = LobbyClientState::kInvalid;
  }

  std::size_t sz_next{0};
  for (const std::string& str_old : vec_old_slots) {
    LobbyClient* client{lobby_info.client_in_slot(str_old)};
    if (client == nullptr) continue;
    client->int4_spawn_point = 0;
    if (sz_next < lobby_info.vec_slots.size()) {
      client->str_slot = lobby_info.vec_slots[sz_next].first;
      client->b_has_slot = true;
      ++sz_next;
      if (const LobbyPlayerRef* ref{player_ref_by_name(*ptr_new, client->str_slot)})
        sync_client_to_player_reference(*client, *ref);
    } else {
      client->b_has_slot = false;
      client->str_slot.clear();
      client->str_color = "FFFFFF";
    }
  }

  lobby_info.vec_disabled_spawn_points.clear();
  return true;
}

bool LobbyServerModel::interpret_command(std::int32_t int4_client_index, std::string_view str_cmd) {
  LobbyClient* client{lobby_info.client_with_index(int4_client_index)};
  if (client == nullptr) return false;

  const std::vector<std::string_view> vec_parts{split_spaces(str_cmd)};
  if (vec_parts.empty()) return false;
  const std::string_view str_name{vec_parts[0]};

  if (str_name == "state") {
    if (vec_parts.size() != 2) return false;
    const std::string_view str_state{vec_parts[1]};
    if (str_state == "NotReady") client->state = LobbyClientState::kNotReady;
    else if (str_state == "Invalid") client->state = LobbyClientState::kInvalid;
    else if (str_state == "Ready") client->state = LobbyClientState::kReady;
    else if (str_state == "Disconnected") client->state = LobbyClientState::kDisconnected;
    else return false;
    return true;
  }

  if (client->is_ready() && str_name != "startgame") return false;

  if (str_name == "startgame") {
    if (!client->b_is_admin) return true;
    for (const auto& [str_key, slot] : lobby_info.vec_slots)
      if (slot.b_required && lobby_info.client_in_slot(str_key) == nullptr) return true;
    bool b_any_player{};
    for (const LobbyClient& c : lobby_info.vec_clients)
      if (c.b_has_slot) b_any_player = true;
    if (!b_any_player) return true;
    std::size_t sz_non_bot{};
    for (const LobbyClient& c : lobby_info.vec_clients)
      if (!c.b_is_bot) ++sz_non_bot;
    if (!lobby_info.global.b_enable_singleplayer && sz_non_bot < 2) return true;
    lobby_info.global.b_enable_game_saves = sz_non_bot == 1;
    b_start_requested = true;
    return true;
  }

  if (str_name == "map") {
    if (vec_parts.size() != 2 || !client->b_is_admin) return true;
    return select_map(vec_parts[1]);
  }

  if (vec_parts.size() != 3) return false;
  const auto opt_target{parse_int(vec_parts[1])};
  if (!opt_target.has_value()) return false;
  LobbyClient* target{lobby_info.client_with_index(*opt_target)};
  if (target == nullptr) return false;

  if (str_name == "faction") {
    if (!target->b_has_slot) return true;
    if (target->int4_index != client->int4_index && !client->b_is_admin) return true;
    const LobbySlot* slot{lobby_info.slot(target->str_slot)};
    if (slot == nullptr || slot->b_lock_faction) return true;
    if (std::find(map.vec_selectable_factions.begin(), map.vec_selectable_factions.end(), vec_parts[2]) ==
        map.vec_selectable_factions.end())
      return true;
    target->str_faction = std::string{vec_parts[2]};
    return true;
  }

  if (str_name == "team") {
    if (!target->b_has_slot) return true;
    if (target->int4_index != client->int4_index && !client->b_is_admin) return true;
    const LobbySlot* slot{lobby_info.slot(target->str_slot)};
    if (slot == nullptr || slot->b_lock_team) return true;
    const auto opt_team{parse_int(vec_parts[2])};
    if (!opt_team.has_value()) return false;
    target->int4_team = *opt_team;
    return true;
  }

  return false;
}

}
