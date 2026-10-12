// UPSTREAM: OpenRA.Game/Server/MapStatusCache.cs @e180e3225 L19-106 全文
//          (索引器的规则装载/UnsafeCustomRules 位/玩家数上限三段定级 +
//          Validating 的后台 lint 线程 + RunLintTests 的失败定位级;
//          ILintServerMapPass 的类型枚举 = ObjectCreator 反射 → 注入面
//          (Mods.Common 的 lint pass 族未移植);ThreadPool → 自持
//          std::thread(析构 join;登记 D224);Dictionary → mutex + map
//          (worker 线程的并发写;上游裸字典竞态不设防,C++ 侧加锁 —— 行
//          为等价、竞态面收紧)
//          The whole of MapStatusCache.cs L19-106 (the indexer's three-stage
//          grading — the rules load / the UnsafeCustomRules bit / the
//          player-count cap — + Validating's background lint thread +
//          RunLintTests's failure-to-grade flow; ILintServerMapPass's type
//          enumeration = ObjectCreator reflection → an injection face (the
//          Mods.Common lint-pass family is unported); ThreadPool → a
//          self-held std::thread (joined at destruction; registered D224);
//          Dictionary → a mutex + map (the worker thread's concurrent write;
//          upstream's bare dictionary is race-unguarded, C++ locks —
//          behavior-equivalent, the race window closed).
#pragma once
import std;

#include "game/ruleset.hpp"
#include "map/map_cache.hpp"
#include "net/session.hpp"


namespace ora::server {

/// ILintServerMapPass(MapStatusCache.cs L19)的注入面:Run(emitError,
/// emitWarning, modData, map, mapRules) —— modData 承载于 lint pass 的
/// 装配侧
/// ILintServerMapPass's injection face (L19): Run(emitError, emitWarning,
/// modData, map, mapRules) — modData rides the pass's assembly side.
using LintServerMapPassFn =
    std::function<void(const std::function<void(std::string_view)>&,
                       const std::function<void(std::string_view)>&,
                       const map::MapPreview&, const game::Ruleset&)>;

struct MapStatusCacheDeps {
  std::vector<LintServerMapPassFn> vec_lint_passes;
  std::function<void(std::string_view)> fn_server_log;
};

/// MapStatusCache(MapStatusCache.cs L21)
/// MapStatusCache (MapStatusCache.cs L21).
class MapStatusCache final {
 public:
  using StatusChangedFn =
      std::function<void(std::string_view, int)>;

  MapStatusCache(MapStatusCacheDeps deps, StatusChangedFn fn_on_status_changed,
                 bool b_enable_remote_linting);
  ~MapStatusCache();

  MapStatusCache(const MapStatusCache&) = delete;
  MapStatusCache& operator=(const MapStatusCache&) = delete;

  /// this[MapPreview](L68-104):缓存/定级/入队后台 lint
  /// this[MapPreview] (L68-104): the cache/grading/background-lint enqueue.
  int StatusOf(const map::MapPreview& map);

 private:
  /// RunLintTests(L35-66;后台线程体)
  /// RunLintTests (L35-66; the background-thread body).
  void RunLintTests(const map::MapPreview* p_map,
                    std::unique_ptr<game::Ruleset> up_rules);

  MapStatusCacheDeps deps_;
  StatusChangedFn fn_on_status_changed_;
  bool b_enable_remote_linting_;

  std::mutex mutex_cache_;
  std::map<const map::MapPreview*, int> map_cache_;
  std::vector<std::thread> vec_lint_threads_;
};

}  // namespace ora::server
