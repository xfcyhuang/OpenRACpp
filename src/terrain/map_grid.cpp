import std;

#include "terrain/map_grid.hpp"

#include "meta/field_loader.hpp"
#include "meta/parse.hpp"

namespace ora::map {

void RegisterMapEnums() {
  static constexpr meta::EnumMemberDesc kMapGridType[] = {
      {.str_name = "Rectangular", .int4_value = 0},
      {.str_name = "RectangularIsometric", .int4_value = 1}};
  static constexpr meta::EnumMemberDesc kMapVisibility[] = {
      {.str_name = "Lobby", .int4_value = 1},
      {.str_name = "Shellmap", .int4_value = 2},
      {.str_name = "MissionSelector", .int4_value = 4}};
  static const bool b_done = [] {
    meta::RegisterEnum("OpenRA.MapGridType", kMapGridType);
    meta::RegisterEnum("OpenRA.MapVisibility", kMapVisibility);
    return true;
  }();
  (void)b_done;
}

}

namespace ora::map {

CellRamp::CellRamp(MapGridType type, WRot rot_orientation, RampCornerHeight tl,
                   RampCornerHeight tr, RampCornerHeight br,
                   RampCornerHeight bl, RampSplit split)
    : orientation{rot_orientation} {
  const std::int32_t int4_tl = static_cast<std::int32_t>(tl);
  const std::int32_t int4_tr = static_cast<std::int32_t>(tr);
  const std::int32_t int4_br = static_cast<std::int32_t>(br);
  const std::int32_t int4_bl = static_cast<std::int32_t>(bl);

  if (type == MapGridType::RectangularIsometric) {
    arr_corners = {
        WVec{0, -724, 724 * int4_tl},
        WVec{724, 0, 724 * int4_tr},
        WVec{0, 724, 724 * int4_br},
        WVec{-724, 0, 724 * int4_bl},
    };
  } else {
    arr_corners = {
        WVec{-512, -512, 512 * int4_tl},
        WVec{512, -512, 512 * int4_tr},
        WVec{512, 512, 512 * int4_br},
        WVec{-512, 512, 512 * int4_bl},
    };
  }

  if (split == RampSplit::X) {
    int4_polygon_count = 2;
    arr_polygons[0] = {arr_corners[0], arr_corners[1], arr_corners[3]};
    arr_polygons[1] = {arr_corners[1], arr_corners[2], arr_corners[3]};
  } else if (split == RampSplit::Y) {
    int4_polygon_count = 2;
    arr_polygons[0] = {arr_corners[0], arr_corners[1], arr_corners[2]};
    arr_polygons[1] = {arr_corners[0], arr_corners[2], arr_corners[3]};
  } else {
    int4_polygon_count = 1;
    arr_polygons[0] = {arr_corners[0], arr_corners[1], arr_corners[2]};
  }

  int4_center_height_offset = 0;
  int4_center_height_offset = HeightOffset(0, 0);
}

std::int32_t CellRamp::HeightOffset(std::int32_t d_x, std::int32_t d_y) const {

  std::int32_t u = 0;
  std::int32_t v = 0;
  const std::array<WVec, 3>* ptr_p = &arr_polygons[0];
  std::size_t i = 0;
  do {
    ptr_p = &arr_polygons[i];
    u = ((*ptr_p)[1].Y - (*ptr_p)[2].Y) * (d_x - (*ptr_p)[2].X) -
        ((*ptr_p)[1].X - (*ptr_p)[2].X) * (d_y - (*ptr_p)[2].Y);
    u /= 1024;
    v = ((*ptr_p)[0].X - (*ptr_p)[2].X) * (d_y - (*ptr_p)[2].Y) -
        ((*ptr_p)[0].Y - (*ptr_p)[2].Y) * (d_x - (*ptr_p)[2].X);
    v /= 1024;

    if (u >= 0 && u <= 1024 && v >= 0 && v <= 1024)
      break;

    ++i;
  } while (i < static_cast<std::size_t>(int4_polygon_count));

  return (u * (*ptr_p)[0].Z + v * (*ptr_p)[1].Z +
          (1024 - u - v) * (*ptr_p)[2].Z) /
         1024;
}

void MapGrid::InitDerived() {

  const std::uint8_t default_sub_cell_index = uint1_default_sub_cell;
  if (default_sub_cell_index == 255) {

    uint1_default_sub_cell_ =
        static_cast<std::uint8_t>(kSubCellOffsets.size() / 2);
  } else {
    const std::uint8_t min_sub_cell_offset =
        kSubCellOffsets.size() > 1 ? 1 : 0;
    if (default_sub_cell_index < min_sub_cell_offset ||
        default_sub_cell_index >= kSubCellOffsets.size())
      throw std::runtime_error(
          "Subcell default index must be a valid index into the offset "
          "triples and must be greater than 0 for mods with subcells");
  }

  const WVec south_east{724, 724, 0};
  const WVec south_west{-724, 724, 0};
  const WVec south{0, 1024, 0};
  const WVec east{1024, 0, 0};

  const WAngle forward{64};
  const WAngle backward = -forward;
  const WAngle half_forward{48};
  const WAngle half_backward = -half_forward;

  arr_ramps_ = {

      CellRamp{Type, WRot::None()},

      CellRamp{Type, WRot{south_east, backward}, RampCornerHeight::Low,
               RampCornerHeight::Half, RampCornerHeight::Half},
      CellRamp{Type, WRot{south_west, backward}, RampCornerHeight::Low,
               RampCornerHeight::Low, RampCornerHeight::Half,
               RampCornerHeight::Half},
      CellRamp{Type, WRot{south_east, forward}, RampCornerHeight::Half,
               RampCornerHeight::Low, RampCornerHeight::Low,
               RampCornerHeight::Half},
      CellRamp{Type, WRot{south_west, forward}, RampCornerHeight::Half,
               RampCornerHeight::Half},

      CellRamp{Type, WRot{south, half_backward}, RampCornerHeight::Low,
               RampCornerHeight::Low, RampCornerHeight::Half,
               RampCornerHeight::Low, RampSplit::X},
      CellRamp{Type, WRot{east, half_forward}, RampCornerHeight::Low,
               RampCornerHeight::Low, RampCornerHeight::Low,
               RampCornerHeight::Half, RampSplit::Y},
      CellRamp{Type, WRot{south, half_forward}, RampCornerHeight::Half,
               RampCornerHeight::Low, RampCornerHeight::Low,
               RampCornerHeight::Low, RampSplit::X},
      CellRamp{Type, WRot{east, half_backward}, RampCornerHeight::Low,
               RampCornerHeight::Half, RampCornerHeight::Low,
               RampCornerHeight::Low, RampSplit::Y},

      CellRamp{Type, WRot{south, half_backward}, RampCornerHeight::Low,
               RampCornerHeight::Half, RampCornerHeight::Half,
               RampCornerHeight::Half, RampSplit::X},
      CellRamp{Type, WRot{east, half_forward}, RampCornerHeight::Half,
               RampCornerHeight::Low, RampCornerHeight::Half,
               RampCornerHeight::Half, RampSplit::Y},
      CellRamp{Type, WRot{south, half_forward}, RampCornerHeight::Half,
               RampCornerHeight::Half, RampCornerHeight::Low,
               RampCornerHeight::Half, RampSplit::X},
      CellRamp{Type, WRot{east, half_backward}, RampCornerHeight::Half,
               RampCornerHeight::Half, RampCornerHeight::Half,
               RampCornerHeight::Low, RampSplit::Y},

      CellRamp{Type, WRot{south, backward}, RampCornerHeight::Low,
               RampCornerHeight::Half, RampCornerHeight::Full,
               RampCornerHeight::Half},
      CellRamp{Type, WRot{east, forward}, RampCornerHeight::Half,
               RampCornerHeight::Low, RampCornerHeight::Half,
               RampCornerHeight::Full},
      CellRamp{Type, WRot{south, forward}, RampCornerHeight::Full,
               RampCornerHeight::Half, RampCornerHeight::Low,
               RampCornerHeight::Half},
      CellRamp{Type, WRot{east, backward}, RampCornerHeight::Half,
               RampCornerHeight::Full, RampCornerHeight::Half,
               RampCornerHeight::Low},

      CellRamp{Type, WRot::None(), RampCornerHeight::Low,
               RampCornerHeight::Half, RampCornerHeight::Low,
               RampCornerHeight::Half, RampSplit::Y},
      CellRamp{Type, WRot::None(), RampCornerHeight::Half,
               RampCornerHeight::Low, RampCornerHeight::Half,
               RampCornerHeight::Low, RampSplit::Y},
      CellRamp{Type, WRot::None(), RampCornerHeight::Low,
               RampCornerHeight::Half, RampCornerHeight::Low,
               RampCornerHeight::Half, RampSplit::X},
      CellRamp{Type, WRot::None(), RampCornerHeight::Half,
               RampCornerHeight::Low, RampCornerHeight::Half,
               RampCornerHeight::Low, RampSplit::X},
  };

  vec_tiles_by_distance_ = [&] {

    std::vector<std::vector<CVec>> ts(
        static_cast<std::size_t>(int4_maximum_tile_search_range) + 1);
    for (auto j = -int4_maximum_tile_search_range;
         j <= int4_maximum_tile_search_range; j++)
      for (auto i = -int4_maximum_tile_search_range;
           i <= int4_maximum_tile_search_range; i++)
        if (int4_maximum_tile_search_range * int4_maximum_tile_search_range >=
            i * i + j * j)
          ts[static_cast<std::size_t>(ora::ISqrt(
                 static_cast<std::uint32_t>(i * i + j * j),
                 ora::ISqrtRoundMode::Ceiling))]
              .emplace_back(i, j);

    for (auto& list : ts) {
      std::ranges::sort(list, [](const CVec& a, const CVec& b) {
        const std::int32_t result = a.LengthSquared() < b.LengthSquared() ? -1
                                    : a.LengthSquared() > b.LengthSquared()
                                        ? 1
                                        : 0;
        if (result != 0)
          return result < 0;

        const std::int32_t result_hash = a.Hash() < b.Hash() ? -1
                                         : a.Hash() > b.Hash() ? 1
                                                               : 0;
        if (result_hash != 0)
          return result_hash < 0;

        if (a.X != b.X)
          return a.X < b.X;

        return a.Y < b.Y;
      });
    }

    return ts;
  }();
}

MapGrid::MapGrid(const yaml::MiniYaml& yaml) {
  RegisterMapEnums();

  const auto read = [&](std::string_view key) -> const yaml::MiniYamlNode* {
    return yaml.NodeWithKeyOrDefault(key);
  };
  const auto value_of = [](const yaml::MiniYamlNode& node) -> std::string_view {
    return node.Value.Value != nullptr ? std::string_view{*node.Value.Value}
                                       : std::string_view{};
  };

  if (const auto* node = read("Type"))
    Type = static_cast<MapGridType>(meta::GetEnumValue(
        "Type", value_of(*node), "OpenRA.MapGridType"));
  if (const auto* node = read("MaximumTerrainHeight"))
    uint1_maximum_terrain_height =
        static_cast<std::uint8_t>(meta::GetInt32Value(
            "MaximumTerrainHeight", value_of(*node)));
  if (const auto* node = read("DefaultSubCell"))
    uint1_default_sub_cell = static_cast<std::uint8_t>(
        meta::GetInt32Value("DefaultSubCell", value_of(*node)));
  if (const auto* node = read("MaximumTileSearchRange"))
    int4_maximum_tile_search_range = meta::GetInt32Value(
        "MaximumTileSearchRange", value_of(*node));
  if (const auto* node = read("EnableDepthBuffer"))
    b_enable_depth_buffer =
        meta::GetBoolValue("EnableDepthBuffer", value_of(*node));

  InitDerived();
}

}
