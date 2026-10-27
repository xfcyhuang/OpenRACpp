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

enum class StatusBarsType : std::int32_t {
  Standard = 0,
  DamageShow = 1,
  AlwaysShow = 2,
};

void SetStatusBarsProvider(std::function<StatusBarsType()> fn_provider);

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

  void DrawExtraBars(gfx::WorldRenderer& wr, core::Vector2 vec_start,
                     core::Vector2 vec_end);

  static void DrawSelectionBar(gfx::WorldRenderer& wr,
                               core::Vector2 vec_start,
                               core::Vector2 vec_end, float float_value,
                               core::Color color_bar);

  static core::Color GetHealthColor(sim::IHealth* ptr_health);

  static void DrawHealthBar(gfx::WorldRenderer& wr, sim::IHealth* ptr_health,
                            core::Vector2 vec_start, core::Vector2 vec_end);

  WPos wpos_pos_;
  Actor* ptr_actor_;
  Rectangle rect_bounds_;
  bool b_display_health_;
  bool b_display_extra_;
};

struct SelectionDecorationsBaseInfoData {
  core::Color color_selection_box =
      core::Color::FromArgb(255, 255, 255);

  static SelectionDecorationsBaseInfoData Parse(
      const meta::RecordObject& rec_info);
};

class SelectionDecorationsBase : public TraitBase,
                                 public sim::ISelectionDecorations,
                                 public sim::IRenderAnnotations,
                                 public sim::INotifyCreated {
 public:
  explicit SelectionDecorationsBase(
      const SelectionDecorationsBaseInfoData& info_data)
      : info_data_{info_data} {}

  void Created(Actor& self) override;

  void RenderAnnotations(Actor& self, gfx::WorldRenderer& wr,
                         std::vector<gfx::RenderItem>& vec_out) override;
  bool SpatiallyPartitionable() const override { return true; }

  void RenderSelectionAnnotations(
      Actor& self, gfx::WorldRenderer& world_renderer, core::Color color_c,
      std::vector<gfx::RenderItem>& vec_out) override;

  int2 GetDecorationOrigin(Actor& self, gfx::WorldRenderer& wr,
                           std::string_view pos, int2 margin) override;

 protected:

  void ActivityTargetPath(Actor& self, std::vector<WPos>& vec_out);

  void DrawDecorations(Actor& self, gfx::WorldRenderer& wr,
                       std::vector<gfx::RenderItem>& vec_out);

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

  std::vector<WPos> vec_target_line_waypoints_;
};

class SelectionDecorations final : public SelectionDecorationsBase,
                                   public sim::IRender {
 public:
  SelectionDecorations(const ActorInitializer& init,
                       const SelectionDecorationsBaseInfoData& info_data);

  ORA_TRAIT_INTERFACES(SelectionDecorations,
                       OpenRA_Mods_Common_Traits_Render_SelectionDecorations,
                       sim::ISelectionDecorations, sim::IRenderAnnotations,
                       sim::INotifyCreated, sim::IRender)

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

  static int2 GetDecorationMargin(std::string_view pos, int2 margin);

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

}
