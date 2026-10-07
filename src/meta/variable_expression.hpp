// UPSTREAM: OpenRA.Game/Support/VariableExpression.cs @b6fc03f L22-983(全文件逐语义重写)
//          Full-file statement-by-statement rewrite of upstream
//          VariableExpression.cs.
//
// 语义要点 / Semantic notes:
//  - 构造即完成 tokenize + 语法校验 + 后缀化;所有构造期错误消息逐字一致
//    (C# InvalidDataException 的 Message 在 FieldLoader 侧被包上
//    "FieldLoader: Cannot parse ..." 前缀后重抛,此处保留同构异常链)
//    Construction performs tokenizing + syntax validation + postfix
//    conversion; every construction-time error message is verbatim (the C#
//    InvalidDataException message gets the "FieldLoader: Cannot parse ..."
//    prefix added on rethrow, and this side keeps the same exception chain).
//  - 求值:C# 经 System.Linq.Expressions 编译为委托;此处以同构后缀栈机求值,
//    语义逐条对应(非短路 And/Or、除零/模零得 0、int↔bool 隐式互转:
//    int→bool 非 0 即真,bool→int 1/0)
//    Evaluation: C# compiles to a delegate via System.Linq.Expressions; this
//    side evaluates with an isomorphic postfix stack machine (non-short-circuit
//    And/Or, divide/modulo by zero yield 0, implicit int↔bool conversions:
//    int→bool tests non-zero, bool→int yields 1/0).
//  - Variables 集合按首次出现序去重(上游 HashSet 插入序)
//    The Variables set deduplicates in first-appearance order (the upstream
//    HashSet insertion order).
#pragma once
import std;

namespace ora::expr {

/// 表达式构造期错误(等价 C# InvalidDataException;消息逐字)
/// Construction-time error (equivalent of the C# InvalidDataException;
/// verbatim messages).
class ExpressionException : public std::runtime_error {
 public:
  using std::runtime_error::runtime_error;
};

/// token 类型(上游 VariableExpression.TokenType L113-144)
/// Token types (upstream VariableExpression.TokenType L113-144).
enum class TokenType : std::uint8_t {
  False = 0, True, Number, Variable,
  OpenParen, CloseParen,
  Not, Negate, OnesComplement,
  And, Or,
  Equals, NotEquals,
  LessThan, LessThanOrEqual, GreaterThan, GreaterThanOrEqual,
  Add, Subtract, Multiply, Divide, Modulo,
  Invalid
};

/// 内部 token(元数据查询方法实现见 .cpp)
/// Internal token (metadata queries implemented in the .cpp).
struct Token {
  TokenType type{};
  std::size_t int4_index{};      // 起始下标(错误消息)/ start index (for messages)
  std::int32_t int4_number{};    // Number 字面值 / Number literal
  // Number/Variable 原文。深拷贝持有(上游 C# string 引用类型无悬垂;
  // string_view 会指向表达式首次构造处的 str_expression_,对象经
  // move/拷贝迁移后悬垂 —— 第七批 Cloak 的 PauseOnCondition 实证)
  // The Number/Variable source text, held by deep copy (upstream's C#
  // string is a reference type with no dangling; a string_view would
  // point at the first construction site's str_expression_ and dangle
  // once the object moves — proven live by batch-7's Cloak
  // PauseOnCondition).
  std::string str_symbol;

  int Precedence() const;
  bool LeftOperand() const;
  bool RightOperand() const;
  bool Opens() const;
  bool Closes() const;
  std::string_view Symbol() const;
};

/// 变量表达式基类(BooleanExpression/IntegerExpression 共用解析与求值核)
/// The variable-expression base (the parse/evaluate core shared by
/// BooleanExpression/IntegerExpression).
class VariableExpression {
 public:
  /// 构造并解析;语法错误抛 ExpressionException(消息与 C# 逐字一致)
  /// Constructs and parses; syntax errors throw ExpressionException with the
  /// C#-verbatim message.
  explicit VariableExpression(std::string str_expression);

  const std::string& Expression() const { return str_expression_; }
  std::span<const std::string> Variables() const { return vec_variables_; }
  std::string ToString() const { return str_expression_; }

  /// 求值为 bool(int 栈顶按非零转真;表达式类型混合时按 C# AstStack 转换语义)
  /// Evaluates to bool (int stack tops convert via non-zero; mixed expression
  /// types follow the C# AstStack conversion semantics).
  bool EvaluateBool(std::span<const std::pair<std::string, std::int32_t>> vec_symbols) const;
  /// 求值为 int(bool 栈顶转 1/0)
  /// Evaluates to int (bool stack tops convert to 1/0).
  std::int32_t EvaluateInt(std::span<const std::pair<std::string, std::int32_t>> vec_symbols) const;

 private:
  std::int32_t EvaluateAsInt(std::span<const std::pair<std::string, std::int32_t>> vec_symbols) const;
  bool EvaluateAsBool(std::span<const std::pair<std::string, std::int32_t>> vec_symbols) const;

  std::string str_expression_;
  std::vector<std::string> vec_variables_;  // 首现序去重 / first-appearance dedup
  std::vector<Token> vec_postfix_;          // 后缀 token 流 / postfix token stream
};

/// BooleanExpression(VariableExpression.cs L953-967)
class BooleanExpression : public VariableExpression {
 public:
  explicit BooleanExpression(std::string str_expression)
      : VariableExpression(std::move(str_expression)) {}

  bool Evaluate(std::span<const std::pair<std::string, std::int32_t>> vec_symbols) const {
    return EvaluateBool(vec_symbols);
  }
};

/// IntegerExpression(VariableExpression.cs L969-983)
class IntegerExpression : public VariableExpression {
 public:
  explicit IntegerExpression(std::string str_expression)
      : VariableExpression(std::move(str_expression)) {}

  std::int32_t Evaluate(std::span<const std::pair<std::string, std::int32_t>> vec_symbols) const {
    return EvaluateInt(vec_symbols);
  }
};

}  // namespace ora::expr
