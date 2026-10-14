// UPSTREAM: OpenRA.Mods.Common/Widgets/Logic/SimpleTooltipLogic.cs @e180e3225
// L15-61(全文)。LABEL 模板 + LINE_HEIGHT 间距数据节点 + BeforeRender 的
// cachedText 门控重建:文本变更时逐行克隆 Label 定宽排布并自适应 widget
// 尺寸(horizontalPadding = LABEL 宽 - widget 宽,非正回落 2×LABEL.X)。
// The whole of SimpleTooltipLogic.cs L15-61. The LABEL template + the
// LINE_HEIGHT spacing data node + BeforeRender's cachedText-gated rebuild:
// on a text change, Label clones are laid out one per line at the measured
// width and the widget resizes (horizontalPadding = LABEL width - widget
// width, falling back to 2×LABEL.X when non-positive).
#pragma once
import std;

#include "ui/widget.hpp"
#include "ui/widgets/label_widget.hpp"

namespace ora::ui {

class SimpleTooltipLogic : public ChromeLogic {
 public:
  /// ObjectCreator.UseCtor(Widget, TooltipContainerWidget, Func<string>) →
  /// WidgetArgs 三键("widget"/"tooltipContainer"/"getText")
  /// ObjectCreator.UseCtor (Widget, TooltipContainerWidget, Func<string>) →
  /// the WidgetArgs triple ("widget"/"tooltipContainer"/"getText").
  explicit SimpleTooltipLogic(WidgetArgs& args_args);

 private:
  /// LABEL 模板的自有克隆(RemoveChildren 后上游靠 GC 闭包保活的等价)
  /// The owned clone of the LABEL template (upstream's GC'd closure keeps it
  /// alive after RemoveChildren; this is the equivalent).
  std::unique_ptr<LabelWidget> up_label_template_;
};

}  // namespace ora::ui
