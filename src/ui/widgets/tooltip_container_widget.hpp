#pragma once
import std;

#include "core/int2.hpp"
#include "ui/widget.hpp"

namespace ora::ui {

class TooltipContainerWidget : public Widget {
 public:
  static constexpr std::string_view kWidgetTypeName = "TooltipContainerWidget";

  TooltipContainerWidget();
  explicit TooltipContainerWidget(const TooltipContainerWidget& widget_other);
  ~TooltipContainerWidget() override = default;

  std::unique_ptr<Widget> Clone() override {
    return std::unique_ptr<Widget>(new TooltipContainerWidget(*this));
  }
  std::string_view TypeName() const override { return kWidgetTypeName; }

  int2 int2_cursor_offset{0, 20};
  int int4_bottom_edge_y_offset = -5;

  std::function<void()> fn_before_render = [] {};
  int int4_tooltip_delay_milliseconds = 200;

  int SetTooltip(const std::string& str_id, WidgetArgs args_widget);

  void RemoveTooltip(int int4_token);

  void RemoveTooltip();

  void Draw() override;
  bool EventBoundsContains(int2) const override { return false; }
  int2 ChildOrigin() const override;

  const std::string* GetCursor(int2) const override { return nullptr; }

  bool LoadFieldOrProperty(std::string_view str_key, std::string_view str_value) override;

 private:
  void LoadWidgetInner();

  Widget* ptr_tooltip_ = nullptr;
  int int4_next_token_ = 1;
  int int4_current_token_ = 0;
  std::string str_id_;
  std::optional<WidgetArgs> opt_widget_args_;
};

}
