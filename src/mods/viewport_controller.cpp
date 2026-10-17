import std;

#include "mods/viewport_controller.hpp"

#include "game/actor_info.hpp"
#include "gfx/viewport.hpp"
#include "gfx/world_renderer.hpp"
#include "sim/actor.hpp"
#include "sim/frozen_actor_layer.hpp"
#include "sim/screen_map.hpp"
#include "sim/selectable_exts.hpp"
#include "sim/world.hpp"
#include "ui/ui.hpp"
#include "ui/widget_loader.hpp"
#include "ui/widgets/tooltip_container_widget.hpp"

namespace ora::mods {
namespace {

struct ScrollCursorEntry {
  gfx::ScrollDirection kind_direction;
  const char* str_cursor;
};

constexpr ScrollCursorEntry kScrollCursors[] = {
    {gfx::ScrollDirection(gfx::ScrollDirection::Up | gfx::ScrollDirection::Left),
     "scroll-tl"},
    {gfx::ScrollDirection(gfx::ScrollDirection::Up | gfx::ScrollDirection::Right),
     "scroll-tr"},
    {gfx::ScrollDirection(gfx::ScrollDirection::Down | gfx::ScrollDirection::Left),
     "scroll-bl"},
    {gfx::ScrollDirection(gfx::ScrollDirection::Down | gfx::ScrollDirection::Right),
     "scroll-br"},
    {gfx::ScrollDirection::Up, "scroll-t"},
    {gfx::ScrollDirection::Down, "scroll-b"},
    {gfx::ScrollDirection::Left, "scroll-l"},
    {gfx::ScrollDirection::Right, "scroll-r"},
};

constexpr ScrollCursorEntry kJoystickCursors[] = {
    {gfx::ScrollDirection(gfx::ScrollDirection::Up | gfx::ScrollDirection::Left),
     "joystick-tl-blocked"},
    {gfx::ScrollDirection(gfx::ScrollDirection::Up | gfx::ScrollDirection::Right),
     "joystick-tr-blocked"},
    {gfx::ScrollDirection(gfx::ScrollDirection::Down | gfx::ScrollDirection::Left),
     "joystick-bl-blocked"},
    {gfx::ScrollDirection(gfx::ScrollDirection::Down | gfx::ScrollDirection::Right),
     "joystick-br-blocked"},
    {gfx::ScrollDirection::Up, "joystick-t-blocked"},
    {gfx::ScrollDirection::Down, "joystick-b-blocked"},
    {gfx::ScrollDirection::Left, "joystick-l-blocked"},
    {gfx::ScrollDirection::Right, "joystick-r-blocked"},
};

struct ScrollOffsetEntry {
  gfx::ScrollDirection kind_direction;
  core::Vector2 vec_offset;
};

constexpr ScrollOffsetEntry kScrollOffsets[] = {
    {gfx::ScrollDirection::Up, core::Vector2{0.0f, -1.0f}},
    {gfx::ScrollDirection::Down, core::Vector2{0.0f, 1.0f}},
    {gfx::ScrollDirection::Left, core::Vector2{-1.0f, 0.0f}},
    {gfx::ScrollDirection::Right, core::Vector2{1.0f, 0.0f}},
};

std::string FormatBookmarkIndex(std::int32_t int4_index) {
  std::string str_suffix = std::to_string(int4_index + 1);
  while (str_suffix.size() < 2)
    str_suffix.insert(str_suffix.begin(), '0');
  return str_suffix;
}

}  // namespace

ViewportControllerWidget::ViewportControllerWidget(ora::ui::WidgetArgs& args_args) {
  if (const ora::ui::WidgetArgValue* ptr_value = args_args.TryGet("modData"))
    if (const auto* ptr_mod = std::get_if<game::ModData*>(ptr_value))
      ptr_mod_data = *ptr_mod;

  if (const ora::ui::WidgetArgValue* ptr_value = args_args.TryGet("world"))
    if (const auto* ptr_world_arg = std::get_if<sim::World*>(ptr_value))
      ptr_world = *ptr_world_arg;

  if (const ora::ui::WidgetArgValue* ptr_value = args_args.TryGet("worldRenderer"))
    if (const auto* ptr_wr_arg = std::get_if<gfx::WorldRenderer*>(ptr_value))
      ptr_world_renderer = *ptr_wr_arg;

  if (ptr_world != nullptr && ptr_world->WorldActor() != nullptr)
    vec_resource_renderers =
        ptr_world->WorldActor()->TraitsImplementing<sim::IResourceRenderer>();

  if (ptr_world_renderer != nullptr && ptr_world_renderer->Viewport() != nullptr)
    uint8_viewport_tick_token = ptr_world_renderer->Viewport()->SubscribeViewportTick(
        [this] { UpdateCameraTick(); });
}

ViewportControllerWidget::ViewportControllerWidget(
    const ViewportControllerWidget& widget_other)
    : ora::ui::Widget(widget_other),
      ptr_mod_data(widget_other.ptr_mod_data),
      ptr_world(widget_other.ptr_world),
      ptr_world_renderer(widget_other.ptr_world_renderer),
      hk_zoom_in(widget_other.hk_zoom_in),
      hk_zoom_out(widget_other.hk_zoom_out),
      hk_scroll_up(widget_other.hk_scroll_up),
      hk_scroll_down(widget_other.hk_scroll_down),
      hk_scroll_left(widget_other.hk_scroll_left),
      hk_scroll_right(widget_other.hk_scroll_right),
      hk_jump_to_top_edge(widget_other.hk_jump_to_top_edge),
      hk_jump_to_bottom_edge(widget_other.hk_jump_to_bottom_edge),
      hk_jump_to_left_edge(widget_other.hk_jump_to_left_edge),
      hk_jump_to_right_edge(widget_other.hk_jump_to_right_edge),
      str_bookmark_save_key_prefix(widget_other.str_bookmark_save_key_prefix),
      str_bookmark_restore_key_prefix(widget_other.str_bookmark_restore_key_prefix),
      int4_bookmark_key_count(widget_other.int4_bookmark_key_count),
      str_tooltip_template(widget_other.str_tooltip_template),
      str_tooltip_container(widget_other.str_tooltip_container),
      vec_resource_renderers(widget_other.vec_resource_renderers),
      vec_save_bookmark_hotkeys(widget_other.vec_save_bookmark_hotkeys),
      vec_restore_bookmark_hotkeys(widget_other.vec_restore_bookmark_hotkeys),
      vec_bookmark_positions(widget_other.vec_bookmark_positions) {
  if (ptr_world_renderer != nullptr && ptr_world_renderer->Viewport() != nullptr)
    uint8_viewport_tick_token = ptr_world_renderer->Viewport()->SubscribeViewportTick(
        [this] { UpdateCameraTick(); });
}

ViewportControllerWidget::~ViewportControllerWidget() {
  if (uint8_viewport_tick_token != 0 && ptr_world_renderer != nullptr &&
      ptr_world_renderer->Viewport() != nullptr)
    ptr_world_renderer->Viewport()->UnsubscribeViewportTick(
        uint8_viewport_tick_token);
}

bool ViewportControllerWidget::LoadFieldOrProperty(std::string_view str_key,
                                                   std::string_view str_value) {
  const auto load_hotkey = [&](ui::HotkeyReference& hk_target) {
    hk_target = ui::Ui::Instance().HotkeyLookup(str_value);
    return true;
  };

  if (str_key == "ZoomInKey")
    return load_hotkey(hk_zoom_in);
  if (str_key == "ZoomOutKey")
    return load_hotkey(hk_zoom_out);
  if (str_key == "ScrollUpKey")
    return load_hotkey(hk_scroll_up);
  if (str_key == "ScrollDownKey")
    return load_hotkey(hk_scroll_down);
  if (str_key == "ScrollLeftKey")
    return load_hotkey(hk_scroll_left);
  if (str_key == "ScrollRightKey")
    return load_hotkey(hk_scroll_right);
  if (str_key == "JumpToTopEdgeKey")
    return load_hotkey(hk_jump_to_top_edge);
  if (str_key == "JumpToBottomEdgeKey")
    return load_hotkey(hk_jump_to_bottom_edge);
  if (str_key == "JumpToLeftEdgeKey")
    return load_hotkey(hk_jump_to_left_edge);
  if (str_key == "JumpToRightEdgeKey")
    return load_hotkey(hk_jump_to_right_edge);
  if (str_key == "BookmarkSaveKeyPrefix") {
    str_bookmark_save_key_prefix = std::string{str_value};
    return true;
  }
  if (str_key == "BookmarkRestoreKeyPrefix") {
    str_bookmark_restore_key_prefix = std::string{str_value};
    return true;
  }
  if (str_key == "BookmarkKeyCount") {
    int4_bookmark_key_count = std::stoi(std::string{str_value});
    return true;
  }
  if (str_key == "TooltipTemplate") {
    str_tooltip_template = std::string{str_value};
    return true;
  }
  if (str_key == "TooltipContainer") {
    str_tooltip_container = std::string{str_value};
    return true;
  }

  return ora::ui::Widget::LoadFieldOrProperty(str_key, str_value);
}

void ViewportControllerWidget::Initialize(ora::ui::WidgetArgs& args_args) {
  ora::ui::Widget::Initialize(args_args);

  vec_save_bookmark_hotkeys.clear();
  for (std::int32_t i = 0; i < int4_bookmark_key_count; i++)
    vec_save_bookmark_hotkeys.push_back(
        ui::Ui::Instance().HotkeyLookup(str_bookmark_save_key_prefix +
                                    FormatBookmarkIndex(i)));

  vec_restore_bookmark_hotkeys.clear();
  for (std::int32_t i = 0; i < int4_bookmark_key_count; i++)
    vec_restore_bookmark_hotkeys.push_back(
        ui::Ui::Instance().HotkeyLookup(str_bookmark_restore_key_prefix +
                                    FormatBookmarkIndex(i)));

  vec_bookmark_positions.assign(int4_bookmark_key_count, std::nullopt);
}

void ViewportControllerWidget::MaterializeTooltipContainer() {
  if (b_tooltip_container_materialized)
    return;
  b_tooltip_container_materialized = true;
  if (ui::Ui::Instance().Root() != nullptr)
    ptr_tooltip_container_widget =
        ui::Ui::Instance().Root()->Get<ui::TooltipContainerWidget>(str_tooltip_container);
}

void ViewportControllerWidget::MouseEntered() {
  if (str_tooltip_container.empty())
    return;

  MaterializeTooltipContainer();
  if (ptr_tooltip_container_widget != nullptr)
    ptr_tooltip_container_widget->SetTooltip(
        str_tooltip_template,
        ora::ui::WidgetArgs{{"world", ptr_world}, {"viewport", this}});
}

void ViewportControllerWidget::MouseExited() {
  if (str_tooltip_container.empty())
    return;

  MaterializeTooltipContainer();
  if (ptr_tooltip_container_widget != nullptr)
    ptr_tooltip_container_widget->RemoveTooltip();
}

void ViewportControllerWidget::UpdateCameraTick() {
  const GameSettingsInputFace settings_game = ui::Ui::Instance().GameSettingsInput();
  gfx::IViewportSurface* viewport =
      ptr_world_renderer != nullptr ? ptr_world_renderer->Viewport() : nullptr;
  if (viewport == nullptr)
    return;

  if (IsJoystickScrolling()) {
    const float rate = 0.01f * settings_game.fp4_viewport_edge_scroll_step;

    const core::Vector2 scroll{
        static_cast<float>((*opt_joystick_scroll_end - *opt_joystick_scroll_start).X) *
            rate,
        static_cast<float>((*opt_joystick_scroll_end - *opt_joystick_scroll_start).Y) *
            rate};
    viewport->Scroll(scroll, false);
  } else if (!b_is_standard_scrolling) {
    kind_edge_directions = gfx::ScrollDirection::None;
    if (settings_game.b_viewport_edge_scroll && ui::Ui::Instance().WindowHasInputFocus())
      kind_edge_directions = CheckForDirections();

    if (ui::Ui::Instance().KeyboardFocusWidget() != nullptr)
      kind_keyboard_directions = gfx::ScrollDirection::None;

    if (kind_keyboard_directions != gfx::ScrollDirection::None ||
        kind_edge_directions != gfx::ScrollDirection::None) {
      core::Vector2 scroll{0.0f, 0.0f};

      for (const auto& [kind_direction, vec_offset] : kScrollOffsets)
        if (gfx::Includes(kind_keyboard_directions, kind_direction) ||
            gfx::Includes(kind_edge_directions, kind_direction)) {
          scroll.X += vec_offset.X;
          scroll.Y += vec_offset.Y;
        }

      const std::int64_t now = ui::Ui::Instance().RunTime();
      const float delta_scale =
          static_cast<float>(std::min<std::int64_t>(now - int8_last_scroll_time, 25));

      const float length = std::max(1.0f, std::sqrt(scroll.X * scroll.X +
                                                    scroll.Y * scroll.Y));
      scroll.X *= delta_scale / (25 * length) *
                  settings_game.fp4_viewport_edge_scroll_step;
      scroll.Y *= delta_scale / (25 * length) *
                  settings_game.fp4_viewport_edge_scroll_step;

      viewport->Scroll(scroll, false);
      int8_last_scroll_time = now;
    }
  }

  kind_tooltip_type = WorldTooltipType::None;
  vec_actor_tooltip_extra.clear();
  const Modifiers modifiers = ui::Ui::Instance().GetModifierKeys();
  const int2 last_mouse_pos = gfx::Viewport::int2_last_mouse_pos;
  const CPos cell = viewport->ViewToWorld(last_mouse_pos);
  if (!ptr_world->Map().Contains(cell))
    return;

  if (ptr_world->ShroudObscures(cell)) {
    kind_tooltip_type = WorldTooltipType::Unexplored;
    return;
  }

  const int2 world_pixel = viewport->ViewToWorldPx(last_mouse_pos);
  sim::Actor* under_cursor = nullptr;
  {
    std::vector<sim::ActorBoundsPair> at_mouse =
        ptr_world->ScreenMapFace()->ActorsAtMouse(world_pixel);
    std::vector<sim::ActorBoundsPair> eligible;
    for (const sim::ActorBoundsPair& a : at_mouse) {
      if (a.ptr_actor->Info() == nullptr ||
          !a.ptr_actor->Info()->HasTraitInfoOfInterface(
              "OpenRA.Traits.ITooltipInfo"))
        continue;
      if (ptr_world->FogObscures(*a.ptr_actor))
        continue;
      eligible.push_back(a);
    }
    under_cursor =
        sim::WithHighestSelectionPriority(eligible, world_pixel, modifiers);
  }

  if (under_cursor != nullptr) {
    for (sim::ITooltip* tooltip :
         under_cursor->TraitsImplementing<sim::ITooltip>()) {
      const sim::TraitBase* trait_base =
          dynamic_cast<const sim::TraitBase*>(tooltip);
      if (trait_base != nullptr && trait_base->IsTraitDisabled())
        continue;
      ptr_actor_tooltip = tooltip;
      break;
    }
    if (ptr_actor_tooltip != nullptr) {
      vec_actor_tooltip_extra =
          under_cursor->TraitsImplementing<sim::IProvideTooltipInfo>();
      kind_tooltip_type = WorldTooltipType::Actor;
    }

    return;
  }

  sim::FrozenActor* frozen = nullptr;
  {
    std::vector<sim::FrozenActor*> at_mouse = ptr_world->ScreenMapFace()
                                                   ->FrozenActorsAtMouse(
                                                       ptr_world->RenderPlayer(),
                                                       world_pixel);
    std::vector<sim::FrozenActor*> eligible;
    for (sim::FrozenActor* a : at_mouse) {
      if (a->TooltipInfo() == nullptr || !a->IsValid() || !a->Visible() ||
          a->Hidden())
        continue;
      eligible.push_back(a);
    }
    frozen = sim::WithHighestSelectionPriority(eligible, world_pixel, modifiers);
  }

  if (frozen != nullptr) {
    ptr_frozen_actor_tooltip = frozen;

    if (frozen->ActorPtr() != nullptr)
      vec_actor_tooltip_extra =
          frozen->ActorPtr()->TraitsImplementing<sim::IProvideTooltipInfo>();

    kind_tooltip_type = WorldTooltipType::FrozenActor;
    return;
  }

  for (sim::IResourceRenderer* resource_renderer : vec_resource_renderers) {
    const std::string resource_tooltip =
        resource_renderer->GetRenderedResourceTooltip(cell);
    if (!resource_tooltip.empty()) {
      kind_tooltip_type = WorldTooltipType::Resource;
      str_resource_tooltip = resource_tooltip;
      break;
    }
  }
}

const std::string* ViewportControllerWidget::GetCursor(int2 pt_pos) const {
  (void)pt_pos;
  const GameSettingsInputFace settings_game = ui::Ui::Instance().GameSettingsInput();
  gfx::IViewportSurface* viewport =
      ptr_world_renderer != nullptr ? ptr_world_renderer->Viewport() : nullptr;
  if (viewport == nullptr)
    return nullptr;

  if (!(IsJoystickScrolling() || b_is_standard_scrolling) &&
      (!settings_game.b_viewport_edge_scroll ||
       ui::Ui::Instance().MouseOverWidget() != const_cast<ViewportControllerWidget*>(this)))
    return nullptr;

  const gfx::ScrollDirection blocked_directions = viewport->GetBlockedDirections();

  if (IsJoystickScrolling() || b_is_standard_scrolling) {
    for (const auto& [kind_direction, str_cursor] : kJoystickCursors)
      if (gfx::Includes(blocked_directions, kind_direction)) {
        str_cursor_cache = str_cursor;
        return &str_cursor_cache;
      }
    str_cursor_cache = "joystick-all";
    return &str_cursor_cache;
  }

  for (const auto& [kind_direction, str_cursor] : kScrollCursors)
    if (gfx::Includes(kind_edge_directions, kind_direction)) {
      str_cursor_cache =
          std::string(str_cursor) +
          (gfx::Includes(blocked_directions, kind_direction) ? "-blocked" : "");
      return &str_cursor_cache;
    }

  return nullptr;
}

bool ViewportControllerWidget::IsJoystickScrolling() const {
  return opt_joystick_scroll_start.has_value() &&
         opt_joystick_scroll_end.has_value() &&
         (*opt_joystick_scroll_start - *opt_joystick_scroll_end).Length() >
             ui::Ui::Instance().GameSettingsInput().int4_mouse_scroll_deadzone;
}

bool ViewportControllerWidget::HandleMouseInput(const ora::MouseInput& mi_input) {
  gfx::IViewportSurface* viewport =
      ptr_world_renderer != nullptr ? ptr_world_renderer->Viewport() : nullptr;
  if (viewport == nullptr)
    return false;

  const GameSettingsInputFace settings_game = ui::Ui::Instance().GameSettingsInput();

  if (mi_input.Event == MouseInputEvent::Scroll &&
      HasModifier(mi_input.ModifierFlags, settings_game.kind_zoom_modifier)) {
    viewport->AdjustZoom(
        static_cast<float>(mi_input.Delta.Y) * settings_game.fp4_zoom_speed,
        mi_input.Location);
    return true;
  }

  const bool scroll_button_right =
      (settings_game.kind_mouse_control_style == MouseControlStyle::Classic) !=
      settings_game.b_use_alternate_scroll_button;
  const MouseButton scroll_button =
      scroll_button_right ? MouseButton::Right : MouseButton::Middle;
  const MouseScrollType scroll_type =
      (mi_input.Button & scroll_button) == scroll_button
          ? settings_game.kind_mouse_scroll
          : MouseScrollType::Disabled;

  if (scroll_type == MouseScrollType::Disabled)
    return IsJoystickScrolling() || b_is_standard_scrolling;

  if (scroll_type == MouseScrollType::Standard ||
      scroll_type == MouseScrollType::Inverted) {
    if (mi_input.Event == MouseInputEvent::Down && !b_is_standard_scrolling) {
      if (!TakeMouseFocus(mi_input))
        return false;

      opt_standard_scroll_start = mi_input.Location;
    } else if (mi_input.Event == MouseInputEvent::Move &&
               (b_is_standard_scrolling ||
                (opt_standard_scroll_start.has_value() &&
                 ((*opt_standard_scroll_start - mi_input.Location).Length() >
                  settings_game.int4_mouse_scroll_deadzone)))) {
      b_is_standard_scrolling = true;
      const float d = scroll_type == MouseScrollType::Inverted ? -1.0f : 1.0f;
      viewport->Scroll(
          core::Vector2{
              static_cast<float>((gfx::Viewport::int2_last_mouse_pos -
                                  mi_input.Location).X) *
                  d,
              static_cast<float>((gfx::Viewport::int2_last_mouse_pos -
                                  mi_input.Location).Y) *
                  d},
          false);
      return true;
    } else if (mi_input.Event == MouseInputEvent::Up) {
      const bool was_standard_scrolling = b_is_standard_scrolling;
      b_is_standard_scrolling = false;
      opt_standard_scroll_start.reset();
      YieldMouseFocus(mi_input);

      if (was_standard_scrolling)
        return true;
    }
  }

  if (scroll_type == MouseScrollType::Joystick) {
    if (mi_input.Event == MouseInputEvent::Down) {
      if (!TakeMouseFocus(mi_input))
        return false;

      opt_joystick_scroll_start = mi_input.Location;
    }

    if (mi_input.Event == MouseInputEvent::Up) {
      const bool was_joystick_scrolling = IsJoystickScrolling();

      opt_joystick_scroll_start.reset();
      opt_joystick_scroll_end.reset();
      YieldMouseFocus(mi_input);

      if (was_joystick_scrolling)
        return true;
    }

    if (mi_input.Event == MouseInputEvent::Move) {
      if (!opt_joystick_scroll_start.has_value())
        opt_joystick_scroll_start = mi_input.Location;

      opt_joystick_scroll_end = mi_input.Location;
    }
  }

  return IsJoystickScrolling() || b_is_standard_scrolling;
}

bool ViewportControllerWidget::YieldMouseFocus(const ora::MouseInput& mi_input) {
  opt_joystick_scroll_start.reset();
  opt_joystick_scroll_end.reset();
  return ora::ui::Widget::YieldMouseFocus(mi_input);
}

bool ViewportControllerWidget::YieldKeyboardFocus() {
  kind_keyboard_directions = gfx::ScrollDirection::None;
  return ora::ui::Widget::YieldKeyboardFocus();
}

bool ViewportControllerWidget::HandleMapScrollKey(
    const ui::HotkeyReference& hotkey, gfx::ScrollDirection scroll_direction,
    const ora::KeyInput& e_input) {
  const ui::Hotkey key = ui::Hotkey::FromKeyInput(e_input);

  bool is_hotkey = false;
  const ui::Hotkey key_value = hotkey.GetValue();
  if (key.key_kind == key_value.key_kind) {
    is_hotkey = key == key_value;
    kind_keyboard_directions = gfx::SetDirection(
        kind_keyboard_directions, scroll_direction,
        e_input.Event == KeyInputEvent::Down &&
            (is_hotkey || key_value.kind_modifiers == Modifiers::None));
  }

  return is_hotkey;
}

bool ViewportControllerWidget::HandleKeyPress(const ora::KeyInput& e_input) {
  gfx::IViewportSurface* viewport =
      ptr_world_renderer != nullptr ? ptr_world_renderer->Viewport() : nullptr;
  if (viewport == nullptr)
    return false;

  if (HandleMapScrollKey(hk_scroll_up, gfx::ScrollDirection::Up, e_input) ||
      HandleMapScrollKey(hk_scroll_down, gfx::ScrollDirection::Down, e_input) ||
      HandleMapScrollKey(hk_scroll_left, gfx::ScrollDirection::Left, e_input) ||
      HandleMapScrollKey(hk_scroll_right, gfx::ScrollDirection::Right, e_input))
    return true;

  if (e_input.Event != KeyInputEvent::Down)
    return false;

  if (hk_zoom_in.IsActivatedBy(e_input)) {
    viewport->AdjustZoom(0.25f);
    return true;
  }

  if (hk_zoom_out.IsActivatedBy(e_input)) {
    viewport->AdjustZoom(-0.25f);
    return true;
  }

  if (hk_jump_to_top_edge.IsActivatedBy(e_input)) {
    viewport->Center(WPos{viewport->CenterPosition().X, 0, 0});
    return true;
  }

  if (hk_jump_to_bottom_edge.IsActivatedBy(e_input)) {
    viewport->Center(
        WPos{viewport->CenterPosition().X,
             ptr_world_renderer->World().Map().ProjectedBottomRight().Y, 0});
    return true;
  }

  if (hk_jump_to_left_edge.IsActivatedBy(e_input)) {
    viewport->Center(WPos{0, viewport->CenterPosition().Y, 0});
    return true;
  }

  if (hk_jump_to_right_edge.IsActivatedBy(e_input)) {
    viewport->Center(
        WPos{ptr_world_renderer->World().Map().ProjectedBottomRight().X,
             viewport->CenterPosition().Y, 0});
    return true;
  }

  for (std::size_t i = 0; i < vec_save_bookmark_hotkeys.size(); i++) {
    if (vec_save_bookmark_hotkeys[i].IsActivatedBy(e_input)) {
      vec_bookmark_positions[i] = viewport->CenterPosition();
      return true;
    }
  }

  for (std::size_t i = 0; i < vec_restore_bookmark_hotkeys.size(); i++) {
    if (vec_restore_bookmark_hotkeys[i].IsActivatedBy(e_input)) {
      if (vec_bookmark_positions[i].has_value()) {
        viewport->Center(*vec_bookmark_positions[i]);
        return true;
      }
    }
  }

  return ptr_world->OrderGenerator()->HandleKeyPress(e_input);
}

gfx::ScrollDirection ViewportControllerWidget::CheckForDirections() const {
  const std::int32_t margin =
      ui::Ui::Instance().GameSettingsInput().int4_viewport_edge_scroll_margin;
  gfx::ScrollDirection directions = gfx::ScrollDirection::None;
  const int2 resolution = ui::Ui::Instance().Resolution();
  if (gfx::Viewport::int2_last_mouse_pos.X < margin)
    directions = directions | gfx::ScrollDirection::Left;
  if (gfx::Viewport::int2_last_mouse_pos.Y < margin)
    directions = directions | gfx::ScrollDirection::Up;
  if (gfx::Viewport::int2_last_mouse_pos.X >= resolution.X - margin)
    directions = directions | gfx::ScrollDirection::Right;
  if (gfx::Viewport::int2_last_mouse_pos.Y >= resolution.Y - margin)
    directions = directions | gfx::ScrollDirection::Down;

  return directions;
}

void ViewportControllerWidget::Removed() {
  if (uint8_viewport_tick_token != 0 && ptr_world_renderer != nullptr &&
      ptr_world_renderer->Viewport() != nullptr) {
    ptr_world_renderer->Viewport()->UnsubscribeViewportTick(
        uint8_viewport_tick_token);
    uint8_viewport_tick_token = 0;
  }
  ora::ui::Widget::Removed();
}

void RegisterViewportControllerWidget() {
  static const bool b_registered = [] {
    ui::WidgetTypeRegistry::Instance().Register(
        "ViewportControllerWidget",
        [](ui::WidgetArgs& args_args) -> std::unique_ptr<ui::Widget> {
          return std::make_unique<ViewportControllerWidget>(args_args);
        });
    return true;
  }();
  (void)b_registered;
}

}  // namespace ora::mods
