import std;

#include "ui/widgets/slider_widget.hpp"

#include "core/vector_n.hpp"
#include "gfx/chrome_provider.hpp"
#include "gfx/viewport.hpp"
#include "meta/field_loader.hpp"
#include "ui/ui.hpp"
#include "ui/widget_loader.hpp"
#include "ui/widget_utils.hpp"
#include "ui/widgets/button_widget.hpp"

namespace ora::ui {

SliderWidget::SliderWidget(const SliderWidget& widget_other)
    : InputWidget(widget_other),
      int4_ticks(widget_other.int4_ticks),
      int4_track_height(widget_other.int4_track_height),
      str_thumb(widget_other.str_thumb),
      str_track(widget_other.str_track),
      fp4_minimum_value(widget_other.fp4_minimum_value),
      fp4_maximum_value(widget_other.fp4_maximum_value),
      fp4_value(widget_other.fp4_value),
      fn_get_value(widget_other.fn_get_value),
      fn_on_change(widget_other.fn_on_change) {}

void SliderWidget::UpdateValue(float fp4_new_value) {
  const float fp4_old_value = fp4_value;
  fp4_value = std::clamp(fp4_new_value, fp4_minimum_value, fp4_maximum_value);
  if (fp4_old_value != fp4_value)
    fn_on_change(fp4_value);
}

bool SliderWidget::HandleMouseInput(const MouseInput& mi_input) {
  if (mi_input.Button != MouseButton::Left)
    return false;
  if (fn_is_disabled())
    return false;
  if (mi_input.Event == MouseInputEvent::Down && !TakeMouseFocus(mi_input))
    return false;
  if (!HasMouseFocus())
    return false;

  switch (mi_input.Event) {
    case MouseInputEvent::Up:
      b_is_moving_ = false;
      YieldMouseFocus(mi_input);
      break;

    case MouseInputEvent::Down:
      b_is_moving_ = true;

      UpdateValue(ValueFromPx(mi_input.Location.X - RenderBounds().Left()));
      break;

    case MouseInputEvent::Move:
      if (b_is_moving_)
        UpdateValue(ValueFromPx(mi_input.Location.X - RenderBounds().Left()));
      break;

    default:
      break;
  }

  return ThumbRect().Contains(mi_input.Location);
}

float SliderWidget::ValueFromPx(int int4_x) const {
  const Rectangle rect_rb = RenderBounds();
  return fp4_minimum_value +
         (fp4_maximum_value - fp4_minimum_value) *
             (static_cast<float>(int4_x) - 0.5f * static_cast<float>(rect_rb.Height)) /
             static_cast<float>(rect_rb.Width - rect_rb.Height);
}

int SliderWidget::PxFromValue(float fp4_x) const {
  const Rectangle rect_rb = RenderBounds();
  return static_cast<int>(0.5f * static_cast<float>(rect_rb.Height) +
                          static_cast<float>(rect_rb.Width - rect_rb.Height) *
                              (fp4_x - fp4_minimum_value) /
                              (fp4_maximum_value - fp4_minimum_value));
}

Rectangle SliderWidget::ThumbRect() const {
  const int int4_thumb_pos = PxFromValue(fp4_value);
  const Rectangle rect_rb = RenderBounds();
  const int int4_width = rect_rb.Height;
  const int int4_height = rect_rb.Height;
  const int int4_origin =
      static_cast<int>(static_cast<float>(rect_rb.X) +
                       static_cast<float>(int4_thumb_pos) - static_cast<float>(int4_width) / 2.0f);
  return Rectangle{int4_origin, rect_rb.Y, int4_width, int4_height};
}

bool SliderWidget::LoadFieldOrProperty(std::string_view str_key,
                                       std::string_view str_value) {
  if (str_key == "Ticks") {
    int4_ticks = meta::GetInt32Value("Ticks", str_value);
    return true;
  }
  if (str_key == "TrackHeight") {
    int4_track_height = meta::GetInt32Value("TrackHeight", str_value);
    return true;
  }
  if (str_key == "Thumb") {
    str_thumb = std::string{str_value};
    return true;
  }
  if (str_key == "Track") {
    str_track = std::string{str_value};
    return true;
  }
  if (str_key == "MinimumValue") {
    fp4_minimum_value = meta::GetFloatValue("MinimumValue", str_value);
    return true;
  }
  if (str_key == "MaximumValue") {
    fp4_maximum_value = meta::GetFloatValue("MaximumValue", str_value);
    return true;
  }
  if (str_key == "Value") {
    fp4_value = meta::GetFloatValue("Value", str_value);
    return true;
  }

  return InputWidget::LoadFieldOrProperty(str_key, str_value);
}

void SliderWidget::Draw() {
  if (!fn_is_visible())
    return;

  UpdateValue(fn_get_value());

  const Rectangle rect_thumb = ThumbRect();
  const Rectangle rect_rb = RenderBounds();
  const int int4_track_width = rect_rb.Width - rect_rb.Height;
  const int int4_track_origin = rect_rb.X + rect_rb.Height / 2;
  const Rectangle rect_track{int4_track_origin - 1,
                             rect_rb.Y + (rect_rb.Height - int4_track_height) / 2,
                             int4_track_width + 2, int4_track_height};

  gfx::ChromeProvider* ptr_chrome = ChromeProviderOrNull();
  const gfx::Sprite sprite_tick =
      ptr_chrome != nullptr ? ptr_chrome->GetImage("slider", "tick") : gfx::Sprite{};
  for (int int4_i = 0; int4_i < int4_ticks; ++int4_i) {
    const core::Vector2 vec_tick_pos{
        static_cast<float>(int4_track_origin +
                           int4_i * (rect_track.Width - static_cast<int>(sprite_tick.vec_size.X)) /
                               (int4_ticks - 1) -
                           static_cast<int>(sprite_tick.vec_size.X / 2)),
        static_cast<float>(rect_track.Bottom())};
    DrawSprite(sprite_tick, vec_tick_pos);
  }

  DrawPanel(str_track, rect_track);

  const bool b_thumb_hover = Ui::Instance().MouseOverWidget() == this &&
                             rect_thumb.Contains(gfx::Viewport::int2_last_mouse_pos);
  ButtonWidget::DrawBackground(str_thumb, rect_thumb, fn_is_disabled(), b_is_moving_,
                               b_thumb_hover, false);
}

}
