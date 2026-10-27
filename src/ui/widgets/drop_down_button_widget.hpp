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
  std::string str_panel_root;

  bool HandleKeyPress(const KeyInput& e_input) override;
  bool YieldKeyboardFocus() override;

  void Draw() override;

  int UsableWidth() const override {
    return bounds_widget.int4_width - bounds_widget.int4_height;
  }

  void Hidden() override;
  void Removed() override;

  void RemovePanel();

  void AttachPanel(std::unique_ptr<Widget> up_panel);
  void AttachPanel(std::unique_ptr<Widget> up_panel,
                   std::function<void()> fn_on_cancel);

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
      up_panel.reset(ptr_loaded_panel);
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
      up_panel.reset(ptr_loaded_panel);
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
  Widget* ptr_panel_ = nullptr;
  Widget* ptr_fullscreen_mask_ = nullptr;
  Widget* ptr_panel_root_ = nullptr;
  std::optional<StatefulImageCache> cache_marker_image_;
  std::optional<StatefulImageCache> cache_separator_image_;
};

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

}
