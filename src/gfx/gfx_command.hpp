// UPSTREAM: OpenRA.Platforms.Default/ThreadedGraphicsContext.cs @7d57605 L188-505(命令封送段)
// OPT-A5(docs/OPTIMIZATION_TRACKER.md)重设计:上游以 Post(Action<object>, object) 把每个
// GL 调用装箱成消息 + 全局锁 + Monitor.Pulse(每帧几十到几百次堆分配),本实现改为
// **定长值类型命令 + 内联载荷 blob** 的字节流,SPSC(Lamport)无锁环形缓冲传递:
//   - 零装箱、零委托、零锁(仅空/满时的 atomic wait 唤醒);
//   - 记录布局 [u32 record_size][对齐垫 4B][GfxCmd 定长 48B][payload…(align8)];
//   - 单调递增字节偏移 + 2 的幂容量:写者只发布 head,读者只发布 tail;
//   - 环尾放不下整条时写 4B pad 记录跳到环首;
//   - 同步往返(读回/资源名)内嵌 binary_semaphore 指针于命令。
// 上游 Windows 窗口模式"渲染线程禁用"特例不复刻(统一线程模型)。
// OPT-A5 (docs/OPTIMIZATION_TRACKER.md) redesign: upstream marshals every GL
// call via Post(Action<object>, object) — a boxed message plus a global lock
// plus Monitor.Pulse (tens to hundreds of heap allocations per frame); this
// implementation replaces it with a byte stream of **fixed-size value-type
// commands with inline payload blobs** carried over an SPSC (Lamport)
// lock-free ring:
//   - zero boxing, zero delegates, zero locks (atomic-wait wakeups only when
//     empty/full);
//   - record layout [u32 record_size][4-byte alignment pad][fixed 48-byte GfxCmd][payload… (align8)];
//   - monotonically increasing byte offsets over a power-of-two capacity: the
//     writer publishes only head, the reader only tail;
//   - a 4-byte pad record jumps to the ring start when the tail lacks room;
//   - synchronous round-trips (readbacks/resource names) embed a
//     binary_semaphore pointer in the command.
// The upstream Windows-windowed "render thread disabled" special case is not
// replicated (unified threading model).
#pragma once
import std;
#include <cassert>  // assert 为宏,规范允许的 import/#include 并用正形态 | assert is a macro; the spec-sanctioned mixed form

namespace ora::gfx {

/// 命令种类(与 GfxCmd 载荷字段一一对应;字段语义注释在各枚举值处)。
/// Command kinds (each maps onto GfxCmd payload fields; field semantics noted per enum value).
enum class GfxCmdKind : std::uint8_t {
  Clear,             // float_a..d = RGBA;b_depth = 是否清深度
  SetViewport,       // uint4_a/b/c/d = x,y,w,h
  SetScissor,        // uint4_a/b/c/d = x,y,w,h
  DisableScissor,
  BindFramebuffer,   // uint4_a = id(0 = 默认帧缓冲)
  BindProgram,       // uint4_a = id(0 = 解绑)
  BindTexture,       // uint4_a = unit,uint4_b = target,uint4_c = id
  Uniform1i,         // int(uint4_a) = location,int(uint4_b) = v0
  Uniform2f,         // int(uint4_a) = location,float_a/b
  Uniform4fv,        // int(uint4_a) = location,payload = count×float4
  UniformMatrix4fv,  // int(uint4_a) = location,payload = 16×float
  GenBuffers,        // uint4_a = n;ptr_sync → sem;payload = 出参 u32[](往返)
  DeleteBuffers,     // payload = u32[]
  BindBuffer,        // uint4_a = target,uint4_b = id
  BufferData,        // uint4_a = target,uint4_b = size,usage(uint4_c);payload = 字节
  BufferSubData,     // uint4_a = target,offset/size(uint4_b/c);payload = 字节
  GenVertexArrays,   // uint4_a = n;往返同 GenBuffers
  DeleteVertexArrays,  // 第四批:VAO 缓存的 RAII 析构;payload = u32[]
  BindVertexArray,   // uint4_a = id
  GenTextures,       // uint4_a = n;往返同 GenBuffers
  DeleteTextures,    // payload = u32[]
  TexImage2D,        // target(uint4_a),w/h(uint4_b/c),internal(uint4_d),fmt(float_a 位模式,默认 RGBA);payload = 字节
  TexSubImage2D,     // target(uint4_a),x/y/w(uint4_b/c/d),h 与 fmt(float_a/b 位模式);payload = 字节
  GenFramebuffers,   // uint4_a = n;往返同 GenBuffers
  DeleteFramebuffers,
  FramebufferTexture2D,  // uint4_a = fbo,uint4_b = attachment,uint4_c = tex id
  CreateShader,      // uint4_a = shader 类型;往返出 id
  ShaderSource,      // uint4_a = shader id;payload = NUL 结尾源码
  CompileShader,     // uint4_a = shader id
  GetShaderStatus,   // uint4_a = shader id;往返出 compile status(+log 长度)
  CreateProgramId,   // 往返出 program id
  AttachShader,      // uint4_a = program,uint4_b = shader
  BindAttribLocation, // uint4_a = program,uint4_b = index;payload = NUL 结尾名字
  LinkProgram,       // uint4_a = program
  GetProgramStatus,  // uint4_a = program;往返出 link status
  GetUniformLocation, // uint4_a = program;payload = NUL 结尾名字;往返出 location
  VertexAttribPointer, // uint4_a = index,uint4_b = size,类型(uint4_c);归一化(b_depth);stride(uint4_d)/offset(float_a 位模式)
  VertexAttribIPointer,  // 同上,整数属性路径(无归一化)| as above, the integer-attribute path (no normalization)
  EnableVertexAttribArray,  // uint4_a = index
  DisableVertexAttribArray, // uint4_a = index
  QueryFboStatus,    // 往返出 CheckFramebufferStatus(GL_FRAMEBUFFER) 结果
  DrawArrays,        // uint4_a = mode,uint4_b = first,uint4_c = count
  DrawElements,      // uint4_a = mode,uint4_b = count,uint4_c = type,uint4_d = 偏移;float_a 位模式 = basevertex(第四批:持久 VB 槽基址;0 = 兼容无基址)| float_a bit pattern = basevertex (fourth batch: the persistent-VB slot base; 0 = no base, compatible)
  ReadPixels,        // x/y/w/h(uint4_a..d);ptr_sync → 读回请求(目标指针 + 信号量)
  Finish,            // 哨兵:ptr_sync → sem(同步点)
  Shutdown,          // 渲染线程退出
  // —— 第二批(Shader/Texture 封装)追加 ——
  // —— Appended in the second batch (the Shader/Texture wrappers) ——
  Uniform1f,         // int(uint4_a) = location,float_a
  Uniform3f,         // int(uint4_a) = location,float_a/b/c
  Uniform1fv,        // int(uint4_a) = location,uint4_b = count;payload = float[]
  Uniform2fv,        // 同上 | as above
  Uniform3fv,        // 同上 | as above
  PixelStorei,       // uint4_a = pname,uint4_b = param
  TexParameteri,     // uint4_a = pname,uint4_b = param(target 固定 TEXTURE_2D,上游 Texture.cs 如此)
  CopyTexImage2D,    // x/y(uint4_a/b),w/h(uint4_c/d),internal(float_a 位模式)
  GetTexImage,       // ptr_sync → 读回请求(桌面 glGetTexImage,BGRA/UB,纹理尺寸由调用方保证)
  GetProgramActiveUniforms,   // uint4_a = program;往返出 active uniform 数
  GetActiveUniformAt,  // uint4_a = program,uint4_b = index;ptr_sync → GfxUniformRequest
  BindFragDataLocation,  // uint4_a = program,uint4_b = colorNumber;payload = NUL 结尾名字
  DeleteProgram,     // uint4_a = id
  DeleteShader,      // uint4_a = id
  // —— 第四批(SpriteRenderer + 单级合成 Renderer)追加 ——
  // —— Appended in the fourth batch (SpriteRenderer + the composite Renderer) ——
  SetBlendMode,      // uint4_a = BlendMode 枚举值;消费端 blend 状态机 diff(仅模式变化时发 GL 序列)
  EnableDepthTest,   // 上游 EnableDepthBuffer:清深度 + 开深度测试 + DepthFunc(LEQUAL)
  DisableDepthTest,
  ClearDepth,
  GenRenderbuffers,  // uint4_a = n;往返同 GenBuffers
  DeleteRenderbuffers,  // payload = u32[]
  RenderbufferStorage,  // internal(uint4_a),w/h(uint4_b/c),rb 名(uint4_d);target 固定 GL_RENDERBUFFER
  FramebufferRenderbuffer,  // uint4_a = fbo,uint4_b = attachment,uint4_c = rb id
  BufferStoragePersistent,  // uint4_a = buffer id,uint4_b = 总字节,uint4_c = 槽数;消费端 bind+BufferStorage(MAP_WRITE|PERSISTENT|COHERENT)+整块 MapBufferRange 并登记
  WritePersistent,   // uint4_a = buffer id,uint4_b = 槽序号,uint4_c = 缓冲内字节偏移;payload = 数据;消费端先等该槽 fence 再 memcpy 进映射区
  FencePersistentSlot,  // uint4_a = buffer id,uint4_b = 槽序号;消费端 glFenceSync 记到该槽
  DeletePersistentBuffer,  // uint4_a = buffer id;消费端清 fence/删缓冲/注销
  ConfigureVao,      // uint4_a = vao,uint4_b = vbo,uint4_c = 索引缓冲(0 = 无);payload = VaoAttribDesc[](消费端绑 VAO+VB+IB+属性指针)
  GetViewport,       // 往返:glGetIntegerv(GL_VIEWPORT) → GfxNameRequest.uint4_ids[](FrameBuffer Bind 的保存/恢复)
  Present,           // 渲染线程 SDL_GL_SwapWindow(上下文归渲染线程,主线程不得直调)
};

/// ConfigureVao 的属性描述(payload 元素;定长 24B 便于消费端步进)。
/// One attribute descriptor for ConfigureVao (the payload element; a fixed
/// 24 bytes for straightforward consumer stepping).
struct VaoAttribDesc {
  std::uint32_t uint4_index;        // attribute 索引(BindAttribLocation 决定)
  std::uint32_t uint4_size;         // 分量数 | component count
  std::uint32_t uint4_type;         // GL_FLOAT / GL_UNSIGNED_INT …
  std::uint32_t b_integer;          // 1 = VertexAttribIPointer 路径 | the IPointer path
  std::uint32_t uint4_stride;       // 顶点跨距 | vertex stride
  std::uint32_t uint4_offset;       // 顶点内偏移 | intra-vertex offset
};

/// 定长命令记录(48B;可变数据在 payload)。
/// Fixed-size command record (48B; variable data lives in the payload).
struct GfxCmd {
  GfxCmdKind kind;
  std::uint8_t b_depth;         // Clear 用 | for Clear
  std::uint8_t pad_unused;      // 对齐垫(第四批 size_payload 扩 u32 后遗留)| alignment pad (left over after the fourth batch grew size_payload to u32)
  std::uint32_t size_payload;   // 后跟 payload 字节数(第四批扩 u32:单条 Flush 顶点载荷可达数百 KB)| payload bytes following (widened to u32 in the fourth batch: one flush's vertex payload can reach hundreds of KB)
  std::uint32_t uint4_a;        // 通用载荷字段(按 kind 解释,见枚举注释)
  std::uint32_t uint4_b;
  std::uint32_t uint4_c;
  std::uint32_t uint4_d;
  float float_a;
  float float_b;
  float float_c;
  float float_d;
  void* ptr_sync;               // 同步命令的信号量/出参指针(仅往返命令非空)
};

static_assert(sizeof(GfxCmd) == 48);  // 40B 字段 + 8B 指针对齐 | fields + pointer alignment

/// 同步往返的出参记录(payload 出参 + 完成信号量的成对体;命令只携带指针)。
/// Round-trip out-param record (payload out + completion semaphore pair;
/// commands carry only the pointer).
struct GfxSyncOut {
  std::binary_semaphore sem_done{0};
};

/// SPSC(Lamport)字节环:单写者(主线程)单读者(渲染线程)。
/// SPSC (Lamport) byte ring: single writer (main thread), single reader
/// (render thread).
class GfxCommandQueue {
 public:
  /// size_capacity_bytes 须为 2 的幂。
  /// size_capacity_bytes must be a power of two.
  explicit GfxCommandQueue(std::size_t size_capacity_bytes)
      : vec_bytes_(size_capacity_bytes),
        size_mask_(size_capacity_bytes - 1) {
    assert(size_capacity_bytes != 0 && (size_capacity_bytes & size_mask_) == 0);
  }

  GfxCommandQueue(const GfxCommandQueue&) = delete;
  GfxCommandQueue& operator=(const GfxCommandQueue&) = delete;

  /// —— 写端(仅主线程)——
  /// Reserve 返回可填写的命令引用;payload 经 Commit(data) 追加并发布。
  /// 若等待读者腾出空间期间被 StopReader 中断,返回 nullptr。
  /// —— Writer side (main thread only) ——

  /// Reserve returns a fillable command reference; the payload is appended and
  /// published by Commit(data). Returns nullptr if interrupted by StopReader
  /// while waiting for the reader to free space.
  GfxCmd* Reserve(GfxCmdKind kind, std::uint32_t size_payload) {
    const std::size_t size_payload_aligned = (size_payload + 7) & ~std::size_t{7};
    // 记录 = 8B header(4B 尺寸 + 4B 对齐垫)+ 48B GfxCmd + payload(align8),
    // 总长恒为 8 的倍数 ⇒ 每条记录起点(含环回后)8 对齐 ⇒ GfxCmd/payload
    // 落点 8 对齐(第二批复核修正:第一批 4B header 使 GfxCmd 落点失配 ——
    // Ubsan 实证 UB,Release 向量化后段错误)。
    // Record = an 8-byte header (4-byte size + 4-byte alignment pad) + the
    // 48-byte GfxCmd + payload (align8); the total is always a multiple of 8
    // ⇒ every record start (including after wrap) is 8-aligned ⇒ the
    // GfxCmd/payload landing spots are 8-aligned (second-batch review fix:
    // the first batch's 4-byte header misaligned the GfxCmd spot — UB per
    // Ubsan, segfaulting Release once vectorized).
    const std::size_t size_record = kSizeHeader + sizeof(GfxCmd) + size_payload_aligned;
    assert(size_record <= vec_bytes_.size());  // 单条记录必须能进环 | a record must fit in the ring

    std::uint64_t uint8_head = uint8_head_.load(std::memory_order_relaxed);
    for (;;) {
      const std::uint64_t uint8_tail = uint8_tail_.load(std::memory_order_acquire);
      const std::size_t size_offset = uint8_head & size_mask_;
      const bool b_fits_tail_room = size_offset + size_record <= vec_bytes_.size();
      const bool b_fits_free = (uint8_head - uint8_tail) + size_record <= vec_bytes_.size();

      if (b_fits_tail_room && b_fits_free)
        break;  // 放得下 | fits

      if (!b_fits_tail_room && vec_bytes_.size() - size_offset >= kSizeHeader &&
          (uint8_head - uint8_tail) + kSizeHeader <= vec_bytes_.size()) {
        // 环尾放不下整条:写 8B pad 记录并把 head 跳到环首(保持单调),发布后重试
        // Record cannot fit at the ring end: write an 8-byte pad record, jump
        // head to the ring start (staying monotonic), publish, and retry.
        const std::uint32_t uint4_pad = kSizeHeader;
        std::memcpy(vec_bytes_.data() + size_offset, &uint4_pad, 4);
        const std::uint64_t uint8_head_jumped = (uint8_head + kSizeHeader + size_mask_) & ~static_cast<std::uint64_t>(size_mask_);
        uint8_head_.store(uint8_head_jumped, std::memory_order_release);
        uint8_head_.notify_one();
        uint8_head = uint8_head_jumped;
        continue;
      }

      // 满载或 pad 放不下:等待读者消费 | full (or no pad room): wait for the reader
      if (b_stopped_.load(std::memory_order_acquire))
        return nullptr;
      uint8_tail_.wait(uint8_tail);
      uint8_head = uint8_head_.load(std::memory_order_relaxed);
    }

    const std::size_t size_offset = uint8_head & size_mask_;
    const std::uint32_t uint4_record_size = static_cast<std::uint32_t>(size_record);
    std::memcpy(vec_bytes_.data() + size_offset, &uint4_record_size, 4);  // 尺寸 4B;垫 4B 不必写 | size is 4 bytes; the pad need not be written

    GfxCmd* ptr_cmd = std::launder(reinterpret_cast<GfxCmd*>(vec_bytes_.data() + size_offset + kSizeHeader));
    *ptr_cmd = GfxCmd{};
    ptr_cmd->kind = kind;
    ptr_cmd->size_payload = size_payload;
    ptr_payload_ = vec_bytes_.data() + size_offset + kSizeHeader + sizeof(GfxCmd);
    size_payload_reserved_ = size_payload;              // Commit 拷贝的真实字节数 | real bytes Commit copies
    uint8_head_pending_ = uint8_head + size_record;
    return ptr_cmd;
  }

  /// 无 payload 命令的便捷提交。| Convenience commit for payload-less commands.
  void CommitBare() { Commit(nullptr); }

  /// 追加 payload(可空)并发布 head(release + notify)。
  /// Appends the payload (may be null) and publishes head (release + notify).
  void Commit(const void* ptr_data) {
    if (size_payload_reserved_ != 0 && ptr_data != nullptr)
      std::memcpy(ptr_payload_, ptr_data, size_payload_reserved_);
    uint8_head_.store(uint8_head_pending_, std::memory_order_release);
    uint8_head_.notify_one();
  }

  /// —— 读端(仅渲染线程)——
  /// —— Reader side (render thread only) ——

  /// 取下一条命令;队列空且未停止时阻塞。pad 记录(环尾跳转)被就地跳过。
  /// 返回 nullptr 仅当空且已 Stop。
  /// Fetches the next command, blocking when empty; pad records (ring-end
  /// jumps) are skipped in place. nullptr only when empty and stopped.
  const GfxCmd* FetchNext(std::uint32_t& uint4_record_size_out) {
    for (;;) {
      const std::uint64_t uint8_head = uint8_head_.load(std::memory_order_acquire);
      const std::uint64_t uint8_tail = uint8_tail_.load(std::memory_order_relaxed);
      if (uint8_head != uint8_tail) {
        const std::size_t size_offset = uint8_tail & size_mask_;
        std::uint32_t uint4_size = 0;
        std::memcpy(&uint4_size, vec_bytes_.data() + size_offset, 4);
        if (uint4_size == kSizeHeader) {
          // pad 记录:无命令体;tail 与写侧对称跳到环首(单调绝对偏移取整)
          // Pad record: no command body; tail jumps to the ring start
          // symmetrically with the writer's head jump (monotonic absolute
          // offset rounded up).
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

  /// 消费当前记录(payload 读完之后调用;发布 tail)。
  /// Consumes the current record (after reading its payload; publishes tail).
  void Skip(std::uint32_t uint4_record_size) {
    const std::uint64_t uint8_tail = uint8_tail_.load(std::memory_order_relaxed);
    uint8_tail_.store(uint8_tail + uint4_record_size, std::memory_order_release);
    uint8_tail_.notify_one();
  }

  /// 当前记录的 payload 只读指针(读者,pad 记录不会走到这里)。
  /// Read-only payload pointer of the current record (reader side; pad
  /// records never reach here).
  static const std::byte* PayloadOf(const GfxCmd* ptr_cmd) {
    return reinterpret_cast<const std::byte*>(ptr_cmd) + sizeof(GfxCmd);
  }

  /// 停止:唤醒阻塞的写/读两端(写入返回 nullptr,读取在清空后返回 nullptr)。
  /// Stops the queue: wakes both blocked ends (writes return nullptr, reads
  /// return nullptr once drained).
  void Stop() {
    b_stopped_.store(true, std::memory_order_release);
    uint8_head_.notify_all();
    uint8_tail_.notify_all();
  }

  /// 双端计数(诊断):已发布/已消费字节。
  /// Both-end counters (diagnostics): published/consumed bytes.
  std::uint64_t size_published() const { return uint8_head_.load(std::memory_order_acquire); }
  std::uint64_t size_consumed() const { return uint8_tail_.load(std::memory_order_acquire); }

 private:
  static constexpr std::size_t kSizeHeader = 8;  // 4B 记录尺寸 + 4B 对齐垫 | record size + alignment pad

  std::vector<std::byte> vec_bytes_;
  std::size_t size_mask_;
  std::atomic<std::uint64_t> uint8_head_{0};  // 写者发布(单调字节偏移)| writer-published (monotonic byte offset)
  std::atomic<std::uint64_t> uint8_tail_{0};  // 读者发布(单调字节偏移)| reader-published (monotonic byte offset)
  std::atomic<bool> b_stopped_{false};
  std::byte* ptr_payload_ = nullptr;          // Reserve 的 payload 落点 | Reserve's payload landing spot
  std::size_t size_payload_reserved_ = 0;
  std::uint64_t uint8_head_pending_ = 0;
};

}  // namespace ora::gfx
