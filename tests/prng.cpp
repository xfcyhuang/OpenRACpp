import std;

#include "game/support/mersenne_twister.hpp"
#include "test_util.hpp"
using ora::test::check;

#ifndef ORA_PRNG_GOLDEN_DIR
#define ORA_PRNG_GOLDEN_DIR "tests/golden/prng"
#endif

namespace {

namespace ogs = ora::game::support;



std::optional<std::uint32_t> parse_hex_u32(std::string_view str_text) {
  std::uint32_t u4_value{};
  auto res_parse{std::from_chars(str_text.data(), str_text.data() + str_text.size(), u4_value, 16)};
  if (res_parse.ec != std::errc{} || res_parse.ptr != str_text.data() + str_text.size())
    return std::nullopt;
  return u4_value;
}

std::optional<std::int32_t> parse_dec_i32(std::string_view str_text) {
  std::int32_t int4_value{};
  auto res_parse{std::from_chars(str_text.data(), str_text.data() + str_text.size(), int4_value)};
  if (res_parse.ec != std::errc{} || res_parse.ptr != str_text.data() + str_text.size())
    return std::nullopt;
  return int4_value;
}

std::optional<std::uint64_t> parse_hex_u64(std::string_view str_text) {
  std::uint64_t u8_value{};
  auto res_parse{std::from_chars(str_text.data(), str_text.data() + str_text.size(), u8_value, 16)};
  if (res_parse.ec != std::errc{} || res_parse.ptr != str_text.data() + str_text.size())
    return std::nullopt;
  return u8_value;
}

std::vector<std::string_view> split_tokens(std::string_view str_text) {
  std::vector<std::string_view> vec_tokens{};
  std::size_t sz_pos{0};
  while (sz_pos < str_text.size()) {
    std::size_t sz_next{str_text.find(' ', sz_pos)};
    if (sz_next == std::string_view::npos)
      sz_next = str_text.size();
    if (sz_next > sz_pos)
      vec_tokens.push_back(str_text.substr(sz_pos, sz_next - sz_pos));
    sz_pos = sz_next + 1;
  }
  return vec_tokens;
}

void run_seed(int int4_seed) {
  ogs::MersenneTwister mt_fresh{ogs::make_mersenne_twister(int4_seed)};
  check(mt_fresh.int4_last == 0 && mt_fresh.int4_total_count == 0,
    std::format("seed {}: fresh last/total_count", int4_seed));
  ogs::next_uint(mt_fresh);
  check(mt_fresh.int4_total_count == 1, std::format("seed {}: total_count after one next_uint", int4_seed));

  ogs::MersenneTwister mt_shortcut{ogs::make_mersenne_twister(int4_seed)};
  check(ogs::next(mt_shortcut, 5, 5) == 5 && mt_shortcut.int4_total_count == 0,
    std::format("seed {}: next(5,5) zero-draw", int4_seed));
  check(ogs::next(mt_shortcut, 5, 6) == 5 && mt_shortcut.int4_total_count == 0,
    std::format("seed {}: next(5,6) zero-draw", int4_seed));
  check(ogs::next(mt_shortcut, 1) == 0 && mt_shortcut.int4_total_count == 0,
    std::format("seed {}: next(1) zero-draw", int4_seed));
  check(ogs::next(mt_shortcut, 0) == 0 && mt_shortcut.int4_total_count == 0,
    std::format("seed {}: next(0) zero-draw", int4_seed));
  check(ogs::next(mt_shortcut, -2147483647 - 1, 2147483647) == -2147483647 - 1 && mt_shortcut.int4_total_count == 0,
    std::format("seed {}: next(min,max) overflow diff zero-draw", int4_seed));

  std::string str_path{std::format("{}/seed_{}.txt", ORA_PRNG_GOLDEN_DIR, int4_seed)};
  std::ifstream if_golden{str_path};
  if (!if_golden.is_open()) {
    check(false, std::format("open {}", str_path));
    return;
  }

  const std::set<std::string> set_handled{"u32", "next", "u64", "f32", "range", "rangen", "pw", "pw0", "shuffle", "u32_624"};
  const std::set<std::string> set_pending{};
  std::map<std::string, ogs::MersenneTwister> map_rng;
  std::map<std::string, int> map_counts;

  auto rng_for = [&](std::string_view str_kind) -> ogs::MersenneTwister& {
    std::string str_kind_key{str_kind};
    auto it_rng{map_rng.find(str_kind_key)};
    if (it_rng == map_rng.end()) {
      it_rng = map_rng.emplace(str_kind_key, ogs::make_mersenne_twister(int4_seed)).first;
      if (str_kind_key == "u32_624")
        for (std::int32_t int4_i{0}; int4_i < 623; ++int4_i)
          ogs::next_uint(it_rng->second);
    }
    return it_rng->second;
  };

  std::string str_line;
  while (std::getline(if_golden, str_line)) {
    if (!str_line.empty() && str_line.back() == '\r')
      str_line.pop_back();
    if (str_line.empty() || str_line.front() == '#')
      continue;

    std::size_t sz_pos{str_line.find(' ')};
    if (sz_pos == std::string::npos || sz_pos == 0 || sz_pos + 1 == str_line.size()) {
      check(false, std::format("seed {}: malformed line '{}'", int4_seed, str_line));
      continue;
    }
    std::string str_kind{str_line.substr(0, sz_pos)};
    std::string str_value{str_line.substr(sz_pos + 1)};

    if (set_pending.contains(str_kind))
      continue;
    if (!set_handled.contains(str_kind)) {
      check(false, std::format("seed {}: unknown kind '{}'", int4_seed, str_kind));
      continue;
    }

    int int4_index{map_counts[str_kind]++};

    if (str_kind == "u32") {
      auto opt_expected{parse_hex_u32(str_value)};
      std::uint32_t u4_actual{ogs::next_uint(rng_for(str_kind))};
      check(opt_expected == u4_actual,
        std::format("seed {} u32 #{}: expected {:08x}, got {:08x}", int4_seed, int4_index,
          opt_expected.value_or(0), u4_actual));
    } else if (str_kind == "next") {
      auto opt_expected{parse_dec_i32(str_value)};
      std::int32_t int4_actual{ogs::next(rng_for(str_kind))};
      check(opt_expected == int4_actual && int4_actual >= 0,
        std::format("seed {} next #{}: expected {}, got {}", int4_seed, int4_index,
          opt_expected.value_or(-1), int4_actual));
    } else if (str_kind == "u64") {
      auto opt_expected{parse_hex_u64(str_value)};
      std::uint64_t u8_actual{ogs::next_ulong(rng_for(str_kind))};
      check(opt_expected == u8_actual,
        std::format("seed {} u64 #{}: expected {:016x}, got {:016x}", int4_seed, int4_index,
          opt_expected.value_or(0), u8_actual));
    } else if (str_kind == "f32") {
      auto opt_expected{parse_hex_u32(str_value)};
      std::uint32_t u4_actual{std::bit_cast<std::uint32_t>(ogs::next_float(rng_for(str_kind)))};
      check(opt_expected == u4_actual,
        std::format("seed {} f32 #{}: expected {:08x}, got {:08x}", int4_seed, int4_index,
          opt_expected.value_or(0), u4_actual));
    } else if (str_kind == "range") {
      auto opt_expected{parse_dec_i32(str_value)};
      std::int32_t int4_actual{ogs::next(rng_for(str_kind), 2, 10)};
      check(opt_expected == int4_actual,
        std::format("seed {} range #{}: expected {}, got {}", int4_seed, int4_index,
          opt_expected.value_or(-1), int4_actual));
    } else if (str_kind == "rangen") {
      auto opt_expected{parse_dec_i32(str_value)};
      std::int32_t int4_actual{ogs::next(rng_for(str_kind), -5, 5)};
      check(opt_expected == int4_actual,
        std::format("seed {} rangen #{}: expected {}, got {}", int4_seed, int4_index,
          opt_expected.value_or(-100), int4_actual));
    } else if (str_kind == "pw") {
      auto opt_expected{parse_dec_i32(str_value)};
      const std::array<std::int32_t, 3> vec4_weights{1, 3, 6};
      std::int32_t int4_actual{ogs::pick_weighted(rng_for(str_kind), vec4_weights)};
      check(opt_expected == int4_actual,
        std::format("seed {} pw #{}: expected {}, got {}", int4_seed, int4_index,
          opt_expected.value_or(-1), int4_actual));
    } else if (str_kind == "pw0") {
      auto opt_expected{parse_dec_i32(str_value)};
      const std::array<std::int32_t, 3> vec4_weights{0, 0, 0};
      std::int32_t int4_actual{ogs::pick_weighted(rng_for(str_kind), vec4_weights)};
      check(opt_expected == int4_actual,
        std::format("seed {} pw0 #{}: expected {}, got {}", int4_seed, int4_index,
          opt_expected.value_or(-1), int4_actual));
    } else if (str_kind == "shuffle") {
      std::vector<std::string_view> vec_tokens{split_tokens(str_value)};
      std::array<std::int32_t, 5> vec4_items{10, 11, 12, 13, 14};
      std::span<std::int32_t> span_items{vec4_items};
      ogs::shuffle_in_place(rng_for(str_kind), span_items, 0, 5);
      bool b_ok{vec_tokens.size() == 5};
      for (std::size_t sz_i{0}; b_ok && sz_i < 5; ++sz_i) {
        auto opt_expected{parse_dec_i32(vec_tokens[sz_i])};
        if (!opt_expected.has_value() || *opt_expected != vec4_items[sz_i])
          b_ok = false;
      }
      check(b_ok, std::format("seed {} shuffle #{}: expected '{}', got '{}'", int4_seed, int4_index,
        str_value, std::format("{} {} {} {} {}", vec4_items[0], vec4_items[1], vec4_items[2], vec4_items[3], vec4_items[4])));
    } else if (str_kind == "u32_624") {
      std::vector<std::string_view> vec_tokens{split_tokens(str_value)};
      bool b_ok{vec_tokens.size() == 3};
      for (std::size_t sz_i{0}; b_ok && sz_i < 3; ++sz_i) {
        auto opt_expected{parse_hex_u32(vec_tokens[sz_i])};
        std::uint32_t u4_actual{ogs::next_uint(rng_for(str_kind))};
        if (!opt_expected.has_value() || *opt_expected != u4_actual)
          b_ok = false;
      }
      check(b_ok, std::format("seed {}: u32_624 mismatch: '{}'", int4_seed, str_value));
    }
  }

  const std::map<std::string, int> map_expected{
    {"u32", 1000}, {"next", 200}, {"u64", 100}, {"f32", 200}, {"range", 200}, {"rangen", 100},
    {"pw", 100}, {"pw0", 100}, {"shuffle", 100}, {"u32_624", 1}};
  for (const auto& pair_expected : map_expected) {
    int int4_count{map_counts.contains(pair_expected.first) ? map_counts.at(pair_expected.first) : 0};
    check(int4_count == pair_expected.second,
      std::format("seed {}: kind {} count {} != {}", int4_seed, pair_expected.first, int4_count,
        pair_expected.second));
  }
}

}

int main() {
  const std::array<std::int32_t, 7> vec4_seeds{0, 1, 42, -1, -2147483647 - 1, 2147483647, 20261103};
  for (std::int32_t int4_seed : vec4_seeds)
    run_seed(int4_seed);

  return ora::test::test_exit_code("prng");
}
