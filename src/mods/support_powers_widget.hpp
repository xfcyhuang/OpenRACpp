#pragma once
import std;

#include "core/int2.hpp"
#include "core/rectangle.hpp"
#include "gfx/animation.hpp"
#include "gfx/sprite_font.hpp"
#include "mods/support_powers.hpp"
#include "sim/trait_interfaces.hpp"
#include "ui/hotkey.hpp"
#include "ui/widget.hpp"

namespace ora::gfx {
class WorldRenderer;
}

namespace ora::ui {
class TooltipContainerWidget;
}

namespace ora::mods {

struct SupportPowerIcon {
  SupportPowerInstance* ptr_power = nullptr;
  core::Vector2 pos;
  gfx::Sprite sprite;
  gfx::PaletteReference* ptr_palette = nullptr;
  gfx::PaletteReference* ptr_icon_clock_palette = nullptr;
  ui::HotkeyReference hotkey;
  bool b_has_hotkey = false;
};

class SupportPowersWidget : public ora::ui::Widget {
 public:
  static constexpr std::string_view kWidgetTypeName = "SupportPowersWidget";

  SupportPowersWidget(ora::ui::WidgetArgs& args_args);
  SupportPowersWidget(const SupportPowersWidget& widget_other);
  ~SupportPowersWidget() override = default;

  std::unique_ptr<ora::ui::Widget> Clone() override {
    return std::unique_ptr<ora::ui::Widget>(
        new SupportPowersWidget(*this));
  }
  std::string_view TypeName() const override { return kWidgetTypeName; }

  bool LoadFieldOrProperty(std::string_view str_key,
                           std::string_view str_value) override;
  void Initialize(ora::ui::WidgetArgs& args_args) override;

  void Tick() override;
  void Draw() override;
  void MouseEntered() override;
  void MouseExited() override;
  bool HandleKeyPress(const ora::KeyInput& e_input) override;
  bool HandleMouseInput(const ora::MouseInput& mi_input) override;

  std::string str_ready_text;
  std::string str_hold_text;

  std::string str_overlay_font{"TinyBold"};

  int2 int2_icon_size{64, 48};
  int int4_icon_margin = 10;
  int2 int2_icon_sprite_offset{0, 0};

  std::string str_tooltip_container;
  std::string str_tooltip_template{"SUPPORT_POWER_TOOLTIP"};

  std::string str_hotkey_prefix;
  int int4_hotkey_count = 0;

  std::string str_clock_animation{"clock"};
  std::string str_clock_sequence{"idle"};
  std::string str_clock_palette{"chrome"};

  bool b_horizontal = false;

  int IconCount() const { return int4_icon_count_; }
  std::function<void(int, int)> fn_on_icon_count_changed = [](int, int) {};

  SupportPowerIcon* TooltipIcon() const { return ptr_tooltip_icon_; }
  std::function<SupportPowerIcon*()> fn_get_tooltip_icon;

  const std::vector<std::pair<Rectangle, SupportPowerIcon>>& IconsForTest()
      const {
    return vec_icons_;
  }

 protected:
  void ClickIcon(SupportPowerIcon& clicked);

  sim::World* ptr_world_ = nullptr;
  gfx::WorldRenderer* ptr_world_renderer_ = nullptr;
  SupportPowerManager* ptr_spm_ = nullptr;

  SupportPowerIcon* ptr_tooltip_icon_ = nullptr;
  ora::ui::TooltipContainerWidget* ptr_tooltip_container_widget_ = nullptr;
  bool b_tooltip_container_materialized_ = false;
  std::vector<ui::HotkeyReference> vec_hotkeys_;
  std::vector<std::pair<Rectangle, SupportPowerIcon>> vec_icons_;
  std::unique_ptr<gfx::Animation> ptr_clock_;
  Rectangle rect_event_bounds_ = Rectangle::Empty();
  int int4_icon_count_ = 0;

  gfx::SpriteFont* ptr_overlay_font_ = nullptr;
  core::Vector2 vec_icon_offset_;
  core::Vector2 vec_hold_offset_;
  core::Vector2 vec_ready_offset_;
  core::Vector2 vec_time_offset_;
};

void RegisterSupportPowersWidget();

}
