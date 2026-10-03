// UPSTREAM: OpenRA.Game/FieldLoader.cs @7d57605 L937-1032(FieldLoadInfo/SerializeAttribute 体系)
//          + OpenRA.Game/ObjectCreator.cs L78-137(类型按名实例化)
//          The FieldLoadInfo/SerializeAttribute system + string-named type
//          instantiation.
//
// C++26 反射缺失下的等价机制 / The C++26 equivalent in the absence of reflection:
//  - 上游用反射枚举类型字段(BuildTypeLoadInfo);此处每个 Info 类自带
//    constexpr 字段描述表(kMetaFields),由 tools/schema_dumper 从 C# 程序集
//    反射导出对照(tools/schema_dumper --gen),零漂移
//    Upstream enumerates type fields via reflection; here every Info class
//    carries a constexpr field-descriptor table (kMetaFields), cross-checked
//    against C# reflection output from tools/schema_dumper (--gen), zero drift.
//  - 上游 ObjectCreator 按字符串类名实例化;此处 TypeRegistry 以
//    std::string_view → 工厂(返回 std::unique_ptr<TraitRecordBase> 的
//    std::function)等价
//    Upstream ObjectCreator instantiates by class-name string; the TypeRegistry
//    maps std::string_view → factory (a std::function returning
//    std::unique_ptr<TraitRecordBase>) as the equivalent.
//  - [FieldLoader.LoadUsing] 的自定义加载器:描述表带 loader 名,语义在
//    FieldLoader 内分派(第一版覆盖通用模式,其余登记偏离)
//    [FieldLoader.LoadUsing] custom loaders: the descriptor carries the loader
//    name and dispatch happens inside FieldLoader (the first version covers the
//    common patterns; the rest are registered deviations).
#pragma once
import std;

namespace ora::meta {

class TraitRecordBase;  // 前置:全部可加载记录的共同基(见 src/sim/trait_info.hpp)
                        // Forward: the common base of all loadable records
                        // (see src/sim/trait_info.hpp).

/// 字段类型标签(FieldLoader.cs L71-122 TypeParsers/GenericType*Parsers 的键集,
/// 按 schema_dumper --scan 普查的 103 种具体类型归一化)
/// Field type tag (the key set of FieldLoader.cs L71-122
/// TypeParsers/GenericType*Parsers, normalized from the 103 concrete types
/// surveyed by schema_dumper --scan).
enum class FieldType : std::uint8_t {
  // ———— 标量 / scalars ————
  Bool,                 // bool(bool.TryParse)
  Byte,                 // byte
  UInt16,               // ushort
  Int16,                // short
  Int32,                // int
  Float,                // float(TryParseFloatOrPercentInvariant,"50%" → ×0.01)
  String,               // string
  Color,                // OpenRA.Primitives.Color
  WDist,                // WDist
  WVec,                 // WVec("x,y,z" 三元组)
  WPos,                 // WPos("x,y,z" 三元组)
  WAngle,               // WAngle(int)
  WRot,                 // WRot("roll,pitch,yaw")
  CPos,                 // CPos("x,y[,layer]")
  CVec,                 // CVec("x,y")
  Int2,                 // int2("x,y")
  Size,                 // Size("w,h")(widget 用,Phase 6)
  Vector2,              // Vector2("x,y" float)
  Vector3,              // Vector3("x,y[,z]" float)
  Rectangle,            // Rectangle("x,y,w,h")(widget 用)
  BooleanExpression,    // BooleanExpression(变量表达式)
  IntegerExpression,    // IntegerExpression(变量表达式)
  DateTime,             // DateTime("yyyy-MM-dd HH-mm-ss")
  Hotkey,               // Hotkey("KEY Modifiers")(settings)
  HotkeyReference,      // HotkeyReference(settings;Phase 5 随 HotkeyManager)
  Enum,                 // 枚举(按 EnumDesc 注册表解析;底层 int32)
  // ———— 容器(元素类型见 elem/key/value)/ containers ————
  Array,                // T[](ParseArray 逐元素)
  ImmutableArray,       // ImmutableArray<T>(WVec/CPos/CVec/int2 特化分组语义)
  List,                 // List<T>
  HashSet,              // HashSet<T>
  FrozenSet,            // FrozenSet<T>
  Dictionary,           // Dictionary<K,V>(节点表)
  FrozenDictionary,     // FrozenDictionary<K,V>(节点表)
  BitSet,               // BitSet<Tag>(字符串→位,按位标签注册表)
  Nullable,             // Nullable<T>(空值 = null)
  // ———— 嵌套记录 / nested records ————
  Record,               // 嵌套数据类(如 LocomotorInfo.TerrainInfo),按记录注册表
  // ———— 上游特殊对象字段 / upstream opaque object fields ————
  Opaque,               // 运行时对象字段(WeaponInfo/Locomotor/Lazy<T>/Action 等)或
                        // LoadUsing 自定义加载器字段;不参与通用解析,由加载器分派
                        // Runtime-object fields (WeaponInfo/Locomotor/Lazy<T>/Action)
                        // or LoadUsing custom-loader fields; outside the generic
                        // parse, dispatched by loader name.
};

/// 单字段描述(等价 FieldLoadInfo:FieldInfo + SerializeAttribute)
/// Single-field descriptor (equivalent to FieldLoadInfo: FieldInfo +
/// SerializeAttribute).
struct FieldDesc {
  std::string_view str_name;   // YamlName = C# 字段名(yaml 键,兼容面)
                               // YamlName = the C# field name (the yaml key, part
                               // of the compatibility surface).
  FieldType type;              // 解析分派 / parse dispatch
  bool b_required;             // [FieldLoader.Require]
  std::string_view str_loader; // [FieldLoader.LoadUsing] 加载器名("" = 无)
                               // LoadUsing loader name ("" = none).
  std::ptrdiff_t off_offset{}; // 字段在类内的偏移(offsetof,由 ORA_FIELD 宏填)
                               // In-class field offset (offsetof, filled by the
                               // ORA_FIELD macros).
  const FieldDesc* elem{};     // 单参容器的元素描述(Array/ImmutableArray/List/
                               // HashSet/FrozenSet/Nullable)或 Record 的目标描述
                               // Element descriptor for single-arg containers
                               // (Array/ImmutableArray/List/HashSet/FrozenSet/
                               // Nullable) or the target of Record.
  const FieldDesc* key{};      // 字典键描述(Dictionary/FrozenDictionary)
                               // Dictionary key descriptor.
  const FieldDesc* value{};    // 字典值描述(Dictionary/FrozenDictionary)
                               // Dictionary value descriptor.
  std::string_view str_type_name{};  // Enum:枚举全名;BitSet:标签名;Record:记录名;
                                     // Opaque:上游类型全名/loader 名(dump/分派用)
                                     // Enum: full enum name; BitSet: tag name;
                                     // Record: record name; Opaque: upstream type
                                     // name / loader name (for dumping/dispatch).
};

/// 嵌套元素描述(无偏移的纯类型标签;静态存储,指针可指向字面量)
/// Nested element descriptor (a pure type tag without offset; static storage,
/// the pointer may target a literal).
constexpr FieldDesc ElemOf(FieldType type, std::string_view str_type_name = {}) {
  return FieldDesc{.str_name = {}, .type = type, .b_required = false, .str_loader = {},
                   .off_offset = 0, .elem = nullptr, .key = nullptr, .value = nullptr,
                   .str_type_name = str_type_name};
}

/// 记录描述:一个可按名字实例化、可 FieldLoader 加载的数据类
/// Record descriptor: a data class instantiable by name and loadable by
/// FieldLoader.
struct RecordDesc {
  std::string_view str_name;    // 类名(如 "HealthInfo";注册键)
                                // Class name (e.g. "HealthInfo"; registry key).
  std::string_view str_full_name;  // C# 全名(如 "OpenRA.Mods.Common.Traits.
                                   // HealthInfo";嵌套同名类的精确寻址副键,
                                   // 生成器填充;可为空)
                                   // The C# full name (e.g. "OpenRA.Mods.
                                   // Common.Traits.HealthInfo"; the exact
                                   // addressing secondary key for nested
                                   // same-named classes, filled by the
                                   // generator; may be empty).
  std::string_view str_base;    // 直接基类名("" = 根)
                                // Direct base class name ("" = root).
  std::span<const FieldDesc> fields;  // 本类自有字段(不含继承;加载沿 base 链拼接,
                                      // 与 C# GetFields 序一致:最基类字段在前)
                                      // Own fields only (inherited ones excluded;
                                      // loading walks the base chain in C# GetFields
                                      // order: base-most fields first).
  std::span<const std::string_view> requires_types;    // Requires<XInfo>(构造序依赖)
  std::span<const std::string_view> not_before_types;  // NotBefore<XInfo>
  std::span<const std::string_view> interfaces;  // 实现的接口全名(含基类链;
                                                  // IRulesetLoaded 查询/后续接口
                                                  // ID 表的生成源)
                                                  // Implemented interface full
                                                  // names (base chain included;
                                                  // the IRulesetLoaded queries
                                                  // and the source of the later
                                                  // interface-ID tables).
};

/// 描述表字段声明宏:填偏移并保持名字/类型/required 与 C# 侧一一对应。
/// offsetof 是宏、不穿越 import std; 边界(cpp26.md),故用编译器内建。
/// Descriptor-field declaration macro: fills the offset while keeping
/// name/type/required one-to-one with the C# side. offsetof is a macro and
/// does not cross the import std; boundary (cpp26.md), so the compiler
/// builtin is used instead.
#define ORA_FIELD(Class, Member, Type, Required)                        \
  ::ora::meta::FieldDesc{                                               \
      .str_name = #Member, .type = ::ora::meta::FieldType::Type,        \
      .b_required = (Required), .str_loader = {},                       \
      .off_offset = __builtin_offsetof(Class, Member), .elem = nullptr, \
      .key = nullptr, .value = nullptr, .str_type_name = {}}

/// 带类型名的字段(Enum/BitSet/Record/Opaque)
/// Field with a type name (Enum/BitSet/Record/Opaque).
#define ORA_FIELD_NAMED(Class, Member, Type, Required, TypeName)         \
  ::ora::meta::FieldDesc{                                                \
      .str_name = #Member, .type = ::ora::meta::FieldType::Type,         \
      .b_required = (Required), .str_loader = {},                        \
      .off_offset = __builtin_offsetof(Class, Member), .elem = nullptr,  \
      .key = nullptr, .value = nullptr, .str_type_name = TypeName}

/// LoadUsing 自定义加载器字段(FieldLoader.cs L992-997;加载器语义按名字分派)
/// LoadUsing custom-loader field (FieldLoader.cs L992-997; loader semantics
/// dispatched by name).
#define ORA_FIELD_LOADER(Class, Member, Loader)                           \
  ::ora::meta::FieldDesc{                                                 \
      .str_name = #Member, .type = ::ora::meta::FieldType::Opaque,        \
      .b_required = false, .str_loader = #Loader,                         \
      .off_offset = __builtin_offsetof(Class, Member), .elem = nullptr,   \
      .key = nullptr, .value = nullptr, .str_type_name = #Loader}

}  // namespace ora::meta
