// UPSTREAM: OpenRA.Game/FluentBundle.cs @1f594e320 —— Fluent 资源解析的
// 自研承载(Linguini.Syntax 的等价面;空白规范 = 公共缩进扣除 + 行尾修剪
// + placeable 邻接修剪,Fluent 规范 Patterns 节;语料实测差异由
// golden_fluent 对拍钉住)。
// The self-written carrier of the Fluent resource parse (the Linguini.Syntax
// equivalent face; the whitespace normalization = the common-indent deduction
// + the per-line trailing trim + the placeable-adjacent trim, the Fluent
// spec's Patterns section; measured divergences pinned by golden_fluent).
#include "fluent/fluent_syntax.hpp"

namespace ora::fluent {
namespace {

bool IsBlank(std::string_view str_line) {
  return str_line.find_first_not_of(" \t") == std::string_view::npos;
}

std::size_t IndentOf(std::string_view str_line) {
  const std::size_t uint_len = str_line.size();
  std::size_t uint_indent = 0;
  while (uint_indent < uint_len &&
         (str_line[uint_indent] == ' ' || str_line[uint_indent] == '\t'))
    ++uint_indent;
  return uint_indent;
}

/// 行标识符匹配([A-Za-z][A-Za-z0-9_-]*);返回 = 号位置(不含则 npos)
/// Matches the line identifier ([A-Za-z][A-Za-z0-9_-]*); returns the '='
/// position (npos when absent).
std::size_t IdentifierEnd(std::string_view str_line) {
  if (str_line.empty() ||
      !(std::isalpha(static_cast<unsigned char>(str_line[0])) != 0))
    return std::string_view::npos;
  std::size_t uint_i = 1;
  const std::size_t uint_len = str_line.size();
  while (uint_i < uint_len) {
    const char ch = str_line[uint_i];
    if (std::isalnum(static_cast<unsigned char>(ch)) != 0 || ch == '_' ||
        ch == '-')
      ++uint_i;
    else
      break;
  }
  std::size_t uint_j = uint_i;
  while (uint_j < uint_len && (str_line[uint_j] == ' ' || str_line[uint_j] == '\t'))
    ++uint_j;
  if (uint_j < uint_len && str_line[uint_j] == '=')
    return uint_j;
  return std::string_view::npos;
}

/// 属性行(.name = …)在缩进 >0 处;返回属性名与 = 号位置
/// The attribute line (.name = …) at indent > 0; returns the attribute name
/// and the '=' position.
bool ParseAttributeLine(std::string_view str_line, std::string& str_name,
                        std::size_t& uint_eq) {
  const std::size_t uint_len = str_line.size();
  std::size_t uint_i = 0;
  while (uint_i < uint_len &&
         (str_line[uint_i] == ' ' || str_line[uint_i] == '\t'))
    ++uint_i;
  if (uint_i == 0 || uint_i >= uint_len || str_line[uint_i] != '.')
    return false;
  ++uint_i;
  const std::size_t uint_name_start = uint_i;
  if (uint_i >= uint_len ||
      !(std::isalpha(static_cast<unsigned char>(str_line[uint_i])) != 0))
    return false;
  while (uint_i < uint_len) {
    const char ch = str_line[uint_i];
    if (std::isalnum(static_cast<unsigned char>(ch)) != 0 || ch == '_' ||
        ch == '-')
      ++uint_i;
    else
      break;
  }
  if (uint_i == uint_name_start)
    return false;
  std::size_t uint_j = uint_i;
  while (uint_j < uint_len && (str_line[uint_j] == ' ' || str_line[uint_j] == '\t'))
    ++uint_j;
  if (uint_j >= uint_len || str_line[uint_j] != '=')
    return false;
  str_name = std::string(str_line.substr(uint_name_start, uint_i - uint_name_start));
  uint_eq = uint_j;
  return true;
}

/// 变体键行([key] / *[key] + 内联余文);返回键与余文起点
/// The variant-key line ([key] / *[key] + the inline remainder); returns the
/// key and the remainder offset.
bool ParseVariantKey(std::string_view str_content, std::string& str_key,
                     bool& b_default, std::size_t& uint_rest) {
  b_default = false;
  std::size_t uint_i = 0;
  const std::size_t uint_len = str_content.size();
  if (uint_i < uint_len && str_content[uint_i] == '*') {
    b_default = true;
    ++uint_i;
  }
  if (uint_i >= uint_len || str_content[uint_i] != '[')
    return false;
  ++uint_i;
  const std::size_t uint_key_start = uint_i;
  while (uint_i < uint_len && str_content[uint_i] != ']')
    ++uint_i;
  if (uint_i >= uint_len)
    return false;
  str_key = std::string(str_content.substr(uint_key_start, uint_i - uint_key_start));
  ++uint_i;
  uint_rest = uint_i;
  return true;
}

/// 十六进制码点(\\uXXXX / \\UXXXXXXXX)
/// A hexadecimal code point (\\uXXXX / \\UXXXXXXXX).
std::optional<char32_t> ParseHexCode(std::string_view str_digits) {
  if (str_digits.empty() || str_digits.size() > 8)
    return std::nullopt;
  char32_t ch_value = 0;
  for (const char ch_digit : str_digits) {
    char32_t ch_nibble = 0;
    if (ch_digit >= '0' && ch_digit <= '9')
      ch_nibble = static_cast<char32_t>(ch_digit - '0');
    else if (ch_digit >= 'a' && ch_digit <= 'f')
      ch_nibble = static_cast<char32_t>(ch_digit - 'a' + 10);
    else if (ch_digit >= 'A' && ch_digit <= 'F')
      ch_nibble = static_cast<char32_t>(ch_digit - 'A' + 10);
    else
      return std::nullopt;
    ch_value = ch_value * 16 + ch_nibble;
  }
  return ch_value;
}

void AppendUtf8(std::string& str_out, char32_t ch_code) {
  if (ch_code <= 0x7F) {
    str_out.push_back(static_cast<char>(ch_code));
  } else if (ch_code <= 0x7FF) {
    str_out.push_back(static_cast<char>(0xC0 | (ch_code >> 6)));
    str_out.push_back(static_cast<char>(0x80 | (ch_code & 0x3F)));
  } else if (ch_code <= 0xFFFF) {
    str_out.push_back(static_cast<char>(0xE0 | (ch_code >> 12)));
    str_out.push_back(static_cast<char>(0x80 | ((ch_code >> 6) & 0x3F)));
    str_out.push_back(static_cast<char>(0x80 | (ch_code & 0x3F)));
  } else {
    str_out.push_back(static_cast<char>(0xF0 | (ch_code >> 18)));
    str_out.push_back(static_cast<char>(0x80 | ((ch_code >> 12) & 0x3F)));
    str_out.push_back(static_cast<char>(0x80 | ((ch_code >> 6) & 0x3F)));
    str_out.push_back(static_cast<char>(0x80 | (ch_code & 0x3F)));
  }
}

/// 模式源行(原始缩进保留 —— 选择子的变体/闭合判定依赖层级)
/// A pattern source line (the raw indent kept — the selector's
/// variant/close detection depends on the hierarchy).
struct PatternLine {
  std::string_view str_raw;
  std::size_t uint_indent = 0;
  bool b_blank = false;
};

/// 段:模式自其起始行(内联值或块首行)至下一属性行/块尾的行集
/// A segment: the line set of one pattern from its opening line (the inline
/// value or the block's first line) to the next attribute line/block end.
struct Segment {
  std::string str_name;  // 属性段名(主值段为空)| the attribute-segment
                         // name (empty for the main-value segment).
  std::string str_inline;
  bool b_has_inline = false;
  std::vector<PatternLine> vec_lines;
};

/// 行集 → 公共缩进(内联起始 = 0,块起始 = 非空行最小缩进)
/// The line set → the common indent (an inline start = 0; a block start =
/// the minimum indent over non-blank lines).
std::size_t CommonIndent(const Segment& seg_segment) {
  std::size_t uint_common = std::numeric_limits<std::size_t>::max();
  for (const PatternLine& line : seg_segment.vec_lines) {
    if (line.b_blank)
      continue;
    uint_common = std::min(uint_common, line.uint_indent);
  }
  return uint_common == std::numeric_limits<std::size_t>::max() ? 0 : uint_common;
}

/// 单行元素扫描目标(text/placeable 文本流)
/// The per-line element-scan target (the text/placeable stream).
class PatternBuilder {
 public:
  explicit PatternBuilder(Pattern& pattern_target) : pattern_(pattern_target) {}

  void AppendText(std::string_view str_text) {
    if (str_text.empty())
      return;
    if (!pattern_.vec_elements.empty() && pattern_.vec_elements.back().b_text)
      pattern_.vec_elements.back().str_text.append(str_text);
    else
      pattern_.vec_elements.push_back(PatternElement{true, std::string(str_text), {}});
  }

  void AppendRawText(char ch) { AppendText(std::string_view(&ch, 1)); }

  void AppendPlaceable(Placeable placeable_new) {
    pattern_.vec_elements.push_back(
        PatternElement{false, std::string{},
                       std::make_unique<Placeable>(std::move(placeable_new))});
  }

  bool LastIsPlaceable() const {
    return !pattern_.vec_elements.empty() && !pattern_.vec_elements.back().b_text;
  }

 private:
  Pattern& pattern_;
};

/// 行文本直载(反斜杠字面 —— Linguini 0.8.1 的实测面;转义仅存在于
/// 字符串字面量)
/// The in-line text appended verbatim (a literal backslash — the measured
/// Linguini 0.8.1 face; escapes live only inside string literals).
void AppendTextWithEscapes(PatternBuilder& builder_target,
                           std::string_view str_text) {
  builder_target.AppendText(str_text);
}

/// {"…"} 字符串字面量的转义展开
/// The escape expansion of the {"…"} string literal.
bool ParseStringLiteral(std::string_view str_inner, std::string& str_out) {
  str_out.clear();
  const std::size_t uint_len = str_inner.size();
  std::size_t uint_i = 0;
  while (uint_i < uint_len) {
    const char ch = str_inner[uint_i];
    if (ch == '\\') {
      if (uint_i + 1 >= uint_len)
        return false;
      const char ch_next = str_inner[uint_i + 1];
      if (ch_next == '"' || ch_next == '\\') {
        str_out.push_back(ch_next);
        uint_i += 2;
        continue;
      }
      if (ch_next == 'u' || ch_next == 'U') {
        const std::size_t uint_digits = ch_next == 'u' ? 4 : 8;
        if (uint_i + 2 + uint_digits <= uint_len) {
          const std::optional<char32_t> ch_code =
              ParseHexCode(str_inner.substr(uint_i + 2, uint_digits));
          if (ch_code.has_value() && *ch_code > 0 && *ch_code <= 0x10FFFF) {
            AppendUtf8(str_out, *ch_code);
            uint_i += 2 + uint_digits;
            continue;
          }
        }
        return false;
      }
      return false;
    }
    str_out.push_back(ch);
    ++uint_i;
  }
  return true;
}

/// 变量名([A-Za-z][A-Za-z0-9_-]*)
/// The variable name ([A-Za-z][A-Za-z0-9_-]*).
std::size_t VariableNameEnd(std::string_view str_line, std::size_t uint_start) {
  const std::size_t uint_len = str_line.size();
  if (uint_start >= uint_len ||
      !(std::isalpha(static_cast<unsigned char>(str_line[uint_start])) != 0))
    return std::string_view::npos;
  std::size_t uint_i = uint_start + 1;
  while (uint_i < uint_len) {
    const char ch = str_line[uint_i];
    if (std::isalnum(static_cast<unsigned char>(ch)) != 0 || ch == '_' ||
        ch == '-')
      ++uint_i;
    else
      break;
  }
  return uint_i;
}

/// 段 → 模式(递归下降:选择子以开启行缩进为基,变体更深、闭合同行级)
/// The segment → the pattern (recursive descent: a selector keys on its
/// opening line's indent; variants sit deeper, the close line at its level).
struct PatternCursor {
  const std::vector<PatternLine>* ptr_lines;
  std::size_t uint_index = 0;
};

bool MaterializePattern(PatternCursor& cur_cursor, std::size_t uint_base_indent,
                        std::size_t uint_stop_indent, std::string_view str_first,
                        bool b_has_first, Pattern& pattern_out,
                        const ErrorSink& fn_on_error,
                        bool b_variant_scope = false);

/// 单行元素扫描(text/placeable 流;uint_line_indent = 行原始缩进 —— 行内
/// 选择子的基)
/// The single-line element scan (the text/placeable stream; uint_line_indent
/// = the line's raw indent — the base of an in-line selector).
void ScanPatternLine(std::string_view str_content, std::size_t uint_line_indent,
                     PatternBuilder& builder_target, PatternCursor& cur_cursor,
                     const std::vector<PatternLine>& vec_lines,
                     const ErrorSink& fn_on_error);

bool MaterializeSelector(PatternCursor& cur_cursor, std::size_t uint_base_indent,
                         Placeable& placeable_selector,
                         std::string_view& str_close_remainder,
                         const ErrorSink& fn_on_error);


/// 单行扫描体(供行内值与块行共用)
/// The single-line scan body (shared by the inline value and block lines).
void ScanPatternLine(std::string_view str_content, std::size_t uint_line_indent,
                     PatternBuilder& builder_target, PatternCursor& cur_cursor,
                     const std::vector<PatternLine>& vec_lines,
                     const ErrorSink& fn_on_error) {
  (void)vec_lines;
  std::size_t uint_i = 0;
  while (uint_i < str_content.size()) {
    const char ch = str_content[uint_i];
    if (ch != '{') {
      const std::size_t uint_run_start = uint_i;
      while (uint_i < str_content.size() && str_content[uint_i] != '{')
        ++uint_i;
      AppendTextWithEscapes(builder_target, str_content.substr(uint_run_start, uint_i - uint_run_start));
      continue;
    }
    // placeable 开启 | a placeable opens.
    const std::size_t uint_open = uint_i;
    std::size_t uint_scan = uint_open + 1;
    while (uint_scan < str_content.size() &&
           (str_content[uint_scan] == ' ' || str_content[uint_scan] == '\t'))
      ++uint_scan;
    if (uint_scan < str_content.size() && str_content[uint_scan] == '-') {
      // { -name }:术语引用 | { -name }: a term reference.
      const std::size_t uint_name_end = VariableNameEnd(str_content, uint_scan + 1);
      if (uint_name_end != std::string_view::npos) {
        std::size_t uint_after = uint_name_end;
        while (uint_after < str_content.size() &&
               (str_content[uint_after] == ' ' || str_content[uint_after] == '	'))
          ++uint_after;
        if (uint_after < str_content.size() && str_content[uint_after] == '}') {
          Placeable placeable_new;
          placeable_new.kind = Placeable::Kind::TermRef;
          placeable_new.str_name =
              std::string(str_content.substr(uint_scan + 1, uint_name_end - (uint_scan + 1)));
          builder_target.AppendPlaceable(std::move(placeable_new));
          uint_i = uint_after + 1;
          continue;
        }
      }
    }
    if (uint_scan < str_content.size() && str_content[uint_scan] == '$') {
      const std::size_t uint_name_end = VariableNameEnd(str_content, uint_scan + 1);
      if (uint_name_end != std::string_view::npos) {
        std::size_t uint_after = uint_name_end;
        while (uint_after < str_content.size() &&
               (str_content[uint_after] == ' ' || str_content[uint_after] == '\t'))
          ++uint_after;
        if (uint_after < str_content.size() && str_content[uint_after] == '}') {
          Placeable placeable_new;
          placeable_new.kind = Placeable::Kind::Variable;
          placeable_new.str_name =
              std::string(str_content.substr(uint_scan + 1, uint_name_end - (uint_scan + 1)));
          builder_target.AppendPlaceable(std::move(placeable_new));
          uint_i = uint_after + 1;
          continue;
        }
        if (uint_after + 1 < str_content.size() &&
            str_content[uint_after] == '-' && str_content[uint_after + 1] == '>') {
          // 选择子:变体起于后续行
          // a selector: the variants start on the following lines.
          Placeable placeable_new;
          placeable_new.kind = Placeable::Kind::Selector;
          placeable_new.str_name =
              std::string(str_content.substr(uint_scan + 1, uint_name_end - (uint_scan + 1)));
          std::string_view str_close_remainder;
          if (!MaterializeSelector(cur_cursor, uint_line_indent, placeable_new,
                                   str_close_remainder, fn_on_error))
            return;
          builder_target.AppendPlaceable(std::move(placeable_new));
          // 闭合行 '}' 后的余文归外层流(同行,无断行)
          // the remainder after the close line's '}' stays in the outer
          // stream (same line, no break).
          AppendTextWithEscapes(builder_target, str_close_remainder);
          uint_i = str_content.size();
          continue;
        }
      }
    }
    if (uint_scan < str_content.size() && str_content[uint_scan] == '"') {
      std::size_t uint_close = std::string_view::npos;
      for (std::size_t uint_k = uint_scan + 1; uint_k < str_content.size();
           ++uint_k) {
        if (str_content[uint_k] == '\\') {
          ++uint_k;
          continue;
        }
        if (str_content[uint_k] == '"') {
          uint_close = uint_k;
          break;
        }
      }
      if (uint_close != std::string_view::npos &&
          uint_close + 1 < str_content.size() &&
          str_content[uint_close + 1] == '}') {
        std::string str_literal;
        if (ParseStringLiteral(
                str_content.substr(uint_scan + 1, uint_close - (uint_scan + 1)),
                str_literal)) {
          Placeable placeable_new;
          placeable_new.kind = Placeable::Kind::StringLiteral;
          placeable_new.str_name = std::move(str_literal);
          builder_target.AppendPlaceable(std::move(placeable_new));
          uint_i = uint_close + 2;
          continue;
        }
      }
    }
    // 未支持/畸形的 placeable:按解析错误上抛
    // an unsupported/malformed placeable: raised as a parse error.
    if (fn_on_error)
      fn_on_error("fluent: unsupported placeable");
    return;
  }

}

/// 选择子体:自开启行下一行至同级 '}' 闭合行;变体行更深。闭合时游标
/// 越过闭合行,余文(若有)经出参交还外层模式。
/// The selector body: from the line after the opening one to the peer-level
/// '}' close line; variant lines sit deeper. On the close the cursor moves
/// past the close line; any remainder returns to the outer pattern through
/// the out-parameter.
bool MaterializeSelector(PatternCursor& cur_cursor, std::size_t uint_base_indent,
                         Placeable& placeable_selector,
                         std::string_view& str_close_remainder,
                         const ErrorSink& fn_on_error) {
  const auto& vec_lines = *cur_cursor.ptr_lines;
  while (cur_cursor.uint_index < vec_lines.size()) {
    const PatternLine& line = vec_lines[cur_cursor.uint_index];
    if (line.b_blank) {
      ++cur_cursor.uint_index;
      continue;
    }
    if (line.uint_indent <= uint_base_indent) {
      std::string_view str_content = line.str_raw;
      const std::size_t uint_strip = std::min(line.uint_indent, str_content.size());
      str_content = str_content.substr(uint_strip);
      if (!str_content.empty() && str_content.front() == '}') {
        str_close_remainder = str_content.substr(1);
        ++cur_cursor.uint_index;
        return true;
      }
      break;
    }
    std::string_view str_content = line.str_raw;
    const std::size_t uint_strip = std::min(line.uint_indent, str_content.size());
    str_content = str_content.substr(uint_strip);
    std::string str_key;
    bool b_default = false;
    std::size_t uint_rest = 0;
    if (!ParseVariantKey(str_content, str_key, b_default, uint_rest)) {
      if (fn_on_error)
        fn_on_error("fluent: malformed selector variant line");
      ++cur_cursor.uint_index;
      continue;
    }
    Variant variant_new;
    variant_new.str_key = str_key;
    variant_new.b_default = b_default;
    const std::size_t uint_variant_indent = line.uint_indent;
    ++cur_cursor.uint_index;
    PatternCursor cur_variant{cur_cursor.ptr_lines, cur_cursor.uint_index};
    Pattern pattern_variant;
    std::string_view str_variant_inline = str_content.substr(uint_rest);
    {
      const std::size_t uint_first = str_variant_inline.find_first_not_of(" 	");
      str_variant_inline = uint_first == std::string_view::npos
                               ? std::string_view{}
                               : str_variant_inline.substr(uint_first);
    }

    // 空内联的多行变体:续行按自身公共缩进 dedent(变体键行缩进仅作
    // 层级参考)
    // a multiline variant with an empty inline: the continuation lines
    // dedent by their own common indent (the variant-key line's indent is
    // a hierarchy reference only).
    std::size_t uint_variant_base = uint_variant_indent;
    if (str_variant_inline.empty()) {
      std::size_t uint_common = std::numeric_limits<std::size_t>::max();
      for (std::size_t uint_k = cur_variant.uint_index;
           uint_k < vec_lines.size(); ++uint_k) {
        const PatternLine& line_peek = vec_lines[uint_k];
        if (line_peek.b_blank)
          continue;
        std::string_view str_peek = line_peek.str_raw.substr(line_peek.uint_indent);
        std::size_t uint_peek = 0;
        if (uint_peek < str_peek.size() && str_peek[uint_peek] == '*')
          ++uint_peek;
        const bool b_key_line =
            (uint_peek < str_peek.size() && str_peek[uint_peek] == '[') ||
            (!str_peek.empty() && str_peek.front() == '}');
        if (b_key_line || line_peek.uint_indent <= uint_base_indent)
          break;
        uint_common = std::min(uint_common, line_peek.uint_indent);
      }
      if (uint_common != std::numeric_limits<std::size_t>::max())
        uint_variant_base = uint_common;
    }

    if (!MaterializePattern(cur_variant, uint_variant_base,
                            uint_variant_indent,
                            str_variant_inline, !str_variant_inline.empty(),
                            pattern_variant, fn_on_error, true))
      return false;
    if (!pattern_variant.vec_elements.empty() &&
        pattern_variant.vec_elements.front().b_text &&
        !pattern_variant.vec_elements.front().str_text.empty() &&
        pattern_variant.vec_elements.front().str_text.front() == '\n')
      pattern_variant.vec_elements.front().str_text.erase(0, 1);
    cur_cursor.uint_index = cur_variant.uint_index;
    variant_new.vec_pattern = std::move(pattern_variant.vec_elements);
    placeable_selector.vec_variants.push_back(std::move(variant_new));
  }
  if (fn_on_error)
    fn_on_error("fluent: unterminated selector");
  return false;
}

bool MaterializePattern(PatternCursor& cur_cursor, std::size_t uint_base_indent,
                        std::size_t uint_stop_indent, std::string_view str_first,
                        bool b_has_first, Pattern& pattern_out,
                        const ErrorSink& fn_on_error,
                        bool b_variant_scope) {
  const auto& vec_lines = *cur_cursor.ptr_lines;
  PatternBuilder builder(pattern_out);
  // 行内值/首行走同一 placeable 扫描(无行首折行;选择子的基缩进取 0)
  // the inline value/first line runs the same placeable scan (no leading
  // line break; a selector keyed there bases its indent at 0).
  if (b_has_first)
    ScanPatternLine(str_first, 0, builder, cur_cursor, vec_lines, fn_on_error);

  while (cur_cursor.uint_index < vec_lines.size()) {
    const PatternLine& line = vec_lines[cur_cursor.uint_index];
    if (!line.b_blank && line.uint_indent <= uint_stop_indent)
      break;
    if (b_variant_scope && !line.b_blank) {
      // 变体域:任何变体键行/闭合行终结本变体的续行(变体键行缩进
      // 可不一致 —— Linguini 的实测面)
      // the variant scope: any variant-key/close line ends this variant's
      // continuation (variant-key indents may disagree — the measured
      // Linguini face).
      std::string_view str_peek = line.str_raw.substr(line.uint_indent);
      if (!str_peek.empty() && str_peek.front() == '}')
        break;
      std::size_t uint_peek = 0;
      if (uint_peek < str_peek.size() && str_peek[uint_peek] == '*')
        ++uint_peek;
      if (uint_peek < str_peek.size() && str_peek[uint_peek] == '[')
        break;
    }
    ++cur_cursor.uint_index;
    if (line.b_blank) {
      builder.AppendText("\n");
      continue;
    }
    std::string_view str_content = line.str_raw;
    const std::size_t uint_strip = std::min(uint_base_indent, str_content.size());
    str_content = str_content.substr(uint_strip);

    // 行首换行(邻接 placeable 时由规范段统一修剪)
    // the leading line break (the spec-stage pass trims it when adjacent to
    // a placeable).
    builder.AppendText("\n");
    ScanPatternLine(str_content, line.uint_indent, builder, cur_cursor,
                    vec_lines, fn_on_error);
  }
  return true;
}

/// 段物化(公共缩进扣除 + 递归扫描 + 规范化)
/// The segment materialization (the common-indent deduction + the recursive
/// scan + the normalization).
bool MaterializeSegment(const Segment& seg_segment, Pattern& pattern_out,
                        const ErrorSink& fn_on_error) {
  const std::size_t uint_common = CommonIndent(seg_segment);
  PatternCursor cur_cursor{&seg_segment.vec_lines, 0};
  if (!MaterializePattern(cur_cursor, uint_common, 0, seg_segment.str_inline,
                          seg_segment.b_has_inline, pattern_out, fn_on_error))
    return false;
  // 块起始的首行行首 '\n' 去除(模式不以前导断行开头)
  // drop the first line's leading '\n' (a pattern never starts with a
  // leading break).
  if (!pattern_out.vec_elements.empty() && pattern_out.vec_elements.front().b_text &&
      !pattern_out.vec_elements.front().str_text.empty() &&
      pattern_out.vec_elements.front().str_text.front() == '\n')
    pattern_out.vec_elements.front().str_text.erase(0, 1);
  return true;
}

}  // namespace

FluentResource ParseResource(std::string_view str_text,
                             const ErrorSink& fn_on_error) {
  FluentResource resource_out;

  std::vector<std::string_view> vec_lines;
  {
    std::size_t uint_start = 0;
    const std::size_t uint_len = str_text.size();
    while (uint_start <= uint_len) {
      const std::size_t uint_eol = str_text.find('\n', uint_start);
      const std::size_t uint_end =
          uint_eol == std::string_view::npos ? uint_len : uint_eol;
      std::string_view str_line = str_text.substr(uint_start, uint_end - uint_start);
      if (!str_line.empty() && str_line.back() == '\r')
        str_line.remove_suffix(1);
      vec_lines.push_back(str_line);
      if (uint_eol == std::string_view::npos)
        break;
      uint_start = uint_eol + 1;
    }
  }

  std::size_t uint_i = 0;
  const std::size_t uint_count = vec_lines.size();
  while (uint_i < uint_count) {
    std::string_view str_line = vec_lines[uint_i];
    if (IsBlank(str_line) || (!str_line.empty() && str_line.front() == '#')) {
      ++uint_i;
      continue;
    }

    bool b_term = false;
    std::size_t uint_id_start = 0;
    if (!str_line.empty() && str_line.front() == '-') {
      b_term = true;
      uint_id_start = 1;
    }
    const std::size_t uint_eq_sub =
        IdentifierEnd(b_term ? str_line.substr(uint_id_start) : str_line);
    const std::size_t uint_eq = uint_eq_sub + uint_id_start;
    if (uint_eq == std::string_view::npos) {
      // junk:跳至下一空行/合法条目/注释
      // junk: skip to the next blank line/valid entry/comment.
      if (fn_on_error)
        fn_on_error("fluent: junk entry");
      ++resource_out.num_errors;
      ++uint_i;
      while (uint_i < uint_count) {
        std::string_view str_scan = vec_lines[uint_i];
        if (IsBlank(str_scan) || (!str_scan.empty() && str_scan.front() == '#'))
          break;
        if (IdentifierEnd(str_scan) != std::string_view::npos)
          break;
        if (!str_scan.empty() && str_scan.front() == '-') {
          const std::size_t uint_eq_term = IdentifierEnd(str_scan.substr(1));
          if (uint_eq_term != std::string_view::npos)
            break;
        }
        ++uint_i;
      }
      continue;
    }

    std::size_t uint_id_end = uint_eq;
    while (uint_id_end > uint_id_start &&
           (str_line[uint_id_end - 1] == ' ' || str_line[uint_id_end - 1] == '	'))
      --uint_id_end;
    const std::string str_id =
        std::string(str_line.substr(uint_id_start, uint_id_end - uint_id_start));
    std::string_view str_inline = str_line.substr(uint_eq + 1);
    {
      const std::size_t uint_trim = str_inline.find_first_not_of(" \t");
      str_inline = uint_trim == std::string_view::npos
                       ? std::string_view{}
                       : str_inline.substr(uint_trim);
    }

    // 收集块:空行或缩进 >0 的行;尾随空行剪除
    // collect the block: blank or indented lines; trailing blanks removed.
    std::size_t uint_block_end = uint_i + 1;
    while (uint_block_end < uint_count) {
      std::string_view str_next = vec_lines[uint_block_end];
      if (!IsBlank(str_next) && IndentOf(str_next) == 0)
        break;
      ++uint_block_end;
    }
    std::size_t uint_block_last = uint_block_end;
    while (uint_block_last > uint_i + 1 && IsBlank(vec_lines[uint_block_last - 1]))
      --uint_block_last;

    FluentMessage message_new;
    Segment seg_current;
    if (!str_inline.empty()) {
      seg_current.str_inline = std::string(str_inline);
      seg_current.b_has_inline = true;
    }
    bool b_main_open = true;

    const auto flush_segment = [&]() {
      if (seg_current.vec_lines.empty() && !seg_current.b_has_inline) {
        // 空主段也关闭值槽(上游 `msg =` 仅属性行的合法形态)
        // an empty main segment still closes the value slot (upstream's
        // legal `msg =` attribute-only form).
        if (b_main_open)
          b_main_open = false;
        return;
      }
      Pattern pattern_new;
      if (!MaterializeSegment(seg_current, pattern_new, fn_on_error)) {
        ++resource_out.num_errors;
        return;
      }
      if (b_main_open) {
        message_new.b_has_value = true;
        message_new.pattern_value = std::move(pattern_new);
        b_main_open = false;
      } else {
        message_new.vec_attributes.emplace_back(std::move(seg_current.str_name),
                                                std::move(pattern_new));
      }
      seg_current = Segment{};
      seg_current.str_name.clear();
    };

    for (std::size_t uint_k = uint_i + 1; uint_k < uint_block_last; ++uint_k) {
      std::string_view str_block_line = vec_lines[uint_k];
      std::string str_attr_name;
      std::size_t uint_attr_eq = 0;
      if (!IsBlank(str_block_line) &&
          ParseAttributeLine(str_block_line, str_attr_name, uint_attr_eq)) {
        flush_segment();
        seg_current.str_name = std::move(str_attr_name);
        std::string_view str_attr_inline = str_block_line.substr(uint_attr_eq + 1);
        const std::size_t uint_trim = str_attr_inline.find_first_not_of(" \t");
        str_attr_inline = uint_trim == std::string_view::npos
                              ? std::string_view{}
                              : str_attr_inline.substr(uint_trim);
        if (!str_attr_inline.empty()) {
          seg_current.str_inline = std::string(str_attr_inline);
          seg_current.b_has_inline = true;
        }
        continue;
      }
      PatternLine line_new;
      line_new.str_raw = str_block_line;
      line_new.uint_indent = IndentOf(str_block_line);
      line_new.b_blank = IsBlank(str_block_line);
      seg_current.vec_lines.push_back(line_new);
    }
    flush_segment();

    if (!b_term)
      resource_out.vec_messages.emplace_back(str_id, std::move(message_new));
    else if (message_new.b_has_value)
      resource_out.vec_terms.emplace_back(str_id,
                                          std::move(message_new.pattern_value));
    uint_i = uint_block_end;
  }

  return resource_out;
}

}  // namespace ora::fluent
