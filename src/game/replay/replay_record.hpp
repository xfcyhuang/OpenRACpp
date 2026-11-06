#pragma once

import std;

#include "game/miniyaml.hpp"
#include "game/network/order.hpp"
#include "game/replay/replay_format.hpp"

namespace ora::game::replay {

// —— Session（SyncInfo 引导包内容）最小面 ——
// 重演世界重建所需字段：seed / 地图 uid / 帧间隔 / client 列表。
// 上游默认 NetFrameInterval=3（Session.cs:222）——黄金与本录制器统一 1（t07 帧 1:1 模型）。

struct ReplaySessionInfo {
  std::int32_t int4_seed{};
  std::string str_map_uid;
  std::int32_t int4_net_frame_interval{1};
  std::int32_t int4_game_timestep{40};
  std::vector<std::int32_t> vec_client_indices;  // Client@N 的 Index（升序）
};

// 上游 Session.Serialize yaml 格式的核心子集（字段序对齐探针实测输出）。
[[nodiscard]] std::string replay_session_yaml(const ReplaySessionInfo& info);

// 解析（兼容上游全字段 Session yaml——黄金文件的 SyncInfo 单）：GlobalSettings 与 Client@N。
[[nodiscard]] std::expected<ReplaySessionInfo, std::string> replay_session_parse(
    std::string_view str_yaml);

// —— GameInformation 元数据（ReplayMetadata.cs 写侧 / GameInformation.cs 字段序）——

struct ReplayGameInfo {
  std::string str_mod;
  std::string str_version;
  std::string str_map_uid;
  std::string str_map_title;
  std::int32_t int4_final_game_tick{};
  std::string str_start_time_utc;  // "yyyy-MM-dd HH-mm-ss"（invariant，上游 FieldSaver.cs:131-137）

  struct PlayerInfo {
    std::int32_t int4_client_index{};
    std::string str_name;
    bool b_is_human{true};
  };
  std::vector<PlayerInfo> vec_players;
};

[[nodiscard]] std::vector<MiniYamlNode> replay_metadata_nodes(const ReplayGameInfo& info);

// —— 录制器（ReplayRecorder.cs 语义 + RecordingEchoConnection 挂接形态，探针 §3.2）——
//
// 挂接五点（OrderManager 侧）：
//   StartGame → [1] pacing 空包；帧单 → [应用帧 = N+1]（服务器 Ack 投影）；
//   immediate → [0]；SendSync → 17 字节 sync 封包；引导包两枚由宿主手工注入（from=0 frame=0，
//   SyncInfo 在 StartGame 之前——后者触发上游 pre-start 落盘）。

struct ReplayRecorderModel {
  std::vector<ReplayRecord> vec_records;
  bool b_game_start_seen{};  // 上游 IsGameStart：收到含 frame-0 StartGame 单的包才落盘
  std::vector<MiniYamlNode> vec_meta_nodes;
  bool b_metadata_attached{};
};

void recorder_receive(ReplayRecorderModel& rec, std::int32_t int4_client,
                      std::vector<std::uint8_t> vec_data);

// 元数据挂接（上游 World.LoadComplete 尾部 nc.Recorder.Metadata = ... 的等价物）。
void recorder_attach_metadata(ReplayRecorderModel& rec, std::vector<MiniYamlNode> vec_nodes);

// Dispose（ReplayRecorder.cs:102-117 等价）：EndTimeUtc = 当前 UTC 墙钟写入 Root 节点，
// 产出整文件字节。未见过 StartGame 引导包（pre-start 缓冲从未落盘）或未挂元数据 → 错误。
[[nodiscard]] std::expected<std::vector<std::uint8_t>, std::string> recorder_dispose(
    ReplayRecorderModel& rec);

// 服务器引导包（Server.cs:1288 SyncInfo / :1428 StartGame，from=0 frame=0 的 immediate 单）。
[[nodiscard]] std::vector<std::uint8_t> make_boot_packet(std::string_view str_order,
                                                         std::string_view str_target);

}  // namespace ora::game::replay
