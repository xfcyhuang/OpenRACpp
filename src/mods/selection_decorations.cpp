// UPSTREAM: OpenRA.Mods.Common/Traits/Render/SelectionDecorationsBase.cs
//          @b6fc03f
#include "gfx/renderer.hpp"
#include "gfx/sprite_renderer.hpp"
#include "gfx/world_renderer.hpp"
#include "mods/production_support.hpp"
#include "mods/render_utils.hpp"
#include "mods/selection_decorations.hpp"
#include "sim/target.hpp"
#include "sim/trait_registry.hpp"
#include "sim/world.hpp"
#include "ui/ui.hpp"

namespace ora::mods {

namespace {

/// StatusBars provider(缺省 Standard;Settings 实体随 Phase 6)
/// The StatusBars provider (Standard by default; the Settings entity rides
/// Phase 6).
std::function<StatusBarsType()>& StatusBarsProvider() {
  static std::function<StatusBarsType()> fn_provider = [] {
    return StatusBarsType::Standard;
  };
  return fn_provider;
}

/// Vector2.Lerp(SelectionBarsAnnotationRenderable.cs 的条值端点)
/// Vector2.Lerp (the bar-value endpoint).
core::Vector2 Lerp(core::Vector2 v_a, core::Vector2 v_b, float fp4_amount) {
  return core::Vector2{v_a.X + (v_b.X - v_a.X) * fp4_amount,
                       v_a.Y + (v_b.Y - v_a.Y) * fp4_amount};
}

core::Vector2 AddOffset(core::Vector2 v, core::Vector2 v_offset) {
  return core::Vector2{v.X + v_offset.X, v.Y + v_offset.Y};
}

/// Vector3 无运算符(POD);注释框四角的分量加减
/// Vector3 carries no operators (a POD); the corner component math of the
/// annotation box.
core::Vector3 AddV(core::Vector3 v_a, core::Vector3 v_b) {
  return core::Vector3{v_a.X + v_b.X, v_a.Y + v_b.Y, v_a.Z + v_b.Z};
}

core::Vector3 SubV(core::Vector3 v_a, core::Vector3 v_b) {
  return core::Vector3{v_a.X - v_b.X, v_a.Y - v_b.Y, v_a.Z - v_b.Z};
}

/// Color.FromArgb(255, r/2, g/2, b/2)(值色减半)
/// Color.FromArgb(255, r/2, g/2, b/2) (the halved value color).
core::Color HalveColor(core::Color color_c) {
  return core::Color::FromArgb(255, color_c.R() / 2, color_c.G() / 2,
                               color_c.B() / 2);
}

/// ISelectionBar 的通用 Custom 槽项构造
/// The generic Custom-slot item construction for the annotation
/// renderables.
template <class AnnotationT>
gfx::RenderItem MakeCustomItem(AnnotationT& r_annotation) {
  gfx::RenderItem item{};
  item.kind = gfx::RenderableKind::Custom;
  item.ptr_custom = static_cast<gfx::IRenderable*>(&r_annotation);
  item.ptr_custom_finalized =
      static_cast<gfx::IFinalizedRenderable*>(&r_annotation);
  item.wpos_pos = item.ptr_custom->Pos();
  return item;
}

}  // namespace

void SetStatusBarsProvider(std::function<StatusBarsType()> fn_provider) {
  StatusBarsProvider() = std::move(fn_provider);
}

// ———— SelectionBoxAnnotationRenderable ————

void SelectionBoxAnnotationRenderable::Render(gfx::WorldRenderer& wr) {
  gfx::Renderer* ptr_renderer = wr.RendererPtr();
  if (ptr_renderer == nullptr)
    return;  // 无头装配跳过绘制(上游 Game.Renderer 恒在)
             // A headless assembly skips the draw (upstream's
             // Game.Renderer always exists).

  const core::Vector3 vec_tl{
      static_cast<float>(wr.Viewport()->WorldToViewPx(int2{
          rect_bounds_.Left(), rect_bounds_.Top()}).X),
      static_cast<float>(wr.Viewport()->WorldToViewPx(int2{
          rect_bounds_.Left(), rect_bounds_.Top()}).Y),
      0};
  const core::Vector3 vec_br{
      static_cast<float>(wr.Viewport()->WorldToViewPx(int2{
          rect_bounds_.Right(), rect_bounds_.Bottom()}).X),
      static_cast<float>(wr.Viewport()->WorldToViewPx(int2{
          rect_bounds_.Right(), rect_bounds_.Bottom()}).Y),
      0};
  const core::Vector3 vec_tr{vec_br.X, vec_tl.Y, 0};
  const core::Vector3 vec_bl{vec_tl.X, vec_br.Y, 0};
  const core::Vector3 vec_u{4, 0, 0};
  const core::Vector3 vec_v{0, 4, 0};

  gfx::RgbaColorRenderer& cr = ptr_renderer->WorldRgbaColorRenderer();
  const std::array<core::Vector3, 3> arr_l1{AddV(vec_tl, vec_u), vec_tl,
                                            AddV(vec_tl, vec_v)};
  const std::array<core::Vector3, 3> arr_l2{SubV(vec_tr, vec_u), vec_tr,
                                            AddV(vec_tr, vec_v)};
  const std::array<core::Vector3, 3> arr_l3{SubV(vec_br, vec_u), vec_br,
                                            SubV(vec_br, vec_v)};
  const std::array<core::Vector3, 3> arr_l4{AddV(vec_bl, vec_u), vec_bl,
                                            SubV(vec_bl, vec_v)};
  cr.DrawLine(arr_l1, 1, color_c_, true);
  cr.DrawLine(arr_l2, 1, color_c_, true);
  cr.DrawLine(arr_l3, 1, color_c_, true);
  cr.DrawLine(arr_l4, 1, color_c_, true);
}

// ———— SelectionBarsAnnotationRenderable ————

void SelectionBarsAnnotationRenderable::DrawExtraBars(
    gfx::WorldRenderer& wr, core::Vector2 vec_start, core::Vector2 vec_end) {
  const core::Vector2 vec_offset{0, 4};
  vec_start = AddOffset(vec_start, vec_offset);
  vec_end = AddOffset(vec_end, vec_offset);
  for (sim::ISelectionBar* ptr_extra_bar :
       ptr_actor_->TraitsImplementing<sim::ISelectionBar>()) {
    const float float_value = ptr_extra_bar->GetValue();
    if (float_value != 0 || ptr_extra_bar->DisplayWhenEmpty()) {
      DrawSelectionBar(wr, vec_start, vec_end, ptr_extra_bar->GetValue(),
                       ptr_extra_bar->GetColor());
      vec_start = AddOffset(vec_start, vec_offset);
      vec_end = AddOffset(vec_end, vec_offset);
    }
  }
}

void SelectionBarsAnnotationRenderable::DrawSelectionBar(
    gfx::WorldRenderer& wr, core::Vector2 vec_start, core::Vector2 vec_end,
    float float_value, core::Color color_bar) {
  const core::Color color_c = core::Color::FromArgb(128, 30, 30, 30);
  const core::Color color_c2 = core::Color::FromArgb(128, 10, 10, 10);
  const core::Vector2 vec_p{0, -4};
  const core::Vector2 vec_q{0, -3};
  const core::Vector2 vec_r{0, -2};

  const core::Color color_bar2 = HalveColor(color_bar);
  const core::Vector2 vec_z = Lerp(vec_start, vec_end, float_value);

  gfx::Renderer* ptr_renderer = wr.RendererPtr();
  if (ptr_renderer == nullptr)
    return;
  gfx::RgbaColorRenderer& cr = ptr_renderer->WorldRgbaColorRenderer();

  cr.DrawLine(AddOffset(vec_start, vec_p), AddOffset(vec_end, vec_p), 1,
              color_c);
  cr.DrawLine(AddOffset(vec_start, vec_q), AddOffset(vec_end, vec_q), 1,
              color_c2);
  cr.DrawLine(AddOffset(vec_start, vec_r), AddOffset(vec_end, vec_r), 1,
              color_c);
  cr.DrawLine(AddOffset(vec_start, vec_p), AddOffset(vec_z, vec_p), 1,
              color_bar2);
  cr.DrawLine(AddOffset(vec_start, vec_q), AddOffset(vec_z, vec_q), 1,
              color_bar);
  cr.DrawLine(AddOffset(vec_start, vec_r), AddOffset(vec_z, vec_r), 1,
              color_bar2);
}

core::Color SelectionBarsAnnotationRenderable::GetHealthColor(
    sim::IHealth* ptr_health) {
  return ptr_health->DamageState() == sim::DamageState::Critical
             ? core::Color::FromArgb(255, 255, 0, 0)  // Red
         : ptr_health->DamageState() == sim::DamageState::Heavy
             ? core::Color::FromArgb(255, 255, 255, 0)  // Yellow
             : core::Color::FromArgb(255, 50, 205, 50);  // LimeGreen
}

void SelectionBarsAnnotationRenderable::DrawHealthBar(
    gfx::WorldRenderer& wr, sim::IHealth* ptr_health,
    core::Vector2 vec_start, core::Vector2 vec_end) {
  if (ptr_health == nullptr || ptr_health->IsDead())
    return;

  const core::Color color_c = core::Color::FromArgb(128, 30, 30, 30);
  const core::Color color_c2 = core::Color::FromArgb(128, 10, 10, 10);
  const core::Vector2 vec_p{0, -4};
  const core::Vector2 vec_q{0, -3};
  const core::Vector2 vec_r{0, -2};

  const core::Color color_health = GetHealthColor(ptr_health);
  const core::Color color_health2 = HalveColor(color_health);

  const core::Vector2 vec_z =
      Lerp(vec_start, vec_end,
           static_cast<float>(ptr_health->HP()) /
               static_cast<float>(ptr_health->MaxHP()));

  gfx::Renderer* ptr_renderer = wr.RendererPtr();
  if (ptr_renderer == nullptr)
    return;
  gfx::RgbaColorRenderer& cr = ptr_renderer->WorldRgbaColorRenderer();

  cr.DrawLine(AddOffset(vec_start, vec_p), AddOffset(vec_end, vec_p), 1,
              color_c);
  cr.DrawLine(AddOffset(vec_start, vec_q), AddOffset(vec_end, vec_q), 1,
              color_c2);
  cr.DrawLine(AddOffset(vec_start, vec_r), AddOffset(vec_end, vec_r), 1,
              color_c);
  cr.DrawLine(AddOffset(vec_start, vec_p), AddOffset(vec_z, vec_p), 1,
              color_health2);
  cr.DrawLine(AddOffset(vec_start, vec_q), AddOffset(vec_z, vec_q), 1,
              color_health);
  cr.DrawLine(AddOffset(vec_start, vec_r), AddOffset(vec_z, vec_r), 1,
              color_health2);

  // DisplayHP 漂移段 = 伤害预告
  // The DisplayHP drift segment = the damage forecast.
  if (ptr_health->DisplayHP() != ptr_health->HP()) {
    const core::Color color_delta = core::Color::FromArgb(255, 255, 69, 0);  // OrangeRed
    const core::Color color_delta2 = HalveColor(color_delta);
    const core::Vector2 vec_zz =
        Lerp(vec_start, vec_end,
             static_cast<float>(ptr_health->DisplayHP()) /
                 static_cast<float>(ptr_health->MaxHP()));

    cr.DrawLine(AddOffset(vec_z, vec_p), AddOffset(vec_zz, vec_p), 1,
                color_delta2);
    cr.DrawLine(AddOffset(vec_z, vec_q), AddOffset(vec_zz, vec_q), 1,
                color_delta);
    cr.DrawLine(AddOffset(vec_z, vec_r), AddOffset(vec_zz, vec_r), 1,
                color_delta2);
  }
}

void SelectionBarsAnnotationRenderable::Render(gfx::WorldRenderer& wr) {
  if (ptr_actor_ == nullptr || !ptr_actor_->IsInWorld() ||
      ptr_actor_->IsDead())
    return;

  sim::IHealth* ptr_health = ptr_actor_->TraitOrDefault<sim::IHealth>();
  const core::Vector2 vec_start{
      static_cast<float>(wr.Viewport()->WorldToViewPx(int2{
          rect_bounds_.Left() + 1, rect_bounds_.Top()}).X),
      static_cast<float>(wr.Viewport()->WorldToViewPx(int2{
          rect_bounds_.Left() + 1, rect_bounds_.Top()}).Y)};
  const core::Vector2 vec_end{
      static_cast<float>(wr.Viewport()->WorldToViewPx(int2{
          rect_bounds_.Right() - 1, rect_bounds_.Top()}).X),
      static_cast<float>(wr.Viewport()->WorldToViewPx(int2{
          rect_bounds_.Right() - 1, rect_bounds_.Top()}).Y)};

  if (b_display_health_)
    DrawHealthBar(wr, ptr_health, vec_start, vec_end);
  if (b_display_extra_)
    DrawExtraBars(wr, vec_start, vec_end);
}

// ———— SelectionDecorationsBase ————

SelectionDecorationsBaseInfoData SelectionDecorationsBaseInfoData::Parse(
    const meta::RecordObject& rec_info) {
  SelectionDecorationsBaseInfoData data;
  if (const auto* gv = sim::RecordFieldValue(rec_info, "SelectionBoxColor"))
    if (const auto* n = std::get_if<std::int64_t>(&gv->val))
      data.color_selection_box = core::Color::FromArgbRaw(
          static_cast<std::uint32_t>(*n));
  return data;
}

void SelectionDecorationsBase::Created(Actor& self) {
  vec_selected_decorations_ = self.TraitsImplementing<sim::IDecoration>();
  vec_decorations_.clear();
  for (sim::IDecoration* decoration : vec_selected_decorations_)
    if (!decoration->RequiresSelection())
      vec_decorations_.push_back(decoration);
}

void SelectionDecorationsBase::ActivityTargetPath(Actor& self,
                                                  std::vector<WPos>& vec_out) {
  if (!self.IsInWorld() || self.IsDead())
    return;

  sim::Activity* activity = self.CurrentActivity();
  if (activity != nullptr) {
    vec_out.push_back(self.CenterPosition());
    std::vector<sim::Target> vec_targets;
    activity->GetTargets(self, vec_targets);
    for (const sim::Target& t : vec_targets)
      if (t.Type() != sim::TargetType::Invalid)
        vec_out.push_back(t.CenterPosition());
  }
}

void SelectionDecorationsBase::RenderAnnotations(
    Actor& self, gfx::WorldRenderer& wr,
    std::vector<gfx::RenderItem>& vec_out) {
  if (self.world().FogObscures(self)) {
    vec_out.clear();
    return;
  }
  DrawDecorations(self, wr, vec_out);
}

void SelectionDecorationsBase::RenderSelectionAnnotations(
    Actor& self, gfx::WorldRenderer& world_renderer, core::Color color_c,
    std::vector<gfx::RenderItem>& vec_out) {
  RenderSelectionBox(self, world_renderer, color_c, vec_out);
}

int2 SelectionDecorationsBase::GetDecorationOrigin(
    Actor& self, gfx::WorldRenderer& wr, std::string_view pos,
    int2 margin) {
  return GetDecorationOriginImpl(self, wr, pos, margin);
}

void SelectionDecorationsBase::DrawDecorations(
    Actor& self, gfx::WorldRenderer& wr,
    std::vector<gfx::RenderItem>& vec_out) {
  if (!ui::Ui::Instance().WidgetsVisible()) {
    vec_out.clear();
    return;
  }

  const bool b_selected = self.world().Selection()->Contains(&self);
  const bool b_regular_world = self.world().Type() == sim::WorldType::Regular;
  const StatusBarsType status_bars = StatusBarsProvider()();

  bool b_display_health =
      b_selected ||
      (b_regular_world && status_bars == StatusBarsType::AlwaysShow) ||
      (b_regular_world && status_bars == StatusBarsType::DamageShow &&
       render::GetDamageState(self) != sim::DamageState::Undamaged);

  bool b_display_extra =
      b_selected || (b_regular_world && status_bars != StatusBarsType::Standard);

  // 仅需要时查 rollover;命中则两局部同置真
  // The rollover lookup only when needed; a hit sets both locals true.
  if (!b_display_health || !b_display_extra)
    if (self.world().Selection()->RolloverContains(&self))
      b_display_health = b_display_extra = true;

  if (b_selected)
    RenderSelectionBox(self, wr, info_data_.color_selection_box, vec_out);
  if (b_display_health || b_display_extra)
    RenderSelectionBars(self, wr, b_display_health, b_display_extra, vec_out);

  if (b_selected && self.world().LocalPlayer() != nullptr) {
    DeveloperMode* developer_mode =
        self.world().LocalPlayer()->PlayerActor()->TraitOrDefault<
            DeveloperMode>();
    if (developer_mode != nullptr && developer_mode->PathDebug) {
      vec_target_line_waypoints_.clear();
      ActivityTargetPath(self, vec_target_line_waypoints_);
      vec_out.push_back(gfx::MakeTargetLineRenderable(
          vec_target_line_waypoints_, core::Color::FromArgb(255, 0, 128, 0),
          1, 2));
    }
  }

  if (wr.Viewport() != nullptr &&
      wr.Viewport()->Zoom() < wr.Viewport()->MinZoom())
    return;

  const std::vector<sim::IDecoration*>& vec_render_decorations =
      b_selected ? vec_selected_decorations_ : vec_decorations_;
  for (sim::IDecoration* decoration : vec_render_decorations)
    decoration->RenderDecoration(self, wr, *this, vec_out);
}

// ———— SelectionDecorations ————

SelectionDecorations::SelectionDecorations(
    const ActorInitializer& init,
    const SelectionDecorationsBaseInfoData& info_data)
    : SelectionDecorationsBase{info_data} {
  ptr_interactable_ = init.Self().Trait<Interactable>();
}

int2 SelectionDecorations::GetDecorationPosition(Actor& self,
                                                 gfx::WorldRenderer& wr,
                                                 std::string_view pos) {
  const Rectangle bounds = ptr_interactable_->DecorationBounds(self, wr);
  if (pos == "TopLeft")
    return bounds.TopLeft();
  if (pos == "TopRight")
    return bounds.TopRight();
  if (pos == "BottomLeft")
    return bounds.BottomLeft();
  if (pos == "BottomRight")
    return bounds.BottomRight();
  if (pos == "Top")
    return int2{bounds.Left() + bounds.Width / 2, bounds.Top()};
  return bounds.TopLeft() +
         int2{bounds.Width / 2, bounds.Height / 2};
}

int2 SelectionDecorations::GetDecorationMargin(std::string_view pos,
                                               int2 margin) {
  // L49-63
  if (pos == "TopLeft")
    return margin;
  if (pos == "TopRight")
    return int2{-margin.X, margin.Y};
  if (pos == "BottomLeft")
    return int2{margin.X, -margin.Y};
  if (pos == "BottomRight")
    return int2{-margin.X, -margin.Y};
  if (pos == "Top")
    return int2{0, margin.Y};
  return int2{};
}

int2 SelectionDecorations::GetDecorationOriginImpl(Actor& self,
                                                   gfx::WorldRenderer& wr,
                                                   std::string_view pos,
                                                   int2 margin) {
  // L74-76
  const int2 int2_px = wr.Viewport()->WorldToViewPx(
      GetDecorationPosition(self, wr, pos));
  return int2_px + GetDecorationMargin(pos, margin);
}

void SelectionDecorations::RenderSelectionBox(
    Actor& self, gfx::WorldRenderer& wr, core::Color color_c,
    std::vector<gfx::RenderItem>& vec_out) {
  box_annotation_ = SelectionBoxAnnotationRenderable{
      self.CenterPosition(),
      ptr_interactable_->DecorationBounds(self, wr), color_c};
  vec_out.push_back(MakeCustomItem(box_annotation_));
}

void SelectionDecorations::RenderSelectionBars(
    Actor& self, gfx::WorldRenderer& wr, bool b_display_health,
    bool b_display_extra, std::vector<gfx::RenderItem>& vec_out) {
  // 非 Selectable 不画条(上游 is-Selectable 判定)
  // Non-Selectable draws no bars (upstream's is-Selectable test).
  if (dynamic_cast<Selectable*>(ptr_interactable_) == nullptr ||
      (!b_display_health && !b_display_extra))
    return;

  bars_annotation_ = SelectionBarsAnnotationRenderable{
      self.CenterPosition(), &self,
      ptr_interactable_->DecorationBounds(self, wr), b_display_health,
      b_display_extra};
  vec_out.push_back(MakeCustomItem(bars_annotation_));
}

std::vector<Rectangle> SelectionDecorations::ScreenBounds(
    Actor& self, gfx::WorldRenderer& wr) {
  return {ptr_interactable_->DecorationBounds(self, wr)};
}

}  // namespace ora::mods
