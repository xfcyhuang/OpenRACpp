// UPSTREAM: OpenRA.Platforms.Default/ThreadedGraphicsContext.cs @7d57605 L188-820(渲染线程宿主)
// OPT-A5(docs/OPTIMIZATION_TRACKER.md):统一线程模型 —— 渲染线程永远存在并独占 GL 上下文
// (上游 Windows 窗口模式禁用渲染线程的特例不复刻,Sdl2PlatformWindow.cs L345-353);
// 命令消费见 gfx_command.hpp。OPT-A6:消费侧对 Bind* 类命令做状态 diff(仅变化时发 GL
// 调用),替代上游 Shader.cs L147-172 每次 flush 重播绑定;Debug 构建注册 KHR_debug 回调,
// 全程零 glGetError 轮询。
// The render-thread host. OPT-A5 (docs/OPTIMIZATION_TRACKER.md): a unified
// threading model — the render thread always exists and solely owns the GL
// context (the upstream Windows-windowed special case of
// Sdl2PlatformWindow.cs L345-353 is not replicated); command consumption is
// in gfx_command.hpp. OPT-A6: the consumer state-diffs Bind* commands (GL
// calls only on change), replacing upstream's per-flush rebinding
// (Shader.cs L147-172); Debug builds register a KHR_debug callback and no
// glGetError polling happens anywhere.
#pragma once
import std;

#include "gfx/gfx_command.hpp"
#include "platform/sdl2_window.hpp"

namespace ora::gfx {

/// 资源名往返请求(Gen* 出参 + 完成信号量;命令仅携带指针,零队列内回写)。
/// Resource-name round-trip request (Gen* out-params + completion semaphore;
/// commands carry only the pointer — nothing is written back inside the queue).
struct GfxNameRequest {
  GfxSyncOut sync;
  std::uint32_t uint4_ids[16];
};

/// 标量往返请求(状态/位置/枚举查询)。
/// Scalar round-trip request (status/location/enum queries).
struct GfxScalarRequest {
  GfxSyncOut sync;
  std::uint32_t uint4_value;
};

/// 读回请求(目标指针归调用方;渲染线程 glReadPixels 直写后 release)。
/// Readback request (the destination pointer is caller-owned; the render
/// thread writes via glReadPixels and then releases).
struct GfxReadbackRequest {
  GfxSyncOut sync;
  void* ptr_dst;
};

/// 渲染线程:持有 GL 上下文,消费命令队列,维护绑定状态缓存。
/// The render thread: owns the GL context, consumes the command queue, and
/// maintains the binding-state cache.
class RenderThread {
 public:
  /// 在 window 上创建 GL 上下文并启动线程;size_queue_bytes 须为 2 的幂。
  /// Creates the GL context on the window and starts the thread;
  /// size_queue_bytes must be a power of two.
  explicit RenderThread(platform::Sdl2Window& window, std::size_t size_queue_bytes = 1uz << 22);

  RenderThread(const RenderThread&) = delete;
  RenderThread& operator=(const RenderThread&) = delete;
  ~RenderThread();

  /// 命令队列写端(主线程)。| Command-queue writer side (main thread).
  GfxCommandQueue& queue() { return queue_; }

  /// —— 主线程便捷 API(内部走命令往返)——
  /// —— Main-thread conveniences (round-trips under the hood) ——

  /// 推 Finish 哨兵并等待其执行完毕(帧末/测试同步点)。
  /// Posts a Finish sentinel and waits for it (frame-end/test sync point).
  void FlushAndWait();

  /// 同步申请 n 个资源名(kind ∈ {GenBuffers, GenVertexArrays, GenTextures,
  /// GenFramebuffers});ids 写入 out。
  /// Synchronously generates n resource names; ids are written to out.
  void GenNames(GfxCmdKind kind_gen, std::uint32_t uint4_n, std::uint32_t* ptr_ids_out);

  /// 同步创建并编译一对着色器;失败返回 0 并打印 info log。
  /// Synchronously creates and compiles a shader pair; returns 0 on failure
  /// with the info log printed.
  std::uint32_t CreateProgramFromSources(std::string_view str_vertex, std::string_view str_fragment);

  /// 同步查询当前绑定 FBO 的完整性状态(经 QueryFboStatus 往返)。
  /// Synchronously queries the completeness status of the currently bound FBO.
  std::uint32_t QueryFboStatus();

  /// 同步 glReadPixels(RGBA8/UNSIGNED_BYTE 语义);渲染线程故障时返回 false。
  /// Synchronous glReadPixels (RGBA8/UNSIGNED_BYTE semantics); returns false
  /// when the render thread has failed.
  bool ReadPixels(std::int32_t int4_x, std::int32_t int4_y, std::int32_t int4_w, std::int32_t int4_h,
                  void* ptr_dst);

  /// 渲染线程是否已就绪(GL 加载完毕)或已失败(上下文/加载失败)。
  /// Whether the render thread is ready (GL loaded) or has failed
  /// (context/loading failure).
  bool b_thread_failed() const { return b_thread_failed_.load(std::memory_order_acquire); }
  bool b_thread_ready() const { return b_thread_ready_.load(std::memory_order_acquire); }

 private:
  void Run();
  bool WaitDone(std::binary_semaphore& sem_done);

  platform::Sdl2Window& window_;
  GfxCommandQueue queue_;
  void* ptr_context_ = nullptr;       // SDL_GLContext(渲染线程所有)
  std::jthread thread_render_;
  std::atomic<bool> b_thread_failed_{false};
  std::atomic<bool> b_thread_ready_{false};
};

}  // namespace ora::gfx
