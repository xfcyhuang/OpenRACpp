#include "game/replay/replay_player.hpp"

#include "game/network/order.hpp"
#include "game/network/order_packet.hpp"

namespace ora::game::replay {

namespace {

// 帧号取 data 前 4 字节（ReplayConnection ctor：packet 前 4 字节为 frame）
[[nodiscard]] std::int32_t frame_of(const std::vector<std::uint8_t>& vec_data) {
  if (vec_data.size() < 4) return 0;
  return static_cast<std::int32_t>(static_cast<std::uint32_t>(vec_data[0]) |
                                   (static_cast<std::uint32_t>(vec_data[1]) << 8) |
                                   (static_cast<std::uint32_t>(vec_data[2]) << 16) |
                                   (static_cast<std::uint32_t>(vec_data[3]) << 24));
}

// frame-0 包内的订单串里找指定 immediate 单的 TargetString（SyncInfo 提取）
[[nodiscard]] std::optional<std::string> packet_find_target_string(
    std::span<const std::uint8_t> span_data, std::string_view str_order_name) {
  if (span_data.size() < 4) return std::nullopt;
  network::OrderReader reader{{span_data.begin() + 4, span_data.end()}};
  while (reader.idx_pos < reader.span_bytes.size()) {
    const std::optional<network::OrderModel> opt_order{
        network::deserialize_order(reader, nullptr)};
    if (!opt_order.has_value()) break;
    if (opt_order->str_order == str_order_name && opt_order->opt_str_target.has_value())
      return *opt_order->opt_str_target;
  }
  return std::nullopt;
}

}  // namespace

std::expected<ReplayFeedModel, std::string> replay_feed_build(const ReplayFile& file) {
  ReplayFeedModel feed;

  std::vector<std::pair<std::int32_t, std::vector<std::uint8_t>>> vec_pending_packets;
  bool b_session_seen{};
  std::int32_t int4_max_frame{};

  for (const ReplayRecord& record : file.vec_records) {
    const std::int32_t int4_frame{frame_of(record.vec_data)};
    if (int4_frame > int4_max_frame) int4_max_frame = int4_frame;

    vec_pending_packets.emplace_back(record.int4_client, record.vec_data);

    const bool b_is_sync{record.vec_data.size() == 17 && record.vec_data[4] == 0x65};
    const bool b_is_disconnect{record.vec_data.size() == 9 && record.vec_data[4] == 0xbf};
    if (b_is_sync) {
      const auto opt_parsed{network::try_parse_sync_packet(record.vec_data)};
      if (opt_parsed.has_value())
        feed.map_recorded_syncs.emplace(
            opt_parsed->int4_frame,
            std::make_pair(opt_parsed->int4_sync_hash, opt_parsed->u8_defeat_state));
      continue;
    }
    if (b_is_disconnect) continue;

    if (int4_frame == 0) {
      // 引导/immediate 单：不终结 chunk；提取 SyncInfo（!IsValid 时最后一份生效）与 StartGame
      if (const auto opt_session{
              packet_find_target_string(record.vec_data, "SyncInfo")}) {
        auto res_session{replay_session_parse(*opt_session)};
        if (!res_session.has_value()) return std::unexpected(res_session.error());
        if (!b_session_seen || !feed.b_valid) {
          feed.session = std::move(*res_session);
          b_session_seen = true;
        }
      }
      if (packet_find_target_string(record.vec_data, "StartGame").has_value())
        feed.b_valid = true;
      continue;
    }

    ReplayChunk chunk;
    chunk.int4_frame = int4_frame;
    chunk.vec_packets = std::move(vec_pending_packets);
    vec_pending_packets.clear();
    feed.deque_chunks.push_back(std::move(chunk));
  }

  feed.int4_tick_count = int4_max_frame;
  if (!feed.b_valid)
    return std::unexpected("replay: no StartGame boot packet (upstream ReplayConnection invalid)");
  if (!b_session_seen)
    return std::unexpected("replay: no SyncInfo boot packet (cannot rebuild world)");
  return feed;
}

}  // namespace ora::game::replay
