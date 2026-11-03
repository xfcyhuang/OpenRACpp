module;

#include <array>
#include <bit>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cmath>
#include <expected>
#include <format>
#include <fstream>
#include <limits>
#include <map>
#include <optional>
#include <print>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

export module std;

export namespace std {
using std::abort;
using std::array;
using std::bit_cast;
using std::byte;
using std::errc;
using std::expected;
using std::fabs;
using std::format;
using std::from_chars;
using std::from_chars_result;
using std::getline;
using std::hash;
using std::ifstream;
using std::int16_t;
using std::int32_t;
using std::int64_t;
using std::int8_t;
using std::map;
using std::nullopt;
using std::nullopt_t;
using std::optional;
using std::pair;
using std::print;
using std::println;
using std::set;
using std::size_t;
using std::span;
using std::string;
using std::string_view;
using std::to_chars;
using std::to_chars_result;
using std::uint16_t;
using std::uint32_t;
using std::uint64_t;
using std::uint8_t;
using std::vector;

using std::begin;
using std::data;
using std::empty;
using std::end;
using std::size;
using std::swap;

using std::operator==;
using std::operator!=;
using std::operator<;
using std::operator<=;
using std::operator>;
using std::operator>=;
using std::operator<<;
using std::operator>>;
}
