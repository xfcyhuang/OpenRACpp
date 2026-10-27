#pragma once
import std;

#include "core/rectangle.hpp"
#include "net/text_notifications.hpp"
#include "ui/widget.hpp"

namespace ora::ui {

class TextNotificationsDisplayWidget : public Widget {
 public:
  static constexpr std::string_view kWidgetTypeName = "TextNotificationsDisplayWidget";

  TextNotificationsDisplayWidget() = default;
  explicit TextNotificationsDisplayWidget(WidgetArgs& args_args) {
    (void)args_args;
  }
  ~TextNotificationsDisplayWidget() override = default;

  std::string_view TypeName() const override { return kWidgetTypeName; }

  void Initialize(WidgetArgs& args_args) override;
  bool LoadFieldOrProperty(std::string_view str_key, std::string_view str_value) override;
  Rectangle EventBounds() const override { return Rectangle{}; }
  void DrawOuter() override;
  void Tick() override;

  void AddNotification(const net::TextNotification& notification_new);
  void RemoveMostRecentNotification();

  int int4_display_duration_ms = 0;
  int int4_item_spacing = 4;
  int int4_bottom_spacing = 0;
  int int4_log_length = 8;
  bool b_hide_overflow = true;

  std::string str_chat_template = "CHAT_LINE_TEMPLATE";
  std::string str_system_template = "SYSTEM_LINE_TEMPLATE";
  std::string str_mission_template = "CHAT_LINE_TEMPLATE";
  std::string str_feedback_template = "TRANSIENT_LINE_TEMPLATE";
  std::string str_transients_template = "TRANSIENT_LINE_TEMPLATE";

 private:
  void RemoveNotification();

  std::map<net::TextNotificationPool, std::unique_ptr<Widget>> map_templates_;
  std::vector<std::int64_t> vec_expirations_;
  Rectangle rect_overflow_draw_bounds_{};
};

}
