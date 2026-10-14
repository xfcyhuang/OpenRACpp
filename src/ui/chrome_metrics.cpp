// UPSTREAM: OpenRA.Game/Widgets/ChromeMetrics.cs @e180e3225(chrome_metrics.hpp
// 的实现;Get<T> 的标量域经 meta GetValue,类型名进错误文本)
// The implementation of chrome_metrics.hpp (Get<T>'s scalar domain through
// meta GetValue; the type names enter the error texts).
import std;

#include "ui/chrome_metrics.hpp"

#include "meta/field_loader.hpp"
#include "yaml/mini_yaml.hpp"

namespace ora::ui {

void ChromeMetrics::Initialize(Deps deps) {
  yaml::StringPool pool_metrics;  // 上游 stringPool | the upstream stringPool

  std::vector<std::vector<yaml::MiniYamlNode>> vec_sources;
  vec_sources.reserve(deps.vec_metric_files->size());
  for (const std::string& str_file : *deps.vec_metric_files) {
    const std::vector<char> vec_bytes = deps.ptr_file_system->Open(str_file);
    vec_sources.push_back(yaml::MiniYaml::FromStream(
        std::string_view{vec_bytes.data(), vec_bytes.size()}, str_file, true, pool_metrics));
  }

  vec_data_.clear();
  for (const yaml::MiniYamlNode& node_metric : yaml::MiniYaml::Merge(std::move(vec_sources)))
    for (const yaml::MiniYamlNode& node_entry : node_metric.Value.Nodes) {
      // data[n.Key] = n.Value.Value(索引写覆盖原位,保持首插序)
      // data[n.Key] = n.Value.Value (an indexer write overwriting in place,
      // keeping the first-insert order)
      const std::string str_key = node_entry.Key != nullptr ? *node_entry.Key : std::string{};
      const std::string str_value = node_entry.Value.Value != nullptr ? *node_entry.Value.Value
                                                                      : std::string{};
      bool b_overwritten = false;
      for (auto& [str_entry_key, str_entry_value] : vec_data_)
        if (str_entry_key == str_key) {
          str_entry_value = str_value;
          b_overwritten = true;
          break;
        }
      if (!b_overwritten)
        vec_data_.emplace_back(str_key, str_value);
    }
}

const std::string& ChromeMetrics::Get(std::string_view str_key) const {
  const std::string* ptr_value = Find(str_key);
  if (ptr_value == nullptr)
    throw std::runtime_error("The given key was not present in the dictionary.");
  return *ptr_value;
}

std::int32_t ChromeMetrics::GetInt32(std::string_view str_key) const {
  return meta::GetInt32Value(str_key, Get(str_key));
}

bool ChromeMetrics::GetBool(std::string_view str_key) const {
  return meta::GetBoolValue(str_key, Get(str_key));
}

core::Color ChromeMetrics::GetColor(std::string_view str_key) const {
  return meta::GetColorValue(str_key, Get(str_key));
}

bool ChromeMetrics::TryGet(std::string_view str_key, std::string& str_result) const {
  const std::string* ptr_value = Find(str_key);
  if (ptr_value == nullptr) {
    // 上游 default 值面(T = string → 空) | upstream's default face
    // (T = string → empty).
    str_result.clear();
    return false;
  }

  str_result = *ptr_value;
  return true;
}

const std::string* ChromeMetrics::Find(std::string_view str_key) const {
  for (const auto& [str_entry_key, str_entry_value] : vec_data_)
    if (str_entry_key == str_key)
      return &str_entry_value;
  return nullptr;
}

}  // namespace ora::ui
