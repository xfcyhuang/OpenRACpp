// UPSTREAM: OpenRA.Game/Network/ReplayRecorder.cs @b6fc03f L19-118 全文 +
//           OpenRA.Game/FileFormats/ReplayMetadata.cs L17-108 全文 +
//           OpenRA.Game/Network/ReplayConnection.cs L19-135 的起步承载面
//           The whole of ReplayRecorder.cs L19-118 + ReplayMetadata.cs
//           L17-108 + the starter face of ReplayConnection.cs L19-135.
//
// 机制对照 / Mechanism mapping:
//  - GameInformation 的完整序列化(ReplayMetadata 载荷)随 Phase 7 大厅
//    协议 —— 承载面 = 版本/标记/长度前缀协议全保真,载荷串以注入面出入
//    GameInformation's full serialization (ReplayMetadata's payload) rides
//    Phase 7's lobby protocol — the carrier keeps the marker/version/
//    length-prefix protocol fully faithful, the payload string passes
//    through an injection face.
//  - preStartBuffer(MemoryStream)→ vector<uint8_t> 追加缓冲(writer 的
//    clientID/length/data 三段写 = 同一序)
//    The preStartBuffer (a MemoryStream) → an append buffer (the writer's
//    clientID/length/data three-part writes keep the same order).
//  - Platform.SupportDir/ModData.Manifest 的目录解析 → chooseFilename
//    回调 + 目录参数(引擎装配面注入)
//    The Platform.SupportDir/ModData.Manifest directory resolution → the
//    chooseFilename callback + a directory parameter (injected by the
//    engine assembly).
#pragma once
import std;

#include "net/byte_io.hpp"

namespace ora::net {

/// ReplayMetadata(ReplayMetadata.cs L20-108)
/// ReplayMetadata (ReplayMetadata.cs L20-108).
class ReplayMetadata {
 public:
  static constexpr int kMetaStartMarker = -1;  // L24(无效 client 值域)
  static constexpr int kMetaEndMarker = -2;    // L25
  static constexpr int kMetaVersion = 0x00000001;  // L26

  /// GameInformation 载荷的注入面(Phase 7 的序列化面接入前的等价物)
  /// The GameInformation payload's injection face (the equivalent until
  /// Phase 7's serialization lands).
  explicit ReplayMetadata(std::string str_game_info_data)
      : str_game_info_data_{std::move(str_game_info_data)} {}
  ReplayMetadata() = default;

  const std::string& GameInfoData() const { return str_game_info_data_; }

  /// Write(L51-67):start 标记 + 版本 + 长度前缀载荷 + 总长 + end 标记
  /// Write (L51-67): the start marker + version + the length-prefixed
  /// payload + the total length + the end marker.
  void Write(std::vector<std::uint8_t>& vec_out) const;

  /// Read(L79-106):尾扫 end 标记;短/无标记/异常 → nullptr
  /// Read (L79-106): the tail scan for the end marker; short/missing/
  /// exceptional files yield nullptr.
  static std::unique_ptr<ReplayMetadata> Read(
      const std::vector<std::uint8_t>& vec_bytes);

 private:
  std::string str_game_info_data_;
};

/// ReplayRecorder(ReplayRecorder.cs L22-117)
/// ReplayRecorder (ReplayRecorder.cs L22-117).
class ReplayRecorder {
 public:
  /// 文件名选择回调(上游 Func<string> chooseFilename;目录 = 调用方)
  /// The filename chooser (upstream's Func<string> chooseFilename; the
  /// directory is the caller's).
  using ChooseFilename = std::function<std::string()>;

  static constexpr int kCreateReplayFileMaxRetryCount = 128;  // L24

  explicit ReplayRecorder(ChooseFilename fn_choose_filename);

  /// Receive(L74-90):clientID + length + data 三段写;开局首包切盘
  /// Receive (L74-90): the clientID + length + data writes; the game-start
  /// packet switches to the file sink.
  void Receive(int client_id, const std::vector<std::uint8_t>& vec_data);

  /// ReceiveFrame(L92-98):frame 前缀包
  /// ReceiveFrame (L92-98): the frame-prefixed packet.
  void ReceiveFrame(int client_id, int frame,
                    const std::vector<std::uint8_t>& vec_data);

  void Dispose();

  ~ReplayRecorder() { Dispose(); }

  ReplayMetadata* Metadata = nullptr;  // L26(所有权 = 调用方)

  /// 内存盘内容(测试/装配面;文件落盘随 Game 装配批)
  /// The in-memory sink contents (the test/assembly face; the on-disk
  /// flush rides the Game-assembly batch).
  const std::vector<std::uint8_t>& Bytes() const { return vec_bytes_; }

 private:
  /// IsGameStart(L29-34):frame 0 且任一 order 为 "StartGame"
  /// IsGameStart (L29-34): frame 0 with any order being "StartGame".
  static bool IsGameStart(const std::vector<std::uint8_t>& vec_data);

  /// StartSavingReplay(L44-72):preStart 缓冲 → 内存盘(vec_bytes_ 的
  /// 追加;文件落盘由 FlushToFile 承载)
  /// StartSavingReplay (L44-72): the pre-start buffer → the in-memory
  /// sink (appended into vec_bytes_; the on-disk flush rides
  /// FlushToFile).
  void StartSavingReplay(const std::vector<std::uint8_t>& vec_initial);

  ChooseFilename fn_choose_filename_;
  std::vector<std::uint8_t> vec_bytes_;
  bool b_started_saving_ = false;  // preStartBuffer == null 的判别
  std::string str_filename_;
  bool b_disposed_ = false;
};

/// ReplayConnection(ReplayConnection.cs L21-134)的起步面:回放文件 →
/// 逐包 (clientID, data) 重放(录像驱动世界 = OrderManager 装配)
/// The starter face of ReplayConnection (ReplayConnection.cs L21-134):
/// the replay file → the per-packet (clientID, data) replay (driving the
/// world = the OrderManager assembly).
class ReplayConnection {
 public:
  /// ctor(metadata 解析 + 录像流装载)
  /// The ctor (the metadata parse + the replay stream load).
  explicit ReplayConnection(std::vector<std::uint8_t> vec_replay_bytes);

  /// 下一包;耗尽 → false
  /// The next packet; false when drained.
  bool TryReadNext(int& out_client_id, std::vector<std::uint8_t>& out_data);

  const ReplayMetadata* Metadata() const { return up_metadata_.get(); }

 private:
  std::vector<std::uint8_t> vec_bytes_;
  std::size_t int8_cursor_ = 0;
  std::unique_ptr<ReplayMetadata> up_metadata_;
};

}  // namespace ora::net
