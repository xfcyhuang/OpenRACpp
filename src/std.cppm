module;

#include <algorithm>
#include <array>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <fstream>
#include <ios>
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
using std::sort;
using std::byte;
using std::expected;
using std::ifstream;
using std::hash;
using std::ios;
using std::streampos;
using std::streamsize;
using std::to_chars;
using std::int16_t;
using std::int32_t;
using std::int64_t;
using std::int8_t;
using std::optional;
using std::unexpected;
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
using std::move;

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
using std::operator==;
using std::operator!=;
using std::operator<;
using std::operator<=;
using std::operator>;
using std::operator>=;
using std::operator<<;
using std::operator>>;
}
