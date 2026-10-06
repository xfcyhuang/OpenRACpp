// UPSTREAM: OpenRA.Game/Map/MapGrid.cs @b6fc03f L20-256(逐语义重写)
//          Verbatim-semantics rewrite.
//
// 机制对照 / Mechanism mapping:
//  - CellRamp 的 ImmutableArray → 固定 std::array;HeightOffset 的
//    "do-while 多边形三角化"逐控制流(含未劈分时取前三顶点)
//    CellRamp's ImmutableArray becomes fixed std::arrays; HeightOffset's
//    "do-while polygon triangulation" keeps the control flow (including
//    the unsplit three-first-vertices case).
//  - MapGrid 字段加载(FieldLoader.Load)→ 手写字段表(Name/Type/MaximumTerrain
//    Height/DefaultSubCell/MaximumTileSearchRange/EnableDepthBuffer;
//    SubCellOffsets/Ramps 为常量,上游 [FieldLoader.Ignore] 语义)
//    MapGrid's FieldLoader.Load becomes a hand-written field table
//    (SubCellOffsets/Ramps are constants — the upstream [FieldLoader.Ignore]
//    semantics).
#pragma once
import std;

#include "core/cell_pos.hpp"
#include "core/size.hpp"
#include "core/wangle.hpp"
#include "core/wrot.hpp"
#include "core/wvec.hpp"
#include "yaml/mini_yaml.hpp"

namespace ora::map {

// MapGridType 见 core/cell_pos.hpp(MapGrid.cs L20 的一处权威定义)
// MapGridType lives in core/cell_pos.hpp (the single authoritative
// definition of MapGrid.cs L20).

/// RampSplit / RampCornerHeight(MapGrid.cs L22-23)
/// RampSplit / RampCornerHeight (MapGrid.cs L22-23).
enum class RampSplit : std::int32_t { Flat, X, Y };
enum class RampCornerHeight : std::int32_t { Low = 0, Half = 1, Full = 2 };

/// CellRamp(MapGrid.cs L25-108):斜坡角高/多边形劈分/高度插值
/// CellRamp (MapGrid.cs L25-108): ramp corner heights / polygon splits /
/// height interpolation.
struct CellRamp {
  std::array<WVec, 4> arr_corners{};
  std::array<std::array<WVec, 3>, 2> arr_polygons{};
  std::int32_t int4_polygon_count = 1;
  WRot orientation{WAngle{0}, WAngle{0}, WAngle{0}};
  std::int32_t int4_center_height_offset = 0;

  CellRamp() = default;
  CellRamp(MapGridType type, WRot rot_orientation,
           RampCornerHeight tl = RampCornerHeight::Low,
           RampCornerHeight tr = RampCornerHeight::Low,
           RampCornerHeight br = RampCornerHeight::Low,
           RampCornerHeight bl = RampCornerHeight::Low,
           RampSplit split = RampSplit::Flat);

  /// HeightOffset(L83-107):逐多边形三角形判定(0<=u,v<=1024),w 由
  /// u,v 插值 —— 整数除法向零截断与上游一致
  /// HeightOffset (L83-107): per-polygon triangle containment
  /// (0<=u,v<=1024); w interpolates from u,v — integer division truncates
  /// toward zero exactly as upstream.
  std::int32_t HeightOffset(std::int32_t d_x, std::int32_t d_y) const;
};

/// MapGrid(MapGrid.cs L110):IGlobalModData 等价(mod.yaml Grid 节点)
/// MapGrid (MapGrid.cs L110): the IGlobalModData equivalent (the mod.yaml
/// Grid node).
class MapGrid final {
 public:
  MapGridType Type{MapGridType::Rectangular};                 // L112
  std::uint8_t uint1_maximum_terrain_height{0};               // L113
  std::uint8_t uint1_default_sub_cell{255};                   // L114
  std::int32_t int4_maximum_tile_search_range{50};            // L116
  bool b_enable_depth_buffer{false};                          // L118

  /// SubCellOffsets(L120-128):常量表 | SubCellOffsets (L120-128): constants.
  static constexpr std::array<WVec, 6> kSubCellOffsets{
      WVec{0, 0, 0},        // full cell - index 0
      WVec{-299, -256, 0},  // top left - index 1
      WVec{256, -256, 0},   // top right - index 2
      WVec{0, 0, 0},        // center - index 3
      WVec{-299, 256, 0},   // bottom left - index 4
      WVec{256, 256, 0},    // bottom right - index 5
  };

  /// MapGrid(MiniYaml)(L136-203):字段加载 + DefaultSubCell 归中校验 +
  /// 21 个斜坡常量 + TilesByDistance
  /// MapGrid (MiniYaml) (L136-203): field loading + the DefaultSubCell
  /// centering/validation + the 21 ramp constants + TilesByDistance.
  explicit MapGrid(const yaml::MiniYaml& yaml);

  /// 测试/无 yaml 直构(默认值面)| the test/no-yaml form (defaults).
  MapGrid() { InitDerived(); }

  std::int32_t TileScale() const {
    return Type == MapGridType::RectangularIsometric ? 1448 : 1024;
  }  // L140

  std::uint8_t MaximumTerrainHeight() const {
    return uint1_maximum_terrain_height;
  }
  std::uint8_t DefaultSubCell() const { return uint1_default_sub_cell_; }
  std::int32_t MaximumTileSearchRange() const {
    return int4_maximum_tile_search_range;
  }
  bool EnableDepthBuffer() const { return b_enable_depth_buffer; }

  const std::array<CellRamp, 21>& Ramps() const { return arr_ramps_; }

  /// TilesByDistance(L205-242):ISqrt(Ceiling) 分桶 + 组内四键稳定排序
  /// (LengthSquared → GetHashCode → X → Y)
  /// TilesByDistance (L205-242): ISqrt(Ceiling) buckets + the in-group
  /// four-key stable sort (LengthSquared → GetHashCode → X → Y).
  const std::vector<std::vector<CVec>>& TilesByDistance() const {
    return vec_tiles_by_distance_;
  }

  /// OffsetOfSubCell(L244-254) | OffsetOfSubCell (L244-254).
  WVec OffsetOfSubCell(SubCell sub_cell) const {
    if (sub_cell == SubCell::Invalid || sub_cell == SubCell::Any)
      return WVec{0, 0, 0};

    const int index = static_cast<int>(sub_cell);
    if (index >= 0 && index < static_cast<int>(kSubCellOffsets.size()))
      return kSubCellOffsets[index];

    return WVec{0, 0, 0};
  }

 private:
  void InitDerived();

  std::uint8_t uint1_default_sub_cell_ = 255;
  std::array<CellRamp, 21> arr_ramps_{};
  std::vector<std::vector<CVec>> vec_tiles_by_distance_;
};

}  // namespace ora::map
