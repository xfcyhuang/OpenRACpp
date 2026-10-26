// UPSTREAM: OpenRA.Game/FluentBundle.cs @1f594e320(经 Linguini.Bundle/
// Linguini.Syntax 的 Fluent 0.x 子集自研承载 —— 上游依赖 Linguini 0.8.1 的
// 解析与求值,本侧按 Fluent 语法规范 + mods 语料实测面逐语义重写;语料
// 未用的面(term 引用/内建函数/消息引用)按解析错误落 junk,与上游
// Linguini 报错误但继续装载后续条目的可观察行为对齐)。
// Self-written carrier of the Fluent 0.x subset that stands in for the
// upstream Linguini.Bundle/Linguini.Syntax dependency — parsed and evaluated
// per the Fluent syntax specification and the mods corpus's measured face;
// corpus-unused faces (term references/builtin functions/message references)
// fall to junk as parse errors, matching the observable shape of upstream
// Linguini logging errors but continuing with the remaining entries.
//
// 机制对照 / Mechanism mapping:
//  - 资源(Linguini Syntax.Ast.Resource):条目表(消息/注释/junk 跳过);
//    解析错误经 ErrorSink 报告(上游 ParseError → Log.Write("debug"))
//    The resource (Linguini Syntax.Ast.Resource): the entry table
//    (messages/comments/junk skipped); parse errors surface through the
//    ErrorSink (upstream's ParseError → Log.Write("debug")).
//  - 消息(Syntax.Ast.Message):主值 + 属性行(.name = …);选择子
//    ({$var -> [key] … *[key] …})为其内嵌递归形态
//    The message (Syntax.Ast.Message): the main value + attribute lines
//    (.name = …); the selector ({$var -> [key] … *[key] …}) is its nested
//    recursive form.
//  - 空白规范:公共缩进扣除 + 逐行尾空白修剪 + placeable 邻接修剪
//    (Fluent 规范 Patterns 节;Linguini 实测差异由 golden_fluent 对拍钉住)
//    Whitespace normalization: the common-indent deduction + the per-line
//    trailing trim + the placeable-adjacent trim (the Fluent spec's
//    Patterns section; measured Linguini divergences pinned by the
//    golden_fluent differential).
#pragma once
import std;

namespace ora::fluent {

/// 解析/求值错误报告面(上游 error => Log.Write("debug", error.Message))
/// The parse/eval error report face (upstream's error =>
/// Log.Write("debug", error.Message)).
using ErrorSink = std::function<void(std::string_view)>;

/// 选择子变体([key] / *[key];键 = 精确字面或 CLDR 复数类别名);变体模式
/// 以元素表直载(模式 = 元素序列)
/// A selector variant ([key] / *[key]; the key = an exact literal or a
/// CLDR plural-category name); the variant's pattern rides inline as its
/// element list (a pattern = an element sequence).
struct PatternElement;

struct Variant {
  std::string str_key;
  bool b_default = false;
  std::vector<PatternElement> vec_pattern;
};

/// placeable({$var} / {"literal"} / 选择子)
/// A placeable ({$var} / {"literal"} / a selector).
struct Placeable {
  enum class Kind { Variable, StringLiteral, Selector, TermRef };
  Kind kind = Kind::Variable;
  std::string str_name;  // 变量名或字面值 | the variable name or literal
  std::vector<Variant> vec_variants;
};

/// 模式元素:文本段或 placeable(placeable 经间接持有,解
/// Pattern↔Placeable 值环)| A pattern element: a text run or a placeable
/// (the placeable held indirectly, breaking the Pattern↔Placeable
/// by-value cycle).
struct PatternElement {
  bool b_text = true;
  std::string str_text;
  std::unique_ptr<Placeable> ptr_placeable;
};

/// 模式(元素序列;行折为文本段内的 '\n')| A pattern (the element
/// sequence; line breaks live inside text runs as '\n').
struct Pattern {
  std::vector<PatternElement> vec_elements;
};

/// 一条消息(主值可缺 —— 上游 button-x = 仅含属性行的合法形态)
/// One message (the main value may be absent — upstream's legal
/// button-x = attribute-only form).
struct FluentMessage {
  bool b_has_value = false;
  Pattern pattern_value;
  std::vector<std::pair<std::string, Pattern>> vec_attributes;
};

/// 解析产物(插入序消息/术语表;错误计数供诊断)
/// The parse product (the insertion-ordered message/term tables; the error
/// count feeds diagnostics).
struct FluentResource {
  std::vector<std::pair<std::string, FluentMessage>> vec_messages;
  std::vector<std::pair<std::string, Pattern>> vec_terms;
  std::size_t num_errors = 0;
};

/// 解析一份资源文本(UTF-8;错误容错 = junk 跳至下一合法条目)
/// Parses one resource text (UTF-8; error tolerance = junk skipped to the
/// next valid entry).
FluentResource ParseResource(std::string_view str_text,
                             const ErrorSink& fn_on_error);

}  // namespace ora::fluent
