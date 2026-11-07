module;

#include <algorithm>
#include <array>
#include <bit>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <deque>
#include <expected>
#include <filesystem>
#include <format>
#include <fstream>
#include <functional>
#include <ios>
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
using std::abs;
using std::abort;
using std::array;
using std::bit_cast;
using std::clamp;
using std::find;
using std::reverse;
using std::find_if;
using std::greater;
using std::initializer_list;
using std::less;
using std::lower_bound;
using std::max;
using std::min;
using std::numeric_limits;
using std::sort;
using std::sqrt;
using std::stable_sort;
using std::unique;
using std::byte;
using std::deque;
using std::errc;
using std::expected;
using std::fabs;
using std::function;
using std::format;
using std::from_chars;
using std::from_chars_result;
using std::getline;
using std::hash;
using std::ifstream;
using std::ios;
using std::streampos;
using std::streamsize;
using std::map;
using std::nullopt;
using std::nullopt_t;
using std::optional;
using std::unexpected;
using std::pair;
using std::make_pair;
using std::print;
using std::println;
using std::set;
using std::ptrdiff_t;
using std::size_t;
using std::span;
using std::string;
using std::string_view;
using std::to_chars;
using std::to_chars_result;
using std::to_string;
using std::int16_t;
using std::int32_t;
using std::int64_t;
using std::int8_t;
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
using std::move;

namespace chrono {
using std::chrono::system_clock;
using std::chrono::floor;
using std::chrono::days;
using std::chrono::seconds;
using std::chrono::sys_days;
using std::chrono::year_month_day;
using std::chrono::hh_mm_ss;
using std::chrono::operator-;
}

namespace filesystem {
using std::filesystem::path;
using std::filesystem::directory_entry;
using std::filesystem::directory_iterator;
using std::filesystem::recursive_directory_iterator;
using std::filesystem::begin;
using std::filesystem::end;
using std::filesystem::exists;
}
using std::operator+;
using std::operator-;
using std::operator==;
using std::operator!=;
using std::operator<;
using std::operator<=;
using std::operator>;
using std::operator>=;
using std::operator<<;
using std::operator>>;
}
