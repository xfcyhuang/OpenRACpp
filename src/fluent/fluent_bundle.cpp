// UPSTREAM: OpenRA.Game/FluentBundle.cs L87-201 @1f594e320 —— 求值承载
// (Linguini.Bundle.TryGetAttrMessage 的等价面;选择子 = 精确数字键 →
// CLDR 类别 → 缺省变体;未解析变量按 Fluent 规范回退为其源文本
// "{ $name }"并计错误)。
// The evaluation carrier of OpenRA.Game/FluentBundle.cs L87-201 (the
// equivalent face of Linguini.Bundle.TryGetAttrMessage; a selector = the
// exact numeric key, then the CLDR category, then the default variant; an
// unresolved variable falls back to its source text "{ $name }" per the
// Fluent spec and counts as an error).
#include "fluent/fluent_bundle.hpp"

namespace ora::fluent {
namespace {

/// CLDR en 基数规则:one ⇔ i%10==1 && i%100!=11(整数面;其余 → other)
/// The CLDR en cardinal rule: one ⇔ i%10==1 && i%100!=11 (the integer
/// face; everything else → other).
constexpr std::string_view kCategoryOne = "one";
constexpr std::string_view kCategoryOther = "other";

bool ParseNumericKey(std::string_view str_key, std::int64_t& out_value) {
  if (str_key.empty())
    return false;
  std::int64_t int8_value = 0;
  std::size_t uint_i = str_key.front() == '-' ? 1 : 0;
  if (uint_i >= str_key.size())
    return false;
  for (; uint_i < str_key.size(); ++uint_i) {
    const char ch = str_key[uint_i];
    if (ch < '0' || ch > '9')
      return false;
    int8_value = int8_value * 10 + (ch - '0');
  }
  if (str_key.front() == '-')
    int8_value = -int8_value;
  out_value = int8_value;
  return true;
}

}  // namespace

std::string_view PluralCategory(std::int64_t int8_value,
                                std::string_view str_culture) {
  (void)str_culture;
  const std::int64_t int8_abs = int8_value < 0 ? -int8_value : int8_value;
  if (int8_abs % 10 == 1 && int8_abs % 100 != 11)
    return kCategoryOne;
  return kCategoryOther;
}

std::string FormatNumber(std::int64_t int8_value) {
  if (int8_value == 0)
    return "0";
  const bool b_negative = int8_value < 0;
  std::uint64_t uint8_abs =
      b_negative ? static_cast<std::uint64_t>(~int8_value) + 1
                 : static_cast<std::uint64_t>(int8_value);
  std::string str_digits;
  while (uint8_abs != 0) {
    str_digits.push_back(static_cast<char>('0' + uint8_abs % 10));
    uint8_abs /= 10;
  }
  if (b_negative)
    str_digits.push_back('-');
  std::ranges::reverse(str_digits);
  return str_digits;
}

FluentBundle::FluentBundle(std::string str_culture,
                           std::span<const std::string> vec_paths,
                           FileOpenFn fn_open, std::string_view str_text,
                           ErrorSink fn_on_error)
    : str_culture_(std::move(str_culture)),
      fn_on_error_(std::move(fn_on_error)) {
  if (fn_open) {
    for (const std::string& str_path : vec_paths) {
      const std::optional<std::vector<char>> opt_bytes = fn_open(str_path);
      if (!opt_bytes.has_value()) {
        if (fn_on_error_)
          fn_on_error_("fluent: cannot open resource " + str_path);
        continue;
      }
      FluentResource resource_parsed = ParseResource(
          std::string_view(opt_bytes->data(), opt_bytes->size()), fn_on_error_);
      IngestResource(std::move(resource_parsed));
    }
  }
  if (!str_text.empty())
    AddResourceText(str_text);
}

void FluentBundle::AddResourceText(std::string_view str_text) {
  IngestResource(ParseResource(str_text, fn_on_error_));
}

void FluentBundle::IngestResource(FluentResource&& resource_parsed) {
  // AddResourceOverriding:同键原位覆盖,否则追加(消息与术语两表)
  // AddResourceOverriding: a same-key in-place overwrite, else appended
  // (the message and term tables both).
  for (auto& [str_id, message_entry] : resource_parsed.vec_messages) {
    bool b_overridden = false;
    for (auto& [str_existing, message_existing] : vec_messages_)
      if (str_existing == str_id) {
        message_existing = std::move(message_entry);
        b_overridden = true;
        break;
      }
    if (!b_overridden)
      vec_messages_.emplace_back(std::move(str_id), std::move(message_entry));
  }
  for (auto& [str_id, pattern_term] : resource_parsed.vec_terms) {
    bool b_overridden = false;
    for (auto& [str_existing, pattern_existing] : vec_terms_)
      if (str_existing == str_id) {
        pattern_existing = std::move(pattern_term);
        b_overridden = true;
        break;
      }
    if (!b_overridden)
      vec_terms_.emplace_back(std::move(str_id), std::move(pattern_term));
  }
}

const FluentMessage* FluentBundle::FindMessage(std::string_view str_id) const {
  for (const auto& [str_existing, message_existing] : vec_messages_)
    if (str_existing == str_id)
      return &message_existing;
  return nullptr;
}

bool FluentBundle::HasAttrMessage(std::string_view str_key) const {
  const std::size_t uint_dot = str_key.find('.');
  const FluentMessage* ptr_message =
      FindMessage(uint_dot == std::string_view::npos
                      ? str_key
                      : str_key.substr(0, uint_dot));
  if (ptr_message == nullptr)
    return false;
  if (uint_dot == std::string_view::npos)
    return ptr_message->b_has_value || !ptr_message->vec_attributes.empty();
  const std::string_view str_attr = str_key.substr(uint_dot + 1);
  for (const auto& [str_name, pattern_attr] : ptr_message->vec_attributes)
    if (str_name == str_attr)
      return true;
  return false;
}

bool FluentBundle::EvaluatePattern(
    const std::vector<PatternElement>& vec_pattern,
    const std::vector<std::pair<std::string, FluentValue>>& vec_args,
    std::string& out_value, std::vector<std::string>& vec_errors) const {
  for (const PatternElement& elem : vec_pattern) {
    if (elem.b_text) {
      out_value.append(elem.str_text);
      continue;
    }
    const Placeable& placeable_elem = *elem.ptr_placeable;
    if (placeable_elem.kind == Placeable::Kind::StringLiteral) {
      out_value.append(placeable_elem.str_name);
      continue;
    }
    if (placeable_elem.kind == Placeable::Kind::Variable) {
      bool b_found = false;
      for (const auto& [str_name, val_arg] : vec_args)
        if (str_name == placeable_elem.str_name) {
          if (const std::int64_t* ptr_number = std::get_if<std::int64_t>(&val_arg.val))
            out_value.append(FormatNumber(*ptr_number));
          else
            out_value.append(std::get<std::string>(val_arg.val));
          b_found = true;
          break;
        }
      if (!b_found) {
        // 未解析变量回退为其源文本(无空格形;Linguini 0.8.1 实测)
        // the unresolved variable falls back to its source text (the
        // spaceless form; the measured Linguini 0.8.1 face).
        out_value.append("{$");
        out_value.append(placeable_elem.str_name);
        out_value.append("}");
        vec_errors.push_back("fluent: unresolved variable $" + placeable_elem.str_name);
      }
      continue;
    }

    if (placeable_elem.kind == Placeable::Kind::TermRef) {
      const Pattern* ptr_term = nullptr;
      for (const auto& [str_existing, pattern_existing] : vec_terms_)
        if (str_existing == placeable_elem.str_name) {
          ptr_term = &pattern_existing;
          break;
        }
      if (ptr_term == nullptr) {
        vec_errors.push_back("fluent: unresolved term -" + placeable_elem.str_name);
        continue;
      }
      if (!EvaluatePattern(ptr_term->vec_elements, vec_args, out_value, vec_errors))
        return false;
      continue;
    }

    // 选择子:取变量值 → 精确数字键 → CLDR 类别 → 缺省
    // the selector: fetch the variable value → the exact numeric key →
    // the CLDR category → the default.
    const FluentValue* ptr_selector_value = nullptr;
    for (const auto& [str_name, val_arg] : vec_args)
      if (str_name == placeable_elem.str_name) {
        ptr_selector_value = &val_arg;
        break;
      }
    const Variant* ptr_variant_chosen = nullptr;
    if (ptr_selector_value != nullptr) {
      if (const std::int64_t* ptr_number =
              std::get_if<std::int64_t>(&ptr_selector_value->val)) {
        for (const Variant& variant_entry : placeable_elem.vec_variants) {
          std::int64_t int8_key = 0;
          if (ParseNumericKey(variant_entry.str_key, int8_key) &&
              int8_key == *ptr_number) {
            ptr_variant_chosen = &variant_entry;
            break;
          }
        }
        if (ptr_variant_chosen == nullptr) {
          const std::string_view str_category =
              PluralCategory(*ptr_number, str_culture_);
          for (const Variant& variant_entry : placeable_elem.vec_variants)
            if (variant_entry.str_key == str_category) {
              ptr_variant_chosen = &variant_entry;
              break;
            }
        }
      } else {
        const std::string& str_selector =
            std::get<std::string>(ptr_selector_value->val);
        for (const Variant& variant_entry : placeable_elem.vec_variants)
          if (variant_entry.str_key == str_selector) {
            ptr_variant_chosen = &variant_entry;
            break;
          }
      }
    } else {
      // 缺选择子值 → 默认变体(错误记账;Linguini 0.8.1 实测)
      // a missing selector value → the default variant (the error kept;
      // the measured Linguini 0.8.1 face).
      vec_errors.push_back("fluent: unresolved selector variable $" +
                           placeable_elem.str_name);
    }
    if (ptr_variant_chosen == nullptr)
      for (const Variant& variant_entry : placeable_elem.vec_variants)
        if (variant_entry.b_default) {
          ptr_variant_chosen = &variant_entry;
          break;
        }
    if (ptr_variant_chosen == nullptr) {
      vec_errors.push_back("fluent: selector without default for $" +
                           placeable_elem.str_name);
      continue;
    }
    if (!EvaluatePattern(ptr_variant_chosen->vec_pattern, vec_args, out_value,
                         vec_errors))
      return false;
  }
  return true;
}

bool FluentBundle::TryGetMessage(
    std::string_view str_key,
    std::span<const std::pair<std::string, FluentValue>> vec_args,
    std::string& out_value) const {
  out_value.clear();
  if (str_key.empty())
    return false;

  if (!HasAttrMessage(str_key))
    return false;

  const std::size_t uint_dot = str_key.find('.');
  const FluentMessage* ptr_message =
      FindMessage(uint_dot == std::string_view::npos
                      ? str_key
                      : str_key.substr(0, uint_dot));
  if (ptr_message == nullptr)
    return false;

  const Pattern* ptr_pattern = nullptr;
  if (uint_dot == std::string_view::npos) {
    if (!ptr_message->b_has_value)
      return false;
    ptr_pattern = &ptr_message->pattern_value;
  } else {
    const std::string_view str_attr = str_key.substr(uint_dot + 1);
    for (const auto& [str_name, pattern_attr] : ptr_message->vec_attributes)
      if (str_name == str_attr) {
        ptr_pattern = &pattern_attr;
        break;
      }
    if (ptr_pattern == nullptr)
      return false;
  }

  // 上游 catch(Exception) 面:求值错误吞并为 false + 日志
  // the upstream catch(Exception) face: eval errors swallow into false +
  // the log.
  std::vector<std::pair<std::string, FluentValue>> vec_args_local(
      vec_args.begin(), vec_args.end());
  std::string str_result;
  std::vector<std::string> vec_errors;
  if (!EvaluatePattern(ptr_pattern->vec_elements, vec_args_local, str_result,
                      vec_errors))
    return false;
  if (fn_on_error_)
    for (const std::string& str_error : vec_errors)
      fn_on_error_("FluentBundle of " + std::string(str_key) + ": " + str_error);
  out_value = std::move(str_result);
  return true;
}

std::string FluentBundle::GetMessage(
    std::string_view str_key,
    std::span<const std::pair<std::string, FluentValue>> vec_args) const {
  std::string str_message;
  if (!TryGetMessage(str_key, vec_args, str_message))
    return std::string{str_key};
  return str_message;
}

}  // namespace ora::fluent
