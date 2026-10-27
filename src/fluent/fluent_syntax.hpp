#pragma once
import std;

namespace ora::fluent {

using ErrorSink = std::function<void(std::string_view)>;

struct PatternElement;

struct Variant {
  std::string str_key;
  bool b_default = false;
  std::vector<PatternElement> vec_pattern;
};

struct Placeable {
  enum class Kind { Variable, StringLiteral, Selector, TermRef };
  Kind kind = Kind::Variable;
  std::string str_name;
  std::vector<Variant> vec_variants;
};

struct PatternElement {
  bool b_text = true;
  std::string str_text;
  std::unique_ptr<Placeable> ptr_placeable;
};

struct Pattern {
  std::vector<PatternElement> vec_elements;
};

struct FluentMessage {
  bool b_has_value = false;
  Pattern pattern_value;
  std::vector<std::pair<std::string, Pattern>> vec_attributes;
};

struct FluentResource {
  std::vector<std::pair<std::string, FluentMessage>> vec_messages;
  std::vector<std::pair<std::string, Pattern>> vec_terms;
  std::size_t num_errors = 0;
};

FluentResource ParseResource(std::string_view str_text,
                             const ErrorSink& fn_on_error);

}
