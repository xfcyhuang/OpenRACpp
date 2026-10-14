// UPSTREAM: OpenRA.Mods.Common/Widgets/ScrollItemWidget.cs @e180e3225 L19-97(全文)。
// IsVisible 恒 false 模板 + VisualHeight 0 + Initialize 的
// IgnoreChildMouseOver 翻转 HACK;Setup 三静态模板装配方;面板图缓存走
// GetCachedStatefulPanelImages。
// The whole of ScrollItemWidget.cs L19-97. The IsVisible-always-false
// template + VisualHeight 0 + Initialize's IgnoreChildMouseOver flip HACK;
// the three static Setup template binders; the panel-image cache through
// GetCachedStatefulPanelImages.
#pragma once
import std;

#include "core/cached_transform.hpp"
#include "ui/widget.hpp"
#include "ui/widget_utils.hpp"
#include "ui/widgets/button_widget.hpp"

namespace ora::ui {

class ScrollItemWidget : public ButtonWidget {
 public:
  static constexpr std::string_view kWidgetTypeName = "ScrollItemWidget";

  explicit ScrollItemWidget(WidgetArgs& args_args);
  explicit ScrollItemWidget(const ScrollItemWidget& widget_other);
  ~ScrollItemWidget() override = default;

  std::unique_ptr<Widget> Clone() override {
    return std::unique_ptr<Widget>(new ScrollItemWidget(*this));
  }
  std::string_view TypeName() const override { return kWidgetTypeName; }

  std::string str_background = "scrollitem";  // new 隐藏 | hides the base field
  bool b_enable_child_mouse_over = false;
  std::string str_item_key;

  std::function<bool()> fn_is_selected = [] { return false; };

  void Initialize(WidgetArgs& args_args) override;
  void Draw() override;
  bool LoadFieldOrProperty(std::string_view str_key, std::string_view str_value) override;

  static std::unique_ptr<ScrollItemWidget> Setup(ScrollItemWidget& widget_template,
                                                 std::function<bool()> fn_is_selected,
                                                 std::function<void()> fn_on_click);
  static std::unique_ptr<ScrollItemWidget> Setup(ScrollItemWidget& widget_template,
                                                 std::function<bool()> fn_is_selected,
                                                 std::function<void()> fn_on_click,
                                                 std::function<void()> fn_on_double_click);
  static std::unique_ptr<ScrollItemWidget> Setup(const std::string& str_key,
                                                 ScrollItemWidget& widget_template,
                                                 std::function<bool()> fn_is_selected,
                                                 std::function<void()> fn_on_click,
                                                 std::function<void()> fn_on_double_click);

 private:
  StatefulPanelCache cache_panel_;
};

}  // namespace ora::ui
