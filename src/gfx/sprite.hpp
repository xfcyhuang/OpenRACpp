// UPSTREAM: OpenRA.Game/Graphics/Sprite.cs @b6fc03f L18-82 +
//           OpenRA.Game/Graphics/SheetBuilder.cs @b6fc03f L26-32(SheetType)+
//           OpenRA.Game/Graphics/PlatformInterfaces.cs @b6fc03f L36-48(BlendMode)+
//           OpenRA.Game/Graphics/SpriteLoader.cs @b6fc03f L24-59(SpriteFrameType)
// 精灵值类型:Bounds/Sheet/BlendMode/Channel/ZRamp/Size/Offset 全 readonly 逐字段;
// 归一化纹理坐标 Left/Top/Right/Bottom 在构造期预计算(含 1/128 像素 inset,
// 防 GPU 在非 1:1 帧缓冲上采样溢出采样到精灵矩形外 —— 上注释逐句保留)。
// SpriteWithSecondaryData = 深度/辅助通道的第二纹理引用(无 inset)。
// Sprite value type: Bounds/Sheet/BlendMode/Channel/ZRamp/Size/Offset are
// all readonly, field for field; the normalized texture coordinates
// Left/Top/Right/Bottom are precomputed at construction (with the 1/128-pixel
// inset guarding GPUs that sample a texel row outside the sprite rect on
// non-1:1 framebuffers — the upstream comment preserved verbatim).
// SpriteWithSecondaryData carries the second (depth/auxiliary) texture
// reference (no inset).
#pragma once
import std;

#include "core/int2.hpp"
#include "core/rectangle.hpp"
#include "core/vector_n.hpp"

namespace ora::gfx {

/// 纹理通道(SPRITE.CS L74-81;字节枚举)。RGBA = 4 是"整张采样"标记,
/// 不是物理通道 —— 顶点属性位域按 RGBA==0x02 打包(见 gfx_util FastCreateQuad)。
/// A texture channel (Sprite.cs L74-81; a byte enum). RGBA = 4 is the
/// "sample-all-channels" marker, not a physical channel — the vertex
/// attribute bitfield packs RGBA as 0x02 (see FastCreateQuad in gfx_util).
enum class TextureChannel : std::uint8_t {
  Red = 0,
  Green = 1,
  Blue = 2,
  Alpha = 3,
  RGBA = 4,
};

/// 混合模式(PlatformInterfaces.cs L36-48;字节枚举;GL 映射在上下文层)。
/// Blend modes (PlatformInterfaces.cs L36-48; a byte enum; the GL mapping
/// lives in the context layer).
enum class BlendMode : std::uint8_t {
  None,
  Alpha,
  Additive,
  Subtractive,
  Multiply,
  Multiplicative,
  DoubleMultiplicative,
  LowAdditive,
  Screen,
  Translucent,
};

/// 帧像素格式(SpriteLoader.cs L24-59)。通道序按小端字节定义:BGRA32 即
/// .NET Color.ToArgb() 的 uint32 在内存里的字节序。
/// Frame pixel formats (SpriteLoader.cs L24-59). The channel order is
/// defined for little-endian bytes: Bgra32 is exactly the in-memory byte
/// order of the uint32 from .NET Color.ToArgb().
enum class SpriteFrameType : std::uint8_t {
  Indexed8,  // 8 位外部调色板索引 | 8-bit index into an external palette
  Bgra32,    // 小端 ARGB(BMP/Color.ToArgb)| little-endian ARGB (BMP/ToArgb)
  Bgr24,     // 无 alpha 的 BGR | BGR without alpha
  Rgba32,    // 大端 ARGB(PNG)| big-endian ARGB (PNG)
  Rgb24,     // 无 alpha 的 RGB | RGB without alpha
};

/// 纹理集类型(SheetBuilder.cs L26-32)。**枚举值 = 通道数,不是任意 ID**
/// (上游注释原文;通道轮换 NextChannel 按该值步进)。
/// Sheet type (SheetBuilder.cs L26-32). **The enum values are the channel
/// counts, not arbitrary IDs** (upstream comment verbatim; channel rotation
/// in NextChannel steps by this value).
enum class SheetType : std::uint8_t {
  Indexed = 1,
  BGRA = 4,
};

class Sheet;  // 前置声明:Sprite 持非拥有引用 | forward: Sprite holds a non-owning reference

/// 精灵(Sprite.cs L18-52;不可变值类型,持有 Sheet 的非拥有指针 ——
/// 引擎内 Sheet 由 SpriteCache/SheetBuilder 拥有,Sprite 生存期嵌于其内)。
/// A sprite (Sprite.cs L18-52; an immutable value type holding a non-owning
/// Sheet pointer — in-engine Sheets are owned by SpriteCache/SheetBuilder,
/// and Sprites never outlive them).
struct Sprite {
  Rectangle Bounds;              // 纹理集内像素矩形 | the pixel rect within the sheet
  Sheet* ptr_sheet = nullptr;    // 所属纹理集(非拥有)| the owning sheet (non-owning)
  BlendMode kind_blend = BlendMode::Alpha;
  TextureChannel kind_channel = TextureChannel::Red;
  float float_z_ramp = 0.0f;
  core::Vector3 vec_size{};      // scale × (w, h, h·zRamp) | scale × (w, h, h·zRamp)
  core::Vector3 vec_offset{};
  // 预计算归一化坐标(含 1/128 inset;上游 readonly 字段直对应)
  // Precomputed normalized coordinates (with the 1/128 inset; the direct
  // counterparts of the upstream readonly fields).
  float float_top = 0.0f, float_left = 0.0f, float_bottom = 0.0f, float_right = 0.0f;

  /// 次纹理引用判别 + 载荷(上游 SpriteWithSecondaryData 以派生类存入
  /// Sprite[] 的数组协变;C++ 按值返回/存 vector 会切片,故字段上提 ——
  /// 第十四批,DefaultSpriteSequence 的深度精灵路径所需)。
  /// The secondary-reference discriminator + payload (upstream stores
  /// SpriteWithSecondaryData into Sprite[] via array covariance; C++
  /// returns/stores Sprites by value and would slice, so the fields move
  /// up — the fourteenth batch, needed by DefaultSpriteSequence's
  /// depth-sprite path).
  bool b_secondary = false;
  Sheet* ptr_secondary_sheet = nullptr;  // 非拥有 | non-owning
  Rectangle SecondaryBounds;
  TextureChannel kind_secondary_channel = TextureChannel::Red;
  float float_secondary_top = 0.0f, float_secondary_left = 0.0f;
  float float_secondary_bottom = 0.0f, float_secondary_right = 0.0f;

  constexpr Sprite() = default;

  // 构造需读 Sheet 尺寸(归一化坐标分母),而 Sheet 返回 Sprite —— 定义
  // 置于 sheet.hpp 末尾(inline),此处仅声明。
  // The constructors read the Sheet size (the normalization denominator)
  // while Sheet returns Sprites — definitions live at the bottom of
  // sheet.hpp (inline); only declarations here.
  Sprite(Sheet& sheet, Rectangle bounds, TextureChannel channel, float scale = 1.0f);
  Sprite(Sheet& sheet, Rectangle bounds, float z_ramp, core::Vector3 offset, TextureChannel channel,
         BlendMode blend_mode = BlendMode::Alpha, float scale = 1.0f);
};

/// 带第二纹理数据的精灵(Sprite.cs L54-72;TS 地形深度通道等场景;
/// 第二归一化坐标**无 inset**)。
/// A sprite with secondary-texture data (Sprite.cs L54-72; the TS terrain
/// depth channel among others; the secondary normalized coordinates carry
/// **no inset**).
struct SpriteWithSecondaryData : Sprite {
  // 载荷已在基类(防切片);本类型保留构造形态与上游类名对照。
  // The payload already sits in the base (against slicing); this type
  // keeps the construction shape and the upstream class-name
  // correspondence.
  SpriteWithSecondaryData(const Sprite& sprite, Sheet& secondary_sheet, Rectangle secondary_bounds,
                          TextureChannel secondary_channel);
};

}  // namespace ora::gfx
