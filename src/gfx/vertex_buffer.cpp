#include "gfx/vertex_buffer.hpp"

#include "platform/gl_types.hpp"

namespace ora::gfx {

IndexBuffer::IndexBuffer(RenderThread& render, std::span<const std::uint32_t> vec_indices)
    : render_{render}, size_count_{vec_indices.size()} {
  render_.GenNames(GfxCmdKind::GenBuffers, 1, &uint4_buffer_);
  if (GfxCmd* ptr_cmd = render_.queue().Reserve(GfxCmdKind::BindBuffer, 0); ptr_cmd != nullptr) {
    ptr_cmd->uint4_a = gl::GL_ELEMENT_ARRAY_BUFFER;
    ptr_cmd->uint4_b = uint4_buffer_;
    render_.queue().CommitBare();
  }
  if (GfxCmd* ptr_cmd = render_.queue().Reserve(GfxCmdKind::BufferData,
                                                static_cast<std::uint32_t>(vec_indices.size_bytes()));
      ptr_cmd != nullptr) {
    ptr_cmd->uint4_a = gl::GL_ELEMENT_ARRAY_BUFFER;
    ptr_cmd->uint4_b = static_cast<std::uint32_t>(vec_indices.size_bytes());
    ptr_cmd->uint4_c = gl::GL_STATIC_DRAW;
    render_.queue().Commit(vec_indices.data());
  }
}

IndexBuffer::~IndexBuffer() {
  if (uint4_buffer_ == 0)
    return;
  if (GfxCmd* ptr_cmd = render_.queue().Reserve(GfxCmdKind::DeleteBuffers, 4); ptr_cmd != nullptr) {
    render_.queue().Commit(&uint4_buffer_);
  }
}

VertexBuffer::VertexBuffer(RenderThread& render, std::span<const ShaderVertexAttribute> vec_attributes,
                           std::int32_t int4_stride)
    : render_{render} {

  for (std::size_t i = 0; i < vec_attributes.size(); ++i) {
    const ShaderVertexAttribute& attribute = vec_attributes[i];
    vec_attributes_.push_back(VaoAttribDesc{
        static_cast<std::uint32_t>(i),
        static_cast<std::uint32_t>(attribute.int4_components),
        attribute.kind_type == ShaderVertexAttributeType::Float
            ? gl::GL_FLOAT
            : (attribute.kind_type == ShaderVertexAttributeType::UInt ? gl::GL_UNSIGNED_INT : gl::GL_INT),
        attribute.kind_type == ShaderVertexAttributeType::Float ? 0u : 1u,
        static_cast<std::uint32_t>(int4_stride),
        static_cast<std::uint32_t>(attribute.int4_offset),
    });
  }
}

VertexBuffer::~VertexBuffer() {
  if (uint4_buffer_ == 0 && map_vaos_.empty())
    return;

  if (!map_vaos_.empty()) {
    if (GfxCmd* ptr_cmd = render_.queue().Reserve(GfxCmdKind::DeleteVertexArrays,
                                                  static_cast<std::uint32_t>(map_vaos_.size() * 4));
        ptr_cmd != nullptr) {
      std::vector<std::uint32_t> vec_vaos;
      vec_vaos.reserve(map_vaos_.size());
      for (const auto& [uint4_program, uint4_vao] : map_vaos_)
        vec_vaos.push_back(uint4_vao);
      render_.queue().Commit(vec_vaos.data());
    }
  }
  if (uint4_buffer_ != 0) {
    if (b_persistent_) {
      if (GfxCmd* ptr_cmd = render_.queue().Reserve(GfxCmdKind::DeletePersistentBuffer, 0); ptr_cmd != nullptr) {
        ptr_cmd->uint4_a = uint4_buffer_;
        render_.queue().CommitBare();
      }
    } else if (GfxCmd* ptr_cmd = render_.queue().Reserve(GfxCmdKind::DeleteBuffers, 4); ptr_cmd != nullptr) {
      render_.queue().Commit(&uint4_buffer_);
    }
  }
}

void VertexBuffer::InitPersistent(std::uint32_t uint4_size_bytes, std::uint32_t uint4_slots) {
  assert(uint4_slots > 0 && uint4_slots <= 8);
  uint4_slots_ = uint4_slots;
  uint4_slot_bytes_ = uint4_size_bytes / std::max(1u, uint4_slots);
  b_persistent_ = true;
  render_.GenNames(GfxCmdKind::GenBuffers, 1, &uint4_buffer_);
  if (GfxCmd* ptr_cmd = render_.queue().Reserve(GfxCmdKind::BufferStoragePersistent, 0); ptr_cmd != nullptr) {
    ptr_cmd->uint4_a = uint4_buffer_;
    ptr_cmd->uint4_b = uint4_size_bytes;
    ptr_cmd->uint4_c = uint4_slots;
    render_.queue().CommitBare();
  }
}

void VertexBuffer::InitStatic(std::span<const std::byte> vec_bytes) {
  b_persistent_ = false;
  render_.GenNames(GfxCmdKind::GenBuffers, 1, &uint4_buffer_);
  if (GfxCmd* ptr_cmd = render_.queue().Reserve(GfxCmdKind::BindBuffer, 0); ptr_cmd != nullptr) {
    ptr_cmd->uint4_a = gl::GL_ARRAY_BUFFER;
    ptr_cmd->uint4_b = uint4_buffer_;
    render_.queue().CommitBare();
  }
  if (GfxCmd* ptr_cmd = render_.queue().Reserve(GfxCmdKind::BufferData,
                                                static_cast<std::uint32_t>(vec_bytes.size()));
      ptr_cmd != nullptr) {
    ptr_cmd->uint4_a = gl::GL_ARRAY_BUFFER;
    ptr_cmd->uint4_b = static_cast<std::uint32_t>(vec_bytes.size());
    ptr_cmd->uint4_c = gl::GL_STATIC_DRAW;
    render_.queue().Commit(vec_bytes.data());
  }
}

void VertexBuffer::UpdateStaticSubData(std::uint32_t uint4_byte_offset, std::span<const std::byte> vec_bytes) {
  assert(!b_persistent_ && uint4_buffer_ != 0);
  if (GfxCmd* ptr_cmd = render_.queue().Reserve(GfxCmdKind::BindBuffer, 0); ptr_cmd != nullptr) {
    ptr_cmd->uint4_a = gl::GL_ARRAY_BUFFER;
    ptr_cmd->uint4_b = uint4_buffer_;
    render_.queue().CommitBare();
  }
  if (GfxCmd* ptr_cmd = render_.queue().Reserve(GfxCmdKind::BufferSubData,
                                                static_cast<std::uint32_t>(vec_bytes.size()));
      ptr_cmd != nullptr) {
    ptr_cmd->uint4_a = gl::GL_ARRAY_BUFFER;
    ptr_cmd->uint4_b = uint4_byte_offset;
    ptr_cmd->uint4_c = static_cast<std::uint32_t>(vec_bytes.size());
    render_.queue().Commit(vec_bytes.data());
  }
}

void VertexBuffer::WriteSlot(std::uint32_t uint4_slot, std::span<const std::byte> vec_bytes) {
  assert(b_persistent_ && uint4_slot < uint4_slots_);
  if (GfxCmd* ptr_cmd = render_.queue().Reserve(GfxCmdKind::WritePersistent,
                                                static_cast<std::uint32_t>(vec_bytes.size()));
      ptr_cmd != nullptr) {
    ptr_cmd->uint4_a = uint4_buffer_;
    ptr_cmd->uint4_b = uint4_slot;
    ptr_cmd->uint4_c = uint4_slot * uint4_slot_bytes_;
    render_.queue().Commit(vec_bytes.data());
  }
}

void VertexBuffer::FenceSlot(std::uint32_t uint4_slot) {
  assert(b_persistent_ && uint4_slot < uint4_slots_);
  if (GfxCmd* ptr_cmd = render_.queue().Reserve(GfxCmdKind::FencePersistentSlot, 0); ptr_cmd != nullptr) {
    ptr_cmd->uint4_a = uint4_buffer_;
    ptr_cmd->uint4_b = uint4_slot;
    render_.queue().CommitBare();
  }
}

void VertexBuffer::BindVao(const Shader& shader, const IndexBuffer* ptr_indices) {
  const std::uint32_t uint4_program = shader.GlId();
  const auto it_vao = map_vaos_.find(uint4_program);
  std::uint32_t uint4_vao = it_vao != map_vaos_.end() ? it_vao->second : 0;
  if (uint4_vao == 0) {

    render_.GenNames(GfxCmdKind::GenVertexArrays, 1, &uint4_vao);
    map_vaos_[uint4_program] = uint4_vao;
    const std::uint32_t uint4_payload =
        static_cast<std::uint32_t>(vec_attributes_.size() * sizeof(VaoAttribDesc));
    if (GfxCmd* ptr_cmd = render_.queue().Reserve(GfxCmdKind::ConfigureVao, uint4_payload); ptr_cmd != nullptr) {
      ptr_cmd->uint4_a = uint4_vao;
      ptr_cmd->uint4_b = uint4_buffer_;
      ptr_cmd->uint4_c = ptr_indices != nullptr ? ptr_indices->GlId() : 0;
      render_.queue().Commit(vec_attributes_.data());
    }
  }
  if (GfxCmd* ptr_cmd = render_.queue().Reserve(GfxCmdKind::BindVertexArray, 0); ptr_cmd != nullptr) {
    ptr_cmd->uint4_a = uint4_vao;
    render_.queue().CommitBare();
  }
}

void VertexBuffer::DrawElements(std::uint32_t uint4_count, std::uint32_t uint4_byte_offset,
                                 std::uint32_t uint4_base_vertex) {
  if (GfxCmd* ptr_cmd = render_.queue().Reserve(GfxCmdKind::DrawElements, 0); ptr_cmd != nullptr) {
    ptr_cmd->uint4_a = gl::GL_TRIANGLES;
    ptr_cmd->uint4_b = uint4_count;
    ptr_cmd->uint4_c = gl::GL_UNSIGNED_INT;
    ptr_cmd->uint4_d = uint4_byte_offset;
    ptr_cmd->float_a = std::bit_cast<float>(uint4_base_vertex);
    render_.queue().CommitBare();
  }
}

}
