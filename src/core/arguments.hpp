// UPSTREAM: OpenRA.Game/Support/Arguments.cs @1f594e320 L17-36
#pragma once
import std;

namespace ora::core {

struct Arguments {
  std::map<std::string, std::string> map_args{};

  Arguments() = default;

  explicit Arguments(std::span<const std::string> vec_src) {
    for (const std::string& str_s : vec_src) {
      const std::size_t sz_start = str_s.find_first_not_of('=');
      if (sz_start == std::string::npos)
        continue;
      const std::size_t sz_pos = str_s.find('=', sz_start);
      if (sz_pos == std::string::npos)
        continue;
      map_args[str_s.substr(sz_start, sz_pos - sz_start)] =
          str_s.substr(sz_pos + 1);
    }
  }

  explicit Arguments(std::initializer_list<std::string> vec_src)
      : Arguments(std::span<const std::string>{vec_src.begin(), vec_src.end()}) {}

  bool Contains(const std::string& str_key) const {
    return map_args.contains(str_key);
  }

  std::string GetValue(const std::string& str_key,
                       const std::string& str_default) const {
    if (const auto it = map_args.find(str_key); it != map_args.end())
      return it->second;
    return str_default;
  }

  void ReplaceValue(const std::string& str_key, std::string str_value) {
    map_args[str_key] = std::move(str_value);
  }
};

}  // namespace ora::core
