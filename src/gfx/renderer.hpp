#pragma once
import std;

#include "core/int2.hpp"
#include "core/rectangle.hpp"
#include "core/vector_n.hpp"
#include "gfx/frame_buffer.hpp"
#include "gfx/hardware_palette.hpp"
#include "gfx/sheet.hpp"
#include "gfx/shader.hpp"
#include "gfx/sprite_font.hpp"
#include "gfx/sprite_renderer.hpp"
#include "gfx/vertex_buffer.hpp"
#include "platform/sdl2_input.hpp"
#include "platform/sdl2_window.hpp"

namespace ora::gfx {

struct WorldSpriteParams {
  std::int32_t int4_downscale;
  int2 int2_size_sub;
  core::Vector2 vec_fractional_offset;
};

WorldSpriteParams ComputeWorldSpriteParams(int2 int2_world_sheet_size, int2 int2_viewport_size,
                                           core::Vector2 vec_viewport_location, int2 int2_center_location,
                                           float float_screen_width);

class Renderer {
 public:

  struct Desc {
    std::int32_t int4_vertex_batch_size = 8192;
    std::string_view str_combined_vert;
    std::string_view str_combined_frag;
  };

  Renderer(platform::Sdl2Window& window, RenderThread& render, const Desc& desc);
  ~Renderer();

  Renderer(const Renderer&) = delete;
  Renderer& operator=(const Renderer&) = delete;

  SpriteRenderer& WorldSpriteRenderer() { return *ptr_world_sprite_renderer_; }
  RgbaSpriteRenderer& WorldRgbaSpriteRenderer() { return *ptr_world_rgba_sprite_renderer_; }
  RgbaColorRenderer& WorldRgbaColorRenderer() { return *ptr_world_rgba_color_renderer_; }
  SpriteRenderer& UISpriteRenderer() { return *ptr_ui_sprite_renderer_; }
  RgbaSpriteRenderer& UIRgbaSpriteRenderer() { return *ptr_ui_rgba_sprite_renderer_; }
  RgbaColorRenderer& UIRgbaColorRenderer() { return *ptr_ui_rgba_color_renderer_; }

  void SetDepthMargin(float float_depth_margin) { float_depth_margin_ = float_depth_margin; }

  void SetMaximumViewportSize(int2 int2_size);

  void BeginWorld(core::Vector2 vec_viewport_location, int2 int2_viewport_size);
  void BeginUI();
  void EndFrame(IInputHandler* ptr_input_handler = nullptr);

  struct FontDesc {
    std::string str_key;
    std::string str_file;
    int int4_size = 0;
    int int4_ascender = 0;
  };

  using FontOpenFn =
      std::function<std::vector<std::uint8_t>(const std::string&)>;
  using CreateFontFn =
      std::function<std::unique_ptr<platform::IFont>(std::span<const std::uint8_t>)>;

  void InitializeFonts(std::span<const FontDesc> vec_fonts, std::int32_t int4_font_sheet_size,
                       float fp4_window_scale, const FontOpenFn& fn_open,
                       const CreateFontFn& fn_create_font);

  SpriteFont& Font(const std::string& str_key);

  SpriteFont* TryGetFont(const std::string& str_key);

  void SetFontsWindowScale(float fp4_scale);

  void SetPalette(HardwarePalette& palette);

  void Flush();

  IBatchRenderer* CurrentBatchRenderer() const { return ptr_current_batch_; }
  void SetCurrentBatchRenderer(IBatchRenderer* ptr_batch) {
    if (ptr_current_batch_ == ptr_batch)
      return;
    if (ptr_current_batch_ != nullptr)
      ptr_current_batch_->Flush();
    ptr_current_batch_ = ptr_batch;
  }

  void EnableScissor(Rectangle rect_region);
  void DisableScissor();

  void EnableDepthBuffer();
  void DisableDepthBuffer();
  void ClearDepthBuffer();

  void EnableAntialiasingFilter();
  void DisableAntialiasingFilter();

  int2 WorldFrameBufferSize() const { return ptr_world_sheet_ != nullptr ? ptr_world_sheet_->Size() : int2{}; }
  std::int32_t WorldDownscaleFactor() const { return int4_world_downscale_factor_; }

  platform::Sdl2Window& window() { return window_; }
  RenderThread& render() { return render_; }

  Sdl2Input& input() { return input_; }
  bool InputQuitRequested() const { return b_input_quit_requested_; }
  void ClearInputQuitRequested() { b_input_quit_requested_ = false; }

  bool SetClipboardText(const std::string& str_text) { return window_.SetClipboardText(str_text); }
  std::string GetClipboardText() const { return window_.GetClipboardText(); }

  void Present();

  float EffectiveWindowScale() const { return window_.Geom().float_scale; }

  float NativeWindowScale() const { return window_.Geom().float_scale; }

  int2 Resolution() const { return window_.EffectiveWindowSize(); }
  int2 NativeResolution() const { return window_.NativeWindowSize(); }
  int DisplayCount() const { return window_.DisplayCount(); }
  int CurrentDisplay() const { return window_.CurrentDisplay(); }
  const std::vector<platform::GLProfileKind>& SupportedGLProfiles() const {
    return vec_supported_profiles_;
  }
  void SetUIScale(float fp4_scale) { window_.SetScaleModifier(fp4_scale); }
  void SetVSyncEnabled(bool b_enabled) { window_.SetVSyncEnabled(b_enabled); }
  void GrabWindowMouseFocus() { window_.GrabWindowMouseFocus(); }
  void ReleaseWindowMouseFocus() { window_.ReleaseWindowMouseFocus(); }
  bool TryOpenUrl(std::string_view str_url) const {
    return window_.TryOpenUrl(str_url);
  }

 private:
  void BeginFrame();

  int2 SurfaceSize() const {
    const auto geom = window_.Geom();
    return int2{static_cast<std::int32_t>(geom.int4_width * geom.float_scale),
                static_cast<std::int32_t>(geom.int4_height * geom.float_scale)};
  }

  enum class RenderType { None, World, UI };

  platform::Sdl2Window& window_;
  RenderThread& render_;
  Sdl2Input input_{window_};
  bool b_input_quit_requested_ = false;

  std::optional<VertexBuffer> opt_vertex_buffer_;
  std::optional<IndexBuffer> opt_index_buffer_;
  std::unique_ptr<SpriteRenderer> ptr_world_sprite_renderer_;
  std::unique_ptr<SpriteRenderer> ptr_ui_sprite_renderer_;
  std::unique_ptr<RgbaSpriteRenderer> ptr_world_rgba_sprite_renderer_;
  std::unique_ptr<RgbaSpriteRenderer> ptr_ui_rgba_sprite_renderer_;
  std::unique_ptr<RgbaColorRenderer> ptr_world_rgba_color_renderer_;
  std::unique_ptr<RgbaColorRenderer> ptr_ui_rgba_color_renderer_;

  std::unique_ptr<FrameBuffer> ptr_world_buffer_;
  std::unique_ptr<Sheet> ptr_world_sheet_;
  std::optional<Sprite> opt_world_sprite_;

  std::unique_ptr<SheetBuilder> ptr_font_sheet_builder_;
  std::vector<std::pair<std::string, std::unique_ptr<SpriteFont>>> vec_fonts_;

  std::vector<Rectangle> vec_scissor_stack_;
  IBatchRenderer* ptr_current_batch_ = nullptr;
  RenderType kind_render_type_ = RenderType::None;
  std::vector<platform::GLProfileKind> vec_supported_profiles_;

  float float_depth_margin_ = 0.0f;
  int2 int2_last_maximum_viewport_size_{};
  int2 int2_last_world_viewport_size_{};
  core::Vector2 vec_last_viewport_location_{};
  Rectangle rect_last_world_viewport_ = Rectangle::Empty();
  int2 int2_last_buffer_size_{-1, -1};
  std::int32_t int4_world_downscale_factor_ = 1;

  const Texture* ptr_current_palette_texture_ = nullptr;
  std::int32_t int4_current_palette_height_ = 0;
};

}
