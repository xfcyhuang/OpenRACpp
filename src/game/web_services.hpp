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

class WebServices final {
 public:
  std::string ServerList = "https://master.openra.net/games";
  std::string ServerAdvertise = "https://master.openra.net/ping";
  std::string MapRepository = "https://resource.openra.net/map/";
  std::string GameNews = "https://master.openra.net/gamenews";
  std::string GameNewsFileName = "news.yaml";
  std::string VersionCheck = "https://master.openra.net/versioncheck";

  void SetVersionInfo(std::string str_engine, std::string str_mod,
                      std::string str_version) {
    str_engine_version_ = std::move(str_engine);
    str_mod_id_ = std::move(str_mod);
    str_mod_version_ = std::move(str_version);
  }

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

std::string BuildHttpQuery(
    std::string_view sv_url,
    std::initializer_list<std::pair<std::string_view, std::string_view>>
        vec_parameters);

std::string EscapeDataString(std::string_view sv_value);

}
