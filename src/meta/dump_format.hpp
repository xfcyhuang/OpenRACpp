// UPSTREAM: OpenRA.Game/FieldLoader.cs @b6fc03f L996-1018(dump 协议的值
//          格式化 —— FieldSaver.FormatValue 语义面;协议由 schema_dumper
//          --dump 与 tests/golden_rules 共享,与 C# 侧 Program.cs DumpValue
//          逐分支对齐)
//          The dump-protocol value formatter (the output spec shared by
//          schema_dumper --dump and tests/golden_rules; branch-aligned with
//          DumpValue of the C# Program.cs).
//
// 协议要点 / Protocol notes:
//  - float:复刻 .NET float.ToString(InvariantCulture) 的最短往返 + 定点/科学
//    阈值(科学当且仅当十进制指数 < -4 或 > 6;"E±dd" 大写两位指数)
//    float: replicates the shortest-round-trip + fixed/scientific threshold
//    of .NET float.ToString(InvariantCulture) (scientific iff the decimal
//    exponent < -4 or > 6; uppercase "E±dd" with two exponent digits).
//  - 字典/集合:按稳定序输出(键升序)——.NET Frozen* 枚举序是内部哈希序,
//    两侧统一排序以保证对拍确定性
//    Dictionaries/sets: stable output order (keys ascending) — the .NET
//    Frozen* enumeration order is an internal hash order; both sides sort
//    for comparison determinism.
//  - null 引用(字符串/表达式/可空)→ "(null)";空容器 → "(empty)";
//    嵌套记录 → "{类型名:{字段=值,…}}"
//    null references (strings/expressions/nullables) → "(null)"; empty
//    containers → "(empty)"; nested records → "{TypeName:{Field=value,…}}".
#pragma once
import std;

#include "meta/generic_record.hpp"

namespace ora::meta {

/// 单值格式化(判别依据 = desc.type;Opaque 槽按实际载荷格式化)
/// Format one value (discriminating on desc.type; Opaque slots format by
/// their actual payload).
std::string FormatValue(const FieldDesc& desc, const GenericValue& val);

/// .NET float.ToString(InvariantCulture) 等价(最短往返 + 阈值记法)
/// The .NET float.ToString(InvariantCulture) equivalent (shortest
/// round-trip + threshold notation).
std::string FormatFloatNet(float fp4_v);

}  // namespace ora::meta
