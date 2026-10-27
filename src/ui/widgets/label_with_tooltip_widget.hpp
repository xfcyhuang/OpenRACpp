#pragma once
import std;

#include "ui/widget.hpp"
#include "ui/widgets/label_widget.hpp"

namespace ora::ui {

class TooltipContainerWidget;

class LabelWithTooltipWidget : public LabelWidget {
 public:
  static constexpr std::string_view kWidgetTypeName = "LabelWithTooltipWidget";

  explicit LabelWithTooltipWidget(WidgetArgs& args_args)
      : LabelWidget(args_args) {}
  explicit LabelWithTooltipWidget(const LabelWithTooltipWidget& widget_other);
  ~LabelWithTooltipWidget() override = default;

  std::unique_ptr<Widget> Clone() override {
    return std::unique_ptr<Widget>(new LabelWithTooltipWidget(*this));
  }
  std::string_view TypeName() const override { return kWidgetTypeName; }

  std::string str_tooltip_template;
  std::string str_tooltip_container;

  std::function<std::string()> fn_get_tooltip_text = [] { return std::string{}; };

  void MouseEntered() override;
  void MouseExited() override;

  bool LoadFieldOrProperty(std::string_view str_key,
                           std::string_view str_value) override;

 protected:

  TooltipContainerWidget& TooltipContainerValue();

  bool b_tooltip_container_created_ = false;
  TooltipContainerWidget* ptr_tooltip_container_ = nullptr;
};

}
