// UPSTREAM: OpenRA.Game/Map/Map.cs @b6fc03f L29-1482(逐语义重写)
//          Verbatim-semantics rewrite.
//
// 机制对照 / Mechanism mapping:
//  - MapField 反射表(YamlFields)→ 逐字段 lambda 表(键/required/ignoreIfValue
//    + Deserialize/Serialize 闭包;类型分派 NodeList/MiniYaml/标量与上游
//    MapField.Type 三态一致)
//    The MapField reflection table (YamlFields) becomes a per-field lambda
//    table (key/required/ignoreIfValue + Deserialize/Serialize closures; the
//    NodeList/MiniYaml/scalar dispatch matches MapField.Type's three states).
//  - CellEntryChanged 事件 → 回调表(construct 序;UpdateRamp/UpdateProjection/
//    InvalidateTerrainIndex 的挂载序照抄 —— 事件序即上游委托序)
//    CellEntryChanged events become callback lists in registration order
    // (the UpdateRamp/UpdateProjection/InvalidateTerrainIndex wiring order
//    is upstream's delegate order).
//  - IReadOnlyFileSystem 面(Open/TryOpen/Exists/TryGetPackageContaining/
//    IsExternalFile)逐语义;Package 优先、modFiles 兜底
//    The IReadOnlyFileSystem face (Open/TryOpen/Exists/
//    TryGetPackageContaining/IsExternalFile) verbatim; the package wins and
//    modFiles is the fallback.
//  - 依赖面:SequenceSet 构造(上游直接 new SequenceSet(this, modData, ...))
//    → Params.fn_sequences_factory 注入(渲染线程/加载器链的装配面随嵌入侧
//    ModData;COVERAGE 登记)
//    Dependency face: the SequenceSet construction (upstream news it
//    directly) rides Params.fn_sequences_factory injection — registered in
//    COVERAGE.
#pragma once
import std;

#include "core/cell_pos.hpp"
#include "core/rectangle.hpp"
#include "core/size.hpp"
#include "core/wdist.hpp"
#include "core/wpos.hpp"
#include "core/wrot.hpp"
#include "fs/i_package.hpp"
#include "map/cell_layer.hpp"
#include "map/cell_region.hpp"
#include "map/map_players.hpp"
#include "terrain/map_grid.hpp"
#include "terrain/terrain_info.hpp"
#include "terrain/tile_reference.hpp"

namespace ora::game {
class ModData;
class Ruleset;
struct MapFileSystemFace;
}  // namespace ora::game
namespace ora::gfx {
class SequenceSet;
}
namespace ora::sim {
class Actor;
}

namespace ora::map {

/// MapVisibility(Map.cs L62-67;[Flags])
/// MapVisibility (Map.cs L62-67; [Flags]).
enum class MapVisibility : std::int32_t {
  Lobby = 1,
  Shellmap = 2,
  MissionSelector = 4,
};

constexpr MapVisibility operator|(MapVisibility a, MapVisibility b) {
  return static_cast<MapVisibility>(static_cast<std::int32_t>(a) |
                                    static_cast<std::int32_t>(b));
}
constexpr MapVisibility operator&(MapVisibility a, MapVisibility b) {
  return static_cast<MapVisibility>(static_cast<std::int32_t>(a) &
                                    static_cast<std::int32_t>(b));
}
/// HasFlag 语义(L62 的 HasFlag 消费点) | the HasFlag consumption semantics.
constexpr bool HasFlag(MapVisibility v, MapVisibility flag) {
  return (static_cast<std::int32_t>(v) & static_cast<std::int32_t>(flag)) ==
         static_cast<std::int32_t>(flag);
}

/// BinaryDataHeader(Map.cs L29-59)
/// BinaryDataHeader (Map.cs L29-59).
struct BinaryDataHeader {
  std::uint8_t Format{0};
  std::uint32_t TilesOffset{0};
  std::uint32_t HeightsOffset{0};
  std::uint32_t ResourcesOffset{0};

  /// 头解析(L36-58):宽高校验失败 "Invalid tile data";未知格式
  /// "Unknown binary map format '{Format}'" 逐字
  /// Header parse (L36-58): a size mismatch throws "Invalid tile data"; an
  /// unknown format throws "Unknown binary map format '{Format}'" verbatim.
  BinaryDataHeader(std::span<const char> s, Size expected_size);
};

/// Map(Map.cs L153)
/// Map (Map.cs L153).
class Map final {
 public:
  static constexpr int kSupportedMapFormat = 11;  // L155
  static constexpr int kCurrentMapFormat = 12;    // L156
  static constexpr short kInvalidCachedTerrainIndex = -1;  // L157

  /// 构造依赖面(见头注)| the construction dependency face (see header).
  struct Params {
    game::ModData* mod_data = nullptr;
    /// SequenceSet 装配面(上游 = new SequenceSet(this, modData, tileset,
    /// seqDefs));空 = Sequences 保持未装配(测试/无渲染路径)
    /// The SequenceSet assembly face (upstream = new SequenceSet(...));
    /// empty keeps Sequences unassembled (tests / no-render paths).
    std::function<std::unique_ptr<gfx::SequenceSet>(Map&)> fn_sequences_factory;
  };

  /// 编辑器/导入器构造(Map.cs L344-373) | the editor/importer ctor
  /// (Map.cs L344-373).
  Map(Params params, const ITerrainInfo& terrain_info, Size size);

  /// 包构造(Map.cs L375-455) | the package ctor (Map.cs L375-455).
  Map(Params params, const fs::IReadOnlyPackage& package);

  ~Map();  // SequenceSet 完整类型的析构面(定义于 map.cpp)
           // the out-of-line dtor (SequenceSet completeness; map.cpp).

  Map(const Map&) = delete;
  Map& operator=(const Map&) = delete;

  // ———— yaml 字段(Map.cs L186-232;访问器命名保留上游)————
  std::int32_t MapFormat() const { return int4_map_format_; }
  std::uint8_t TileFormat() const { return uint1_tile_format_; }  // L187
  const std::string& RequiresMod() const { return str_requires_mod_; }
  const std::string& Title() const { return str_title_; }
  const std::string& Author() const { return str_author_; }
  const std::string& Tileset() const { return str_tileset_; }
  bool LockPreview() const { return b_lock_preview_; }
  const Rectangle& Bounds() const { return rect_bounds_; }
  MapVisibility Visibility() const { return kind_visibility_; }
  const std::vector<std::string>& Categories() const {
    return vec_categories_;
  }
  bool HideSpawnPreviews() const { return b_hide_spawn_previews_; }
  const Size& MapSize() const { return size_map_; }  // L200

  /// Player/actor yaml(L221-222) | the player/actor yaml (L221-222).
  const std::vector<yaml::MiniYamlNode>& PlayerDefinitions() const {
    return vec_player_definitions_;
  }
  const std::vector<yaml::MiniYamlNode>& ActorDefinitions() const {
    return vec_actor_definitions_;
  }

  // 自定义 yaml(L225-232;null = C# null MiniYaml)| the custom yaml
  // (L225-232; null = C# null MiniYaml).
  const yaml::MiniYaml* RuleDefinitions() const {
    return ptr_rule_definitions_.get();
  }
  const yaml::MiniYaml* FluentMessageDefinitions() const {
    return ptr_fluent_message_definitions_.get();
  }
  const yaml::MiniYaml* SequenceDefinitions() const {
    return ptr_sequence_definitions_.get();
  }

  /// Ruleset::Load 的文件系统面(Map.Open/TryOpen/Exists 的闭包快照)
  /// The file-system face of Ruleset::Load (closures over Map.Open/...).
  game::MapFileSystemFace FileSystemFace() const;
  const yaml::MiniYaml* WeaponDefinitions() const {
    return ptr_weapon_definitions_.get();
  }
  const yaml::MiniYaml* VoiceDefinitions() const {
    return ptr_voice_definitions_.get();
  }
  const yaml::MiniYaml* NotificationDefinitions() const {
    return ptr_notification_definitions_.get();
  }
  const yaml::MiniYaml* MusicDefinitions() const {
    return ptr_music_definitions_.get();
  }
  const yaml::MiniYaml* ModelSequenceDefinitions() const {
    return ptr_model_sequence_definitions_.get();
  }

  /// ReplacedInvalidTerrainTiles(L234;插入序字典) | ReplacedInvalidTerrainTiles
  /// (L234; insertion-ordered).
  const std::vector<std::pair<CPos, TerrainTile>>& ReplacedInvalidTerrainTiles()
      const {
    return vec_replaced_invalid_terrain_tiles_;
  }

  // ———— 生成数据(L237-267)————
  MapGridType Type() const { return ptr_grid_->Type; }
  const MapGrid& Grid() const { return *ptr_grid_; }
  const fs::IReadOnlyPackage* Package() const { return ptr_package_; }
  const std::string& Uid() const { return str_uid_; }
  game::Ruleset& Rules() const { return *ptr_rules_; }
  gfx::SequenceSet* Sequences() const { return ptr_sequences_.get(); }

  bool InvalidCustomRules() const { return b_invalid_custom_rules_; }
  const std::exception_ptr& InvalidCustomRulesException() const {
    return ptr_invalid_custom_rules_exception_;
  }

  /// 投影区世界坐标(L247-257) | the projected-area world coordinates.
  WPos ProjectedTopLeft() const { return wpos_projected_top_left_; }
  WPos ProjectedBottomRight() const { return wpos_projected_bottom_right_; }

  const CellLayer<TerrainTile>& Tiles() const { return *ptr_tiles_; }
  CellLayer<TerrainTile>& TilesRef() { return *ptr_tiles_; }
  const CellLayer<ResourceTile>& Resources() const { return *ptr_resources_; }
  const CellLayer<std::uint8_t>& Height() const { return *ptr_height_; }
  const CellLayer<std::uint8_t>& Ramp() const { return *ptr_ramp_; }
  const CellLayer<std::uint8_t>& CustomTerrain() const {
    return *ptr_custom_terrain_;
  }
  /// 变更面(Locomotor.WorldLoaded 的 CustomTerrain.CellEntryChanged 挂载)
  /// The mutable face (Locomotor.WorldLoaded's CustomTerrain.
  /// CellEntryChanged wiring).
  CellLayer<std::uint8_t>& CustomTerrainRef() { return *ptr_custom_terrain_; }

  /// ProjectedCells(L265;SetBounds 装配) | ProjectedCells (L265).
  std::span<const PPos> ProjectedCells() const { return vec_projected_cells_; }
  const CellRegion& AllCells() const { return region_all_cells_; }
  const std::vector<CPos>& AllEdgeCells() const { return vec_all_edge_cells_; }

  // ———— UID(Map.cs L280-338)————
  static std::string ComputeUID(const fs::IReadOnlyPackage& package);
  static std::string ComputeUID(const fs::IReadOnlyPackage& package,
                                int format);
  static int GetMapFormat(const fs::IReadOnlyPackage& p);

  // ———— 存档二进制(Map.cs L708-765;Save(yaml 全量)随编辑器批)————
  std::vector<unsigned char> SaveBinaryData() const;

  // ———— 地形色对(L767-782;SavePreview 域消费)————
  std::pair<core::Color, core::Color> GetTerrainColorPair(MPos uv) const;

  // ———— 包含/换算族(L913-1114)————
  bool Contains(CPos cell) const;
  bool Contains(MPos uv) const;
  bool Contains(PPos puv) const;
  WPos CenterOfCell(CPos cell) const;
  WPos CenterOfSubCell(CPos cell, SubCell sub_cell) const;
  WDist DistanceAboveTerrain(WPos pos) const;
  WRot TerrainOrientation(CPos cell) const;
  WVec Offset(CVec delta, int dz) const;
  WDist CellHeightStep() const {
    return WDist{Type() == MapGridType::RectangularIsometric ? 724 : 512};
  }  // L1051
  CPos CellContaining(WPos pos) const;
  PPos ProjectedCellCovering(WPos pos) const;
  std::span<const PPos> ProjectedCellsCovering(MPos uv);
  std::vector<MPos> Unproject(PPos puv);
  std::uint8_t ProjectedHeight(PPos puv) const;
  WAngle FacingBetween(CPos cell, CPos towards, WAngle fallbackfacing) const;

  // ———— 尺寸/边界(L1116-1174)————
  void Resize(int width, int height);
  void SetBounds(PPos tl, PPos br);

  // ———— 地形索引(L1176-1210)————
  std::uint8_t GetTerrainIndex(CPos cell) const;
  std::uint8_t GetTerrainIndex(MPos uv) const;
  const TerrainTypeInfo& GetTerrainInfo(CPos cell) const;
  const TerrainTypeInfo& GetTerrainInfo(MPos uv) const;

  // ———— 夹取(L1212-1281)————
  CPos Clamp(CPos cell) const;
  MPos Clamp(MPos uv) const;
  PPos Clamp(PPos puv) const;

  // ———— 边格/随机格(L1283-1395)————
  CPos ChooseRandomCell(MersenneTwister& rand);
  CPos ChooseClosestEdgeCell(CPos cell) const;
  MPos ChooseClosestEdgeCell(MPos uv) const;
  CPos ChooseClosestMatchingEdgeCell(
      CPos cell, const std::function<bool(CPos)>& match) const;
  CPos ChooseRandomEdgeCell(MersenneTwister& rand) {
    return RandomOf(vec_all_edge_cells_, rand);
  }  // L1384-1387
  WDist DistanceToEdge(WPos pos, const WVec& dir) const;

  /// FindTilesInAnnulus(L1401-1429):yield → vector 物化(集合参数 D 系
  /// 惯例);range 校验两抛点逐字
  /// FindTilesInAnnulus (L1401-1429): the yield face materializes to a
  /// vector (the collection-parameter D-series convention); the two range
  /// checks keep their verbatim throws.
  std::vector<CPos> FindTilesInAnnulus(CPos center, int min_range,
                                       int max_range,
                                       bool allow_outside_bounds = false) const;
  std::vector<CPos> FindTilesInCircle(CPos center, int max_range,
                                      bool allow_outside_bounds = false) const {
    return FindTilesInAnnulus(center, 0, max_range, allow_outside_bounds);
  }

  // ———— IReadOnlyFileSystem 面(L1431-1475)————
  std::vector<char> Open(const std::string& filename) const;
  bool TryGetPackageContaining(const std::string& path,
                               const fs::IReadOnlyPackage*& package,
                               std::string& filename) const;
  bool TryOpen(const std::string& filename, std::vector<char>& bytes) const;
  bool Exists(const std::string& filename) const;
  bool IsExternalFile(const std::string& filename) const;

  /// Dispose(L1477-1480):Sequences.Dispose(C++ RAII 形态 = 释放)
  /// Dispose (L1477-1480): Sequences.Dispose (the C++ RAII form = release).
  void Dispose();

  /// CellProjectionChanged 事件(L269) | the CellProjectionChanged event.
  void AddCellProjectionChangedListener(std::function<void(CPos)> fn) {
    vec_cell_projection_changed_.push_back(std::move(fn));
  }

  /// Random(L1386 的 Exts.Random 等价) | the Exts.Random equivalent.
  static CPos RandomOf(std::span<const CPos> cells, MersenneTwister& rand);

 private:
  /// 事件表(L269 + L366-369 的挂载)| the callback tables.
  void FireCellProjectionChanged(CPos cell) {
    for (const auto& fn : vec_cell_projection_changed_)
      fn(cell);
  }

  /// PostInit(L457-515) | PostInit (L457-515).
  void PostInit();
  void UpdateRamp(CPos cell);  // L517-520
  /// InitializeCellProjection / UpdateProjection / ProjectedCellHeightInner /
  /// ProjectCellInner(L522-652)
  void InitializeCellProjection();
  void UpdateProjection(CPos cell);
  std::uint8_t ProjectedCellHeightInner(PPos puv) const;
  std::vector<PPos> ProjectCellInner(MPos uv) const;
  bool ContainsAllProjectedCellsCovering(MPos uv) const;  // L942-966
  std::vector<CPos> UpdateEdgeCells();                    // L1354-1382

  // ———— 投影数据(L273-278)————
  mutable std::unique_ptr<CellLayer<short>> ptr_cached_terrain_indexes_;
  bool b_initialized_cell_projection_ = false;
  std::unique_ptr<CellLayer<std::vector<PPos>>> ptr_cell_projection_;
  std::unique_ptr<CellLayer<std::vector<MPos>>> ptr_inverse_cell_projection_;
  std::unique_ptr<CellLayer<std::uint8_t>> ptr_projected_height_;
  Rectangle rect_projection_safe_bounds_{
      Rectangle::FromLTRB(0, 0, 0, 0)};

  // ———— 字段(L186-267 的存储)————
  std::int32_t int4_map_format_ = 0;
  std::uint8_t uint1_tile_format_ = 2;  // L187
  std::string str_requires_mod_;
  std::string str_title_;
  std::string str_author_;
  std::string str_tileset_;
  bool b_lock_preview_ = false;
  Rectangle rect_bounds_{Rectangle::FromLTRB(0, 0, 0, 0)};
  MapVisibility kind_visibility_{MapVisibility::Lobby};
  std::vector<std::string> vec_categories_{"Conquest"};
  bool b_hide_spawn_previews_ = false;
  Size size_map_{0, 0};

  std::vector<yaml::MiniYamlNode> vec_player_definitions_;
  std::vector<yaml::MiniYamlNode> vec_actor_definitions_;
  std::unique_ptr<yaml::MiniYaml> ptr_rule_definitions_;
  std::unique_ptr<yaml::MiniYaml> ptr_fluent_message_definitions_;
  std::unique_ptr<yaml::MiniYaml> ptr_sequence_definitions_;
  std::unique_ptr<yaml::MiniYaml> ptr_model_sequence_definitions_;
  std::unique_ptr<yaml::MiniYaml> ptr_weapon_definitions_;
  std::unique_ptr<yaml::MiniYaml> ptr_voice_definitions_;
  std::unique_ptr<yaml::MiniYaml> ptr_music_definitions_;
  std::unique_ptr<yaml::MiniYaml> ptr_notification_definitions_;
  yaml::StringPool pool_yaml_;  // map.yaml 解析池(节点字符串存活期)
                                // the map.yaml parse pool (string lifetime).

  std::vector<std::pair<CPos, TerrainTile>>
      vec_replaced_invalid_terrain_tiles_;

  MapGrid grid_default_{};  // GetOrCreate<MapGrid>() 的 C++ 形态:
                            // ModData 提供共享实例;本地默认承载测试构造
  MapGrid* ptr_grid_ = nullptr;

  const fs::IReadOnlyPackage* ptr_package_ = nullptr;
  std::string str_uid_;
  std::unique_ptr<game::Ruleset> ptr_rules_;
  std::unique_ptr<gfx::SequenceSet> ptr_sequences_;
  bool b_invalid_custom_rules_ = false;
  std::exception_ptr ptr_invalid_custom_rules_exception_;

  WPos wpos_projected_top_left_{0, 0, 0};
  WPos wpos_projected_bottom_right_{0, 0, 0};
  std::unique_ptr<CellLayer<TerrainTile>> ptr_tiles_;
  std::unique_ptr<CellLayer<ResourceTile>> ptr_resources_;
  std::unique_ptr<CellLayer<std::uint8_t>> ptr_height_;
  std::unique_ptr<CellLayer<std::uint8_t>> ptr_ramp_;
  std::unique_ptr<CellLayer<std::uint8_t>> ptr_custom_terrain_;
  std::vector<PPos> vec_projected_cells_;
  CellRegion region_all_cells_{MapGridType::Rectangular, CPos::Zero(),
                               CPos::Zero()};
  std::vector<CPos> vec_all_edge_cells_;

  std::vector<std::function<void(CPos)>> vec_cell_projection_changed_;

  Params params_;
};

// ———— CellLayerBase<T>(const Map&) 的延后定义(cell_layer.hpp 声明,
// 本处 Map 完整 —— 循环包含的拆解点;定义须落在类所属的 ora::map 命名
// 空间内 —— 本文件尾仍处于其中)
// The deferred definition of CellLayerBase<T>(const Map&) (declared in
// cell_layer.hpp, defined here where Map is complete — the include-cycle
// break point; the definition must live in the class's own ora::map
// namespace, which is still open at this file's tail).
template <class T>
CellLayerBase<T>::CellLayerBase(const Map& map_world)
    : size_{map_world.MapSize()},
      bounds_{Rectangle::FromLTRB(0, 0, size_.Width, size_.Height)},
      grid_type_{map_world.Grid().Type},
      vec_entries_(static_cast<std::size_t>(size_.Width) *
                   static_cast<std::size_t>(size_.Height)) {}

}  // namespace ora::map