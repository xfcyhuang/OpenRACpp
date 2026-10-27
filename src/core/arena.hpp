#pragma once
import std;
#include <cassert>

namespace ora {

struct AlignedBuffer {
  static constexpr std::size_t kMaxAlign = 64;

  std::byte* ptr_data;
  std::size_t size_capacity;

  explicit AlignedBuffer(std::size_t size_bytes)
      : ptr_data(static_cast<std::byte*>(::operator new(size_bytes, std::align_val_t(kMaxAlign)))),
        size_capacity(size_bytes) {}
  ~AlignedBuffer() { ::operator delete(ptr_data, std::align_val_t(kMaxAlign)); }

  AlignedBuffer(AlignedBuffer&& other) noexcept
      : ptr_data(std::exchange(other.ptr_data, nullptr)), size_capacity(other.size_capacity) {}
  AlignedBuffer(const AlignedBuffer&) = delete;
  AlignedBuffer& operator=(AlignedBuffer&&) = delete;
  AlignedBuffer& operator=(const AlignedBuffer&) = delete;
};

struct ArenaChunk {
  AlignedBuffer buf;
  std::size_t size_used = 0;
};

inline void* ChunkBump(std::vector<ArenaChunk>& vec_chunks, std::size_t size_bytes, std::size_t size_align) {
  assert(!vec_chunks.empty());
  assert(size_align != 0 && (size_align & (size_align - 1)) == 0);
  assert(size_align <= AlignedBuffer::kMaxAlign);

  const std::size_t size_aligned_used =
      (vec_chunks.back().size_used + size_align - 1) & ~(size_align - 1);
  if (size_aligned_used + size_bytes > vec_chunks.back().buf.size_capacity) {
    const std::size_t size_grow = std::max(vec_chunks.back().buf.size_capacity * 2, size_bytes + size_align);
    vec_chunks.push_back(ArenaChunk{AlignedBuffer(size_grow), 0});
  }

  ArenaChunk& chunk = vec_chunks.back();
  chunk.size_used = (chunk.size_used + size_align - 1) & ~(size_align - 1);
  const std::size_t size_offset = chunk.size_used;
  chunk.size_used += size_bytes;
  return chunk.buf.ptr_data + size_offset;
}

class FrameArena {
 public:

  explicit FrameArena(std::size_t size_chunk) {
    vec_chunks_.push_back(ArenaChunk{AlignedBuffer(size_chunk), 0});
  }

  FrameArena(const FrameArena&) = delete;
  FrameArena& operator=(const FrameArena&) = delete;

  void* Allocate(std::size_t size_bytes, std::size_t size_align) {
    return ChunkBump(vec_chunks_, size_bytes, size_align);
  }

  template <class T, class... Ts>
  T* Create(Ts&&... args) {
    static_assert(std::is_trivially_destructible_v<T>,
                  "FrameArena 只接受平凡可析构类型;非平凡类型属 WorldArena 域 "
                  "| FrameArena accepts only trivially destructible types; "
                  "non-trivial types belong to WorldArena");
    return new (Allocate(sizeof(T), alignof(T))) T(std::forward<Ts>(args)...);
  }

  template <class T>
  T* CreateArray(std::size_t size_count) {
    static_assert(std::is_trivially_destructible_v<T>,
                  "FrameArena 只接受平凡可析构类型 | FrameArena accepts only "
                  "trivially destructible types");
    return static_cast<T*>(Allocate(sizeof(T) * size_count, alignof(T)));
  }

  std::size_t Mark() const { return CurrentAbsoluteUsed(); }

  void Rewind(std::size_t size_mark) {
    assert(size_mark <= CurrentAbsoluteUsed());
    std::size_t size_absolute = CurrentAbsoluteUsed();
    for (std::size_t i = vec_chunks_.size(); i-- > 0 && size_absolute > size_mark;) {
      const std::size_t size_chunk_base = size_absolute - vec_chunks_[i].size_used;
      if (size_mark > size_chunk_base) {
        vec_chunks_[i].size_used = size_mark - size_chunk_base;
        break;
      }
      vec_chunks_[i].size_used = 0;
      size_absolute = size_chunk_base;
    }
  }

  void Reset() {
    for (ArenaChunk& chunk : vec_chunks_)
      chunk.size_used = 0;

    while (vec_chunks_.size() > 1)
      vec_chunks_.pop_back();
  }

  std::size_t size_used() const { return CurrentAbsoluteUsed(); }

 private:
  std::size_t CurrentAbsoluteUsed() const {
    std::size_t size_total = 0;
    for (const ArenaChunk& chunk : vec_chunks_)
      size_total += chunk.size_used;
    return size_total;
  }

  std::vector<ArenaChunk> vec_chunks_;
};

class WorldArena {
 public:
  explicit WorldArena(std::size_t size_chunk) {
    vec_chunks_.push_back(ArenaChunk{AlignedBuffer(size_chunk), 0});
  }

  WorldArena(const WorldArena&) = delete;
  WorldArena& operator=(const WorldArena&) = delete;

  template <class T, class... Ts>
  T* Create(Ts&&... args) {
    void* ptr_raw = ChunkBump(vec_chunks_, sizeof(T), alignof(T));
    T* ptr_result = new (ptr_raw) T(std::forward<Ts>(args)...);
    if constexpr (!std::is_trivially_destructible_v<T>) {
      vec_dtors_.push_back(DtorRec{ptr_result, [](void* p) { static_cast<T*>(p)->~T(); }});
    }
    return ptr_result;
  }

  template <class T>
  void Destroy(T* ptr_object) {
    if (ptr_object == nullptr)
      return;
    if constexpr (!std::is_trivially_destructible_v<T>) {
      const auto it = std::find_if(vec_dtors_.begin(), vec_dtors_.end(),
                                    [ptr_object](const DtorRec& rec) { return rec.ptr == ptr_object; });
      assert(it != vec_dtors_.end());
      if (it->fn_dtor == nullptr)
        return;
      it->fn_dtor = nullptr;
    }
    ptr_object->~T();
  }

  void Reset() {
    for (auto it = vec_dtors_.rbegin(); it != vec_dtors_.rend(); ++it)
      if (it->fn_dtor != nullptr)
        it->fn_dtor(it->ptr);
    vec_dtors_.clear();
    for (ArenaChunk& chunk : vec_chunks_)
      chunk.size_used = 0;
    while (vec_chunks_.size() > 1)
      vec_chunks_.pop_back();
  }

  void Release() {
    Reset();
    vec_chunks_.clear();
    vec_chunks_.push_back(ArenaChunk{AlignedBuffer(kReleaseSentinelChunk), 0});
  }

  std::size_t size_used() const {
    std::size_t size_total = 0;
    for (const ArenaChunk& chunk : vec_chunks_)
      size_total += chunk.size_used;
    return size_total;
  }

 private:
  static constexpr std::size_t kReleaseSentinelChunk = 64;

  struct DtorRec {
    void* ptr;
    void (*fn_dtor)(void*);
  };

  std::vector<ArenaChunk> vec_chunks_;
  std::vector<DtorRec> vec_dtors_;
};

}
