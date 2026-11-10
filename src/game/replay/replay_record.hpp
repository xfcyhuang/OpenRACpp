#pragma once

import std;

#include "game/miniyaml.hpp"
#include "game/network/order.hpp"
#include "game/replay/replay_format.hpp"

namespace ora::game::replay {

struct ReplaySessionInfo {
  std::int32_t int4_seed{};
  std::string str_map_uid;
  std::int32_t int4_net_frame_interval{1};
  std::int32_t int4_game_timestep{40};
  std::vector<std::int32_t> vec_client_indices;
};

[[nodiscard]] std::string replay_session_yaml(const ReplaySessionInfo& info);

[[nodiscard]] std::expected<ReplaySessionInfo, std::string> replay_session_parse(
    std::string_view str_yaml);

struct ReplayGameInfo {
  std::string str_mod;
  std::string str_version;
  std::string str_map_uid;
  std::string str_map_title;
  std::int32_t int4_final_game_tick{};
  std::string str_start_time_utc;

  struct PlayerInfo {
    std::int32_t int4_client_index{};
    std::string str_name;
    bool b_is_human{true};
  };
  std::vector<PlayerInfo> vec_players;
};

[[nodiscard]] std::vector<MiniYamlNode> replay_metadata_nodes(const ReplayGameInfo& info);

struct ReplayRecorderModel {
  std::vector<ReplayRecord> vec_records;
  bool b_game_start_seen{};
  std::vector<MiniYamlNode> vec_meta_nodes;
  bool b_metadata_attached{};
};

void recorder_receive(ReplayRecorderModel& rec, std::int32_t int4_client,
                      std::vector<std::uint8_t> vec_data);

void recorder_attach_metadata(ReplayRecorderModel& rec, std::vector<MiniYamlNode> vec_nodes);

[[nodiscard]] std::expected<std::vector<std::uint8_t>, std::string> recorder_dispose(
    ReplayRecorderModel& rec);

[[nodiscard]] std::vector<std::uint8_t> make_boot_packet(std::string_view str_order,
                                                         std::string_view str_target);

}
