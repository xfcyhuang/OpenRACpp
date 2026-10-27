#include "gfx/renderer.hpp"

namespace ora::gfx {

namespace {

int2 FromVector(core::Vector2 v) {
  return int2{static_cast<std::int32_t>(v.X), static_cast<std::int32_t>(v.Y)};
}

bool IsInteger(float float_v) { return std::trunc(float_v) == float_v; }

core::Vector2 RoundVector(core::Vector2 v) {
  return core::Vector2{std::nearbyint(v.X), std::nearbyint(v.Y)};
}

}

WorldSpriteParams ComputeWorldSpriteParams(int2 int2_world_sheet_size, int2 int2_viewport_size,
                                           core::Vector2 vec_viewport_location, int2 int2_center_location,
                                           float float_screen_width) {

  const std::int32_t int4_viewport_width = int2_viewport_size.X;
  const std::int32_t int4_viewport_height = int2_viewport_size.Y;
  const std::int32_t int4_buffer_width = int2_world_sheet_size.X;
  const std::int32_t int4_buffer_height = int2_world_sheet_size.Y;
  std::int32_t int4_downscale = 1;
  while (int4_viewport_width / int4_downscale > int4_buffer_width ||
         int4_viewport_height / int4_downscale > int4_buffer_height)
    int4_downscale++;

  const int2 int2_size_sub{int4_viewport_width / int4_downscale + 1,
                           int4_viewport_height / int4_downscale + 1};
  core::Vector2 vec_fractional_offset =
      core::Vector2{static_cast<float>(int2_center_location.X), static_cast<float>(int2_center_location.Y)} -
      vec_viewport_location;

  const float float_render_scale = float_screen_width / static_cast<float>(int2_size_sub.X - 1);
  if (IsInteger(float_render_scale))
    vec_fractional_offset = RoundVector(vec_fractional_offset * float_render_scale) * (1.0f / float_render_scale);

  return WorldSpriteParams{int4_downscale, int2_size_sub, vec_fractional_offset};
}

Renderer::Renderer(platform::Sdl2Window& window, RenderThread& render, const Desc& desc)
    : window_{window}, render_{render} {
  vec_supported_profiles_.push_back(window_.GLProfileUsed());

  const std::int32_t int4_temp_vertex_buffer_size =
      desc.int4_vertex_batch_size - desc.int4_vertex_batch_size % 4;
  const std::int32_t int4_temp_index_buffer_size = int4_temp_vertex_buffer_size / 4 * 6;

  const std::vector<ShaderVertexAttribute> vec_attributes = MakeCombinedAttributes();
  const std::int32_t int4_stride = static_cast<std::int32_t>(sizeof(Vertex));

  ShaderBindingsDesc desc_bindings{"combined", desc.str_combined_vert, "combined", desc.str_combined_frag,
                                   int4_stride, vec_attributes};

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

  SetCurrentBatchRenderer(nullptr);
}

void Renderer::BeginFrame() {

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

  int2 int2_world_buffer_size;
  if (float_depth_margin_ == 0.0f) {
    const int2 int2_surface_size = SurfaceSize();

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

    ptr_world_buffer_->GetTexture().SetScaleFilter(TextureScaleFilter::Linear);
    ptr_world_sheet_ = std::make_unique<Sheet>(SheetType::BGRA, ptr_world_buffer_->GetTexture());

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

    SetCurrentBatchRenderer(nullptr);
    ptr_world_buffer_->Unbind();

    const float float_scale = EffectiveWindowScale();
    const int2 int2_surface_size = SurfaceSize();

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
      ptr_cmd->float_d = 1.0f;
      ptr_cmd->b_depth = 1;
      render_.queue().CommitBare();
    }
  }

  kind_render_type_ = RenderType::UI;
}

void Renderer::EndFrame(IInputHandler* ptr_input_handler) {
  if (kind_render_type_ != RenderType::UI)
    throw std::runtime_error(std::format("EndFrame called with renderType = {}, expected RenderType.UI.",
                                         static_cast<std::int32_t>(kind_render_type_)));

  SetCurrentBatchRenderer(nullptr);

  Present();
  kind_render_type_ = RenderType::None;

  if (ptr_input_handler != nullptr && input_.PumpInput(*ptr_input_handler))
    b_input_quit_requested_ = true;
}

void Renderer::Present() {

  if (GfxCmd* ptr_cmd = render_.queue().Reserve(GfxCmdKind::Present, 0); ptr_cmd != nullptr)
    render_.queue().CommitBare();
}

void Renderer::SetPalette(HardwarePalette& palette) {

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

  if (!vec_scissor_stack_.empty())
    rect_region = Rectangle::Intersect(rect_region, vec_scissor_stack_.back());

  SetCurrentBatchRenderer(nullptr);

  if (kind_render_type_ == RenderType::World) {

    const Rectangle rect_converted = Rectangle::FromLTRB(
        rect_region.X / int4_world_downscale_factor_, rect_region.Top() / int4_world_downscale_factor_,
        (rect_region.Right() + int4_world_downscale_factor_ - 1) / int4_world_downscale_factor_,
        (rect_region.Bottom() + int4_world_downscale_factor_ - 1) / int4_world_downscale_factor_);
    ptr_world_buffer_->EnableScissor(rect_converted);
  } else {

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

void Renderer::InitializeFonts(std::span<const FontDesc> vec_fonts,
                               std::int32_t int4_font_sheet_size, float fp4_window_scale,
                               const FontOpenFn& fn_open, const CreateFontFn& fn_create_font) {

  vec_fonts_.clear();
  ptr_font_sheet_builder_ = std::make_unique<SheetBuilder>(SheetType::BGRA, int4_font_sheet_size);

  vec_fonts_.reserve(vec_fonts.size());
  for (const FontDesc& desc_font : vec_fonts) {
    const std::vector<std::uint8_t> vec_data = fn_open(desc_font.str_file);
    RgbaSpriteRenderer& renderer_rgba = UIRgbaSpriteRenderer();
    auto ptr_font = std::make_unique<SpriteFont>(
        desc_font.str_file, vec_data, fn_create_font(vec_data), desc_font.int4_size,
        desc_font.int4_ascender, fp4_window_scale, *ptr_font_sheet_builder_,
        [&renderer_rgba](const Sprite& sprite_s, core::Vector3 v_location, float fp4_scale,
                         core::Vector3 v_tint, float fp4_alpha) {
          renderer_rgba.DrawSprite(sprite_s, v_location, fp4_scale, v_tint, fp4_alpha);
        },
        [&renderer_rgba](const Sprite& sprite_s, core::Vector3 v_a, core::Vector3 v_b,
                         core::Vector3 v_c, core::Vector3 v_d, core::Vector3 v_tint,
                         float fp4_alpha) {
          renderer_rgba.DrawSprite(sprite_s, v_a, v_b, v_c, v_d, v_tint, fp4_alpha);
        });
    vec_fonts_.emplace_back(desc_font.str_key, std::move(ptr_font));
  }
}

SpriteFont& Renderer::Font(const std::string& str_key) {
  SpriteFont* ptr_font = TryGetFont(str_key);
  if (ptr_font == nullptr)
    throw std::runtime_error("The given key was not present in the dictionary.");
  return *ptr_font;
}

SpriteFont* Renderer::TryGetFont(const std::string& str_key) {
  for (auto& [str_entry_key, ptr_entry_font] : vec_fonts_)
    if (str_entry_key == str_key)
      return ptr_entry_font.get();
  return nullptr;
}

void Renderer::SetFontsWindowScale(float fp4_scale) {
  for (auto& [str_entry_key, ptr_entry_font] : vec_fonts_)
    ptr_entry_font->SetScale(fp4_scale);
}

}
