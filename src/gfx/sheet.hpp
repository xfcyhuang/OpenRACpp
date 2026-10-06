// UPSTREAM: OpenRA.Game/Graphics/Sheet.cs @b6fc03f L19-196 +
//           OpenRA.Game/Graphics/SheetBuilder.cs @b6fc03f L34-175
// Sheet:CPU 侧像素缓冲 + 惰性 GL 纹理;dirty 全量/子区域上传自动切换
// (dirtyRegion 且纹理尺寸匹配时 SetSubData,否则 SetData);缓冲可释放
// (ReleaseBuffer 提交后丢弃,转移给下一张 sheet 复用)。
// SheetBuilder:shelf 打包(行高推进、margin 间隔、Indexed 类型四通道
// 轮换 R→G→B→A,BGRA 类型直接换 sheet)。
// 形态适配(上游 Game.Renderer 全局访问 → RenderThread* 注入;render 为空
// = 上游 Utility 的 Game.Renderer == null 纯数据路径,GetTexture 断言挡):
//   - Sheet 纹理由 optional<Texture> RAII 持有(上游手动 Dispose);
//   - 上游 Sheet(Stream)(Png 解码)随 formats 第二编(png/dds/tga);AsPng 同;
//   - Add(ISpriteFrame) 已随 formats 第一编接线;Add(Png) 随第二编;
//   - ReleaseBufferAndTryTransferTo 转移"清零缓冲"改为重新分配(上游
//     转移原数组对象复用;内容同为全零,行为等价 —— COVERAGE 登记)。
// Sheet: the CPU-side pixel buffer with a lazily created GL texture;
// dirty full/sub-rectangle uploads switch automatically (SetSubData when a
// dirtyRegion exists and the texture size matches, else SetData); the
// buffer may be released (discarded after commit in ReleaseBuffer, or
// transferred to the next sheet for reuse). SheetBuilder: shelf packing
// (row-height advance, margin gaps, and the Indexed four-channel rotation
// R→G→B→A with BGRA types going straight to a new sheet).
// Shape adaptations (upstream's Game.Renderer global → RenderThread*
// injection; a null render reproduces upstream's Game.Renderer == null
// Utility data-only path, with GetTexture asserting):
//   - the Sheet texture is owned by an optional<Texture> RAII handle
//     (upstream Disposes manually);
//   - Sheet(Stream) (Png decode) and AsPng await the second formats
//     batch (png/dds/tga);
//   - Add(ISpriteFrame) is wired with the first formats batch; Add(Png)
//     awaits the second;
//   - ReleaseBufferAndTryTransferTo re-allocates the transferred
//     "cleared buffer" instead of reusing the original array object
//     (upstream recycles the array; contents are equally all-zero —
//     registered in COVERAGE).
#pragma once
import std;

#include "core/int2.hpp"
#include "core/rectangle.hpp"
#include "gfx/sprite.hpp"
#include "gfx/sprite_frame.hpp"
#include "gfx/texture.hpp"

namespace ora::gfx {

/// 纹理集(Sheet.cs L19-196;不可拷贝不可移动 —— Sprite 持其裸指针)。
/// A texture sheet (Sheet.cs L19-196; non-copyable and non-movable —
/// Sprites hold raw pointers to it).
class Sheet {
 public:
  /// 纯数据/惰性纹理模式(ptr_render 为空时 GetTexture 不可用;上游
  /// Game.Renderer == null 的 Utility 路径)。
  /// The data-only/lazy-texture mode (GetTexture unavailable when
  /// ptr_render is null; upstream's Game.Renderer == null Utility path).
  Sheet(SheetType kind_type, int2 int2_size, RenderThread* ptr_render = nullptr);

  /// 包裹既有纹理(FBO 合成目标;Sheet.cs L46-51)。
  /// Wraps an existing texture (an FBO compositing target; Sheet.cs L46-51).
  Sheet(SheetType kind_type, Texture&& texture_owned);

  /// 包裹外部纹理的非拥有形态(单级合成的 worldSheet:引用 FrameBuffer 的
  /// 颜色附件,生命周期归 FrameBuffer;上游 new Sheet(BGRA,
  /// worldBuffer.Texture) 的共享引用语义)。
  /// The non-owning external-texture form (the single-pass compositor's
  /// worldSheet: references the FrameBuffer's color attachment, whose
  /// lifetime belongs to the FrameBuffer; the shared-reference semantics of
  /// upstream's new Sheet(BGRA, worldBuffer.Texture)).
  Sheet(SheetType kind_type, Texture& texture_external);

  /// Png 字节流构造(Sheet.cs L57-66;ChromeProvider 的 BGRA 面使用)。
  /// The Png-bytes constructor (Sheet.cs L57-66; used by ChromeProvider's
  /// BGRA face).
  Sheet(SheetType kind_type, std::span<const std::byte> vec_png_bytes,
        RenderThread* ptr_render = nullptr);

  Sheet(const Sheet&) = delete;
  Sheet& operator=(const Sheet&) = delete;
  Sheet(Sheet&&) = delete;
  Sheet& operator=(Sheet&&) = delete;

  /// CPU 缓冲(不存在则按需建立:无纹理 = 零分配,有纹理 = GPU 读回)。
  /// The CPU buffer (created on demand: zero-filled without a texture,
  /// read back from the GPU with one).
  std::span<std::byte> GetData();

  /// 有 CPU 缓冲,或尚未建纹理(Sheet.cs L37)。
  /// Has a CPU buffer, or no texture yet (Sheet.cs L37).
  bool Buffered() const { return opt_data_.has_value() || (!opt_texture_.has_value() && ptr_texture_external_ == nullptr); }

  /// 取纹理(惰性建 + 提交脏数据;Sheet.cs L64-91)。
  /// The texture (lazily created; commits dirty data; Sheet.cs L64-91).
  Texture& GetTexture();

  /// 建 CPU 缓冲(Sheet.cs L122-133)。
  /// Creates the CPU buffer (Sheet.cs L122-133).
  void CreateBuffer();

  /// 标脏子区域(与既有 dirtyRegion 取并集;Sheet.cs L135-154)。
  /// Marks a sub-rectangle dirty (unioned with any existing dirtyRegion;
  /// Sheet.cs L135-154).
  void CommitBufferedData(Rectangle rect_region);

  /// 标脏全幅(Sheet.cs L151-154)。
  /// Marks the full extent dirty (Sheet.cs L151-154).
  void CommitBufferedData() { CommitBufferedData(Rectangle{0, 0, int2_size_.X, int2_size_.Y}); }

  /// 释放 CPU 缓冲(提交上传后丢弃;Sheet.cs L156-167)。
  /// Releases the CPU buffer (committed to the GPU, then discarded;
  /// Sheet.cs L156-167).
  void ReleaseBuffer();

  /// 释放并把清零缓冲转给目标 sheet 复用(Sheet.cs L169-190)。
  /// Releases, transferring a zeroed buffer to the destination sheet for
  /// reuse (Sheet.cs L169-190).
  bool ReleaseBufferAndTryTransferTo(Sheet& sheet_destination);

  int2 Size() const { return int2_size_; }
  SheetType Type() const { return kind_type_; }

  /// 测试面:当前 dirtyRegion 观察(gfx_test 断言)。
  /// Test surface: observe the current dirtyRegion (gfx_test assertions).
  const std::optional<Rectangle>& DirtyRegion() const { return rect_dirty_region_; }

 private:
  bool b_dirty_ = false;
  std::optional<Rectangle> rect_dirty_region_;
  bool b_release_buffer_on_commit_ = false;
  std::optional<Texture> opt_texture_;
  Texture* ptr_texture_external_ = nullptr;  // 非拥有(单级合成 worldSheet)| non-owning (the compositor's worldSheet)
  std::optional<std::vector<std::byte>> opt_data_;  // has_value ≈ 上游 data != null | ≈ upstream's data != null
  RenderThread* ptr_render_ = nullptr;
  int2 int2_size_{};
  SheetType kind_type_ = SheetType::Indexed;
};

/// 上游 SheetOverflowException(SheetBuilder.cs L20-24)。
/// Upstream's SheetOverflowException (SheetBuilder.cs L20-24).
class SheetOverflowException : public std::runtime_error {
 public:
  explicit SheetOverflowException(const std::string& str_message) : std::runtime_error(str_message) {}
};

/// 纹理集打包器(SheetBuilder.cs L34-175;析构释放全部 sheet)。
/// The sheet packer (SheetBuilder.cs L34-175; destruction releases all
/// sheets).
class SheetBuilder {
 public:
  /// 工厂签名(上游 Func<Sheet>;C++ 返回堆持有)。
  /// The factory signature (upstream Func<Sheet>; heap-owned here).
  using AllocateSheetFn = std::function<std::unique_ptr<Sheet>()>;

  SheetBuilder(SheetType kind_type, std::int32_t int4_sheet_size, std::int32_t int4_margin = 1,
               RenderThread* ptr_render = nullptr);

  SheetBuilder(SheetType kind_type, AllocateSheetFn fn_allocate_sheet, std::int32_t int4_margin = 1)
      : kind_current_channel_{ChannelOf(kind_type)},
        kind_type_{kind_type},
        fn_allocate_sheet_{std::move(fn_allocate_sheet)},
        int4_margin_{int4_margin} {}

  /// AllocateSheet 工厂(SheetBuilder.cs L47-50;ptr_render 透传)。
  /// The AllocateSheet factory (SheetBuilder.cs L47-50; ptr_render passes
  /// through).
  static std::unique_ptr<Sheet> AllocateSheet(SheetType kind_type, std::int32_t int4_sheet_size,
                                              RenderThread* ptr_render = nullptr);

  /// SpriteFrameType → SheetType(SheetBuilder.cs L52-67)。
  /// SpriteFrameType → SheetType (SheetBuilder.cs L52-67).
  static SheetType FrameTypeToSheetType(SpriteFrameType kind_frame_type);

  /// 帧接口入集(SheetBuilder.cs L85-87;Data/Type/Size/Offset 直通,
  /// Offset.AsVector3 = (x, y, 0))。
  /// Adds a frame interface (SheetBuilder.cs L85-87; Data/Type/Size/
  /// Offset passed through, Offset.AsVector3 = (x, y, 0)).
  Sprite Add(const ISpriteFrame& frame, bool b_premultiplied = false) {
    return Add(frame.Data(), frame.Type(), frame.Size(), 0.0f,
               core::Vector3{frame.Offset().X, frame.Offset().Y, 0.0f}, b_premultiplied);
  }

  /// 帧数据入集(SheetBuilder.cs L89-102 的字节区间形态;Png 重载随
  /// formats 第二编)。空尺寸直接返回空 Sprite(不占纹理)。
  /// Copies frame data into the sheet (the byte-span form of
  /// SheetBuilder.cs L89-102; the Png overload awaits the second formats
  /// batch). Empty sizes return an empty Sprite (no texture space).
  Sprite Add(std::span<const std::byte> vec_src, SpriteFrameType kind_frame_type, int2 int2_size,
             bool b_premultiplied = false) {
    return Add(vec_src, kind_frame_type, int2_size, 0.0f, core::Vector3{}, b_premultiplied);
  }

  Sprite Add(std::span<const std::byte> vec_src, SpriteFrameType kind_frame_type, int2 int2_size, float float_z_ramp,
             core::Vector3 v_sprite_offset, bool b_premultiplied = false);

  /// 只分配不拷贝(SheetBuilder.cs L121-167)。scale 缩放 Sprite 尺寸
  /// (bounds 不变)。
  /// Allocates without copying (SheetBuilder.cs L121-167). scale scales the
  /// Sprite size (bounds unchanged).
  Sprite Allocate(int2 int2_image_size, float float_scale = 1.0f) {
    return Allocate(int2_image_size, 0.0f, core::Vector3{}, float_scale);
  }

  Sprite Allocate(int2 int2_image_size, float float_z_ramp, core::Vector3 v_sprite_offset, float float_scale = 1.0f);

  Sheet* Current() const { return ptr_current_; }
  TextureChannel CurrentChannel() const { return kind_current_channel_; }

  /// 全部已分配 sheet(SheetBuilder.cs L45 的 AllSheets)。
  /// Every allocated sheet (the AllSheets of SheetBuilder.cs L45).
  const std::vector<std::unique_ptr<Sheet>>& AllSheets() const { return vec_sheets_; }

 private:
  static TextureChannel ChannelOf(SheetType kind_type) {
    return kind_type == SheetType::Indexed ? TextureChannel::Red : TextureChannel::RGBA;
  }

  std::optional<TextureChannel> NextChannel(TextureChannel kind_channel) const;

  TextureChannel kind_current_channel_ = TextureChannel::Red;
  SheetType kind_type_ = SheetType::Indexed;
  std::vector<std::unique_ptr<Sheet>> vec_sheets_;
  AllocateSheetFn fn_allocate_sheet_;
  std::int32_t int4_margin_ = 1;
  std::int32_t int4_row_height_ = 0;
  int2 int2_p_{};
  Sheet* ptr_current_ = nullptr;
};

// ———— Sprite 构造(需 Sheet 完整类型,自 sprite.hpp 前置声明后移此处)————
// ———— Sprite constructors (require the complete Sheet type; moved here from sprite.hpp's forward declaration) ————

/// 四参便捷构造(Sprite.cs L29-30)。
/// The four-argument convenience constructor (Sprite.cs L29-30).
inline Sprite::Sprite(Sheet& sheet, Rectangle bounds, TextureChannel channel, float scale)
    : Sprite(sheet, bounds, 0.0f, core::Vector3{}, channel, BlendMode::Alpha, scale) {}

/// 全参构造(Sprite.cs L32-51 逐句;归一化坐标 = (边界 ± 1/128 inset)/sheet 尺寸)。
/// The full constructor (Sprite.cs L32-51 statement by statement; the
/// normalized coordinates = (edge ± 1/128 inset) / sheet size).
inline Sprite::Sprite(Sheet& sheet, Rectangle bounds, float z_ramp, core::Vector3 offset, TextureChannel channel,
                      BlendMode blend_mode, float scale)
    : Bounds{bounds},
      ptr_sheet{&sheet},
      kind_blend{blend_mode},
      kind_channel{channel},
      float_z_ramp{z_ramp},
      vec_size{scale * static_cast<float>(bounds.Width), scale * static_cast<float>(bounds.Height),
               scale * bounds.Height * z_ramp},
      vec_offset{offset} {
  // Some GPUs suffer from precision issues when rendering into non 1:1
  // framebuffers that result in rendering a line of texels that sample
  // outside the sprite rectangle. Insetting the texture coordinates by a
  // small fraction of a pixel avoids this with negligible impact on the
  // 1:1 rendering case. (Sprite.cs L42-45)
  constexpr float kInset = 1.0f / 128.0f;
  const int2 int2_size_sheet = ptr_sheet->Size();
  const auto float_left_bound = static_cast<float>(std::min(Bounds.Left(), Bounds.Right()));
  const auto float_top_bound = static_cast<float>(std::min(Bounds.Top(), Bounds.Bottom()));
  const auto float_right_bound = static_cast<float>(std::max(Bounds.Left(), Bounds.Right()));
  const auto float_bottom_bound = static_cast<float>(std::max(Bounds.Top(), Bounds.Bottom()));
  float_left = (float_left_bound + kInset) / int2_size_sheet.X;
  float_top = (float_top_bound + kInset) / int2_size_sheet.Y;
  float_right = (float_right_bound - kInset) / int2_size_sheet.X;
  float_bottom = (float_bottom_bound - kInset) / int2_size_sheet.Y;
}

/// 二级数据构造(Sprite.cs L54-72;置 b_secondary 判别位)。
/// The secondary-data constructor (Sprite.cs L54-72; sets the b_secondary
/// discriminator).
inline SpriteWithSecondaryData::SpriteWithSecondaryData(const Sprite& sprite, Sheet& secondary_sheet,
                                                        Rectangle secondary_bounds, TextureChannel secondary_channel)
    : Sprite{sprite} {
  // 载荷字段在基类(防按值切片)—— 构造体内直写
  // The payload fields sit in the base (against by-value slicing) —
  // written directly in the body.
  ptr_secondary_sheet = &secondary_sheet;
  SecondaryBounds = secondary_bounds;
  kind_secondary_channel = secondary_channel;
  b_secondary = true;
  const int2 int2_size_sheet = ptr_sheet->Size();
  float_secondary_left =
      static_cast<float>(std::min(SecondaryBounds.Left(), SecondaryBounds.Right())) / int2_size_sheet.X;
  float_secondary_top =
      static_cast<float>(std::min(SecondaryBounds.Top(), SecondaryBounds.Bottom())) / int2_size_sheet.Y;
  float_secondary_right =
      static_cast<float>(std::max(SecondaryBounds.Left(), SecondaryBounds.Right())) / int2_size_sheet.X;
  float_secondary_bottom =
      static_cast<float>(std::max(SecondaryBounds.Top(), SecondaryBounds.Bottom())) / int2_size_sheet.Y;
}

}  // namespace ora::gfx
