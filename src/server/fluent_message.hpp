// UPSTREAM: OpenRA.Game/Network/FluentMessage.cs @e180e3225 L19-124(服务
//          器 → 客户端的通知消息线格式:Protocol/Key/Arguments 三段 +
//          FluentArgument 的 Key/Value/Type;Serialize 的 ToLines +
//          JoinWith("\n") 形态逐字;Deserialize 随 Phase 6 Fluent 批)
//          FluentMessage.cs L19-124 (the server→client notification wire
//          format: the Protocol/Key/Arguments trio + FluentArgument's
//          Key/Value/Type; Serialize's ToLines + JoinWith("\n") shape
//          verbatim; Deserialize rides the Phase 6 Fluent batch).
#pragma once
import std;


namespace ora::server {

/// FluentArgumentType(FluentMessage.cs L22-27)
/// FluentArgumentType (FluentMessage.cs L22-27).
enum class FluentArgumentType { String = 0, Number = 1 };

/// FluentArgument(FluentMessage.cs L29-50):key/value 对;上游 value 为
/// object(string 或数值),此处以 value 文本 + 类型位承载(序列化输出
/// 逐字等价 —— FormatValue 对 string 直返、数值不变量十进制)
/// FluentArgument (L29-50): a key/value pair; upstream's value is an
/// object (a string or a number), carried here as the value text + a type
/// bit (serialization output verbatim-equal — FormatValue returns strings
/// as-is and numbers as invariant decimals).
struct FluentArgument {
  std::string str_key;
  std::string str_value;
  FluentArgumentType type = FluentArgumentType::String;
};

/// FluentMessage(FluentMessage.cs L52-124);服务器只消费 Serialize
/// FluentMessage (L52-124); the server consumes only Serialize.
class FluentMessage {
 public:
  static constexpr int kProtocolVersion = 1;  // L54

  /// Serialize(L93-123):Protocol/Key/Arguments 节点树 →
  /// MiniYaml.ToLines("FluentMessage") → '\n' 连接(无尾换行);
  /// args 空(上游 null 判)则无 Arguments 节点
  /// Serialize (L93-123): the Protocol/Key/Arguments node tree →
  /// MiniYaml.ToLines("FluentMessage") → joined with '\n' (no trailing
  /// newline); null args produce no Arguments node.
  static std::string Serialize(std::string_view str_key,
                               std::span<const FluentArgument> vec_args);
};

}  // namespace ora::server
