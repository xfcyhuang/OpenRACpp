import std;

#include "net/fluent_message.hpp"

#include "yaml/mini_yaml.hpp"

namespace ora::net {

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

}

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

FluentMessage FluentMessage::FromYaml(const yaml::MiniYaml& yaml_node) {
  auto message = FluentMessage{};

  const auto* node_key = yaml_node.NodeWithKeyOrDefault("Key");
  if (node_key != nullptr && node_key->Value.Value != nullptr)
    message.str_key_ = *node_key->Value.Value;

  const auto* node_arguments = yaml_node.NodeWithKeyOrDefault("Arguments");
  if (node_arguments == nullptr)
    return message;

  message.vec_args_.reserve(node_arguments->Value.Nodes.size());
  for (const auto& node_argument : node_arguments->Value.Nodes) {

    auto argument = FluentArgument{};
    const auto* node_arg_key =
        node_argument.Value.NodeWithKeyOrDefault("Key");
    if (node_arg_key != nullptr && node_arg_key->Value.Value != nullptr)
      argument.str_key = *node_arg_key->Value.Value;
    const auto* node_arg_value =
        node_argument.Value.NodeWithKeyOrDefault("Value");
    if (node_arg_value != nullptr && node_arg_value->Value.Value != nullptr)
      argument.str_value = *node_arg_value->Value.Value;
    const auto* node_arg_type =
        node_argument.Value.NodeWithKeyOrDefault("Type");
    if (node_arg_type != nullptr && node_arg_type->Value.Value != nullptr) {
      const std::string_view str_type = *node_arg_type->Value.Value;
      if (str_type == "Number")
        argument.type = FluentArgumentType::Number;
      else if (str_type == "String")
        argument.type = FluentArgumentType::String;
    }

    message.vec_args_.push_back(std::move(argument));
  }

  return message;
}

}
