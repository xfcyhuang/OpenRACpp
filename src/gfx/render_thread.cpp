import std;
#include <cstdio>

#include <SDL2/SDL.h>

#include "gfx/render_thread.hpp"
#include "gfx/sprite.hpp"
#include "platform/gl_loader.hpp"

namespace ora::gfx {

namespace {

void OnDebugMessage(gl::GLenum, gl::GLenum, gl::GLuint, gl::GLenum, gl::GLsizei,
                               const gl::GLchar* str_message, const void*) {
  std::println(stderr, "KHR_debug: {}", str_message != nullptr ? str_message : "<null>");
}

struct PersistentBufferState {
  std::byte* ptr_mapped = nullptr;
  std::size_t size_total = 0;
  std::size_t size_slot = 0;
  gl::GLsync sync_fences[8] = {};
};

struct RenderState {
  gl::GLuint uint4_program = 0;
  gl::GLuint uint4_framebuffer = 0;
  gl::GLuint uint4_vertex_array = 0;
  gl::GLuint uint4_texture[16] = {};
  std::int32_t int4_blend_mode = -1;
  std::map<gl::GLuint, PersistentBufferState> map_persistent;
};

void WaitAndDeleteSync(gl::GLsync& sync_fence) {
  if (sync_fence == nullptr)
    return;
  namespace g = ora::gl;
  while (g::ClientWaitSync(sync_fence, g::GL_SYNC_FLUSH_COMMANDS_BIT, g::GL_TIMEOUT_IGNORED) == g::GL_TIMEOUT_EXPIRED) {
  }
  g::DeleteSync(sync_fence);
  sync_fence = nullptr;
}

void ExecuteCommand(const GfxCmd& cmd, const std::byte* ptr_payload, RenderState& state, platform::Sdl2Window& window) {
  namespace g = ora::gl;
  switch (cmd.kind) {
    case GfxCmdKind::Clear:
      g::ClearColor(cmd.float_a, cmd.float_b, cmd.float_c, cmd.float_d);
      g::Clear(g::GL_COLOR_BUFFER_BIT | (cmd.b_depth != 0 ? g::GL_DEPTH_BUFFER_BIT : 0));
      break;
    case GfxCmdKind::SetViewport:
      g::Viewport(static_cast<gl::GLint>(cmd.uint4_a), static_cast<gl::GLint>(cmd.uint4_b),
                  static_cast<gl::GLsizei>(cmd.uint4_c), static_cast<gl::GLsizei>(cmd.uint4_d));
      break;
    case GfxCmdKind::SetScissor:
      g::Enable(g::GL_SCISSOR_TEST);
      g::Scissor(static_cast<gl::GLint>(cmd.uint4_a), static_cast<gl::GLint>(cmd.uint4_b),
                 static_cast<gl::GLsizei>(cmd.uint4_c), static_cast<gl::GLsizei>(cmd.uint4_d));
      break;
    case GfxCmdKind::DisableScissor:
      g::Disable(g::GL_SCISSOR_TEST);
      break;
    case GfxCmdKind::BindFramebuffer:
      if (state.uint4_framebuffer != cmd.uint4_a) {
        state.uint4_framebuffer = cmd.uint4_a;
        g::BindFramebuffer(g::GL_FRAMEBUFFER, cmd.uint4_a);
      }
      break;
    case GfxCmdKind::BindProgram:
      if (state.uint4_program != cmd.uint4_a) {
        state.uint4_program = cmd.uint4_a;
        g::UseProgram(cmd.uint4_a);
      }
      break;
    case GfxCmdKind::BindTexture: {

      if (cmd.uint4_a < std::size(state.uint4_texture) && state.uint4_texture[cmd.uint4_a] != cmd.uint4_c) {
        state.uint4_texture[cmd.uint4_a] = cmd.uint4_c;
        g::ActiveTexture(g::GL_TEXTURE0 + cmd.uint4_a);
        g::BindTexture(static_cast<gl::GLenum>(cmd.uint4_b), cmd.uint4_c);
      }
      break;
    }
    case GfxCmdKind::Uniform1i:
      g::Uniform1i(static_cast<gl::GLint>(cmd.uint4_a), static_cast<gl::GLint>(cmd.uint4_b));
      break;
    case GfxCmdKind::Uniform2f:
      g::Uniform2f(static_cast<gl::GLint>(cmd.uint4_a), cmd.float_a, cmd.float_b);
      break;
    case GfxCmdKind::Uniform4fv:
      g::Uniform4fv(static_cast<gl::GLint>(cmd.uint4_a), static_cast<gl::GLsizei>(cmd.uint4_b),
                    reinterpret_cast<const gl::GLfloat*>(ptr_payload));
      break;
    case GfxCmdKind::UniformMatrix4fv:
      g::UniformMatrix4fv(static_cast<gl::GLint>(cmd.uint4_a), 1, g::GL_FALSE,
                          reinterpret_cast<const gl::GLfloat*>(ptr_payload));
      break;
    case GfxCmdKind::GenBuffers:
    case GfxCmdKind::GenVertexArrays:
    case GfxCmdKind::GenTextures:
    case GfxCmdKind::GenFramebuffers: {
      auto* ptr_request = static_cast<GfxNameRequest*>(cmd.ptr_sync);
      const auto uint4_n = static_cast<gl::GLsizei>(cmd.uint4_a);
      switch (cmd.kind) {
        case GfxCmdKind::GenBuffers: g::GenBuffers(uint4_n, ptr_request->uint4_ids); break;
        case GfxCmdKind::GenVertexArrays: g::GenVertexArrays(uint4_n, ptr_request->uint4_ids); break;
        case GfxCmdKind::GenTextures: g::GenTextures(uint4_n, ptr_request->uint4_ids); break;
        default: g::GenFramebuffers(uint4_n, ptr_request->uint4_ids); break;
      }
      ptr_request->sync.sem_done.release();
      break;
    }
    case GfxCmdKind::DeleteBuffers:
      g::DeleteBuffers(static_cast<gl::GLsizei>(cmd.size_payload / 4),
                       reinterpret_cast<const gl::GLuint*>(ptr_payload));
      break;
    case GfxCmdKind::DeleteTextures: {

      const auto* vec_ids = reinterpret_cast<const gl::GLuint*>(ptr_payload);
      const auto uint4_count = cmd.size_payload / 4;
      for (std::uint32_t i = 0; i < uint4_count; ++i)
        for (auto& uint4_bound : state.uint4_texture)
          if (uint4_bound == vec_ids[i])
            uint4_bound = 0;
      g::DeleteTextures(static_cast<gl::GLsizei>(uint4_count), vec_ids);
      break;
    }
    case GfxCmdKind::DeleteFramebuffers: {
      const auto* vec_ids = reinterpret_cast<const gl::GLuint*>(ptr_payload);
      const auto uint4_count = cmd.size_payload / 4;
      for (std::uint32_t i = 0; i < uint4_count; ++i)
        if (state.uint4_framebuffer == vec_ids[i])
          state.uint4_framebuffer = 0;
      g::DeleteFramebuffers(static_cast<gl::GLsizei>(uint4_count), vec_ids);
      break;
    }
    case GfxCmdKind::BindBuffer:
      g::BindBuffer(static_cast<gl::GLenum>(cmd.uint4_a), cmd.uint4_b);
      break;
    case GfxCmdKind::BufferData:
      g::BufferData(static_cast<gl::GLenum>(cmd.uint4_a), static_cast<gl::GLsizeiptr>(cmd.uint4_b),
                    cmd.size_payload != 0 ? ptr_payload : nullptr, static_cast<gl::GLenum>(cmd.uint4_c));
      break;
    case GfxCmdKind::BufferSubData:
      g::BufferSubData(static_cast<gl::GLenum>(cmd.uint4_a), static_cast<gl::GLintptr>(cmd.uint4_b),
                       static_cast<gl::GLsizeiptr>(cmd.uint4_c), ptr_payload);
      break;
    case GfxCmdKind::BindVertexArray:
      if (state.uint4_vertex_array != cmd.uint4_a) {
        state.uint4_vertex_array = cmd.uint4_a;
        g::BindVertexArray(cmd.uint4_a);
      }
      break;
    case GfxCmdKind::DeleteVertexArrays:

      for (std::uint32_t i = 0; i < cmd.size_payload / 4; ++i) {
        gl::GLuint uint4_vao = reinterpret_cast<const gl::GLuint*>(ptr_payload)[i];
        if (state.uint4_vertex_array == uint4_vao)
          state.uint4_vertex_array = 0;
        g::DeleteVertexArrays(1, &uint4_vao);
      }
      break;
    case GfxCmdKind::TexImage2D:

      g::TexImage2D(static_cast<gl::GLenum>(cmd.uint4_a), 0, static_cast<gl::GLint>(cmd.uint4_d),
                    static_cast<gl::GLsizei>(cmd.uint4_b), static_cast<gl::GLsizei>(cmd.uint4_c),
                    0, cmd.float_a != 0.0f ? std::bit_cast<gl::GLenum>(cmd.float_a) : g::GL_RGBA,
                    cmd.float_b != 0.0f ? std::bit_cast<gl::GLenum>(cmd.float_b) : g::GL_UNSIGNED_BYTE,
                    cmd.size_payload != 0 ? ptr_payload : nullptr);
      break;
    case GfxCmdKind::TexSubImage2D:

      g::TexSubImage2D(static_cast<gl::GLenum>(cmd.uint4_a), 0, static_cast<gl::GLint>(cmd.uint4_b),
                       static_cast<gl::GLint>(cmd.uint4_c), static_cast<gl::GLsizei>(cmd.uint4_d),
                       static_cast<gl::GLsizei>(std::bit_cast<std::uint32_t>(cmd.float_a)),
                       cmd.float_b != 0.0f ? std::bit_cast<gl::GLenum>(cmd.float_b) : g::GL_RGBA,
                       g::GL_UNSIGNED_BYTE, ptr_payload);
      break;
    case GfxCmdKind::FramebufferTexture2D:

      if (state.uint4_framebuffer != cmd.uint4_a) {
        state.uint4_framebuffer = cmd.uint4_a;
        g::BindFramebuffer(g::GL_FRAMEBUFFER, cmd.uint4_a);
      }
      g::FramebufferTexture2D(g::GL_FRAMEBUFFER, static_cast<gl::GLenum>(cmd.uint4_b),
                              g::GL_TEXTURE_2D, cmd.uint4_c, 0);
      break;
    case GfxCmdKind::CreateShader: {
      auto* ptr_request = static_cast<GfxScalarRequest*>(cmd.ptr_sync);
      ptr_request->uint4_value = g::CreateShader(static_cast<gl::GLenum>(cmd.uint4_a));
      ptr_request->sync.sem_done.release();
      break;
    }
    case GfxCmdKind::ShaderSource: {
      const gl::GLchar* str_source = reinterpret_cast<const gl::GLchar*>(ptr_payload);
      const gl::GLint int4_length = static_cast<gl::GLint>(std::strlen(str_source));
      g::ShaderSource(cmd.uint4_a, 1, &str_source, &int4_length);
      break;
    }
    case GfxCmdKind::CompileShader:
      g::CompileShader(cmd.uint4_a);
      break;
    case GfxCmdKind::GetShaderStatus: {
      auto* ptr_request = static_cast<GfxScalarRequest*>(cmd.ptr_sync);
      gl::GLint int4_status = 0;
      g::GetShaderiv(cmd.uint4_a, g::GL_COMPILE_STATUS, &int4_status);
      if (int4_status == 0) {
        gl::GLint int4_log_len = 0;
        gl::GLchar str_log[1024] = {};
        g::GetShaderiv(cmd.uint4_a, g::GL_INFO_LOG_LENGTH, &int4_log_len);
        g::GetShaderInfoLog(cmd.uint4_a, sizeof(str_log) - 1, nullptr, str_log);
        std::println(stderr, "着色器编译失败 | shader compile failed (loglen={}): {}", int4_log_len, str_log);
      }
      ptr_request->uint4_value = static_cast<std::uint32_t>(int4_status);
      ptr_request->sync.sem_done.release();
      break;
    }
    case GfxCmdKind::CreateProgramId: {
      auto* ptr_request = static_cast<GfxScalarRequest*>(cmd.ptr_sync);
      ptr_request->uint4_value = g::CreateProgram();
      ptr_request->sync.sem_done.release();
      break;
    }
    case GfxCmdKind::AttachShader:
      g::AttachShader(cmd.uint4_a, cmd.uint4_b);
      break;
    case GfxCmdKind::BindAttribLocation:
      g::BindAttribLocation(cmd.uint4_a, cmd.uint4_b,
                            reinterpret_cast<const gl::GLchar*>(ptr_payload));
      break;
    case GfxCmdKind::LinkProgram:
      g::LinkProgram(cmd.uint4_a);
      break;
    case GfxCmdKind::GetProgramStatus: {
      auto* ptr_request = static_cast<GfxScalarRequest*>(cmd.ptr_sync);
      gl::GLint int4_status = 0;
      g::GetProgramiv(cmd.uint4_a, g::GL_LINK_STATUS, &int4_status);
      if (int4_status == 0) {
        gl::GLchar str_log[1024];
        g::GetProgramInfoLog(cmd.uint4_a, sizeof(str_log), nullptr, str_log);
        std::println(stderr, "程序链接失败 | program link failed: {}", str_log);
      }
      ptr_request->uint4_value = static_cast<std::uint32_t>(int4_status);
      ptr_request->sync.sem_done.release();
      break;
    }
    case GfxCmdKind::GetUniformLocation: {
      auto* ptr_request = static_cast<GfxScalarRequest*>(cmd.ptr_sync);
      ptr_request->uint4_value = static_cast<std::uint32_t>(
          g::GetUniformLocation(cmd.uint4_a, reinterpret_cast<const gl::GLchar*>(ptr_payload)));
      ptr_request->sync.sem_done.release();
      break;
    }
    case GfxCmdKind::VertexAttribPointer:

      g::VertexAttribPointer(cmd.uint4_a, static_cast<gl::GLint>(cmd.uint4_b),
                             static_cast<gl::GLenum>(cmd.uint4_c), cmd.b_depth != 0 ? g::GL_TRUE : g::GL_FALSE,
                             static_cast<gl::GLsizei>(cmd.uint4_d),
                             reinterpret_cast<const void*>(
                                 static_cast<std::uintptr_t>(std::bit_cast<std::uint32_t>(cmd.float_a))));
      break;
    case GfxCmdKind::VertexAttribIPointer:
      g::VertexAttribIPointer(cmd.uint4_a, static_cast<gl::GLint>(cmd.uint4_b),
                              static_cast<gl::GLenum>(cmd.uint4_c), static_cast<gl::GLsizei>(cmd.uint4_d),
                              reinterpret_cast<const void*>(
                                  static_cast<std::uintptr_t>(std::bit_cast<std::uint32_t>(cmd.float_a))));
      break;
    case GfxCmdKind::EnableVertexAttribArray:
      g::EnableVertexAttribArray(cmd.uint4_a);
      break;
    case GfxCmdKind::DisableVertexAttribArray:
      g::DisableVertexAttribArray(cmd.uint4_a);
      break;
    case GfxCmdKind::QueryFboStatus: {
      auto* ptr_request = static_cast<GfxScalarRequest*>(cmd.ptr_sync);
      ptr_request->uint4_value = g::CheckFramebufferStatus(g::GL_FRAMEBUFFER);
      ptr_request->sync.sem_done.release();
      break;
    }
    case GfxCmdKind::DrawArrays:
      g::DrawArrays(static_cast<gl::GLenum>(cmd.uint4_a), static_cast<gl::GLint>(cmd.uint4_b),
                    static_cast<gl::GLsizei>(cmd.uint4_c));
      break;
    case GfxCmdKind::DrawElements:

      g::DrawElementsBaseVertex(static_cast<gl::GLenum>(cmd.uint4_a), static_cast<gl::GLsizei>(cmd.uint4_b),
                                static_cast<gl::GLenum>(cmd.uint4_c),
                                reinterpret_cast<const void*>(static_cast<std::uintptr_t>(cmd.uint4_d)),
                                static_cast<gl::GLint>(std::bit_cast<std::uint32_t>(cmd.float_a)));
      break;
    case GfxCmdKind::ReadPixels: {
      auto* ptr_request = static_cast<GfxReadbackRequest*>(cmd.ptr_sync);
      g::ReadPixels(static_cast<gl::GLint>(cmd.uint4_a), static_cast<gl::GLint>(cmd.uint4_b),
                    static_cast<gl::GLsizei>(cmd.uint4_c), static_cast<gl::GLsizei>(cmd.uint4_d),
                    g::GL_RGBA, g::GL_UNSIGNED_BYTE, ptr_request->ptr_dst);
      ptr_request->sync.sem_done.release();
      break;
    }
    case GfxCmdKind::Finish: {
      g::Finish();
      static_cast<GfxSyncOut*>(cmd.ptr_sync)->sem_done.release();
      break;
    }
    case GfxCmdKind::Uniform1f:
      g::Uniform1f(static_cast<gl::GLint>(cmd.uint4_a), cmd.float_a);
      break;
    case GfxCmdKind::Uniform3f:
      g::Uniform3f(static_cast<gl::GLint>(cmd.uint4_a), cmd.float_a, cmd.float_b, cmd.float_c);
      break;
    case GfxCmdKind::Uniform1fv:
      g::Uniform1fv(static_cast<gl::GLint>(cmd.uint4_a), static_cast<gl::GLsizei>(cmd.uint4_b),
                    reinterpret_cast<const gl::GLfloat*>(ptr_payload));
      break;
    case GfxCmdKind::Uniform2fv:
      g::Uniform2fv(static_cast<gl::GLint>(cmd.uint4_a), static_cast<gl::GLsizei>(cmd.uint4_b),
                    reinterpret_cast<const gl::GLfloat*>(ptr_payload));
      break;
    case GfxCmdKind::Uniform3fv:
      g::Uniform3fv(static_cast<gl::GLint>(cmd.uint4_a), static_cast<gl::GLsizei>(cmd.uint4_b),
                    reinterpret_cast<const gl::GLfloat*>(ptr_payload));
      break;
    case GfxCmdKind::PixelStorei:
      g::PixelStorei(static_cast<gl::GLenum>(cmd.uint4_a), static_cast<gl::GLint>(cmd.uint4_b));
      break;
    case GfxCmdKind::TexParameteri:

      g::TexParameteri(g::GL_TEXTURE_2D, static_cast<gl::GLenum>(cmd.uint4_a),
                       static_cast<gl::GLint>(cmd.uint4_b));
      break;
    case GfxCmdKind::CopyTexImage2D:

      g::CopyTexImage2D(g::GL_TEXTURE_2D, 0,
                        cmd.float_a != 0.0f ? std::bit_cast<gl::GLenum>(cmd.float_a) : g::GL_RGBA8,
                        static_cast<gl::GLint>(cmd.uint4_a), static_cast<gl::GLint>(cmd.uint4_b),
                        static_cast<gl::GLsizei>(cmd.uint4_c), static_cast<gl::GLsizei>(cmd.uint4_d), 0);
      break;
    case GfxCmdKind::GetTexImage: {
      auto* ptr_request = static_cast<GfxReadbackRequest*>(cmd.ptr_sync);
      g::GetTexImage(g::GL_TEXTURE_2D, 0, g::GL_BGRA, g::GL_UNSIGNED_BYTE, ptr_request->ptr_dst);
      ptr_request->sync.sem_done.release();
      break;
    }
    case GfxCmdKind::GetProgramActiveUniforms: {
      auto* ptr_request = static_cast<GfxScalarRequest*>(cmd.ptr_sync);
      gl::GLint int4_count = 0;
      g::GetProgramiv(cmd.uint4_a, g::GL_ACTIVE_UNIFORMS, &int4_count);
      ptr_request->uint4_value = static_cast<std::uint32_t>(int4_count);
      ptr_request->sync.sem_done.release();
      break;
    }
    case GfxCmdKind::GetActiveUniformAt: {
      auto* ptr_request = static_cast<GfxUniformRequest*>(cmd.ptr_sync);
      gl::GLint int4_size = 0;
      g::GetActiveUniform(cmd.uint4_a, cmd.uint4_b, sizeof(ptr_request->str_name) - 1, nullptr,
                          &int4_size, &ptr_request->uint4_type, ptr_request->str_name);
      ptr_request->int4_location = g::GetUniformLocation(cmd.uint4_a, ptr_request->str_name);
      ptr_request->sync.sem_done.release();
      break;
    }
    case GfxCmdKind::BindFragDataLocation:
      g::BindFragDataLocation(cmd.uint4_a, cmd.uint4_b,
                              reinterpret_cast<const gl::GLchar*>(ptr_payload));
      break;
    case GfxCmdKind::DeleteProgram:
      g::DeleteProgram(cmd.uint4_a);
      break;
    case GfxCmdKind::DeleteShader:
      g::DeleteShader(cmd.uint4_a);
      break;

    case GfxCmdKind::SetBlendMode: {

      if (state.int4_blend_mode == static_cast<std::int32_t>(cmd.uint4_a))
        break;
      state.int4_blend_mode = static_cast<std::int32_t>(cmd.uint4_a);
      g::BlendEquation(g::GL_FUNC_ADD);
      switch (static_cast<BlendMode>(cmd.uint4_a)) {
        case BlendMode::None:
          g::Disable(g::GL_BLEND);
          break;
        case BlendMode::Alpha:
          g::Enable(g::GL_BLEND);
          g::BlendFunc(g::GL_ONE, g::GL_ONE_MINUS_SRC_ALPHA);
          break;
        case BlendMode::Additive:
        case BlendMode::Subtractive:
          g::Enable(g::GL_BLEND);
          g::BlendFunc(g::GL_ONE, g::GL_ONE);
          if (static_cast<BlendMode>(cmd.uint4_a) == BlendMode::Subtractive)
            g::BlendEquationSeparate(g::GL_FUNC_REVERSE_SUBTRACT, g::GL_FUNC_ADD);
          break;
        case BlendMode::Multiply:
          g::Enable(g::GL_BLEND);
          g::BlendFunc(g::GL_DST_COLOR, g::GL_ONE_MINUS_SRC_ALPHA);
          break;
        case BlendMode::Multiplicative:
          g::Enable(g::GL_BLEND);
          g::BlendFunc(g::GL_ZERO, g::GL_SRC_COLOR);
          break;
        case BlendMode::DoubleMultiplicative:
          g::Enable(g::GL_BLEND);
          g::BlendFunc(g::GL_DST_COLOR, g::GL_SRC_COLOR);
          break;
        case BlendMode::LowAdditive:
          g::Enable(g::GL_BLEND);
          g::BlendFunc(g::GL_DST_COLOR, g::GL_ONE);
          break;
        case BlendMode::Screen:
          g::Enable(g::GL_BLEND);
          g::BlendFunc(g::GL_SRC_COLOR, g::GL_ONE_MINUS_SRC_COLOR);
          break;
        case BlendMode::Translucent:
          g::Enable(g::GL_BLEND);
          g::BlendFunc(g::GL_DST_COLOR, g::GL_ONE_MINUS_DST_COLOR);
          break;
      }
      break;
    }
    case GfxCmdKind::EnableDepthTest:

      g::Clear(g::GL_DEPTH_BUFFER_BIT);
      g::Enable(g::GL_DEPTH_TEST);
      g::DepthFunc(g::GL_LEQUAL);
      break;
    case GfxCmdKind::DisableDepthTest:
      g::Disable(g::GL_DEPTH_TEST);
      break;
    case GfxCmdKind::ClearDepth:
      g::Clear(g::GL_DEPTH_BUFFER_BIT);
      break;
    case GfxCmdKind::GenRenderbuffers:
      g::GenRenderbuffers(static_cast<gl::GLsizei>(cmd.uint4_a),
                          static_cast<GfxNameRequest*>(cmd.ptr_sync)->uint4_ids);
      static_cast<GfxNameRequest*>(cmd.ptr_sync)->sync.sem_done.release();
      break;
    case GfxCmdKind::DeleteRenderbuffers:
      g::DeleteRenderbuffers(static_cast<gl::GLsizei>(cmd.size_payload / 4),
                             reinterpret_cast<const gl::GLuint*>(ptr_payload));
      break;
    case GfxCmdKind::RenderbufferStorage:

      g::BindRenderbuffer(g::GL_RENDERBUFFER, cmd.uint4_d);
      g::RenderbufferStorage(g::GL_RENDERBUFFER, static_cast<gl::GLenum>(cmd.uint4_a),
                             static_cast<gl::GLsizei>(cmd.uint4_b), static_cast<gl::GLsizei>(cmd.uint4_c));
      break;
    case GfxCmdKind::FramebufferRenderbuffer:
      if (state.uint4_framebuffer != cmd.uint4_a) {
        state.uint4_framebuffer = cmd.uint4_a;
        g::BindFramebuffer(g::GL_FRAMEBUFFER, cmd.uint4_a);
      }
      g::FramebufferRenderbuffer(g::GL_FRAMEBUFFER, static_cast<gl::GLenum>(cmd.uint4_b),
                                 g::GL_RENDERBUFFER, cmd.uint4_c);
      break;
    case GfxCmdKind::BufferStoragePersistent: {

      PersistentBufferState& buffer_persistent = state.map_persistent[cmd.uint4_a];
      buffer_persistent.size_total = cmd.uint4_b;
      buffer_persistent.size_slot = cmd.uint4_c != 0 ? cmd.uint4_b / cmd.uint4_c : cmd.uint4_b;
      g::BindBuffer(g::GL_ARRAY_BUFFER, cmd.uint4_a);
      constexpr gl::GLbitfield kMapFlags =
          g::GL_MAP_WRITE_BIT | g::GL_MAP_PERSISTENT_BIT | g::GL_MAP_COHERENT_BIT;
      g::BufferStorage(g::GL_ARRAY_BUFFER, static_cast<gl::GLsizeiptr>(cmd.uint4_b), nullptr, kMapFlags);
      buffer_persistent.ptr_mapped =
          static_cast<std::byte*>(g::MapBufferRange(g::GL_ARRAY_BUFFER, 0, static_cast<gl::GLsizeiptr>(cmd.uint4_b), kMapFlags));
      break;
    }
    case GfxCmdKind::WritePersistent: {
      const auto it_buffer = state.map_persistent.find(cmd.uint4_a);
      if (it_buffer == state.map_persistent.end())
        break;
      WaitAndDeleteSync(it_buffer->second.sync_fences[cmd.uint4_b % 8]);
      std::memcpy(it_buffer->second.ptr_mapped + cmd.uint4_c, ptr_payload, cmd.size_payload);
      break;
    }
    case GfxCmdKind::FencePersistentSlot: {
      const auto it_buffer = state.map_persistent.find(cmd.uint4_a);
      if (it_buffer == state.map_persistent.end())
        break;
      auto& sync_slot = it_buffer->second.sync_fences[cmd.uint4_b % 8];
      WaitAndDeleteSync(sync_slot);
      sync_slot = g::FenceSync(g::GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
      break;
    }
    case GfxCmdKind::DeletePersistentBuffer: {
      const auto it_buffer = state.map_persistent.find(cmd.uint4_a);
      if (it_buffer == state.map_persistent.end())
        break;
      for (auto& sync_slot : it_buffer->second.sync_fences)
        WaitAndDeleteSync(sync_slot);
      g::DeleteBuffers(1, &it_buffer->first);
      state.map_persistent.erase(it_buffer);
      break;
    }
    case GfxCmdKind::ConfigureVao: {

      const auto* vec_attribs = reinterpret_cast<const VaoAttribDesc*>(ptr_payload);
      const auto uint4_count = cmd.size_payload / sizeof(VaoAttribDesc);
      if (state.uint4_vertex_array != cmd.uint4_a) {
        state.uint4_vertex_array = cmd.uint4_a;
        g::BindVertexArray(cmd.uint4_a);
      }
      g::BindBuffer(g::GL_ARRAY_BUFFER, cmd.uint4_b);
      if (cmd.uint4_c != 0)
        g::BindBuffer(g::GL_ELEMENT_ARRAY_BUFFER, cmd.uint4_c);
      for (std::uint32_t i = 0; i < uint4_count; ++i) {
        const auto offset = reinterpret_cast<const void*>(static_cast<std::uintptr_t>(vec_attribs[i].uint4_offset));
        if (vec_attribs[i].b_integer != 0)
          g::VertexAttribIPointer(vec_attribs[i].uint4_index, static_cast<gl::GLint>(vec_attribs[i].uint4_size),
                                  vec_attribs[i].uint4_type, static_cast<gl::GLsizei>(vec_attribs[i].uint4_stride), offset);
        else
          g::VertexAttribPointer(vec_attribs[i].uint4_index, static_cast<gl::GLint>(vec_attribs[i].uint4_size),
                                 vec_attribs[i].uint4_type, g::GL_FALSE,
                                 static_cast<gl::GLsizei>(vec_attribs[i].uint4_stride), offset);
        g::EnableVertexAttribArray(vec_attribs[i].uint4_index);
      }
      break;
    }
    case GfxCmdKind::GetViewport: {
      auto* ptr_request = static_cast<GfxNameRequest*>(cmd.ptr_sync);
      g::GetIntegerv(g::GL_VIEWPORT, reinterpret_cast<gl::GLint*>(ptr_request->uint4_ids));
      ptr_request->sync.sem_done.release();
      break;
    }
    case GfxCmdKind::Present:

      window.SwapBuffers();
      break;
    case GfxCmdKind::Shutdown:
      break;
  }
}

}

RenderThread::RenderThread(platform::Sdl2Window& window, std::size_t size_queue_bytes)
    : window_(window), queue_(size_queue_bytes) {

  thread_render_ = std::jthread([this] { Run(); });
}

RenderThread::~RenderThread() {
  if (GfxCmd* ptr_cmd = queue_.Reserve(GfxCmdKind::Shutdown, 0); ptr_cmd != nullptr)
    queue_.CommitBare();
  queue_.Stop();
  thread_render_.join();
}

void RenderThread::Run() {
  ptr_context_ = window_.CreateGlContext();
  if (ptr_context_ == nullptr || !window_.MakeGlCurrent(ptr_context_)) {
    std::println(stderr, "渲染线程:GL 上下文建立失败 | render thread: GL context setup failed");
    b_thread_failed_.store(true, std::memory_order_release);
    queue_.Stop();
    return;
  }

  const gl::GlLoadResult result_load = gl::LoadGl([](const char* str_name) {
    return SDL_GL_GetProcAddress(str_name);
  });
  if (!result_load.b_ok) {
    std::println(stderr, "GL 入口缺失 | missing GL entry: {}", result_load.str_missing);
    window_.MakeGlCurrent(nullptr);
    b_thread_failed_.store(true, std::memory_order_release);
    queue_.Stop();
    return;
  }
  b_thread_ready_.store(true, std::memory_order_release);

#ifndef NDEBUG
  if (gl::HasKHRDebug()) {
    gl::Enable(gl::GL_DEBUG_OUTPUT);
    gl::DebugMessageCallback(&OnDebugMessage, nullptr);
  }
#endif

  gl::GLuint uint4_global_vao = 0;
  gl::GenVertexArrays(1, &uint4_global_vao);
  gl::BindVertexArray(uint4_global_vao);

  RenderState state;
  state.uint4_vertex_array = uint4_global_vao;
  for (;;) {
    std::uint32_t uint4_record = 0;
    const GfxCmd* ptr_cmd = queue_.FetchNext(uint4_record);
    if (ptr_cmd == nullptr)
      break;
    const bool b_shutdown = ptr_cmd->kind == GfxCmdKind::Shutdown;
    ExecuteCommand(*ptr_cmd, GfxCommandQueue::PayloadOf(ptr_cmd), state, window_);
    queue_.Skip(uint4_record);
    if (b_shutdown)
      break;
  }

  window_.MakeGlCurrent(nullptr);
  queue_.Stop();
}

bool RenderThread::WaitDone(std::binary_semaphore& sem_done) {

  while (!sem_done.try_acquire_for(std::chrono::milliseconds(200))) {
    if (b_thread_failed_.load(std::memory_order_acquire)) {

      if (!sem_done.try_acquire_for(std::chrono::milliseconds(500)))
        return false;
    }
  }
  return true;
}

void RenderThread::FlushAndWait() {
  GfxSyncOut sync;
  if (GfxCmd* ptr_cmd = queue_.Reserve(GfxCmdKind::Finish, 0); ptr_cmd != nullptr) {
    ptr_cmd->ptr_sync = &sync;
    queue_.CommitBare();
    WaitDone(sync.sem_done);
  }
}

void RenderThread::GenNames(GfxCmdKind kind_gen, std::uint32_t uint4_n, std::uint32_t* ptr_ids_out) {
  assert(uint4_n <= 16);
  GfxNameRequest request;
  if (GfxCmd* ptr_cmd = queue_.Reserve(kind_gen, 0); ptr_cmd != nullptr) {
    ptr_cmd->uint4_a = uint4_n;
    ptr_cmd->ptr_sync = &request;
    queue_.CommitBare();
    if (!WaitDone(request.sync.sem_done))
      return;
    std::memcpy(ptr_ids_out, request.uint4_ids, uint4_n * sizeof(std::uint32_t));
  }
}

std::uint32_t RenderThread::CreateProgramFromSources(std::string_view str_vertex, std::string_view str_fragment) {
  GfxScalarRequest request_vs;
  GfxCmd* ptr_cmd = queue_.Reserve(GfxCmdKind::CreateShader, 0);
  if (ptr_cmd == nullptr)
    return 0;
  ptr_cmd->uint4_a = gl::GL_VERTEX_SHADER;
  ptr_cmd->ptr_sync = &request_vs;
  queue_.CommitBare();
  if (!WaitDone(request_vs.sync.sem_done))
    return 0;
  const std::uint32_t uint4_vs = request_vs.uint4_value;

  ptr_cmd = queue_.Reserve(GfxCmdKind::ShaderSource, static_cast<std::uint32_t>(str_vertex.size() + 1));
  ptr_cmd->uint4_a = uint4_vs;
  queue_.Commit(std::string(str_vertex).c_str());
  ptr_cmd = queue_.Reserve(GfxCmdKind::CompileShader, 0);
  ptr_cmd->uint4_a = uint4_vs;
  queue_.CommitBare();

  GfxScalarRequest request_compile;
  ptr_cmd = queue_.Reserve(GfxCmdKind::GetShaderStatus, 0);
  ptr_cmd->uint4_a = uint4_vs;
  ptr_cmd->ptr_sync = &request_compile;
  queue_.CommitBare();
  if (!WaitDone(request_compile.sync.sem_done))
    return 0;
  if (request_compile.uint4_value == 0)
    return 0;

  GfxScalarRequest request_fs;
  ptr_cmd = queue_.Reserve(GfxCmdKind::CreateShader, 0);
  ptr_cmd->uint4_a = gl::GL_FRAGMENT_SHADER;
  ptr_cmd->ptr_sync = &request_fs;
  queue_.CommitBare();
  if (!WaitDone(request_fs.sync.sem_done))
    return 0;
  const std::uint32_t uint4_fs = request_fs.uint4_value;

  ptr_cmd = queue_.Reserve(GfxCmdKind::ShaderSource, static_cast<std::uint32_t>(str_fragment.size() + 1));
  ptr_cmd->uint4_a = uint4_fs;
  queue_.Commit(std::string(str_fragment).c_str());
  ptr_cmd = queue_.Reserve(GfxCmdKind::CompileShader, 0);
  ptr_cmd->uint4_a = uint4_fs;
  queue_.CommitBare();

  ptr_cmd = queue_.Reserve(GfxCmdKind::GetShaderStatus, 0);
  ptr_cmd->uint4_a = uint4_fs;
  ptr_cmd->ptr_sync = &request_compile;
  queue_.CommitBare();
  if (!WaitDone(request_compile.sync.sem_done))
    return 0;
  if (request_compile.uint4_value == 0)
    return 0;

  GfxScalarRequest request_program;
  ptr_cmd = queue_.Reserve(GfxCmdKind::CreateProgramId, 0);
  ptr_cmd->ptr_sync = &request_program;
  queue_.CommitBare();
  if (!WaitDone(request_program.sync.sem_done))
    return 0;
  const std::uint32_t uint4_program = request_program.uint4_value;

  ptr_cmd = queue_.Reserve(GfxCmdKind::AttachShader, 0);
  ptr_cmd->uint4_a = uint4_program;
  ptr_cmd->uint4_b = uint4_vs;
  queue_.CommitBare();
  ptr_cmd = queue_.Reserve(GfxCmdKind::AttachShader, 0);
  ptr_cmd->uint4_a = uint4_program;
  ptr_cmd->uint4_b = uint4_fs;
  queue_.CommitBare();
  ptr_cmd = queue_.Reserve(GfxCmdKind::LinkProgram, 0);
  ptr_cmd->uint4_a = uint4_program;
  queue_.CommitBare();

  ptr_cmd = queue_.Reserve(GfxCmdKind::GetProgramStatus, 0);
  ptr_cmd->uint4_a = uint4_program;
  ptr_cmd->ptr_sync = &request_program;
  queue_.CommitBare();
  if (!WaitDone(request_program.sync.sem_done))
    return 0;
  return request_program.uint4_value != 0 ? uint4_program : 0;
}

std::uint32_t RenderThread::QueryFboStatus() {
  GfxScalarRequest request;
  if (GfxCmd* ptr_cmd = queue_.Reserve(GfxCmdKind::QueryFboStatus, 0); ptr_cmd != nullptr) {
    ptr_cmd->ptr_sync = &request;
    queue_.CommitBare();
    if (!WaitDone(request.sync.sem_done))
      return 0;
    return request.uint4_value;
  }
  return 0;
}

bool RenderThread::ReadPixels(std::int32_t int4_x, std::int32_t int4_y, std::int32_t int4_w,
                               std::int32_t int4_h, void* ptr_dst) {
  GfxReadbackRequest request;
  request.ptr_dst = ptr_dst;
  if (GfxCmd* ptr_cmd = queue_.Reserve(GfxCmdKind::ReadPixels, 0); ptr_cmd != nullptr) {
    ptr_cmd->uint4_a = static_cast<std::uint32_t>(int4_x);
    ptr_cmd->uint4_b = static_cast<std::uint32_t>(int4_y);
    ptr_cmd->uint4_c = static_cast<std::uint32_t>(int4_w);
    ptr_cmd->uint4_d = static_cast<std::uint32_t>(int4_h);
    ptr_cmd->ptr_sync = &request;
    queue_.CommitBare();
    return WaitDone(request.sync.sem_done);
  }
  return false;
}

bool RenderThread::GetTexImage(std::size_t, void* ptr_dst) {

  GfxReadbackRequest request;
  request.ptr_dst = ptr_dst;
  if (GfxCmd* ptr_cmd = queue_.Reserve(GfxCmdKind::GetTexImage, 0); ptr_cmd != nullptr) {
    ptr_cmd->ptr_sync = &request;
    queue_.CommitBare();
    return WaitDone(request.sync.sem_done);
  }
  return false;
}

std::uint32_t RenderThread::GetActiveUniformCount(std::uint32_t uint4_program) {
  GfxScalarRequest request;
  if (GfxCmd* ptr_cmd = queue_.Reserve(GfxCmdKind::GetProgramActiveUniforms, 0); ptr_cmd != nullptr) {
    ptr_cmd->uint4_a = uint4_program;
    ptr_cmd->ptr_sync = &request;
    queue_.CommitBare();
    if (!WaitDone(request.sync.sem_done))
      return 0;
    return request.uint4_value;
  }
  return 0;
}

bool RenderThread::GetActiveUniformAt(std::uint32_t uint4_program, std::uint32_t uint4_index,
                                       GfxUniformRequest& request_out) {

  request_out.uint4_type = 0;
  request_out.int4_location = 0;
  request_out.str_name[0] = '\0';
  if (GfxCmd* ptr_cmd = queue_.Reserve(GfxCmdKind::GetActiveUniformAt, 0); ptr_cmd != nullptr) {
    ptr_cmd->uint4_a = uint4_program;
    ptr_cmd->uint4_b = uint4_index;
    ptr_cmd->ptr_sync = &request_out;
    queue_.CommitBare();
    return WaitDone(request_out.sync.sem_done);
  }
  return false;
}

void RenderThread::ReadViewport(std::int32_t (&arr_viewport_out)[4]) {
  GfxNameRequest request;
  if (GfxCmd* ptr_cmd = queue_.Reserve(GfxCmdKind::GetViewport, 0); ptr_cmd != nullptr) {
    ptr_cmd->ptr_sync = &request;
    queue_.CommitBare();
    if (!WaitDone(request.sync.sem_done))
      return;
    for (auto i = 0; i < 4; ++i)
      arr_viewport_out[i] = static_cast<std::int32_t>(request.uint4_ids[i]);
  }
}

}
