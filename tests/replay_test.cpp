import std;

#include "game/game.hpp"

#include "render_sequences_fixture.hpp"
#include "game/game_records.hpp"
#include "game/manifest.hpp"
#include "game/mod_data.hpp"
#include "map/map.hpp"
#include "map/map_cache.hpp"
#include "mods/body_orientation.hpp"
#include "mods/create_map_players.hpp"
#include "net/connection.hpp"
#include "net/order_io.hpp"
#include "net/order_manager.hpp"
#include "net/replay_diff.hpp"
#include "net/replay_recorder.hpp"
#include "net/unit_orders.hpp"
#include "sim/sync_hash.hpp"
#include "sim/trait_registry.hpp"
#include "sim/world.hpp"

#include <cstdio>

namespace ora::gen {
void RegisterGeneratedAll();
}

namespace testfx = ora::testfx;
namespace net = ora::net;
namespace game = ora::game;
namespace map = ora::map;
namespace gen = ora::gen;
namespace sim = ora::sim;
namespace mods = ora::mods;

static int g_failures = 0;

template <class T>
void CheckEq(const T& a, const T& b, std::string_view what) {
  if (!(a == b)) {
    std::println("FAIL: {} ({} != {})", what, a, b);
    ++g_failures;
  }
}

void Check(bool ok, std::string_view what) {
  if (!ok) {
    std::println("FAIL: {}", what);
    ++g_failures;
  }
}

namespace {

constexpr int kRandomSeed = 12345;
constexpr int kNetFrameInterval = 3;
constexpr int kFinalTick = 6;

class RecordingEchoConnection final : public net::IConnection {
 public:
  RecordingEchoConnection()
      : up_recorder_{std::make_unique<net::ReplayRecorder>(
            [] { return std::string{"replay_test"}; })} {}

  int LocalClientId() override { return 1; }

  void StartGame() override {
    echo_.StartGame();
    up_recorder_->Receive(1, net::OrderPacket{}.Serialize(1));
  }

  void Send(int frame,
            const std::vector<const net::Order*>& orders) override {
    echo_.Send(frame, orders);
    up_recorder_->Receive(
        1, net::OrderPacket{orders}.Serialize(frame + 1));
  }

  void SendImmediate(
      const std::vector<const net::Order*>& orders) override {
    echo_.SendImmediate(orders);
    up_recorder_->Receive(1, net::OrderPacket{orders}.Serialize(0));
  }

  void SendSync(int frame, int sync_hash,
                std::uint64_t uint8_defeat_state) override {
    echo_.SendSync(frame, sync_hash, uint8_defeat_state);
    up_recorder_->Receive(
        1, net::OrderIO::SerializeSync({frame, sync_hash, uint8_defeat_state}));
    vec_sync_frames.emplace_back(frame, sync_hash);
  }

  void Receive(net::OrderManager& order_manager) override {
    echo_.Receive(order_manager);
  }

  net::ReplayRecorder& Recorder() { return *up_recorder_; }

  const std::vector<std::pair<int, int>>& SyncFrames() const {
    return vec_sync_frames;
  }

 private:
  net::EchoConnection echo_;
  std::unique_ptr<net::ReplayRecorder> up_recorder_;
  std::vector<std::pair<int, int>> vec_sync_frames;
};

struct ReplayFixture {
  game::InstalledMods mods_installed;
  game::ModData mod_data;
  game::Game game_;
  std::string str_uid;
  std::string str_root;

  explicit ReplayFixture(const char* str_upstream_root)
      : mods_installed{std::string{str_upstream_root} + "/mods"},
        mod_data{*mods_installed.Find("ra"), mods_installed, str_upstream_root},
        game_{game::Game::Deps{
            .ptr_mod_data = &mod_data,
            .fn_prepare_map =
                [this](map::Map& map_world) {
                  testfx::InstallSyntheticSequences(
                      map_world, mod_data.ManifestRef(), mod_data,
                      str_root.c_str());
                }}}, str_root{str_upstream_root} {
    game_.MapCacheFace().LoadMaps(mod_data);
    for (map::MapPreview* p : game_.MapCacheFace().Previews()) {
      if (p->Status() == map::MapStatus::Available) {
        str_uid = p->Uid();
        break;
      }
    }
  }
};

bool TamperSyncHash(std::vector<std::uint8_t>& vec_bytes, int int4_frame,
                    std::size_t& sz_offset_out) {
  std::size_t pos = 0;
  while (pos + 8 <= vec_bytes.size()) {
    std::uint32_t uint4_client = 0;
    for (int i = 0; i < 4; ++i)
      uint4_client |= static_cast<std::uint32_t>(vec_bytes[pos + i]) << (8 * i);
    if (static_cast<std::int32_t>(uint4_client) ==
        net::ReplayMetadata::kMetaStartMarker)
      return false;

    std::uint32_t uint4_len = 0;
    for (int i = 0; i < 4; ++i)
      uint4_len |= static_cast<std::uint32_t>(vec_bytes[pos + 4 + i]) << (8 * i);
    if (pos + 8 + uint4_len > vec_bytes.size())
      return false;

    const std::size_t sz_packet = pos + 8;
    if (uint4_len == 17 && vec_bytes[sz_packet + 4] == 0x65) {
      std::int32_t int4_frame_stored = 0;
      for (int i = 0; i < 4; ++i)
        int4_frame_stored |= static_cast<std::int32_t>(
            vec_bytes[sz_packet + i] << (8 * i));
      if (int4_frame_stored == int4_frame) {
        sz_offset_out = sz_packet + 5;
        vec_bytes[sz_offset_out] ^= 0xFF;
        return true;
      }
    }
    pos = sz_packet + uint4_len;
  }
  return false;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 2) {
    std::println("usage: replay_test <upstream-root>");
    return 1;
  }
  setvbuf(stdout, nullptr, _IOLBF, 0);
  const char* str_root = argv[1];
  gen::RegisterGeneratedAll();
  game::RegisterGameLoaders();
  sim::RegisterWorldTraits();
  mods::RegisterCommonTraits();
  mods::SetBodyOrientationFacingsResolver(
      [](const game::ActorInfo&, const std::string&) { return 8; });

  std::vector<std::uint8_t> vec_replay;
  int int4_last_recorded_hash = 0;
  std::size_t sz_actors_recorded = 0;
  int int4_tick_count_recorded = 0;
  std::string str_recorded_uid;

  {
    ReplayFixture fx{str_root};
    Check(!fx.str_uid.empty(), "map found");
    if (!fx.str_uid.empty()) {
      auto up_conn = std::make_unique<RecordingEchoConnection>();
      RecordingEchoConnection* p_conn = up_conn.get();
      auto up_om = std::make_unique<net::OrderManager>(std::move(up_conn));
      net::OrderManager* p_om = up_om.get();
      fx.game_.JoinInner(std::move(up_om));

      net::SessionClient client_host{};
      client_host.Index = 1;
      client_host.Name = "Tester";
      client_host.Faction = "Random";
      client_host.State = net::ClientState::Ready;
      p_om->LobbyInfo().vec_clients.push_back(client_host);
      p_om->LobbyInfo().global_settings.RandomSeed = kRandomSeed;
      p_om->LobbyInfo().global_settings.NetFrameInterval = kNetFrameInterval;
      p_om->LobbyInfo().global_settings.Map = fx.str_uid;

      p_om->IssueOrder(net::Order::FromTargetString(
          "SyncInfo", p_om->LobbyInfo().Serialize(), true));
      net::Order order_start{"StartGame", nullptr, false};
      order_start.b_is_immediate = true;
      p_om->IssueOrder(order_start);

      int int4_guard = 0;
      while (p_om->NetFrameNumber() <= kFinalTick && int4_guard++ < 5000) {
        p_om->TickImmediate();
        if (p_om->World() != nullptr &&
            p_om->NetFrameNumber() % 7 == 0) {
          net::Order order_chat{"Chat", nullptr, false};
          order_chat.str_target_string =
              std::format("tick {}", p_om->NetFrameNumber());
          p_om->IssueOrder(order_chat);
        }
        if (p_om->TryTick() && p_om->World() != nullptr)
          p_om->World()->Tick();
      }

      Check(p_om->World() != nullptr, "recorded world assembled");
      CheckEq(p_om->NetFrameNumber(), kFinalTick + 1, "recorded final frame");
      CheckEq(p_om->Connection().LocalClientId(), 1, "echo local client");
      CheckEq(p_conn->SyncFrames().size(), std::size_t{kFinalTick},
              "sync packets recorded");
      CheckEq(p_om->LobbyInfo().global_settings.RandomSeed, kRandomSeed,
              "seed survives SyncInfo round-trip");

      int4_last_recorded_hash = p_conn->SyncFrames().back().second;
      sz_actors_recorded = p_om->World()->Actors().size();
      int4_tick_count_recorded = kFinalTick + 1;
      str_recorded_uid = fx.str_uid;

      net::ReplayMetadata meta_final{
          std::format("FinalGameTick: {}", kFinalTick)};
      p_conn->Recorder().Metadata = &meta_final;
      p_conn->Recorder().Dispose();
      vec_replay = p_conn->Recorder().Bytes();

      const auto parsed = net::ParseReplayFile(vec_replay);
      Check(parsed.has_value(), "recorded replay parses");
      if (parsed.has_value()) {
        Check(parsed->b_valid, "recorded replay valid");
        CheckEq(parsed->vec_syncs.size(), std::size_t{kFinalTick},
                "parsed sync count");
        CheckEq(parsed->vec_syncs.back().int4_sync_hash,
                int4_last_recorded_hash, "parsed last hash");
        const net::ReplayDiffReport report_self =
            net::DiffReplays(*parsed, *parsed);
        Check(report_self.b_equal, "recorded replay self-diff equal");
      }
    }
  }

  Check(!vec_replay.empty(), "replay bytes produced");
  if (!vec_replay.empty()) {
    net::ReplayConnection connection_probe{vec_replay};
    Check(connection_probe.IsValid(), "ctor StartGame scan");
    Check(connection_probe.HasLobbyInfo(), "ctor SyncInfo scan");
    CheckEq(connection_probe.LobbyInfo().global_settings.RandomSeed,
            kRandomSeed, "ctor lobby seed");
    CheckEq(connection_probe.LobbyInfo().global_settings.Map,
            str_recorded_uid, "ctor lobby map uid");
    CheckEq(connection_probe.TickCount(), int4_tick_count_recorded,
            "ctor tick count");
    CheckEq(connection_probe.FinalGameTick(), kFinalTick,
            "ctor final game tick");
  }

  if (!vec_replay.empty()) {
    ReplayFixture fx{str_root};
    fx.game_.JoinReplay(vec_replay, kNetFrameInterval);
    net::OrderManager* p_om = fx.game_.OrderManagerFace();
    Check(p_om != nullptr, "replay order manager");
    Check(p_om->Connection().IsReplay(), "connection is replay");
    CheckEq(p_om->Connection().ReplayTickCount(), int4_tick_count_recorded,
            "connection tick count");

    p_om->TickImmediate();
    Check(p_om->World() != nullptr, "replay world assembled");
    Check(p_om->World()->IsReplay(), "world IsReplay");
    if (p_om->World() != nullptr) {
      CheckEq(p_om->World()->Actors().size(), sz_actors_recorded,
              "replay actor count parity");
      CheckEq(p_om->LobbyInfo().global_settings.RandomSeed, kRandomSeed,
              "replay lobby seed via SyncInfo");

      int int4_guard = 0;
      while (p_om->NetFrameNumber() <= kFinalTick && int4_guard++ < 5000) {
        p_om->TickImmediate();
        if (!p_om->TryTick())
          continue;
        p_om->World()->Tick();
        if (p_om->IsOutOfSync())
          break;
      }
      p_om->TickImmediate();

      CheckEq(p_om->NetFrameNumber(), kFinalTick + 1, "replay final frame");
      Check(!p_om->IsOutOfSync(), "replay stays in sync");
      CheckEq(p_om->World()->SyncHash(), int4_last_recorded_hash,
              "final SyncHash parity");
    }
  }

  if (!vec_replay.empty()) {
    std::size_t sz_tampered = 0;
    const int int4_tamper_frame = kFinalTick - 5;
    Check(TamperSyncHash(vec_replay, int4_tamper_frame, sz_tampered),
          "tamper offset located");
    if (sz_tampered != 0) {
      ReplayFixture fx{str_root};
      fx.game_.JoinReplay(vec_replay, kNetFrameInterval);
      net::OrderManager* p_om = fx.game_.OrderManagerFace();

      int int4_guard = 0;
      while (p_om->NetFrameNumber() <= kFinalTick && int4_guard++ < 5000) {
        p_om->TickImmediate();
        if (p_om->World() == nullptr)
          continue;
        if (!p_om->TryTick())
          continue;
        p_om->World()->Tick();
        if (p_om->IsOutOfSync())
          break;
      }
      p_om->TickImmediate();
      Check(p_om->IsOutOfSync(), "tampered replay desyncs");
    }
  }

  if (g_failures == 0)
    std::println("replay_test: all passed");
  else
    std::println("replay_test: {} FAILURES", g_failures);
  return g_failures == 0 ? 0 : 1;
}
