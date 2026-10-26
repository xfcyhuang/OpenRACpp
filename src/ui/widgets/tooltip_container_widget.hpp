// UPSTREAM: OpenRA.Mods.Common/Widgets/TooltipContainerWidget.cs @1f594e320
// L18-114(全文)。令牌化的 SetTooltip/RemoveTooltip 对 + 懒装载的 tooltip
// 子树 + 光标偏移 ChildOrigin 的屏幕边缘回退。Game.Settings.Graphics.
// CursorDouble / Game.RunTime / Game.Renderer.Resolution / Viewport 静态 →
// Ui::Instance() 注入承载(CursorDouble 随 GraphicSettings 批,缺省 false)。
// The whole of TooltipContainerWidget.cs L18-114. The tokenized
// SetTooltip/RemoveTooltip pair + the lazily loaded tooltip subtree + the
// cursor-offset ChildOrigin with its screen-edge fallbacks.
// Game.Settings.Graphics.CursorDouble / Game.RunTime /
// Game.Renderer.Resolution / the Viewport statics go through the
// Ui::Instance() injected carrier (CursorDouble awaits the GraphicSettings
// batch, defaulting false).
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

  /// SetTooltip(L51-59):返回新令牌 | SetTooltip (L51-59): returns the new
  /// token.
  int SetTooltip(const std::string& str_id, WidgetArgs args_widget);

  /// RemoveTooltip(token)(L61-72)。
  /// RemoveTooltip(token) (L61-72).
  void RemoveTooltip(int int4_token);

  /// RemoveTooltip()(L74-77)。
  /// RemoveTooltip() (L74-77).
  void RemoveTooltip();

  void Draw() override;
  bool EventBoundsContains(int2) const override { return false; }
  int2 ChildOrigin() const override;

  const std::string* GetCursor(int2) const override { return nullptr; }

  bool LoadFieldOrProperty(std::string_view str_key, std::string_view str_value) override;

 private:
  void LoadWidgetInner();

  Widget* ptr_tooltip_ = nullptr;  // 非拥有(子树持有)| non-owning (child-owned)
  int int4_next_token_ = 1;
  int int4_current_token_ = 0;
  std::string str_id_;
  std::optional<WidgetArgs> opt_widget_args_;
};

}  // namespace ora::ui
