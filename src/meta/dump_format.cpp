// UPSTREAM: OpenRA.Game/FieldLoader.cs @7d57605(dump_format.hpp 的实现;
//          .NET float/WDist/枚举格式化复刻,见 hpp 头注)
//          Implementation of dump_format.hpp — the .NET float/WDist/enum
//          formatting replicas; see the hpp header notes.
import std;
#include "meta/dump_format.hpp"
#include "meta/field_loader.hpp"
#include "meta/parse.hpp"
#include "core/color.hpp"
#include "core/text.hpp"

namespace ora::meta {

/// WDist.ToString()(WDist.cs L110-116):"NcM" 定点形式(|v|=N*1024+M)
/// WDist.ToString() (WDist.cs L110-116): the "NcM" fixed-point form.
std::string FormatWDistNet(std::int64_t int8_length) {
  const std::int64_t int8_abs = std::abs(int8_length);
  return std::format("{}{}c{}", int8_length < 0 ? "-" : "", int8_abs / 1024,
                     int8_abs % 1024);
}

std::string FormatFloatNet(float fp4_v) {
  if (std::isnan(fp4_v))
    return "NaN";
  if (std::isinf(fp4_v))
    return fp4_v > 0 ? "Infinity" : "-Infinity";
  if (fp4_v == 0.0f)
    return std::signbit(fp4_v) ? "-0" : "0";

  // 最短往返科学形式 "d[.ddd]e±dd" → digits + exp10
  // The shortest-round-trip scientific form "d[.ddd]e±dd" → digits + exp10.
  char arr_buf[64]{};
  const auto [ptr_end, ec_err] =
      std::to_chars(arr_buf, arr_buf + sizeof arr_buf, fp4_v,
                    std::chars_format::scientific);
  std::string_view sv_sci{arr_buf, static_cast<std::size_t>(ptr_end - arr_buf)};

  bool b_negative{false};
  if (!sv_sci.empty() && sv_sci.front() == '-') {
    b_negative = true;
    sv_sci.remove_prefix(1);
  }

  const std::size_t int4_e = sv_sci.find('e');
  std::string str_digits;  // 去小数点的数字串 / the digit string without the point
  for (const char ch : sv_sci.substr(0, int4_e))
    if (ch != '.')
      str_digits.push_back(ch);
  const std::int32_t int4_exp =
      std::stoi(std::string{sv_sci.substr(int4_e + 1)});  // 'e' 后 ±dd
  // 首位有效数字的十进制指数 = e + (digits-1) 形式上即 int4_exp 本身
  // The decimal exponent of the leading digit is int4_exp itself.

  std::string str_ret;
  if (int4_exp >= -4 && int4_exp <= 6) {
    // 定点摆位 / fixed-point placement.
    if (int4_exp >= 0) {
      if (static_cast<std::size_t>(int4_exp) + 1 >= str_digits.size()) {
        str_ret = str_digits + std::string(
            static_cast<std::size_t>(int4_exp) + 1 - str_digits.size(), '0');
      } else {
        str_ret = str_digits.substr(0, static_cast<std::size_t>(int4_exp) + 1) +
                  "." + str_digits.substr(static_cast<std::size_t>(int4_exp) + 1);
      }
    } else {
      str_ret = "0." + std::string(static_cast<std::size_t>(-int4_exp - 1), '0') +
                str_digits;
    }
  } else {
    // 科学记法 "d[.ddd]E±dd" / the scientific form.
    str_ret = str_digits.substr(0, 1);
    if (str_digits.size() > 1)
      str_ret += "." + str_digits.substr(1);
    const std::int32_t int4_absExp = std::abs(int4_exp);
    str_ret += std::format("E{}{:02d}", int4_exp >= 0 ? "+" : "-", int4_absExp);
  }
  return b_negative ? "-" + str_ret : str_ret;
}

namespace {

/// 排序键(字典输出的稳定序):int64 数值序 < string 字典序;混合不发生
/// (键类型一致)
/// Sort key (the stable dictionary order): int64 numeric < string
/// lexicographic; never mixed (key types are uniform).
std::string DictSortKeyOf(const GenericValue& val) {
  if (const auto* int8_p = std::get_if<std::int64_t>(&val.val))
    return std::format("{:020d}", *int8_p);
  if (const auto* str_p = std::get_if<std::string>(&val.val))
    return *str_p;
  if (const auto* tuple_p = std::get_if<GenericTuple>(&val.val))
    return std::format("{:011d},{:011d},{:011d}", tuple_p->arr_ints[0],
                       tuple_p->arr_ints[1], tuple_p->arr_ints[2]);
  if (const auto* fp4_p = std::get_if<float>(&val.val))
    return FormatFloatNet(*fp4_p);
  return {};
}

/// 嵌套记录的递归展开("{类型名:{字段=值,…}}";字段 = CollectFields 展开)
/// Recursive nested-record expansion ("{TypeName:{Field=value,…}}"; fields
/// via the CollectFields expansion).
std::string FormatRecord(const RecordObject& rec) {
  const RecordDesc& desc = rec.record_desc();
  const auto* rec_generated = dynamic_cast<const GeneratedRecord*>(&rec);
  if (rec_generated == nullptr)
    return std::format("{{{}:<?>}}", desc.str_name);

  const std::vector<const FieldDesc*> vec_fields = CollectFields(desc);
  const std::vector<GenericValue>& vec_values = rec_generated->Values();
  std::string str_ret = std::format("{{{}:{{", desc.str_name);
  bool b_first = true;
  for (std::size_t int4_i{}; int4_i < vec_fields.size() && int4_i < vec_values.size();
       int4_i++) {
    if (!b_first)
      str_ret += ", ";
    b_first = false;
    str_ret += std::format("{}={}", vec_fields[int4_i]->str_name,
                           FormatValue(*vec_fields[int4_i], vec_values[int4_i]));
  }
  str_ret += "}}";
  return str_ret;
}

/// 元组分量的类型化格式化:WVec/WPos 分量 = WDist("NcM"),其余整型按十进制,
/// float 分量走 FormatFloatNet
/// Typed tuple-component formatting: WVec/WPos components = WDist ("NcM"),
/// other integers in decimal, float components through FormatFloatNet.
std::string FormatTupleComponents(const GenericTuple& t_v, FieldType type_owner) {
  const bool b_wdist = type_owner == FieldType::WVec || type_owner == FieldType::WPos;
  const bool b_floats =
      type_owner == FieldType::Vector2 || type_owner == FieldType::Vector3;
  std::string str_ret;
  bool b_first = true;
  for (std::size_t int4_i{}; int4_i < t_v.uint1_count; int4_i++) {
    if (!b_first)
      str_ret += ",";
    b_first = false;
    if (b_floats)
      str_ret += FormatFloatNet(t_v.arr_floats[int4_i]);
    else if (b_wdist)
      str_ret += FormatWDistNet(t_v.arr_ints[int4_i]);
    else
      str_ret += std::format("{}", t_v.arr_ints[int4_i]);
  }
  return str_ret;
}

}  // namespace

std::string FormatValue(const FieldDesc& desc, const GenericValue& val) {
  if (val.IsNull()) {
    // C# null(字符串/表达式/Nullable/引用容器 null)一律 → "(null)"
    // C# null (strings/expressions/Nullable/null reference containers) —
    // uniformly "(null)".
    return "(null)";
  }

  switch (desc.type) {
    case FieldType::Bool:
      return std::get<bool>(val.val) ? "True" : "False";
    case FieldType::WDist:
      // WDist.ToString()(WDist.cs L110-116)
      return FormatWDistNet(std::get<std::int64_t>(val.val));
    case FieldType::Byte:
    case FieldType::UInt16:
    case FieldType::Int16:
    case FieldType::Int32:
    case FieldType::WAngle:
      // 有符号/无符号宽度差异在合法数据不可见(解析阶段已拒越界);
      // UInt16 按无符号十进制输出
      // Width/sign differences are invisible on valid data (parsing rejected
      // out-of-range); UInt16 prints unsigned decimal.
      return std::format("{}", std::get<std::int64_t>(val.val));
    case FieldType::Enum:
      return EnumToStringNet(desc.str_type_name,
                             static_cast<std::int32_t>(std::get<std::int64_t>(val.val)));
    case FieldType::Float:
      return FormatFloatNet(std::get<float>(val.val));
    case FieldType::String:
      return std::get<std::string>(val.val);
    case FieldType::Color:
      return core::Color{static_cast<std::uint32_t>(
          std::get<std::int64_t>(val.val))}.ToString();
    case FieldType::WVec:
    case FieldType::WPos:
    case FieldType::WRot:
    case FieldType::CPos:
    case FieldType::CVec:
    case FieldType::Int2:
    case FieldType::Vector2:
    case FieldType::Vector3:
    case FieldType::Size:
    case FieldType::Rectangle:
      return FormatTupleComponents(std::get<GenericTuple>(val.val), desc.type);
    case FieldType::BooleanExpression:
    case FieldType::IntegerExpression:
      return std::get<std::string>(val.val);
    case FieldType::BitSet: {
      const std::vector<std::string> vec_strs = StringsOfBits(
          desc.str_type_name,
          static_cast<std::uint64_t>(std::get<std::int64_t>(val.val)));
      if (vec_strs.empty())
        return "(empty)";
      std::string str_ret;
      bool b_first = true;
      for (const std::string& str_s : vec_strs) {
        if (!b_first)
          str_ret += ",";
        b_first = false;
        str_ret += str_s;
      }
      return str_ret;
    }

    case FieldType::Array:
    case FieldType::ImmutableArray:
    case FieldType::List:
    case FieldType::HashSet:
    case FieldType::FrozenSet: {
      const std::vector<GenericValue>& vec_vals =
          std::get<std::vector<GenericValue>>(val.val);
      if (vec_vals.empty())
        return "(empty)";
      // FrozenSet/HashSet 输出按值排序(枚举序不稳定;见文件头协议)
      // FrozenSet/HashSet output sorts by value (their enumeration order is
      // unstable; see the protocol note).
      std::vector<std::string> vec_formatted;
      vec_formatted.reserve(vec_vals.size());
      for (const GenericValue& val_e : vec_vals)
        vec_formatted.push_back(FormatValue(*desc.elem, val_e));
      if (desc.type == FieldType::HashSet || desc.type == FieldType::FrozenSet)
        std::ranges::sort(vec_formatted);
      std::string str_ret;
      bool b_first = true;
      for (const std::string& str_f : vec_formatted) {
        if (!b_first)
          str_ret += ",";
        b_first = false;
        str_ret += str_f;
      }
      return str_ret;
    }

    case FieldType::Dictionary:
    case FieldType::FrozenDictionary: {
      const GenericDict& map_vals = std::get<GenericDict>(val.val);
      if (map_vals.empty())
        return "(empty)";
      // 稳定序:按排序键升序(见文件头)
      // Stable order: ascending by sort key (see the header note).
      std::vector<const std::pair<GenericValue, GenericValue>*> vec_sorted;
      vec_sorted.reserve(map_vals.size());
      for (const auto& pair_kv : map_vals)
        vec_sorted.push_back(&pair_kv);
      std::ranges::sort(vec_sorted, [](const auto* a, const auto* b) {
        return DictSortKeyOf(a->first) < DictSortKeyOf(b->first);
      });
      std::string str_ret;
      bool b_first = true;
      for (const auto* pair_kv : vec_sorted) {
        if (!b_first)
          str_ret += ",";
        b_first = false;
        str_ret += std::format("{}={}", FormatValue(*desc.key, pair_kv->first),
                               FormatValue(*desc.value, pair_kv->second));
      }
      return str_ret;
    }

    case FieldType::Nullable:
      // 非空:按内层格式化 / non-null: format the inner value.
      return FormatValue(*desc.elem, val);

    case FieldType::Record:
    case FieldType::Opaque: {
      // Opaque 槽:按实际载荷(loader 写入形态)格式化
      // Opaque slots: format by the actual payload (the loader-written
      // shape).
      if (const auto* rec_p = std::get_if<std::shared_ptr<RecordObject>>(&val.val))
        return *rec_p != nullptr ? FormatRecord(**rec_p) : "(null)";
      if (const auto* map_p = std::get_if<GenericDict>(&val.val)) {
        if (map_p->empty())
          return "(empty)";
        // MapSmudge 等 dict 值形态:统一 k=v;排序同上
        // dict-shaped payloads (MapSmudge etc.): uniform k=v; sorted as
        // above.
        std::vector<const std::pair<GenericValue, GenericValue>*> vec_sorted;
        for (const auto& pair_kv : *map_p)
          vec_sorted.push_back(&pair_kv);
        std::ranges::sort(vec_sorted, [](const auto* a, const auto* b) {
          return DictSortKeyOf(a->first) < DictSortKeyOf(b->first);
        });
        FieldDesc desc_key = ElemOf(FieldType::String);
        std::string str_ret;
        bool b_first = true;
        for (const auto* pair_kv : vec_sorted) {
          if (!b_first)
            str_ret += ",";
          b_first = false;
          // 值按实际载荷自识别(dict = MapSmudge 的 {Type,Depth};
          // tuple = TerrainInfo;rec = 嵌套记录)
          // Values self-identify by payload (dict = the MapSmudge
          // {Type,Depth}; tuple = TerrainInfo; rec = nested records).
          // 键同样自识别(Footprint 键 = CVec 元组,TerrainSpeeds 键 = string)
          // Keys self-identify too (Footprint keys = CVec tuples,
          // TerrainSpeeds keys = strings).
          const FieldDesc desc_opaque = ElemOf(FieldType::Opaque);
          str_ret += std::format("{}={}", FormatValue(desc_opaque, pair_kv->first),
                                 FormatValue(desc_opaque, pair_kv->second));
        }
        return str_ret;
      }
      if (const auto* vec_p = std::get_if<std::vector<GenericValue>>(&val.val)) {
        if (vec_p->empty())
          return "(empty)";
        FieldDesc desc_elem = ElemOf(FieldType::Opaque);
        std::string str_ret;
        bool b_first = true;
        for (const GenericValue& val_e : *vec_p) {
          if (!b_first)
            str_ret += ",";
          b_first = false;
          str_ret += FormatValue(desc_elem, val_e);
        }
        return str_ret;
      }
      if (const auto* tuple_p = std::get_if<GenericTuple>(&val.val))
        return FormatTupleComponents(*tuple_p, FieldType::Opaque);
      if (const auto* int8_p = std::get_if<std::int64_t>(&val.val))
        return std::format("{}", *int8_p);
      if (const auto* str_p = std::get_if<std::string>(&val.val))
        return *str_p;
      if (const auto* fp4_p = std::get_if<float>(&val.val))
        return FormatFloatNet(*fp4_p);
      return "(null)";
    }

    case FieldType::DateTime:
    case FieldType::Hotkey:
    case FieldType::HotkeyReference:
      return "(null)";
  }
  std::unreachable();
}

}  // namespace ora::meta
