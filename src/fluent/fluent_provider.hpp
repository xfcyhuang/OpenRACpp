#pragma once
import std;

#ifdef GetMessage
#undef GetMessage
#endif

#include "fluent/fluent_bundle.hpp"

namespace ora::yaml {
class MiniYaml;
}

namespace ora::fluent {

class FluentProvider final {
 public:
  static FluentProvider& Instance() {
    static FluentProvider provider_singleton;
    return provider_singleton;
  }

  FluentProvider(const FluentProvider&) = delete;
  FluentProvider& operator=(const FluentProvider&) = delete;

  void Initialize(std::string_view str_culture,
                  std::span<const std::string> vec_paths, FileOpenFn fn_open,
                  const yaml::MiniYaml* ptr_map_definitions,
                  ErrorSink fn_on_error = nullptr);

  std::string GetMessage(
      std::string_view str_key,
      std::span<const std::pair<std::string, FluentValue>> vec_args = {});

  bool TryGetMessage(
      std::string_view str_key,
      std::span<const std::pair<std::string, FluentValue>> vec_args,
      std::string& out_message);

  bool TryGetModMessage(
      std::string_view str_key,
      std::span<const std::pair<std::string, FluentValue>> vec_args,
      std::string& out_message);

  void ResetForTest();

 private:
  FluentProvider() = default;

  std::mutex mtx_sync_;
  std::unique_ptr<FluentBundle> ptr_mod_bundle_;
  std::unique_ptr<FluentBundle> ptr_map_bundle_;
};

}
