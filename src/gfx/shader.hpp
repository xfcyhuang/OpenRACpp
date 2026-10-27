#pragma once
import std;

#include "gfx/render_thread.hpp"
#include "gfx/texture.hpp"
#include "platform/gl_types.hpp"

namespace ora::gfx {

enum class ShaderVertexAttributeType : std::uint16_t {
  Float = 0x1406,
  Int = 0x1404,
  UInt = 0x1405,
};

struct ShaderVertexAttribute {
  std::string str_name;
  ShaderVertexAttributeType kind_type = ShaderVertexAttributeType::Float;
  std::int32_t int4_components = 0;
  std::int32_t int4_offset = 0;
};

struct ShaderBindingsDesc {
  std::string_view str_vertex_shader_name;
  std::string_view str_vertex_shader_code;
  std::string_view str_fragment_shader_name;
  std::string_view str_fragment_shader_code;
  std::int32_t int4_stride = 0;
  std::span<const ShaderVertexAttribute> vec_attributes;
};

class Shader {
 public:

  static std::optional<Shader> Create(RenderThread& render, const ShaderBindingsDesc& desc);

  ~Shader();
  Shader(const Shader&) = delete;
  Shader& operator=(const Shader&) = delete;
  Shader(Shader&& other) noexcept;
  Shader& operator=(Shader&&) = delete;

  void Bind();

  void PrepareRender();

  void SetTexture(std::string_view str_name, const Texture& texture);

  std::int32_t Location(std::string_view str_name) const;

  void SetBool(std::string_view str_name, bool b_value);
  void SetVec(std::string_view str_name, float float_x);
  void SetVec(std::string_view str_name, float float_x, float float_y);
  void SetVec(std::string_view str_name, float float_x, float float_y, float float_z);
  void SetVec(std::string_view str_name, std::span<const float> vec_values, std::int32_t int4_length);
  void SetMatrix(std::string_view str_name, std::span<const float> vec_matrix16);

  void SetBoolAt(std::int32_t int4_location, bool b_value);
  void SetVecAt(std::int32_t int4_location, float float_x);
  void SetVecAt(std::int32_t int4_location, float float_x, float float_y);
  void SetVecAt(std::int32_t int4_location, float float_x, float float_y, float float_z);
  void SetVecAt(std::int32_t int4_location, std::span<const float> vec_values, std::int32_t int4_length);
  void SetMatrixAt(std::int32_t int4_location, std::span<const float> vec_matrix16);

  std::uint32_t GlId() const { return uint4_program_; }

 private:
  Shader() = default;
  void PostUseProgram();

  RenderThread* ptr_render_ = nullptr;
  std::uint32_t uint4_program_ = 0;
  std::vector<ShaderVertexAttribute> vec_attributes_;
  std::int32_t int4_stride_ = 0;
  std::map<std::string, std::int32_t, std::less<>> map_uniform_cache_;
  std::map<std::string, std::uint32_t, std::less<>> map_samplers_;
  std::map<std::uint32_t, std::uint32_t> map_bound_textures_;
};

}
