module ora.game.ui;

import std;

namespace ora::game::ui {

namespace {

enum class CharClass : std::uint8_t { kWhitespace, kOperator, kMixed, kId, kDigit };

constexpr CharClass char_class_of(char c) {
  switch (c) {
    case '~': case '!': case '%': case '^': case '&': case '*': case '(': case ')':
    case '+': case '=': case '[': case ']': case '{': case '}': case '|': case ':':
    case ';': case '\'': case '"': case '<': case '>': case '?': case ',': case '/':
      return CharClass::kOperator;
    case '.': case '$': case '-': case '@':
      return CharClass::kMixed;
    case '0': case '1': case '2': case '3': case '4':
    case '5': case '6': case '7': case '8': case '9':
      return CharClass::kDigit;
    case ' ': case '\t': case '\n': case '\r':
      return CharClass::kWhitespace;
    default:
      break;
  }
  return (c == ' ' || c == '\t' || c == '\n' || c == '\r' || static_cast<unsigned char>(c) >= 0x80)
             ? CharClass::kWhitespace
             : CharClass::kId;
}

enum class TokenType : std::uint8_t {
  kFalse, kTrue, kNumber, kVariable,
  kOpenParen, kCloseParen, kNot, kNegate, kOnesComplement,
  kAnd, kOr, kEquals, kNotEquals, kLessThan, kLessThanOrEqual,
  kGreaterThan, kGreaterThanOrEqual, kAdd, kSubtract, kMultiply, kDivide, kModulo,
  kInvalid,
};

enum class Sides : std::uint8_t { kNone = 0, kLeft = 1, kRight = 2, kBoth = 3 };

constexpr bool sides_has(Sides s, Sides m) { return (static_cast<unsigned>(s) & static_cast<unsigned>(m)) != 0; }

enum class Assoc : std::uint8_t { kLeft, kRight };

struct TokenTypeInfo {
  std::string_view str_symbol;
  int int4_precedence{};
  Sides sides_operand{};
  Sides sides_whitespace{};
  Assoc assoc{Assoc::kLeft};
};

constexpr int kPrecUnary = 16;
constexpr int kPrecMultiplication = 12;
constexpr int kPrecAddition = 11;
constexpr int kPrecRelation = 9;
constexpr int kPrecEquality = 8;
constexpr int kPrecAnd = 4;
constexpr int kPrecOr = 3;
constexpr int kPrecValue = 0;
constexpr int kPrecParens = -1;
constexpr int kPrecInvalid = ~0;

constexpr TokenTypeInfo kTokenInfos[] = {
    {"false", kPrecValue},
    {"true", kPrecValue},
    {"(<number>)", kPrecValue},
    {"(<variable>)", kPrecValue},
    {"(", kPrecParens, Sides::kRight, Sides::kNone, Assoc::kLeft},
    {")", kPrecParens, Sides::kLeft, Sides::kNone, Assoc::kLeft},
    {"!", kPrecUnary, Sides::kRight, Sides::kNone, Assoc::kRight},
    {"-", kPrecUnary, Sides::kRight, Sides::kNone, Assoc::kRight},
    {"~", kPrecUnary, Sides::kRight, Sides::kNone, Assoc::kRight},
    {"&&", kPrecAnd, Sides::kBoth, Sides::kBoth, Assoc::kLeft},
    {"||", kPrecOr, Sides::kBoth, Sides::kBoth, Assoc::kLeft},
    {"==", kPrecEquality, Sides::kBoth, Sides::kBoth, Assoc::kLeft},
    {"!=", kPrecEquality, Sides::kBoth, Sides::kBoth, Assoc::kLeft},
    {"<", kPrecRelation, Sides::kBoth, Sides::kBoth, Assoc::kLeft},
    {"<=", kPrecRelation, Sides::kBoth, Sides::kBoth, Assoc::kLeft},
    {">", kPrecRelation, Sides::kBoth, Sides::kBoth, Assoc::kLeft},
    {">=", kPrecRelation, Sides::kBoth, Sides::kBoth, Assoc::kLeft},
    {"+", kPrecAddition, Sides::kBoth, Sides::kBoth, Assoc::kLeft},
    {"-", kPrecAddition, Sides::kBoth, Sides::kBoth, Assoc::kLeft},
    {"*", kPrecMultiplication, Sides::kBoth, Sides::kBoth, Assoc::kLeft},
    {"/", kPrecMultiplication, Sides::kBoth, Sides::kBoth, Assoc::kLeft},
    {"%", kPrecMultiplication, Sides::kBoth, Sides::kBoth, Assoc::kLeft},
    {"(<INVALID>)", kPrecInvalid},
};

constexpr const TokenTypeInfo& token_info(TokenType t) {
  return kTokenInfos[static_cast<std::size_t>(t)];
}

struct Token {
  TokenType type{TokenType::kInvalid};
  int int4_index{};
  std::string str_symbol;
  int int4_value{};
};

bool has_right_operand(TokenType t) {
  return sides_has(token_info(t).sides_operand, Sides::kRight);
}

bool is_left_operand_or_none(TokenType t) {
  return t == TokenType::kInvalid || has_right_operand(t);
}

bool requires_whitespace_after(TokenType t) {
  return sides_has(token_info(t).sides_whitespace, Sides::kRight);
}

bool requires_whitespace_before(TokenType t) {
  return sides_has(token_info(t).sides_whitespace, Sides::kLeft);
}

struct ScanState {
  std::string_view str_expr;
  std::size_t sz_i{};
  TokenType last_type{TokenType::kInvalid};
};

bool scan_is_number(ScanState& s) {
  if (char_class_of(s.str_expr[s.sz_i]) != CharClass::kDigit) return false;
  ++s.sz_i;
  while (s.sz_i < s.str_expr.size()) {
    const CharClass cc{char_class_of(s.str_expr[s.sz_i])};
    if (cc == CharClass::kDigit) {
      ++s.sz_i;
      continue;
    }
    if (cc != CharClass::kWhitespace && cc != CharClass::kOperator && cc != CharClass::kMixed)
      return false;
    return true;
  }
  return true;
}

TokenType variable_or_keyword(std::string_view str_name) {
  if (str_name == "true") return TokenType::kTrue;
  if (str_name == "false") return TokenType::kFalse;
  return TokenType::kVariable;
}

std::expected<TokenType, std::string> get_next_type(ScanState& s, int int4_start) {
  const char c{s.str_expr[s.sz_i]};
  switch (c) {
    case '!':
      ++s.sz_i;
      if (s.sz_i < s.str_expr.size() && s.str_expr[s.sz_i] == '=') {
        ++s.sz_i;
        return TokenType::kNotEquals;
      }
      return TokenType::kNot;
    case '<':
      ++s.sz_i;
      if (s.sz_i < s.str_expr.size() && s.str_expr[s.sz_i] == '=') {
        ++s.sz_i;
        return TokenType::kLessThanOrEqual;
      }
      return TokenType::kLessThan;
    case '>':
      ++s.sz_i;
      if (s.sz_i < s.str_expr.size() && s.str_expr[s.sz_i] == '=') {
        ++s.sz_i;
        return TokenType::kGreaterThanOrEqual;
      }
      return TokenType::kGreaterThan;
    case '=':
      ++s.sz_i;
      if (s.sz_i < s.str_expr.size() && s.str_expr[s.sz_i] == '=') {
        ++s.sz_i;
        return TokenType::kEquals;
      }
      return std::unexpected(std::format("Unexpected character '=' at index {} - should it be `==`?", int4_start));
    case '&':
      ++s.sz_i;
      if (s.sz_i < s.str_expr.size() && s.str_expr[s.sz_i] == '&') {
        ++s.sz_i;
        return TokenType::kAnd;
      }
      return std::unexpected(std::format("Unexpected character '&' at index {} - should it be `&&`?", int4_start));
    case '|':
      ++s.sz_i;
      if (s.sz_i < s.str_expr.size() && s.str_expr[s.sz_i] == '|') {
        ++s.sz_i;
        return TokenType::kOr;
      }
      return std::unexpected(std::format("Unexpected character '|' at index {} - should it be `||`?", int4_start));
    case '(':
      ++s.sz_i;
      return TokenType::kOpenParen;
    case ')':
      ++s.sz_i;
      return TokenType::kCloseParen;
    case '~':
      ++s.sz_i;
      return TokenType::kOnesComplement;
    case '+':
      ++s.sz_i;
      return TokenType::kAdd;
    case '-': {
      ++s.sz_i;
      if (s.sz_i < s.str_expr.size() && scan_is_number(s)) return TokenType::kNumber;
      s.sz_i = static_cast<std::size_t>(int4_start) + 1;
      if (is_left_operand_or_none(s.last_type)) return TokenType::kNegate;
      return TokenType::kSubtract;
    }
    case '*':
      ++s.sz_i;
      return TokenType::kMultiply;
    case '/':
      ++s.sz_i;
      return TokenType::kDivide;
    case '%':
      ++s.sz_i;
      return TokenType::kModulo;
    default:
      break;
  }

  if (char_class_of(c) == CharClass::kDigit) {
    static_cast<void>(scan_is_number(s));
    return TokenType::kNumber;
  }

  if (char_class_of(c) != CharClass::kId)
    return std::unexpected(std::format("Invalid character '{}' at index {}", c, int4_start));

  while (s.sz_i < s.str_expr.size()) {
    const CharClass cc{char_class_of(s.str_expr[s.sz_i])};
    if (cc == CharClass::kWhitespace || cc == CharClass::kOperator) {
      if (s.sz_i > 0 && char_class_of(s.str_expr[s.sz_i - 1]) == CharClass::kMixed)
        return std::unexpected(std::format(
            "Invalid identifier end character at index {} for `{}`", s.sz_i - 1,
            s.str_expr.substr(static_cast<std::size_t>(int4_start), s.sz_i - static_cast<std::size_t>(int4_start))));
      return variable_or_keyword(s.str_expr.substr(
          static_cast<std::size_t>(int4_start), s.sz_i - static_cast<std::size_t>(int4_start)));
    }
    ++s.sz_i;
  }
  if (s.sz_i > 0 && char_class_of(s.str_expr[s.sz_i - 1]) == CharClass::kMixed)
    return std::unexpected(std::format(
        "Invalid identifier end character at index {} for `{}`", s.sz_i - 1,
        s.str_expr.substr(static_cast<std::size_t>(int4_start))));
  return variable_or_keyword(s.str_expr.substr(static_cast<std::size_t>(int4_start)));
}

std::expected<std::optional<Token>, std::string> token_next(ScanState& s) {
  if (s.sz_i == s.str_expr.size()) return std::nullopt;

  bool b_whitespace_before{false};
  if (char_class_of(s.str_expr[s.sz_i]) == CharClass::kWhitespace) {
    b_whitespace_before = true;
    while (char_class_of(s.str_expr[s.sz_i]) == CharClass::kWhitespace) {
      ++s.sz_i;
      if (s.sz_i == s.str_expr.size()) return std::nullopt;
    }
  } else if (s.last_type == TokenType::kInvalid) {
    b_whitespace_before = true;
  } else if (requires_whitespace_after(s.last_type)) {
    return std::unexpected(std::format(
        "Missing whitespace at index {}, after `{}` operator.", s.sz_i, token_info(s.last_type).str_symbol));
  }

  const int int4_start{static_cast<int>(s.sz_i)};
  const auto res_type{get_next_type(s, int4_start)};
  if (!res_type) return std::unexpected(res_type.error());
  const TokenType type{*res_type};
  if (!b_whitespace_before && requires_whitespace_before(type))
    return std::unexpected(std::format(
        "Missing whitespace at index {}, before `{}` operator.", s.sz_i, token_info(type).str_symbol));

  Token token{};
  token.type = type;
  token.int4_index = int4_start;
  if (type == TokenType::kNumber) {
    token.str_symbol = std::string{s.str_expr.substr(
        static_cast<std::size_t>(int4_start), s.sz_i - static_cast<std::size_t>(int4_start))};
    const auto res_val{std::from_chars(token.str_symbol.data(), token.str_symbol.data() + token.str_symbol.size(), token.int4_value)};
    if (res_val.ec != std::errc{})
      return std::unexpected(std::format("number parse failed for `{}`", token.str_symbol));
  } else if (type == TokenType::kVariable) {
    token.str_symbol = std::string{s.str_expr.substr(
        static_cast<std::size_t>(int4_start), s.sz_i - static_cast<std::size_t>(int4_start))};
  }
  return token;
}

std::expected<std::vector<Token>, std::string> tokenize(std::string_view str_expr) {
  std::vector<Token> vec_tokens;
  std::vector<Token> vec_openers;
  std::optional<Token> opt_last;
  ScanState s{str_expr, 0, TokenType::kInvalid};
  while (true) {
    s.last_type = opt_last.has_value() ? opt_last->type : TokenType::kInvalid;
    const auto res_token{token_next(s)};
    if (!res_token) return std::unexpected(res_token.error());
    if (!res_token->has_value()) {
      if (!opt_last.has_value()) return std::unexpected(std::string{"Empty expression"});
      if (has_right_operand(opt_last->type))
        return std::unexpected(std::format(
            "Missing value or sub-expression at end for `{}` operator", opt_last->str_symbol));
      break;
    }
    Token token{**res_token};

    if (token.type == TokenType::kCloseParen) {
      if (vec_openers.empty())
        return std::unexpected(std::format("Unmatched closing parenthesis at index {}", token.int4_index));
      vec_openers.pop_back();
    }
    if (token.type == TokenType::kOpenParen) vec_openers.push_back(token);

    if (!opt_last.has_value()) {
      if (sides_has(token_info(token.type).sides_operand, Sides::kLeft))
        return std::unexpected(std::format(
            "Missing value or sub-expression at beginning for `{}` operator", token.str_symbol.empty()
                ? std::string{token_info(token.type).str_symbol}
                : token.str_symbol));
    } else {
      if (opt_last->type == TokenType::kOpenParen && token.type == TokenType::kCloseParen)
        return std::unexpected(std::format("Empty parenthesis at index {}", opt_last->int4_index));
      if (has_right_operand(opt_last->type) == sides_has(token_info(token.type).sides_operand, Sides::kLeft)) {
        if (has_right_operand(opt_last->type))
          return std::unexpected(std::format(
              "Missing value or sub-expression or there is an extra operator `{}` at index {} or `{}` at index {}",
              opt_last->str_symbol, opt_last->int4_index,
              token.str_symbol.empty() ? std::string{token_info(token.type).str_symbol} : token.str_symbol,
              token.int4_index));
        return std::unexpected(std::format(
            "Missing binary operation before `{}` at index {}",
            token.str_symbol.empty() ? std::string{token_info(token.type).str_symbol} : token.str_symbol,
            token.int4_index));
      }
    }

    vec_tokens.push_back(std::move(token));
    opt_last = vec_tokens.back();
  }

  if (!vec_openers.empty())
    return std::unexpected(std::format("Unclosed opening parenthesis at index {}", vec_openers.back().int4_index));
  return vec_tokens;
}

std::vector<Token> to_postfix(const std::vector<Token>& vec_tokens) {
  std::vector<Token> vec_out;
  std::vector<Token> vec_stack;
  for (const Token& t : vec_tokens) {
    if (t.type == TokenType::kOpenParen) {
      vec_stack.push_back(t);
    } else if (t.type == TokenType::kCloseParen) {
      while (!vec_stack.empty() && vec_stack.back().type != TokenType::kOpenParen) {
        vec_out.push_back(vec_stack.back());
        vec_stack.pop_back();
      }
      if (!vec_stack.empty()) vec_stack.pop_back();
    } else if (token_info(t.type).sides_operand == Sides::kNone) {
      vec_out.push_back(t);
    } else {
      const TokenTypeInfo& info{token_info(t.type)};
      while (!vec_stack.empty()
             && ((info.assoc == Assoc::kRight && info.int4_precedence < token_info(vec_stack.back().type).int4_precedence)
                 || (info.assoc == Assoc::kLeft && info.int4_precedence <= token_info(vec_stack.back().type).int4_precedence))) {
        vec_out.push_back(vec_stack.back());
        vec_stack.pop_back();
      }
      vec_stack.push_back(t);
    }
  }
  while (!vec_stack.empty()) {
    vec_out.push_back(vec_stack.back());
    vec_stack.pop_back();
  }
  return vec_out;
}

enum class AstKind : std::uint8_t { kInt, kBool };

struct AstValue {
  AstKind kind{AstKind::kInt};
  int int4_value{};
};

AstValue ast_as_int(const AstValue& v) {
  if (v.kind == AstKind::kInt) return v;
  return AstValue{AstKind::kInt, v.int4_value != 0 ? 1 : 0};
}

std::expected<int, std::string> eval_postfix(
    const std::vector<Token>& vec_postfix, const std::map<std::string, int>& map_symbols) {
  std::vector<AstValue> vec_stack;
  auto pop_int = [&vec_stack]() -> std::expected<AstValue, std::string> {
    if (vec_stack.empty()) return std::unexpected(std::string{"expression stack underflow"});
    AstValue v{ast_as_int(vec_stack.back())};
    vec_stack.pop_back();
    return v;
  };
  auto pop_bool = [&vec_stack]() -> std::expected<bool, std::string> {
    if (vec_stack.empty()) return std::unexpected(std::string{"expression stack underflow"});
    const bool b{vec_stack.back().int4_value != 0};
    vec_stack.pop_back();
    return b;
  };

  for (const Token& t : vec_postfix) {
    switch (t.type) {
      case TokenType::kNumber:
      case TokenType::kVariable: {
        int int4_v{};
        if (t.type == TokenType::kNumber) {
          int4_v = t.int4_value;
        } else {
          if (const auto it{map_symbols.find(t.str_symbol)}; it != map_symbols.end()) int4_v = it->second;
        }
        vec_stack.push_back(AstValue{AstKind::kInt, int4_v});
        break;
      }
      case TokenType::kTrue:
        vec_stack.push_back(AstValue{AstKind::kBool, 1});
        break;
      case TokenType::kFalse:
        vec_stack.push_back(AstValue{AstKind::kBool, 0});
        break;
      case TokenType::kNot: {
        const auto res{pop_bool()};
        if (!res) return std::unexpected(res.error());
        vec_stack.push_back(AstValue{AstKind::kBool, !*res ? 1 : 0});
        break;
      }
      case TokenType::kNegate: {
        const auto res{pop_int()};
        if (!res) return std::unexpected(res.error());
        vec_stack.push_back(AstValue{AstKind::kInt, -res->int4_value});
        break;
      }
      case TokenType::kOnesComplement: {
        const auto res{pop_int()};
        if (!res) return std::unexpected(res.error());
        vec_stack.push_back(AstValue{AstKind::kInt, ~res->int4_value});
        break;
      }
      case TokenType::kAnd: {
        const auto res_y{pop_bool()};
        const auto res_x{pop_bool()};
        if (!res_y || !res_x) return std::unexpected(std::string{"expression stack underflow"});
        vec_stack.push_back(AstValue{AstKind::kBool, (*res_x && *res_y) ? 1 : 0});
        break;
      }
      case TokenType::kOr: {
        const auto res_y{pop_bool()};
        const auto res_x{pop_bool()};
        if (!res_y || !res_x) return std::unexpected(std::string{"expression stack underflow"});
        vec_stack.push_back(AstValue{AstKind::kBool, (*res_x || *res_y) ? 1 : 0});
        break;
      }
      case TokenType::kEquals: case TokenType::kNotEquals:
      case TokenType::kLessThan: case TokenType::kLessThanOrEqual:
      case TokenType::kGreaterThan: case TokenType::kGreaterThanOrEqual:
      case TokenType::kAdd: case TokenType::kSubtract:
      case TokenType::kMultiply: case TokenType::kDivide: case TokenType::kModulo: {
        const auto res_y{pop_int()};
        const auto res_x{pop_int()};
        if (!res_y || !res_x) return std::unexpected(std::string{"expression stack underflow"});
        const int int4_x{res_x->int4_value};
        const int int4_y{res_y->int4_value};
        int int4_r{};
        bool b_bool_result{false};
        switch (t.type) {
          case TokenType::kEquals: int4_r = int4_x == int4_y ? 1 : 0; b_bool_result = true; break;
          case TokenType::kNotEquals: int4_r = int4_x != int4_y ? 1 : 0; b_bool_result = true; break;
          case TokenType::kLessThan: int4_r = int4_x < int4_y ? 1 : 0; b_bool_result = true; break;
          case TokenType::kLessThanOrEqual: int4_r = int4_x <= int4_y ? 1 : 0; b_bool_result = true; break;
          case TokenType::kGreaterThan: int4_r = int4_x > int4_y ? 1 : 0; b_bool_result = true; break;
          case TokenType::kGreaterThanOrEqual: int4_r = int4_x >= int4_y ? 1 : 0; b_bool_result = true; break;
          case TokenType::kAdd: int4_r = int4_x + int4_y; break;
          case TokenType::kSubtract: int4_r = int4_x - int4_y; break;
          case TokenType::kMultiply: int4_r = int4_x * int4_y; break;
          case TokenType::kDivide: int4_r = int4_y != 0 ? int4_x / int4_y : 0; break;
          case TokenType::kModulo: int4_r = int4_y != 0 ? int4_x % int4_y : 0; break;
          default: break;
        }
        vec_stack.push_back(AstValue{b_bool_result ? AstKind::kBool : AstKind::kInt, int4_r});
        break;
      }
      default:
        return std::unexpected(std::format("unexpected token `{}`", token_info(t.type).str_symbol));
    }
  }

  if (vec_stack.size() != 1) return std::unexpected(std::string{"expression did not reduce to one value"});
  return ast_as_int(vec_stack.back()).int4_value;
}

}  // namespace

std::expected<int, std::string> widget_int_eval(
    std::string_view str_expr, const std::map<std::string, int>& map_symbols) {
  const auto res_tokens{tokenize(str_expr)};
  if (!res_tokens) return std::unexpected(res_tokens.error());
  const std::vector<Token> vec_postfix{to_postfix(*res_tokens)};
  return eval_postfix(vec_postfix, map_symbols);
}

}
