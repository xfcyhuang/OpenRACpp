// UPSTREAM: OpenRA.Game/Widgets/ChromeMetrics.cs @e180e3225(chrome.yaml 指标表
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

#include "core/color.hpp"
#include "core/int2.hpp"
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

  /// Get<T>(L32-35):缺键抛 KeyNotFoundException 等价消息;T 域经
  /// FieldLoader.GetValue(string/int/bool/Color 四型,ChromeMetrics.Get 的
  /// 消费域)
  /// Get<T> (L32-35): a missing key throws the KeyNotFoundException
  /// equivalent; the T domain goes through FieldLoader.GetValue (the
  /// string/int/bool/Color quartet — ChromeMetrics.Get's consumer domain).
  const std::string& Get(std::string_view str_key) const;
  std::int32_t GetInt32(std::string_view str_key) const;
  bool GetBool(std::string_view str_key) const;
  core::Color GetColor(std::string_view str_key) const;
  int2 GetInt2(std::string_view str_key) const;
  core::Vector2 GetVector2(std::string_view str_key) const;

  /// TryGet(L37-47) | TryGet (L37-47).
  bool TryGet(std::string_view str_key, std::string& str_result) const;
  bool TryGetColor(std::string_view str_key, core::Color& color_result) const;
  bool TryGetVector2(std::string_view str_key, core::Vector2& vec_result) const;

 private:
  const std::string* Find(std::string_view str_key) const;

  std::vector<std::pair<std::string, std::string>> vec_data_;
};

}  // namespace ora::ui
