// UPSTREAM: OpenRA.Game/Widgets/ChromeMetrics.cs @b6fc03f(chrome.yaml 指标表
// 逐语义:Merge 全部 ChromeMetrics 源 → 键值覆盖)。C# static → 实例类
// (ChromeProvider 先例);Get<T> 的 T 域收敛为 string(引擎核心唯一消费
// 形态,泛型面随消费批次扩展);Dictionary → 插入序条目表(索引写覆盖原位)。
// Verbatim-semantics rewrite of the chrome.yaml metrics table (merging every
// ChromeMetrics source → key/value overwrite). C#'s static becomes an
// instance class (the ChromeProvider precedent); Get<T>'s T domain narrows
// to string (the engine core's only consumer shape; the generic face
// extends with its consumer batches); the Dictionary becomes an
// insertion-ordered entry list (indexer writes overwrite in place).
#pragma once
import std;

#include "fs/file_system.hpp"

namespace ora::ui {

class ChromeMetrics {
 public:
  struct Deps {
    fs::FileSystem* ptr_file_system = nullptr;
    const std::vector<std::string>* vec_metric_files = nullptr;  // Manifest.ChromeMetrics
  };

  /// Initialize(L21-30) | Initialize (L21-30).
  void Initialize(Deps deps);

  /// Get(L32-35):缺键抛 KeyNotFoundException 等价消息
  /// Get (L32-35): a missing key throws the KeyNotFoundException
  /// equivalent.
  const std::string& Get(std::string_view str_key) const;

  /// TryGet(L37-47) | TryGet (L37-47).
  bool TryGet(std::string_view str_key, std::string& str_result) const;

 private:
  std::vector<std::pair<std::string, std::string>> vec_data_;
};

}  // namespace ora::ui
