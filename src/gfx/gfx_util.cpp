// UPSTREAM: OpenRA.Game/Graphics/Util.cs @b6fc03f L23-320(渲染域子集;头文件已注记范围)
// The rendering-domain subset of Util.cs (scope noted in the header).
#include "gfx/gfx_util.hpp"

#include <cassert>

#include "gfx/sheet.hpp"  // FastCopyIntoChannel 经 Sprite->ptr_sheet 访问 Sheet | FastCopyIntoChannel reaches the Sheet via Sprite->ptr_sheet

namespace ora::gfx {

std::vector<std::uint32_t> CreateQuadIndices(std::int32_t int4_quads) {
  std::vector<std::uint32_t> vec_indices(static_cast<std::size_t>(int4_quads) * 6);
  std::uint32_t uint4_vertex_offset = 0;
  for (std::size_t i = 0; i < vec_indices.size(); i += 6) {
    vec_indices[i] = uint4_vertex_offset;
    vec_indices[i + 1] = uint4_vertex_offset + 1;
    vec_indices[i + 2] = uint4_vertex_offset + 2;
    vec_indices[i + 3] = uint4_vertex_offset + 2;
    vec_indices[i + 4] = uint4_vertex_offset + 3;
    vec_indices[i + 5] = uint4_vertex_offset;
    uint4_vertex_offset += 4;
  }
  return vec_indices;
}

void FastCreateQuad(std::span<Vertex> vec_vertices, core::Vector3 v_a, core::Vector3 v_b, core::Vector3 v_c,
                    core::Vector3 v_d, const Sprite& sprite_r, int2 int2_samplers,
                    std::int32_t int4_palette_index, core::Vector3 v_tint, float float_alpha,
                    std::int32_t int4_nv) {
  assert(static_cast<std::size_t>(int4_nv) + 4 <= vec_vertices.size());

  float float_sl = 0.0f, float_st = 0.0f, float_sr = 0.0f, float_sb = 0.0f;

  // See combined.vert for documentation on the channel attribute format
  // (Util.cs L75-76)。RGBA → 0x02(全通道采样);索引通道 → 通道号 << 1 | 0x01。
  // (Util.cs L75-76). RGBA packs 0x02 (sample-all-channels); an index
  // channel packs channel << 1 | 0x01.
  std::uint32_t uint4_attrib_c =
      sprite_r.kind_channel == TextureChannel::RGBA
          ? 0x02
          : (static_cast<std::uint32_t>(sprite_r.kind_channel) << 1) | 0x01;
  uint4_attrib_c |= static_cast<std::uint32_t>(int2_samplers.X) << 6;
  if (sprite_r.b_secondary) {
    const auto& sprite_ss = static_cast<const SpriteWithSecondaryData&>(sprite_r);
    float_sl = sprite_ss.float_secondary_left;
    float_st = sprite_ss.float_secondary_top;
    float_sr = sprite_ss.float_secondary_right;
    float_sb = sprite_ss.float_secondary_bottom;

    uint4_attrib_c |= (static_cast<std::uint32_t>(sprite_ss.kind_secondary_channel) << 4) | 0x08;
    uint4_attrib_c |= static_cast<std::uint32_t>(int2_samplers.Y) << 9;
  }

  uint4_attrib_c |= (static_cast<std::uint32_t>(int4_palette_index) & 0xFFFF) << 16;

  vec_vertices[int4_nv] = {v_a.X, v_a.Y, v_a.Z, sprite_r.float_left, sprite_r.float_top,
                           float_sl, float_st, uint4_attrib_c, v_tint.X, v_tint.Y, v_tint.Z, float_alpha};
  vec_vertices[int4_nv + 1] = {v_b.X, v_b.Y, v_b.Z, sprite_r.float_right, sprite_r.float_top,
                               float_sr, float_st, uint4_attrib_c, v_tint.X, v_tint.Y, v_tint.Z, float_alpha};
  vec_vertices[int4_nv + 2] = {v_c.X, v_c.Y, v_c.Z, sprite_r.float_right, sprite_r.float_bottom,
                               float_sr, float_sb, uint4_attrib_c, v_tint.X, v_tint.Y, v_tint.Z, float_alpha};
  vec_vertices[int4_nv + 3] = {v_d.X, v_d.Y, v_d.Z, sprite_r.float_left, sprite_r.float_bottom,
                               float_sl, float_sb, uint4_attrib_c, v_tint.X, v_tint.Y, v_tint.Z, float_alpha};
}

void FastCreateQuad(std::span<Vertex> vec_vertices, core::Vector3 v_o, const Sprite& sprite_r, int2 int2_samplers,
                    std::int32_t int4_palette_index, std::int32_t int4_nv, core::Vector3 v_size, core::Vector3 v_tint,
                    float float_alpha, float float_rotation) {
  // Rotate sprite if rotation angle is not equal to 0(Util.cs L48-62)
  if (float_rotation != 0.0f) {
    core::Vector3 vec_rot[4];
    RotateQuadInto(vec_rot, v_o, v_size, float_rotation);
    FastCreateQuad(vec_vertices, vec_rot[0], vec_rot[1], vec_rot[2], vec_rot[3], sprite_r, int2_samplers,
                   int4_palette_index, v_tint, float_alpha, int4_nv);
  } else {
    const core::Vector3 v_b{v_o.X + v_size.X, v_o.Y, v_o.Z};
    const core::Vector3 v_c{v_o.X + v_size.X, v_o.Y + v_size.Y, v_o.Z + v_size.Z};
    const core::Vector3 v_d{v_o.X, v_o.Y + v_size.Y, v_o.Z + v_size.Z};
    FastCreateQuad(vec_vertices, v_o, v_b, v_c, v_d, sprite_r, int2_samplers, int4_palette_index, v_tint,
                   float_alpha, int4_nv);
  }
}

/// CopyIntoRgba(Util.cs L132-201):RGBA 目标路径。Bgra32 源(与目标格式一致)
/// 走逐行 memcpy 快路径;非 premultiplied 时逐像素预乘(原地覆盖)。
/// CopyIntoRgba (Util.cs L132-201): the RGBA-destination path. Bgra32
/// sources (matching the destination format) take the per-row memcpy fast
/// path; non-premultiplied data is premultiplied per pixel in place.
namespace {
void CopyIntoRgba(std::span<const std::byte> vec_src, SpriteFrameType kind_src_type, bool b_premultiplied,
                  std::span<std::byte> vec_dest, std::int32_t int4_x, std::int32_t int4_y, std::int32_t int4_width,
                  std::int32_t int4_height, std::int32_t int4_stride) {
  auto* ptr_d = reinterpret_cast<std::uint32_t*>(vec_dest.data());  // 全局 -fno-strict-aliasing | global -fno-strict-aliasing
  const auto bytes_per_px = kind_src_type == SpriteFrameType::Bgra32 || kind_src_type == SpriteFrameType::Rgba32
                                ? 4
                                : 3;
  assert(vec_src.size() >= static_cast<std::size_t>(int4_width) * int4_height * bytes_per_px);
  assert(vec_dest.size() >= (static_cast<std::size_t>(int4_y) + int4_height) *
                                static_cast<std::size_t>(int4_stride) * 4);
  std::size_t size_si = 0;
  std::size_t size_di = static_cast<std::size_t>(int4_y) * int4_stride + int4_x;

  if (kind_src_type == SpriteFrameType::Bgra32) {
    const auto* ptr_s = reinterpret_cast<const std::uint32_t*>(vec_src.data());
    for (auto h = 0; h < int4_height; ++h) {
      std::memcpy(&ptr_d[size_di], &ptr_s[size_si], static_cast<std::size_t>(int4_width) * 4);
      if (!b_premultiplied) {
        for (auto w = 0; w < int4_width; ++w) {
          ptr_d[size_di] = PremultiplyAlpha(core::Color::FromArgb(ptr_d[size_di])).ToArgb();
          ++size_di;
        }
        size_di -= static_cast<std::size_t>(int4_width);
      }
      size_si += int4_width;
      size_di += int4_stride;
    }
    return;
  }

  for (auto h = 0; h < int4_height; ++h) {
    for (auto w = 0; w < int4_width; ++w) {
      std::uint8_t u8_r, u8_g, u8_b, u8_a;
      switch (kind_src_type) {
        case SpriteFrameType::Bgra32:
        case SpriteFrameType::Bgr24:
          u8_b = std::to_integer<std::uint8_t>(vec_src[size_si++]);
          u8_g = std::to_integer<std::uint8_t>(vec_src[size_si++]);
          u8_r = std::to_integer<std::uint8_t>(vec_src[size_si++]);
          u8_a = kind_src_type == SpriteFrameType::Bgra32 ? std::to_integer<std::uint8_t>(vec_src[size_si++])
                                                          : std::uint8_t{255};
          break;
        case SpriteFrameType::Rgba32:
        case SpriteFrameType::Rgb24:
          u8_r = std::to_integer<std::uint8_t>(vec_src[size_si++]);
          u8_g = std::to_integer<std::uint8_t>(vec_src[size_si++]);
          u8_b = std::to_integer<std::uint8_t>(vec_src[size_si++]);
          u8_a = kind_src_type == SpriteFrameType::Rgba32 ? std::to_integer<std::uint8_t>(vec_src[size_si++])
                                                          : std::uint8_t{255};
          break;
        default:
          throw std::runtime_error("Unknown SpriteFrameType " +
                                   std::to_string(static_cast<int>(kind_src_type)));
      }

      auto color_c = core::Color::FromArgb(u8_a, u8_r, u8_g, u8_b);
      if (!b_premultiplied)
        color_c = PremultiplyAlpha(color_c);
      ptr_d[size_di++] = color_c.ToArgb();
    }
    size_di += static_cast<std::size_t>(int4_stride) - int4_width;
  }
}
}  // namespace

void FastCopyIntoChannel(const Sprite& sprite_dest, std::span<const std::byte> vec_src, SpriteFrameType kind_src_type,
                         bool b_premultiplied) {
  auto vec_dest_data = sprite_dest.ptr_sheet->GetData();
  const auto int4_stride = sprite_dest.ptr_sheet->Size().X;
  const auto int4_x = sprite_dest.Bounds.Left();
  const auto int4_y = sprite_dest.Bounds.Top();
  const auto int4_width = sprite_dest.Bounds.Width;
  const auto int4_height = sprite_dest.Bounds.Height;

  if (sprite_dest.kind_channel == TextureChannel::RGBA) {
    CopyIntoRgba(vec_src, kind_src_type, b_premultiplied, vec_dest_data, int4_x, int4_y, int4_width, int4_height,
                 int4_stride);
  } else {
    // yes, our channel order is nuts.(Util.cs L24)
    constexpr std::int32_t kChannelMasks[4] = {2, 1, 0, 3};
    const auto int4_dest_stride = int4_stride * 4;
    auto int4_dest_offset =
        int4_dest_stride * int4_y + int4_x * 4 + kChannelMasks[static_cast<std::int32_t>(sprite_dest.kind_channel)];
    const auto int4_dest_skip = int4_dest_stride - 4 * int4_width;

    assert(vec_src.size() >= static_cast<std::size_t>(int4_width) * int4_height);
    std::size_t size_src_offset = 0;
    for (auto j = 0; j < int4_height; j++) {
      for (auto i = 0; i < int4_width; i++, ++size_src_offset) {
        vec_dest_data[int4_dest_offset] = vec_src[size_src_offset];
        int4_dest_offset += 4;
      }
      int4_dest_offset += int4_dest_skip;
    }
  }
}

void RotateQuadInto(std::span<core::Vector3> vec_des, core::Vector3 v_tl, core::Vector3 v_size, float float_rotation) {
  assert(vec_des.size() >= 4);
  const core::Vector3 v_center = v_tl + 0.5f * v_size;

  // rotMatrix = Matrix3x2.CreateRotation(-rotation);m11=cos, m12=sin,
  // m21=-sin, m22=cos;Vector2.Transform(v, m) = (v.X·m11 + v.Y·m21, v.X·m12 + v.Y·m22)。
  // rotMatrix = Matrix3x2.CreateRotation(-rotation); m11=cos, m12=sin,
  // m21=-sin, m22=cos; Vector2.Transform(v, m) = (v.X·m11 + v.Y·m21, v.X·m12 + v.Y·m22).
  const float float_rad = -float_rotation;
  const float float_cos = std::cos(float_rad);
  const float float_sin = std::sin(float_rad);
  const auto Transform2 = [float_cos, float_sin](core::Vector2 v) {
    return core::Vector2{v.X * float_cos - v.Y * float_sin, v.X * float_sin + v.Y * float_cos};
  };

  const float float_half_x = v_size.X * 0.5f;
  const float float_half_y = v_size.Y * 0.5f;
  const float float_z_scale = v_size.Y != 0.0f ? v_size.Z / v_size.Y : 0.0f;

  const auto v_ra2d = Transform2({float_half_x, float_half_y});
  const auto v_rb2d = Transform2({float_half_x, -float_half_y});

  // Rotated offset for +/- x with +/- y | with -/+ y(Util.cs L277-281)
  const core::Vector3 v_ra{v_ra2d.X, v_ra2d.Y, v_ra2d.Y * float_z_scale};
  const core::Vector3 v_rb{v_rb2d.X, v_rb2d.Y, v_rb2d.Y * float_z_scale};

  vec_des[0] = v_center - v_ra;
  vec_des[1] = v_center + v_rb;
  vec_des[2] = v_center + v_ra;
  vec_des[3] = v_center - v_rb;
}

Rectangle BoundingRectangle(core::Vector3 v_offset, core::Vector3 v_size, float float_rotation) {
  if (float_rotation == 0.0f)
    return Rectangle{static_cast<std::int32_t>(v_offset.X), static_cast<std::int32_t>(v_offset.Y),
                           static_cast<std::int32_t>(v_size.X), static_cast<std::int32_t>(v_size.Y)};

  core::Vector3 vec_rotated_quad[4];
  RotateQuadInto(vec_rotated_quad, v_offset, v_size, float_rotation);

  float float_min_x = vec_rotated_quad[0].X;
  float float_max_x = vec_rotated_quad[0].X;
  float float_min_y = vec_rotated_quad[0].Y;
  float float_max_y = vec_rotated_quad[0].Y;
  for (auto i = 1; i < 4; ++i) {
    float_min_x = std::min(vec_rotated_quad[i].X, float_min_x);
    float_max_x = std::max(vec_rotated_quad[i].X, float_max_x);
    float_min_y = std::min(vec_rotated_quad[i].Y, float_min_y);
    float_max_y = std::max(vec_rotated_quad[i].Y, float_max_y);
  }

  // C#:(int) 向零截断;Math.Ceiling(double) 后转 int(整值,无方向歧义)。
  // C#: (int) truncates towards zero; Math.Ceiling(double) then (int) (an
  // integral value — no direction ambiguity).
  const auto int4_min_x = static_cast<std::int32_t>(float_min_x);
  const auto int4_min_y = static_cast<std::int32_t>(float_min_y);
  return Rectangle{int4_min_x, int4_min_y,
                         static_cast<std::int32_t>(std::ceil(static_cast<double>(float_max_x))) - int4_min_x,
                         static_cast<std::int32_t>(std::ceil(static_cast<double>(float_max_y))) - int4_min_y};
}

void FastCopyIntoSprite(const Sprite& sprite_dest, const fmt::Png& png_src) {
  std::span<std::byte> vec_dest_data = sprite_dest.ptr_sheet->GetData();
  const std::int32_t int4_stride = sprite_dest.ptr_sheet->Size().X;
  const std::int32_t int4_x = sprite_dest.Bounds.Left();
  const std::int32_t int4_y = sprite_dest.Bounds.Top();
  const std::int32_t int4_width = sprite_dest.Bounds.Width;
  const std::int32_t int4_height = sprite_dest.Bounds.Height;

  std::size_t st_si = 0;
  std::size_t st_di = static_cast<std::size_t>(int4_y) * static_cast<std::size_t>(int4_stride) +
                      static_cast<std::size_t>(int4_x);

  const auto store_u32 = [&](std::size_t st_index, std::uint32_t uint4_v) {
    vec_dest_data[st_index] = static_cast<std::byte>(uint4_v & 0xFF);
    vec_dest_data[st_index + 1] = static_cast<std::byte>((uint4_v >> 8) & 0xFF);
    vec_dest_data[st_index + 2] = static_cast<std::byte>((uint4_v >> 16) & 0xFF);
    vec_dest_data[st_index + 3] = static_cast<std::byte>((uint4_v >> 24) & 0xFF);
  };

  for (std::int32_t int4_h{}; int4_h < int4_height; int4_h++) {
    for (std::int32_t int4_w{}; int4_w < int4_width; int4_w++) {
      core::Color color_c;
      switch (png_src.Type()) {
        case SpriteFrameType::Indexed8: {
          const std::uint8_t uint1_index =
              static_cast<std::uint8_t>(png_src.Data()[st_si++]);
          const fmt::PngColor& color_pal =
              (*png_src.Palette())[uint1_index];  // 上游:调色板必在 | upstream: the palette must exist
          color_c = core::Color::FromArgb(color_pal.uint1_a, color_pal.uint1_r, color_pal.uint1_g,
                                          color_pal.uint1_b);
          break;
        }
        case SpriteFrameType::Rgba32:
        case SpriteFrameType::Rgb24: {
          const auto uint1_r = static_cast<std::uint8_t>(png_src.Data()[st_si++]);
          const auto uint1_g = static_cast<std::uint8_t>(png_src.Data()[st_si++]);
          const auto uint1_b = static_cast<std::uint8_t>(png_src.Data()[st_si++]);
          const auto uint1_a = png_src.Type() == SpriteFrameType::Rgba32
                                   ? static_cast<std::uint8_t>(png_src.Data()[st_si++])
                                   : std::uint8_t{255};
          color_c = core::Color::FromArgb(uint1_a, uint1_r, uint1_g, uint1_b);
          break;
        }

        // PNG does not support BGR[A], so no need to include them here
        // (Util.cs L234-235 逐字)
        // PNG does not support BGR[A], so no need to include them here
        // (Util.cs L234-235 verbatim).
        default:
          throw std::runtime_error(std::format("Unknown SpriteFrameType {}",
                                               static_cast<int>(png_src.Type())));
      }

      store_u32(st_di++ * 4, PremultiplyAlpha(color_c).ToArgb());
    }

    st_di += static_cast<std::size_t>(int4_stride - int4_width);
  }
}

}  // namespace ora::gfx
