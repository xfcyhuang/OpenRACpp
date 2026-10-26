// UPSTREAM: OpenRA.Mods.Common/Widgets/ColorMixerWidget.cs @1f594e320
// (color_mixer_widget.hpp 的实现)
// The implementation of color_mixer_widget.hpp.
import std;

#include "ui/widgets/color_mixer_widget.hpp"

#include "core/vector_n.hpp"
#include "gfx/chrome_provider.hpp"
#include "gfx/renderer.hpp"
#include "game/mod_data.hpp"
#include "game/ruleset.hpp"
#include "ui/chrome_metrics.hpp"
#include "ui/ui.hpp"
#include "ui/widget_utils.hpp"

namespace ora::ui {

ColorMixerWidget::ColorMixerWidget(WidgetArgs& args_args) {
  if (const WidgetArgValue* ptr_value = args_args.TryGet("modData"))
    if (const auto* ptr_mod = std::get_if<game::ModData*>(ptr_value))
      mod_rules = &(*ptr_mod)->DefaultRules();

  if (ChromeMetrics* ptr_metrics = Ui::HasInstance()
                                       ? Ui::Instance().ChromeMetricsDeps()
                                       : nullptr)
    str_click_sound = ptr_metrics->Get("ClickSound");
}

ColorMixerWidget::ColorMixerWidget(const ColorMixerWidget& widget_other)
    : Widget(widget_other),
      str_click_sound(widget_other.str_click_sound),
      vec_on_change(widget_other.vec_on_change),
      mod_rules(widget_other.mod_rules),
      fp4_h(widget_other.fp4_h),
      fp4_s(widget_other.fp4_s),
      fp4_v(widget_other.fp4_v),
      fp4_min_sat(widget_other.fp4_min_sat),
      fp4_max_sat(widget_other.fp4_max_sat),
      fp4_min_val(widget_other.fp4_min_val),
      fp4_max_val(widget_other.fp4_max_val) {}

void ColorMixerWidget::SetColorLimits(float fp4_min_saturation,
                                      float fp4_max_saturation,
                                      float fp4_min_value, float fp4_max_value,
                                      std::optional<float> opt_new_hue) {
  fp4_min_sat = fp4_min_saturation;
  fp4_max_sat = fp4_max_saturation;
  fp4_min_val = fp4_min_value;
  fp4_max_val = fp4_max_value;

  const float fp4_hue = opt_new_hue.value_or(fp4_h);
  const std::span<std::byte> vec_data = up_mixer_sheet_->GetData();
  for (int int4_v = 0; int4_v < 256; int4_v++) {
    for (int int4_s = 0; int4_s < 256; int4_s++) {
      const std::uint32_t uint4_pixel =
          core::Color::FromAhsv(fp4_hue, 1.0f - int4_s / 255.0f,
                                int4_v / 255.0f)
              .ToArgb();
      std::memcpy(vec_data.data() +
                      4 * (static_cast<std::size_t>(int4_s) * 256 +
                           static_cast<std::size_t>(int4_v)),
                  &uint4_pixel, sizeof(uint4_pixel));
    }
  }
  up_mixer_sheet_->CommitBufferedData();

  const Rectangle rect_bounds{
      static_cast<int>(255 * fp4_min_val),
      static_cast<int>(255 * (1 - fp4_max_sat)),
      static_cast<int>(255 * (fp4_max_val - fp4_min_val)),
      static_cast<int>(255 * (fp4_max_sat - fp4_min_sat)) + 1};
  sprite_mixer_ = gfx::Sprite(*up_mixer_sheet_, rect_bounds,
                              gfx::TextureChannel::RGBA);
}

void ColorMixerWidget::Initialize(WidgetArgs& args_args) {
  Widget::Initialize(args_args);

  gfx::Renderer* ptr_renderer = Ui::Instance().RendererDeps();
  up_mixer_sheet_ = std::make_unique<gfx::Sheet>(
      gfx::SheetType::BGRA, int2{256, 256},
      ptr_renderer != nullptr ? &ptr_renderer->render() : nullptr);
  SetColorLimits(fp4_min_sat, fp4_max_sat, fp4_min_val, fp4_max_val);
}

void ColorMixerWidget::Draw() {
  const int2 int2_ro = RenderOrigin();
  const Rectangle rect_rb = RenderBounds();
  DrawSprite(sprite_mixer_, core::Vector2{static_cast<float>(int2_ro.X),
                                          static_cast<float>(int2_ro.Y)},
             int2{rect_rb.Width, rect_rb.Height});

  gfx::ChromeProvider* ptr_chrome = ChromeProviderOrNull();
  const gfx::Sprite sprite_picker =
      ptr_chrome != nullptr ? ptr_chrome->GetImage("lobby-bits", "colorpicker")
                            : gfx::Sprite{};
  const int2 int2_px = MixerPxFromValue();
  const int2 int2_pos =
      int2_ro + int2_px -
      int2{static_cast<int>(sprite_picker.vec_size.X),
           static_cast<int>(sprite_picker.vec_size.Y)} / 2;
  FillEllipseWithColor(Rectangle{int2_pos.X + 1, int2_pos.Y + 1,
                                 static_cast<int>(sprite_picker.vec_size.X) - 2,
                                 static_cast<int>(sprite_picker.vec_size.Y) - 2},
                       ColorValue());
  DrawSprite(sprite_picker,
             core::Vector2{static_cast<float>(int2_pos.X),
                           static_cast<float>(int2_pos.Y)});
}

void ColorMixerWidget::SetValueFromPx(int2 int2_xy) {
  const Rectangle rect_rb = RenderBounds();
  const float fp4_v_new = fp4_min_val +
                          (fp4_max_val - fp4_min_val) *
                              (static_cast<float>(int2_xy.X) /
                               static_cast<float>(rect_rb.Width));
  const float fp4_s_new =
      fp4_min_sat + (fp4_max_sat - fp4_min_sat) *
                        (1.0f - static_cast<float>(int2_xy.Y) /
                                    static_cast<float>(rect_rb.Height));
  fp4_v = std::clamp(fp4_v_new, fp4_min_val, fp4_max_val);
  fp4_s = std::clamp(fp4_s_new, fp4_min_sat, fp4_max_sat);
}

int2 ColorMixerWidget::MixerPxFromValue() const {
  const Rectangle rect_rb = RenderBounds();
  const float fp4_x =
      static_cast<float>(rect_rb.Width) * (fp4_v - fp4_min_val) /
      (fp4_max_val - fp4_min_val);
  const float fp4_y = static_cast<float>(rect_rb.Height) *
                      (1 - (fp4_s - fp4_min_sat) / (fp4_max_sat - fp4_min_sat));
  return int2{
      static_cast<int>(std::clamp(fp4_x, 0.0f, static_cast<float>(rect_rb.Width))),
      static_cast<int>(std::clamp(fp4_y, 0.0f, static_cast<float>(rect_rb.Height)))};
}

bool ColorMixerWidget::HandleMouseInput(const MouseInput& mi_input) {
  if (mi_input.Button != MouseButton::Left)
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
      SetValueFromPx(mi_input.Location - RenderOrigin());
      NotifyOnChange();

      Ui::Instance().PlayNotification(mod_rules, str_click_sound);
      break;

    case MouseInputEvent::Move:
      if (b_is_moving_) {
        SetValueFromPx(mi_input.Location - RenderOrigin());
        NotifyOnChange();
      }
      break;

    default:
      break;
  }

  return true;
}

void ColorMixerWidget::Set(core::Color color_value) {
  const auto [int4_a, fp4_h_new, fp4_s_new, fp4_v_new] = color_value.ToAhsv();
  (void)int4_a;

  if (fp4_h != fp4_h_new || fp4_s != fp4_s_new || fp4_v != fp4_v_new) {
    fp4_h = fp4_h_new;
    fp4_s = std::clamp(fp4_s_new, fp4_min_sat, fp4_max_sat);
    fp4_v = std::clamp(fp4_v_new, fp4_min_val, fp4_max_val);
    NotifyOnChange();
  }
}

void ColorMixerWidget::Removed() {
  up_mixer_sheet_.reset();
  Widget::Removed();
}

}  // namespace ora::ui
