// UPSTREAM: OpenRA.Game/Support/VariableExpression.cs @b6fc03f L22-983
//          (variable_expression.hpp 的实现;tokenizer/校验/后缀化/求值逐行对照)
//          Implementation of variable_expression.hpp (tokenizer/validation/
//          postfix/evaluation line-checked against upstream).
// 已登记偏离(docs/COVERAGE.md)/ Registered deviations:
//  - 错误消息中的 index 按 UTF-8 字节计(C# 按 UTF-16 code unit);
//    ASCII 输入下完全一致,mods 表达式均为 ASCII
//    Indices in error messages count UTF-8 bytes (C# counts UTF-16 code
//    units); identical for ASCII input, and mods expressions are ASCII.
import std;
#include "variable_expression.hpp"
#include "parse.hpp"
#include "core/text.hpp"

namespace ora::expr {

namespace {

// ———— 字符分类(VariableExpression.cs L30-91)/ Character classes ————

enum class CharClass : std::uint8_t { Whitespace, Operator, Mixed, Id, Digit };

CharClass CharClassOf(char chr_c) {
  switch (chr_c) {
    case '~': case '!': case '%': case '^': case '&': case '*':
    case '(': case ')': case '+': case '=': case '[': case ']':
    case '{': case '}': case '|': case ':': case ';': case '\'':
    case '"': case '<': case '>': case '?': case ',': case '/':
      return CharClass::Operator;
    case '.': case '$': case '-': case '@':
      return CharClass::Mixed;
    case '0': case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8': case '9':
      return CharClass::Digit;
    case ' ': case '\t': case '\n': case '\r':
      return CharClass::Whitespace;
    default:
      // 其余 Unicode 空白在 mods 表达式(ASCII)外不可达;按 Id 处理
      // Non-ASCII Unicode whitespace is unreachable in mods (ASCII)
      // expressions; treated as Id.
      return CharClass::Id;
  }
}

// ———— token 元数据表(L113-294)/ Token metadata table ————

enum class Assoc : std::uint8_t { Left, Right };

// Sides 掩码(None=0, Left=1, Right=2, Both=3)/ Sides bit mask.
enum Sides : std::uint8_t { kSidesNone = 0, kSidesLeft = 1, kSidesRight = 2, kSidesBoth = 3 };

enum Grouping : std::uint8_t { kGroupNone = 0, kGroupParens = 1 };

struct TokenTypeInfo {
  std::string_view str_symbol;
  int int4_precedence;   // 上游 Precedence 枚举的 int 值 / int value of the upstream Precedence enum
  std::uint8_t uint1_operand_sides;
  std::uint8_t uint1_whitespace_sides;
  Assoc assoc_associativity;
  std::uint8_t uint1_opens;
  std::uint8_t uint1_closes;
};

// CreateTokenTypeInfoEnumeration(L213-292)的扁平等价
// (顺序 = TokenType 枚举序:False..Invalid,共 23 项)
// Flat equivalent of CreateTokenTypeInfoEnumeration (L213-292)
// (order = the TokenType enumeration: False..Invalid, 23 entries).
constexpr std::array<TokenTypeInfo, 23> kTokenTypeInfo{{
    {"false", 0, kSidesNone, kSidesNone, Assoc::Left, kGroupNone, kGroupNone},
    {"true", 0, kSidesNone, kSidesNone, Assoc::Left, kGroupNone, kGroupNone},
    {"(<number>)", 0, kSidesNone, kSidesNone, Assoc::Left, kGroupNone, kGroupNone},
    {"(<variable>)", 0, kSidesNone, kSidesNone, Assoc::Left, kGroupNone, kGroupNone},
    {"(", -1, kSidesRight, kSidesNone, Assoc::Left, kGroupParens, kGroupNone},
    {")", -1, kSidesLeft, kSidesNone, Assoc::Left, kGroupNone, kGroupParens},
    {"!", 16, kSidesRight, kSidesNone, Assoc::Right, kGroupNone, kGroupNone},
    {"-", 16, kSidesRight, kSidesNone, Assoc::Right, kGroupNone, kGroupNone},
    {"~", 16, kSidesRight, kSidesNone, Assoc::Right, kGroupNone, kGroupNone},
    {"&&", 4, kSidesBoth, kSidesBoth, Assoc::Left, kGroupNone, kGroupNone},
    {"||", 3, kSidesBoth, kSidesBoth, Assoc::Left, kGroupNone, kGroupNone},
    {"==", 8, kSidesBoth, kSidesBoth, Assoc::Left, kGroupNone, kGroupNone},
    {"!=", 8, kSidesBoth, kSidesBoth, Assoc::Left, kGroupNone, kGroupNone},
    {"<", 9, kSidesBoth, kSidesBoth, Assoc::Left, kGroupNone, kGroupNone},
    {"<=", 9, kSidesBoth, kSidesBoth, Assoc::Left, kGroupNone, kGroupNone},
    {">", 9, kSidesBoth, kSidesBoth, Assoc::Left, kGroupNone, kGroupNone},
    {">=", 9, kSidesBoth, kSidesBoth, Assoc::Left, kGroupNone, kGroupNone},
    {"+", 11, kSidesBoth, kSidesBoth, Assoc::Left, kGroupNone, kGroupNone},
    {"-", 11, kSidesBoth, kSidesBoth, Assoc::Left, kGroupNone, kGroupNone},
    {"*", 12, kSidesBoth, kSidesBoth, Assoc::Left, kGroupNone, kGroupNone},
    {"/", 12, kSidesBoth, kSidesBoth, Assoc::Left, kGroupNone, kGroupNone},
    {"%", 12, kSidesBoth, kSidesBoth, Assoc::Left, kGroupNone, kGroupNone},
    {"(<INVALID>)", ~0, kSidesNone, kSidesNone, Assoc::Left, kGroupNone, kGroupNone},
}};

const TokenTypeInfo& InfoOf(TokenType type) {
  return kTokenTypeInfo[static_cast<std::size_t>(type)];
}

bool HasRightOperand(TokenType type) {
  return (InfoOf(type).uint1_operand_sides & kSidesRight) != 0;
}

bool IsLeftOperandOrNone(TokenType type) {
  return type == TokenType::Invalid || HasRightOperand(type);
}

bool RequiresWhitespaceAfter(TokenType type) {
  return (InfoOf(type).uint1_whitespace_sides & kSidesRight) != 0;
}

bool RequiresWhitespaceBefore(TokenType type) {
  return (InfoOf(type).uint1_whitespace_sides & kSidesLeft) != 0;
}

// ———— tokenizer(L391-577)/ tokenizer ————

/// ScanIsNumber(L343-367):当前位置起扫描数字;Id 字符紧贴数字 → merged 异常
/// ScanIsNumber (L343-367): scans digits from the current position; an Id
/// character glued to the digits raises the merged exception.
bool ScanIsNumber(std::string_view str_expr, std::size_t int4_start, std::size_t& int4_i) {
  CharClass cc_class = CharClassOf(str_expr[int4_i]);
  if (cc_class == CharClass::Digit) {
    int4_i++;
    for (; int4_i < str_expr.size(); int4_i++) {
      cc_class = CharClassOf(str_expr[int4_i]);
      if (cc_class != CharClass::Digit) {
        if (cc_class != CharClass::Whitespace && cc_class != CharClass::Operator &&
            cc_class != CharClass::Mixed) {
          // 消息含解析后的数字值(C# 直接 Parse;切片恒为合法整数,失败即 abort)
          // The message embeds the parsed number (C# uses Parse; the slice is
          // always a valid integer — abort otherwise).
          std::int32_t int4_value{};
          if (!meta::TryParseInt32Invariant(str_expr.substr(int4_start, int4_i - int4_start),
                                            int4_value))
            std::abort();
          throw ExpressionException(std::format("Number {} and variable merged at index {}",
                                                int4_value, int4_start));
        }
        return true;
      }
    }
    return true;
  }
  return false;
}

/// VariableOrKeyword(L369-389):Id 末字符为 Mixed → 异常;true/false 关键字精确小写
/// VariableOrKeyword (L369-389): a Mixed final Id character raises; the
/// true/false keywords match lowercase exactly.
TokenType VariableOrKeywordChecked(std::string_view str_expr, std::size_t int4_start,
                                   std::size_t& int4_i) {
  if (CharClassOf(str_expr[int4_i - 1]) == CharClass::Mixed)
    throw ExpressionException(std::format("Invalid identifier end character at index {} for `{}`",
                                          int4_i - 1,
                                          str_expr.substr(int4_start, int4_i - int4_start)));
  const std::string_view sv_word = str_expr.substr(int4_start, int4_i - int4_start);
  if (sv_word == "true")
    return TokenType::True;
  if (sv_word == "false")
    return TokenType::False;
  return TokenType::Variable;
}

TokenType GetNextType(std::string_view str_expr, std::size_t& int4_i, TokenType type_last) {
  const std::size_t int4_start = int4_i;
  switch (str_expr[int4_i]) {
    case '!':
      int4_i++;
      if (int4_i < str_expr.size() && str_expr[int4_i] == '=') {
        int4_i++;
        return TokenType::NotEquals;
      }
      return TokenType::Not;
    case '<':
      int4_i++;
      if (int4_i < str_expr.size() && str_expr[int4_i] == '=') {
        int4_i++;
        return TokenType::LessThanOrEqual;
      }
      return TokenType::LessThan;
    case '>':
      int4_i++;
      if (int4_i < str_expr.size() && str_expr[int4_i] == '=') {
        int4_i++;
        return TokenType::GreaterThanOrEqual;
      }
      return TokenType::GreaterThan;
    case '=':
      int4_i++;
      if (int4_i < str_expr.size() && str_expr[int4_i] == '=') {
        int4_i++;
        return TokenType::Equals;
      }
      throw ExpressionException(
          std::format("Unexpected character '=' at index {} - should it be `==`?", int4_start));
    case '&':
      int4_i++;
      if (int4_i < str_expr.size() && str_expr[int4_i] == '&') {
        int4_i++;
        return TokenType::And;
      }
      throw ExpressionException(
          std::format("Unexpected character '&' at index {} - should it be `&&`?", int4_start));
    case '|':
      int4_i++;
      if (int4_i < str_expr.size() && str_expr[int4_i] == '|') {
        int4_i++;
        return TokenType::Or;
      }
      throw ExpressionException(
          std::format("Unexpected character '|' at index {} - should it be `||`?", int4_start));
    case '(':
      int4_i++;
      return TokenType::OpenParen;
    case ')':
      int4_i++;
      return TokenType::CloseParen;
    case '~':
      int4_i++;
      return TokenType::OnesComplement;
    case '+':
      int4_i++;
      return TokenType::Add;
    case '-':
      if (++int4_i < str_expr.size() && ScanIsNumber(str_expr, int4_start, int4_i))
        return TokenType::Number;
      int4_i = int4_start + 1;
      if (IsLeftOperandOrNone(type_last))
        return TokenType::Negate;
      return TokenType::Subtract;
    case '*':
      int4_i++;
      return TokenType::Multiply;
    case '/':
      int4_i++;
      return TokenType::Divide;
    case '%':
      int4_i++;
      return TokenType::Modulo;
    default:
      break;
  }

  if (ScanIsNumber(str_expr, int4_start, int4_i))
    return TokenType::Number;

  // Id 扫描:空白/操作符终止;Digit/Mixed 可并入名字(如 "a1.2$")
  // Id scan: whitespace/operator terminate; Digit/Mixed may join the name
  // (e.g. "a1.2$").
  for (int4_i = int4_start; int4_i < str_expr.size(); int4_i++) {
    const CharClass cc_class = CharClassOf(str_expr[int4_i]);
    if (cc_class == CharClass::Whitespace || cc_class == CharClass::Operator)
      return VariableOrKeywordChecked(str_expr, int4_start, int4_i);
  }
  return VariableOrKeywordChecked(str_expr, int4_start, int4_i);
}

/// GetNext(L513-551):吃空白 + 运算符空白守护 + 产 token;nullopt = 流结束
/// GetNext (L513-551): eats whitespace, enforces operator whitespace guards,
/// produces the token; nullopt = end of stream.
std::optional<Token> GetNextToken(std::string_view str_expr, std::size_t& int4_i,
                                  TokenType type_last) {
  if (int4_i == str_expr.size())
    return std::nullopt;

  bool b_whitespace_before{false};
  if (CharClassOf(str_expr[int4_i]) == CharClass::Whitespace) {
    b_whitespace_before = true;
    while (CharClassOf(str_expr[int4_i]) == CharClass::Whitespace) {
      if (++int4_i == str_expr.size())
        return std::nullopt;
    }
  } else if (type_last == TokenType::Invalid) {
    b_whitespace_before = true;
  } else if (RequiresWhitespaceAfter(type_last)) {
    throw ExpressionException(std::format("Missing whitespace at index {}, after `{}` operator.",
                                          int4_i, InfoOf(type_last).str_symbol));
  }

  const std::size_t int4_start = int4_i;
  const TokenType type = GetNextType(str_expr, int4_i, type_last);
  if (!b_whitespace_before && RequiresWhitespaceBefore(type))
    throw ExpressionException(std::format("Missing whitespace at index {}, before `{}` operator.",
                                          int4_i, InfoOf(type).str_symbol));

  Token token_ret;
  token_ret.type = type;
  token_ret.int4_index = int4_start;
  if (type == TokenType::Number || type == TokenType::Variable) {
    token_ret.str_symbol = str_expr.substr(int4_start, int4_i - int4_start);
    if (type == TokenType::Number) {
      if (!meta::TryParseInt32Invariant(token_ret.str_symbol, token_ret.int4_number))
        std::abort();  // ScanIsNumber 已保证合法 / ScanIsNumber guarantees validity
    }
  }
  return token_ret;
}

/// ToPostfix(L672-699):调度场(右结合 <,左结合 <=)
/// ToPostfix (L672-699): shunting-yard (right-assoc pops on <, left-assoc on <=).
std::vector<Token> ToPostfix(const std::vector<Token>& vec_tokens) {
  std::vector<Token> vec_output;
  std::vector<const Token*> vec_stack;
  for (const Token& token_t : vec_tokens) {
    if (token_t.Opens()) {
      vec_stack.push_back(&token_t);
    } else if (token_t.Closes()) {
      while (!vec_stack.back()->Opens()) {
        vec_output.push_back(*vec_stack.back());
        vec_stack.pop_back();
      }
      vec_stack.pop_back();  // 弹出开括号(不输出)/ pop the opener (not emitted)
    } else if (InfoOf(token_t.type).uint1_operand_sides == kSidesNone) {
      vec_output.push_back(token_t);
    } else {
      const bool b_right = InfoOf(token_t.type).assoc_associativity == Assoc::Right;
      while (!vec_stack.empty() &&
             ((b_right && token_t.Precedence() < vec_stack.back()->Precedence()) ||
              (!b_right && token_t.Precedence() <= vec_stack.back()->Precedence()))) {
        vec_output.push_back(*vec_stack.back());
        vec_stack.pop_back();
      }
      vec_stack.push_back(&token_t);
    }
  }
  while (!vec_stack.empty()) {
    vec_output.push_back(*vec_stack.back());
    vec_stack.pop_back();
  }
  return vec_output;
}

}  // namespace

// ———— Token 元数据查询(表在匿名命名空间)/ Token metadata queries ————

int Token::Precedence() const { return InfoOf(type).int4_precedence; }
bool Token::LeftOperand() const { return (InfoOf(type).uint1_operand_sides & kSidesLeft) != 0; }
bool Token::RightOperand() const { return (InfoOf(type).uint1_operand_sides & kSidesRight) != 0; }
bool Token::Opens() const { return InfoOf(type).uint1_opens != kGroupNone; }
bool Token::Closes() const { return InfoOf(type).uint1_closes != kGroupNone; }
std::string_view Token::Symbol() const {
  // Number/Variable 分支读深拷贝成员(无悬垂);操作符分支读静态表
  // The Number/Variable branch reads the deep-copied member (no
  // dangling); the operator branch reads the static table.
  if (type == TokenType::Number || type == TokenType::Variable)
    return str_symbol;
  return InfoOf(type).str_symbol;
}

VariableExpression::VariableExpression(std::string str_expression)
    : str_expression_{std::move(str_expression)} {
  // Build(L586-651):token 流生成 + 语法校验
  // Build (L586-651): token stream generation + syntax validation.
  std::vector<Token> vec_tokens;
  std::vector<std::size_t> vec_openers;
  std::optional<Token> token_last;
  for (std::size_t int4_i{};;) {
    const std::optional<Token> token_next = GetNextToken(
        str_expression_, int4_i, token_last.has_value() ? token_last->type : TokenType::Invalid);
    if (!token_next.has_value()) {
      // 流结束的尾部校验(L596-604)/ end-of-stream tail checks (L596-604).
      if (!token_last.has_value())
        throw ExpressionException("Empty expression");
      if (token_last->RightOperand())
        throw ExpressionException(std::format(
            "Missing value or sub-expression at end for `{}` operator", token_last->Symbol()));
      break;
    }
    const Token& token = *token_next;

    if (token.Closes()) {
      if (vec_openers.empty())
        throw ExpressionException(std::format("Unmatched closing parenthesis at index {}",
                                              token.int4_index));
      vec_openers.pop_back();
    }

    if (token.Opens())
      vec_openers.push_back(token.int4_index);

    if (!token_last.has_value()) {
      if (token.LeftOperand())
        throw ExpressionException(std::format(
            "Missing value or sub-expression at beginning for `{}` operator", token.Symbol()));
    } else {
      if (token_last->Opens() && token.Closes())
        throw ExpressionException(
            std::format("Empty parenthesis at index {}", token_last->int4_index));

      if (token_last->RightOperand() == token.LeftOperand()) {
        if (token_last->RightOperand())
          throw ExpressionException(std::format(
              "Missing value or sub-expression or there is an extra operator `{}` at index {} or `{}` at index {}",
              token_last->Symbol(), token_last->int4_index, token.Symbol(), token.int4_index));
        throw ExpressionException(std::format("Missing binary operation before `{}` at index {}",
                                              token.Symbol(), token.int4_index));
      }
    }

    if (token.type == TokenType::Variable) {
      if (std::ranges::find(vec_variables_, token.str_symbol) == vec_variables_.end())
        vec_variables_.emplace_back(token.str_symbol);
    }

    vec_tokens.push_back(token);
    token_last = token;
  }

  if (!vec_openers.empty())
    throw ExpressionException(
        std::format("Unclosed opening parenthesis at index {}", vec_openers.back()));

  vec_postfix_ = ToPostfix(vec_tokens);
}

namespace {

/// 求值栈元素(AstStack 的值语义:Int/Bool 双态互转)/ Evaluation stack entry
/// (value semantics of AstStack: Int/Bool with mutual conversion).
struct EvalValue {
  std::int32_t int4_value{};
  bool b_is_bool{};

  static EvalValue OfInt(std::int32_t int4_v) { return {int4_v, false}; }
  static EvalValue OfBool(bool b_v) { return {b_v ? 1 : 0, true}; }
};

/// Peek(ExprType) 转换(L727-746):int→bool 非 0 真;bool→int 1/0
/// Peek conversion (L727-746): int→bool tests non-zero; bool→int yields 1/0.
EvalValue ConvertTo(const EvalValue& value, bool b_to_bool) {
  if (value.b_is_bool == b_to_bool)
    return value;
  if (b_to_bool)
    return EvalValue::OfBool(value.int4_value != 0);
  return EvalValue::OfInt(value.int4_value);
}

/// 后缀栈机求值(Compiler.Build L783-949 的求值镜像)
/// Postfix stack-machine evaluation (the evaluation mirror of Compiler.Build
/// L783-949).
std::vector<EvalValue> EvaluatePostfix(
    std::span<const Token> vec_postfix,
    std::span<const std::pair<std::string, std::int32_t>> vec_symbols) {
  std::vector<EvalValue> vec_stack;
  const auto pop_int = [&vec_stack]() {
    if (vec_stack.empty())
      std::abort();  // 语法校验后不可达 / unreachable after validation
    const EvalValue v = ConvertTo(vec_stack.back(), false);
    vec_stack.pop_back();
    return v.int4_value;
  };
  const auto pop_bool = [&vec_stack]() {
    if (vec_stack.empty())
      std::abort();
    const EvalValue v = ConvertTo(vec_stack.back(), true);
    vec_stack.pop_back();
    return v.int4_value != 0;
  };

  for (const Token& token_t : vec_postfix) {
    switch (token_t.type) {
      case TokenType::And: {
        const bool b_y = pop_bool();
        const bool b_x = pop_bool();
        vec_stack.push_back(EvalValue::OfBool(b_x && b_y));
        continue;
      }
      case TokenType::Or: {
        const bool b_y = pop_bool();
        const bool b_x = pop_bool();
        vec_stack.push_back(EvalValue::OfBool(b_x || b_y));
        continue;
      }
      case TokenType::Equals: {
        const std::int32_t int4_y = pop_int();
        const std::int32_t int4_x = pop_int();
        vec_stack.push_back(EvalValue::OfBool(int4_x == int4_y));
        continue;
      }
      case TokenType::NotEquals: {
        const std::int32_t int4_y = pop_int();
        const std::int32_t int4_x = pop_int();
        vec_stack.push_back(EvalValue::OfBool(int4_x != int4_y));
        continue;
      }
      case TokenType::Not:
        vec_stack.push_back(EvalValue::OfBool(!pop_bool()));
        continue;
      case TokenType::Negate:
        vec_stack.push_back(EvalValue::OfInt(-pop_int()));
        continue;
      case TokenType::OnesComplement:
        vec_stack.push_back(EvalValue::OfInt(~pop_int()));
        continue;
      case TokenType::LessThan: {
        const std::int32_t int4_y = pop_int();
        const std::int32_t int4_x = pop_int();
        vec_stack.push_back(EvalValue::OfBool(int4_x < int4_y));
        continue;
      }
      case TokenType::LessThanOrEqual: {
        const std::int32_t int4_y = pop_int();
        const std::int32_t int4_x = pop_int();
        vec_stack.push_back(EvalValue::OfBool(int4_x <= int4_y));
        continue;
      }
      case TokenType::GreaterThan: {
        const std::int32_t int4_y = pop_int();
        const std::int32_t int4_x = pop_int();
        vec_stack.push_back(EvalValue::OfBool(int4_x > int4_y));
        continue;
      }
      case TokenType::GreaterThanOrEqual: {
        const std::int32_t int4_y = pop_int();
        const std::int32_t int4_x = pop_int();
        vec_stack.push_back(EvalValue::OfBool(int4_x >= int4_y));
        continue;
      }
      case TokenType::Add: {
        const std::int32_t int4_y = pop_int();
        const std::int32_t int4_x = pop_int();
        vec_stack.push_back(EvalValue::OfInt(int4_x + int4_y));
        continue;
      }
      case TokenType::Subtract: {
        const std::int32_t int4_y = pop_int();
        const std::int32_t int4_x = pop_int();
        vec_stack.push_back(EvalValue::OfInt(int4_x - int4_y));
        continue;
      }
      case TokenType::Multiply: {
        const std::int32_t int4_y = pop_int();
        const std::int32_t int4_x = pop_int();
        vec_stack.push_back(EvalValue::OfInt(int4_x * int4_y));
        continue;
      }
      case TokenType::Divide: {
        // C# 编译为条件表达式:y != 0 ? x/y : 0(向零截断)
        // C# compiles a conditional: y != 0 ? x/y : 0 (truncating toward zero).
        const std::int32_t int4_y = pop_int();
        const std::int32_t int4_x = pop_int();
        vec_stack.push_back(EvalValue::OfInt(int4_y != 0 ? int4_x / int4_y : 0));
        continue;
      }
      case TokenType::Modulo: {
        const std::int32_t int4_y = pop_int();
        const std::int32_t int4_x = pop_int();
        vec_stack.push_back(EvalValue::OfInt(int4_y != 0 ? int4_x % int4_y : 0));
        continue;
      }
      case TokenType::False:
        vec_stack.push_back(EvalValue::OfBool(false));
        continue;
      case TokenType::True:
        vec_stack.push_back(EvalValue::OfBool(true));
        continue;
      case TokenType::Number:
        vec_stack.push_back(EvalValue::OfInt(token_t.int4_number));
        continue;
      case TokenType::Variable: {
        // ParseSymbol:缺失变量取 0 / ParseSymbol: missing symbols read as 0.
        std::int32_t int4_v{0};
        for (const auto& [str_name, int4_sym] : vec_symbols) {
          if (str_name == token_t.str_symbol) {
            int4_v = int4_sym;
            break;
          }
        }
        vec_stack.push_back(EvalValue::OfInt(int4_v));
        continue;
      }
      default:
        std::abort();  // 括号不进后缀流 / parens never enter the postfix stream
    }
  }
  return vec_stack;
}

}  // namespace

std::int32_t VariableExpression::EvaluateAsInt(
    std::span<const std::pair<std::string, std::int32_t>> vec_symbols) const {
  const std::vector<EvalValue> vec_stack = EvaluatePostfix(vec_postfix_, vec_symbols);
  if (vec_stack.size() != 1)
    std::abort();
  return ConvertTo(vec_stack.back(), false).int4_value;
}

bool VariableExpression::EvaluateAsBool(
    std::span<const std::pair<std::string, std::int32_t>> vec_symbols) const {
  const std::vector<EvalValue> vec_stack = EvaluatePostfix(vec_postfix_, vec_symbols);
  if (vec_stack.size() != 1)
    std::abort();
  return ConvertTo(vec_stack.back(), true).int4_value != 0;
}

std::int32_t VariableExpression::EvaluateInt(
    std::span<const std::pair<std::string, std::int32_t>> vec_symbols) const {
  return EvaluateAsInt(vec_symbols);
}

bool VariableExpression::EvaluateBool(
    std::span<const std::pair<std::string, std::int32_t>> vec_symbols) const {
  return EvaluateAsBool(vec_symbols);
}

}  // namespace ora::expr
