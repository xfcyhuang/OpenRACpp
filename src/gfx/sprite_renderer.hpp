// UPSTREAM: OpenRA.Game/Graphics/SpriteRenderer.cs @b6fc03f L20-305(逐方法)+
//           OpenRA.Game/Graphics/RgbaSpriteRenderer.cs @b6fc03f L17-58 +
//           OpenRA.Game/Graphics/RgbaColorRenderer.cs @b6fc03f L20-256
// SpriteRenderer:8 纹理槽批渲染 + BlendSpan 交错混合段 + SetRenderStateForSprite
// 的槽位映射(含 SpriteWithSecondaryData 的双 sheet 逻辑与满槽 flush 重试),
// 逐行复刻;Flush 的绘制路径挂接第四批的持久映射 VB(OPT-A5:顶点一次
// memcpy 进命令 payload,渲染线程等槽 fence 后 memcpy 进映射区)+ per-program
// VAO(OPT-A6:绑定/属性一次固化)+ blend 状态机 diff(消费端)。
// OPT-A7:ResolveTextureIndex 的 PaletteReference::HasColorShift 每精灵字符串
// 字典查找,以 HardwarePalette 的色移 epoch 缓存消除(见 palette.hpp)。
// 形态适配:
//   - renderer.CurrentBatchRenderer(上游属性)→ 构造注入 Renderer 的批槽指针
//     (IBatchRenderer**),ActivateAsCurrent 复刻 setter 的"前批 Flush"语义;
//   - 上游 internal DrawSprite(int paletteTextureIndex, …)族 → C++ 公开
//     (上游 internal = 程序集内可见;引擎内 RgbaSpriteRenderer/调用方同权);
//   - BlendSpan 列表提出为 BlendSpanTracker、RgbaColorRenderer 的落点提出为
//     IRgbaQuadSink 注入面 —— 两者纯逻辑可测(上游私有直连);
//   - PerfHistory.Increment("batches") 不移植(诊断面,Phase 9 统计);
//   - 上游 vertices 数组 = renderer.Context.CreateVertices(size)(每渲染器一
//     份);C++ 侧同构(成员 vector 复用,不逐帧分配)。
// SpriteRenderer: the 8-texture-slot batch renderer with BlendSpan
// interleaved blending and SetRenderStateForSprite's slot mapping (including
// the SpriteWithSecondaryData dual-sheet logic and the full-slot flush
// retry), ported line by line; Flush's draw path wires onto the fourth
// batch's persistent-mapped VB (OPT-A5: vertices memcpy once into the command
// payload, the render thread awaits the slot fence then memcpys into the
// mapping) plus the per-program VAO (OPT-A6: bindings/attributes baked once)
// plus the consumer-side blend state diff. OPT-A7: ResolveTextureIndex's
// per-sprite string-dictionary HasColorShift lookup is eliminated via the
// HardwarePalette color-shift epoch cache (see palette.hpp). Shape
// adaptations: renderer.CurrentBatchRenderer (an upstream property) → the
// Renderer's batch-slot pointer injected at construction (IBatchRenderer**),
// with ActivateAsCurrent reproducing the setter's flush-the-previous-batch
// semantics; the upstream internal DrawSprite(int paletteTextureIndex, …)
// family → public in C++ (internal = assembly-visible; the in-engine callers
// share the same reach); the BlendSpan list is extracted into
// BlendSpanTracker and RgbaColorRenderer's sink into the injected
// IRgbaQuadSink — both pure-logic testable (upstream kept them private
// direct-wired); PerfHistory.Increment("batches") is not ported (a
// diagnostics surface, settling with Phase 9); upstream's vertices array =
// renderer.Context.CreateVertices(size) (one per renderer) — mirrored as a
// reused member vector (no per-frame allocation).
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

/// 批渲染器接口(Renderer.IBatchRenderer;Renderer 持当前批指针,切批时
/// Flush 前一批)。
/// The batch-renderer interface (Renderer.IBatchRenderer; Renderer holds the
/// current-batch pointer and flushes the previous batch on switch).
struct IBatchRenderer {
  virtual void Flush() = 0;
  virtual ~IBatchRenderer() = default;
};

/// 混合段跟踪(SpriteRenderer.cs L23-35/L97-109;独立纯逻辑类,可单测)。
/// 同模式四边形原地扩长,模式切换开新段。
/// Blend-span tracking (SpriteRenderer.cs L23-35/L97-109; a standalone
/// pure-logic class, unit-testable). Same-mode quads extend the last span in
/// place; a mode switch opens a new span.
class BlendSpanTracker {
 public:
  struct Span {
    std::int32_t int4_start;  // 段首四边形的顶点序号 | the span's first quad's vertex index
    std::int32_t int4_length; // 段内顶点数(4/四边形)| vertices in the span (4 per quad)
    BlendMode kind_mode;
  };

  /// TrackQuad(blendMode):以当前 vertexCount 开段或扩长末段。
  /// TrackQuad(blendMode): opens a span at the current vertexCount or
  /// extends the last one.
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

/// RGBA 四边形落点(RgbaColorRenderer 的注入面;SpriteRenderer 实现,
/// 测试可替换捕获)。
/// The RGBA-quad sink (RgbaColorRenderer's injection point; implemented by
/// SpriteRenderer, replaceable by test captures).
struct IRgbaQuadSink {
  virtual void DrawRGBAQuad(std::span<const Vertex, 4> vec_vertices, BlendMode kind_mode) = 0;
  virtual ~IRgbaQuadSink() = default;
};

/// 精灵批渲染器(SpriteRenderer.cs;拥有独立 combined shader,与兄弟渲染器
/// 共享 VB/IB)。
/// The sprite batch renderer (SpriteRenderer.cs; owns its combined shader,
/// sharing VB/IB with its siblings).
class SpriteRenderer : public IBatchRenderer, public IRgbaQuadSink {
 public:
  static constexpr std::int32_t kSheetCount = 8;

  /// ptr_current_batch_slot = Renderer 的当前批槽地址(可能为 null =
  /// 独立使用,如测试)。
  /// ptr_current_batch_slot = the address of Renderer's current-batch slot
  /// (possibly null = standalone use, e.g. tests).
  SpriteRenderer(RenderThread& render, VertexBuffer& vertex_buffer, IndexBuffer& index_buffer, Shader&& shader,
                 IBatchRenderer** ptr_current_batch_slot, std::int32_t int4_temp_vertex_buffer_size);

  ~SpriteRenderer() override = default;
  SpriteRenderer(const SpriteRenderer&) = delete;
  SpriteRenderer& operator=(const SpriteRenderer&) = delete;

  /// Flush(SpriteRenderer.cs L62-95):纹理绑定 → PrepareRender → 顶点写入
  /// → VAO 绑定 → 逐混合段 SetBlendMode + DrawElements → 复位 None → 槽
  /// fence → 清状态。
  /// Flush (SpriteRenderer.cs L62-95): texture binds → PrepareRender →
  /// vertex write → VAO bind → per-span SetBlendMode + DrawElements → None
  /// restore → slot fence → state clear.
  void Flush() override;

  /// RGBA 精灵包装(RgbaSpriteRenderer.cs 的四形态;Channel != RGBA 抛上游
  /// 文本)。
  void DrawRgbaSprite(const Sprite& sprite_r, core::Vector3 v_location, float float_scale = 1.0f,
                      float float_rotation = 0.0f);
  void DrawRgbaSprite(const Sprite& sprite_r, core::Vector3 v_location, core::Vector3 v_scale,
                      float float_rotation = 0.0f);
  void DrawRgbaSprite(const Sprite& sprite_r, core::Vector3 v_location, float float_scale, core::Vector3 v_tint,
                      float float_alpha, float float_rotation = 0.0f);
  void DrawRgbaSprite(const Sprite& sprite_r, core::Vector3 v_a, core::Vector3 v_b, core::Vector3 v_c,
                      core::Vector3 v_d, core::Vector3 v_tint, float float_alpha);

  // —— 上游 public/internal DrawSprite 族(internal = 引擎内公开)——
  // —— The upstream public/internal DrawSprite family (internal = public
  // in-engine) ——
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

  /// 外部静态 VB 的整批绘制(SpriteRenderer.cs L221-237;TerrainSpriteLayer
  /// 等用)。
  /// Whole-batch draw of an external static VB (SpriteRenderer.cs L221-237;
  /// for TerrainSpriteLayer among others).
  void DrawVertexBuffer(VertexBuffer& vertex_buffer, IndexBuffer& index_buffer, std::int32_t int4_start,
                        std::int32_t int4_length, std::span<Sheet* const> vec_sheets, BlendMode kind_blend);

  /// RGBA 彩色四边形直送(RgbaColorRenderer 的落点;SpriteRenderer.cs
  /// L246-255)。
  /// Direct RGBA colored-quad intake (RgbaColorRenderer's sink;
  /// SpriteRenderer.cs L246-255).
  void DrawRGBAQuad(std::span<const Vertex, 4> vec_vertices, BlendMode kind_mode) override;

  void SetPalette(HardwarePalette& palette);
  void SetViewportParams(int2 int2_sheet_size, std::int32_t int4_downscale, float float_depth_margin,
                         int2 int2_scroll);
  void SetDepthPreview(bool b_enabled, float float_contrast, float float_offset);
  void EnablePixelArtScaling(bool b_enabled);

  Shader& shader() { return shader_; }

  /// ResolveTextureIndex(SpriteRenderer.cs L164-176;OPT-A7:HasColorShift 走
  /// epoch 缓存)。
  /// ResolveTextureIndex (SpriteRenderer.cs L164-176; OPT-A7: HasColorShift
  /// via the epoch cache).
  static std::int32_t ResolveTextureIndex(const Sprite& sprite_r, const PaletteReference* ptr_palette);

  /// —— 测试面:批状态观察 ——
  /// —— Test surface: batch-state observation ——
  std::int32_t VertexCountForTest() const { return int4_vertex_count_; }
  std::int32_t SheetCountForTest() const { return int4_sheet_count_; }
  const BlendSpanTracker& SpansForTest() const { return tracker_spans_; }

 private:
  /// CurrentBatchRenderer = this 的上游语义(切批先 Flush 前批)。
  /// The upstream semantics of CurrentBatchRenderer = this (switching
  /// flushes the previous batch first).
  void ActivateAsCurrent();

  /// SetRenderStateForSprite(SpriteRenderer.cs L111-162 逐行)。
  /// SetRenderStateForSprite (SpriteRenderer.cs L111-162 line by line).
  int2 SetRenderStateForSprite(const Sprite& sprite_r);

  RenderThread& render_;
  VertexBuffer& vertex_buffer_;
  IndexBuffer& index_buffer_;
  Shader shader_;
  IBatchRenderer** ptr_current_batch_slot_ = nullptr;

  std::vector<Vertex> vec_vertices_;   // 上游 vertices 数组(复用)| upstream's vertices array (reused)
  Sheet* arr_sheets_[kSheetCount] = {};
  BlendSpanTracker tracker_spans_;
  std::int32_t int4_vertex_count_ = 0;
  std::int32_t int4_sheet_count_ = 0;
  std::int32_t int4_temp_vertex_buffer_size_ = 0;

  // uniform 位置缓存(构造期一次;OPT-A6 整数热路径)。
  // Uniform-location cache (once at construction; the OPT-A6 integer hot path).
  std::int32_t int4_loc_palette_rows_ = -1;
  std::int32_t int4_loc_depth_tex_scale_ = -1;
  std::int32_t int4_loc_scroll_ = -1;
  std::int32_t int4_loc_p1_ = -1;
  std::int32_t int4_loc_p2_ = -1;
  std::int32_t int4_loc_enable_depth_preview_ = -1;
  std::int32_t int4_loc_depth_preview_params_ = -1;
  std::int32_t int4_loc_enable_pixel_art_scaling_ = -1;
};

/// RGBA 精灵薄包装(RgbaSpriteRenderer.cs)。
/// The thin RGBA-sprite wrapper (RgbaSpriteRenderer.cs).
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

/// RGBA 彩色图元渲染器(RgbaColorRenderer.cs;几何逐行,落点注入)。
/// The RGBA colored-primitives renderer (RgbaColorRenderer.cs; geometry
/// line by line, sink injected).
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

}  // namespace ora::gfx
