#pragma once
import std;

#include "core/int2.hpp"
#include "core/rectangle.hpp"
#include "ui/widget.hpp"
#include "ui/widget_utils.hpp"
#include "ui/widgets/button_widget.hpp"

namespace ora::game {
class Ruleset;
}

namespace ora::ui {

class ScrollPanelWidget;

class ILayout {
 public:
  virtual ~ILayout() = default;
  virtual void AdjustChild(Widget& widget_child) = 0;
  virtual void AdjustChildren() = 0;

 protected:
  ILayout() = default;
};

class ListLayout final : public ILayout {
 public:
  explicit ListLayout(ScrollPanelWidget& widget_panel) : ptr_widget_(&widget_panel) {}

  void AdjustChild(Widget& widget_child) override;
  void AdjustChildren() override;

 private:
  ScrollPanelWidget* ptr_widget_;
};

class GridLayout final : public ILayout {
 public:
  explicit GridLayout(ScrollPanelWidget& widget_panel) : ptr_widget_(&widget_panel) {}

  void AdjustChild(Widget& widget_child) override;
  void AdjustChildren() override {}

 private:
  ScrollPanelWidget* ptr_widget_;
  int2 int2_pos{};
};

enum class ScrollPanelAlign { Bottom, Top };
enum class ScrollBar { Left, Right, Hidden };

class ScrollPanelWidget : public Widget {
 public:
  static constexpr std::string_view kWidgetTypeName = "ScrollPanelWidget";

  explicit ScrollPanelWidget(WidgetArgs& args_args);
  explicit ScrollPanelWidget(const ScrollPanelWidget& widget_other);
  ~ScrollPanelWidget() override = default;

  std::unique_ptr<Widget> Clone() override {
    return std::unique_ptr<Widget>(new ScrollPanelWidget(*this));
  }
  std::string_view TypeName() const override { return kWidgetTypeName; }

  int int4_scrollbar_width = 24;
  int int4_border_width = 1;
  int int4_top_bottom_spacing = 2;
  int int4_item_spacing = 0;
  int int4_button_depth = 0;
  std::string str_click_sound;
  std::string str_click_disabled_sound;
  std::string str_background = "scrollpanel-bg";
  std::string str_scrollbar_background = "scrollpanel-bg";
  std::string str_button = "scrollpanel-button";
  std::string str_decorations = "scrollpanel-decorations";
  std::string str_decoration_scroll_up = "up";
  std::string str_decoration_scroll_down = "down";
  int int4_content_height = 0;
  std::unique_ptr<ILayout> ptr_layout;
  int int4_minimum_thumb_size = 10;
  ScrollPanelAlign kind_align = ScrollPanelAlign::Top;
  ScrollBar kind_scrollbar = ScrollBar::Right;
  bool b_collapse_hidden_children = false;

  float fp4_smooth_scroll_speed = 0.333f;

  void RemoveChildren() override;
  void AddChild(std::unique_ptr<Widget> ptr_child) override;
  std::unique_ptr<Widget> RemoveChild(Widget& child_widget) override;
  void ReplaceChild(Widget& old_child, std::unique_ptr<Widget> up_new_child);

  void DrawOuter() override;
  int2 ChildOrigin() const override;
  bool EventBoundsContains(int2 location) const override;

  void ScrollToBottom(bool b_smooth = false);
  void ScrollToTop(bool b_smooth = false);
  bool ScrolledToBottom() const;
  void ScrollToItem(const std::string& str_item_key, bool b_smooth = false);
  void ScrollToSelectedItem();
  void Tick() override;
  bool YieldMouseFocus(const MouseInput& mi_input) override;
  bool HandleMouseInput(const MouseInput& mi_input) override;
  bool LoadFieldOrProperty(std::string_view str_key, std::string_view str_value) override;

  Rectangle ThumbRect() const { return rect_thumb_; }
  bool UpPressed() const { return b_up_pressed_; }
  bool DownPressed() const { return b_down_pressed_; }
  bool UpDisabled() const { return b_up_disabled_; }
  bool DownDisabled() const { return b_down_disabled_; }
  float CurrentListOffset() const { return fp4_current_list_offset_; }

 protected:
  bool b_up_pressed_ = false;
  bool b_down_pressed_ = false;
  bool b_up_disabled_ = false;
  bool b_down_disabled_ = false;
  bool b_thumb_pressed_ = false;
  Rectangle rect_up_button_;
  Rectangle rect_down_button_;
  Rectangle rect_background_;
  Rectangle rect_scrollbar_;
  Rectangle rect_thumb_;

  void SetListOffset(float fp4_value, bool b_smooth);

 private:
  void Scroll(int int4_amount, bool b_smooth = false);
  void ScrollToItemInner(Widget& item_widget, bool b_smooth = false);
  void UpdateSmoothScrolling();

  float fp4_target_list_offset_ = 0.0f;
  float fp4_current_list_offset_ = 0.0f;
  std::int64_t int8_last_smooth_scroll_time_ = 0;
  int2 int2_last_mouse_location_{};

  const game::Ruleset* mod_rules = nullptr;
  StatefulImageCache cache_up_arrow_;
  StatefulImageCache cache_down_arrow_;
};

}
