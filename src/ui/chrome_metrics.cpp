// UPSTREAM: OpenRA.Game/Widgets/ChromeMetrics.cs @b6fc03f(chrome_metrics.hpp
// 的实现)
// The implementation of chrome_metrics.hpp.
import std;

#include "ui/chrome_metrics.hpp"

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
  for (const auto& [str_entry_key, str_entry_value] : vec_data_)
    if (str_entry_key == str_key)
      return str_entry_value;

  throw std::runtime_error("The given key was not present in the dictionary.");
}

bool ChromeMetrics::TryGet(std::string_view str_key, std::string& str_result) const {
  for (const auto& [str_entry_key, str_entry_value] : vec_data_)
    if (str_entry_key == str_key) {
      str_result = str_entry_value;
      return true;
    }

  // 上游 default 值面(T = string → 空) | upstream's default face
  // (T = string → empty).
  str_result.clear();
  return false;
}

}  // namespace ora::ui
