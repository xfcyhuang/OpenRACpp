#pragma once
import std;

#include "gfx/gfx_command.hpp"
#include "platform/sdl2_window.hpp"

namespace ora::gfx {

struct GfxNameRequest {
  GfxSyncOut sync;
  std::uint32_t uint4_ids[16];
};

struct GfxScalarRequest {
  GfxSyncOut sync;
  std::uint32_t uint4_value;
};

struct GfxReadbackRequest {
  GfxSyncOut sync;
  void* ptr_dst;
};

struct GfxUniformRequest {
  GfxSyncOut sync;
  std::uint32_t uint4_type;
  std::int32_t int4_location;
  char str_name[128];
};

class RenderThread {
 public:

  explicit RenderThread(platform::Sdl2Window& window, std::size_t size_queue_bytes = 1uz << 22);

  RenderThread(const RenderThread&) = delete;
  RenderThread& operator=(const RenderThread&) = delete;
  ~RenderThread();

  GfxCommandQueue& queue() { return queue_; }

  void FlushAndWait();

  void GenNames(GfxCmdKind kind_gen, std::uint32_t uint4_n, std::uint32_t* ptr_ids_out);

  std::uint32_t CreateProgramFromSources(std::string_view str_vertex, std::string_view str_fragment);

  std::uint32_t QueryFboStatus();

  bool ReadPixels(std::int32_t int4_x, std::int32_t int4_y, std::int32_t int4_w, std::int32_t int4_h,
                  void* ptr_dst);

  bool GetTexImage(std::size_t size_bytes, void* ptr_dst);

  std::uint32_t GetActiveUniformCount(std::uint32_t uint4_program);

  bool GetActiveUniformAt(std::uint32_t uint4_program, std::uint32_t uint4_index,
                          GfxUniformRequest& request_out);

  void ReadViewport(std::int32_t (&arr_viewport_out)[4]);

  bool b_thread_failed() const { return b_thread_failed_.load(std::memory_order_acquire); }
  bool b_thread_ready() const { return b_thread_ready_.load(std::memory_order_acquire); }

  bool WaitDone(std::binary_semaphore& sem_done);

 private:
  void Run();

  platform::Sdl2Window& window_;
  GfxCommandQueue queue_;
  void* ptr_context_ = nullptr;
  std::jthread thread_render_;
  std::atomic<bool> b_thread_failed_{false};
  std::atomic<bool> b_thread_ready_{false};
};

}
