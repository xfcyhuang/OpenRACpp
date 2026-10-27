import std;

#include "ui/widgets/resource_bar_widget.hpp"

#include "core/vector_n.hpp"
#include "gfx/chrome_provider.hpp"
#include "gfx/renderer.hpp"
#include "gfx/sprite_renderer.hpp"
#include "meta/field_loader.hpp"
#include "ui/ui.hpp"
#include "ui/widget_utils.hpp"
#include "ui/widgets/tooltip_container_widget.hpp"

namespace ora::ui {

float EWMA::Update(float fp4_new_value) {
  const float fp4_current = opt_value_.value_or(fp4_new_value);
  opt_value_ = fp4_current + fp4_anim_rate_ * (fp4_new_value - fp4_current);
  return *opt_value_;
}

ResourceBarWidget::ResourceBarWidget() {}

ResourceBarWidget::ResourceBarWidget(WidgetArgs& args_args) {
  if (const WidgetArgValue* ptr_value = args_args.TryGet("world"))
    if (const auto* ptr_world = std::get_if<sim::World*>(ptr_value))
      ptr_world_ = *ptr_world;
}

TooltipContainerWidget* ResourceBarWidget::TooltipContainerLazy() {
  if (!b_tooltip_container_created_) {
    b_tooltip_container_created_ = true;
    if (Ui::Instance().Root() != nullptr)
      ptr_tooltip_container_ =
          Ui::Instance().Root()->GetOrNull<TooltipContainerWidget>(str_tooltip_container);
  }
  return ptr_tooltip_container_;
}

bool ResourceBarWidget::LoadFieldOrProperty(std::string_view str_key,
                                            std::string_view str_value) {
  if (str_key == "TooltipTemplate")
    str_tooltip_template = str_value;
  else if (str_key == "TooltipContainer")
    str_tooltip_container = str_value;
  else if (str_key == "Orientation")
    kind_orientation = str_value == "Horizontal" ? ResourceBarOrientation::Horizontal
                                                 : ResourceBarOrientation::Vertical;
  else if (str_key == "IndicatorCollection")
    str_indicator_collection = str_value;
  else if (str_key == "IndicatorImage")
    str_indicator_image = str_value;
  else
    return Widget::LoadFieldOrProperty(str_key, str_value);
  return true;
}

void ResourceBarWidget::Initialize(WidgetArgs& args_args) {
  Widget::Initialize(args_args);

  gfx::ChromeProvider* ptr_chrome = ChromeProviderOrNull();
  if (ptr_chrome != nullptr)
    sprite_indicator_ =
        ptr_chrome->GetImage(str_indicator_collection, str_indicator_image);
}

void ResourceBarWidget::MouseEntered() {
  if (str_tooltip_container.empty())
    return;

  std::function<std::string()> fn_get_text = [this] {
    return cache_tooltip_text.Update(
        std::tuple<float, float>{fn_get_used(), fn_get_provided()});
  };
  TooltipContainerLazy()->SetTooltip(
      str_tooltip_template,
      WidgetArgs{{"getText", fn_get_text}, {"world", ptr_world_}});
}

void ResourceBarWidget::MouseExited() {
  if (str_tooltip_container.empty())
    return;

  TooltipContainerLazy()->RemoveTooltip();
}

void ResourceBarWidget::Draw() {
  float fp4_scale_by = 100.0f;
  const float fp4_provided = fn_get_provided();
  const float fp4_used = fn_get_used();
  const float fp4_max = std::max(fp4_provided, fp4_used);
  while (fp4_max >= fp4_scale_by)
    fp4_scale_by *= 2;

  const float fp4_provided_frac = lerp_provided_.Update(fp4_provided / fp4_scale_by);
  const float fp4_used_frac = lerp_used_.Update(fp4_used / fp4_scale_by);

  const Rectangle rect = RenderBounds();

  const core::Color color_c = fn_get_bar_color();
  if (kind_orientation == ResourceBarOrientation::Vertical) {
    const int int4_top_y = static_cast<int>(
        rect.Bottom() + fp4_provided_frac * (rect.Top() - rect.Bottom()));
    const core::Vector3 vec_tl{static_cast<float>(rect.X),
                               static_cast<float>(int4_top_y), 0.0f};
    const core::Vector3 vec_br{
        vec_tl.X + static_cast<float>(rect.Width),
        vec_tl.Y + static_cast<float>(static_cast<int>(fp4_provided_frac * rect.Height)),
        0.0f};

    gfx::Renderer* ptr_renderer = Ui::Instance().RendererDeps();
    if (ptr_renderer == nullptr)
      return;
    ptr_renderer->UIRgbaColorRenderer().FillRect(vec_tl, vec_br, color_c);

    const int int4_x = (rect.Left() + rect.Right() -
                        static_cast<int>(sprite_indicator_.vec_size.X)) /
                       2;
    const int int4_y =
        static_cast<int>(rect.Bottom() +
                         fp4_used_frac * (rect.Top() - rect.Bottom())) -
        static_cast<int>(sprite_indicator_.vec_size.Y) / 2;
    DrawSprite(sprite_indicator_,
               core::Vector2{static_cast<float>(int4_x), static_cast<float>(int4_y)});
  } else {
    const core::Vector3 vec_tl{static_cast<float>(rect.X), static_cast<float>(rect.Y),
                               0.0f};
    const core::Vector3 vec_br{
        vec_tl.X + static_cast<float>(static_cast<int>(fp4_provided_frac * rect.Width)),
        vec_tl.Y + static_cast<float>(rect.Height), 0.0f};

    gfx::Renderer* ptr_renderer = Ui::Instance().RendererDeps();
    if (ptr_renderer == nullptr)
      return;
    ptr_renderer->UIRgbaColorRenderer().FillRect(vec_tl, vec_br, color_c);

    const int int4_x = static_cast<int>(rect.Left() +
                                        fp4_used_frac * (rect.Right() - rect.Left())) -
                       static_cast<int>(sprite_indicator_.vec_size.X) / 2;
    const int int4_y = (rect.Bottom() + rect.Top() -
                        static_cast<int>(sprite_indicator_.vec_size.Y)) /
                       2;
    DrawSprite(sprite_indicator_,
               core::Vector2{static_cast<float>(int4_x), static_cast<float>(int4_y)});
  }
}

}
