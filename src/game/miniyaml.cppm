export module ora.game.miniyaml;

import std;

export {
namespace ora::game {

struct SourceLocation {
  std::string str_name;
  int int4_line{};
};

struct MiniYamlNode {
  std::string str_key;
  std::string str_value;
  std::optional<std::string> opt_comment;
  SourceLocation loc_location;
  std::vector<MiniYamlNode> vec_children;
};

[[nodiscard]] std::expected<std::vector<MiniYamlNode>, std::string> miniyaml_from_string(
    std::string_view str_text, std::string_view str_name, bool b_discard_comments_and_whitespace = true);

[[nodiscard]] std::expected<std::vector<MiniYamlNode>, std::string> miniyaml_from_file(
    std::string_view str_path, bool b_discard_comments_and_whitespace = true);

[[nodiscard]] std::expected<std::vector<MiniYamlNode>, std::string> miniyaml_from_file_named(
    std::string_view str_path, std::string_view str_name,
    bool b_discard_comments_and_whitespace = true);

[[nodiscard]] std::string miniyaml_write_to_string(const std::vector<MiniYamlNode>& vec_nodes);

[[nodiscard]] std::expected<void, std::string> miniyaml_write_to_file(
    std::string_view str_path, const std::vector<MiniYamlNode>& vec_nodes);

}

namespace ora::game {

[[nodiscard]] std::expected<std::vector<MiniYamlNode>, std::string> miniyaml_merge(
    const std::vector<std::vector<MiniYamlNode>>& vec_sources);

}
}
