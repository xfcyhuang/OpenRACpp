#pragma once
import std;

#include "game/game_speed.hpp"

namespace ora::server {

class OrderBuffer {
 public:
  static constexpr int kNumberOfFrames = 20;
  static constexpr int kInterval = 1000;
  static constexpr float kMaxTickScale = 1.1f;
  static constexpr long kEmptyValue = -1;

  void AddOrderTimestamp(int int4_player_index);

  void Start(const game::GameSpeed& game_speed, std::vector<int> vec_players);

  std::vector<std::pair<int, float>> GetTickScales();

  void RemovePlayer(int int4_player);

 private:

  static long Median(std::vector<long> vec_a);

  bool b_started_ = false;
  std::chrono::steady_clock::time_point time_game_start_{};
  std::int64_t i8_next_update = 0;

  std::map<int, long> map_timestamps;
  std::map<int, std::deque<long>> map_deltas;

  int int4_timestep = 0;
  int int4_ticks_per_interval = 0;
  int int4_baseline_player = 0;
  std::vector<int> vec_players;
};

}
