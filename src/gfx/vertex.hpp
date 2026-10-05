// UPSTREAM: OpenRA.Game/Graphics/Vertex.cs @b6fc03f L17-64
// 批渲染顶点(48B 胖顶点,字段序 = 上游 StructLayout(LayoutKind.Sequential)):
//   [x,y,z] 位置;[s,t] 主纹理坐标(或 RGBA 颜色);[u,v] 次纹理坐标;
//   [c] 调色板/通道打包位域(布局注释见 combined.vert,gfx_util::FastCreateQuad);
//   [r,g,b,a] 颜色 tint。
// CombinedShaderBindings 的属性表(顶点格式契约)一并列出,第四批接
// SpriteRenderer 的 VAO 缓存(OPT-A6)时按 (顶点格式, shader) 建 VAO。
// The batch-render vertex (a 48-byte fat vertex; field order = upstream's
// StructLayout(LayoutKind.Sequential)):
//   [x,y,z] position; [s,t] primary texcoords (or an RGBA color); [u,v]
//   secondary texcoords; [c] the packed palette/channel bitfield (layout
//   documented in combined.vert; packed by gfx_util::FastCreateQuad);
//   [r,g,b,a] color tint.
// The CombinedShaderBindings attribute table (the vertex-format contract)
// is listed alongside; the fourth batch wires it into the SpriteRenderer
// VAO cache (OPT-A6) as one VAO per (vertex format, shader).
#pragma once
import std;

#include "platform/gl_types.hpp"

namespace ora::gfx {

/// 48B 批顶点(Vertex.cs L18-49;可平凡拷贝)。
/// The 48-byte batch vertex (Vertex.cs L18-49; trivially copyable).
struct Vertex {
  float x = 0.0f, y = 0.0f, z = 0.0f;             // 3D 位置 | 3D position
  float s = 0.0f, t = 0.0f, u = 0.0f, v = 0.0f;   // 主/次纹理坐标或 RGBA 色 | primary/secondary texcoords or RGBA color
  std::uint32_t c = 0;                            // 调色板与通道标志 | palette & channel flags
  float r = 0.0f, g = 0.0f, b = 0.0f, a = 0.0f;   // 颜色 tint | color tint

  constexpr Vertex() = default;
  constexpr Vertex(float float_x, float float_y, float float_z, float float_s, float float_t, float float_u,
                   float float_v, std::uint32_t uint4_c, float float_r, float float_g, float float_b, float float_a)
      : x{float_x}, y{float_y}, z{float_z}, s{float_s}, t{float_t}, u{float_u}, v{float_v}, c{uint4_c},
        r{float_r}, g{float_g}, b{float_b}, a{float_a} {}
};

static_assert(sizeof(Vertex) == 48, "Vertex 布局须与上游 12×4B 逐字节一致 | must match upstream's 12×4B layout");

/// combined 着色器的顶点属性契约(Vertex.cs L57-63;名字/类型/分量数/偏移)。
/// The vertex-attribute contract of the combined shader (Vertex.cs L57-63;
/// name/type/components/offset).
struct VertexAttributeDesc {
  std::string_view str_name;
  std::uint16_t kind_gl_type;  // GL_FLOAT / GL_UNSIGNED_INT(gl_types 常量)| from gl_types constants
  std::int32_t int4_components;
  std::int32_t int4_offset;
};

inline constexpr std::array<VertexAttributeDesc, 4> kCombinedAttributes{{
    {"aVertexPosition", gl::GL_FLOAT, 3, 0},
    {"aVertexTexCoord", gl::GL_FLOAT, 4, 12},
    {"aVertexAttributes", gl::GL_UNSIGNED_INT, 1, 28},
    {"aVertexTint", gl::GL_FLOAT, 4, 32},
}};

// 布局契约编译期自检(漂移即编译失败)。
// Compile-time layout self-check (any drift fails the build).
static_assert(kCombinedAttributes[3].int4_offset + 4 * 4 == sizeof(Vertex));

}  // namespace ora::gfx
