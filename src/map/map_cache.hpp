#pragma once
import std;

#include "core/rectangle.hpp"
#include "fs/file_system.hpp"
#include "fs/i_package.hpp"
#include "formats/png.hpp"
#include "game/actor_info.hpp"
#include "game/ruleset.hpp"
#include "map/map.hpp"
#include "map/map_directory_tracker.hpp"
#include "map/map_players.hpp"
#include "yaml/mini_yaml.hpp"

namespace ora::game {
class Manifest;
class ModData;
}

namespace ora::map {

enum class MapStatus : std::int32_t {
  Available,
  Unavailable,
  Searching,
  DownloadAvailable,
  Downloading,
  DownloadError,
  Generatable,
  Generating,
};

enum class MapClassification : std::int32_t {
  Unknown = 0,
  System = 1,
  User = 2,
  Remote = 4,
  Generated = 8,
};

constexpr MapClassification operator&(MapClassification a, MapClassification b) {
  return static_cast<MapClassification>(static_cast<std::int32_t>(a) &
                                        static_cast<std::int32_t>(b));
}
constexpr bool HasFlag(MapClassification v, MapClassification f) {
  return (static_cast<std::int32_t>(v) & static_cast<std::int32_t>(f)) ==
         static_cast<std::int32_t>(f);
}

struct MapGenerationArgs {
  enum class PreviewVisibilityFlags : std::int32_t {
    None = 0,
    Lobby = 1,
    MapChooser = 2,
    All = 3,
  };

  std::string str_uid;
  std::string str_generator;
  std::string str_tileset;
  Size size_{};
  std::string str_title;
  std::string str_author;
  PreviewVisibilityFlags kind_preview_visibility =
      PreviewVisibilityFlags::All;
  std::vector<std::pair<std::string, std::string>> vec_options;
};

class MapCache;

class MapPreview final {
 public:
  struct InnerData {
    int int4_map_format = 0;
    std::string str_title = "Unknown Map";
    std::vector<std::string> vec_categories{"Unknown"};
    std::string str_author = "Unknown Author";
    std::string str_tile_set = "unknown";
    std::optional<MapPlayers> opt_players;
    int int4_player_count = 0;
    std::vector<CPos> vec_spawn_points;
    bool b_hide_spawn_previews = false;
    MapGridType grid_type = MapGridType::Rectangular;
    Rectangle rect_bounds{Rectangle::FromLTRB(0, 0, 0, 0)};
    std::shared_ptr<const fmt::Png> ptr_preview;
    MapStatus kind_status = MapStatus::Unavailable;
    MapClassification kind_class = MapClassification::Unknown;
    MapVisibility kind_visibility = MapVisibility::Lobby;
    std::int64_t int8_modified_date = 0;
    std::optional<MapGenerationArgs> opt_generation_args;
    Size size_{};

    std::optional<yaml::MiniYaml> opt_rule_definitions;
    std::optional<yaml::MiniYaml> opt_weapon_definitions;
    std::optional<yaml::MiniYaml> opt_voice_definitions;
    std::optional<yaml::MiniYaml> opt_music_definitions;
    std::optional<yaml::MiniYaml> opt_notification_definitions;
    std::optional<yaml::MiniYaml> opt_sequence_definitions;
    std::optional<yaml::MiniYaml> opt_model_sequence_definitions;
    std::optional<yaml::MiniYaml> opt_fluent_message_definitions;

    std::shared_ptr<const game::ActorInfo> ptr_world_actor_info;
    std::shared_ptr<const game::ActorInfo> ptr_player_actor_info;
  };

  MapPreview(game::ModData* mod_data, std::string str_uid,
             std::optional<MapGridType> grid_type,
             MapCache* ptr_cache = nullptr);
  ~MapPreview();

  MapPreview(const MapPreview&) = delete;
  MapPreview& operator=(const MapPreview&) = delete;

  int MapFormat() const;
  MapStatus Status() const;
  MapClassification Class() const;
  const std::string& Uid() const;
  const std::string& Path() const;
  const std::string& Title() const;
  const std::string& Author() const;
  const std::string& TileSet() const;
  const std::vector<std::string>& Categories() const;
  MapVisibility Visibility() const;
  const Rectangle& Bounds() const;
  const std::vector<CPos>& SpawnPoints() const;
  const MapPlayers& Players() const;
  int PlayerCount() const;
  Size MapSize() const;
  bool HideSpawnPreviews() const;
  /// 上游 GridType 属性(InnerData 直读)
  /// Upstream's GridType property (a direct InnerData read).
  std::optional<MapGridType> GridType() const;
  /// 上游 L256-257 的下载进度对(Install 的进度回调写)
  /// Upstream's L256-257 download-progress pair (written by Install's
  /// progress callback).
  long long DownloadBytes() const;
  int DownloadPercentage() const;
  const fmt::Png* Preview() const;
  std::int64_t ModifiedDate() const;
  const std::optional<MapGenerationArgs>& GenerationArgs() const;
  const std::optional<yaml::MiniYaml>& RuleDefinitions() const;
  const std::optional<yaml::MiniYaml>& WeaponDefinitions() const;
  const std::optional<yaml::MiniYaml>& SequenceDefinitions() const;
  std::shared_ptr<const game::ActorInfo> WorldActorInfo() const;
  std::shared_ptr<const game::ActorInfo> PlayerActorInfo() const;

  std::string GetMessage(std::string_view str_key) const;
  bool TryGetMessage(std::string_view str_key, std::string& out_message) const;

  std::uintptr_t GetMinimap();
  void SetMinimap(std::uintptr_t uint8_minimap);

  bool DefinesUnsafeCustomRules() const;
  std::unique_ptr<game::Ruleset> LoadRuleset() const;

  void SetOpenContext(fs::FileSystem* mod_files);

  bool UpdateFromMapWithoutOwningPackage(
      const fs::IReadOnlyPackage& p, const fs::IReadOnlyPackage& parent,
      MapClassification classification, std::optional<MapGridType> grid_type,
      const std::vector<std::vector<yaml::MiniYamlNode>>* vec_mod_data_rules = nullptr);

  void UpdateFromGenerationArgs(const MapGenerationArgs& args);
  void Generate();

  void BeginRemoteSearch();
  /// node_yaml == nullptr = 上游 CompleteRemoteSearch(null)(查无此图:
  /// 状态 Unavailable + fn_parse_metadata 照跑 = fail 回调面)
  /// node_yaml == nullptr = upstream's CompleteRemoteSearch(null) (the map
  /// is unknown: status Unavailable + fn_parse_metadata still runs = the
  /// fail-callback face).
  void CompleteRemoteSearch(
      const yaml::MiniYamlNode* node_yaml,
      const std::function<void(MapPreview&)>& fn_parse_metadata = nullptr);
  void Install(const std::string& str_map_repository_url);

  void Invalidate();
  void Delete();
  void LoadPackage() const;

  std::vector<char> OpenFile(const std::string& filename) const;
  bool TryGetPackageContainingFile(const std::string& path,
                                   fs::IReadOnlyPackage*& out_package,
                                   std::string& out_filename) const;
  bool TryOpenFile(const std::string& filename, std::vector<char>& out_bytes) const;
  bool ExistsFile(const std::string& filename) const;
  bool IsExternalFile(const std::string& filename) const;

  std::unique_ptr<Map> ToMap();

 private:
  bool UpdateFromMap(const fs::IReadOnlyPackage& p,
                     MapClassification classification,
                     std::optional<MapGridType> grid_type,
                     const std::vector<std::vector<yaml::MiniYamlNode>>* vec_mod_data_rules);

  void SetCustomRules(
      const std::vector<std::pair<std::string_view, const yaml::MiniYaml*>>& yaml,
      const std::vector<std::vector<yaml::MiniYamlNode>>* vec_mod_data_rules,
      InnerData& data_new);

  std::shared_ptr<const InnerData> Snapshot() const;

  /// 上游对 innerData.Status 的就地赋值(clone+swap 承载)
  /// Upstream's in-place innerData.Status assignment (carried by
  /// clone+swap).
  void SetStatus(MapStatus kind_status);

  game::ModData* ptr_mod_data_ = nullptr;
  MapCache* ptr_cache_ = nullptr;
  fs::FileSystem* ptr_mod_files_ = nullptr;
  const std::string str_uid_;
  std::optional<MapGridType> opt_grid_type_;

  mutable std::mutex mtx_inner_;
  std::shared_ptr<const InnerData> ptr_inner_;

  std::string str_path_;
  mutable std::unique_ptr<fs::IReadOnlyPackage> ptr_owned_package_;
  const fs::IReadOnlyPackage* ptr_parent_package_ = nullptr;

  std::atomic<std::uintptr_t> uint8_minimap_{0};
  std::atomic<bool> b_generating_minimap_{false};
  std::atomic<long long> int8_download_bytes_{0};
  std::atomic<int> int4_download_percentage_{0};
};

class MapCache final {
 public:
  using RunAfterTickFn = std::function<void(std::function<void()>)>;
  using MinimapMaterializerFn = std::function<void(MapPreview&)>;
  using ReleaseBufferFn = std::function<void()>;

  MapCache(const game::Manifest& manifest, fs::FileSystem& mod_files);
  ~MapCache();

  MapCache(const MapCache&) = delete;
  MapCache& operator=(const MapCache&) = delete;

  static MapPreview& UnknownMap();

  /// LoadMaps()(L84-124);filter_contains 非空时仅装载包名含该子串的地图
  /// (逐图处理面不变;测试/嵌入侧的提速面 —— 上游无此参,登记 D209)
  /// LoadMaps() (L84-124); a non-empty filter_contains loads only the maps
  /// whose package name contains the substring (the per-map processing is
  /// unchanged; a test/embedding-side speedup face — upstream has no such
  /// parameter, registered as D209).
  void LoadMaps(game::ModData& mod_data,
                std::string_view str_filter_contains = {});
  void UpdateMaps();
  void LoadMap(const std::string& str_map, const fs::IReadOnlyPackage& package,
               MapClassification classification, const std::string& str_old_map);

  MapPreview& At(const std::string& str_uid);

  std::string ChooseInitialMap(const std::string& str_initial_uid,
                               MersenneTwister& random);

  std::string GetUpdatedMap(const std::string& str_uid);
  std::string PickLastModifiedMap(MapVisibility visibility);

  std::vector<MapPreview*> Previews() const;
  const std::vector<std::pair<const fs::IReadOnlyPackage*, MapClassification>>&
  MapLocations() const;

  bool LoadPreviewImages = true;

  void SetRunAfterTickSink(RunAfterTickFn fn) {
    fn_run_after_tick_ = std::move(fn);
  }
  void SetMinimapMaterializer(MinimapMaterializerFn fn) {
    fn_materialize_minimap_ = std::move(fn);
  }
  void SetSheetReleaseBuffer(ReleaseBufferFn fn) {
    fn_release_buffer_ = std::move(fn);
  }

  void CacheMinimap(MapPreview& preview);

  /// QueryRemoteMapDetails(L223-260):Unavailable 态的 uid 去重后批查(
  /// 每批 50),GET {repository}/hash/{uids}/yaml,逐节点 CompleteRemoteSearch;
  /// 批尾仍 Searching 的 = 查无此图(CompleteRemoteSearch(null) → fail 回调)
  /// —— 后台线程承载(上游 Task.Run)。
  /// QueryRemoteMapDetails (L223-260): the Unavailable-status uids,
  /// deduplicated then batched (50 per batch), GET {repository}/hash/
  /// {uids}/yaml with CompleteRemoteSearch per node; those still Searching
  /// at the batch tail = unknown maps (CompleteRemoteSearch(null) → the
  /// fail callback) — carried on a background thread (upstream's Task.Run).
  void QueryRemoteMapDetails(
      const std::string& str_repository_url,
      const std::vector<std::string>& vec_uids,
      const std::function<void(MapPreview&)>& fn_map_details_received =
          nullptr,
      const std::function<void(MapPreview&)>& fn_map_query_failed = nullptr);

  /// Game.Settings.Game.AllowDownloading(缺省 true)
  /// Game.Settings.Game.AllowDownloading (defaulting to true).
  void SetAllowDownloadingFn(std::function<bool()> fn) {
    fn_allow_downloading_ = std::move(fn);
  }
  bool AllowDownloading() const {
    return fn_allow_downloading_ ? fn_allow_downloading_() : true;
  }

  /// 上游 Log.Write("debug", …) 的注入面(缺省丢弃 —— 日志副作用面)
  /// The injected face of upstream's Log.Write("debug", …) (defaulting to
  /// discard — a logging side-effect face).
  void SetDebugLogFn(std::function<void(std::string_view)> fn) {
    fn_debug_log_ = std::move(fn);
  }
  void DebugLog(std::string_view sv_line) const {
    if (fn_debug_log_)
      fn_debug_log_(sv_line);
  }

 private:
  void LoadMapInternal(const std::string& map_path,
                       const fs::IReadOnlyPackage& package,
                       MapClassification classification,
                       const std::string& str_old_map,
                       std::optional<MapGridType> grid_type,
                       const std::vector<std::vector<yaml::MiniYamlNode>>* vec_mod_data_rules);

  void LoadAsyncInternal();

  const game::Manifest* ptr_manifest_ = nullptr;
  fs::FileSystem* ptr_mod_files_ = nullptr;
  game::ModData* ptr_mod_data_ = nullptr;
  MapGridType grid_type_ = MapGridType::Rectangular;

  std::vector<std::pair<const fs::IReadOnlyPackage*, MapClassification>>
      vec_map_locations_;

  std::deque<std::pair<std::string, std::unique_ptr<MapPreview>>>
      vec_previews_;

  /// At() 的并发面(QueryRemoteMapDetails 的后台线程 vs 主线程装载);
  /// deque 节点稳 —— emplace_back 不失效已发引用
  /// At()'s concurrency face (QueryRemoteMapDetails's background thread
  /// vs the main-thread loading); deque nodes are stable — emplace_back
  /// never invalidates a handed-out reference.
  mutable std::mutex mtx_previews_;

  std::vector<std::unique_ptr<fs::IReadOnlyPackage>> vec_owned_packages_;
  std::vector<std::unique_ptr<MapDirectoryTracker>> vec_trackers_;

  std::string str_last_modified_map_;
  std::string str_last_loaded_last_modified_map_;
  std::vector<std::pair<std::string, std::string>> vec_map_updates_;

  std::mutex mtx_async_;
  std::vector<MapPreview*> vec_generate_minimap_;
  std::thread thread_loader_;
  bool b_loader_shutdown_ = true;
  std::atomic<bool> b_disposed_{false};

  RunAfterTickFn fn_run_after_tick_;
  MinimapMaterializerFn fn_materialize_minimap_;
  ReleaseBufferFn fn_release_buffer_;
  std::function<bool()> fn_allow_downloading_;
  std::function<void(std::string_view)> fn_debug_log_;
};

}  // namespace ora::map
