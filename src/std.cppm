module;

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <optional>
#include <print>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

export module std;

export namespace std {
using std::array;
using std::byte;
using std::expected;
using std::hash;
using std::int16_t;
using std::int32_t;
using std::int64_t;
using std::int8_t;
using std::optional;
using std::pair;
using std::print;
using std::println;
using std::size_t;
using std::span;
using std::string;
using std::string_view;
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
