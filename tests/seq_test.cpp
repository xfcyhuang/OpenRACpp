#include <cstdio>
import std;

#include "formats/lcw.hpp"
#include "formats/shp_td.hpp"
#include "gfx/sequence_set.hpp"
#include "gfx/sprite_loader.hpp"
#include "meta/field_loader.hpp"
#include "mods/classic_sprite_sequence.hpp"
#include "mods/classic_tileset_specific_sprite_sequence.hpp"
#include "mods/default_sprite_sequence.hpp"
#include "mods/d2k_sprite_sequence.hpp"
#include "mods/sequence_loader_factory.hpp"
#include "mods/tileset_specific_sprite_sequence.hpp"
#include "yaml/mini_yaml.hpp"

int int4_failures = 0;

#define ORA_CHECK(cond)                                                        \
  do {                                                                         \
    if (!(cond)) {                                                             \
      int4_failures++;                                                         \
      std::println(stderr, "FAIL {}:{}: {}", __FILE__, __LINE__, #cond);       \
    }                                                                          \
  } while (0)

#define ORA_CHECK_THROWS_TEXT(expr, text_sub)                                  \
  do {                                                                         \
    bool b_caught = false;                                                     \
    try {                                                                      \
      (void)(expr);                                                            \
    } catch (const std::exception& e) {                                        \
      b_caught = true;                                                         \
      if (std::string_view{e.what()}.find(text_sub) == std::string_view::npos) { \
        int4_failures++;                                                       \
        std::println(stderr, "FAIL {}:{}: 抛文不含 `{}`: {}", __FILE__, __LINE__, \
                     text_sub, e.what());                                      \
      }                                                                        \
    }                                                                          \
    if (!b_caught) {                                                           \
      int4_failures++;                                                         \
      std::println(stderr, "FAIL {}:{}: 未抛(期望含 `{}`)", __FILE__, __LINE__, \
                   text_sub);                                                  \
    }                                                                          \
  } while (0)

namespace {

std::vector<std::byte> MakeFramesShpTD(std::uint8_t uint1_seed, std::int32_t int4_frame_count) {
  auto vec_encoded_all = std::vector<std::byte>{};
  for (auto int4_f = 0; int4_f < int4_frame_count; int4_f++) {
    std::vector<std::byte> vec_frame(64);
    for (auto int4_i = 0; int4_i < 64; int4_i++)
      vec_frame[static_cast<std::size_t>(int4_i)] =
          static_cast<std::byte>((int4_i * 3 + uint1_seed + int4_f * 17) & 0xFF);
    const auto vec_encoded = ora::fmt::lcw::Encode(vec_frame);
    vec_encoded_all.insert(vec_encoded_all.end(), vec_encoded.begin(), vec_encoded.end());
  }

  const std::size_t st_data_base = 14 + 8 * (static_cast<std::size_t>(int4_frame_count) + 2);
  auto vec_file = std::vector<std::byte>(st_data_base + vec_encoded_all.size());
  const auto put_u16 = [&vec_file](std::size_t st_pos, std::uint16_t uint2_v) {
    vec_file[st_pos] = static_cast<std::byte>(uint2_v & 0xFF);
    vec_file[st_pos + 1] = static_cast<std::byte>(uint2_v >> 8);
  };
  const auto put_u32 = [&vec_file](std::size_t st_pos, std::uint32_t uint4_v) {
    for (auto int4_i = 0; int4_i < 4; int4_i++)
      vec_file[st_pos + static_cast<std::size_t>(int4_i)] =
          static_cast<std::byte>(uint4_v >> (8 * int4_i));
  };

  put_u16(0, static_cast<std::uint16_t>(int4_frame_count));
  put_u16(6, 8);
  put_u16(8, 8);

  std::size_t st_off = st_data_base;
  std::size_t st_encoded = 0;
  for (auto int4_f = 0; int4_f < int4_frame_count; int4_f++) {
    put_u32(14 + 8 * static_cast<std::size_t>(int4_f),
            static_cast<std::uint32_t>(st_off) | (0x80u << 24));
    st_off += st_encoded;
  }
  st_off = st_data_base;
  st_encoded = 0;
  for (auto int4_f = 0; int4_f < int4_frame_count; int4_f++) {
    std::vector<std::byte> vec_frame(64);
    for (auto int4_i = 0; int4_i < 64; int4_i++)
      vec_frame[static_cast<std::size_t>(int4_i)] =
          static_cast<std::byte>((int4_i * 3 + uint1_seed + int4_f * 17) & 0xFF);
    const auto vec_encoded = ora::fmt::lcw::Encode(vec_frame);
    put_u32(14 + 8 * static_cast<std::size_t>(int4_f),
            static_cast<std::uint32_t>(st_data_base + st_encoded) | (0x80u << 24));
    st_encoded += vec_encoded.size();
  }
  put_u32(14 + 8 * static_cast<std::size_t>(int4_frame_count),
          static_cast<std::uint32_t>(vec_file.size()));
  put_u32(14 + 8 * (static_cast<std::size_t>(int4_frame_count) + 1), 0);

  std::ranges::copy(vec_encoded_all, vec_file.begin() + static_cast<std::ptrdiff_t>(st_data_base));
  return vec_file;
}

void WriteBinary(const std::filesystem::path& path_file, const std::vector<std::byte>& vec_bytes) {
  std::ofstream stream_out{path_file, std::ios::binary};
  stream_out.write(reinterpret_cast<const char*>(vec_bytes.data()),
                   static_cast<std::streamsize>(vec_bytes.size()));
}

void WriteText(const std::filesystem::path& path_file, std::string_view sv_text) {
  std::ofstream stream_out{path_file};
  stream_out.write(sv_text.data(), static_cast<std::streamsize>(sv_text.size()));
}

struct SeqFixture {
  std::filesystem::path dir_root;
  ora::fs::FileSystem file_system{};
  std::vector<ora::gfx::SpriteLoaderFn> vec_loaders{&ora::fmt::TryParseShpTD};

  explicit SeqFixture(std::string_view sv_yaml) {
    auto dir_temp = std::filesystem::temp_directory_path();
    dir_root = dir_temp / ("ora_seq_test_" + std::to_string(std::chrono::steady_clock::now()
                                                                .time_since_epoch()
                                                                .count()));
    std::filesystem::create_directories(dir_root);
    WriteBinary(dir_root / "a.shp", MakeFramesShpTD(1, 10));
    WriteBinary(dir_root / "b.shp", MakeFramesShpTD(7, 3));
    WriteBinary(dir_root / "p1.shp", MakeFramesShpTD(11, 1));
    WriteBinary(dir_root / "p2.shp", MakeFramesShpTD(13, 1));
    WriteBinary(dir_root / "d.shp", MakeFramesShpTD(17, 1));
    WriteText(dir_root / "sequences.yaml", sv_yaml);
    file_system.Mount(dir_root.generic_string());
  }

  ~SeqFixture() {
    std::filesystem::remove_all(dir_root);
  }

  std::unique_ptr<ora::gfx::SequenceSet> MakeDefaultSet() {
    std::vector<std::string> vec_files{"sequences.yaml"};
    ora::gfx::SequenceSet::Deps deps;
    deps.ptr_file_system = &file_system;
    deps.vec_sprite_loaders = vec_loaders;
    deps.vec_sequence_files = &vec_files;
    deps.int4_bgra_sheet_size = 128;
    deps.int4_indexed_sheet_size = 128;
    auto up_loader = std::make_unique<ora::mods::DefaultSpriteSequenceLoader>();
    return std::make_unique<ora::gfx::SequenceSet>(deps, *up_loader, "temperat");
  }

  template <class Loader>
  std::unique_ptr<ora::gfx::SequenceSet> MakeSet(const std::string& str_tile_set = "temperat") {
    std::vector<std::string> vec_files{"sequences.yaml"};
    ora::gfx::SequenceSet::Deps deps;
    deps.ptr_file_system = &file_system;
    deps.vec_sprite_loaders = vec_loaders;
    deps.vec_sequence_files = &vec_files;
    deps.int4_bgra_sheet_size = 128;
    deps.int4_indexed_sheet_size = 128;
    auto up_loader = std::make_unique<Loader>();
    return std::make_unique<ora::gfx::SequenceSet>(deps, *up_loader, str_tile_set);
  }
};

void TestFacingAndPolygon() {
  using namespace ora;
  using ora::gfx::IndexFacing;
  ORA_CHECK(IndexFacing(WAngle{0}, 8) == 0);
  ORA_CHECK(IndexFacing(WAngle{63}, 8) == 0);
  ORA_CHECK(IndexFacing(WAngle{64}, 8) == 1);

  ORA_CHECK(IndexFacing(WAngle{1023}, 8) == 0);
  ORA_CHECK(gfx::AngleDiffToStep(WAngle{8}, 8).Angle == 8);
  ORA_CHECK(gfx::GetInterpolatedFacingRotation(WAngle{40}, 8, 32).Angle == 32);
  ORA_CHECK(gfx::GetInterpolatedFacingRotation(WAngle{16}, 8, 32).Angle == 0);

  ORA_CHECK(mods::cnc::ClassicIndexFacing(WAngle{19}, 32) == 0);
  ORA_CHECK(mods::cnc::ClassicIndexFacing(WAngle{20}, 32) == 1);
  ORA_CHECK(mods::cnc::ClassicIndexFacing(WAngle{999}, 32) == 31);
  ORA_CHECK(mods::cnc::ClassicIndexFacing(WAngle{1000}, 32) == 0);
  ORA_CHECK(mods::cnc::ClassicIndexFacing(WAngle{128}, 4) == gfx::IndexFacing(WAngle{128}, 4));
  ORA_CHECK(mods::cnc::ClassicQuantizeFacing(WAngle{20}, 32).Angle == 40);

  const std::array<int2, 4> arr_square{int2{0, 0}, int2{10, 0}, int2{10, 10}, int2{0, 10}};
  ORA_CHECK(gfx::PolygonContains(arr_square, int2{5, 5}));
  ORA_CHECK(gfx::PolygonContains(arr_square, int2{0, 0}));
  ORA_CHECK(!gfx::PolygonContains(arr_square, int2{15, 5}));
  const std::array<int2, 0> arr_empty{};
  ORA_CHECK(!gfx::PolygonContains(arr_empty, int2{1, 1}));
}

void TestDefaultSequenceMatrix() {
  static constexpr std::string_view kYaml = R"YAML(
icons:
    Defaults:
        Tick: 25
        ZOffset: 512
    idle:
        Filename: a.shp
        Start: 0
        Length: 2
    run:
        Filename: a.shp
        Start: 1
        Length: 3
        Facings: 2
        ShadowStart: 4
    all:
        Filename: a.shp
        Length: *
    rev:
        Filename: a.shp
        Length: 4
        Reverses: True
        AlphaFade: True
    trans:
        Filename: a.shp
        Start: 0
        Length: 2
        Stride: 3
        Transpose: True
        Facings: 2
    frames:
        Filename: a.shp
        Frames: 4,2,0
    flip:
        Filename: a.shp
        Length: 2
        FlipX: True
        FlipY: True
        Offset: 5,6,7
    alpha1:
        Filename: a.shp
        Length: 2
        Alpha: 0.25
    alphaN:
        Filename: a.shp
        Length: 3
        Alpha: 0.1,0.2,0.3
    interp:
        Filename: a.shp
        Length: 1
        Facings: 8
        InterpolatedFacings: 32
    pattern:
        FilenamePattern: p{0}.shp
            Start: 1
            Count: 2
    depth:
        Filename: a.shp
        Length: 2
        DepthSprite: d.shp
        DepthSpriteOffset: 3,4
)YAML";

  SeqFixture fixture{kYaml};
  auto ptr_set = fixture.MakeDefaultSet();
  auto& set_sequences = *ptr_set;
  set_sequences.LoadSprites();

  auto& seq_idle = set_sequences.GetSequence("icons", "idle");
  ORA_CHECK(seq_idle.Name() == "idle");
  ORA_CHECK(seq_idle.Tick() == 25);
  ORA_CHECK(seq_idle.ZOffset() == 512);
  ORA_CHECK(seq_idle.Length() == 2);
  ORA_CHECK(seq_idle.Facings() == 1);
  ORA_CHECK(seq_idle.ShadowZOffset() == -5);
  ORA_CHECK(!seq_idle.IgnoreWorldTint());
  ORA_CHECK(seq_idle.Scale() == 1.0f);

  auto& seq_run = set_sequences.GetSequence("icons", "run");
  ORA_CHECK(seq_run.Length() == 3);
  ORA_CHECK(seq_run.Facings() == 2);
  auto& seq_all = set_sequences.GetSequence("icons", "all");
  ORA_CHECK(seq_all.Length() == 10);

  const ora::gfx::Sprite& sprite_run_0 = seq_run.GetSprite(0, ora::WAngle{0});
  const ora::gfx::Sprite& sprite_run_512 = seq_run.GetSprite(0, ora::WAngle{512});
  ORA_CHECK(sprite_run_0.Bounds != sprite_run_512.Bounds);
  const ora::gfx::Sprite& sprite_run_512_f1 = seq_run.GetSprite(1, ora::WAngle{512});
  ORA_CHECK(sprite_run_512.Bounds != sprite_run_512_f1.Bounds);

  const ora::gfx::Sprite& sprite_shadow = seq_run.GetShadow(0, ora::WAngle{0});
  ORA_CHECK(sprite_shadow.ptr_sheet != nullptr);
  ORA_CHECK(seq_idle.GetShadow(0, ora::WAngle{0}).ptr_sheet == nullptr);

  auto& seq_rev = set_sequences.GetSequence("icons", "rev");
  ORA_CHECK(seq_rev.Length() == 6);
  ORA_CHECK(seq_rev.GetAlpha(0) == 1.0f);
  ORA_CHECK(std::abs(seq_rev.GetAlpha(1) - ora::gfx::Lerp(1.0f, 0.0f, 1.0f / 3.0f)) < 1e-6f);
  ORA_CHECK(seq_rev.GetAlpha(3) == 0.0f);
  ORA_CHECK(seq_rev.GetAlpha(4) == seq_rev.GetAlpha(2));
  ORA_CHECK(seq_rev.GetAlpha(5) == seq_rev.GetAlpha(1));

  auto& seq_trans = set_sequences.GetSequence("icons", "trans");
  ORA_CHECK(seq_trans.Length() == 2);
  ORA_CHECK(seq_trans.GetSprite(0, ora::WAngle{0}).Bounds !=
            seq_trans.GetSprite(1, ora::WAngle{0}).Bounds);

  auto& seq_frames = set_sequences.GetSequence("icons", "frames");
  ORA_CHECK(seq_frames.Length() == 1);
  ORA_CHECK(seq_frames.GetSprite(0).ptr_sheet != nullptr);

  auto& seq_flip = set_sequences.GetSequence("icons", "flip");
  const ora::gfx::Sprite& sprite_flip = seq_flip.GetSprite(0);
  ORA_CHECK(sprite_flip.vec_offset.X == 5.0f);
  ORA_CHECK(sprite_flip.vec_offset.Y == 6.0f);
  ORA_CHECK(sprite_flip.vec_offset.Z == 7.0f);

  auto& seq_alpha1 = set_sequences.GetSequence("icons", "alpha1");
  ORA_CHECK(seq_alpha1.GetAlpha(0) == 0.25f);
  ORA_CHECK(seq_alpha1.GetAlpha(1) == 0.25f);
  auto& seq_alphaN = set_sequences.GetSequence("icons", "alphaN");
  ORA_CHECK(std::abs(seq_alphaN.GetAlpha(1) - 0.2f) < 1e-6f);

  auto& seq_interp = set_sequences.GetSequence("icons", "interp");
  auto [sprite_i, wangle_i] = seq_interp.GetSpriteWithRotation(0, ora::WAngle{40});
  ORA_CHECK(wangle_i.Angle == 32);
  ORA_CHECK(sprite_i.ptr_sheet != nullptr);

  auto& seq_pattern = set_sequences.GetSequence("icons", "pattern");
  ORA_CHECK(seq_pattern.Length() == 1);

  auto& seq_depth = set_sequences.GetSequence("icons", "depth");
  const ora::gfx::Sprite& sprite_depth = seq_depth.GetSprite(0);
  ORA_CHECK(sprite_depth.b_secondary);

  ORA_CHECK(sprite_depth.SecondaryBounds.Width == 8);
  ORA_CHECK(sprite_depth.SecondaryBounds.Height == 8);
  ORA_CHECK(sprite_depth.ptr_secondary_sheet != nullptr);

  static constexpr std::string_view kYamlBad = R"YAML(
laterimg:
    later:
        Filename: a.shp
        Length: 2
)YAML";
  SeqFixture fixture_unresolved{kYamlBad};
  auto ptr_set_unresolved = fixture_unresolved.MakeDefaultSet();
  auto& set_unresolved = *ptr_set_unresolved;
  ORA_CHECK_THROWS_TEXT(set_unresolved.GetSequence("laterimg", "later").Length(),
                        "Unable to query unresolved sequence laterimg.later.");
  set_unresolved.LoadSprites();
  ORA_CHECK(set_unresolved.GetSequence("laterimg", "later").Length() == 2);
}

void TestSequenceValidationTexts() {
  static constexpr std::string_view kYaml = R"YAML(
badfacings:
    badfacings:
        Filename: a.shp
        Length: 2
        Facings: 3
badlength:
    badlength:
        Filename: a.shp
        Length: 0
starfacings:
    starfacings:
        Filename: a.shp
        Length: *
        Facings: 2
badalpha:
    badalpha:
        Filename: a.shp
        Length: 2
        AlphaFade: True
        Alpha: 0.5
badinterp:
    badinterp:
        Filename: a.shp
        Length: 1
        Facings: 2
        InterpolatedFacings: 2
badrange:
    badrange:
        Filename: a.shp
        Frames: 99
)YAML";

  {
    SeqFixture fixture{kYaml};
    ORA_CHECK_THROWS_TEXT(fixture.MakeDefaultSet(),
                          "Failed to parse sequences for badfacings.badfacings at");
  }
  {
    SeqFixture fixture{kYaml};
    ORA_CHECK_THROWS_TEXT(fixture.MakeDefaultSet(),
                          "Facings must be within the (positive or negative) "
                          "range of 1 to 1024, and a power of 2.");
  }

  auto ExpectInner = [&](std::string_view sv_seq, std::string_view sv_text) {
    std::string str_yaml = std::format("one:\n    seq:\n        Filename: a.shp\n        {}\n", sv_seq);
    SeqFixture fixture_one{str_yaml};

    ORA_CHECK_THROWS_TEXT(fixture_one.MakeDefaultSet()->LoadSprites(), sv_text);
  };

  ExpectInner("Length: 0", "Length must be positive.");
  ExpectInner("Length: *\n        Facings: 2", "Facings cannot be used with Length: *.");
  ExpectInner("Length: 2\n        AlphaFade: True\n        Alpha: 0.5",
              "AlphaFade cannot be used with Alpha.");
  ExpectInner("Length: 1\n        Facings: 2\n        InterpolatedFacings: 2",
              "InterpolatedFacings must be greater than Facings, within the range of 2 to 1024, "
              "and a power of 2.");

  ExpectInner("Frames: 99", "a.shp does not contain frames: 99");

  ExpectInner("Start: 8\n        Length: 3\n        Frames: 5,6,7",
              "uses frames between 8..10, but only 0..2 exist.");
}

void TestVariantSequences() {
  static constexpr std::string_view kYamlTileset = R"YAML(
gen:
    gen:
        TilesetFilenames:
            temperat: b.shp
            snow: a.shp
        Length: 3
pat:
    pat:
        TilesetFilenamesPattern:
            temperat: p{0}.shp
                Start: 1
                Count: 2
        Filename: a.shp
        Length: 2
fallback:
    fallback:
        Filename: a.shp
        Length: 2
)YAML";

  {
    SeqFixture fixture{kYamlTileset};
    auto ptr_set = fixture.MakeSet<ora::mods::TilesetSpecificSpriteSequenceLoader>();
    ptr_set->LoadSprites();
    auto& seq_gen = ptr_set->GetSequence("gen", "gen");
    ORA_CHECK(seq_gen.Length() == 3);

    auto& seq_fallback = ptr_set->GetSequence("fallback", "fallback");
    ORA_CHECK(seq_gen.GetSprite(0).Bounds != seq_fallback.GetSprite(0).Bounds);
    auto& seq_pat = ptr_set->GetSequence("pat", "pat");
    ORA_CHECK(seq_pat.Length() == 2);

    auto ptr_snow = fixture.MakeSet<ora::mods::TilesetSpecificSpriteSequenceLoader>("snow");
    ptr_snow->LoadSprites();

    ORA_CHECK(ptr_snow->GetSequence("gen", "gen").GetSprite(0).Bounds.Width ==
              seq_fallback.GetSprite(0).Bounds.Width);
    ORA_CHECK(ptr_snow->GetSequence("gen", "gen").GetSprite(0).Bounds.Height ==
              seq_fallback.GetSprite(0).Bounds.Height);

    ORA_CHECK(ptr_snow->GetSequence("pat", "pat").Length() == 2);
  }

  {
    std::string str_frames = "0,1,2,3,4,5";
    for (auto int4_i = 6; int4_i < 32; int4_i++)
      str_frames += std::format(",{}", int4_i % 6);
    const std::string str_yaml = std::format(
        "unit:\n    unit:\n        Filename: a.shp\n        Length: 1\n        Facings: 32\n        Frames: {}\n        UseClassicFacings: True\n",
        str_frames);
    SeqFixture fixture{str_yaml};
    auto ptr_set = fixture.MakeSet<ora::mods::cnc::ClassicTilesetSpecificSpriteSequenceLoader>();
    ptr_set->LoadSprites();
    auto& seq_unit = ptr_set->GetSequence("unit", "unit");
    ORA_CHECK(seq_unit.Facings() == 32);
    ORA_CHECK(seq_unit.Length() == 1);

    const auto& sprite_a = seq_unit.GetSprite(0, ora::WAngle{20});
    const auto& sprite_b = seq_unit.GetSprite(0, ora::WAngle{0});
    ORA_CHECK(sprite_a.Bounds != sprite_b.Bounds);
    ORA_CHECK(seq_unit.GetSprite(0, ora::WAngle{19}).Bounds == sprite_b.Bounds);

    SeqFixture fixture_bad{"unit:\n    unit:\n        Filename: a.shp\n        Length: 2\n        Facings: 2\n        "
                           "UseClassicFacings: True\n"};

    ORA_CHECK_THROWS_TEXT(
        fixture_bad.MakeSet<ora::mods::cnc::ClassicSpriteSequenceLoader>(),
        "UseClassicFacings is only valid for 32 facings");
  }

  {
    SeqFixture fixture{"worm:\n    worm:\n        Filename: a.shp\n        Length: 2\n        Remap: FF00FF\n"};
    auto ptr_set = fixture.MakeSet<ora::mods::d2k::D2kSpriteSequenceLoader>();
    ptr_set->LoadSprites();
    auto& seq_worm = ptr_set->GetSequence("worm", "worm");
    ORA_CHECK(seq_worm.Length() == 2);
    ORA_CHECK(seq_worm.GetSprite(0).ptr_sheet != nullptr);
  }

  ORA_CHECK_THROWS_TEXT(ora::mods::MakeSequenceLoader("NoSuchSequence"),
                        "Unable to find a sequence loader for type 'NoSuchSequence'.");
  ORA_CHECK(ora::mods::MakeSequenceLoader("DefaultSpriteSequence") != nullptr);
  ORA_CHECK(ora::mods::MakeSequenceLoader("TilesetSpecificSpriteSequence") != nullptr);
  ORA_CHECK(ora::mods::MakeSequenceLoader("ClassicSpriteSequence") != nullptr);
  ORA_CHECK(ora::mods::MakeSequenceLoader("ClassicTilesetSpecificSpriteSequence") != nullptr);
  ORA_CHECK(ora::mods::MakeSequenceLoader("D2kSpriteSequence") != nullptr);
}

}

int main() {
  setvbuf(stdout, nullptr, _IONBF, 0);
  const std::pair<std::string_view, void (*)()> arr_tests[] = {
      {"FacingAndPolygon", &TestFacingAndPolygon},
      {"DefaultSequenceMatrix", &TestDefaultSequenceMatrix},
      {"SequenceValidationTexts", &TestSequenceValidationTexts},
      {"VariantSequences", &TestVariantSequences}};
  for (const auto& [str_name, fn_test] : arr_tests) {
    try {
      fn_test();
    } catch (const std::exception& e) {
      int4_failures++;
      std::println(stderr, "CRASH {}: {}", str_name, e.what());
    }
  }

  if (int4_failures != 0) {
    std::println(stderr, "seq_test: {} 项失败 | {} failure(s)", int4_failures, int4_failures);
    return 1;
  }
  std::println("seq_test: 全部通过 | all passed");
  return 0;
}
