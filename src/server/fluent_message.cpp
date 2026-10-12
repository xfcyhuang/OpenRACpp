// UPSTREAM: OpenRA.Game/Network/FluentMessage.cs @e180e3225 L93-124
//          Serialize 全文(Serialize's whole text)
import std;

#include "server/fluent_message.hpp"

#include "yaml/mini_yaml.hpp"

namespace ora::server {

namespace {

yaml::MiniYamlNode MakeFluentLeaf(yaml::StringPool& pool,
                                  std::string_view str_key,
                                  std::string_view str_text) {
  return yaml::MiniYamlNode{pool.Intern(str_key),
                            yaml::MiniYaml{pool.Intern(str_text)}};
}

const char* FormatArgumentType(FluentArgumentType type) {
  return type == FluentArgumentType::Number ? "Number" : "String";
}

std::string JoinWithNewline(const std::vector<std::string>& vec_lines) {
  auto str_out = std::string{};
  for (std::size_t i = 0; i < vec_lines.size(); ++i) {
    if (i != 0)
      str_out += '\n';
    str_out += vec_lines[i];
  }
  return str_out;
}

}  // namespace

std::string FluentMessage::Serialize(
    std::string_view str_key, std::span<const FluentArgument> vec_args) {
  auto pool = yaml::StringPool{};
  auto vec_root = std::vector<yaml::MiniYamlNode>{};

  vec_root.push_back(MakeFluentLeaf(pool, "Protocol", "1"));
  vec_root.push_back(MakeFluentLeaf(pool, "Key", str_key));

  if (!vec_args.empty()) {
    auto vec_argument_nodes = std::vector<yaml::MiniYamlNode>{};
    for (std::size_t i = 0; i < vec_args.size(); ++i) {
      const auto& arg = vec_args[i];
      auto vec_fields = std::vector<yaml::MiniYamlNode>{};
      vec_fields.push_back(MakeFluentLeaf(pool, "Key", arg.str_key));
      vec_fields.push_back(MakeFluentLeaf(pool, "Value", arg.str_value));
      vec_fields.push_back(
          MakeFluentLeaf(pool, "Type", FormatArgumentType(arg.type)));
      vec_argument_nodes.push_back(
          yaml::MiniYamlNode{pool.Intern(std::format("Argument@{}", i)),
                             yaml::MiniYaml{nullptr, std::move(vec_fields)}});
    }

    vec_root.push_back(yaml::MiniYamlNode{
        pool.Intern("Arguments"),
        yaml::MiniYaml{nullptr, std::move(vec_argument_nodes)}});
  }

  auto vec_lines = std::vector<std::string>{};
  const std::string* str_key_line = pool.Intern("FluentMessage");
  yaml::MiniYaml{nullptr, std::move(vec_root)}.ToLines(vec_lines, str_key_line,
                                                       nullptr);
  return JoinWithNewline(vec_lines);
}

}  // namespace ora::server
