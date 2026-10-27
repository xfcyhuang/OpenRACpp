#pragma once
import std;

#include "core/int2.hpp"
#include "meta/parse.hpp"
#include "yaml/mini_yaml.hpp"

namespace ora::gfx {

namespace {

std::int32_t ParseInt32(std::string_view sv_value) {
  std::int32_t int4_out{};
  if (!meta::TryParseInt32Invariant(sv_value, int4_out))
    throw std::runtime_error(std::format(
        "System.FormatException: The input string '{}' was not in a correct format.", sv_value));
  return int4_out;
}

}

class CursorSequence {
 public:

  CursorSequence(std::string str_name, std::string str_cursor_src, std::string str_palette,
                 const yaml::MiniYaml& yaml_info)
      : str_palette_{std::move(str_palette)}, str_name_{std::move(str_name)},
        str_src_{std::move(str_cursor_src)} {
    const std::vector<std::pair<std::string_view, const yaml::MiniYaml*>> vec_d =
        yaml_info.ToDictionary();
    const auto find_value = [&vec_d](std::string_view sv_key) -> const std::string* {
      for (const auto& [sv_k, yaml_v] : vec_d)
        if (sv_k == sv_key)
          return yaml_v->Value;
      return nullptr;
    };

    if (const std::string* str_value = find_value("X")) {
      std::int32_t int4_x{};
      meta::TryParseInt32Invariant(*str_value, int4_x);
      int2_hotspot_ = int2_hotspot_.WithX(int4_x);
    }

    if (const std::string* str_value = find_value("Y")) {
      std::int32_t int4_y{};
      meta::TryParseInt32Invariant(*str_value, int4_y);
      int2_hotspot_ = int2_hotspot_.WithY(int4_y);
    }

    const std::string* str_start = find_value("Start");
    if (str_start == nullptr)
      throw std::runtime_error(
          "System.Collections.Generic.KeyNotFoundException: The given key was not present in the "
          "dictionary.");
    int4_start_ = ParseInt32(*str_start);

    if (const std::string* str_value = find_value("Length")) {
      opt_int4_length_ = *str_value != "*" ? std::optional<std::int32_t>{
                                                 ParseInt32(*str_value)}
                                           : std::nullopt;
    } else if ((str_value = find_value("End")) != nullptr && *str_value == "*") {

      opt_int4_length_ = *str_value != "*"
                             ? std::optional<std::int32_t>{ParseInt32(*str_value) -
                                                           int4_start_}
                             : std::nullopt;
    } else {
      opt_int4_length_ = std::int32_t{1};
    }
  }

  const std::string& Name() const { return str_name_; }
  const std::string& Src() const { return str_src_; }
  std::int32_t Start() const { return int4_start_; }
  const std::optional<std::int32_t>& Length() const { return opt_int4_length_; }
  const std::string& Palette() const { return str_palette_; }
  const int2& Hotspot() const { return int2_hotspot_; }

 private:
  std::string str_name_;
  std::string str_src_;
  std::int32_t int4_start_ = 0;
  std::optional<std::int32_t> opt_int4_length_;
  std::string str_palette_;
  int2 int2_hotspot_{};
};

}
