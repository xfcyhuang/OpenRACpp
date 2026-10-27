import std;

namespace ora::gen {
void RegisterGeneratedAll();
}

#include "core/polygon.hpp"
#include "fs/folder.hpp"
#include "game/game.hpp"
#include "game/mod_data.hpp"
#include "game/platform.hpp"
#include "game/web_services.hpp"
#include "map/map_cache.hpp"
#include "server/map_status_cache.hpp"
#include "map/cell_layer.hpp"
#include "map/cell_region.hpp"
#include "map/map.hpp"
#include "platform/tcp_socket.hpp"
#include "sim/selection.hpp"
#include "sim/trait_registry.hpp"
#include "sim/spatially_partitioned.hpp"
#include "mods/create_map_players.hpp"
#include "sim/world.hpp"
#include <boost/hash2/sha1.hpp>
#include <cstdio>
#ifdef GetMessage
#undef GetMessage
#endif

using namespace ora;
using namespace ora::map;
using namespace ora::sim;

int g_failures = 0;
void Check(bool ok, const std::string& what) {
  if (ok)
    std::println("ok: {}", what);
  else {
    ++g_failures;
    std::println("FAIL: {}", what);
  }
}
template <class A, class B>
void CheckEq(const A& a, const B& b, const std::string& what) {
  Check(a == b, std::format("{} ({} == {})", what, a, b));
}

void TestSha1() {
  const auto hex_digest = [](const boost::hash2::digest<20>& d) {
    std::string out;
    for (std::size_t st_i = 0; st_i < d.size(); ++st_i) {
      const unsigned char b = d.data()[st_i];
      out += "0123456789abcdef"[b >> 4];
      out += "0123456789abcdef"[b & 0xF];
    }
    return out;
  };
  const auto hex = [&](std::span<const unsigned char> bytes) {
    boost::hash2::sha1_160 hasher;
    hasher.update(bytes.data(), bytes.size());
    return hex_digest(hasher.result());
  };
  CheckEq(hex({}), "da39a3ee5e6b4b0d3255bfef95601890afd80709", "SHA1('')");
  const std::string abc = "abc";
  CheckEq(hex(std::span<const unsigned char>(
              reinterpret_cast<const unsigned char*>(abc.data()), 3)),
          "a9993e364706816aba3e25717850c26c9cd0d89d", "SHA1('abc')");

  const std::string az = "abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz";
  const std::span<const unsigned char> az_bytes{
      reinterpret_cast<const unsigned char*>(az.data()), az.size()};
  boost::hash2::sha1_160 split;
  split.update(az_bytes.data(), 26);
  split.update(az_bytes.data() + 26, az_bytes.size() - 26);
  Check(hex_digest(split.result()) == hex(az_bytes),
        "streaming == one-shot digest");
}

void TestCellRegions() {

  CellRegion region{MapGridType::Rectangular, CPos{2, 1}, CPos{3, 2}};
  std::vector<CPos> seen;
  for (const CPos c : region)
    seen.push_back(c);
  CheckEq(seen.size(), std::size_t{4}, "CellRegion enumerates 4 cells");
  CheckEq(seen[0].X(), 2, "row-major order: first x=2,y=1");
  CheckEq(seen[1].X(), 3, "row-major order: second x=3,y=1");
  CheckEq(seen[2].Y(), 2, "row-major order: third y=2");

  Check(region.Contains(CPos{3, 2}), "Contains inside");
  Check(!region.Contains(CPos{4, 2}), "Contains outside");

  CellRegion from_map{MapGridType::Rectangular, ora::MPos{2, 1},
                      ora::MPos{3, 2}};
  Check(from_map.Contains(CPos{2, 1}), "MPos ctor equality");

  const CellRegion expanded = CellRegion::Expand(region, 1);
  Check(expanded.Contains(CPos{1, 0}), "Expand grows the cordon");

  bool threw = false;
  try {
    (void)CellRegion::BoundingRegion(MapGridType::Rectangular, {});
  } catch (const std::invalid_argument& e) {
    threw = std::string_view(e.what()) == "cells must not be null or empty.";
  }
  Check(threw, "BoundingRegion empty-argument text verbatim");

  CellCoordsRegion coords{CPos{0, 0}, CPos{1, 1}};
  int count = 0;
  for (const CPos c : coords) {
    (void)c;
    ++count;
  }
  CheckEq(count, 4, "CellCoordsRegion enumerates 4");

  MapCoordsRegion map_coords{ora::MPos{1, 1}, ora::MPos{2, 2}};
  count = 0;
  for (const ora::MPos uv : map_coords) {
    (void)uv;
    ++count;
  }
  CheckEq(count, 4, "MapCoordsRegion enumerates 4");
}

void TestCellLayer() {
    CellLayer<int> layer{MapGridType::Rectangular, ora::Size{4, 3}};
  layer.Set(ora::MPos{2, 1}, 42);
  CheckEq(layer.Get(ora::MPos{2, 1}), 42, "CellLayer Set/Get by MPos");
  CheckEq(layer.Get(CPos{2, 1}), 42, "CellLayer Get by CPos (rectangular)");
  Check(layer.Contains(ora::MPos{3, 2}), "Contains in-bounds");
  Check(!layer.Contains(ora::MPos{4, 2}), "Contains out-of-bounds");

  bool threw = false;
  try {
    (void)layer.Get(ora::MPos{4, 0});
  } catch (const std::out_of_range&) {
    threw = true;
  }
  Check(threw, "out-of-bounds MPos index throws");

  CellLayer<int> iso{MapGridType::RectangularIsometric, ora::Size{8, 8}};
  int v = 0;
  Check(!iso.TryGetValue(CPos{1, 2}, v), "isometric X<Y TryGetValue false");
  Check(!iso.Contains(CPos{1, 2}), "isometric X<Y Contains false");

  auto resized = Resize(layer, ora::Size{5, 4}, 7);
  CheckEq(resized.Get(ora::MPos{2, 1}), 42, "Resize keeps the intersection");
  CheckEq(resized.Get(ora::MPos{4, 3}), 7, "Resize fills the default");
}

void TestMapGrid() {
    const MapGrid grid;
  CheckEq(grid.Ramps().size(), std::size_t{21}, "21 ramp constants");
  CheckEq(grid.TileScale(), 1024, "rectangular tile scale");
  CheckEq(static_cast<int>(grid.DefaultSubCell()), 3,
          "default subcell = center entry");
  CheckEq(grid.TilesByDistance().size(),
          std::size_t{50 + 1},
          "TilesByDistance bucket count");
  CheckEq(grid.TilesByDistance()[0].size(), std::size_t{1},
          "distance 0 = single tile");
  CheckEq(grid.TilesByDistance()[0][0].X, 0, "distance-0 tile is (0,0)");
  CheckEq(grid.OffsetOfSubCell(ora::sim::SubCell::First).X, -299,
          "subcell 1 offset x");

  CheckEq(grid.Ramps()[0].HeightOffset(0, 0), 0, "flat ramp height 0");
  CheckEq(grid.Ramps()[1].int4_center_height_offset, 256,
          "half-ramp center height = 724*512/1024 = 362? (实测按公式)");

  ora::yaml::MiniYaml bad_grid_yaml;
  ora::yaml::StringPool& pool = ora::yaml::MiniYaml::GlobalPool();
  ora::yaml::MiniYamlNode node{pool.Intern("DefaultSubCell"),
                               ora::yaml::MiniYaml{pool.Intern("9")}};
  bad_grid_yaml.Nodes.push_back(node);
  bool threw = false;
  try {
    MapGrid bad{bad_grid_yaml};
    (void)bad;
  } catch (const std::runtime_error& e) {
    threw = std::string_view(e.what()).find(
                "Subcell default index must be a valid index") == 0;
  }
  Check(threw, "DefaultSubCell out-of-range text");
}

void TestSpatialPartition() {
  using namespace ora::sim;
  SpatiallyPartitioned<Actor*> partitioned{1000, 1000, 100};
  ora::Rectangle bounds = ora::Rectangle::FromLTRB(0, 0, 50, 50);

  bool threw = false;
  try {
    ora::Rectangle empty = ora::Rectangle::FromLTRB(0, 0, 0, 0);
    partitioned.Add(nullptr, empty);
  } catch (const std::invalid_argument& e) {
    threw = std::string_view(e.what()) == "Bounds of null are empty.";
  }
  Check(threw, "empty-bounds text verbatim (null actor)");

  ora::sim::World world(ora::sim::WorldSimParams{});
  std::vector<ora::sim::Actor*> actors;
  {
    ora::sim::TypeDictionary dict;
    for (int i = 0; i < 3; ++i) {
      ora::sim::TypeDictionary d;
      actors.push_back(world.CreateActor(false, "", d));
    }
  }
  partitioned.Add(actors[0], ora::Rectangle::FromLTRB(0, 0, 50, 50));
  partitioned.Add(actors[1], ora::Rectangle::FromLTRB(40, 40, 160, 160));
  partitioned.Add(actors[2], ora::Rectangle::FromLTRB(500, 500, 600, 600));

  CheckEq(partitioned.Count(), 3, "three items partitioned");
  Check(partitioned.ContainsKey(actors[0]), "ContainsKey");
  CheckEq(partitioned.At(ora::int2{10, 10}).size(), std::size_t{1},
          "At resolves one bin item");
  CheckEq(partitioned.InBox(ora::Rectangle::FromLTRB(0, 0, 100, 100)).size(),
          std::size_t{2}, "InBox spans two bins, dedup applied");

  Check(partitioned.Remove(actors[1]), "Remove returns true");
  Check(!partitioned.Remove(actors[1]), "second Remove returns false");
  CheckEq(partitioned.InBox(ora::Rectangle::FromLTRB(0, 0, 100, 100)).size(),
          std::size_t{1}, "InBox after removal");
  partitioned.Clear();
  CheckEq(partitioned.Count(), 0, "Clear empties the partition");
}

void TestRemoteMapRepository(const char* ch_upstream_root) {
  std::println("[map] remote map repository");
  const std::string str_root{ch_upstream_root};

  std::filesystem::path path_support =
      std::filesystem::temp_directory_path() / "ora_map_test_support";
  std::error_code ec_fs{};
  std::filesystem::remove_all(path_support, ec_fs);
  std::filesystem::create_directories(path_support / "maps" / "ra" /
                                       "{DEV_VERSION}", ec_fs);

  game::InstalledMods mods_remote{str_root + "/mods"};
  const game::Manifest* manifest_remote = mods_remote.Find("ra");
  Check(manifest_remote != nullptr, "ra manifest found (remote face)");
  if (manifest_remote == nullptr)
    return;
  game::ModData mod_data{*manifest_remote, mods_remote, str_root};
  mod_data.ModFiles().SetPathResolver(
      game::PathResolver{str_root, path_support.string()});

  const std::string str_uid_hit = "deadbeefdeadbeefdeadbeefdeadbeefdeadbeef";
  const std::string str_uid_miss = "ffffffffffffffffffffffffffffffffffffffff";

  auto listener = platform::TcpSocket::Listen(0);
  Check(listener.IsValid(), "fake resource center listens");
  const auto int4_port = listener.BoundPort();

  auto thread_serve = std::thread{[&] {
    while (true) {
      auto client = listener.Accept();
      if (!client.IsValid())
        return;
      auto str_req = std::string{};
      const auto read_more = [&]() {
        std::array<std::uint8_t, 8192> arr_buf{};
        const auto int4_n =
            client.ReceiveUpTo(arr_buf.data(), arr_buf.size());
        if (int4_n <= 0)
          return false;
        str_req.append(reinterpret_cast<const char*>(arr_buf.data()),
                       static_cast<std::size_t>(int4_n));
        return true;
      };
      while (str_req.find("\r\n\r\n") == std::string::npos) {
        if (!client.PollReadable(3000000) || !read_more())
          break;
      }
      if (!str_req.starts_with("GET "))
        continue;

      const std::size_t sz_path_end = str_req.find(' ', 4);
      const std::string str_path =
          str_req.substr(4, sz_path_end == std::string::npos
                                 ? std::string::npos
                                 : sz_path_end - 4);
      if (str_path.find("/hash/") == 0) {
        Check(str_path.find(str_uid_hit) != std::string::npos &&
                  str_path.ends_with("/yaml"),
              "query path carries the uid batch");
        const std::string str_yaml = std::format(
            "{}:\n\ttitle: Remote Test Map\n\tauthor: tester\n\ttileset: "
            "TEMPERAT\n\tdownloading: True\n\tplayers: 2\n\tcategories: "
            "Conquest\n\tbounds: 0,0,32,32\n\tmap_grid_type: "
            "Rectangular\n\tmapformat: 11\n",
            str_uid_hit);
        const std::string str_response =
            std::format("HTTP/1.1 200 OK\r\nContent-Length: {}\r\n\r\n{}",
                        str_yaml.size(), str_yaml);
        client.SendAll(std::span<const std::uint8_t>{
            reinterpret_cast<const std::uint8_t*>(str_response.data()),
            str_response.size()});
      } else if (str_path.find("/download/") == 0) {
        std::ifstream file_map{str_root + "/mods/ra/maps/a-path-beyond.oramap",
                               std::ios::binary};
        std::string str_bytes{std::istreambuf_iterator<char>{file_map},
                              std::istreambuf_iterator<char>{}};
        const std::string str_response = std::format(
            "HTTP/1.1 200 OK\r\nContent-Length: {}\r\nContent-Disposition: "
            "attachment; filename=\"downloaded-map.oramap\"\r\n\r\n{}",
            str_bytes.size(), str_bytes);
        client.SendAll(std::span<const std::uint8_t>{
            reinterpret_cast<const std::uint8_t*>(str_response.data()),
            str_response.size()});
      } else if (str_path.find("/version") == 0) {
        static constexpr std::string_view kBody = "outdated";
        const std::string str_response =
            std::format("HTTP/1.1 200 OK\r\nContent-Length: {}\r\n\r\n{}",
                        kBody.size(), kBody);
        client.SendAll(std::span<const std::uint8_t>{
            reinterpret_cast<const std::uint8_t*>(str_response.data()),
            str_response.size()});
      } else {
        static constexpr std::string_view kBody = "not-found";
        const std::string str_response = std::format(
            "HTTP/1.1 404 Not Found\r\nContent-Length: {}\r\n\r\n{}",
            kBody.size(), kBody);
        client.SendAll(std::span<const std::uint8_t>{
            reinterpret_cast<const std::uint8_t*>(str_response.data()),
            str_response.size()});
      }
    }
  }};

  {
    auto cache = map::MapCache{mod_data.ManifestRef(), mod_data.ModFiles()};
    cache.LoadMaps(mod_data, "a-path-beyond");

    const fs::IReadOnlyPackage* ptr_user_package = nullptr;
    for (const auto& [ptr_package, kind_class] : cache.MapLocations())
      if (kind_class == map::MapClassification::User)
        ptr_user_package = ptr_package;
    Check(ptr_user_package != nullptr, "User map location registered");

    auto int4_received = 0;
    auto vec_failed_uids = std::vector<std::string>{};
    auto mutex_callbacks = std::mutex{};
    cache.QueryRemoteMapDetails(
        std::format("http://127.0.0.1:{}/", int4_port),
        {str_uid_hit, str_uid_miss, str_uid_hit},
        [&](map::MapPreview&) {
          const std::lock_guard lock{mutex_callbacks};
          int4_received++;
        },
        [&](map::MapPreview& p) {
          const std::lock_guard lock{mutex_callbacks};
          vec_failed_uids.push_back(p.Uid());
        });

    bool b_hit_done = false;
    for (int i = 0; i < 400 && !b_hit_done; ++i) {
      std::this_thread::sleep_for(std::chrono::milliseconds{10});
      bool b_callbacks = false;
      {
        const std::lock_guard lock{mutex_callbacks};
        b_callbacks = int4_received == 1 && vec_failed_uids.size() == 1;
      }
      b_hit_done =
          b_callbacks &&
          cache.At(str_uid_hit).Status() ==
              map::MapStatus::DownloadAvailable &&
          cache.At(str_uid_miss).Status() == map::MapStatus::Unavailable;
    }
    Check(b_hit_done,
          "query resolves hit=DownloadAvailable miss=Unavailable");
    Check(cache.At(str_uid_hit).Title() == "Remote Test Map",
          "remote title loaded");
    Check(cache.At(str_uid_hit).Author() == "tester", "remote author loaded");
    Check(cache.At(str_uid_hit).TileSet() == "TEMPERAT",
          "remote tileset loaded");
    CheckEq(cache.At(str_uid_hit).PlayerCount(), 2, "remote player count");
    Check(cache.At(str_uid_hit).Bounds().Width == 32, "remote bounds loaded");
    CheckEq(int4_received, 1, "received callback fires once");
    CheckEq(vec_failed_uids.size(), std::size_t{1},
            "fail callback fires for the missed uid");
    if (!vec_failed_uids.empty())
      CheckEq(vec_failed_uids.front(), str_uid_miss, "fail uid is the miss");

    cache.At(str_uid_hit).Install(
        std::format("http://127.0.0.1:{}/download/", int4_port));
    bool b_installed = false;
    for (int i = 0; i < 600 && !b_installed; ++i) {
      std::this_thread::sleep_for(std::chrono::milliseconds{10});
      b_installed =
          cache.At(str_uid_hit).Status() == map::MapStatus::Available;
    }
    Check(b_installed, "install lands the map as Available");
    Check(std::filesystem::exists(path_support / "maps" / "ra" /
                                   "{DEV_VERSION}" /
                                   "downloaded-map.oramap"),
          "downloaded file written into the User location");
    Check(cache.At(str_uid_hit).DownloadBytes() > 0, "download bytes set");
    CheckEq(cache.At(str_uid_hit).DownloadPercentage(), 100,
            "download percentage reaches 100");

    cache.At(str_uid_hit).Install(
        std::format("http://127.0.0.1:{}/download/", int4_port));
    Check(cache.At(str_uid_hit).Status() == map::MapStatus::Available,
          "install at Available stays a no-op");
  }

  {
    auto services = game::WebServices{};
    services.VersionCheck =
        std::format("http://127.0.0.1:{}/version", int4_port);
    services.SetVersionInfo("test-engine", "ra", "test-version");
    services.CheckModVersion();
    bool b_checked = false;
    for (int i = 0; i < 300 && !b_checked; ++i) {
      std::this_thread::sleep_for(std::chrono::milliseconds{10});
      b_checked =
          services.ModVersionStatus_() == game::ModVersionStatus::Outdated;
    }
    Check(b_checked, "CheckModVersion maps 'outdated'");

    Check(game::BuildHttpQuery("http://x/y", {{"a", "1 2"}, {"b", "z/z"}}) ==
              "http://x/y?a=1%202&b=z%2Fz&",
          "BuildHttpQuery escapes and joins");
    Check(game::EscapeDataString("AZaz09-_.~ x") == "AZaz09-_.~%20x",
          "EscapeDataString keeps RFC3986 unreserved");
  }

  listener.Close();
  thread_serve.join();
  std::filesystem::remove_all(path_support, ec_fs);
}

int main(int argc, char** argv) {
  setvbuf(stdout, nullptr, _IONBF, 0);
  if (argc < 2) {
    std::println("usage: map_test <upstream-root>");
    return 1;
  }

  gen::RegisterGeneratedAll();
  game::RegisterGameLoaders();
  ora::sim::RegisterWorldTraits();

  ora::mods::RegisterCommonTraits();

  TestSha1();
  TestCellRegions();
  TestCellLayer();
  TestMapGrid();
  TestSpatialPartition();

  game::InstalledMods mods_installed{std::string{argv[1]} + "/mods"};
  const game::Manifest* manifest = mods_installed.Find("ra");
  Check(manifest != nullptr, "ra mod found");
  if (manifest == nullptr)
    return 1;
  game::ModData mod_data{*manifest, mods_installed, argv[1]};

  const map::ITerrainInfo& terrain = mod_data.GetTerrainInfo("TEMPERAT");
  CheckEq(terrain.Id(), std::string_view{"TEMPERAT"}, "terrain Id");
  Check(terrain.TerrainTypes().size() > 0, "terrain types parsed");
  {
    const auto* dt = dynamic_cast<const map::DefaultTerrain*>(&terrain);
    Check(dt != nullptr, "terrain is DefaultTerrain");
    if (dt != nullptr) {
      std::println("terrain: types={} templates={}",
                   dt->TerrainTypes().size(), dt->TemplatesInDefinitionOrder().size());
      Check(dt->TemplatesInDefinitionOrder().size() > 0, "terrain templates parsed");
      Check(!dt->RestrictedPlayerColors().empty() ||
                dt->RestrictedPlayerColors().empty(),
            "restricted colors query stable");
    }
  }

  ora::Size map_size{8, 6};
  map::Map editor{map::Map::Params{.mod_data = &mod_data}, terrain,
                  map_size};
  CheckEq(editor.MapSize().Width, 8, "editor map width");

  const auto bin = editor.SaveBinaryData();

  CheckEq(bin.size(), std::size_t{17 + 5 * 8 * 6},
          "flat map.bin size = header 17 + tiles(3) + resources(2)");

  const std::string str_dir = std::format("{}/_map_test_map", argv[1]);
  std::filesystem::create_directories(str_dir);
  {
    std::ofstream f(str_dir + "/map.bin", std::ios::binary);
    f.write(reinterpret_cast<const char*>(bin.data()),
            static_cast<std::streamsize>(bin.size()));
  }
  {
    std::ofstream f(str_dir + "/map.yaml", std::ios::binary);
    const std::string yaml_text =
        "MapFormat: 11\n"
        "RequiresMod: ra\n"
        "Title: Test\n"
        "Author: T\n"
        "Tileset: TEMPERAT\n"
        "MapSize: 8,6\n"
        "Bounds: 1,1,7,5\n"
        "Visibility: Lobby\n"
        "Categories: Conquest\n"
        "Players:\n"
        "\tPlayerReference@Neutral:\n"
        "\t\tName: Neutral\n"
        "\t\tOwnsWorld: True\n"
        "\t\tNonCombatant: True\n"
        "\t\tFaction: allies\n"
        "Actors:\n";
    f.write(yaml_text.data(),
            static_cast<std::streamsize>(yaml_text.size()));
  }

  fs::Folder package{str_dir};
  map::Map loaded{map::Map::Params{.mod_data = &mod_data}, package};
  CheckEq(loaded.MapFormat(), 11, "loaded map format");
  Check(!loaded.Uid().empty(), "UID computed");
  Check(loaded.CustomTerrain().Get(ora::MPos{0, 0}) == 255,
        "CustomTerrain default 255");
  const ora::WPos center = loaded.CenterOfCell(ora::CPos{2, 3});
  CheckEq(center.X, 2 * 1024 + 512, "CenterOfCell.x");
  CheckEq(center.Y, 3 * 1024 + 512, "CenterOfCell.y");
  Check(!loaded.Contains(ora::CPos{0, 0}), "Contains bounds-clipped");
  Check(loaded.Contains(ora::CPos{2, 2}), "Contains inside bounds");
  (void)loaded.GetTerrainIndex(ora::CPos{2, 2});
  Check(loaded.GetTerrainInfo(ora::CPos{2, 2}).Type ==
            terrain
                .TerrainTypes()[loaded.GetTerrainIndex(ora::CPos{2, 2})]
                .Type,
        "GetTerrainInfo consistent");
  const auto bin2 = loaded.SaveBinaryData();
  Check(bin == bin2, "SaveBinaryData roundtrip identical");

  game::Game game_{game::Game::Deps{.ptr_mod_data = &mod_data}};
  game_.JoinLocal();

  map::MapCache cache{mod_data.ManifestRef(), mod_data.ModFiles()};
  cache.LoadMaps(mod_data);
  Check(cache.Previews().size() > 0, "MapCache.LoadMaps found previews");

  std::string str_uid;
  for (map::MapPreview* p2 : cache.Previews())
    if (p2->Status() == map::MapStatus::Available) {
      str_uid = p2->Uid();
      break;
    }
  Check(!str_uid.empty(), "an available ra map exists");

  if (!str_uid.empty()) {
    map::MapPreview& preview = cache.At(str_uid);
    Check(preview.MapFormat() >= Map::kSupportedMapFormat,
          "preview MapFormat supported");
    Check(preview.ModifiedDate() > 0, "preview ModifiedDate stamped");
    Check(preview.WorldActorInfo() != nullptr, "preview WorldActorInfo set");
    Check(preview.PlayerActorInfo() != nullptr,
          "preview PlayerActorInfo set");
    Check(preview.WorldActorInfo()->Name() == "world",
          "preview WorldActorInfo name");
    Check(!preview.DefinesUnsafeCustomRules(),
          "stock ra map defines no unsafe custom rules");
    Check(preview.LoadRuleset() != nullptr, "preview LoadRuleset constructs");
    Check(preview.ExistsFile("map.yaml"), "preview ExistsFile map.yaml");
    Check(!preview.OpenFile("map.yaml").empty(), "preview OpenFile map.yaml");
    std::string str_message;
    Check(!preview.TryGetMessage("nomatch", str_message),
          "preview TryGetMessage fluent fallback");
    Check(preview.GetMessage("nomatch") == "nomatch",
          "preview GetMessage returns key");

    cache.SetMinimapMaterializer(
        [](map::MapPreview& p_mini) { p_mini.SetMinimap(0x1234); });
    if (preview.Preview() != nullptr) {
      preview.GetMinimap();
      for (int i = 0; i < 200 && preview.GetMinimap() == 0; ++i)
        std::this_thread::sleep_for(std::chrono::milliseconds{10});
      Check(preview.GetMinimap() == 0x1234,
            "minimap materialized by loader thread");
    }
    preview.SetMinimap(0);
    cache.UpdateMaps();
    cache.UpdateMaps();
    Check(cache.Previews().size() > 0, "UpdateMaps no-op keeps previews");

    {
      auto vec_status_log = std::vector<std::string>{};
      auto deps_cache = ora::server::MapStatusCacheDeps{};
      deps_cache.fn_server_log = [&vec_status_log](std::string_view l) {
        vec_status_log.emplace_back(l);
      };
      auto str_changed_uid = std::string{};
      auto int4_changed_status = -1;
      auto changed_count = 0;
      auto cache_status = ora::server::MapStatusCache{
          std::move(deps_cache),
          [&](std::string_view u, int st) {
            str_changed_uid = std::string{u};
            int4_changed_status = st;
            changed_count++;
          },
          false};
      const auto int4_status =
          cache_status.StatusOf(preview);
      Check((int4_status &
             static_cast<int>(ora::net::SessionMapStatus::Playable)) != 0,
            "stock map grades Playable");
      Check(!ora::net::HasMapStatusFlag(
                int4_status, ora::net::SessionMapStatus::UnsafeCustomRules),
            "stock map carries no unsafe-rules bit");
      Check(cache_status.StatusOf(preview) == int4_status,
            "status cached across queries");
      Check(changed_count == 0, "no lint callback without remote linting");
    }
  }

  if (!str_uid.empty()) {
    map::MapPreview& preview = cache.At(str_uid);
    auto map_world = preview.ToMap();
    Check(map_world != nullptr, "MapPreview.ToMap constructs");

    if (map_world != nullptr) {
      auto world = std::make_unique<sim::World>(
          *map_world, mod_data, *game_.OrderManagerFace(),
          sim::WorldType::Regular);
      Check(world->WorldActor() != nullptr, "WorldActor created");
      Check(world->ScreenMapFace() != nullptr,
            "ScreenMap resolved from world actor");
      Check(world->Selection() != nullptr,
            "Selection resolved from world actor");
      Check(world->Timestep() > 0, "Timestep from GameSpeeds");
      world->Tick();
      CheckEq(world->WorldTick(), 1, "first tick advances");
      Check(world->ScreenMapFace()->GetTraitTypeId() ==
                sim::ScreenMap::kTypeId,
            "arena-allocated traits stay valid");
      Check(world->MapPtr() != nullptr, "Map face wired");
    }
  }

  if (g_failures == 0)
    TestRemoteMapRepository(argv[1]);

  return g_failures == 0 ? 0 : 1;
}

#if 0
  if (g_failures == 0)
    std::println("\nmap_test: all passed");
  else
    std::println("\nmap_test: {} FAILURES", g_failures);
  return g_failures == 0 ? 0 : 1;
}
#endif
