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
#include "mods/production.hpp"
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

struct GoldenReplayFixture {
  game::InstalledMods mods_installed;
  game::ModData mod_data;
  game::Game game_;
  std::string str_root;

  explicit GoldenReplayFixture(const char* str_upstream_root)
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
    game_.MapCacheFace().LoadMaps(mod_data, "great-sahara-2");
  }
};

struct GoldenTable {
  int int4_frames = 0;
  int int4_seed = 0;
  int int4_net_frame_interval = 0;
  std::size_t sz_actors = 0;
  std::vector<std::pair<int, int>> vec_syncs;
};

std::optional<GoldenTable> ReadGoldenTable(const char* str_path) {
  std::FILE* f_in = std::fopen(str_path, "rb");
  if (f_in == nullptr)
    return std::nullopt;
  std::string str_content;
  char buf_ch[4096];
  std::size_t sz_read = 0;
  while ((sz_read = std::fread(buf_ch, 1, sizeof buf_ch, f_in)) > 0)
    str_content.append(buf_ch, sz_read);
  std::fclose(f_in);

  GoldenTable table{};
  std::size_t pos = 0;
  while (pos < str_content.size()) {
    std::size_t eol = str_content.find('\n', pos);
    if (eol == std::string::npos)
      eol = str_content.size();
    std::string_view line{str_content.data() + pos, eol - pos};
    pos = eol + 1;
    if (line.starts_with("# uid=")) {
      line.remove_prefix(6);
      auto grab = [&line](std::string_view key) -> int {
        std::size_t at = line.find(key);
        if (at == std::string_view::npos)
          return -1;
        at += key.size();
        std::size_t end = line.find(' ', at);
        if (end == std::string_view::npos)
          end = line.size();
        int value = 0;
        std::from_chars(line.data() + at, line.data() + end, value);
        return value;
      };
      table.int4_frames = grab("frames=");
      table.int4_seed = grab("seed=");
      table.int4_net_frame_interval = grab("netFrameInterval=");
      table.sz_actors = static_cast<std::size_t>(grab("actors="));
    } else if (line.starts_with("F ")) {
      int frame = 0;
      int hash = 0;
      std::from_chars_result r1 = std::from_chars(
          line.data() + 2, line.data() + line.size(), frame);
      if (r1.ec != std::errc{})
        return std::nullopt;
      std::size_t h_at = line.find("H ") + 2;
      std::from_chars(line.data() + h_at, line.data() + line.size(), hash);
      table.vec_syncs.emplace_back(frame, hash);
    }
  }
  return table;
}

}

int main(int argc, char** argv) {
  if (argc < 4) {
    std::println("usage: golden_replay_test <upstream-root> <replay> <table>");
    return 1;
  }

  setvbuf(stdout, nullptr, _IONBF, 0);
  const char* str_root = argv[1];
  const char* str_replay_path = argv[2];
  const char* str_table_path = argv[3];
  gen::RegisterGeneratedAll();
  game::RegisterGameLoaders();
  sim::RegisterWorldTraits();
  mods::RegisterCommonTraits();

  mods::SetBodyOrientationFacingsResolver(
      [](const game::ActorInfo& ai, const std::string& str_faction) {
        return testfx::ResolveGoldenFacings(ai, str_faction);
      });

  std::vector<std::uint8_t> vec_replay;
  {
    std::FILE* f_in = std::fopen(str_replay_path, "rb");
    Check(f_in != nullptr, "golden replay file opens");
    if (f_in == nullptr) {
      std::println("golden_replay_test: {} FAILURES (no replay)", g_failures);
      return 1;
    }
    std::uint8_t buf_u1[8192];
    std::size_t sz_read = 0;
    while ((sz_read = std::fread(buf_u1, 1, sizeof buf_u1, f_in)) > 0)
      vec_replay.insert(vec_replay.end(), buf_u1, buf_u1 + sz_read);
    std::fclose(f_in);
  }

  const std::optional<GoldenTable> table_opt = ReadGoldenTable(str_table_path);
  Check(table_opt.has_value(), "golden table parses");
  if (!table_opt.has_value()) {
    std::println("golden_replay_test: {} FAILURES (no table)", g_failures);
    return 1;
  }
  const GoldenTable& table = *table_opt;

  const auto parsed = net::ParseReplayFile(vec_replay);
  Check(parsed.has_value(), "C# replay parses");
  Check(parsed->b_valid, "C# replay valid");

  int int4_first_divergence = 0;
  if (parsed.has_value()) {
    CheckEq(parsed->vec_syncs.size(), table.vec_syncs.size(),
            "replay sync count == table sync count");
    const std::size_t n = std::min(parsed->vec_syncs.size(),
                                   table.vec_syncs.size());
    for (std::size_t i = 0; i < n; ++i) {
      if (parsed->vec_syncs[i].int4_sync_hash !=
          table.vec_syncs[i].second) {
        int4_first_divergence = table.vec_syncs[i].first;
        break;
      }
    }
    CheckEq(int4_first_divergence, 0,
            "replay file hashes == golden table (self-consistent)");
  }

  {
    std::println("[golden-replay] fixture + JoinReplay");
    GoldenReplayFixture fx{str_root};
    fx.game_.JoinReplay(vec_replay,
                        table.int4_net_frame_interval > 0
                            ? table.int4_net_frame_interval
                            : 3);
    net::OrderManager* p_om = fx.game_.OrderManagerFace();
    Check(p_om != nullptr, "replay order manager");
    Check(p_om->Connection().IsReplay(), "connection is replay");
    CheckEq(p_om->Connection().ReplayTickCount(),
            table.int4_frames + 1, "replay tick count");

    p_om->TickImmediate();
    Check(p_om->World() != nullptr, "C# replay world assembled");
    Check(p_om->World()->IsReplay(), "world IsReplay");
    if (p_om->World() != nullptr) {
      CheckEq(p_om->World()->Actors().size(), table.sz_actors,
              "C# replay actor count parity");
      CheckEq(p_om->LobbyInfo().global_settings.RandomSeed,
              table.int4_seed, "C# lobby seed via SyncInfo");
      CheckEq(p_om->LobbyInfo().global_settings.NetFrameInterval,
              table.int4_net_frame_interval, "C# lobby net frame interval");

      int int4_hash_at_last_net_frame = 0;
      int int4_diverge_frame = 0;
      int int4_diverge_local = 0;
      int int4_diverge_recorded = 0;
      std::println("[golden-replay] frame-by-frame sync compare");
      int int4_guard = 0;
      while (p_om->NetFrameNumber() <= table.int4_frames &&
             int4_guard++ < 10000) {
        p_om->TickImmediate();
        const int int4_frame_before = p_om->NetFrameNumber();
        if (!p_om->TryTick())
          continue;

        if (p_om->NetFrameNumber() != int4_frame_before &&
            p_om->NetFrameNumber() % 25 == 1)
          std::println("[golden-replay] net frame {}/{}",
                       p_om->NetFrameNumber() - 1, table.int4_frames);
        if (p_om->NetFrameNumber() != int4_frame_before) {
          const int int4_frame_just_processed =
              p_om->NetFrameNumber() - 1;
          if (int4_frame_just_processed >= 1 &&
              static_cast<std::size_t>(int4_frame_just_processed) <=
                  table.vec_syncs.size()) {
            const int int4_local_hash = p_om->World()->SyncHash();
            const auto [int4_rec_frame, int4_rec_hash] =
                table.vec_syncs[static_cast<std::size_t>(
                    int4_frame_just_processed - 1)];
            int4_hash_at_last_net_frame = int4_local_hash;
            if (int4_diverge_frame == 0 &&
                int4_local_hash != int4_rec_hash) {
              int4_diverge_frame = int4_rec_frame;
              int4_diverge_local = int4_local_hash;
              int4_diverge_recorded = int4_rec_hash;
            }
          }
        }
        p_om->World()->Tick();
        if (p_om->IsOutOfSync())
          break;
      }
      p_om->TickImmediate();

      CheckEq(p_om->NetFrameNumber(), table.int4_frames + 1,
              "C# replay final frame");
      Check(!p_om->IsOutOfSync(), "C# replay stays in sync (G2 gate)");
      CheckEq(int4_diverge_frame, 0, "C++ local SyncHash == C# golden per frame");
      if (int4_diverge_frame != 0)
        std::println(
            "  first divergent frame {}: local {} vs C# recorded {}",
            int4_diverge_frame, int4_diverge_local, int4_diverge_recorded);

      CheckEq(int4_hash_at_last_net_frame,
              table.vec_syncs.back().second,
              "C# replay final SyncHash parity");
    }
  }

  if (g_failures == 0)
    std::println("golden_replay_test: all passed");
  else
    std::println("golden_replay_test: {} FAILURES", g_failures);
  return g_failures == 0 ? 0 : 1;
}
