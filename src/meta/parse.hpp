// UPSTREAM: OpenRA.Game/Exts.cs @7d57605 L485-566(Parse*/TryParse*Invariant 家族)
//          + FieldLoader.cs L146-591(标量 ParseXxx 委托体)
//          The Parse*/TryParse*Invariant family + the scalar ParseXxx delegate
//          bodies of FieldLoader.
//
// BCL 语义复刻要点 / BCL semantics being replicated:
//  - NumberStyles.Integer:允许首尾 Unicode 空白 + 前导符号,十进制整串,溢出即失败
//    NumberStyles.Integer: leading/trailing Unicode whitespace + leading sign,
//    decimal, whole-string, overflow fails.
//  - NumberStyles.Float:上述 + 小数点/指数;C# 仅接受字面 "Infinity"/"-Infinity"/"NaN"
//    (区分大小写),from_chars 更宽(接受任意大小写 inf/nan)——结果非有限时按
//    C# 符号集回验收紧(偏离表已登记的无穷角落不可达,双保险)
//    NumberStyles.Float: the above + decimal point/exponent; C# only accepts
//    the literal "Infinity"/"-Infinity"/"NaN" (case-sensitive), while
//    from_chars is looser — non-finite results are re-validated against the C#
//    symbol set (belt-and-braces for an unreachable corner).
//  - float 结果与 C# 逐位一致:两侧均为最短往返正确舍入解析
//    float results are bit-identical to C#: both sides parse with
//    correctly-rounded shortest-round-trip semantics.
#pragma once
import std;

namespace ora::meta {

/// Exts.TryParseInt32Invariant(s, out i)(NumberStyles.Integer)
bool TryParseInt32Invariant(std::string_view sv, std::int32_t& int4_out);
/// Exts.TryParseUInt16Invariant(s, out i)
bool TryParseUInt16Invariant(std::string_view sv, std::uint16_t& uint2_out);
/// Exts.TryParseInt16Invariant(s, out i)
bool TryParseInt16Invariant(std::string_view sv, std::int16_t& int2_out);
/// Exts.TryParseByteInvariant(s, out i)
bool TryParseByteInvariant(std::string_view sv, std::uint8_t& uint1_out);
/// Exts.TryParseFloatOrPercentInvariant(s, out f)("50%" → 50×0.01f;全部 '%' 剥离)
bool TryParseFloatOrPercentInvariant(std::string_view sv, float& fp4_out);

/// bool.TryParse:Trim + 大小写不敏感 "true"/"false"
/// bool.TryParse: Trim + case-insensitive "true"/"false".
bool TryParseBoolNet(std::string_view sv, bool& b_out);

/// C# MemoryExtensions.Split(span, ',', RemoveEmptyEntries | TrimEntries):
/// 按逗号切分,段 Trim(Unicode 空白),空段丢弃(FieldLoader 元组解析的通用底座)
/// C# MemoryExtensions.Split(span, ',', RemoveEmptyEntries | TrimEntries):
/// comma-split with per-segment Unicode-whitespace Trim and empty-segment
/// removal (the common base of FieldLoader tuple parsing).
std::vector<std::string_view> SplitCommaTrimmed(std::string_view sv);

/// C# MemoryExtensions.Split(span, ',')(无选项):按逗号切分,保留空段(数组解析用)
/// C# MemoryExtensions.Split(span, ',') (no options): comma-split keeping empty
/// segments (for array parsing).
std::vector<std::string_view> SplitComma(std::string_view sv);

}  // namespace ora::meta

namespace ora {
struct CPos;  // core/cell_pos.hpp(实现侧包含)/ core/cell_pos.hpp
              // (included by the implementation side).
struct CVec;  // core/cvec.hpp
}  // namespace ora

namespace ora::meta {

/// 三元组公共骨架(WVec L235-249 / WPos L284-298 / WRot L307-321):
/// 恰 3 段且逐段可解析;fn_make 由调用方完成目标构造
/// Common 3-tuple skeleton (WVec L235-249 / WPos L284-298 / WRot L307-321):
/// exactly 3 segments, each parseable; fn_make assembles the target.
template <class ParseElem, class Make>
std::optional<std::invoke_result_t<Make, std::int32_t, std::int32_t, std::int32_t>>
TryParseTuple3(std::string_view sv, const ParseElem& fn_parse, const Make& fn_make) {
  if (sv.empty())
    return std::nullopt;
  const std::vector<std::string_view> vec_parts = SplitCommaTrimmed(sv);
  if (vec_parts.size() != 3)
    return std::nullopt;
  std::array<std::int32_t, 3> arr_e{};
  for (std::size_t int4_i{}; int4_i < 3; int4_i++)
    if (!fn_parse(vec_parts[int4_i], arr_e[int4_i]))
      return std::nullopt;
  return fn_make(arr_e[0], arr_e[1], arr_e[2]);
}

/// 二元组公共骨架(CVec L377-390 / Int2 L516-529 / Size L501-514 / Vector2 L531-544)
/// Common 2-tuple skeleton (CVec L377-390 / Int2 L516-529 / Size L501-514 /
/// Vector2 L531-544).目标类型需有 .X/.Y 公有成员
/// The target type needs public .X/.Y members.
template <class T, class ParseElem>
bool TryParseTuple2(std::string_view sv, const ParseElem& fn_parse, T& out_t) {
  if (sv.empty())
    return false;
  const std::vector<std::string_view> vec_parts = SplitCommaTrimmed(sv);
  if (vec_parts.size() != 2)
    return false;
  if (!fn_parse(vec_parts[0], out_t.X) || !fn_parse(vec_parts[1], out_t.Y))
    return false;
  return true;
}

/// ParseCPos(L323-342):3 段(x,y,layer-byte)或 2 段(x,y)
/// ParseCPos (L323-342): 3 segments (x,y,layer-byte) or 2 (x,y).
bool TryParseCPosNet(std::string_view sv, CPos& cpos_out);

/// ParseCVec(L377-390)
bool TryParseCVecNet(std::string_view sv, CVec& cvec_out);

/// Enum.TryParse(fieldType, value, true, out) 的 C++ 等价:
/// 按枚举注册表解析(名字不分大小写 / 有符号十进制数值 / 逗号分隔名字按位或)
/// The C++ equivalent of Enum.TryParse(fieldType, value, true, out):
/// resolves through the enum registry (case-insensitive names / signed decimal
/// numerics / comma-separated names ORed together).
/// enum_full_name:C# 枚举全名(如 "OpenRA.Traits.PlayerRelationship")
/// enum_full_name: the C# full enum name (e.g. "OpenRA.Traits.PlayerRelationship").
bool TryParseEnumNet(std::string_view enum_full_name, std::string_view sv, std::int32_t& int4_out);

/// Enum.ToString():flags 语义(升序单值名,剩余位十进制数值,"" 空枚举值)
/// Enum.ToString(): flags semantics (ascending single-bit names, leftover bits
/// as a decimal number, "" for an unnamed zero).
std::string EnumToStringNet(std::string_view enum_full_name, std::int32_t int4_value);

/// 枚举注册表装载(gen/enums 定义;schema_dumper 导出)
/// Enum registry loading (gen/enums definitions exported by schema_dumper).
struct EnumMemberDesc {
  std::string_view str_name;  // 成员名(区分大小写输出用)
  std::int32_t int4_value;    // 成员值
};
void RegisterEnum(std::string_view enum_full_name, std::span<const EnumMemberDesc> members);
/// 测试/工具用:清空注册表(引擎路径不调用)
/// Tests/tools only: clear the registry (engine paths never call this).
void ClearEnumsForTest();

}  // namespace ora::meta
