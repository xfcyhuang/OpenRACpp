import std;

#include "ui/widgets/badge_widget.hpp"

#include "core/vector_n.hpp"
#include "ui/widget_utils.hpp"

namespace ora::ui {

void BadgeWidget::Draw() {
  if (!opt_badge.has_value())
    return;

  const gfx::Sprite sprite_icon = Database().GetIcon(*opt_badge);
  if (sprite_icon.ptr_sheet != nullptr) {
    const int2 int2_origin = RenderOrigin();
    DrawSprite(sprite_icon,
               core::Vector2{static_cast<float>(int2_origin.X),
                             static_cast<float>(int2_origin.Y)});
  }
}

}  // namespace ora::ui
