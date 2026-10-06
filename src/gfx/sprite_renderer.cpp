// UPSTREAM: OpenRA.Game/Graphics/SpriteRenderer.cs @b6fc03f L20-305(实现)+
//           OpenRA.Game/Graphics/RgbaSpriteRenderer.cs @b6fc03f L17-58 +
//           OpenRA.Game/Graphics/RgbaColorRenderer.cs @b6fc03f L20-256
// 头文件携带完整 UPSTREAM 锚点与第四批优化挂接说明。
// Implementations; the header carries the full UPSTREAM anchors and the
// fourth-batch optimization wiring notes.
#include "gfx/sprite_renderer.hpp"

#include "gfx/sheet.hpp"


namespace ora::gfx {

namespace {

/// SheetIndexToTextureName(SpriteRenderer.cs L38:Exts.MakeArray → "Texture{i}")。
/// SheetIndexToTextureName (SpriteRenderer.cs L38: Exts.MakeArray →
/// "Texture{i}").
constexpr std::array<std::string_view, SpriteRenderer::kSheetCount> kSheetSamplerNames{
    "Texture0", "Texture1", "Texture2", "Texture3", "Texture4", "Texture5", "Texture6", "Texture7"};

constexpr std::uint32_t kUintSize = sizeof(std::uint32_t);  // 上游 UintSize | upstream's UintSize

/// 预乘后的颜色 float4(core 无 Vector4;上游 PremultiplyAlpha(color)
/// .ToVector4(),Color.ToVector4 = (R,G,B,A)/255 的渲染域承载)。
/// The premultiplied color float4 (core has no Vector4; the render-domain
/// carrier of upstream's PremultiplyAlpha(color).ToVector4(), where
/// Color.ToVector4 = (R,G,B,A)/255).
struct ColorF4 {
  float X, Y, Z, W;
};

ColorF4 ColorToVector4(core::Color color_c) {
  const core::Color color_pre = PremultiplyAlpha(color_c);
  return ColorF4{color_pre.R() / 255.0f, color_pre.G() / 255.0f, color_pre.B() / 255.0f, color_pre.A() / 255.0f};
}

/// 上游 new Vertex(pos, c.X, c.Y, c.Z, c.W, 0):xyz = pos;s..v = 颜色四分量;
/// c = 0;tint 默认 (1,1,1,1)。
/// Upstream's new Vertex(pos, c.X, c.Y, c.Z, c.W, 0): xyz = pos; s..v = the
/// four color components; c = 0; tint defaults to (1,1,1,1).
Vertex VertexColor(core::Vector3 v_pos, ColorF4 v_color) {
  return Vertex{v_pos.X, v_pos.Y, v_pos.Z, v_color.X, v_color.Y, v_color.Z, v_color.W, 0, 1.0f, 1.0f, 1.0f, 1.0f};
}

/// 上游 Vector3.AsVector2()(丢弃 z)。
/// Upstream's Vector3.AsVector2() (drops z).
core::Vector2 AsVector2(core::Vector3 v) { return {v.X, v.Y}; }

/// 上游 Util.Lerp(float, float, float)(a + (b-a)*t)。
/// Upstream's Util.Lerp(float, float, float) (a + (b-a)*t).
float LerpF(float float_a, float float_b, float float_t) { return float_a + (float_b - float_a) * float_t; }

}  // namespace

// ———— SpriteRenderer ————

SpriteRenderer::SpriteRenderer(RenderThread& render, VertexBuffer& vertex_buffer, IndexBuffer& index_buffer,
                               Shader&& shader, IBatchRenderer** ptr_current_batch_slot,
                               std::int32_t int4_temp_vertex_buffer_size)
    : render_{render},
      vertex_buffer_{vertex_buffer},
      index_buffer_{index_buffer},
      shader_{std::move(shader)},
      ptr_current_batch_slot_{ptr_current_batch_slot},
      int4_temp_vertex_buffer_size_{int4_temp_vertex_buffer_size} {
  vec_vertices_.resize(static_cast<std::size_t>(int4_temp_vertex_buffer_size));

  // uniform 位置一次枚举(OPT-A6:热路径零字符串)。
  // Uniform locations enumerated once (OPT-A6: zero strings on hot paths).
  int4_loc_palette_rows_ = shader_.Location("PaletteRows");
  int4_loc_depth_tex_scale_ = shader_.Location("DepthTextureScale");
  int4_loc_scroll_ = shader_.Location("Scroll");
  int4_loc_p1_ = shader_.Location("p1");
  int4_loc_p2_ = shader_.Location("p2");
  int4_loc_enable_depth_preview_ = shader_.Location("EnableDepthPreview");
  int4_loc_depth_preview_params_ = shader_.Location("DepthPreviewParams");
  int4_loc_enable_pixel_art_scaling_ = shader_.Location("EnablePixelArtScaling");
}

void SpriteRenderer::ActivateAsCurrent() {
  if (ptr_current_batch_slot_ == nullptr || *ptr_current_batch_slot_ == this)
    return;
  if (*ptr_current_batch_slot_ != nullptr)
    (*ptr_current_batch_slot_)->Flush();
  *ptr_current_batch_slot_ = this;
}

void SpriteRenderer::Flush() {
  if (int4_vertex_count_ <= 0)
    return;

  for (std::int32_t i = 0; i < int4_sheet_count_; i++)
    shader_.SetTexture(kSheetSamplerNames[static_cast<std::size_t>(i)], arr_sheets_[static_cast<std::size_t>(i)]->GetTexture());

  shader_.PrepareRender();

  // OPT-A5:整批顶点一次进命令 payload,渲染线程等槽 fence 后 memcpy 进
  // 持久映射区;槽轮换隔开仍在被 GPU 消费的上一批。
  // OPT-A5: the whole batch rides the command payload once; the render
  // thread awaits the slot fence and memcpys into the persistent mapping,
  // with slot rotation spacing out the previous batch still under GPU
  // consumption.
  const std::uint32_t uint4_slot = vertex_buffer_.AcquireSlot();
  vertex_buffer_.WriteSlot(uint4_slot,
                           std::as_bytes(std::span{vec_vertices_.data(), static_cast<std::size_t>(int4_vertex_count_)}));
  vertex_buffer_.BindVao(shader_, &index_buffer_);

  // PERF: this allows us to batch render sprites with interleaved blend modes
  // without expensive binding and data writing between draw calls.
  // (SpriteRenderer.cs L76-77)
  for (const auto& span : tracker_spans_.Spans()) {
    if (span.int4_length <= 0)
      continue;

    if (GfxCmd* ptr_cmd = render_.queue().Reserve(GfxCmdKind::SetBlendMode, 0); ptr_cmd != nullptr) {
      ptr_cmd->uint4_a = static_cast<std::uint32_t>(span.kind_mode);
      render_.queue().CommitBare();
    }
    vertex_buffer_.DrawElements(static_cast<std::uint32_t>(span.int4_length / 4 * 6),
                                static_cast<std::uint32_t>(span.int4_start) * 6,
                                uint4_slot * vertex_buffer_.SlotVertices());
  }

  if (!tracker_spans_.Spans().empty() && tracker_spans_.Spans().back().kind_mode != BlendMode::None) {
    if (GfxCmd* ptr_cmd = render_.queue().Reserve(GfxCmdKind::SetBlendMode, 0); ptr_cmd != nullptr) {
      ptr_cmd->uint4_a = static_cast<std::uint32_t>(BlendMode::None);
      render_.queue().CommitBare();
    }
  }

  // 该槽本批全部 DrawElements 已提交,fence 之(下一次复用该槽时等待)。
  // Every DrawElements of this slot has been posted; fence it (awaited when
  // the slot is next reused).
  vertex_buffer_.FenceSlot(uint4_slot);

  tracker_spans_.Clear();
  std::fill(std::begin(arr_sheets_), std::end(arr_sheets_), nullptr);
  int4_vertex_count_ = 0;
  int4_sheet_count_ = 0;
}

int2 SpriteRenderer::SetRenderStateForSprite(const Sprite& sprite_r) {
  ActivateAsCurrent();

  if (int4_vertex_count_ + 4 > int4_temp_vertex_buffer_size_)
    Flush();

  // Check if the sheet (or secondary data sheet) have already been mapped
  // (SpriteRenderer.cs L118-121)
  Sheet* ptr_sheet = sprite_r.ptr_sheet;
  std::int32_t int4_sheet_index = 0;
  for (; int4_sheet_index < int4_sheet_count_; int4_sheet_index++)
    if (arr_sheets_[static_cast<std::size_t>(int4_sheet_index)] == ptr_sheet)
      break;

  std::int32_t int4_secondary_sheet_index = 0;
  const bool b_secondary = sprite_r.b_secondary;  // 载荷已在基类 | the payload sits in the base
  if (b_secondary) {
    Sheet* ptr_secondary_sheet = sprite_r.ptr_secondary_sheet;
    for (; int4_secondary_sheet_index < int4_sheet_count_; int4_secondary_sheet_index++)
      if (arr_sheets_[static_cast<std::size_t>(int4_secondary_sheet_index)] == ptr_secondary_sheet)
        break;

    // If neither sheet has been mapped both index values will be set to ns.
    // This is fine if they both reference the same texture, but if they don't
    // we must increment the secondary sheet index to the next free sampler.
    // (SpriteRenderer.cs L134-138)
    if (int4_secondary_sheet_index == int4_sheet_index && ptr_secondary_sheet != ptr_sheet)
      int4_secondary_sheet_index++;
  }

  // Make sure that we have enough free samplers to map both if needed,
  // otherwise flush (SpriteRenderer.cs L141-147)
  if (std::max(int4_sheet_index, int4_secondary_sheet_index) >= kSheetCount) {
    Flush();
    int4_sheet_index = 0;
    // 载荷在基类(sprite.hpp 第十四批头注)| the payload sits in the base
    // (the fourteenth-batch note in sprite.hpp).
    int4_secondary_sheet_index =
        sprite_r.b_secondary && sprite_r.ptr_secondary_sheet != ptr_sheet ? 1 : 0;
  }

  if (int4_sheet_index >= int4_sheet_count_) {
    arr_sheets_[static_cast<std::size_t>(int4_sheet_index)] = ptr_sheet;
    int4_sheet_count_++;
  }

  if (int4_secondary_sheet_index >= int4_sheet_count_ && b_secondary) {
    arr_sheets_[static_cast<std::size_t>(int4_secondary_sheet_index)] = sprite_r.ptr_secondary_sheet;
    int4_sheet_count_++;
  }

  return int2{int4_sheet_index, int4_secondary_sheet_index};
}

std::int32_t SpriteRenderer::ResolveTextureIndex(const Sprite& sprite_r, const PaletteReference* ptr_palette) {
  if (ptr_palette == nullptr)
    return 0;

  // PERF: Remove useless palette assignments for RGBA sprites
  // HACK: This is working around the limitation that palettes are defined on
  // traits rather than on sequences, and can be removed once this has been
  // fixed (SpriteRenderer.cs L169-172)
  if (sprite_r.kind_channel == TextureChannel::RGBA && !ptr_palette->HasColorShift())
    return 0;

  return ptr_palette->TextureIndex();
}

void SpriteRenderer::DrawSprite(const Sprite& sprite_r, std::int32_t int4_palette_texture_index,
                                core::Vector3 v_location, core::Vector3 v_scale, float float_rotation) {
  const int2 int2_samplers = SetRenderStateForSprite(sprite_r);
  FastCreateQuad(vec_vertices_, v_location + v_scale * sprite_r.vec_offset, sprite_r, int2_samplers,
                 int4_palette_texture_index, int4_vertex_count_, v_scale * sprite_r.vec_size, core::Vector3{1, 1, 1},
                 1.0f, float_rotation);
  tracker_spans_.TrackQuad(sprite_r.kind_blend, int4_vertex_count_);
  int4_vertex_count_ += 4;
}

void SpriteRenderer::DrawSprite(const Sprite& sprite_r, std::int32_t int4_palette_texture_index,
                                core::Vector3 v_location, float float_scale, float float_rotation) {
  const int2 int2_samplers = SetRenderStateForSprite(sprite_r);
  FastCreateQuad(vec_vertices_, v_location + float_scale * sprite_r.vec_offset, sprite_r, int2_samplers,
                 int4_palette_texture_index, int4_vertex_count_, float_scale * sprite_r.vec_size,
                 core::Vector3{1, 1, 1}, 1.0f, float_rotation);
  tracker_spans_.TrackQuad(sprite_r.kind_blend, int4_vertex_count_);
  int4_vertex_count_ += 4;
}

void SpriteRenderer::DrawSprite(const Sprite& sprite_r, const PaletteReference* ptr_palette,
                                core::Vector3 v_location, float float_scale, float float_rotation) {
  DrawSprite(sprite_r, ResolveTextureIndex(sprite_r, ptr_palette), v_location, float_scale, float_rotation);
}

void SpriteRenderer::DrawSprite(const Sprite& sprite_r, std::int32_t int4_palette_texture_index,
                                core::Vector3 v_location, float float_scale, core::Vector3 v_tint, float float_alpha,
                                float float_rotation) {
  const int2 int2_samplers = SetRenderStateForSprite(sprite_r);
  FastCreateQuad(vec_vertices_, v_location + float_scale * sprite_r.vec_offset, sprite_r, int2_samplers,
                 int4_palette_texture_index, int4_vertex_count_, float_scale * sprite_r.vec_size, v_tint,
                 float_alpha, float_rotation);
  tracker_spans_.TrackQuad(sprite_r.kind_blend, int4_vertex_count_);
  int4_vertex_count_ += 4;
}

void SpriteRenderer::DrawSprite(const Sprite& sprite_r, const PaletteReference* ptr_palette,
                                core::Vector3 v_location, float float_scale, core::Vector3 v_tint, float float_alpha,
                                float float_rotation) {
  DrawSprite(sprite_r, ResolveTextureIndex(sprite_r, ptr_palette), v_location, float_scale, v_tint, float_alpha,
             float_rotation);
}

void SpriteRenderer::DrawSprite(const Sprite& sprite_r, std::int32_t int4_palette_texture_index, core::Vector3 v_a,
                                core::Vector3 v_b, core::Vector3 v_c, core::Vector3 v_d, core::Vector3 v_tint,
                                float float_alpha) {
  const int2 int2_samplers = SetRenderStateForSprite(sprite_r);
  FastCreateQuad(vec_vertices_, v_a, v_b, v_c, v_d, sprite_r, int2_samplers, int4_palette_texture_index, v_tint,
                 float_alpha, int4_vertex_count_);
  tracker_spans_.TrackQuad(sprite_r.kind_blend, int4_vertex_count_);
  int4_vertex_count_ += 4;
}

void SpriteRenderer::DrawVertexBuffer(VertexBuffer& vertex_buffer, IndexBuffer& index_buffer,
                                      std::int32_t int4_start, std::int32_t int4_length,
                                      std::span<Sheet* const> vec_sheets, BlendMode kind_blend) {
  std::int32_t int4_i = 0;
  for (Sheet* ptr_sheet : vec_sheets) {
    if (int4_i >= kSheetCount)
      throw std::invalid_argument(
          std::format("SpriteRenderer only supports {} simultaneous textures", kSheetCount));

    if (ptr_sheet != nullptr)
      shader_.SetTexture(kSheetSamplerNames[static_cast<std::size_t>(int4_i++)], ptr_sheet->GetTexture());
  }

  if (GfxCmd* ptr_cmd = render_.queue().Reserve(GfxCmdKind::SetBlendMode, 0); ptr_cmd != nullptr) {
    ptr_cmd->uint4_a = static_cast<std::uint32_t>(kind_blend);
    render_.queue().CommitBare();
  }
  shader_.PrepareRender();

  // 上游 renderer.DrawQuadBatch(buffer, indices, shader, length, UintSize *
  // start):DrawElements(length 个索引,字节偏移 4*start)(Renderer.cs L372-380)。
  // Upstream's renderer.DrawQuadBatch(buffer, indices, shader, length,
  // UintSize * start): DrawElements(length indices at byte offset 4*start)
  // (Renderer.cs L372-380).
  vertex_buffer.BindVao(shader_, &index_buffer);
  vertex_buffer.DrawElements(static_cast<std::uint32_t>(int4_length), kUintSize * static_cast<std::uint32_t>(int4_start));

  if (GfxCmd* ptr_cmd = render_.queue().Reserve(GfxCmdKind::SetBlendMode, 0); ptr_cmd != nullptr) {
    ptr_cmd->uint4_a = static_cast<std::uint32_t>(BlendMode::None);
    render_.queue().CommitBare();
  }
}

void SpriteRenderer::DrawRGBAQuad(std::span<const Vertex, 4> vec_vertices, BlendMode kind_mode) {
  ActivateAsCurrent();

  if (int4_vertex_count_ + 4 > int4_temp_vertex_buffer_size_)
    Flush();

  std::copy(vec_vertices.begin(), vec_vertices.end(), vec_vertices_.begin() + int4_vertex_count_);
  tracker_spans_.TrackQuad(kind_mode, int4_vertex_count_);
  int4_vertex_count_ += 4;
}

void SpriteRenderer::SetPalette(HardwarePalette& palette) {
  Texture* ptr_palette_texture = palette.TextureOrNull();
  Texture* ptr_shifts_texture = palette.ColorShiftsOrNull();
  assert(ptr_palette_texture != nullptr && ptr_shifts_texture != nullptr);
  shader_.SetTexture("Palette", *ptr_palette_texture);
  shader_.SetTexture("ColorShifts", *ptr_shifts_texture);
  shader_.SetVecAt(int4_loc_palette_rows_, static_cast<float>(palette.Height()));
}

void SpriteRenderer::SetViewportParams(int2 int2_sheet_size, std::int32_t int4_downscale,
                                       float float_depth_margin, int2 int2_scroll) {
  // OpenGL only renders x and y coordinates inside [-1, 1] range. We project
  // world coordinates using p1 to values [0, 2] and then subtract by 1 using
  // p2, where p stands for projection. It's standard practice for shaders to
  // use a projection matrix, but as we project orthographically we are able
  // to send less data to the GPU. (SpriteRenderer.cs L265-271)
  const float float_width = 2.0f / (static_cast<float>(int4_downscale) * static_cast<float>(int2_sheet_size.X));
  const float float_height = 2.0f / (static_cast<float>(int4_downscale) * static_cast<float>(int2_sheet_size.Y));

  // * The OpenGL z axis is inverted (negative is closer) relative to OpenRA
  //   (positive is closer).
  // * We want to avoid clipping pixels that are behind the nominal z == y
  //   plane at the top of the map, or above the nominal z == y plane at the
  //   bottom of the map. We therefore expand the depth range by an extra
  //   margin that is calculated based on the maximum expected world height
  //   (see Renderer.InitializeDepthBuffer).
  // * Sprites can specify an additional per-pixel depth offset map, which is
  //   applied in the fragment shader. The fragment shader operates in OpenGL
  //   window coordinates, not NDC, with a depth range [0, 1] corresponding to
  //   the NDC [-1, 1]. We must therefore multiply the sprite channel value
  //   [0, 1] by 255 to find the pixel depth offset, then by our depth scale
  //   to find the equivalent NDC offset, then divide by 2 to find the window
  //   coordinate offset.
  // * If depthMargin == 0 (which indicates per-pixel depth testing is
  //   disabled) sprites that extend beyond the top of bottom edges of the
  //   screen may be pushed outside [-1, 1] and culled by the GPU. We avoid
  //   this by forcing everything into the z = 0 plane.
  // (SpriteRenderer.cs L273-287)
  const float float_depth = float_depth_margin != 0.0f
                                ? 2.0f / (static_cast<float>(int4_downscale) *
                                          static_cast<float>(int2_sheet_size.Y + static_cast<std::int32_t>(float_depth_margin)))
                                : 0.0f;
  shader_.SetVecAt(int4_loc_depth_tex_scale_, 128 * float_depth);
  shader_.SetVecAt(int4_loc_scroll_, static_cast<float>(int2_scroll.X), static_cast<float>(int2_scroll.Y),
                   float_depth_margin != 0.0f ? static_cast<float>(int2_scroll.Y) : 0.0f);
  shader_.SetVecAt(int4_loc_p1_, float_width, float_height, -float_depth);
  shader_.SetVecAt(int4_loc_p2_, -1.0f, -1.0f, float_depth_margin != 0.0f ? 1.0f : 0.0f);
}

void SpriteRenderer::SetDepthPreview(bool b_enabled, float float_contrast, float float_offset) {
  shader_.SetBoolAt(int4_loc_enable_depth_preview_, b_enabled);
  shader_.SetVecAt(int4_loc_depth_preview_params_, float_contrast, float_offset);
}

void SpriteRenderer::EnablePixelArtScaling(bool b_enabled) {
  shader_.SetBoolAt(int4_loc_enable_pixel_art_scaling_, b_enabled);
}

// ———— RgbaSpriteRenderer ————

namespace {
/// Channel != RGBA 的上游抛出文本(RgbaSpriteRenderer.cs L28-29 等)。
/// The upstream throw text for Channel != RGBA (RgbaSpriteRenderer.cs
/// L28-29 et al.).
void RequireRgbaChannel(const Sprite& sprite_r) {
  if (sprite_r.kind_channel != TextureChannel::RGBA)
    throw std::runtime_error("DrawRGBASprite requires a RGBA sprite.");
}
}  // namespace

void RgbaSpriteRenderer::DrawSprite(const Sprite& sprite_r, core::Vector3 v_location, core::Vector3 v_scale,
                                    float float_rotation) {
  RequireRgbaChannel(sprite_r);
  parent_.DrawSprite(sprite_r, 0, v_location, v_scale, float_rotation);
}

void RgbaSpriteRenderer::DrawSprite(const Sprite& sprite_r, core::Vector3 v_location, float float_scale,
                                    float float_rotation) {
  RequireRgbaChannel(sprite_r);
  parent_.DrawSprite(sprite_r, 0, v_location, float_scale, float_rotation);
}

void RgbaSpriteRenderer::DrawSprite(const Sprite& sprite_r, core::Vector3 v_location, float float_scale,
                                    core::Vector3 v_tint, float float_alpha, float float_rotation) {
  RequireRgbaChannel(sprite_r);
  parent_.DrawSprite(sprite_r, 0, v_location, float_scale, v_tint, float_alpha, float_rotation);
}

void RgbaSpriteRenderer::DrawSprite(const Sprite& sprite_r, core::Vector3 v_a, core::Vector3 v_b, core::Vector3 v_c,
                                    core::Vector3 v_d, core::Vector3 v_tint, float float_alpha) {
  RequireRgbaChannel(sprite_r);
  parent_.DrawSprite(sprite_r, 0, v_a, v_b, v_c, v_d, v_tint, float_alpha);
}

// ———— RgbaColorRenderer(RgbaColorRenderer.cs L20-256 几何逐行)————
// ———— RgbaColorRenderer (the geometry of RgbaColorRenderer.cs L20-256 line by line) ————

void RgbaColorRenderer::DrawLine(core::Vector3 v_start, core::Vector3 v_end, float float_width,
                                 core::Color color_start, core::Color color_end, BlendMode kind_mode) {
  static constexpr core::Vector3 kOffset{0.5f, 0.5f, 0.0f};

  const core::Vector3 v_direction = v_end - v_start;
  const core::Vector2 v_delta2 = AsVector2(v_direction) * (1.0f / Length(AsVector2(v_direction)));
  const core::Vector3 v_corner =
      (float_width / 2) * core::Vector3{-v_delta2.Y, v_delta2.X, v_direction.Z};

  const ColorF4 v_sc = ColorToVector4(color_start);
  const ColorF4 v_ec = ColorToVector4(color_end);

  const std::array<Vertex, 4> arr_vertices{
      VertexColor(v_start - v_corner + kOffset, v_sc),
      VertexColor(v_start + v_corner + kOffset, v_sc),
      VertexColor(v_end + v_corner + kOffset, v_ec),
      VertexColor(v_end - v_corner + kOffset, v_ec),
  };
  sink_.DrawRGBAQuad(arr_vertices, kind_mode);
}

void RgbaColorRenderer::DrawLine(core::Vector2 v_start, core::Vector2 v_end, float float_width, core::Color color_c,
                                 BlendMode kind_mode) {
  DrawLine(core::Vector3{v_start.X, v_start.Y, 0}, core::Vector3{v_end.X, v_end.Y, 0}, float_width, color_c,
           kind_mode);
}

void RgbaColorRenderer::DrawLine(core::Vector3 v_start, core::Vector3 v_end, float float_width, core::Color color_c,
                                 BlendMode kind_mode) {
  static constexpr core::Vector3 kOffset{0.5f, 0.5f, 0.0f};

  const core::Vector3 v_direction = v_end - v_start;
  const core::Vector2 v_delta2 = AsVector2(v_direction) * (1.0f / Length(AsVector2(v_direction)));
  const core::Vector3 v_corner = (float_width / 2) * core::Vector3{-v_delta2.Y, v_delta2.X, 0};

  const ColorF4 v_color = ColorToVector4(color_c);

  const std::array<Vertex, 4> arr_vertices{
      VertexColor(v_start - v_corner + kOffset, v_color),
      VertexColor(v_start + v_corner + kOffset, v_color),
      VertexColor(v_end + v_corner + kOffset, v_color),
      VertexColor(v_end - v_corner + kOffset, v_color),
  };
  sink_.DrawRGBAQuad(arr_vertices, kind_mode);
}

/// Calculate the 2D intersection of two lines.
/// Will behave badly if the lines are parallel.
/// Z position is the average of a and b (ignores actual intersection point if
/// it exists). (RgbaColorRenderer.cs L69-82)
core::Vector3 RgbaColorRenderer::IntersectionOf(core::Vector3 v_a, core::Vector3 v_da, core::Vector3 v_b,
                                                core::Vector3 v_db) {
  const float float_cross_a = v_a.X * (v_a.Y + v_da.Y) - v_a.Y * (v_a.X + v_da.X);
  const float float_cross_b = v_b.X * (v_b.Y + v_db.Y) - v_b.Y * (v_b.X + v_db.X);
  const float float_x = v_da.X * float_cross_b - v_db.X * float_cross_a;
  const float float_y = v_da.Y * float_cross_b - v_db.Y * float_cross_a;
  const float float_d = v_da.X * v_db.Y - v_da.Y * v_db.X;
  return core::Vector3{float_x / float_d, float_y / float_d, 0.5f * (v_a.Z + v_b.Z)};
}

void RgbaColorRenderer::DrawDisconnectedLine(std::span<const core::Vector3> vec_points, float float_width,
                                             core::Color color_c, BlendMode kind_mode) {
  if (vec_points.empty())
    return;
  core::Vector3 v_last = vec_points.front();
  for (std::size_t i = 1; i < vec_points.size(); ++i) {
    DrawLine(v_last, vec_points[i], float_width, color_c, kind_mode);
    v_last = vec_points[i];
  }
}

void RgbaColorRenderer::DrawConnectedLine(std::span<const core::Vector3> vec_points, float float_width,
                                          core::Color color_c, bool b_closed, BlendMode kind_mode) {
  static constexpr core::Vector3 kOffset{0.5f, 0.5f, 0.0f};

  // Not a line (RgbaColorRenderer.cs L115-118)
  if (vec_points.size() < 2)
    return;

  // Single segment (L120-124)
  if (vec_points.size() == 2) {
    DrawLine(vec_points[0], vec_points[1], float_width, color_c, kind_mode);
    return;
  }

  const ColorF4 v_color = ColorToVector4(color_c);

  const core::Vector3 v_start = vec_points[0];
  core::Vector3 v_end = vec_points[1];
  core::Vector3 v_delta = v_end - v_start;
  core::Vector2 v_dir = AsVector2(v_delta) * (1.0f / Length(AsVector2(v_delta)));
  core::Vector3 v_corner = (float_width / 2) * core::Vector3{-v_dir.Y, v_dir.X, v_delta.Z};

  // Corners for start of line segment (L128-131)
  core::Vector3 v_ca = v_start - v_corner;
  core::Vector3 v_cb = v_start + v_corner;

  // Segment is part of closed loop (L133-147)
  if (b_closed) {
    const core::Vector3 v_prev = vec_points.back();
    const core::Vector3 v_prev_delta = v_start - v_prev;
    const core::Vector2 v_prev_dir = AsVector2(v_prev_delta) * (1.0f / Length(AsVector2(v_prev_delta)));
    const core::Vector3 v_prev_corner = (float_width / 2) * core::Vector3{-v_prev_dir.Y, v_prev_dir.X, v_prev_delta.Z};
    v_ca = IntersectionOf(v_start - v_prev_corner, core::Vector3{v_prev_dir.X, v_prev_dir.Y, 0},
                          v_start - v_corner, core::Vector3{v_dir.X, v_dir.Y, 0});
    v_cb = IntersectionOf(v_start + v_prev_corner, core::Vector3{v_prev_dir.X, v_prev_dir.Y, 0},
                          v_start + v_corner, core::Vector3{v_dir.X, v_dir.Y, 0});
  }

  const std::size_t size_limit = b_closed ? vec_points.size() : vec_points.size() - 1;
  for (std::size_t i = 0; i < size_limit; ++i) {
    const core::Vector3 v_next = vec_points[(i + 2) % vec_points.size()];
    const core::Vector3 v_next_delta = v_next - v_end;
    const core::Vector2 v_next_dir = AsVector2(v_next_delta) * (1.0f / Length(AsVector2(v_next_delta)));
    const core::Vector3 v_next_corner = (float_width / 2) * core::Vector3{-v_next_dir.Y, v_next_dir.X, v_next_delta.Z};

    // Vertices for the corners joining start-end to end-next (L157-159)
    const core::Vector3 v_cc =
        b_closed || i < size_limit - 1
            ? IntersectionOf(v_end + v_corner, core::Vector3{v_dir.X, v_dir.Y, 0}, v_end + v_next_corner,
                             core::Vector3{v_next_dir.X, v_next_dir.Y, 0})
            : v_end + v_corner;
    const core::Vector3 v_cd =
        b_closed || i < size_limit - 1
            ? IntersectionOf(v_end - v_corner, core::Vector3{v_dir.X, v_dir.Y, 0}, v_end - v_next_corner,
                             core::Vector3{v_next_dir.X, v_next_dir.Y, 0})
            : v_end - v_corner;

    // Fill segment (L161-166)
    const std::array<Vertex, 4> arr_vertices{
        VertexColor(v_ca + kOffset, v_color), VertexColor(v_cb + kOffset, v_color),
        VertexColor(v_cc + kOffset, v_color), VertexColor(v_cd + kOffset, v_color),
    };
    sink_.DrawRGBAQuad(arr_vertices, kind_mode);

    // Advance line segment (L168-174)
    v_end = v_next;
    v_dir = v_next_dir;
    v_corner = v_next_corner;
    v_ca = v_cd;
    v_cb = v_cc;
  }
}

void RgbaColorRenderer::DrawLine(std::span<const core::Vector3> vec_points, float float_width, core::Color color_c,
                                 bool b_connect_segments, BlendMode kind_mode) {
  if (!b_connect_segments)
    DrawDisconnectedLine(vec_points, float_width, color_c, kind_mode);
  else
    DrawConnectedLine(vec_points, float_width, color_c, false, kind_mode);
}

void RgbaColorRenderer::DrawPolygon(std::span<const core::Vector3> vec_vertices, float float_width,
                                    core::Color color_c, BlendMode kind_mode) {
  DrawConnectedLine(vec_vertices, float_width, color_c, true, kind_mode);
}

void RgbaColorRenderer::DrawPolygon(std::span<const core::Vector2> vec_vertices, float float_width,
                                    core::Color color_c, BlendMode kind_mode) {
  if (vec_vertices.size() <= 1024) {
    core::Vector3 arr_stack[1024];
    for (std::size_t i = 0; i < vec_vertices.size(); ++i)
      arr_stack[i] = core::Vector3{vec_vertices[i].X, vec_vertices[i].Y, 0};
    DrawConnectedLine(std::span{arr_stack, vec_vertices.size()}, float_width, color_c, true, kind_mode);
  } else {
    std::vector<core::Vector3> vec_points3d(vec_vertices.size());
    for (std::size_t i = 0; i < vec_vertices.size(); ++i)
      vec_points3d[i] = core::Vector3{vec_vertices[i].X, vec_vertices[i].Y, 0};
    DrawConnectedLine(vec_points3d, float_width, color_c, true, kind_mode);
  }
}

void RgbaColorRenderer::DrawRect(core::Vector3 v_tl, core::Vector3 v_br, float float_width, core::Color color_c,
                                 BlendMode kind_mode) {
  const core::Vector3 v_tr{v_br.X, v_tl.Y, v_tl.Z};
  const core::Vector3 v_bl{v_tl.X, v_br.Y, v_br.Z};
  const std::array<core::Vector3, 4> arr_corners{v_tl, v_tr, v_br, v_bl};
  DrawPolygon(arr_corners, float_width, color_c, kind_mode);
}

void RgbaColorRenderer::FillRect(core::Vector3 v_tl, core::Vector3 v_br, core::Color color_c, BlendMode kind_mode) {
  const core::Vector3 v_tr{v_br.X, v_tl.Y, v_tl.Z};
  const core::Vector3 v_bl{v_tl.X, v_br.Y, v_br.Z};
  FillRect(v_tl, v_tr, v_br, v_bl, color_c, kind_mode);
}

void RgbaColorRenderer::FillRect(core::Vector3 v_a, core::Vector3 v_b, core::Vector3 v_c, core::Vector3 v_d,
                                 core::Color color_c, BlendMode kind_mode) {
  const ColorF4 v_color = ColorToVector4(color_c);

  const std::array<Vertex, 4> arr_vertices{
      VertexColor(v_a + core::Vector3{0.5f, 0.5f, 0}, v_color),
      VertexColor(v_b + core::Vector3{0.5f, 0.5f, 0}, v_color),
      VertexColor(v_c + core::Vector3{0.5f, 0.5f, 0}, v_color),
      VertexColor(v_d + core::Vector3{0.5f, 0.5f, 0}, v_color),
  };
  sink_.DrawRGBAQuad(arr_vertices, kind_mode);
}

void RgbaColorRenderer::FillRect(core::Vector3 v_a, core::Vector3 v_b, core::Vector3 v_c, core::Vector3 v_d,
                                 core::Color color_top_left, core::Color color_top_right,
                                 core::Color color_bottom_right, core::Color color_bottom_left,
                                 BlendMode kind_mode) {
  static constexpr core::Vector3 kOffset{0.5f, 0.5f, 0.0f};

  const std::array<Vertex, 4> arr_vertices{
      VertexColor(v_a + kOffset, ColorToVector4(color_top_left)),
      VertexColor(v_b + kOffset, ColorToVector4(color_top_right)),
      VertexColor(v_c + kOffset, ColorToVector4(color_bottom_right)),
      VertexColor(v_d + kOffset, ColorToVector4(color_bottom_left)),
  };
  sink_.DrawRGBAQuad(arr_vertices, kind_mode);
}

void RgbaColorRenderer::FillEllipse(core::Vector3 v_tl, core::Vector3 v_br, core::Color color_c,
                                    BlendMode kind_mode) {
  // TODO: Create an ellipse polygon instead (RgbaColorRenderer.cs L240)
  const float float_a = (v_br.X - v_tl.X) / 2;
  const float float_b = (v_br.Y - v_tl.Y) / 2;
  const float float_xc = (v_br.X + v_tl.X) / 2;
  const float float_yc = (v_br.Y + v_tl.Y) / 2;

  const float float_height = v_br.Y - v_tl.Y;
  for (float float_y = v_tl.Y; float_y <= v_br.Y; float_y++) {
    const float float_z = LerpF(v_tl.Z, v_br.Z, (float_y - v_tl.Y) / float_height);
    const float float_t = (float_y - float_yc) / float_b;
    const float float_dx = float_a * std::sqrt(1 - float_t * float_t);
    DrawLine(core::Vector3{float_xc - float_dx, float_y, float_z},
             core::Vector3{float_xc + float_dx, float_y, float_z}, 1, color_c, kind_mode);
  }
}

}  // namespace ora::gfx
