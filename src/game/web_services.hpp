// UPSTREAM: OpenRA.Mods.Common/WebServices.cs @e180e3225 L15-60 +
// OpenRA.Game/Support/HttpQueryBuilder.cs L16-59(值面 + CheckModVersion)
// 五服务 URL 的值面(上游 readonly 字面量;此处可注入覆写 = 测试/宿主
// 面)+ CheckModVersion 的三态查询(GET {VersionCheck}?protocol=1&engine=
// &mod=&version= → "outdated"/"unknown"/"playtest"/其他=Latest);上游
// Task.Run + Game.RunAfterTick 的赋值序 → detach 线程 + 可注入 sink(缺
// 省 = 立即置,观察序等价)。
// The five-service-URL value face (upstream's readonly literals; here
// injectable for override = the test/host face) + CheckModVersion's
// three-state query (GET {VersionCheck}?protocol=1&engine=&mod=&version=
// → "outdated"/"unknown"/"playtest"/anything-else=Latest); upstream's
// Task.Run + Game.RunAfterTick assignment ordering → a detached thread +
// an injectable sink (defaulting to immediate assignment, observably
// equivalent ordering).
#pragma once
import std;

namespace ora::game {

enum class ModVersionStatus {
  NotChecked,
  Latest,
  Outdated,
  Unknown,
  PlaytestAvailable,
};

/// WebServices(IGlobalModData 的值面承载)
/// WebServices (the value face of IGlobalModData).
class WebServices final {
 public:
  std::string ServerList = "https://master.openra.net/games";
  std::string ServerAdvertise = "https://master.openra.net/ping";
  std::string MapRepository = "https://resource.openra.net/map/";
  std::string GameNews = "https://master.openra.net/gamenews";
  std::string GameNewsFileName = "news.yaml";
  std::string VersionCheck = "https://master.openra.net/versioncheck";

  /// Game.EngineVersion / Manifest.Id / Manifest.Metadata.Version 的注入
  /// (CheckModVersion 的查询参数源)
  /// The injections of Game.EngineVersion / Manifest.Id /
  /// Manifest.Metadata.Version (the query-parameter sources of
  /// CheckModVersion).
  void SetVersionInfo(std::string str_engine, std::string str_mod,
                      std::string str_version) {
    str_engine_version_ = std::move(str_engine);
    str_mod_id_ = std::move(str_mod);
    str_mod_version_ = std::move(str_version);
  }

  /// Game.RunAfterTick 的注入面(缺省 = 立即执行)
  /// The injected face of Game.RunAfterTick (defaulting to immediate
  /// execution).
  void SetRunAfterTickSink(
      std::function<void(std::function<void()>)> fn_sink) {
    fn_run_after_tick_ = std::move(fn_sink);
  }

  ModVersionStatus ModVersionStatus_() const {
    return kind_status_.load(std::memory_order_relaxed);
  }

  void CheckModVersion();

 private:
  std::string str_engine_version_ = "dev";
  std::string str_mod_id_;
  std::string str_mod_version_;
  std::atomic<ModVersionStatus> kind_status_{ModVersionStatus::NotChecked};
  std::function<void(std::function<void()>)> fn_run_after_tick_;
};

/// HttpQueryBuilder.ToString(L43-57):url + '?' + name=value&…(value 过
/// Uri.EscapeDataString)
/// HttpQueryBuilder.ToString (L43-57): url + '?' + name=value&… (the
/// value through Uri.EscapeDataString).
std::string BuildHttpQuery(
    std::string_view sv_url,
    std::initializer_list<std::pair<std::string_view, std::string_view>>
        vec_parameters);

/// Uri.EscapeDataString:RFC 3986 unreserved(A-Za-z0-9 - _ . ~)之外全部
/// UTF-8 %XX
/// Uri.EscapeDataString: everything outside RFC 3986's unreserved set
/// (A-Za-z0-9 - _ . ~) as UTF-8 %XX.
std::string EscapeDataString(std::string_view sv_value);

}  // namespace ora::game
