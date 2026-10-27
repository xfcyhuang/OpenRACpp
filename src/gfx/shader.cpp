import std;
#include <cassert>
#include <cstdio>

#include "gfx/shader.hpp"

namespace ora::gfx {

namespace {

std::string ApplyVersionPlaceholder(std::string_view str_code, bool b_embedded) {
  std::string str_result(str_code);
  const std::string_view str_version = b_embedded ? "300 es" : "140";
  for (std::size_t size_at = 0; (size_at = str_result.find("{VERSION}", size_at)) != std::string::npos;
       size_at += str_version.size())
    str_result.replace(size_at, sizeof("{VERSION}") - 1, str_version);
  return str_result;
}

std::uint32_t CompileShaderObject(RenderThread& render, gl::GLenum kind_type, std::string_view str_code,
                                  std::string_view str_name, bool b_embedded) {
  const std::string str_resolved = ApplyVersionPlaceholder(str_code, b_embedded);

  GfxScalarRequest request_shader;
  GfxCmd* ptr_cmd = render.queue().Reserve(GfxCmdKind::CreateShader, 0);
  if (ptr_cmd == nullptr)
    return 0;
  ptr_cmd->uint4_a = kind_type;
  ptr_cmd->ptr_sync = &request_shader;
  render.queue().CommitBare();
  if (!render.WaitDone(request_shader.sync.sem_done))
    return 0;
  const std::uint32_t uint4_shader = request_shader.uint4_value;
  if (uint4_shader == 0)
    return 0;

  ptr_cmd = render.queue().Reserve(GfxCmdKind::ShaderSource,
                                   static_cast<std::uint32_t>(str_resolved.size() + 1));
  if (ptr_cmd == nullptr)
    return 0;
  ptr_cmd->uint4_a = uint4_shader;
  render.queue().Commit(str_resolved.c_str());

  ptr_cmd = render.queue().Reserve(GfxCmdKind::CompileShader, 0);
  if (ptr_cmd == nullptr)
    return 0;
  ptr_cmd->uint4_a = uint4_shader;
  render.queue().CommitBare();

  GfxScalarRequest request_status;
  ptr_cmd = render.queue().Reserve(GfxCmdKind::GetShaderStatus, 0);
  if (ptr_cmd == nullptr)
    return 0;
  ptr_cmd->uint4_a = uint4_shader;
  ptr_cmd->ptr_sync = &request_status;
  render.queue().CommitBare();
  if (!render.WaitDone(request_status.sync.sem_done))
    return 0;
  if (request_status.uint4_value == 0) {

    std::println(stderr, "Compile error in shader object {}.", str_name);
    return 0;
  }
  return uint4_shader;
}

}

std::optional<Shader> Shader::Create(RenderThread& render, const ShaderBindingsDesc& desc) {

  const bool b_embedded = false;

  const std::uint32_t uint4_vs = CompileShaderObject(render, gl::GL_VERTEX_SHADER, desc.str_vertex_shader_code,
                                                     desc.str_vertex_shader_name, b_embedded);
  if (uint4_vs == 0)
    return std::nullopt;
  const std::uint32_t uint4_fs = CompileShaderObject(render, gl::GL_FRAGMENT_SHADER, desc.str_fragment_shader_code,
                                                     desc.str_fragment_shader_name, b_embedded);
  if (uint4_fs == 0)
    return std::nullopt;

  GfxScalarRequest request_program;
  GfxCmd* ptr_cmd = render.queue().Reserve(GfxCmdKind::CreateProgramId, 0);
  if (ptr_cmd == nullptr)
    return std::nullopt;
  ptr_cmd->ptr_sync = &request_program;
  render.queue().CommitBare();
  if (!render.WaitDone(request_program.sync.sem_done))
    return std::nullopt;
  const std::uint32_t uint4_program = request_program.uint4_value;

  for (std::uint32_t i = 0; i < desc.vec_attributes.size(); ++i) {
    ptr_cmd = render.queue().Reserve(GfxCmdKind::EnableVertexAttribArray, 0);
    if (ptr_cmd == nullptr)
      return std::nullopt;
    ptr_cmd->uint4_a = i;
    render.queue().CommitBare();

    const std::size_t size_name_bytes = desc.vec_attributes[i].str_name.size() + 1;
    assert(size_name_bytes <= 0xFFFF);
    ptr_cmd = render.queue().Reserve(GfxCmdKind::BindAttribLocation,
                                     static_cast<std::uint32_t>(size_name_bytes));
    if (ptr_cmd == nullptr)
      return std::nullopt;
    ptr_cmd->uint4_a = uint4_program;
    ptr_cmd->uint4_b = i;
    render.queue().Commit(desc.vec_attributes[i].str_name.c_str());
  }

  if (!b_embedded) {
    ptr_cmd = render.queue().Reserve(GfxCmdKind::BindFragDataLocation, sizeof("fragColor"));
    if (ptr_cmd == nullptr)
      return std::nullopt;
    ptr_cmd->uint4_a = uint4_program;
    ptr_cmd->uint4_b = 0;
    render.queue().Commit("fragColor");
  }

  ptr_cmd = render.queue().Reserve(GfxCmdKind::AttachShader, 0);
  if (ptr_cmd == nullptr)
    return std::nullopt;
  ptr_cmd->uint4_a = uint4_program;
  ptr_cmd->uint4_b = uint4_vs;
  render.queue().CommitBare();
  ptr_cmd = render.queue().Reserve(GfxCmdKind::AttachShader, 0);
  if (ptr_cmd == nullptr)
    return std::nullopt;
  ptr_cmd->uint4_a = uint4_program;
  ptr_cmd->uint4_b = uint4_fs;
  render.queue().CommitBare();
  ptr_cmd = render.queue().Reserve(GfxCmdKind::LinkProgram, 0);
  if (ptr_cmd == nullptr)
    return std::nullopt;
  ptr_cmd->uint4_a = uint4_program;
  render.queue().CommitBare();

  ptr_cmd = render.queue().Reserve(GfxCmdKind::GetProgramStatus, 0);
  if (ptr_cmd == nullptr)
    return std::nullopt;
  ptr_cmd->uint4_a = uint4_program;
  ptr_cmd->ptr_sync = &request_program;
  render.queue().CommitBare();
  if (!render.WaitDone(request_program.sync.sem_done) || request_program.uint4_value == 0) {

    std::println(stderr, "Link error in shader program '{}' and '{}'", desc.str_vertex_shader_name,
                 desc.str_fragment_shader_name);
    return std::nullopt;
  }

  for (const std::uint32_t uint4_shader : {uint4_vs, uint4_fs}) {
    ptr_cmd = render.queue().Reserve(GfxCmdKind::DeleteShader, 0);
    if (ptr_cmd != nullptr) {
      ptr_cmd->uint4_a = uint4_shader;
      render.queue().CommitBare();
    }
  }

  Shader shader_result;
  shader_result.ptr_render_ = &render;
  shader_result.uint4_program_ = uint4_program;
  shader_result.vec_attributes_.assign(desc.vec_attributes.begin(), desc.vec_attributes.end());
  shader_result.int4_stride_ = desc.int4_stride;

  shader_result.PostUseProgram();
  const std::uint32_t uint4_uniforms = render.GetActiveUniformCount(uint4_program);
  std::uint32_t uint4_next_tex_unit = 0;
  for (std::uint32_t i = 0; i < uint4_uniforms; ++i) {
    GfxUniformRequest request_uniform;
    if (!render.GetActiveUniformAt(uint4_program, i, request_uniform))
      return std::nullopt;
    const std::string str_sampler = request_uniform.str_name;
    shader_result.map_uniform_cache_[str_sampler] = request_uniform.int4_location;

    if (request_uniform.uint4_type == gl::GL_SAMPLER_2D) {
      shader_result.map_samplers_[str_sampler] = uint4_next_tex_unit;
      ptr_cmd = render.queue().Reserve(GfxCmdKind::Uniform1i, 0);
      if (ptr_cmd == nullptr)
        return std::nullopt;
      ptr_cmd->uint4_a = static_cast<std::uint32_t>(request_uniform.int4_location);
      ptr_cmd->uint4_b = uint4_next_tex_unit;
      render.queue().CommitBare();
      ++uint4_next_tex_unit;
    }
  }

  return shader_result;
}

Shader::~Shader() {
  if (uint4_program_ != 0 && ptr_render_ != nullptr) {
    if (GfxCmd* ptr_cmd = ptr_render_->queue().Reserve(GfxCmdKind::DeleteProgram, 0); ptr_cmd != nullptr) {
      ptr_cmd->uint4_a = uint4_program_;
      ptr_render_->queue().CommitBare();
    }
    uint4_program_ = 0;
  }
}

Shader::Shader(Shader&& other) noexcept
    : ptr_render_(std::exchange(other.ptr_render_, nullptr)),
      uint4_program_(std::exchange(other.uint4_program_, 0)),
      vec_attributes_(std::move(other.vec_attributes_)),
      int4_stride_(std::exchange(other.int4_stride_, 0)),
      map_uniform_cache_(std::move(other.map_uniform_cache_)),
      map_samplers_(std::move(other.map_samplers_)),
      map_bound_textures_(std::move(other.map_bound_textures_)) {}

void Shader::PostUseProgram() {

  if (GfxCmd* ptr_cmd = ptr_render_->queue().Reserve(GfxCmdKind::BindProgram, 0); ptr_cmd != nullptr) {
    ptr_cmd->uint4_a = uint4_program_;
    ptr_render_->queue().CommitBare();
  }
}

void Shader::Bind() {

  for (std::uint32_t i = 0; i < vec_attributes_.size(); ++i) {
    GfxCmd* ptr_cmd = ptr_render_->queue().Reserve(GfxCmdKind::EnableVertexAttribArray, 0);
    if (ptr_cmd != nullptr) {
      ptr_cmd->uint4_a = i;
      ptr_render_->queue().CommitBare();
    }
  }
  for (std::uint32_t i = 0; i < vec_attributes_.size(); ++i) {
    const ShaderVertexAttribute& attribute = vec_attributes_[i];
    const GfxCmdKind kind = attribute.kind_type == ShaderVertexAttributeType::Float
                                ? GfxCmdKind::VertexAttribPointer
                                : GfxCmdKind::VertexAttribIPointer;
    GfxCmd* ptr_cmd = ptr_render_->queue().Reserve(kind, 0);
    if (ptr_cmd == nullptr)
      continue;
    ptr_cmd->uint4_a = i;
    ptr_cmd->uint4_b = static_cast<std::uint32_t>(attribute.int4_components);
    ptr_cmd->uint4_c = static_cast<std::uint32_t>(attribute.kind_type);
    ptr_cmd->b_depth = 0;
    ptr_cmd->uint4_d = static_cast<std::uint32_t>(int4_stride_);
    ptr_cmd->float_a = std::bit_cast<float>(static_cast<std::uint32_t>(attribute.int4_offset));
    ptr_render_->queue().CommitBare();
  }
}

void Shader::PrepareRender() {
  PostUseProgram();
  for (const auto& [uint4_unit, uint4_texture] : map_bound_textures_) {

    GfxCmd* ptr_cmd = ptr_render_->queue().Reserve(GfxCmdKind::BindTexture, 0);
    if (ptr_cmd == nullptr)
      continue;
    ptr_cmd->uint4_a = uint4_unit;
    ptr_cmd->uint4_b = gl::GL_TEXTURE_2D;
    ptr_cmd->uint4_c = uint4_texture;
    ptr_render_->queue().CommitBare();
  }
}

void Shader::SetTexture(std::string_view str_name, const Texture& texture) {
  const auto it = map_samplers_.find(str_name);
  if (it != map_samplers_.end())
    map_bound_textures_[it->second] = texture.GlId();
}

std::int32_t Shader::Location(std::string_view str_name) const {
  const auto it = map_uniform_cache_.find(str_name);
  return it != map_uniform_cache_.end() ? it->second : -1;
}

void Shader::SetBool(std::string_view str_name, bool b_value) {
  SetBoolAt(Location(str_name), b_value);
}

void Shader::SetVec(std::string_view str_name, float float_x) {
  SetVecAt(Location(str_name), float_x);
}

void Shader::SetVec(std::string_view str_name, float float_x, float float_y) {
  SetVecAt(Location(str_name), float_x, float_y);
}

void Shader::SetVec(std::string_view str_name, float float_x, float float_y, float float_z) {
  SetVecAt(Location(str_name), float_x, float_y, float_z);
}

void Shader::SetVec(std::string_view str_name, std::span<const float> vec_values, std::int32_t int4_length) {
  SetVecAt(Location(str_name), vec_values, int4_length);
}

void Shader::SetMatrix(std::string_view str_name, std::span<const float> vec_matrix16) {
  SetMatrixAt(Location(str_name), vec_matrix16);
}

void Shader::SetBoolAt(std::int32_t int4_location, bool b_value) {
  PostUseProgram();
  if (GfxCmd* ptr_cmd = ptr_render_->queue().Reserve(GfxCmdKind::Uniform1i, 0); ptr_cmd != nullptr) {
    ptr_cmd->uint4_a = static_cast<std::uint32_t>(int4_location);
    ptr_cmd->uint4_b = b_value ? 1 : 0;
    ptr_render_->queue().CommitBare();
  }
}

void Shader::SetVecAt(std::int32_t int4_location, float float_x) {
  PostUseProgram();
  if (GfxCmd* ptr_cmd = ptr_render_->queue().Reserve(GfxCmdKind::Uniform1f, 0); ptr_cmd != nullptr) {
    ptr_cmd->uint4_a = static_cast<std::uint32_t>(int4_location);
    ptr_cmd->float_a = float_x;
    ptr_render_->queue().CommitBare();
  }
}

void Shader::SetVecAt(std::int32_t int4_location, float float_x, float float_y) {
  PostUseProgram();
  if (GfxCmd* ptr_cmd = ptr_render_->queue().Reserve(GfxCmdKind::Uniform2f, 0); ptr_cmd != nullptr) {
    ptr_cmd->uint4_a = static_cast<std::uint32_t>(int4_location);
    ptr_cmd->float_a = float_x;
    ptr_cmd->float_b = float_y;
    ptr_render_->queue().CommitBare();
  }
}

void Shader::SetVecAt(std::int32_t int4_location, float float_x, float float_y, float float_z) {
  PostUseProgram();
  if (GfxCmd* ptr_cmd = ptr_render_->queue().Reserve(GfxCmdKind::Uniform3f, 0); ptr_cmd != nullptr) {
    ptr_cmd->uint4_a = static_cast<std::uint32_t>(int4_location);
    ptr_cmd->float_a = float_x;
    ptr_cmd->float_b = float_y;
    ptr_cmd->float_c = float_z;
    ptr_render_->queue().CommitBare();
  }
}

void Shader::SetVecAt(std::int32_t int4_location, std::span<const float> vec_values, std::int32_t int4_length) {

  assert(int4_length >= 1 && int4_length <= 4);
  PostUseProgram();
  const GfxCmdKind kind = int4_length == 1 ? GfxCmdKind::Uniform1fv
                        : int4_length == 2 ? GfxCmdKind::Uniform2fv
                        : int4_length == 3 ? GfxCmdKind::Uniform3fv
                                           : GfxCmdKind::Uniform4fv;
  const std::uint32_t size_payload = static_cast<std::uint32_t>(int4_length * sizeof(float));
  if (GfxCmd* ptr_cmd = ptr_render_->queue().Reserve(kind, size_payload); ptr_cmd != nullptr) {
    ptr_cmd->uint4_a = static_cast<std::uint32_t>(int4_location);
    ptr_cmd->uint4_b = 1;
    ptr_render_->queue().Commit(vec_values.data());
  }
}

void Shader::SetMatrixAt(std::int32_t int4_location, std::span<const float> vec_matrix16) {
  assert(vec_matrix16.size() == 16);
  PostUseProgram();
  if (GfxCmd* ptr_cmd = ptr_render_->queue().Reserve(GfxCmdKind::UniformMatrix4fv, 16 * sizeof(float));
      ptr_cmd != nullptr) {
    ptr_cmd->uint4_a = static_cast<std::uint32_t>(int4_location);
    ptr_render_->queue().Commit(vec_matrix16.data());
  }
}

}
