#pragma once
import std;

#include "platform/gl_types.hpp"

namespace ora::gfx {

struct Vertex {
  float x = 0.0f, y = 0.0f, z = 0.0f;
  float s = 0.0f, t = 0.0f, u = 0.0f, v = 0.0f;
  std::uint32_t c = 0;
  float r = 0.0f, g = 0.0f, b = 0.0f, a = 0.0f;

  constexpr Vertex() = default;
  constexpr Vertex(float float_x, float float_y, float float_z, float float_s, float float_t, float float_u,
                   float float_v, std::uint32_t uint4_c, float float_r, float float_g, float float_b, float float_a)
      : x{float_x}, y{float_y}, z{float_z}, s{float_s}, t{float_t}, u{float_u}, v{float_v}, c{uint4_c},
        r{float_r}, g{float_g}, b{float_b}, a{float_a} {}
};

static_assert(sizeof(Vertex) == 48, "Vertex 布局须与上游 12×4B 逐字节一致 | must match upstream's 12×4B layout");

struct VertexAttributeDesc {
  std::string_view str_name;
  std::uint16_t kind_gl_type;
  std::int32_t int4_components;
  std::int32_t int4_offset;
};

inline constexpr std::array<VertexAttributeDesc, 4> kCombinedAttributes{{
    {"aVertexPosition", gl::GL_FLOAT, 3, 0},
    {"aVertexTexCoord", gl::GL_FLOAT, 4, 12},
    {"aVertexAttributes", gl::GL_UNSIGNED_INT, 1, 28},
    {"aVertexTint", gl::GL_FLOAT, 4, 32},
}};

static_assert(kCombinedAttributes[3].int4_offset + 4 * 4 == sizeof(Vertex));

}
