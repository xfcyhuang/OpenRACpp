#pragma once
import std;

#include "core/color.hpp"
#include "core/int2.hpp"
#include "game/input.hpp"
#include "ui/widget.hpp"

namespace ora::sim {
class World;
}

namespace ora::gfx {
class WorldRenderer;
}

namespace ora::mods {

class UnitOrderGenerator;

class WorldInteractionControllerWidget : public ora::ui::Widget {
 public:
  static constexpr std::string_view kWidgetTypeName =
      "WorldInteractionControllerWidget";

  WorldInteractionControllerWidget(ora::ui::WidgetArgs& args_args);
  WorldInteractionControllerWidget(const WorldInteractionControllerWidget& widget_other);
  ~WorldInteractionControllerWidget() override = default;

  std::unique_ptr<ora::ui::Widget> Clone() override {
    return std::unique_ptr<ora::ui::Widget>(
        new WorldInteractionControllerWidget(*this));
  }
  std::string_view TypeName() const override { return kWidgetTypeName; }

  void Draw() override;
  bool HandleMouseInput(const ora::MouseInput& mi_input) override;
  const std::string* GetCursor(int2 pt_pos) const override;
  bool HandleKeyPress(const ora::KeyInput& e_input) override;

  sim::World* ptr_world = nullptr;
  gfx::WorldRenderer* ptr_world_renderer = nullptr;

  std::string str_click_sound;
  std::string str_click_disabled_sound;

private:
  void ApplyOrders(sim::World& world, const ora::MouseInput& mi_input);
  bool IsValidDragbox() const;

  core::Color color_normal_selection;
  core::Color color_alt_selection;
  core::Color color_ctrl_selection;
  GameSettingsFace game_settings_{};

  int2 int2_drag_start{};
  int2 int2_mouse_pos{};
  bool b_is_dragging = false;

  mutable std::string str_cursor_cache;
};

void RegisterWorldInteractionControllerWidget();

}  // namespace ora::mods
