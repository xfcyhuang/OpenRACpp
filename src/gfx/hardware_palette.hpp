// UPSTREAM: OpenRA.Game/Graphics/HardwarePalette.cs @b6fc03f L18-161 +
//           OpenRA.Game/Traits/IPaletteModifier.cs @b6fc03f(接口)
// 硬件调色板:256×N 纹理(行 0 保留为"无色移占位"—— 上游 PERF 注释保留)+
// ColorShifts 浮点纹理(HSV 色移参数,combined.frag 的 rgb2hsv 消费)。
// 可变调色板经 modifier 每次调整后写回缓冲并上传;ApplyModifiers 末尾把
// 可变调色板重置回原色(modifier 是无状态逐帧调整)。
//
// OPT-A7(docs/OPTIMIZATION_TRACKER.md)落地点 —— 调色板 dirty 行:
//   上游 HardwarePalette.cs L132-154 每次 ReplacePalette/ApplyModifiers/
//   Initialize 无条件 CopyBufferToTexture 全量上传(256×H RGBA8 + float 表);
//   本实现按调色板行跟踪脏位 —— 单行变化走 SetSubData 逐行增量,高度增长
//   或脏行过半(启发式)退回全量 SetData。
//   OPT-C5 语义论证:上传字节与落点完全相同(SetSubData 与 SetData 写同一
//   mipmap level 0 的同一矩形),渲染结果逐像素一致 —— gfx_test 的 GL 集成
//   断言"增量上传读回 == 全量上传读回"逐字节验证。
//   ColorShifts 表(2×H float)保持全量上传(整表 ≤ 数百字节,逐行收益为零)。
//
// 形态适配:Game.Renderer.Context.CreateTexture() → RenderThread* 注入
// (空 = 纯数据模式:脏位跟踪照常,上传跳过;Utility 路径);三 Dictionary
// → std::map(查询按名;遍历仅写互不相交的行,顺序不可观测);IPaletteModifier
// 的 IReadOnlyDictionary → map 引用直传。ReplacePalette/AddPalette 的
// InvalidOperationException → std::runtime_error(消息文本逐字)。
// The hardware palette: a 256×N texture (row 0 reserved as the "no color
// shift" placeholder — the upstream PERF comment preserved) plus the
// ColorShifts float texture (HSV shift parameters consumed by
// combined.frag's rgb2hsv). Mutable palettes are adjusted by modifiers,
// written back into the buffer, and uploaded; ApplyModifiers resets them
// to their original colors afterwards (modifiers are stateless per-frame
// adjustments).
// OPT-A7 landing — palette dirty rows: upstream's
// ReplacePalette/ApplyModifiers/Initialize all upload the full texture
// unconditionally (HardwarePalette.cs L132-154); this implementation
// tracks per-row dirty bits — single-row changes go through per-row
// SetSubData, with height growth or a dirty-majority heuristic falling
// back to a full SetData. OPT-C5 argument: the uploaded bytes and their
// destinations are identical (SetSubData and SetData write the same rect
// of the same mipmap level 0), so the render output is pixel-identical —
// asserted byte-for-byte in gfx_test's GL integration ("incremental
// upload readback == full upload readback"). The ColorShifts table
// (2×H floats) stays a full upload (a few hundred bytes at most; per-row
// gains are zero). Shape adaptations: Game.Renderer.Context.CreateTexture()
// → RenderThread* injection (null = the data-only mode: dirty tracking
// continues, uploads skip; the Utility path); the three Dictionaries →
// std::map (lookups by name; iterations write disjoint rows, so ordering
// is unobservable); IPaletteModifier's IReadOnlyDictionary → a map
// reference. The InvalidOperationExceptions of ReplacePalette/AddPalette
// → std::runtime_error (message texts verbatim).
#pragma once
import std;

#include "gfx/palette.hpp"
#include "gfx/texture.hpp"

namespace ora::gfx {

/// IPaletteModifier(Traits/IPaletteModifier.cs;AdjustPalette 直接改可变
/// 调色板字典;上游经 WorldRenderer 每帧收集)。
/// IPaletteModifier (Traits/IPaletteModifier.cs; AdjustPalette mutates the
/// mutable-palette dictionary directly; collected per frame by
/// WorldRenderer upstream).
struct IPaletteModifier {
  virtual void AdjustPalette(std::map<std::string, MutablePalette, std::less<>>& map_mutable_palettes) = 0;
  virtual ~IPaletteModifier() = default;
};

/// 硬件调色板(HardwarePalette.cs L18-161;不可拷贝移动 —— 持纹理 RAII)。
/// The hardware palette (HardwarePalette.cs L18-161; non-copyable and
/// non-movable — holds RAII textures).
class HardwarePalette {
 public:
  /// 构造即建两张纹理(上游 L30-34);ptr_render 为空 = 纯数据模式。
  /// Creates both textures on construction (upstream L30-34); a null
  /// ptr_render selects the data-only mode.
  explicit HardwarePalette(RenderThread* ptr_render);

  HardwarePalette(const HardwarePalette&) = delete;
  HardwarePalette& operator=(const HardwarePalette&) = delete;

  bool Contains(std::string_view str_name) const;

  /// 取调色板(可变优先;上游 AsReadOnly 包装 → const 引用;找不到
  /// throw "Palette `name` does not exist")。
  /// The palette (mutables first; upstream's AsReadOnly wrapper → a const
  /// reference; throws "Palette `name}` does not exist" when absent).
  const IPalette& GetPalette(std::string_view str_name) const;

  /// 行索引(找不到 throw 同文本)。
  /// The row index (same throw text when absent).
  std::int32_t GetPaletteIndex(std::string_view str_name) const;

  /// 登记调色板(行 0 保留 ⇒ 首个索引为 1;高度按 NextPowerOf2(index+1)
  /// 增长;allowModifiers = 登记 MutablePalette,否则直接写缓冲)。
  /// Registers a palette (row 0 reserved ⇒ the first index is 1; height
  /// grows as NextPowerOf2(index+1); allowModifiers registers a
  /// MutablePalette, else the buffer is written directly).
  void AddPalette(std::string_view str_name, ImmutablePalette palette_p, bool b_allow_modifiers);

  /// 整体替换(上游 L85-97;可变侧重建 MutablePalette;随后上传)。
  /// Replaces wholesale (upstream L85-97; the mutable side is rebuilt;
  /// uploads afterwards).
  void ReplacePalette(std::string_view str_name, const IPalette& palette_p);

  /// 写 HSV 色移参数(上游 L99-107;仅写 CPU 缓冲 + 标脏 —— 上传延迟到
  /// 下一次 CopyBufferToTexture 调用点,上游行为一致)。
  /// Writes the HSV color-shift parameters (upstream L99-107; CPU buffer +
  /// dirty bit only — the upload defers to the next CopyBufferToTexture
  /// call site, matching upstream).
  void SetColorShift(std::string_view str_name, float float_hue_offset, float float_sat_offset,
                     float float_value_multiplier, float float_min_hue, float float_max_hue);

  bool HasColorShift(std::string_view str_name) const;

  /// 可变调色板写入缓冲 + 上传(上游 L115-119)。
  /// Writes mutable palettes into the buffer + uploads (upstream L115-119).
  void Initialize();

  /// 应用 modifier(上游 L138-154):逐个调整 → 写缓冲 → 上传(OPT-A7
  /// 增量)→ 把可变调色板重置回原色(为下一次调整做准备)。
  /// Applies modifiers (upstream L138-154): adjust each → write the
  /// buffer → upload (OPT-A7 incremental) → reset mutables to their
  /// original colors (ready for the next round).
  void ApplyModifiers(std::span<IPaletteModifier* const> vec_palette_mods);

  /// 上游 CopyBufferToTexture(L132-136)—— OPT-A7 后为脏驱动的增量上传。
  /// Upstream's CopyBufferToTexture (L132-136) — a dirty-driven
  /// incremental upload after OPT-A7.
  void CopyBufferToTexture();

  std::int32_t Height() const { return int4_height_; }

  /// OPT-A7:色移 epoch —— HasColorShift 可观察结果每次翻转时递增;
  /// PaletteReference 以 (epoch, value) 缓存消除每精灵字符串字典查找。
  /// OPT-A7: the color-shift epoch — incremented whenever HasColorShift's
  /// observable result flips; PaletteReference caches (epoch, value) to kill
  /// the per-sprite string-dictionary lookup.
  std::uint32_t ShiftEpoch() const { return uint4_shift_epoch_; }

  Texture* TextureOrNull() { return opt_texture_ ? &*opt_texture_ : nullptr; }
  Texture* ColorShiftsOrNull() { return opt_color_shifts_ ? &*opt_color_shifts_ : nullptr; }

  /// —— OPT-A7 诊断/测试面 ——
  /// —— OPT-A7 diagnostics/test surface ——
  bool FullDirty() const { return b_full_dirty_; }
  bool ShiftsDirty() const { return b_shifts_dirty_; }
  bool RowDirty(std::int32_t int4_row) const {
    return int4_row < static_cast<std::int32_t>(vecb_dirty_rows_.size()) && vecb_dirty_rows_[int4_row];
  }

  /// 最近一次上传是否走了全量路径(增量性断言:单行 ReplacePalette 后
  /// 应为 false)。
  /// Whether the most recent upload took the full path (the incrementality
  /// assertion: false after a single-row ReplacePalette).
  bool LastUploadFull() const { return b_last_upload_full_; }

  /// CPU 缓冲只读视图(OPT-C5 验证:与全量上传参考逐字节对比)。
  /// Read-only CPU buffer view (the OPT-C5 verification: compared
  /// byte-for-byte against a full-upload reference).
  std::span<const std::byte> BufferForTest() const { return vec_buffer_; }

 private:
  /// 写行入缓冲 + 标脏(OPT-A7;上游 L121-124 无标脏)。
  /// Writes a row into the buffer + marks dirty (OPT-A7; upstream L121-124
  /// has no dirty tracking).
  void CopyPaletteToBuffer(std::int32_t int4_index, const IPalette& palette_p);

  /// 全部可变调色板写入缓冲(各自行标脏;上游 L126-130)。
  /// Writes every mutable palette into the buffer (rows marked dirty;
  /// upstream L126-130).
  void CopyModifiablePalettesToBuffer();

  RenderThread* ptr_render_ = nullptr;
  std::optional<Texture> opt_texture_;
  std::optional<Texture> opt_color_shifts_;

  std::int32_t int4_height_ = 0;
  std::map<std::string, ImmutablePalette, std::less<>> map_palettes_;
  std::map<std::string, MutablePalette, std::less<>> map_mutable_palettes_;
  std::map<std::string, std::int32_t, std::less<>> map_indices_;
  std::vector<std::byte> vec_buffer_;        // Height×256×4,BGRA 字节序 | BGRA bytes
  std::vector<float> vec_color_shift_buffer_;  // Height×8(2×H 上传布局)| the 2×H upload layout

  // —— OPT-A7 dirty 跟踪 ——
  // —— OPT-A7 dirty tracking ——
  bool b_full_dirty_ = true;              // 初始/高度变化 → 全量 | initial/height change → full
  bool b_shifts_dirty_ = true;
  bool b_last_upload_full_ = true;        // 诊断:上次上传路径 | diagnostic: the last upload's path
  std::vector<bool> vecb_dirty_rows_;     // 尺寸 = int4_height_ | sized to int4_height_
  std::uint32_t uint4_shift_epoch_ = 0;   // OPT-A7:HasColorShift 结果翻转计数 | OPT-A7: flips of HasColorShift's result
};

}  // namespace ora::gfx
