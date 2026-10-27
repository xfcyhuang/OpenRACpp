import std;
#include "parse.hpp"
#include "core/cell_pos.hpp"
#include "core/cvec.hpp"
#include "core/text.hpp"

namespace ora::meta {

namespace {

template <class T>
  requires(std::integral<T>)
bool TryParseIntegerNet(std::string_view sv, T& out) {
  sv = TrimNetWhiteSpace(sv);
  bool b_negative{false};
  if (!sv.empty() && (sv.front() == '+' || sv.front() == '-')) {
    b_negative = sv.front() == '-';
    sv.remove_prefix(1);
  }

  if (sv.empty())
    return false;

  using Mag = std::conditional_t<std::signed_integral<T>, std::uint64_t, T>;
  Mag mag{};
  auto [ptr_end, ec_err] = std::from_chars(sv.data(), sv.data() + sv.size(), mag, 10);
  if (ec_err != std::errc{} || ptr_end != sv.data() + sv.size())
    return false;

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

struct EnumTable {

  std::unordered_map<std::string_view, std::vector<EnumMemberDesc>> map_enums;
};
EnumTable& Enums() {
  static EnumTable table;
  return table;
}

}

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

  auto [ptr_end, ec_err] = std::from_chars(sv_raw.data(), sv_raw.data() + sv_raw.size(), fp4_v);
  if (ec_err != std::errc{} || ptr_end != sv_raw.data() + sv_raw.size())
    return false;

  if (b_negative)
    fp4_v = -fp4_v;

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

const EnumMemberDesc* FindEnumMember(std::span<const EnumMemberDesc> members, std::string_view sv_name) {
  for (const EnumMemberDesc& member : members)
    if (EqualsIgnoreCase(sv_name, member.str_name))
      return &member;
  return nullptr;
}

}

bool TryParseEnumNet(std::string_view enum_full_name, std::string_view sv, std::int32_t& int4_out) {
  const auto it_find = Enums().map_enums.find(enum_full_name);
  if (it_find == Enums().map_enums.end())
    return false;
  const std::vector<EnumMemberDesc>& vec_members = it_find->second;

  sv = TrimNetWhiteSpace(sv);

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

  if (const EnumMemberDesc* member = FindEnumMember(vec_members, sv); member != nullptr) {
    int4_out = member->int4_value;
    return true;
  }

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

  const std::vector<EnumMemberDesc>& vec_members = it_find->second;
  for (const EnumMemberDesc& member : vec_members) {
    if (member.int4_value == int4_value)
      return std::string{member.str_name};
  }

  if (int4_value == 0)
    return "0";

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

}
