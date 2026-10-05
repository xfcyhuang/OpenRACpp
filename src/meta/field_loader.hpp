// UPSTREAM: OpenRA.Game/FieldLoader.cs @b6fc03f L27-1032(全文件逐语义重写)
//          Full-file statement-by-statement rewrite of upstream FieldLoader.cs.
//
// 机制对照 / Mechanism mapping:
//  - C# 反射(BuildTypeLoadInfo 收集 Public|NonPublic 字段 + SerializeAttribute)
//    → RecordDesc::fields(schema_dumper 从 C# 反射导出,零漂移;加载沿 base 链
//    拼接,顺序 = C# GetFields:最远基类字段在前)
//    C# reflection (BuildTypeLoadInfo collecting Public|NonPublic fields +
//    SerializeAttribute) → RecordDesc::fields (exported from C# reflection by
//    schema_dumper, zero drift; loading concatenates along the base chain in
//    C# GetFields order: base-most fields first).
//  - TypeParsers/GenericType*Parsers 的 FrozenDictionary 分派 → FieldType switch
//    The FrozenDictionary dispatch of TypeParsers/GenericType*Parsers → a
//    FieldType switch.
//  - InvalidValueAction/UnknownFieldAction 可替换委托 → 全局 std::function
//    (默认实现消息逐字;lint 侧替换行为随后续阶段接入)
//    The replaceable InvalidValueAction/UnknownFieldAction delegates → global
//    std::function objects (verbatim default messages; linter replacements
//    arrive with later phases).
//
// 已登记偏离(docs/COVERAGE.md)/ Registered deviations:
//  - string 字段 null 与 "" 合流(C# string 可 null;dump/FormatValue 层两者
//    均为 "",运行时 null 判断在 trait 移植时以 empty() 等价改写)
//    string null and "" conflate (C# strings are nullable; both dump/format
//    as "" — runtime null checks become empty() checks as traits are ported).
//  - TypeConverter 兜底路径未实现(trait/weapon 加载链字段类型已被
//    TypeParsers + 枚举全覆盖,兜底不可达)
//    The TypeConverter fallback is not implemented (the trait/weapon loading
//    surface is fully covered by TypeParsers + enums; the fallback is
//    unreachable there).
#pragma once
import std;
#include "field_desc.hpp"
#include "type_registry.hpp"
#include "core/int2.hpp"
#include "core/rectangle.hpp"
#include "core/vector_n.hpp"
#include "yaml/mini_yaml.hpp"

namespace ora::meta {

/// FieldLoader.MissingFieldsException(L31-50):缺失必填字段清单 + 可定制头部
/// FieldLoader.MissingFieldsException (L31-50): the missing-required list with
/// a customizable header.
class MissingFieldsException : public yaml::YamlException {
 public:
  /// missing:缺失字段名列表 / missing: the missing field names.
  /// str_header/str_header_single:多/单缺失时使用的头部(上游构造器语义)
  /// str_header/str_header_single: headers for multiple/single missing
  /// (upstream constructor semantics).
  MissingFieldsException(std::vector<std::string> vec_missing, std::string str_header = {},
                         std::string str_header_single = {})
      : yaml::YamlException(BuildMessage(vec_missing,
                                         vec_missing.size() > 1 ? str_header
                                                                : FirstNonEmpty(str_header_single,
                                                                                str_header))),
        vec_missing_{std::move(vec_missing)} {}

  const std::vector<std::string>& Missing() const { return vec_missing_; }

 private:
  static std::string FirstNonEmpty(std::string_view sv_a, std::string_view sv_b) {
    return std::string{sv_a.empty() ? sv_b : sv_a};
  }
  static std::string BuildMessage(const std::vector<std::string>& vec_missing,
                                  std::string_view sv_header) {
    // Message 属性(L36-42):(Header 为空 ? "" : Header + ": ") + Missing[0] + ", m"...
    // The Message property (L36-42): (Header empty ? "" : Header + ": ") +
    // Missing[0] + ", m"...
    std::string str_ret{sv_header.empty() ? std::string{} : std::string{sv_header} + ": "};
    if (!vec_missing.empty()) {
      str_ret += vec_missing.front();
      for (std::size_t int4_i{1}; int4_i < vec_missing.size(); int4_i++)
        str_ret += ", " + vec_missing[int4_i];
    }
    return str_ret;
  }

  std::vector<std::string> vec_missing_;
};

/// InvalidValueAction(L55-56):默认抛
/// "FieldLoader: Cannot parse `{value}` into field `{fieldName}` of type `{fieldType}`"
/// InvalidValueAction (L55-56): default throws
/// "FieldLoader: Cannot parse `{value}` into field `{fieldName}` of type `{fieldType}`".
[[noreturn]] void DefaultInvalidValue(std::string_view sv_value, std::string_view sv_field_type,
                                      std::string_view sv_field_name);

/// UnknownFieldAction(L58-59):默认抛
/// "FieldLoader: Missing field `{fieldName}`"(NotImplementedException 同文本)
/// UnknownFieldAction (L58-59): default throws "FieldLoader: Missing field
/// `{fieldName}`" (same text as the NotImplementedException).
[[noreturn]] void DefaultUnknownField(std::string_view sv_field_name);

/// FieldLoader.Load(self, my)(L754-796):按描述表加载一个记录对象
/// FieldLoader.Load (self, my) (L754-796): load one record object per its
/// descriptor table.
/// 缺 Required 字段抛 MissingFieldsException;值非法抛 YamlException(逐字消息)
/// Missing required fields raise MissingFieldsException; invalid values raise
/// YamlException (verbatim messages).
void Load(RecordObject* obj_record, const yaml::MiniYaml& yaml_my);

/// GetValue<T>(field, value)(L839-842)的标量便捷入口(测试/加载链外部使用)
/// The scalar convenience entries of GetValue<T>(field, value) (L839-842),
/// used by tests and external loading paths.
std::int32_t GetInt32Value(std::string_view str_field, std::string_view sv_value);
float GetFloatValue(std::string_view str_field, std::string_view sv_value);
bool GetBoolValue(std::string_view str_field, std::string_view sv_value);
std::vector<std::string> GetStringArrayValue(std::string_view str_field, std::string_view sv_value);

/// GetValue<Rectangle/Size/Vector2>(field, value):PngSheetLoader/ChromeProvider
/// 等外部加载路径使用(类型名进错误消息,与上游 GetValue 相同)。
/// GetValue<Rectangle/Size/Vector2>(field, value): consumed by external
/// loading paths such as PngSheetLoader/ChromeProvider (the type name enters
/// the error message, same as upstream's GetValue).
Rectangle GetRectangleValue(std::string_view str_field, std::string_view sv_value);
int2 GetSizeValue(std::string_view str_field, std::string_view sv_value);
core::Vector2 GetVector2Value(std::string_view str_field, std::string_view sv_value);

/// LoadUsing 加载器未实现时的占位(登记偏离用;消息与未知 loader 一致)
/// Placeholder for unimplemented LoadUsing loaders (for the deviation log;
/// message matches the unknown-loader case).
[[noreturn]] void UnknownLoader(std::string_view str_loader, std::string_view str_field);

/// 描述表访问辅助:沿 base 链收集全部字段(基类在前;等价 C# GetFields 序)
/// Descriptor-table helper: collect all fields along the base chain (base
/// classes first; equivalent to the C# GetFields order).
std::vector<const FieldDesc*> CollectFields(const RecordDesc& desc);

// ———— BitSet 标签注册表 / BitSet tag registry ————
// 每个具体 BitSet<Tag>(TargetableType 等)由 mods 层注册字符串↔位互转;
// 所有 BitSet<Tag> 布局相同(单 uint64),加载/序列化经注册表按位读写
// Every concrete BitSet<Tag> (TargetableType etc.) registers its string↔bit
// conversions from the mods layer; all BitSet<Tag> share one layout (a single
// uint64), so loading/serialization go through the registry by raw bits.
void RegisterBitSet(std::string_view str_tag,
                    std::function<std::uint64_t(std::span<const std::string>)> fn_get_bits,
                    std::function<std::vector<std::string>(std::uint64_t)> fn_get_strings);

/// gen/ 源用:注册一个按"字符串首遇序"分配位的运行时标签
/// (BitSet.cs BitSetAllocator<T> 语义的字符串键形态;位互转经该表)
/// For gen/ sources: register a runtime tag whose bits allocate in
/// first-appearance order (the string-keyed form of the BitSetAllocator<T>
/// semantics of BitSet.cs; bit↔string conversions go through the table).
void RegisterRuntimeBitSet(std::string_view str_tag);
/// 标签 → 位(ParseBitSet 的字符串列表路径;未注册标签 = 生成器错误,abort)
/// Tag → bits (the string-list path of ParseBitSet; an unregistered tag is a
/// generator error and aborts).
std::uint64_t BitsOf(std::string_view str_tag, std::span<const std::string> vec_values);
/// 位 → 分配序字符串(BitSet.ToString 的输出序)
/// Bits → allocation-order strings (the output order of BitSet.ToString).
std::vector<std::string> StringsOfBits(std::string_view str_tag, std::uint64_t uint8_bits);

// ———— LoadUsing 加载器注册表 / LoadUsing loader registry ————
// 加载器实现随各 Info 类的 C++ 源注册(FieldLoader.GetLoader 的委托创建等价);
// trait_yaml 为整个 Info 节点(与上游 fli.Loader(my) 一致),field_ptr 为字段内存
// Loader implementations register alongside each Info class source (the
// equivalent of FieldLoader.GetLoader's delegate creation); trait_yaml is the
// whole Info node (matching upstream fli.Loader(my)), field_ptr the field
// storage.
void RegisterLoader(
    std::string_view str_loader,
    std::function<void(void* field_ptr, const yaml::MiniYaml& trait_yaml)> fn_load);

}  // namespace ora::meta
