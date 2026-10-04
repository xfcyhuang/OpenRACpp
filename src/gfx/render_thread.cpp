// UPSTREAM: OpenRA.Platforms.Default/ThreadedGraphicsContext.cs @7d57605 L188-820(线程循环与 GL 分发)
// 命令消费的 GL 分发 + 状态 diff(绑定缓存:program/fbo/vao/纹理单元)。
// The GL dispatch of command consumption plus the state diff (binding caches:
// program/fbo/vao/texture units).
import std;
#include <cstdio>  // stderr 为宏,不穿越 import 边界(规范允许的并用正形态) | stderr is a macro; the spec-sanctioned mixed form

#include <SDL2/SDL.h>  // 第三方 C 头(std_import_check 白名单登记) | third-party C header (registered in the std_import_check whitelist)

#include "gfx/render_thread.hpp"
#include "platform/gl_loader.hpp"

namespace ora::gfx {

namespace {

/// KHR_debug 回调(OPT-A6:替代 glGetError 轮询;仅 Debug 构建注册)。
/// KHR_debug callback (OPT-A6: replaces glGetError polling; Debug builds only).
void OnDebugMessage(gl::GLenum, gl::GLenum, gl::GLuint, gl::GLenum, gl::GLsizei,
                               const gl::GLchar* str_message, const void*) {
  std::println(stderr, "KHR_debug: {}", str_message != nullptr ? str_message : "<null>");
}

/// 消费侧绑定状态缓存(A6:仅变化时发 GL 调用)。
/// Consumer-side binding-state cache (A6: GL calls only on change).
struct RenderState {
  gl::GLuint uint4_program = 0;
  gl::GLuint uint4_framebuffer = 0;
  gl::GLuint uint4_vertex_array = 0;
  gl::GLuint uint4_texture[8] = {};  // 各纹理单元当前绑定 | per-texture-unit binding
};

void ExecuteCommand(const GfxCmd& cmd, const std::byte* ptr_payload, RenderState& state) {
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
      // uint4_a = unit,uint4_b = target,uint4_c = id(见 GfxCmdKind 注释)
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
    case GfxCmdKind::DeleteTextures:
      g::DeleteTextures(static_cast<gl::GLsizei>(cmd.size_payload / 4),
                        reinterpret_cast<const gl::GLuint*>(ptr_payload));
      break;
    case GfxCmdKind::DeleteFramebuffers:
      g::DeleteFramebuffers(static_cast<gl::GLsizei>(cmd.size_payload / 4),
                            reinterpret_cast<const gl::GLuint*>(ptr_payload));
      break;
    case GfxCmdKind::BindBuffer:
      g::BindBuffer(static_cast<gl::GLenum>(cmd.uint4_a), cmd.uint4_b);
      break;
    case GfxCmdKind::BufferData:
      g::BufferData(static_cast<gl::GLenum>(cmd.uint4_a), static_cast<gl::GLsizeiptr>(cmd.uint4_b),
                    ptr_payload, static_cast<gl::GLenum>(cmd.uint4_c));
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
    case GfxCmdKind::TexImage2D:
      // 约定:target=uint4_a,w=uint4_b,h=uint4_c,internal=uint4_d;fmt/type 固定
      // RGBA/UNSIGNED_BYTE(第一批 RGBA8 路线;RGBA16F 调色板随 A7 批次)。
      g::TexImage2D(static_cast<gl::GLenum>(cmd.uint4_a), 0, static_cast<gl::GLint>(cmd.uint4_d),
                    static_cast<gl::GLsizei>(cmd.uint4_b), static_cast<gl::GLsizei>(cmd.uint4_c),
                    0, g::GL_RGBA, g::GL_UNSIGNED_BYTE, ptr_payload);
      break;
    case GfxCmdKind::TexSubImage2D:
      // 约定:target=uint4_a,x=uint4_b,y=uint4_c,w=uint4_d,h=float_a 位模式。
      g::TexSubImage2D(static_cast<gl::GLenum>(cmd.uint4_a), 0, static_cast<gl::GLint>(cmd.uint4_b),
                       static_cast<gl::GLint>(cmd.uint4_c), static_cast<gl::GLsizei>(cmd.uint4_d),
                       static_cast<gl::GLsizei>(std::bit_cast<std::uint32_t>(cmd.float_a)),
                       g::GL_RGBA, g::GL_UNSIGNED_BYTE, ptr_payload);
      break;
    case GfxCmdKind::FramebufferTexture2D:
      // uint4_a = fbo,uint4_b = attachment,uint4_c = tex(操作前须绑定该 fbo)
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
                             reinterpret_cast<const void*>(static_cast<std::uintptr_t>(cmd.float_a)));
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
      g::DrawElements(static_cast<gl::GLenum>(cmd.uint4_a), static_cast<gl::GLsizei>(cmd.uint4_b),
                      static_cast<gl::GLenum>(cmd.uint4_c),
                      reinterpret_cast<const void*>(static_cast<std::uintptr_t>(cmd.uint4_d)));
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
    case GfxCmdKind::Shutdown:
      break;  // 由 Run() 循环处理 | handled by the Run() loop
  }
}

}  // namespace

RenderThread::RenderThread(platform::Sdl2Window& window, std::size_t size_queue_bytes)
    : window_(window), queue_(size_queue_bytes) {
  // GL 上下文在本线程创建(构造即渲染线程?不:构造发生在主线程,上下文与线程在
  // thread_render_ 内创建/绑定 —— jthread 的入口即渲染线程)
  // The GL context is created inside the render thread (the jthread entry).
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

  RenderState state;
  for (;;) {
    std::uint32_t uint4_record = 0;
    const GfxCmd* ptr_cmd = queue_.FetchNext(uint4_record);
    if (ptr_cmd == nullptr)
      break;
    const bool b_shutdown = ptr_cmd->kind == GfxCmdKind::Shutdown;
    ExecuteCommand(*ptr_cmd, GfxCommandQueue::PayloadOf(ptr_cmd), state);
    queue_.Skip(uint4_record);
    if (b_shutdown)
      break;
  }

  window_.MakeGlCurrent(nullptr);
  queue_.Stop();
}

bool RenderThread::WaitDone(std::binary_semaphore& sem_done) {
  // 渲染线程故障时信号量永不释放:带超时轮询 + 失败感知
  // On render-thread failure the semaphore is never released: poll with a
  // timeout plus failure awareness.
  while (!sem_done.try_acquire_for(std::chrono::milliseconds(200))) {
    if (b_thread_failed_.load(std::memory_order_acquire)) {
      // 给在途命令一点排空时间,再判定失败
      // Allow in-flight commands a moment to drain before declaring failure.
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

  ptr_cmd = queue_.Reserve(GfxCmdKind::ShaderSource, static_cast<std::uint16_t>(str_vertex.size() + 1));
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

  ptr_cmd = queue_.Reserve(GfxCmdKind::ShaderSource, static_cast<std::uint16_t>(str_fragment.size() + 1));
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

}  // namespace ora::gfx
