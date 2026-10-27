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
}
namespace ora::gfx {
class SequenceSet;
}
namespace ora::sim {
class Actor;
}

namespace ora::map {

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

constexpr bool HasFlag(MapVisibility v, MapVisibility flag) {
  return (static_cast<std::int32_t>(v) & static_cast<std::int32_t>(flag)) ==
         static_cast<std::int32_t>(flag);
}

struct BinaryDataHeader {
  std::uint8_t Format{0};
  std::uint32_t TilesOffset{0};
  std::uint32_t HeightsOffset{0};
  std::uint32_t ResourcesOffset{0};

  BinaryDataHeader(std::span<const char> s, Size expected_size);
};

class Map final {
 public:
  static constexpr int kSupportedMapFormat = 11;
  static constexpr int kCurrentMapFormat = 12;
  static constexpr short kInvalidCachedTerrainIndex = -1;

  struct Params {
    game::ModData* mod_data = nullptr;

    std::function<std::unique_ptr<gfx::SequenceSet>(Map&)> fn_sequences_factory;
  };

  Map(Params params, const ITerrainInfo& terrain_info, Size size);

  Map(Params params, const fs::IReadOnlyPackage& package);

  ~Map();

  Map(const Map&) = delete;
  Map& operator=(const Map&) = delete;

  std::int32_t MapFormat() const { return int4_map_format_; }
  std::uint8_t TileFormat() const { return uint1_tile_format_; }
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
  const Size& MapSize() const { return size_map_; }

  const std::vector<yaml::MiniYamlNode>& PlayerDefinitions() const {
    return vec_player_definitions_;
  }
  const std::vector<yaml::MiniYamlNode>& ActorDefinitions() const {
    return vec_actor_definitions_;
  }

  const yaml::MiniYaml* RuleDefinitions() const {
    return ptr_rule_definitions_.get();
  }
  const yaml::MiniYaml* FluentMessageDefinitions() const {
    return ptr_fluent_message_definitions_.get();
  }
  const yaml::MiniYaml* SequenceDefinitions() const {
    return ptr_sequence_definitions_.get();
  }

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

  const std::vector<std::pair<CPos, TerrainTile>>& ReplacedInvalidTerrainTiles()
      const {
    return vec_replaced_invalid_terrain_tiles_;
  }

  MapGridType Type() const { return ptr_grid_->Type; }
  const MapGrid& Grid() const { return *ptr_grid_; }
  const fs::IReadOnlyPackage* Package() const { return ptr_package_; }
  const std::string& Uid() const { return str_uid_; }
  game::Ruleset& Rules() const { return *ptr_rules_; }
  gfx::SequenceSet* Sequences() const { return ptr_sequences_.get(); }

  void SetSequences(std::unique_ptr<gfx::SequenceSet> up_sequences);

  bool InvalidCustomRules() const { return b_invalid_custom_rules_; }
  const std::exception_ptr& InvalidCustomRulesException() const {
    return ptr_invalid_custom_rules_exception_;
  }

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

  CellLayer<std::uint8_t>& CustomTerrainRef() { return *ptr_custom_terrain_; }

  std::span<const PPos> ProjectedCells() const { return vec_projected_cells_; }
  const CellRegion& AllCells() const { return region_all_cells_; }
  const std::vector<CPos>& AllEdgeCells() const { return vec_all_edge_cells_; }

  static std::string ComputeUID(const fs::IReadOnlyPackage& package);
  static std::string ComputeUID(const fs::IReadOnlyPackage& package,
                                int format);
  static int GetMapFormat(const fs::IReadOnlyPackage& p);

  std::vector<unsigned char> SaveBinaryData() const;

  std::vector<std::byte> SavePreview();

  std::pair<int, int> GetCellSpaceBounds();

  std::pair<core::Color, core::Color> GetTerrainColorPair(MPos uv) const;

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
  }
  CPos CellContaining(WPos pos) const;
  PPos ProjectedCellCovering(WPos pos) const;
  std::span<const PPos> ProjectedCellsCovering(MPos uv);
  std::vector<MPos> Unproject(PPos puv);
  std::uint8_t ProjectedHeight(PPos puv) const;
  WAngle FacingBetween(CPos cell, CPos towards, WAngle fallbackfacing) const;

  void Resize(int width, int height);
  void SetBounds(PPos tl, PPos br);

  std::uint8_t GetTerrainIndex(CPos cell) const;
  std::uint8_t GetTerrainIndex(MPos uv) const;
  const TerrainTypeInfo& GetTerrainInfo(CPos cell) const;
  const TerrainTypeInfo& GetTerrainInfo(MPos uv) const;

  CPos Clamp(CPos cell) const;
  MPos Clamp(MPos uv) const;
  PPos Clamp(PPos puv) const;

  CPos ChooseRandomCell(MersenneTwister& rand);
  CPos ChooseClosestEdgeCell(CPos cell) const;
  MPos ChooseClosestEdgeCell(MPos uv) const;
  CPos ChooseClosestMatchingEdgeCell(
      CPos cell, const std::function<bool(CPos)>& match) const;
  CPos ChooseRandomEdgeCell(MersenneTwister& rand) {
    return RandomOf(vec_all_edge_cells_, rand);
  }
  WDist DistanceToEdge(WPos pos, const WVec& dir) const;

  std::vector<CPos> FindTilesInAnnulus(CPos center, int min_range,
                                       int max_range,
                                       bool allow_outside_bounds = false) const;
  std::vector<CPos> FindTilesInCircle(CPos center, int max_range,
                                      bool allow_outside_bounds = false) const {
    return FindTilesInAnnulus(center, 0, max_range, allow_outside_bounds);
  }

  std::vector<char> Open(const std::string& filename) const;
  bool TryGetPackageContaining(const std::string& path,
                               const fs::IReadOnlyPackage*& package,
                               std::string& filename) const;
  bool TryOpen(const std::string& filename, std::vector<char>& bytes) const;
  bool Exists(const std::string& filename) const;
  bool IsExternalFile(const std::string& filename) const;

  void Dispose();

  void AddCellProjectionChangedListener(std::function<void(CPos)> fn) {
    vec_cell_projection_changed_.push_back(std::move(fn));
  }

  static CPos RandomOf(std::span<const CPos> cells, MersenneTwister& rand);

 private:

  void FireCellProjectionChanged(CPos cell) {
    for (const auto& fn : vec_cell_projection_changed_)
      fn(cell);
  }

  void PostInit();
  void UpdateRamp(CPos cell);

  void InitializeCellProjection();
  void UpdateProjection(CPos cell);
  std::uint8_t ProjectedCellHeightInner(PPos puv) const;
  std::vector<PPos> ProjectCellInner(MPos uv) const;
  bool ContainsAllProjectedCellsCovering(MPos uv) const;
  std::vector<CPos> UpdateEdgeCells();

  mutable std::unique_ptr<CellLayer<short>> ptr_cached_terrain_indexes_;
  bool b_initialized_cell_projection_ = false;
  std::unique_ptr<CellLayer<std::vector<PPos>>> ptr_cell_projection_;
  std::unique_ptr<CellLayer<std::vector<MPos>>> ptr_inverse_cell_projection_;
  std::unique_ptr<CellLayer<std::uint8_t>> ptr_projected_height_;
  Rectangle rect_projection_safe_bounds_{
      Rectangle::FromLTRB(0, 0, 0, 0)};

  std::int32_t int4_map_format_ = 0;
  std::uint8_t uint1_tile_format_ = 2;
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
  yaml::StringPool pool_yaml_;

  std::vector<std::pair<CPos, TerrainTile>>
      vec_replaced_invalid_terrain_tiles_;

  MapGrid grid_default_{};

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

template <class T>
CellLayerBase<T>::CellLayerBase(const Map& map_world)
    : size_{map_world.MapSize()},
      bounds_{Rectangle::FromLTRB(0, 0, size_.Width, size_.Height)},
      grid_type_{map_world.Grid().Type},
      vec_entries_(static_cast<std::size_t>(size_.Width) *
                   static_cast<std::size_t>(size_.Height)) {}

}
