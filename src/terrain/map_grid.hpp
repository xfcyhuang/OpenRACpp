#pragma once
import std;

#include "core/cell_pos.hpp"
#include "core/size.hpp"
#include "core/wangle.hpp"
#include "core/wrot.hpp"
#include "core/wvec.hpp"
#include "yaml/mini_yaml.hpp"

namespace ora::map {

enum class RampSplit : std::int32_t { Flat, X, Y };
enum class RampCornerHeight : std::int32_t { Low = 0, Half = 1, Full = 2 };

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

  std::int32_t HeightOffset(std::int32_t d_x, std::int32_t d_y) const;
};

class MapGrid final {
 public:
  MapGridType Type{MapGridType::Rectangular};
  std::uint8_t uint1_maximum_terrain_height{0};
  std::uint8_t uint1_default_sub_cell{255};
  std::int32_t int4_maximum_tile_search_range{50};
  bool b_enable_depth_buffer{false};

  static constexpr std::array<WVec, 6> kSubCellOffsets{
      WVec{0, 0, 0},
      WVec{-299, -256, 0},
      WVec{256, -256, 0},
      WVec{0, 0, 0},
      WVec{-299, 256, 0},
      WVec{256, 256, 0},
  };

  explicit MapGrid(const yaml::MiniYaml& yaml);

  MapGrid() { InitDerived(); }

  std::int32_t TileScale() const {
    return Type == MapGridType::RectangularIsometric ? 1448 : 1024;
  }

  std::uint8_t MaximumTerrainHeight() const {
    return uint1_maximum_terrain_height;
  }
  std::uint8_t DefaultSubCell() const { return uint1_default_sub_cell_; }
  std::int32_t MaximumTileSearchRange() const {
    return int4_maximum_tile_search_range;
  }
  bool EnableDepthBuffer() const { return b_enable_depth_buffer; }

  const std::array<CellRamp, 21>& Ramps() const { return arr_ramps_; }

  const std::vector<std::vector<CVec>>& TilesByDistance() const {
    return vec_tiles_by_distance_;
  }

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

}
