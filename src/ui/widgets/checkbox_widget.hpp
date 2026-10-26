// UPSTREAM: OpenRA.Mods.Common/Widgets/CheckboxWidget.cs @1f594e320 L19-91(全文)。
// Background 以新字段隐藏 ButtonWidget.Background(C# new);双层
// CachedTransform((CheckType,Checked) → 状态五元 → Sprite);文本色对来自
// chrome metrics 的 TextColor/TextDisabledColor。
// The whole of CheckboxWidget.cs L19-91. Background hides
// ButtonWidget.Background with a new field (C# new); the two-layer
// CachedTransform ((CheckType,Checked) → the five-state tuple → Sprite); the
// text-color pair comes from the chrome metrics' TextColor/TextDisabledColor.
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

  std::string str_background = "checkbox";  // new 隐藏 | hides the base field
  std::string str_checkmark = "tick";
  std::function<std::string()> fn_get_checkmark;
  std::function<bool()> fn_is_checked = [] { return false; };

  void Draw() override;
  bool LoadFieldOrProperty(std::string_view str_key, std::string_view str_value) override;

 private:
  core::CachedTransform<std::pair<std::string, bool>, StatefulImageCache> cache_checkmark_;
};

}  // namespace ora::ui
