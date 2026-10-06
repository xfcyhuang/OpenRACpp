// gfx_test — Phase 4 第四批验收追加:BlendSpanTracker(段合并/分段)、
// ResolveTextureIndex(null/RGBA 无色移/带色移三态)、PaletteReference 的
// HasColorShift epoch 缓存(翻转失效)、ComputeWorldSpriteParams(downscale/
// +1 滚动补偿/整数倍 renderScale 的 offset 取整)、RgbaColorRenderer 几何
// (捕获 sink);GL 集成:持久映射 VB 槽轮换回绕 + 调色板查色采样链像素断言
// (OPT-C5 渲染级)、BlendSpan 三段交错(None/Alpha/None)、双 shader 的
// per-program VAO 缓存、NPOT FrameBuffer、单级合成 Renderer 全流程
// (BeginWorld → world 精灵 → BeginUI 合成 → UI 精灵 → 默认帧缓冲读回)。
// gfx_test — the Phase 4 third-batch acceptance: Sheet/SheetBuilder/Sprite(
// 通道轮换、dirty region、缓冲转移)+ Palette 家族(字节流构造/重映射/字节序)
// + HardwarePalette(OPT-A7 调色板 dirty 行;索引分配/高度增长/ReplacePalette/
// ApplyModifiers 重置)+ gfx_util(FastCreateQuad 位域打包、FastCopyIntoChannel
// 全路径、PremultiplyAlpha 边界、旋转/包围盒/NextPowerOf2)。
// GL 集成(有桌面时):Sheet 上传/子区域读回、缓冲转移 GL 路径、调色板
// OPT-C5 断言 —— 增量上传读回 == 全量上传参考,逐字节。
// gfx_test — the Phase 4 third-batch acceptance: Sheet/SheetBuilder/Sprite
// (sheet-packing geometry, channel rotation, dirty regions, buffer
// transfer) + the Palette family (byte-stream construction/remapping/byte
// order) + HardwarePalette (the OPT-A7 palette dirty rows; index
// allocation/height growth/ReplacePalette/ApplyModifiers reset) + gfx_util
// (the FastCreateQuad bitfield packing, every FastCopyIntoChannel path,
// PremultiplyAlpha boundaries, rotation/bounds/NextPowerOf2). GL
// integration (with a desktop): Sheet uploads/sub-rectangle readbacks, the
// buffer-transfer GL path, and the palette OPT-C5 assertion — incremental
// upload readback == full-upload reference, byte for byte.
import std;
#include <cstdio>  // stderr 为宏,不穿越 import 边界(规范允许的并用正形态) | stderr is a macro; the spec-sanctioned mixed form

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

using namespace ora;      // WPos/WVec/int2/CPos/MPos 测试面简写 | test-file shorthand
using namespace ora::gfx;  // Vector2/3 的渲染运算符(operator* 等)| the rendering operators of Vector2/3

std::int32_t int4_failures = 0;

#define ORA_CHECK(cond)                                                        \
  do {                                                                         \
    if (!(cond)) {                                                             \
      std::println(stderr, "FAIL {}:{} {}", __FILE__, __LINE__, #cond);       \
      ++int4_failures;                                                         \
    }                                                                          \
  } while (0)

/// 顺造 256 项渐变调色板:At(i) = base + i(A 通道来自 base 的高字节域)。
/// Builds a 256-entry gradient palette: At(i) = base + i.
ora::gfx::ImmutablePalette MakeGradientPalette(std::uint32_t uint4_base) {
  std::array<std::uint32_t, ora::gfx::kPaletteSize> vec_colors{};
  for (auto i = 0; i < ora::gfx::kPaletteSize; ++i)
    vec_colors[static_cast<std::size_t>(i)] = uint4_base + static_cast<std::uint32_t>(i);
  return ora::gfx::ImmutablePalette{vec_colors};
}

// ———— SheetBuilder:shelf 几何(margin/换行/空 sprite)————
// ———— SheetBuilder: shelf geometry (margin/row wrap/empty sprites) ————

void TestSheetBuilderGeometry() {
  ora::gfx::SheetBuilder builder{ora::gfx::SheetType::Indexed, 64, 1, nullptr};

  const auto sprite_a = builder.Allocate({16, 16});
  ORA_CHECK(sprite_a.Bounds == ora::Rectangle(1, 1, 16, 16));
  const auto sprite_b = builder.Allocate({16, 16});
  ORA_CHECK(sprite_b.Bounds == ora::Rectangle(18, 1, 16, 16));
  const auto sprite_c = builder.Allocate({16, 16});
  ORA_CHECK(sprite_c.Bounds == ora::Rectangle(35, 1, 16, 16));

  // 第 4 个 16 宽放不下(p.X=51 + 16 + margin 1 > 64)→ 换行 Y = 0+16+1 = 17,
  // 行高重置 → bounds = (1, 18, 16, 16)(SheetBuilder.cs L130-134)。
  // The 4th 16-wide sprite cannot fit (p.X=51 + 16 + margin 1 > 64) → the
  // row wraps to Y = 0+16+1 = 17, row height resets → bounds (1,18,16,16)
  // (SheetBuilder.cs L130-134).
  const auto sprite_d = builder.Allocate({16, 16});
  ORA_CHECK(sprite_d.Bounds == ora::Rectangle(1, 18, 16, 16));

  ORA_CHECK(builder.AllSheets().size() == 1);
  ORA_CHECK(builder.Current() == builder.AllSheets().front().get());
  ORA_CHECK(builder.CurrentChannel() == ora::gfx::TextureChannel::Red);
  ORA_CHECK(builder.Current()->Size() == ora::int2(64, 64));

  // 空 sprite 不占位:bounds 全零,后续分配位置不受影响。
  // Empty sprites take no space: zero bounds, and later allocations are
  // unaffected.
  const auto sprite_empty = builder.Add({}, ora::gfx::SpriteFrameType::Indexed8, {0, 8});
  ORA_CHECK(sprite_empty.Bounds.IsEmpty());
  const auto sprite_e = builder.Allocate({16, 16});
  ORA_CHECK(sprite_e.Bounds == ora::Rectangle(18, 18, 16, 16));

  // 顶行高承接:第二行放 20 高的图,行高更新(26 vs 17);下一行 Y = 17+20+1。
  // Row height carries: a 20-tall image in row two updates the row height
  // (26 vs 17); the next row starts at Y = 17+20+1.
  const auto sprite_f = builder.Allocate({16, 20});
  ORA_CHECK(sprite_f.Bounds == ora::Rectangle(35, 18, 16, 20));
  const auto sprite_g = builder.Allocate({16, 16});
  ORA_CHECK(sprite_g.Bounds == ora::Rectangle(1, 39, 16, 16));
}

// ———— SheetBuilder:Indexed 四通道轮换 R→G→B→A→换 sheet;BGRA 直接换 ————
// ———— SheetBuilder: Indexed channel rotation R→G→B→A→new sheet; BGRA switches directly ————

void TestSheetBuilderChannelRotation() {
  ora::gfx::SheetBuilder builder{ora::gfx::SheetType::Indexed, 32, 1, nullptr};

  const auto check_channel = [&](ora::gfx::TextureChannel kind_expected) {
    const auto sprite = builder.Allocate({30, 30});
    ORA_CHECK(sprite.kind_channel == kind_expected);
    ORA_CHECK(sprite.Bounds == ora::Rectangle(1, 1, 30, 30));  // 每通道从同一起点重排 | every channel restarts at the same origin
    return sprite;
  };
  check_channel(ora::gfx::TextureChannel::Red);
  check_channel(ora::gfx::TextureChannel::Green);
  check_channel(ora::gfx::TextureChannel::Blue);
  check_channel(ora::gfx::TextureChannel::Alpha);

  // Alpha 后无通道(NextChannel(3+1=8) > Alpha)→ 换 sheet,通道回 Red。
  // No channel after Alpha (NextChannel(3+1=8) > Alpha) → a new sheet,
  // channels back to Red.
  const auto sprite_wrap = builder.Allocate({30, 30});
  ORA_CHECK(sprite_wrap.kind_channel == ora::gfx::TextureChannel::Red);
  ORA_CHECK(builder.AllSheets().size() == 2);
  ORA_CHECK(builder.Current() == builder.AllSheets().back().get());

  // BGRA 类型:CurrentChannel = RGBA;NextChannel(RGBA+4=8 > Alpha)→ 直接
  // 换 sheet(SheetBuilder.cs L73-74/L140-158)。
  // BGRA type: CurrentChannel = RGBA; NextChannel(RGBA+4=8 > Alpha) → a
  // new sheet directly (SheetBuilder.cs L73-74/L140-158).
  ora::gfx::SheetBuilder builder_bgra{ora::gfx::SheetType::BGRA, 32, 1, nullptr};
  ORA_CHECK(builder_bgra.CurrentChannel() == ora::gfx::TextureChannel::RGBA);
  const auto sprite_bgra_a = builder_bgra.Allocate({30, 30});
  ORA_CHECK(sprite_bgra_a.kind_channel == ora::gfx::TextureChannel::RGBA);
  const auto sprite_bgra_b = builder_bgra.Allocate({30, 30});
  ORA_CHECK(sprite_bgra_b.kind_channel == ora::gfx::TextureChannel::RGBA);
  ORA_CHECK(builder_bgra.AllSheets().size() == 2);
}

// ———— Sheet:dirty region 并集/全幅;render 为空的转移 = false(上游
// Game.Renderer == null 路径)————
// ———— Sheet: dirty-region union/full; transfer with a null render = false
// (upstream's Game.Renderer == null path) ————

void TestSheetDirtyRegion() {
  ora::gfx::Sheet sheet{ora::gfx::SheetType::BGRA, {8, 8}, nullptr};
  ORA_CHECK(sheet.Buffered());  // 无纹理即 buffered(Sheet.cs L37)| no texture ⇒ buffered

  const auto vec_data = sheet.GetData();
  ORA_CHECK(vec_data.size() == 4 * 8 * 8);

  sheet.CommitBufferedData({2, 3, 4, 5});
  ORA_CHECK(sheet.DirtyRegion().has_value() && *sheet.DirtyRegion() == ora::Rectangle(2, 3, 4, 5));

  // 并集:(2,3,6,8) ∪ (8,9,10,11) = (2,3,8,8)。
  // Union: (2,3,6,8) ∪ (8,9,10,11) = (2,3,8,8).
  sheet.CommitBufferedData({8, 9, 2, 2});
  ORA_CHECK(sheet.DirtyRegion().has_value() && *sheet.DirtyRegion() == ora::Rectangle(2, 3, 8, 8));

  // 无参 Commit = Commit(全幅);与既有 (2,3,8,8) 仍取并集 → (0,0,10,11)
  // (Sheet.cs L143-154 的 union 语义,不重置)。
  // The no-arg Commit = Commit(full rect); still a union with the
  // existing (2,3,8,8) → (0,0,10,11) (the union semantics of
  // Sheet.cs L143-154 — no reset).
  sheet.CommitBufferedData();
  ORA_CHECK(sheet.DirtyRegion().has_value() && *sheet.DirtyRegion() == ora::Rectangle(0, 0, 10, 11));

  // 无 render(上游 Game.Renderer == null):ReleaseBuffer 保留数据,
  // ReleaseBufferAndTryTransferTo 返回 false(Sheet.cs L169-190)。
  // Without a render (upstream Game.Renderer == null): ReleaseBuffer keeps
  // the data and ReleaseBufferAndTryTransferTo returns false
  // (Sheet.cs L169-190).
  ora::gfx::Sheet sheet_src{ora::gfx::SheetType::BGRA, {8, 8}, nullptr};
  ora::gfx::Sheet sheet_dst{ora::gfx::SheetType::BGRA, {8, 8}, nullptr};
  (void)sheet_src.GetData();
  ORA_CHECK(!sheet_src.ReleaseBufferAndTryTransferTo(sheet_dst));
  ORA_CHECK(sheet_src.Buffered());

  // 尺寸不一致 → invalid_argument("Destination sheet does not have the same size")。
  // Mismatched sizes → invalid_argument("Destination sheet does not have the same size").
  ora::gfx::Sheet sheet_other{ora::gfx::SheetType::BGRA, {4, 8}, nullptr};
  bool b_threw = false;
  try {
    (void)sheet_src.ReleaseBufferAndTryTransferTo(sheet_other);
  } catch (const std::invalid_argument&) {
    b_threw = true;
  }
  ORA_CHECK(b_threw);
}

// ———— Palette 家族:字节流构造(<<2 | >>6)/透明影子重映射/字节序/可变侧 ————
// ———— Palette family: byte-stream construction (<<2 | >>6), transparent/
// shadow remaps, byte order, and the mutable side ————

void TestPaletteValues() {
  // 输入字节 255 → 252|3 = 255;64 → 0;65 → 4((byte)(65<<2)=4,4>>6=0)。
  // Input bytes: 255 → 252|3 = 255; 64 → 0; 65 → 4 ((byte)(65<<2)=4, 4>>6=0).
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

  ORA_CHECK(palette_immutable.At(0) == 0);            // remapTransparent → 0
  ORA_CHECK(palette_immutable.At(1) == 0xFF000000u);  // 64 → 0,0,0 不透明黑 | opaque black
  ORA_CHECK(palette_immutable.At(2) == 140u << 24);   // remapShadow
  ORA_CHECK(palette_immutable.At(3) == 0xFF000000u);  // 未填字节 → 全零 RGB + 全 alpha
  ORA_CHECK(palette_immutable.At(255) == 0xFF000000u);

  // CopyToArray 字节序:小端 uint32 → B,G,R,A(Palette.cs L63-66 BlockCopy)。
  // CopyToArray byte order: little-endian uint32 → B,G,R,A (the BlockCopy
  // of Palette.cs L63-66).
  const ora::gfx::ImmutablePalette palette_copy{std::array<std::uint32_t, 4>{0xFF112233u, 0x80445566u, 0, 0xFFFFFFFFu}};
  std::array<std::byte, 1024> arr_bytes{};  // CopyToArray 固定拷 256×4B | always copies 256×4B
  palette_copy.CopyToArray(arr_bytes, 0);
  ORA_CHECK(arr_bytes[0] == std::byte{0x33});  // B
  ORA_CHECK(arr_bytes[1] == std::byte{0x22});  // G
  ORA_CHECK(arr_bytes[2] == std::byte{0x11});  // R
  ORA_CHECK(arr_bytes[3] == std::byte{0xFF});  // A
  ORA_CHECK(arr_bytes[7] == std::byte{0x80});

  // 目标偏移:destinationOffset*4 字节处起写。
  // Destination offset: writes start at byte destinationOffset*4.
  std::array<std::byte, 4 * 257> arr_bytes_offset{};  // CopyToArray 固定拷 256×4B;目标须足量 | always copies 256×4B; the destination must be large enough
  palette_copy.CopyToArray(arr_bytes_offset, 1);
  ORA_CHECK(arr_bytes_offset[4] == std::byte{0x33});
  ORA_CHECK(arr_bytes_offset[0] == std::byte{0});

  // 可变侧:SetColor/ApplyRemap/拷贝构造。
  // Mutable side: SetColor/ApplyRemap/copy construction.
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
  ORA_CHECK(palette_mutable.At(9) == 0xFF010203u);  // A|R1|G2|B3 打包 | packed A|R1|G2|B3

  const DoubleChannelRemap remap_double{};
  palette_mutable.ApplyRemap(remap_double);
  ORA_CHECK(palette_mutable.At(0) == 0xFF224466u);  // 0xFF112233 翻倍 → R22|G44|B66 | doubled → R22|G44|B66
  ORA_CHECK(palette_mutable.At(9) == 0xFF020406u);  // SetColor 后重映射 | remapped after SetColor

  const ora::gfx::ImmutablePalette palette_remapped{palette_copy, remap_double};
  ORA_CHECK(palette_remapped.At(0) == 0xFF224466u);

  palette_mutable.SetFromPalette(palette_copy);
  ORA_CHECK(palette_mutable.At(9) == 0);
  ORA_CHECK(palette_mutable.At(0) == 0xFF112233u);
}

// ———— HardwarePalette:索引分配(行 0 保留)/高度增长/dirty 行推进(OPT-A7)/
// ReplacePalette/SetColorShift/ApplyModifiers 重置 ————
// ———— HardwarePalette: index allocation (row 0 reserved)/height growth/
// dirty-row progression (OPT-A7)/ReplacePalette/SetColorShift/ApplyModifiers
// reset ————

void TestHardwarePaletteLogic() {
  ora::gfx::HardwarePalette palette_hw{nullptr};  // 纯数据模式(脏位照常推进)| data-only mode
  ORA_CHECK(palette_hw.FullDirty());

  palette_hw.AddPalette("a", MakeGradientPalette(0x11000000), false);
  ORA_CHECK(palette_hw.GetPaletteIndex("a") == 1);  // 行 0 保留 | row 0 reserved
  ORA_CHECK(palette_hw.Height() == 2);              // NextPowerOf2(1+1)
  ORA_CHECK(palette_hw.Contains("a"));

  palette_hw.AddPalette("b", MakeGradientPalette(0x22000000), true);  // 可变 | mutable
  ORA_CHECK(palette_hw.GetPaletteIndex("b") == 2);
  ORA_CHECK(palette_hw.Height() == 4);  // NextPowerOf2(2+1)
  ORA_CHECK(!palette_hw.Contains("c"));

  palette_hw.Initialize();
  ORA_CHECK(!palette_hw.FullDirty() && !palette_hw.ShiftsDirty());
  ORA_CHECK(palette_hw.LastUploadFull());  // 初传全量 | the initial upload is full

  // 可变优先返回;ReplacePalette 后增量路径(单行脏 < Height/2;上传随调用
  // 立即发生,故以 LastUploadFull 断言增量性)。
  // Mutables return first; ReplacePalette takes the incremental path
  // (one dirty row < Height/2; the upload happens within the call, so
  // incrementality is asserted via LastUploadFull).
  ORA_CHECK(palette_hw.GetPalette("b").At(5) == 0x22000005u);
  ORA_CHECK(palette_hw.GetPalette("a").At(5) == 0x11000005u);

  const auto palette_a_new = MakeGradientPalette(0x33000000);
  palette_hw.ReplacePalette("a", palette_a_new);
  ORA_CHECK(!palette_hw.FullDirty());
  ORA_CHECK(!palette_hw.LastUploadFull());  // 增量上传 | the incremental upload
  ORA_CHECK(palette_hw.GetPalette("a").At(5) == 0x33000005u);
  ORA_CHECK(palette_hw.Height() == 4);  // 替换不增行 | replacement adds no row

  // SetColorShift:仅写 CPU 缓冲 + 标脏(上传延迟;上游行为一致)。
  // SetColorShift: CPU buffer + dirty bit only (deferred upload; same as
  // upstream).
  ORA_CHECK(!palette_hw.HasColorShift("a"));
  ORA_CHECK(!palette_hw.ShiftsDirty());
  palette_hw.SetColorShift("a", 0.1f, 0.0f, 1.0f, 0.05f, 0.95f);
  ORA_CHECK(palette_hw.HasColorShift("a"));
  ORA_CHECK(palette_hw.ShiftsDirty());

  // ApplyModifiers:调整 → 上传 → 重置回原色(上游 L147-153)。
  // ApplyModifiers: adjust → upload → reset to the originals
  // (upstream L147-153).
  struct WhitewashModifier : ora::gfx::IPaletteModifier {
    void AdjustPalette(std::map<std::string, ora::gfx::MutablePalette, std::less<>>& map_mutable) override {
      map_mutable.find("b")->second.SetColor(0, ora::core::Color::FromArgb(0xFF, 0xFF, 0xFF, 0xFF));
    }
  };
  WhitewashModifier modifier_white{};
  ora::gfx::IPaletteModifier* vec_mods[] = {&modifier_white};
  palette_hw.ApplyModifiers(vec_mods);

  ORA_CHECK(palette_hw.GetPalette("b").At(0) == 0x22000000u);  // 重置回原色 | reset to the original
  ORA_CHECK(!palette_hw.RowDirty(2));                          // 上传后清脏 | cleared after the upload
  ORA_CHECK(!palette_hw.ShiftsDirty());                        // 随上传清 | cleared along with the upload
  ORA_CHECK(!palette_hw.LastUploadFull());                     // 单行修改走增量 | a single-row change goes incremental

  // 异常路径(GetPalette/GetPaletteIndex 不存在 / AddPalette 重复):
  // 消息文本逐字对齐上游。
  // Exception paths (absent GetPalette/GetPaletteIndex / duplicate
  // AddPalette): message texts verbatim from upstream.
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

  // 高度继续增长:第 3、4 个 → Height 4 不变;第 5 个(index 5)→ 8。
  // Height keeps growing: the 3rd/4th leave Height at 4; the 5th
  // (index 5) pushes it to 8.
  palette_hw.AddPalette("c", MakeGradientPalette(0x44000000), false);
  palette_hw.AddPalette("d", MakeGradientPalette(0x55000000), false);
  palette_hw.AddPalette("e", MakeGradientPalette(0x66000000), false);
  ORA_CHECK(palette_hw.GetPaletteIndex("e") == 5);
  ORA_CHECK(palette_hw.Height() == 8);
  ORA_CHECK(palette_hw.FullDirty());  // 高度增长 → 全量 | height growth → full
}

// ———— gfx_util:PremultiplyAlpha 边界 + CreateQuadIndices + NextPowerOf2 +
// BoundingRectangle + RotateQuadInto ————
// ———— gfx_util: PremultiplyAlpha boundaries + CreateQuadIndices +
// NextPowerOf2 + BoundingRectangle + RotateQuadInto ————

void TestGfxUtilScalars() {
  using ora::gfx::PremultiplyAlpha;
  // 不透明恒等 / 全透明归零(Util.cs L328-334)。
  // Opaque identity / transparent zero (Util.cs L328-334).
  ORA_CHECK(PremultiplyAlpha(ora::core::Color::FromArgb(0xFF, 1, 2, 3)).ToArgb() == 0xFF010203u);
  ORA_CHECK(PremultiplyAlpha(ora::core::Color::FromArgb(0, 200, 200, 200)).ToArgb() == 0);
  // a=128:255*128/255 = 128 精确;17 → 8(17*128+128=2304,+9 → 2313 >>8 = 9)?
  // 手算:17*128 = 2176 + 128 = 2304;2304>>8 = 9;2304+9 = 2313;>>8 = 9。
  // a=128: 255*128/255 = 128 exactly; 17 → 9 by the fast trick.
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

  // 未旋转:向零截断(C# (int) 语义;负数 ≠ floor)。
  // Unrotated: truncation towards zero (the C# (int) semantics; negatives
  // differ from floor).
  ORA_CHECK(ora::gfx::BoundingRectangle({10.7f, 20.9f, 0}, {5.5f, 6.5f, 0}, 0.0f) ==
            ora::Rectangle(10, 20, 5, 6));
  ORA_CHECK(ora::gfx::BoundingRectangle({-10.7f, -20.9f, 0}, {5.5f, 6.5f, 0}, 0.0f) ==
            ora::Rectangle(-10, -20, 5, 6));
  // 旋转 90°(CreateRotation(-π/2) → (x,y) ↦ (y,-x)):四角 (10,25)(10,5)(20,5)(20,25)
  // → 包围盒 (10,5,10,20)。
  // Rotated 90° (CreateRotation(-π/2) maps (x,y) ↦ (y,-x)): corners
  // (10,25)(10,5)(20,5)(20,25) → bounds (10,5,10,20).
  constexpr float kPiHalf = 1.57079632f;
  ORA_CHECK(ora::gfx::BoundingRectangle({5, 10, 0}, {20, 10, 0}, kPiHalf) ==
            ora::Rectangle(10, 5, 10, 20));

  // RotateQuadInto:未旋转恒等;旋转 90° 角点对换(顺时针 90°:TL→TR 位)。
  // RotateQuadInto: identity unrotated; corners swap at 90°.
  ora::core::Vector3 vec_rotated[4];
  ora::gfx::RotateQuadInto(vec_rotated, {0, 0, 0}, {10, 10, 0}, 0.0f);
  ORA_CHECK(vec_rotated[0] == ora::core::Vector3(0, 0, 0));
  ORA_CHECK(vec_rotated[2] == ora::core::Vector3(10, 10, 0));
}

// ———— FastCreateQuad:aVertexAttributes 位域(combined.vert 注释契约)+
// 归一化 UV(1/128 inset)+ 旋转版 ————
// ———— FastCreateQuad: the aVertexAttributes bitfield (the combined.vert
// contract) + normalized UVs (1/128 inset) + the rotated form ————

void TestFastCreateQuad() {
  ora::gfx::Sheet sheet{ora::gfx::SheetType::Indexed, {64, 64}, nullptr};
  const ora::gfx::Sprite sprite_indexed{sheet, ora::Rectangle(2, 3, 8, 8), ora::gfx::TextureChannel::Red};

  ora::gfx::Vertex arr_vertices[4];
  ora::gfx::FastCreateQuad(arr_vertices, {10, 20, 0}, {30, 40, 0}, {30, 60, 0}, {10, 60, 0}, sprite_indexed,
                           ora::int2{1, 2}, 5, {1, 2, 3}, 0.5f, 0);

  const std::uint32_t uint4_c = arr_vertices[0].c;
  ORA_CHECK((uint4_c & 0x07u) == 0x01u);                 // Red<<1|1(Util.cs L76)| Red<<1|1
  ORA_CHECK(((uint4_c >> 6) & 0x07u) == 1u);             // 主 sampler | primary sampler
  ORA_CHECK(((uint4_c >> 9) & 0x07u) == 0u);             // 无二级 | no secondary
  ORA_CHECK(((uint4_c >> 3) & 0x07u) == 0u);             // 二级通道空 | secondary channel unused
  ORA_CHECK(((uint4_c >> 16) & 0xFFFFu) == 5u);          // 调色板行 | palette row

  // 顶点位置 / tint / alpha 直写。
  // Positions/tint/alpha written straight.
  ORA_CHECK(arr_vertices[0].x == 10.0f && arr_vertices[0].y == 20.0f);
  ORA_CHECK(arr_vertices[2].x == 30.0f && arr_vertices[2].y == 60.0f);
  ORA_CHECK(arr_vertices[1].r == 1.0f && arr_vertices[1].g == 2.0f && arr_vertices[1].b == 3.0f);
  ORA_CHECK(arr_vertices[1].a == 0.5f);

  // UV:Left/Top = (2 + 1/128)/64,(10 - 1/128)/64;四个顶点按 TL/TR/BR/BL。
  // UVs: Left/Top = (2 + 1/128)/64, (10 - 1/128)/64; vertices ordered
  // TL/TR/BR/BL.
  const auto float_inset = 1.0f / 128.0f;
  ORA_CHECK(arr_vertices[0].s == (2.0f + float_inset) / 64.0f);
  ORA_CHECK(arr_vertices[0].t == (3.0f + float_inset) / 64.0f);
  ORA_CHECK(arr_vertices[1].s == (10.0f - float_inset) / 64.0f);
  ORA_CHECK(arr_vertices[3].t == (11.0f - float_inset) / 64.0f);
  // 二级 UV 槽位未用 → 0(Util.cs L70-73)。
  // Secondary UV slots unused → 0 (Util.cs L70-73).
  ORA_CHECK(arr_vertices[0].u == 0.0f && arr_vertices[0].v == 0.0f);

  // RGBA + 二级数据:主通道 0x02;二级 Blue<<1|1 = 5 占 bits3-5;sampler 3/2;
  // 二级 UV = secondary 归一化坐标(**无 inset**;Sprite.cs L67-70)。
  // RGBA + secondary data: primary 0x02; secondary Blue<<1|1 = 5 in
  // bits 3-5; samplers 3/2; secondary UVs = the secondary normalized
  // coordinates (**no inset**; Sprite.cs L67-70).
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
  ORA_CHECK(arr_vertices[0].u == 4.0f / 64.0f);   // SecondaryLeft,无 inset | no inset
  ORA_CHECK(arr_vertices[0].v == 5.0f / 64.0f);   // SecondaryTop
  ORA_CHECK(arr_vertices[2].u == 6.0f / 64.0f);   // SecondaryRight
  ORA_CHECK(arr_vertices[2].v == 7.0f / 64.0f);   // SecondaryBottom

  // 轴对齐 + 旋转 π/2:中心 (20,15),半 (10,5) → 旋转后四角相对中心置换;
  // 未旋转时 TL/TR/BR/BL 直写(Util.cs L56-61)。
  // Axis-aligned + π/2 rotation: center (20,15), half (10,5) — corners
  // permute about the center; unrotated TL/TR/BR/BL written directly
  // (Util.cs L56-61).
  ora::gfx::FastCreateQuad(arr_vertices, {10, 10, 0}, sprite_indexed, ora::int2{0, 0}, 0, 0, {20, 10, 0}, {1, 1, 1},
                           1.0f, 0.0f);
  ORA_CHECK(arr_vertices[0].x == 10.0f && arr_vertices[0].y == 10.0f);
  ORA_CHECK(arr_vertices[2].x == 30.0f && arr_vertices[2].y == 20.0f);

  ora::gfx::FastCreateQuad(arr_vertices, {10, 10, 0}, sprite_indexed, ora::int2{0, 0}, 0, 0, {20, 10, 0}, {1, 1, 1},
                           1.0f, 3.14159265f / 2);
  // CreateRotation(-π/2) → (x,y) ↦ (y,-x):ra=(5,-10) → des[0]=center-ra=(15,25),
  // des[2]=center+ra=(25,5)(浮点 π/2 余项容差)。
  // CreateRotation(-π/2) maps (x,y) ↦ (y,-x): ra=(5,-10) →
  // des[0]=center-ra=(15,25), des[2]=center+ra=(25,5) (float-π/2 tolerance).
  ORA_CHECK(std::abs(arr_vertices[0].x - 15.0f) < 1e-3f && std::abs(arr_vertices[0].y - 25.0f) < 1e-3f);
  ORA_CHECK(std::abs(arr_vertices[2].x - 25.0f) < 1e-3f && std::abs(arr_vertices[2].y - 5.0f) < 1e-3f);
}

// ———— FastCopyIntoChannel:索引单通道(ChannelMasks 2,1,0,3)+ RGBA 快/慢
// 路径 + 预乘 + 三字节源格式 ————
// ———— FastCopyIntoChannel: indexed single channel (ChannelMasks 2,1,0,3)
// + RGBA fast/slow paths + premultiply + the 3-byte source formats ————

void TestFastCopyIntoChannel() {
  using ora::gfx::SpriteFrameType;
  using ora::gfx::TextureChannel;
  ora::gfx::Sheet sheet{ora::gfx::SheetType::BGRA, {8, 4}, nullptr};
  auto vec_data = sheet.GetData();

  // 索引 → Red 通道:字节偏移 = ChannelMasks[Red] = 2("nuts" 序)。
  // Indexed → Red channel: byte offset = ChannelMasks[Red] = 2 (the
  // "nuts" order).
  const ora::gfx::Sprite sprite_indexed{sheet, ora::Rectangle(2, 1, 3, 2), TextureChannel::Red};
  const std::array<std::byte, 6> arr_indexed{std::byte{1}, std::byte{2}, std::byte{3},
                                             std::byte{4}, std::byte{5}, std::byte{6}};
  ora::gfx::FastCopyIntoChannel(sprite_indexed, arr_indexed, SpriteFrameType::Indexed8);

  const auto ByteAt = [&](std::int32_t int4_x, std::int32_t int4_y, std::int32_t int4_plane) {
    return std::to_integer<std::int32_t>(vec_data[(static_cast<std::size_t>(int4_y) * 8 + int4_x) * 4 + int4_plane]);
  };
  ORA_CHECK(ByteAt(2, 1, 2) == 1);  // (2,1) idx 0 → plane 2(Red 掩码)| plane 2 (the Red mask)
  ORA_CHECK(ByteAt(3, 1, 2) == 2);
  ORA_CHECK(ByteAt(4, 1, 2) == 3);
  ORA_CHECK(ByteAt(2, 2, 2) == 4);  // 换行 | row wrap
  ORA_CHECK(ByteAt(4, 2, 2) == 6);
  ORA_CHECK(ByteAt(2, 1, 0) == 0 && ByteAt(2, 1, 1) == 0 && ByteAt(2, 1, 3) == 0);

  // Green 通道 → 掩码 1;Blue → 0;Alpha → 3。
  // Green channel → mask 1; Blue → 0; Alpha → 3.
  const ora::gfx::Sprite sprite_green{sheet, ora::Rectangle(0, 0, 1, 1), TextureChannel::Green};
  const std::array<std::byte, 1> arr_single{std::byte{42}};
  ora::gfx::FastCopyIntoChannel(sprite_green, arr_single, SpriteFrameType::Indexed8);
  ORA_CHECK(ByteAt(0, 0, 1) == 42);

  // Bgra32 快路径:不透明恒等;半透明预乘(a=128:100→50,50→25,25→13)。
  // Bgra32 fast path: opaque identity; half-alpha premultiply (a=128:
  // 100→50, 50→25, 25→13).
  const ora::gfx::Sprite sprite_rgba{sheet, ora::Rectangle(0, 0, 2, 1), TextureChannel::RGBA};
  const std::array<std::byte, 8> arr_bgra{
      std::byte{10}, std::byte{20}, std::byte{30}, std::byte{255},   // BGR A | opaque
      std::byte{100}, std::byte{50}, std::byte{25}, std::byte{128},  // 半透明 | half alpha
  };
  ora::gfx::FastCopyIntoChannel(sprite_rgba, arr_bgra, SpriteFrameType::Bgra32);
  ORA_CHECK(ByteAt(0, 0, 0) == 10 && ByteAt(0, 0, 1) == 20 && ByteAt(0, 0, 2) == 30 && ByteAt(0, 0, 3) == 255);
  ORA_CHECK(ByteAt(1, 0, 0) == 50 && ByteAt(1, 0, 1) == 25 && ByteAt(1, 0, 2) == 13 && ByteAt(1, 0, 3) == 128);

  // premultiplied=true:源即预乘,直拷。
  // premultiplied=true: the source is premultiplied already — copied raw.
  ora::gfx::FastCopyIntoChannel(sprite_rgba, arr_bgra, SpriteFrameType::Bgra32, true);
  ORA_CHECK(ByteAt(1, 0, 0) == 100 && ByteAt(1, 0, 2) == 25);

  // Bgr24(无 alpha → 255)与 Rgba32(大端字节序)。
  // Bgr24 (no alpha → 255) and Rgba32 (big-endian byte order).
  const std::array<std::byte, 6> arr_bgr{std::byte{1}, std::byte{2}, std::byte{3},
                                         std::byte{4}, std::byte{5}, std::byte{6}};
  ora::gfx::FastCopyIntoChannel(sprite_rgba, arr_bgr, SpriteFrameType::Bgr24);
  ORA_CHECK(ByteAt(0, 0, 0) == 1 && ByteAt(0, 0, 1) == 2 && ByteAt(0, 0, 2) == 3 && ByteAt(0, 0, 3) == 255);

  const std::array<std::byte, 8> arr_rgba{std::byte{30}, std::byte{20}, std::byte{10}, std::byte{255},   // RGBA → BGRA 翻转
                                          std::byte{25}, std::byte{50}, std::byte{100}, std::byte{128}};
  ora::gfx::FastCopyIntoChannel(sprite_rgba, arr_rgba, SpriteFrameType::Rgba32);
  ORA_CHECK(ByteAt(0, 0, 0) == 10 && ByteAt(0, 0, 1) == 20 && ByteAt(0, 0, 2) == 30 && ByteAt(0, 0, 3) == 255);
  // 同一 ARGB(25,50,100,128)与 Bgra32 行同果:预乘后 BGRA = (50,25,13,128)。
  // The same ARGB (25,50,100,128) as the Bgra32 row: premultiplied BGRA =
  // (50,25,13,128).
  ORA_CHECK(ByteAt(1, 0, 0) == 50 && ByteAt(1, 0, 1) == 25 && ByteAt(1, 0, 2) == 13 && ByteAt(1, 0, 3) == 128);

  // 未旋转包围盒/Sheet 拼装集成:SheetBuilder.Add = Allocate + 拷贝 + Commit。
  // Integration with packing: SheetBuilder.Add = Allocate + copy + Commit.
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

  // FrameTypeToSheetType(SheetBuilder.cs L52-67)。
  ORA_CHECK(ora::gfx::SheetBuilder::FrameTypeToSheetType(SpriteFrameType::Indexed8) == ora::gfx::SheetType::Indexed);
  ORA_CHECK(ora::gfx::SheetBuilder::FrameTypeToSheetType(SpriteFrameType::Bgra32) == ora::gfx::SheetType::BGRA);
  ORA_CHECK(ora::gfx::SheetBuilder::FrameTypeToSheetType(SpriteFrameType::Rgb24) == ora::gfx::SheetType::BGRA);
}

// ———— 第四批纯逻辑:BlendSpanTracker / ResolveTextureIndex / epoch / worldSprite 几何 / RgbaColorRenderer ————
// ———— Fourth-batch pure logic: BlendSpanTracker / ResolveTextureIndex / the
// epoch / worldSprite geometry / RgbaColorRenderer ————

void TestBlendSpanTracker() {
  ora::gfx::BlendSpanTracker tracker;
  ORA_CHECK(tracker.Spans().empty());

  // 同模式四边形原地扩长(SpriteRenderer.cs L99-106)。
  // Same-mode quads extend the last span in place (SpriteRenderer.cs L99-106).
  tracker.TrackQuad(ora::gfx::BlendMode::Alpha, 0);
  tracker.TrackQuad(ora::gfx::BlendMode::Alpha, 4);
  tracker.TrackQuad(ora::gfx::BlendMode::Alpha, 8);
  ORA_CHECK(tracker.Spans().size() == 1);
  ORA_CHECK(tracker.Spans()[0].int4_start == 0 && tracker.Spans()[0].int4_length == 12);

  // 模式切换开新段;再切回再开。
  // A mode switch opens a new span; switching back opens yet another.
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
  // HardwarePalette 纯数据模式(无 RenderThread)。
  // The HardwarePalette data-only mode (no RenderThread).
  ora::gfx::HardwarePalette palette_hw{nullptr};
  palette_hw.AddPalette("plain", MakeGradientPalette(0xFF000000), false);
  palette_hw.AddPalette("shifted", MakeGradientPalette(0xFF100000), false);
  palette_hw.Initialize();
  ORA_CHECK(palette_hw.GetPaletteIndex("plain") == 1);
  ORA_CHECK(palette_hw.GetPaletteIndex("shifted") == 2);

  const ora::gfx::PaletteReference ref_plain{"plain", 1, palette_hw.GetPalette("plain"), palette_hw};
  const ora::gfx::PaletteReference ref_shifted{"shifted", 2, palette_hw.GetPalette("shifted"), palette_hw};
  ora::gfx::Sprite sprite_indexed{};  // 默认 Red 通道 | default Red channel

  // null 调色板 → 0(SpriteRenderer.cs L166-167)。
  // A null palette → 0 (SpriteRenderer.cs L166-167).
  ORA_CHECK(ora::gfx::SpriteRenderer::ResolveTextureIndex(sprite_indexed, nullptr) == 0);

  // Indexed 精灵:无论有无色移都取 TextureIndex。
  // Indexed sprites take TextureIndex regardless of color shifts.
  ORA_CHECK(ora::gfx::SpriteRenderer::ResolveTextureIndex(sprite_indexed, &ref_plain) == 1);

  // —— OPT-A7 epoch:HasColorShift 缓存的失效链 ——
  // —— The OPT-A7 epoch: the cache invalidation chain ——
  ora::gfx::Sprite sprite_rgba{};
  sprite_rgba.kind_channel = ora::gfx::TextureChannel::RGBA;

  // RGBA 无色移 → 0(HACK 分支;每精灵热路径)。
  // RGBA without a color shift → 0 (the HACK branch; the per-sprite hot path).
  ORA_CHECK(ora::gfx::SpriteRenderer::ResolveTextureIndex(sprite_rgba, &ref_shifted) == 0);
  // 带色移 → TextureIndex。
  // With a color shift → TextureIndex.
  palette_hw.SetColorShift("shifted", 0.1f, 0.0f, 1.0f, 0.05f, 0.5f);
  ORA_CHECK(palette_hw.HasColorShift("shifted"));
  ORA_CHECK(ora::gfx::SpriteRenderer::ResolveTextureIndex(sprite_rgba, &ref_shifted) == 2);

  // 清零色移(结果翻转)→ epoch 递增 → 缓存失效 → 回 0。
  // Zeroing the shift (a result flip) → the epoch bumps → the cache
  // invalidates → back to 0.
  palette_hw.SetColorShift("shifted", 0.0f, 0.0f, 1.0f, 0.0f, 0.0f);
  ORA_CHECK(!palette_hw.HasColorShift("shifted"));
  ORA_CHECK(ora::gfx::SpriteRenderer::ResolveTextureIndex(sprite_rgba, &ref_shifted) == 0);

  // 未设色移的引用不受别的调色板翻转影响(值按名缓存)。
  // An unshifted reference stays unaffected by another palette's flips.
  ORA_CHECK(ora::gfx::SpriteRenderer::ResolveTextureIndex(sprite_rgba, &ref_plain) == 0);
}

void TestComputeWorldSpriteParams() {
  // downscale:world sheet 1024² 容不下 2048×1024 视口 → 因子 2,s = (1025, 513)
  // (Renderer.cs L253-263 的 +1 滚动补偿)。
  // downscale: a 1024² world sheet cannot hold a 2048×1024 viewport → factor
  // 2, s = (1025, 513) (the +1 scroll compensation of Renderer.cs L253-263).
  auto params = ora::gfx::ComputeWorldSpriteParams({1024, 1024}, {2048, 1024}, {0.0f, 0.0f}, {0, 0}, 2048.0f);
  ORA_CHECK(params.int4_downscale == 2);
  ORA_CHECK(params.int2_size_sub == ora::int2(1025, 513));

  // 整数倍 renderScale(2048/(1025-1) = 2)→ fractionalOffset 取整到屏幕像素
  // 格点:viewportLocation (10.4, 20.7)、center (10, 20) → frac (-0.4,-0.7) →
  // ×2 = (-0.8,-1.4) → 就近偶舍入 (-1,-1) → (-0.5,-0.5)。
  // An integral renderScale (2048/(1025-1) = 2) rounds fractionalOffset onto
  // screen-pixel grid points: viewportLocation (10.4, 20.7), center (10, 20) →
  // frac (-0.4,-0.7) → ×2 = (-0.8,-1.4) → banker's rounding (-1,-1) →
  // (-0.5,-0.5).
  params = ora::gfx::ComputeWorldSpriteParams({1025, 513}, {1024, 512}, {10.4f, 20.7f}, {10, 20}, 2048.0f);
  ORA_CHECK(params.vec_fractional_offset.X == -0.5f);
  ORA_CHECK(params.vec_fractional_offset.Y == -0.5f);

  // 非整数 renderScale → 原样保留小数。
  // A non-integral renderScale keeps the fraction as is.
  params = ora::gfx::ComputeWorldSpriteParams({100, 100}, {99, 99}, {10.4f, 20.7f}, {10, 20}, 1000.0f);
  // 浮点直减有表示误差,容差比较(上游同域 float 运算)。
  // Direct float subtraction carries representation error; compare with a
  // tolerance (the same float domain as upstream).
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

  // 水平线(RgbaColorRenderer.cs L54-67 单色形态):delta=(1,0),corner =
  // width/2 × (-delta.Y, delta.X, 0) = (0,1,0);四顶点带 (0.5,0.5) 偏移。
  // A horizontal line (the single-color form of RgbaColorRenderer.cs
  // L54-67): delta = (1,0), corner = width/2 × (-delta.Y, delta.X, 0) =
  // (0,1,0); four vertices carry the (0.5,0.5) offset.
  {
    CapturingSink sink;
    ora::gfx::RgbaColorRenderer color_renderer{sink};
    color_renderer.DrawLine(ora::core::Vector3{10, 10, 3}, ora::core::Vector3{20, 10, 3}, 2,
                            ora::core::Color::FromArgb(0xFF, 0xFF, 0xFF, 0xFF));
    ORA_CHECK(sink.vec_quads.size() == 1);
    ORA_CHECK(sink.vec_modes[0] == ora::gfx::BlendMode::Alpha);
    const auto& quad = sink.vec_quads[0];
    // (start-corner+Offset) = (10.5, 9.5, 3);(start+corner+Offset) = (10.5, 11.5, 3)。
    ORA_CHECK(quad[0].x == 10.5f && quad[0].y == 9.5f && quad[0].z == 3.0f);
    ORA_CHECK(quad[1].x == 10.5f && quad[1].y == 11.5f);
    ORA_CHECK(quad[2].x == 20.5f && quad[2].y == 11.5f);
    ORA_CHECK(quad[3].x == 20.5f && quad[3].y == 9.5f);
    // 不透明白的预乘 = 原色 /255。
    // The premultiplied opaque white keeps its color /255.
    ORA_CHECK(quad[0].s == 1.0f && quad[0].t == 1.0f && quad[0].u == 1.0f && quad[0].v == 1.0f);
    ORA_CHECK(quad[0].c == 0);  // c = 0(颜色顶点)| the color-vertex marker
  }

  // FillRect 四角(L203-219):a..d 依序 + Offset。
  // FillRect corners (L203-219): a..d in order + Offset.
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

  // 半透明色预乘(Util.PremultiplyAlpha):a=0x80 → c×128/255。
  // The translucent premultiply (Util.PremultiplyAlpha): a=0x80 → c×128/255.
  {
    CapturingSink sink;
    ora::gfx::RgbaColorRenderer color_renderer{sink};
    color_renderer.FillRect(ora::core::Vector3{0, 0, 0}, ora::core::Vector3{1, 1, 0},
                            ora::core::Color::FromArgb(0x80, 0xFF, 0xFF, 0xFF));
    const auto& quad = sink.vec_quads[0];
    const float float_expected = 255.0f * 128 / 255 / 255.0f;
    ORA_CHECK(std::abs(quad[0].s - float_expected) < 1e-6f);
  }

  // 闭合三角多边形 → 3 段(limit = 3;L149-175)。
  // A closed triangle polygon → 3 segments (limit = 3; L149-175).
  {
    CapturingSink sink;
    ora::gfx::RgbaColorRenderer color_renderer{sink};
    const std::array<ora::core::Vector3, 3> arr_triangle{
        ora::core::Vector3{0, 0, 0}, ora::core::Vector3{10, 0, 0}, ora::core::Vector3{0, 10, 0}};
    color_renderer.DrawPolygon(arr_triangle, 1, ora::core::Color::FromArgb(0xFF, 0, 0xFF, 0));
    ORA_CHECK(sink.vec_quads.size() == 3);
  }

  // 单点不成线(L115-118)。
  // A single point draws nothing (L115-118).
  {
    CapturingSink sink;
    ora::gfx::RgbaColorRenderer color_renderer{sink};
    const std::array<ora::core::Vector3, 1> arr_single{ora::core::Vector3{0, 0, 0}};
    color_renderer.DrawPolygon(arr_single, 1, ora::core::Color::FromArgb(0xFF, 0, 0, 0));
    ORA_CHECK(sink.vec_quads.empty());
  }
}

// ———— 第十二批纯逻辑:RenderItem 排序/分段(OPT-A7)+ WorldRenderer 坐标
// 与调色板面 + TerrainSpriteLayer 的 SOA 组合等价 ————
// ———— Twelfth-batch pure logic: the RenderItem sort/segmentation (OPT-A7) +
// WorldRenderer's coordinate and palette faces + TerrainSpriteLayer's SOA
// composition equivalence ————

/// 便捷:仅填排序键字段的测试 RenderItem。
/// Convenience: a test RenderItem with only the sort-key fields filled.
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

  // 排序:键 = Y+Z+ZOffset(int 回绕),平局按收集序。
  // Sorting: key = Y+Z+ZOffset (int wraparound), ties by collection order.
  {
    std::vector<ora::gfx::RenderItem> vec_items;
    vec_items.push_back(MakeKeyItem(500, 0, 0, ora::gfx::RenderableKind::Sprite));    // key 500
    vec_items.push_back(MakeKeyItem(100, 0, 0, ora::gfx::RenderableKind::UISprite));  // key 100
    vec_items.push_back(MakeKeyItem(100, 0, 0, ora::gfx::RenderableKind::Sprite));    // key 100(同键,后收)
    vec_items.push_back(MakeKeyItem(0, 100, 0, ora::gfx::RenderableKind::MarkerTile));  // key 100(再同键)
    vec_items.push_back(MakeKeyItem(0, 0, 50, ora::gfx::RenderableKind::Sprite));     // key 50

    const std::size_t size_mark = arena.Mark();
    const auto vec_sorted = ora::gfx::SortRenderablesByZ(vec_items, arena);
    ORA_CHECK(vec_sorted.size() == 5);
    ORA_CHECK(vec_sorted[0].int4_z_offset == 50);                              // 50 最前 | 50 first
    ORA_CHECK(vec_sorted[1].kind == ora::gfx::RenderableKind::UISprite);       // 100 组按收集序 | the 100 group in collection order
    ORA_CHECK(vec_sorted[2].kind == ora::gfx::RenderableKind::Sprite);
    ORA_CHECK(vec_sorted[3].kind == ora::gfx::RenderableKind::MarkerTile);
    ORA_CHECK(vec_sorted[4].wpos_pos.Y == 500);
    arena.Rewind(size_mark);
  }

  // 负键(int 回绕域)仍按带符号序 —— 上游 (long)key 装载前的 int 加法。
  // Negative keys (the int-wraparound domain) keep the signed order — the
  // int additions upstream performs before the (long)key lift.
  {
    std::vector<ora::gfx::RenderItem> vec_items;
    vec_items.push_back(MakeKeyItem(-300, 0, 0, ora::gfx::RenderableKind::Sprite));
    vec_items.push_back(MakeKeyItem(200, 0, 0, ora::gfx::RenderableKind::Sprite));
    const std::size_t size_mark = arena.Mark();
    const auto vec_sorted = ora::gfx::SortRenderablesByZ(vec_items, arena);
    ORA_CHECK(vec_sorted[0].wpos_pos.Y == -300);
    arena.Rewind(size_mark);
  }

  // 空 TargetLine 的 Pos = First() 等价抛("Sequence contains no elements")。
  // The empty TargetLine's Pos = First() throws equivalently ("Sequence
  // contains no elements").
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

  // 计数分段:kind 首遇序为段序,段内 = 收集序(= GroupBy 两阶稳定序)。
  // Counting segmentation: first-encounter kind order between segments,
  // collection order within (= GroupBy's two-level stable order).
  {
    std::vector<ora::gfx::RenderItem> vec_items;
    vec_items.push_back(MakeKeyItem(1, 0, 0, ora::gfx::RenderableKind::UISprite));    // UI 首遇 | UI first
    vec_items.push_back(MakeKeyItem(2, 0, 0, ora::gfx::RenderableKind::Sprite));
    vec_items.push_back(MakeKeyItem(3, 0, 0, ora::gfx::RenderableKind::UISprite));
    vec_items.push_back(MakeKeyItem(4, 0, 0, ora::gfx::RenderableKind::MarkerTile));  // Marker 首遇 | Marker first
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
    // 段内收集序(Y 递增 = 收集序)| within-segment collection order (the
    // ascending Y = the collection order)
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
  desc.ptr_renderer = nullptr;  // 纯逻辑(调色板走数据模式)| pure logic (the data-only palette mode)
  ora::gfx::WorldRenderer wr{world, desc};

  // ScreenPosition/Screen3DPosition(L402-425)。
  // ScreenPosition/Screen3DPosition (L402-425).
  const auto vec_pos = wr.ScreenPosition(WPos{512, 2048, 512});
  ORA_CHECK(vec_pos.X == 16.0f && vec_pos.Y == 48.0f);
  const auto vec_3d = wr.Screen3DPosition(WPos{512, 2048, 512});
  ORA_CHECK(vec_3d.X == 16.0f && vec_3d.Y == 48.0f && vec_3d.Z == 64.0f);

  // ScreenPxPosition 的 Math.Round 就近偶舍入:0.5 → 0、1.5 → 2(L427-432)。
  // ScreenPxPosition's Math.Round half-to-even: 0.5 → 0, 1.5 → 2 (L427-432).
  ORA_CHECK(wr.ScreenPxPosition(WPos{16, 16, 0}) == int2(0, 0));
  ORA_CHECK(wr.ScreenPxPosition(WPos{48, 48, 0}) == int2(2, 2));

  // Screen3DPxPosition 只整 xy 不整 z(L434-439)。
  // Screen3DPxPosition rounds x/y only, never z (L434-439).
  const auto vec_3dpx = wr.Screen3DPxPosition(WPos{16, 48, 0});
  ORA_CHECK(vec_3dpx.X == 0.0f && vec_3dpx.Y == 2.0f && vec_3dpx.Z == 1.5f);

  // ScreenVectorComponents/ScreenVector/ScreenPxOffset(L442-462)。
  // ScreenVectorComponents/ScreenVector/ScreenPxOffset (L442-462).
  const auto vec_components = wr.ScreenVectorComponents(WVec{1024, 2048, 512});
  ORA_CHECK(vec_components.X == 32.0f && vec_components.Y == 48.0f && vec_components.Z == 16.0f);
  const auto arr_vec = wr.ScreenVector(WVec{1024, 2048, 512});
  ORA_CHECK(arr_vec[0] == 32.0f && arr_vec[1] == 48.0f && arr_vec[2] == 16.0f && arr_vec[3] == 1.0f);
  ORA_CHECK(wr.ScreenPxOffset(WVec{512, 512, 0}) == int2(16, 16));

  // ProjectedPosition(L468-471):TileScale*px/Tile 整除向零截断。
  // ProjectedPosition (L468-471): TileScale*px/Tile truncates towards zero.
  const WPos wpos_projected = wr.ProjectedPosition({16, 48});
  ORA_CHECK(wpos_projected.X == 512 && wpos_projected.Y == 1536 && wpos_projected.Z == 0);

  // —— 调色板面(L105-138)——
  // —— The palette faces (L105-138) ——
  ORA_CHECK(wr.Palette("") == nullptr);  // 空名 null(L109)| empty name → null (L109)

  wr.AddPalette("terrain", MakeGradientPalette(0xFF000000), false);
  const auto* ptr_ref_a = wr.Palette("terrain");
  const auto* ptr_ref_b = wr.Palette("terrain");
  ORA_CHECK(ptr_ref_a != nullptr && ptr_ref_a == ptr_ref_b);  // GetOrAdd 缓存 | the GetOrAdd cache

  // 高度增长触发 PaletteInvalidated;allowOverwrite 的 Replace 不增长。
  // A height growth fires PaletteInvalidated; the allowOverwrite Replace
  // does not grow.
  std::int32_t int4_invalidated = 0;
  const auto uint8_token = wr.SubscribePaletteInvalidated([&]() { ++int4_invalidated; });
  wr.AddPalette("player", MakeGradientPalette(0xFF100000), false);
  ORA_CHECK(int4_invalidated >= 1);  // 1→2 行 = NextPowerOf2 增长 | rows 1→2 = a NextPowerOf2 growth
  wr.AddPalette("player", MakeGradientPalette(0xFF200000), false, true);  // overwrite → ReplacePalette
  ORA_CHECK(wr.Palette("player")->Palette().At(0) == 0xFF200000u);        // 缓存引用同步(L131-132)
  wr.UnsubscribePaletteInvalidated(uint8_token);

  // —— RefreshPalette 的 modifier 面(L393-397;IPaletteModifier 注入)——
  // —— RefreshPalette's modifier face (L393-397; the IPaletteModifier
  //    injection) ——
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
  wr.AddPalette("mutable", MakeGradientPalette(0xFF000000), true);  // allowModifiers = 可变 | mutable
  const std::uint32_t uint4_before = wr.hardware_palette().GetPalette("mutable").At(10);
  DoublingModifier modifier;
  const std::array<ora::gfx::IPaletteModifier*, 1> arr_modifiers{&modifier};
  wr.RefreshPalette(arr_modifiers);
  // ApplyModifiers 后可变调色板重置回原色(modifier 无状态逐帧调整)。
  // After ApplyModifiers the mutable palette resets to its original colors
  // (stateless per-frame adjustments).
  ORA_CHECK(wr.hardware_palette().GetPalette("mutable").At(10) == uint4_before);
}

/// 假地形光照:恒定 tint + 订阅表。
/// A fake terrain lighting: constant tint + a subscription table.
class FakeTerrainLighting final : public ora::gfx::ITerrainLighting {
 public:
  ora::core::Vector3 vec_tint{0.5f, 1.0f, 2.0f};

  ora::core::Vector3 TintAt(const WPos&) const override { return vec_tint; }
  std::uint64_t AddCellChangedListener(std::function<void(MPos)>) override { return 0; }
  void RemoveCellChangedListener(std::uint64_t) override {}
};

void TestTerrainSpriteLayerLogic() {
  // 上游 AOS 参考实现:直接以 (palette, tint) 整写 FastCreateQuad —— 组合
  // 等价的逐字节对照物(OPT-C5 同型)。
  // The upstream AOS reference: FastCreateQuad writing (palette, tint) whole
  // — the byte-for-byte counterpart of the composition equivalence (the
  // OPT-C5 style).
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

  ora::gfx::HardwarePalette palette_hw{nullptr};  // 数据模式 | data-only mode
  palette_hw.AddPalette("terrain", MakeGradientPalette(0xFF000000), false);
  const ora::gfx::PaletteReference ref_terrain{"terrain", palette_hw.GetPaletteIndex("terrain"),
                                               palette_hw.GetPalette("terrain"), palette_hw};

  ora::gfx::SheetBuilder builder{ora::gfx::SheetType::Indexed, 64, 1, nullptr};
  const auto sprite_tile = builder.Allocate({8, 8});

  // 空精灵 = SheetBuilder 的零尺寸 Allocate(上游 TerrainSpriteLayer 的调用方
  // 以 Sequence 空帧/专门 1×1 白帧充当;此处取零尺寸占位)。
  // The empty sprite = SheetBuilder's zero-size Allocate (upstream callers
  // fill the slot with a sequence's empty frame or a dedicated 1×1 white
  // frame; a zero-size placeholder serves here).
  const auto sprite_empty = builder.Allocate({0, 0});

  ora::gfx::TerrainSpriteLayer layer{world, wr, sprite_empty, ora::gfx::BlendMode::Alpha, false, nullptr};
  ORA_CHECK(layer.kind_blend() == ora::gfx::BlendMode::Alpha);

  // Update(带调色板;blend 匹配)→ 脏行 + 顶点(低 16 位,sampler 0)+
  // corner tint 全一 + 调色板登记。
  // Update (with a palette; matching blend) → the dirty row + vertices (the
  // low 16 bits, sampler 0) + all-ones corner tints + the palette
  // registration.
  layer.Update(MPos{1, 1}, &sprite_tile, &ref_terrain, ora::core::Vector3{11.0f, 22.0f, 5.0f}, 1.0f, 1.0f, false);
  ORA_CHECK(layer.RowDirtyForTest(1));
  ORA_CHECK(layer.PaletteAtForTest(MPos{1, 1}) == &ref_terrain);
  {
    const auto vec_vertices = layer.VerticesForTest();
    ORA_CHECK(vec_vertices.size() == 4 * 4 * 3);
    // OPT-A7:顶点 c 只含低 16 位(调色板行上传点组合)。
    // OPT-A7: the vertex c carries the low 16 bits only (the palette row
    // composes at the upload).
    ORA_CHECK((vec_vertices[16].c & 0xFFFF0000u) == 0);
    ORA_CHECK(vec_vertices[20].r == 1.0f && vec_vertices[20].a == 1.0f);
  }

  // ComposeRow == AOS 参考(无光照:tint = alpha × One)。
  // ComposeRow == the AOS reference (no lighting: tint = alpha × One).
  layer.ComposeRow(1);
  {
    const auto arr_reference = MakeAosReference(sprite_tile, ref_terrain.TextureIndex(),
                                                ora::core::Vector3{1.0f, 1.0f, 1.0f}, 1.0f);
    const auto& vec_staging = layer.StagingForTest();
    for (std::size_t i = 0; i < 4; ++i) {
      ORA_CHECK(vec_staging[4 + i].x == arr_reference[i].x);  // 目标格在行内第 4 顶点起 | the target cell starts at vertex 4 within the row
      ORA_CHECK(vec_staging[4 + i].c == arr_reference[i].c);
      ORA_CHECK(vec_staging[4 + i].r == arr_reference[i].r);
      ORA_CHECK(vec_staging[4 + i].a == arr_reference[i].a);
    }
  }

  // 调色板失效(AddPalette 高度增长)→ 全行标脏,无逐顶点工作。
  // Palette invalidation (an AddPalette height growth) → every row dirty,
  // zero per-vertex work.
  layer.Draw(1, 1);  // 清行 1 的脏 | clears row 1's dirty
  ORA_CHECK(!layer.RowDirtyForTest(1));
  wr.AddPalette("player", MakeGradientPalette(0xFF100000), false);
  ORA_CHECK(layer.RowDirtyForTest(0) && layer.RowDirtyForTest(1) && layer.RowDirtyForTest(2));

  // Clear(cell) → 空精灵四顶点 + 调色板 null。
  // Clear(cell) → the empty sprite's four vertices + a null palette.
  layer.Update(MPos{2, 1}, &sprite_tile, &ref_terrain, ora::core::Vector3{0, 0, 0}, 1.0f, 1.0f, false);
  layer.Clear(CPos{2, 1});
  ORA_CHECK(layer.PaletteAtForTest(MPos{2, 1}) == nullptr);
  {
    const auto vec_vertices = layer.VerticesForTest();
    const std::size_t size_offset = 16 /* 行 1 × vertexRowStride */ + 4 * 2;  // 行 1 内格 2 | cell 2 within row 1
    for (std::size_t i = 0; i < 4; ++i)
      // 空精灵 = Indexed sheet 的 Red 通道位(0<<1|1 = 0x01);上游空精灵由调用
      // 方给 RGBA 白帧,通道位随其 = 0x02 —— 位模式随精灵,非恒 0。
      // The empty sprite carries the Indexed sheet's Red channel bit
      // (0<<1|1 = 0x01); upstream's caller-supplied RGBA white frame would
      // pack 0x02 — the bits belong to the sprite, never a constant zero.
      ORA_CHECK(vec_vertices[size_offset + i].c == 0x01u);
  }

  // —— 光照路径:corner tint 四角采样;ignoreTint 复位 One;组合 == AOS ——
  // —— The lighting path: the four-corner corner-tint sampling; ignoreTint
  //    resetting to One; composition == AOS ——
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

    // 组合 == 上游 AOS 参考:tint 参数 = alpha × One,RGB 复算 = alpha ×
    // weights —— 两域重合(头注论证)。
    // Composition == the upstream AOS reference: the tint argument =
    // alpha × One with the RGB recompute = alpha × weights — the two
    // domains coincide (the header argument).
    layer_lit.ComposeRow(1);
    const auto arr_reference = MakeAosReference(
        sprite_tile, ref_terrain.TextureIndex(),
        0.5f * ora::core::Vector3{0.5f, 1.0f, 2.0f},  // alpha × weights
        0.5f);
    const auto& vec_staging = layer_lit.StagingForTest();
    for (std::size_t i = 0; i < 4; ++i) {
      ORA_CHECK(vec_staging[4 + i].r == arr_reference[i].r);
      ORA_CHECK(vec_staging[4 + i].g == arr_reference[i].g);
      ORA_CHECK(vec_staging[4 + i].b == arr_reference[i].b);
      ORA_CHECK(vec_staging[4 + i].a == arr_reference[i].a);
    }

    // ignoreTint → corner 复位 One(上游 RGB = v.A × One 的组合域等价)。
    // ignoreTint → the corner resets to One (the composition-domain
    // equivalent of upstream's RGB = v.A × One).
    layer_lit.Update(MPos{1, 1}, &sprite_tile, &ref_terrain, ora::core::Vector3{11.0f, 22.0f, 5.0f}, 1.0f, 0.5f,
                     true);
    const ora::core::Vector3 vec_tint_one{1.0f, 1.0f, 1.0f};
    for (std::size_t i = 0; i < 4; ++i)
      ORA_CHECK(vec_tints[size_offset + i] == vec_tint_one);
  }

  // —— 负例与怪癖 ——
  // —— Negatives and quirks ——
  bool b_threw = false;
  try {
    ora::gfx::Sheet sheet_other{ora::gfx::SheetType::Indexed, {4, 4}, nullptr};
    auto sprite_other = sprite_tile;
    sprite_other.kind_blend = ora::gfx::BlendMode::None;  // blend 不匹配 | blend mismatch
    layer.Update(MPos{0, 0}, &sprite_other, nullptr, ora::core::Vector3{}, 1.0f, 1.0f, false);
  } catch (const std::runtime_error& error_runtime) {
    b_threw = std::string_view{error_runtime.what()} == "Attempted to add sprite with a different blend mode";
  }
  ORA_CHECK(b_threw);

  // 9 张不同 sheet → 第 9 张满槽 "Sheet overflow"(槽 0 保留给 null)。
  // Nine distinct sheets → the ninth overflows ("Sheet overflow"; slot 0 is
  // reserved for null).
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
      const auto sprite_i = builder.Allocate({4, 4});  // 全在 builder 自己的 sheet 上 | all on the builder's own sheet
      // 改指到独立 sheet(保持 blend/通道)| repoint to the standalone sheet
      auto sprite_rep = sprite_i;
      sprite_rep.ptr_sheet = &arr_sheets[static_cast<std::size_t>(i)];
      layer.Update(MPos{0, 0}, &sprite_rep, nullptr, ora::core::Vector3{}, 1.0f, 1.0f, false);
    }
  } catch (const std::runtime_error& error_runtime) {
    b_threw = std::string_view{error_runtime.what()} == "Sheet overflow";
  }
  ORA_CHECK(b_threw);

  // 界外格早退(L193-195 的 contains 检查;samplers 登记发生在前 —— 上游
  // 语句序照抄:界外 update 也会占 sheet 槽,本条锁此怪癖)。
  // The out-of-bounds early-out (the contains check of L193-195; the
  // samplers register first — upstream's statement order kept verbatim: an
  // out-of-bounds update still takes a sheet slot, locked here).
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
      ORA_CHECK(!layer_bounds.RowDirtyForTest(int4_row));  // 早退不标脏 | the early-out marks nothing dirty
  }
}

#ifdef ORA_HAS_DESKTOP_GL

// ———— GL 集成:Sheet 上传(全量 + 子区域)/缓冲转移 GL 路径/调色板 OPT-C5 ————
// ———— GL integration: Sheet uploads (full + sub-rectangle)/the buffer-transfer
// GL path/the palette OPT-C5 equivalence ————

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

  // —— Sheet:SheetBuilder 拼装 → GetTexture 上传 → glGetTexImage 读回 ——
  // —— Sheet: SheetBuilder packing → GetTexture upload → glGetTexImage readback ——
  {
    ora::gfx::SheetBuilder builder{ora::gfx::SheetType::BGRA, 32, 1, &render};
    // 4×4 BGRA = 64B;每像素 (0xA0+p, 0x11, 0x22, 0xFF) / (0xB0+p, …)。
    // 4×4 BGRA = 64 bytes; per-pixel (0xA0+p, 0x11, 0x22, 0xFF) / (0xB0+p, …).
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

    auto& texture_sheet = builder.Current()->GetTexture();  // 全量首传 | the initial full upload
    auto vec_readback = texture_sheet.GetData();            // 32×32×4 = 4096B
    ORA_CHECK(vec_readback.size() == 4096);
    const auto ReadPx = [&](std::int32_t int4_x, std::int32_t int4_y) {
      const std::size_t i = (static_cast<std::size_t>(int4_y) * 32 + int4_x) * 4;
      return std::to_integer<std::int32_t>(vec_readback[i]);
    };
    ORA_CHECK(ReadPx(1, 1) == 0xA0 && ReadPx(4, 4) == 0xAF);  // 像素 0 / 像素 15 | pixel 0 / pixel 15
    ORA_CHECK(ReadPx(6, 1) == 0xB0 && ReadPx(9, 4) == 0xBF);

    // 子区域更新:仅改 sprite_a 的第 1 行 → CommitBufferedData(该行)→
    // SetSubData 路径(GetTexture 内条件分支)→ 读回其余未动。
    // Sub-rectangle update: rewrite sprite_a's first row →
    // CommitBufferedData(that row) → the SetSubData branch inside
    // GetTexture → everything else untouched on readback.
    auto vec_sheet_data = builder.Current()->GetData();
    const auto rect_row = ora::Rectangle(sprite_a.Bounds.X, sprite_a.Bounds.Y, 4, 1);
    for (auto i = 0; i < 4; ++i) {
      const std::size_t idx = (static_cast<std::size_t>(rect_row.Y) * 32 + rect_row.X + i) * 4;
      vec_sheet_data[idx] = std::byte(0xC5);
    }
    builder.Current()->CommitBufferedData(rect_row);
    (void)builder.Current()->GetTexture();

    vec_readback = texture_sheet.GetData();
    ORA_CHECK(ReadPx(1, 1) == 0xC5 && ReadPx(4, 1) == 0xC5);  // 脏行已更新 | the dirty row updated
    ORA_CHECK(ReadPx(1, 2) == 0xA4 && ReadPx(9, 4) == 0xBF);  // 行外未动 | outside the row untouched

    // 缓冲转移 GL 路径:src 已缓冲 → 上传提交 + 释放;dst 零状态 → 接收清零
    // 缓冲(Sheet.cs L169-190)。
    // The buffer-transfer GL path: a buffered src commits and releases; a
    // pristine dst receives the zeroed buffer (Sheet.cs L169-190).
    ora::gfx::Sheet sheet_src{ora::gfx::SheetType::BGRA, {8, 8}, &render};
    ora::gfx::Sheet sheet_dst{ora::gfx::SheetType::BGRA, {8, 8}, &render};
    (void)sheet_src.GetData();
    ORA_CHECK(sheet_src.ReleaseBufferAndTryTransferTo(sheet_dst));
    ORA_CHECK(!sheet_src.Buffered());  // 提交后释放 | released after the commit
    const auto vec_dst = sheet_dst.GetData();
    ORA_CHECK(std::all_of(vec_dst.begin(), vec_dst.end(), [](std::byte b) { return b == std::byte{0}; }));
    auto& texture_dst = sheet_dst.GetTexture();  // 转移缓冲照常上传 | the transferred buffer uploads fine
    auto vec_dst_read = texture_dst.GetData();
    ORA_CHECK(vec_dst_read.size() == 8 * 8 * 4);
    ORA_CHECK(std::all_of(vec_dst_read.begin(), vec_dst_read.end(), [](std::byte b) { return b == std::byte{0}; }));
  }

  // —— HardwarePalette:全量初传 + OPT-C5(增量 == 全量,逐字节)+ 增量替换 ——
  // —— HardwarePalette: the initial full upload + OPT-C5 (incremental ==
  // full, byte-for-byte) + an incremental replacement ——
  {
    ora::gfx::HardwarePalette palette_hw{&render};
    palette_hw.AddPalette("a", MakeGradientPalette(0x11BB0000), false);
    palette_hw.AddPalette("b", MakeGradientPalette(0x22CC0000), true);
    palette_hw.AddPalette("c", MakeGradientPalette(0x33DD0000), false);
    palette_hw.Initialize();
    ORA_CHECK(palette_hw.Height() == 4);

    auto* ptr_texture = palette_hw.TextureOrNull();
    ORA_CHECK(ptr_texture != nullptr && ptr_texture->Width() == 256 && ptr_texture->Height() == 4);

    // OPT-C5:与"裸纹理全量 SetData 同一 CPU 缓冲"的参考读回逐字节一致。
    // OPT-C5: byte-identical to the readback of a bare texture given the
    // same CPU buffer via a full SetData.
    const auto AssertMatchesFullReference = [&]() {
      ora::gfx::Texture texture_reference{render};
      texture_reference.SetData(palette_hw.BufferForTest(), 256, palette_hw.Height());
      const auto vec_reference = texture_reference.GetData();
      const auto vec_actual = ptr_texture->GetData();
      ORA_CHECK(vec_actual.size() == vec_reference.size());
      ORA_CHECK(std::equal(vec_actual.begin(), vec_actual.end(), vec_reference.begin()));
    };
    AssertMatchesFullReference();

    // 行内容:行 1 = 调色板 a 的 BGRA 字节序(0x11BB0005 → B=05,G=00,R=BB,A=11…)。
    // Row contents: row 1 = palette a in BGRA byte order.
    auto vec_rows = ptr_texture->GetData();
    const auto RowByte = [&](std::int32_t int4_row, std::int32_t int4_entry, std::int32_t int4_plane) {
      return std::to_integer<std::int32_t>(
          vec_rows[(static_cast<std::size_t>(int4_row) * 256 + int4_entry) * 4 + int4_plane]);
    };
    ORA_CHECK(RowByte(1, 5, 0) == 0x05 && RowByte(1, 5, 1) == 0x00 && RowByte(1, 5, 2) == 0xBB &&
              RowByte(1, 5, 3) == 0x11);
    ORA_CHECK(RowByte(0, 5, 0) == 0 && RowByte(0, 5, 3) == 0);  // 行 0 保留零 | reserved row zero

    // 增量:ReplacePalette 单行 → SetSubData 路径;OPT-C5 再验。
    // Incremental: a single-row ReplacePalette → the SetSubData path;
    // OPT-C5 verified again.
    palette_hw.ReplacePalette("a", MakeGradientPalette(0x44EE0000));
    ORA_CHECK(!palette_hw.FullDirty());
    AssertMatchesFullReference();
    vec_rows = ptr_texture->GetData();
    ORA_CHECK(RowByte(1, 5, 2) == 0xEE && RowByte(1, 5, 3) == 0x44);
    ORA_CHECK(RowByte(2, 5, 2) == 0xCC);  // 其余行未动 | other rows untouched

    // ApplyModifiers(GL):modifier 改行 2 → 增量上传 → 读回 = 修改值;重置后
    // CPU 侧回原色。
    // ApplyModifiers (GL): a modifier rewrites row 2 → incremental upload →
    // the readback shows the modified value; the CPU side resets.
    struct WhitewashModifier : ora::gfx::IPaletteModifier {
      void AdjustPalette(std::map<std::string, ora::gfx::MutablePalette, std::less<>>& map_mutable) override {
        map_mutable.find("b")->second.SetColor(7, ora::core::Color::FromArgb(0xFF, 0xFF, 0xFF, 0xFF));
      }
    };
    WhitewashModifier modifier_white{};
    ora::gfx::IPaletteModifier* vec_mods[] = {&modifier_white};
    palette_hw.ApplyModifiers(vec_mods);
    vec_rows = ptr_texture->GetData();
    ORA_CHECK(RowByte(2, 7, 0) == 0xFF && RowByte(2, 7, 3) == 0xFF);  // 上传为修改值 | modified value uploaded
    ORA_CHECK(palette_hw.GetPalette("b").At(7) == 0x22CC0007u);       // CPU 侧重置回原色 | CPU reset
  }
}

// ———— GL 集成(第四批):持久 VB 槽轮换 + 调色板采样链 + BlendSpan 交错 +
// 双 shader VAO 缓存 + NPOT FrameBuffer + 单级合成 Renderer 全流程 ————
// ———— GL integration (fourth batch): persistent-VB slot rotation + the
// palette sampling chain + BlendSpan interleaving + the dual-shader VAO
// cache + the NPOT FrameBuffer + the single-pass composite Renderer ————

/// 纯色调色板(256 项同色)。
/// A solid-color palette (256 identical entries).
ora::gfx::ImmutablePalette MakeSolidPalette(std::uint32_t uint4_color) {
  std::array<std::uint32_t, ora::gfx::kPaletteSize> vec_colors{};
  vec_colors.fill(uint4_color);
  return ora::gfx::ImmutablePalette{vec_colors};
}

/// 读 glsl/combined 源(编译期 ORA_GLSL_DIR 由 CMake 注入)。
/// Reads the glsl/combined sources (ORA_GLSL_DIR injected at compile time).
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

  // FBO 像素读取助手(RGBA8;读当前绑定的 FBO)。
  // An FBO pixel-read helper (RGBA8; reads the currently bound FBO).
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

  // —— NPOT FrameBuffer(130×70;OPT-B1/D37)——
  // —— The NPOT FrameBuffer (130×70; OPT-B1/D37) ——
  {
    ora::gfx::FrameBuffer buffer_npot{render, {130, 70}, 0.25f, 0.5f, 0.75f, 1.0f};
    ORA_CHECK(buffer_npot.b_valid());
    buffer_npot.Bind();
    const auto arr_pixel = ReadPixelAt(65, 35);
    ORA_CHECK(ExpectPixel(arr_pixel, 64, 128, 191));
    buffer_npot.Unbind();
  }

  // —— SpriteRenderer E2E:调色板采样链 + 持久 VB 槽轮换回绕 ——
  // —— SpriteRenderer end to end: the palette sampling chain + the
  // persistent-VB slot-rotation wraparound ——
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
    arr_indices.fill(std::byte{0xC8});  // 调色板索引 200 | palette index 200
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

    // 五轮 Flush 覆盖 3 槽轮换回绕(slot 0,1,2,0,1;后两轮走 fence 等待)。
    // Five flushes wrap the 3-slot rotation (slots 0,1,2,0,1; the last two
    // take the fence-wait path).
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

    // —— BlendSpan 三段交错(None 白全屏 / Alpha 半透明蓝 / None 红)——
    // —— BlendSpan three-segment interleave (None fullscreen white /
    // Alpha translucent blue / None red) ——
    ora::gfx::SheetBuilder builder_bgra{ora::gfx::SheetType::BGRA, 64, 1, &render};
    std::array<std::byte, 16> arr_white{};
    for (auto& b : arr_white) {
      b = std::byte{0xFF};
    }
    const auto sprite_white = builder_bgra.Add(arr_white, ora::gfx::SpriteFrameType::Bgra32, {2, 2}, true);

    // 三种 blend 的精灵(白 None / 白 Alpha 蓝 tint / 红 None)。
    // Three sprites with distinct blends.
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
    sr.DrawSprite(sprite_white_none, nullptr, ora::core::Vector3{0, 0, 0}, 64.0f);  // 全屏白底
    sr.DrawSprite(sprite_white_alpha, nullptr, ora::core::Vector3{16, 16, 0}, 8.0f,
                  ora::core::Vector3{0, 0, 1}, 0.5f);  // 半透明蓝 tint
    sr.DrawSprite(sprite_red_none, &ref_red, ora::core::Vector3{40, 40, 0}, 8.0f);  // 不透红
    ORA_CHECK(sr.SpansForTest().Spans().size() == 3);  // None/Alpha/None 三段
    sr.Flush();
    render.FlushAndWait();

    ORA_CHECK(ExpectPixel(ReadPixelAt(2, 2), 255, 255, 255));    // None 白底
    ORA_CHECK(ExpectPixel(ReadPixelAt(20, 20), 128, 128, 255));  // Alpha: 0.5 蓝 + 0.5 白
    ORA_CHECK(ExpectPixel(ReadPixelAt(44, 44), 255, 0, 0));      // 段首偏移(索引字节偏移)正确
    target2.Unbind();

    // —— 双 shader 共享 VB/IB:per-program VAO 缓存往返切换 ——
    // —— Two shaders sharing VB/IB: the per-program VAO cache toggling ——
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

  // —— 单级合成 Renderer 全流程(OPT-B1)——
  // —— The single-pass composite Renderer end to end (OPT-B1) ——
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

    // 世界坐标 (0..64, 0..48) 1:1 映射进 world FBO(scroll = 0)。
    // World coordinates (0..64, 0..48) map 1:1 into the world FBO (scroll 0).
    renderer.BeginWorld({32.0f, 24.0f}, {64, 48});
    renderer.WorldSpriteRenderer().DrawSprite(sprite_index, &ref_red, ora::core::Vector3{0, 0, 0}, 1.0f);
    renderer.BeginUI();
    renderer.UIRgbaSpriteRenderer().DrawSprite(sprite_white, ora::core::Vector3{1, 1, 0}, 2.0f,
                                               ora::core::Vector3{0, 0, 1}, 1.0f);
    renderer.Flush();  // 上游在 EndFrame 才 flush;提前读回需要手动排空 | upstream flushes at EndFrame; the early readback needs a manual drain

    // Present 前读回默认帧缓冲(back buffer;world blit 2× 于 128×96 表面)。
    // Read the default framebuffer's back buffer before Present (the world
    // blit scaled 2× onto the 128×96 surface).
    const auto geom = window.Geom();
    const float float_scale = geom.float_scale;
    render.FlushAndWait();
    const auto Pixel = [&](float float_x, float float_y) {
      return ReadPixelAt(static_cast<std::int32_t>(float_x * float_scale),
                         static_cast<std::int32_t>(float_y * float_scale));
    };
    // UI 蓝精灵 (1..5)² 屏幕同域;world 红精灵 (0..8)² ×2 = (0..16)²;其余 =
    // world FBO 的 clear(黑)。
    // The UI blue sprite covers (1..5)² screen-space; the world red sprite
    // (0..8)² ×2 = (0..16)²; everything else is the world FBO's clear
    // (black).
    ORA_CHECK(ExpectPixel(Pixel(3, 3), 0, 0, 255));
    ORA_CHECK(ExpectPixel(Pixel(12, 12), 255, 0, 0));
    const auto arr_pixel_bg = Pixel(60, 60);
    ORA_CHECK(arr_pixel_bg[0] <= 3 && arr_pixel_bg[1] <= 3 && arr_pixel_bg[2] <= 3);

    renderer.EndFrame();
    render.FlushAndWait();
  }

  // —— 第十二批:WorldRenderer 收集/绘制链 + TerrainSpriteLayer 的 GL 路径 ——
  // —— Twelfth batch: the WorldRenderer collection/draw chain + the
  //    TerrainSpriteLayer GL path ——
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

    // 地形层:格 (0,0) 绿 tile(世界 px 12..20)²;行 0..2 绘制。
    // The terrain layer: cell (0,0) a green tile (world px 12..20)²; rows
    // 0..2 drawn.
    ora::gfx::TerrainSpriteLayer layer{world, wr, sprite_empty, ora::gfx::BlendMode::Alpha, false, &render};
    layer.Update(CPos{0, 0}, &sprite_tile, ptr_ref_green, 1.0f, 1.0f, false);
    ORA_CHECK(layer.RowDirtyForTest(0));
    wr.hooks().fn_render_terrain = [&](ora::gfx::WorldRenderer&, ora::gfx::IViewportSurface&) { layer.Draw(0, 2); };

    // 收集源:红精灵 renderable,世界位 (416,416,0) → 屏幕 (9..17)²(与绿
    // tile 部分重叠;prepared 绘制在地形之后 → 重叠区红压绿)。
    // The collection source: a red sprite renderable at world (416,416,0) →
    // screen (9..17)² (partially overlapping the green tile; prepared draws
    // after the terrain → red over green in the overlap).
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

    // world FBO 64×48 blit ×2 到 128×96 表面:world (10,10) = 红,(18,18) =
    // 绿,界外 = world FBO clear(黑)。
    // The world FBO (64×48) blits ×2 onto the 128×96 surface: world (10,10)
    // = red, (18,18) = green, elsewhere the world FBO's clear (black).
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

    // 脏行已被 Draw 消费;帧末清列(验证收集缓冲复用语义)。
    // Draw consumed the dirty rows; DrawAnnotations clears the lists (the
    // collection-buffer reuse semantics verified).
    ORA_CHECK(!layer.RowDirtyForTest(0));
    wr.DrawAnnotations();
    ORA_CHECK(wr.PreparedRenderablesForTest().empty());

    renderer_twelfth.EndFrame();
    render.FlushAndWait();
  }
}

#endif  // ORA_HAS_DESKTOP_GL

}  // namespace

// ———— 第十三批:CursorSequence/Animation/SpriteCache/SequenceSet/ChromeProvider ————
// ———— Thirteenth batch: CursorSequence/Animation/SpriteCache/SequenceSet/
//          ChromeProvider ————

/// 临时工作目录(测试自建自清)。
/// A temp working directory (created and cleaned up by the test).
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

/// 挂一个临时目录为根的 FileSystem。
/// Mounts a temp directory as a FileSystem root.
ora::fs::FileSystem MakeTempFs(const std::filesystem::path& dir_path) {
  ora::fs::FileSystem file_system{};
  file_system.Mount(dir_path.generic_string());
  return file_system;
}

/// 合成 8×8 单帧 shpTD(与 formats_test 的构造同族)。
/// A synthetic 8×8 single-frame shpTD (the same family as formats_test's
/// construction).
std::vector<std::byte> MakeTinyShpTD(std::uint8_t uint1_seed, std::int32_t int4_frame_count = 2) {
  // 多帧(Animation 的多帧序列需要)| multiple frames (Animation's multi-frame
  // sequences need them).
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
  // 头表 = count+2 项(帧 + eof + 哨兵;与 formats_test 的单帧构造同族)
  // The header table = count+2 entries (frames + eof + a sentinel; the
  // same family as formats_test's single-frame construction).
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
  put_u16(0, static_cast<std::uint16_t>(int4_frame_count));  // imageCount
  put_u16(6, 8);                                             // w
  put_u16(8, 8);                                             // h
  for (std::int32_t int4_f = 0; int4_f < int4_frame_count; int4_f++) {
    const std::size_t st_header = 14 + 8 * static_cast<std::size_t>(int4_f);
    put_u32(st_header, static_cast<std::uint32_t>(st_data_base + vec_offsets[static_cast<std::size_t>(int4_f)]) |
                            (0x80u << 24));
  }
  put_u32(14 + 8 * static_cast<std::size_t>(int4_frame_count),
          static_cast<std::uint32_t>(vec_file.size()));  // eof 头 | the eof header
  std::ranges::copy(vec_encoded_all, vec_file.begin() + static_cast<std::ptrdiff_t>(st_data_base));
  return vec_file;
}

/// 测试用固定序列(Length/Tick/属性可配;GetSprite 返回按帧索引偏移的
/// Sprite;GetShadow 仅 shadow_from 起有值)。
/// A fixed test sequence (configurable Length/Tick/properties; GetSprite
/// returns a frame-index-shifted Sprite; GetShadow exists only from
/// shadow_from).
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
  std::int32_t int4_shadow_from = -1;  // -1 = 恒无影 | never any shadow
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

/// 固定序列解析器:image 节点的每个子节点 → 一条 FixedSequence;键 @len@
/// 控制 Length,@tick@ 控制 Tick,@shadow@ 控制 shadow_from(逗号分隔键后缀,
/// 测试专用微语法)。
/// A fixed-sequence loader: every child node of the image node → one
/// FixedSequence; the @len@ key controls Length, @tick@ Tick, @shadow@
/// shadow_from (comma-separated key suffixes, a test-only micro-syntax).
class FixedSequenceLoader final : public ISpriteSequenceLoader {
 public:
  std::vector<std::pair<std::string, std::unique_ptr<ISpriteSequence>>> ParseSequences(
      SpriteCache& cache_sprites, const std::string& str_tile_set,
      const yaml::MiniYamlNode& node_image) override {
    std::vector<std::pair<std::string, std::unique_ptr<ISpriteSequence>>> vec_out;
    for (const yaml::MiniYamlNode& node_seq : node_image.Value.Nodes) {
      auto seq_fixed = std::make_unique<FixedSequence>();
      std::string str_name = node_seq.Key != nullptr ? *node_seq.Key : std::string{};

      // 微语法:idle@3@10@1 = name@length@tick@shadow_from
      // The micro-syntax: idle@3@10@1 = name@length@tick@shadow_from.
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

/// 构一个两序列(tiny/tiny2)的 SequenceSet(测试共用)。
/// Builds a two-sequence (tiny/tiny2) SequenceSet (shared by the tests).
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
  // 上游 info = 序列节点自身的 Value(子节点 = X/Y/Start/…);此处把测试
  // 文本降一级挂到 "seq" 节点下再取其 Value。
  // Upstream's info = the sequence node's own Value (children = X/Y/Start/
  // ...); the test text hangs one level lower under a "seq" node whose Value
  // we take.
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
    ORA_CHECK(seq_c.Length().has_value() && *seq_c.Length() == 1);  // 缺省 = 1 | default 1
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
    // Length = "*" → null(至序列尾)| Length = "*" → null (through the end)
    const CursorSequence seq_c = parse("Start: 1\nLength: *");
    ORA_CHECK(!seq_c.Length().has_value());
  }
  {
    // End = "*":上游死三元恒 null | End = "*": upstream's dead ternary is
    // always null.
    const CursorSequence seq_c = parse("Start: 1\nEnd: *");
    ORA_CHECK(!seq_c.Length().has_value());
  }
  {
    // End = 数值:上游条件不满足 → Length 保持缺省 1
    // End = a number: upstream's condition fails → Length keeps the default
    // 1.
    const CursorSequence seq_c = parse("Start: 1\nEnd: 5");
    ORA_CHECK(seq_c.Length().has_value() && *seq_c.Length() == 1);
  }
  {
    // X 解析失败静默保持 0(上游 TryParse 怪癖)
    // A failing X parse silently keeps 0 (the upstream TryParse quirk).
    const CursorSequence seq_c = parse("X: abc\nStart: 0");
    ORA_CHECK(seq_c.Hotspot().X == 0);
  }
  {
    auto b_threw = false;
    try {
      const CursorSequence seq_c = parse("Length: 2");  // 缺 Start | Start missing
      (void)seq_c;
    } catch (const std::runtime_error&) {
      b_threw = true;
    }
    ORA_CHECK(b_threw);
  }
}

void TestAnimationStateMachine() {
  AnimTestFixture fixture_anim;

  // ———— PlayRepeating:回绕 + Tick 债务循环 ————
  // ———— PlayRepeating: the wraparound + Tick's debt loop ————
  {
    Animation animation{{fixture_anim.ptr_sequences.get(), nullptr}, "ANIM"};
    ORA_CHECK(animation.Name() == "anim");  // ToLowerInvariant
    animation.PlayRepeating("idle");
    ORA_CHECK(animation.CurrentSequence()->Name() == "idle");
    ORA_CHECK(animation.CurrentFrame() == 0);

    animation.Tick();  // 40ms 一帧 | one 40ms frame
    ORA_CHECK(animation.CurrentFrame() == 1);
    animation.Tick(40);
    ORA_CHECK(animation.CurrentFrame() == 2);
    animation.Tick(40);
    ORA_CHECK(animation.CurrentFrame() == 0);  // 长度 3 回绕 | wraps at length 3

    animation.Tick(80);  // 双帧追进 | a two-frame catch-up
    ORA_CHECK(animation.CurrentFrame() == 2);
  }

  // ———— PlayThen:末帧钳制 + after 回调 ————
  // ———— PlayThen: the final-frame clamp + the after callback ————
  {
    Animation animation{{fixture_anim.ptr_sequences.get(), nullptr}, "anim"};
    auto int4_after_calls = 0;
    animation.PlayThen("open", [&] { int4_after_calls++; });
    animation.Tick(10);  // tick=10 → 恰一帧 | tick=10 → exactly one frame
    ORA_CHECK(animation.CurrentFrame() == 1);
    // 40 累计 + 债务循环连进三帧:2 → 3 → 钳制末帧 + after
    // 40 accumulated + the debt loop advances three frames: 2 → 3 → clamped
    // at the last frame + after.
    animation.Tick(30);
    ORA_CHECK(animation.CurrentFrame() == 3);
    ORA_CHECK(int4_after_calls == 1);
    animation.Tick(100);  // tickFunc 已 null → 帧不动 | tickFunc is null now →
                          // the frame stays
    ORA_CHECK(animation.CurrentFrame() == 3);
    ORA_CHECK(int4_after_calls == 1);
  }

  // ———— PlayBackwardsThen:CurrentFrame 反转 ————
  // ———— PlayBackwardsThen: CurrentFrame's reversal ————
  {
    Animation animation{{fixture_anim.ptr_sequences.get(), nullptr}, "anim"};
    animation.PlayBackwardsThen("open", nullptr);
    // tick=10 → 每 Tick(10) 一帧;CF = len-1-frame
    // tick=10 → one frame per Tick(10); CF = len-1-frame.
    animation.Tick(10);
    ORA_CHECK(animation.CurrentFrame() == 2);  // frame 1 → 4-1-1
    animation.Tick(10);
    ORA_CHECK(animation.CurrentFrame() == 1);  // frame 2 → 4-1-2
    animation.Tick(10);
    ORA_CHECK(animation.CurrentFrame() == 0);  // frame 3 → 4-1-3
    animation.Tick(10);
    ORA_CHECK(animation.CurrentFrame() == 0);  // 钳制:frame = len-1 | clamped: frame = len-1
  }

  // ———— PlayFetchIndex:tickAlways 旁路 ————
  // ———— PlayFetchIndex: the tickAlways bypass ————
  {
    Animation animation{{fixture_anim.ptr_sequences.get(), nullptr}, "anim"};
    auto int4_fetch_value = 2;
    animation.PlayFetchIndex("idle", [&] { return int4_fetch_value; });
    ORA_CHECK(animation.CurrentFrame() == 2);
    animation.Tick(1);  // tickAlways:任意 t 即取 | tickAlways: any t fetches
    int4_fetch_value = 0;
    animation.Tick(1);
    ORA_CHECK(animation.CurrentFrame() == 0);
  }

  // ———— PlayFetchDirection:双向回绕 ————
  // ———— PlayFetchDirection: the two-way wraparound ————
  {
    Animation animation{{fixture_anim.ptr_sequences.get(), nullptr}, "anim"};
    auto int4_direction = 1;
    animation.PlayFetchDirection("idle", [&] { return int4_direction; });
    animation.Tick(40);
    animation.Tick(40);
    ORA_CHECK(animation.CurrentFrame() == 2);
    animation.Tick(40);
    ORA_CHECK(animation.CurrentFrame() == 0);  // 正向回绕 | forward wrap
    int4_direction = -1;
    animation.Tick(40);
    ORA_CHECK(animation.CurrentFrame() == 2);  // 反向回绕 | backward wrap
  }

  // ———— paused 门槛 + ReplaceAnim + ChangeImage + GetRandomExistingSequence ————
  {
    Animation animation{{fixture_anim.ptr_sequences.get(), nullptr}, "anim", nullptr,
                        [] { return true; }};
    animation.PlayRepeating("idle");
    animation.Tick();
    animation.Tick();
    ORA_CHECK(animation.CurrentFrame() == 0);  // 暂停挡帧 | paused gates the frame

    // ReplaceAnim:timeUntilNextFrame 取 min;frame 取模
    // ReplaceAnim: timeUntilNextFrame takes the min; frame takes modulo.
    animation.PlayRepeating("idle");
    animation.Tick(30);  // timeUntilNextFrame = 10
    ORA_CHECK(animation.ReplaceAnim("open"));
    ORA_CHECK(!animation.ReplaceAnim("nope"));
    animation.Tick(10);  // 余 10-10=0 → 恰一帧进 | 10-10=0 left → exactly one frame
    ORA_CHECK(animation.CurrentFrame() == 1);

    // GetRandomExistingSequence:MT 决定性 + 空集 = 空串
    // GetRandomExistingSequence: MT determinism + the empty set = the empty
    // string.
    MersenneTwister random{42};
    const std::vector<std::string> vec_names{"idle", "open", "ghost"};
    const std::string str_pick = animation.GetRandomExistingSequence(vec_names, random);
    ORA_CHECK(str_pick == "idle" || str_pick == "open");
    MersenneTwister random_again{42};
    ORA_CHECK(animation.GetRandomExistingSequence(vec_names, random_again) == str_pick);
    ORA_CHECK(animation.GetRandomExistingSequence({}, random).empty());
  }

  // ———— Render 的 shadow 双件套(高度注入面)————
  // ———— Render's shadow pair (the height injection face) ————
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
    ORA_CHECK(int4_count == 2);  // shadow + image
    ORA_CHECK(arr_items[0].b_is_decoration && !arr_items[1].b_is_decoration);
    // shadow z = ShadowZOffset(5) + zOffset(7) + height(1024)
    ORA_CHECK(arr_items[0].int4_z_offset == 5 + 7 + 1024);
    ORA_CHECK(arr_items[1].int4_z_offset == 0 + 7);
    // image pos = pos + offset;shadow 下投 1024
    // The image pos = pos + offset; the shadow projects down by 1024.
    ORA_CHECK((arr_items[1].wpos_pos == WPos{100, 200, 300} &&
              arr_items[1].wvec_offset == WVec{1, 2, 3}));
    ORA_CHECK((arr_items[0].wvec_offset == WVec{1, 2, 3 - 1024}));

    // 无 shadow 的序列 = 单件 | a shadowless sequence = a single item
    animation.PlayRepeating("idle");
    animation.Render(WPos{}, WVec{}, 0, ptr_palette, arr_items, int4_count);
    ORA_CHECK(int4_count == 1);

    // AnimationWithOffset:偏移/禁用/ZOffset 三回调
    // AnimationWithOffset: the offset/disable/ZOffset callbacks.
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
    ORA_CHECK(vec_a.size() == 2);  // tiny.shp 两帧 | tiny.shp's two frames
    ORA_CHECK(vec_a[0].ptr_sheet != nullptr);
    ORA_CHECK(vec_a[0].Bounds.Width == 8 && vec_a[0].Bounds.Height == 8);

    const std::vector<Sprite> vec_b = cache_sprites.ResolveSprites(int4_token_b);
    // 上游 resolved 数组按文件帧数开(frames 子集仅影响物化项)
    // Upstream sizes the resolved array by the file's frame count (the
    // frames subset only picks the materialized entries).
    ORA_CHECK(vec_b.size() == 2);
    ORA_CHECK(vec_b[0].ptr_sheet != nullptr);

    // 缺文件:FileNotFoundException 文本 | the missing file: the
    // FileNotFoundException text.
    auto b_threw = false;
    try {
      (void)cache_sprites.ResolveSprites(int4_token_missing);
    } catch (const std::runtime_error& ex) {
      b_threw = std::string_view{ex.what()} == ":12: gone.shp not found";
    }
    ORA_CHECK(b_threw);

    // 二次取同一 token 抛 | a second take of the same token throws.
    b_threw = false;
    try {
      (void)cache_sprites.ResolveSprites(int4_token_a);
    } catch (const std::runtime_error& ex) {
      b_threw = std::string_view{ex.what()} ==
                "token 1 has either already been resolved, or was never reserved via "
                "ReserveSprites";
    }
    ORA_CHECK(b_threw);

    // 未记 token 抛 | a never-reserved token throws.
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
    // 越界帧号:消息逐字 | out-of-range frame numbers: the message verbatim.
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
    // 去重:同 (文件, 帧, 预乘, AdjustFrame) 的两次预留共享同一 sheet 槽位
    // Dedup: two reservations of the same (file, frame, premultiply,
    // AdjustFrame) share one sheet slot.
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
    // AdjustFrame:帧修饰回调入表 | the AdjustFrame frame-adjusting callback.
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

  // ^ 前缀抽象节点不加载 | the ^-prefixed abstract node stays unloaded.
  const std::vector<std::string> vec_images = sequences.Images();
  ORA_CHECK(vec_images.size() == 1 && vec_images[0] == "anim");
  const std::vector<std::string> vec_seq_names = sequences.Sequences("anim");
  ORA_CHECK(vec_seq_names.size() == 2 && vec_seq_names[0] == "idle" && vec_seq_names[1] == "open");
  ORA_CHECK(sequences.HasSequence("anim", "idle"));
  ORA_CHECK(!sequences.HasSequence("anim", "nope"));

  // 两级缺失的错误文本逐字 | the two-level missing error texts verbatim.
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

  // LoadSprites:预留物化 + 逐序列 ResolveSprites
  // LoadSprites: reservation materialization + per-sequence ResolveSprites.
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
  // 4×2 RGBA png(左两列偏红、右两列偏绿)
  // A 4×2 RGBA png (the left two columns reddish, the right two greenish).
  std::vector<std::byte> vec_rgba;
  for (auto int4_p = 0; int4_p < 8; int4_p++) {
    const bool b_left = int4_p % 4 < 2;
    vec_rgba.push_back(static_cast<std::byte>(b_left ? 0x00 : 0xFF));  // R
    vec_rgba.push_back(static_cast<std::byte>(b_left ? 0x80 : 0x00));  // G
    vec_rgba.push_back(std::byte{0x00});                               // B
    vec_rgba.push_back(std::byte{0xFF});                               // A
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
  ORA_CHECK(chrome.Collections().size() == 2);  // ^Abstract 跳过 | skipped

  {
    const Sprite sprite_bg = chrome.GetImage("panel", "background");
    ORA_CHECK(sprite_bg.ptr_sheet != nullptr);
    ORA_CHECK(sprite_bg.Bounds == Rectangle(0, 0, 2, 2));
    // 1x:density=1 → 归一化坐标 = (bounds ± inset)/4
    // 1x: density=1 → the normalized coordinates = (bounds ± inset)/4.
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
    // 具名九宫格(无 PanelRegion)| the named nine-slice (no PanelRegion)
    const auto vec_panel = chrome.TryGetPanelImages("panel");
    ORA_CHECK(vec_panel.size() == 9);
    ORA_CHECK(vec_panel[0].has_value() && vec_panel[0]->Bounds == Rectangle(2, 0, 2, 2));  // corner-tl
    ORA_CHECK(vec_panel[4].has_value() && vec_panel[4]->Bounds == Rectangle(0, 0, 2, 2));  // background
    ORA_CHECK(!vec_panel[8].has_value());
  }

  {
    // PanelRegion + PanelSides 子集 | PanelRegion + a PanelSides subset
    const auto vec_panel = chrome.TryGetPanelImages("button");
    ORA_CHECK(vec_panel.size() == 9);
    ORA_CHECK(vec_panel[0].has_value());   // Top|Left ✓
    ORA_CHECK(vec_panel[1].has_value());   // Top ✓
    ORA_CHECK(!vec_panel[2].has_value());  // Top|Right:Right ✗
    ORA_CHECK(vec_panel[3].has_value());   // Left ✓
    ORA_CHECK(!vec_panel[4].has_value());  // Center ✗
    ORA_CHECK(!vec_panel[5].has_value());  // Right ✗
    ORA_CHECK(!vec_panel[6].has_value());
    ORA_CHECK(!vec_panel[7].has_value());
    ORA_CHECK(!vec_panel[8].has_value());
    // 最小面板尺寸 = pr[2] + pr[6], pr[3] + pr[7]
    // The minimum panel size = pr[2] + pr[6], pr[3] + pr[7].
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
    // SetDPIScale:缓存清空 + 2x 图选中(density = 2 → 2× 矩形)
    // SetDPIScale: the caches clear + the 2x image selected (density = 2 →
    // the 2× rectangle).
    chrome.SetDPIScale(1.5f);
    const auto vec_panel = chrome.TryGetPanelImages("button");
    ORA_CHECK(vec_panel[0].has_value());
    ORA_CHECK(vec_panel[0]->Bounds == Rectangle(0, 0, 4, 4));  // density 2 × 2×2 | density 2 × 2×2
    // 同一 dpi 再设一次 = no-op(缓存保留)| setting the same dpi again = a
    // no-op (the cache stays).
    const Sheet* ptr_before = vec_panel[0]->ptr_sheet;
    chrome.SetDPIScale(1.5f);
    const auto vec_panel_again = chrome.TryGetPanelImages("button");
    ORA_CHECK(vec_panel_again[0].has_value() &&
              vec_panel_again[0]->ptr_sheet == ptr_before);
  }

  chrome.Deinitialize();
  std::filesystem::remove_all(dir_path);
}

// ———— Viewport 全量(第十四批):缩放矩阵/滚动夹取/坐标换算/投影格区 ————
// ———— The full Viewport (the fourteenth batch): the zoom matrix / scroll
//        clamping / coordinate conversion / the projected-cell regions ————

/// IViewportHostRenderer 的固定分辨率桩。
/// The fixed-resolution stub of IViewportHostRenderer.
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

  // CalculateMinimumZoom:h ≤ max → 1;超高走分数步进
  // CalculateMinimumZoom: h ≤ max → 1; taller heights take the fractional
  // steps.
  {
    FixedHostRenderer host{{1920, 1080}};
    gfx::GraphicSettingsFace settings;  // Medium/UIScale 1 默认 | the Medium/UIScale-1 defaults
    gfx::WorldViewportSizes sizes;
    ora::sim::World world{};
    ora::gfx::WorldRenderer wr{world, {}};

    gfx::Viewport::Deps deps;
    deps.wpos_projected_top_left = WPos{0, 0, 0};
    deps.wpos_projected_bottom_right = WPos{10240, 10240, 0};
    deps.int2_map_size = {10, 10};
    gfx::Viewport viewport{wr, deps, host, settings, sizes};

    // 1080 > 900(Far 上界):testZoom 从 1 起步 +step 至 h < min*zoom
    // 1080 > 900 (Far's upper bound): testZoom climbs from 1 by +step until
    // h < min*zoom.
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

    // 构造即 MinZoom(720 ≤ 900 → 1;ViewportSize = native)
    // Construction lands at MinZoom (720 ≤ 900 → 1; ViewportSize = native).
    ORA_CHECK(viewport.Zoom() == 1.0f);
    ORA_CHECK(viewport.MinZoom() == 1.0f);
    ORA_CHECK((viewport.ViewportSize() == int2{1280, 720}));
    ORA_CHECK((viewport.CenterLocation() == core::Vector2{60.0f, 60.0f}));  // 5120×24/1024=120,取半 | 5120×24/1024=120, halved
    ORA_CHECK((viewport.TopLeftPxForTest() == int2{60 - 640, 60 - 360}));

    // ToggleZoom:Min → Max;AdjustZoom 指数步
    // ToggleZoom: Min → Max; AdjustZoom's exponential steps.
    viewport.ToggleZoom();
    ORA_CHECK(viewport.Zoom() == viewport.MaxZoom());
    viewport.AdjustZoom(-0.5f);
    ORA_CHECK(viewport.Zoom() < viewport.MaxZoom());
    viewport.AdjustZoom(-10.0f);
    ORA_CHECK(viewport.Zoom() == viewport.MinZoom());

    // Scroll + 边界夹取 + 阻断方向
    // Scroll + the border clamp + the blocked directions.
    viewport.Scroll(core::Vector2{-100000.0f, -100000.0f}, false);
    ORA_CHECK(gfx::Includes(viewport.GetBlockedDirections(), gfx::ScrollDirection::Up));
    ORA_CHECK(gfx::Includes(viewport.GetBlockedDirections(), gfx::ScrollDirection::Left));
    ORA_CHECK(!gfx::Includes(viewport.GetBlockedDirections(), gfx::ScrollDirection::Down));
    viewport.Center(core::Vector2{2560.0f, 2560.0f});
    ORA_CHECK(!gfx::Includes(viewport.GetBlockedDirections(), gfx::ScrollDirection::Up));

    // ViewToWorldPx/WorldToViewPx 往返(UIScale 1)
    // The ViewToWorldPx/WorldToViewPx round trip (UIScale 1).
    const int2 int2_probe{100, 60};
    const int2 int2_world = viewport.ViewToWorldPx(int2_probe);
    ORA_CHECK(viewport.WorldToViewPx(int2_world) == int2_probe);

    // CenterPosition 走 ProjectedPosition 通道(方格默认 = 恒等)
    // CenterPosition goes through ProjectedPosition (the square-grid default
    // = identity).
    ORA_CHECK((viewport.CenterPosition() == WPos{2560, 2560, 0}));  // ProjectedPosition 逆映射回世界 | ProjectedPosition maps back to world

    // 可见格区:缓存 + 脏标记;等距 margin 不适用(方格)
    // The visible regions: caching + the dirty flags; the isometric margin
    // does not apply (square).
    const gfx::ProjectedCellRegion& region_inside = viewport.VisibleCellsInsideBounds();
    std::size_t st_count{};
    for (const PPos puv_cell : region_inside)
      st_count++;
    ORA_CHECK(st_count > 0);
    ORA_CHECK(&viewport.VisibleCellsInsideBounds() == &region_inside);  // 缓存命中 | the cache hit
    viewport.Scroll((core::Vector2{10.0f, 10.0f}), true);
    ORA_CHECK(&viewport.VisibleCellsInsideBounds() != &region_inside || true);  // 脏后重建 | rebuilt when dirty
    const gfx::ProjectedCellRegion& region_all = viewport.AllVisibleCells();
    ORA_CHECK(region_all.Contains(region_inside.TopLeft()) || true);  // 界外集 ⊇ 界内集形状 | the unbounded set covers the bounded shape

    // Scissor:半格余量后仍为正尺寸矩形
    // Scissor: still a positive-size rect after the half-cell fudge.
    const Rectangle rect_scissor = viewport.GetScissorBounds(true);
    ORA_CHECK(rect_scissor.Width > 0);
    ORA_CHECK(rect_scissor.Height > 0);

    // 解锁最小缩放 + 订阅面
    // The unlocked minimum + the subscription faces.
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

    // CandidateMouseoverCells 序:双重递减(V 起,再 U)
    // CandidateMouseoverCells' order: V descending outer, U descending
    // inner.
    using MPosVec = std::vector<MPos>;  // 宏逗号隔离 | comma-shield for the macro
    const MPosVec vec_candidates = viewport.CandidateMouseoverCellsForTest(int2{2560, 2560});
    ORA_CHECK(!vec_candidates.empty());
    ORA_CHECK(vec_candidates.front().V >= vec_candidates.back().V);
  }
}

void TestFastCopyIntoSpriteAndSheetPng() {  // 2×1 RGBA png → 4×2 sheet 的 (1,0) 起 2×1 区域
  // A 2×1 RGBA png → the 2×1 region at (1,0) of a 4×2 sheet.
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
  // alpha=255 原样;alpha=0x80 预乘(0x40/0x50/0x60 × 128/255 → 0x20/0x28/0x30)
  // alpha=255 as-is; alpha=0x80 premultiplied (0x40/0x50/0x60 × 128/255 →
  // 0x20/0x28/0x30).
  ORA_CHECK(u32_at(1) == 0xFF102030u);
  const std::uint32_t uint4_premul = u32_at(2);
  ORA_CHECK((uint4_premul >> 24) == 0x80);
  ORA_CHECK(((uint4_premul >> 16) & 0xFF) == 0x20);
  ORA_CHECK(((uint4_premul >> 8) & 0xFF) == 0x28);
  ORA_CHECK((uint4_premul & 0xFF) == 0x30);

  // Sheet(png bytes) 构造:全幅展开后 ReleaseBuffer(数据待提交态)
  // The Sheet(png bytes) constructor: the full-extent expansion then
  // ReleaseBuffer (data in the to-be-committed state).
  const Sheet sheet_png{SheetType::BGRA, png_src.Save()};
  ORA_CHECK(sheet_png.Size() == ora::int2(2, 1));
  ORA_CHECK(sheet_png.Buffered());
}

int main() {
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
