import std;

#include "ui/widgets/mouse_attachment_widget.hpp"

#include "core/vector_n.hpp"
#include "gfx/renderer.hpp"
#include "ui/ui.hpp"
#include "ui/widget_utils.hpp"

namespace ora::ui {

MouseAttachmentWidget::MouseAttachmentWidget(WidgetArgs& args_args) {
  if (const WidgetArgValue* ptr_value = args_args.TryGet("worldRenderer"))
    if (const auto* ptr_world_renderer = std::get_if<gfx::WorldRenderer*>(ptr_value))
      ptr_world_renderer_ = *ptr_world_renderer;
}

bool MouseAttachmentWidget::LoadFieldOrProperty(std::string_view str_key,
                                                std::string_view str_value) {
  if (str_key == "ClickThrough")
    b_click_through = str_value == "true";
  else
    return Widget::LoadFieldOrProperty(str_key, str_value);
  return true;
}

void MouseAttachmentWidget::Draw() {
  if (sprite_attachment_.ptr_sheet != nullptr && !str_palette_.empty()) {
    gfx::PaletteReference* ptr_direction_palette =
        ptr_world_renderer_->Palette(str_palette_);

    gfx::Renderer* ptr_renderer = Ui::Instance().RendererDeps();
    if (ptr_renderer == nullptr)
      return;

    const float fp4_scale = (Ui::Instance().CursorDouble() ? 2.0f : 1.0f) *
                            (ptr_renderer->NativeWindowScale() > 1.5f ? 2.0f : 1.0f);
    DrawSpriteCentered(
        sprite_attachment_, ptr_direction_palette,
        core::Vector2{static_cast<float>(int2_location_.X),
                      static_cast<float>(int2_location_.Y)},
        fp4_scale / ptr_renderer->EffectiveWindowScale());
  }
}

void MouseAttachmentWidget::SetAttachment(int2 int2_location_new,
                                          gfx::Sprite sprite_new,
                                          std::string str_palette_new) {
  sprite_attachment_ = sprite_new;
  int2_location_ = int2_location_new;
  str_palette_ = std::move(str_palette_new);
}

void MouseAttachmentWidget::Reset() {
  sprite_attachment_ = gfx::Sprite{};
  str_palette_.clear();
}

}  // namespace ora::ui
