#pragma once
import std;

#include "core/color.hpp"
#include "core/int2.hpp"
#include "gfx/sheet.hpp"
#include "ui/widget.hpp"

namespace ora::game {
class Ruleset;
}

namespace ora::ui {

class ColorMixerWidget : public Widget {
 public:
  static constexpr std::string_view kWidgetTypeName = "ColorMixerWidget";

  explicit ColorMixerWidget(WidgetArgs& args_args);
  explicit ColorMixerWidget(const ColorMixerWidget& widget_other);
  ~ColorMixerWidget() override = default;

  std::unique_ptr<Widget> Clone() override {
    return std::unique_ptr<Widget>(new ColorMixerWidget(*this));
  }
  std::string_view TypeName() const override { return kWidgetTypeName; }

  std::string str_click_sound;
  std::vector<std::function<void()>> vec_on_change{{[] {}}};
  const game::Ruleset* mod_rules = nullptr;

  float fp4_h = 0.0f;
  float fp4_s = 0.0f;
  float fp4_v = 0.0f;

  void OnChangeAdd(std::function<void()> fn_handler) {
    vec_on_change.push_back(std::move(fn_handler));
  }

  void SetColorLimits(float fp4_min_saturation, float fp4_max_saturation,
                      float fp4_min_value, float fp4_max_value,
                      std::optional<float> opt_new_hue = std::nullopt);

  void Initialize(WidgetArgs& args_args) override;
  void Draw() override;
  void Removed() override;

  bool HandleMouseInput(const MouseInput& mi_input) override;

  core::Color ColorValue() const {
    return core::Color::FromAhsv(fp4_h, fp4_s, fp4_v);
  }

  void Set(core::Color color_value);

  int2 MixerPxFromValue() const;

 private:
  void SetValueFromPx(int2 int2_xy);
  void NotifyOnChange() {
    const std::vector<std::function<void()>> vec_snapshot = vec_on_change;
    for (const auto& fn_handler : vec_snapshot)
      fn_handler();
  }

  float fp4_min_sat = 0.0f;
  float fp4_max_sat = 0.0f;
  float fp4_min_val = 0.0f;
  float fp4_max_val = 0.0f;

  std::unique_ptr<gfx::Sheet> up_mixer_sheet_;
  gfx::Sprite sprite_mixer_;
  bool b_is_moving_ = false;
};

}
