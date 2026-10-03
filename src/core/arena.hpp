// UPSTREAM: NONE —— PORTING_PLAN §4.5 自有内存基础设施:C# GC 的分区替代,上游无对应物;NONE = C++ 侧自有设计,见 tools/upstream_check.py 登记形式
// 两个分配器对应 PORTING_PLAN §4.5 的两个区域:
//   FrameArena —— 帧临时区(渲染收集列表/排序 key/每 tick 临时),线性 bump,
//     帧末 reset 归零;只接受平凡可析构类型(reset 不调析构)。
//   WorldArena —— 每局世界区(actor/trait/activity/effect),chunk 链 bump +
//     析构记录(规范允许的 arena 例外:非平凡析构由登记表逆序调用);
//     destroy() 支持帧末显式销毁(幂等),reset() 保留 chunk 复用,release()
//     整体归还 OS。
// 对齐契约:所有 chunk 基址 64 对齐(AlignedBuffer),chunk 内相对偏移对齐到
// 请求的 2 的幂对齐后,绝对地址即满足该对齐(请求 ≤ 64,断言强制)。
// 单线程约定:sim 主线程与渲染线程各持实例,不做任何加锁(§4.8 并发模型);
// 同步路径禁 shared_ptr 的硬规则由"arena 裸引用"承载(§4.5)。
// Two allocators for two PORTING_PLAN §4.5 regions:
//   FrameArena — the frame-transient area (render collection lists / sort keys /
//     per-tick temporaries): linear bump, reset to zero at frame end; only
//     trivially destructible types are accepted (reset runs no destructors).
//   WorldArena — the per-world area (actor/trait/activity/effect): chunk-chained
//     bump allocation with destructor records (the guideline-sanctioned arena
//     exception: non-trivial destructors are invoked in reverse registration
//     order); destroy() supports idempotent frame-end teardown, reset() keeps
//     chunks for reuse, release() returns everything to the OS.
// Alignment contract: every chunk base is 64-aligned (AlignedBuffer), so an
// in-chunk relative offset aligned to a requested power-of-two alignment yields
// an absolutely aligned address (requests are capped at 64 by assertion).
// Single-threaded by convention: the sim thread and the render thread each own
// their instances, with no locking whatsoever (§4.8 concurrency model); the
// "no shared_ptr on the synced path" hard rule is carried by bare arena
// references (§4.5).
#pragma once
import std;
#include <cassert>  // assert 为宏,规范允许的 import/#include 并用正形态 | assert is a macro; the spec-sanctioned mixed form

namespace ora {

/// 64 对齐的裸缓冲:chunk 基址对齐是"chunk 内相对对齐 → 绝对对齐"成立的前提。
/// A 64-aligned raw buffer: chunk-base alignment is the precondition for
/// in-chunk relative alignment to carry over to absolute alignment.
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

/// chunk 链的共享外壳:缓冲 + 容量 + 水位。
/// The shared chunk-chain shell: buffer + capacity + watermark.
struct ArenaChunk {
  AlignedBuffer buf;
  std::size_t size_used = 0;
};

/// 在尾块内做一次对齐 bump;放不下则先增长一块再 bump。
/// One aligned bump inside the tail chunk; grows a chunk first if it cannot fit.
inline void* ChunkBump(std::vector<ArenaChunk>& vec_chunks, std::size_t size_bytes, std::size_t size_align) {
  assert(!vec_chunks.empty());  // 构造函数保证首块存在 | constructors guarantee the first chunk
  assert(size_align != 0 && (size_align & (size_align - 1)) == 0);  // 2 的幂 | power of two
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

/// 帧临时线性分配器(§4.5 帧临时区)。非平凡可析构类型在此编译期拒绝。
/// Frame-transient linear allocator (§4.5 frame area). Non-trivially
/// destructible types are rejected at compile time.
class FrameArena {
 public:
  /// 构造并立即持有首块;块大小即后续增长粒度。
  /// Constructs and immediately owns the first chunk; the chunk size is the
  /// subsequent growth granularity.
  explicit FrameArena(std::size_t size_chunk) {
    vec_chunks_.push_back(ArenaChunk{AlignedBuffer(size_chunk), 0});
  }

  FrameArena(const FrameArena&) = delete;
  FrameArena& operator=(const FrameArena&) = delete;

  /// 原始分配(对齐向上取整;不做类型构造)。
  /// Raw allocation (alignment rounded up; no type construction).
  void* Allocate(std::size_t size_bytes, std::size_t size_align) {
    return ChunkBump(vec_chunks_, size_bytes, size_align);
  }

  /// 构造单个对象(仅平凡可析构;placement new,帧末随 reset 蒸发)。
  /// Constructs a single object (trivially destructible only; placement new,
  /// evaporates with reset at frame end).
  template <class T, class... Ts>
  T* Create(Ts&&... args) {
    static_assert(std::is_trivially_destructible_v<T>,
                  "FrameArena 只接受平凡可析构类型;非平凡类型属 WorldArena 域 "
                  "| FrameArena accepts only trivially destructible types; "
                  "non-trivial types belong to WorldArena");
    return new (Allocate(sizeof(T), alignof(T))) T(std::forward<Ts>(args)...);
  }

  /// 构造连续数组(仅平凡可析构;不值初始化,元素须随即写入)。
  /// Constructs a contiguous array (trivially destructible only;
  /// no value-initialization, elements must be written right away).
  template <class T>
  T* CreateArray(std::size_t size_count) {
    static_assert(std::is_trivially_destructible_v<T>,
                  "FrameArena 只接受平凡可析构类型 | FrameArena accepts only "
                  "trivially destructible types");
    return static_cast<T*>(Allocate(sizeof(T) * size_count, alignof(T)));
  }

  /// 当前水位(绝对已用字节;rewind 的锚点)。
  /// Current watermark (absolute bytes in use; the anchor for rewind).
  std::size_t Mark() const { return CurrentAbsoluteUsed(); }

  /// 回退到水位(只回退各块水位,不析构不清内存,不收缩块结构 —— 保证
  /// Mark/Rewind 水位语义自洽;块收缩统一由帧末 Reset 做)。
  /// Rewinds to a watermark (per-chunk watermarks only; no destructors, no
  /// wiping, no chunk shrinking — keeps the Mark/Rewind watermark semantics
  /// self-consistent; chunk shrinking happens uniformly at frame-end Reset).
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

  /// 帧末重置:水位归零,块内存保留复用。
  /// Frame-end reset: watermarks to zero, chunk memory kept for reuse.
  void Reset() {
    for (ArenaChunk& chunk : vec_chunks_)
      chunk.size_used = 0;
    // 保留首块,归还增长块(下帧按需再长;pop 路径不要求默认构造)
    // Keep the first chunk; return grown ones (regrow on demand next frame;
    // the pop path requires no default constructor).
    while (vec_chunks_.size() > 1)
      vec_chunks_.pop_back();
  }

  /// 已用字节数(调试/水位监控)。
  /// Bytes in use (debugging / watermark monitoring).
  std::size_t size_used() const { return CurrentAbsoluteUsed(); }

 private:
  std::size_t CurrentAbsoluteUsed() const {
    std::size_t size_total = 0;
    for (const ArenaChunk& chunk : vec_chunks_)
      size_total += chunk.size_used;
    return size_total;
  }

  std::vector<ArenaChunk> vec_chunks_;  // chunk 链(首块常驻) | chunk chain (first chunk resident)
};

/// 每局世界 arena(§4.5 每局世界区)。chunk 链 bump + 非平凡析构登记(逆序);
/// 平凡可析构类型零登记开销。
/// The per-world arena (§4.5 per-world area). Chunk-chained bump allocation
/// with reverse-order destructor registration for non-trivial types; trivially
/// destructible types pay no registration.
class WorldArena {
 public:
  explicit WorldArena(std::size_t size_chunk) {
    vec_chunks_.push_back(ArenaChunk{AlignedBuffer(size_chunk), 0});
  }

  WorldArena(const WorldArena&) = delete;
  WorldArena& operator=(const WorldArena&) = delete;

  /// 构造对象:平凡可析构直接 bump;否则登记析构,reset 时逆序调用。
  /// Constructs an object: trivially destructible types bump directly;
  /// otherwise the destructor is registered and invoked in reverse order at reset.
  template <class T, class... Ts>
  T* Create(Ts&&... args) {
    void* ptr_raw = ChunkBump(vec_chunks_, sizeof(T), alignof(T));
    T* ptr_result = new (ptr_raw) T(std::forward<Ts>(args)...);
    if constexpr (!std::is_trivially_destructible_v<T>) {
      vec_dtors_.push_back(DtorRec{ptr_result, [](void* p) { static_cast<T*>(p)->~T(); }});
    }
    return ptr_result;
  }

  /// 显式析构(帧末销毁语义,幂等):调用析构并在登记表置空,reset 不再重复。
  /// Explicit destruction (frame-end teardown semantics, idempotent): invokes
  /// the destructor and blanks its record so reset will not repeat it.
  /// 登记表线性查找;帧末销毁量级小,若 Phase 5 成为热点见 OPT-A9 的句柄化方案。
  /// Linear scan of the record table; frame-end teardown volumes are small —
  /// see the handle-based plan in OPT-A9 if this ever becomes hot in Phase 5.
  template <class T>
  void Destroy(T* ptr_object) {
    if (ptr_object == nullptr)
      return;
    if constexpr (!std::is_trivially_destructible_v<T>) {
      const auto it = std::find_if(vec_dtors_.begin(), vec_dtors_.end(),
                                    [ptr_object](const DtorRec& rec) { return rec.ptr == ptr_object; });
      assert(it != vec_dtors_.end());  // 未登记即销毁 = 破坏所有权约定 | destroying an unregistered object breaks the ownership contract
      if (it->fn_dtor == nullptr)
        return;  // 已显式析构:幂等直接返回 | already destroyed: idempotent early-out
      it->fn_dtor = nullptr;
    }
    ptr_object->~T();
  }

  /// 局末重置:逆序调用未显式析构的析构函数,水位归零;chunk 内存保留复用
  /// (shellmap/重开局频繁,避免反复向 OS 要大块)。
  /// End-of-round reset: invokes the not-yet-destroyed destructors in reverse
  /// order and zeroes the watermarks; chunk memory is kept for reuse (the
  /// shellmap/restart path is frequent — avoid repeated large OS allocations).
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

  /// 整体释放(切 mod/进程退出):析构全部并归还所有 chunk。
  /// Full release (mod switch / process exit): destroys everything and
  /// returns all chunks.
  void Release() {
    Reset();
    vec_chunks_.clear();
    vec_chunks_.push_back(ArenaChunk{AlignedBuffer(kReleaseSentinelChunk), 0});
  }

  /// 已用字节数(调试)。
  /// Bytes in use (debugging).
  std::size_t size_used() const {
    std::size_t size_total = 0;
    for (const ArenaChunk& chunk : vec_chunks_)
      size_total += chunk.size_used;
    return size_total;
  }

 private:
  static constexpr std::size_t kReleaseSentinelChunk = 64;  // Release 后保留最小块,免空判 | minimal chunk kept after Release to avoid null checks

  struct DtorRec {
    void* ptr;
    void (*fn_dtor)(void*);  // 置空 = 已显式析构 | null = already explicitly destroyed
  };

  std::vector<ArenaChunk> vec_chunks_;  // chunk 链 | chunk chain
  std::vector<DtorRec> vec_dtors_;      // 非平凡析构登记(构造序) | non-trivial destructor records (construction order)
};

}  // namespace ora
