#pragma once
import std;

#include "net/replay_recorder.hpp"

namespace ora::net {

struct ReplayPacketRecord {
  int int4_frame = 0;
  int int4_client_id = 0;
  std::vector<std::uint8_t> vec_data;
};

struct ReplaySyncRecord {
  int int4_frame = 0;
  int int4_sync_hash = 0;
  std::uint64_t uint8_defeat_state = 0;
};

struct ReplayFileData {
  std::vector<ReplayPacketRecord> vec_packets;
  std::vector<ReplaySyncRecord> vec_syncs;
  std::unique_ptr<ReplayMetadata> up_metadata;
  bool b_valid = false;
  int int4_tick_count = 0;
};

std::optional<ReplayFileData> ParseReplayFile(
    const std::vector<std::uint8_t>& vec_bytes);

struct ReplayDiffReport {
  bool b_equal = true;
  int int4_first_divergent_frame = -1;
  std::vector<std::string> vec_lines;
};

ReplayDiffReport DiffReplays(const ReplayFileData& replay_a,
                             const ReplayFileData& replay_b);

}  // namespace ora::net
