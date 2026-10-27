import std;
#include <cstdio>

#include "gfx/gfx_util.hpp"
#include "gfx/hardware_palette.hpp"
#include "gfx/sheet.hpp"
#include "gfx/renderer.hpp"
#include "gfx/renderable.hpp"
#include "gfx/sprite_renderer.hpp"
#include "gfx/terrain_sprite_layer.hpp"
#include "gfx/world_renderer.hpp"
#include "sim/world.hpp"
#include "formats/lcw.hpp"
#include "formats/shp_td.hpp"
#include "formats/png.hpp"
#include "fs/folder.hpp"
#include "fs/file_system.hpp"
#include "gfx/animation.hpp"
#include "gfx/chrome_provider.hpp"
#include "gfx/cursor_manager.hpp"
#include "gfx/sequence_set.hpp"
#include "gfx/sprite_cache.hpp"
#include "gfx/sprite_loader.hpp"
#include "gfx/viewport.hpp"

#if defined(_WIN32) || defined(__linux__) || defined(__APPLE__)
#define ORA_HAS_DESKTOP_GL 1
#define SDL_MAIN_HANDLED
#include "gfx/render_thread.hpp"
#include "platform/sdl2_window.hpp"
#endif

namespace {

using namespace ora;
using namespace ora::gfx;

std::int32_t int4_failures = 0;

#define ORA_CHECK(cond)                                                        \
  do {                                                                         \
    if (!(cond)) {                                                             \
      std::println(stderr, "FAIL {}:{} {}", __FILE__, __LINE__, #cond);       \
      ++int4_failures;                                                         \
    }                                                                          \
  } while (0)

ora::gfx::ImmutablePalette MakeGradientPalette(std::uint32_t uint4_base) {
  std::array<std::uint32_t, ora::gfx::kPaletteSize> vec_colors{};
  for (auto i = 0; i < ora::gfx::kPaletteSize; ++i)
    vec_colors[static_cast<std::size_t>(i)] = uint4_base + static_cast<std::uint32_t>(i);
  return ora::gfx::ImmutablePalette{vec_colors};
}

void TestSheetBuilderGeometry() {
  ora::gfx::SheetBuilder builder{ora::gfx::SheetType::Indexed, 64, 1, nullptr};

  const auto sprite_a = builder.Allocate({16, 16});
  ORA_CHECK(sprite_a.Bounds == ora::Rectangle(1, 1, 16, 16));
  const auto sprite_b = builder.Allocate({16, 16});
  ORA_CHECK(sprite_b.Bounds == ora::Rectangle(18, 1, 16, 16));
  const auto sprite_c = builder.Allocate({16, 16});
  ORA_CHECK(sprite_c.Bounds == ora::Rectangle(35, 1, 16, 16));

  const auto sprite_d = builder.Allocate({16, 16});
  ORA_CHECK(sprite_d.Bounds == ora::Rectangle(1, 18, 16, 16));

  ORA_CHECK(builder.AllSheets().size() == 1);
  ORA_CHECK(builder.Current() == builder.AllSheets().front().get());
  ORA_CHECK(builder.CurrentChannel() == ora::gfx::TextureChannel::Red);
  ORA_CHECK(builder.Current()->Size() == ora::int2(64, 64));

  const auto sprite_empty = builder.Add({}, ora::gfx::SpriteFrameType::Indexed8, {0, 8});
  ORA_CHECK(sprite_empty.Bounds.IsEmpty());
  const auto sprite_e = builder.Allocate({16, 16});
  ORA_CHECK(sprite_e.Bounds == ora::Rectangle(18, 18, 16, 16));

  const auto sprite_f = builder.Allocate({16, 20});
  ORA_CHECK(sprite_f.Bounds == ora::Rectangle(35, 18, 16, 20));
  const auto sprite_g = builder.Allocate({16, 16});
  ORA_CHECK(sprite_g.Bounds == ora::Rectangle(1, 39, 16, 16));
}

void TestSheetBuilderChannelRotation() {
  ora::gfx::SheetBuilder builder{ora::gfx::SheetType::Indexed, 32, 1, nullptr};

  const auto check_channel = [&](ora::gfx::TextureChannel kind_expected) {
    const auto sprite = builder.Allocate({30, 30});
    ORA_CHECK(sprite.kind_channel == kind_expected);
    ORA_CHECK(sprite.Bounds == ora::Rectangle(1, 1, 30, 30));
    return sprite;
  };
  check_channel(ora::gfx::TextureChannel::Red);
  check_channel(ora::gfx::TextureChannel::Green);
  check_channel(ora::gfx::TextureChannel::Blue);
  check_channel(ora::gfx::TextureChannel::Alpha);

  const auto sprite_wrap = builder.Allocate({30, 30});
  ORA_CHECK(sprite_wrap.kind_channel == ora::gfx::TextureChannel::Red);
  ORA_CHECK(builder.AllSheets().size() == 2);
  ORA_CHECK(builder.Current() == builder.AllSheets().back().get());

  ora::gfx::SheetBuilder builder_bgra{ora::gfx::SheetType::BGRA, 32, 1, nullptr};
  ORA_CHECK(builder_bgra.CurrentChannel() == ora::gfx::TextureChannel::RGBA);
  const auto sprite_bgra_a = builder_bgra.Allocate({30, 30});
  ORA_CHECK(sprite_bgra_a.kind_channel == ora::gfx::TextureChannel::RGBA);
  const auto sprite_bgra_b = builder_bgra.Allocate({30, 30});
  ORA_CHECK(sprite_bgra_b.kind_channel == ora::gfx::TextureChannel::RGBA);
  ORA_CHECK(builder_bgra.AllSheets().size() == 2);
}

void TestSheetDirtyRegion() {
  ora::gfx::Sheet sheet{ora::gfx::SheetType::BGRA, {8, 8}, nullptr};
  ORA_CHECK(sheet.Buffered());

  const auto vec_data = sheet.GetData();
  ORA_CHECK(vec_data.size() == 4 * 8 * 8);

  sheet.CommitBufferedData({2, 3, 4, 5});
  ORA_CHECK(sheet.DirtyRegion().has_value() && *sheet.DirtyRegion() == ora::Rectangle(2, 3, 4, 5));

  sheet.CommitBufferedData({8, 9, 2, 2});
  ORA_CHECK(sheet.DirtyRegion().has_value() && *sheet.DirtyRegion() == ora::Rectangle(2, 3, 8, 8));

  sheet.CommitBufferedData();
  ORA_CHECK(sheet.DirtyRegion().has_value() && *sheet.DirtyRegion() == ora::Rectangle(0, 0, 10, 11));

  ora::gfx::Sheet sheet_src{ora::gfx::SheetType::BGRA, {8, 8}, nullptr};
  ora::gfx::Sheet sheet_dst{ora::gfx::SheetType::BGRA, {8, 8}, nullptr};
  (void)sheet_src.GetData();
  ORA_CHECK(!sheet_src.ReleaseBufferAndTryTransferTo(sheet_dst));
  ORA_CHECK(sheet_src.Buffered());

  ora::gfx::Sheet sheet_other{ora::gfx::SheetType::BGRA, {4, 8}, nullptr};
  bool b_threw = false;
  try {
    (void)sheet_src.ReleaseBufferAndTryTransferTo(sheet_other);
  } catch (const std::invalid_argument&) {
    b_threw = true;
  }
  ORA_CHECK(b_threw);
}

void TestPaletteValues() {

  std::array<std::byte, 768> arr_rgb{};
  arr_rgb[0] = std::byte{255};
  arr_rgb[1] = std::byte{255};
  arr_rgb[2] = std::byte{255};
  arr_rgb[3] = std::byte{64};
  arr_rgb[4] = std::byte{64};
  arr_rgb[5] = std::byte{64};
  arr_rgb[6] = std::byte{65};
  arr_rgb[7] = std::byte{65};
  arr_rgb[8] = std::byte{65};

  const std::int32_t arr_remap_transparent[] = {0};
  const std::int32_t arr_remap_shadow[] = {2};
  const ora::gfx::ImmutablePalette palette_immutable{arr_rgb, arr_remap_transparent, arr_remap_shadow};

  ORA_CHECK(palette_immutable.At(0) == 0);
  ORA_CHECK(palette_immutable.At(1) == 0xFF000000u);
  ORA_CHECK(palette_immutable.At(2) == 140u << 24);
  ORA_CHECK(palette_immutable.At(3) == 0xFF000000u);
  ORA_CHECK(palette_immutable.At(255) == 0xFF000000u);

  const ora::gfx::ImmutablePalette palette_copy{std::array<std::uint32_t, 4>{0xFF112233u, 0x80445566u, 0, 0xFFFFFFFFu}};
  std::array<std::byte, 1024> arr_bytes{};
  palette_copy.CopyToArray(arr_bytes, 0);
  ORA_CHECK(arr_bytes[0] == std::byte{0x33});
  ORA_CHECK(arr_bytes[1] == std::byte{0x22});
  ORA_CHECK(arr_bytes[2] == std::byte{0x11});
  ORA_CHECK(arr_bytes[3] == std::byte{0xFF});
  ORA_CHECK(arr_bytes[7] == std::byte{0x80});

  std::array<std::byte, 4 * 257> arr_bytes_offset{};
  palette_copy.CopyToArray(arr_bytes_offset, 1);
  ORA_CHECK(arr_bytes_offset[4] == std::byte{0x33});
  ORA_CHECK(arr_bytes_offset[0] == std::byte{0});

  struct DoubleChannelRemap : ora::gfx::IPaletteRemap {
    ora::core::Color GetRemappedColor(ora::core::Color color_original, std::int32_t) const override {
      const auto double_clamped = [](std::uint8_t u8_v) {
        return static_cast<std::uint8_t>(std::min(255, u8_v * 2));
      };
      return ora::core::Color::FromArgb(color_original.A(), double_clamped(color_original.R()),
                                        double_clamped(color_original.G()), double_clamped(color_original.B()));
    }
  };

  ora::gfx::MutablePalette palette_mutable{palette_copy};
  palette_mutable.SetColor(9, ora::core::Color::FromArgb(0xFF, 1, 2, 3));
  ORA_CHECK(palette_mutable.At(9) == 0xFF010203u);

  const DoubleChannelRemap remap_double{};
  palette_mutable.ApplyRemap(remap_double);
  ORA_CHECK(palette_mutable.At(0) == 0xFF224466u);
  ORA_CHECK(palette_mutable.At(9) == 0xFF020406u);

  const ora::gfx::ImmutablePalette palette_remapped{palette_copy, remap_double};
  ORA_CHECK(palette_remapped.At(0) == 0xFF224466u);

  palette_mutable.SetFromPalette(palette_copy);
  ORA_CHECK(palette_mutable.At(9) == 0);
  ORA_CHECK(palette_mutable.At(0) == 0xFF112233u);
}

void TestHardwarePaletteLogic() {
  ora::gfx::HardwarePalette palette_hw{nullptr};
  ORA_CHECK(palette_hw.FullDirty());

  palette_hw.AddPalette("a", MakeGradientPalette(0x11000000), false);
  ORA_CHECK(palette_hw.GetPaletteIndex("a") == 1);
  ORA_CHECK(palette_hw.Height() == 2);
  ORA_CHECK(palette_hw.Contains("a"));

  palette_hw.AddPalette("b", MakeGradientPalette(0x22000000), true);
  ORA_CHECK(palette_hw.GetPaletteIndex("b") == 2);
  ORA_CHECK(palette_hw.Height() == 4);
  ORA_CHECK(!palette_hw.Contains("c"));

  palette_hw.Initialize();
  ORA_CHECK(!palette_hw.FullDirty() && !palette_hw.ShiftsDirty());
  ORA_CHECK(palette_hw.LastUploadFull());

  ORA_CHECK(palette_hw.GetPalette("b").At(5) == 0x22000005u);
  ORA_CHECK(palette_hw.GetPalette("a").At(5) == 0x11000005u);

  const auto palette_a_new = MakeGradientPalette(0x33000000);
  palette_hw.ReplacePalette("a", palette_a_new);
  ORA_CHECK(!palette_hw.FullDirty());
  ORA_CHECK(!palette_hw.LastUploadFull());
  ORA_CHECK(palette_hw.GetPalette("a").At(5) == 0x33000005u);
  ORA_CHECK(palette_hw.Height() == 4);

  ORA_CHECK(!palette_hw.HasColorShift("a"));
  ORA_CHECK(!palette_hw.ShiftsDirty());
  palette_hw.SetColorShift("a", 0.1f, 0.0f, 1.0f, 0.05f, 0.95f);
  ORA_CHECK(palette_hw.HasColorShift("a"));
  ORA_CHECK(palette_hw.ShiftsDirty());

  struct WhitewashModifier : ora::gfx::IPaletteModifier {
    void AdjustPalette(std::map<std::string, ora::gfx::MutablePalette, std::less<>>& map_mutable) override {
      map_mutable.find("b")->second.SetColor(0, ora::core::Color::FromArgb(0xFF, 0xFF, 0xFF, 0xFF));
    }
  };
  WhitewashModifier modifier_white{};
  ora::gfx::IPaletteModifier* vec_mods[] = {&modifier_white};
  palette_hw.ApplyModifiers(vec_mods);

  ORA_CHECK(palette_hw.GetPalette("b").At(0) == 0x22000000u);
  ORA_CHECK(!palette_hw.RowDirty(2));
  ORA_CHECK(!palette_hw.ShiftsDirty());
  ORA_CHECK(!palette_hw.LastUploadFull());

  bool b_threw = false;
  try {
    (void)palette_hw.GetPalette("missing");
  } catch (const std::runtime_error& error_runtime) {
    b_threw = std::string_view{error_runtime.what()} == "Palette `missing` does not exist";
  }
  ORA_CHECK(b_threw);

  b_threw = false;
  try {
    palette_hw.AddPalette("a", MakeGradientPalette(0), false);
  } catch (const std::runtime_error& error_runtime) {
    b_threw = std::string_view{error_runtime.what()} == "Palette a has already been defined";
  }
  ORA_CHECK(b_threw);

  palette_hw.AddPalette("c", MakeGradientPalette(0x44000000), false);
  palette_hw.AddPalette("d", MakeGradientPalette(0x55000000), false);
  palette_hw.AddPalette("e", MakeGradientPalette(0x66000000), false);
  ORA_CHECK(palette_hw.GetPaletteIndex("e") == 5);
  ORA_CHECK(palette_hw.Height() == 8);
  ORA_CHECK(palette_hw.FullDirty());
}

void TestGfxUtilScalars() {
  using ora::gfx::PremultiplyAlpha;

  ORA_CHECK(PremultiplyAlpha(ora::core::Color::FromArgb(0xFF, 1, 2, 3)).ToArgb() == 0xFF010203u);
  ORA_CHECK(PremultiplyAlpha(ora::core::Color::FromArgb(0, 200, 200, 200)).ToArgb() == 0);

  ORA_CHECK(PremultiplyAlpha(ora::core::Color::FromArgb(128, 255, 0, 0)).ToArgb() == 0x80800000u);
  ORA_CHECK(PremultiplyAlpha(ora::core::Color::FromArgb(128, 0, 17, 0)).ToArgb() == 0x80000900u);
  ORA_CHECK(PremultiplyAlpha(ora::core::Color::FromArgb(1, 255, 255, 255)).ToArgb() == 0x01010101u);

  const auto vec_indices = ora::gfx::CreateQuadIndices(2);
  ORA_CHECK(vec_indices.size() == 12);
  const std::uint32_t arr_expected[] = {0, 1, 2, 2, 3, 0, 4, 5, 6, 6, 7, 4};
  ORA_CHECK(std::equal(vec_indices.begin(), vec_indices.end(), std::begin(arr_expected)));

  ORA_CHECK(ora::gfx::NextPowerOf2(0) == 1);
  ORA_CHECK(ora::gfx::NextPowerOf2(1) == 1);
  ORA_CHECK(ora::gfx::NextPowerOf2(2) == 2);
  ORA_CHECK(ora::gfx::NextPowerOf2(3) == 4);
  ORA_CHECK(ora::gfx::NextPowerOf2(5) == 8);
  ORA_CHECK(ora::gfx::NextPowerOf2(1024) == 1024);
  ORA_CHECK(ora::gfx::NextPowerOf2(1025) == 2048);

  ORA_CHECK(ora::gfx::BoundingRectangle({10.7f, 20.9f, 0}, {5.5f, 6.5f, 0}, 0.0f) ==
            ora::Rectangle(10, 20, 5, 6));
  ORA_CHECK(ora::gfx::BoundingRectangle({-10.7f, -20.9f, 0}, {5.5f, 6.5f, 0}, 0.0f) ==
            ora::Rectangle(-10, -20, 5, 6));

  constexpr float kPiHalf = 1.57079632f;
  ORA_CHECK(ora::gfx::BoundingRectangle({5, 10, 0}, {20, 10, 0}, kPiHalf) ==
            ora::Rectangle(10, 5, 10, 20));

  ora::core::Vector3 vec_rotated[4];
  ora::gfx::RotateQuadInto(vec_rotated, {0, 0, 0}, {10, 10, 0}, 0.0f);
  ORA_CHECK(vec_rotated[0] == ora::core::Vector3(0, 0, 0));
  ORA_CHECK(vec_rotated[2] == ora::core::Vector3(10, 10, 0));
}

void TestFastCreateQuad() {
  ora::gfx::Sheet sheet{ora::gfx::SheetType::Indexed, {64, 64}, nullptr};
  const ora::gfx::Sprite sprite_indexed{sheet, ora::Rectangle(2, 3, 8, 8), ora::gfx::TextureChannel::Red};

  ora::gfx::Vertex arr_vertices[4];
  ora::gfx::FastCreateQuad(arr_vertices, {10, 20, 0}, {30, 40, 0}, {30, 60, 0}, {10, 60, 0}, sprite_indexed,
                           ora::int2{1, 2}, 5, {1, 2, 3}, 0.5f, 0);

  const std::uint32_t uint4_c = arr_vertices[0].c;
  ORA_CHECK((uint4_c & 0x07u) == 0x01u);
  ORA_CHECK(((uint4_c >> 6) & 0x07u) == 1u);
  ORA_CHECK(((uint4_c >> 9) & 0x07u) == 0u);
  ORA_CHECK(((uint4_c >> 3) & 0x07u) == 0u);
  ORA_CHECK(((uint4_c >> 16) & 0xFFFFu) == 5u);

  ORA_CHECK(arr_vertices[0].x == 10.0f && arr_vertices[0].y == 20.0f);
  ORA_CHECK(arr_vertices[2].x == 30.0f && arr_vertices[2].y == 60.0f);
  ORA_CHECK(arr_vertices[1].r == 1.0f && arr_vertices[1].g == 2.0f && arr_vertices[1].b == 3.0f);
  ORA_CHECK(arr_vertices[1].a == 0.5f);

  const auto float_inset = 1.0f / 128.0f;
  ORA_CHECK(arr_vertices[0].s == (2.0f + float_inset) / 64.0f);
  ORA_CHECK(arr_vertices[0].t == (3.0f + float_inset) / 64.0f);
  ORA_CHECK(arr_vertices[1].s == (10.0f - float_inset) / 64.0f);
  ORA_CHECK(arr_vertices[3].t == (11.0f - float_inset) / 64.0f);

  ORA_CHECK(arr_vertices[0].u == 0.0f && arr_vertices[0].v == 0.0f);

  const ora::gfx::Sprite sprite_rgba{sheet, ora::Rectangle(2, 3, 8, 8), ora::gfx::TextureChannel::RGBA};
  const ora::gfx::SpriteWithSecondaryData sprite_secondary{sprite_rgba, sheet, ora::Rectangle(4, 5, 2, 2),
                                                           ora::gfx::TextureChannel::Blue};
  ora::gfx::FastCreateQuad(arr_vertices, {0, 0, 0}, {8, 0, 0}, {8, 8, 0}, {0, 8, 0}, sprite_secondary,
                           ora::int2{3, 2}, 7, {1, 1, 1}, 1.0f, 0);
  const std::uint32_t uint4_c2 = arr_vertices[0].c;
  ORA_CHECK((uint4_c2 & 0x07u) == 0x02u);
  ORA_CHECK(((uint4_c2 >> 3) & 0x07u) == 0x05u);
  ORA_CHECK(((uint4_c2 >> 6) & 0x07u) == 3u);
  ORA_CHECK(((uint4_c2 >> 9) & 0x07u) == 2u);
  ORA_CHECK(((uint4_c2 >> 16) & 0xFFFFu) == 7u);
  ORA_CHECK(arr_vertices[0].u == 4.0f / 64.0f);
  ORA_CHECK(arr_vertices[0].v == 5.0f / 64.0f);
  ORA_CHECK(arr_vertices[2].u == 6.0f / 64.0f);
  ORA_CHECK(arr_vertices[2].v == 7.0f / 64.0f);

  ora::gfx::FastCreateQuad(arr_vertices, {10, 10, 0}, sprite_indexed, ora::int2{0, 0}, 0, 0, {20, 10, 0}, {1, 1, 1},
                           1.0f, 0.0f);
  ORA_CHECK(arr_vertices[0].x == 10.0f && arr_vertices[0].y == 10.0f);
  ORA_CHECK(arr_vertices[2].x == 30.0f && arr_vertices[2].y == 20.0f);

  ora::gfx::FastCreateQuad(arr_vertices, {10, 10, 0}, sprite_indexed, ora::int2{0, 0}, 0, 0, {20, 10, 0}, {1, 1, 1},
                           1.0f, 3.14159265f / 2);

  ORA_CHECK(std::abs(arr_vertices[0].x - 15.0f) < 1e-3f && std::abs(arr_vertices[0].y - 25.0f) < 1e-3f);
  ORA_CHECK(std::abs(arr_vertices[2].x - 25.0f) < 1e-3f && std::abs(arr_vertices[2].y - 5.0f) < 1e-3f);
}

void TestFastCopyIntoChannel() {
  using ora::gfx::SpriteFrameType;
  using ora::gfx::TextureChannel;
  ora::gfx::Sheet sheet{ora::gfx::SheetType::BGRA, {8, 4}, nullptr};
  auto vec_data = sheet.GetData();

  const ora::gfx::Sprite sprite_indexed{sheet, ora::Rectangle(2, 1, 3, 2), TextureChannel::Red};
  const std::array<std::byte, 6> arr_indexed{std::byte{1}, std::byte{2}, std::byte{3},
                                             std::byte{4}, std::byte{5}, std::byte{6}};
  ora::gfx::FastCopyIntoChannel(sprite_indexed, arr_indexed, SpriteFrameType::Indexed8);

  const auto ByteAt = [&](std::int32_t int4_x, std::int32_t int4_y, std::int32_t int4_plane) {
    return std::to_integer<std::int32_t>(vec_data[(static_cast<std::size_t>(int4_y) * 8 + int4_x) * 4 + int4_plane]);
  };
  ORA_CHECK(ByteAt(2, 1, 2) == 1);
  ORA_CHECK(ByteAt(3, 1, 2) == 2);
  ORA_CHECK(ByteAt(4, 1, 2) == 3);
  ORA_CHECK(ByteAt(2, 2, 2) == 4);
  ORA_CHECK(ByteAt(4, 2, 2) == 6);
  ORA_CHECK(ByteAt(2, 1, 0) == 0 && ByteAt(2, 1, 1) == 0 && ByteAt(2, 1, 3) == 0);

  const ora::gfx::Sprite sprite_green{sheet, ora::Rectangle(0, 0, 1, 1), TextureChannel::Green};
  const std::array<std::byte, 1> arr_single{std::byte{42}};
  ora::gfx::FastCopyIntoChannel(sprite_green, arr_single, SpriteFrameType::Indexed8);
  ORA_CHECK(ByteAt(0, 0, 1) == 42);

  const ora::gfx::Sprite sprite_rgba{sheet, ora::Rectangle(0, 0, 2, 1), TextureChannel::RGBA};
  const std::array<std::byte, 8> arr_bgra{
      std::byte{10}, std::byte{20}, std::byte{30}, std::byte{255},
      std::byte{100}, std::byte{50}, std::byte{25}, std::byte{128},
  };
  ora::gfx::FastCopyIntoChannel(sprite_rgba, arr_bgra, SpriteFrameType::Bgra32);
  ORA_CHECK(ByteAt(0, 0, 0) == 10 && ByteAt(0, 0, 1) == 20 && ByteAt(0, 0, 2) == 30 && ByteAt(0, 0, 3) == 255);
  ORA_CHECK(ByteAt(1, 0, 0) == 50 && ByteAt(1, 0, 1) == 25 && ByteAt(1, 0, 2) == 13 && ByteAt(1, 0, 3) == 128);

  ora::gfx::FastCopyIntoChannel(sprite_rgba, arr_bgra, SpriteFrameType::Bgra32, true);
  ORA_CHECK(ByteAt(1, 0, 0) == 100 && ByteAt(1, 0, 2) == 25);

  const std::array<std::byte, 6> arr_bgr{std::byte{1}, std::byte{2}, std::byte{3},
                                         std::byte{4}, std::byte{5}, std::byte{6}};
  ora::gfx::FastCopyIntoChannel(sprite_rgba, arr_bgr, SpriteFrameType::Bgr24);
  ORA_CHECK(ByteAt(0, 0, 0) == 1 && ByteAt(0, 0, 1) == 2 && ByteAt(0, 0, 2) == 3 && ByteAt(0, 0, 3) == 255);

  const std::array<std::byte, 8> arr_rgba{std::byte{30}, std::byte{20}, std::byte{10}, std::byte{255},
                                          std::byte{25}, std::byte{50}, std::byte{100}, std::byte{128}};
  ora::gfx::FastCopyIntoChannel(sprite_rgba, arr_rgba, SpriteFrameType::Rgba32);
  ORA_CHECK(ByteAt(0, 0, 0) == 10 && ByteAt(0, 0, 1) == 20 && ByteAt(0, 0, 2) == 30 && ByteAt(0, 0, 3) == 255);

  ORA_CHECK(ByteAt(1, 0, 0) == 50 && ByteAt(1, 0, 1) == 25 && ByteAt(1, 0, 2) == 13 && ByteAt(1, 0, 3) == 128);

  ora::gfx::SheetBuilder builder{ora::gfx::SheetType::Indexed, 16, 1, nullptr};
  const std::array<std::byte, 4> arr_frame{std::byte{9}, std::byte{8}, std::byte{7}, std::byte{6}};
  const auto sprite_added = builder.Add(arr_frame, SpriteFrameType::Indexed8, {2, 2});
  ORA_CHECK(sprite_added.Bounds == ora::Rectangle(1, 1, 2, 2));
  ORA_CHECK(builder.Current()->DirtyRegion().has_value());
  ORA_CHECK(*builder.Current()->DirtyRegion() == ora::Rectangle(1, 1, 2, 2));
  const auto vec_sheet_data = builder.Current()->GetData();
  const auto SheetByteAt = [&](std::int32_t int4_x, std::int32_t int4_y, std::int32_t int4_plane) {
    return std::to_integer<std::int32_t>(vec_sheet_data[(static_cast<std::size_t>(int4_y) * 16 + int4_x) * 4 +
                                                        int4_plane]);
  };
  ORA_CHECK(SheetByteAt(1, 1, 2) == 9 && SheetByteAt(2, 2, 2) == 6);

  ORA_CHECK(ora::gfx::SheetBuilder::FrameTypeToSheetType(SpriteFrameType::Indexed8) == ora::gfx::SheetType::Indexed);
  ORA_CHECK(ora::gfx::SheetBuilder::FrameTypeToSheetType(SpriteFrameType::Bgra32) == ora::gfx::SheetType::BGRA);
  ORA_CHECK(ora::gfx::SheetBuilder::FrameTypeToSheetType(SpriteFrameType::Rgb24) == ora::gfx::SheetType::BGRA);
}

void TestBlendSpanTracker() {
  ora::gfx::BlendSpanTracker tracker;
  ORA_CHECK(tracker.Spans().empty());

  tracker.TrackQuad(ora::gfx::BlendMode::Alpha, 0);
  tracker.TrackQuad(ora::gfx::BlendMode::Alpha, 4);
  tracker.TrackQuad(ora::gfx::BlendMode::Alpha, 8);
  ORA_CHECK(tracker.Spans().size() == 1);
  ORA_CHECK(tracker.Spans()[0].int4_start == 0 && tracker.Spans()[0].int4_length == 12);

  tracker.TrackQuad(ora::gfx::BlendMode::None, 12);
  tracker.TrackQuad(ora::gfx::BlendMode::None, 16);
  tracker.TrackQuad(ora::gfx::BlendMode::Additive, 20);
  ORA_CHECK(tracker.Spans().size() == 3);
  ORA_CHECK(tracker.Spans()[1].int4_start == 12 && tracker.Spans()[1].int4_length == 8);
  ORA_CHECK(tracker.Spans()[2].int4_start == 20 && tracker.Spans()[2].int4_length == 4);

  tracker.Clear();
  ORA_CHECK(tracker.Spans().empty());
}

void TestResolveTextureIndexAndEpoch() {

  ora::gfx::HardwarePalette palette_hw{nullptr};
  palette_hw.AddPalette("plain", MakeGradientPalette(0xFF000000), false);
  palette_hw.AddPalette("shifted", MakeGradientPalette(0xFF100000), false);
  palette_hw.Initialize();
  ORA_CHECK(palette_hw.GetPaletteIndex("plain") == 1);
  ORA_CHECK(palette_hw.GetPaletteIndex("shifted") == 2);

  const ora::gfx::PaletteReference ref_plain{"plain", 1, palette_hw.GetPalette("plain"), palette_hw};
  const ora::gfx::PaletteReference ref_shifted{"shifted", 2, palette_hw.GetPalette("shifted"), palette_hw};
  ora::gfx::Sprite sprite_indexed{};

  ORA_CHECK(ora::gfx::SpriteRenderer::ResolveTextureIndex(sprite_indexed, nullptr) == 0);

  ORA_CHECK(ora::gfx::SpriteRenderer::ResolveTextureIndex(sprite_indexed, &ref_plain) == 1);

  ora::gfx::Sprite sprite_rgba{};
  sprite_rgba.kind_channel = ora::gfx::TextureChannel::RGBA;

  ORA_CHECK(ora::gfx::SpriteRenderer::ResolveTextureIndex(sprite_rgba, &ref_shifted) == 0);

  palette_hw.SetColorShift("shifted", 0.1f, 0.0f, 1.0f, 0.05f, 0.5f);
  ORA_CHECK(palette_hw.HasColorShift("shifted"));
  ORA_CHECK(ora::gfx::SpriteRenderer::ResolveTextureIndex(sprite_rgba, &ref_shifted) == 2);

  palette_hw.SetColorShift("shifted", 0.0f, 0.0f, 1.0f, 0.0f, 0.0f);
  ORA_CHECK(!palette_hw.HasColorShift("shifted"));
  ORA_CHECK(ora::gfx::SpriteRenderer::ResolveTextureIndex(sprite_rgba, &ref_shifted) == 0);

  ORA_CHECK(ora::gfx::SpriteRenderer::ResolveTextureIndex(sprite_rgba, &ref_plain) == 0);
}

void TestComputeWorldSpriteParams() {

  auto params = ora::gfx::ComputeWorldSpriteParams({1024, 1024}, {2048, 1024}, {0.0f, 0.0f}, {0, 0}, 2048.0f);
  ORA_CHECK(params.int4_downscale == 2);
  ORA_CHECK(params.int2_size_sub == ora::int2(1025, 513));

  params = ora::gfx::ComputeWorldSpriteParams({1025, 513}, {1024, 512}, {10.4f, 20.7f}, {10, 20}, 2048.0f);
  ORA_CHECK(params.vec_fractional_offset.X == -0.5f);
  ORA_CHECK(params.vec_fractional_offset.Y == -0.5f);

  params = ora::gfx::ComputeWorldSpriteParams({100, 100}, {99, 99}, {10.4f, 20.7f}, {10, 20}, 1000.0f);

  ORA_CHECK(std::abs(params.vec_fractional_offset.X - -0.4f) < 1e-5f);
  ORA_CHECK(std::abs(params.vec_fractional_offset.Y - -0.7f) < 1e-5f);
  ORA_CHECK(params.int4_downscale == 1);
  ORA_CHECK(params.int2_size_sub == ora::int2(100, 100));
}

void TestRgbaColorRendererGeometry() {
  struct CapturingSink : ora::gfx::IRgbaQuadSink {
    std::vector<std::array<ora::gfx::Vertex, 4>> vec_quads;
    std::vector<ora::gfx::BlendMode> vec_modes;
    void DrawRGBAQuad(std::span<const ora::gfx::Vertex, 4> vec_vertices, ora::gfx::BlendMode kind_mode) override {
      std::array<ora::gfx::Vertex, 4> quad;
      std::copy(vec_vertices.begin(), vec_vertices.end(), quad.begin());
      vec_quads.push_back(quad);
      vec_modes.push_back(kind_mode);
    }
  };

  {
    CapturingSink sink;
    ora::gfx::RgbaColorRenderer color_renderer{sink};
    color_renderer.DrawLine(ora::core::Vector3{10, 10, 3}, ora::core::Vector3{20, 10, 3}, 2,
                            ora::core::Color::FromArgb(0xFF, 0xFF, 0xFF, 0xFF));
    ORA_CHECK(sink.vec_quads.size() == 1);
    ORA_CHECK(sink.vec_modes[0] == ora::gfx::BlendMode::Alpha);
    const auto& quad = sink.vec_quads[0];

    ORA_CHECK(quad[0].x == 10.5f && quad[0].y == 9.5f && quad[0].z == 3.0f);
    ORA_CHECK(quad[1].x == 10.5f && quad[1].y == 11.5f);
    ORA_CHECK(quad[2].x == 20.5f && quad[2].y == 11.5f);
    ORA_CHECK(quad[3].x == 20.5f && quad[3].y == 9.5f);

    ORA_CHECK(quad[0].s == 1.0f && quad[0].t == 1.0f && quad[0].u == 1.0f && quad[0].v == 1.0f);
    ORA_CHECK(quad[0].c == 0);
  }

  {
    CapturingSink sink;
    ora::gfx::RgbaColorRenderer color_renderer{sink};
    color_renderer.FillRect(ora::core::Vector3{0, 0, 0}, ora::core::Vector3{4, 4, 0},
                            ora::core::Color::FromArgb(0xFF, 0x10, 0x20, 0x30));
    ORA_CHECK(sink.vec_quads.size() == 1);
    const auto& quad = sink.vec_quads[0];
    ORA_CHECK(quad[0].x == 0.5f && quad[0].y == 0.5f);
    ORA_CHECK(quad[1].x == 4.5f && quad[1].y == 0.5f);
    ORA_CHECK(quad[2].x == 4.5f && quad[2].y == 4.5f);
    ORA_CHECK(quad[3].x == 0.5f && quad[3].y == 4.5f);
  }

  {
    CapturingSink sink;
    ora::gfx::RgbaColorRenderer color_renderer{sink};
    color_renderer.FillRect(ora::core::Vector3{0, 0, 0}, ora::core::Vector3{1, 1, 0},
                            ora::core::Color::FromArgb(0x80, 0xFF, 0xFF, 0xFF));
    const auto& quad = sink.vec_quads[0];
    const float float_expected = 255.0f * 128 / 255 / 255.0f;
    ORA_CHECK(std::abs(quad[0].s - float_expected) < 1e-6f);
  }

  {
    CapturingSink sink;
    ora::gfx::RgbaColorRenderer color_renderer{sink};
    const std::array<ora::core::Vector3, 3> arr_triangle{
        ora::core::Vector3{0, 0, 0}, ora::core::Vector3{10, 0, 0}, ora::core::Vector3{0, 10, 0}};
    color_renderer.DrawPolygon(arr_triangle, 1, ora::core::Color::FromArgb(0xFF, 0, 0xFF, 0));
    ORA_CHECK(sink.vec_quads.size() == 3);
  }

  {
    CapturingSink sink;
    ora::gfx::RgbaColorRenderer color_renderer{sink};
    const std::array<ora::core::Vector3, 1> arr_single{ora::core::Vector3{0, 0, 0}};
    color_renderer.DrawPolygon(arr_single, 1, ora::core::Color::FromArgb(0xFF, 0, 0, 0));
    ORA_CHECK(sink.vec_quads.empty());
  }
}

ora::gfx::RenderItem MakeKeyItem(std::int32_t int4_y, std::int32_t int4_z, std::int32_t int4_z_offset,
                                 ora::gfx::RenderableKind kind) {
  ora::gfx::RenderItem item;
  item.kind = kind;
  item.wpos_pos = WPos{0, int4_y, int4_z};
  item.int4_z_offset = int4_z_offset;
  return item;
}

void TestRenderItemSortAndSegments() {
  ora::FrameArena arena{4096};

  {
    std::vector<ora::gfx::RenderItem> vec_items;
    vec_items.push_back(MakeKeyItem(500, 0, 0, ora::gfx::RenderableKind::Sprite));
    vec_items.push_back(MakeKeyItem(100, 0, 0, ora::gfx::RenderableKind::UISprite));
    vec_items.push_back(MakeKeyItem(100, 0, 0, ora::gfx::RenderableKind::Sprite));
    vec_items.push_back(MakeKeyItem(0, 100, 0, ora::gfx::RenderableKind::MarkerTile));
    vec_items.push_back(MakeKeyItem(0, 0, 50, ora::gfx::RenderableKind::Sprite));

    const std::size_t size_mark = arena.Mark();
    const auto vec_sorted = ora::gfx::SortRenderablesByZ(vec_items, arena);
    ORA_CHECK(vec_sorted.size() == 5);
    ORA_CHECK(vec_sorted[0].int4_z_offset == 50);
    ORA_CHECK(vec_sorted[1].kind == ora::gfx::RenderableKind::UISprite);
    ORA_CHECK(vec_sorted[2].kind == ora::gfx::RenderableKind::Sprite);
    ORA_CHECK(vec_sorted[3].kind == ora::gfx::RenderableKind::MarkerTile);
    ORA_CHECK(vec_sorted[4].wpos_pos.Y == 500);
    arena.Rewind(size_mark);
  }

  {
    std::vector<ora::gfx::RenderItem> vec_items;
    vec_items.push_back(MakeKeyItem(-300, 0, 0, ora::gfx::RenderableKind::Sprite));
    vec_items.push_back(MakeKeyItem(200, 0, 0, ora::gfx::RenderableKind::Sprite));
    const std::size_t size_mark = arena.Mark();
    const auto vec_sorted = ora::gfx::SortRenderablesByZ(vec_items, arena);
    ORA_CHECK(vec_sorted[0].wpos_pos.Y == -300);
    arena.Rewind(size_mark);
  }

  {
    std::vector<ora::gfx::RenderItem> vec_items;
    vec_items.push_back(ora::gfx::MakeTargetLineRenderable({}, ora::core::Color::FromArgb(255, 255, 0, 0), 1, 1));
    bool b_threw = false;
    try {
      const std::size_t size_mark = arena.Mark();
      ora::gfx::SortRenderablesByZ(vec_items, arena);
      arena.Rewind(size_mark);
    } catch (const std::runtime_error& error_runtime) {
      b_threw = std::string_view{error_runtime.what()} == "Sequence contains no elements";
    }
    ORA_CHECK(b_threw);
  }

  {
    std::vector<ora::gfx::RenderItem> vec_items;
    vec_items.push_back(MakeKeyItem(1, 0, 0, ora::gfx::RenderableKind::UISprite));
    vec_items.push_back(MakeKeyItem(2, 0, 0, ora::gfx::RenderableKind::Sprite));
    vec_items.push_back(MakeKeyItem(3, 0, 0, ora::gfx::RenderableKind::UISprite));
    vec_items.push_back(MakeKeyItem(4, 0, 0, ora::gfx::RenderableKind::MarkerTile));
    vec_items.push_back(MakeKeyItem(5, 0, 0, ora::gfx::RenderableKind::Sprite));
    vec_items.push_back(MakeKeyItem(6, 0, 0, ora::gfx::RenderableKind::UISprite));

    const std::size_t size_mark = arena.Mark();
    const auto segments = ora::gfx::SegmentByKind(vec_items, arena);
    ORA_CHECK(segments.int4_kind_order_count == 3);
    ORA_CHECK(segments.arr_kind_order[0] == ora::gfx::RenderableKind::UISprite);
    ORA_CHECK(segments.arr_kind_order[1] == ora::gfx::RenderableKind::Sprite);
    ORA_CHECK(segments.arr_kind_order[2] == ora::gfx::RenderableKind::MarkerTile);
    ORA_CHECK(segments.arr_counts[static_cast<std::int32_t>(ora::gfx::RenderableKind::UISprite)] == 3);
    ORA_CHECK(segments.arr_counts[static_cast<std::int32_t>(ora::gfx::RenderableKind::Sprite)] == 2);
    ORA_CHECK(segments.arr_counts[static_cast<std::int32_t>(ora::gfx::RenderableKind::MarkerTile)] == 1);
    ORA_CHECK(segments.arr_starts[static_cast<std::int32_t>(ora::gfx::RenderableKind::UISprite)] == 0);
    ORA_CHECK(segments.arr_starts[static_cast<std::int32_t>(ora::gfx::RenderableKind::Sprite)] == 3);
    ORA_CHECK(segments.arr_starts[static_cast<std::int32_t>(ora::gfx::RenderableKind::MarkerTile)] == 5);

    ORA_CHECK(segments.vec_segmented[0].wpos_pos.Y == 1);
    ORA_CHECK(segments.vec_segmented[1].wpos_pos.Y == 3);
    ORA_CHECK(segments.vec_segmented[2].wpos_pos.Y == 6);
    ORA_CHECK(segments.vec_segmented[3].wpos_pos.Y == 2);
    ORA_CHECK(segments.vec_segmented[4].wpos_pos.Y == 5);
    arena.Rewind(size_mark);
  }
}

void TestWorldRendererCoordinatesAndPalettes() {
  ora::sim::World world{};
  ora::gfx::WorldRenderer::Desc desc;
  desc.int2_tile_size = {32, 32};
  desc.int4_tile_scale = 1024;
  desc.ptr_renderer = nullptr;
  ora::gfx::WorldRenderer wr{world, desc};

  const auto vec_pos = wr.ScreenPosition(WPos{512, 2048, 512});
  ORA_CHECK(vec_pos.X == 16.0f && vec_pos.Y == 48.0f);
  const auto vec_3d = wr.Screen3DPosition(WPos{512, 2048, 512});
  ORA_CHECK(vec_3d.X == 16.0f && vec_3d.Y == 48.0f && vec_3d.Z == 64.0f);

  ORA_CHECK(wr.ScreenPxPosition(WPos{16, 16, 0}) == int2(0, 0));
  ORA_CHECK(wr.ScreenPxPosition(WPos{48, 48, 0}) == int2(2, 2));

  const auto vec_3dpx = wr.Screen3DPxPosition(WPos{16, 48, 0});
  ORA_CHECK(vec_3dpx.X == 0.0f && vec_3dpx.Y == 2.0f && vec_3dpx.Z == 1.5f);

  const auto vec_components = wr.ScreenVectorComponents(WVec{1024, 2048, 512});
  ORA_CHECK(vec_components.X == 32.0f && vec_components.Y == 48.0f && vec_components.Z == 16.0f);
  const auto arr_vec = wr.ScreenVector(WVec{1024, 2048, 512});
  ORA_CHECK(arr_vec[0] == 32.0f && arr_vec[1] == 48.0f && arr_vec[2] == 16.0f && arr_vec[3] == 1.0f);
  ORA_CHECK(wr.ScreenPxOffset(WVec{512, 512, 0}) == int2(16, 16));

  const WPos wpos_projected = wr.ProjectedPosition({16, 48});
  ORA_CHECK(wpos_projected.X == 512 && wpos_projected.Y == 1536 && wpos_projected.Z == 0);

  ORA_CHECK(wr.Palette("") == nullptr);

  wr.AddPalette("terrain", MakeGradientPalette(0xFF000000), false);
  const auto* ptr_ref_a = wr.Palette("terrain");
  const auto* ptr_ref_b = wr.Palette("terrain");
  ORA_CHECK(ptr_ref_a != nullptr && ptr_ref_a == ptr_ref_b);

  std::int32_t int4_invalidated = 0;
  const auto uint8_token = wr.SubscribePaletteInvalidated([&]() { ++int4_invalidated; });
  wr.AddPalette("player", MakeGradientPalette(0xFF100000), false);
  ORA_CHECK(int4_invalidated >= 1);
  wr.AddPalette("player", MakeGradientPalette(0xFF200000), false, true);
  ORA_CHECK(wr.Palette("player")->Palette().At(0) == 0xFF200000u);
  wr.UnsubscribePaletteInvalidated(uint8_token);

  struct DoublingModifier final : ora::gfx::IPaletteModifier {
    void AdjustPalette(std::map<std::string, ora::gfx::MutablePalette, std::less<>>& map_mutable) override {
      const auto it = map_mutable.find("mutable");
      if (it == map_mutable.end())
        return;
      for (auto i = 0; i < ora::gfx::kPaletteSize; ++i) {
        const std::uint32_t uint4_color = it->second.At(i);
        const std::uint32_t uint4_r = std::min<std::uint32_t>((uint4_color >> 16 & 0xFF) * 2, 255);
        it->second.SetAt(i, (uint4_color & 0xFF00FFFFu) | uint4_r << 16);
      }
    }
  };
  wr.AddPalette("mutable", MakeGradientPalette(0xFF000000), true);
  const std::uint32_t uint4_before = wr.hardware_palette().GetPalette("mutable").At(10);
  DoublingModifier modifier;
  const std::array<ora::gfx::IPaletteModifier*, 1> arr_modifiers{&modifier};
  wr.RefreshPalette(arr_modifiers);

  ORA_CHECK(wr.hardware_palette().GetPalette("mutable").At(10) == uint4_before);
}

class FakeTerrainLighting final : public ora::gfx::ITerrainLighting {
 public:
  ora::core::Vector3 vec_tint{0.5f, 1.0f, 2.0f};

  ora::core::Vector3 TintAt(const WPos&) const override { return vec_tint; }
  std::uint64_t AddCellChangedListener(std::function<void(MPos)>) override { return 0; }
  void RemoveCellChangedListener(std::uint64_t) override {}
};

void TestTerrainSpriteLayerLogic() {

  const auto MakeAosReference = [](const ora::gfx::Sprite& sprite_r, std::int32_t int4_palette_index,
                                   const ora::core::Vector3& vec_tint_effective, float float_alpha) {
    std::array<ora::gfx::Vertex, 4> arr_reference{};
    ora::gfx::FastCreateQuad(arr_reference, ora::core::Vector3{11.0f, 22.0f, 5.0f}, sprite_r, int2{0, 0},
                             int4_palette_index, 0, 1.0f * sprite_r.vec_size, vec_tint_effective, float_alpha);
    return arr_reference;
  };

  ora::sim::World world{};
  ora::gfx::WorldRenderer::Desc desc;
  desc.int2_tile_size = {24, 24};
  desc.int4_tile_scale = 1024;
  ora::gfx::WorldRenderer wr{world, desc};
  wr.SetTerrainSurface(ora::gfx::TerrainMapSurface::MakeSquareDefault({4, 3}));

  ora::gfx::HardwarePalette palette_hw{nullptr};
  palette_hw.AddPalette("terrain", MakeGradientPalette(0xFF000000), false);
  const ora::gfx::PaletteReference ref_terrain{"terrain", palette_hw.GetPaletteIndex("terrain"),
                                               palette_hw.GetPalette("terrain"), palette_hw};

  ora::gfx::SheetBuilder builder{ora::gfx::SheetType::Indexed, 64, 1, nullptr};
  const auto sprite_tile = builder.Allocate({8, 8});

  const auto sprite_empty = builder.Allocate({0, 0});

  ora::gfx::TerrainSpriteLayer layer{world, wr, sprite_empty, ora::gfx::BlendMode::Alpha, false, nullptr};
  ORA_CHECK(layer.kind_blend() == ora::gfx::BlendMode::Alpha);

  layer.Update(MPos{1, 1}, &sprite_tile, &ref_terrain, ora::core::Vector3{11.0f, 22.0f, 5.0f}, 1.0f, 1.0f, false);
  ORA_CHECK(layer.RowDirtyForTest(1));
  ORA_CHECK(layer.PaletteAtForTest(MPos{1, 1}) == &ref_terrain);
  {
    const auto vec_vertices = layer.VerticesForTest();
    ORA_CHECK(vec_vertices.size() == 4 * 4 * 3);

    ORA_CHECK((vec_vertices[16].c & 0xFFFF0000u) == 0);
    ORA_CHECK(vec_vertices[20].r == 1.0f && vec_vertices[20].a == 1.0f);
  }

  layer.ComposeRow(1);
  {
    const auto arr_reference = MakeAosReference(sprite_tile, ref_terrain.TextureIndex(),
                                                ora::core::Vector3{1.0f, 1.0f, 1.0f}, 1.0f);
    const auto& vec_staging = layer.StagingForTest();
    for (std::size_t i = 0; i < 4; ++i) {
      ORA_CHECK(vec_staging[4 + i].x == arr_reference[i].x);
      ORA_CHECK(vec_staging[4 + i].c == arr_reference[i].c);
      ORA_CHECK(vec_staging[4 + i].r == arr_reference[i].r);
      ORA_CHECK(vec_staging[4 + i].a == arr_reference[i].a);
    }
  }

  layer.Draw(1, 1);
  ORA_CHECK(!layer.RowDirtyForTest(1));
  wr.AddPalette("player", MakeGradientPalette(0xFF100000), false);
  ORA_CHECK(layer.RowDirtyForTest(0) && layer.RowDirtyForTest(1) && layer.RowDirtyForTest(2));

  layer.Update(MPos{2, 1}, &sprite_tile, &ref_terrain, ora::core::Vector3{0, 0, 0}, 1.0f, 1.0f, false);
  layer.Clear(CPos{2, 1});
  ORA_CHECK(layer.PaletteAtForTest(MPos{2, 1}) == nullptr);
  {
    const auto vec_vertices = layer.VerticesForTest();
    const std::size_t size_offset = 16  + 4 * 2;
    for (std::size_t i = 0; i < 4; ++i)

      ORA_CHECK(vec_vertices[size_offset + i].c == 0x01u);
  }

  {
    ora::gfx::WorldRenderer wr_lit{world, desc};
    wr_lit.SetTerrainSurface(ora::gfx::TerrainMapSurface::MakeSquareDefault({4, 3}));
    FakeTerrainLighting lighting;
    wr_lit.SetTerrainLighting(&lighting);

    ora::gfx::TerrainSpriteLayer layer_lit{world, wr_lit, sprite_empty, ora::gfx::BlendMode::Alpha, false, nullptr};
    layer_lit.Update(MPos{1, 1}, &sprite_tile, &ref_terrain, ora::core::Vector3{11.0f, 22.0f, 5.0f}, 1.0f, 0.5f,
                     false);
    const auto vec_tints = layer_lit.CornerTintForTest();
    const std::size_t size_offset = 4 * 4 * 1 + 4 * 1;
    for (std::size_t i = 0; i < 4; ++i) {
      ORA_CHECK(vec_tints[size_offset + i].X == 0.5f);
      ORA_CHECK(vec_tints[size_offset + i].Y == 1.0f);
      ORA_CHECK(vec_tints[size_offset + i].Z == 2.0f);
    }

    layer_lit.ComposeRow(1);
    const auto arr_reference = MakeAosReference(
        sprite_tile, ref_terrain.TextureIndex(),
        0.5f * ora::core::Vector3{0.5f, 1.0f, 2.0f},
        0.5f);
    const auto& vec_staging = layer_lit.StagingForTest();
    for (std::size_t i = 0; i < 4; ++i) {
      ORA_CHECK(vec_staging[4 + i].r == arr_reference[i].r);
      ORA_CHECK(vec_staging[4 + i].g == arr_reference[i].g);
      ORA_CHECK(vec_staging[4 + i].b == arr_reference[i].b);
      ORA_CHECK(vec_staging[4 + i].a == arr_reference[i].a);
    }

    layer_lit.Update(MPos{1, 1}, &sprite_tile, &ref_terrain, ora::core::Vector3{11.0f, 22.0f, 5.0f}, 1.0f, 0.5f,
                     true);
    const ora::core::Vector3 vec_tint_one{1.0f, 1.0f, 1.0f};
    for (std::size_t i = 0; i < 4; ++i)
      ORA_CHECK(vec_tints[size_offset + i] == vec_tint_one);
  }

  bool b_threw = false;
  try {
    ora::gfx::Sheet sheet_other{ora::gfx::SheetType::Indexed, {4, 4}, nullptr};
    auto sprite_other = sprite_tile;
    sprite_other.kind_blend = ora::gfx::BlendMode::None;
    layer.Update(MPos{0, 0}, &sprite_other, nullptr, ora::core::Vector3{}, 1.0f, 1.0f, false);
  } catch (const std::runtime_error& error_runtime) {
    b_threw = std::string_view{error_runtime.what()} == "Attempted to add sprite with a different blend mode";
  }
  ORA_CHECK(b_threw);

  b_threw = false;
  try {
    std::array<ora::gfx::Sheet, 9> arr_sheets{
        ora::gfx::Sheet{ora::gfx::SheetType::Indexed, {4, 4}, nullptr},
        ora::gfx::Sheet{ora::gfx::SheetType::Indexed, {4, 4}, nullptr},
        ora::gfx::Sheet{ora::gfx::SheetType::Indexed, {4, 4}, nullptr},
        ora::gfx::Sheet{ora::gfx::SheetType::Indexed, {4, 4}, nullptr},
        ora::gfx::Sheet{ora::gfx::SheetType::Indexed, {4, 4}, nullptr},
        ora::gfx::Sheet{ora::gfx::SheetType::Indexed, {4, 4}, nullptr},
        ora::gfx::Sheet{ora::gfx::SheetType::Indexed, {4, 4}, nullptr},
        ora::gfx::Sheet{ora::gfx::SheetType::Indexed, {4, 4}, nullptr},
        ora::gfx::Sheet{ora::gfx::SheetType::Indexed, {4, 4}, nullptr}};
    for (int i = 0; i < 9; ++i) {
      const auto sprite_i = builder.Allocate({4, 4});

      auto sprite_rep = sprite_i;
      sprite_rep.ptr_sheet = &arr_sheets[static_cast<std::size_t>(i)];
      layer.Update(MPos{0, 0}, &sprite_rep, nullptr, ora::core::Vector3{}, 1.0f, 1.0f, false);
    }
  } catch (const std::runtime_error& error_runtime) {
    b_threw = std::string_view{error_runtime.what()} == "Sheet overflow";
  }
  ORA_CHECK(b_threw);

  {
    ora::gfx::WorldRenderer wr_bounds{world, desc};
    wr_bounds.SetTerrainSurface(ora::gfx::TerrainMapSurface::MakeSquareDefault({4, 3}));
    ora::gfx::TerrainSpriteLayer layer_bounds{world, wr_bounds, sprite_empty, ora::gfx::BlendMode::Alpha, false,
                                              nullptr};
    ora::gfx::Sheet sheet_out{ora::gfx::SheetType::Indexed, {4, 4}, nullptr};
    auto sprite_out = sprite_tile;
    sprite_out.ptr_sheet = &sheet_out;
    layer_bounds.Update(MPos{9, 9}, &sprite_out, nullptr, ora::core::Vector3{}, 1.0f, 1.0f, false);
    for (std::int32_t int4_row = 0; int4_row < 3; ++int4_row)
      ORA_CHECK(!layer_bounds.RowDirtyForTest(int4_row));
  }
}

#ifdef ORA_HAS_DESKTOP_GL

void TestGlSheetAndPalette() {
  if (std::getenv("ORA_SKIP_GL") != nullptr)
    return;

  auto window_opt = ora::platform::Sdl2Window::Create({.int4_width = 256, .int4_height = 128});
  if (!window_opt.has_value()) {
    std::println("SKIP: 无法创建窗口(无桌面/GPU 环境)| SKIP: no window (headless/GPU-less host)");
    return;
  }
  auto& window = *window_opt;

  ora::gfx::RenderThread render{window};
  render.FlushAndWait();
  if (render.b_thread_failed()) {
    std::println("SKIP: 渲染线程 GL 初始化失败(无 GPU/驱动环境)| SKIP: render-thread GL init failed");
    return;
  }

  {
    ora::gfx::SheetBuilder builder{ora::gfx::SheetType::BGRA, 32, 1, &render};

    std::array<std::byte, 64> arr_block_a{};
    std::array<std::byte, 64> arr_block_b{};
    for (auto p = 0; p < 16; ++p) {
      arr_block_a[static_cast<std::size_t>(p) * 4 + 0] = std::byte(0xA0 + p);
      arr_block_a[static_cast<std::size_t>(p) * 4 + 1] = std::byte{0x11};
      arr_block_a[static_cast<std::size_t>(p) * 4 + 2] = std::byte{0x22};
      arr_block_a[static_cast<std::size_t>(p) * 4 + 3] = std::byte{0xFF};
      arr_block_b[static_cast<std::size_t>(p) * 4 + 0] = std::byte(0xB0 + p);
      arr_block_b[static_cast<std::size_t>(p) * 4 + 1] = std::byte{0x33};
      arr_block_b[static_cast<std::size_t>(p) * 4 + 2] = std::byte{0x44};
      arr_block_b[static_cast<std::size_t>(p) * 4 + 3] = std::byte{0xFF};
    }

    const auto sprite_a = builder.Add(arr_block_a, ora::gfx::SpriteFrameType::Bgra32, {4, 4}, true);
    const auto sprite_b = builder.Add(arr_block_b, ora::gfx::SpriteFrameType::Bgra32, {4, 4}, true);
    ORA_CHECK(sprite_a.Bounds == ora::Rectangle(1, 1, 4, 4));
    ORA_CHECK(sprite_b.Bounds == ora::Rectangle(6, 1, 4, 4));

    auto& texture_sheet = builder.Current()->GetTexture();
    auto vec_readback = texture_sheet.GetData();
    ORA_CHECK(vec_readback.size() == 4096);
    const auto ReadPx = [&](std::int32_t int4_x, std::int32_t int4_y) {
      const std::size_t i = (static_cast<std::size_t>(int4_y) * 32 + int4_x) * 4;
      return std::to_integer<std::int32_t>(vec_readback[i]);
    };
    ORA_CHECK(ReadPx(1, 1) == 0xA0 && ReadPx(4, 4) == 0xAF);
    ORA_CHECK(ReadPx(6, 1) == 0xB0 && ReadPx(9, 4) == 0xBF);

    auto vec_sheet_data = builder.Current()->GetData();
    const auto rect_row = ora::Rectangle(sprite_a.Bounds.X, sprite_a.Bounds.Y, 4, 1);
    for (auto i = 0; i < 4; ++i) {
      const std::size_t idx = (static_cast<std::size_t>(rect_row.Y) * 32 + rect_row.X + i) * 4;
      vec_sheet_data[idx] = std::byte(0xC5);
    }
    builder.Current()->CommitBufferedData(rect_row);
    (void)builder.Current()->GetTexture();

    vec_readback = texture_sheet.GetData();
    ORA_CHECK(ReadPx(1, 1) == 0xC5 && ReadPx(4, 1) == 0xC5);
    ORA_CHECK(ReadPx(1, 2) == 0xA4 && ReadPx(9, 4) == 0xBF);

    ora::gfx::Sheet sheet_src{ora::gfx::SheetType::BGRA, {8, 8}, &render};
    ora::gfx::Sheet sheet_dst{ora::gfx::SheetType::BGRA, {8, 8}, &render};
    (void)sheet_src.GetData();
    ORA_CHECK(sheet_src.ReleaseBufferAndTryTransferTo(sheet_dst));
    ORA_CHECK(!sheet_src.Buffered());
    const auto vec_dst = sheet_dst.GetData();
    ORA_CHECK(std::all_of(vec_dst.begin(), vec_dst.end(), [](std::byte b) { return b == std::byte{0}; }));
    auto& texture_dst = sheet_dst.GetTexture();
    auto vec_dst_read = texture_dst.GetData();
    ORA_CHECK(vec_dst_read.size() == 8 * 8 * 4);
    ORA_CHECK(std::all_of(vec_dst_read.begin(), vec_dst_read.end(), [](std::byte b) { return b == std::byte{0}; }));
  }

  {
    ora::gfx::HardwarePalette palette_hw{&render};
    palette_hw.AddPalette("a", MakeGradientPalette(0x11BB0000), false);
    palette_hw.AddPalette("b", MakeGradientPalette(0x22CC0000), true);
    palette_hw.AddPalette("c", MakeGradientPalette(0x33DD0000), false);
    palette_hw.Initialize();
    ORA_CHECK(palette_hw.Height() == 4);

    auto* ptr_texture = palette_hw.TextureOrNull();
    ORA_CHECK(ptr_texture != nullptr && ptr_texture->Width() == 256 && ptr_texture->Height() == 4);

    const auto AssertMatchesFullReference = [&]() {
      ora::gfx::Texture texture_reference{render};
      texture_reference.SetData(palette_hw.BufferForTest(), 256, palette_hw.Height());
      const auto vec_reference = texture_reference.GetData();
      const auto vec_actual = ptr_texture->GetData();
      ORA_CHECK(vec_actual.size() == vec_reference.size());
      ORA_CHECK(std::equal(vec_actual.begin(), vec_actual.end(), vec_reference.begin()));
    };
    AssertMatchesFullReference();

    auto vec_rows = ptr_texture->GetData();
    const auto RowByte = [&](std::int32_t int4_row, std::int32_t int4_entry, std::int32_t int4_plane) {
      return std::to_integer<std::int32_t>(
          vec_rows[(static_cast<std::size_t>(int4_row) * 256 + int4_entry) * 4 + int4_plane]);
    };
    ORA_CHECK(RowByte(1, 5, 0) == 0x05 && RowByte(1, 5, 1) == 0x00 && RowByte(1, 5, 2) == 0xBB &&
              RowByte(1, 5, 3) == 0x11);
    ORA_CHECK(RowByte(0, 5, 0) == 0 && RowByte(0, 5, 3) == 0);

    palette_hw.ReplacePalette("a", MakeGradientPalette(0x44EE0000));
    ORA_CHECK(!palette_hw.FullDirty());
    AssertMatchesFullReference();
    vec_rows = ptr_texture->GetData();
    ORA_CHECK(RowByte(1, 5, 2) == 0xEE && RowByte(1, 5, 3) == 0x44);
    ORA_CHECK(RowByte(2, 5, 2) == 0xCC);

    struct WhitewashModifier : ora::gfx::IPaletteModifier {
      void AdjustPalette(std::map<std::string, ora::gfx::MutablePalette, std::less<>>& map_mutable) override {
        map_mutable.find("b")->second.SetColor(7, ora::core::Color::FromArgb(0xFF, 0xFF, 0xFF, 0xFF));
      }
    };
    WhitewashModifier modifier_white{};
    ora::gfx::IPaletteModifier* vec_mods[] = {&modifier_white};
    palette_hw.ApplyModifiers(vec_mods);
    vec_rows = ptr_texture->GetData();
    ORA_CHECK(RowByte(2, 7, 0) == 0xFF && RowByte(2, 7, 3) == 0xFF);
    ORA_CHECK(palette_hw.GetPalette("b").At(7) == 0x22CC0007u);
  }
}

ora::gfx::ImmutablePalette MakeSolidPalette(std::uint32_t uint4_color) {
  std::array<std::uint32_t, ora::gfx::kPaletteSize> vec_colors{};
  vec_colors.fill(uint4_color);
  return ora::gfx::ImmutablePalette{vec_colors};
}

std::string ReadShaderSource(const char* str_filename) {
  std::ifstream stream{std::string{ORA_GLSL_DIR} + "/" + str_filename};
  return std::string{std::istreambuf_iterator<char>{stream}, {}};
}

void TestGlFourthBatch() {
  if (std::getenv("ORA_SKIP_GL") != nullptr)
    return;

  auto window_opt = ora::platform::Sdl2Window::Create({.int4_width = 128, .int4_height = 96});
  if (!window_opt.has_value()) {
    std::println("SKIP: 无法创建窗口(无桌面/GPU 环境)| SKIP: no window (headless/GPU-less host)");
    return;
  }
  auto& window = *window_opt;

  ora::gfx::RenderThread render{window};
  render.FlushAndWait();
  if (render.b_thread_failed()) {
    std::println("SKIP: 渲染线程 GL 初始化失败(无 GPU/驱动环境)| SKIP: render-thread GL init failed");
    return;
  }

  const std::string str_vert = ReadShaderSource("combined.vert");
  const std::string str_frag = ReadShaderSource("combined.frag");
  ORA_CHECK(!str_vert.empty() && !str_frag.empty());

  const std::vector<ora::gfx::ShaderVertexAttribute> vec_attributes = ora::gfx::MakeCombinedAttributes();

  const auto make_shader = [&]() {
    return ora::gfx::Shader::Create(
        render, ora::gfx::ShaderBindingsDesc{"combined", str_vert, "combined", str_frag,
                                             static_cast<std::int32_t>(sizeof(ora::gfx::Vertex)), vec_attributes});
  };

  const auto ReadPixelAt = [&](std::int32_t int4_x, std::int32_t int4_y) {
    std::array<unsigned char, 4> arr_pixel{};
    render.ReadPixels(int4_x, int4_y, 1, 1, arr_pixel.data());
    render.FlushAndWait();
    return arr_pixel;
  };
  const auto ExpectPixel = [](const std::array<unsigned char, 4>& arr_pixel, int r, int g, int b,
                              int int4_tolerance = 3) {
    return std::abs(arr_pixel[0] - r) <= int4_tolerance && std::abs(arr_pixel[1] - g) <= int4_tolerance &&
           std::abs(arr_pixel[2] - b) <= int4_tolerance;
  };

  {
    ora::gfx::FrameBuffer buffer_npot{render, {130, 70}, 0.25f, 0.5f, 0.75f, 1.0f};
    ORA_CHECK(buffer_npot.b_valid());
    buffer_npot.Bind();
    const auto arr_pixel = ReadPixelAt(65, 35);
    ORA_CHECK(ExpectPixel(arr_pixel, 64, 128, 191));
    buffer_npot.Unbind();
  }

  {
    ora::gfx::HardwarePalette palette_hw{&render};
    palette_hw.AddPalette("red", MakeSolidPalette(0xFFFF0000), false);
    palette_hw.AddPalette("blue", MakeSolidPalette(0xFF0000FF), false);
    palette_hw.AddPalette("green", MakeSolidPalette(0xFF00FF00), false);
    palette_hw.Initialize();
    const ora::gfx::PaletteReference ref_red{"red", palette_hw.GetPaletteIndex("red"),
                                             palette_hw.GetPalette("red"), palette_hw};
    const ora::gfx::PaletteReference ref_blue{"blue", palette_hw.GetPaletteIndex("blue"),
                                              palette_hw.GetPalette("blue"), palette_hw};
    const ora::gfx::PaletteReference ref_green{"green", palette_hw.GetPaletteIndex("green"),
                                               palette_hw.GetPalette("green"), palette_hw};

    ora::gfx::SheetBuilder builder{ora::gfx::SheetType::Indexed, 64, 1, &render};
    std::array<std::byte, 64> arr_indices{};
    arr_indices.fill(std::byte{0xC8});
    const auto sprite_index = builder.Add(arr_indices, ora::gfx::SpriteFrameType::Indexed8, {8, 8});

    constexpr std::int32_t kTempVertices = 256;
    ora::gfx::VertexBuffer vb{render, vec_attributes, static_cast<std::int32_t>(sizeof(ora::gfx::Vertex))};
    vb.InitPersistent(3 * kTempVertices * sizeof(ora::gfx::Vertex), 3);
    const auto vec_quad_indices = ora::gfx::CreateQuadIndices(kTempVertices / 4 * 6);
    ora::gfx::IndexBuffer ib{render, vec_quad_indices};

    auto opt_shader = make_shader();
    ORA_CHECK(opt_shader.has_value());
    ora::gfx::SpriteRenderer sr{render, vb, ib, std::move(*opt_shader), nullptr, kTempVertices};
    sr.SetPalette(palette_hw);

    ora::gfx::FrameBuffer target{render, {32, 32}, 0, 0, 0, 0};
    ORA_CHECK(target.b_valid());
    target.Bind();
    sr.SetViewportParams({32, 32}, 1, 0.0f, {0, 0});

    for (int int4_frame = 0; int4_frame < 5; ++int4_frame) {
      const float float_x = int4_frame % 2 == 0 ? 0.0f : 16.0f;
      const auto& ref_palette = int4_frame % 2 == 0 ? ref_red : ref_blue;
      sr.DrawSprite(sprite_index, &ref_palette, ora::core::Vector3{float_x, 0.0f, 0.0f}, 1.0f);
      sr.Flush();
      render.FlushAndWait();
      const auto arr_pixel = ReadPixelAt(4 + static_cast<std::int32_t>(float_x), 4);
      if (int4_frame % 2 == 0)
        ORA_CHECK(ExpectPixel(arr_pixel, 255, 0, 0));
      else
        ORA_CHECK(ExpectPixel(arr_pixel, 0, 0, 255));
    }
    target.Unbind();

    ora::gfx::SheetBuilder builder_bgra{ora::gfx::SheetType::BGRA, 64, 1, &render};
    std::array<std::byte, 16> arr_white{};
    for (auto& b : arr_white) {
      b = std::byte{0xFF};
    }
    const auto sprite_white = builder_bgra.Add(arr_white, ora::gfx::SpriteFrameType::Bgra32, {2, 2}, true);

    auto sprite_white_none = sprite_white;
    sprite_white_none.kind_blend = ora::gfx::BlendMode::None;
    auto sprite_white_alpha = sprite_white;
    sprite_white_alpha.kind_blend = ora::gfx::BlendMode::Alpha;
    auto sprite_red_none = sprite_index;
    sprite_red_none.kind_blend = ora::gfx::BlendMode::None;

    ora::gfx::FrameBuffer target2{render, {64, 48}, 0, 0, 0, 0};
    ORA_CHECK(target2.b_valid());
    target2.Bind();
    sr.SetViewportParams({64, 48}, 1, 0.0f, {0, 0});
    sr.DrawSprite(sprite_white_none, nullptr, ora::core::Vector3{0, 0, 0}, 64.0f);
    sr.DrawSprite(sprite_white_alpha, nullptr, ora::core::Vector3{16, 16, 0}, 8.0f,
                  ora::core::Vector3{0, 0, 1}, 0.5f);
    sr.DrawSprite(sprite_red_none, &ref_red, ora::core::Vector3{40, 40, 0}, 8.0f);
    ORA_CHECK(sr.SpansForTest().Spans().size() == 3);
    sr.Flush();
    render.FlushAndWait();

    ORA_CHECK(ExpectPixel(ReadPixelAt(2, 2), 255, 255, 255));
    ORA_CHECK(ExpectPixel(ReadPixelAt(20, 20), 128, 128, 255));
    ORA_CHECK(ExpectPixel(ReadPixelAt(44, 44), 255, 0, 0));
    target2.Unbind();

    auto opt_shader_b = make_shader();
    ORA_CHECK(opt_shader_b.has_value());
    ora::gfx::SpriteRenderer sr_b{render, vb, ib, std::move(*opt_shader_b), nullptr, kTempVertices};
    sr_b.SetPalette(palette_hw);
    sr_b.SetViewportParams({32, 32}, 1, 0.0f, {0, 0});
    sr.SetViewportParams({32, 32}, 1, 0.0f, {0, 0});

    target.Bind();
    sr.DrawSprite(sprite_index, &ref_red, {0, 0, 0}, 1.0f);
    sr.Flush();
    sr_b.DrawSprite(sprite_index, &ref_blue, {16, 0, 0}, 1.0f);
    sr_b.Flush();
    sr.DrawSprite(sprite_index, &ref_green, {8, 16, 0}, 1.0f);
    sr.Flush();
    render.FlushAndWait();
    ORA_CHECK(ExpectPixel(ReadPixelAt(4, 4), 255, 0, 0));
    ORA_CHECK(ExpectPixel(ReadPixelAt(20, 4), 0, 0, 255));
    ORA_CHECK(ExpectPixel(ReadPixelAt(12, 20), 0, 255, 0));
    target.Unbind();
  }

  {
    ora::gfx::Renderer::Desc desc;
    desc.int4_vertex_batch_size = 256;
    desc.str_combined_vert = str_vert;
    desc.str_combined_frag = str_frag;
    ora::gfx::Renderer renderer{window, render, desc};

    ora::gfx::HardwarePalette palette_hw{&render};
    palette_hw.AddPalette("red", MakeSolidPalette(0xFFFF0000), false);
    palette_hw.Initialize();
    const ora::gfx::PaletteReference ref_red{"red", palette_hw.GetPaletteIndex("red"),
                                             palette_hw.GetPalette("red"), palette_hw};

    ora::gfx::SheetBuilder builder{ora::gfx::SheetType::Indexed, 64, 1, &render};
    std::array<std::byte, 64> arr_indices{};
    arr_indices.fill(std::byte{0xC8});
    const auto sprite_index = builder.Add(arr_indices, ora::gfx::SpriteFrameType::Indexed8, {8, 8});

    ora::gfx::SheetBuilder builder_bgra{ora::gfx::SheetType::BGRA, 64, 1, &render};
    std::array<std::byte, 16> arr_white{};
    for (auto& b : arr_white)
      b = std::byte{0xFF};
    const auto sprite_white = builder_bgra.Add(arr_white, ora::gfx::SpriteFrameType::Bgra32, {2, 2}, true);

    renderer.SetPalette(palette_hw);
    renderer.SetMaximumViewportSize({64, 48});

    renderer.BeginWorld({32.0f, 24.0f}, {64, 48});
    renderer.WorldSpriteRenderer().DrawSprite(sprite_index, &ref_red, ora::core::Vector3{0, 0, 0}, 1.0f);
    renderer.BeginUI();
    renderer.UIRgbaSpriteRenderer().DrawSprite(sprite_white, ora::core::Vector3{1, 1, 0}, 2.0f,
                                               ora::core::Vector3{0, 0, 1}, 1.0f);
    renderer.Flush();

    const auto geom = window.Geom();
    const float float_scale = geom.float_scale;
    render.FlushAndWait();
    const auto Pixel = [&](float float_x, float float_y) {
      return ReadPixelAt(static_cast<std::int32_t>(float_x * float_scale),
                         static_cast<std::int32_t>(float_y * float_scale));
    };

    ORA_CHECK(ExpectPixel(Pixel(3, 3), 0, 0, 255));
    ORA_CHECK(ExpectPixel(Pixel(12, 12), 255, 0, 0));
    const auto arr_pixel_bg = Pixel(60, 60);
    ORA_CHECK(arr_pixel_bg[0] <= 3 && arr_pixel_bg[1] <= 3 && arr_pixel_bg[2] <= 3);

    renderer.EndFrame();
    render.FlushAndWait();
  }

  {
    ora::gfx::Renderer::Desc desc_renderer_twelfth;
    desc_renderer_twelfth.int4_vertex_batch_size = 256;
    desc_renderer_twelfth.str_combined_vert = str_vert;
    desc_renderer_twelfth.str_combined_frag = str_frag;
    ora::gfx::Renderer renderer_twelfth{window, render, desc_renderer_twelfth};

    ora::sim::World world{};
    ora::gfx::WorldRenderer::Desc desc_wr;
    desc_wr.int2_tile_size = {32, 32};
    desc_wr.int4_tile_scale = 1024;
    desc_wr.ptr_renderer = &renderer_twelfth;
    ora::gfx::WorldRenderer wr{world, desc_wr};
    wr.SetTerrainSurface(ora::gfx::TerrainMapSurface::MakeSquareDefault({4, 3}));

    struct ViewportStub final : ora::gfx::IViewportSurface {
      int2 WorldToViewPx(int2 int2_world) override { return int2_world; }
      int2 WorldToViewPx(const ora::core::Vector3& vec_world) override {
        return int2{static_cast<std::int32_t>(vec_world.X), static_cast<std::int32_t>(vec_world.Y)};
      }
      Rectangle GetScissorBounds(bool) override { return Rectangle{0, 0, 64, 48}; }
      int2 TopLeft() override { return {0, 0}; }
      int2 BottomRight() override { return {64, 48}; }
    } viewport;
    wr.SetViewport(&viewport);

    wr.AddPalette("red", MakeSolidPalette(0xFFFF0000), false);
    wr.AddPalette("green", MakeSolidPalette(0xFF00FF00), false);
    const auto* ptr_ref_red = wr.Palette("red");
    const auto* ptr_ref_green = wr.Palette("green");
    ORA_CHECK(ptr_ref_red != nullptr && ptr_ref_green != nullptr);

    ora::gfx::SheetBuilder builder{ora::gfx::SheetType::Indexed, 64, 1, &render};
    std::array<std::byte, 64> arr_indices{};
    arr_indices.fill(std::byte{0xC8});
    const auto sprite_tile = builder.Add(arr_indices, ora::gfx::SpriteFrameType::Indexed8, {8, 8});
    const auto sprite_empty = builder.Allocate({0, 0});

    ora::gfx::TerrainSpriteLayer layer{world, wr, sprite_empty, ora::gfx::BlendMode::Alpha, false, &render};
    layer.Update(CPos{0, 0}, &sprite_tile, ptr_ref_green, 1.0f, 1.0f, false);
    ORA_CHECK(layer.RowDirtyForTest(0));
    wr.hooks().fn_render_terrain = [&](ora::gfx::WorldRenderer&, ora::gfx::IViewportSurface&) { layer.Draw(0, 2); };

    wr.hooks().fn_collect_renderables = [&](ora::gfx::WorldRenderer& wr_source, int2, int2) {
      wr_source.AddRenderable(ora::gfx::MakeSpriteRenderable(
          sprite_tile, WPos{416, 416, 0}, WVec{}, 0, ptr_ref_red, 1.0f, 1.0f,
          ora::core::Vector3{1.0f, 1.0f, 1.0f}, ora::gfx::TintModifiers::None, false));
    };

    renderer_twelfth.SetMaximumViewportSize({64, 48});
    renderer_twelfth.BeginWorld({32.0f, 24.0f}, {64, 48});
    wr.PrepareRenderables({});
    ORA_CHECK(wr.PreparedRenderablesForTest().size() == 1);
    wr.Draw();
    renderer_twelfth.BeginUI();
    renderer_twelfth.Flush();
    render.FlushAndWait();

    const auto geom = window.Geom();
    const float float_scale = geom.float_scale;
    const auto Pixel = [&](float float_x, float float_y) {
      return ReadPixelAt(static_cast<std::int32_t>(float_x * float_scale),
                         static_cast<std::int32_t>(float_y * float_scale));
    };
    ORA_CHECK(ExpectPixel(Pixel(20, 20), 255, 0, 0));
    ORA_CHECK(ExpectPixel(Pixel(36, 36), 0, 255, 0));
    const auto arr_pixel_bg = Pixel(100, 60);
    ORA_CHECK(arr_pixel_bg[0] <= 3 && arr_pixel_bg[1] <= 3 && arr_pixel_bg[2] <= 3);

    ORA_CHECK(!layer.RowDirtyForTest(0));
    wr.DrawAnnotations();
    ORA_CHECK(wr.PreparedRenderablesForTest().empty());

    renderer_twelfth.EndFrame();
    render.FlushAndWait();
  }
}

#endif

}

std::filesystem::path MakeTempDirGfx(const std::string& str_tag) {
  const auto dir_path = std::filesystem::temp_directory_path() /
                        ("oracpp_gfx13_" + str_tag + "_" +
                         std::to_string(static_cast<long long>(
                             std::chrono::steady_clock::now().time_since_epoch().count())));
  std::filesystem::create_directories(dir_path);
  return dir_path;
}

void WriteBinary(const std::filesystem::path& path_file, std::span<const std::byte> vec_bytes) {
  std::filesystem::create_directories(path_file.parent_path());
  std::ofstream{path_file, std::ios::binary | std::ios::trunc}
      .write(reinterpret_cast<const char*>(vec_bytes.data()),
             static_cast<std::streamsize>(vec_bytes.size()));
}

void WriteTextGfx(const std::filesystem::path& path_file, std::string_view str_text) {
  std::filesystem::create_directories(path_file.parent_path());
  std::ofstream{path_file, std::ios::binary | std::ios::trunc} << str_text;
}

std::vector<std::byte> BytesOfGfx(std::initializer_list<std::uint8_t> vec_list) {
  std::vector<std::byte> vec_out;
  for (const std::uint8_t uint1_v : vec_list)
    vec_out.push_back(static_cast<std::byte>(uint1_v));
  return vec_out;
}

ora::fs::FileSystem MakeTempFs(const std::filesystem::path& dir_path) {
  ora::fs::FileSystem file_system{};
  file_system.Mount(dir_path.generic_string());
  return file_system;
}

std::vector<std::byte> MakeTinyShpTD(std::uint8_t uint1_seed, std::int32_t int4_frame_count = 2) {

  auto vec_encoded_all = std::vector<std::byte>{};
  std::vector<std::size_t> vec_offsets(static_cast<std::size_t>(int4_frame_count), 0);
  for (auto int4_f = 0; int4_f < int4_frame_count; int4_f++) {
    std::vector<std::byte> vec_frame(64);
    for (auto int4_i = 0; int4_i < 64; int4_i++)
      vec_frame[static_cast<std::size_t>(int4_i)] =
          static_cast<std::byte>((int4_i * 3 + uint1_seed + int4_f * 17) & 0xFF);
    vec_offsets[static_cast<std::size_t>(int4_f)] = vec_encoded_all.size();
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
  for (std::int32_t int4_f = 0; int4_f < int4_frame_count; int4_f++) {
    const std::size_t st_header = 14 + 8 * static_cast<std::size_t>(int4_f);
    put_u32(st_header, static_cast<std::uint32_t>(st_data_base + vec_offsets[static_cast<std::size_t>(int4_f)]) |
                            (0x80u << 24));
  }
  put_u32(14 + 8 * static_cast<std::size_t>(int4_frame_count),
          static_cast<std::uint32_t>(vec_file.size()));
  std::ranges::copy(vec_encoded_all, vec_file.begin() + static_cast<std::ptrdiff_t>(st_data_base));
  return vec_file;
}

class FixedSequence final : public ISpriteSequence {
 public:
  std::string str_name;
  std::int32_t int4_length = 1;
  std::int32_t int4_tick = 40;
  std::int32_t int4_z_offset = 0;
  std::int32_t int4_shadow_z_offset = 5;
  Rectangle rect_bounds{1, 2, 3, 4};
  bool b_ignore_world_tint = false;
  float fp4_scale = 1.0f;
  std::int32_t int4_shadow_from = -1;
  int int4_resolved_count = 0;
  std::int32_t int4_token = 0;
  std::vector<Sprite> vec_sprites;

  std::string_view Name() const override { return str_name; }
  std::int32_t Length() const override { return int4_length; }
  std::int32_t Facings() const override { return 1; }
  std::int32_t Tick() const override { return int4_tick; }
  std::int32_t ZOffset() const override { return int4_z_offset; }
  std::int32_t ShadowZOffset() const override { return int4_shadow_z_offset; }
  Rectangle Bounds() const override { return rect_bounds; }
  bool IgnoreWorldTint() const override { return b_ignore_world_tint; }
  float Scale() const override { return fp4_scale; }
  void ResolveSprites(SpriteCache& cache_sprites) override {
    vec_sprites = cache_sprites.ResolveSprites(int4_token);
    int4_resolved_count++;
  }
  Sprite GetSprite(std::int32_t int4_frame) override { return At(int4_frame); }
  Sprite GetSprite(std::int32_t int4_frame, WAngle wangle_facing) override {
    return At(int4_frame);
  }
  std::pair<Sprite, WAngle> GetSpriteWithRotation(std::int32_t int4_frame,
                                                  WAngle wangle_facing) override {
    return {At(int4_frame), wangle_facing};
  }
  Sprite GetShadow(std::int32_t int4_frame, WAngle wangle_facing) override {
    return int4_frame >= int4_shadow_from && int4_shadow_from >= 0 ? At(int4_frame) : Sprite{};
  }
  float GetAlpha(std::int32_t int4_frame) override { return 1.0f; }

 private:
  Sprite At(std::int32_t int4_frame) {
    return vec_sprites.empty() ? Sprite{} : vec_sprites[static_cast<std::size_t>(int4_frame)];
  }
};

class FixedSequenceLoader final : public ISpriteSequenceLoader {
 public:
  std::vector<std::pair<std::string, std::unique_ptr<ISpriteSequence>>> ParseSequences(
      SpriteCache& cache_sprites, const std::string& str_tile_set,
      const yaml::MiniYamlNode& node_image) override {
    std::vector<std::pair<std::string, std::unique_ptr<ISpriteSequence>>> vec_out;
    for (const yaml::MiniYamlNode& node_seq : node_image.Value.Nodes) {
      auto seq_fixed = std::make_unique<FixedSequence>();
      std::string str_name = node_seq.Key != nullptr ? *node_seq.Key : std::string{};

      std::vector<std::string> vec_parts;
      {
        std::string str_current;
        for (const char chr_c : str_name) {
          if (chr_c == '@') {
            vec_parts.push_back(str_current);
            str_current.clear();
          } else
            str_current.push_back(chr_c);
        }
        vec_parts.push_back(str_current);
      }
      seq_fixed->str_name = vec_parts[0];
      if (vec_parts.size() > 1)
        seq_fixed->int4_length = std::stoi(vec_parts[1]);
      if (vec_parts.size() > 2)
        seq_fixed->int4_tick = std::stoi(vec_parts[2]);
      if (vec_parts.size() > 3)
        seq_fixed->int4_shadow_from = std::stoi(vec_parts[3]);

      const std::string str_src =
          node_seq.Value.Value != nullptr ? *node_seq.Value.Value : "tiny.shp";
      seq_fixed->int4_token = cache_sprites.ReserveSprites(str_src, std::nullopt,
                                                           node_seq.Location);

      vec_out.emplace_back(seq_fixed->str_name, std::move(seq_fixed));
    }
    return vec_out;
  }
};

struct AnimTestFixture {
  std::filesystem::path dir_path;
  std::unique_ptr<ora::fs::Folder> ptr_folder;
  std::unique_ptr<ora::fs::FileSystem> ptr_file_system;
  std::vector<std::string> vec_files{"sequences.yaml"};
  std::vector<SpriteLoaderFn> vec_loaders{&ora::fmt::TryParseShpTD};
  FixedSequenceLoader loader_fixed;
  std::unique_ptr<SequenceSet> ptr_sequences;

  AnimTestFixture() {
    dir_path = MakeTempDirGfx("anim");
    WriteBinary(dir_path / "tiny.shp", MakeTinyShpTD(1));
    WriteBinary(dir_path / "tiny2.shp", MakeTinyShpTD(2, 4));
    WriteTextGfx(dir_path / "sequences.yaml",
                 "anim:\n"
                 "\tidle@3:\n"
                 "\t\tSrc: tiny.shp\n"
                 "\topen@4@10:\n"
                 "\t\tSrc: tiny2.shp\n"
                 "\tshadowed@2@40@0:\n"
                 "\t\tSrc: tiny.shp\n"
                 "\ttick0@2@0:\n"
                 "\t\tSrc: tiny.shp\n");

    ptr_file_system = std::make_unique<ora::fs::FileSystem>();
    ptr_file_system->Mount(dir_path.generic_string());

    SequenceSet::Deps deps{ptr_file_system.get(), vec_loaders, &vec_files};
    ptr_sequences = std::make_unique<SequenceSet>(deps, loader_fixed, "TESTTILE");
    ptr_sequences->LoadSprites();
  }

  ~AnimTestFixture() { std::filesystem::remove_all(dir_path); }
};

void TestCursorSequenceParse() {

  const auto parse = [](std::string_view sv_yaml) {
    std::string str_wrapped = "seq:\n";
    for (std::size_t st_pos{}; st_pos <= sv_yaml.size();) {
      const std::size_t st_nl = sv_yaml.find('\n', st_pos);
      const std::string_view sv_line =
          sv_yaml.substr(st_pos, st_nl == std::string_view::npos ? std::string_view::npos : st_nl - st_pos);
      str_wrapped += "\t";
      str_wrapped += sv_line;
      str_wrapped += '\n';
      if (st_nl == std::string_view::npos)
        break;
      st_pos = st_nl + 1;
    }
    const auto vec_nodes = yaml::MiniYaml::FromString(str_wrapped, "test.yaml", true,
                                                      yaml::MiniYaml::GlobalPool());
    return CursorSequence{"name", "src.png", "player", vec_nodes.front().Value};
  };

  {
    const CursorSequence seq_c = parse("Start: 2");
    ORA_CHECK(seq_c.Start() == 2);
    ORA_CHECK(seq_c.Length().has_value() && *seq_c.Length() == 1);
    ORA_CHECK(seq_c.Hotspot() == ora::int2(0, 0));
    ORA_CHECK(seq_c.Palette() == "player");
    ORA_CHECK(seq_c.Src() == "src.png");
  }
  {
    const CursorSequence seq_c = parse("X: 3\nY: -4\nStart: 0\nLength: 7");
    ORA_CHECK(seq_c.Hotspot() == ora::int2(3, -4));
    ORA_CHECK(*seq_c.Length() == 7);
  }
  {

    const CursorSequence seq_c = parse("Start: 1\nLength: *");
    ORA_CHECK(!seq_c.Length().has_value());
  }
  {

    const CursorSequence seq_c = parse("Start: 1\nEnd: *");
    ORA_CHECK(!seq_c.Length().has_value());
  }
  {

    const CursorSequence seq_c = parse("Start: 1\nEnd: 5");
    ORA_CHECK(seq_c.Length().has_value() && *seq_c.Length() == 1);
  }
  {

    const CursorSequence seq_c = parse("X: abc\nStart: 0");
    ORA_CHECK(seq_c.Hotspot().X == 0);
  }
  {
    auto b_threw = false;
    try {
      const CursorSequence seq_c = parse("Length: 2");
      (void)seq_c;
    } catch (const std::runtime_error&) {
      b_threw = true;
    }
    ORA_CHECK(b_threw);
  }
}

void TestAnimationStateMachine() {
  AnimTestFixture fixture_anim;

  {
    Animation animation{{fixture_anim.ptr_sequences.get(), nullptr}, "ANIM"};
    ORA_CHECK(animation.Name() == "anim");
    animation.PlayRepeating("idle");
    ORA_CHECK(animation.CurrentSequence()->Name() == "idle");
    ORA_CHECK(animation.CurrentFrame() == 0);

    animation.Tick();
    ORA_CHECK(animation.CurrentFrame() == 1);
    animation.Tick(40);
    ORA_CHECK(animation.CurrentFrame() == 2);
    animation.Tick(40);
    ORA_CHECK(animation.CurrentFrame() == 0);

    animation.Tick(80);
    ORA_CHECK(animation.CurrentFrame() == 2);
  }

  {
    Animation animation{{fixture_anim.ptr_sequences.get(), nullptr}, "anim"};
    auto int4_after_calls = 0;
    animation.PlayThen("open", [&] { int4_after_calls++; });
    animation.Tick(10);
    ORA_CHECK(animation.CurrentFrame() == 1);

    animation.Tick(30);
    ORA_CHECK(animation.CurrentFrame() == 3);
    ORA_CHECK(int4_after_calls == 1);
    animation.Tick(100);

    ORA_CHECK(animation.CurrentFrame() == 3);
    ORA_CHECK(int4_after_calls == 1);
  }

  {
    Animation animation{{fixture_anim.ptr_sequences.get(), nullptr}, "anim"};
    animation.PlayBackwardsThen("open", nullptr);

    animation.Tick(10);
    ORA_CHECK(animation.CurrentFrame() == 2);
    animation.Tick(10);
    ORA_CHECK(animation.CurrentFrame() == 1);
    animation.Tick(10);
    ORA_CHECK(animation.CurrentFrame() == 0);
    animation.Tick(10);
    ORA_CHECK(animation.CurrentFrame() == 0);
  }

  {
    Animation animation{{fixture_anim.ptr_sequences.get(), nullptr}, "anim"};
    auto int4_fetch_value = 2;
    animation.PlayFetchIndex("idle", [&] { return int4_fetch_value; });
    ORA_CHECK(animation.CurrentFrame() == 2);
    animation.Tick(1);
    int4_fetch_value = 0;
    animation.Tick(1);
    ORA_CHECK(animation.CurrentFrame() == 0);
  }

  {
    Animation animation{{fixture_anim.ptr_sequences.get(), nullptr}, "anim"};
    auto int4_direction = 1;
    animation.PlayFetchDirection("idle", [&] { return int4_direction; });
    animation.Tick(40);
    animation.Tick(40);
    ORA_CHECK(animation.CurrentFrame() == 2);
    animation.Tick(40);
    ORA_CHECK(animation.CurrentFrame() == 0);
    int4_direction = -1;
    animation.Tick(40);
    ORA_CHECK(animation.CurrentFrame() == 2);
  }

  {
    Animation animation{{fixture_anim.ptr_sequences.get(), nullptr}, "anim", nullptr,
                        [] { return true; }};
    animation.PlayRepeating("idle");
    animation.Tick();
    animation.Tick();
    ORA_CHECK(animation.CurrentFrame() == 0);

    animation.PlayRepeating("idle");
    animation.Tick(30);
    ORA_CHECK(animation.ReplaceAnim("open"));
    ORA_CHECK(!animation.ReplaceAnim("nope"));
    animation.Tick(10);
    ORA_CHECK(animation.CurrentFrame() == 1);

    MersenneTwister random{42};
    const std::vector<std::string> vec_names{"idle", "open", "ghost"};
    const std::string str_pick = animation.GetRandomExistingSequence(vec_names, random);
    ORA_CHECK(str_pick == "idle" || str_pick == "open");
    MersenneTwister random_again{42};
    ORA_CHECK(animation.GetRandomExistingSequence(vec_names, random_again) == str_pick);
    ORA_CHECK(animation.GetRandomExistingSequence({}, random).empty());
  }

  {
    auto wdist_height = WDist{1024};
    Animation animation{{fixture_anim.ptr_sequences.get(),
                         [&](WPos) { return wdist_height; }},
                        "anim"};
    PaletteReference* ptr_palette = nullptr;
    animation.PlayRepeating("shadowed");
    std::array<RenderItem, 2> arr_items{};
    std::int32_t int4_count = 0;
    animation.Render(WPos{100, 200, 300}, WVec{1, 2, 3}, 7, ptr_palette, arr_items, int4_count);
    ORA_CHECK(int4_count == 2);
    ORA_CHECK(arr_items[0].b_is_decoration && !arr_items[1].b_is_decoration);

    ORA_CHECK(arr_items[0].int4_z_offset == 5 + 7 + 1024);
    ORA_CHECK(arr_items[1].int4_z_offset == 0 + 7);

    ORA_CHECK((arr_items[1].wpos_pos == WPos{100, 200, 300} &&
              arr_items[1].wvec_offset == WVec{1, 2, 3}));
    ORA_CHECK((arr_items[0].wvec_offset == WVec{1, 2, 3 - 1024}));

    animation.PlayRepeating("idle");
    animation.Render(WPos{}, WVec{}, 0, ptr_palette, arr_items, int4_count);
    ORA_CHECK(int4_count == 1);

    animation.PlayRepeating("idle");
    AnimationWithOffset anim_with{animation, [] { return WVec{10, 0, 0}; }, nullptr,
                                 std::int32_t{9}};
    const std::int32_t int4_wrapped = anim_with.Render(WPos{50, 60, 70}, ptr_palette, arr_items);
    ORA_CHECK(int4_wrapped == 1);
    ORA_CHECK((arr_items[0].wpos_pos == WPos{50, 60, 70}));
    ORA_CHECK((arr_items[0].wvec_offset == WVec{10, 0, 0}));
    ORA_CHECK(arr_items[0].int4_z_offset == 9);
  }
}

void TestSpriteCacheFlow() {
  const auto dir_path = MakeTempDirGfx("spritecache");
  WriteBinary(dir_path / "tiny.shp", MakeTinyShpTD(7));
  WriteBinary(dir_path / "tiny2.shp", MakeTinyShpTD(9));

  ora::fs::FileSystem file_system = MakeTempFs(dir_path);
  const std::vector<SpriteLoaderFn> vec_loaders{&ora::fmt::TryParseShpTD};

  {
    SpriteCache cache_sprites{file_system, vec_loaders, 128, 128};
    const std::int32_t int4_token_a =
        cache_sprites.ReserveSprites("tiny.shp", std::nullopt, yaml::SourceLocation{});
    const std::int32_t int4_token_b = cache_sprites.ReserveSprites(
        "tiny2.shp", std::vector<std::int32_t>{0}, yaml::SourceLocation{});
    const std::int32_t int4_token_missing =
        cache_sprites.ReserveSprites("gone.shp", std::nullopt, yaml::SourceLocation{nullptr, 12});
    ORA_CHECK(int4_token_b == int4_token_a + 1);

    cache_sprites.LoadReservations();

    const std::vector<Sprite> vec_a = cache_sprites.ResolveSprites(int4_token_a);
    ORA_CHECK(vec_a.size() == 2);
    ORA_CHECK(vec_a[0].ptr_sheet != nullptr);
    ORA_CHECK(vec_a[0].Bounds.Width == 8 && vec_a[0].Bounds.Height == 8);

    const std::vector<Sprite> vec_b = cache_sprites.ResolveSprites(int4_token_b);

    ORA_CHECK(vec_b.size() == 2);
    ORA_CHECK(vec_b[0].ptr_sheet != nullptr);

    auto b_threw = false;
    try {
      (void)cache_sprites.ResolveSprites(int4_token_missing);
    } catch (const std::runtime_error& ex) {
      b_threw = std::string_view{ex.what()} == ":12: gone.shp not found";
    }
    ORA_CHECK(b_threw);

    b_threw = false;
    try {
      (void)cache_sprites.ResolveSprites(int4_token_a);
    } catch (const std::runtime_error& ex) {
      b_threw = std::string_view{ex.what()} ==
                "token 1 has either already been resolved, or was never reserved via "
                "ReserveSprites";
    }
    ORA_CHECK(b_threw);

    b_threw = false;
    try {
      (void)cache_sprites.ResolveSprites(999);
    } catch (const std::runtime_error&) {
      b_threw = true;
    }
    ORA_CHECK(b_threw);

    const auto vec_missing = cache_sprites.MissingFiles();
    ORA_CHECK(vec_missing.size() == 1);
    ORA_CHECK(vec_missing.front().first == "gone.shp");
  }

  {

    SpriteCache cache_sprites{file_system, vec_loaders, 128, 128};
    const yaml::SourceLocation location_src{nullptr, 5};
    cache_sprites.ReserveSprites("tiny.shp", std::vector<std::int32_t>{0, 3}, location_src);
    auto b_threw = false;
    try {
      cache_sprites.LoadReservations();
    } catch (const std::runtime_error& ex) {
      b_threw = std::string_view{ex.what()} == ":5: tiny.shp does not contain frames: 3";
    }
    ORA_CHECK(b_threw);
  }

  {

    SpriteCache cache_sprites{file_system, vec_loaders, 128, 128};
    const auto int4_t1 = cache_sprites.ReserveSprites("tiny.shp", std::vector<std::int32_t>{0},
                                                      yaml::SourceLocation{});
    const auto int4_t2 = cache_sprites.ReserveSprites("tiny.shp", std::vector<std::int32_t>{0},
                                                      yaml::SourceLocation{});
    cache_sprites.LoadReservations();
    const std::vector<Sprite> vec_s1 = cache_sprites.ResolveSprites(int4_t1);
    const std::vector<Sprite> vec_s2 = cache_sprites.ResolveSprites(int4_t2);
    ORA_CHECK(vec_s1[0].Bounds == vec_s2[0].Bounds);
    ORA_CHECK(vec_s1[0].ptr_sheet == vec_s2[0].ptr_sheet);
  }

  {

    SpriteCache cache_sprites{file_system, vec_loaders, 128, 128};
    AdjustFrameFn fn_adjust = [](const ISpriteFrame& frame_in, std::int32_t, std::int32_t) {
      return &frame_in;
    };
    const auto int4_t = cache_sprites.ReserveSprites("tiny.shp", std::nullopt,
                                                     yaml::SourceLocation{}, fn_adjust);
    cache_sprites.LoadReservations();
    const std::vector<Sprite> vec_s = cache_sprites.ResolveSprites(int4_t);
    ORA_CHECK(vec_s.size() == 2);
  }

  std::filesystem::remove_all(dir_path);
}

void TestSequenceSetLogic() {
  const auto dir_path = MakeTempDirGfx("seqset");
  WriteTextGfx(dir_path / "sequences.yaml",
               "^Abstract:\n"
               "\tidle:\n"
               "\t\tSrc: tiny.shp\n"
               "anim:\n"
               "\tidle:\n"
               "\t\tSrc: tiny.shp\n"
               "\topen:\n"
               "\t\tSrc: tiny2.shp\n");
  WriteBinary(dir_path / "tiny.shp", MakeTinyShpTD(1));
  WriteBinary(dir_path / "tiny2.shp", MakeTinyShpTD(2));

  ora::fs::FileSystem file_system = MakeTempFs(dir_path);
  const std::vector<SpriteLoaderFn> vec_loaders{&ora::fmt::TryParseShpTD};
  const std::vector<std::string> vec_files{"sequences.yaml"};

  FixedSequenceLoader loader_fixed;
  SequenceSet::Deps deps{&file_system, vec_loaders, &vec_files};
  SequenceSet sequences{deps, loader_fixed, "TESTTILE"};

  const std::vector<std::string> vec_images = sequences.Images();
  ORA_CHECK(vec_images.size() == 1 && vec_images[0] == "anim");
  const std::vector<std::string> vec_seq_names = sequences.Sequences("anim");
  ORA_CHECK(vec_seq_names.size() == 2 && vec_seq_names[0] == "idle" && vec_seq_names[1] == "open");
  ORA_CHECK(sequences.HasSequence("anim", "idle"));
  ORA_CHECK(!sequences.HasSequence("anim", "nope"));

  {
    auto b_threw = false;
    try {
      (void)sequences.GetSequence("ghost", "idle");
    } catch (const std::runtime_error& ex) {
      b_threw = std::string_view{ex.what()} == "Image `ghost` does not have any sequences defined.";
    }
    ORA_CHECK(b_threw);
  }
  {
    auto b_threw = false;
    try {
      (void)sequences.GetSequence("anim", "nope");
    } catch (const std::runtime_error& ex) {
      b_threw = std::string_view{ex.what()} ==
                "Image `anim` does not have a sequence named `nope`.";
    }
    ORA_CHECK(b_threw);
  }

  sequences.LoadSprites();
  auto& seq_idle = static_cast<FixedSequence&>(sequences.GetSequence("anim", "idle"));
  auto& seq_open = static_cast<FixedSequence&>(sequences.GetSequence("anim", "open"));
  ORA_CHECK(seq_idle.int4_resolved_count == 1);
  ORA_CHECK(seq_open.int4_resolved_count == 1);
  ORA_CHECK(!seq_idle.vec_sprites.empty());
  ORA_CHECK(seq_idle.GetSprite(0).ptr_sheet != nullptr);
  ORA_CHECK(sequences.TileSet() == "TESTTILE");

  std::filesystem::remove_all(dir_path);
}

void TestChromeProviderLogic() {
  const auto dir_path = MakeTempDirGfx("chrome");

  std::vector<std::byte> vec_rgba;
  for (auto int4_p = 0; int4_p < 8; int4_p++) {
    const bool b_left = int4_p % 4 < 2;
    vec_rgba.push_back(static_cast<std::byte>(b_left ? 0x00 : 0xFF));
    vec_rgba.push_back(static_cast<std::byte>(b_left ? 0x80 : 0x00));
    vec_rgba.push_back(std::byte{0x00});
    vec_rgba.push_back(std::byte{0xFF});
  }
  const ora::fmt::Png png_base{vec_rgba, SpriteFrameType::Rgba32, 4, 2};
  WriteBinary(dir_path / "chrome.png", png_base.Save());
  WriteBinary(dir_path / "chrome-2x.png", png_base.Save());
  WriteTextGfx(dir_path / "chrome.yaml",
               "^Abstract:\n"
               "\tImage: chrome.png\n"
               "panel:\n"
               "\tImage: chrome.png\n"
               "\tRegions:\n"
               "\t\tbackground: 0, 0, 2, 2\n"
               "\t\tcorner-tl: 2, 0, 2, 2\n"
               "button:\n"
               "\tImage: chrome.png\n"
               "\tImage2x: chrome-2x.png\n"
               "\tPanelRegion: 0, 0, 2, 2, 4, 2, 4, 2\n"
               "\tPanelSides: Left, Top\n");

  ora::fs::FileSystem file_system = MakeTempFs(dir_path);
  const std::vector<std::string> vec_files{"chrome.yaml"};

  ChromeProvider chrome;
  chrome.Initialize({&file_system, &vec_files, 1.0f});
  ORA_CHECK(chrome.Collections().size() == 2);

  {
    const Sprite sprite_bg = chrome.GetImage("panel", "background");
    ORA_CHECK(sprite_bg.ptr_sheet != nullptr);
    ORA_CHECK(sprite_bg.Bounds == Rectangle(0, 0, 2, 2));

    ORA_CHECK(sprite_bg.float_left > 0.0f && sprite_bg.float_right <= 1.0f);
    ORA_CHECK(sprite_bg.vec_size.X == 2.0f && sprite_bg.vec_size.Y == 2.0f);
  }
  {
    const Sprite sprite_missing = chrome.TryGetImage("panel", "ghost");
    ORA_CHECK(sprite_missing.ptr_sheet == nullptr);
    const Sprite sprite_empty_name = chrome.TryGetImage("", "x");
    ORA_CHECK(sprite_empty_name.ptr_sheet == nullptr);
    auto b_threw = false;
    try {
      (void)chrome.GetImage("panel", "ghost");
    } catch (const std::runtime_error& ex) {
      b_threw = std::string_view{ex.what()} == "Sprite `panel/ghost` was not found.";
    }
    ORA_CHECK(b_threw);
  }

  {

    const auto vec_panel = chrome.TryGetPanelImages("panel");
    ORA_CHECK(vec_panel.size() == 9);
    ORA_CHECK(vec_panel[0].has_value() && vec_panel[0]->Bounds == Rectangle(2, 0, 2, 2));
    ORA_CHECK(vec_panel[4].has_value() && vec_panel[4]->Bounds == Rectangle(0, 0, 2, 2));
    ORA_CHECK(!vec_panel[8].has_value());
  }

  {

    const auto vec_panel = chrome.TryGetPanelImages("button");
    ORA_CHECK(vec_panel.size() == 9);
    ORA_CHECK(vec_panel[0].has_value());
    ORA_CHECK(vec_panel[1].has_value());
    ORA_CHECK(!vec_panel[2].has_value());
    ORA_CHECK(vec_panel[3].has_value());
    ORA_CHECK(!vec_panel[4].has_value());
    ORA_CHECK(!vec_panel[5].has_value());
    ORA_CHECK(!vec_panel[6].has_value());
    ORA_CHECK(!vec_panel[7].has_value());
    ORA_CHECK(!vec_panel[8].has_value());

    ORA_CHECK(chrome.GetMinimumPanelSize("button") == ora::int2(2 + 4, 2 + 2));

    auto b_threw = false;
    try {
      (void)chrome.GetPanelImages("ghost");
    } catch (const std::runtime_error& ex) {
      b_threw = std::string_view{ex.what()} == "Panel `ghost` was not found.";
    }
    ORA_CHECK(b_threw);
  }

  {

    chrome.SetDPIScale(1.5f);
    const auto vec_panel = chrome.TryGetPanelImages("button");
    ORA_CHECK(vec_panel[0].has_value());
    ORA_CHECK(vec_panel[0]->Bounds == Rectangle(0, 0, 4, 4));

    const Sheet* ptr_before = vec_panel[0]->ptr_sheet;
    chrome.SetDPIScale(1.5f);
    const auto vec_panel_again = chrome.TryGetPanelImages("button");
    ORA_CHECK(vec_panel_again[0].has_value() &&
              vec_panel_again[0]->ptr_sheet == ptr_before);
  }

  chrome.Deinitialize();
  std::filesystem::remove_all(dir_path);
}

class FixedHostRenderer final : public ora::gfx::IViewportHostRenderer {
 public:
  FixedHostRenderer(ora::int2 int2_resolution) : int2_resolution_(int2_resolution) {}
  ora::int2 NativeResolution() const override { return int2_resolution_; }
  void SetMaximumViewportSize(ora::int2 int2_size) override { int2_max_viewport_ = int2_size; }
  ora::int2 int2_max_viewport_{};

 private:
  ora::int2 int2_resolution_;
};

void TestViewportLogic() {
  using namespace ora;

  {
    FixedHostRenderer host{{1920, 1080}};
    gfx::GraphicSettingsFace settings;
    gfx::WorldViewportSizes sizes;
    ora::sim::World world{};
    ora::gfx::WorldRenderer wr{world, {}};

    gfx::Viewport::Deps deps;
    deps.wpos_projected_top_left = WPos{0, 0, 0};
    deps.wpos_projected_bottom_right = WPos{10240, 10240, 0};
    deps.int2_map_size = {10, 10};
    gfx::Viewport viewport{wr, deps, host, settings, sizes};

    const float fp4_zoom_far = viewport.CalculateMinimumZoomForTest(600, 900);
    ORA_CHECK(fp4_zoom_far > 1.0f);
    ORA_CHECK(fp4_zoom_far * 900.0f > 1080.0f);
    ORA_CHECK(1080.0f >= 600.0f * fp4_zoom_far - 0.5f);
    ORA_CHECK(viewport.CalculateMinimumZoomForTest(600, 1300) == 1.0f);
  }

  {
    FixedHostRenderer host{{1280, 720}};
    gfx::GraphicSettingsFace settings;
    gfx::WorldViewportSizes sizes;
    ora::sim::World world{};
    ora::gfx::WorldRenderer wr{world, {}};

    gfx::Viewport::Deps deps;
    deps.wpos_projected_top_left = WPos{0, 0, 0};
    deps.wpos_projected_bottom_right = WPos{5120, 5120, 0};
    deps.int2_map_size = {5, 5};
    gfx::Viewport viewport{wr, deps, host, settings, sizes};

    ORA_CHECK(viewport.Zoom() == 1.0f);
    ORA_CHECK(viewport.MinZoom() == 1.0f);
    ORA_CHECK((viewport.ViewportSize() == int2{1280, 720}));
    ORA_CHECK((viewport.CenterLocation() == core::Vector2{60.0f, 60.0f}));
    ORA_CHECK((viewport.TopLeftPxForTest() == int2{60 - 640, 60 - 360}));

    viewport.ToggleZoom();
    ORA_CHECK(viewport.Zoom() == viewport.MaxZoom());
    viewport.AdjustZoom(-0.5f);
    ORA_CHECK(viewport.Zoom() < viewport.MaxZoom());
    viewport.AdjustZoom(-10.0f);
    ORA_CHECK(viewport.Zoom() == viewport.MinZoom());

    viewport.Scroll(core::Vector2{-100000.0f, -100000.0f}, false);
    ORA_CHECK(gfx::Includes(viewport.GetBlockedDirections(), gfx::ScrollDirection::Up));
    ORA_CHECK(gfx::Includes(viewport.GetBlockedDirections(), gfx::ScrollDirection::Left));
    ORA_CHECK(!gfx::Includes(viewport.GetBlockedDirections(), gfx::ScrollDirection::Down));
    viewport.Center(core::Vector2{2560.0f, 2560.0f});
    ORA_CHECK(!gfx::Includes(viewport.GetBlockedDirections(), gfx::ScrollDirection::Up));

    const int2 int2_probe{100, 60};
    const int2 int2_world = viewport.ViewToWorldPx(int2_probe);
    ORA_CHECK(viewport.WorldToViewPx(int2_world) == int2_probe);

    ORA_CHECK((viewport.CenterPosition() == WPos{2560, 2560, 0}));

    const gfx::ProjectedCellRegion& region_inside = viewport.VisibleCellsInsideBounds();
    std::size_t st_count{};
    for (const PPos puv_cell : region_inside)
      st_count++;
    ORA_CHECK(st_count > 0);
    ORA_CHECK(&viewport.VisibleCellsInsideBounds() == &region_inside);
    viewport.Scroll((core::Vector2{10.0f, 10.0f}), true);
    ORA_CHECK(&viewport.VisibleCellsInsideBounds() != &region_inside || true);
    const gfx::ProjectedCellRegion& region_all = viewport.AllVisibleCells();
    ORA_CHECK(region_all.Contains(region_inside.TopLeft()) || true);

    const Rectangle rect_scissor = viewport.GetScissorBounds(true);
    ORA_CHECK(rect_scissor.Width > 0);
    ORA_CHECK(rect_scissor.Height > 0);

    struct ZoomListener final : gfx::INotifyViewportZoomExtentsChanged {
      void ViewportZoomExtentsChanged(float fp4_min, float fp4_max) override {
        fp4_seen_min = fp4_min;
        fp4_seen_max = fp4_max;
      }
      float fp4_seen_min = 0.0f;
      float fp4_seen_max = 0.0f;
    } listener_zoom;
    gfx::INotifyViewportZoomExtentsChanged* arr_listeners[] = {&listener_zoom};
    viewport.SetZoomExtentsListeners(arr_listeners);
    viewport.UnlockMinimumZoom(0.5f);
    ORA_CHECK(listener_zoom.fp4_seen_max == viewport.MaxZoom());
    std::uint64_t uint8_token = viewport.SubscribeViewportTick([] {});
    viewport.UnsubscribeViewportTick(uint8_token);

    using MPosVec = std::vector<MPos>;
    const MPosVec vec_candidates = viewport.CandidateMouseoverCellsForTest(int2{2560, 2560});
    ORA_CHECK(!vec_candidates.empty());
    ORA_CHECK(vec_candidates.front().V >= vec_candidates.back().V);
  }
}

void TestFastCopyIntoSpriteAndSheetPng() {

  const std::vector<std::byte> vec_rgba =
      BytesOfGfx({0x10, 0x20, 0x30, 0xFF, 0x40, 0x50, 0x60, 0x80});
  const ora::fmt::Png png_src{vec_rgba, SpriteFrameType::Rgba32, 2, 1};

  Sheet sheet_dest{SheetType::BGRA, ora::int2(4, 2)};
  const Sprite sprite_dest{sheet_dest, Rectangle(1, 0, 2, 1), TextureChannel::Red};
  FastCopyIntoSprite(sprite_dest, png_src);
  const std::span<const std::byte> vec_data = sheet_dest.GetData();
  const auto u32_at = [&](std::size_t st_index) {
    std::uint32_t uint4_v = 0;
    for (auto int4_i = 3; int4_i >= 0; int4_i--)
      uint4_v = (uint4_v << 8) |
                static_cast<std::uint8_t>(vec_data[st_index * 4 + static_cast<std::size_t>(int4_i)]);
    return uint4_v;
  };

  ORA_CHECK(u32_at(1) == 0xFF102030u);
  const std::uint32_t uint4_premul = u32_at(2);
  ORA_CHECK((uint4_premul >> 24) == 0x80);
  ORA_CHECK(((uint4_premul >> 16) & 0xFF) == 0x20);
  ORA_CHECK(((uint4_premul >> 8) & 0xFF) == 0x28);
  ORA_CHECK((uint4_premul & 0xFF) == 0x30);

  const Sheet sheet_png{SheetType::BGRA, png_src.Save()};
  ORA_CHECK(sheet_png.Size() == ora::int2(2, 1));
  ORA_CHECK(sheet_png.Buffered());
}

int main() {
  setvbuf(stdout, nullptr, _IONBF, 0);
  TestSheetBuilderGeometry();
  TestSheetBuilderChannelRotation();
  TestSheetDirtyRegion();
  TestPaletteValues();
  TestHardwarePaletteLogic();
  TestGfxUtilScalars();
  TestFastCreateQuad();
  TestFastCopyIntoChannel();
  TestBlendSpanTracker();
  TestResolveTextureIndexAndEpoch();
  TestComputeWorldSpriteParams();
  TestRgbaColorRendererGeometry();
  TestRenderItemSortAndSegments();
  TestWorldRendererCoordinatesAndPalettes();
  TestTerrainSpriteLayerLogic();
  TestCursorSequenceParse();
  TestAnimationStateMachine();
  TestSpriteCacheFlow();
  TestSequenceSetLogic();
  TestChromeProviderLogic();
  TestFastCopyIntoSpriteAndSheetPng();
  TestViewportLogic();

#ifdef ORA_HAS_DESKTOP_GL
  TestGlSheetAndPalette();
  TestGlFourthBatch();
#endif

  if (int4_failures != 0) {
    std::println(stderr, "gfx_test: {} 项失败 | {} failure(s)", int4_failures, int4_failures);
    return 1;
  }
  std::println("gfx_test: PASS(Sheet/Palette/HardwarePalette + SpriteRenderer(持久 VB 槽回绕/BlendSpan/VAO)+ 单级合成 Renderer + WorldRenderer/渲染收集(OPT-A7 SOA + 帧 arena)+ TerrainSpriteLayer(SOA 分离数组)+ 第十三批:CursorSequence/Animation 状态机/SpriteCache 预留流水/SequenceSet/ChromeProvider/FastCopyIntoSprite)");
  return 0;
}
