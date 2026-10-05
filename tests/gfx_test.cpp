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
#include "gfx/sprite_renderer.hpp"

#if defined(_WIN32) || defined(__linux__) || defined(__APPLE__)
#define ORA_HAS_DESKTOP_GL 1
#define SDL_MAIN_HANDLED
#include "gfx/render_thread.hpp"
#include "platform/sdl2_window.hpp"
#endif

namespace {

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
}

#endif  // ORA_HAS_DESKTOP_GL

}  // namespace

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

#ifdef ORA_HAS_DESKTOP_GL
  TestGlSheetAndPalette();
  TestGlFourthBatch();
#endif

  if (int4_failures != 0) {
    std::println(stderr, "gfx_test: {} 项失败 | {} failure(s)", int4_failures, int4_failures);
    return 1;
  }
  std::println("gfx_test: PASS(Sheet/Palette/HardwarePalette + SpriteRenderer(持久 VB 槽回绕/BlendSpan/VAO)+ 单级合成 Renderer)");
  return 0;
}
