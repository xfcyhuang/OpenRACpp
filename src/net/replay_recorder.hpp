#pragma once
import std;

#include "net/byte_io.hpp"
#include "net/connection.hpp"
#include "net/session.hpp"

namespace ora::net {

class ReplayMetadata {
 public:
  static constexpr int kMetaStartMarker = -1;
  static constexpr int kMetaEndMarker = -2;
  static constexpr int kMetaVersion = 0x00000001;

  explicit ReplayMetadata(std::string str_game_info_data)
      : str_game_info_data_{std::move(str_game_info_data)} {}
  ReplayMetadata() = default;

  const std::string& GameInfoData() const { return str_game_info_data_; }

  const std::string& FilePath() const { return str_file_path_; }
  void SetFilePath(std::string str_path) {
    str_file_path_ = std::move(str_path);
  }

  bool RenameFile(const std::string& str_new_filename_without_extension);

  void Write(std::vector<std::uint8_t>& vec_out) const;

  static std::unique_ptr<ReplayMetadata> Read(
      const std::vector<std::uint8_t>& vec_bytes);

  static std::unique_ptr<ReplayMetadata> ReadFile(const std::string& str_path);

 private:
  std::string str_game_info_data_;
  std::string str_file_path_;
};

class ReplayRecorder {
 public:

  using ChooseFilename = std::function<std::string()>;

  static constexpr int kCreateReplayFileMaxRetryCount = 128;

  explicit ReplayRecorder(ChooseFilename fn_choose_filename);

  void Receive(int client_id, const std::vector<std::uint8_t>& vec_data);

  void ReceiveFrame(int client_id, int frame,
                    const std::vector<std::uint8_t>& vec_data);

  void Dispose();

  ~ReplayRecorder() { Dispose(); }

  ReplayMetadata* Metadata = nullptr;

  const std::vector<std::uint8_t>& Bytes() const { return vec_bytes_; }

 private:

  static bool IsGameStart(const std::vector<std::uint8_t>& vec_data);

  void StartSavingReplay(const std::vector<std::uint8_t>& vec_initial);

  ChooseFilename fn_choose_filename_;
  std::vector<std::uint8_t> vec_bytes_;
  bool b_started_saving_ = false;
  std::string str_filename_;
  bool b_disposed_ = false;
};

class ReplayConnection final : public IConnection {
 public:
  struct Chunk {
    int frame = 0;
    std::vector<std::pair<int, std::vector<std::uint8_t>>> vec_packets;
  };

  explicit ReplayConnection(std::vector<std::uint8_t> vec_replay_bytes,
                            int int4_order_latency = 3);

  int LocalClientId() override;
  void StartGame() override;
  void Send(int frame, const std::vector<const Order*>& orders) override;
  void SendImmediate(const std::vector<const Order*>& orders) override;
  void SendSync(int frame, int sync_hash,
                std::uint64_t uint8_defeat_state) override;
  void Receive(OrderManager& order_manager) override;
  bool IsReplay() const override { return true; }
  int ReplayTickCount() const override { return int4_tick_count_; }

  bool IsValid() const { return b_valid_; }

  bool HasLobbyInfo() const { return b_has_lobby_info_; }
  const Session& LobbyInfo() const { return lobby_info_; }

  int TickCount() const { return int4_tick_count_; }
  int FinalGameTick() const { return int4_final_game_tick_; }
  const ReplayMetadata* Metadata() const { return up_metadata_.get(); }

  const std::vector<std::uint8_t>& ReplayBytes() const {
    return vec_replay_bytes_;
  }

 private:
  std::vector<std::uint8_t> vec_replay_bytes_;
  std::size_t sz_body_end_ = 0;
  std::unique_ptr<ReplayMetadata> up_metadata_;
  std::queue<Chunk> queue_chunks_;
  std::queue<SyncPacketData> queue_sync_;
  int int4_order_latency_ = 3;
  int int4_tick_count_ = 0;
  int int4_final_game_tick_ = 0;
  bool b_valid_ = false;
  bool b_has_lobby_info_ = false;
  Session lobby_info_{};
};

}
