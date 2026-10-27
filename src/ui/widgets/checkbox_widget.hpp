#pragma once
import std;

#include "core/cached_transform.hpp"
#include "core/vector_n.hpp"
#include "ui/widget.hpp"
#include "ui/widget_utils.hpp"
#include "ui/widgets/button_widget.hpp"

namespace ora::ui {

class CheckboxWidget : public ButtonWidget {
 public:
  static constexpr std::string_view kWidgetTypeName = "CheckboxWidget";

  explicit CheckboxWidget(WidgetArgs& args_args);
  explicit CheckboxWidget(const CheckboxWidget& widget_other);
  ~CheckboxWidget() override = default;

  std::unique_ptr<Widget> Clone() override {
    return std::unique_ptr<Widget>(new CheckboxWidget(*this));
  }
  std::string_view TypeName() const override { return kWidgetTypeName; }

  std::string str_background = "checkbox";
  std::string str_checkmark = "tick";
  std::function<std::string()> fn_get_checkmark;
  std::function<bool()> fn_is_checked = [] { return false; };

  void Draw() override;
  bool LoadFieldOrProperty(std::string_view str_key, std::string_view str_value) override;

 private:
  core::CachedTransform<std::pair<std::string, bool>, StatefulImageCache> cache_checkmark_;
};

}
