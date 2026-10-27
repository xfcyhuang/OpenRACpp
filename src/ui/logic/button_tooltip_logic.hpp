#pragma once
import std;

#include "ui/widget.hpp"
#include "ui/widgets/label_widget.hpp"

namespace ora::ui {

class ButtonTooltipLogic : public ChromeLogic {
 public:
  explicit ButtonTooltipLogic(WidgetArgs& args_args);

 private:
  std::unique_ptr<LabelWidget> up_desc_template_;
};

}
