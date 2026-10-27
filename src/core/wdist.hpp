#pragma once
import std;
#include <cassert>

#include "mersenne_twister.hpp"

namespace ora {

struct WDist {
  std::int32_t Length{0};

  constexpr WDist() = default;
  constexpr explicit WDist(std::int32_t int4_r) : Length{int4_r} {}

  static constexpr WDist Zero() { return WDist{0}; }
  static constexpr WDist MaxValue() { return WDist{std::numeric_limits<std::int32_t>::max()}; }
  static constexpr WDist FromCells(std::int32_t int4_cells) { return WDist{1024 * int4_cells}; }

  constexpr std::int64_t LengthSquared() const {
    return static_cast<std::int64_t>(Length) * Length;
  }

  static constexpr WDist FromPDF(MersenneTwister& rng_r, std::int32_t int4_samples) {
    std::int32_t int4_sum{0};
    for (std::int32_t int4_i{0}; int4_i < int4_samples; int4_i++)
      int4_sum += rng_r.Next(-1024, 1024);
    return WDist{int4_sum / int4_samples};
  }

  static std::expected<WDist, bool> TryParse(std::string_view str_s) {
    if (str_s.empty())
      return std::unexpected{false};

    const std::size_t size_split{str_s.find_first_of("cC")};
    std::int32_t int4_cell{0};
    std::int32_t int4_subcell{0};

    if (size_split == std::string_view::npos) {

      if (!ParseInt(str_s, int4_subcell))
        return std::unexpected{false};
    } else {

      if (!ParseInt(str_s.substr(0, size_split), int4_cell))
        return std::unexpected{false};
      if (!ParseInt(str_s.substr(size_split + 1), int4_subcell))
        return std::unexpected{false};
    }

    if (int4_cell < 0)
      int4_subcell = -int4_subcell;

    return WDist{1024 * int4_cell + int4_subcell};
  }

  constexpr std::int32_t CompareTo(WDist other) const {
    return Length < other.Length ? -1 : (Length == other.Length ? 0 : 1);
  }

  std::string ToString() const {
    const std::int32_t int4_abs{Length < 0 ? -Length : Length};
    std::string str_out{Length < 0 ? "-" : ""};
    str_out += std::format("{}c{}", int4_abs / 1024, int4_abs % 1024);
    return str_out;
  }

  constexpr std::int32_t Hash() const { return Length; }

  friend constexpr WDist operator+(WDist d_a, WDist d_b) { return WDist{d_a.Length + d_b.Length}; }
  friend constexpr WDist operator-(WDist d_a, WDist d_b) { return WDist{d_a.Length - d_b.Length}; }
  friend constexpr WDist operator-(WDist d_a) { return WDist{-d_a.Length}; }
  friend constexpr WDist operator/(WDist d_a, std::int32_t int4_b) { return WDist{d_a.Length / int4_b}; }
  friend constexpr WDist operator*(WDist d_a, std::int32_t int4_b) { return WDist{d_a.Length * int4_b}; }
  friend constexpr WDist operator*(std::int32_t int4_a, WDist d_b) { return WDist{int4_a * d_b.Length}; }
  friend constexpr bool operator<(WDist d_a, WDist d_b) { return d_a.Length < d_b.Length; }
  friend constexpr bool operator>(WDist d_a, WDist d_b) { return d_a.Length > d_b.Length; }
  friend constexpr bool operator<=(WDist d_a, WDist d_b) { return d_a.Length <= d_b.Length; }
  friend constexpr bool operator>=(WDist d_a, WDist d_b) { return d_a.Length >= d_b.Length; }
  friend constexpr bool operator==(WDist d_a, WDist d_b) { return d_a.Length == d_b.Length; }
  friend constexpr bool operator!=(WDist d_a, WDist d_b) { return !(d_a == d_b); }

 private:

  static bool ParseInt(std::string_view str_s, std::int32_t& int4_out) {
    if (str_s.empty())
      return false;
    const char* ptr_first{str_s.data()};
    const char* ptr_last{str_s.data() + str_s.size()};
    const auto [ptr_next, err_ec]{std::from_chars(ptr_first, ptr_last, int4_out)};
    return err_ec == std::errc{} && ptr_next == ptr_last;
  }
};

}
