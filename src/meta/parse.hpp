#pragma once
import std;

namespace ora::meta {

bool TryParseInt32Invariant(std::string_view sv, std::int32_t& int4_out);

bool TryParseUInt16Invariant(std::string_view sv, std::uint16_t& uint2_out);

bool TryParseInt16Invariant(std::string_view sv, std::int16_t& int2_out);

bool TryParseByteInvariant(std::string_view sv, std::uint8_t& uint1_out);

bool TryParseFloatOrPercentInvariant(std::string_view sv, float& fp4_out);

bool TryParseBoolNet(std::string_view sv, bool& b_out);

std::vector<std::string_view> SplitCommaTrimmed(std::string_view sv);

std::vector<std::string_view> SplitComma(std::string_view sv);

}

namespace ora {
struct CPos;

struct CVec;
}

namespace ora::meta {

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

bool TryParseCPosNet(std::string_view sv, CPos& cpos_out);

bool TryParseCVecNet(std::string_view sv, CVec& cvec_out);

bool TryParseEnumNet(std::string_view enum_full_name, std::string_view sv, std::int32_t& int4_out);

std::string EnumToStringNet(std::string_view enum_full_name, std::int32_t int4_value);

struct EnumMemberDesc {
  std::string_view str_name;
  std::int32_t int4_value;
};
void RegisterEnum(std::string_view enum_full_name, std::span<const EnumMemberDesc> members);

void ClearEnumsForTest();

}
