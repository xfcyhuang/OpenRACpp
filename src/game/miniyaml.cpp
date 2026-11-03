#include "game/miniyaml.hpp"

namespace ora::game {

namespace {

constexpr int kSpacesPerLevel{4};

bool is_ascii_space_char(char ch) {
  return ch == ' ' || ch == '\t' || ch == '\n' || ch == '\v' || ch == '\f' || ch == '\r';
}

std::string_view trim_ascii(std::string_view str_in) {
  while (!str_in.empty() && is_ascii_space_char(str_in.front())) str_in.remove_prefix(1);
  while (!str_in.empty() && is_ascii_space_char(str_in.back())) str_in.remove_suffix(1);
  return str_in;
}

std::string replace_all(std::string_view str_in, std::string_view str_from, std::string_view str_to) {
  std::string str_out;
  str_out.reserve(str_in.size());
  std::size_t pos{0};
  while (pos < str_in.size()) {
    const std::size_t idx_hit{str_in.find(str_from, pos)};
    if (idx_hit == std::string_view::npos) {
      str_out.append(str_in.substr(pos));
      break;
    }
    str_out.append(str_in.substr(pos, idx_hit - pos));
    str_out.append(str_to);
    pos = idx_hit + str_from.size();
  }
  return str_out;
}

std::vector<std::string_view> split_lines(std::string_view str_text) {
  std::vector<std::string_view> vec_lines;
  std::size_t pos{0};
  while (pos < str_text.size()) {
    const std::size_t idx_nl{str_text.find('\n', pos)};
    if (idx_nl == std::string_view::npos) {
      vec_lines.push_back(str_text.substr(pos));
      break;
    }
    std::size_t sz_len{idx_nl - pos};
    if (sz_len > 0 && str_text[idx_nl - 1] == '\r') --sz_len;
    vec_lines.push_back(str_text.substr(pos, sz_len));
    pos = idx_nl + 1;
  }
  return vec_lines;
}

struct ParsedLine {
  int int4_level{};
  std::string str_key;
  std::string str_value;
  std::optional<std::string> opt_comment;
  SourceLocation loc_location;
};

ParsedLine parse_line(std::string_view str_line, std::string_view str_name, int int4_line,
                       bool b_discard_comments_and_whitespace) {
  ParsedLine line_out;
  line_out.loc_location = SourceLocation{std::string{str_name}, int4_line};

  int int4_spaces{0};
  std::size_t idx_key_start{0};
  bool b_text_start{false};
  while (idx_key_start < str_line.size() && !b_text_start) {
    const char ch{str_line[idx_key_start]};
    if (ch == '\n' || ch == '\r') break;
    if (ch == ' ') {
      ++int4_spaces;
      if (int4_spaces >= kSpacesPerLevel) {
        int4_spaces = 0;
        ++line_out.int4_level;
      }
      ++idx_key_start;
    } else if (ch == '\t') {
      ++line_out.int4_level;
      ++idx_key_start;
    } else {
      b_text_start = true;
    }
  }

  std::size_t sz_key_length{str_line.size() - idx_key_start};
  std::size_t idx_value_start{std::string_view::npos};
  std::size_t sz_value_length{0};
  std::size_t idx_comment_start{std::string_view::npos};
  for (std::size_t i{0}; i < str_line.size(); ++i) {
    if (idx_value_start == std::string_view::npos && str_line[i] == ':') {
      idx_value_start = i + 1;
      sz_key_length = i - idx_key_start;
      sz_value_length = str_line.size() - i - 1;
    }
    if (idx_comment_start == std::string_view::npos && str_line[i] == '#' &&
        (i == 0 || str_line[i - 1] != '\\')) {
      idx_comment_start = i + 1;
      if (i <= idx_key_start + sz_key_length)
        sz_key_length = i - idx_key_start;
      else
        sz_value_length = i - idx_value_start;
      break;
    }
  }

  if (sz_key_length > 0)
    line_out.str_key = std::string{trim_ascii(str_line.substr(idx_key_start, sz_key_length))};

  if (idx_value_start != std::string_view::npos) {
    const std::string_view str_trimmed{trim_ascii(str_line.substr(idx_value_start, sz_value_length))};
    if (!str_trimmed.empty()) line_out.str_value = std::string{str_trimmed};
  }

  if (idx_comment_start != std::string_view::npos && !b_discard_comments_and_whitespace)
    line_out.opt_comment = std::string{str_line.substr(idx_comment_start)};

  if (line_out.str_value.size() > 1) {
    std::size_t sz_trim_leading{0};
    std::size_t sz_trim_trailing{0};
    if (line_out.str_value[0] == '\\' && (line_out.str_value[1] == ' ' || line_out.str_value[1] == '\t'))
      sz_trim_leading = 1;
    if (line_out.str_value.back() == '\\' &&
        (line_out.str_value[line_out.str_value.size() - 2] == ' ' ||
         line_out.str_value[line_out.str_value.size() - 2] == '\t'))
      sz_trim_trailing = 1;
    if (sz_trim_leading + sz_trim_trailing > 0)
      line_out.str_value = line_out.str_value.substr(
          sz_trim_leading, line_out.str_value.size() - sz_trim_leading - sz_trim_trailing);
    if (line_out.str_value.find("\\#") != std::string::npos)
      line_out.str_value = replace_all(line_out.str_value, "\\#", "#");
  }

  return line_out;
}

void build_completed_subnode(std::vector<std::vector<MiniYamlNode>>& vec_result,
                             std::vector<ParsedLine>& vec_pending, int int4_level) {
  const int int4_last_level{vec_pending.back().int4_level};
  while (static_cast<int>(vec_result.size()) <= int4_last_level) vec_result.emplace_back();

  while (!vec_pending.empty() && vec_pending.back().int4_level >= int4_level) {
    const ParsedLine& line_parent{vec_pending.back()};
    std::size_t sz_start_of_range{vec_pending.size() - 1};
    while (sz_start_of_range > 0 && vec_pending[sz_start_of_range - 1].int4_level == line_parent.int4_level)
      --sz_start_of_range;

    for (std::size_t i{sz_start_of_range}; i + 1 < vec_pending.size(); ++i) {
      const ParsedLine& line_sibling{vec_pending[i]};
      vec_result[line_sibling.int4_level].push_back(
          MiniYamlNode{line_sibling.str_key, line_sibling.str_value, line_sibling.opt_comment,
                       line_sibling.loc_location, {}});
    }

    std::vector<MiniYamlNode> vec_children{};
    if (static_cast<int>(vec_result.size()) > line_parent.int4_level + 1)
      vec_children = std::move(vec_result[line_parent.int4_level + 1]);
    vec_result[line_parent.int4_level].push_back(
        MiniYamlNode{line_parent.str_key, line_parent.str_value, line_parent.opt_comment,
                     line_parent.loc_location, std::move(vec_children)});

    vec_pending.resize(sz_start_of_range);
  }
}

std::string bad_indent_message(std::string_view str_name, int int4_line) {
  char buf_line[16];
  const auto res_chars{std::to_chars(buf_line, buf_line + sizeof(buf_line), int4_line)};
  std::string str_msg{"Bad indent in miniyaml at "};
  str_msg += str_name;
  str_msg += ':';
  str_msg.append(buf_line, res_chars.ptr);
  return str_msg;
}

void append_node_lines(const MiniYamlNode& node, int int4_depth, std::vector<std::string>& vec_lines) {
  const bool b_has_key{!node.str_key.empty()};
  const bool b_has_value{!node.str_value.empty()};
  const bool b_has_comment{node.opt_comment.has_value()};

  std::string str_line;
  if (b_has_key) {
    str_line += node.str_key;
    str_line += ':';
  }
  if (b_has_value) {
    str_line += ' ';
    str_line += replace_all(node.str_value, "#", "\\#");
  }
  if (b_has_comment) {
    if (b_has_key || b_has_value) str_line += ' ';
    str_line += '#';
    str_line += *node.opt_comment;
  }

  std::string str_indented(static_cast<std::size_t>(int4_depth), '\t');
  str_indented += str_line;
  vec_lines.push_back(std::move(str_indented));

  for (const MiniYamlNode& node_child : node.vec_children)
    append_node_lines(node_child, int4_depth + 1, vec_lines);
}

}

std::expected<std::vector<MiniYamlNode>, std::string> miniyaml_from_string(
    std::string_view str_text, std::string_view str_name, bool b_discard_comments_and_whitespace) {
  std::vector<std::vector<MiniYamlNode>> vec_result{{}};
  std::vector<ParsedLine> vec_pending;

  int int4_line{0};
  for (const std::string_view str_line : split_lines(str_text)) {
    ++int4_line;
    ParsedLine line_parsed{parse_line(str_line, str_name, int4_line, b_discard_comments_and_whitespace)};
    if (!line_parsed.str_key.empty() || !b_discard_comments_and_whitespace) {
      if (!vec_pending.empty() && vec_pending.back().int4_level < line_parsed.int4_level - 1)
        return std::unexpected(bad_indent_message(str_name, int4_line));

      while (!vec_pending.empty() && vec_pending.back().int4_level > line_parsed.int4_level)
        build_completed_subnode(vec_result, vec_pending, line_parsed.int4_level);

      vec_pending.push_back(std::move(line_parsed));
    }
  }

  if (!vec_pending.empty()) build_completed_subnode(vec_result, vec_pending, 0);

  return std::move(vec_result[0]);
}

std::expected<std::vector<MiniYamlNode>, std::string> miniyaml_from_file(
    std::string_view str_path, bool b_discard_comments_and_whitespace) {
  return miniyaml_from_file_named(str_path, str_path, b_discard_comments_and_whitespace);
}

std::expected<std::vector<MiniYamlNode>, std::string> miniyaml_from_file_named(
    std::string_view str_path, std::string_view str_name,
    bool b_discard_comments_and_whitespace) {
  std::ifstream fs_file{std::string{str_path}, std::ios::binary};
  if (!fs_file) return std::unexpected("cannot open file: " + std::string{str_path});

  std::string str_text;
  fs_file.seekg(0, std::ios::end);
  const std::streampos pos_end{fs_file.tellg()};
  str_text.resize(static_cast<std::size_t>(pos_end));
  fs_file.seekg(0, std::ios::beg);
  fs_file.read(str_text.data(), static_cast<std::streamsize>(str_text.size()));

  if (str_text.starts_with("\xEF\xBB\xBF")) str_text.erase(0, 3);

  return miniyaml_from_string(str_text, str_name, b_discard_comments_and_whitespace);
}

std::string miniyaml_write_to_string(const std::vector<MiniYamlNode>& vec_nodes) {
  std::vector<std::string> vec_lines;
  for (const MiniYamlNode& node : vec_nodes) append_node_lines(node, 0, vec_lines);

  std::string str_out;
  for (std::size_t i{0}; i < vec_lines.size(); ++i) {
    if (i > 0) str_out += '\n';
    str_out += vec_lines[i];
  }
  while (!str_out.empty() && str_out.back() == '\n') str_out.pop_back();
  str_out += '\n';
  return str_out;
}

}
