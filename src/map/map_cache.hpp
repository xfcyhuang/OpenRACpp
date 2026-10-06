// UPSTREAM: OpenRA.Game/Map/MapPreview.cs @b6fc03f L31-470(同步核心)+
//          OpenRA.Game/Map/MapCache.cs @b6fc03f L27-461(同步核心) |
//          Verbatim-semantics rewrite of the synchronous cores.
//
// 已登记偏离(docs/COVERAGE.md,第十六批)/ Registered deviations (batch 16):
//  - MapPreview 的异步预览线程(LoadAsyncInternal/CacheMinimap)、远程详情
//    查询(QueryRemoteMapDetails)、minimap sheet 物化(SetMinimap)、
//    ModifiedDate/SetCustomRules(Preview 的自定义规则元数据)与 map.png
//    预览解码随 Phase 6 大厅/地图选择器批
//    MapPreview's async preview thread / remote queries / minimap sheet
//    materialization / ModifiedDate / SetCustomRules / map.png preview
//    decoding land with the Phase 6 lobby/map-selector batch.
//  - MapDirectoryTracker(文件监视)随同一批;LoadMaps 的枚举序保持上游
//    manifest.MapFolders 事实序
//    MapDirectoryTracker (file watching) lands with the same batch;
//    LoadMaps' enumeration keeps the manifest.MapFolders fact order.
#pragma once
import std;

#include "core/rectangle.hpp"
#include "fs/i_package.hpp"
#include "map/map.hpp"
#include "map/map_players.hpp"

namespace ora::game {
class Manifest;
class ModData;
}

namespace ora::map {

/// MapStatus(MapPreview.cs L31)
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

/// MapClassification(MapPreview.cs L35-43;[Flags])
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

/// MapPreview(MapPreview.cs L228-793 的同步面)
/// MapPreview (the synchronous face of MapPreview.cs L228-793).
class MapPreview final {
 public:
  /// MapPreview(modData, uid, gridType, cache)(L283-296 的 Unknown 初态)
  MapPreview(game::ModData* mod_data, std::string str_uid,
             std::optional<MapGridType> grid_type);

  MapStatus Status() const { return kind_status_; }                // L242
  MapClassification Class() const { return kind_class_; }          // L243
  const std::string& Uid() const { return str_uid_; }              // L246
  const std::string& Title() const { return str_title_; }          // L247
  const std::string& Author() const { return str_author_; }        // L250
  const std::string& TileSet() const { return str_tile_set_; }     // L251
  const std::vector<std::string>& Categories() const { return vec_categories_; }
  MapVisibility Visibility() const { return kind_visibility_; }    // L256
  const Rectangle& Bounds() const { return rect_bounds_; }         // L258
  const std::vector<CPos>& SpawnPoints() const { return vec_spawn_points_; }
  const MapPlayers& Players() const { return players_; }           // L262
  int PlayerCount() const { return int4_player_count_; }           // L263
  Size MapSize() const { return size_; }                           // L264(来自 map.yaml)
  const std::string& Path() const { return str_path_; }            // L244
  bool HideSpawnPreviews() const { return b_hide_spawn_previews_; }

  /// ToMap 的文件系统语境(MapCache 装配) | the ToMap file-system context.
  void SetOpenContext(fs::FileSystem* mod_files) { ptr_mod_files_ = mod_files; }

  /// UpdateFromMapWithoutOwningPackage(L352-358)
  void UpdateFromMapWithoutOwningPackage(const fs::IReadOnlyPackage& p,
                                         const fs::IReadOnlyPackage& parent,
                                         MapClassification classification,
                                         std::optional<MapGridType> grid_type);

  /// ToMap(L566-573):惰性打开地图包 → new Map(...)(C++ 侧即时构造;
  /// 包生命周期归 MapCache)
  std::unique_ptr<Map> ToMap();

 private:
  /// UpdateFromMap(L364-470) | UpdateFromMap (L364-470).
  void UpdateFromMap(const fs::IReadOnlyPackage& p,
                     MapClassification classification,
                     std::optional<MapGridType> grid_type);

  game::ModData* ptr_mod_data_ = nullptr;
  std::string str_uid_;
  std::optional<MapGridType> opt_grid_type_;

  std::string str_path_;
  MapStatus kind_status_ = MapStatus::Unavailable;
  MapGridType grid_type_ = MapGridType::Rectangular;
  fs::FileSystem* ptr_mod_files_ = nullptr;
  MapClassification kind_class_ = MapClassification::Unknown;
  std::string str_title_;
  std::string str_author_;
  std::string str_tile_set_;
  std::vector<std::string> vec_categories_;
  MapVisibility kind_visibility_ = MapVisibility::Lobby;
  Rectangle rect_bounds_{Rectangle::FromLTRB(0, 0, 0, 0)};
  std::vector<CPos> vec_spawn_points_;
  MapPlayers players_{};
  int int4_player_count_ = 0;
  Size size_{0, 0};
  bool b_hide_spawn_previews_ = false;

  const fs::IReadOnlyPackage* ptr_parent_package_ = nullptr;
  std::unique_ptr<fs::IReadOnlyPackage> ptr_owned_package_;

  static bool value_of_is(const yaml::MiniYamlNode& node, std::string_view value);
};

/// MapCache(MapCache.cs L27-461 的同步核心)
/// MapCache (the synchronous core of MapCache.cs).
class MapCache final {
 public:
  /// MapCache(manifest, modFiles)(L71-76):modData 面经 LoadMaps 注入
  MapCache(const game::Manifest& manifest, fs::FileSystem& mod_files);

  /// UnknownMap(L29) | UnknownMap (L29).
  static MapPreview& UnknownMap();

  /// LoadMaps(L84-135) | LoadMaps (L84-135).
  void LoadMaps(game::ModData& mod_data);

  /// this[uid](L418-425):缺项 = Lazy 新建 Unknown 预览 | the indexer: a
  /// missing uid lazily creates an Unknown preview.
  MapPreview& At(const std::string& str_uid);

  /// ChooseInitialMap(L398-416) | ChooseInitialMap (L398-416).
  std::string ChooseInitialMap(const std::string& str_initial_uid,
                               MersenneTwister& random);

  /// GetUpdatedMap(L337-351) | GetUpdatedMap (L337-351).
  std::string GetUpdatedMap(const std::string& str_uid);

  /// PickLastModifiedMap(L58-69) | PickLastModifiedMap (L58-69).
  std::string PickLastModifiedMap(MapVisibility visibility);

  /// 预览枚举(Values 的快照面) | the preview enumeration snapshot.
  std::vector<MapPreview*> Previews() const;

  /// Minimap sheet / async / remote faces land with the Phase 6 batch(见
  /// 文件头偏离)。

 private:
  /// LoadMapInternal(L142-176) | LoadMapInternal (L142-176).
  void LoadMapInternal(const std::string& map_path,
                       const fs::IReadOnlyPackage& package,
                       MapClassification classification,
                       const std::string& str_old_map,
                       std::optional<MapGridType> grid_type);

  const game::Manifest* ptr_manifest_ = nullptr;
  fs::FileSystem* ptr_mod_files_ = nullptr;
  game::ModData* ptr_mod_data_ = nullptr;
  MapGridType grid_type_ = MapGridType::Rectangular;

  /// mapLocations(L31):package → classification(插入序枚举)
  std::vector<std::pair<const fs::IReadOnlyPackage*, MapClassification>>
      vec_map_locations_;

  /// previews Cache<string, MapPreview>(L37):插入序
  std::vector<std::pair<std::string, std::unique_ptr<MapPreview>>>
      vec_previews_;

  std::vector<std::unique_ptr<fs::IReadOnlyPackage>> vec_owned_packages_;

  std::string str_last_modified_map_;
  std::string str_last_loaded_last_modified_map_;
  std::vector<std::pair<std::string, std::string>> vec_map_updates_;
};

}  // namespace ora::map
