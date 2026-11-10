module;

#include <cstdio>

module ora.test.util;

import std;

namespace ora::test {

TestStats& test_stats() {
  static TestStats s_stats;
  return s_stats;
}

void check(bool b_ok, std::string_view str_what) {
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

int test_exit_code(std::string_view str_suite) {
  TestStats& s{test_stats()};
  std::fflush(nullptr);
  if (s.int4_failures != 0) {
    std::println("{}: {} of {} checks failed", str_suite, s.int4_failures, s.int4_checks);
    return 1;
  }
  std::println("{}: all {} checks passed", str_suite, s.int4_checks);
  return 0;
}

std::optional<std::string> read_text_file(const std::string& str_path) {
  std::ifstream fs_file{str_path, std::ios::binary};
  if (!fs_file.is_open()) return std::nullopt;
  std::string str_out;
  char buf_chars[4096];
  while (fs_file.read(buf_chars, sizeof(buf_chars)) || fs_file.gcount() > 0)
    str_out.append(buf_chars, static_cast<std::size_t>(fs_file.gcount()));
  return str_out;
}

}
