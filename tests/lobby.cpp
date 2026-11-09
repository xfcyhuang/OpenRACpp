import std;

#include "game/filesystem/zip_read.hpp"
#include "game/map_read.hpp"
#include "game/miniyaml.hpp"
#include "game/network/lobby_session.hpp"
#include "game/render/png_decode.hpp"
#include "game/ui/chrome_provider.hpp"
#include "game/ui/widget_loader.hpp"
#include "launcher/game_session.hpp"
#include "launcher/lobby_ui.hpp"
#include "test_util.hpp"
using ora::test::check;

#ifndef ORA_LOBBY_GOLDEN_DIR
#define ORA_LOBBY_GOLDEN_DIR "tests/golden/lobby"
#endif

#ifndef ORA_MAPS_GOLDEN_DIR
#define ORA_MAPS_GOLDEN_DIR "tests/golden/maps"
#endif

namespace {

namespace net = ora::game::network;
namespace ogf = ora::game::filesystem;
namespace ui = ora::game::ui;
namespace lch = ora::launcher;
namespace opro = ora::game::production;

std::int32_t config_seed() { return 11; }

opro::LobbyModel lobby_model_from(const lch::LobbyStartConfig& config) {
  opro::LobbyModel model;
  model.vec_playable_factions = config.vec_factions;
  model.int4_starting_cash = 5000;
  model.str_techlevel = "unrestricted";
  model.b_build_radius = true;
  return model;
}

std::optional<std::string> read_text(std::string_view str_path) {
  std::ifstream stream_in{std::filesystem::path{std::string{str_path}}, std::ios::binary};
  if (!stream_in) return std::nullopt;
  std::string str_bytes;
  char buf1_chunk[8192];
  while (stream_in.read(buf1_chunk, static_cast<std::streamsize>(sizeof buf1_chunk))
      || stream_in.gcount() > 0)
    str_bytes.append(buf1_chunk, static_cast<std::size_t>(stream_in.gcount()));
  return str_bytes;
}

std::vector<std::string> split_lines(const std::string& str_text) {
  std::vector<std::string> vec_out;
  std::size_t sz_pos{0};
  while (sz_pos < str_text.size()) {
    const std::size_t sz_nl{str_text.find('\n', sz_pos)};
    std::string str_line{str_text.substr(
        sz_pos, sz_nl == std::string::npos ? std::string::npos : sz_nl - sz_pos)};
    if (!str_line.empty() && str_line.back() == '\r') str_line.pop_back();
    vec_out.push_back(std::move(str_line));
    if (sz_nl == std::string::npos) break;
    sz_pos = sz_nl + 1;
  }
  return vec_out;
}

std::vector<std::string> golden_lines(const char* sz_name) {
  const auto opt_text{read_text(std::string{ORA_LOBBY_GOLDEN_DIR} + "/" + sz_name)};
  if (!opt_text.has_value()) {
    check(false, std::format("lobby: golden {} 缺失", sz_name));
    return {};
  }
  return split_lines(*opt_text);
}

std::string_view bool_name(bool b_value) { return b_value ? "True" : "False"; }

std::string join_lines(const std::vector<std::string>& vec_lines) {
  std::string str_out;
  for (const std::string& str_line : vec_lines) {
    str_out += str_line;
    str_out += '\n';
  }
  return str_out;
}

void check_lines_equal(std::string_view str_what, const std::vector<std::string>& vec_produced,
                       const std::vector<std::string>& vec_golden) {
  const std::size_t sz_n{std::min(vec_produced.size(), vec_golden.size())};
  std::size_t sz_first_diff{vec_produced.size()};
  bool b_same{vec_produced.size() == vec_golden.size()};
  for (std::size_t sz_i{0}; sz_i < sz_n; ++sz_i)
    if (vec_produced[sz_i] != vec_golden[sz_i]) {
      b_same = false;
      sz_first_diff = sz_i;
      break;
    }
  if (b_same) {
    check(true, std::format("{}: 全等（{} 行）", str_what, vec_produced.size()));
    return;
  }
  check(false, std::format("{}: 不一致（produced {} 行 / golden {} 行）", str_what,
                           vec_produced.size(), vec_golden.size()));
  if (sz_first_diff < sz_n)
    std::println("  first diff at line {}: produced [{}] golden [{}]", sz_first_diff + 1,
                 vec_produced[sz_first_diff], vec_golden[sz_first_diff]);
}

struct MapFixture {
  std::string str_name;
  std::string str_uid;
  std::string str_map_yaml;
  std::vector<std::uint8_t> vec1u_map_png;
};

std::optional<MapFixture> load_fixture(const std::string& str_name) {
  const auto opt_bytes{read_text(std::string{ORA_MAPS_GOLDEN_DIR} + "/" + str_name + ".oramap")};
  if (!opt_bytes.has_value()) return std::nullopt;
  const std::vector<std::uint8_t> vec1u_bytes{opt_bytes->begin(), opt_bytes->end()};
  const auto res_zip{ogf::zip_read(vec1u_bytes)};
  if (!res_zip.has_value()) return std::nullopt;
  MapFixture fixture;
  fixture.str_name = str_name;
  const auto vec_yaml{ogf::zip_entry_data(*res_zip, "map.yaml")};
  const auto vec_png{ogf::zip_entry_data(*res_zip, "map.png")};
  if (!vec_yaml.has_value() || !vec_png.has_value()) return std::nullopt;
  fixture.str_map_yaml.assign(vec_yaml->begin(), vec_yaml->end());
  fixture.vec1u_map_png = *vec_png;
  const auto res_uid{ora::game::map_uid(*res_zip)};
  if (res_uid.has_value()) fixture.str_uid = *res_uid;
  return fixture;
}

std::array<std::uint32_t, 8> sha256_rounds(const std::vector<std::uint8_t>& vec1u_message) {
  constexpr std::array<std::uint32_t, 64> kK{
      0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4,
      0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe,
      0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f,
      0x4a7484aa, 0x5cb0a9dc, 0x76f988da, 0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
      0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc,
      0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b,
      0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070, 0x19a4c116,
      0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
      0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7,
      0xc67178f2};
  std::vector<std::uint8_t> vec1u_data{vec1u_message};
  const std::uint64_t u8_bits{static_cast<std::uint64_t>(vec1u_message.size()) * 8};
  vec1u_data.push_back(0x80);
  while (vec1u_data.size() % 64 != 56) vec1u_data.push_back(0);
  for (int int4_shift{56}; int4_shift >= 0; int4_shift -= 8)
    vec1u_data.push_back(static_cast<std::uint8_t>((u8_bits >> int4_shift) & 0xFF));

  std::array<std::uint32_t, 8> arr_h{0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                                     0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
  const auto rotr{[](std::uint32_t u4_x, int int4_n) {
    return (u4_x >> int4_n) | (u4_x << (32 - int4_n));
  }};
  for (std::size_t sz_block{0}; sz_block < vec1u_data.size(); sz_block += 64) {
    std::array<std::uint32_t, 64> arr_w{};
    for (int int4_i{0}; int4_i < 16; ++int4_i)
      for (int int4_b{0}; int4_b < 4; ++int4_b)
        arr_w[int4_i] = (arr_w[int4_i] << 8)
            | vec1u_data[sz_block + static_cast<std::size_t>(int4_i) * 4
                         + static_cast<std::size_t>(int4_b)];
    for (int int4_i{16}; int4_i < 64; ++int4_i) {
      const std::uint32_t u4_s0{rotr(arr_w[int4_i - 15], 7) ^ rotr(arr_w[int4_i - 15], 18)
          ^ (arr_w[int4_i - 15] >> 3)};
      const std::uint32_t u4_s1{rotr(arr_w[int4_i - 2], 17) ^ rotr(arr_w[int4_i - 2], 19)
          ^ (arr_w[int4_i - 2] >> 10)};
      arr_w[int4_i] = arr_w[int4_i - 16] + u4_s0 + arr_w[int4_i - 7] + u4_s1;
    }
    std::uint32_t u4_a{arr_h[0]}, u4_b{arr_h[1]}, u4_c{arr_h[2]}, u4_d{arr_h[3]};
    std::uint32_t u4_e{arr_h[4]}, u4_f{arr_h[5]}, u4_g{arr_h[6]}, u4_h{arr_h[7]};
    for (int int4_i{0}; int4_i < 64; ++int4_i) {
      const std::uint32_t u4_s1{rotr(u4_e, 6) ^ rotr(u4_e, 11) ^ rotr(u4_e, 25)};
      const std::uint32_t u4_ch{(u4_e & u4_f) ^ (~u4_e & u4_g)};
      const std::uint32_t u4_temp1{u4_h + u4_s1 + u4_ch + kK[static_cast<std::size_t>(int4_i)]
          + arr_w[static_cast<std::size_t>(int4_i)]};
      const std::uint32_t u4_s0{rotr(u4_a, 2) ^ rotr(u4_a, 13) ^ rotr(u4_a, 22)};
      const std::uint32_t u4_maj{(u4_a & u4_b) ^ (u4_a & u4_c) ^ (u4_b & u4_c)};
      const std::uint32_t u4_temp2{u4_s0 + u4_maj};
      u4_h = u4_g;
      u4_g = u4_f;
      u4_f = u4_e;
      u4_e = u4_d + u4_temp1;
      u4_d = u4_c;
      u4_c = u4_b;
      u4_b = u4_a;
      u4_a = u4_temp1 + u4_temp2;
    }
    arr_h[0] += u4_a; arr_h[1] += u4_b; arr_h[2] += u4_c; arr_h[3] += u4_d;
    arr_h[4] += u4_e; arr_h[5] += u4_f; arr_h[6] += u4_g; arr_h[7] += u4_h;
  }
  return arr_h;
}

std::string sha256_hex(const std::vector<std::uint8_t>& vec1u_message) {
  std::string str_out;
  for (const std::uint32_t u4_word : sha256_rounds(vec1u_message))
    str_out += std::format("{:08x}", u4_word);
  return str_out;
}

}

int main() {
  check(sha256_hex({'a', 'b', 'c'}) == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
        "lobby: sha256 abc 向量");
  const std::vector<std::string> vec_names{"ore-lord", "doubles", "a-path-beyond"};

  std::vector<MapFixture> vec_fixtures;
  for (const std::string& str_name : vec_names) {
    auto opt{load_fixture(str_name)};
    check(opt.has_value(), std::format("lobby: fixture {} 装载", str_name));
    if (opt.has_value()) vec_fixtures.push_back(std::move(*opt));
  }
  if (vec_fixtures.size() != vec_names.size()) return ora::test::test_exit_code("lobby");

  net::LobbyServerModel server;
  {
    std::vector<net::LobbyMapInfo> vec_maps;
    for (MapFixture& fixture : vec_fixtures) {
      net::LobbyMapInfo info;
      info.str_name = fixture.str_name;
      info.str_uid = fixture.str_uid;
      info.vec_selectable_factions = net::ra_selectable_factions();
      const auto res_refs{
          net::lobby_player_refs_from_map_yaml(fixture.str_map_yaml, fixture.str_name)};
      check(res_refs.has_value(),
            std::format("lobby: {} PlayerReference 解析", fixture.str_name));
      if (!res_refs.has_value()) return ora::test::test_exit_code("lobby");
      info.vec_players = *res_refs;
      vec_maps.push_back(std::move(info));
    }
    server.server_start(std::move(vec_maps), 0);
  }

  {
    std::vector<std::string> vec_lines;
    for (const MapFixture& fixture : vec_fixtures) {
      const net::LobbyMapInfo* ptr_info{nullptr};
      for (const net::LobbyMapInfo& info : server.vec_maps)
        if (info.str_name == fixture.str_name) ptr_info = &info;
      if (ptr_info == nullptr) continue;
      vec_lines.push_back(std::format("map {} uid {}", fixture.str_name, fixture.str_uid));
      for (const net::LobbyPlayerRef& ref : ptr_info->vec_players)
        vec_lines.push_back(std::format(
            "player {} playable={} allowBots={} lockFaction={} lockColor={} lockTeam={} "
            "lockHandicap={} lockSpawn={} required={} faction={} team={} bot={}",
            ref.str_name, bool_name(ref.b_playable), bool_name(ref.b_allow_bots),
            bool_name(ref.b_lock_faction), bool_name(ref.b_lock_color), bool_name(ref.b_lock_team),
            bool_name(ref.b_lock_handicap), bool_name(ref.b_lock_spawn), bool_name(ref.b_required),
            ref.str_faction, ref.int4_team, ref.str_bot));
      std::string str_factions;
      for (std::size_t sz_i{0}; sz_i < ptr_info->vec_selectable_factions.size(); ++sz_i) {
        if (sz_i != 0) str_factions += ',';
        str_factions += ptr_info->vec_selectable_factions[sz_i];
      }
      vec_lines.push_back("factions " + str_factions);
    }
    std::vector<std::string> vec_golden;
    for (const std::string& str_line : golden_lines("slots.txt"))
      if (!str_line.empty() && str_line.front() != '#') vec_golden.push_back(str_line);
    check_lines_equal("lobby slots 对拍", vec_lines, vec_golden);
  }

  {
    std::vector<std::string> vec_lines;
    std::vector<std::string> vec_golden_raw{golden_lines("session.txt")};
    std::size_t sz_golden{0};
    while (sz_golden < vec_golden_raw.size()
           && (vec_golden_raw[sz_golden].empty()
               || vec_golden_raw[sz_golden].front() == '#'))
      ++sz_golden;
    const std::string str_color_line{vec_golden_raw[sz_golden]};
    ++sz_golden;

    auto expect_step{[&](std::string_view str_op, std::string_view str_cmd_line) {
      if (sz_golden >= vec_golden_raw.size()) {
        check(false, "lobby session: 黄金步数不足");
        return;
      }
      if (!str_cmd_line.empty()) {
        const std::string str_golden_cmd{vec_golden_raw[sz_golden++]};
        check(str_golden_cmd == str_cmd_line,
              std::format("lobby session: cmd 行 [{}]（golden [{}]）", str_cmd_line,
                          str_golden_cmd));
      }
      const std::string str_golden_op{vec_golden_raw[sz_golden++]};
      check(str_golden_op == std::format("step {}", str_op),
            std::format("lobby session: 步序 [{}]（golden [{}]）", str_op, str_golden_op));
      const std::string str_sep{vec_golden_raw[sz_golden++]};
      check(str_sep == "--- session ---",
            std::format("lobby session: 分隔行（golden [{}]）", str_sep));
      std::vector<std::string> str_block;
      while (sz_golden < vec_golden_raw.size() && vec_golden_raw[sz_golden] != "--- end ---")
        str_block.push_back(vec_golden_raw[sz_golden++]);
      ++sz_golden;
      const std::string str_produced{server.lobby_info.serialize()};
      const std::string str_golden_text{join_lines(str_block)};
      check(str_produced == str_golden_text,
            std::format("lobby session [{}] Serialize 字节恒等", str_op));
      if (str_produced != str_golden_text) {
        std::vector<std::string> vec_prod{split_lines(str_produced)};
        std::size_t sz_i{0};
        for (; sz_i < std::min(vec_prod.size(), str_block.size()); ++sz_i)
          if (vec_prod[sz_i] != str_block[sz_i]) break;
        if (sz_i < std::min(vec_prod.size(), str_block.size()))
          std::println("  first diff at line {}: produced [{}] golden [{}]", sz_i + 1,
                       vec_prod[sz_i], str_block[sz_i]);
      }
    }};

    check(str_color_line == "color fixed C81E28", "lobby session: 固定色行一致");
    expect_step("server-started ore-lord", "");

    server.join_local_client("Commander", "C81E28");
    expect_step("join-client-0 Commander", "");

    check(server.interpret_command(0, "faction 0 russia"),
          "lobby session: faction russia 受理");
    expect_step("faction 0 russia", "cmd faction 0 russia -> ok");

    check(server.interpret_command(0, "faction 0 creepus"),
          "lobby session: faction creepus 受理（服务端消化不发变更）");
    expect_step("faction 0 creepus", "cmd faction 0 creepus -> rejected");

    check(server.interpret_command(0, "team 0 2"), "lobby session: team 2 受理");
    expect_step("team 0 2", "cmd team 0 2 -> ok");

    check(server.interpret_command(0, "team 0 0"), "lobby session: team 0 受理");
    expect_step("team 0 0", "cmd team 0 0 -> ok");

    check(server.select_map("doubles"), "lobby session: 切换 doubles");
    expect_step("map doubles", "cmd map doubles -> ok");

    check(server.select_map("ore-lord"), "lobby session: 切回 ore-lord");
    expect_step("map ore-lord", "cmd map ore-lord -> ok");

    check(server.interpret_command(0, "faction 0 Random"), "lobby session: faction Random 受理");
    expect_step("faction 0 Random", "cmd faction 0 Random -> ok");

    check(server.interpret_command(0, "state Ready"), "lobby session: state Ready 受理");
    expect_step("state Ready", "cmd state Ready -> ok");

    check(server.interpret_command(0, "startgame"), "lobby session: startgame 受理");
    check(server.b_start_requested, "lobby session: startgame 通过导航置位");
    expect_step("startgame", "cmd startgame -> ok");

    check(sz_golden == vec_golden_raw.size(), "lobby session: 黄金步数全消费");
  }

  {
    const auto res_tree{ora::game::miniyaml_from_file_named(
        std::string{ORA_LOBBY_GOLDEN_DIR} + "/fixture/lobby.yaml", "lobby.yaml")};
    check(res_tree.has_value(), "lobby chrome: lobby.yaml 解析");
    if (res_tree.has_value()) {
      const auto res_loader{ui::widget_loader_from_nodes(*res_tree)};
      check(res_loader.has_value(), "lobby chrome: loader 构建");
      if (res_loader.has_value()) {
        ui::WidgetModel root{};
        root.str_type = "Container";
        root.bounds.int4_width = 1024;
        root.bounds.int4_height = 768;
        ui::WidgetInitCtx ctx{};
        ctx.int4_window_width = 1024;
        ctx.int4_window_height = 768;
        const auto res_widget{ui::widget_loader_load_widget(*res_loader, root, ctx,
                                                            "SERVER_LOBBY")};
        check(res_widget.has_value(), "lobby chrome: SERVER_LOBBY 装载");
        if (res_widget.has_value()) {
          const ui::WidgetModel* ptr_root{*res_widget};
          check(ptr_root->bounds.int4_x == 62 && ptr_root->bounds.int4_y == 84
                    && ptr_root->bounds.int4_width == 900
                    && ptr_root->bounds.int4_height == 600,
                std::format("lobby chrome: SERVER_LOBBY bounds {}x{}@{},{}",
                            ptr_root->bounds.int4_width, ptr_root->bounds.int4_height,
                            ptr_root->bounds.int4_x, ptr_root->bounds.int4_y));
          const ui::WidgetModel* ptr_start{nullptr};
          std::vector<const ui::WidgetModel*> vec_stack{ptr_root};
          while (!vec_stack.empty()) {
            const ui::WidgetModel* ptr_w{vec_stack.back()};
            vec_stack.pop_back();
            if (ptr_w->str_id == "START_GAME_BUTTON") ptr_start = ptr_w;
            for (const auto& ptr_child : ptr_w->vec_children)
              vec_stack.push_back(ptr_child.get());
          }
          check(ptr_start != nullptr && ptr_start->bounds.int4_x == 630
                    && ptr_start->bounds.int4_y == 555 && ptr_start->bounds.int4_width == 120
                    && ptr_start->bounds.int4_height == 25,
                "lobby chrome: START_GAME_BUTTON bounds 120x25@630,555");
        }
      }
    }
  }

  {
    std::vector<std::string> vec_lines;
    ui::ChromeSheetSet set_sheets;
    for (const MapFixture& fixture : vec_fixtures) {
      const auto res_sheet{
          ui::chrome_sheet_load(set_sheets, fixture.str_name + "-map.png", fixture.vec1u_map_png)};
      check(res_sheet.has_value(), std::format("lobby preview: {} map.png 解码", fixture.str_name));
      if (!res_sheet.has_value()) continue;
      const ui::ChromeSheet& sheet{set_sheets.map_sheets.at(fixture.str_name + "-map.png")};
      std::vector<std::uint8_t> vec1u_bytes;
      vec1u_bytes.reserve(sheet.vec_u4_argb.size() * 4);
      for (const std::uint32_t u4_pixel : sheet.vec_u4_argb)
        for (int int4_shift{0}; int4_shift < 32; int4_shift += 8)
          vec1u_bytes.push_back(static_cast<std::uint8_t>((u4_pixel >> int4_shift) & 0xFF));
      vec_lines.push_back(std::format("map {} {}x{} sha256={}", fixture.str_name,
                                      sheet.int4_width, sheet.int4_height,
                                      sha256_hex(vec1u_bytes)));
      for (int int4_y{0}; int4_y < sheet.int4_height; int4_y += 53)
        for (int int4_x{0}; int4_x < sheet.int4_width; int4_x += 47)
          vec_lines.push_back(std::format(
              "p {},{} {:08X}", int4_x, int4_y,
              sheet.vec_u4_argb[static_cast<std::size_t>(int4_y)
                                * static_cast<std::size_t>(sheet.int4_width)
                                + static_cast<std::size_t>(int4_x)]));

      const float fp4_scale{std::min(172.0f / static_cast<float>(sheet.int4_width),
                                     172.0f / static_cast<float>(sheet.int4_height))};
      const int int4_w{static_cast<int>(fp4_scale * static_cast<float>(sheet.int4_width))};
      const int int4_h{static_cast<int>(fp4_scale * static_cast<float>(sheet.int4_height))};
      const int int4_x{1 + (172 - int4_w) / 2};
      const int int4_y{1 + (172 - int4_h) / 2};
      vec_lines.push_back(std::format("rect {} {},{},{},{}", fixture.str_name, int4_x, int4_y,
                                      int4_w, int4_h));
    }
    std::vector<std::string> vec_golden;
    for (const std::string& str_line : golden_lines("preview.txt"))
      if (!str_line.empty() && str_line.front() != '#') vec_golden.push_back(str_line);
    check_lines_equal("lobby preview 对拍", vec_lines, vec_golden);
  }

  {
    lch::LobbyRuntime lobby;
    const auto res_lobby{
        lch::lobby_runtime_create(lobby, ORA_MAPS_GOLDEN_DIR, ORA_LOBBY_GOLDEN_DIR "/fixture")};
    check(res_lobby.has_value(), "lobby ui: runtime 装配");
    if (res_lobby.has_value()) {
      check(lobby.b_tree_loaded, "lobby ui: SERVER_LOBBY chrome 树装载");
      check(lobby_current_map(lobby) != nullptr
                && lobby_current_map(lobby)->str_name == "ore-lord",
            "lobby ui: 初始图 ore-lord");

      {
        const lch::LobbyStartConfig config{lobby_start_config(lobby)};
        check(config.str_map_path.ends_with("ore-lord.oramap"),
              "lobby ui: 默认终态地图路径");
        check(config.vec_factions.size() == 1 && config.vec_factions[0].first == "Multi0"
                  && config.vec_factions[0].second == "soviet",
              "lobby ui: 默认终态与 T15 拼装等价（Multi0/soviet）");
      }

      check(lch::lobby_cycle_map(lobby, 1), "lobby ui: 循环切图受理");
      check(lobby_current_map(lobby) != nullptr
                && lobby_current_map(lobby)->str_name == "a-path-beyond",
            "lobby ui: 切图后 a-path-beyond（目录序回绕）");
      check(lch::lobby_cycle_map(lobby, -1), "lobby ui: 反向切图受理");
      check(lobby_current_map(lobby) != nullptr
                && lobby_current_map(lobby)->str_name == "ore-lord",
            "lobby ui: 回到 ore-lord");

      check(lch::lobby_cycle_faction(lobby, 1), "lobby ui: 循环换势力受理");
      check(lch::lobby_faction_display(lobby) == "RandomAllies",
            "lobby ui: Random -> RandomAllies（ra 可选表序）");
      check(lch::lobby_cycle_faction(lobby, 1), "lobby ui: 再换势力受理");
      check(lch::lobby_faction_display(lobby) == "RandomSoviet", "lobby ui: 表序推进");

      {
        const lch::LobbyStartConfig config{lobby_start_config(lobby)};
        check(config.vec_factions.size() == 1
                  && config.vec_factions[0].second == "soviet",
              "lobby ui: 非沙盒势力折算 soviet（直进链裁定）");
      }

      check(lch::lobby_cycle_faction(lobby, -1), "lobby ui: 反向换势力受理");
      check(lch::lobby_cycle_faction(lobby, -1), "lobby ui: 反向换势力受理 2");
      check(lch::lobby_faction_display(lobby) == "Random", "lobby ui: 回到 Random");

      check(lch::lobby_cycle_team(lobby, 1), "lobby ui: 循环换队伍受理");
      check(lch::lobby_team_display(lobby) == "1", "lobby ui: 队伍 0 -> 1");
      check(lch::lobby_cycle_team(lobby, 1), "lobby ui: 再换队伍受理");
      check(lch::lobby_team_display(lobby) == "2", "lobby ui: 队伍 1 -> 2");

      check(lch::lobby_request_start(lobby), "lobby ui: StartGame 受理");
      check(lobby.server.b_start_requested, "lobby ui: StartGame 置位");

      {
        const lch::LobbyStartConfig config{lobby_start_config(lobby)};
        check(config.vec_factions.size() == 1 && config.vec_factions[0].first == "Multi0"
                  && config.vec_factions[0].second == "soviet",
              "lobby ui: 默认势力终态与 T15 拼装等价");
        check(config.int4_seed == 0, "lobby ui: 种子透传");
      }

      lch::SessionEnv env_start;
      env_start.int4_seed = config_seed();
      env_start.str_map_path = lobby_start_config(lobby).str_map_path;
      env_start.str_tileset_path = std::string{ORA_MAPS_GOLDEN_DIR} + "/tilesets/temperat.yaml";
      env_start.opt_lobby = lobby_model_from(lobby_start_config(lobby));
      const auto res_session{lch::session_setup(env_start)};
      check(res_session.has_value(), "lobby ui: 直进链 session_setup");
      if (res_session.has_value()) {
        const opro::LobbyModel& model{res_session->w.lobby};
        check(model.vec_playable_factions.size() == 1
                  && model.vec_playable_factions[0].first == "Multi0",
              "lobby ui: 直进链 world 装载 lobby 槽位");
        check(model.int4_starting_cash == 5000 && model.str_techlevel == "unrestricted"
                  && model.b_build_radius,
              "lobby ui: 直进链 T15 常量面保留");
      }
    }
  }

  return ora::test::test_exit_code("lobby");
}
