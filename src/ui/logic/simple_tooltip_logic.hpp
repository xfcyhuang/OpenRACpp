#pragma once
import std;

#include "ui/widget.hpp"
#include "ui/widgets/label_widget.hpp"

namespace ora::ui {

class SimpleTooltipLogic : public ChromeLogic {
 public:

  explicit SimpleTooltipLogic(WidgetArgs& args_args);

 private:

  std::unique_ptr<LabelWidget> up_label_template_;
};

}
