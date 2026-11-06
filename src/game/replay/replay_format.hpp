#pragma once

import std;

#include "game/miniyaml.hpp"

namespace ora::game::replay {

// .orarep 容器层（ReplayRecorder.cs / ReplayMetadata.cs / ReplayConnection.cs 的读写面）。
//
// Record := [clientID i32 LE][dataLen i32 LE][data]
//   client 0=服务器引导包；1..N=客户端；kMetaStartMarker 终结记录流。
//   data 三形态按内容判别（无类型标签）：帧订单包 [frame i32][订单串接]、
//   sync 包 17B [frame][0x65][hash i32][defeatState u64]、disconnect 包 9B。
// MetaBlock := [kMetaStartMarker][kMetaVersion i32][yamlLen i32][yaml UTF-8]
//              [dataLength = 4 + yamlLen i32][kMetaEndMarker]   ← i32 长度前缀（非 varint）
//
// 读→写整文件恒等 = 记录流逐条透传 + 元数据 yaml 走 MiniYaml 节点级往返（字段序保持）。

constexpr std::int32_t kMetaStartMarker{-1};
constexpr std::int32_t kMetaEndMarker{-2};
constexpr std::int32_t kMetaVersion{1};

struct ReplayRecord {
  std::int32_t int4_client{};
  std::vector<std::uint8_t> vec_data;
  std::size_t idx_offset{};  // 读侧原文件偏移（篡改负向样本定位用）
};

struct ReplayFile {
  std::vector<ReplayRecord> vec_records;  // MetaStartMarker 之前的全部记录（原序）
  bool b_has_metadata{};
  std::vector<MiniYamlNode> vec_meta_nodes;  // 元数据 yaml 节点树（保持上游字段序）
  std::int32_t int4_final_game_tick{};       // Root.FinalGameTick 便捷抽取（回放计时口径）
};

[[nodiscard]] std::expected<ReplayFile, std::string> replay_read(
    std::span<const std::uint8_t> span_bytes);

[[nodiscard]] std::vector<std::uint8_t> replay_write(const ReplayFile& file);

// 元数据节点树的一级/二级字段值查找（"Root" 下的字段或其子节点字段）。
[[nodiscard]] std::optional<std::string_view> meta_find_value(
    const std::vector<MiniYamlNode>& vec_nodes, std::string_view str_parent,
    std::string_view str_key);

}  // namespace ora::game::replay
