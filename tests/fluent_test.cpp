// UPSTREAM: OpenRA.Game/FluentProvider.cs + FluentBundle.cs @1f594e320
// (Phase 6 第一批验收:golden_fluent 对拍门 + 单元面)。
// 重放器与 golden_gen FluentGoldenMode 同一行扫描/同一探针表;微用例集
// 逐字节同文嵌入。oracle = Linguini 0.8.1(上游 FluentBundle 的依赖)。
// The Phase 6 batch-one acceptance: the golden_fluent differential gate +
// the unit faces. The replayer shares the same line scan/probe table with
// golden_gen's FluentGoldenMode; the micro-case set is embedded
// byte-identically. The oracle = Linguini 0.8.1 (the upstream FluentBundle
// dependency).
import std;

#include "fluent/fluent_bundle.hpp"
#include "fluent/fluent_provider.hpp"
#include "yaml/mini_yaml.hpp"

namespace {

using namespace ora::fluent;

int int4_checks_failed = 0;

void Check(bool b_condition, std::string_view str_what) {
  if (!b_condition) {
    std::println("FAIL: {}", str_what);
    ++int4_checks_failed;
  }
}

std::string EscapeOut(std::string_view sv_text) {
  std::string str_out;
  for (const char ch : sv_text) {
    if (ch == '\\')
      str_out += "\\\\";
    else if (ch == '\n')
      str_out += "\\n";
    else if (ch == '\r')
      str_out += "\\r";
    else if (ch == '\t')
      str_out += "\\t";
    else
      str_out.push_back(ch);
  }
  return str_out;
}

std::optional<std::size_t> MatchMessageDef(std::string_view sv_line) {
  if (sv_line.empty() || !(std::isalpha(static_cast<unsigned char>(sv_line[0])) != 0))
    return std::nullopt;
  std::size_t uint_i = 1;
  while (uint_i < sv_line.size()) {
    const char ch = sv_line[uint_i];
    if (std::isalnum(static_cast<unsigned char>(ch)) != 0 || ch == '_' || ch == '-')
      ++uint_i;
    else
      break;
  }
  std::size_t uint_j = uint_i;
  while (uint_j < sv_line.size() && (sv_line[uint_j] == ' ' || sv_line[uint_j] == '\t'))
    ++uint_j;
  if (uint_j < sv_line.size() && sv_line[uint_j] == '=')
    return uint_i;  // 标识符终点 = 键长 | the identifier end = the key length
  return std::nullopt;
}

std::optional<std::size_t> MatchAttributeDef(std::string_view sv_line) {
  std::size_t uint_i = 0;
  while (uint_i < sv_line.size() && (sv_line[uint_i] == ' ' || sv_line[uint_i] == '\t'))
    ++uint_i;
  if (uint_i == 0 || uint_i >= sv_line.size() || sv_line[uint_i] != '.')
    return std::nullopt;
  ++uint_i;
  const std::size_t uint_start = uint_i;
  if (uint_i >= sv_line.size() ||
      !(std::isalpha(static_cast<unsigned char>(sv_line[uint_i])) != 0))
    return std::nullopt;
  while (uint_i < sv_line.size()) {
    const char ch = sv_line[uint_i];
    if (std::isalnum(static_cast<unsigned char>(ch)) != 0 || ch == '_' || ch == '-')
      ++uint_i;
    else
      break;
  }
  std::size_t uint_j = uint_i;
  while (uint_j < sv_line.size() && (sv_line[uint_j] == ' ' || sv_line[uint_j] == '\t'))
    ++uint_j;
  if (uint_j < sv_line.size() && sv_line[uint_j] == '=')
    return uint_i - uint_start;  // 属性名长 | the attribute-name length
  return std::nullopt;
}

std::vector<std::string_view> MatchVariables(std::string_view sv_line) {
  std::vector<std::string_view> vec_out;
  std::size_t uint_i = 0;
  while (true) {
    const std::size_t uint_brace = sv_line.find('{', uint_i);
    if (uint_brace == std::string_view::npos)
      break;
    std::size_t uint_j = uint_brace + 1;
    while (uint_j < sv_line.size() && (sv_line[uint_j] == ' ' || sv_line[uint_j] == '\t'))
      ++uint_j;
    if (uint_j < sv_line.size() && sv_line[uint_j] == '$') {
      ++uint_j;
      const std::size_t uint_start = uint_j;
      if (uint_j < sv_line.size() &&
          std::isalpha(static_cast<unsigned char>(sv_line[uint_j])) != 0) {
        ++uint_j;
        while (uint_j < sv_line.size()) {
          const char ch = sv_line[uint_j];
          if (std::isalnum(static_cast<unsigned char>(ch)) != 0 || ch == '_' || ch == '-')
            ++uint_j;
          else
            break;
        }
        vec_out.emplace_back(sv_line.substr(uint_start, uint_j - uint_start));
      }
    }
    uint_i = uint_brace + 1;
  }
  return vec_out;
}

struct QueryKey {
  std::string str_key;
  std::vector<std::string> vec_vars;
};

std::vector<QueryKey> EnumerateKeys(std::string_view sv_text) {
  std::vector<QueryKey> vec_keys;
  std::vector<std::string_view> vec_lines;
  {
    std::size_t uint_start = 0;
    while (uint_start <= sv_text.size()) {
      const std::size_t uint_eol = sv_text.find('\n', uint_start);
      const std::size_t uint_end =
          uint_eol == std::string_view::npos ? sv_text.size() : uint_eol;
      std::string_view sv_line = sv_text.substr(uint_start, uint_end - uint_start);
      if (!sv_line.empty() && sv_line.back() == '\r')
        sv_line.remove_suffix(1);
      vec_lines.push_back(sv_line);
      if (uint_eol == std::string_view::npos)
        break;
      uint_start = uint_eol + 1;
    }
  }

  std::optional<std::string> opt_current;
  std::vector<std::string> vec_vars;
  const auto flush = [&]() {
    if (!opt_current.has_value())
      return;
    std::vector<std::string> vec_distinct;
    for (const std::string& str_var : vec_vars)
      if (std::ranges::find(vec_distinct, str_var) == vec_distinct.end())
        vec_distinct.push_back(str_var);
    vec_keys.push_back(QueryKey{*opt_current, std::move(vec_distinct)});
    opt_current.reset();
    vec_vars.clear();
  };

  for (const std::string_view sv_line : vec_lines) {
    if (const std::optional<std::size_t> uint_len = MatchMessageDef(sv_line)) {
      flush();
      opt_current = std::string{sv_line.substr(0, *uint_len)};
    } else if (const std::optional<std::size_t> uint_len_attr =
                   MatchAttributeDef(sv_line);
               opt_current.has_value() && uint_len_attr.has_value()) {
      std::string str_prefix = *opt_current;
      const std::size_t uint_dot = str_prefix.find('.');
      if (uint_dot != std::string::npos)
        str_prefix.erase(uint_dot);
      flush();
      const std::size_t uint_i = sv_line.find('.') + 1;
      opt_current = str_prefix + "." +
                    std::string{sv_line.substr(uint_i, *uint_len_attr)};
    }
    if (!opt_current.has_value())
      continue;
    for (const std::string_view sv_var : MatchVariables(sv_line))
      vec_vars.emplace_back(sv_var);
  }
  flush();
  return vec_keys;
}

constexpr std::string_view kProbes[] = {"i1", "i0", "i42", "i11", "i-1", "sx",
                                        "sone", "none"};

std::string EvaluateOutcome(const FluentBundle& bundle_ref,
                            const QueryKey& key_query,
                            std::string_view sv_probe) {
  std::vector<std::pair<std::string, FluentValue>> vec_args;
  if (sv_probe != "-" && sv_probe != "none" && !key_query.vec_vars.empty()) {
    FluentValue val_value{std::variant<std::int64_t, std::string>{
        std::string{sv_probe.substr(1)}}};
    if (sv_probe.front() == 'i') {
      std::int64_t int8_value = 0;
      const std::string_view sv_digits = sv_probe.substr(1);
      const bool b_negative = !sv_digits.empty() && sv_digits.front() == '-';
      for (std::size_t uint_k = b_negative ? 1 : 0; uint_k < sv_digits.size();
           ++uint_k)
        int8_value = int8_value * 10 + (sv_digits[uint_k] - '0');
      val_value.val = b_negative ? -int8_value : int8_value;
    }
    for (const std::string& str_var : key_query.vec_vars)
      vec_args.emplace_back(str_var, val_value);
  }

  if (!bundle_ref.HasAttrMessage(key_query.str_key))
    return "MISS";
  std::string str_value;
  if (!bundle_ref.TryGetMessage(key_query.str_key, vec_args, str_value))
    return "FALSE";
  return "=" + EscapeOut(str_value);
}

std::vector<std::string> ReplayActual(const std::string& str_upstream_root) {
  namespace stdfs = std::filesystem;
  std::vector<std::string> vec_lines;
  const std::string str_mods_root = str_upstream_root + "/mods";

  std::vector<std::string> vec_mod_dirs;
  std::error_code ec;
  for (const auto& entry : stdfs::directory_iterator{str_mods_root, ec}) {
    if (!entry.is_directory())
      continue;
    vec_mod_dirs.push_back(entry.path().filename().string());
  }
  std::ranges::sort(vec_mod_dirs);

  for (const std::string& str_mod : vec_mod_dirs) {
    const std::string str_mod_dir = str_mods_root + "/" + str_mod;
    const std::string str_mod_yaml = str_mod_dir + "/mod.yaml";
    if (!stdfs::exists(str_mod_yaml))
      continue;

    const auto read_lines = [](const std::string& str_path) {
      std::vector<std::string> vec_out;
      std::ifstream stream_in{str_path, std::ios::binary};
      std::string str_line;
      while (std::getline(stream_in, str_line)) {
        if (!str_line.empty() && str_line.back() == '\r')
          str_line.pop_back();
        vec_out.push_back(str_line);
      }
      return vec_out;
    };

    const std::vector<std::string> vec_yaml = read_lines(str_mod_yaml);

    std::vector<std::string> vec_files;
    bool b_in_section = false;
    for (const std::string& str_line : vec_yaml) {
      if (str_line.empty() || str_line.front() == '#')
        continue;
      const bool b_indented = str_line.front() == ' ' || str_line.front() == '\t';
      if (!b_indented) {
        std::string str_header = str_line;
        while (!str_header.empty() && (str_header.back() == ' ' || str_header.back() == '\t'))
          str_header.pop_back();
        if (!str_header.empty() && str_header.back() == ':')
          str_header.pop_back();
        while (!str_header.empty() && (str_header.back() == ' ' || str_header.back() == '\t'))
          str_header.pop_back();
        b_in_section = str_header == "FluentMessages";
        continue;
      }
      if (!b_in_section)
        continue;
      std::string str_entry = str_line;
      const std::size_t uint_first = str_entry.find_first_not_of(" \t");
      const std::size_t uint_last = str_entry.find_last_not_of(" \t");
      if (uint_first == std::string::npos)
        continue;
      str_entry = str_entry.substr(uint_first, uint_last - uint_first + 1);
      const std::size_t uint_colon = str_entry.find(':');
      if (uint_colon != std::string::npos)
        str_entry = str_entry.substr(0, uint_colon);
      if (!str_entry.empty())
        vec_files.push_back(str_entry);
    }
    if (vec_files.empty())
      continue;

    std::map<std::string, std::string> map_mounts{{str_mod, str_mod_dir}};
    {
      bool b_in_fs = false;
      for (const std::string& str_line : vec_yaml) {
        if (str_line.empty())
          continue;
        const bool b_indented = str_line.front() == ' ' || str_line.front() == '\t';
        if (!b_indented) {
          b_in_fs = str_line.rfind("FileSystem:") == 0;
          continue;
        }
        if (!b_in_fs)
          continue;
        std::string str_entry = str_line;
        const std::size_t uint_first = str_entry.find_first_not_of(" \t");
        const std::size_t uint_last = str_entry.find_last_not_of(" \t");
        str_entry = str_entry.substr(uint_first, uint_last - uint_first + 1);
        const std::size_t uint_colon = str_entry.rfind(':');
        if (uint_colon == std::string::npos || uint_colon + 1 >= str_entry.size())
          continue;
        std::string str_mount = str_entry.substr(uint_colon + 1);
        const std::size_t uint_mount_first = str_mount.find_first_not_of(" \t");
        const std::size_t uint_mount_last = str_mount.find_last_not_of(" \t");
        if (uint_mount_first == std::string::npos)
          continue;
        str_mount = str_mount.substr(uint_mount_first,
                                     uint_mount_last - uint_mount_first + 1);
        std::string str_path = str_entry.substr(0, uint_colon);
        const std::size_t uint_path_first = str_path.find_first_not_of(" \t");
        const std::size_t uint_path_last = str_path.find_last_not_of(" \t");
        str_path = str_path.substr(uint_path_first, uint_path_last - uint_path_first + 1);
        if (str_path.starts_with('$'))
          map_mounts[str_mount] = str_mod_dir;
        else {
          constexpr std::string_view kEnginePrefix = "^EngineDir|";
          if (str_path.starts_with(kEnginePrefix))
            str_path = str_path.substr(kEnginePrefix.size());
          std::ranges::replace(str_path, '/', '\\');
          map_mounts[str_mount] = str_upstream_root + "/" + str_path;
        }
      }
    }

    const FileOpenFn fn_open =
        [&map_mounts, &str_mods_root](const std::string& str_name)
        -> std::optional<std::vector<char>> {
      std::string str_path = str_name;
      const std::size_t uint_bar = str_name.find('|');
      if (uint_bar != std::string::npos) {
        const std::string str_mount = str_name.substr(0, uint_bar);
        const auto it_mount = map_mounts.find(str_mount);
        str_path = (it_mount != map_mounts.end() ? it_mount->second
                                                 : str_mods_root + "/" + str_mount) +
                   "/" + str_name.substr(uint_bar + 1);
      } else {
        str_path = str_mods_root + "/" + str_name;
      }
      std::ifstream stream_in{str_path, std::ios::binary};
      if (!stream_in)
        return std::nullopt;
      return std::vector<char>{std::istreambuf_iterator<char>{stream_in},
                               std::istreambuf_iterator<char>{}};
    };

    const FluentBundle bundle_mod{"en", vec_files, fn_open};

    for (const std::string& str_file : vec_files) {
      std::optional<std::vector<char>> opt_bytes = fn_open(str_file);
      if (!opt_bytes.has_value())
        continue;
      const std::string_view sv_text{opt_bytes->data(), opt_bytes->size()};
      for (const QueryKey& key_query : EnumerateKeys(sv_text)) {
        if (key_query.vec_vars.empty()) {
          vec_lines.push_back("M " + str_mod + " " + str_file + " " +
                              key_query.str_key + " - " +
                              EvaluateOutcome(bundle_mod, key_query, "-"));
        } else {
          for (const std::string_view sv_probe : kProbes)
            vec_lines.push_back("M " + str_mod + " " + str_file + " " +
                                key_query.str_key + " " + std::string{sv_probe} +
                                " " +
                                EvaluateOutcome(bundle_mod, key_query, sv_probe));
        }
      }
    }
  }
  return vec_lines;
}

constexpr std::string_view kMicroResource = R"FTL(# micro cases(与 C++ fluent_test 同文)
micro-plain = Plain value
micro-inline-var = Hello { $name }!
micro-multiline =
    line one
    line two
micro-blank =
    para one

    para two
micro-placeable-first =
    { $name }
    tail
micro-placeable-last =
    head
    { $name }
micro-attr =
    .tip = Tip text
    .tip2 =
        Tip two
        continued
micro-esc = back\slash and \u0041
micro-lit = pre {"lit\"eral"} post
micro-sel-num =
    { $count ->
        [0] zero-exact
        [3] three-exact
        [one] one-cat
       *[other] other-cat ({ $count })
    }
micro-sel-str =
    { $faction ->
        [soviet] SOV
       *[other] FALLBACK
    }
micro-nested =
    { $count ->
        [one] one with { $name }
       *[other]
            other deep
            lines
    }
micro-only-attrs =
    .a = A
micro-indent-variant =
    value
      extra kept
micro-inline-then-block = top
    cont
micro-trail = alpha 
    beta
micro-mid = head { $name }
    tail
)FTL";

std::vector<std::string> ReplayMicro() {
  std::vector<std::string> vec_lines;
  const FluentBundle bundle_micro{"en", kMicroResource, nullptr};
  for (const QueryKey& key_query : EnumerateKeys(kMicroResource)) {
    if (key_query.vec_vars.empty()) {
      vec_lines.push_back("M micro micro.ftl " + key_query.str_key + " - " +
                          EvaluateOutcome(bundle_micro, key_query, "-"));
    } else {
      for (const std::string_view sv_probe : kProbes)
        vec_lines.push_back("M micro micro.ftl " + key_query.str_key + " " +
                            std::string{sv_probe} + " " +
                            EvaluateOutcome(bundle_micro, key_query, sv_probe));
    }
  }
  return vec_lines;
}

int RunGolden(const std::string& str_golden_path,
              const std::string& str_upstream_root) {
  std::ifstream stream_golden{str_golden_path, std::ios::binary};
  if (!stream_golden) {
    std::println("cannot open {}", str_golden_path);
    return 1;
  }
  std::vector<std::string> vec_expected;
  std::string str_line;
  while (std::getline(stream_golden, str_line)) {
    if (!str_line.empty() && str_line.back() == '\r')
      str_line.pop_back();
    vec_expected.push_back(str_line);
  }

  std::vector<std::string> vec_actual = ReplayActual(str_upstream_root);
  for (std::string& str_line_micro : ReplayMicro())
    vec_actual.push_back(std::move(str_line_micro));

  std::size_t uint_mismatches = 0;
  const std::size_t uint_count = std::max(vec_expected.size(), vec_actual.size());
  for (std::size_t uint_i = 0; uint_i < uint_count; ++uint_i) {
    const std::string& str_expected =
        uint_i < vec_expected.size() ? vec_expected[uint_i] : "<missing>";
    const std::string& str_actual =
        uint_i < vec_actual.size() ? vec_actual[uint_i] : "<missing>";
    if (str_expected != str_actual) {
      if (uint_mismatches < 20)
        std::println("line {}\n  oracle: {}\n  actual: {}", uint_i + 1,
                     str_expected, str_actual);
      ++uint_mismatches;
    }
  }
  std::println("golden_fluent: {} expected / {} actual / {} mismatches",
               vec_expected.size(), vec_actual.size(), uint_mismatches);
  return uint_mismatches == 0 ? 0 : 1;
}

std::string EncodeBase64(std::string_view sv_text);

void RunUnits() {
  // 复数类别(en CLDR)/ plural categories (the en CLDR).
  Check(PluralCategory(1, "en") == "one", "plural 1 = one");
  Check(PluralCategory(0, "en") == "other", "plural 0 = other");
  Check(PluralCategory(2, "en") == "other", "plural 2 = other");
  Check(PluralCategory(11, "en") == "other", "plural 11 = other");
  Check(PluralCategory(21, "en") == "one", "plural 21 = one");
  Check(PluralCategory(111, "en") == "other", "plural 111 = other");
  Check(PluralCategory(-1, "en") == "one", "plural -1 = one");

  Check(FormatNumber(-42) == "-42", "format -42");
  Check(FormatNumber(0) == "0", "format 0");
  Check(FormatNumber(1234567) == "1234567", "format 1234567");

  // Provider:mod 优先 → map → 键回落/ provider: mod first → map → key.
  FluentProvider& provider = FluentProvider::Instance();
  provider.ResetForTest();
  Check(provider.GetMessage("missing-key") == "missing-key",
        "uninitialized provider falls back to the key");

  {
    ora::yaml::StringPool pool_strings;
    std::vector<ora::yaml::MiniYamlNode> vec_nodes;
    ora::yaml::MiniYaml yaml_map;
    yaml_map.Nodes.push_back(ora::yaml::MiniYamlNode{
        pool_strings.Intern("base64"),
        ora::yaml::MiniYaml{pool_strings.Intern(EncodeBase64(
            "shared = MAP\nnum = M{ $v }"))}});
    provider.Initialize("en", std::span<const std::string>{}, nullptr, &yaml_map);
    Check(provider.GetMessage("shared") == "MAP",
          "map-level key when the mod level misses");
    std::string str_message;
    const std::vector<std::pair<std::string, FluentValue>> vec_args{
        {"v", FluentValue{std::variant<std::int64_t, std::string>{std::int64_t{7}}}}};
    Check(provider.TryGetMessage("num", vec_args, str_message) &&
              str_message == "M7",
          "map-level variable");
    Check(provider.GetMessage("nope") == "nope", "key fallback");
    Check(!provider.TryGetModMessage("shared", {}, str_message),
          "TryGetModMessage misses the map-only key");
  }
  provider.ResetForTest();
}

/// 测试侧 base64 编码(map 级 FluentMessageDefinitions 的 base64 子节点)
/// The test-side base64 encoding (the map-level FluentMessageDefinitions'
/// base64 child node).
std::string EncodeBase64(std::string_view sv_text) {
  constexpr std::string_view kDigits =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  std::string str_out;
  std::size_t uint_i = 0;
  while (uint_i + 2 < sv_text.size()) {
    const std::uint32_t uint4_chunk =
        (static_cast<unsigned char>(sv_text[uint_i]) << 16) |
        (static_cast<unsigned char>(sv_text[uint_i + 1]) << 8) |
        static_cast<unsigned char>(sv_text[uint_i + 2]);
    str_out.push_back(kDigits[(uint4_chunk >> 18) & 63]);
    str_out.push_back(kDigits[(uint4_chunk >> 12) & 63]);
    str_out.push_back(kDigits[(uint4_chunk >> 6) & 63]);
    str_out.push_back(kDigits[uint4_chunk & 63]);
    uint_i += 3;
  }
  const std::size_t uint_left = sv_text.size() - uint_i;
  if (uint_left == 1) {
    const std::uint32_t uint4_chunk =
        static_cast<unsigned char>(sv_text[uint_i]) << 16;
    str_out.push_back(kDigits[(uint4_chunk >> 18) & 63]);
    str_out.push_back(kDigits[(uint4_chunk >> 12) & 63]);
    str_out += "==";
  } else if (uint_left == 2) {
    const std::uint32_t uint4_chunk =
        (static_cast<unsigned char>(sv_text[uint_i]) << 16) |
        (static_cast<unsigned char>(sv_text[uint_i + 1]) << 8);
    str_out.push_back(kDigits[(uint4_chunk >> 18) & 63]);
    str_out.push_back(kDigits[(uint4_chunk >> 12) & 63]);
    str_out.push_back(kDigits[(uint4_chunk >> 6) & 63]);
    str_out += "=";
  }
  return str_out;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 3) {
    std::println("usage: fluent_test <golden_fluent.txt> <upstream-root>");
    return 2;
  }
  const int int4_golden = RunGolden(argv[1], argv[2]);
  RunUnits();
  if (int4_golden != 0 || int4_checks_failed != 0) {
    std::println("fluent_test FAILED (golden={} unit-checks={})", int4_golden,
                 int4_checks_failed);
    return 1;
  }
  std::println("fluent_test PASS");
  return 0;
}
