#include "server/map_status_cache.hpp"

#include "map/map_players.hpp"
#include "yaml/mini_yaml.hpp"

namespace ora::server {

namespace {

void LogLine(const std::function<void(std::string_view)>& fn_log,
             std::string_view str_line) {
  if (fn_log)
    fn_log(str_line);
}

}

MapStatusCache::MapStatusCache(
    MapStatusCacheDeps deps, StatusChangedFn fn_on_status_changed,
    bool b_enable_remote_linting)
    : deps_{std::move(deps)},
      fn_on_status_changed_{std::move(fn_on_status_changed)},
      b_enable_remote_linting_{b_enable_remote_linting} {}

MapStatusCache::~MapStatusCache() {
  for (auto& t : vec_lint_threads_)
    if (t.joinable())
      t.join();
}

void MapStatusCache::RunLintTests(const map::MapPreview* p_map,
                                  std::unique_ptr<game::Ruleset> up_rules) {
  int int4_status = 0;
  {
    std::lock_guard lock{mutex_cache_};
    const auto it = map_cache_.find(p_map);
    if (it == map_cache_.end())
      return;
    int4_status = it->second;
  }

  auto b_failed = false;
  const auto on_lint_failure = [&](std::string_view str_message) {
    LogLine(deps_.fn_server_log,
            std::format("Map {} failed lint with error: {}", p_map->Title(),
                        str_message));
    b_failed = true;
  };
  const auto on_lint_warning = [](std::string_view) {};

  if (up_rules != nullptr) {
    for (const auto& fn_pass : deps_.vec_lint_passes)
      fn_pass(on_lint_failure, on_lint_warning, *p_map, *up_rules);
  } else {

    b_failed = true;
  }

  int4_status &= ~static_cast<int>(net::SessionMapStatus::Validating);
  int4_status |= b_failed ? static_cast<int>(net::SessionMapStatus::Incompatible)
                          : static_cast<int>(net::SessionMapStatus::Playable);

  {
    std::lock_guard lock{mutex_cache_};
    map_cache_[p_map] = int4_status;
  }
  if (fn_on_status_changed_)
    fn_on_status_changed_(p_map->Uid(), int4_status);
}

int MapStatusCache::StatusOf(const map::MapPreview& map) {
  {
    std::lock_guard lock{mutex_cache_};
    const auto it = map_cache_.find(&map);
    if (it != map_cache_.end())
      return it->second;
  }

  auto up_rules = std::unique_ptr<game::Ruleset>{};
  auto int4_status = 0;
  try {
    up_rules = map.LoadRuleset();
    int4_status =
        b_enable_remote_linting_ && map.Status() != map::MapStatus::Available
            ? static_cast<int>(net::SessionMapStatus::Validating)
            : static_cast<int>(net::SessionMapStatus::Playable);
    if (map.DefinesUnsafeCustomRules())
      int4_status |= static_cast<int>(net::SessionMapStatus::UnsafeCustomRules);
  } catch (const std::exception& e) {
    LogLine(deps_.fn_server_log,
            std::format("Failed to load rules for `{}` with error: {}",
                        map.Title(), e.what()));
    int4_status = static_cast<int>(net::SessionMapStatus::Incompatible);
  }

  if (static_cast<int>(map.Players().Players.size()) >
      map::MapPlayers::kMaximumPlayerCount) {
    LogLine(deps_.fn_server_log,
            std::format("Failed to load `{}`: Player count exceeds maximum ({}/{}).",
                        map.Title(), map.Players().Players.size(),
                        map::MapPlayers::kMaximumPlayerCount));
    int4_status = static_cast<int>(net::SessionMapStatus::Incompatible);
  }

  {
    std::lock_guard lock{mutex_cache_};
    map_cache_[&map] = int4_status;
  }

  if (net::HasMapStatusFlag(int4_status, net::SessionMapStatus::Validating))
    vec_lint_threads_.emplace_back(
        [this, p_map = &map, up_rules = std::move(up_rules)]() mutable {
          RunLintTests(p_map, std::move(up_rules));
        });

  return int4_status;
}

}
