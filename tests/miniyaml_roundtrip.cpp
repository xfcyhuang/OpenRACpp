import std;


import ora.game.miniyaml;
import ora.test.util;
#ifndef ORA_MINIYAML_GOLDEN_DIR
#define ORA_MINIYAML_GOLDEN_DIR "tests/golden/miniyaml"
#endif

namespace {



void fail_line(std::string_view str_what) {
  ora::test::check(false, str_what);
}

std::string read_file_bytes(const std::filesystem::path& path_file) {
  std::ifstream fs_file{path_file, std::ios::binary};
  if (!fs_file) return {};
  std::string str_data;
  fs_file.seekg(0, std::ios::end);
  const std::streampos pos_end{fs_file.tellg()};
  str_data.resize(static_cast<std::size_t>(pos_end));
  fs_file.seekg(0, std::ios::beg);
  fs_file.read(str_data.data(), static_cast<std::streamsize>(str_data.size()));
  return str_data;
}

std::string decimal(unsigned long long u8_value) {
  char buf[24];
  const auto res_chars{std::to_chars(buf, buf + sizeof(buf), u8_value)};
  return std::string{buf, res_chars.ptr};
}

std::string describe_first_diff(std::string_view str_want, std::string_view str_got) {
  const std::size_t sz_min{str_want.size() < str_got.size() ? str_want.size() : str_got.size()};
  std::size_t idx{0};
  while (idx < sz_min && str_want[idx] == str_got[idx]) ++idx;
  std::string str_detail{"first diff at byte "};
  str_detail += decimal(idx);
  if (idx >= sz_min) {
    str_detail += " (length differs): want ";
    str_detail += decimal(str_want.size());
    str_detail += " got ";
    str_detail += decimal(str_got.size());
    return str_detail;
  }
  char buf_want[8];
  char buf_got[8];
  const auto res_want{std::to_chars(buf_want, buf_want + sizeof(buf_want),
                                    static_cast<unsigned>(static_cast<unsigned char>(str_want[idx])), 16)};
  const auto res_got{std::to_chars(buf_got, buf_got + sizeof(buf_got),
                                   static_cast<unsigned>(static_cast<unsigned char>(str_got[idx])), 16)};
  str_detail += ": want 0x";
  str_detail.append(buf_want, res_want.ptr);
  str_detail += " got 0x";
  str_detail.append(buf_got, res_got.ptr);
  return str_detail;
}

}

int main() {
  std::vector<std::string> vec_files;
  for (const auto& entry :
       std::filesystem::recursive_directory_iterator{std::filesystem::path{ORA_MINIYAML_GOLDEN_DIR}}) {
    if (!entry.is_regular_file()) continue;
    const std::string str_name{entry.path().filename().string()};
    if (str_name.size() > 5 && str_name.substr(str_name.size() - 5) == ".yaml")
      vec_files.push_back(entry.path().string());
  }
  std::sort(vec_files.begin(), vec_files.end());
  if (vec_files.empty()) {
    std::println("miniyaml_roundtrip: no golden files under {}", ORA_MINIYAML_GOLDEN_DIR);
    return 1;
  }

  int int4_identical{0};
  int int4_normalized{0};
  for (const std::string& str_path : vec_files) {
    const std::string str_input{read_file_bytes(std::filesystem::path{str_path})};
    const std::string str_expected_path{str_path.substr(0, str_path.size() - 5) + ".expected"};
    const std::string str_expected{std::filesystem::exists(std::filesystem::path{str_expected_path})
                                       ? read_file_bytes(std::filesystem::path{str_expected_path})
                                       : str_input};

    auto res_nodes = ora::game::miniyaml_from_file(str_path, false);
    if (!res_nodes.has_value()) {
      fail_line(str_path + ": parse error: " + res_nodes.error());
      continue;
    }
    const std::string str_out{ora::game::miniyaml_write_to_string(*res_nodes)};
    if (str_out == str_expected) {
      if (str_input == str_expected)
        ++int4_identical;
      else
        ++int4_normalized;
    } else {
      fail_line(str_path + ": " + describe_first_diff(str_expected, str_out));
    }
  }

  std::println("miniyaml_roundtrip: {} identity, {} oracle-normalized, {} failed of {} files",
               int4_identical, int4_normalized, ora::test::test_stats().int4_failures,
                vec_files.size());
  if (ora::test::test_stats().int4_failures != 0) {
    std::println("miniyaml_roundtrip: {} files failed",
                 ora::test::test_stats().int4_failures);
    return 1;
  }
  std::println("miniyaml_roundtrip: all {} files byte-identical with oracle", vec_files.size());
  return 0;
}
