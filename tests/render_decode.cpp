import std;

#include <cstdlib>

#include "game/render/lcw.hpp"
#include "game/render/palette.hpp"
#include "game/render/shp_td.hpp"
#include "game/tileset.hpp"
#include "game/render/tmp_ra.hpp"
#include "game/render/xor_delta.hpp"

#ifndef ORA_RENDER_GOLDEN_DIR
#define ORA_RENDER_GOLDEN_DIR "tests/golden/render"
#endif

namespace {

int int4_failures{0};
int int4_checks{0};
int int4_reports{0};

constexpr int kMaxReports{20};

void check(bool b_ok, std::string_view str_what) {
  ++int4_checks;
  if (!b_ok) {
    ++int4_failures;
    if (int4_reports < kMaxReports) {
      ++int4_reports;
      std::println("render_decode: {}", str_what);
    }
  }
}

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

void run_palette() {
  const auto opt_bytes{read_file_bytes(std::string{ORA_RENDER_GOLDEN_DIR} + "/temperat.pal")};
  if (!opt_bytes.has_value()) {
    check(false, "golden temperat.pal missing");
    return;
  }
  const std::span<const std::uint8_t> span1u_pal{
      reinterpret_cast<const std::uint8_t*>(opt_bytes->data()), opt_bytes->size()};

  check(opt_bytes->size() == 768, std::format("pal size {} != 768", opt_bytes->size()));
  std::uint8_t u1_max{0};
  for (const std::uint8_t u1_b : *opt_bytes) u1_max = u1_max > u1_b ? u1_max : u1_b;
  check(u1_max == 63, std::format("pal max byte {} != 63 (6-bit source)", u1_max));

  const auto res_palette{ora::game::render::palette_from_pal_bytes(span1u_pal, "temperat.pal")};
  if (!res_palette) {
    check(false, std::format("palette_from_pal_bytes error: {}", res_palette.error()));
    return;
  }
  const auto& arr4u{res_palette->arr4u_colors};
  const std::pair<int, std::uint32_t> vec_anchor[]{
      {0, 0xFF000000u},   {1, 0xFFAA00AAu},  {3, 0xFF00AA00u},  {4, 0xFF59FF55u},
      {16, 0xFF100C0Cu},  {80, 0xFFF7D779u}, {95, 0xFF282008u}, {96, 0xFF5DC7DFu},
      {127, 0xFF005900u}, {255, 0xFFFFFFFFu},
  };
  for (const auto& [int4_idx, u4_expected] : vec_anchor)
    check(arr4u[static_cast<std::size_t>(int4_idx)] == u4_expected,
        std::format("pal[{}] = 0x{:08X} != 0x{:08X}", int4_idx,
            arr4u[static_cast<std::size_t>(int4_idx)], u4_expected));

  auto pal_shadow{res_palette.value()};
  const int vec4_transparent[]{0};
  const int vec4_shadow[]{3, 4};
  ora::game::render::palette_remap_indexes(pal_shadow, vec4_transparent, vec4_shadow);
  check(pal_shadow.arr4u_colors[0] == 0u, "remap transparent[0] != 0");
  check(pal_shadow.arr4u_colors[3] == (140u << 24), "remap shadow[3] != 140 alpha");
  check(pal_shadow.arr4u_colors[4] == (140u << 24), "remap shadow[4] != 140 alpha");
  check(pal_shadow.arr4u_colors[1] == 0xFFAA00AAu, "remap keeps untouched entries");

  const std::uint8_t vec1u_short[767]{};
  check(!ora::game::render::palette_from_pal_bytes(vec1u_short, "short.pal").has_value(),
      "767-byte pal must be rejected");
}

void run_lcw() {
  struct LcwCase {
    std::string str_what;
    std::vector<std::uint8_t> vec1u_src;
    std::size_t sz_dest_size{};
    std::size_t sz_expected_written{};
    std::vector<std::uint8_t> vec1u_expected;
  };
  const LcwCase vec_cases[]{
      {"case1 literal", {0x85, 1, 2, 3, 4, 5, 0x80}, 8, 5, {1, 2, 3, 4, 5, 0, 0, 0}},
      {"case4 rle fill", {0xFE, 0x05, 0x00, 9, 0x80}, 8, 5, {9, 9, 9, 9, 9, 0, 0, 0}},
      {"case2 short backref dist1", {0x81, 7, 0x00, 0x01, 0x80}, 8, 4, {7, 7, 7, 7, 0, 0, 0, 0}},
      {"case3 long backref", {0x81, 5, 0xC1, 0x00, 0x00, 0x80}, 8, 5, {5, 5, 5, 5, 5, 0, 0, 0}},
      {"case5 far copy", {0x83, 3, 4, 6, 0xFF, 0x02, 0x00, 0x00, 0x00, 0x80}, 8, 5,
       {3, 4, 6, 3, 4, 0, 0, 0}},
      {"case2 overflow silent stop", {0x82, 1, 2, 0x00, 0x01, 0x80}, 4, 2, {1, 2, 0, 0}},
      {"multi-block case1+case4+case1",
       {0x82, 8, 8, 0xFE, 0x03, 0x00, 7, 0x81, 9, 0x80}, 8, 6, {8, 8, 7, 7, 7, 9, 0, 0}},
  };
  for (const LcwCase& kase : vec_cases) {
    std::vector<std::uint8_t> vec1u_dest(kase.sz_dest_size, 0);
    const auto res{ora::game::render::lcw_decode_into(kase.vec1u_src, 0, vec1u_dest)};
    if (!res) {
      check(false, std::format("lcw {}: unexpected error {}", kase.str_what, res.error()));
      continue;
    }
    check(*res == kase.sz_expected_written,
        std::format("lcw {}: written {} != {}", kase.str_what, *res, kase.sz_expected_written));
    check(vec1u_dest == kase.vec1u_expected,
        std::format("lcw {}: dest bytes differ", kase.str_what));
  }

  std::vector<std::uint8_t> vec1u_dest(8, 0);
  const std::vector<std::uint8_t> vec1u_bad_ref{0x81, 5, 0xC1, 0x01, 0x00, 0x80};
  const std::vector<std::uint8_t> vec1u_short{0x81, 5};
  check(!ora::game::render::lcw_decode_into(vec1u_bad_ref, 0, vec1u_dest).has_value(),
      "lcw case3 srcIndex >= destIndex must error");
  check(!ora::game::render::lcw_decode_into(vec1u_short, 8, vec1u_dest).has_value(),
      "lcw src_offset past end must error");
  const std::vector<std::uint8_t> vec1u_bad_rpos{0x41, 0x00};
  check(!ora::game::render::lcw_decode_into(vec1u_bad_rpos, 0, vec1u_dest).has_value(),
      "lcw case2 rpos > destIndex must error");
}

}

void run_xor_delta() {
  struct XorCase {
    std::string str_what;
    std::vector<std::uint8_t> vec1u_src;
    std::vector<std::uint8_t> vec1u_dest_init;
    std::size_t sz_expected_written{};
    std::vector<std::uint8_t> vec1u_expected;
  };
  const XorCase vec_cases[]{
      {"case5 short xor literals", {0x02, 0x0F, 0xF0, 0x80, 0x00, 0x00}, {0xFF, 0xFF}, 2,
       {0xF0, 0x0F}},
      {"case6+case1+case6", {0x00, 0x02, 0x03, 0x81, 0x00, 0x02, 0x05, 0x80, 0x00, 0x00},
       {1, 1, 1, 1, 1, 1}, 5, {2, 2, 1, 4, 4, 1}},
      {"case2 skip+case3 xor literals+case4 xor value",
       {0x80, 0x02, 0x00, 0x80, 0x03, 0x80, 1, 2, 3, 0x80, 0x03, 0xC0, 0x0F, 0x80, 0x00, 0x00},
       {0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F}, 8,
       {0x0F, 0x0F, 0x0E, 0x0D, 0x0C, 0x00, 0x00, 0x00}},
  };
  for (const XorCase& kase : vec_cases) {
    std::vector<std::uint8_t> vec1u_dest{kase.vec1u_dest_init};
    const auto res{ora::game::render::xor_delta_decode_into(kase.vec1u_src, 0, vec1u_dest)};
    if (!res) {
      check(false, std::format("xor {}: unexpected error {}", kase.str_what, res.error()));
      continue;
    }
    check(*res == kase.sz_expected_written,
        std::format("xor {}: written {} != {}", kase.str_what, *res, kase.sz_expected_written));
    check(vec1u_dest == kase.vec1u_expected, std::format("xor {}: dest bytes differ", kase.str_what));
  }

  std::vector<std::uint8_t> vec1u_dest{0, 0};
  const std::vector<std::uint8_t> vec1u_overflow{0x05, 1, 2, 3, 4, 5, 0x80, 0x00, 0x00};
  check(!ora::game::render::xor_delta_decode_into(vec1u_overflow, 0, vec1u_dest).has_value(),
      "xor case5 overrun must error");
  check(!ora::game::render::xor_delta_decode_into(vec1u_overflow, 40, vec1u_dest).has_value(),
      "xor src_offset past end must error");
}

void run_tmp_ra() {
  const std::vector<std::uint8_t> vec1u_tem{
      2, 0, 2, 0, 0, 0, 0, 0, 0, 255, 1, 0, 0, 0, 0, 0, 40, 0, 0, 0, 0, 0, 0, 0, 0, 0, 115, 44,
      11, 0, 0, 0, 0, 0, 0, 0, 8, 0, 0, 0, 10, 11, 12, 13, 20, 21, 22, 23};
  const auto res{ora::game::render::tmp_ra_load(vec1u_tem, "synthetic.tem")};
  if (!res) {
    check(false, std::format("tmp_ra synthetic: error {}", res.error()));
    return;
  }
  check(res->int4_width == 2 && res->int4_height == 2, "tmp_ra synthetic size");
  check(res->vec_frames.size() == 3, std::format("tmp_ra frames {} != 3", res->vec_frames.size()));
  if (res->vec_frames.size() == 3) {
    check(res->vec_frames[0].vec1u_data == std::vector<std::uint8_t>{10, 11, 12, 13},
        "tmp_ra frame0 pixels");
    check(res->vec_frames[0].int4_width == 2 && res->vec_frames[0].int4_height == 2,
        "tmp_ra frame0 size");
    check(res->vec_frames[1].int4_width == 0 && res->vec_frames[1].vec1u_data.empty(),
        "tmp_ra frame1 missing (255)");
    check(res->vec_frames[2].vec1u_data == std::vector<std::uint8_t>{20, 21, 22, 23},
        "tmp_ra frame2 pixels");
  }

  std::vector<std::uint8_t> vec1u_bad_sig{vec1u_tem};
  vec1u_bad_sig[26] = 0;
  vec1u_bad_sig[27] = 0;
  check(!ora::game::render::tmp_ra_load(vec1u_bad_sig, "badsig.tem").has_value(),
      "tmp_ra signature 0x2c73 required");

  std::vector<std::uint8_t> vec1u_trunc{vec1u_tem.begin(), vec1u_tem.begin() + 44};
  check(!ora::game::render::tmp_ra_load(vec1u_trunc, "trunc.tem").has_value(),
      "tmp_ra truncated tile data must error");

  std::vector<std::uint8_t> vec1u_short(20, 0);
  check(!ora::game::render::tmp_ra_load(vec1u_short, "short.tem").has_value(),
      "tmp_ra short header must error");
}

void run_shp_td() {
  const std::vector<std::uint8_t> vec1u_shp{
      2, 0, 0, 0, 0, 0, 4, 0, 2, 0, 0, 0, 0, 0,
      46, 0, 0, 128, 0, 0, 0, 0, 51, 0, 0, 32, 0, 0,
      0, 0, 58, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 254, 8, 0, 7, 128, 2, 7, 7, 134, 128,
      0, 0};
  const auto res{ora::game::render::shp_td_load(vec1u_shp, "synthetic.shp")};
  if (!res) {
    check(false, std::format("shp_td synthetic: error {}", res.error()));
    return;
  }
  check(res->int4_frame_width == 4 && res->int4_frame_height == 2, "shp_td frame size");
  check(res->vec_frames.size() == 2, std::format("shp_td frames {} != 2", res->vec_frames.size()));
  if (res->vec_frames.size() == 2) {
    const auto& frm0{res->vec_frames[0]};
    check(frm0.int4_width == 4 && frm0.int4_height == 2 && !frm0.b_trimmed && frm0.int4_off_x == 0
            && frm0.int4_off_y == 0,
        "shp_td frame0 notrim");
    check(frm0.vec1u_data == std::vector<std::uint8_t>(8, 7), "shp_td frame0 pixels");
    const auto& frm1{res->vec_frames[1]};
    check(frm1.int4_width == 4 && frm1.int4_height == 2 && !frm1.b_trimmed,
        "shp_td frame1 full box stays notrim");
    check(frm1.vec1u_data
              == std::vector<std::uint8_t>{0, 0, 7, 7, 7, 7, 7, 7},
        "shp_td frame1 pixels");
  }

  const std::vector<std::uint8_t> vec1u_trim{
      2, 0, 0, 0, 0, 0, 4, 0, 2, 0, 0, 0, 0, 0, 46, 0, 0, 128, 0, 0, 0, 0, 51, 0, 0, 32, 0, 0,
      0, 0, 62, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 254, 8, 0, 7, 128, 0, 2, 7, 130,
      0, 2, 7, 130, 128, 0, 0};
  const auto res_trim{ora::game::render::shp_td_load(vec1u_trim, "trim.shp")};
  if (!res_trim) {
    check(false, std::format("shp_td trim: error {}", res_trim.error()));
    return;
  }
  if (res_trim->vec_frames.size() == 2) {
    const auto& frm1t{res_trim->vec_frames[1]};
    check(frm1t.int4_width == 2 && frm1t.int4_height == 2 && frm1t.b_trimmed && frm1t.int4_off_x == 1
            && frm1t.int4_off_y == 0,
        std::format("shp_td trim frame {}x{} off {},{}", frm1t.int4_width, frm1t.int4_height,
            frm1t.int4_off_x, frm1t.int4_off_y));
    check(frm1t.vec1u_data == std::vector<std::uint8_t>{7, 7, 7, 7}, "shp_td trim frame pixels");
  } else {
    check(false, "shp_td trim frames != 2");
  }

  const std::vector<std::uint8_t> vec1u_empty{
      1, 0, 0, 0, 0, 0, 4, 0, 2, 0, 0, 0, 0, 0,
      38, 0, 0, 128, 0, 0, 0, 0, 43, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 254, 8, 0, 0,
      128};
  const auto res_empty{ora::game::render::shp_td_load(vec1u_empty, "empty.shp")};
  if (!res_empty) {
    check(false, std::format("shp_td empty: error {}", res_empty.error()));
    return;
  }
  check(res_empty->vec_frames.size() == 1 && res_empty->vec_frames[0].int4_width == 0
          && res_empty->vec_frames[0].vec1u_data.empty(),
      "shp_td all-zero frame trims to empty");

  const std::vector<std::uint8_t> vec1u_bad_ref{
      1, 0, 0, 0, 0, 0, 4, 0, 2, 0, 0, 0, 0, 0, 38, 0, 0, 64, 231, 3, 0, 0, 38, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 128, 0, 0};
  check(!ora::game::render::shp_td_load(vec1u_bad_ref, "badref.shp").has_value(),
      "shp_td XORLCW dangling ref must error");

  std::vector<std::uint8_t> vec1u_bad_eof{vec1u_shp};
  vec1u_bad_eof[30] = 57;
  check(!ora::game::render::shp_td_load(vec1u_bad_eof, "badeof.shp").has_value(),
      "shp_td eof mismatch must be rejected");
}

void run_tileset_images() {
  const auto opt_yaml{read_file_bytes(std::string{ORA_MAPS_GOLDEN_DIR} + "/tilesets/temperat.yaml")};
  if (!opt_yaml.has_value()) {
    check(false, "golden temperat.yaml missing");
    return;
  }
  const auto res{ora::game::tileset_load(*opt_yaml, "temperat.yaml")};
  if (!res) {
    check(false, std::format("temperat.yaml load: {}", res.error()));
    return;
  }

  std::size_t sz_entries{0};
  std::set<std::string> set_str_files;
  for (const auto& tpl : res->vec_templates) {
    sz_entries += tpl.vec_str_images.size();
    for (const std::string& str_file : tpl.vec_str_images) set_str_files.insert(str_file);
  }
  check(sz_entries == 308, std::format("temperat images entries {} != 308", sz_entries));
  check(set_str_files.size() == 306, std::format("temperat unique images {} != 306", set_str_files.size()));

  const std::string vec_str_orelord[]{
      "clear1.tem", "d01.tem", "d02.tem", "d03.tem", "d04.tem", "d05.tem", "d06.tem", "d07.tem",
      "d08.tem", "d09.tem", "d10.tem", "d11.tem", "d12.tem", "d13.tem", "d14.tem", "d15.tem",
      "d16.tem", "d17.tem", "d18.tem", "d19.tem", "d20.tem", "d21.tem", "d22.tem", "d23.tem",
      "d44.tem", "d45.tem", "falls1.tem", "falls2.tem", "p03.tem", "p13.tem", "p14.tem",
      "rf05.tem", "rf11.tem", "rv01.tem", "rv02.tem", "rv03.tem", "rv04.tem", "rv05.tem",
      "rv06.tem", "rv07.tem", "rv08.tem", "rv09.tem", "rv10.tem", "rv11.tem", "rv12.tem",
      "rv13.tem", "rv14.tem", "rv15.tem", "s01.tem", "s04.tem", "s05.tem", "s06.tem", "s07.tem",
      "s09.tem", "s10.tem", "s11.tem", "s13.tem", "s29.tem",
  };
  for (const std::string& str_need : vec_str_orelord)
    check(set_str_files.count(str_need) != 0, std::format("ore-lord 素材 {} 不在 tileset images", str_need));

  const ora::game::TerrainTemplate* tpl_clear{res->find_template(255)};
  check(tpl_clear != nullptr && tpl_clear->vec_str_images.size() == 1
          && tpl_clear->vec_str_images[0] == "clear1.tem" && tpl_clear->b_pickany,
      "模板 255 = clear1.tem PickAny");
  const ora::game::TerrainTemplate* tpl_wild{res->find_template(65535)};
  check(tpl_wild != nullptr && tpl_wild->vec_str_images.size() == 1
          && tpl_wild->vec_str_images[0] == "clear1.tem",
      "模板 65535 = clear1.tem");

  const std::string str_multi_yaml{
      "General:\n"
      "\tTileSize: 24,24\n"
      "Terrain:\n"
      "\tTerrainType@Clear:\n"
      "\t\tType: Clear\n"
      "\tTerrainType@Road:\n"
      "\t\tType: Road\n"
      "Templates:\n"
      "\tTemplate@1:\n"
      "\t\tId: 1\n"
      "\t\tImages: a.tem, b.tem\n"
      "\t\tSize: 1,1\n"
      "\t\tTiles:\n"
      "\t\t\t0: Clear\n"
      "\t\t\t1: Road\n"
      "\t\tPickAny: True\n"};
  const auto res_multi{ora::game::tileset_load(str_multi_yaml, "multi.yaml")};
  if (!res_multi) {
    check(false, std::format("multi yaml load: {}", res_multi.error()));
    return;
  }
  check(res_multi->vec_templates.size() == 1 && res_multi->vec_templates[0].vec_str_images.size() == 2
          && res_multi->vec_templates[0].vec_str_images[0] == "a.tem"
          && res_multi->vec_templates[0].vec_str_images[1] == "b.tem",
      "多 Images CSV 解析");
}

int main() {
  run_palette();
  run_lcw();
  run_xor_delta();
  run_tmp_ra();
  run_shp_td();
  run_tileset_images();

  if (int4_failures != 0) {
    std::println("render_decode: {} checks failed", int4_failures);
    return 1;
  }
  std::println("render_decode: all checks passed ({} checks)", int4_checks);
  return 0;
}
