#pragma once
import std;

#include "core/color.hpp"
#include "core/int2.hpp"
#include "fs/file_system.hpp"

namespace ora::ui {

class ChromeMetrics {
 public:
  struct Deps {
    fs::FileSystem* ptr_file_system = nullptr;
    const std::vector<std::string>* vec_metric_files = nullptr;
  };

  void Initialize(Deps deps);

  const std::string& Get(std::string_view str_key) const;
  std::int32_t GetInt32(std::string_view str_key) const;
  bool GetBool(std::string_view str_key) const;
  core::Color GetColor(std::string_view str_key) const;
  int2 GetInt2(std::string_view str_key) const;
  core::Vector2 GetVector2(std::string_view str_key) const;

  bool TryGet(std::string_view str_key, std::string& str_result) const;
  bool TryGetColor(std::string_view str_key, core::Color& color_result) const;
  bool TryGetVector2(std::string_view str_key, core::Vector2& vec_result) const;

 private:
  const std::string* Find(std::string_view str_key) const;

  std::vector<std::pair<std::string, std::string>> vec_data_;
};

}
