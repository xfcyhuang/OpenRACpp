import std;

#include "mods/world_interaction_controller.hpp"

#include "gfx/renderer.hpp"
#include "gfx/world_renderer.hpp"
#include "mods/unit_order_generator.hpp"
#include "net/order.hpp"
#include "net/voice_exts.hpp"
#include "sim/actor.hpp"
#include "sim/screen_map.hpp"
#include "sim/selectable_exts.hpp"
#include "sim/selection.hpp"
#include "sim/trait_interfaces.hpp"
#include "sim/world.hpp"
#include "ui/chrome_metrics.hpp"
#include "ui/selection_utils.hpp"
#include "ui/ui.hpp"
#include "ui/widget_loader.hpp"

namespace ora::mods {

WorldInteractionControllerWidget::WorldInteractionControllerWidget(
    ora::ui::WidgetArgs& args_args) {
  if (const ora::ui::WidgetArgValue* ptr_value = args_args.TryGet("world"))
    if (const auto* ptr_world_arg = std::get_if<sim::World*>(ptr_value))
      ptr_world = *ptr_world_arg;

  if (const ora::ui::WidgetArgValue* ptr_value = args_args.TryGet("worldRenderer"))
    if (const auto* ptr_wr_arg = std::get_if<gfx::WorldRenderer*>(ptr_value))
      ptr_world_renderer = *ptr_wr_arg;

  game_settings_.mouse_control_style =
      ui::Ui::Instance().GameSettingsInput().kind_mouse_control_style;

  color_alt_selection = core::Color::FromArgb(255, 255, 255, 255);
  color_ctrl_selection = core::Color::FromArgb(255, 255, 255, 255);
  color_normal_selection = core::Color::FromArgb(255, 255, 255, 255);
  if (ui::ChromeMetrics* ptr_metrics = ui::Ui::HasInstance()
                                            ? ui::Ui::Instance().ChromeMetricsDeps()
                                            : nullptr) {
    ptr_metrics->TryGetColor("AltSelectionColor", color_alt_selection);
    ptr_metrics->TryGetColor("CtrlSelectionColor", color_ctrl_selection);
    ptr_metrics->TryGetColor("NormalSelectionColor", color_normal_selection);
    str_click_sound = ptr_metrics->Get("ClickSound");
    str_click_disabled_sound = ptr_metrics->Get("ClickDisabledSound");
  }
}

WorldInteractionControllerWidget::WorldInteractionControllerWidget(
    const WorldInteractionControllerWidget& widget_other)
    : ora::ui::Widget(widget_other),
      ptr_world(widget_other.ptr_world),
      ptr_world_renderer(widget_other.ptr_world_renderer),
      str_click_sound(widget_other.str_click_sound),
      str_click_disabled_sound(widget_other.str_click_disabled_sound),
      color_normal_selection(widget_other.color_normal_selection),
      color_alt_selection(widget_other.color_alt_selection),
      color_ctrl_selection(widget_other.color_ctrl_selection),
      game_settings_(widget_other.game_settings_),
      int2_drag_start(widget_other.int2_drag_start),
      int2_mouse_pos(widget_other.int2_mouse_pos),
      b_is_dragging(widget_other.b_is_dragging) {}

bool WorldInteractionControllerWidget::IsValidDragbox() const {
  return b_is_dragging && (int2_drag_start - int2_mouse_pos).Length() >
                              ui::Ui::Instance().SelectionDeadzone();
}

void WorldInteractionControllerWidget::Draw() {
  const Modifiers modifiers = ui::Ui::Instance().GetModifierKeys();
  std::vector<sim::Actor*> rollover;
  if (IsValidDragbox() && ptr_world_renderer != nullptr) {
    gfx::IViewportSurface* viewport = ptr_world_renderer->Viewport();
    const core::Vector3 a{
        static_cast<float>(viewport->WorldToViewPx(int2_drag_start).X),
        static_cast<float>(viewport->WorldToViewPx(int2_drag_start).Y), 0.0f};
    const core::Vector3 b{
        static_cast<float>(viewport->WorldToViewPx(int2_mouse_pos).X),
        static_cast<float>(viewport->WorldToViewPx(int2_mouse_pos).Y), 0.0f};

    core::Color color = color_normal_selection;
    if (HasModifier(modifiers, Modifiers::Alt) &&
        !HasModifier(modifiers, Modifiers::Ctrl))
      color = color_alt_selection;
    else if (HasModifier(modifiers, Modifiers::Ctrl) &&
             !HasModifier(modifiers, Modifiers::Alt))
      color = color_ctrl_selection;

    if (gfx::Renderer* ptr_renderer = ui::Ui::Instance().RendererDeps())
      ptr_renderer->WorldRgbaColorRenderer().DrawRect(a, b, 1.0f, color);

    rollover = ui::SelectActorsInBoxWithDeadzone(*ptr_world, int2_drag_start,
                                                 int2_mouse_pos, modifiers);
  } else {
    rollover = ui::SelectActorsInBoxWithDeadzone(*ptr_world, int2_mouse_pos,
                                                 int2_mouse_pos, modifiers);
  }

  if (sim::ISelection* selection = ptr_world->Selection())
    selection->SetRollover(rollover);
}

bool WorldInteractionControllerWidget::HandleMouseInput(
    const ora::MouseInput& mi_input) {
  int2_mouse_pos =
      ptr_world_renderer->Viewport()->ViewToWorldPx(mi_input.Location);

  const bool use_classic_mouse_style =
      game_settings_.mouse_control_style == MouseControlStyle::Classic;
  const MouseButton action_button = ptr_world->OrderGenerator()->ActionButton();

  const bool multi_click = mi_input.MultiTapCount >= 2;

  UnitOrderGenerator* uog =
      dynamic_cast<UnitOrderGenerator*>(ptr_world->OrderGenerator());
  if (uog == nullptr) {
    ApplyOrders(*ptr_world, mi_input);
    b_is_dragging = false;
    YieldMouseFocus(mi_input);
    return true;
  }

  if (mi_input.Button == MouseButton::Left &&
      mi_input.Event == MouseInputEvent::Down) {
    if (!TakeMouseFocus(mi_input))
      return false;

    if (use_classic_mouse_style || action_button != MouseButton::Left) {
      int2_drag_start = int2_mouse_pos;
      b_is_dragging = true;
    }
  }

  if (mi_input.Button == MouseButton::Left &&
      mi_input.Event == MouseInputEvent::Up) {
    if (((use_classic_mouse_style && !multi_click) ||
         (!use_classic_mouse_style && action_button == MouseButton::Left)) &&
        HasMouseFocus() && !IsValidDragbox() &&
        !ptr_world->Selection()->Actors().empty() &&
        uog->InputOverridesSelection(*ptr_world, int2_mouse_pos, mi_input)) {
      ApplyOrders(*ptr_world, mi_input);
      b_is_dragging = false;
      YieldMouseFocus(mi_input);
      return true;
    }

    if (multi_click) {
      sim::Actor* unit = nullptr;
      {
        std::vector<sim::ActorBoundsPair> at_mouse =
            ptr_world->ScreenMapFace()->ActorsAtMouse(int2_mouse_pos);
        unit = sim::WithHighestSelectionPriority(at_mouse, int2_mouse_pos,
                                                 mi_input.ModifierFlags);
      }

      std::vector<sim::Player*> eligible_players =
          ui::GetPlayersToIncludeInSelection(*ptr_world);

      bool owner_eligible = false;
      if (unit != nullptr)
        for (const sim::Player* p : eligible_players)
          if (p == unit->Owner())
            owner_eligible = true;

      if (unit != nullptr && owner_eligible) {
        if (sim::ISelectable* s = unit->TraitOrDefault<sim::ISelectable>()) {
          const std::vector<std::string> selection_classes{
              std::string(s->Class())};
          std::vector<sim::Actor*> new_selection = ui::SelectActorsOnScreen(
              *ptr_world, *ptr_world_renderer, &selection_classes,
              eligible_players);

          ptr_world->Selection()->Combine(*ptr_world, new_selection, true, false);
        }
      }
    } else {
      if (b_is_dragging && (uog->ClearSelectionOnLeftClick() || IsValidDragbox())) {
        std::vector<sim::Actor*> new_selection = ui::SelectActorsInBoxWithDeadzone(
            *ptr_world, int2_drag_start, int2_mouse_pos, mi_input.ModifierFlags);
        ptr_world->Selection()->Combine(
            *ptr_world, new_selection,
            HasModifier(mi_input.ModifierFlags, Modifiers::Shift),
            int2_drag_start == int2_mouse_pos);
      }
    }

    ptr_world->CancelInputMode();

    b_is_dragging = false;
    YieldMouseFocus(mi_input);
  }

  if (mi_input.Button == MouseButton::Right &&
      mi_input.Event == MouseInputEvent::Up) {
    if (!IsValidDragbox()) {
      if (use_classic_mouse_style && uog->ClearSelectionOnLeftClick())
        ptr_world->Selection()->Clear();

      ApplyOrders(*ptr_world, mi_input);
    }
  }

  return true;
}

void WorldInteractionControllerWidget::ApplyOrders(sim::World& world,
                                                   const ora::MouseInput& mi_input) {
  sim::IOrderGenerator* generator = world.OrderGenerator();
  if (generator == nullptr)
    return;

  const CPos cell = ptr_world_renderer->Viewport()->ViewToWorld(mi_input.Location);
  const int2 world_pixel =
      ptr_world_renderer->Viewport()->ViewToWorldPx(mi_input.Location);
  std::vector<net::Order*> orders =
      generator->Order(world, cell, world_pixel, mi_input);
  PlayVoiceForOrders(orders);

  bool flashed = false;
  for (net::Order* o : orders) {
    if (o == nullptr)
      continue;

    if (!flashed && !o->b_suppress_visual_feedback) {
      const sim::Target visual_target =
          o->visual_feedback_target.Type() != sim::TargetType::Invalid
              ? o->visual_feedback_target
              : o->target;

      for (auto* notify_order_issued :
           world.WorldActor()->TraitsImplementing<sim::INotifyOrderIssued>())
        flashed = notify_order_issued->OrderIssued(world, o->str_order_string,
                                                   visual_target);
    }

    world.IssueOrder(o);
  }

  for (net::Order* o : orders)
    delete o;
}

const std::string* WorldInteractionControllerWidget::GetCursor(
    int2 pt_pos) const {
  if (IsValidDragbox())
    return nullptr;

  const CPos cell = ptr_world_renderer->Viewport()->ViewToWorld(pt_pos);
  const int2 world_pixel = ptr_world_renderer->Viewport()->ViewToWorldPx(pt_pos);

  const ora::MouseInput mi{
      .Event = MouseInputEvent::Down,
      .Button = ptr_world->OrderGenerator()->ActionButton(),
      .Location = pt_pos,
      .Delta = int2{0, 0},
      .ModifierFlags = ui::Ui::Instance().GetModifierKeys(),
      .MultiTapCount = 0};

  str_cursor_cache =
      ptr_world->OrderGenerator()->GetCursor(*ptr_world, cell, world_pixel, mi);
  return &str_cursor_cache;
}

bool WorldInteractionControllerWidget::HandleKeyPress(
    const ora::KeyInput& e_input) {
  UnitOrderGenerator* uog =
      dynamic_cast<UnitOrderGenerator*>(ptr_world->OrderGenerator());
  if (uog == nullptr)
    return false;

  if (uog->HasIssuedQueuedCommand() && e_input.Event == KeyInputEvent::Up &&
      (e_input.Key == Keycode::LSHIFT || e_input.Key == Keycode::RSHIFT)) {
    ptr_world->CancelInputMode();
    uog->SetHasIssuedQueuedCommand(false);
    return true;
  }

  return false;
}

void RegisterWorldInteractionControllerWidget() {
  static const bool b_registered = [] {
    ui::WidgetTypeRegistry::Instance().Register(
        "WorldInteractionControllerWidget",
        [](ui::WidgetArgs& args_args) -> std::unique_ptr<ui::Widget> {
          return std::make_unique<WorldInteractionControllerWidget>(args_args);
        });
    return true;
  }();
  (void)b_registered;
}

}  // namespace ora::mods
