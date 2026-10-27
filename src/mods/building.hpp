#pragma once
import std;

#include "core/cell_pos.hpp"
#include "core/cvec.hpp"
#include "core/wpos.hpp"
#include "sim/actor.hpp"
#include "sim/trait_interfaces.hpp"

namespace ora::meta {
class RecordObject;
}

namespace ora::sim {
class ActorInitializer;
class World;
}

namespace ora::game {
class ActorInfo;
}

namespace ora::mods {

enum class FootprintCellType : std::int32_t {
  Empty = '_',
  OccupiedPassable = '=',
  Occupied = 'x',
  OccupiedUntargetable = 'X',
  OccupiedPassableTransitOnly = '+',
};

class BuildingInfoData {
 public:
  std::vector<std::string> vec_terrain_types;
  std::vector<std::pair<CVec, FootprintCellType>> vec_footprint{
      {CVec{0, 0}, FootprintCellType::Occupied}};
  CVec dimensions{1, 1};
  WVec local_center_offset{0, 0, 0};
  bool b_requires_base_provider = false;
  bool b_allow_invalid_placement = false;
  bool b_remove_smudges_on_build = true;
  bool b_remove_smudges_on_sell = true;
  bool b_remove_smudges_on_transform = true;
  std::vector<std::string> vec_build_sounds;
  std::vector<std::string> vec_undeploy_sounds;

  static BuildingInfoData Parse(const meta::RecordObject& rec_info);

  std::vector<CPos> FootprintTiles(CPos location,
                                   FootprintCellType type) const;

  std::vector<CPos> Tiles(CPos location) const;

  std::vector<CPos> FrozenUnderFogTiles(CPos location) const;

  std::vector<CPos> OccupiedTiles(CPos location) const;

  std::vector<CPos> PathableTiles(CPos location) const;

  std::vector<CPos> TransitOnlyTiles(CPos location) const;

  WVec CenterOffset(sim::World& w) const;

  const void* FindBaseProvider(sim::World& ,
                               sim::Player& ,
                               CPos ) const {
    return nullptr;
  }

  bool IsCloseEnoughToBase(sim::World& , sim::Player& ,
                           const game::ActorInfo& ,
                           CPos ) const {
    return true;
  }

  std::vector<std::pair<CPos, SubCell>> OccupiedCellsInfo(
      CPos top_left) const;
};

class Building final : public sim::TraitBase,
                       public sim::IOccupySpace,
                       public sim::ITargetableCells,
                       public sim::ISync,
                       public sim::INotifyAddedToWorld,
                       public sim::INotifyRemovedFromWorld,
                       public sim::INotifySold,
                       public sim::INotifyTransform {
 public:
  ORA_TRAIT_INTERFACES(
      Building, OpenRA_Mods_Common_Traits_Building, sim::IOccupySpace,
      sim::ITargetableCells, sim::ISync, sim::INotifyAddedToWorld,
      sim::INotifyRemovedFromWorld, sim::INotifySold,
      sim::INotifyTransform)

  Building(sim::ActorInitializer& init, BuildingInfoData info);

  WPos CenterPosition() const override { return center_position_; }
  CPos TopLeft() const override { return top_left_; }
  std::vector<std::pair<CPos, SubCell>> OccupiedCells() const override {
    return vec_occupied_cells_;
  }

  const std::vector<CPos>& TransitOnlyCells() const {
    return vec_transit_only_cells_;
  }
  const BuildingInfoData& Info() const { return info_; }

  std::vector<std::pair<CPos, SubCell>> TargetableCells() override {
    return vec_targetable_cells_;
  }

  void AddedToWorld(sim::Actor& self) override;
  void RemovedFromWorld(sim::Actor& self) override;

  void Selling(sim::Actor& self) override;
  void Sold(sim::Actor& self) override {}

  void BeforeTransform(sim::Actor& self) override;
  void OnTransform(sim::Actor& self) override {}
  void AfterTransform(sim::Actor& to_actor) override {}

  void RemoveSmudges(sim::Actor& self);

 private:
  BuildingInfoData info_;
  CPos top_left_;
  WPos center_position_;
  std::vector<std::pair<CPos, SubCell>> vec_occupied_cells_;
  std::vector<std::pair<CPos, SubCell>> vec_targetable_cells_;
  std::vector<CPos> vec_transit_only_cells_;
};

}
