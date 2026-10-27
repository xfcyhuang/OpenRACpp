#pragma once
import std;

#include "core/cell_pos.hpp"
#include "core/color.hpp"
#include "core/int2.hpp"
#include "core/vector_n.hpp"
#include "core/wpos.hpp"
#include "gfx/palette.hpp"
#include "gfx/sprite.hpp"
#include "gfx/sprite_renderer.hpp"
#include "gfx/vertex.hpp"
#include "gfx/vertex_buffer.hpp"
#include "gfx/world_renderer.hpp"

namespace ora::sim {
class World;
}

namespace ora::gfx {

class RenderThread;

struct SharedIndexBufferRc;

class TerrainSpriteLayer {
 public:

  TerrainSpriteLayer(sim::World& world, WorldRenderer& wr, const Sprite& sprite_empty, BlendMode kind_blend,
                     bool b_restrict_to_bounds, RenderThread* ptr_render);
  ~TerrainSpriteLayer();

  TerrainSpriteLayer(const TerrainSpriteLayer&) = delete;
  TerrainSpriteLayer& operator=(const TerrainSpriteLayer&) = delete;

  BlendMode kind_blend() const { return kind_blend_; }

  bool b_restrict_to_bounds() const { return b_restrict_to_bounds_; }

  void Clear(CPos cpos_cell);

  void Update(CPos cpos_cell, const Sprite* ptr_sprite, const PaletteReference* ptr_palette,
              float float_scale = 1.0f, float float_alpha = 1.0f, bool b_ignore_tint = false);

  void Update(MPos uv_cell, const Sprite* ptr_sprite, const PaletteReference* ptr_palette,
              const core::Vector3& vec_pos, float float_scale, float float_alpha, bool b_ignore_tint);

  void UpdateTint(MPos uv_cell);

  void Draw(std::int32_t int4_top_left_v, std::int32_t int4_bottom_right_v);

  bool RowDirtyForTest(std::int32_t int4_row) const {
    return int4_row >= 0 && int4_row < static_cast<std::int32_t>(vecb_dirty_rows_.size()) &&
           vecb_dirty_rows_[static_cast<std::size_t>(int4_row)];
  }
  std::span<const Vertex> VerticesForTest() const { return vec_vertices_; }
  const PaletteReference* PaletteAtForTest(MPos uv_cell) const {
    return vec_palette_[static_cast<std::size_t>(uv_cell.V) * static_cast<std::size_t>(int4_width_) +
                        static_cast<std::size_t>(uv_cell.U)];
  }
  std::span<const core::Vector3> CornerTintForTest() const { return vec_corner_tint_; }
  const std::vector<Vertex>& StagingForTest() const { return vec_staging_row_; }

  void ComposeRow(std::int32_t int4_row);

 private:

  std::int32_t GetOrAddSheetIndex(const Sheet* ptr_sheet);

  void UpdatePaletteIndices();

  WorldRenderer& wr_;
  bool b_restrict_to_bounds_ = false;
  Sprite sprite_empty_;
  BlendMode kind_blend_;
  std::int32_t int4_width_ = 0;
  std::int32_t int4_height_ = 0;

  std::array<Sheet*, SpriteRenderer::kSheetCount> arr_sheets_{};

  std::optional<VertexBuffer> opt_vertex_buffer_;
  std::vector<Vertex> vec_vertices_;
  std::vector<const PaletteReference*> vec_palette_;
  std::vector<core::Vector3> vec_corner_tint_;
  std::vector<bool> vecb_ignore_tint_;
  std::vector<std::uint8_t> vecb_dirty_rows_;
  std::vector<Vertex> vec_staging_row_;
  std::int32_t int4_index_row_stride_ = 0;
  std::int32_t int4_vertex_row_stride_ = 0;

  SharedIndexBufferRc* ptr_shared_index_rc_ = nullptr;
  std::uint64_t uint8_palette_invalidated_token_ = 0;
  std::uint64_t uint8_cell_changed_token_ = 0;
};

}
