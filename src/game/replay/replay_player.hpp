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

// 投喂提前量 = 服务器 OrderLatency（ra "default" 速度 mod.yaml:296 = 3；
// 只影响 chunk 投喂提前量，不影响确定性）
constexpr std::int32_t kReplayOrderLatency{3};

// chunk 聚合（ReplayConnection.cs:44-93 语义）：自上一常规帧包后的全部包
// （含 sync/disconnect/frame-0 包——它们不终结 chunk）+ 本帧订单包。
struct ReplayChunk {
  std::int32_t int4_frame{};
  std::vector<std::pair<std::int32_t, std::vector<std::uint8_t>>> vec_packets;
};

struct ReplayFeedModel {
  std::deque<ReplayChunk> deque_chunks;
  // 录中 sync 封包（frame → (hash, defeatState)）——逐帧对拍参照
  std::map<std::int32_t, std::pair<std::int32_t, std::uint64_t>> map_recorded_syncs;
  ReplaySessionInfo session;      // SyncInfo 单解析（seed/帧间隔/client 表）
  bool b_valid{};                 // 上游 IsValid：流内见过 StartGame 引导单
  std::int32_t int4_tick_count{}; // 上游 TickCount = 最大 frame（尾帧投影可比仿真帧多 1）
  // 逐帧对拍累计：本地 ProcessOrders 点 (frame → (hash, defeat))（探针 SentSyncs 口径）
  std::map<std::int32_t, std::pair<std::int32_t, std::uint64_t>> map_local_syncs;
  // 投喂过的订单包（frame → 订单字节，无帧前缀）：launcher 桥（Move/Attack 等订单管道外
  // 效果）重演时按帧消费——C++ OrderManager 管道只覆盖 trait resolve 面
  std::vector<std::pair<std::int32_t, std::vector<std::uint8_t>>> vec_replayed_orders;
  // 每帧 mismatch 行（探针 desync 报告格式复刻）：
  //   "frame F: recorded (h,d) != local (h,d)"
  std::vector<std::string> vec_desync_reports;
};

// 从 .orarep 构建重演 feed（ReplayConnection ctor 聚合语义；frame-0 包不投喂不建世界——
// 无头宿主世界由调用方按 session 重建，与探针 ReplayPumpConnection 同构）。
[[nodiscard]] std::expected<ReplayFeedModel, std::string> replay_feed_build(
    const ReplayFile& file);

// 每帧投喂（ReplayConnection.Receive 语义，OrderManager.connection_receive 的重演分支）：
// 先排干本地 sync 队列（首到者成 syncForFrame 参照），再按 chunk.Frame <= NetFrameNumber +
// kReplayOrderLatency 投喂——订单包入 per-client pending、sync 包与参照比对（不等 → desync
// 报告 + om.b_out_of_sync）。定义在 order_manager.cpp（复用其 receive_orders/receive_sync）。
void replay_feed_pump(ReplayFeedModel& feed, network::OrderManagerModel& om,
                      world::WorldModel& w);

}  // namespace ora::game::replay
