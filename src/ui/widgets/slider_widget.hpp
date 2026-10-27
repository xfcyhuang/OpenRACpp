#pragma once
import std;

#include "core/color.hpp"
#include "core/int2.hpp"
#include "core/rectangle.hpp"
#include "ui/widget.hpp"

namespace ora::ui {

class SliderWidget : public InputWidget {
 public:
  static constexpr std::string_view kWidgetTypeName = "SliderWidget";

  SliderWidget() : InputWidget() { fn_get_value = [this] { return fp4_value; }; }
  explicit SliderWidget(WidgetArgs& args_args) : SliderWidget() { (void)args_args; }
  explicit SliderWidget(const SliderWidget& widget_other);
  ~SliderWidget() override = default;

  std::unique_ptr<Widget> Clone() override {
    return std::unique_ptr<Widget>(new SliderWidget(*this));
  }
  std::string_view TypeName() const override { return kWidgetTypeName; }

  int int4_ticks = 0;
  int int4_track_height = 5;
  std::string str_thumb = "slider-thumb";
  std::string str_track = "slider-track";
  float fp4_minimum_value = 0.0f;
  float fp4_maximum_value = 1.0f;
  float fp4_value = 0.0f;
  std::function<float()> fn_get_value;

  std::function<void(float)> fn_on_change = [](float) {};

  void UpdateValue(float fp4_new_value);

  bool HandleMouseInput(const MouseInput& mi_input) override;

  bool LoadFieldOrProperty(std::string_view str_key,
                           std::string_view str_value) override;

  void Draw() override;

 protected:

  virtual float ValueFromPx(int int4_x) const;

  virtual int PxFromValue(float fp4_x) const;

  Rectangle ThumbRect() const;

  bool b_is_moving_ = false;
};

}
