#include "gfx/terrain_sprite_layer.hpp"

#include <cassert>

#include "gfx/gfx_util.hpp"
#include "gfx/renderer.hpp"
#include "gfx/sprite_renderer.hpp"
#include "sim/world.hpp"

namespace ora::gfx {

namespace {

std::int32_t ClampInt(std::int32_t int4_v, std::int32_t int4_lo, std::int32_t int4_hi) {
  return std::min(std::max(int4_v, int4_lo), int4_hi);
}

}

struct SharedIndexBufferRc {
  std::unique_ptr<IndexBuffer> ptr_buffer;
  std::int32_t int4_ref_count = 0;
  const sim::World* ptr_world = nullptr;
};

namespace {

std::mutex& SharedIndexTableLock() {
  static std::mutex mutex_shared;
  return mutex_shared;
}

std::map<const sim::World*, SharedIndexBufferRc>& SharedIndexTable() {
  static std::map<const sim::World*, SharedIndexBufferRc> map_shared;
  return map_shared;
}

}

TerrainSpriteLayer::TerrainSpriteLayer(sim::World& world, WorldRenderer& wr, const Sprite& sprite_empty,
                                       BlendMode kind_blend, bool b_restrict_to_bounds, RenderThread* ptr_render)
    : wr_{wr},
      b_restrict_to_bounds_{b_restrict_to_bounds},
      sprite_empty_{sprite_empty},
      kind_blend_{kind_blend} {

  const TerrainMapSurface& surface = wr.TerrainSurface();
  int4_width_ = surface.int2_map_size.X;
  int4_height_ = surface.int2_map_size.Y;

  int4_vertex_row_stride_ = 4 * int4_width_;
  vec_vertices_.assign(static_cast<std::size_t>(int4_vertex_row_stride_) * static_cast<std::size_t>(int4_height_),
                       Vertex{});

  int4_index_row_stride_ = 6 * int4_width_;

  if (ptr_render != nullptr) {
    opt_vertex_buffer_.emplace(*ptr_render, MakeCombinedAttributes(), sizeof(Vertex));
    const std::size_t size_bytes =
        static_cast<std::size_t>(int4_vertex_row_stride_) * static_cast<std::size_t>(int4_height_) * sizeof(Vertex);
    std::vector<std::byte> vec_zero(size_bytes);
    opt_vertex_buffer_->InitStatic(vec_zero);

    std::lock_guard lock_shared{SharedIndexTableLock()};
    auto& map_shared = SharedIndexTable();
    auto [it_rc, b_inserted] = map_shared.try_emplace(&world);
    if (b_inserted) {
      it_rc->second.ptr_world = &world;
      it_rc->second.ptr_buffer = std::make_unique<IndexBuffer>(
          *ptr_render,
          CreateQuadIndices(static_cast<std::int32_t>(
              static_cast<std::size_t>(int4_width_) * static_cast<std::size_t>(int4_height_))));
    }
    ptr_shared_index_rc_ = &it_rc->second;
    ++ptr_shared_index_rc_->int4_ref_count;
  }

  vec_palette_.assign(static_cast<std::size_t>(int4_width_) * static_cast<std::size_t>(int4_height_), nullptr);
  vec_corner_tint_.assign(static_cast<std::size_t>(int4_vertex_row_stride_) * static_cast<std::size_t>(int4_height_),
                          core::Vector3{1.0f, 1.0f, 1.0f});
  vecb_dirty_rows_.assign(static_cast<std::size_t>(int4_height_), 0);
  uint8_palette_invalidated_token_ = wr_.SubscribePaletteInvalidated([this]() { UpdatePaletteIndices(); });

  if (wr_.TerrainLighting() != nullptr) {
    vecb_ignore_tint_.assign(
        static_cast<std::size_t>(int4_vertex_row_stride_) * static_cast<std::size_t>(int4_height_), false);
    uint8_cell_changed_token_ = wr_.TerrainLighting()->AddCellChangedListener([this](MPos uv) { UpdateTint(uv); });
  }
}

TerrainSpriteLayer::~TerrainSpriteLayer() {
  wr_.UnsubscribePaletteInvalidated(uint8_palette_invalidated_token_);
  if (uint8_cell_changed_token_ != 0 && wr_.TerrainLighting() != nullptr)
    wr_.TerrainLighting()->RemoveCellChangedListener(uint8_cell_changed_token_);

  if (ptr_shared_index_rc_ != nullptr) {
    std::lock_guard lock_shared{SharedIndexTableLock()};
    if (--ptr_shared_index_rc_->int4_ref_count == 0)
      SharedIndexTable().erase(ptr_shared_index_rc_->ptr_world);
  }
}

void TerrainSpriteLayer::Clear(CPos cpos_cell) {
  Update(cpos_cell, nullptr, nullptr, 1.0f, 1.0f, true);
}

void TerrainSpriteLayer::Update(CPos cpos_cell, const Sprite* ptr_sprite, const PaletteReference* ptr_palette,
                                float float_scale, float float_alpha, bool b_ignore_tint) {

  core::Vector3 vec_xyz{};
  if (ptr_sprite != nullptr) {
    const TerrainMapSurface& surface = wr_.TerrainSurface();
    const WPos wpos_cell_origin = surface.fn_center_of_cell(cpos_cell) -
                                  WVec{0, 0, surface.fn_ramp_center_height_offset(cpos_cell)};
    vec_xyz = wr_.Screen3DPosition(wpos_cell_origin) +
              float_scale * (ptr_sprite->vec_offset - 0.5f * ptr_sprite->vec_size);
  }

  Update(cpos_cell.ToMPos(wr_.TerrainSurface().kind_grid_type), ptr_sprite, ptr_palette, vec_xyz, float_scale,
         float_alpha, b_ignore_tint);
}

void TerrainSpriteLayer::UpdateTint(MPos uv_cell) {
  assert(wr_.TerrainLighting() != nullptr);

  const std::size_t size_offset =
      static_cast<std::size_t>(int4_vertex_row_stride_) * static_cast<std::size_t>(uv_cell.V) +
      4 * static_cast<std::size_t>(uv_cell.U);
  if (!vecb_ignore_tint_.empty() && vecb_ignore_tint_[size_offset]) {

    for (std::size_t i = 0; i < 4; i++)
      vec_corner_tint_[size_offset + i] = core::Vector3{1.0f, 1.0f, 1.0f};
    return;
  }

  const ITerrainLighting& lighting = *wr_.TerrainLighting();
  const TerrainMapSurface& surface = wr_.TerrainSurface();
  const WPos wpos_center = surface.fn_center_of_cell(uv_cell.ToCPos(surface.kind_grid_type));
  const std::int32_t int4_step = surface.int4_tile_scale / 2;
  const core::Vector3 arr_weights[4] = {
      lighting.TintAt(wpos_center + WVec{-int4_step, -int4_step, 0}),
      lighting.TintAt(wpos_center + WVec{int4_step, -int4_step, 0}),
      lighting.TintAt(wpos_center + WVec{int4_step, int4_step, 0}),
      lighting.TintAt(wpos_center + WVec{-int4_step, int4_step, 0}),
  };

  for (std::size_t i = 0; i < 4; i++)
    vec_corner_tint_[size_offset + i] = arr_weights[i];

  vecb_dirty_rows_[static_cast<std::size_t>(uv_cell.V)] = 1;
}

std::int32_t TerrainSpriteLayer::GetOrAddSheetIndex(const Sheet* ptr_sheet) {

  if (ptr_sheet == nullptr)
    return 0;

  for (std::int32_t i = 0; i < SpriteRenderer::kSheetCount; i++) {
    if (arr_sheets_[static_cast<std::size_t>(i)] == ptr_sheet)
      return i;

    if (arr_sheets_[static_cast<std::size_t>(i)] == nullptr) {
      arr_sheets_[static_cast<std::size_t>(i)] = const_cast<Sheet*>(ptr_sheet);
      return i;
    }
  }

  throw std::runtime_error("Sheet overflow");
}

void TerrainSpriteLayer::Update(MPos uv_cell, const Sprite* ptr_sprite, const PaletteReference* ptr_palette,
                                const core::Vector3& vec_pos, float float_scale, float float_alpha,
                                bool b_ignore_tint) {

  int2 int2_samplers{0, 0};
  if (ptr_sprite != nullptr) {
    if (ptr_sprite->kind_blend != kind_blend_)
      throw std::runtime_error("Attempted to add sprite with a different blend mode");

    const Sheet* ptr_secondary = nullptr;
    if (ptr_sprite->b_secondary)
      ptr_secondary = static_cast<const SpriteWithSecondaryData*>(ptr_sprite)->ptr_secondary_sheet;
    int2_samplers = int2{GetOrAddSheetIndex(ptr_sprite->ptr_sheet), GetOrAddSheetIndex(ptr_secondary)};

    if (ptr_sprite->kind_channel == TextureChannel::RGBA &&
        !(ptr_palette != nullptr && ptr_palette->HasColorShift()))
      ptr_palette = nullptr;
  } else {
    ptr_sprite = &sprite_empty_;
    int2_samplers = int2{0, 0};
  }

  if (!wr_.TerrainSurface().fn_tiles_contains(uv_cell))
    return;

  const std::size_t size_offset =
      static_cast<std::size_t>(int4_vertex_row_stride_) * static_cast<std::size_t>(uv_cell.V) +
      4 * static_cast<std::size_t>(uv_cell.U);

  FastCreateQuad(std::span<Vertex>(vec_vertices_).subspan(size_offset, 4), vec_pos, *ptr_sprite, int2_samplers, 0, 0,
                 float_scale * ptr_sprite->vec_size, float_alpha * core::Vector3{1.0f, 1.0f, 1.0f}, float_alpha);
  vec_palette_[static_cast<std::size_t>(uv_cell.V) * static_cast<std::size_t>(int4_width_) +
               static_cast<std::size_t>(uv_cell.U)] = ptr_palette;
  for (std::size_t i = 0; i < 4; i++)
    vec_corner_tint_[size_offset + i] = core::Vector3{1.0f, 1.0f, 1.0f};

  if (wr_.TerrainLighting() != nullptr) {
    vecb_ignore_tint_[size_offset] = b_ignore_tint;
    UpdateTint(uv_cell);
  }

  vecb_dirty_rows_[static_cast<std::size_t>(uv_cell.V)] = 1;
}

void TerrainSpriteLayer::UpdatePaletteIndices() {

  for (std::int32_t int4_row = 0; int4_row < int4_height_; int4_row++)
    vecb_dirty_rows_[static_cast<std::size_t>(int4_row)] = 1;
}

void TerrainSpriteLayer::ComposeRow(std::int32_t int4_row) {
  const std::size_t size_row_start =
      static_cast<std::size_t>(int4_vertex_row_stride_) * static_cast<std::size_t>(int4_row);
  vec_staging_row_.resize(static_cast<std::size_t>(int4_vertex_row_stride_));
  for (std::int32_t i = 0; i < int4_vertex_row_stride_; i++) {
    const std::size_t size_index = size_row_start + static_cast<std::size_t>(i);
    const Vertex& vertex_v = vec_vertices_[size_index];

    const std::uint32_t uint4_p = static_cast<std::uint32_t>(
        vec_palette_[size_index / 4] != nullptr ? vec_palette_[size_index / 4]->TextureIndex() : 0);
    Vertex vertex_out = vertex_v;
    vertex_out.c = ((uint4_p & 0xFFFF) << 16) | (vertex_v.c & 0xFFFF);

    const core::Vector3& vec_tint = vec_corner_tint_[size_index];
    vertex_out.r = vertex_v.r * vec_tint.X;
    vertex_out.g = vertex_v.g * vec_tint.Y;
    vertex_out.b = vertex_v.b * vec_tint.Z;

    vec_staging_row_[static_cast<std::size_t>(i)] = vertex_out;
  }
}

void TerrainSpriteLayer::Draw(std::int32_t int4_top_left_v, std::int32_t int4_bottom_right_v) {

  const std::int32_t int4_first_row = ClampInt(int4_top_left_v, 0, int4_height_);
  const std::int32_t int4_last_row = ClampInt(int4_bottom_right_v + 1, int4_first_row, int4_height_);

  if (wr_.RendererPtr() != nullptr)
    wr_.RendererPtr()->Flush();

  for (std::int32_t int4_row = int4_first_row; int4_row <= int4_last_row; int4_row++) {

    if (int4_row >= int4_height_ || !vecb_dirty_rows_[static_cast<std::size_t>(int4_row)])
      continue;

    ComposeRow(int4_row);
    const std::size_t size_row_offset =
        static_cast<std::size_t>(int4_vertex_row_stride_) * static_cast<std::size_t>(int4_row);
    if (opt_vertex_buffer_.has_value())
      opt_vertex_buffer_->UpdateStaticSubData(static_cast<std::uint32_t>(size_row_offset * sizeof(Vertex)),
                                              std::as_bytes(std::span<const Vertex>{vec_staging_row_}));
    vecb_dirty_rows_[static_cast<std::size_t>(int4_row)] = 0;
  }

  if (wr_.RendererPtr() != nullptr && opt_vertex_buffer_.has_value() && ptr_shared_index_rc_ != nullptr)
    wr_.RendererPtr()->WorldSpriteRenderer().DrawVertexBuffer(
        *opt_vertex_buffer_, *ptr_shared_index_rc_->ptr_buffer, int4_index_row_stride_ * int4_first_row,
        int4_index_row_stride_ * (int4_last_row - int4_first_row), std::span<Sheet* const>{arr_sheets_},
        kind_blend_);

  if (wr_.RendererPtr() != nullptr)
    wr_.RendererPtr()->Flush();
}

}
