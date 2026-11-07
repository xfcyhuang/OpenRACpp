#pragma once

import std;

#include <cstdio>

namespace ora::test {

struct TestStats {
  int int4_checks{};
  int int4_failures{};
  int int4_reports{};
};

constexpr int kMaxReports{20};

inline TestStats& test_stats() {
  static TestStats s_stats;
  return s_stats;
}

inline void check(bool b_ok, std::string_view str_what) {
  TestStats& s{test_stats()};
  ++s.int4_checks;
  if (!b_ok) {
    ++s.int4_failures;
    if (s.int4_reports < kMaxReports) {
      ++s.int4_reports;
      std::println("check failed: {}", str_what);
    }
  }
}

inline int test_exit_code(std::string_view str_suite) {
  TestStats& s{test_stats()};
  std::fflush(nullptr);
  if (s.int4_failures != 0) {
    std::println("{}: {} of {} checks failed", str_suite, s.int4_failures, s.int4_checks);
    return 1;
  }
  std::println("{}: all {} checks passed", str_suite, s.int4_checks);
  return 0;
}

}
