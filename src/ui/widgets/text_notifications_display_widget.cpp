import std;

#include "ui/widgets/text_notifications_display_widget.hpp"

#include "gfx/renderer.hpp"
#include "gfx/sprite_font.hpp"
#include "meta/field_loader.hpp"
#include "ui/ui.hpp"
#include "ui/widget_utils.hpp"
#include "ui/widgets/label_widget.hpp"

namespace ora::ui {

bool TextNotificationsDisplayWidget::LoadFieldOrProperty(
    std::string_view str_key, std::string_view str_value) {
  if (str_key == "DisplayDurationMs")
    int4_display_duration_ms = meta::GetInt32Value(str_key, str_value);
  else if (str_key == "ItemSpacing")
    int4_item_spacing = meta::GetInt32Value(str_key, str_value);
  else if (str_key == "BottomSpacing")
    int4_bottom_spacing = meta::GetInt32Value(str_key, str_value);
  else if (str_key == "LogLength")
    int4_log_length = meta::GetInt32Value(str_key, str_value);
  else if (str_key == "HideOverflow")
    b_hide_overflow = str_value == "true";
  else if (str_key == "ChatTemplate")
    str_chat_template = str_value;
  else if (str_key == "SystemTemplate")
    str_system_template = str_value;
  else if (str_key == "MissionTemplate")
    str_mission_template = str_value;
  else if (str_key == "FeedbackTemplate")
    str_feedback_template = str_value;
  else if (str_key == "TransientsTemplate")
    str_transients_template = str_value;
  else
    return Widget::LoadFieldOrProperty(str_key, str_value);
  return true;
}

void TextNotificationsDisplayWidget::Initialize(WidgetArgs& args_args) {
  Widget::Initialize(args_args);

  Widget* ptr_chat = Ui::Instance().LoadWidget(str_chat_template, nullptr, WidgetArgs{});
  Widget* ptr_system =
      Ui::Instance().LoadWidget(str_system_template, nullptr, WidgetArgs{});
  Widget* ptr_mission =
      Ui::Instance().LoadWidget(str_mission_template, nullptr, WidgetArgs{});
  Widget* ptr_feedback =
      Ui::Instance().LoadWidget(str_feedback_template, nullptr, WidgetArgs{});
  Widget* ptr_transients =
      Ui::Instance().LoadWidget(str_transients_template, nullptr, WidgetArgs{});
  map_templates_.emplace(net::TextNotificationPool::Chat,
                         Ui::Instance().ReleaseWidget(ptr_chat));
  map_templates_.emplace(net::TextNotificationPool::System,
                         Ui::Instance().ReleaseWidget(ptr_system));
  map_templates_.emplace(net::TextNotificationPool::Mission,
                         Ui::Instance().ReleaseWidget(ptr_mission));
  map_templates_.emplace(net::TextNotificationPool::Feedback,
                         Ui::Instance().ReleaseWidget(ptr_feedback));
  map_templates_.emplace(net::TextNotificationPool::Transients,
                         Ui::Instance().ReleaseWidget(ptr_transients));

  const std::string& str_font =
      map_templates_[net::TextNotificationPool::Chat]->Get<LabelWidget>("TEXT")->str_font;
  const int int4_line_height = RequireWidgetFont(str_font).Measure("").Y;
  const int int4_whole_lines =
      static_cast<int>(std::floor(static_cast<double>(
          (bounds_widget.int4_height - int4_bottom_spacing) / int4_line_height)));
  const int int4_visible_children_height = int4_whole_lines * int4_line_height;

  const int int4_y =
      RenderOrigin().Y + bounds_widget.int4_height - int4_visible_children_height;
  rect_overflow_draw_bounds_ =
      Rectangle{RenderOrigin().X, int4_y, bounds_widget.int4_width,
                int4_visible_children_height};
}

void TextNotificationsDisplayWidget::DrawOuter() {
  if (!fn_is_visible() || OwnedChildren().empty())
    return;

  const std::vector<Widget*> vec_children = Children();
  const bool b_most_recent_overflows =
      bounds_widget.int4_height <
      vec_children.back()->bounds_widget.int4_height;

  if (b_most_recent_overflows && b_hide_overflow)
    RequireRenderer().EnableScissor(rect_overflow_draw_bounds_);

  const Rectangle rect = bounds_widget.ToRectangle();
  for (std::size_t sz_i = vec_children.size(); sz_i-- > 0;) {
    if (!b_hide_overflow || b_most_recent_overflows ||
        rect.Contains(vec_children[sz_i]->bounds_widget.ToRectangle()))
      vec_children[sz_i]->DrawOuter();

    if (b_most_recent_overflows)
      break;
  }

  if (b_most_recent_overflows && b_hide_overflow)
    RequireRenderer().DisableScissor();
}

void TextNotificationsDisplayWidget::AddNotification(
    const net::TextNotification& notification_new) {
  std::unique_ptr<Widget> up_notification_widget(
      map_templates_[notification_new.Pool()]->Clone());
  Widget& widget_notification = *up_notification_widget;
  SetupTextNotification(widget_notification, notification_new,
                        bounds_widget.int4_width, false);

  if (OwnedChildren().empty())
    widget_notification.bounds_widget.int4_y =
        bounds_widget.Bottom() - widget_notification.bounds_widget.int4_height -
        int4_bottom_spacing;
  else {
    for (Widget* ptr_line : Children())
      ptr_line->bounds_widget.int4_y -=
          widget_notification.bounds_widget.int4_height + int4_item_spacing;

    Widget& widget_last_line = *Children().back();
    widget_notification.bounds_widget.int4_y =
        widget_last_line.bounds_widget.Bottom() + int4_item_spacing;
  }

  AddChild(std::move(up_notification_widget));
  vec_expirations_.push_back(Ui::Instance().RunTime() + int4_display_duration_ms);

  while (static_cast<int>(OwnedChildren().size()) > int4_log_length)
    RemoveNotification();
}

void TextNotificationsDisplayWidget::RemoveMostRecentNotification() {
  if (OwnedChildren().empty())
    return;

  Widget& widget_most_recent = *Children().back();
  const int int4_most_recent_height = widget_most_recent.bounds_widget.int4_height;

  RemoveChild(widget_most_recent);
  vec_expirations_.pop_back();

  const std::vector<Widget*> vec_children = Children();
  for (std::size_t sz_i = vec_children.size(); sz_i-- > 0;)
    vec_children[sz_i]->bounds_widget.int4_y +=
        int4_most_recent_height + int4_item_spacing;
}

void TextNotificationsDisplayWidget::RemoveNotification() {
  if (OwnedChildren().empty())
    return;

  RemoveChild(*Children().front());
  vec_expirations_.erase(vec_expirations_.begin());
}

void TextNotificationsDisplayWidget::Tick() {
  if (int4_display_duration_ms == 0)
    return;

  while (!OwnedChildren().empty() &&
         Ui::Instance().RunTime() >= vec_expirations_.front())
    RemoveNotification();
}

}
