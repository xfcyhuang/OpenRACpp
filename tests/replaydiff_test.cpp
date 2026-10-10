import std;

#include "net/order.hpp"
#include "net/order_io.hpp"
#include "net/replay_diff.hpp"
#include "net/replay_recorder.hpp"

namespace net = ora::net;

namespace {


int g_failures = 0;

void Check(bool b_ok, std::string_view str_msg) {
  if (!b_ok) {
    ++g_failures;
    std::println("FAIL: {}", str_msg);
  }
}

void CheckEq(auto a, auto b, std::string_view str_msg) {
  if (!(a == b)) {
    ++g_failures;
    std::println("FAIL: {} ({} != {})", str_msg, a, b);
  }
}

std::vector<std::uint8_t> FramePacket(std::int32_t int4_frame,
                                      const net::OrderPacket& packet) {
  const std::vector<std::uint8_t> vec_payload = packet.Serialize(int4_frame);
  return vec_payload;
}

void AppendPacket(std::vector<std::uint8_t>& vec_out, std::int32_t int4_client,
                  const std::vector<std::uint8_t>& vec_data) {
  for (int i = 0; i < 4; ++i)
    vec_out.push_back(static_cast<std::uint8_t>(
        (static_cast<std::uint32_t>(int4_client) >> (8 * i)) & 0xFF));
  const std::uint32_t uint4_len = static_cast<std::uint32_t>(vec_data.size());
  for (int i = 0; i < 4; ++i)
    vec_out.push_back(
        static_cast<std::uint8_t>((uint4_len >> (8 * i)) & 0xFF));
  vec_out.insert(vec_out.end(), vec_data.begin(), vec_data.end());
}

std::vector<std::uint8_t> MakeReplay(const std::vector<std::int32_t>& vec_hashes,
                                     bool b_drop_order) {
  const net::Order order_start{"StartGame", nullptr, false};
  const net::OrderPacket packet_start{std::vector<const net::Order*>{
      &order_start}};

  const net::Order order_move{"Move", nullptr, false};
  const net::OrderPacket packet_move{std::vector<const net::Order*>{
      &order_move}};

  std::vector<std::uint8_t> vec_bytes;
  AppendPacket(vec_bytes, 1, FramePacket(0, packet_start));
  if (!b_drop_order)
    AppendPacket(vec_bytes, 2, FramePacket(1, packet_move));

  for (std::size_t i = 0; i < vec_hashes.size(); ++i) {
    const net::SyncPacketData sync{
        .frame = static_cast<std::int32_t>(i + 1),
        .sync_hash = vec_hashes[i],
        .uint8_defeat_state = 0};
    AppendPacket(vec_bytes, 1, net::OrderIO::SerializeSync(sync));
  }

  net::ReplayMetadata metadata{"game-info-payload"};
  metadata.Write(vec_bytes);
  return vec_bytes;
}

}  // namespace

int main() {
  const std::vector<std::int32_t> vec_hashes{100, 200, 300, 400};
  const std::vector<std::uint8_t> vec_a = MakeReplay(vec_hashes, false);

  const auto replay_a = net::ParseReplayFile(vec_a);
  Check(replay_a.has_value(), "parse A");
  if (replay_a.has_value()) {
    Check(replay_a->b_valid, "StartGame marks replay valid");
    CheckEq(replay_a->vec_packets.size(),
            std::size_t{2 + vec_hashes.size()}, "packet count");
    CheckEq(replay_a->vec_syncs.size(), vec_hashes.size(), "sync count");
    CheckEq(replay_a->int4_tick_count, 1, "tick count");
    Check(replay_a->up_metadata != nullptr, "metadata read");
    Check(replay_a->up_metadata->GameInfoData() == "game-info-payload",
          "metadata payload");
    CheckEq(replay_a->vec_syncs[2].int4_sync_hash, 300, "sync hash value");

    const net::ReplayDiffReport report_self =
        net::DiffReplays(*replay_a, *replay_a);
    Check(report_self.b_equal, "self diff equal");

    std::vector<std::int32_t> vec_hashes_bad = vec_hashes;
    vec_hashes_bad[2] = 301;
    const auto replay_bad = net::ParseReplayFile(
        MakeReplay(vec_hashes_bad, false));
    Check(replay_bad.has_value(), "parse B");
    const net::ReplayDiffReport report_hash =
        net::DiffReplays(*replay_a, *replay_bad);
    Check(!report_hash.b_equal, "hash tamper detected");
    CheckEq(report_hash.int4_first_divergent_frame, 3,
            "first divergent frame");
    Check(!report_hash.vec_lines.empty(), "hash diff lines");

    const auto replay_short = net::ParseReplayFile(
        MakeReplay(vec_hashes, true));
    Check(replay_short.has_value(), "parse C");
    const net::ReplayDiffReport report_drop =
        net::DiffReplays(*replay_a, *replay_short);
    Check(!report_drop.b_equal, "dropped order packet detected");
    CheckEq(report_drop.int4_first_divergent_frame, 1,
            "drop divergent frame");

    net::ReplayConnection connection{vec_a};
    Check(connection.IsValid(), "ReplayConnection StartGame valid");
    Check(!connection.HasLobbyInfo(), "ReplayConnection no SyncInfo");
    CheckEq(connection.TickCount(), replay_a->int4_tick_count,
            "ReplayConnection tick count parity");
  }

  if (g_failures == 0)
    std::println("replaydiff_test: all passed");
  else
    std::println("replaydiff_test: {} FAILURES", g_failures);
  return g_failures == 0 ? 0 : 1;
}
