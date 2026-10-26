// UPSTREAM: OpenRA.Game/Server/OrderBuffer.cs @1f594e320 L21-138 全文
//          (TickScale 动态延迟的测量核:玩家订单时间戳 → 基线差值的中位
//          数 → (timestep + deltaPerTick)/timestep,钳 [1, 1.1];20 帧
//          窗口/1000ms 更新间隔;ConcurrentDictionary → 单线程服务器事
//          件的插入序容器 —— 上游并发容器仅为防御形态,本侧单线程访问
//          等价)
//          The whole of OrderBuffer.cs L21-138 (the measurement core of
//          the dynamic TickScale latency: player order timestamps → the
//          median of the baseline deltas → (timestep + deltaPerTick) /
//          timestep clamped to [1, 1.1]; the 20-frame window / 1000ms
//          update interval; ConcurrentDictionary → the single-threaded
//          server-event insertion-ordered container — upstream's
//          concurrent containers are a defensive shape only, and this
//          side's single-threaded access is equivalent).
#pragma once
import std;

#include "game/game_speed.hpp"

namespace ora::server {

/// OrderBuffer(OrderBuffer.cs L21-138)
/// OrderBuffer (OrderBuffer.cs L21-138).
class OrderBuffer {
 public:
  static constexpr int kNumberOfFrames = 20;  // L23
  static constexpr int kInterval = 1000;      // L24
  static constexpr float kMaxTickScale = 1.1f;  // L27
  static constexpr long kEmptyValue = -1;  // L29

  /// AddOrderTimestamp(L42-63):全员到齐时结算基线差值并入窗
  /// AddOrderTimestamp (L42-63): once every player has checked in, the
  /// baseline deltas settle into the window.
  void AddOrderTimestamp(int int4_player_index);

  /// Start(L65-81)
  void Start(const game::GameSpeed& game_speed, std::vector<int> vec_players);

  /// GetTickScales(L83-113):未到更新时刻/窗未满 = 空;否则逐玩家的
  /// (index, scale) 序列(玩家序 = Start 装载序)
  /// GetTickScales (L83-113): empty before the update moment or with an
  /// unfilled window; otherwise the per-player (index, scale) sequence
  /// (player order = the Start order).
  std::vector<std::pair<int, float>> GetTickScales();

  /// RemovePlayer(L126-137):基线玩家移除时移交首位
  /// RemovePlayer (L126-137): removing the baseline player hands the role
  /// to the first remaining.
  void RemovePlayer(int int4_player);

 private:
  /// Median(L115-124):排序取中(偶数个 = 两中位均值,向零截断)
  /// Median (L115-124): sort-and-pick (an even count averages the two
  /// middles, truncating toward zero).
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

}  // namespace ora::server
