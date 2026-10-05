// UPSTREAM: OpenRA.Game/Graphics/PlatformInterfaces.cs @b6fc03f(IVertexBuffer/IIndexBuffer 接口族)+
//           OpenRA.Platforms.Default/VertexBuffer.cs @b6fc03f L17-125 +
//           OpenRA.Platforms.Default/StaticIndexBuffer.cs @b6fc03f
// 顶点/索引缓冲封装(主线程侧命令发射器)。两种 VB 形态:
//   - 持久映射(OPT-A5):glBufferStorage + MapBufferRange(PERSISTENT|WRITE|
//     COHERENT)整块映射,N 槽轮换 + 槽级 fence;写入 = 命令 payload → 渲染
//     线程 memcpy 进映射区(槽 fence 先行等待)。上游 Flush 路径 = "CPU 数组
//     → 装箱消息(或 LOH 分支拷贝)→ glBufferSubData(驱动内再拷贝)",本实现
//     免去驱动路径的额外拷贝与 glBufferData 的存储重分配;映射与 fence 全在
//     渲染线程内闭环,主线程零等待(详证 gfx_command.hpp 的 OPT-A5 头注)。
//   - 静态:一次 BufferData(GL_STATIC_DRAW)(上游 VertexBuffer(T[] data)
//     构造;TerrainSpriteLayer 的静态大 VB 等)。
// OPT-A6:每 (顶点格式, program) 一个 VAO —— VB/IB/属性指针一次固化进 VAO,
// 绘制路径只剩 BindVertexArray(消费端 diff);上游每 flush 重播 Shader.Bind
// 的属性指针序列 + indexBuffer.Bind(Shader.cs L134-145)。
// Vertex/index buffer wrappers (main-thread command emitters). Two VB forms:
// persistent-mapped (OPT-A5: glBufferStorage + whole-buffer
// PERSISTENT|WRITE|COHERENT mapping, N rotating slots with per-slot fences;
// writes ride the command payload and the render thread memcpys into the
// mapping after awaiting the slot fence — upstream's flush path was "CPU
// array → boxed message (or the LOH-branch copy) → glBufferSubData (yet
// another copy inside the driver)", which this removes along with
// glBufferData's storage reallocation; mapping and fences stay entirely
// inside the render thread, so the main thread never waits — see the OPT-A5
// header note in gfx_command.hpp) and static (one BufferData(GL_STATIC_DRAW),
// upstream's VertexBuffer(T[] data) constructor; the big static
// TerrainSpriteLayer VBs among others). OPT-A6: one VAO per (vertex format,
// program) — VB/IB/attribute pointers bake into the VAO once, leaving only
// BindVertexArray on the draw path (diffed by the consumer), replacing
// upstream's per-flush attribute-pointer replay of Shader.Bind
// (Shader.cs L134-145) plus indexBuffer.Bind.
#pragma once
import std;

#include "gfx/gfx_command.hpp"
#include "gfx/render_thread.hpp"
#include "gfx/shader.hpp"
#include "gfx/vertex.hpp"

namespace ora::gfx {

/// kCombinedAttributes(vertex.hpp 的顶点格式契约)→ Shader 属性描述表
/// (Renderer 与测试共用的转换)。
/// kCombinedAttributes (the vertex-format contract in vertex.hpp) → the
/// Shader attribute-descriptor table (the conversion shared by Renderer and
/// tests).
inline std::vector<ShaderVertexAttribute> MakeCombinedAttributes() {
  std::vector<ShaderVertexAttribute> vec_attributes;
  for (const VertexAttributeDesc& desc_attribute : kCombinedAttributes)
    vec_attributes.push_back(ShaderVertexAttribute{
        std::string{desc_attribute.str_name},
        static_cast<ShaderVertexAttributeType>(desc_attribute.kind_gl_type),
        desc_attribute.int4_components, desc_attribute.int4_offset});
  return vec_attributes;
}

/// 静态索引缓冲(StaticIndexBuffer.cs;RAII 异步删除)。
/// A static index buffer (StaticIndexBuffer.cs; RAII async deletion).
class IndexBuffer {
 public:
  IndexBuffer(RenderThread& render, std::span<const std::uint32_t> vec_indices);
  ~IndexBuffer();

  IndexBuffer(const IndexBuffer&) = delete;
  IndexBuffer& operator=(const IndexBuffer&) = delete;

  std::size_t Count() const { return size_count_; }
  std::uint32_t GlId() const { return uint4_buffer_; }

 private:
  RenderThread& render_;
  std::uint32_t uint4_buffer_ = 0;
  std::size_t size_count_ = 0;
};

/// 顶点缓冲(持久映射或静态;per-program VAO 缓存;RAII)。
/// A vertex buffer (persistent-mapped or static; a per-program VAO cache;
/// RAII).
class VertexBuffer {
 public:
  /// attribs/stride = 顶点格式契约(CombinedShaderBindings 的属性表形态)。
  /// attribs/stride = the vertex-format contract (the CombinedShaderBindings
  /// attribute-table shape).
  VertexBuffer(RenderThread& render, std::span<const ShaderVertexAttribute> vec_attributes,
               std::int32_t int4_stride);

  ~VertexBuffer();

  VertexBuffer(const VertexBuffer&) = delete;
  VertexBuffer& operator=(const VertexBuffer&) = delete;

  /// 持久映射形态:总容量 size_bytes × 槽(上游 CreateEmptyVertexBuffer 的
  /// 动态等价物;上游用 GL_DYNAMIC_DRAW + BufferSubData)。
  /// Persistent-mapped form: size_bytes total × slots (the dynamic
  /// counterpart of upstream's CreateEmptyVertexBuffer; upstream used
  /// GL_DYNAMIC_DRAW + BufferSubData).
  void InitPersistent(std::uint32_t uint4_size_bytes, std::uint32_t uint4_slots);

  /// 静态形态:一次上传(VertexBuffer(T[] data, dynamic: false) 等价物)。
  /// Static form: a one-shot upload (the VertexBuffer(T[] data, dynamic:
  /// false) counterpart).
  void InitStatic(std::span<const std::byte> vec_bytes);

  /// 静态形态的子区域更新(上游 IVertexBuffer.SetData(T[], srcOffset,
  /// dstOffset, count) —— TerrainSpriteLayer 的脏行上传,L225-228;经
  /// BufferSubData 命令,目标 GL_STATIC_DRAW 缓冲)。
  /// The static form's sub-range update (upstream IVertexBuffer.SetData(T[],
  /// srcOffset, dstOffset, count) — TerrainSpriteLayer's dirty-row upload,
  /// L225-228; via the BufferSubData command onto the GL_STATIC_DRAW
  /// buffer).
  void UpdateStaticSubData(std::uint32_t uint4_byte_offset, std::span<const std::byte> vec_bytes);

  /// 写入持久区(槽 slot,槽内偏移 0;渲染线程等槽 fence 后 memcpy)。
  /// Writes into the persistent store (slot slot, offset 0 within it; the
  /// render thread awaits the slot fence, then memcpys).
  void WriteSlot(std::uint32_t uint4_slot, std::span<const std::byte> vec_bytes);

  /// 该槽 fence(在该槽全部 DrawElements 提交后调用)。
  /// Fences the slot (call after that slot's DrawElements were all posted).
  void FenceSlot(std::uint32_t uint4_slot);

  /// 槽的字节容量(写入分片用)。
  /// The byte capacity of one slot (for write splitting).
  std::uint32_t SlotBytes() const { return uint4_slot_bytes_; }

  /// 槽的顶点容量(draw 的 basevertex = slot × 该值;第四批:写入落槽 N 而
  /// 索引寻址自 buffer 首 —— basevertex 把槽偏移补进顶点引用)。
  /// The vertex capacity of one slot (the draw's basevertex = slot × this;
  /// fourth batch: writes land in slot N while indices address from the
  /// buffer start — basevertex folds the slot offset into the vertex
  /// references).
  std::uint32_t SlotVertices() const { return uint4_slot_bytes_ / sizeof(Vertex); }

  /// 取下一槽(轮换归 VB 所有:共享 VB 的多个 SpriteRenderer 自然错开,
  /// 不会同槽互相覆盖)。
  /// Acquires the next slot (the rotation belongs to the VB: the
  /// SpriteRenderers sharing one VB space out naturally instead of
  /// stomping the same slot).
  std::uint32_t AcquireSlot() {
    const std::uint32_t uint4_slot = uint4_next_slot_;
    uint4_next_slot_ = (uint4_next_slot_ + 1) % std::max(1u, uint4_slots_);
    return uint4_slot;
  }

  /// 确保 (本格式, program) 的 VAO 存在并绑定(带可选索引缓冲固化)。
  /// Ensures the (this format, program) VAO exists and binds it (with an
  /// optional index buffer baked in).
  void BindVao(const Shader& shader, const IndexBuffer* ptr_indices);

  /// 已索引绘制(TRIANGLES/UNSIGNED_INT;byte_offset 为索引缓冲内字节偏移;
  /// base_vertex 平移顶点引用,持久 VB 槽绘制用)。
  /// Indexed draw (TRIANGLES/UNSIGNED_INT; byte_offset is the byte offset
  /// within the index buffer; base_vertex shifts the vertex references, used
  /// by the persistent-VB slot draws).
  void DrawElements(std::uint32_t uint4_count, std::uint32_t uint4_byte_offset,
                    std::uint32_t uint4_base_vertex = 0);

  std::uint32_t GlId() const { return uint4_buffer_; }
  bool b_persistent() const { return b_persistent_; }

 private:
  RenderThread& render_;
  std::vector<VaoAttribDesc> vec_attributes_;
  std::map<std::uint32_t, std::uint32_t> map_vaos_;  // program → VAO
  std::uint32_t uint4_buffer_ = 0;
  std::uint32_t uint4_slot_bytes_ = 0;
  std::uint32_t uint4_slots_ = 0;
  std::uint32_t uint4_next_slot_ = 0;
  bool b_persistent_ = false;
};

}  // namespace ora::gfx
