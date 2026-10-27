#pragma once
import std;

#include "game/ruleset.hpp"
#include "map/map_cache.hpp"
#include "net/session.hpp"

namespace ora::server {

using LintServerMapPassFn =
    std::function<void(const std::function<void(std::string_view)>&,
                       const std::function<void(std::string_view)>&,
                       const map::MapPreview&, const game::Ruleset&)>;

struct MapStatusCacheDeps {
  std::vector<LintServerMapPassFn> vec_lint_passes;
  std::function<void(std::string_view)> fn_server_log;
};

class MapStatusCache final {
 public:
  using StatusChangedFn =
      std::function<void(std::string_view, int)>;

  MapStatusCache(MapStatusCacheDeps deps, StatusChangedFn fn_on_status_changed,
                 bool b_enable_remote_linting);
  ~MapStatusCache();

  MapStatusCache(const MapStatusCache&) = delete;
  MapStatusCache& operator=(const MapStatusCache&) = delete;

  int StatusOf(const map::MapPreview& map);

 private:

  void RunLintTests(const map::MapPreview* p_map,
                    std::unique_ptr<game::Ruleset> up_rules);

  MapStatusCacheDeps deps_;
  StatusChangedFn fn_on_status_changed_;
  bool b_enable_remote_linting_;

  std::mutex mutex_cache_;
  std::map<const map::MapPreview*, int> map_cache_;
  std::vector<std::thread> vec_lint_threads_;
};

}
