#pragma once
import std;

#include "fluent/fluent_syntax.hpp"

namespace ora::fluent {

using FileOpenFn =
    std::function<std::optional<std::vector<char>>(const std::string&)>;

struct FluentValue {
  std::variant<std::int64_t, std::string> val;

  bool IsNumber() const { return std::holds_alternative<std::int64_t>(val); }
};

std::string_view PluralCategory(std::int64_t int8_value,
                                std::string_view str_culture);

std::string FormatNumber(std::int64_t int8_value);

class FluentBundle {
 public:

  FluentBundle(std::string str_culture,
               std::span<const std::string> vec_paths,
               FileOpenFn fn_open, std::string_view str_text = {},
               ErrorSink fn_on_error = nullptr);

  FluentBundle(std::string str_culture, std::string_view str_text,
               ErrorSink fn_on_error)
      : FluentBundle(std::move(str_culture), std::span<const std::string>{},
                     nullptr, str_text, std::move(fn_on_error)) {}

  bool HasAttrMessage(std::string_view str_key) const;

  bool TryGetMessage(std::string_view str_key,
                     std::span<const std::pair<std::string, FluentValue>> vec_args,
                     std::string& out_value) const;

  bool TryGetMessage(std::string_view str_key, std::string& out_value) const {
    return TryGetMessage(str_key, std::span<const std::pair<std::string, FluentValue>>{},
                         out_value);
  }

  std::string GetMessage(
      std::string_view str_key,
      std::span<const std::pair<std::string, FluentValue>> vec_args = {}) const;

  void AddResourceText(std::string_view str_text);

  const std::string& Culture() const { return str_culture_; }

 private:
  void IngestResource(FluentResource&& resource_parsed);
  const FluentMessage* FindMessage(std::string_view str_id) const;
  bool EvaluatePattern(const std::vector<PatternElement>& vec_pattern,
                       const std::vector<std::pair<std::string, FluentValue>>& vec_args,
                       std::string& out_value,
                       std::vector<std::string>& vec_errors) const;

  std::string str_culture_;
  std::vector<std::pair<std::string, FluentMessage>> vec_messages_;
  std::vector<std::pair<std::string, Pattern>> vec_terms_;
  ErrorSink fn_on_error_;
};

}
