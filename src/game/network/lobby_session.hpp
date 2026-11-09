#pragma once

import std;

#include "game/miniyaml.hpp"

namespace ora::game::network {

enum class LobbyClientState : std::int32_t {
  kNotReady = 0,
  kInvalid = 1,
  kReady = 2,
  kDisconnected = 1000,
};

[[nodiscard]] std::string_view lobby_client_state_name(LobbyClientState state);

struct LobbyClient {
  std::int32_t int4_index{};
  std::string str_preferred_color;
  std::string str_color;
  std::string str_faction;
  std::int32_t int4_spawn_point{};
  std::string str_name;
  bool b_has_anonymized_ip{};
  std::string str_anonymized_ip_address;
  bool b_has_location{};
  std::string str_location;
  std::string str_connection_quality{"Good"};
  LobbyClientState state{LobbyClientState::kInvalid};
  std::int32_t int4_team{};
  std::int32_t int4_handicap{};
  bool b_has_slot{};
  std::string str_slot;
  bool b_is_bot{};
  std::string str_bot;
  std::int32_t int4_bot_controller_client_index{};
  bool b_is_admin{};
  bool b_has_fingerprint{};
  std::string str_fingerprint;

  [[nodiscard]] bool is_ready() const { return state == LobbyClientState::kReady; }
  [[nodiscard]] bool is_observer() const { return !b_has_slot; }
};

struct LobbySlot {
  std::string str_player_reference;
  bool b_closed{};
  bool b_allow_bots{};
  bool b_lock_faction{};
  bool b_lock_color{};
  bool b_lock_team{};
  bool b_lock_handicap{};
  bool b_lock_spawn{};
  bool b_required{};
};

struct LobbyOptionState {
  std::string str_value;
  std::string str_preferred_value;
  bool b_is_locked{};
};

struct LobbyGlobal {
  std::string str_server_name;
  std::string str_map;
  std::string str_map_status;
  std::int32_t int4_random_seed{};
  bool b_allow_spectators{true};
  std::string str_game_uid;
  bool b_enable_singleplayer{};
  bool b_enable_map_generation{};
  bool b_enable_game_saves{};
  bool b_enable_sync_reports{};
  bool b_dedicated{};
  std::int32_t int4_net_frame_interval{3};
  std::int32_t int4_game_timestep{};
  std::vector<std::pair<std::string, LobbyOptionState>> vec_options;
};

struct LobbySession {
  std::vector<LobbyClient> vec_clients;
  std::vector<std::pair<std::string, LobbySlot>> vec_slots;
  std::vector<std::int32_t> vec_disabled_spawn_points;
  LobbyGlobal global;

  [[nodiscard]] const LobbyClient* client_with_index(std::int32_t int4_index) const;
  [[nodiscard]] LobbyClient* client_with_index(std::int32_t int4_index);
  [[nodiscard]] const LobbyClient* client_in_slot(std::string_view str_slot) const;
  [[nodiscard]] LobbyClient* client_in_slot(std::string_view str_slot);
  [[nodiscard]] const LobbySlot* slot(std::string_view str_player_reference) const;
  [[nodiscard]] const std::string* first_empty_slot() const;
  [[nodiscard]] std::string serialize() const;
};

struct LobbyPlayerRef {
  std::string str_name;
  bool b_playable{};
  bool b_allow_bots{true};
  bool b_lock_faction{};
  bool b_lock_color{};
  bool b_lock_team{};
  bool b_lock_handicap{};
  bool b_lock_spawn{};
  bool b_required{};
  std::string str_faction;
  std::int32_t int4_team{};
  std::string str_bot;
};

[[nodiscard]] std::expected<std::vector<LobbyPlayerRef>, std::string>
lobby_player_refs_from_map_yaml(std::string_view str_yaml, std::string_view str_source_name);

struct LobbyMapInfo {
  std::string str_name;
  std::string str_uid;
  std::string str_title;
  std::vector<LobbyPlayerRef> vec_players;
  std::vector<std::string> vec_selectable_factions;
};

[[nodiscard]] std::vector<std::string> ra_selectable_factions();

struct LobbyServerModel {
  LobbySession lobby_info;
  LobbyMapInfo map;
  std::vector<LobbyMapInfo> vec_maps;
  bool b_start_requested{};

  void server_start(std::vector<LobbyMapInfo> vec_maps_in, std::size_t sz_initial_index);
  void join_local_client(std::string_view str_name, std::string_view str_color_hex);
  [[nodiscard]] bool select_map(std::string_view str_map_name);
  [[nodiscard]] bool interpret_command(std::int32_t int4_client_index, std::string_view str_cmd);
};

}
