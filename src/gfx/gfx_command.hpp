#pragma once
import std;
#include <cassert>

namespace ora::gfx {

enum class GfxCmdKind : std::uint8_t {
  Clear,
  SetViewport,
  SetScissor,
  DisableScissor,
  BindFramebuffer,
  BindProgram,
  BindTexture,
  Uniform1i,
  Uniform2f,
  Uniform4fv,
  UniformMatrix4fv,
  GenBuffers,
  DeleteBuffers,
  BindBuffer,
  BufferData,
  BufferSubData,
  GenVertexArrays,
  DeleteVertexArrays,
  BindVertexArray,
  GenTextures,
  DeleteTextures,
  TexImage2D,
  TexSubImage2D,
  GenFramebuffers,
  DeleteFramebuffers,
  FramebufferTexture2D,
  CreateShader,
  ShaderSource,
  CompileShader,
  GetShaderStatus,
  CreateProgramId,
  AttachShader,
  BindAttribLocation,
  LinkProgram,
  GetProgramStatus,
  GetUniformLocation,
  VertexAttribPointer,
  VertexAttribIPointer,
  EnableVertexAttribArray,
  DisableVertexAttribArray,
  QueryFboStatus,
  DrawArrays,
  DrawElements,
  ReadPixels,
  Finish,
  Shutdown,

  Uniform1f,
  Uniform3f,
  Uniform1fv,
  Uniform2fv,
  Uniform3fv,
  PixelStorei,
  TexParameteri,
  CopyTexImage2D,
  GetTexImage,
  GetProgramActiveUniforms,
  GetActiveUniformAt,
  BindFragDataLocation,
  DeleteProgram,
  DeleteShader,

  SetBlendMode,
  EnableDepthTest,
  DisableDepthTest,
  ClearDepth,
  GenRenderbuffers,
  DeleteRenderbuffers,
  RenderbufferStorage,
  FramebufferRenderbuffer,
  BufferStoragePersistent,
  WritePersistent,
  FencePersistentSlot,
  DeletePersistentBuffer,
  ConfigureVao,
  GetViewport,
  Present,
};

struct VaoAttribDesc {
  std::uint32_t uint4_index;
  std::uint32_t uint4_size;
  std::uint32_t uint4_type;
  std::uint32_t b_integer;
  std::uint32_t uint4_stride;
  std::uint32_t uint4_offset;
};

struct GfxCmd {
  GfxCmdKind kind;
  std::uint8_t b_depth;
  std::uint8_t pad_unused;
  std::uint32_t size_payload;
  std::uint32_t uint4_a;
  std::uint32_t uint4_b;
  std::uint32_t uint4_c;
  std::uint32_t uint4_d;
  float float_a;
  float float_b;
  float float_c;
  float float_d;
  void* ptr_sync;
};

static_assert(sizeof(GfxCmd) == 48);

struct GfxSyncOut {
  std::binary_semaphore sem_done{0};
};

class GfxCommandQueue {
 public:

  explicit GfxCommandQueue(std::size_t size_capacity_bytes)
      : vec_bytes_(size_capacity_bytes),
        size_mask_(size_capacity_bytes - 1) {
    assert(size_capacity_bytes != 0 && (size_capacity_bytes & size_mask_) == 0);
  }

  GfxCommandQueue(const GfxCommandQueue&) = delete;
  GfxCommandQueue& operator=(const GfxCommandQueue&) = delete;

  GfxCmd* Reserve(GfxCmdKind kind, std::uint32_t size_payload) {
    const std::size_t size_payload_aligned = (size_payload + 7) & ~std::size_t{7};

    const std::size_t size_record = kSizeHeader + sizeof(GfxCmd) + size_payload_aligned;
    assert(size_record <= vec_bytes_.size());

    std::uint64_t uint8_head = uint8_head_.load(std::memory_order_relaxed);
    for (;;) {
      const std::uint64_t uint8_tail = uint8_tail_.load(std::memory_order_acquire);
      const std::size_t size_offset = uint8_head & size_mask_;
      const bool b_fits_tail_room = size_offset + size_record <= vec_bytes_.size();
      const bool b_fits_free = (uint8_head - uint8_tail) + size_record <= vec_bytes_.size();

      if (b_fits_tail_room && b_fits_free)
        break;

      if (!b_fits_tail_room && vec_bytes_.size() - size_offset >= kSizeHeader &&
          (uint8_head - uint8_tail) + kSizeHeader <= vec_bytes_.size()) {

        const std::uint32_t uint4_pad = kSizeHeader;
        std::memcpy(vec_bytes_.data() + size_offset, &uint4_pad, 4);
        const std::uint64_t uint8_head_jumped = (uint8_head + kSizeHeader + size_mask_) & ~static_cast<std::uint64_t>(size_mask_);
        uint8_head_.store(uint8_head_jumped, std::memory_order_release);
        uint8_head_.notify_one();
        uint8_head = uint8_head_jumped;
        continue;
      }

      if (b_stopped_.load(std::memory_order_acquire))
        return nullptr;
      uint8_tail_.wait(uint8_tail);
      uint8_head = uint8_head_.load(std::memory_order_relaxed);
    }

    const std::size_t size_offset = uint8_head & size_mask_;
    const std::uint32_t uint4_record_size = static_cast<std::uint32_t>(size_record);
    std::memcpy(vec_bytes_.data() + size_offset, &uint4_record_size, 4);

    GfxCmd* ptr_cmd = std::launder(reinterpret_cast<GfxCmd*>(vec_bytes_.data() + size_offset + kSizeHeader));
    *ptr_cmd = GfxCmd{};
    ptr_cmd->kind = kind;
    ptr_cmd->size_payload = size_payload;
    ptr_payload_ = vec_bytes_.data() + size_offset + kSizeHeader + sizeof(GfxCmd);
    size_payload_reserved_ = size_payload;
    uint8_head_pending_ = uint8_head + size_record;
    return ptr_cmd;
  }

  void CommitBare() { Commit(nullptr); }

  void Commit(const void* ptr_data) {
    if (size_payload_reserved_ != 0 && ptr_data != nullptr)
      std::memcpy(ptr_payload_, ptr_data, size_payload_reserved_);
    uint8_head_.store(uint8_head_pending_, std::memory_order_release);
    uint8_head_.notify_one();
  }

  const GfxCmd* FetchNext(std::uint32_t& uint4_record_size_out) {
    for (;;) {
      const std::uint64_t uint8_head = uint8_head_.load(std::memory_order_acquire);
      const std::uint64_t uint8_tail = uint8_tail_.load(std::memory_order_relaxed);
      if (uint8_head != uint8_tail) {
        const std::size_t size_offset = uint8_tail & size_mask_;
        std::uint32_t uint4_size = 0;
        std::memcpy(&uint4_size, vec_bytes_.data() + size_offset, 4);
        if (uint4_size == kSizeHeader) {

          const std::uint64_t uint8_tail_now = uint8_tail_.load(std::memory_order_relaxed);
          const std::uint64_t uint8_tail_jumped =
              (uint8_tail_now + kSizeHeader + size_mask_) & ~static_cast<std::uint64_t>(size_mask_);
          uint8_tail_.store(uint8_tail_jumped, std::memory_order_release);
          uint8_tail_.notify_one();
          continue;
        }
        uint4_record_size_out = uint4_size;
        return std::launder(reinterpret_cast<const GfxCmd*>(vec_bytes_.data() + size_offset + kSizeHeader));
      }
      if (b_stopped_.load(std::memory_order_acquire))
        return nullptr;
      uint8_head_.wait(uint8_head);
    }
  }

  void Skip(std::uint32_t uint4_record_size) {
    const std::uint64_t uint8_tail = uint8_tail_.load(std::memory_order_relaxed);
    uint8_tail_.store(uint8_tail + uint4_record_size, std::memory_order_release);
    uint8_tail_.notify_one();
  }

  static const std::byte* PayloadOf(const GfxCmd* ptr_cmd) {
    return reinterpret_cast<const std::byte*>(ptr_cmd) + sizeof(GfxCmd);
  }

  void Stop() {
    b_stopped_.store(true, std::memory_order_release);
    uint8_head_.notify_all();
    uint8_tail_.notify_all();
  }

  std::uint64_t size_published() const { return uint8_head_.load(std::memory_order_acquire); }
  std::uint64_t size_consumed() const { return uint8_tail_.load(std::memory_order_acquire); }

 private:
  static constexpr std::size_t kSizeHeader = 8;

  std::vector<std::byte> vec_bytes_;
  std::size_t size_mask_;
  std::atomic<std::uint64_t> uint8_head_{0};
  std::atomic<std::uint64_t> uint8_tail_{0};
  std::atomic<bool> b_stopped_{false};
  std::byte* ptr_payload_ = nullptr;
  std::size_t size_payload_reserved_ = 0;
  std::uint64_t uint8_head_pending_ = 0;
};

}
