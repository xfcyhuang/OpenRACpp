// UPSTREAM: OpenRA.Mods.Common/Widgets/RGBASpriteWidget.cs @1f594e320 L19-39(全文)。
#pragma once
import std;

#include "gfx/sprite.hpp"
#include "ui/widget.hpp"

namespace ora::ui {

class RGBASpriteWidget : public Widget {
 public:
  static constexpr std::string_view kWidgetTypeName = "RGBASpriteWidget";

  RGBASpriteWidget() = default;
  explicit RGBASpriteWidget(WidgetArgs&) {}
  explicit RGBASpriteWidget(const RGBASpriteWidget& widget_other)
      : Widget(widget_other), fn_get_sprite(widget_other.fn_get_sprite) {}
  ~RGBASpriteWidget() override = default;

  std::unique_ptr<Widget> Clone() override {
    return std::unique_ptr<Widget>(new RGBASpriteWidget(*this));
  }
  std::string_view TypeName() const override { return kWidgetTypeName; }

  std::function<gfx::Sprite()> fn_get_sprite = [] { return gfx::Sprite{}; };

  void Draw() override;
};

}  // namespace ora::ui
