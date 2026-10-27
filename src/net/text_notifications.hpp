#pragma once
import std;

#include "core/color.hpp"
#include "net/fluent_message.hpp"

namespace ora::sim {
class Player;
}

namespace ora::net {

enum class TextNotificationPool {
  System,
  Join,
  Leave,
  Chat,
  Mission,
  Feedback,
  Transients,
};

enum TextNotificationPoolFilters : int {
  kPoolFilterNone = 0,
  kPoolFilterFeedback = 1,
  kPoolFilterTransients = 2,
};

class TextNotification {
 public:
  TextNotification(TextNotificationPool kind_pool_new, int int4_client_id_new,
                   std::string str_prefix_new, std::string str_text_new,
                   std::optional<core::Color> opt_prefix_color_new,
                   std::optional<core::Color> opt_text_color_new);

  TextNotificationPool Pool() const { return kind_pool; }
  int ClientId() const { return int4_client_id; }
  const std::string& Prefix() const { return str_prefix; }
  const std::string& Text() const { return str_text; }
  const std::optional<core::Color>& PrefixColor() const {
    return opt_prefix_color;
  }
  const std::optional<core::Color>& TextColor() const {
    return opt_text_color;
  }

  bool CanIncrementOnDuplicate() const;

  std::size_t Hash() const;

  bool operator==(const TextNotification& other) const {
    return Hash() == other.Hash();
  }

 private:
  TextNotificationPool kind_pool = TextNotificationPool::System;
  int int4_client_id = 0;
  std::string str_prefix;
  std::string str_text;
  std::optional<core::Color> opt_prefix_color;
  std::optional<core::Color> opt_text_color;

 public:

  std::chrono::system_clock::time_point time_time{};
};

class TextNotificationsManager {
 public:

  static constexpr int kSystemClientId = -1;

  static TextNotificationsManager& Instance();

  TextNotificationsManager(const TextNotificationsManager&) = delete;
  TextNotificationsManager& operator=(const TextNotificationsManager&) =
      delete;

  using FluentFn = std::function<std::string(
      std::string_view, std::span<const FluentArgument>)>;
  void SetFluentProvider(FluentFn fn_fluent) {
    fn_fluent_ = std::move(fn_fluent);
  }

  void SetSystemMessageLabel(std::string str_label) {
    str_system_message_label_ = std::move(str_label);
  }

  void SetSink(std::function<void(const TextNotification&)> fn_sink) {
    fn_sink_ = std::move(fn_sink);
  }

  void SetClock(std::function<std::int64_t()> fn_clock) {
    fn_clock_ = std::move(fn_clock);
  }

  void SetPoolFilters(int int4_filters) { int4_pool_filters_ = int4_filters; }

  std::int64_t ChatDisabledUntil() const { return int8_chat_disabled_until_; }
  void SetChatDisabledUntil(std::int64_t int8_when) {
    int8_chat_disabled_until_ = int8_when;
  }

  std::map<int, bool>& MutedPlayers() { return map_muted_players_; }

  const std::vector<TextNotification>& Notifications() const {
    return vec_notifications_;
  }

  void AddTransientLine(sim::Player* p_player, std::string_view str_text);
  void AddFeedbackLine(std::string_view str_text,
                       std::span<const FluentArgument> vec_args = {});
  void AddMissionLine(std::string_view str_prefix, std::string_view str_text,
                      std::optional<core::Color> opt_prefix_color = {});
  void AddPlayerJoinedLine(std::string_view str_text,
                           std::span<const FluentArgument> vec_args = {});
  void AddPlayerLeftLine(std::string_view str_text,
                         std::span<const FluentArgument> vec_args = {});
  void AddSystemLine(std::string_view str_text,
                     std::span<const FluentArgument> vec_args = {});
  void AddSystemLine(std::string_view str_prefix, std::string_view str_text);
  void AddChatLine(int int4_client_id, std::string_view str_prefix,
                   std::string_view str_text,
                   std::optional<core::Color> opt_prefix_color = {},
                   std::optional<core::Color> opt_text_color = {});
  void Debug(std::string_view str_format);
  void Clear();

  void ResetForTest();

 private:
  TextNotificationsManager() = default;

  void AddTextNotification(TextNotificationPool kind_pool, int int4_client_id,
                           std::string_view str_prefix, std::string_view str_text,
                           std::optional<core::Color> opt_prefix_color = {},
                           std::optional<core::Color> opt_text_color = {});

  bool IsPoolEnabled(TextNotificationPool kind_pool) const;

  std::string FluentGetMessage(
      std::string_view str_key,
      std::span<const FluentArgument> vec_args) const;

  std::string str_system_message_label_ = "Battlefield Control";
  std::int64_t int8_chat_disabled_until_ = 0;
  int int4_pool_filters_ =
      kPoolFilterFeedback | kPoolFilterTransients;
  std::map<int, bool> map_muted_players_;
  std::vector<TextNotification> vec_notifications_;

  FluentFn fn_fluent_;
  std::function<void(const TextNotification&)> fn_sink_;
  std::function<std::int64_t()> fn_clock_ = [] { return std::int64_t{0}; };
};

}
