import std;

#include "ui/widgets/rgba_sprite_widget.hpp"

#include "ui/widget_utils.hpp"

namespace ora::ui {

void RGBASpriteWidget::Draw() {
  const gfx::Sprite sprite_drawn = fn_get_sprite();
  if (sprite_drawn.ptr_sheet != nullptr)
    DrawSprite(sprite_drawn, RenderOrigin().ToVector2());
}

}
