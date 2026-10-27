#pragma once
import std;

namespace ora::yaml {
class MiniYaml;
}

namespace ora::net {

enum class FluentArgumentType { String = 0, Number = 1 };

struct FluentArgument {
  std::string str_key;
  std::string str_value;
  FluentArgumentType type = FluentArgumentType::String;
};

class FluentMessage {
 public:
  static constexpr int kProtocolVersion = 1;

  static std::string Serialize(std::string_view str_key,
                               std::span<const FluentArgument> vec_args);

  static FluentMessage FromYaml(const yaml::MiniYaml& yaml_node);

  const std::string& Key() const { return str_key_; }
  const std::vector<FluentArgument>& Arguments() const { return vec_args_; }

 private:
  std::string str_key_;
  std::vector<FluentArgument> vec_args_;
};

}
