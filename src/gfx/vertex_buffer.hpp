#pragma once
import std;

#include "gfx/gfx_command.hpp"
#include "gfx/render_thread.hpp"
#include "gfx/shader.hpp"
#include "gfx/vertex.hpp"

namespace ora::gfx {

inline std::vector<ShaderVertexAttribute> MakeCombinedAttributes() {
  std::vector<ShaderVertexAttribute> vec_attributes;
  for (const VertexAttributeDesc& desc_attribute : kCombinedAttributes)
    vec_attributes.push_back(ShaderVertexAttribute{
        std::string{desc_attribute.str_name},
        static_cast<ShaderVertexAttributeType>(desc_attribute.kind_gl_type),
        desc_attribute.int4_components, desc_attribute.int4_offset});
  return vec_attributes;
}

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

class VertexBuffer {
 public:

  VertexBuffer(RenderThread& render, std::span<const ShaderVertexAttribute> vec_attributes,
               std::int32_t int4_stride);

  ~VertexBuffer();

  VertexBuffer(const VertexBuffer&) = delete;
  VertexBuffer& operator=(const VertexBuffer&) = delete;

  void InitPersistent(std::uint32_t uint4_size_bytes, std::uint32_t uint4_slots);

  void InitStatic(std::span<const std::byte> vec_bytes);

  void UpdateStaticSubData(std::uint32_t uint4_byte_offset, std::span<const std::byte> vec_bytes);

  void WriteSlot(std::uint32_t uint4_slot, std::span<const std::byte> vec_bytes);

  void FenceSlot(std::uint32_t uint4_slot);

  std::uint32_t SlotBytes() const { return uint4_slot_bytes_; }

  std::uint32_t SlotVertices() const { return uint4_slot_bytes_ / sizeof(Vertex); }

  std::uint32_t AcquireSlot() {
    const std::uint32_t uint4_slot = uint4_next_slot_;
    uint4_next_slot_ = (uint4_next_slot_ + 1) % std::max(1u, uint4_slots_);
    return uint4_slot;
  }

  void BindVao(const Shader& shader, const IndexBuffer* ptr_indices);

  void DrawElements(std::uint32_t uint4_count, std::uint32_t uint4_byte_offset,
                    std::uint32_t uint4_base_vertex = 0);

  std::uint32_t GlId() const { return uint4_buffer_; }
  bool b_persistent() const { return b_persistent_; }

 private:
  RenderThread& render_;
  std::vector<VaoAttribDesc> vec_attributes_;
  std::map<std::uint32_t, std::uint32_t> map_vaos_;
  std::uint32_t uint4_buffer_ = 0;
  std::uint32_t uint4_slot_bytes_ = 0;
  std::uint32_t uint4_slots_ = 0;
  std::uint32_t uint4_next_slot_ = 0;
  bool b_persistent_ = false;
};

}
