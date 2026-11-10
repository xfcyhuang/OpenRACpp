export module ora.test.util;

import std;

export {
namespace ora::test {

struct TestStats {
  int int4_checks{};
  int int4_failures{};
  int int4_reports{};
};

constexpr int kMaxReports{20};

TestStats& test_stats();

void check(bool b_ok, std::string_view str_what);

int test_exit_code(std::string_view str_suite);

std::optional<std::string> read_text_file(const std::string& str_path);

}
}
