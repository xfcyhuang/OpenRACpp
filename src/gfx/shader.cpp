// UPSTREAM: OpenRA.Platforms.Default/Shader.cs @7d57605 L29-259(实现体)
// 实现体翻译:CompileShaderObject({VERSION} 替换 + 编译 + 状态检查)、
// program 组装(属性循环/fragColor/attach/link)、active uniform 枚举、
// Set*/PrepareRender/Bind。GL 访问全部经命令队列;同步往返仅构造期发生。
// The implementation body translated: CompileShaderObject ({VERSION}
// substitution + compile + status check), program assembly (the attribute
// loop / fragColor / attach / link), active-uniform enumeration, and
// Set*/PrepareRender/Bind. All GL access goes through the command queue;
// synchronous round-trips happen only during construction.
import std;
#include <cassert>  // assert 为宏,规范允许的 import/#include 并用正形态 | assert is a macro; the spec-sanctioned mixed form
#include <cstdio>   // stderr 为宏,不穿越 import 边界(规范允许的并用正形态) | stderr is a macro; the spec-sanctioned mixed form

#include "gfx/shader.hpp"

namespace ora::gfx {

namespace {

/// {VERSION} 全量替换(Shader.cs L31-33:Embedded→"300 es",否则→"140")。
/// Replace-all of {VERSION} (Shader.cs L31-33: Embedded→"300 es", else→"140").
std::string ApplyVersionPlaceholder(std::string_view str_code, bool b_embedded) {
  std::string str_result(str_code);
  const std::string_view str_version = b_embedded ? "300 es" : "140";
  for (std::size_t size_at = 0; (size_at = str_result.find("{VERSION}", size_at)) != std::string::npos;
       size_at += str_version.size())
    str_result.replace(size_at, sizeof("{VERSION}") - 1, str_version);
  return str_result;
}

/// 编译一个着色器对象(Shader.cs L29-59;同步往返)。失败 = 0(InfoLog 已打印)。
/// Compiles one shader object (Shader.cs L29-59; synchronous round-trips).
/// Zero on failure (the info log has been printed).
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
                                   static_cast<std::uint16_t>(str_resolved.size() + 1));
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
    // 消费端已打印 InfoLog(GetShaderStatus 分支);此处对齐上游异常文本
    // (Shader.cs L55)。
    // The consumer already printed the info log (the GetShaderStatus branch);
    // this aligns with the upstream exception text (Shader.cs L55).
    std::println(stderr, "Compile error in shader object {}.", str_name);
    return 0;
  }
  return uint4_shader;
}

/// 渲染线程的 GL 档位(Embedded 判定用)。Modern 档按窗口创建参数唯一,
/// 此处由调用方传入 —— Shader 不再二次询问 GL。
/// The render thread's GL profile (for the Embedded test). The Modern profile
/// is fixed by the window-creation parameters and passed in by the caller —
/// Shader never re-queries GL.

}  // namespace

std::optional<Shader> Shader::Create(RenderThread& render, const ShaderBindingsDesc& desc) {
  // Embedded 档由窗口档位决定(sdl2_window 创建时 Modern/Embedded 二选一);
  // 现行桌面主路径 = Modern。ES 档位批次接入后从此处读取档位标志。
  // The Embedded tier is decided by the window profile (Modern/Embedded chosen
  // at sdl2_window creation); the current desktop main path is Modern. The ES
  // batch will read the profile flag here.
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

  // 属性循环(Shader.cs L71-77:EnableVertexAttribArray + BindAttribLocation)
  // The attribute loop (Shader.cs L71-77).
  for (std::uint32_t i = 0; i < desc.vec_attributes.size(); ++i) {
    ptr_cmd = render.queue().Reserve(GfxCmdKind::EnableVertexAttribArray, 0);
    if (ptr_cmd == nullptr)
      return std::nullopt;
    ptr_cmd->uint4_a = i;
    render.queue().CommitBare();

    const std::size_t size_name_bytes = desc.vec_attributes[i].str_name.size() + 1;
    assert(size_name_bytes <= 0xFFFF);
    ptr_cmd = render.queue().Reserve(GfxCmdKind::BindAttribLocation,
                                     static_cast<std::uint16_t>(size_name_bytes));
    if (ptr_cmd == nullptr)
      return std::nullopt;
    ptr_cmd->uint4_a = uint4_program;
    ptr_cmd->uint4_b = i;
    render.queue().Commit(desc.vec_attributes[i].str_name.c_str());
  }

  // Modern 档:fragColor 输出绑定(Shader.cs L79-83)
  // Modern profile: the fragColor output binding (Shader.cs L79-83).
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
    // 上游异常文本(Shader.cs L101)
    // The upstream exception text (Shader.cs L101).
    std::println(stderr, "Link error in shader program '{}' and '{}'", desc.str_vertex_shader_name,
                 desc.str_fragment_shader_name);
    return std::nullopt;
  }

  // 链接产物已嵌入 program:编译对象即刻删除(见文件头偏离注)
  // The linked artifact is embedded in the program: delete the compiled
  // objects right away (see the deviation note in the header).
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

  // use + active uniform 枚举(Shader.cs L104-131)
  // Use + active-uniform enumeration (Shader.cs L104-131).
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
  // 消费端绑定 diff 吞掉重复(OPT-A6;对齐上游每次显式 glUseProgram 的
  // 语义而不付其代价)。
  // The consumer-side binding diff absorbs repeats (OPT-A6; preserves the
  // semantics of upstream's explicit glUseProgram-per-call without its cost).
  if (GfxCmd* ptr_cmd = ptr_render_->queue().Reserve(GfxCmdKind::BindProgram, 0); ptr_cmd != nullptr) {
    ptr_cmd->uint4_a = uint4_program_;
    ptr_render_->queue().CommitBare();
  }
}

void Shader::Bind() {
  // 上游在构造期一次 glEnableVertexAttribArray(全局单 VAO 模型,Shader.cs
  // L71-77);C++ 侧 VAO 化后 enable 状态属于各 VAO,故 Bind 重播时一并
  // enable(幂等),保证任意新建 VAO 上的属性状态完备。
  // Upstream enables the arrays once at construction (the global-single-VAO
  // model, Shader.cs L71-77); once VAO-ized on the C++ side the enable state
  // belongs to each VAO, so Bind re-enables alongside the replay (idempotent)
  // keeping any freshly created VAO attribute-complete.
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
    ptr_cmd->uint4_c = static_cast<std::uint32_t>(attribute.kind_type);  // 枚举值 = GL 类型常量 | enum value = the GL type constant
    ptr_cmd->b_depth = 0;  // 未归一化(上游固定 false)| never normalized (upstream passes false)
    ptr_cmd->uint4_d = static_cast<std::uint32_t>(int4_stride_);
    ptr_cmd->float_a = std::bit_cast<float>(static_cast<std::uint32_t>(attribute.int4_offset));
    ptr_render_->queue().CommitBare();
  }
}

void Shader::PrepareRender() {
  PostUseProgram();
  for (const auto& [uint4_unit, uint4_texture] : map_bound_textures_) {
    // 生命周期契约见文件头:调用方保证 Texture 存活(替代上游 glIsTexture
    // 逐帧驱逐;COVERAGE 登记)。
    // The lifetime contract is in the header: the caller keeps the Texture
    // alive (replacing upstream's per-flush glIsTexture eviction; registered
    // in COVERAGE).
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
  // 上游按长度分发 1fv..4fv(Shader.cs L229-235);length 1..4 之外为异常
  // (InvalidDataException),C++ 侧断言防御。
  // Upstream dispatches 1fv..4fv by length (Shader.cs L229-235); lengths
  // outside 1..4 are exceptions (InvalidDataException), asserted here.
  assert(int4_length >= 1 && int4_length <= 4);
  PostUseProgram();
  const GfxCmdKind kind = int4_length == 1 ? GfxCmdKind::Uniform1fv
                        : int4_length == 2 ? GfxCmdKind::Uniform2fv
                        : int4_length == 3 ? GfxCmdKind::Uniform3fv
                                           : GfxCmdKind::Uniform4fv;
  const std::uint16_t size_payload = static_cast<std::uint16_t>(int4_length * sizeof(float));
  if (GfxCmd* ptr_cmd = ptr_render_->queue().Reserve(kind, size_payload); ptr_cmd != nullptr) {
    ptr_cmd->uint4_a = static_cast<std::uint32_t>(int4_location);
    ptr_cmd->uint4_b = 1;  // 上游 count=1 | upstream count=1
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

}  // namespace ora::gfx
