// UPSTREAM: OpenRA.Mods.Common/Widgets/SliderWidget.cs @1f594e320 L20-146(全文)。
// 左键拖拽取值(ValueFromPx/PxFromValue 的 thumb 半宽补偿)/UpdateValue 的
// Clamp + OnChange 变更通知/thumb 命中即消费;Draw 的刻度条(GetImage 直
// 取)/轨道九宫格/ButtonWidget.DrawBackground 静态 thumb。ChromeProvider →
// Ui::Instance() 注入承载。
// The whole of SliderWidget.cs L20-146. The left-drag value taking
// (ValueFromPx/PxFromValue's thumb half-width compensation) / UpdateValue's
// Clamp + OnChange change notification / consuming while over the thumb;
// Draw's tick marks (GetImage direct) / the nine-slice track /
// ButtonWidget.DrawBackground's static thumb. ChromeProvider goes through
// the Ui::Instance() injected carrier.
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
  /// ValueFromPx(L89-92;thumb 半宽补偿)。
  /// ValueFromPx (L89-92; the thumb half-width compensation).
  virtual float ValueFromPx(int int4_x) const;

  /// PxFromValue(L94-97)。
  /// PxFromValue (L94-97).
  virtual int PxFromValue(float fp4_x) const;

  /// ThumbRect(L107-121);测试可见面。
  /// ThumbRect (L107-121); the test-visible face.
  Rectangle ThumbRect() const;

  bool b_is_moving_ = false;
};

}  // namespace ora::ui
