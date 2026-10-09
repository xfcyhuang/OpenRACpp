// UPSTREAM: OpenRA.Mods.Common/Traits/Render/SelectionDecorationsBase.cs
//          @b6fc03f L18-139 + SelectionDecorations.cs L18-93 +
//          Graphics/SelectionBoxAnnotationRenderable.cs +
//          SelectionBarsAnnotationRenderable.cs 选择框/血条/装饰注释族。
//          StatusBars 读取 = 注入 provider(缺省 Standard;Settings 实体随
//          Phase 6);注释 renderable 的每帧 new → trait 所有可能重配置对象
//          经 Custom 槽入收集流。
//          The selection-box/health-bar/decoration family. The StatusBars
//          read = an injected provider (Standard by default; the Settings
//          entity rides Phase 6); the annotation renderables' per-frame new
//          → the trait-owned objects reconfigured per frame, entering the
//          collection flow via the Custom slot.
#pragma once
import std;

#include "core/color.hpp"
#include "core/int2.hpp"
#include "core/rectangle.hpp"
#include "core/vector_n.hpp"
#include "gfx/renderable.hpp"
#include "meta/generic_record.hpp"
#include "mods/selectable.hpp"
#include "sim/actor.hpp"
#include "sim/trait_interfaces.hpp"

namespace ora::gfx {
class WorldRenderer;
}

namespace ora::mods {

using sim::Actor;
using sim::ActorInitializer;
using sim::TraitBase;

/// Settings.cs L26:StatusBarsType
/// Settings.cs L26: StatusBarsType.
enum class StatusBarsType : std::int32_t {
  Standard = 0,
  DamageShow = 1,
  AlwaysShow = 2,
};

/// StatusBars 的注入面(测试/装配接线;缺省 Standard)
/// The StatusBars injection face (wired by the test/assembly; the default
/// is Standard).
void SetStatusBarsProvider(std::function<StatusBarsType()> fn_provider);

/// SelectionBoxAnnotationRenderable(SelectionBoxAnnotationRenderable.cs
/// L19-58):四角短线选择框
/// SelectionBoxAnnotationRenderable
/// (SelectionBoxAnnotationRenderable.cs L19-58): the four-corner selection
/// box.
class SelectionBoxAnnotationRenderable final : public gfx::IRenderable,
                                               public gfx::IFinalizedRenderable {
 public:
  SelectionBoxAnnotationRenderable(WPos wpos_pos, Rectangle rect_bounds,
                                   core::Color color_c)
      : wpos_pos_{wpos_pos}, rect_bounds_{rect_bounds}, color_c_{color_c} {}

  WPos Pos() const override { return wpos_pos_; }
  std::int32_t ZOffset() const override { return 0; }
  bool IsDecoration() const override { return true; }
  gfx::IRenderable* WithZOffset(std::int32_t) override { return this; }
  gfx::IRenderable* OffsetBy(const WVec& vec_offset) override {
    wpos_pos_ = wpos_pos_ + vec_offset;
    return this;
  }
  gfx::IRenderable* AsDecoration() override { return this; }
  gfx::IFinalizedRenderable* PrepareRender(gfx::WorldRenderer&) override {
    return this;
  }

  void Render(gfx::WorldRenderer& wr) override;
  void RenderDebugGeometry(gfx::WorldRenderer&) override {}
  Rectangle ScreenBounds(gfx::WorldRenderer&) override {
    return Rectangle{};
  }

 private:
  WPos wpos_pos_;
  Rectangle rect_bounds_;
  core::Color color_c_;
};

/// SelectionBarsAnnotationRenderable(SelectionBarsAnnotationRenderable.cs
/// L19-152):血条 + 附加条
/// SelectionBarsAnnotationRenderable
/// (SelectionBarsAnnotationRenderable.cs L19-152): the health + extra
/// bars.
class SelectionBarsAnnotationRenderable final : public gfx::IRenderable,
                                                public gfx::IFinalizedRenderable {
 public:
  SelectionBarsAnnotationRenderable(WPos wpos_pos, Actor* ptr_actor,
                                    Rectangle rect_bounds,
                                    bool b_display_health, bool b_display_extra)
      : wpos_pos_{wpos_pos},
        ptr_actor_{ptr_actor},
        rect_bounds_{rect_bounds},
        b_display_health_{b_display_health},
        b_display_extra_{b_display_extra} {}

  WPos Pos() const override { return wpos_pos_; }
  std::int32_t ZOffset() const override { return 0; }
  bool IsDecoration() const override { return true; }
  gfx::IRenderable* WithZOffset(std::int32_t) override { return this; }
  gfx::IRenderable* OffsetBy(const WVec& vec_offset) override {
    wpos_pos_ = wpos_pos_ + vec_offset;
    return this;
  }
  gfx::IRenderable* AsDecoration() override { return this; }
  gfx::IFinalizedRenderable* PrepareRender(gfx::WorldRenderer&) override {
    return this;
  }

  void Render(gfx::WorldRenderer& wr) override;
  void RenderDebugGeometry(gfx::WorldRenderer&) override {}
  Rectangle ScreenBounds(gfx::WorldRenderer&) override {
    return Rectangle{};
  }

 private:
  /// DrawExtraBars(L30-42):ISelectionBar 族逐条 +4px 下移
  /// DrawExtraBars (L30-42): the ISelectionBar family, +4px per row.
  void DrawExtraBars(gfx::WorldRenderer& wr, core::Vector2 vec_start,
                     core::Vector2 vec_end);

  /// DrawSelectionBar(L44-63):三层底 + 三层值
  /// DrawSelectionBar (L44-63): the three base lines + the three value
  /// lines.
  static void DrawSelectionBar(gfx::WorldRenderer& wr,
                               core::Vector2 vec_start,
                               core::Vector2 vec_end, float float_value,
                               core::Color color_bar);

  /// GetHealthColor(L65-69)
  static core::Color GetHealthColor(sim::IHealth* ptr_health);

  /// DrawHealthBar(L71-107):含 DisplayHP 差值段
  /// DrawHealthBar (L71-107): with the DisplayHP delta segment.
  static void DrawHealthBar(gfx::WorldRenderer& wr, sim::IHealth* ptr_health,
                            core::Vector2 vec_start, core::Vector2 vec_end);

  WPos wpos_pos_;
  Actor* ptr_actor_;
  Rectangle rect_bounds_;
  bool b_display_health_;
  bool b_display_extra_;
};

/// SelectionDecorationsBaseInfo(L17-21)的解析面
/// The parsed face of SelectionDecorationsBaseInfo (L17-21).
struct SelectionDecorationsBaseInfoData {
  core::Color color_selection_box =
      core::Color::FromArgb(255, 255, 255);  // L19 Color.White

  static SelectionDecorationsBaseInfoData Parse(
      const meta::RecordObject& rec_info);
};

/// SelectionDecorationsBase(L23-139):注释族抽象基座
/// SelectionDecorationsBase (L23-139): the annotation family's abstract
/// base.
class SelectionDecorationsBase : public TraitBase,
                                 public sim::ISelectionDecorations,
                                 public sim::IRenderAnnotations,
                                 public sim::INotifyCreated {
 public:
  explicit SelectionDecorationsBase(
      const SelectionDecorationsBaseInfoData& info_data)
      : info_data_{info_data} {}

  void Created(Actor& self) override;

  /// IRenderAnnotations(L90-98)
  void RenderAnnotations(Actor& self, gfx::WorldRenderer& wr,
                         std::vector<gfx::RenderItem>& vec_out) override;
  bool SpatiallyPartitionable() const override { return true; }

  /// ISelectionDecorations(L131-134):委托受保护的 GetDecorationOriginImpl
  /// ISelectionDecorations (L131-134): delegates to the protected
  /// GetDecorationOriginImpl.
  void RenderSelectionAnnotations(
      Actor& self, gfx::WorldRenderer& world_renderer, core::Color color_c,
      std::vector<gfx::RenderItem>& vec_out) override;

  int2 GetDecorationOrigin(Actor& self, gfx::WorldRenderer& wr,
                           std::string_view pos, int2 margin) override;

 protected:
  /// ActivityTargetPath(L65-80):当前活动目标线锚点序
  /// ActivityTargetPath (L65-80): the current activity's target-line
  /// anchor sequence.
  void ActivityTargetPath(Actor& self, std::vector<WPos>& vec_out);

  /// DrawDecorations(L82-128)
  void DrawDecorations(Actor& self, gfx::WorldRenderer& wr,
                       std::vector<gfx::RenderItem>& vec_out);

  /// L136-138:三抽象钩(子类 SelectionDecorations 实现)
  /// L136-138: the three abstract hooks (the SelectionDecorations subclass
  /// implements).
  virtual int2 GetDecorationOriginImpl(Actor& self, gfx::WorldRenderer& wr,
                                       std::string_view pos,
                                       int2 margin) = 0;
  virtual void RenderSelectionBox(Actor& self, gfx::WorldRenderer& wr,
                                  core::Color color_c,
                                  std::vector<gfx::RenderItem>& vec_out) = 0;
  virtual void RenderSelectionBars(Actor& self, gfx::WorldRenderer& wr,
                                   bool b_display_health,
                                   bool b_display_extra,
                                   std::vector<gfx::RenderItem>& vec_out) = 0;

  SelectionDecorationsBaseInfoData info_data_;
  std::vector<sim::IDecoration*> vec_selected_decorations_;
  std::vector<sim::IDecoration*> vec_decorations_;

  /// TargetLine 锚点的帧缓冲(TargetLine 槽以 span 引用调用方存储)
  /// The frame buffer of the TargetLine anchors (the TargetLine slot
  /// references the caller's storage by span).
  std::vector<WPos> vec_target_line_waypoints_;
};

/// SelectionDecorations(SelectionDecorations.cs L27-93)
class SelectionDecorations final : public SelectionDecorationsBase,
                                   public sim::IRender {
 public:
  SelectionDecorations(const ActorInitializer& init,
                       const SelectionDecorationsBaseInfoData& info_data);

  ORA_TRAIT_INTERFACES(SelectionDecorations,
                       OpenRA_Mods_Common_Traits_Render_SelectionDecorations,
                       sim::ISelectionDecorations, sim::IRenderAnnotations,
                       sim::INotifyCreated, sim::IRender)

  /// IRender(L89-92):恒空(注释走 IRenderAnnotations;共享出参 vector
  /// 不可清空,追加零项)
  /// IRender (L89-92): always empty (the annotations ride
  /// IRenderAnnotations; the shared out vector must not be cleared —
  /// appends zero items).
  void Render(Actor& self, gfx::WorldRenderer& wr,
              std::vector<gfx::RenderItem>& vec_out) override {
    (void)self;
    (void)wr;
    (void)vec_out;
  }

  std::vector<Rectangle> ScreenBounds(Actor& self,
                                      gfx::WorldRenderer& wr) override;

 protected:
  int2 GetDecorationPosition(Actor& self, gfx::WorldRenderer& wr,
                             std::string_view pos);

  /// GetDecorationMargin(L49-63)
  static int2 GetDecorationMargin(std::string_view pos, int2 margin);

  /// L77-78:抽象钩实现
  /// L77-78: the abstract hooks.
  int2 GetDecorationOriginImpl(Actor& self, gfx::WorldRenderer& wr,
                               std::string_view pos, int2 margin) override;
  void RenderSelectionBox(Actor& self, gfx::WorldRenderer& wr,
                          core::Color color_c,
                          std::vector<gfx::RenderItem>& vec_out) override;
  void RenderSelectionBars(Actor& self, gfx::WorldRenderer& wr,
                           bool b_display_health, bool b_display_extra,
                           std::vector<gfx::RenderItem>& vec_out) override;

 private:
  Interactable* ptr_interactable_ = nullptr;
  SelectionBoxAnnotationRenderable box_annotation_{
      WPos{}, Rectangle{}, core::Color{}};
  SelectionBarsAnnotationRenderable bars_annotation_{
      WPos{}, nullptr, Rectangle{}, false, false};
};

}  // namespace ora::mods
