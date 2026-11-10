import std;

#include <cstdio>
import ora.game.miniyaml;
import ora.game.render;
import ora.test.util;

using ora::test::check;

#ifndef ORA_FONTS_GOLDEN_DIR
#define ORA_FONTS_GOLDEN_DIR "tests/golden/fonts"
#endif

namespace {

namespace fr = ora::game::render;

constexpr std::string_view kGoldenDir{ORA_FONTS_GOLDEN_DIR};

struct FontSpec {
  std::string_view str_font;
  int int4_size;
};

constexpr FontSpec kMetricsSpecs[]{
    {"FreeSans", 10}, {"FreeSans", 12}, {"FreeSans", 14},
    {"FreeSans", 18}, {"FreeSans", 24}, {"FreeSansBold", 14},
};

constexpr std::string_view kMiscDumpName{"ft-misc-FreeSans-14.bin"};
constexpr int kMiscSize{14};
constexpr int kMiscCodepoints[]{0x20, 0xA0, 0x7F, 0x4E2D, 0x263A, 0xD83D, 0x30};

struct DumpGlyph {
  int int4_cp{};
  int int4_w{};
  int int4_h{};
  float fp4_adv{};
  int int4_off_x{};
  int int4_off_y{};
  std::vector<std::uint8_t> vec1u_data;
};

struct DumpSet {
  int int4_size{};
  int int4_upem{};
  std::vector<DumpGlyph> vec_glyphs;
};

std::vector<std::uint8_t> read_bytes(std::string_view str_path) {
  std::ifstream fs{std::string{str_path}, std::ios::binary};
  return std::vector<std::uint8_t>{
      std::istreambuf_iterator<char>{fs}, std::istreambuf_iterator<char>{}};
}

std::vector<std::string> read_lines(std::string_view str_path) {
  std::vector<std::string> vec_out;
  std::ifstream fs{std::string{str_path}, std::ios::binary};
  std::string str_line;
  while (std::getline(fs, str_line)) {
    if (!str_line.empty() && str_line.back() == '\r') str_line.pop_back();
    vec_out.push_back(std::move(str_line));
  }
  return vec_out;
}

std::u16string unescape_ascii(std::string_view str_text) {
  std::u16string str_out;
  for (std::size_t sz_i{0}; sz_i < str_text.size(); ++sz_i) {
    if (str_text[sz_i] != '\\') {
      str_out.push_back(static_cast<char16_t>(static_cast<unsigned char>(str_text[sz_i])));
      continue;
    }
    ++sz_i;
    if (sz_i >= str_text.size()) break;
    switch (str_text[sz_i]) {
      case 'n': str_out.push_back(u'\n'); break;
      case 't': str_out.push_back(u'\t'); break;
      case 'r': str_out.push_back(u'\r'); break;
      case '\\': str_out.push_back(u'\\'); break;
      default: break;
    }
  }
  return str_out;
}

void write_u32le(std::vector<std::uint8_t>& vec1u_out, std::uint32_t u4_v) {
  vec1u_out.push_back(static_cast<std::uint8_t>(u4_v));
  vec1u_out.push_back(static_cast<std::uint8_t>(u4_v >> 8));
  vec1u_out.push_back(static_cast<std::uint8_t>(u4_v >> 16));
  vec1u_out.push_back(static_cast<std::uint8_t>(u4_v >> 24));
}

void write_f32le(std::vector<std::uint8_t>& vec1u_out, float fp4_v) {
  write_u32le(vec1u_out, std::bit_cast<std::uint32_t>(fp4_v));
}

[[nodiscard]] bool read_u32le(
    std::span<const std::uint8_t> span1u, std::size_t& sz_off, std::uint32_t& u4_out) {
  if (sz_off + 4 > span1u.size()) return false;
  u4_out = static_cast<std::uint32_t>(span1u[sz_off])
      | (static_cast<std::uint32_t>(span1u[sz_off + 1]) << 8)
      | (static_cast<std::uint32_t>(span1u[sz_off + 2]) << 16)
      | (static_cast<std::uint32_t>(span1u[sz_off + 3]) << 24);
  sz_off += 4;
  return true;
}

std::expected<DumpSet, std::string> parse_dump(std::span<const std::uint8_t> span1u_bytes) {
  if (span1u_bytes.size() < 18) return std::unexpected("dump too small");
  const std::string_view str_magic{reinterpret_cast<const char*>(span1u_bytes.data()), 6};
  if (str_magic != "FTD1u8" && str_magic != "STB1u8")
    return std::unexpected(std::format("bad magic {}", str_magic));

  std::size_t sz_off{6};
  std::uint32_t u4_count{};
  if (!read_u32le(span1u_bytes, sz_off, u4_count) || u4_count != 1)
    return std::unexpected("unexpected size count");
  std::uint32_t u4_size{};
  std::uint32_t u4_cp_count{};
  if (!read_u32le(span1u_bytes, sz_off, u4_size) || !read_u32le(span1u_bytes, sz_off, u4_cp_count))
    return std::unexpected("truncated header");

  DumpSet set;
  set.int4_size = static_cast<int>(u4_size);
  set.vec_glyphs.reserve(u4_cp_count);
  for (std::uint32_t u4_i{0}; u4_i < u4_cp_count; ++u4_i) {
    std::uint32_t u4_cp{};
    std::uint32_t u4_w{};
    std::uint32_t u4_h{};
    std::uint32_t u4_adv{};
    std::uint32_t u4_off_x{};
    std::uint32_t u4_off_y{};
    if (!read_u32le(span1u_bytes, sz_off, u4_cp) || !read_u32le(span1u_bytes, sz_off, u4_w)
        || !read_u32le(span1u_bytes, sz_off, u4_h) || !read_u32le(span1u_bytes, sz_off, u4_adv)
        || !read_u32le(span1u_bytes, sz_off, u4_off_x) || !read_u32le(span1u_bytes, sz_off, u4_off_y))
      return std::unexpected("truncated glyph record");
    DumpGlyph glyph;
    glyph.int4_cp = static_cast<int>(u4_cp);
    glyph.int4_w = static_cast<int>(u4_w);
    glyph.int4_h = static_cast<int>(u4_h);
    glyph.fp4_adv = std::bit_cast<float>(u4_adv);
    glyph.int4_off_x = static_cast<int>(u4_off_x);
    glyph.int4_off_y = static_cast<int>(u4_off_y);
    const std::size_t sz_pixels{static_cast<std::size_t>(u4_w) * u4_h};
    if (sz_off + sz_pixels > span1u_bytes.size()) return std::unexpected("truncated bitmap");
    glyph.vec1u_data.assign(span1u_bytes.begin() + static_cast<std::ptrdiff_t>(sz_off),
        span1u_bytes.begin() + static_cast<std::ptrdiff_t>(sz_off + sz_pixels));
    sz_off += sz_pixels;
    set.vec_glyphs.push_back(std::move(glyph));
  }

  std::uint32_t u4_upem{};
  if (read_u32le(span1u_bytes, sz_off, u4_upem)) set.int4_upem = static_cast<int>(u4_upem);
  return set;
}

[[nodiscard]] std::vector<std::uint8_t> build_stb_dump(const DumpSet& set) {
  std::vector<std::uint8_t> vec1u_out;
  vec1u_out.reserve(64 * 1024);
  const auto push_str{[&](std::string_view str_v) {
    vec1u_out.insert(vec1u_out.end(), str_v.begin(), str_v.end());
  }};
  push_str("STB1u8");
  write_u32le(vec1u_out, 1);
  write_u32le(vec1u_out, static_cast<std::uint32_t>(set.int4_size));
  write_u32le(vec1u_out, static_cast<std::uint32_t>(set.vec_glyphs.size()));
  for (const DumpGlyph& glyph : set.vec_glyphs) {
    write_u32le(vec1u_out, static_cast<std::uint32_t>(glyph.int4_cp));
    write_u32le(vec1u_out, static_cast<std::uint32_t>(glyph.int4_w));
    write_u32le(vec1u_out, static_cast<std::uint32_t>(glyph.int4_h));
    write_f32le(vec1u_out, glyph.fp4_adv);
    write_u32le(vec1u_out, static_cast<std::uint32_t>(glyph.int4_off_x));
    write_u32le(vec1u_out, static_cast<std::uint32_t>(glyph.int4_off_y));
    vec1u_out.insert(vec1u_out.end(), glyph.vec1u_data.begin(), glyph.vec1u_data.end());
  }
  write_u32le(vec1u_out, static_cast<std::uint32_t>(set.int4_upem));
  write_u32le(vec1u_out, 0);
  write_u32le(vec1u_out, 0);
  write_u32le(vec1u_out, 0);
  return vec1u_out;
}

[[nodiscard]] DumpSet live_stb_dump(
    std::span<const std::uint8_t> span1u_ttf, int int4_size, const int* ptr_cps, std::size_t sz_cp_count) {
  const fr::FontFace face{std::move(*fr::font_face_load(span1u_ttf))};
  DumpSet set;
  set.int4_size = int4_size;
  set.int4_upem = face.int4_upem;
  set.vec_glyphs.reserve(sz_cp_count);
  for (std::size_t sz_i{0}; sz_i < sz_cp_count; ++sz_i) {
    const fr::FontGlyph glyph{fr::font_create_glyph(face, static_cast<char16_t>(ptr_cps[sz_i]), int4_size, 1.0f)};
    DumpGlyph rec;
    rec.int4_cp = ptr_cps[sz_i];
    rec.int4_w = glyph.int4_width;
    rec.int4_h = glyph.int4_height;
    rec.fp4_adv = glyph.fp4_advance;
    rec.int4_off_x = glyph.int4_off_x;
    rec.int4_off_y = glyph.int4_off_y;
    rec.vec1u_data = glyph.vec1u_data;
    set.vec_glyphs.push_back(std::move(rec));
  }
  return set;
}

[[nodiscard]] std::vector<int> ascii_codepoints() {
  std::vector<int> vec_cps;
  vec_cps.reserve(0x7f - 0x20);
  for (int int4_cp{0x20}; int4_cp < 0x7f; ++int4_cp) vec_cps.push_back(int4_cp);
  return vec_cps;
}

std::string golden_path(std::string_view str_rel) {
  return std::format("{}/{}", kGoldenDir, str_rel);
}

[[nodiscard]] bool file_exists(std::string_view str_path) {
  std::ifstream fs{std::string{str_path}, std::ios::binary};
  return static_cast<bool>(fs);
}

[[nodiscard]] std::vector<std::string> split_tab(std::string_view str_line) {
  std::vector<std::string> vec_fields;
  std::size_t idx_begin{0};
  while (true) {
    const std::size_t idx_tab{str_line.find('\t', idx_begin)};
    if (idx_tab == std::string_view::npos) {
      vec_fields.emplace_back(str_line.substr(idx_begin));
      return vec_fields;
    }
    vec_fields.emplace_back(str_line.substr(idx_begin, idx_tab - idx_begin));
    idx_begin = idx_tab + 1;
  }
}

[[nodiscard]] int parse_int_ascii(std::string_view str_text) {
  int int4_out{};
  std::from_chars(str_text.data(), str_text.data() + str_text.size(), int4_out);
  return int4_out;
}

int run_gen_golden() {
  const std::vector<int> vec_ascii{ascii_codepoints()};
  for (const FontSpec& spec : kMetricsSpecs) {
    const auto vec1u_ttf{read_bytes(golden_path(
        std::format("fixture/{}.ttf", spec.str_font)))};
    if (vec1u_ttf.empty()) {
      std::println(stderr, "gen-golden: fixture missing: {}", spec.str_font);
      return 3;
    }
    const DumpSet set{live_stb_dump(vec1u_ttf, spec.int4_size, vec_ascii.data(), vec_ascii.size())};
    const auto vec1u_bytes{build_stb_dump(set)};
    const std::string str_path{golden_path(std::format("stb-{}-{}.bin", spec.str_font, spec.int4_size))};
    std::ofstream fs{str_path, std::ios::binary};
    fs.write(reinterpret_cast<const char*>(vec1u_bytes.data()),
        static_cast<std::streamsize>(vec1u_bytes.size()));
    fs.close();
    std::println("gen-golden: {} bytes={}", str_path, vec1u_bytes.size());
  }
  {
    const auto vec1u_ttf{read_bytes(golden_path("fixture/FreeSans.ttf"))};
    const DumpSet set{live_stb_dump(
        vec1u_ttf, kMiscSize, kMiscCodepoints, std::size(kMiscCodepoints))};
    const auto vec1u_bytes{build_stb_dump(set)};
    std::ofstream fs{golden_path("stb-misc-FreeSans-14.bin"), std::ios::binary};
    fs.write(reinterpret_cast<const char*>(vec1u_bytes.data()),
        static_cast<std::streamsize>(vec1u_bytes.size()));
    std::println("gen-golden: stb-misc-FreeSans-14.bin bytes={}", vec1u_bytes.size());
  }
  return 0;
}

struct StringsSet {
  std::vector<std::u16string> vec_texts;
};

[[nodiscard]] StringsSet load_strings() {
  StringsSet set;
  for (const std::string& str_line : read_lines(golden_path("strings.txt"))) {
    if (str_line.empty() || str_line.front() == '#') continue;
    const std::size_t idx_tab{str_line.find('\t')};
    if (idx_tab == std::string::npos) continue;
    set.vec_texts.push_back(unescape_ascii(std::string_view{str_line}.substr(idx_tab + 1)));
  }
  return set;
}

struct MeasureRec {
  int int4_index{};
  int int4_w{};
  int int4_h{};
};

[[nodiscard]] std::vector<MeasureRec> load_measure(std::string_view str_rel) {
  std::vector<MeasureRec> vec_out;
  for (const std::string& str_line : read_lines(golden_path(str_rel))) {
    if (str_line.empty() || str_line.front() == '#') continue;
    const std::vector<std::string> vec_fields{split_tab(str_line)};
    if (vec_fields.size() < 3) continue;
    MeasureRec rec;
    rec.int4_w = parse_int_ascii(vec_fields[1]);
    rec.int4_h = parse_int_ascii(vec_fields[2]);
    rec.int4_index = static_cast<int>(vec_out.size());
    vec_out.push_back(rec);
  }
  return vec_out;
}

[[nodiscard]] std::vector<std::u16string> load_wrap(std::string_view str_rel) {
  std::vector<std::u16string> vec_out;
  for (const std::string& str_line : read_lines(golden_path(str_rel))) {
    if (str_line.empty() || str_line.front() == '#') continue;
    const std::size_t idx_tab{str_line.find('\t')};
    vec_out.push_back(unescape_ascii(
        idx_tab == std::string::npos ? std::string_view{} : std::string_view{str_line}.substr(idx_tab + 1)));
  }
  return vec_out;
}

[[nodiscard]] std::vector<std::pair<std::string, fr::FontDataEntry>> load_manifest() {
  const auto res_yaml{ora::game::miniyaml_from_file(golden_path("ra-mod.yaml"), "ra-mod.yaml")};
  if (!res_yaml) return {};
  const auto res_fonts{fr::fonts_load(*res_yaml)};
  if (!res_fonts) return {};
  return *res_fonts;
}

struct PixelStats {
  long long lg4_sum{};
  long long lg4_count{};
  long long lg4_over32{};
  long long lg4_max{};
};

void accumulate_same_box(PixelStats& st, const DumpGlyph& ft, const DumpGlyph& stb) {
  if (ft.int4_w != stb.int4_w || ft.int4_h != stb.int4_h) return;
  const std::size_t sz_n{ft.vec1u_data.size()};
  for (std::size_t sz_i{0}; sz_i < sz_n; ++sz_i) {
    const int int4_d{std::abs(static_cast<int>(ft.vec1u_data[sz_i]) - static_cast<int>(stb.vec1u_data[sz_i]))};
    st.lg4_sum += int4_d;
    ++st.lg4_count;
    if (int4_d > 32) ++st.lg4_over32;
    if (int4_d > st.lg4_max) st.lg4_max = int4_d;
  }
}

void test_layer1_self_golden() {
  const std::vector<int> vec_ascii{ascii_codepoints()};
  for (const FontSpec& spec : kMetricsSpecs) {
    const std::string str_golden{golden_path(std::format("stb-{}-{}.bin", spec.str_font, spec.int4_size))};
    const auto vec1u_ttf_a{read_bytes(golden_path(std::format("fixture/{}.ttf", spec.str_font)))};
    const auto vec1u_ttf_b{read_bytes(golden_path(std::format("fixture/{}.ttf", spec.str_font)))};
    check(!vec1u_ttf_a.empty(), std::format("layer1: fixture {} readable", spec.str_font));

    const DumpSet first{live_stb_dump(vec1u_ttf_a, spec.int4_size, vec_ascii.data(), vec_ascii.size())};
    const DumpSet second{live_stb_dump(vec1u_ttf_b, spec.int4_size, vec_ascii.data(), vec_ascii.size())};
    const auto vec1u_first{build_stb_dump(first)};
    const auto vec1u_second{build_stb_dump(second)};
    check(vec1u_first == vec1u_second,
        std::format("layer1: {} @{} double-run byte-identical", spec.str_font, spec.int4_size));

    const auto vec1u_golden{read_bytes(str_golden)};
    check(!vec1u_golden.empty(), std::format("layer1: golden {} present", str_golden));
    check(vec1u_first == vec1u_golden,
        std::format("layer1: {} @{} matches committed golden", spec.str_font, spec.int4_size));
  }
  {
    const auto vec1u_ttf{read_bytes(golden_path("fixture/FreeSans.ttf"))};
    const DumpSet set{live_stb_dump(vec1u_ttf, kMiscSize, kMiscCodepoints, std::size(kMiscCodepoints))};
    const auto vec1u_live{build_stb_dump(set)};
    const auto vec1u_golden{read_bytes(golden_path("stb-misc-FreeSans-14.bin"))};
    check(vec1u_live == vec1u_golden, "layer1: misc cp set matches committed golden");
  }
}

void test_layer2_metrics() {
  const std::vector<int> vec_ascii{ascii_codepoints()};
  for (const FontSpec& spec : kMetricsSpecs) {
    const auto vec1u_ttf{read_bytes(golden_path(std::format("fixture/{}.ttf", spec.str_font)))};
    const DumpSet live{live_stb_dump(vec1u_ttf, spec.int4_size, vec_ascii.data(), vec_ascii.size())};
    const auto res_ft{parse_dump(read_bytes(golden_path(
        std::format("ft-{}-{}.bin", spec.str_font, spec.int4_size))))};
    check(res_ft.has_value(), std::format("layer2: ft dump {} @{} parses", spec.str_font, spec.int4_size));
    if (!res_ft) continue;
    const DumpSet& ft{*res_ft};
    check(ft.vec_glyphs.size() == live.vec_glyphs.size(),
        std::format("layer2: {} @{} glyph count", spec.str_font, spec.int4_size));

    int int4_adv_equal{0};
    int int4_box_equal{0};
    for (std::size_t sz_i{0}; sz_i < ft.vec_glyphs.size() && sz_i < live.vec_glyphs.size(); ++sz_i) {
      const DumpGlyph& ft_g{ft.vec_glyphs[sz_i]};
      const DumpGlyph& live_g{live.vec_glyphs[sz_i]};
      const bool b_adv{ft_g.fp4_adv == live_g.fp4_adv};
      const bool b_box{ft_g.int4_w == live_g.int4_w && ft_g.int4_h == live_g.int4_h
          && ft_g.int4_off_x == live_g.int4_off_x && ft_g.int4_off_y == live_g.int4_off_y};
      int4_adv_equal += b_adv ? 1 : 0;
      int4_box_equal += b_box ? 1 : 0;
    }
    check(int4_adv_equal == static_cast<int>(ft.vec_glyphs.size()),
        std::format("layer2: {} @{} advance 100% (got {}/{})",
            spec.str_font, spec.int4_size, int4_adv_equal, ft.vec_glyphs.size()));
    check(int4_box_equal >= 93,
        std::format("layer2: {} @{} box >=93/95 (got {})", spec.str_font, spec.int4_size, int4_box_equal));
  }
}

void test_layer2_measure_and_wrap() {
  const StringsSet strings{load_strings()};
  check(strings.vec_texts.size() == 16, "layer2: strings set size");

  const auto vec1u_sans{read_bytes(golden_path("fixture/FreeSans.ttf"))};
  const auto vec1u_bold{read_bytes(golden_path("fixture/FreeSansBold.ttf"))};
  auto face_sans{std::make_shared<const fr::FontFace>(std::move(*fr::font_face_load(vec1u_sans)))};
  auto face_bold{std::make_shared<const fr::FontFace>(std::move(*fr::font_face_load(vec1u_bold)))};

  fr::SheetBuilder builder;
  auto res_sans10{fr::spritefont_create(face_sans, builder, 10, 8, 1.0f)};
  auto res_bold10{fr::spritefont_create(face_bold, builder, 10, 8, 1.0f)};
  auto res_sans12{fr::spritefont_create(face_sans, builder, 12, 9, 1.0f)};
  auto res_sans14{fr::spritefont_create(face_sans, builder, 14, 11, 1.0f)};
  auto res_bold14{fr::spritefont_create(face_bold, builder, 14, 11, 1.0f)};
  auto res_bold18{fr::spritefont_create(face_bold, builder, 18, 14, 1.0f)};
  auto res_bold24{fr::spritefont_create(face_bold, builder, 24, 18, 1.0f)};
  check(res_sans14.has_value() && res_bold14.has_value() && res_sans10.has_value(),
      "layer2: spritefont create");

  const std::pair<std::string_view, fr::SpriteFont*> kMeasureSpecs[]{
      {"measure-FreeSans-14.txt", &*res_sans14},
      {"measure-FreeSans-10.txt", &*res_sans10},
      {"measure-FreeSansBold-14.txt", &*res_bold14},
  };
  for (const auto& [str_rel, ptr_font] : kMeasureSpecs) {
    const auto vec_golden{load_measure(str_rel)};
    check(vec_golden.size() == strings.vec_texts.size(),
        std::format("layer2: {} rows", str_rel));
    for (std::size_t sz_i{0}; sz_i < vec_golden.size() && sz_i < strings.vec_texts.size(); ++sz_i) {
      const fr::FontMeasure m{fr::spritefont_measure(*ptr_font, strings.vec_texts[sz_i])};
      check(m.int4_width == vec_golden[sz_i].int4_w && m.int4_height == vec_golden[sz_i].int4_h,
          std::format("layer2: {} string {} == {}x{}", str_rel, sz_i, vec_golden[sz_i].int4_w,
              vec_golden[sz_i].int4_h));
    }
  }

  const struct {
    std::string_view str_rel;
    fr::SpriteFont* ptr_font;
    int int4_width;
  } kWrapSpecs[]{
      {"wrap-FreeSans-14-w40.txt", &*res_sans14, 40},
      {"wrap-FreeSans-14-w80.txt", &*res_sans14, 80},
      {"wrap-FreeSans-10-w32.txt", &*res_sans10, 32},
      {"wrap-FreeSansBold-14-w60.txt", &*res_bold14, 60},
  };
  for (const auto& spec : kWrapSpecs) {
    const auto vec_golden{load_wrap(spec.str_rel)};
    check(vec_golden.size() == strings.vec_texts.size(),
        std::format("layer2: {} rows", spec.str_rel));
    for (std::size_t sz_i{0}; sz_i < vec_golden.size() && sz_i < strings.vec_texts.size(); ++sz_i) {
      const std::u16string wrapped{
          fr::spritefont_wrap_text(*spec.ptr_font, strings.vec_texts[sz_i], spec.int4_width)};
      check(wrapped == vec_golden[sz_i],
          std::format("layer2: {} string {}", spec.str_rel, sz_i));
    }
  }

  check(fr::spritefont_wrap_text(*res_sans14, u"Supercalifragilisticexpialidocious", 40)
              == u"Supercalifragilisticexpialidocious",
      "layer2: wrap keeps unbreakable word intact (overflow line abandoned)");
  check(fr::spritefont_wrap_text(*res_sans14, u"aaaa bbbb cccc dddd", 1)
              == u"aaaa bbbb cccc dddd",
      "layer2: wrap abandons line when first word prefix exceeds width");
}

void test_layer2_manifest() {
  const auto vec_manifest{load_manifest()};
  check(vec_manifest.size() == 8, "layer2: manifest entry count");

  std::vector<std::string> vec_golden;
  for (const std::string& str_line : read_lines(golden_path("ra-fonts.txt"))) {
    if (str_line.empty() || str_line.front() == '#') continue;
    vec_golden.push_back(str_line);
  }
  check(vec_golden.size() == vec_manifest.size(), "layer2: manifest golden rows");

  for (std::size_t sz_i{0}; sz_i < vec_manifest.size() && sz_i < vec_golden.size(); ++sz_i) {
    const std::vector<std::string> vec_fields{split_tab(vec_golden[sz_i])};
    if (vec_fields.size() < 4) continue;
    const std::string& str_name{vec_fields[0]};
    const std::string& str_font{vec_fields[1]};
    const int int4_size{parse_int_ascii(vec_fields[2])};
    const int int4_asc{parse_int_ascii(vec_fields[3])};
    const auto& [str_key, entry]{vec_manifest[sz_i]};
    check(str_key == str_name && entry.str_font == str_font && entry.int4_size == int4_size
              && entry.int4_ascender == int4_asc,
        std::format("layer2: manifest {} triple", str_name));
  }

  bool b_found_regular{false};
  for (const auto& [str_key, entry] : vec_manifest)
    if (str_key == "Regular") {
      b_found_regular = true;
      check(entry.int4_size == 14 && entry.int4_ascender == 11,
          "layer2: Regular ascender is the hand-written yaml value 11");
    }
  check(b_found_regular, "layer2: manifest has Regular");
}

void test_layer3_bitmap_quality() {
  const std::vector<int> vec_ascii{ascii_codepoints()};
  for (const FontSpec& spec : kMetricsSpecs) {
    const auto vec1u_ttf{read_bytes(golden_path(std::format("fixture/{}.ttf", spec.str_font)))};
    const DumpSet live{live_stb_dump(vec1u_ttf, spec.int4_size, vec_ascii.data(), vec_ascii.size())};
    const auto res_ft{parse_dump(read_bytes(golden_path(
        std::format("ft-{}-{}.bin", spec.str_font, spec.int4_size))))};
    if (!res_ft) continue;
    PixelStats st;
    const DumpSet& ft{*res_ft};
    for (std::size_t sz_i{0}; sz_i < ft.vec_glyphs.size() && sz_i < live.vec_glyphs.size(); ++sz_i)
      accumulate_same_box(st, ft.vec_glyphs[sz_i], live.vec_glyphs[sz_i]);

    const double fp8_mae{st.lg4_count ? static_cast<double>(st.lg4_sum) / static_cast<double>(st.lg4_count) : 0.0};
    const double fp8_ratio{st.lg4_count
            ? static_cast<double>(st.lg4_over32) / static_cast<double>(st.lg4_count)
            : 0.0};
    check(fp8_mae <= 6.0,
        std::format("layer3: {} @{} MAE {:.3f} <= 6", spec.str_font, spec.int4_size, fp8_mae));
    check(fp8_ratio <= 0.05,
        std::format("layer3: {} @{} |d|>32 ratio {:.4f} <= 0.05", spec.str_font, spec.int4_size, fp8_ratio));
  }
}

void test_missing_glyph_semantics() {
  const auto res_ft{parse_dump(read_bytes(golden_path(kMiscDumpName)))};
  check(res_ft.has_value(), "notdef: misc ft dump parses");
  if (!res_ft) return;
  const DumpSet& ft{*res_ft};
  check(ft.vec_glyphs.size() == std::size(kMiscCodepoints), "notdef: misc dump count");

  const auto vec1u_ttf{read_bytes(golden_path("fixture/FreeSans.ttf"))};
  const fr::FontFace face{std::move(*fr::font_face_load(vec1u_ttf))};

  const DumpGlyph* ptr_ft_space{nullptr};
  const DumpGlyph* ptr_ft_cjk{nullptr};
  const DumpGlyph* ptr_ft_surr{nullptr};
  const DumpGlyph* ptr_ft_del{nullptr};
  const DumpGlyph* ptr_ft_nbbsp{nullptr};
  const DumpGlyph* ptr_ft_smile{nullptr};
  for (const DumpGlyph& g : ft.vec_glyphs) {
    if (g.int4_cp == 0x20) ptr_ft_space = &g;
    if (g.int4_cp == 0x4E2D) ptr_ft_cjk = &g;
    if (g.int4_cp == 0xD83D) ptr_ft_surr = &g;
    if (g.int4_cp == 0x7F) ptr_ft_del = &g;
    if (g.int4_cp == 0xA0) ptr_ft_nbbsp = &g;
    if (g.int4_cp == 0x263A) ptr_ft_smile = &g;
  }
  check(ptr_ft_space && ptr_ft_cjk && ptr_ft_surr && ptr_ft_del && ptr_ft_nbbsp && ptr_ft_smile,
      "notdef: misc dump has all special cps");

  const fr::FontGlyph space{fr::font_create_glyph(face, u' ', kMiscSize, 1.0f)};
  check(space.b_loaded && space.int4_width == 0 && space.int4_height == 0 && space.vec1u_data.empty(),
      "notdef: space is 0x0 bitmap");
  check(ptr_ft_space && space.fp4_advance == ptr_ft_space->fp4_adv,
      "notdef: space advance matches FT");

  const fr::FontGlyph cjk{fr::font_create_glyph(face, 0x4E2D, kMiscSize, 1.0f)};
  check(ptr_ft_cjk && cjk.int4_width == ptr_ft_cjk->int4_w && cjk.int4_height == ptr_ft_cjk->int4_h
              && cjk.fp4_advance == ptr_ft_cjk->fp4_adv && cjk.int4_off_x == ptr_ft_cjk->int4_off_x
              && cjk.int4_off_y == ptr_ft_cjk->int4_off_y,
      "notdef: CJK walks glyph 0 with same box/advance as FT");
  if (ptr_ft_cjk) {
    check(ptr_ft_cjk->int4_w == 7 && ptr_ft_cjk->int4_h == 14
              && static_cast<int>(ptr_ft_cjk->fp4_adv) == 7,
        "notdef: upstream .notdef box is 7x14 adv 7 @14px");
  }

  const fr::FontGlyph surr{fr::font_create_glyph(face, 0xD83D, kMiscSize, 1.0f)};
  check(ptr_ft_surr && surr.int4_width == ptr_ft_surr->int4_w && surr.int4_height == ptr_ft_surr->int4_h
              && surr.fp4_advance == ptr_ft_surr->fp4_adv,
      "notdef: isolated surrogate walks glyph 0 like FT");

  const fr::FontGlyph del{fr::font_create_glyph(face, 0x7F, kMiscSize, 1.0f)};
  check(ptr_ft_del && del.fp4_advance == ptr_ft_del->fp4_adv, "notdef: DEL walks glyph 0");

  PixelStats st;
  for (const DumpGlyph& g : ft.vec_glyphs) {
    const fr::FontGlyph live{fr::font_create_glyph(face, static_cast<char16_t>(g.int4_cp), kMiscSize, 1.0f)};
    DumpGlyph live_rec;
    live_rec.int4_w = live.int4_width;
    live_rec.int4_h = live.int4_height;
    live_rec.vec1u_data = live.vec1u_data;
    accumulate_same_box(st, g, live_rec);
  }
  const double fp8_mae{st.lg4_count ? static_cast<double>(st.lg4_sum) / static_cast<double>(st.lg4_count) : 0.0};
  check(fp8_mae <= 6.0, std::format("notdef: misc set MAE {:.3f} <= 6", fp8_mae));

  const fr::FontGlyph nbsp{fr::font_create_glyph(face, 0xA0, kMiscSize, 1.0f)};
  check(ptr_ft_nbbsp && nbsp.fp4_advance == ptr_ft_nbbsp->fp4_adv,
      "notdef: nbsp advance matches FT");

  const fr::FontGlyph smile{fr::font_create_glyph(face, 0x263A, kMiscSize, 1.0f)};
  check(ptr_ft_smile && smile.int4_width == ptr_ft_smile->int4_w
              && smile.int4_height == ptr_ft_smile->int4_h,
      "notdef: U+263A walks glyph 0 like FT");
}

void test_sheet_and_scale() {
  const auto vec1u_sans{read_bytes(golden_path("fixture/FreeSans.ttf"))};
  auto face{std::make_shared<const fr::FontFace>(std::move(*fr::font_face_load(vec1u_sans)))};

  fr::SheetBuilder builder;
  check(builder.int4_sheet_size == 512 && builder.int4_margin == 1,
      "sheet: defaults 512/margin 1");

  const fr::SheetSprite empty{fr::sheetbuilder_allocate(builder, 0, 0)};
  check(empty.int4_width == 0 && empty.int4_height == 0 && empty.int4_left == 1 && empty.int4_top == 1,
      "sheet: zero-size sprite occupies margin slot like upstream");

  auto res_font{fr::spritefont_create(face, builder, 14, 11, 1.0f)};
  check(res_font.has_value(), "sheet: spritefont create");
  fr::SpriteFont& font{*res_font};
  check(font.int4_top_offset == 3, "sheet: TopOffset = size - ascender");

  std::size_t sz_nonzero{0};
  for (char16_t ch{0x20}; ch < 0x7f; ++ch) {
    const fr::GlyphInfo& info{fr::spritefont_glyph(font, ch)};
    check(info.sprite.int4_left >= 0 && info.sprite.int4_top >= 0
              && info.sprite.int4_left + info.sprite.int4_width <= 512
              && info.sprite.int4_top + info.sprite.int4_height <= 512,
        "sheet: sprite bounds inside sheet");
    if (info.b_visible && info.sprite.int4_width > 0) {
      const auto& vec1u_sheet{builder.vec_vec1u_sheets[info.sprite.u4_sheet]};
      bool b_any{false};
      for (int int4_j{0}; int4_j < info.sprite.int4_height && !b_any; ++int4_j)
        for (int int4_i{0}; int4_i < info.sprite.int4_width && !b_any; ++int4_i) {
          const std::size_t sz_q{(static_cast<std::size_t>(info.sprite.int4_top + int4_j) * 512
                                     + static_cast<std::size_t>(info.sprite.int4_left + int4_i)) * 4};
          if (vec1u_sheet[sz_q] != 0 && vec1u_sheet[sz_q] == vec1u_sheet[sz_q + 1]
              && vec1u_sheet[sz_q + 1] == vec1u_sheet[sz_q + 2]
              && vec1u_sheet[sz_q + 2] == vec1u_sheet[sz_q + 3])
            b_any = true;
        }
      if (b_any) ++sz_nonzero;
    }
  }
  check(sz_nonzero >= 90, "sheet: glyph texels written as 4-channel gray");

  const fr::GlyphInfo& space_info{fr::spritefont_glyph(font, u' ')};
  check(space_info.b_visible && space_info.fp4_advance == 4.0f
              && space_info.sprite.int4_width == 0 && space_info.sprite.int4_height == 0,
      "sheet: space glyph keeps advance with empty sprite");

  fr::spritefont_set_scale(font, 2.0f);
  check(font.map_glyphs.empty(), "scale: set_scale clears cache");
  const fr::FontMeasure m2{fr::spritefont_measure(font, u"Hello World")};
  check(m2.int4_height == 14, "scale: row height stays size");
}

}

int main(int argc, char** argv) {
  if (argc > 1 && std::string_view{argv[1]} == "--gen-golden") return run_gen_golden();

  check(file_exists(golden_path("fixture/FreeSans.ttf")), "prereq: FreeSans fixture");
  check(file_exists(golden_path("ft-FreeSans-14.bin")), "prereq: ft golden");

test_layer1_self_golden();
test_layer2_metrics();
test_layer2_measure_and_wrap();
test_layer2_manifest();
test_layer3_bitmap_quality();
test_missing_glyph_semantics();
test_sheet_and_scale();

  return ora::test::test_exit_code("fonts");
}
