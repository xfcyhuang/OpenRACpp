// UPSTREAM: OpenRA.Game/FluentProvider.cs @1f594e320(逐语义重写)。C#
// static → 进程级单例(上游 Lock SyncObject → mutex);Initialize 的
// `fileSystem is Map` 类型判 → 显式 map 定义节点出参(C++ 侧 Map 为具体
// 类且住 ora_map,本库不得上依 —— 由调用方(ModData.InitializeLoaders)
// 判型传入;偏离登记见 COVERAGE)。map 级 bundle 仅作 mod 级未中后的备
// 查(上游注释:禁止地图覆写 UI 级字符串键)。
// A verbatim-semantics rewrite of OpenRA.Game/FluentProvider.cs. The C#
// static becomes the process singleton (upstream's Lock SyncObject → a
// mutex); Initialize's `fileSystem is Map` type test becomes an explicit
// map-definitions out-parameter (the C++ Map is a concrete class living in
// ora_map, which this library must not depend on — the caller
// (ModData.InitializeLoaders) does the type test and passes the node; see
// the COVERAGE registry). The map-level bundle only backs up mod-level
// misses (upstream comment: maps must not overwrite UI string keys).
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

/// FluentProvider(FluentProvider.cs;进程级单例)
/// FluentProvider (FluentProvider.cs; the process singleton).
class FluentProvider final {
 public:
  static FluentProvider& Instance() {
    static FluentProvider provider_singleton;
    return provider_singleton;
  }

  FluentProvider(const FluentProvider&) = delete;
  FluentProvider& operator=(const FluentProvider&) = delete;

  /// Initialize(L27-52):mod 级恒建;map 定义节点非空时另建 map 级
  /// bundle(节点 Value = 逗号分隔文件表;base64 子节点拼接文本)
  /// Initialize (L27-52): the mod level always builds; a non-null map
  /// definitions node additionally builds the map-level bundle (the node's
  /// Value = the comma-separated file list; the base64 child nodes
  /// concatenate into the text).
  void Initialize(std::string_view str_culture,
                  std::span<const std::string> vec_paths, FileOpenFn fn_open,
                  const yaml::MiniYaml* ptr_map_definitions,
                  ErrorSink fn_on_error = nullptr);

  /// GetMessage(L54-68):mod 优先 → map → 键回落
  /// GetMessage (L54-68): mod first → map → the key fallback.
  std::string GetMessage(
      std::string_view str_key,
      std::span<const std::pair<std::string, FluentValue>> vec_args = {});

  /// TryGetMessage(L70-84):mod 优先 → map
  /// TryGetMessage (L70-84): mod first → map.
  bool TryGetMessage(
      std::string_view str_key,
      std::span<const std::pair<std::string, FluentValue>> vec_args,
      std::string& out_message);

  /// TryGetModMessage(L87-93;仅 MapPreview 面)
  /// TryGetModMessage (L87-93; the MapPreview-only face).
  bool TryGetModMessage(
      std::string_view str_key,
      std::span<const std::pair<std::string, FluentValue>> vec_args,
      std::string& out_message);

  /// 测试复位面(上游无 —— 进程级单例的测试隔离)
  /// The test-reset face (no upstream — the process singleton's test
  /// isolation).
  void ResetForTest();

 private:
  FluentProvider() = default;

  std::mutex mtx_sync_;
  std::unique_ptr<FluentBundle> ptr_mod_bundle_;
  std::unique_ptr<FluentBundle> ptr_map_bundle_;
};

}  // namespace ora::fluent
