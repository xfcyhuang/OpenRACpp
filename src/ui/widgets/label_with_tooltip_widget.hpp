// UPSTREAM: OpenRA.Mods.Common/Widgets/LabelWithTooltipWidget.cs @1f594e320
// L20-64(全文)。Tooltip 容器的 Lazy 解析(Ui.Root.Get<TooltipContainer
// Widget>)挂 MouseEntered/MouseExited 对;MouseExited 仅在容器已物化时才
// RemoveTooltip(上游 IsValueCreated 门,防 widget 与容器同批移除的崩溃)。
// C# Lazy → 指针 + 物化标志。
// The whole of LabelWithTooltipWidget.cs L20-64. The tooltip container's
// Lazy resolution (Ui.Root.Get<TooltipContainerWidget>) hangs on the
// MouseEntered/MouseExited pair; MouseExited removes the tooltip only when
// the container has materialized (upstream's IsValueCreated gate, guarding
// the widget-and-container-removed-together crash). C#'s Lazy becomes a
// pointer + a materialized flag.
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

  std::string str_tooltip_template;  // 空 = C# null | null
  std::string str_tooltip_container;  // 空 = C# null | null

  std::function<std::string()> fn_get_tooltip_text = [] { return std::string{}; };

  void MouseEntered() override;
  void MouseExited() override;

  bool LoadFieldOrProperty(std::string_view str_key,
                           std::string_view str_value) override;

 protected:
  /// Exts.Lazy(() => Ui.Root.Get<TooltipContainerWidget>(…))的物化面
  /// Exts.Lazy(() => Ui.Root.Get<TooltipContainerWidget>(…))'s
  /// materialization face.
  TooltipContainerWidget& TooltipContainerValue();

  bool b_tooltip_container_created_ = false;
  TooltipContainerWidget* ptr_tooltip_container_ = nullptr;
};

}  // namespace ora::ui
