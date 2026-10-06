// UPSTREAM: OpenRA.Game/Graphics/Util.cs @b6fc03f L23-320(渲染域子集)+
//           OpenRA.Game/Exts.cs @b6fc03f L270-273(NextPowerOf2)+
//           OpenRA.Mods.Common/Util.cs @b6fc03f L79-104(面向索引族,
//           第十四批随 mods 序列移植)+
//           OpenRA.Game/Exts.cs @b6fc03f L77-99(WindingDirectionTest/
//           PolygonContains,第十四批随 Viewport 移植)
// 渲染域工具:四边形索引表、FastCreateQuad(顶点生成 + combined.vert 的
// aVertexAttributes 位域打包)、FastCopyIntoChannel(Indexed8→单通道 /
// Bgr[a]/Rgb[a]→RGBA 预乘拷贝)、FastCopyIntoSprite(Png→sheet 的预乘
// 展开,ChromeProvider 的 Sheet(Stream) 路径,随第十三批)、
// PremultiplyAlpha(uint 快速整数预乘)、RotateQuadInto(绕中心 x-y 旋转)、
// BoundingRectangle、NextPowerOf2、IndexFacing/AngleDiffToStep/
// GetInterpolatedFacingRotation(Mods.Common 的面向量化索引)、Lerp、
// PolygonContains(回绕数点在多边形内测试)。
// 同时给出 core::Vector2/Vector3 的渲染运算(vector_n.hpp 注记的
// "渲染运算 Phase 4 随 gfx 扩展" 落地点)。
// Not ported here: RotateQuad(数组分配版)、PremultipliedColorLerp/FromAngle
// (随 WorldRenderer 批次)。
// Rendering-domain utilities: the quad-index table, FastCreateQuad (vertex
// generation with the aVertexAttributes bitfield packing documented in
// combined.vert), FastCopyIntoChannel (Indexed8 → single channel /
// Bgr[a]/Rgb[a] → premultiplied RGBA copies), FastCopyIntoSprite (the
// Png→sheet premultiplied expansion of ChromeProvider's Sheet(Stream) path,
// landed with the thirteenth batch), PremultiplyAlpha (the fast integer
// uint premultiply), RotateQuadInto (a rotation about the center in the x-y
// plane), BoundingRectangle, NextPowerOf2, the IndexFacing/
// AngleDiffToStep/GetInterpolatedFacingRotation family (the facing-quantized
// indices of Mods.Common), Lerp, and PolygonContains (the winding-number
// point-in-polygon test). Also extends core::Vector2/Vector3 with the
// rendering arithmetic (the "rendering math extends with gfx in Phase 4"
// note in vector_n.hpp lands here).
// Not ported here: RotateQuad (the array-allocating overload) and
// PremultipliedColorLerp/FromAngle (the WorldRenderer batch).
#pragma once
import std;

#include "core/color.hpp"
#include "core/int2.hpp"
#include "core/rectangle.hpp"
#include "core/vector_n.hpp"
#include "core/wangle.hpp"
#include "formats/png.hpp"
#include "gfx/sprite.hpp"
#include "gfx/vertex.hpp"

namespace ora::gfx {

// ———— core::Vector2/Vector3 渲染运算(Upstream = System.Numerics 语义)————
// ———— Rendering arithmetic for core::Vector2/Vector3 (Upstream = System.Numerics semantics) ————

constexpr core::Vector2 operator+(core::Vector2 a, core::Vector2 b) { return {a.X + b.X, a.Y + b.Y}; }
constexpr core::Vector2 operator-(core::Vector2 a, core::Vector2 b) { return {a.X - b.X, a.Y - b.Y}; }
constexpr core::Vector2 operator*(core::Vector2 v, float k) { return {v.X * k, v.Y * k}; }
constexpr core::Vector2 operator*(float k, core::Vector2 v) { return v * k; }
constexpr core::Vector2 operator-(core::Vector2 v) { return {-v.X, -v.Y}; }

constexpr core::Vector3 operator+(core::Vector3 a, core::Vector3 b) { return {a.X + b.X, a.Y + b.Y, a.Z + b.Z}; }
constexpr core::Vector3 operator-(core::Vector3 a, core::Vector3 b) { return {a.X - b.X, a.Y - b.Y, a.Z - b.Z}; }
constexpr core::Vector3 operator*(core::Vector3 v, float k) { return {v.X * k, v.Y * k, v.Z * k}; }
constexpr core::Vector3 operator*(float k, core::Vector3 v) { return v * k; }
constexpr core::Vector3 operator*(core::Vector3 a, core::Vector3 b) { return {a.X * b.X, a.Y * b.Y, a.Z * b.Z}; }  // System.Numerics 分量乘 | componentwise multiply
constexpr core::Vector3 operator/(core::Vector3 v, float k) { return v * (1.0f / k); }
constexpr core::Vector3 operator-(core::Vector3 v) { return {-v.X, -v.Y, -v.Z}; }

/// System.Numerics.Vector2.Length(float 路径)。
/// System.Numerics.Vector2.Length (the float path).
inline float Length(core::Vector2 v) { return std::sqrt(v.X * v.X + v.Y * v.Y); }

// ———— Util.cs ————

/// 四边形索引表(Util.cs L26-43):每四边形 0,1,2 / 2,3,0,顶点基址步进 4。
/// The quad-index table (Util.cs L26-43): 0,1,2 / 2,3,0 per quad, the vertex
/// base advancing by 4.
std::vector<std::uint32_t> CreateQuadIndices(std::int32_t int4_quads);

/// aVertexAttributes 位域打包(combined.vert 注释;Util.cs L70-90 逐行):
///   bits 0-2   主通道(000=不用 / 010=RGBA / 001,011,101,111=R,G,B,A 索引)
///   bits 3-5   次通道(深度精灵)
///   bits 6-8   主纹理 sampler 序号(0-7)
///   bits 9-11  次纹理 sampler 序号
///   bits 16-31 调色板行
/// The aVertexAttributes bitfield packing (documented in combined.vert;
/// Util.cs L70-90 line by line): see the bit table above.
/// 四角版:直接按 a/b/c/d 四顶点写 UV(L65-96 逐行)。
/// The four-corner form: writes UVs straight from a/b/c/d (Util.cs L65-96).
void FastCreateQuad(std::span<Vertex> vec_vertices, core::Vector3 v_a, core::Vector3 v_b, core::Vector3 v_c,
                    core::Vector3 v_d, const Sprite& sprite_r, int2 int2_samplers, std::int32_t int4_palette_index,
                    core::Vector3 v_tint, float float_alpha, std::int32_t int4_nv);

/// 轴对齐版(Util.cs L45-63):rotation != 0 时先绕中心旋转四角。
/// The axis-aligned form (Util.cs L45-63): rotates the corners about the
/// center first when rotation != 0.
void FastCreateQuad(std::span<Vertex> vec_vertices, core::Vector3 v_o, const Sprite& sprite_r, int2 int2_samplers,
                    std::int32_t int4_palette_index, std::int32_t int4_nv, core::Vector3 v_size, core::Vector3 v_tint,
                    float float_alpha, float float_rotation = 0.0f);

/// 拷贝帧数据进精灵纹理通道(Util.cs L98-130):RGBA 精灵写四通道(源
/// Bgra32 走逐行 memcpy 快路径),索引精灵按 ChannelMasks(2,1,0,3 ——
/// "yes, our channel order is nuts")写入 BGRA 字节序的单个字节平面。
/// Copies frame data into the sprite's texture channel (Util.cs L98-130):
/// RGBA sprites fill all four channels (Bgra32 sources take the per-row
/// memcpy fast path); indexed sprites write one byte plane of the BGRA-byte
/// buffer, selected via ChannelMasks (2,1,0,3 — "yes, our channel order is
/// nuts").
void FastCopyIntoChannel(const Sprite& sprite_dest, std::span<const std::byte> vec_src, SpriteFrameType kind_src_type,
                         bool b_premultiplied = false);

/// Png → sheet 的预乘展开拷贝(Util.cs L203-241):Indexed8 查 Png 自带调色
/// 板,Rgb[a] 逐通道组装,统一 PremultiplyAlpha 后写 uint32。PNG 不支持
/// BGR[A](上游 default 抛逐字)。
/// The Png → sheet premultiplied copy (Util.cs L203-241): Indexed8 indexes
/// Png's own palette, Rgb[a] assembles channels, and everything writes the
/// uint32 after PremultiplyAlpha. PNGs carry no BGR[A] (upstream's default
/// throw verbatim).
void FastCopyIntoSprite(const Sprite& sprite_dest, const fmt::Png& png_src);

/// 绕中心在 x-y 平面旋转四边形(Util.cs L265-287;des 至少 4 元素;
/// Matrix3x2.CreateRotation(-rotation) 的 2D 变换逐语义复刻)。
/// Rotates a quad about its center in the x-y plane (Util.cs L265-287; des
/// holds at least 4 elements; the 2D transform of
/// Matrix3x2.CreateRotation(-rotation) reproduced semantically).
void RotateQuadInto(std::span<core::Vector3> vec_des, core::Vector3 v_tl, core::Vector3 v_size, float float_rotation);

/// 对象屏幕包围盒(Util.cs L295-320;(int) 全部向零截断 = C# 语义)。
/// The on-screen bounding rectangle of an object (Util.cs L295-320; every
/// (int) truncates towards zero, the C# semantics).
Rectangle BoundingRectangle(core::Vector3 v_offset, core::Vector3 v_size, float float_rotation);

/// Exts.NextPowerOf2(Exts.cs L270-273;BitOperations.RoundUpToPowerOf2)。
/// v <= 1 时收敛到 1(上游 uint 下溢域之外的安全化)。
/// Exts.NextPowerOf2 (Exts.cs L270-273; BitOperations.RoundUpToPowerOf2).
/// Collapses to 1 for v <= 1 (a safety net outside upstream's uint domain).
constexpr std::int32_t NextPowerOf2(std::int32_t int4_v) {
  if (int4_v <= 1)
    return 1;
  std::uint32_t u = static_cast<std::uint32_t>(int4_v - 1);
  u |= u >> 1;
  u |= u >> 2;
  u |= u >> 4;
  u |= u >> 8;
  u |= u >> 16;
  return static_cast<std::int32_t>(u + 1);
}

/// Exts.IsPowerOf2(Exts.cs L277-280;BitOperations.IsPow2;v>0 且单一位)。
/// Exts.IsPowerOf2 (Exts.cs L277-280; BitOperations.IsPow2; v>0 with a
/// single bit set).
constexpr bool IsPowerOf2(std::int32_t int4_v) { return int4_v > 0 && (int4_v & (int4_v - 1)) == 0; }

/// Util.Lerp(Util.cs L368):a + t*(b-a)。
/// Util.Lerp (Util.cs L368): a + t*(b-a).
constexpr float Lerp(float fp4_a, float fp4_b, float fp4_t) { return fp4_a + fp4_t * (fp4_b - fp4_a); }

/// 面向 → 帧索引(Mods.Common/Util.cs L79-87):step = 1024/numFrames,
/// (angle + step/2) & 1023 后整除。
/// Facing → the frame index (Mods.Common/Util.cs L79-87): step =
/// 1024/numFrames, then (angle + step/2) & 1023 divided by step.
constexpr std::int32_t IndexFacing(WAngle wangle_facing, std::int32_t int4_num_frames) {
  const std::int32_t int4_step = 1024 / int4_num_frames;
  const std::int32_t int4_a = (wangle_facing.Angle + int4_step / 2) & 1023;
  return int4_a / int4_step;
}

/// 最近整步面向的余角(Mods.Common/Util.cs L90-96)。
/// The remainder angle after rounding to the nearest whole step (Mods.Common/
/// Util.cs L90-96).
constexpr WAngle AngleDiffToStep(WAngle wangle_facing, std::int32_t int4_num_frames) {
  const std::int32_t int4_step = 1024 / int4_num_frames;
  const std::int32_t int4_a = (wangle_facing.Angle + int4_step / 2) & 1023;
  return WAngle{int4_a % int4_step - int4_step / 2};
}

/// 最近插值面向相对最近帧面向的旋转角(Mods.Common/Util.cs L98-102)。
/// The angle the closest facing sprite should rotate by to reach the closest
/// interpolated facing (Mods.Common/Util.cs L98-102).
constexpr WAngle GetInterpolatedFacingRotation(WAngle wangle_facing, std::int32_t int4_facings,
                                               std::int32_t int4_interpolated_facings) {
  const std::int32_t int4_step = 1024 / int4_interpolated_facings;
  return WAngle{AngleDiffToStep(wangle_facing, int4_facings).Angle / int4_step * int4_step};
}

/// 回绕数点在多边形内测试(Exts.cs L77-99;WindingDirectionTest 的符号判
/// 定逐行)。span 至少 1 元素(上游 ImmutableArray 空时恒假 —— 循环零次,
/// windingNumber 0)。
/// The winding-number point-in-polygon test (Exts.cs L77-99;
/// WindingDirectionTest's sign checks line by line). The span needs at least
/// one element (an empty upstream ImmutableArray stays false — zero loop
/// iterations, windingNumber 0).
constexpr bool PolygonContains(std::span<const int2> vec_polygon, int2 int2_p) {
  const auto winding_test = [](int2 v0, int2 v1, int2 p) {
    return int2::SignOf((v1.X - v0.X) * (p.Y - v0.Y) - (p.X - v0.X) * (v1.Y - v0.Y));
  };

  auto int4_winding_number = 0;
  for (std::size_t int4_i{}; int4_i < vec_polygon.size(); int4_i++) {
    const int2 tv = vec_polygon[int4_i];
    const int2 nv = vec_polygon[(int4_i + 1) % vec_polygon.size()];

    if (tv.Y <= int2_p.Y && nv.Y > int2_p.Y && winding_test(tv, nv, int2_p) > 0)
      int4_winding_number++;
    else if (tv.Y > int2_p.Y && nv.Y <= int2_p.Y && winding_test(tv, nv, int2_p) < 0)
      int4_winding_number--;
  }

  return int4_winding_number != 0;
}

/// 预乘 alpha(Util.cs L322-354;**uint32 语义** —— 上游自研 Color.ToArgb()
/// 返回 uint,>> 24 是逻辑右移,a==255 命中快路径)。(x + (x >> 8)) >> 8
/// 技巧在 0-255 域位精确。热路径:头文件 inline。
/// Premultiplied alpha (Util.cs L322-354; **uint32 semantics** — upstream's
/// own Color.ToArgb() returns uint, so >> 24 is a logical shift and a==255
/// hits the fast path). The (x + (x >> 8)) >> 8 trick is bit-perfect over
/// 0-255. Hot path: inline in the header.
inline core::Color PremultiplyAlpha(core::Color color_c) {
  const std::uint32_t uint4_argb = color_c.ToArgb();
  const std::uint32_t uint4_a = uint4_argb >> 24;

  // Fully opaque.
  if (uint4_a == 255)
    return color_c;

  // Fully transparent.
  if (uint4_a == 0)
    return core::Color{};

  // Extract channels.
  std::uint32_t uint4_r = (uint4_argb >> 16) & 0xFF;
  std::uint32_t uint4_g = (uint4_argb >> 8) & 0xFF;
  std::uint32_t uint4_b = uint4_argb & 0xFF;

  // Fast integer premultiply: (c * a) / 255
  uint4_r = uint4_r * uint4_a + 128;
  uint4_r = (uint4_r + (uint4_r >> 8)) >> 8;
  uint4_g = uint4_g * uint4_a + 128;
  uint4_g = (uint4_g + (uint4_g >> 8)) >> 8;
  uint4_b = uint4_b * uint4_a + 128;
  uint4_b = (uint4_b + (uint4_b >> 8)) >> 8;

  return core::Color::FromArgbRaw((uint4_a << 24) | (uint4_r << 16) | (uint4_g << 8) | uint4_b);
}

}  // namespace ora::gfx
