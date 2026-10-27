#pragma once
import std;

namespace ora::expr {

class ExpressionException : public std::runtime_error {
 public:
  using std::runtime_error::runtime_error;
};

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

struct Token {
  TokenType type{};
  std::size_t int4_index{};
  std::int32_t int4_number{};

  std::string str_symbol;

  int Precedence() const;
  bool LeftOperand() const;
  bool RightOperand() const;
  bool Opens() const;
  bool Closes() const;
  std::string_view Symbol() const;
};

class VariableExpression {
 public:

  explicit VariableExpression(std::string str_expression);

  const std::string& Expression() const { return str_expression_; }
  std::span<const std::string> Variables() const { return vec_variables_; }
  std::string ToString() const { return str_expression_; }

  bool EvaluateBool(std::span<const std::pair<std::string, std::int32_t>> vec_symbols) const;

  std::int32_t EvaluateInt(std::span<const std::pair<std::string, std::int32_t>> vec_symbols) const;

 private:
  std::int32_t EvaluateAsInt(std::span<const std::pair<std::string, std::int32_t>> vec_symbols) const;
  bool EvaluateAsBool(std::span<const std::pair<std::string, std::int32_t>> vec_symbols) const;

  std::string str_expression_;
  std::vector<std::string> vec_variables_;
  std::vector<Token> vec_postfix_;
};

class BooleanExpression : public VariableExpression {
 public:
  explicit BooleanExpression(std::string str_expression)
      : VariableExpression(std::move(str_expression)) {}

  bool Evaluate(std::span<const std::pair<std::string, std::int32_t>> vec_symbols) const {
    return EvaluateBool(vec_symbols);
  }
};

class IntegerExpression : public VariableExpression {
 public:
  explicit IntegerExpression(std::string str_expression)
      : VariableExpression(std::move(str_expression)) {}

  std::int32_t Evaluate(std::span<const std::pair<std::string, std::int32_t>> vec_symbols) const {
    return EvaluateInt(vec_symbols);
  }
};

}
