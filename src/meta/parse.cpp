// UPSTREAM: OpenRA.Game/Exts.cs @b6fc03f L485-566 + OpenRA.Game/FieldLoader.cs L146-591
//          (parse.hpp 的实现;BCL 语义注记见头文件)
//          Implementation of parse.hpp (BCL semantics documented in the header).
import std;
#include "parse.hpp"
#include "core/cell_pos.hpp"
#include "core/cvec.hpp"
#include "core/text.hpp"

namespace ora::meta {

namespace {

/// NumberStyles.Integer 的公共骨架:码点级 Trim → 可选 '+'(from_chars 不吃 '+',
/// 手动剥)→ from_chars 十进制整串 → 溢出由 from_chars 判定
/// The common NumberStyles.Integer skeleton: code-point Trim → optional '+'
/// (from_chars rejects it, so strip manually) → from_chars decimal over the
/// whole remainder → overflow decided by from_chars.
template <class T>
  requires(std::integral<T>)
bool TryParseIntegerNet(std::string_view sv, T& out) {
  sv = TrimNetWhiteSpace(sv);
  bool b_negative{false};
  if (!sv.empty() && (sv.front() == '+' || sv.front() == '-')) {
    b_negative = sv.front() == '-';
    sv.remove_prefix(1);
  }
  // 符号后必须至少一位数字 / At least one digit must follow the sign.
  if (sv.empty())
    return false;

  // from_chars 对无符号类型不接受 '-',对有符号接受;统一按无符号幅值解析后应用符号
  // from_chars rejects '-' for unsigned types and accepts it for signed ones;
  // parse the magnitude as unsigned uniformly, then apply the sign.
  using Mag = std::conditional_t<std::signed_integral<T>, std::uint64_t, T>;
  Mag mag{};
  auto [ptr_end, ec_err] = std::from_chars(sv.data(), sv.data() + sv.size(), mag, 10);
  if (ec_err != std::errc{} || ptr_end != sv.data() + sv.size())
    return false;

  // 幅值/符号落回目标宽度的溢出检查(等价 C# "数值超出范围即 false");
  // int64 中转覆盖 ≤32 位类型,-fwrapv 回绕兜底 int64 负端
  // Range check back into the target width (equivalent to C# returning false
  // on out-of-range values); the int64 intermediate covers widths ≤ 32 bits,
  // and -fwrapv wraparound covers the int64 negative edge.
  if constexpr (sizeof(T) < 8) {
    std::int64_t int8_mag{static_cast<std::int64_t>(mag)};
    if (b_negative)
      int8_mag = -int8_mag;
    if (int8_mag < static_cast<std::int64_t>(std::numeric_limits<T>::min()) ||
        int8_mag > static_cast<std::int64_t>(std::numeric_limits<T>::max()))
      return false;
    out = static_cast<T>(int8_mag);
  } else if constexpr (std::same_as<T, std::int64_t>) {
    if ((!b_negative && mag > static_cast<Mag>(std::numeric_limits<T>::max())) ||
        (b_negative && mag > (std::uint64_t{1} << 63)))
      return false;
    out = b_negative ? static_cast<T>(~mag + 1) : static_cast<T>(mag);
  } else {
    if (b_negative || mag > std::numeric_limits<T>::max())
      return false;
    out = static_cast<T>(mag);
  }
  return true;
}

/// 大小写不敏感比较 / Case-insensitive compare.
bool EqualsIgnoreCase(std::string_view sv_a, std::string_view sv_b) {
  if (sv_a.size() != sv_b.size())
    return false;
  for (std::size_t int4_i{}; int4_i < sv_a.size(); int4_i++) {
    const char chr_a = static_cast<char>(std::tolower(static_cast<unsigned char>(sv_a[int4_i])));
    const char chr_b = static_cast<char>(std::tolower(static_cast<unsigned char>(sv_b[int4_i])));
    if (chr_a != chr_b)
      return false;
  }
  return true;
}

/// 枚举注册表(名字 → 成员表;RegisterEnum 填充,gen/ 静态初始化)
/// Enum registry (name → member table; filled by RegisterEnum from the static
/// initializers of gen/).
struct EnumTable {
  // 成员表按值拷贝(调用方可传栈上数组;str_name 的 string_view 指向
  // 字面量,静态存储安全)
  // Member tables stored by value (callers may pass stack arrays; the
  // str_name string_views point at literals and stay statically safe).
  std::unordered_map<std::string_view, std::vector<EnumMemberDesc>> map_enums;
};
EnumTable& Enums() {
  static EnumTable table;
  return table;
}

}  // namespace

bool TryParseInt32Invariant(std::string_view sv, std::int32_t& int4_out) {
  return TryParseIntegerNet(sv, int4_out);
}

bool TryParseUInt16Invariant(std::string_view sv, std::uint16_t& uint2_out) {
  return TryParseIntegerNet(sv, uint2_out);
}

bool TryParseInt16Invariant(std::string_view sv, std::int16_t& int2_out) {
  return TryParseIntegerNet(sv, int2_out);
}

bool TryParseByteInvariant(std::string_view sv, std::uint8_t& uint1_out) {
  return TryParseIntegerNet(sv, uint1_out);
}

bool TryParseFloatOrPercentInvariant(std::string_view sv, float& fp4_out) {
  std::string_view sv_raw{sv};
  float fp4_mult{1.0f};
  if (sv.find('%') != std::string_view::npos) {
    // C# Replace("%",""):去掉全部百分号(解析缓存串)
    // C# Replace("%", ""): strip every percent sign (over a cached string).
    static thread_local std::string str_buf;
    str_buf.clear();
    for (const char chr_c : sv)
      if (chr_c != '%')
        str_buf += chr_c;
    sv_raw = str_buf;
    fp4_mult = 0.01f;
  }

  sv_raw = TrimNetWhiteSpace(sv_raw);
  bool b_negative{false};
  if (!sv_raw.empty() && (sv_raw.front() == '+' || sv_raw.front() == '-')) {
    b_negative = sv_raw.front() == '-';
    sv_raw.remove_prefix(1);
  }
  if (sv_raw.empty())
    return false;

  float fp4_v{};
  // chars_format::general = C# NumberStyles.Float 的主体形态
  // chars_format::general matches the bulk of C# NumberStyles.Float.
  auto [ptr_end, ec_err] = std::from_chars(sv_raw.data(), sv_raw.data() + sv_raw.size(), fp4_v);
  if (ec_err != std::errc{} || ptr_end != sv_raw.data() + sv_raw.size())
    return false;

  if (b_negative)
    fp4_v = -fp4_v;

  // 非有限值收紧:C# 仅接受字面 "Infinity"/"-Infinity"/"NaN"(区分大小写);
  // from_chars 的 inf/nan 宽松集在此复核(上游数据不可达,双保险)
  // Non-finite tightening: C# accepts only the case-sensitive literals
  // "Infinity"/"-Infinity"/"NaN"; the looser from_chars inf/nan set is
  // re-validated here (unreachable with upstream data, belt-and-braces).
  if (!std::isfinite(fp4_v)) {
    const std::string_view sv_trimmed = TrimNetWhiteSpace(sv);
    const bool b_ok = sv_trimmed == "Infinity" || sv_trimmed == "-Infinity" || sv_trimmed == "NaN";
    if (!b_ok)
      return false;
  }

  fp4_out = fp4_v * fp4_mult;
  return true;
}

bool TryParseBoolNet(std::string_view sv, bool& b_out) {
  sv = TrimNetWhiteSpace(sv);
  if (EqualsIgnoreCase(sv, "true")) {
    b_out = true;
    return true;
  }
  if (EqualsIgnoreCase(sv, "false")) {
    b_out = false;
    return true;
  }
  return false;
}

std::vector<std::string_view> SplitCommaTrimmed(std::string_view sv) {
  std::vector<std::string_view> vec_parts = SplitComma(sv);
  std::vector<std::string_view> vec_ret;
  for (const std::string_view sv_part : vec_parts) {
    const std::string_view sv_trimmed = TrimNetWhiteSpace(sv_part);
    if (!sv_trimmed.empty())
      vec_ret.push_back(sv_trimmed);
  }
  return vec_ret;
}

std::vector<std::string_view> SplitComma(std::string_view sv) {
  std::vector<std::string_view> vec_ret;
  std::size_t int4_begin{};
  while (true) {
    const std::size_t int4_comma = sv.find(',', int4_begin);
    if (int4_comma == std::string_view::npos) {
      vec_ret.push_back(sv.substr(int4_begin));
      break;
    }
    vec_ret.push_back(sv.substr(int4_begin, int4_comma - int4_begin));
    int4_begin = int4_comma + 1;
  }
  return vec_ret;
}

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

bool TryParseCVecNet(std::string_view sv, CVec& cvec_out) {
  const auto fn_int = [](std::string_view sv_p, std::int32_t& int4_v) {
    return TryParseInt32Invariant(sv_p, int4_v);
  };
  return TryParseTuple2(sv, fn_int, cvec_out);
}

void RegisterEnum(std::string_view enum_full_name, std::span<const EnumMemberDesc> members) {
  Enums().map_enums.insert_or_assign(enum_full_name,
                                     std::vector<EnumMemberDesc>{members.begin(), members.end()});
}

void ClearEnumsForTest() {
  Enums().map_enums.clear();
}

namespace {

/// 单个枚举成员/数值解析(Enum.TryParse 名字分支,ignoreCase)
/// Single member/numeric resolution (the Enum.TryParse name branch, ignoreCase).
const EnumMemberDesc* FindEnumMember(std::span<const EnumMemberDesc> members, std::string_view sv_name) {
  for (const EnumMemberDesc& member : members)
    if (EqualsIgnoreCase(sv_name, member.str_name))
      return &member;
  return nullptr;
}

}  // namespace

bool TryParseEnumNet(std::string_view enum_full_name, std::string_view sv, std::int32_t& int4_out) {
  const auto it_find = Enums().map_enums.find(enum_full_name);
  if (it_find == Enums().map_enums.end())
    return false;
  const std::vector<EnumMemberDesc>& vec_members = it_find->second;

  sv = TrimNetWhiteSpace(sv);

  // 逗号分隔名字列表 → 按位或(.NET Enum.TryParse 对任意枚举接受该形态)
  // Comma-separated name list → bitwise OR (.NET Enum.TryParse accepts this
  // form for any enum type).
  if (sv.find(',') != std::string_view::npos) {
    std::uint32_t uint4_accum{};
    for (const std::string_view sv_part : SplitComma(sv)) {
      std::int32_t int4_part{};
      if (!TryParseEnumNet(enum_full_name, sv_part, int4_part))
        return false;
      uint4_accum |= static_cast<std::uint32_t>(int4_part);
    }
    int4_out = static_cast<std::int32_t>(uint4_accum);
    return true;
  }

  // 名字(不分大小写)
  // Name (case-insensitive).
  if (const EnumMemberDesc* member = FindEnumMember(vec_members, sv); member != nullptr) {
    int4_out = member->int4_value;
    return true;
  }

  // 有符号十进制数值(允许未定义成员;Enum.TryParse 数值分支 = 底层整型解析)
  // Signed decimal numeric (undefined members allowed; the Enum.TryParse
  // numeric branch parses as the underlying integer).
  std::int32_t int4_numeric{};
  if (TryParseInt32Invariant(sv, int4_numeric)) {
    int4_out = int4_numeric;
    return true;
  }

  return false;
}

std::string EnumToStringNet(std::string_view enum_full_name, std::int32_t int4_value) {
  const auto it_find = Enums().map_enums.find(enum_full_name);
  if (it_find == Enums().map_enums.end())
    return std::to_string(int4_value);

  // .NET Enum.ToString(CachedName/ToString):先试整体精确匹配(任意位数的已定义成员,
  // 声明序首个匹配),再走 flags 分解
  // .NET Enum.ToString: first an exact whole-value match (any defined member,
  // first in declaration order), then the flags decomposition.
  const std::vector<EnumMemberDesc>& vec_members = it_find->second;
  for (const EnumMemberDesc& member : vec_members) {
    if (member.int4_value == int4_value)
      return std::string{member.str_name};
  }

  if (int4_value == 0)
    return "0";

  // flags 分解(.NET InternalFlagsFormat):按值升序取"单一已定义值"位;剩余位以
  // 十进制数值输出(.NET Core 3.0+ 语义);名字间 ", "
  // Flags decomposition (.NET InternalFlagsFormat): single-bit defined values
  // in ascending order; leftover bits print as a decimal number (.NET Core 3.0+
  // semantics); names joined with ", ".
  std::vector<const EnumMemberDesc*> vec_sorted;
  for (const EnumMemberDesc& member : vec_members)
    vec_sorted.push_back(&member);
  std::ranges::stable_sort(vec_sorted,
                           [](const EnumMemberDesc* a, const EnumMemberDesc* b) {
                             return static_cast<std::uint32_t>(a->int4_value) <
                                    static_cast<std::uint32_t>(b->int4_value);
                           });

  std::uint32_t uint4_remaining = static_cast<std::uint32_t>(int4_value);
  std::uint32_t uint4_processed{};
  std::string str_ret;
  for (const EnumMemberDesc* member : vec_sorted) {
    const std::uint32_t uint4_v = static_cast<std::uint32_t>(member->int4_value);
    // 仅单一位且被 result 包含且尚未处理(与 .NET 逐步 processed 掩码一致)
    // Only a single bit, contained in the result, not yet processed (matching
    // the step-by-step processed mask of .NET).
    if (uint4_v != 0 && (uint4_v & (uint4_v - 1)) == 0 &&
        (uint4_v & static_cast<std::uint32_t>(int4_value)) == uint4_v &&
        (uint4_v & uint4_processed) == 0) {
      if (!str_ret.empty())
        str_ret += ", ";
      str_ret += member->str_name;
      uint4_processed |= uint4_v;
      uint4_remaining &= ~uint4_v;
    }
  }

  if (uint4_remaining != 0) {
    if (!str_ret.empty())
      str_ret += ", ";
    str_ret += std::to_string(uint4_remaining);
  }

  return str_ret;
}

}  // namespace ora::meta
