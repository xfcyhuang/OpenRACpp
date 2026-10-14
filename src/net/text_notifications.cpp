// UPSTREAM: OpenRA.Game/TextNotificationsManager.cs @e180e3225 L26-117
//          (实现全文)
//          (the whole implementation).
import std;

#include "net/text_notifications.hpp"

#include "fluent/fluent_provider.hpp"

#include "sim/player.hpp"
#include "sim/world.hpp"

namespace ora::net {

namespace {

/// HashCode.Combine(Prefix, Text, (int)Pool) 的等价复合(展开序 = 上游
/// Combine 的种子链:hash(Prefix) → 混入 hash(Text) → 混入 Pool)
/// The equivalent composite of HashCode.Combine(Prefix, Text, (int)Pool)
/// (the expansion order = upstream Combine's seed chain: hash(Prefix) →
/// fold hash(Text) → fold Pool).
std::size_t CombineHash(std::size_t seed, std::size_t value) {
  const std::size_t int8_rotl = 5;
  return seed ^ (value + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2) +
                 (seed << int8_rotl));
}

}  // namespace

TextNotification::TextNotification(
    TextNotificationPool kind_pool_new, int int4_client_id_new,
    std::string str_prefix_new, std::string str_text_new,
    std::optional<core::Color> opt_prefix_color_new,
    std::optional<core::Color> opt_text_color_new)
    : kind_pool(kind_pool_new),
      int4_client_id(int4_client_id_new),
      str_prefix(std::move(str_prefix_new)),
      str_text(std::move(str_text_new)),
      opt_prefix_color(opt_prefix_color_new),
      opt_text_color(opt_text_color_new),
      time_time(std::chrono::system_clock::now()) {}

bool TextNotification::CanIncrementOnDuplicate() const {
  return kind_pool == TextNotificationPool::Feedback ||
         kind_pool == TextNotificationPool::System ||
         kind_pool == TextNotificationPool::Transients;
}

std::size_t TextNotification::Hash() const {
  return CombineHash(CombineHash(std::hash<std::string>{}(str_prefix),
                                 std::hash<std::string>{}(str_text)),
                     static_cast<std::size_t>(static_cast<int>(kind_pool)));
}

TextNotificationsManager& TextNotificationsManager::Instance() {
  static TextNotificationsManager instance;
  return instance;
}

std::string TextNotificationsManager::FluentGetMessage(
    std::string_view str_key, std::span<const FluentArgument> vec_args) const {
  if (fn_fluent_)
    return fn_fluent_(str_key, vec_args);
  // 上游直调 FluentProvider.GetMessage;未初始化 = 恒未中 → 键回落
  // upstream calls FluentProvider.GetMessage directly; uninitialized =
  // never a hit → the key fallback.
  std::vector<std::pair<std::string, fluent::FluentValue>> vec_fluent_args;
  vec_fluent_args.reserve(vec_args.size());
  for (const FluentArgument& arg : vec_args) {
    std::variant<std::int64_t, std::string> val;
    if (arg.type == FluentArgumentType::Number) {
      std::int64_t int8_parsed = 0;
      std::from_chars(arg.str_value.data(), arg.str_value.data() + arg.str_value.size(),
                      int8_parsed);
      val = int8_parsed;
    } else {
      val = arg.str_value;
    }
    vec_fluent_args.emplace_back(arg.str_key, fluent::FluentValue{std::move(val)});
  }
  return fluent::FluentProvider::Instance().GetMessage(str_key, vec_fluent_args);
}

bool TextNotificationsManager::IsPoolEnabled(
    TextNotificationPool kind_pool) const {
  // TextNotificationPoolFilters 的 Transients/Feedback 位(Settings.cs
  // L43-48);其余池恒放行
  // The Transients/Feedback bits of TextNotificationPoolFilters
  // (Settings.cs L43-48); every other pool always passes.
  switch (kind_pool) {
    case TextNotificationPool::Transients:
      return (int4_pool_filters_ & kPoolFilterTransients) != 0;
    case TextNotificationPool::Feedback:
      return (int4_pool_filters_ & kPoolFilterFeedback) != 0;
    default:
      return true;
  }
}

void TextNotificationsManager::AddTextNotification(
    TextNotificationPool kind_pool, int int4_client_id,
    std::string_view str_prefix, std::string_view str_text,
    std::optional<core::Color> opt_prefix_color,
    std::optional<core::Color> opt_text_color) {
  if (!IsPoolEnabled(kind_pool))
    return;

  vec_notifications_.emplace_back(kind_pool, int4_client_id,
                                  std::string{str_prefix},
                                  std::string{str_text}, opt_prefix_color,
                                  opt_text_color);
  if (fn_sink_)
    fn_sink_(vec_notifications_.back());
}

void TextNotificationsManager::AddTransientLine(sim::Player* p_player,
                                                std::string_view str_text) {
  // L34-40:空文本直返;null/本地玩家才上屏
  // L34-40: empty text returns; only null/the local player reaches the
  // screen.
  if (str_text.empty())
    return;

  if (p_player == nullptr || p_player->GetWorld().LocalPlayer() == p_player)
    AddTextNotification(TextNotificationPool::Transients, kSystemClientId,
                        str_system_message_label_,
                        FluentGetMessage(str_text, {}));
}

void TextNotificationsManager::AddFeedbackLine(
    std::string_view str_text, std::span<const FluentArgument> vec_args) {
  AddTextNotification(TextNotificationPool::Feedback, kSystemClientId,
                      str_system_message_label_,
                      FluentGetMessage(str_text, vec_args));
}

void TextNotificationsManager::AddMissionLine(
    std::string_view str_prefix, std::string_view str_text,
    std::optional<core::Color> opt_prefix_color) {
  AddTextNotification(TextNotificationPool::Mission, kSystemClientId,
                      str_prefix, str_text, opt_prefix_color);
}

void TextNotificationsManager::AddPlayerJoinedLine(
    std::string_view str_text, std::span<const FluentArgument> vec_args) {
  AddTextNotification(TextNotificationPool::Join, kSystemClientId,
                      str_system_message_label_,
                      FluentGetMessage(str_text, vec_args));
}

void TextNotificationsManager::AddPlayerLeftLine(
    std::string_view str_text, std::span<const FluentArgument> vec_args) {
  AddTextNotification(TextNotificationPool::Leave, kSystemClientId,
                      str_system_message_label_,
                      FluentGetMessage(str_text, vec_args));
}

void TextNotificationsManager::AddSystemLine(
    std::string_view str_text, std::span<const FluentArgument> vec_args) {
  AddSystemLine(str_system_message_label_, FluentGetMessage(str_text, vec_args));
}

void TextNotificationsManager::AddSystemLine(std::string_view str_prefix,
                                             std::string_view str_text) {
  AddTextNotification(TextNotificationPool::System, kSystemClientId,
                      str_prefix, str_text);
}

void TextNotificationsManager::AddChatLine(
    int int4_client_id, std::string_view str_prefix, std::string_view str_text,
    std::optional<core::Color> opt_prefix_color,
    std::optional<core::Color> opt_text_color) {
  AddTextNotification(TextNotificationPool::Chat, int4_client_id, str_prefix,
                      str_text, opt_prefix_color, opt_text_color);
}

void TextNotificationsManager::Debug(std::string_view str_format) {
  AddSystemLine("Debug", str_format);
}

void TextNotificationsManager::Clear() {
  int8_chat_disabled_until_ = fn_clock_();
  vec_notifications_.clear();
  map_muted_players_.clear();
}

void TextNotificationsManager::ResetForTest() {
  Clear();
  int4_pool_filters_ = kPoolFilterFeedback | kPoolFilterTransients;
  str_system_message_label_ = "Battlefield Control";
  fn_fluent_ = nullptr;
  fn_sink_ = nullptr;
  fn_clock_ = [] { return std::int64_t{0}; };
}

}  // namespace ora::net
