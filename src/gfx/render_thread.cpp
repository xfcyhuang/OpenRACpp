// UPSTREAM: OpenRA.Platforms.Default/ThreadedGraphicsContext.cs @b6fc03f L188-820(线程循环与 GL 分发)+
//           OpenRA.Platforms.Default/Sdl2GraphicsContext.cs @b6fc03f L159-269(DrawPrimitives/DrawElements/
//           Clear/Enable-DisableDepthBuffer/SetBlendMode 的 GL 序列)
// 命令消费的 GL 分发 + 状态 diff(绑定缓存:program/fbo/vao/纹理单元/blend)。
// SetBlendMode 的 9 模式映射逐上游(Sdl2GraphicsContext.cs L207-269);blend
// 状态机 diff(OPT-A6)只跳过"同模式重复",模式变化时始终发完整 GL 序列 ——
// 与上游每次全发语义等价(GL 状态机收敛到同一状态)。
// The GL dispatch of command consumption plus the state diff (binding caches:
// program/fbo/vao/texture units/blend). The nine blend-mode mappings follow
// upstream one for one (Sdl2GraphicsContext.cs L207-269); the blend
// state-machine diff (OPT-A6) skips only same-mode repeats — a mode change
// always emits the full GL sequence, equivalent to upstream's unconditional
// emission (the GL state machine converges to the same state).
import std;
#include <cstdio>  // stderr 为宏,不穿越 import 边界(规范允许的并用正形态) | stderr is a macro; the spec-sanctioned mixed form

#include <SDL2/SDL.h>  // 第三方 C 头(std_import_check 白名单登记) | third-party C header (registered in the std_import_check whitelist)

#include "gfx/render_thread.hpp"
#include "gfx/sprite.hpp"  // BlendMode(SetBlendMode 消费端映射)| BlendMode (the SetBlendMode consumer mapping)
#include "platform/gl_loader.hpp"

namespace ora::gfx {

namespace {

/// KHR_debug 回调(OPT-A6:替代 glGetError 轮询;仅 Debug 构建注册)。
/// KHR_debug callback (OPT-A6: replaces glGetError polling; Debug builds only).
void OnDebugMessage(gl::GLenum, gl::GLenum, gl::GLuint, gl::GLenum, gl::GLsizei,
                               const gl::GLchar* str_message, const void*) {
  std::println(stderr, "KHR_debug: {}", str_message != nullptr ? str_message : "<null>");
}

/// 一个持久映射缓冲的消费侧记录(OPT-A5):整块映射指针 + 每槽 fence。
/// 槽 fence 在该槽最后一次 DrawElements 提交后由 FencePersistentSlot 创建;
/// 下一次 WritePersistent 落到同一槽时先 ClientWaitSync 再 memcpy —— 全部
/// 发生在渲染线程内,单线程顺序保证无竞争,主线程零等待。
/// The consumer-side record of one persistent-mapped buffer (OPT-A5): the
/// whole-buffer mapping pointer plus per-slot fences. A slot fence is created
/// by FencePersistentSlot after that slot's last DrawElements was submitted;
/// the next WritePersistent onto the same slot ClientWaitSyncs before its
/// memcpy — everything inside the render thread, where single-threaded
/// ordering rules out races, and the main thread never waits.
struct PersistentBufferState {
  std::byte* ptr_mapped = nullptr;
  std::size_t size_total = 0;
  std::size_t size_slot = 0;
  gl::GLsync sync_fences[8] = {};  // 槽 fence(槽数 ≤ 8 由创建方约束)| per-slot fences (slot count ≤ 8 by the creator's contract)
};

/// 消费侧绑定状态缓存(A6:仅变化时发 GL 调用)。
/// Consumer-side binding-state cache (A6: GL calls only on change).
struct RenderState {
  gl::GLuint uint4_program = 0;
  gl::GLuint uint4_framebuffer = 0;
  gl::GLuint uint4_vertex_array = 0;
  gl::GLuint uint4_texture[16] = {};      // 各纹理单元当前绑定(16 ≥ 8 sheet + Palette + ColorShifts;第四批复核修正:第一批的 8 把 sampler 8/9 挡在门外,调色板从未绑上)| per-unit bindings (16 ≥ 8 sheets + Palette + ColorShifts; fourth-batch review fix: the first batch's 8 shut units 8/9 out, so the palette never bound)
  std::int32_t int4_blend_mode = -1;      // 上次 SetBlendMode(-1 = 未知,首条必发)| the last SetBlendMode (-1 = unknown, the first post always emits)
  std::map<gl::GLuint, PersistentBufferState> map_persistent;  // 持久映射缓冲登记 | persistent-mapped buffer registry
};

/// 阻塞等待并回收一个 fence(GPU 完成 or 撤销路径共用)。
/// Blocks on and reclaims one fence (shared by the write and teardown paths).
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
    case GfxCmdKind::DeleteTextures: {
      // 第四批复核修正:删除后 GL 可能复用名字 —— 清绑定缓存,使后续同 id 的
      // BindTexture 必发(diff 以 (unit,id) 为键,名字复用会误判"未变")。
      // Fourth-batch review fix: GL may recycle deleted names — clear the
      // binding caches so a later BindTexture of a recycled id must emit
      // (the diff keys on (unit, id), and name recycling would read as
      // "unchanged").
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
      // VAO 无渲染线程侧登记,直接删除(名字由 GL 统一管理)。
      // VAOs carry no render-thread registry; delete outright (names are
      // GL-managed).
      for (std::uint32_t i = 0; i < cmd.size_payload / 4; ++i) {
        gl::GLuint uint4_vao = reinterpret_cast<const gl::GLuint*>(ptr_payload)[i];
        if (state.uint4_vertex_array == uint4_vao)
          state.uint4_vertex_array = 0;  // 解绑态失效,下一条 Bind 重新发出 | the binding went stale; the next Bind re-emits
        g::DeleteVertexArrays(1, &uint4_vao);
      }
      break;
    case GfxCmdKind::TexImage2D:
      // 约定:target=uint4_a,w=uint4_b,h=uint4_c,internal=uint4_d,fmt=float_a
      // 位模式(0 = 默认 RGBA),type=float_b 位模式(0 = 默认 UNSIGNED_BYTE;
      // RGBA16F 路径传 GL_FLOAT)。payload 为空 = data null(分配不初始化)。
      // Convention: target=uint4_a, w=uint4_b, h=uint4_c, internal=uint4_d,
      // fmt as the float_a bit pattern (0 = RGBA default), type as the float_b
      // bit pattern (0 = UNSIGNED_BYTE default; the RGBA16F path passes
      // GL_FLOAT). An empty payload means a null data pointer (allocate
      // uninitialized).
      g::TexImage2D(static_cast<gl::GLenum>(cmd.uint4_a), 0, static_cast<gl::GLint>(cmd.uint4_d),
                    static_cast<gl::GLsizei>(cmd.uint4_b), static_cast<gl::GLsizei>(cmd.uint4_c),
                    0, cmd.float_a != 0.0f ? std::bit_cast<gl::GLenum>(cmd.float_a) : g::GL_RGBA,
                    cmd.float_b != 0.0f ? std::bit_cast<gl::GLenum>(cmd.float_b) : g::GL_UNSIGNED_BYTE,
                    cmd.size_payload != 0 ? ptr_payload : nullptr);
      break;
    case GfxCmdKind::TexSubImage2D:
      // 约定:target=uint4_a,x=uint4_b,y=uint4_c,w=uint4_d,h/fmt(float_a/b 位模式)。
      // Convention: target=uint4_a, x=uint4_b, y=uint4_c, w=uint4_d, h/fmt as
      // the float_a/float_b bit patterns.
      g::TexSubImage2D(static_cast<gl::GLenum>(cmd.uint4_a), 0, static_cast<gl::GLint>(cmd.uint4_b),
                       static_cast<gl::GLint>(cmd.uint4_c), static_cast<gl::GLsizei>(cmd.uint4_d),
                       static_cast<gl::GLsizei>(std::bit_cast<std::uint32_t>(cmd.float_a)),
                       cmd.float_b != 0.0f ? std::bit_cast<gl::GLenum>(cmd.float_b) : g::GL_RGBA,
                       g::GL_UNSIGNED_BYTE, ptr_payload);
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
      // offset 携带于 float_a 位模式(第二批复核修正:第一批误作数值转换,
      // 大偏移将损失精度;0 偏移行为不变)
      // The offset rides in the float_a bit pattern (second-batch review fix:
      // the first batch mistakenly numeric-converted it, losing precision on
      // large offsets; zero-offset behavior is unchanged).
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
      // float_a 位模式 = basevertex(0 = 无基址,等价 glDrawElements)。
      // The float_a bit pattern = basevertex (0 = no base, equal to
      // glDrawElements).
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
      // target 固定 TEXTURE_2D(Texture.cs 的 PrepareTexture 即如此)
      // The target is fixed TEXTURE_2D (as in Texture.cs's PrepareTexture).
      g::TexParameteri(g::GL_TEXTURE_2D, static_cast<gl::GLenum>(cmd.uint4_a),
                       static_cast<gl::GLint>(cmd.uint4_b));
      break;
    case GfxCmdKind::CopyTexImage2D:
      // 约定:x=uint4_a,y=uint4_b,w=uint4_c,h=uint4_d,internal=float_a 位模式
      // Convention: x=uint4_a, y=uint4_b, w=uint4_c, h=uint4_d, internal as
      // the float_a bit pattern.
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
    // —— 第四批:blend/深度/持久 VB/VAO/FBO depth ——
    // —— Fourth batch: blend/depth/persistent VBs/VAO/FBO depth ——
    case GfxCmdKind::SetBlendMode: {
      // OPT-A6 状态机 diff:同模式直接跳过;模式变化发全序列(上游
      // Sdl2GraphicsContext.cs L207-269 的映射逐分支保留)。
      // OPT-A6 state-machine diff: same mode skips outright; a mode change
      // emits the full sequence (the mapping of Sdl2GraphicsContext.cs
      // L207-269 kept branch for branch).
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
      // 上游 EnableDepthBuffer(Sdl2GraphicsContext.cs L182-191)。
      // Upstream's EnableDepthBuffer (Sdl2GraphicsContext.cs L182-191).
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
      // 约定:internal = uint4_a,w/h = uint4_b/c,rb 名 = uint4_d;target 固定
      // GL_RENDERBUFFER(FrameBuffer.cs L50-54;桌面 GL_DEPTH_COMPONENT 与 ES
      // GL_DEPTH_COMPONENT16 的档位差异随 ES 批次)。
      // Convention: internal = uint4_a, w/h = uint4_b/c, the rb name = uint4_d;
      // the target is fixed GL_RENDERBUFFER (FrameBuffer.cs L50-54; the desktop
      // GL_DEPTH_COMPONENT vs ES GL_DEPTH_COMPONENT16 profile split arrives
      // with the ES batch).
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
      // OPT-A5:glBufferStorage + 整块持久映射(COHERENT 使写入对 GPU 立即可
      // 见,免 glFlushMappedMemoryRange);映射指针只在本线程使用。
      // OPT-A5: glBufferStorage plus a whole-buffer persistent mapping
      // (COHERENT makes writes immediately visible to the GPU, avoiding
      // glFlushMappedMemoryRange); the mapping pointer is used only on this
      // thread.
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
        break;  // 未登记缓冲:命令序破损,防御性丢弃 | unregistered buffer: broken ordering; dropped defensively
      WaitAndDeleteSync(it_buffer->second.sync_fences[cmd.uint4_b % 8]);
      std::memcpy(it_buffer->second.ptr_mapped + cmd.uint4_c, ptr_payload, cmd.size_payload);
      break;
    }
    case GfxCmdKind::FencePersistentSlot: {
      const auto it_buffer = state.map_persistent.find(cmd.uint4_a);
      if (it_buffer == state.map_persistent.end())
        break;
      auto& sync_slot = it_buffer->second.sync_fences[cmd.uint4_b % 8];
      WaitAndDeleteSync(sync_slot);  // 防御:槽轮换约定被破坏时先回收旧 fence | defensive: reclaim the stale fence if the rotation contract broke
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
      // OPT-A6 VAO 缓存:(顶点格式, shader) → VAO;绑定 VB/IB/属性一次固化,
      // 绘制路径只剩 BindVertexArray(消费端已 diff)。GL_ELEMENT_ARRAY_BUFFER
      // 绑定是 VAO 状态的一部分,VAO 内固化的索引绑定正是上游每帧
      // indexBuffer.Bind() 重播的消除。
      // The OPT-A6 VAO cache: (vertex format, shader) → VAO; VB/IB/attributes
      // bake in once, leaving only BindVertexArray on the draw path (already
      // diffed by the consumer). The GL_ELEMENT_ARRAY_BUFFER binding is part
      // of VAO state — baking the index binding into the VAO is precisely
      // what removes upstream's per-flush indexBuffer.Bind() replay.
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
      // GL 上下文归渲染线程 ⇒ 交换后台缓冲也在此(Sdl2GraphicsContext.Present)。
      // The GL context belongs to the render thread, hence the buffer swap
      // too (Sdl2GraphicsContext.Present).
      window.SwapBuffers();
      break;
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

  // 上游 Sdl2GraphicsContext.InitializeOpenGL L46-49:上下文就绪即生成一个
  // 全局 VAO 并保持绑定(Core profile 下 GL_ELEMENT_ARRAY_BUFFER 绑定必须在
  // 非 0 VAO 上,上游的静态索引缓冲上传即依赖它;C++ 侧专属 VAO(OPT-A6)在
  // 此全局 VAO 之外另行生成)。
  // Upstream's Sdl2GraphicsContext.InitializeOpenGL L46-49: a global VAO is
  // generated and kept bound as soon as the context is ready (a
  // GL_ELEMENT_ARRAY_BUFFER bind requires a non-zero VAO under Core; the
  // static index-buffer upload relies on it. The C++-side dedicated VAOs
  // (OPT-A6) are generated on top of this global one).
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
  // size_bytes 仅作文档性参数(桌面 glGetTexImage 无尺寸参数;缓冲区尺寸
  // 由调用方按纹理大小保证),保持与其他读回 API 的签名一致性。
  // size_bytes is documentary only (desktop glGetTexImage takes no size; the
  // buffer sizing is the caller's contract), keeping signature parity with the
  // other readback APIs.
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
  // binary_semaphore 不可拷贝:字段级清零(名字缓冲整块 memset)。
  // binary_semaphore is non-copyable: clear field-wise (the name buffer
  // memset wholesale).
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

}  // namespace ora::gfx
