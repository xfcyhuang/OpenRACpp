#pragma once
import std;

#include "core/color.hpp"
#include "core/int2.hpp"
#include "core/vector_n.hpp"
#include "gfx/gfx_util.hpp"
#include "gfx/hardware_palette.hpp"
#include "gfx/sprite.hpp"
#include "gfx/vertex.hpp"
#include "gfx/vertex_buffer.hpp"

namespace ora::gfx {

class SpriteRenderer;

struct IBatchRenderer {
  virtual void Flush() = 0;
  virtual ~IBatchRenderer() = default;
};

class BlendSpanTracker {
 public:
  struct Span {
    std::int32_t int4_start;
    std::int32_t int4_length;
    BlendMode kind_mode;
  };

  void TrackQuad(BlendMode kind_mode, std::int32_t int4_vertex_count) {
    if (vec_spans_.empty() || vec_spans_.back().kind_mode != kind_mode)
      vec_spans_.push_back(Span{int4_vertex_count, 4, kind_mode});
    else
      vec_spans_.back().int4_length += 4;
  }

  void Clear() { vec_spans_.clear(); }
  const std::vector<Span>& Spans() const { return vec_spans_; }

 private:
  std::vector<Span> vec_spans_;
};

struct IRgbaQuadSink {
  virtual void DrawRGBAQuad(std::span<const Vertex, 4> vec_vertices, BlendMode kind_mode) = 0;
  virtual ~IRgbaQuadSink() = default;
};

class SpriteRenderer : public IBatchRenderer, public IRgbaQuadSink {
 public:
  static constexpr std::int32_t kSheetCount = 8;

  SpriteRenderer(RenderThread& render, VertexBuffer& vertex_buffer, IndexBuffer& index_buffer, Shader&& shader,
                 IBatchRenderer** ptr_current_batch_slot, std::int32_t int4_temp_vertex_buffer_size);

  ~SpriteRenderer() override = default;
  SpriteRenderer(const SpriteRenderer&) = delete;
  SpriteRenderer& operator=(const SpriteRenderer&) = delete;

  void Flush() override;

  void DrawRgbaSprite(const Sprite& sprite_r, core::Vector3 v_location, float float_scale = 1.0f,
                      float float_rotation = 0.0f);
  void DrawRgbaSprite(const Sprite& sprite_r, core::Vector3 v_location, core::Vector3 v_scale,
                      float float_rotation = 0.0f);
  void DrawRgbaSprite(const Sprite& sprite_r, core::Vector3 v_location, float float_scale, core::Vector3 v_tint,
                      float float_alpha, float float_rotation = 0.0f);
  void DrawRgbaSprite(const Sprite& sprite_r, core::Vector3 v_a, core::Vector3 v_b, core::Vector3 v_c,
                      core::Vector3 v_d, core::Vector3 v_tint, float float_alpha);

  void DrawSprite(const Sprite& sprite_r, std::int32_t int4_palette_texture_index, core::Vector3 v_location,
                  core::Vector3 v_scale, float float_rotation = 0.0f);
  void DrawSprite(const Sprite& sprite_r, std::int32_t int4_palette_texture_index, core::Vector3 v_location,
                  float float_scale, float float_rotation = 0.0f);
  void DrawSprite(const Sprite& sprite_r, const PaletteReference* ptr_palette, core::Vector3 v_location,
                  float float_scale = 1.0f, float float_rotation = 0.0f);
  void DrawSprite(const Sprite& sprite_r, std::int32_t int4_palette_texture_index, core::Vector3 v_location,
                  float float_scale, core::Vector3 v_tint, float float_alpha, float float_rotation = 0.0f);
  void DrawSprite(const Sprite& sprite_r, const PaletteReference* ptr_palette, core::Vector3 v_location,
                  float float_scale, core::Vector3 v_tint, float float_alpha, float float_rotation = 0.0f);
  void DrawSprite(const Sprite& sprite_r, std::int32_t int4_palette_texture_index, core::Vector3 v_a,
                  core::Vector3 v_b, core::Vector3 v_c, core::Vector3 v_d, core::Vector3 v_tint, float float_alpha);

  void DrawVertexBuffer(VertexBuffer& vertex_buffer, IndexBuffer& index_buffer, std::int32_t int4_start,
                        std::int32_t int4_length, std::span<Sheet* const> vec_sheets, BlendMode kind_blend);

  void DrawRGBAQuad(std::span<const Vertex, 4> vec_vertices, BlendMode kind_mode) override;

  void SetPalette(HardwarePalette& palette);
  void SetViewportParams(int2 int2_sheet_size, std::int32_t int4_downscale, float float_depth_margin,
                         int2 int2_scroll);
  void SetDepthPreview(bool b_enabled, float float_contrast, float float_offset);
  void EnablePixelArtScaling(bool b_enabled);

  Shader& shader() { return shader_; }

  static std::int32_t ResolveTextureIndex(const Sprite& sprite_r, const PaletteReference* ptr_palette);

  std::int32_t VertexCountForTest() const { return int4_vertex_count_; }
  std::int32_t SheetCountForTest() const { return int4_sheet_count_; }
  const BlendSpanTracker& SpansForTest() const { return tracker_spans_; }

 private:

  void ActivateAsCurrent();

  int2 SetRenderStateForSprite(const Sprite& sprite_r);

  RenderThread& render_;
  VertexBuffer& vertex_buffer_;
  IndexBuffer& index_buffer_;
  Shader shader_;
  IBatchRenderer** ptr_current_batch_slot_ = nullptr;

  std::vector<Vertex> vec_vertices_;
  Sheet* arr_sheets_[kSheetCount] = {};
  BlendSpanTracker tracker_spans_;
  std::int32_t int4_vertex_count_ = 0;
  std::int32_t int4_sheet_count_ = 0;
  std::int32_t int4_temp_vertex_buffer_size_ = 0;

  std::int32_t int4_loc_palette_rows_ = -1;
  std::int32_t int4_loc_depth_tex_scale_ = -1;
  std::int32_t int4_loc_scroll_ = -1;
  std::int32_t int4_loc_p1_ = -1;
  std::int32_t int4_loc_p2_ = -1;
  std::int32_t int4_loc_enable_depth_preview_ = -1;
  std::int32_t int4_loc_depth_preview_params_ = -1;
  std::int32_t int4_loc_enable_pixel_art_scaling_ = -1;
};

class RgbaSpriteRenderer {
 public:
  explicit RgbaSpriteRenderer(SpriteRenderer& parent) : parent_{parent} {}

  void DrawSprite(const Sprite& sprite_r, core::Vector3 v_location, core::Vector3 v_scale,
                  float float_rotation = 0.0f);
  void DrawSprite(const Sprite& sprite_r, core::Vector3 v_location, float float_scale = 1.0f,
                  float float_rotation = 0.0f);
  void DrawSprite(const Sprite& sprite_r, core::Vector3 v_location, float float_scale, core::Vector3 v_tint,
                  float float_alpha, float float_rotation = 0.0f);
  void DrawSprite(const Sprite& sprite_r, core::Vector3 v_a, core::Vector3 v_b, core::Vector3 v_c,
                  core::Vector3 v_d, core::Vector3 v_tint, float float_alpha);

 private:
  SpriteRenderer& parent_;
};

class RgbaColorRenderer {
 public:
  explicit RgbaColorRenderer(IRgbaQuadSink& sink) : sink_{sink} {}

  void DrawLine(core::Vector3 v_start, core::Vector3 v_end, float float_width, core::Color color_start,
                core::Color color_end, BlendMode kind_mode = BlendMode::Alpha);
  void DrawLine(core::Vector2 v_start, core::Vector2 v_end, float float_width, core::Color color_c,
                BlendMode kind_mode = BlendMode::Alpha);
  void DrawLine(core::Vector3 v_start, core::Vector3 v_end, float float_width, core::Color color_c,
                BlendMode kind_mode = BlendMode::Alpha);
  void DrawLine(std::span<const core::Vector3> vec_points, float float_width, core::Color color_c,
                bool b_connect_segments = false, BlendMode kind_mode = BlendMode::Alpha);
  void DrawPolygon(std::span<const core::Vector3> vec_vertices, float float_width, core::Color color_c,
                   BlendMode kind_mode = BlendMode::Alpha);
  void DrawPolygon(std::span<const core::Vector2> vec_vertices, float float_width, core::Color color_c,
                   BlendMode kind_mode = BlendMode::Alpha);
  void DrawRect(core::Vector3 v_tl, core::Vector3 v_br, float float_width, core::Color color_c,
                BlendMode kind_mode = BlendMode::Alpha);
  void FillRect(core::Vector3 v_tl, core::Vector3 v_br, core::Color color_c, BlendMode kind_mode = BlendMode::Alpha);
  void FillRect(core::Vector3 v_a, core::Vector3 v_b, core::Vector3 v_c, core::Vector3 v_d, core::Color color_c,
                BlendMode kind_mode = BlendMode::Alpha);
  void FillRect(core::Vector3 v_a, core::Vector3 v_b, core::Vector3 v_c, core::Vector3 v_d, core::Color color_top_left,
                core::Color color_top_right, core::Color color_bottom_right, core::Color color_bottom_left,
                BlendMode kind_mode = BlendMode::Alpha);
  void FillEllipse(core::Vector3 v_tl, core::Vector3 v_br, core::Color color_c, BlendMode kind_mode = BlendMode::Alpha);

 private:
  static core::Vector3 IntersectionOf(core::Vector3 v_a, core::Vector3 v_da, core::Vector3 v_b, core::Vector3 v_db);
  void DrawDisconnectedLine(std::span<const core::Vector3> vec_points, float float_width, core::Color color_c,
                            BlendMode kind_mode);
  void DrawConnectedLine(std::span<const core::Vector3> vec_points, float float_width, core::Color color_c,
                         bool b_closed, BlendMode kind_mode);

  IRgbaQuadSink& sink_;
};

}
