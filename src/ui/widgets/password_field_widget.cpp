// UPSTREAM: OpenRA.Mods.Common/Widgets/PasswordFieldWidget.cs @e180e3225 L15-23(全文)。
import std;

#include "ui/widgets/password_field_widget.hpp"

namespace ora::ui {

const std::string& PasswordFieldWidget::GetApparentText() const {
  str_apparent_.assign(Text().size(), '*');
  return str_apparent_;
}

}  // namespace ora::ui
