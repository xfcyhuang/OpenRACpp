#pragma once
import std;

#include "core/cell_pos.hpp"
#include "core/wdist.hpp"
#include "core/wpos.hpp"
#include "map/cell_layer.hpp"
#include "map/map.hpp"
#include "sim/sync_hash.hpp"
#include "sim/trait_interfaces.hpp"

namespace ora::sim {

enum class ShroudSourceType : std::uint8_t {
  PassiveVisibility = 0,
  Shroud = 1,
  Visibility = 2,
};

enum class CellVisibility : std::uint8_t {
  Hidden = 0x0,
  Explored = 0x1,
  Visible = 0x2,
};

inline bool HasCellVisibility(CellVisibility state, CellVisibility flag) {
  return (static_cast<std::uint8_t>(state) &
          static_cast<std::uint8_t>(flag)) != 0;
}

struct ShroudInfoData {
  bool b_fog_checkbox_enabled = true;
  bool b_explored_map_checkbox_enabled = false;
};

class Shroud final : public TraitBase,
                     public INotifyCreated,
                     public ITick,
                     public ISync {
 public:
  Shroud(Actor& self, const ShroudInfoData& info);

  ORA_TRAIT_INTERFACES(Shroud, OpenRA_Traits_Shroud, INotifyCreated, ITick,
                       ISync)

  int RevealedCells() const { return int4_revealed_cells_; }

  bool Disabled() const { return b_disabled_; }
  void SetDisabled(bool value);

  bool FogEnabled() const { return !Disabled() && b_fog_enabled_; }
  bool ExploreMapEnabled() const { return b_explore_map_enabled_; }

  int Hash() const { return int4_hash_; }

  void AddShroudChangedCallback(std::function<void(PPos)> fn) {
    vec_on_shroud_changed_.push_back(std::move(fn));
  }

  std::size_t SubscribeShroudChanged(std::function<void(PPos)> fn) {
    vec_on_shroud_changed_.push_back(std::move(fn));
    return vec_on_shroud_changed_.size() - 1;
  }
  void RemoveShroudChangedCallback(std::size_t n_token) {
    if (n_token < vec_on_shroud_changed_.size())
      vec_on_shroud_changed_[n_token] = nullptr;
  }

  void Created(Actor& self) override;
  void Tick(Actor& self) override;

  static std::vector<PPos> ProjectedCellsInRange(const map::Map& map,
                                                 const WPos& pos,
                                                 const WDist& min_range,
                                                 const WDist& max_range,
                                                 int max_height_delta = -1);

  static std::vector<PPos> ProjectedCellsInRange(const map::Map& map,
                                                 const CPos& cell,
                                                 const WDist& range,
                                                 int max_height_delta = -1);

  void AddSource(const void* key, ShroudSourceType type,
                 std::vector<PPos> vec_projected_cells);

  void RemoveSource(const void* key);

  void ExploreProjectedCells(const std::vector<PPos>& vec_cells);

  void Explore(const Shroud& s);

  void ExploreAll();

  void ResetExploration();

  bool IsExplored(const WPos& pos) const;
  bool IsExplored(const CPos& cell) const;
  bool IsExplored(const MPos& uv) const;
  bool IsExplored(const PPos& puv) const;

  bool IsVisible(const WPos& pos) const;
  bool IsVisible(const CPos& cell) const;
  bool IsVisible(const MPos& uv) const;
  bool IsVisible(const PPos& puv) const;

  bool Contains(const PPos& uv) const;

  CellVisibility GetVisibility(const WPos& pos) const;
  CellVisibility GetVisibility(const PPos& puv) const;

 private:

  struct ShroudSource {
    ShroudSourceType type;
    std::vector<PPos> vec_projected_cells;
  };

  enum class ShroudCellType : std::uint8_t { Shroud = 0, Fog = 1, Visible = 2 };

  void UpdateCell(int index, Actor& self);

  ShroudInfoData info_;
  map::Map& map_;

  std::map<const void*, ShroudSource> map_sources_;

  map::ProjectedCellLayer<std::int16_t> layer_passive_visible_count_;
  map::ProjectedCellLayer<std::int16_t> layer_visible_count_;
  map::ProjectedCellLayer<std::int16_t> layer_generated_shroud_count_;
  map::ProjectedCellLayer<bool> layer_explored_;
  map::ProjectedCellLayer<bool> layer_touched_;
  bool b_any_cell_touched_ = true;

  map::ProjectedCellLayer<ShroudCellType> layer_resolved_type_;

  bool b_disabled_changed_ = false;
  bool b_disabled_ = false;
  bool b_fog_enabled_ = false;
  bool b_explore_map_enabled_ = false;
  int int4_revealed_cells_ = 0;
  int int4_hash_ = 0;

  bool b_shroud_generation_enabled_ = false;
  bool b_passive_visibility_enabled_ = false;

  std::vector<std::function<void(PPos)>> vec_on_shroud_changed_;
};

}
