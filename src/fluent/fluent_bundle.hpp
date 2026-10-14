// UPSTREAM: OpenRA.Game/FluentBundle.cs L87-201 @e180e3225(逐语义重写;
// Linguini.Bundle 的承载面 = 自研 Fluent 子集)。args 的 name/value 对校验
// 与异常吞并 → false + 错误报告(上游 ArgumentException/ArgumentNullException
// 由 catch(Exception) 转 Log.Write + false;错误路径文本仅在 debug 日志可
// 观察,不进入返回值面)。
// A verbatim-semantics rewrite of OpenRA.Game/FluentBundle.cs L87-201 (the
// carrier face of Linguini.Bundle = the self-written Fluent subset). The
// args' name/value-pair validation and the exception swallowing become
// false + an error report (upstream's ArgumentException/ArgumentNullException
// reach the catch(Exception) → Log.Write + false; the error-path texts are
// observable only in the debug log, never through the return face).
#pragma once
import std;

#include "fluent/fluent_syntax.hpp"

namespace ora::fluent {

/// 文件打开面(上游 IReadOnlyFileSystem.Open 的注入等价;C++ 侧
/// FileSystem/Map 为具体类无公共接口,以函数封装器承载 —— 偏离登记
/// 见 COVERAGE)
/// The file-open face (the injected equivalent of upstream
/// IReadOnlyFileSystem.Open; the C++ FileSystem/Map are concrete classes
/// without a common interface, carried by a function wrapper — see the
/// COVERAGE registry).
using FileOpenFn =
    std::function<std::optional<std::vector<char>>(const std::string&)>;

/// Fluent 变量值(IFluentType 的语料闭包:整数与字符串)
/// A Fluent variable value (the corpus closure of IFluentType: integers and
/// strings).
struct FluentValue {
  std::variant<std::int64_t, std::string> val;

  bool IsNumber() const { return std::holds_alternative<std::int64_t>(val); }
};

/// CLDR 基数复数类别(en 全规则;其余 culture 以 en 规则承载 —— 语料仅
/// en,偏离见 COVERAGE 登记表)
/// The CLDR cardinal plural category (the full en rule; other cultures ride
/// the en rule — the corpus is en-only, see the COVERAGE registry).
std::string_view PluralCategory(std::int64_t int8_value,
                                std::string_view str_culture);

/// 数字面渲染(无分组;上游 FluentNumber 的 en 文化面)
/// The numeric-literal rendering (no grouping; upstream FluentNumber's en
/// face).
std::string FormatNumber(std::int64_t int8_value);

/// FluentBundle(L87-201)
/// FluentBundle (L87-201).
class FluentBundle {
 public:
  /// L91-92 / L97-98:文件集装载路径 + 文本面(缺省错误日志 = no-op)
  /// L91-92 / L97-98: the file-set loading paths + the text face (the
  /// default error log = no-op).
  FluentBundle(std::string str_culture,
               std::span<const std::string> vec_paths,
               FileOpenFn fn_open, std::string_view str_text = {},
               ErrorSink fn_on_error = nullptr);

  /// L100-101 / L103-110:纯文本构造面(测试/微用例)
  /// L100-101 / L103-110: the text-only construction face (tests/micro
  /// cases).
  FluentBundle(std::string str_culture, std::string_view str_text,
               ErrorSink fn_on_error)
      : FluentBundle(std::move(str_culture), std::span<const std::string>{},
                     nullptr, str_text, std::move(fn_on_error)) {}

  /// L197-200:HasAttrMessage(消息存在且带值或属性行)
  /// L197-200: HasAttrMessage (the message exists with a value or attribute
  /// lines).
  bool HasAttrMessage(std::string_view str_key) const;

  /// L148-195:TryGetMessage —— 键 = "msg" 或 "msg.attr";args =
  /// name/value 对;失败 false 且 out 清空
  /// L148-195: TryGetMessage — the key = "msg" or "msg.attr"; the args =
  /// name/value pairs; a miss returns false with out cleared.
  bool TryGetMessage(std::string_view str_key,
                     std::span<const std::pair<std::string, FluentValue>> vec_args,
                     std::string& out_value) const;

  bool TryGetMessage(std::string_view str_key, std::string& out_value) const {
    return TryGetMessage(str_key, std::span<const std::pair<std::string, FluentValue>>{},
                         out_value);
  }

  /// L140-146:GetMessage(未中回落键)
  /// L140-146: GetMessage (a miss falls back to the key).
  std::string GetMessage(
      std::string_view str_key,
      std::span<const std::pair<std::string, FluentValue>> vec_args = {}) const;

  /// 文本资源叠加(L112-127 的 AddResourceOverriding 等价;同键覆盖)
  /// The text-resource overlay (the AddResourceOverriding equivalent of
  /// L112-127; same-key overwrite).
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

}  // namespace ora::fluent
