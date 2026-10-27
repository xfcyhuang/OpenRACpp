#pragma once
import std;

#include "map/map_cache.hpp"

namespace ora::server {

struct LobbyOptionData {
  std::string Id;
  std::string Name;
  std::string Description;
  std::vector<std::pair<std::string, std::string>> vec_values;
  std::string DefaultValue;
  bool IsLocked = false;
  bool IsVisible = true;
  int DisplayOrder = 0;

  bool IsBooleanOption = false;

  bool HasValue(std::string_view str_value) const {
    for (const auto& [k, v] : vec_values)
      if (k == str_value)
        return true;
    return false;
  }

  const std::string* FindValueLabel(std::string_view str_value) const {
    for (const auto& [k, v] : vec_values)
      if (k == str_value)
        return &v;
    return nullptr;
  }

  std::string Label(std::string_view str_value) const {
    const std::string* label = FindValueLabel(str_value);
    return label ? *label : std::string{};
  }
};

LobbyOptionData MakeLobbyBooleanOption(std::string_view str_id,
                                       std::string_view str_name,
                                       std::string_view str_description,
                                       bool b_visible, int int4_display_order,
                                       bool b_default_value, bool b_locked);

}
