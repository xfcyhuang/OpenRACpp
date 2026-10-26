// UPSTREAM: OpenRA.Mods.Common/Widgets/DropDownButtonWidget.cs @1f594e320
// L20-257(全文)。开面板三件套(AttachPanel 的全屏 MaskWidget + 边缘回退
// 定位/RemovePanel 的退焦点与 tooltip 重置/ShowDropDown 双模板 —— 平面与
// 分组头部形态);箭头与分隔条的状态图缓存(??= 惰性初建 → optional);
// UsableWidth = Width - Height(上游 "This is crap" 注释照抄)。上游 clone
// 不拷 PanelAlign(保护拷贝 ctor 的遗漏 quirk)保真。Game.Renderer.
// Resolution/Game.Sound/Ui.Root/Ui.LoadWidget → Ui::Instance() 注入承载。
// The whole of DropDownButtonWidget.cs L20-257. The open-panel trio
// (AttachPanel's fullscreen MaskWidget + the edge-fallback positioning /
// RemovePanel's focus yield + tooltip reset / ShowDropDown's two templates —
// the flat and the grouped-header shapes); the marker and separator
// stateful-image caches (??= lazy init → optional); UsableWidth = Width -
// Height (upstream's "This is crap" comment kept). Upstream's clone not
// copying PanelAlign (the protected copy-ctor's omission quirk) is kept
// faithful. Game.Renderer.Resolution/Game.Sound/Ui.Root/Ui.LoadWidget go
// through the Ui::Instance() injected carrier.
#pragma once
import std;

#include "core/int2.hpp"
#include "ui/ui.hpp"
#include "ui/widget.hpp"
#include "ui/widget_utils.hpp"
#include "ui/widgets/button_widget.hpp"
#include "ui/widgets/scroll_item_widget.hpp"
#include "ui/widgets/scroll_panel_widget.hpp"

namespace ora::ui {

/// 模板项的所有权摘取(上游 RemoveChildren 后 itemTemplate 局部引用靠 GC
/// 存活;C++ 侧先摘除保活 —— Removed() 通知与上游 RemoveChildren 同可观测)
/// Ownership extraction of a template item (upstream's itemTemplate local
/// reference outlives RemoveChildren through GC; the C++ side extracts it
/// first — the Removed() notification stays as observable as upstream's
/// RemoveChildren).
inline std::unique_ptr<ScrollItemWidget> TakeItemTemplate(ScrollPanelWidget& panel_items,
                                                          ScrollItemWidget* ptr_template) {
  if (ptr_template == nullptr)
    return nullptr;
  std::unique_ptr<Widget> up_widget = panel_items.RemoveChild(*ptr_template);
  return std::unique_ptr<ScrollItemWidget>(
      static_cast<ScrollItemWidget*>(up_widget.release()));
}

class DropDownButtonWidget : public ButtonWidget {
 public:
  static constexpr std::string_view kWidgetTypeName = "DropDownButtonWidget";

  explicit DropDownButtonWidget(WidgetArgs& args_args) : ButtonWidget(args_args) {}
  explicit DropDownButtonWidget(const DropDownButtonWidget& widget_other);
  ~DropDownButtonWidget() override = default;

  std::unique_ptr<Widget> Clone() override {
    return std::unique_ptr<Widget>(new DropDownButtonWidget(*this));
  }
  std::string_view TypeName() const override { return kWidgetTypeName; }

  std::string str_decorations = "dropdown-decorations";
  std::string str_decoration_marker = "marker";
  std::string str_separators = "dropdown-separators";
  std::string str_separator_image = "separator";
  TextAlign kind_panel_align = TextAlign::Left;
  std::string str_panel_root;  // 空 = C# null | null

  bool HandleKeyPress(const KeyInput& e_input) override;
  bool YieldKeyboardFocus() override;

  void Draw() override;

  // This is crap(上游注释)| This is crap (the upstream comment).
  int UsableWidth() const override {
    return bounds_widget.int4_width - bounds_widget.int4_height;
  }

  void Hidden() override;
  void Removed() override;

  void RemovePanel();

  /// AttachPanel(p)/AttachPanel(p, onCancel)(L154-201)。
  /// AttachPanel(p)/AttachPanel(p, onCancel) (L154-201).
  void AttachPanel(std::unique_ptr<Widget> up_panel);
  void AttachPanel(std::unique_ptr<Widget> up_panel,
                   std::function<void()> fn_on_cancel);

  /// ShowDropDown 平面形态(L204-226)。
  /// ShowDropDown's flat shape (L204-226).
  template <class T>
  void ShowDropDown(const std::string& str_panel_template, int int4_max_height,
                    const std::vector<T>& vec_options,
                    const std::function<std::unique_ptr<ScrollItemWidget>(
                        const T&, ScrollItemWidget&)>& fn_setup_item) {
    std::map<std::string, std::int32_t> map_substitutions;
    map_substitutions.emplace("DROPDOWN_WIDTH", bounds_widget.int4_width);
    WidgetArgs args_panel;
    args_panel.Add("substitutions", &map_substitutions);
    Widget* ptr_loaded_panel =
        Ui::Instance().LoadWidget(str_panel_template, nullptr, std::move(args_panel));
    std::unique_ptr<Widget> up_panel = Ui::Instance().ReleaseWidget(ptr_loaded_panel);
    if (!up_panel)
      up_panel.reset(ptr_loaded_panel);  // 未接线宿主:裸管(测试域)| an unwired host: raw ownership (the test domain)
    ScrollPanelWidget* ptr_panel = static_cast<ScrollPanelWidget*>(up_panel.get());

    std::unique_ptr<ScrollItemWidget> up_item_template =
        TakeItemTemplate(*ptr_panel, ptr_panel->Get<ScrollItemWidget>("TEMPLATE"));
    ptr_panel->RemoveChildren();
    for (const T& val_option : vec_options) {
      std::unique_ptr<ScrollItemWidget> up_item = fn_setup_item(val_option, *up_item_template);
      std::function<void()> fn_on_click_orig = up_item->fn_on_click;
      up_item->fn_on_click = [this, fn_on_click_orig] {
        fn_on_click_orig();
        RemovePanel();
      };

      ptr_panel->AddChild(std::move(up_item));
    }

    ptr_panel->bounds_widget.int4_height =
        std::min(int4_max_height, ptr_panel->int4_content_height);
    AttachPanel(std::move(up_panel));
  }

  /// ShowDropDown 分组形态(L228-257):空组名不出 HEADER。
  /// ShowDropDown's grouped shape (L228-257): an empty group name emits no
  /// HEADER.
  template <class T>
  void ShowDropDown(const std::string& str_panel_template, int int4_height,
                    const std::vector<std::pair<std::string, std::vector<T>>>& vec_groups,
                    const std::function<std::unique_ptr<ScrollItemWidget>(
                        const T&, ScrollItemWidget&)>& fn_setup_item) {
    std::map<std::string, std::int32_t> map_substitutions;
    map_substitutions.emplace("DROPDOWN_WIDTH", bounds_widget.int4_width);
    WidgetArgs args_panel;
    args_panel.Add("substitutions", &map_substitutions);
    Widget* ptr_loaded_panel =
        Ui::Instance().LoadWidget(str_panel_template, nullptr, std::move(args_panel));
    std::unique_ptr<Widget> up_panel = Ui::Instance().ReleaseWidget(ptr_loaded_panel);
    if (!up_panel)
      up_panel.reset(ptr_loaded_panel);  // 未接线宿主:裸管(测试域)| an unwired host: raw ownership (the test domain)
    ScrollPanelWidget* ptr_panel = static_cast<ScrollPanelWidget*>(up_panel.get());

    std::unique_ptr<ScrollItemWidget> up_header_template =
        TakeItemTemplate(*ptr_panel, ptr_panel->GetOrNull<ScrollItemWidget>("HEADER"));
    std::unique_ptr<ScrollItemWidget> up_item_template =
        TakeItemTemplate(*ptr_panel, ptr_panel->Get<ScrollItemWidget>("TEMPLATE"));
    ptr_panel->RemoveChildren();

    for (const auto& [str_group, vec_options] : vec_groups) {
      if (!str_group.empty() && up_header_template != nullptr) {
        std::unique_ptr<ScrollItemWidget> up_header = ScrollItemWidget::Setup(
            *up_header_template, [] { return false; }, [] {});
        LabelWidget* ptr_label = up_header->Get<LabelWidget>("LABEL");
        ptr_label->fn_get_text = [str_group] { return str_group; };
        ptr_panel->AddChild(std::move(up_header));
      }

      for (const T& val_option : vec_options) {
        std::unique_ptr<ScrollItemWidget> up_item = fn_setup_item(val_option, *up_item_template);
        std::function<void()> fn_on_click_orig = up_item->fn_on_click;
        up_item->fn_on_click = [this, fn_on_click_orig] {
          fn_on_click_orig();
          RemovePanel();
        };

        ptr_panel->AddChild(std::move(up_item));
      }
    }

    ptr_panel->bounds_widget.int4_height =
        std::min(int4_height, ptr_panel->int4_content_height);
    AttachPanel(std::move(up_panel));
  }

  bool LoadFieldOrProperty(std::string_view str_key,
                           std::string_view str_value) override;

 private:
  Widget* ptr_panel_ = nullptr;            // 非拥有(panelRoot 持有)| non-owning
  Widget* ptr_fullscreen_mask_ = nullptr;  // 非拥有 | non-owning
  Widget* ptr_panel_root_ = nullptr;       // 非拥有 | non-owning
  std::optional<StatefulImageCache> cache_marker_image_;
  std::optional<StatefulImageCache> cache_separator_image_;
};

/// MaskWidget(L229-256):全屏遮罩,吞全部非 Move 鼠标输入;Down 广播
/// OnMouseDown 多播表(AttachPanel 挂 click 音 + RemovePanel + onCancel)。
/// MaskWidget (L229-256): the fullscreen mask swallowing every non-Move
/// mouse input; Down broadcasts the OnMouseDown multicast table (AttachPanel
/// hangs the click sound + RemovePanel + onCancel).
class MaskWidget : public Widget {
 public:
  static constexpr std::string_view kWidgetTypeName = "MaskWidget";

  MaskWidget() = default;
  explicit MaskWidget(const MaskWidget& widget_other)
      : Widget(widget_other), vec_on_mouse_down(widget_other.vec_on_mouse_down) {}
  ~MaskWidget() override = default;

  std::unique_ptr<Widget> Clone() override {
    return std::unique_ptr<Widget>(new MaskWidget(*this));
  }
  std::string_view TypeName() const override { return kWidgetTypeName; }

  std::vector<std::function<void(const MouseInput&)>> vec_on_mouse_down;

  bool HandleMouseInput(const MouseInput& mi_input) override;
  const std::string* GetCursor(int2) const override { return nullptr; }
};

}  // namespace ora::ui
