#pragma once
import std;

#include "core/int2.hpp"
#include "core/wpos.hpp"
#include "gfx/world_renderer.hpp"
#include "sim/trait_interfaces.hpp"
#include "ui/hotkey.hpp"
#include "ui/widget.hpp"

namespace ora::game {
class ModData;
}

namespace ora::sim {
class World;
class FrozenActor;
class ITooltip;
}

namespace ora::gfx {
class WorldRenderer;
}

namespace ora::ui {
class TooltipContainerWidget;
}

namespace ora::mods {

enum class WorldTooltipType : std::uint8_t {
  None,
  Unexplored,
  Actor,
  FrozenActor,
  Resource,
};

class ViewportControllerWidget : public ora::ui::Widget {
 public:
  static constexpr std::string_view kWidgetTypeName = "ViewportControllerWidget";

  ViewportControllerWidget(ora::ui::WidgetArgs& args_args);
  ViewportControllerWidget(const ViewportControllerWidget& widget_other);
  ~ViewportControllerWidget() override;

  std::unique_ptr<ora::ui::Widget> Clone() override {
    return std::unique_ptr<ora::ui::Widget>(new ViewportControllerWidget(*this));
  }
  std::string_view TypeName() const override { return kWidgetTypeName; }

  bool LoadFieldOrProperty(std::string_view str_key,
                           std::string_view str_value) override;
  void Initialize(ora::ui::WidgetArgs& args_args) override;
  void MouseEntered() override;
  void MouseExited() override;
  void UpdateCameraTick();
  const std::string* GetCursor(int2 pt_pos) const override;
  bool HandleMouseInput(const ora::MouseInput& mi_input) override;
  bool YieldMouseFocus(const ora::MouseInput& mi_input) override;
  bool YieldKeyboardFocus() override;
  bool HandleKeyPress(const ora::KeyInput& e_input) override;
  void Removed() override;

  game::ModData* ptr_mod_data = nullptr;
  sim::World* ptr_world = nullptr;
  gfx::WorldRenderer* ptr_world_renderer = nullptr;

  ui::HotkeyReference hk_zoom_in;
  ui::HotkeyReference hk_zoom_out;
  ui::HotkeyReference hk_scroll_up;
  ui::HotkeyReference hk_scroll_down;
  ui::HotkeyReference hk_scroll_left;
  ui::HotkeyReference hk_scroll_right;
  ui::HotkeyReference hk_jump_to_top_edge;
  ui::HotkeyReference hk_jump_to_bottom_edge;
  ui::HotkeyReference hk_jump_to_left_edge;
  ui::HotkeyReference hk_jump_to_right_edge;

  std::string str_bookmark_save_key_prefix;
  std::string str_bookmark_restore_key_prefix;
  std::int32_t int4_bookmark_key_count = 0;

  std::string str_tooltip_template = "WORLD_TOOLTIP";
  std::string str_tooltip_container;

  WorldTooltipType kind_tooltip_type = WorldTooltipType::None;
  sim::ITooltip* ptr_actor_tooltip = nullptr;
  std::vector<sim::IProvideTooltipInfo*> vec_actor_tooltip_extra;
  sim::FrozenActor* ptr_frozen_actor_tooltip = nullptr;
  std::string str_resource_tooltip;

private:
  bool IsJoystickScrolling() const;
  ora::gfx::ScrollDirection CheckForDirections() const;
  void MaterializeTooltipContainer();
  bool HandleMapScrollKey(const ui::HotkeyReference& hotkey,
                          ora::gfx::ScrollDirection scroll_direction,
                          const ora::KeyInput& e_input);

  std::vector<sim::IResourceRenderer*> vec_resource_renderers;

  ui::TooltipContainerWidget* ptr_tooltip_container_widget = nullptr;
  bool b_tooltip_container_materialized = false;

  std::optional<int2> opt_joystick_scroll_start;
  std::optional<int2> opt_joystick_scroll_end;
  std::optional<int2> opt_standard_scroll_start;
  bool b_is_standard_scrolling = false;

  ora::gfx::ScrollDirection kind_keyboard_directions =
      ora::gfx::ScrollDirection::None;
  ora::gfx::ScrollDirection kind_edge_directions = ora::gfx::ScrollDirection::None;

  std::vector<ui::HotkeyReference> vec_save_bookmark_hotkeys;
  std::vector<ui::HotkeyReference> vec_restore_bookmark_hotkeys;
  std::vector<std::optional<WPos>> vec_bookmark_positions;

  std::int64_t int8_last_scroll_time = 0;
  std::uint64_t uint8_viewport_tick_token = 0;
  mutable std::string str_cursor_cache;
};

void RegisterViewportControllerWidget();

}
