// UPSTREAM: OpenRA.Mods.Common/Widgets/HueSliderWidget.cs @e180e3225
// (hue_slider_widget.hpp 的实现)
// The implementation of hue_slider_widget.hpp.
import std;

#include "ui/widgets/hue_slider_widget.hpp"

#include "core/color.hpp"
#include "core/vector_n.hpp"
#include "gfx/chrome_provider.hpp"
#include "gfx/renderer.hpp"
#include "ui/ui.hpp"
#include "ui/widget_utils.hpp"

namespace ora::ui {

void HueSliderWidget::Initialize(WidgetArgs& args_args) {
  SliderWidget::Initialize(args_args);

  gfx::Renderer* ptr_renderer = Ui::Instance().RendererDeps();
  up_hue_sheet_ = std::make_unique<gfx::Sheet>(
      gfx::SheetType::BGRA, int2{256, 1},
      ptr_renderer != nullptr ? &ptr_renderer->render() : nullptr);

  const std::span<std::byte> vec_data = up_hue_sheet_->GetData();
  for (int int4_h = 0; int4_h < 256; int4_h++) {
    const std::uint32_t uint4_pixel =
        core::Color::FromAhsv(int4_h / 255.0f, 1.0f, 1.0f).ToArgb();
    std::memcpy(vec_data.data() + 4 * static_cast<std::size_t>(int4_h),
                &uint4_pixel, sizeof(uint4_pixel));
  }
  up_hue_sheet_->CommitBufferedData();

  sprite_hue_ = gfx::Sprite(*up_hue_sheet_, Rectangle{0, 0, 256, 1},
                            gfx::TextureChannel::RGBA);

  gfx::ChromeProvider* ptr_chrome = ChromeProviderOrNull();
  sprite_picker_ =
      ptr_chrome != nullptr ? ptr_chrome->GetImage("lobby-bits", "huepicker")
                            : gfx::Sprite{};
}

void HueSliderWidget::Draw() {
  if (!fn_is_visible())
    return;

  const int2 int2_ro = RenderOrigin();
  const Rectangle rect_rb = RenderBounds();
  DrawSprite(sprite_hue_, core::Vector2{static_cast<float>(int2_ro.X),
                                        static_cast<float>(int2_ro.Y)},
             int2{rect_rb.Width, rect_rb.Height});

  const int int4_thumb_x = PxFromValue(fp4_value);
  const int2 int2_pos =
      int2_ro + int2{std::clamp(int4_thumb_x, 0, rect_rb.Width - 1) -
                         static_cast<int>(sprite_picker_.vec_size.X) / 2,
                     (rect_rb.Height - static_cast<int>(sprite_picker_.vec_size.Y)) /
                         2};
  DrawSprite(sprite_picker_,
             core::Vector2{static_cast<float>(int2_pos.X),
                           static_cast<float>(int2_pos.Y)});
}

void HueSliderWidget::Removed() {
  up_hue_sheet_.reset();
  SliderWidget::Removed();
}

}  // namespace ora::ui
