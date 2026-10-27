#pragma once
import std;

#include "core/int2.hpp"
#include "gfx/sprite.hpp"
#include "gfx/world_renderer.hpp"
#include "ui/widget.hpp"

namespace ora::ui {

class MouseAttachmentWidget : public Widget {
 public:
  static constexpr std::string_view kWidgetTypeName = "MouseAttachmentWidget";

  MouseAttachmentWidget() = default;
  explicit MouseAttachmentWidget(WidgetArgs& args_args);
  ~MouseAttachmentWidget() override = default;

  std::string_view TypeName() const override { return kWidgetTypeName; }

  bool LoadFieldOrProperty(std::string_view str_key, std::string_view str_value) override;
  void Draw() override;
  int2 ChildOrigin() const override { return int2_location_; }

  void SetAttachment(int2 int2_location_new, gfx::Sprite sprite_new,
                     std::string str_palette_new);
  void Reset();

  bool b_click_through = true;

 private:
  gfx::Sprite sprite_attachment_;
  gfx::WorldRenderer* ptr_world_renderer_ = nullptr;
  std::string str_palette_;
  int2 int2_location_{};
};

}
