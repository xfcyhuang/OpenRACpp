// UPSTREAM: OpenRA.Game/Network/ReplayRecorder.cs @b6fc03f L19-118 +
//           OpenRA.Game/FileFormats/ReplayMetadata.cs L17-108 +
//           OpenRA.Game/Network/ReplayConnection.cs L19-135(实现部分)
//           The implementation halves (the header note lives in
//           replay_recorder.hpp).
#include "net/replay_recorder.hpp"

#include "net/order.hpp"
#include "net/order_io.hpp"

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

// ———— ReplayConnection(起步面)————
// ———— ReplayConnection (the starter face) ————

ReplayConnection::ReplayConnection(std::vector<std::uint8_t> vec_replay_bytes)
    : vec_bytes_{std::move(vec_replay_bytes)} {
  // 上游 ctor:metadata 尾解析(录像主体 = 标记前的字节流)
  // The upstream ctor: the metadata tail parse (the replay body = the
  // bytes before the marker block).
  if (auto up_metadata = ReplayMetadata::Read(vec_bytes_); up_metadata) {
    up_metadata_ = std::move(up_metadata);
    // 主体截到 start 标记前(Read 的同式回退)
    // The body truncates ahead of the start marker (Read's back-step).
    const std::size_t sz_tail = vec_bytes_.size() - 8;
    ByteReader tail{vec_bytes_.data() + sz_tail, 8};
    const std::int32_t int4_data_length = tail.ReadInt32();
    const std::int64_t int8_back =
        4 + 4 + static_cast<std::int64_t>(int4_data_length) + 4 + 4;
    vec_bytes_.resize(vec_bytes_.size() -
                      static_cast<std::size_t>(int8_back));
  }
}

bool ReplayConnection::TryReadNext(
    int& out_client_id, std::vector<std::uint8_t>& out_data) {
  // 上游 ctor 的逐包读循环:clientID + length + data
  // Upstream's per-packet read loop: clientID + length + data.
  if (int8_cursor_ + 8 > vec_bytes_.size())
    return false;

  ByteReader reader{vec_bytes_.data() + int8_cursor_,
                    vec_bytes_.size() - int8_cursor_};
  try {
    out_client_id = reader.ReadInt32();
    const std::int32_t int4_length = reader.ReadInt32();
    if (static_cast<std::uint64_t>(int4_length) >
        static_cast<std::uint64_t>(reader.Remaining()))
      return false;
    out_data.clear();
    reader.ReadBytes(static_cast<std::size_t>(int4_length), out_data);
    int8_cursor_ += 8 + static_cast<std::size_t>(int4_length);
    return true;
  } catch (const std::exception&) {
    return false;
  }
}

}  // namespace ora::net
