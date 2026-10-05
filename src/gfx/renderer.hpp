// UPSTREAM: OpenRA.Game/Renderer.cs @7d57605 L24-585(逐方法)
// 帧编排:BeginWorld(world FBO 渲染)→ BeginUI(合成 + UI)→ EndFrame(Present)。
// OPT-B1(docs/OPTIMIZATION_TRACKER.md)单级合成 —— 上游三级 pow2 链
// (world FBO → screen FBO → 默认帧缓冲;1920×1080 窗口被迫 2048² 双 FBO,
// 每帧 3 次全屏 clear + 2 次全屏拷贝)改为 NPOT + 单级:
//   - world FBO 按视口精确尺寸(OPT-B1 去 pow2,D37 延续);
//   - screen FBO 整级删除:BeginUI 把 worldSprite 直接画进默认帧缓冲
//     (像素风放大保留),UI 随后同缓冲绘制;
//   - BeginFrame 对默认帧缓冲的 Context.Clear() 删除(上游注释自认冗余 ——
//     EndFrame 的全屏 quad 覆盖之;单级下由 BeginUI 的全屏 blit 覆盖);
//     无 world 的帧(UI-only)在 BeginUI 的 else 分支显式清一次;
//   - EndFrame 的最终全屏拷贝随之消失(默认帧缓冲已是最终画面)。
// **NDC 数学等价论证**(渲染对拍关卡的先验论证):上游 UI 顶点坐标 c ∈
// [0, bufferSize(pow2/scale)],p1 = 2/bufferSize → NDC = 2c/bufferSize - 1;
// 随后 screenSprite 把 pow2 缓冲的 [0,surface] 子矩形缩放到窗口,净效果
// NDC = 2c/surface - 1。单级下 bufferSize = surface(NPOT)/scale,同一净映射,
// 像素落点相同(HiDPI scale 两版同除)。
// 形态适配:
//   - 着色器源码构造注入(上游 ShaderBindings.GetShaderCode 读 EngineDir/
//     glsl;引擎侧文件系统随 Game 批次接线,测试直接传仓库内 glsl/ 源码);
//   - Fonts/InitializeFonts/SaveScreenshot/GetRenderBufferSnapshot 随字体与
//     Png 批次(formats/);WorldRenderers(IRenderer 后处理族)随后处理批次;
//   - window/scale 查询经 Sdl2Window 几何快照(OPT-B5);
//   - GraphicSettings 的窗口模式解析(GetResolution)由调用方决定窗口尺寸,
//     本类只接窗口与批次参数。
// The frame orchestration: BeginWorld (render into the world FBO) → BeginUI
// (composite + UI) → EndFrame (Present). OPT-B1
// (docs/OPTIMIZATION_TRACKER.md) single-pass compositing — upstream's
// three-stage pow2 chain (world FBO → screen FBO → default framebuffer; a
// 1920×1080 window forced dual 2048² FBOs with three fullscreen clears and
// two fullscreen copies per frame) becomes NPOT + single-pass:
//   - the world FBO at the exact viewport size (OPT-B1 drops pow2, extending
//     D37);
//   - the screen FBO stage deleted outright: BeginUI draws worldSprite
//     straight into the default framebuffer (pixel-art scaling preserved),
//     with the UI drawn into the same buffer afterwards;
//   - BeginFrame's Context.Clear() of the default framebuffer is deleted
//     (upstream's own comment calls it redundant — the EndFrame fullscreen
//     quad overwrites it; under single-pass, BeginUI's fullscreen blit does)
//     with UI-only frames cleared once explicitly in BeginUI's else branch;
//   - EndFrame's final fullscreen copy disappears with it (the default
//     framebuffer already holds the final image).
// **NDC-math equivalence argument** (the a-priori argument for the
// render-differential gate): upstream UI vertex coordinates c ∈ [0,
// bufferSize (pow2/scale)] with p1 = 2/bufferSize → NDC = 2c/bufferSize - 1;
// the screenSprite then maps the pow2 buffer's [0,surface] sub-rectangle onto
// the window, netting NDC = 2c/surface - 1. Single-pass has bufferSize =
// surface (NPOT)/scale — the same net mapping, identical pixel landing spots
// (HiDPI scale divides identically in both). Shape adaptations: shader
// sources injected at construction (upstream's
// ShaderBindings.GetShaderCode reads EngineDir/glsl; the engine-side
// filesystem wires up with the Game batch — tests pass the repo's glsl/
// sources directly); Fonts/InitializeFonts/SaveScreenshot/
// GetRenderBufferSnapshot await the font and Png batches (formats/);
// WorldRenderers (the IRenderer post-process family) await the
// post-process batch; window/scale queries go through the Sdl2Window
// geometry snapshot (OPT-B5); GraphicSettings' window-mode resolution
// (GetResolution) is the caller's business — this class takes the window
// and batch parameters only.
#pragma once
import std;

#include "core/int2.hpp"
#include "core/rectangle.hpp"
#include "core/vector_n.hpp"
#include "gfx/frame_buffer.hpp"
#include "gfx/hardware_palette.hpp"
#include "gfx/sheet.hpp"
#include "gfx/shader.hpp"
#include "gfx/sprite_renderer.hpp"
#include "gfx/vertex_buffer.hpp"
#include "platform/sdl2_window.hpp"

namespace ora::gfx {

/// worldSprite 几何参数(BeginWorld 内联计算;提取为纯函数以便单测)。
/// The worldSprite geometry parameters (computed inline in BeginWorld
/// upstream; extracted as a pure function for unit testing).
struct WorldSpriteParams {
  std::int32_t int4_downscale;      // 世界渲染降采样因子 | the world render downscale factor
  int2 int2_size_sub;               // world FBO 内的子矩形尺寸(s)| the sub-rectangle size (s) in the world FBO
  core::Vector2 vec_fractional_offset;
};

/// BeginWorld 的 worldSprite 计算(Renderer.cs L248-273 逐行;OPT-B1 后
/// screen 宽 = 表面宽,不再 pow2)。
/// The worldSprite computation of BeginWorld (Renderer.cs L248-273 line by
/// line; after OPT-B1 the screen width = the surface width, no longer pow2).
WorldSpriteParams ComputeWorldSpriteParams(int2 int2_world_sheet_size, int2 int2_viewport_size,
                                           core::Vector2 vec_viewport_location, int2 int2_center_location,
                                           float float_screen_width);

/// 单级合成渲染器(Renderer.cs;不可拷贝移动)。
/// The single-pass composite renderer (Renderer.cs; non-copyable and
/// non-movable).
class Renderer {
 public:
  /// vertex_batch_size 上游默认 8192(Manifest.cs L47);着色器源码调用方注入。
  /// The upstream default vertex_batch_size is 8192 (Manifest.cs L47); shader
  /// sources are injected by the caller.
  struct Desc {
    std::int32_t int4_vertex_batch_size = 8192;
    std::string_view str_combined_vert;
    std::string_view str_combined_frag;
  };

  Renderer(platform::Sdl2Window& window, RenderThread& render, const Desc& desc);
  ~Renderer();

  Renderer(const Renderer&) = delete;
  Renderer& operator=(const Renderer&) = delete;

  // —— 渲染器族(Renderer.cs L28-34)——
  // —— The renderer family (Renderer.cs L28-34) ——
  SpriteRenderer& WorldSpriteRenderer() { return *ptr_world_sprite_renderer_; }
  RgbaSpriteRenderer& WorldRgbaSpriteRenderer() { return *ptr_world_rgba_sprite_renderer_; }
  RgbaColorRenderer& WorldRgbaColorRenderer() { return *ptr_world_rgba_color_renderer_; }
  SpriteRenderer& UISpriteRenderer() { return *ptr_ui_sprite_renderer_; }
  RgbaSpriteRenderer& UIRgbaSpriteRenderer() { return *ptr_ui_rgba_sprite_renderer_; }
  RgbaColorRenderer& UIRgbaColorRenderer() { return *ptr_ui_rgba_color_renderer_; }

  void SetDepthMargin(float float_depth_margin) { float_depth_margin_ = float_depth_margin; }

  /// 最大视口尺寸(Renderer.cs L201-235;OPT-B1 去 NextPowerOf2)。
  /// The maximum viewport size (Renderer.cs L201-235; OPT-B1 drops
  /// NextPowerOf2).
  void SetMaximumViewportSize(int2 int2_size);

  void BeginWorld(core::Vector2 vec_viewport_location, int2 int2_viewport_size);
  void BeginUI();
  void EndFrame();

  /// 调色板纹理绑定缓存(Renderer.cs L321-338)。
  /// The palette-texture binding cache (Renderer.cs L321-338).
  void SetPalette(HardwarePalette& palette);

  /// CurrentBatchRenderer = null(Renderer.cs L382-385)。
  /// CurrentBatchRenderer = null (Renderer.cs L382-385).
  void Flush();

  /// 当前批(Renderer.cs L396-407;切批 flush 前一批)。
  /// The current batch (Renderer.cs L396-407; switching flushes the previous).
  IBatchRenderer* CurrentBatchRenderer() const { return ptr_current_batch_; }
  void SetCurrentBatchRenderer(IBatchRenderer* ptr_batch) {
    if (ptr_current_batch_ == ptr_batch)
      return;
    if (ptr_current_batch_ != nullptr)
      ptr_current_batch_->Flush();
    ptr_current_batch_ = ptr_batch;
  }

  /// 裁剪栈(Renderer.cs L424-479;World 时按降采样因子换算进 world FBO,
  /// 否则按 HiDPI scale 换算进当前缓冲)。
  /// The scissor stack (Renderer.cs L424-479; World converts by the
  /// downscale factor into the world FBO, else by the HiDPI scale into the
  /// current buffer).
  void EnableScissor(Rectangle rect_region);
  void DisableScissor();

  void EnableDepthBuffer();
  void DisableDepthBuffer();
  void ClearDepthBuffer();

  /// 抗锯齿(像素风放大)开关(Renderer.cs L499-515)。
  /// The antialiasing (pixel-art scaling) toggles (Renderer.cs L499-515).
  void EnableAntialiasingFilter();
  void DisableAntialiasingFilter();

  int2 WorldFrameBufferSize() const { return ptr_world_sheet_ != nullptr ? ptr_world_sheet_->Size() : int2{}; }
  std::int32_t WorldDownscaleFactor() const { return int4_world_downscale_factor_; }

  platform::Sdl2Window& window() { return window_; }
  RenderThread& render() { return render_; }

  /// 默认帧缓冲的呈现(EndFrame 内部;渲染线程交换)。
  /// Presents the default framebuffer (inside EndFrame; swapped on the
  /// render thread).
  void Present();

 private:
  /// BeginFrame(Renderer.cs L164-199;OPT-B1 后仅剩 UI 投影参数更新 ——
  /// 默认 FB 的 Clear 与 screen buffer/sprite 重建全数消失)。
  /// BeginFrame (Renderer.cs L164-199; after OPT-B1 only the UI projection
  /// update remains — the default-FB Clear and the screen buffer/sprite
  /// rebuilds are all gone).
  void BeginFrame();

  /// 表面尺寸(drawable 像素;Sdl2PlatformWindow.SurfaceSize)。
  /// The surface size (drawable pixels; Sdl2PlatformWindow.SurfaceSize).
  int2 SurfaceSize() const {
    const auto geom = window_.Geom();
    return int2{static_cast<std::int32_t>(geom.int4_width * geom.float_scale),
                static_cast<std::int32_t>(geom.int4_height * geom.float_scale)};
  }

  float EffectiveWindowScale() const { return window_.Geom().float_scale; }

  enum class RenderType { None, World, UI };

  platform::Sdl2Window& window_;
  RenderThread& render_;

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

  std::vector<Rectangle> vec_scissor_stack_;
  IBatchRenderer* ptr_current_batch_ = nullptr;
  RenderType kind_render_type_ = RenderType::None;

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

}  // namespace ora::gfx
