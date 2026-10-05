// UPSTREAM: OpenRA.Game/Renderer.cs @b6fc03f L24-585(实现)
// 头文件携带完整 UPSTREAM 锚点、OPT-B1 单级合成论证与形态适配清单。
// Implementation of Renderer.cs; the header carries the full UPSTREAM
// anchors, the OPT-B1 single-pass compositing argument, and the shape
// adaptations.
#include "gfx/renderer.hpp"


namespace ora::gfx {

namespace {

/// int2.FromVector(Vector2)((int) 向零截断;上游 int2.cs 渲染域浮点形态)。
/// int2.FromVector(Vector2) ((int) truncates towards zero; the render-domain
/// float form of upstream's int2.cs).
int2 FromVector(core::Vector2 v) {
  return int2{static_cast<std::int32_t>(v.X), static_cast<std::int32_t>(v.Y)};
}

/// .NET float.IsInteger。
/// .NET's float.IsInteger.
bool IsInteger(float float_v) { return std::trunc(float_v) == float_v; }

/// .NET Vector2.Round(逐分量 MathF.Round 默认档 = 就近偶舍入;
/// std::nearbyint 在默认 FE_TONEAREST 下同语义)。
/// .NET's Vector2.Round (per-component MathF.Round with its default
/// banker's rounding; std::nearbyint under the default FE_TONEAREST
/// matches).
core::Vector2 RoundVector(core::Vector2 v) {
  return core::Vector2{std::nearbyint(v.X), std::nearbyint(v.Y)};
}

}  // namespace

WorldSpriteParams ComputeWorldSpriteParams(int2 int2_world_sheet_size, int2 int2_viewport_size,
                                           core::Vector2 vec_viewport_location, int2 int2_center_location,
                                           float float_screen_width) {
  // Downscale world rendering if needed to fit within the framebuffer
  // (Renderer.cs L253-260)
  const std::int32_t int4_viewport_width = int2_viewport_size.X;
  const std::int32_t int4_viewport_height = int2_viewport_size.Y;
  const std::int32_t int4_buffer_width = int2_world_sheet_size.X;
  const std::int32_t int4_buffer_height = int2_world_sheet_size.Y;
  std::int32_t int4_downscale = 1;
  while (int4_viewport_width / int4_downscale > int4_buffer_width ||
         int4_viewport_height / int4_downscale > int4_buffer_height)
    int4_downscale++;

  // We need to add 1 to scroll in order to handle interpixel 0-0.99
  // fractionalOffset. (Renderer.cs L262-263)
  const int2 int2_size_sub{int4_viewport_width / int4_downscale + 1,
                           int4_viewport_height / int4_downscale + 1};
  core::Vector2 vec_fractional_offset =
      core::Vector2{static_cast<float>(int2_center_location.X), static_cast<float>(int2_center_location.Y)} -
      vec_viewport_location;

  // If scaling by an integer factor (including 1:1) we must round the offset
  // to an integer number of screen-space pixels to preserve sharp pixel
  // edges (Renderer.cs L266-270)
  const float float_render_scale = float_screen_width / static_cast<float>(int2_size_sub.X - 1);
  if (IsInteger(float_render_scale))
    vec_fractional_offset = RoundVector(vec_fractional_offset * float_render_scale) * (1.0f / float_render_scale);

  return WorldSpriteParams{int4_downscale, int2_size_sub, vec_fractional_offset};
}

Renderer::Renderer(platform::Sdl2Window& window, RenderThread& render, const Desc& desc)
    : window_{window}, render_{render} {
  // Renderer.cs L93-94:批次尺寸按 4 对齐。
  // Renderer.cs L93-94: batch sizes align to 4.
  const std::int32_t int4_temp_vertex_buffer_size =
      desc.int4_vertex_batch_size - desc.int4_vertex_batch_size % 4;
  const std::int32_t int4_temp_index_buffer_size = int4_temp_vertex_buffer_size / 4 * 6;

  // combined 着色器 ×2(world/UI 各一,上游两个 SpriteRenderer 实例共享
  // VB/IB;Renderer.cs L105-111)。属性表 = CombinedShaderBindings(Vertex.cs
  // L57-63,C++ 侧 vertex.hpp 的 kCombinedAttributes 契约)。
  // Two combined shaders (one per world/UI — upstream's two SpriteRenderer
  // instances share VB/IB; Renderer.cs L105-111). The attribute table =
  // CombinedShaderBindings (Vertex.cs L57-63 — the C++ kCombinedAttributes
  // contract in vertex.hpp).
  const std::vector<ShaderVertexAttribute> vec_attributes = MakeCombinedAttributes();
  const std::int32_t int4_stride = static_cast<std::int32_t>(sizeof(Vertex));

  ShaderBindingsDesc desc_bindings{"combined", desc.str_combined_vert, "combined", desc.str_combined_frag,
                                   int4_stride, vec_attributes};

  // 持久 VB:三槽 × 全批顶点(OPT-A5;上游 CreateEmptyVertexBuffer +
  // GL_DYNAMIC_DRAW)。
  // The persistent VB: three slots × the full batch (OPT-A5; upstream's
  // CreateEmptyVertexBuffer + GL_DYNAMIC_DRAW).
  opt_vertex_buffer_.emplace(render_, vec_attributes, int4_stride);
  opt_vertex_buffer_->InitPersistent(3 * static_cast<std::uint32_t>(int4_temp_vertex_buffer_size) * sizeof(Vertex), 3);

  const std::vector<std::uint32_t> vec_quad_indices = CreateQuadIndices(int4_temp_index_buffer_size / 6);
  opt_index_buffer_.emplace(render_, vec_quad_indices);

  auto opt_shader_world = Shader::Create(render_, desc_bindings);
  auto opt_shader_ui = Shader::Create(render_, desc_bindings);
  assert(opt_shader_world.has_value() && opt_shader_ui.has_value());

  ptr_world_sprite_renderer_ = std::make_unique<SpriteRenderer>(
      render_, *opt_vertex_buffer_, *opt_index_buffer_, std::move(*opt_shader_world), &ptr_current_batch_,
      int4_temp_vertex_buffer_size);
  ptr_world_rgba_sprite_renderer_ = std::make_unique<RgbaSpriteRenderer>(*ptr_world_sprite_renderer_);
  ptr_world_rgba_color_renderer_ = std::make_unique<RgbaColorRenderer>(*ptr_world_sprite_renderer_);
  ptr_ui_sprite_renderer_ = std::make_unique<SpriteRenderer>(render_, *opt_vertex_buffer_, *opt_index_buffer_,
                                                             std::move(*opt_shader_ui), &ptr_current_batch_,
                                                             int4_temp_vertex_buffer_size);
  ptr_ui_rgba_sprite_renderer_ = std::make_unique<RgbaSpriteRenderer>(*ptr_ui_sprite_renderer_);
  ptr_ui_rgba_color_renderer_ = std::make_unique<RgbaColorRenderer>(*ptr_ui_sprite_renderer_);
}

Renderer::~Renderer() {
  // 上游 Dispose 序(Renderer.cs L546-558):world/screen buffer、快照纹理、
  // VB/IB、字体;单级下 screen buffer 不存在,unique_ptr/RAII 逆序自动覆盖。
  // Upstream's Dispose order (Renderer.cs L546-558): the world/screen
  // buffers, the snapshot texture, the VB/IB, the fonts; the screen buffer
  // does not exist single-pass, and RAII reverse-order destruction covers
  // the rest.
  SetCurrentBatchRenderer(nullptr);
}

void Renderer::BeginFrame() {
  // OPT-B1:上游此处 Context.Clear() 默认帧缓冲(上游自认冗余 —— 终末全屏
  // quad 覆盖)与本批删除;screen buffer/sprite 的重建一并消失。仅保留 UI
  // 投影参数的按需更新(Renderer.cs L192-198)。
  // OPT-B1: upstream's Context.Clear() of the default framebuffer here (its
  // own comment calls it redundant — the final fullscreen quad overwrites
  // it) is deleted with this batch, along with the screen-buffer/sprite
  // rebuilds; only the on-demand UI projection update remains (Renderer.cs
  // L192-198).
  const int2 int2_surface_size = SurfaceSize();
  const float float_scale = EffectiveWindowScale();
  const int2 int2_buffer_size{
      static_cast<std::int32_t>(static_cast<float>(int2_surface_size.X) / float_scale),
      static_cast<std::int32_t>(static_cast<float>(int2_surface_size.Y) / float_scale)};
  if (!(int2_last_buffer_size_ == int2_buffer_size)) {
    UISpriteRenderer().SetViewportParams(int2_buffer_size, 1, 0.0f, int2{0, 0});
    int2_last_buffer_size_ = int2_buffer_size;
  }
}

void Renderer::SetMaximumViewportSize(int2 int2_size) {
  // Aim to render the world into a framebuffer at 1:1 scaling which is then
  // up/downscaled using a custom filter to provide crisp scaling and avoid
  // rendering glitches when the depth buffer is used and samples don't
  // match. This approach does not scale well to large sizes, first
  // saturating GPU fill rate and then crashing when reaching the
  // framebuffer size limits (typically 16k). We therefore clamp the maximum
  // framebuffer size to twice the window surface size, which strikes a
  // reasonable balance between rendering quality and performance. Mods that
  // use the depth buffer must instead limit their artwork resolution or
  // maximum zoom-out levels. (Renderer.cs L202-208)
  int2 int2_world_buffer_size;
  if (float_depth_margin_ == 0.0f) {
    const int2 int2_surface_size = SurfaceSize();
    // OPT-B1:上游此处 min(视口, 2×表面) 后再 NextPowerOf2()(Renderer.cs
    // L213);NPOT 直用精确尺寸(FBO 每视口像素 1:1,省填充与内存;
    // SetMaximumViewportSize 仅在窗口/缩放变化时到达,重建频率不受影响)。
    // OPT-B1: upstream follows the min with NextPowerOf2() here (Renderer.cs
    // L213); NPOT uses the exact size directly (1:1 framebuffer pixels per
    // viewport pixel, saving fill and memory; SetMaximumViewportSize is
    // reached only on window/scale changes, so the rebuild frequency is
    // unaffected).
    int2_world_buffer_size =
        int2{std::min(int2_size.X, 2 * int2_surface_size.X), std::min(int2_size.Y, 2 * int2_surface_size.Y)};
  } else {
    int2_world_buffer_size = int2_size;
  }

  if (!(opt_world_sprite_.has_value() && ptr_world_sheet_ != nullptr &&
        ptr_world_sheet_->Size() == int2_world_buffer_size)) {
    ptr_world_buffer_.reset();
    ptr_world_sheet_.reset();
    opt_world_sprite_.reset();

    ptr_world_buffer_ = std::make_unique<FrameBuffer>(render_, int2_world_buffer_size, 0.0f, 0.0f, 0.0f, 0.0f);

    // Pixel art scaling mode is a customized bilinear sampling
    // (Renderer.cs L225-226)
    ptr_world_buffer_->GetTexture().SetScaleFilter(TextureScaleFilter::Linear);
    ptr_world_sheet_ = std::make_unique<Sheet>(SheetType::BGRA, ptr_world_buffer_->GetTexture());

    // Invalidate cached state to force a shader update (Renderer.cs L229-231)
    rect_last_world_viewport_ = Rectangle::Empty();
    opt_world_sprite_.reset();
  }

  int2_last_maximum_viewport_size_ = int2_size;
}

void Renderer::BeginWorld(core::Vector2 vec_viewport_location, int2 int2_viewport_size) {
  if (kind_render_type_ != RenderType::None)
    throw std::runtime_error(std::format("BeginWorld called with renderType = {}, expected RenderType.None.",
                                         static_cast<std::int32_t>(kind_render_type_)));

  BeginFrame();

  if (ptr_world_sheet_ == nullptr)
    throw std::runtime_error("BeginWorld called before SetMaximumViewportSize has been set.");

  const int2 int2_center_location = FromVector(vec_viewport_location);
  if (!opt_world_sprite_.has_value() || !(int2_viewport_size == int2_last_world_viewport_size_) ||
      !(vec_viewport_location == vec_last_viewport_location_)) {
    vec_last_viewport_location_ = vec_viewport_location;
    int2_last_world_viewport_size_ = int2_viewport_size;

    const WorldSpriteParams params = ComputeWorldSpriteParams(
        ptr_world_sheet_->Size(), int2_viewport_size, vec_viewport_location, int2_center_location,
        static_cast<float>(SurfaceSize().X));
    int4_world_downscale_factor_ = params.int4_downscale;

    opt_world_sprite_ = Sprite{*ptr_world_sheet_,
                               Rectangle{0, 0, params.int2_size_sub.X, params.int2_size_sub.Y},
                               0.0f,
                               core::Vector3{params.vec_fractional_offset.X, params.vec_fractional_offset.Y, 0},
                               TextureChannel::RGBA,
                               BlendMode::Alpha,
                               1.0f};
  }

  ptr_world_buffer_->Bind();
  const Rectangle rect_viewport{int2_center_location.X, int2_center_location.Y, int2_viewport_size.X,
                                int2_viewport_size.Y};
  if (!(rect_last_world_viewport_ == rect_viewport)) {
    const int2 int2_top_left = int2{int2_center_location.X - int2_viewport_size.X / 2,
                                    int2_center_location.Y - int2_viewport_size.Y / 2};
    WorldSpriteRenderer().SetViewportParams(ptr_world_sheet_->Size(), int4_world_downscale_factor_,
                                            float_depth_margin_, int2_top_left);
    rect_last_world_viewport_ = rect_viewport;
  }

  kind_render_type_ = RenderType::World;
}

void Renderer::BeginUI() {
  if (kind_render_type_ == RenderType::World) {
    // Complete world rendering(Renderer.cs L290-293)
    SetCurrentBatchRenderer(nullptr);
    ptr_world_buffer_->Unbind();

    // OPT-B1:上游此处绑定 screen buffer 并把 worldSprite 合成进去,UI 亦入
    // screen buffer,EndFrame 再整级拷贝回默认帧缓冲;单级合成把 worldSprite
    // 直接画进默认帧缓冲(worldBuffer.Unbind 已恢复表面 viewport),UI 随后
    // 同缓冲绘制 —— 每帧少一次全屏 clear 与两次全屏拷贝,p1/p2 投影的 NDC
    // 净映射不变(等价论证见文件头)。
    // OPT-B1: upstream bound the screen buffer here, composited worldSprite
    // into it, drew the UI into it too, and EndFrame copied the whole stage
    // back to the default framebuffer; single-pass draws worldSprite
    // straight into the default framebuffer (worldBuffer.Unbind already
    // restored the surface viewport) with the UI following in the same
    // buffer — one fewer fullscreen clear and two fewer fullscreen copies
    // per frame, with p1/p2's net NDC mapping unchanged (the equivalence
    // argument is in the file header).
    const float float_scale = EffectiveWindowScale();
    const int2 int2_surface_size = SurfaceSize();

    // We added 1 to worldSprite now we need to subtract. (Renderer.cs
    // L300-304)
    const core::Vector3 vec_buffer_scale{
        static_cast<float>(static_cast<std::int32_t>(static_cast<float>(int2_surface_size.X) / float_scale)) /
            (opt_world_sprite_->vec_size.X - 1),
        static_cast<float>(static_cast<std::int32_t>(static_cast<float>(int2_surface_size.Y) / float_scale)) /
            (opt_world_sprite_->vec_size.Y - 1),
        1.0f};

    UISpriteRenderer().EnablePixelArtScaling(true);
    UIRgbaSpriteRenderer().DrawSprite(*opt_world_sprite_, core::Vector3{}, vec_buffer_scale);
    SetCurrentBatchRenderer(nullptr);
    UISpriteRenderer().EnablePixelArtScaling(false);
  } else {
    // World rendering was skipped(Renderer.cs L311-316);单级下无 screen
    // buffer:直接清默认帧缓冲(上游经 screenBuffer.Bind 的 clear 完成同效)
    // 并确保表面 viewport。
    // World rendering was skipped (Renderer.cs L311-316); single-pass has no
    // screen buffer: clear the default framebuffer directly (upstream did
    // the equivalent via screenBuffer.Bind's clear) and pin the surface
    // viewport.
    BeginFrame();
    if (GfxCmd* ptr_cmd = render_.queue().Reserve(GfxCmdKind::BindFramebuffer, 0); ptr_cmd != nullptr) {
      ptr_cmd->uint4_a = 0;
      render_.queue().CommitBare();
    }
    const int2 int2_surface = SurfaceSize();
    if (GfxCmd* ptr_cmd = render_.queue().Reserve(GfxCmdKind::SetViewport, 0); ptr_cmd != nullptr) {
      ptr_cmd->uint4_c = static_cast<std::uint32_t>(int2_surface.X);
      ptr_cmd->uint4_d = static_cast<std::uint32_t>(int2_surface.Y);
      render_.queue().CommitBare();
    }
    if (GfxCmd* ptr_cmd = render_.queue().Reserve(GfxCmdKind::Clear, 0); ptr_cmd != nullptr) {
      ptr_cmd->float_d = 1.0f;  // 上游 screenBuffer 的 clear 色 FromArgb(0xFF,0,0,0)| upstream's clear color
      ptr_cmd->b_depth = 1;
      render_.queue().CommitBare();
    }
  }

  kind_render_type_ = RenderType::UI;
}

void Renderer::EndFrame() {
  if (kind_render_type_ != RenderType::UI)
    throw std::runtime_error(std::format("EndFrame called with renderType = {}, expected RenderType.UI.",
                                         static_cast<std::int32_t>(kind_render_type_)));

  // OPT-B1:上游把 screenSprite 再拷贝回默认帧缓冲(Renderer.cs L349-354);
  // 单级下默认帧缓冲已是最终画面,直接呈现。
  // OPT-B1: upstream copied screenSprite back to the default framebuffer
  // (Renderer.cs L349-354); single-pass presents the default framebuffer —
  // already the final image — directly.
  SetCurrentBatchRenderer(nullptr);

  Present();
  kind_render_type_ = RenderType::None;
}

void Renderer::Present() {
  // 交换必须在 GL 上下文线程(OPT-A5 统一线程模型)→ Present 命令。
  // The swap must run on the GL-context thread (the OPT-A5 unified model) →
  // a Present command.
  if (GfxCmd* ptr_cmd = render_.queue().Reserve(GfxCmdKind::Present, 0); ptr_cmd != nullptr)
    render_.queue().CommitBare();
}

void Renderer::SetPalette(HardwarePalette& palette) {
  // Note: palette.Texture and palette.ColorShifts are updated at the same
  // time so we only need to check one of the two to know whether we must
  // update the textures also compare heights in case new palettes have been
  // added (Renderer.cs L323-327)
  if (palette.TextureOrNull() == ptr_current_palette_texture_ && palette.Height() == int4_current_palette_height_)
    return;

  SetCurrentBatchRenderer(nullptr);
  ptr_current_palette_texture_ = palette.TextureOrNull();
  int4_current_palette_height_ = palette.Height();

  UISpriteRenderer().SetPalette(palette);
  WorldSpriteRenderer().SetPalette(palette);
}

void Renderer::Flush() { SetCurrentBatchRenderer(nullptr); }

void Renderer::EnableScissor(Rectangle rect_region) {
  // Must remain inside the current scissor rect(Renderer.cs L426-428)
  if (!vec_scissor_stack_.empty())
    rect_region = Rectangle::Intersect(rect_region, vec_scissor_stack_.back());

  SetCurrentBatchRenderer(nullptr);

  if (kind_render_type_ == RenderType::World) {
    // 上游 World 分支:按降采样因子换算进 world FBO(Renderer.cs L432-440)。
    // Upstream's World branch: convert by the downscale factor into the
    // world FBO (Renderer.cs L432-440).
    const Rectangle rect_converted = Rectangle::FromLTRB(
        rect_region.X / int4_world_downscale_factor_, rect_region.Top() / int4_world_downscale_factor_,
        (rect_region.Right() + int4_world_downscale_factor_ - 1) / int4_world_downscale_factor_,
        (rect_region.Bottom() + int4_world_downscale_factor_ - 1) / int4_world_downscale_factor_);
    ptr_world_buffer_->EnableScissor(rect_converted);
  } else {
    // HiDPI 换算(Sdl2GraphicsContext.cs L106-132):窗口逻辑尺寸 ≠ 表面尺寸
    // 时四值 × scale 取整;负宽高 clamp 到 0。
    // The HiDPI conversion (Sdl2GraphicsContext.cs L106-132): when the
    // logical window size differs from the surface size, scale the four
    // values with rounding; negative widths/heights clamp to 0.
    const auto geom = window_.Geom();
    const int2 int2_window_size{geom.int4_width, geom.int4_height};
    const int2 int2_surface_size = SurfaceSize();
    std::int32_t int4_x = rect_region.X;
    std::int32_t int4_y = rect_region.Y;
    std::int32_t int4_w = rect_region.Width;
    std::int32_t int4_h = rect_region.Height;
    if (int4_w < 0)
      int4_w = 0;
    if (int4_h < 0)
      int4_h = 0;
    if (!(int2_window_size == int2_surface_size)) {
      const float float_scale = geom.float_scale;
      int4_x = static_cast<std::int32_t>(std::lround(float_scale * static_cast<float>(int4_x)));
      int4_y = static_cast<std::int32_t>(std::lround(float_scale * static_cast<float>(int4_y)));
      int4_w = static_cast<std::int32_t>(std::lround(float_scale * static_cast<float>(int4_w)));
      int4_h = static_cast<std::int32_t>(std::lround(float_scale * static_cast<float>(int4_h)));
    }
    if (GfxCmd* ptr_cmd = render_.queue().Reserve(GfxCmdKind::SetScissor, 0); ptr_cmd != nullptr) {
      ptr_cmd->uint4_a = static_cast<std::uint32_t>(int4_x);
      ptr_cmd->uint4_b = static_cast<std::uint32_t>(int4_y);
      ptr_cmd->uint4_c = static_cast<std::uint32_t>(int4_w);
      ptr_cmd->uint4_d = static_cast<std::uint32_t>(int4_h);
      render_.queue().CommitBare();
    }
  }

  vec_scissor_stack_.push_back(rect_region);
}

void Renderer::DisableScissor() {
  vec_scissor_stack_.pop_back();
  SetCurrentBatchRenderer(nullptr);

  if (kind_render_type_ == RenderType::World) {
    // Restore previous scissor rect(Renderer.cs L452-466)
    if (!vec_scissor_stack_.empty()) {
      const Rectangle rect_region = vec_scissor_stack_.back();
      const Rectangle rect_converted = Rectangle::FromLTRB(
          rect_region.X / int4_world_downscale_factor_, rect_region.Top() / int4_world_downscale_factor_,
          (rect_region.Right() + int4_world_downscale_factor_ - 1) / int4_world_downscale_factor_,
          (rect_region.Bottom() + int4_world_downscale_factor_ - 1) / int4_world_downscale_factor_);
      ptr_world_buffer_->EnableScissor(rect_converted);
    } else {
      ptr_world_buffer_->DisableScissor();
    }
  } else {
    if (!vec_scissor_stack_.empty()) {
      const Rectangle rect_region = vec_scissor_stack_.back();
      const auto geom = window_.Geom();
      const int2 int2_window_size{geom.int4_width, geom.int4_height};
      const int2 int2_surface_size = SurfaceSize();
      std::int32_t int4_x = rect_region.X;
      std::int32_t int4_y = rect_region.Y;
      std::int32_t int4_w = rect_region.Width;
      std::int32_t int4_h = rect_region.Height;
      if (int4_w < 0)
        int4_w = 0;
      if (int4_h < 0)
        int4_h = 0;
      if (!(int2_window_size == int2_surface_size)) {
        const float float_scale = geom.float_scale;
        int4_x = static_cast<std::int32_t>(std::lround(float_scale * static_cast<float>(int4_x)));
        int4_y = static_cast<std::int32_t>(std::lround(float_scale * static_cast<float>(int4_y)));
        int4_w = static_cast<std::int32_t>(std::lround(float_scale * static_cast<float>(int4_w)));
        int4_h = static_cast<std::int32_t>(std::lround(float_scale * static_cast<float>(int4_h)));
      }
      if (GfxCmd* ptr_cmd = render_.queue().Reserve(GfxCmdKind::SetScissor, 0); ptr_cmd != nullptr) {
        ptr_cmd->uint4_a = static_cast<std::uint32_t>(int4_x);
        ptr_cmd->uint4_b = static_cast<std::uint32_t>(int4_y);
        ptr_cmd->uint4_c = static_cast<std::uint32_t>(int4_w);
        ptr_cmd->uint4_d = static_cast<std::uint32_t>(int4_h);
        render_.queue().CommitBare();
      }
    } else {
      if (GfxCmd* ptr_cmd = render_.queue().Reserve(GfxCmdKind::DisableScissor, 0); ptr_cmd != nullptr)
        render_.queue().CommitBare();
    }
  }
}

void Renderer::EnableDepthBuffer() {
  SetCurrentBatchRenderer(nullptr);
  if (GfxCmd* ptr_cmd = render_.queue().Reserve(GfxCmdKind::EnableDepthTest, 0); ptr_cmd != nullptr)
    render_.queue().CommitBare();
}

void Renderer::DisableDepthBuffer() {
  SetCurrentBatchRenderer(nullptr);
  if (GfxCmd* ptr_cmd = render_.queue().Reserve(GfxCmdKind::DisableDepthTest, 0); ptr_cmd != nullptr)
    render_.queue().CommitBare();
}

void Renderer::ClearDepthBuffer() {
  SetCurrentBatchRenderer(nullptr);
  if (GfxCmd* ptr_cmd = render_.queue().Reserve(GfxCmdKind::ClearDepth, 0); ptr_cmd != nullptr)
    render_.queue().CommitBare();
}

void Renderer::EnableAntialiasingFilter() {
  if (kind_render_type_ != RenderType::UI)
    throw std::runtime_error(std::format("EndFrame called with renderType = {}, expected RenderType.UI.",
                                         static_cast<std::int32_t>(kind_render_type_)));

  SetCurrentBatchRenderer(nullptr);
  UISpriteRenderer().EnablePixelArtScaling(true);
}

void Renderer::DisableAntialiasingFilter() {
  if (kind_render_type_ != RenderType::UI)
    throw std::runtime_error(std::format("EndFrame called with renderType = {}, expected RenderType.UI.",
                                         static_cast<std::int32_t>(kind_render_type_)));

  SetCurrentBatchRenderer(nullptr);
  UISpriteRenderer().EnablePixelArtScaling(false);
}

}  // namespace ora::gfx
