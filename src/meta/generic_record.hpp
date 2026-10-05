// UPSTREAM: OpenRA.Game/FieldLoader.cs @b6fc03f L937-975(schema_dumper --gen
//          的运行时载体 —— C# 反射对象模型 FieldLoadInfo 的值形态等价)
//          The runtime carrier of schema_dumper --gen output — the
//          value-shaped equivalent of the C# reflection FieldLoadInfo model.
//
// 机制对照 / Mechanism mapping:
//  - 601 个可加载类型(Trait/Warhead/Projectile 普查)不逐一手写 C++ 类:
//    描述表 + 默认值表由 schema_dumper 从 C# 反射导出(gen/ 源),运行时以
//    GeneratedRecord(按 CollectFields 序对齐的值袋)承载 —— dump/lint/
//    resolved-rules 全走值袋;Phase 5 手写 Info 类(offset 路径)按需替换工厂
//    The 601 loadable types (the Trait/Warhead/Projectile survey) are not
//    hand-written one by one: descriptor + default tables are exported from C#
//    reflection by schema_dumper (gen/ sources) and carried at runtime by
//    GeneratedRecord (a value bag aligned to the CollectFields order) —
//    dump/lint/resolved-rules all go through the bag; Phase 5 hand-written Info
//    classes (the offset path) replace factories on demand.
//  - Opaque 字段(上游无 TypeParser 的对象字段,如 WeaponInfo 引用/Lazy<T>):
//    保留描述条目与值槽(恒 null),yaml 出现该键时走 DefaultUnknownField
//    —— 与上游 GetValue 找不到解析器时的报错行为逐字一致
//    Opaque fields (upstream object fields without a TypeParser, e.g.
//    WeaponInfo references / Lazy<T>): keep the descriptor entry and a value
//    slot (permanently null); a yaml key hitting them goes through
//    DefaultUnknownField — verbatim identical to upstream GetValue failing to
//    find a parser.
#pragma once
import std;

#include "meta/field_desc.hpp"
#include "meta/type_registry.hpp"
#include "yaml/mini_yaml.hpp"

namespace ora::meta {

/// 元组载荷:整型分量(WVec/WPos/WRot/CPos/CVec/int2/Size/Rectangle)与
/// 浮点分量(Vector2/Vector3)分列;desc.type 决定读取哪一列与分量数
/// Tuple payload: integer components (WVec/WPos/WRot/CPos/CVec/int2/Size/
/// Rectangle) and float components (Vector2/Vector3) in separate columns;
/// desc.type decides which column and how many components to read.
struct GenericTuple {
  std::array<std::int64_t, 4> arr_ints{};  // WDist.Length/WAngle.Angle/int 分量
  std::array<float, 4> arr_floats{};       // float 分量 / float components.
  std::uint8_t uint1_count{};              // 有效分量数(2/3/4)/ live components.
};

/// 字典载荷:插入序键值对(与 FieldType::Dictionary 的存储注释一致)
/// Dictionary payload: insertion-order pairs (matching the FieldType::
/// Dictionary storage note).
using GenericDict = std::vector<std::pair<struct GenericValue, struct GenericValue>>;

/// 通用值:生成类型的字段存储(dump 协议的判别依据 = FieldDesc.type)
/// Generic value: the field storage of generated types (the dump protocol
/// discriminates on FieldDesc.type).
struct GenericValue {
  /// 载荷形态 / payload shape.
  std::variant<std::monostate,  // C# null / Nullable 空(C# null / empty Nullable)
               bool,            // Bool
               std::int64_t,    // Byte/UInt16/Int16/Int32/Enum/WAngle/WDist/
                               // Color(argb)/BitSet(原始位)
               float,           // Float
               std::string,     // String/表达式规范文本(表达式文本)
               GenericTuple,    // 元组类型 / tuple types
               std::vector<GenericValue>,       // 数组/集合 / arrays/sets
               GenericDict,                      // 字典 / dictionaries
               std::shared_ptr<RecordObject>>   // 嵌套记录(LoadUsing 写入;
                                               // shared 使默认值表可拷贝克隆)
                                               // Nested records (written by
                                               // LoadUsing; shared keeps the
                                               // default tables cloneable).
               val{};

  bool IsNull() const { return std::holds_alternative<std::monostate>(val); }

  // ———— 构造辅助(schema_dumper gen/ 源与加载器用)/ factories ————
  static GenericValue Null() { return {}; }
  static GenericValue Of(bool b_v) {
    GenericValue v;
    v.val = b_v;
    return v;
  }
  static GenericValue Of(std::int64_t int8_v) {
    GenericValue v;
    v.val = int8_v;
    return v;
  }
  static GenericValue Of(float fp4_v) {
    GenericValue v;
    v.val = fp4_v;
    return v;
  }
  static GenericValue Of(std::string str_v) {
    GenericValue v;
    v.val = std::move(str_v);
    return v;
  }
  /// 字面量重载:阻止 const char* 退化为 bool 重载(gen/ 源大量
  /// Of("literal") 调用)
  /// Literal overload: stops const char* from decaying into the bool
  /// overload (gen/ sources call Of("literal") pervasively).
  static GenericValue Of(const char* sz_v) { return Of(std::string{sz_v}); }
  static GenericValue Of(GenericTuple t_v) {
    GenericValue v;
    v.val = std::move(t_v);
    return v;
  }
  static GenericValue Of(std::vector<GenericValue> vec_v) {
    GenericValue v;
    v.val = std::move(vec_v);
    return v;
  }
  static GenericValue Of(GenericDict map_v) {
    GenericValue v;
    v.val = std::move(map_v);
    return v;
  }
  static GenericValue Of(std::shared_ptr<RecordObject> rec_v) {
    GenericValue v;
    v.val = std::move(rec_v);
    return v;
  }
};

/// 生成记录:描述表 + 默认值表驱动的通用 RecordObject(工厂产物)
/// Generated record: a generic RecordObject driven by descriptor + default
/// tables (the factory product).
class GeneratedRecord : public RecordObject {
 public:
  /// defaults 与 CollectFields(desc) 展开序对齐(gen/ 源保证)
  /// defaults aligns with the CollectFields(desc) expansion order
  /// (guaranteed by the gen/ sources).
  GeneratedRecord(const RecordDesc& desc, std::span<const GenericValue> defaults);

  const RecordDesc& record_desc() const override { return *desc_; }

  /// 按展开字段序号取值槽(Load 主循环 / loader 写入)
  /// Value slot by expanded field index (the Load loop / loader writes).
  GenericValue& Slot(std::size_t int4_index) { return vec_values_[int4_index]; }
  const GenericValue& Slot(std::size_t int4_index) const { return vec_values_[int4_index]; }

  const std::vector<GenericValue>& Values() const { return vec_values_; }

 private:
  const RecordDesc* desc_;
  std::vector<GenericValue> vec_values_;
};

/// 唯一解析实现:desc + yaml 节点 → GenericValue(内存路径经 AssignToMemory
/// 二次转换;错误消息与上游 GetValue/ParseXxx 逐字一致)
/// The single parse implementation: desc + yaml node → GenericValue (the
/// memory path converts a second time via AssignToMemory; error messages are
/// verbatim from upstream GetValue/ParseXxx).
void LoadValueIntoValue(const FieldDesc& desc, GenericValue& val_out,
                        const yaml::MiniYaml& node);

/// GenericValue → typed 内存(desc.off_offset 指向的目标;手写 Info 类路径)
/// GenericValue → typed memory (the target pointed to by desc.off_offset; the
/// hand-written Info-class path).
void AssignToMemory(const FieldDesc& desc, const GenericValue& val, void* p_field);

/// typed 内存 → GenericValue(AssignToMemory 的逆;手写类 dump/序列化用)
/// typed memory → GenericValue (the inverse of AssignToMemory; for
/// hand-written class dumps/serialization).
GenericValue ReadFromMemory(const FieldDesc& desc, const void* p_field);

/// 生成类型的 LoadUsing 加载器注册表(区别于手写类的 void* 注册表:
/// 值袋路径直接收 GenericValue&,加载器无需转型)
/// The LoadUsing loader registry for generated types (distinct from the
/// void* registry for hand-written classes: the bag path receives
/// GenericValue& directly, no casting needed inside loaders).
void RegisterGeneratedLoader(
    std::string_view str_loader,
    std::function<void(GenericValue& val_slot, const yaml::MiniYaml& trait_yaml)> fn_load);

/// Load 主循环分派用:按名取生成 loader(未注册返回 nullptr)
/// For the Load-loop dispatch: fetch a generated loader by name (nullptr
/// when unregistered).
const std::function<void(GenericValue&, const yaml::MiniYaml&)>* FindGeneratedLoader(
    std::string_view str_loader);

/// 测试用:清空生成 loader 注册表(引擎路径不调用)
/// Tests only: clear the generated-loader registry.
void ClearGeneratedLoadersForTest();

}  // namespace ora::meta
