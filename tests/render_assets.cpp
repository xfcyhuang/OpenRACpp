import std;

#include <cstdlib>
import ora.game.render;
import ora.test.util;

using ora::test::check;

#ifndef ORA_CONTENT_TEMPERAT_DIR
#define ORA_CONTENT_TEMPERAT_DIR ""
#endif

namespace {


constexpr int kSkip{77};

std::optional<std::string> read_file_bytes(std::string_view str_path) {
  std::ifstream stream_in{std::filesystem::path{std::string{str_path}}, std::ios::binary};
  if (!stream_in) return std::nullopt;
  std::string str_bytes;
  char buf1_chunk[8192];
  while (stream_in.read(buf1_chunk, static_cast<std::streamsize>(sizeof buf1_chunk))
      || stream_in.gcount() > 0)
    str_bytes.append(buf1_chunk, static_cast<std::size_t>(stream_in.gcount()));
  return str_bytes;
}

std::optional<std::string> content_dir() {
  std::string str_dir{ORA_CONTENT_TEMPERAT_DIR};
  if (const char* sz_env{std::getenv("ORA_CONTENT_TEMPERAT")}; sz_env != nullptr && *sz_env != '\0')
    str_dir = sz_env;
  if (str_dir.empty()) return std::nullopt;
  if (!std::filesystem::exists(std::filesystem::path{str_dir + "/clear1.tem"}))
    return std::nullopt;
  return str_dir;
}

template <typename T>
std::span<const std::uint8_t> as_bytes(const T& str_bytes) {
  return std::span<const std::uint8_t>{reinterpret_cast<const std::uint8_t*>(str_bytes.data()),
      str_bytes.size()};
}

void run_clear1(std::string_view str_dir) {
  const auto opt_bytes{read_file_bytes(std::string{str_dir} + "/clear1.tem")};
  if (!opt_bytes.has_value()) {
    check(false, "clear1.tem unreadable");
    return;
  }
  check(opt_bytes->size() == 9289, std::format("clear1.tem size {} != 9289", opt_bytes->size()));

  const auto res{ora::game::render::tmp_ra_load(as_bytes(*opt_bytes), "clear1.tem")};
  if (!res) {
    check(false, std::format("clear1.tem decode: {}", res.error()));
    return;
  }
  check(res->int4_width == 24 && res->int4_height == 24, "clear1 tile 24x24");
  check(res->vec_frames.size() == 16, std::format("clear1 frames {} != 16", res->vec_frames.size()));

  const std::vector<std::uint8_t> vec1u_head{27, 27, 30, 26, 26, 28, 21, 23, 26, 30, 28, 26, 26,
      28, 28, 35, 28, 29, 27, 23, 23, 27, 27, 19};
  const std::vector<std::uint8_t> vec1u_tail{23, 23, 26, 26, 26, 30, 23, 29, 19, 19, 23, 27};
  const std::set<std::uint8_t> set1u_tile0{18, 19, 21, 23, 24, 26, 27, 28, 29, 30, 31, 32, 35, 36,
      141};
  const std::set<std::uint8_t> set1u_tile3{18, 19, 21, 23, 26, 27, 28, 29, 30, 31, 32, 36, 141};
  if (res->vec_frames.size() == 16) {
    const auto& vec1u_t0{res->vec_frames[0].vec1u_data};
    check(std::vector<std::uint8_t>(vec1u_t0.begin(), vec1u_t0.begin() + 24) == vec1u_head,
        "clear1 tile0 first 24 bytes");
    check(std::vector<std::uint8_t>(vec1u_t0.end() - 12, vec1u_t0.end()) == vec1u_tail,
        "clear1 tile0 last 12 bytes");
    check(std::set<std::uint8_t>(vec1u_t0.begin(), vec1u_t0.end()) == set1u_tile0,
        "clear1 tile0 index set");
    check(std::set<std::uint8_t>(res->vec_frames[3].vec1u_data.begin(),
              res->vec_frames[3].vec1u_data.end())
              == set1u_tile3,
        "clear1 tile3 index set");
  }
}

void run_1tnk(std::string_view str_dir) {
  const auto opt_bytes{read_file_bytes(std::string{str_dir} + "/1tnk.shp")};
  if (!opt_bytes.has_value()) {
    check(false, "1tnk.shp unreadable");
    return;
  }
  check(opt_bytes->size() == 8824, std::format("1tnk.shp size {} != 8824", opt_bytes->size()));

  const auto res{ora::game::render::shp_td_load(as_bytes(*opt_bytes), "1tnk.shp")};
  if (!res) {
    check(false, std::format("1tnk.shp decode: {}", res.error()));
    return;
  }
  check(res->int4_frame_width == 24 && res->int4_frame_height == 24, "1tnk frame 24x24");
  check(res->vec_frames.size() == 64, std::format("1tnk frames {} != 64", res->vec_frames.size()));

  const std::map<std::uint8_t, int> map1u_hist0{{0, 36}, {4, 29}, {12, 7}, {80, 6}, {82, 14},
      {83, 15}, {85, 25}, {86, 6}, {88, 16}, {89, 8}, {90, 3}, {91, 4}, {92, 11}, {93, 2}, {94, 10}};
  std::map<std::uint8_t, int> map1u_hist{};
  for (const std::uint8_t u1 : res->vec_frames[0].vec1u_data) ++map1u_hist[u1];
  check(map1u_hist == map1u_hist0, "1tnk frame0 trimmed histogram");

  const std::map<std::uint8_t, int> map1u_hist32{{0, 44}, {80, 2}, {81, 3}, {82, 5}, {84, 2},
      {85, 3}, {86, 3}, {89, 4}, {90, 1}, {91, 2}, {92, 5}, {93, 6}, {94, 2}, {95, 7}, {143, 7}};
  std::map<std::uint8_t, int> map1u_frame32{};
  for (const std::uint8_t u1 : res->vec_frames[32].vec1u_data) ++map1u_frame32[u1];
  check(map1u_frame32 == map1u_hist32, "1tnk frame32 trimmed histogram");

  const std::vector<std::uint8_t> vec1u_f0_tail{0, 0, 0, 4, 4, 4, 4, 4, 4, 4, 4, 0};
  check(std::vector<std::uint8_t>(res->vec_frames[0].vec1u_data.end() - 12,
              res->vec_frames[0].vec1u_data.end())
              == vec1u_f0_tail,
      "1tnk frame0 trimmed tail bytes");
  check(res->vec_frames[0].vec1u_data.size() == 192
          && res->vec_frames[0].vec1u_data[13] == 80,
      "1tnk frame0 first nonzero at trimmed index 13 == 80");
  const std::vector<std::uint8_t> vec1u_f32_head{0, 0, 0, 95, 143, 0, 0, 0, 0, 0, 0, 95};
  check(std::vector<std::uint8_t>(res->vec_frames[32].vec1u_data.begin(),
              res->vec_frames[32].vec1u_data.begin() + 12)
              == vec1u_f32_head,
      "1tnk frame32 trimmed head bytes");

  struct TrimExpect {
    std::size_t sz_frame{};
    int int4_w{};
    int int4_h{};
    int int4_ox{};
    int int4_oy{};
  };
  const TrimExpect vec_trim[]{
      {0, 12, 16, 0, 0}, {1, 14, 16, 0, 0}, {4, 22, 16, -1, 0}, {31, 14, 16, 0, 1},
      {32, 8, 12, 0, -6}, {63, 8, 12, 0, -6},
  };
  for (const TrimExpect& expect : vec_trim) {
    const auto& frm{res->vec_frames[expect.sz_frame]};
    check(frm.int4_width == expect.int4_w && frm.int4_height == expect.int4_h
            && frm.int4_off_x == expect.int4_ox && frm.int4_off_y == expect.int4_oy
            && frm.b_trimmed,
        std::format("1tnk frame{} trimmed {}x{} off {},{} vs expect {}x{} off {},{}",
            expect.sz_frame, frm.int4_width, frm.int4_height, frm.int4_off_x, frm.int4_off_y,
            expect.int4_w, expect.int4_h, expect.int4_ox, expect.int4_oy));
  }
}

void run_all_tem(std::string_view str_dir) {
  const std::string vec_str_files[]{
      "clear1.tem", "d01.tem", "d02.tem", "d03.tem", "d04.tem", "d05.tem", "d06.tem", "d07.tem",
      "d08.tem", "d09.tem", "d10.tem", "d11.tem", "d12.tem", "d13.tem", "d14.tem", "d15.tem",
      "d16.tem", "d17.tem", "d18.tem", "d19.tem", "d20.tem", "d21.tem", "d22.tem", "d23.tem",
      "d44.tem", "d45.tem", "falls1.tem", "falls2.tem", "p03.tem", "p13.tem", "p14.tem",
      "rf05.tem", "rf11.tem", "rv01.tem", "rv02.tem", "rv03.tem", "rv04.tem", "rv05.tem",
      "rv06.tem", "rv07.tem", "rv08.tem", "rv09.tem", "rv10.tem", "rv11.tem", "rv12.tem",
      "rv13.tem", "rv14.tem", "rv15.tem", "s01.tem", "s04.tem", "s05.tem", "s06.tem", "s07.tem",
      "s09.tem", "s10.tem", "s11.tem", "s13.tem", "s29.tem",
  };
  std::size_t sz_decoded{0};
  for (const std::string& str_name : vec_str_files) {
    const auto opt_bytes{read_file_bytes(std::string{str_dir} + "/" + str_name)};
    if (!opt_bytes.has_value()) {
      check(false, std::format("ore-lord 素材 {} 缺失", str_name));
      continue;
    }
    const auto res{ora::game::render::tmp_ra_load(as_bytes(*opt_bytes), str_name)};
    if (!res) {
      check(false, std::format("{} decode: {}", str_name, res.error()));
      continue;
    }
    check(res->vec_frames.size() > 0, std::format("{} 无帧", str_name));
    ++sz_decoded;
  }
  check(sz_decoded == std::size(vec_str_files),
      std::format("ore-lord 素材解码 {}/{}", sz_decoded, std::size(vec_str_files)));
}

}

int main() {
  const auto opt_dir{content_dir()};
  if (!opt_dir.has_value()) {
    std::println("render_assets: SKIP（无 EA 内容包抽取素材）");
    std::println("  获取方式：OpenRAUtility ra --extract <58 .tem + 1tnk.shp>（详见 docs/render-assets.md）");
    std::println("  或设 ORA_CONTENT_TEMPERAT 环境变量指向素材目录");
    return kSkip;
  }

  run_clear1(*opt_dir);
  run_1tnk(*opt_dir);
  run_all_tem(*opt_dir);

  return ora::test::test_exit_code("render_assets");
}
