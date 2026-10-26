// UPSTREAM: OpenRA.Game/TextNotification.cs @1f594e320 L19-54 +
//          OpenRA.Game/TextNotificationsManager.cs L19-117(全文;C#
//          static → 进程级单例;FluentProvider.GetMessage/
//          ChromeMetrics["SystemMessageLabel"]/Ui.Send/Game.RunTime/
//          Game.Settings.Game.TextNotificationPoolFilters 五静态面以注入
//          + 上游字面量缺省承载 —— 见各 setter;线程面同上游:全部调用
//          发生于主线程/OM 驱动线程,无内部锁)
//          The whole of TextNotification.cs L19-54 +
//          TextNotificationsManager.cs L19-117 (the C# static becomes a
//          process-wide singleton; the five static faces —
//          FluentProvider.GetMessage / ChromeMetrics["SystemMessageLabel"]
//          / Ui.Send / Game.RunTime / Game.Settings.Game.
//          TextNotificationPoolFilters — ride injections with upstream's
//          literal defaults, see each setter; threading as upstream: every
//          call lands on the main/OM-driving thread, no internal locks).
//
// 机制对照 / Mechanism mapping:
//  - FluentProvider.GetMessage(key, args)(Phase 6)→ 注入;缺省 = key 回落
//    (第十五批同登记)
//    FluentProvider.GetMessage(key, args) (Phase 6) → injected; the
//    default falls back to the key (registered the same as batch 15).
//  - Ui.Send(notification)(Mediator 直连,Phase 6)→ sink 注入,缺省
//    no-op
//    Ui.Send(notification) (the Mediator direct call, Phase 6) → a sink
//    injection defaulting to no-op.
//  - DateTime.Now → system_clock::now()(UI 展示面专用,不入同步面)
//    DateTime.Now → system_clock::now() (the UI display face only, never
//    a synced face).
#pragma once
import std;

#include "core/color.hpp"
#include "net/fluent_message.hpp"

namespace ora::sim {
class Player;
}  // namespace ora::sim

namespace ora::net {

/// TextNotificationPool(TextNotification.cs L19)
/// TextNotificationPool (TextNotification.cs L19).
enum class TextNotificationPool {
  System,
  Join,
  Leave,
  Chat,
  Mission,
  Feedback,
  Transients,
};

/// Settings.cs L43-48 的 TextNotificationPoolFilters(位标志枚举)
/// Settings.cs L43-48's TextNotificationPoolFilters (a flags enum).
enum TextNotificationPoolFilters : int {
  kPoolFilterNone = 0,
  kPoolFilterFeedback = 1,
  kPoolFilterTransients = 2,
};

/// TextNotification(TextNotification.cs L26-54);operator== 仅比哈希的上游
/// quirk 保真(HashCode.Combine(Prefix, Text, (int)Pool))
/// TextNotification (TextNotification.cs L26-54); upstream's operator==
/// compares hashes only — the quirk kept (HashCode.Combine(Prefix, Text,
/// (int)Pool)).
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

  /// GetHashCode(L49-53)
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
  /// Time(DateTime.Now;公开展示面)
  /// Time (DateTime.Now; the public display face).
  std::chrono::system_clock::time_point time_time{};
};

/// TextNotificationsManager(TextNotificationsManager.cs L19-117);C#
/// static → 进程级单例(Instance)
/// TextNotificationsManager (TextNotificationsManager.cs L19-117); the
/// C# static becomes the process-wide singleton (Instance).
class TextNotificationsManager {
 public:
  /// SystemClientId(L23)
  /// SystemClientId (L23).
  static constexpr int kSystemClientId = -1;

  static TextNotificationsManager& Instance();

  TextNotificationsManager(const TextNotificationsManager&) = delete;
  TextNotificationsManager& operator=(const TextNotificationsManager&) =
      delete;

  // ———— 注入面(上游静态直连面的 C++ 承载)————
  // The injection faces (the C++ carriers of upstream's static calls).

  /// FluentProvider.GetMessage(缺省 = key 回落;第十五批同偏离)
  /// FluentProvider.GetMessage (default = the key fallback; the same
  /// deviation as batch 15).
  using FluentFn = std::function<std::string(
      std::string_view, std::span<const FluentArgument>)>;
  void SetFluentProvider(FluentFn fn_fluent) {
    fn_fluent_ = std::move(fn_fluent);
  }

  /// ChromeMetrics.TryGet("SystemMessageLabel") 的装配面(缺省 = 上游
  /// 回落字面量 "Battlefield Control";chrome.yaml 装载随 Phase 6)
  /// ChromeMetrics.TryGet("SystemMessageLabel")'s assembly face (default =
  /// upstream's fallback literal "Battlefield Control"; the chrome.yaml
  /// load rides Phase 6).
  void SetSystemMessageLabel(std::string str_label) {
    str_system_message_label_ = std::move(str_label);
  }

  /// Ui.Send(notification)(缺省 no-op;Mediator 直连随 Phase 6)
  /// Ui.Send(notification) (default no-op; the Mediator direct call rides
  /// Phase 6).
  void SetSink(std::function<void(const TextNotification&)> fn_sink) {
    fn_sink_ = std::move(fn_sink);
  }

  /// Game.RunTime(缺省恒 0;Clear/DisableChatEntry 消费)
  /// Game.RunTime (default 0; consumed by Clear/DisableChatEntry).
  void SetClock(std::function<std::int64_t()> fn_clock) {
    fn_clock_ = std::move(fn_clock);
  }

  /// Game.Settings.Game.TextNotificationPoolFilters(缺省 = 上游默认
  /// Feedback|Transients;Settings 装载随 Phase 6)
  /// Game.Settings.Game.TextNotificationPoolFilters (default =
  /// upstream's Feedback|Transients; the Settings load rides Phase 6).
  void SetPoolFilters(int int4_filters) { int4_pool_filters_ = int4_filters; }

  // ———— 公开面(TextNotificationsManager.cs)————
  // The public faces (TextNotificationsManager.cs).

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

  /// 测试隔离面:状态 + 全部注入回缺省(上游无此面)
  /// The test-isolation face: state + every injection back to the defaults
  /// (no upstream counterpart).
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
      kPoolFilterFeedback | kPoolFilterTransients;  // L362 默认 | the L362 default
  std::map<int, bool> map_muted_players_;
  std::vector<TextNotification> vec_notifications_;

  FluentFn fn_fluent_;
  std::function<void(const TextNotification&)> fn_sink_;
  std::function<std::int64_t()> fn_clock_ = [] { return std::int64_t{0}; };
};

}  // namespace ora::net
