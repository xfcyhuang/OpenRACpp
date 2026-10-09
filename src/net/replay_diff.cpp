import std;

#include "net/replay_diff.hpp"

#include "net/order_io.hpp"

namespace ora::net {

namespace {

bool ReadInt32Le(const std::vector<std::uint8_t>& vec_bytes,
                 std::size_t& pos, std::int32_t& out) {
  if (pos + 4 > vec_bytes.size())
    return false;
  std::uint32_t uint4_v = 0;
  for (int i = 0; i < 4; ++i)
    uint4_v |= static_cast<std::uint32_t>(vec_bytes[pos + static_cast<std::size_t>(i)])
               << (8 * i);
  pos += 4;
  out = static_cast<std::int32_t>(uint4_v);
  return true;
}

std::string HexDump(std::span<const std::uint8_t> bytes, std::size_t max_n) {
  std::string str_out;
  for (std::size_t i = 0; i < bytes.size() && i < max_n; ++i) {
    if (i != 0)
      str_out += ' ';
    str_out += std::format("{:02X}", bytes[i]);
  }
  if (bytes.size() > max_n)
    str_out += std::format(" ... ({} bytes)", bytes.size());
  return str_out;
}

}  // namespace

std::optional<ReplayFileData> ParseReplayFile(
    const std::vector<std::uint8_t>& vec_bytes) {
  ReplayFileData data_out;
  std::size_t pos = 0;

  while (pos < vec_bytes.size()) {
    std::int32_t int4_client = 0;
    if (!ReadInt32Le(vec_bytes, pos, int4_client))
      return std::nullopt;
    if (int4_client == ReplayMetadata::kMetaStartMarker)
      break;

    std::int32_t int4_packet_len = 0;
    if (!ReadInt32Le(vec_bytes, pos, int4_packet_len))
      return std::nullopt;
    if (int4_packet_len < 0 ||
        pos + static_cast<std::uint64_t>(
                   static_cast<std::uint64_t>(int4_packet_len)) >
            vec_bytes.size())
      return std::nullopt;

    std::vector<std::uint8_t> vec_packet(
        vec_bytes.begin() + static_cast<std::ptrdiff_t>(pos),
        vec_bytes.begin() + static_cast<std::ptrdiff_t>(
                                pos + static_cast<std::size_t>(int4_packet_len)));
    pos += static_cast<std::size_t>(int4_packet_len);

    std::int32_t int4_frame = 0;
    std::size_t pos_frame = 0;
    if (!ReadInt32Le(vec_packet, pos_frame, int4_frame))
      int4_frame = 0;

    data_out.vec_packets.push_back(
        ReplayPacketRecord{int4_frame, int4_client, std::move(vec_packet)});

    const std::vector<std::uint8_t>& vec_packet_kept =
        data_out.vec_packets.back().vec_data;
    if (vec_packet_kept.size() > 4 &&
        (vec_packet_kept[4] ==
             static_cast<std::uint8_t>(OrderType::Disconnect) ||
         vec_packet_kept[4] ==
             static_cast<std::uint8_t>(OrderType::SyncHash))) {
      SyncPacketData sync;
      if (OrderIO::TryParseSync(vec_packet_kept, sync))
        data_out.vec_syncs.push_back(
            ReplaySyncRecord{sync.frame, sync.sync_hash,
                             sync.uint8_defeat_state});
      continue;
    }

    if (int4_frame == 0) {
      int int4_frame_parsed = 0;
      OrderPacket packet;
      if (OrderIO::TryParseOrderPacket(vec_packet_kept,
                                       int4_frame_parsed, packet)) {
        for (const std::unique_ptr<Order>& order : packet.GetOrders(nullptr)) {
          if (order->str_order_string == "StartGame")
            data_out.b_valid = true;
        }
      }
    } else {
      data_out.int4_tick_count =
          std::max(data_out.int4_tick_count, int4_frame);
    }
  }

  data_out.up_metadata = ReplayMetadata::Read(vec_bytes);
  return data_out;
}

ReplayDiffReport DiffReplays(const ReplayFileData& replay_a,
                             const ReplayFileData& replay_b) {
  ReplayDiffReport report;

  const auto note_frame = [&](int int4_frame) {
    if (report.int4_first_divergent_frame < 0 ||
        int4_frame < report.int4_first_divergent_frame)
      report.int4_first_divergent_frame = int4_frame;
    report.b_equal = false;
  };

  if (replay_a.b_valid != replay_b.b_valid) {
    report.b_equal = false;
    report.vec_lines.push_back(
        std::format("valid: {} vs {}", replay_a.b_valid, replay_b.b_valid));
  }

  if (replay_a.int4_tick_count != replay_b.int4_tick_count) {
    report.b_equal = false;
    report.vec_lines.push_back(std::format(
        "tick count: {} vs {}", replay_a.int4_tick_count,
        replay_b.int4_tick_count));
  }

  if (replay_a.up_metadata == nullptr || replay_b.up_metadata == nullptr) {
    if (replay_a.up_metadata != replay_b.up_metadata) {
      report.b_equal = false;
      report.vec_lines.push_back(
          std::format("metadata: {} vs {}",
                      replay_a.up_metadata != nullptr ? "present" : "missing",
                      replay_b.up_metadata != nullptr ? "present" : "missing"));
    }
  } else if (replay_a.up_metadata->GameInfoData() !=
             replay_b.up_metadata->GameInfoData()) {
    report.b_equal = false;
    report.vec_lines.push_back("metadata: game info differs");
  }

  const std::size_t n_packets =
      std::min(replay_a.vec_packets.size(), replay_b.vec_packets.size());
  for (std::size_t i = 0; i < n_packets; ++i) {
    const ReplayPacketRecord& rec_a = replay_a.vec_packets[i];
    const ReplayPacketRecord& rec_b = replay_b.vec_packets[i];
    if (rec_a.int4_client_id != rec_b.int4_client_id ||
        rec_a.int4_frame != rec_b.int4_frame ||
        rec_a.vec_data != rec_b.vec_data) {
      note_frame(rec_a.int4_frame);
      report.vec_lines.push_back(std::format(
          "packet #{}: frame {} client {} vs frame {} client {}", i,
          rec_a.int4_frame, rec_a.int4_client_id, rec_b.int4_frame,
          rec_b.int4_client_id));
      if (rec_a.vec_data != rec_b.vec_data) {
        report.vec_lines.push_back(std::format(
            "  A: {}", HexDump(rec_a.vec_data, 32)));
        report.vec_lines.push_back(std::format(
            "  B: {}", HexDump(rec_b.vec_data, 32)));
      }
    }
  }

  if (replay_a.vec_packets.size() != replay_b.vec_packets.size()) {
    report.b_equal = false;
    const auto& longer = replay_a.vec_packets.size() >
                                 replay_b.vec_packets.size()
                             ? replay_a
                             : replay_b;
    for (std::size_t i = n_packets; i < longer.vec_packets.size(); ++i)
      note_frame(longer.vec_packets[i].int4_frame);
    report.vec_lines.push_back(std::format(
        "packet count: {} vs {}", replay_a.vec_packets.size(),
        replay_b.vec_packets.size()));
  }

  const std::size_t n_syncs =
      std::min(replay_a.vec_syncs.size(), replay_b.vec_syncs.size());
  for (std::size_t i = 0; i < n_syncs; ++i) {
    const ReplaySyncRecord& rec_a = replay_a.vec_syncs[i];
    const ReplaySyncRecord& rec_b = replay_b.vec_syncs[i];
    if (rec_a.int4_frame != rec_b.int4_frame ||
        rec_a.int4_sync_hash != rec_b.int4_sync_hash ||
        rec_a.uint8_defeat_state != rec_b.uint8_defeat_state) {
      note_frame(rec_a.int4_frame);
      report.vec_lines.push_back(std::format(
          "sync #{}: frame {} hash {} defeat {} vs frame {} hash {} defeat {}",
          i, rec_a.int4_frame, rec_a.int4_sync_hash, rec_a.uint8_defeat_state,
          rec_b.int4_frame, rec_b.int4_sync_hash, rec_b.uint8_defeat_state));
    }
  }

  if (replay_a.vec_syncs.size() != replay_b.vec_syncs.size()) {
    report.b_equal = false;
    const auto& longer = replay_a.vec_syncs.size() > replay_b.vec_syncs.size()
                             ? replay_a
                             : replay_b;
    for (std::size_t i = n_syncs; i < longer.vec_syncs.size(); ++i)
      note_frame(longer.vec_syncs[i].int4_frame);
    report.vec_lines.push_back(std::format(
        "sync count: {} vs {}", replay_a.vec_syncs.size(),
        replay_b.vec_syncs.size()));
  }

  return report;
}

}  // namespace ora::net
