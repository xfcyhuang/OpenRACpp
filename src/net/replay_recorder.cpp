// UPSTREAM: OpenRA.Game/Network/ReplayRecorder.cs @b6fc03f L19-118 +
//           OpenRA.Game/FileFormats/ReplayMetadata.cs L17-108 +
//           OpenRA.Game/Network/ReplayConnection.cs L19-135(实现部分)
//           The implementation halves (the header note lives in
//           replay_recorder.hpp).
#include "net/replay_recorder.hpp"

#include "net/order.hpp"
#include "net/order_io.hpp"
#include "net/order_manager.hpp"

namespace ora::net {

namespace {

/// WriteLengthPrefixedString(UTF8, 1024*100 上限)的写侧等价
/// The write-side equivalent of WriteLengthPrefixedString (UTF8, the
/// 1024*100 cap).
void WriteLengthPrefixedString(ByteWriter& writer,
                               std::string_view sv_data) {
  writer.Write(sv_data);
}

/// Write 的 dataLength 语义(L58-66):载荷长度(前缀串整体 —— 含 7-bit
/// 前缀字节)
/// Write's dataLength semantics (L58-66): the payload length (the whole
/// prefixed string — the 7-bit prefix bytes included).
std::int32_t LengthPrefixedSize(std::string_view sv_data) {
  std::size_t sz_prefix = 0;
  std::uint64_t v = sv_data.size();
  while (v >= 0x80) {
    ++sz_prefix;
    v >>= 7;
  }
  ++sz_prefix;
  return static_cast<std::int32_t>(sz_prefix + sv_data.size());
}

}  // namespace

// ———— ReplayMetadata ————

void ReplayMetadata::Write(std::vector<std::uint8_t>& vec_out) const {
  // L51-67
  ByteWriter writer;
  writer.Write(static_cast<std::int32_t>(kMetaStartMarker));
  writer.Write(static_cast<std::int32_t>(kMetaVersion));

  std::int32_t int4_data_length = 0;
  {
    // 大厅信息数据 | the lobby-info data.
    WriteLengthPrefixedString(writer, str_game_info_data_);
    int4_data_length = LengthPrefixedSize(str_game_info_data_);
  }

  writer.Write(int4_data_length);
  writer.Write(static_cast<std::int32_t>(kMetaEndMarker));

  vec_out.insert(vec_out.end(), writer.Bytes().begin(), writer.Bytes().end());
}

std::unique_ptr<ReplayMetadata> ReplayMetadata::Read(
    const std::vector<std::uint8_t>& vec_bytes) {
  // L79-106:尾扫;短文件/无标记/损坏 → nullptr
  // L79-106: the tail scan; short files/missing markers/corruption →
  // nullptr.
  try {
    if (vec_bytes.size() < 20)
      return nullptr;

    const std::size_t sz_tail = vec_bytes.size() - 8;
    ByteReader tail{vec_bytes.data() + sz_tail, 8};
    const std::int32_t int4_data_length = tail.ReadInt32();
    if (tail.ReadInt32() != kMetaEndMarker)
      return nullptr;

    // 回退(end 标记 + 长度存储 + 数据 + 版本 + start 标记)
    // Step back (the end marker + the length storage + the data + the
    // version + the start marker).
    const std::int64_t int8_back =
        4 + 4 + static_cast<std::int64_t>(int4_data_length) + 4 + 4;
    if (static_cast<std::int64_t>(vec_bytes.size()) < int8_back)
      return nullptr;

    ByteReader reader{vec_bytes.data() + (vec_bytes.size() -
                                          static_cast<std::size_t>(int8_back)),
                      static_cast<std::size_t>(int8_back)};
    if (reader.ReadInt32() != kMetaStartMarker)
      throw std::runtime_error(
          "Expected MetaStartMarker but found an invalid value.");

    const std::int32_t int4_version = reader.ReadInt32();
    if (int4_version != kMetaVersion)
      throw std::runtime_error("Metadata version " +
                               std::to_string(int4_version) +
                               " is not supported");

    // 100K 上限(损坏文件的保险)| the 100K cap (corruption safeguard).
    if (reader.Remaining() < 0 ||
        static_cast<std::uint64_t>(reader.Remaining()) > 1024 * 100)
      return nullptr;

    auto up_metadata = std::make_unique<ReplayMetadata>();
    up_metadata->str_game_info_data_ = reader.ReadString();
    return up_metadata;
  } catch (const std::exception&) {
    return nullptr;
  }
}

// ———— ReplayRecorder ————

ReplayRecorder::ReplayRecorder(ChooseFilename fn_choose_filename)
    : fn_choose_filename_{std::move(fn_choose_filename)} {}

bool ReplayRecorder::IsGameStart(const std::vector<std::uint8_t>& vec_data) {
  // L29-34
  int int4_frame = 0;
  OrderPacket packet_orders;
  if (!OrderIO::TryParseOrderPacket(vec_data, int4_frame, packet_orders))
    return false;

  if (int4_frame != 0)
    return false;

  for (const auto& up_order : packet_orders.GetOrders(nullptr))
    if (up_order != nullptr && up_order->str_order_string == "StartGame")
      return true;

  return false;
}

void ReplayRecorder::StartSavingReplay(
    const std::vector<std::uint8_t>& vec_initial) {
  // L44-72:文件名冲突重试域由 chooseFilename 回调承载(引擎装配面)。
  // 上游 file.Write(initialContent) 把 preStart 缓冲落盘 —— 内存盘的
  // preStart 内容已在位,该写为幂等 no-op(vec_initial 即 vec_bytes_
  // 自身,追加即自拷贝)
  // L44-72: the filename-conflict retry domain rides the chooseFilename
  // callback (the engine assembly). Upstream's file.Write(initialContent)
  // flushes the pre-start buffer to disk — the in-memory sink already
  // holds the pre-start bytes, so the write is an idempotent no-op
  // (vec_initial IS vec_bytes_; appending would self-copy).
  if (fn_choose_filename_)
    str_filename_ = fn_choose_filename_();
  (void)vec_initial;
}

void ReplayRecorder::Receive(int client_id,
                             const std::vector<std::uint8_t>& vec_data) {
  // L74-90
  if (b_disposed_)
    return;

  if (!b_started_saving_ && IsGameStart(vec_data)) {
    b_started_saving_ = true;
    StartSavingReplay(vec_bytes_);
  }

  ByteWriter writer;
  writer.Write(client_id);
  writer.Write(static_cast<std::int32_t>(vec_data.size()));
  writer.WriteBytes(vec_data);
  const std::vector<std::uint8_t>& vec_written = writer.Bytes();
  vec_bytes_.insert(vec_bytes_.end(), vec_written.begin(), vec_written.end());
}

void ReplayRecorder::ReceiveFrame(int client_id, int frame,
                                  const std::vector<std::uint8_t>& vec_data) {
  // L92-98
  std::vector<std::uint8_t> vec_packet;
  ByteWriter writer;
  writer.Write(frame);
  writer.WriteBytes(vec_data);
  vec_packet = writer.TakeBytes();
  Receive(client_id, vec_packet);
}

void ReplayRecorder::Dispose() {
  // L101-117
  if (b_disposed_)
    return;
  b_disposed_ = true;

  if (Metadata != nullptr)
    Metadata->Write(vec_bytes_);
}

// ———— ReplayConnection ————

namespace {

std::optional<int> ExtractFinalGameTick(const std::string& str_game_info) {
  const std::string_view sv{str_game_info};
  const std::size_t pos = sv.find("FinalGameTick:");
  if (pos == std::string_view::npos)
    return std::nullopt;
  std::size_t cursor = pos + std::string_view{"FinalGameTick:"}.size();
  while (cursor < sv.size() && (sv[cursor] == ' ' || sv[cursor] == '\t'))
    ++cursor;
  std::size_t end = cursor;
  while (end < sv.size() && sv[end] >= '0' && sv[end] <= '9')
    ++end;
  if (end == cursor)
    return std::nullopt;
  int out = 0;
  const auto [ptr, ec] =
      std::from_chars(sv.data() + cursor, sv.data() + end, out, 10);
  if (ec != std::errc{})
    return std::nullopt;
  return out;
}

}  // namespace

ReplayConnection::ReplayConnection(
    std::vector<std::uint8_t> vec_replay_bytes, int int4_order_latency)
    : vec_replay_bytes_{std::move(vec_replay_bytes)},
      int4_order_latency_{int4_order_latency} {
  if (auto up_metadata = ReplayMetadata::Read(vec_replay_bytes_); up_metadata) {
    up_metadata_ = std::move(up_metadata);
    const std::size_t sz_tail = vec_replay_bytes_.size() - 8;
    ByteReader tail{vec_replay_bytes_.data() + sz_tail, 8};
    const std::int32_t int4_data_length = tail.ReadInt32();
    const std::int64_t int8_back =
        4 + 4 + static_cast<std::int64_t>(int4_data_length) + 4 + 4;
    sz_body_end_ =
        vec_replay_bytes_.size() - static_cast<std::size_t>(int8_back);
    if (const auto final_tick =
            ExtractFinalGameTick(up_metadata_->GameInfoData());
        final_tick)
      int4_final_game_tick_ = *final_tick;
  } else {
    sz_body_end_ = vec_replay_bytes_.size();
  }

  // Parse replay data into a struct that can be fed to the game in chunks
  // to avoid issues with all immediate orders being resolved on the first
  // tick. (上游注释 / upstream comment)
  std::vector<std::pair<int, std::vector<std::uint8_t>>> vec_packets;
  std::size_t pos = 0;
  while (pos < sz_body_end_) {
    if (pos + 8 > sz_body_end_)
      break;
    ByteReader head{vec_replay_bytes_.data() + pos, sz_body_end_ - pos};
    const std::int32_t int4_client = head.ReadInt32();
    if (int4_client == ReplayMetadata::kMetaStartMarker)
      break;

    const std::int32_t int4_packet_len = head.ReadInt32();
    pos += 8;
    if (int4_packet_len < 0 ||
        static_cast<std::uint64_t>(pos + static_cast<std::uint64_t>(
                                             int4_packet_len)) >
            sz_body_end_)
      break;
    std::vector<std::uint8_t> vec_packet(
        vec_replay_bytes_.begin() + static_cast<std::ptrdiff_t>(pos),
        vec_replay_bytes_.begin() +
            static_cast<std::ptrdiff_t>(pos + int4_packet_len));
    pos += static_cast<std::size_t>(int4_packet_len);

    std::int32_t int4_frame = 0;
    if (vec_packet.size() >= 4) {
      ByteReader frame_reader{vec_packet.data(), 4};
      int4_frame = frame_reader.ReadInt32();
    }
    vec_packets.emplace_back(int4_client, std::move(vec_packet));

    const std::vector<std::uint8_t>& vec_packet_kept = vec_packets.back().second;
    if (vec_packet_kept.size() > 4 &&
        (vec_packet_kept[4] ==
             static_cast<std::uint8_t>(OrderType::Disconnect) ||
         vec_packet_kept[4] ==
             static_cast<std::uint8_t>(OrderType::SyncHash)))
      continue;

    if (int4_frame == 0) {
      int int4_frame_parsed = 0;
      OrderPacket packet_orders;
      if (OrderIO::TryParseOrderPacket(vec_packet_kept, int4_frame_parsed,
                                       packet_orders)) {
        for (const auto& up_order :
             packet_orders.GetOrders(nullptr)) {
          if (up_order->str_order_string == "StartGame")
            b_valid_ = true;
          else if (up_order->str_order_string == "SyncInfo" && !b_valid_) {
            lobby_info_ = Session::Deserialize(
                up_order->str_target_string.value_or(""),
                up_order->str_order_string);
            b_has_lobby_info_ = true;
          }
        }
      }
    } else {
      Chunk chunk_next;
      chunk_next.frame = int4_frame;
      chunk_next.vec_packets = vec_packets;
      vec_packets.clear();
      queue_chunks_.push(std::move(chunk_next));

      int4_tick_count_ = std::max(int4_tick_count_, int4_frame);
    }
  }
}

int ReplayConnection::LocalClientId() { return -1; }

void ReplayConnection::StartGame() {}

void ReplayConnection::Send(int frame,
                            const std::vector<const Order*>& orders) {}

void ReplayConnection::SendImmediate(
    const std::vector<const Order*>& orders) {}

void ReplayConnection::SendSync(int frame, int sync_hash,
                                std::uint64_t uint8_defeat_state) {
  queue_sync_.push(SyncPacketData{frame, sync_hash, uint8_defeat_state});
}

void ReplayConnection::Receive(OrderManager& order_manager) {
  while (!queue_sync_.empty()) {
    order_manager.ReceiveSync(queue_sync_.front());
    queue_sync_.pop();
  }

  while (!queue_chunks_.empty() &&
         queue_chunks_.front().frame <=
             order_manager.NetFrameNumber() + int4_order_latency_) {
    Chunk chunk = std::move(queue_chunks_.front());
    queue_chunks_.pop();
    for (auto& [client_id, vec_packet] : chunk.vec_packets) {
      Packet packet{client_id, vec_packet};
      DisconnectData disconnect;
      SyncPacketData sync;
      int int4_frame = 0;
      OrderPacket packet_orders;
      if (OrderIO::TryParseDisconnect(packet, disconnect))
        order_manager.ReceiveDisconnect(disconnect.client_id, disconnect.frame);
      else if (OrderIO::TryParseSync(vec_packet, sync))
        order_manager.ReceiveSync(sync);
      else if (OrderIO::TryParseOrderPacket(vec_packet, int4_frame,
                                            packet_orders)) {
        if (int4_frame == 0)
          order_manager.ReceiveImmediateOrders(client_id, packet_orders);
        else
          order_manager.ReceiveOrders(client_id, int4_frame,
                                      std::move(packet_orders));
      } else
        throw std::runtime_error(
            "Received unknown packet from client " + std::to_string(client_id) +
            " with length " + std::to_string(vec_packet.size()));
    }
  }
}

}  // namespace ora::net
