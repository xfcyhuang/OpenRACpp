// UPSTREAM: OpenRA.Game/FieldLoader.cs @7d57605 L27-1032(field_loader.hpp 的实现)
//          Implementation of field_loader.hpp.
// 分派结构 / Dispatch structure:
//  - Load(L754-796):描述表字段循环 + MissingFieldsException
//  - LoadValueInto ≈ GetValue(L854-892)与各 ParseXxx(L146-752)的合体,
//    按 FieldType switch;失败统一走 DefaultInvalidValue(消息逐字)
//  - 元组(WVec/WPos/WRot/CPos/CVec/Int2/Size/Vector2/Vector3/Rectangle):
//    SplitCommaTrimmed(= Split 的 RemoveEmptyEntries|TrimEntries 语义)
//  - 数组:SplitComma 逐段 Trim、空段跳过;ImmutableArray<WVec/CPos/CVec/Int2>
//    走上游分组特化(L251-282/344-375/392-423/468-499),其余逐元素
//  - 字典:节点迭代,键/值各自解析;重复键按 Dictionary.Add 语义抛
//    (偏离登记:异常类型 YamlException,C# 为反射包装异常;消息文本一致)
//  - LoadUsing:名字注册表(mods 层各 Info 源注册实现;meta 层不依赖 mods 符号)
import std;
#include "field_loader.hpp"
#include "parse.hpp"
#include "variable_expression.hpp"
#include "core/bitset.hpp"
#include "core/cell_pos.hpp"
#include "core/color.hpp"
#include "core/cvec.hpp"
#include "core/int2.hpp"
#include "core/text.hpp"
#include "core/vector_n.hpp"
#include "core/wangle.hpp"
#include "core/wdist.hpp"
#include "core/wpos.hpp"
#include "core/wrot.hpp"
#include "core/wvec.hpp"

namespace ora::meta {

namespace {

/// 字段内存指针 / Pointer to the field storage.
void* FieldPtr(RecordObject* obj_record, const FieldDesc& desc) {
  return static_cast<void*>(reinterpret_cast<char*>(obj_record) + desc.off_offset);
}

/// 标量 FieldType 的 C# Type.ToString() 缺省名(ORA_FIELD 宏不带类型名时兜底;
/// 生成器输出的描述通常已带全名)
/// The C# Type.ToString() default names for scalar FieldTypes (the fallback
/// when the ORA_FIELD macro omits the type name; generator output usually
/// carries the full name).
std::string_view DefaultTypeName(FieldType type) {
  switch (type) {
    case FieldType::Bool: return "System.Boolean";
    case FieldType::Byte: return "System.Byte";
    case FieldType::UInt16: return "System.UInt16";
    case FieldType::Int16: return "System.Int16";
    case FieldType::Int32: return "System.Int32";
    case FieldType::Float: return "System.Single";
    case FieldType::String: return "System.String";
    case FieldType::Color: return "OpenRA.Primitives.Color";
    case FieldType::WDist: return "OpenRA.WDist";
    case FieldType::WVec: return "OpenRA.WVec";
    case FieldType::WPos: return "OpenRA.WPos";
    case FieldType::WAngle: return "OpenRA.WAngle";
    case FieldType::WRot: return "OpenRA.WRot";
    case FieldType::CPos: return "OpenRA.CPos";
    case FieldType::CVec: return "OpenRA.CVec";
    case FieldType::Int2: return "OpenRA.int2";
    case FieldType::Size: return "System.Drawing.Size";
    case FieldType::Vector2: return "System.Numerics.Vector2";
    case FieldType::Vector3: return "System.Numerics.Vector3";
    case FieldType::Rectangle: return "OpenRA.Primitives.Rectangle";
    case FieldType::BooleanExpression: return "OpenRA.Support.BooleanExpression";
    case FieldType::IntegerExpression: return "OpenRA.Support.IntegerExpression";
    case FieldType::DateTime: return "System.DateTime";
    case FieldType::Hotkey: return "OpenRA.Hotkey";
    case FieldType::HotkeyReference: return "OpenRA.HotkeyReference";
    default: return {};
  }
}

/// InvalidValueAction 的本字段包装(L55-56;fieldType 取 C# Type.ToString(),
/// 由 schema_dumper 存入 str_type_name,空则按 FieldType 兜底)
/// Per-field wrapper of InvalidValueAction (L55-56; fieldType is the C#
/// Type.ToString() string stored in str_type_name by schema_dumper, with a
/// FieldType-based fallback when empty).
[[noreturn]] void InvalidValueFor(const FieldDesc& desc, std::string_view sv_value) {
  const std::string_view sv_type =
      desc.str_type_name.empty() ? DefaultTypeName(desc.type) : desc.str_type_name;
  DefaultInvalidValue(sv_value, sv_type, desc.str_name);
}

/// MiniYaml.Value 的 Trim 视图(GetValue 开头的 value.Trim(),L856)
/// The Trim view of MiniYaml.Value (the value.Trim() at the head of GetValue,
/// L856).
std::string_view TrimmedValue(const yaml::MiniYaml& node) {
  return TrimNetWhiteSpace(node.Value != nullptr ? std::string_view{*node.Value}
                                                 : std::string_view{});
}

// ———— 元组解析(L235-583)/ Tuple parsing ————

/// 三元组公共骨架(WVec L235-249 / WPos L284-298 / WRot L307-321):
/// 恰 3 段且逐段可解析;out_make 由调用方完成目标构造
/// Common 3-tuple skeleton (WVec L235-249 / WPos L284-298 / WRot L307-321):
/// exactly 3 segments, each parseable; the caller assembles the target.
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
/// Vector2 L531-544).
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
bool TryParseCPosNet(std::string_view sv, CPos& cpos_out) {
  if (sv.empty())
    return false;
  const std::vector<std::string_view> vec_parts = SplitCommaTrimmed(sv);
  std::int32_t int4_x{}, int4_y{};
  if (vec_parts.size() == 3) {
    std::uint8_t uint1_layer{};
    if (TryParseInt32Invariant(vec_parts[0], int4_x) &&
        TryParseInt32Invariant(vec_parts[1], int4_y) &&
        TryParseByteInvariant(vec_parts[2], uint1_layer)) {
      cpos_out = CPos{int4_x, int4_y, uint1_layer};
      return true;
    }
    return false;
  }
  if (vec_parts.size() == 2 && TryParseInt32Invariant(vec_parts[0], int4_x) &&
      TryParseInt32Invariant(vec_parts[1], int4_y)) {
    cpos_out = CPos{int4_x, int4_y};
    return true;
  }
  return false;
}

/// ParseCVec(L377-390)
bool TryParseCVecNet(std::string_view sv, CVec& cvec_out) {
  const auto fn_int = [](std::string_view sv_p, std::int32_t& int4_v) {
    return TryParseInt32Invariant(sv_p, int4_v);
  };
  return TryParseTuple2(sv, fn_int, cvec_out);
}

// ———— 容器元素解析 / Container element parsing ————

/// 元素载荷(覆盖全部标量元素形态;目标容器元素类型由 CastElem 转换)
/// Element payload (covering every scalar element shape; CastElem converts to
/// the target container element type).
struct ElemValue {
  std::int64_t int8_i{};    // Byte/UInt16/Int16/Int32/Enum + WAngle/WDist 底层数值
  float fp4_f{};            // Float
  std::string str_s{};      // String
  bool b_b{};               // Bool
  core::Color color_c{};    // Color
  std::uint64_t uint8_bits{};  // BitSet 原始位 / raw BitSet bits
};

/// 按元素 desc 解析一段文本(失败抛 InvalidValueFor,值传整段——与上游
/// ParseXxxArray 的 value.Span 传播一致)
/// Parse one segment per the element desc (failures raise InvalidValueFor with
/// the segment — matching upstream value.Span propagation in ParseXxxArray).
ElemValue ParseElemValue(const FieldDesc& desc_field, const FieldDesc& desc_elem,
                         std::string_view sv_elem) {
  ElemValue val_ret;
  switch (desc_elem.type) {
    case FieldType::Bool:
      if (!TryParseBoolNet(sv_elem, val_ret.b_b))
        InvalidValueFor(desc_field, sv_elem);
      return val_ret;
    case FieldType::Byte: {
      std::uint8_t uint1_v{};
      if (!TryParseByteInvariant(sv_elem, uint1_v))
        InvalidValueFor(desc_field, sv_elem);
      val_ret.int8_i = uint1_v;
      return val_ret;
    }
    case FieldType::UInt16: {
      std::uint16_t uint2_v{};
      if (!TryParseUInt16Invariant(sv_elem, uint2_v))
        InvalidValueFor(desc_field, sv_elem);
      val_ret.int8_i = uint2_v;
      return val_ret;
    }
    case FieldType::Int16: {
      std::int16_t int2_v{};
      if (!TryParseInt16Invariant(sv_elem, int2_v))
        InvalidValueFor(desc_field, sv_elem);
      val_ret.int8_i = int2_v;
      return val_ret;
    }
    case FieldType::Int32:
    case FieldType::Enum: {
      std::int32_t int4_v{};
      if (desc_elem.type == FieldType::Int32 ?
              !TryParseInt32Invariant(sv_elem, int4_v) :
              !TryParseEnumNet(desc_elem.str_type_name, sv_elem, int4_v))
        InvalidValueFor(desc_field, sv_elem);
      val_ret.int8_i = int4_v;
      return val_ret;
    }
    case FieldType::Float:
      if (!TryParseFloatOrPercentInvariant(sv_elem, val_ret.fp4_f))
        InvalidValueFor(desc_field, sv_elem);
      return val_ret;
    case FieldType::String:
      val_ret.str_s = std::string{sv_elem};
      return val_ret;
    case FieldType::Color:
      if (!core::Color::TryParse(sv_elem, val_ret.color_c))
        InvalidValueFor(desc_field, sv_elem);
      return val_ret;
    case FieldType::WAngle: {
      std::int32_t int4_v{};
      if (!TryParseInt32Invariant(sv_elem, int4_v))
        InvalidValueFor(desc_field, sv_elem);
      val_ret.int8_i = int4_v;
      return val_ret;
    }
    case FieldType::WDist: {
      // WDist.TryParse(WDist.cs L62-94):"NcM"/"N" 定点形式
      // WDist.TryParse (WDist.cs L62-94): the "NcM"/"N" fixed-point forms.
      const std::expected<WDist, bool> exp_d = WDist::TryParse(sv_elem);
      if (!exp_d.has_value())
        InvalidValueFor(desc_field, sv_elem);
      val_ret.int8_i = exp_d->Length;
      return val_ret;
    }
    case FieldType::BitSet: {
      std::vector<std::string> vec_strs;
      for (const std::string_view sv_w : SplitCommaTrimmed(sv_elem))
        vec_strs.emplace_back(sv_w);
      val_ret.uint8_bits = BitsOf(desc_elem.str_type_name, vec_strs);
      return val_ret;
    }
    default:
      // 非标量元素不在通用数组路径(上游 ParseArray 同构限制);生成器保证
      // Non-scalar elements never take the generic array path (the same-shaped
      // upstream ParseArray restriction); guaranteed by the generator.
      DefaultUnknownField(desc_field.str_name);
  }
}

/// 载荷 → 目标元素类型 / Payload → target element type.
template <class E>
E CastElem(const ElemValue& v) {
  if constexpr (std::same_as<E, bool>) {
    return v.b_b;
  } else if constexpr (std::same_as<E, std::string>) {
    return v.str_s;
  } else if constexpr (std::same_as<E, float>) {
    return v.fp4_f;
  } else if constexpr (std::same_as<E, core::Color>) {
    return v.color_c;
  } else if constexpr (std::same_as<E, WAngle>) {
    return WAngle{static_cast<std::int32_t>(v.int8_i)};
  } else if constexpr (std::same_as<E, WDist>) {
    return WDist{static_cast<std::int32_t>(v.int8_i)};
  } else if constexpr (std::same_as<E, std::uint64_t>) {
    return v.uint8_bits;
  } else {
    return static_cast<E>(v.int8_i);  // 整型族/枚举(底层 int 存储)
  }
}

/// ParseHashSetOrList/ParseArray/泛型 ImmutableArray(L593-639,660-684):
/// Split(Comma) → 逐段 Trim、空段跳过 → 元素解析;空串 = 空集合
/// ParseHashSetOrList/ParseArray/generic ImmutableArray (L593-639, 660-684):
/// Split(Comma) → per-segment Trim, empties skipped → element parse; an empty
/// string yields an empty collection.
template <class E>
std::vector<E> ParseVectorOf(const FieldDesc& desc_field, const FieldDesc& desc_elem,
                             std::string_view sv_value) {
  std::vector<E> vec_ret;
  if (sv_value.empty())
    return vec_ret;
  for (const std::string_view sv_part : SplitComma(sv_value)) {
    const std::string_view sv_trimmed = TrimNetWhiteSpace(sv_part);
    if (sv_trimmed.empty())
      continue;
    vec_ret.push_back(CastElem<E>(ParseElemValue(desc_field, desc_elem, sv_trimmed)));
  }
  return vec_ret;
}

/// 分组特化(WVec×3 L251-282 / CPos、CVec、Int2 ×2):固定宽度成组,尾部残组
/// 抛 InvalidValue(值传整串);空串也抛(与上游一致)
/// Grouped specializations (WVec×3 L251-282 / CPos, CVec, Int2 ×2): fixed-width
/// grouping; a trailing partial group raises InvalidValue with the whole
/// string; an empty string also raises (matching upstream).
template <class G, std::size_t kWidth>
std::vector<G> ParseGroupedVectorOf(const FieldDesc& desc_field, FieldType type_elem,
                                    std::string_view sv_value) {
  // 元素解析所需的 desc(元素段仅用于解析;失败消息走 desc_field,与上游一致)
  // A scratch element desc for parsing (failure messages go through
  // desc_field, matching upstream).
  const FieldDesc desc_elem = ElemOf(type_elem);
  std::vector<G> vec_ret;
  if (sv_value.empty())
    InvalidValueFor(desc_field, sv_value);

  std::array<ElemValue, kWidth> arr_elems{};
  std::size_t int4_index{};
  for (const std::string_view sv_part : SplitComma(sv_value)) {
    const std::string_view sv_trimmed = TrimNetWhiteSpace(sv_part);
    if (sv_trimmed.empty())
      continue;
    arr_elems[int4_index++] = ParseElemValue(desc_field, desc_elem, sv_trimmed);
    if (int4_index == kWidth) {
      if constexpr (kWidth == 3) {
        vec_ret.push_back(G{WDist{static_cast<std::int32_t>(arr_elems[0].int8_i)},
                            WDist{static_cast<std::int32_t>(arr_elems[1].int8_i)},
                            WDist{static_cast<std::int32_t>(arr_elems[2].int8_i)}});
      } else if constexpr (std::same_as<G, CPos>) {
        vec_ret.push_back(CPos{static_cast<std::int32_t>(arr_elems[0].int8_i),
                               static_cast<std::int32_t>(arr_elems[1].int8_i)});
      } else if constexpr (std::same_as<G, CVec>) {
        vec_ret.push_back(CVec{static_cast<std::int32_t>(arr_elems[0].int8_i),
                               static_cast<std::int32_t>(arr_elems[1].int8_i)});
      } else {
        vec_ret.push_back(int2{static_cast<std::int32_t>(arr_elems[0].int8_i),
                               static_cast<std::int32_t>(arr_elems[1].int8_i)});
      }
      int4_index = 0;
    }
  }
  if (int4_index != 0)
    InvalidValueFor(desc_field, sv_value);
  return vec_ret;
}

}  // namespace

// ———— BitSet 标签注册表 / BitSet tag registry ————

namespace {
struct BitSetOps {
  std::function<std::uint64_t(std::span<const std::string>)> fn_get_bits;
  std::function<std::vector<std::string>(std::uint64_t)> fn_get_strings;
};

std::unordered_map<std::string_view, BitSetOps>& BitSetTable() {
  static std::unordered_map<std::string_view, BitSetOps> map_table;
  return map_table;
}
}  // namespace

void RegisterBitSet(std::string_view str_tag,
                    std::function<std::uint64_t(std::span<const std::string>)> fn_get_bits,
                    std::function<std::vector<std::string>(std::uint64_t)> fn_get_strings) {
  BitSetTable().insert_or_assign(str_tag,
                                 BitSetOps{.fn_get_bits = std::move(fn_get_bits),
                                           .fn_get_strings = std::move(fn_get_strings)});
}

std::uint64_t BitsOf(std::string_view str_tag, std::span<const std::string> vec_values) {
  const auto it_find = BitSetTable().find(str_tag);
  if (it_find == BitSetTable().end())
    std::abort();  // 生成器保证注册 / the generator guarantees registration
  return it_find->second.fn_get_bits(vec_values);
}

std::vector<std::string> StringsOfBits(std::string_view str_tag, std::uint64_t uint8_bits) {
  const auto it_find = BitSetTable().find(str_tag);
  if (it_find == BitSetTable().end())
    std::abort();
  return it_find->second.fn_get_strings(uint8_bits);
}

// ———— LoadUsing 注册表 / LoadUsing registry ————

namespace {
std::unordered_map<std::string_view,
                   std::function<void(void* field_ptr, const yaml::MiniYaml& trait_yaml)>>&
LoaderTable() {
  static std::unordered_map<
      std::string_view,
      std::function<void(void* field_ptr, const yaml::MiniYaml& trait_yaml)>>
      map_table;
  return map_table;
}
}  // namespace

void RegisterLoader(
    std::string_view str_loader,
    std::function<void(void* field_ptr, const yaml::MiniYaml& trait_yaml)> fn_load) {
  LoaderTable().insert_or_assign(str_loader, std::move(fn_load));
}

[[noreturn]] void UnknownLoader(std::string_view str_loader, std::string_view str_field) {
  throw yaml::YamlException(std::format(
      "FieldLoader: LoadUsing loader `{}` for field `{}` is not registered", str_loader,
      str_field));
}

// ———— 缺省动作 / Default actions ————

[[noreturn]] void DefaultInvalidValue(std::string_view sv_value, std::string_view sv_field_type,
                                      std::string_view sv_field_name) {
  throw yaml::YamlException(std::format("FieldLoader: Cannot parse `{}` into field `{}` of type `{}`",
                                        sv_value, sv_field_name, sv_field_type));
}

[[noreturn]] void DefaultUnknownField(std::string_view sv_field_name) {
  throw yaml::YamlException(std::format("FieldLoader: Missing field `{}`", sv_field_name));
}

// ———— 描述表访问 / Descriptor access ————

std::vector<const FieldDesc*> CollectFields(const RecordDesc& desc) {
  // 沿 base 链收至根再倒序展开(最远基类字段在前 = C# GetFields 序)
  // Walk the base chain to the root then unwind (base-most fields first = the
  // C# GetFields order).
  std::vector<const RecordDesc*> vec_chain;
  const RecordDesc* desc_cur = &desc;
  while (desc_cur != nullptr) {
    vec_chain.push_back(desc_cur);
    desc_cur =
        desc_cur->str_base.empty() ? nullptr : TypeRegistry::FindType(desc_cur->str_base);
  }
  std::vector<const FieldDesc*> vec_fields;
  for (auto it_desc = vec_chain.rbegin(); it_desc != vec_chain.rend(); ++it_desc)
    for (const FieldDesc& desc_f : (*it_desc)->fields)
      vec_fields.push_back(&desc_f);
  return vec_fields;
}

// ———— 值写入分派 / Value-write dispatch ————

namespace {

/// LoadValueInto:desc 所指字段(目标内存 p_field)从节点加载(GetValue+ParseXxx)
/// LoadValueInto: load the field pointed to by desc (target memory p_field)
/// from the node (GetValue + ParseXxx).
void LoadValueInto(const FieldDesc& desc, void* p_field, const yaml::MiniYaml& node) {
  const std::string_view sv_value = TrimmedValue(node);

  switch (desc.type) {
    // ———— 标量 / scalars ————
    case FieldType::Bool: {
      bool b_v{};
      if (!TryParseBoolNet(sv_value, b_v))
        InvalidValueFor(desc, sv_value);
      *reinterpret_cast<bool*>(p_field) = b_v;
      return;
    }
    case FieldType::Byte: {
      std::uint8_t uint1_v{};
      if (!TryParseByteInvariant(sv_value, uint1_v))
        InvalidValueFor(desc, sv_value);
      *reinterpret_cast<std::uint8_t*>(p_field) = uint1_v;
      return;
    }
    case FieldType::UInt16: {
      std::uint16_t uint2_v{};
      if (!TryParseUInt16Invariant(sv_value, uint2_v))
        InvalidValueFor(desc, sv_value);
      *reinterpret_cast<std::uint16_t*>(p_field) = uint2_v;
      return;
    }
    case FieldType::Int16: {
      std::int16_t int2_v{};
      if (!TryParseInt16Invariant(sv_value, int2_v))
        InvalidValueFor(desc, sv_value);
      *reinterpret_cast<std::int16_t*>(p_field) = int2_v;
      return;
    }
    case FieldType::Int32: {
      std::int32_t int4_v{};
      if (!TryParseInt32Invariant(sv_value, int4_v))
        InvalidValueFor(desc, sv_value);
      *reinterpret_cast<std::int32_t*>(p_field) = int4_v;
      return;
    }
    case FieldType::Enum: {
      std::int32_t int4_v{};
      if (!TryParseEnumNet(desc.str_type_name, sv_value, int4_v))
        InvalidValueFor(desc, sv_value);
      *reinterpret_cast<std::int32_t*>(p_field) = int4_v;
      return;
    }
    case FieldType::Float: {
      float fp4_v{};
      if (!TryParseFloatOrPercentInvariant(sv_value, fp4_v))
        InvalidValueFor(desc, sv_value);
      *reinterpret_cast<float*>(p_field) = fp4_v;
      return;
    }
    case FieldType::String:
      // 偏离登记:null 与 "" 合流(ParseString 的 value.ToString())
      // Registered deviation: null and "" conflate (ParseString's
      // value.ToString()).
      *reinterpret_cast<std::string*>(p_field) = std::string{sv_value};
      return;
    case FieldType::Color: {
      core::Color color_v{};
      if (!core::Color::TryParse(sv_value, color_v))
        InvalidValueFor(desc, sv_value);
      *reinterpret_cast<core::Color*>(p_field) = color_v;
      return;
    }
    case FieldType::WAngle: {
      std::int32_t int4_v{};
      if (!TryParseInt32Invariant(sv_value, int4_v))
        InvalidValueFor(desc, sv_value);
      *reinterpret_cast<WAngle*>(p_field) = WAngle{int4_v};
      return;
    }
    case FieldType::WDist: {
      const std::expected<WDist, bool> exp_d = WDist::TryParse(sv_value);
      if (!exp_d.has_value())
        InvalidValueFor(desc, sv_value);
      *reinterpret_cast<WDist*>(p_field) = *exp_d;
      return;
    }
    case FieldType::WVec: {
      const auto fn_wdist = [](std::string_view sv_p, std::int32_t& int4_len) {
        const std::expected<WDist, bool> exp_d = WDist::TryParse(sv_p);
        if (!exp_d.has_value())
          return false;
        int4_len = exp_d->Length;
        return true;
      };
      const auto fn_make_vec = [](std::int32_t int4_x, std::int32_t int4_y, std::int32_t int4_z) {
        return WVec{int4_x, int4_y, int4_z};
      };
      const auto opt_v = TryParseTuple3(sv_value, fn_wdist, fn_make_vec);
      if (!opt_v.has_value())
        InvalidValueFor(desc, sv_value);
      *reinterpret_cast<WVec*>(p_field) = *opt_v;
      return;
    }
    case FieldType::WPos: {
      const auto fn_wdist = [](std::string_view sv_p, std::int32_t& int4_len) {
        const std::expected<WDist, bool> exp_d = WDist::TryParse(sv_p);
        if (!exp_d.has_value())
          return false;
        int4_len = exp_d->Length;
        return true;
      };
      const auto fn_make_pos = [](std::int32_t int4_x, std::int32_t int4_y, std::int32_t int4_z) {
        return WPos{int4_x, int4_y, int4_z};
      };
      const auto opt_v = TryParseTuple3(sv_value, fn_wdist, fn_make_pos);
      if (!opt_v.has_value())
        InvalidValueFor(desc, sv_value);
      *reinterpret_cast<WPos*>(p_field) = *opt_v;
      return;
    }
    case FieldType::WRot: {
      const auto fn_int = [](std::string_view sv_p, std::int32_t& int4_v) {
        return TryParseInt32Invariant(sv_p, int4_v);
      };
      const auto fn_make_rot = [](std::int32_t int4_r, std::int32_t int4_p, std::int32_t int4_y) {
        return WRot{WAngle{int4_r}, WAngle{int4_p}, WAngle{int4_y}};
      };
      const auto opt_v = TryParseTuple3(sv_value, fn_int, fn_make_rot);
      if (!opt_v.has_value())
        InvalidValueFor(desc, sv_value);
      *reinterpret_cast<WRot*>(p_field) = *opt_v;
      return;
    }
    case FieldType::CPos: {
      CPos cpos_v{};
      if (!TryParseCPosNet(sv_value, cpos_v))
        InvalidValueFor(desc, sv_value);
      *reinterpret_cast<CPos*>(p_field) = cpos_v;
      return;
    }
    case FieldType::CVec: {
      CVec cvec_v{};
      if (!TryParseCVecNet(sv_value, cvec_v))
        InvalidValueFor(desc, sv_value);
      *reinterpret_cast<CVec*>(p_field) = cvec_v;
      return;
    }
    case FieldType::Int2: {
      int2 int2_v{};
      const auto fn_int = [](std::string_view sv_p, std::int32_t& int4_v) {
        return TryParseInt32Invariant(sv_p, int4_v);
      };
      if (!TryParseTuple2(sv_value, fn_int, int2_v))
        InvalidValueFor(desc, sv_value);
      *reinterpret_cast<int2*>(p_field) = int2_v;
      return;
    }
    case FieldType::Vector2: {
      core::Vector2 vec2_v{};
      const auto fn_float = [](std::string_view sv_p, float& fp4_v) {
        return TryParseFloatOrPercentInvariant(sv_p, fp4_v);
      };
      if (!TryParseTuple2(sv_value, fn_float, vec2_v))
        InvalidValueFor(desc, sv_value);
      *reinterpret_cast<core::Vector2*>(p_field) = vec2_v;
      return;
    }
    case FieldType::Vector3: {
      // ParseVector3(L546-566):3 段;2 段时 z=0(旧 Vector2 兼容)
      // ParseVector3 (L546-566): 3 segments; with 2 segments z=0 (legacy
      // Vector2 compatibility).
      core::Vector3 vec3_v{};
      if (!sv_value.empty()) {
        const std::vector<std::string_view> vec_parts = SplitCommaTrimmed(sv_value);
        float fp4_x{}, fp4_y{}, fp4_z{};
        if (vec_parts.size() == 3 &&
            TryParseFloatOrPercentInvariant(vec_parts[0], fp4_x) &&
            TryParseFloatOrPercentInvariant(vec_parts[1], fp4_y) &&
            TryParseFloatOrPercentInvariant(vec_parts[2], fp4_z)) {
          vec3_v = core::Vector3{fp4_x, fp4_y, fp4_z};
          *reinterpret_cast<core::Vector3*>(p_field) = vec3_v;
          return;
        }
        if (vec_parts.size() == 2 &&
            TryParseFloatOrPercentInvariant(vec_parts[0], fp4_x) &&
            TryParseFloatOrPercentInvariant(vec_parts[1], fp4_y)) {
          vec3_v = core::Vector3{fp4_x, fp4_y, 0.0f};
          *reinterpret_cast<core::Vector3*>(p_field) = vec3_v;
          return;
        }
      }
      InvalidValueFor(desc, sv_value);
    }
    case FieldType::BooleanExpression:
    case FieldType::IntegerExpression: {
      // ParseBooleanExpression/ParseIntegerExpression(L425-447):构造失败包上
      // InvalidValue 文本前缀(类型名后附 ": 内层消息")
      // ParseBooleanExpression/ParseIntegerExpression (L425-447): construction
      // failures rewrap with the InvalidValue text prefix.
      using expr::VariableExpression;
      std::optional<VariableExpression>* p_opt =
          reinterpret_cast<std::optional<VariableExpression>*>(p_field);
      try {
        *p_opt = VariableExpression{std::string{sv_value}};
      } catch (const expr::ExpressionException& e_except) {
        throw yaml::YamlException(std::format(
            "FieldLoader: Cannot parse `{}` into field `{}` of type `{}`: {}", sv_value,
            desc.str_name, desc.str_type_name, e_except.what()));
      }
      return;
    }
    case FieldType::BitSet: {
      // ParseBitSet(L720-742):Split(Comma) trim/跳空 → 字符串组
      // ParseBitSet (L720-742): Split(Comma), trimmed, empties skipped → the
      // string set.
      std::vector<std::string> vec_strs;
      if (!sv_value.empty()) {
        for (const std::string_view sv_part : SplitComma(sv_value)) {
          const std::string_view sv_trimmed = TrimNetWhiteSpace(sv_part);
          if (!sv_trimmed.empty())
            vec_strs.emplace_back(sv_trimmed);
        }
      }
      *reinterpret_cast<std::uint64_t*>(p_field) = BitsOf(desc.str_type_name, vec_strs);
      return;
    }

    // ———— 数组 / arrays ————
    // 目标 C++ 类型一律 std::vector<E>(ImmutableArray/List/HashSet/FrozenSet 合流;
    // 运行时查找/去重语义 Phase 5 需要时再分化)
    // The target C++ type is uniformly std::vector<E> (ImmutableArray/List/
    // HashSet/FrozenSet conflate; runtime lookup/dedup semantics split later
    // in Phase 5 as needed).
    case FieldType::Array:
    case FieldType::ImmutableArray:
    case FieldType::List:
    case FieldType::HashSet:
    case FieldType::FrozenSet: {
      const FieldDesc& desc_elem = *desc.elem;
      switch (desc_elem.type) {
        case FieldType::Int32:
        case FieldType::Enum:
        case FieldType::Byte:
        case FieldType::UInt16:
        case FieldType::Int16:
        case FieldType::WAngle: {
          // WAngle 走 int 载荷;元素 vector 统一为元素类型的底层形态由
          // 生成器按 C++ 字段声明选择(见 gen:vector<int32_t> 装载 WAngle
          // 的场景不存在——WAngle 数组在 vector<WAngle> 分支)
          if (desc_elem.type == FieldType::WAngle) {
            *reinterpret_cast<std::vector<WAngle>*>(p_field) =
                ParseVectorOf<WAngle>(desc, desc_elem, sv_value);
            return;
          }
          // 枚举数组:vector<int32_t>(枚举字段底层 int32)
          // Enum arrays: vector<int32_t> (enum fields are int32-backed).
          *reinterpret_cast<std::vector<std::int32_t>*>(p_field) =
              ParseVectorOf<std::int32_t>(desc, desc_elem, sv_value);
          return;
        }
        case FieldType::Float:
          *reinterpret_cast<std::vector<float>*>(p_field) =
              ParseVectorOf<float>(desc, desc_elem, sv_value);
          return;
        case FieldType::String:
          *reinterpret_cast<std::vector<std::string>*>(p_field) =
              ParseVectorOf<std::string>(desc, desc_elem, sv_value);
          return;
        case FieldType::Color:
          *reinterpret_cast<std::vector<core::Color>*>(p_field) =
              ParseVectorOf<core::Color>(desc, desc_elem, sv_value);
          return;
        case FieldType::WDist:
          *reinterpret_cast<std::vector<WDist>*>(p_field) =
              ParseVectorOf<WDist>(desc, desc_elem, sv_value);
          return;
        case FieldType::BitSet:
          *reinterpret_cast<std::vector<std::uint64_t>*>(p_field) =
              ParseVectorOf<std::uint64_t>(desc, desc_elem, sv_value);
          return;
        case FieldType::WVec:
          // ImmutableArray<WVec> 分组特化(L251-282)
          // The ImmutableArray<WVec> grouped specialization (L251-282).
          *reinterpret_cast<std::vector<WVec>*>(p_field) =
              ParseGroupedVectorOf<WVec, 3>(desc, FieldType::WDist, sv_value);
          return;
        case FieldType::CPos:
          *reinterpret_cast<std::vector<CPos>*>(p_field) =
              ParseGroupedVectorOf<CPos, 2>(desc, FieldType::Int32, sv_value);
          return;
        case FieldType::CVec:
          *reinterpret_cast<std::vector<CVec>*>(p_field) =
              ParseGroupedVectorOf<CVec, 2>(desc, FieldType::Int32, sv_value);
          return;
        case FieldType::Int2:
          *reinterpret_cast<std::vector<int2>*>(p_field) =
              ParseGroupedVectorOf<int2, 2>(desc, FieldType::Int32, sv_value);
          return;
        default:
          DefaultUnknownField(desc.str_name);
      }
    }

    // ———— 字典 / dictionaries ————
    // 目标 C++ 类型 std::vector<std::pair<K,V>>(插入序;FrozenDictionary 的
    // .NET 枚举序≠插入序,dump 协议按键排序,运行时序 Phase 5 评估——偏离已登记)
    // Target C++ type std::vector<std::pair<K,V>> (insertion order; the .NET
    // FrozenDictionary enumeration order differs — the dump protocol sorts by
    // key and the runtime order is a Phase 5 concern; registered deviation).
    case FieldType::Dictionary:
    case FieldType::FrozenDictionary: {
      const FieldDesc& desc_key = *desc.key;
      const FieldDesc& desc_val = *desc.value;

      // 值解析:标量值走 node.Value(Trim);数组值走 node.Value 字符串
      // Value parse: scalar values use node.Value (trimmed); array values use
      // the node.Value string.
      // —— 组合展开:K ∈ {Int32,String,BooleanExpression,CPos,CVec};
      //    V ∈ {Int32,String,Float,BooleanExpression,数组(String/Int32/Float)}
      // Combined expansion: K in {Int32,String,BooleanExpression,CPos,CVec};
      // V in {Int32,String,Float,BooleanExpression,arrays(String/Int32/Float)}.
      const auto load_dict = [&]<class K, class V>(K&&, V&&) {
        std::vector<std::pair<K, V>> vec_ret;
        for (const yaml::MiniYamlNode& node_entry : node.Nodes) {
          K key_k{};
          const std::string_view sv_key =
              TrimNetWhiteSpace(node_entry.Key != nullptr ? std::string_view{*node_entry.Key}
                                                          : std::string_view{});
          if constexpr (std::same_as<K, std::int32_t>) {
            if (desc_key.type == FieldType::Enum ? !TryParseEnumNet(desc_key.str_type_name,
                                                                    sv_key, key_k)
                                                 : !TryParseInt32Invariant(sv_key, key_k))
              InvalidValueFor(desc, sv_key);
          } else if constexpr (std::same_as<K, std::string>) {
            key_k = std::string{sv_key};
          } else if constexpr (std::same_as<K, std::optional<expr::VariableExpression>>) {
            try {
              key_k = expr::VariableExpression{std::string{sv_key}};
            } catch (const expr::ExpressionException& e_except) {
              throw yaml::YamlException(std::format(
                  "FieldLoader: Cannot parse `{}` into field `{}` of type `{}`: {}",
                  sv_key, desc.str_name, desc.str_type_name, e_except.what()));
            }
          } else if constexpr (std::same_as<K, CPos>) {
            if (!TryParseCPosNet(sv_key, key_k))
              InvalidValueFor(desc, sv_key);
          } else if constexpr (std::same_as<K, CVec>) {
            if (!TryParseCVecNet(sv_key, key_k))
              InvalidValueFor(desc, sv_key);
          }

          V val_v{};
          const std::string_view sv_val = TrimmedValue(node_entry.Value);
          if constexpr (std::same_as<V, std::int32_t>) {
            if (desc_val.type == FieldType::Enum ? !TryParseEnumNet(desc_val.str_type_name,
                                                                    sv_val, val_v)
                                                 : !TryParseInt32Invariant(sv_val, val_v))
              InvalidValueFor(desc, sv_val);
          } else if constexpr (std::same_as<V, std::string>) {
            val_v = std::string{sv_val};
          } else if constexpr (std::same_as<V, float>) {
            if (!TryParseFloatOrPercentInvariant(sv_val, val_v))
              InvalidValueFor(desc, sv_val);
          } else if constexpr (std::same_as<V, std::optional<expr::VariableExpression>>) {
            try {
              val_v = expr::VariableExpression{std::string{sv_val}};
            } catch (const expr::ExpressionException& e_except) {
              throw yaml::YamlException(std::format(
                  "FieldLoader: Cannot parse `{}` into field `{}` of type `{}`: {}",
                  sv_val, desc.str_name, desc.str_type_name, e_except.what()));
            }
          } else {
            // vector 元素值 / vector-valued entries.
            val_v = ParseVectorOf<typename V::value_type>(desc, *desc_val.elem, sv_val);
          }

          // Dictionary.Add 重复键语义 / the Dictionary.Add duplicate semantics.
          for (const auto& [k_existing, v_existing] : vec_ret) {
            (void)v_existing;
            if constexpr (std::same_as<K, std::int32_t> || std::same_as<K, float>) {
              if (k_existing == key_k)
                throw yaml::YamlException("An item with the same key has already been added.");
            } else if constexpr (std::same_as<K, std::string>) {
              if (k_existing == key_k)
                throw yaml::YamlException("An item with the same key has already been added.");
            } else if constexpr (std::same_as<K, CPos> || std::same_as<K, CVec>) {
              if (k_existing == key_k)
                throw yaml::YamlException("An item with the same key has already been added.");
            } else {
              if (k_existing.has_value() && key_k.has_value() &&
                  k_existing->Expression() == key_k->Expression())
                throw yaml::YamlException("An item with the same key has already been added.");
            }
          }
          vec_ret.emplace_back(std::move(key_k), std::move(val_v));
        }
        *reinterpret_cast<std::vector<std::pair<K, V>>*>(p_field) = std::move(vec_ret);
      };

      // 键类型分派 / key-type dispatch.
      const auto dispatch_value = [&]<class K>(K&& key_tag) {
        switch (desc_val.type) {
          case FieldType::Int32:
          case FieldType::Enum:
            load_dict(K{}, std::int32_t{});
            break;
          case FieldType::String:
            load_dict(K{}, std::string{});
            break;
          case FieldType::Float:
            load_dict(K{}, float{});
            break;
          case FieldType::BooleanExpression:
          case FieldType::IntegerExpression:
            load_dict(K{}, std::optional<expr::VariableExpression>{});
            break;
          case FieldType::Array:
          case FieldType::ImmutableArray:
          case FieldType::List:
          case FieldType::HashSet:
          case FieldType::FrozenSet: {
            const FieldType type_elem = desc_val.elem->type;
            if (type_elem == FieldType::String)
              load_dict(K{}, std::vector<std::string>{});
            else if (type_elem == FieldType::Int32 || type_elem == FieldType::Enum)
              load_dict(K{}, std::vector<std::int32_t>{});
            else if (type_elem == FieldType::Float)
              load_dict(K{}, std::vector<float>{});
            else
              DefaultUnknownField(desc.str_name);
            break;
          }
          default:
            // 嵌套记录字典值经 LoadUsing 加载器(mods 层),不走通用分派
            // Nested-record dictionary values go through LoadUsing loaders
            // (mods layer), not the generic dispatch.
            DefaultUnknownField(desc.str_name);
        }
      };

      switch (desc_key.type) {
        case FieldType::Int32:
        case FieldType::Enum:
          dispatch_value(std::int32_t{});
          break;
        case FieldType::String:
          dispatch_value(std::string{});
          break;
        case FieldType::BooleanExpression:
        case FieldType::IntegerExpression:
          dispatch_value(std::optional<expr::VariableExpression>{});
          break;
        case FieldType::CPos:
          dispatch_value(CPos{});
          break;
        case FieldType::CVec:
          dispatch_value(CVec{});
          break;
        default:
          DefaultUnknownField(desc.str_name);
      }
      return;
    }

    // ———— Nullable<T>(L744-752)————
    case FieldType::Nullable: {
      if (sv_value.empty()) {
        // 空 = null:nullopt(所有 nullable 目标形态一致置空)
        // Empty = null: nullopt (every nullable target shape clears alike).
        switch (desc.elem->type) {
          case FieldType::Color:
            reinterpret_cast<std::optional<core::Color>*>(p_field)->reset();
            return;
          case FieldType::WAngle:
            reinterpret_cast<std::optional<WAngle>*>(p_field)->reset();
            return;
          case FieldType::WDist:
            reinterpret_cast<std::optional<WDist>*>(p_field)->reset();
            return;
          case FieldType::Bool:
            reinterpret_cast<std::optional<bool>*>(p_field)->reset();
            return;
          default:
            DefaultUnknownField(desc.str_name);
        }
      }
      // 非空:按内部类型写入(目标 = optional<T>)
      // Non-empty: parse the inner type (target = optional<T>).
      switch (desc.elem->type) {
        case FieldType::Color: {
          core::Color color_v{};
          if (!core::Color::TryParse(sv_value, color_v))
            InvalidValueFor(desc, sv_value);
          *reinterpret_cast<std::optional<core::Color>*>(p_field) = color_v;
          return;
        }
        case FieldType::WAngle: {
          std::int32_t int4_v{};
          if (!TryParseInt32Invariant(sv_value, int4_v))
            InvalidValueFor(desc, sv_value);
          *reinterpret_cast<std::optional<WAngle>*>(p_field) = WAngle{int4_v};
          return;
        }
        case FieldType::WDist: {
          const std::expected<WDist, bool> exp_d = WDist::TryParse(sv_value);
          if (!exp_d.has_value())
            InvalidValueFor(desc, sv_value);
          *reinterpret_cast<std::optional<WDist>*>(p_field) = *exp_d;
          return;
        }
        case FieldType::Bool: {
          bool b_v{};
          if (!TryParseBoolNet(sv_value, b_v))
            InvalidValueFor(desc, sv_value);
          *reinterpret_cast<std::optional<bool>*>(p_field) = b_v;
          return;
        }
        default:
          DefaultUnknownField(desc.str_name);
      }
    }

    // ———— 其余暂未覆盖形态 / shapes not yet covered ————
    case FieldType::DateTime:
    case FieldType::Hotkey:
    case FieldType::HotkeyReference:
    case FieldType::Size:
    case FieldType::Rectangle:
      // trait/weapon 加载链无使用点(schema_dumper --scan 验证);
      // widget/settings 阶段落地
      // No use sites in the trait/weapon loading chain (verified by
      // schema_dumper --scan); lands with the widget/settings phases.
      DefaultUnknownField(desc.str_name);

    case FieldType::Record:
      // 嵌套记录仅经 LoadUsing 进入(通用路径无字符串解析器,与上游一致)
      // Nested records only enter via LoadUsing (the generic path has no
      // string parser, matching upstream).
      DefaultUnknownField(desc.str_name);

    case FieldType::Opaque:
      // 运行时对象字段:上游 GetValue 落到 TypeConverter/UnknownFieldAction;
      // 合法 mods 数据不触发;有 LoadUsing 的字段在 Load 主循环分派,不到这里
      // Runtime-object fields: upstream GetValue falls through to
      // TypeConverter/UnknownFieldAction; valid mods data never triggers it.
      // LoadUsing fields dispatch in the Load loop and never reach here.
      DefaultUnknownField(desc.str_name);
  }
  std::unreachable();
}

}  // namespace

// ———— Load 主循环(L754-796)/ The Load loop ————

void Load(RecordObject* obj_record, const yaml::MiniYaml& yaml_my) {
  const RecordDesc& desc = obj_record->record_desc();
  std::vector<const FieldDesc*> vec_fields = CollectFields(desc);
  std::vector<std::string> vec_missing;

  // md = my.ToDictionary()(L765;重复键抛 YamlException,Phase 1 已逐字)
  // md = my.ToDictionary() (L765; duplicate keys raise YamlException,
  // verbatim since Phase 1).
  const std::vector<std::pair<std::string_view, const yaml::MiniYaml*>> vec_md =
      yaml_my.ToDictionary();
  const auto find_md = [&vec_md](std::string_view sv_key) -> const yaml::MiniYaml* {
    for (const auto& [sv_k, yaml_v] : vec_md)
      if (sv_k == sv_key)
        return yaml_v;
    return nullptr;
  };

  for (const FieldDesc* desc_f : vec_fields) {
    const FieldDesc& desc_field = *desc_f;

    if (!desc_field.str_loader.empty()) {
      // LoadUsing(L766-776):required 时键必须在;loader 收到整个对象节点
      // LoadUsing (L766-776): the key must exist when required; the loader
      // receives the whole object node.
      const yaml::MiniYaml* yaml_found = find_md(desc_field.str_name);
      if (desc_field.b_required && yaml_found == nullptr) {
        vec_missing.push_back(std::string{desc_field.str_name});
        continue;
      }
      const auto it_loader = LoaderTable().find(desc_field.str_loader);
      if (it_loader == LoaderTable().end())
        UnknownLoader(desc_field.str_loader, desc_field.str_name);
      it_loader->second(FieldPtr(obj_record, desc_field), yaml_my);
      continue;
    }

    const yaml::MiniYaml* yaml_found = find_md(desc_field.str_name);
    if (yaml_found == nullptr) {
      // TryGetValueFromYaml miss(L779-787):Required 记缺失,否则保持默认
      // TryGetValueFromYaml miss (L779-787): record required misses,
      // otherwise keep the default.
      if (desc_field.b_required)
        vec_missing.push_back(std::string{desc_field.str_name});
      continue;
    }

    LoadValueInto(desc_field, FieldPtr(obj_record, desc_field), *yaml_found);
  }

  if (!vec_missing.empty())
    throw MissingFieldsException(std::move(vec_missing));
}

// ———— 便捷入口(L839-842)/ Convenience entries ————

std::int32_t GetInt32Value(std::string_view str_field, std::string_view sv_value) {
  std::int32_t int4_v{};
  const std::string_view sv_trimmed = TrimNetWhiteSpace(sv_value);
  if (!TryParseInt32Invariant(sv_trimmed, int4_v))
    DefaultInvalidValue(sv_trimmed, "System.Int32", str_field);
  return int4_v;
}

float GetFloatValue(std::string_view str_field, std::string_view sv_value) {
  float fp4_v{};
  const std::string_view sv_trimmed = TrimNetWhiteSpace(sv_value);
  if (!TryParseFloatOrPercentInvariant(sv_trimmed, fp4_v))
    DefaultInvalidValue(sv_trimmed, "System.Single", str_field);
  return fp4_v;
}

bool GetBoolValue(std::string_view str_field, std::string_view sv_value) {
  bool b_v{};
  const std::string_view sv_trimmed = TrimNetWhiteSpace(sv_value);
  if (!TryParseBoolNet(sv_trimmed, b_v))
    DefaultInvalidValue(sv_trimmed, "System.Boolean", str_field);
  return b_v;
}

std::vector<std::string> GetStringArrayValue(std::string_view str_field, std::string_view sv_value) {
  // FieldLoader.GetValue<ImmutableArray<string>>(L839-842 + ParseImmutableArray);
  // 字段名取调用方参数(上游 GetValue<T>(field, value) 的 field 传播)
  // The field name is the caller's argument (the field propagation of the
  // upstream GetValue<T>(field, value)).
  static constexpr FieldDesc kElemStr = ElemOf(FieldType::String);
  const FieldDesc desc_arr{
      .str_name = str_field, .type = FieldType::ImmutableArray, .b_required = false,
      .str_loader = {}, .off_offset = 0, .elem = &kElemStr, .key = nullptr,
      .value = nullptr,
      .str_type_name =
          "System.Collections.Immutable.ImmutableArray`1[System.String]"};
  const std::string_view sv_trimmed = TrimNetWhiteSpace(sv_value);
  return ParseVectorOf<std::string>(desc_arr, kElemStr, sv_trimmed);
}

}  // namespace ora::meta
