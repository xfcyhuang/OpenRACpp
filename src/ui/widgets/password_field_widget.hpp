// UPSTREAM: OpenRA.Mods.Common/Widgets/PasswordFieldWidget.cs @e180e3225 L15-23(全文)。
#pragma once
import std;

#include "ui/widgets/text_field_widget.hpp"

namespace ora::ui {

class PasswordFieldWidget : public TextFieldWidget {
 public:
  static constexpr std::string_view kWidgetTypeName = "PasswordFieldWidget";

  PasswordFieldWidget() : TextFieldWidget() {}
  explicit PasswordFieldWidget(WidgetArgs& args_args) : TextFieldWidget(args_args) {}
  explicit PasswordFieldWidget(const PasswordFieldWidget& widget_other)
      : TextFieldWidget(widget_other) {}
  ~PasswordFieldWidget() override = default;

  std::unique_ptr<Widget> Clone() override {
    return std::unique_ptr<Widget>(new PasswordFieldWidget(*this));
  }
  std::string_view TypeName() const override { return kWidgetTypeName; }

 protected:
  const std::string& GetApparentText() const override;

 private:
  mutable std::string str_apparent_;
};

}  // namespace ora::ui
