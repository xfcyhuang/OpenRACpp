import std;

#include "server/order_buffer.hpp"

namespace ora::server {

namespace {

std::int64_t ElapsedMsSince(std::chrono::steady_clock::time_point time_start) {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::steady_clock::now() - time_start)
      .count();
}

}

void OrderBuffer::AddOrderTimestamp(int int4_player_index) {
  if (!b_started_)
    return;

  map_timestamps[int4_player_index] = ElapsedMsSince(time_game_start_);

  bool all_set = true;
  for (const auto& [player, timestamp] : map_timestamps)
    if (timestamp == kEmptyValue)
      all_set = false;

  if (all_set) {
    const auto baseline = map_timestamps[int4_baseline_player];

    for (auto& [player, timestamp] : map_timestamps) {
      const auto dt = baseline - timestamp;

      auto& player_deltas = map_deltas[player];
      player_deltas.push_back(dt);

      if (player_deltas.size() > static_cast<std::size_t>(kNumberOfFrames))
        player_deltas.pop_front();

      timestamp = kEmptyValue;
    }
  }
}

void OrderBuffer::Start(const game::GameSpeed& game_speed,
                        std::vector<int> vec_new_players) {
  int4_timestep = game_speed.Timestep;
  int4_ticks_per_interval = kInterval / int4_timestep;

  vec_players = std::move(vec_new_players);
  int4_baseline_player = vec_players.front();

  for (const auto player : vec_players) {
    map_timestamps[player] = kEmptyValue;
    map_deltas[player] = std::deque<long>{};
  }

  time_game_start_ = std::chrono::steady_clock::now();
  i8_next_update =
      ElapsedMsSince(time_game_start_) + kInterval;
  b_started_ = true;
}

std::vector<std::pair<int, float>> OrderBuffer::GetTickScales() {
  auto vec_out = std::vector<std::pair<int, float>>{};
  if (!b_started_)
    return vec_out;

  const auto now = ElapsedMsSince(time_game_start_);
  if (now < i8_next_update)
    return vec_out;

  i8_next_update = now + kInterval;

  if (map_deltas.empty())
    return vec_out;

  for (const auto& [player, deltas] : map_deltas)
    if (deltas.size() != static_cast<std::size_t>(kNumberOfFrames))
      return vec_out;

  auto vec_medians = std::vector<std::pair<int, long>>{};
  for (const auto player : vec_players) {
    const auto it = map_deltas.find(player);
    if (it == map_deltas.end())
      continue;
    vec_medians.emplace_back(
        player, Median({it->second.begin(), it->second.end()}));
  }

  auto min_value = vec_medians.front().second;
  for (const auto& [player, delta] : vec_medians) {
    static_cast<void>(player);
    if (delta < min_value)
      min_value = delta;
  }

  const auto offset = min_value < 0 ? -min_value : 0;

  for (const auto& [player, delta] : vec_medians) {
    const auto delta_per_tick =
        static_cast<float>(delta + offset) / static_cast<float>(int4_ticks_per_interval);

    const auto tick_scale =
        (static_cast<float>(int4_timestep) + delta_per_tick) /
        static_cast<float>(int4_timestep);

    auto adjusted = tick_scale;
    if (adjusted < 1.0f)
      adjusted = 1.0f;
    if (adjusted > kMaxTickScale)
      adjusted = kMaxTickScale;

    vec_out.emplace_back(player, adjusted);
  }

  return vec_out;
}

long OrderBuffer::Median(std::vector<long> vec_a) {
  std::sort(vec_a.begin(), vec_a.end());
  const auto n = vec_a.size();

  if (n % 2 != 0)
    return vec_a[n / 2];

  return (vec_a[(n - 1) / 2] + vec_a[n / 2]) / 2;
}

void OrderBuffer::RemovePlayer(int int4_player) {
  vec_players.erase(
      std::remove(vec_players.begin(), vec_players.end(), int4_player),
      vec_players.end());
  if (int4_player == int4_baseline_player && !vec_players.empty())
    int4_baseline_player = vec_players.front();

  map_timestamps.erase(int4_player);
  map_deltas.erase(int4_player);
}

}
