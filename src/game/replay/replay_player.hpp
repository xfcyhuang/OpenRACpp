#pragma once

import std;

#include "game/replay/replay_format.hpp"
#include "game/replay/replay_record.hpp"

namespace ora::game::world {
struct WorldModel;
}

namespace ora::game::network {
struct OrderManagerModel;
}

namespace ora::game::replay {

constexpr std::int32_t kReplayOrderLatency{3};

struct ReplayChunk {
  std::int32_t int4_frame{};
  std::vector<std::pair<std::int32_t, std::vector<std::uint8_t>>> vec_packets;
};

struct ReplayFeedModel {
  std::deque<ReplayChunk> deque_chunks;

  std::map<std::int32_t, std::pair<std::int32_t, std::uint64_t>> map_recorded_syncs;
  ReplaySessionInfo session;
  bool b_valid{};
  std::int32_t int4_tick_count{};

  std::map<std::int32_t, std::pair<std::int32_t, std::uint64_t>> map_local_syncs;

  std::vector<std::pair<std::int32_t, std::vector<std::uint8_t>>> vec_replayed_orders;

  std::vector<std::string> vec_desync_reports;
};

[[nodiscard]] std::expected<ReplayFeedModel, std::string> replay_feed_build(
    const ReplayFile& file);

void replay_feed_pump(ReplayFeedModel& feed, network::OrderManagerModel& om,
                      world::WorldModel& w);

}
