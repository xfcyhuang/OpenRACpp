#pragma once
import std;

#include "core/bitset.hpp"
#include "map/cell_layer.hpp"
#include "mods/pathfinding/path_graph.hpp"

namespace ora::meta {
class RecordObject;
}
#include "core/long_bitset.hpp"
#include "sim/trait_interfaces.hpp"

namespace ora::mods::pathfinding {

using sim::Actor;
using sim::ICrushable;
using sim::IWorldLoaded;
using sim::PlayerMaskSet;
using sim::TraitBase;

enum class CellFlag : std::uint8_t {
  HasFreeSpace = 0,
  HasMovingActor = 1,
  HasStationaryActor = 2,
  HasMovableActor = 4,
  HasCrushableActor = 8,
  HasTemporaryBlocker = 16,
  HasTransitOnlyActor = 32,
};

inline bool HasCellFlag(CellFlag c, CellFlag cell_flag) {

  return (static_cast<std::uint8_t>(c) & static_cast<std::uint8_t>(cell_flag)) ==
         static_cast<std::uint8_t>(cell_flag);
}

namespace CustomMovementLayerType {
inline constexpr std::uint8_t kTunnel = 1;
inline constexpr std::uint8_t kSubterranean = 2;
inline constexpr std::uint8_t kJumpjet = 3;
inline constexpr std::uint8_t kElevatedBridge = 4;
}

class LocomotorInfo {
 public:

  struct TerrainInfo {
    static TerrainInfo Impassable() { return TerrainInfo{}; }

    TerrainInfo() = default;
    TerrainInfo(int speed, short cost) : speed_{speed}, cost_{cost} {}

    short Cost() const { return cost_; }
    int Speed() const { return speed_; }

   private:
    int speed_ = 0;
    short cost_ = kMovementCostForUnreachableCell;
  };

  LocomotorInfo() = default;
  LocomotorInfo(std::string name, int wait_average, int wait_spread,
                bool shares_cell, bool move_into_shroud,
                core::BitSet<sim::CrushClass> crushes,
                core::BitSet<sim::DamageType> crush_damage_types,
                std::vector<TerrainInfo> terrain_infos_by_index,
                std::vector<std::pair<std::string, TerrainInfo>> terrain_speeds,
                bool disable_domain_passability_check)
      : str_name_{std::move(name)},
        wait_average_{wait_average},
        wait_spread_{wait_spread},
        shares_cell_{shares_cell},
        move_into_shroud_{move_into_shroud},
        crushes_{crushes},
        crush_damage_types_{crush_damage_types},
        vec_terrain_infos_{std::move(terrain_infos_by_index)},
        vec_terrain_speeds_{std::move(terrain_speeds)},
        b_disable_domain_passability_check_{disable_domain_passability_check} {}

  const std::string& Name() const { return str_name_; }
  int WaitAverage() const { return wait_average_; }
  int WaitSpread() const { return wait_spread_; }
  bool SharesCell() const { return shares_cell_; }
  bool MoveIntoShroud() const { return move_into_shroud_; }
  const core::BitSet<sim::CrushClass>& Crushes() const { return crushes_; }
  const core::BitSet<sim::DamageType>& CrushDamageTypes() const {
    return crush_damage_types_;
  }

  const std::vector<std::pair<std::string, TerrainInfo>>& TerrainSpeeds()
      const {
    return vec_terrain_speeds_;
  }
  bool DisableDomainPassabilityCheck() const {
    return b_disable_domain_passability_check_;
  }

  const TerrainInfo& TerrainInfoAt(std::size_t index) const {
    return vec_terrain_infos_[index];
  }

 private:
  std::string str_name_{"default"};
  int wait_average_ = 40;
  int wait_spread_ = 10;
  bool shares_cell_ = false;
  bool move_into_shroud_ = true;
  core::BitSet<sim::CrushClass> crushes_;
  core::BitSet<sim::DamageType> crush_damage_types_;
  std::vector<TerrainInfo> vec_terrain_infos_;
  std::vector<std::pair<std::string, TerrainInfo>> vec_terrain_speeds_;
  bool b_disable_domain_passability_check_ = false;
};

LocomotorInfo ParseLocomotorInfo(const meta::RecordObject& rec_info);

class Locomotor final : public TraitBase, public IWorldLoaded {
 public:
  Locomotor(sim::Actor& self, LocomotorInfo info);

  ORA_TRAIT_INTERFACES(Locomotor, OpenRA_Mods_Common_Traits_Locomotor,
                       IWorldLoaded)

  const LocomotorInfo& Info() const { return info_; }

  void AddCellCostChangedListener(
      std::function<void(CPos, short, short)> fn) {
    vec_cell_cost_changed_.push_back(std::move(fn));
  }

  short MovementCostForCell(CPos cell) const {
    return MovementCostForCell(cell, nullptr);
  }
  int MovementSpeedForCell(CPos cell) const;
  short MovementCostToEnterCell(sim::Actor* actor, CPos dest_node,
                                sim::BlockedByActor check,
                                sim::Actor* ignore_actor,
                                bool ignore_self = false,
                                sim::SubCell sub_cell = sim::SubCell::FullCell);
  short MovementCostToEnterCell(sim::Actor* actor, CPos src_node, CPos dest_node,
                                sim::BlockedByActor check,
                                sim::Actor* ignore_actor,
                                bool ignore_self = false);
  bool CanStayInCell(CPos cell);
  sim::SubCell GetAvailableSubCell(sim::Actor* self, CPos cell,
                                   sim::BlockedByActor check,
                                   sim::SubCell preferred_sub_cell = sim::SubCell::Any,
                                   sim::Actor* ignore_actor = nullptr);

  void WorldLoaded(sim::World& w, gfx::WorldRenderer* wr) override;

 private:

  struct CellCache {
    PlayerMaskSet immovable{};
    CellFlag cell_flag = CellFlag::HasFreeSpace;
    PlayerMaskSet crushable{};
  };

  short MovementCostForCell(CPos cell, const CPos* from_cell) const;
  bool CanMoveFreelyInto(sim::Actor* actor, CPos cell, sim::SubCell sub_cell,
                         sim::BlockedByActor check, sim::Actor* ignore_actor,
                         bool ignore_self);
  bool IsBlockedBy(sim::Actor* actor, sim::Actor* other_actor,
                   sim::Actor* ignore_actor, CPos cell,
                   sim::BlockedByActor check, CellFlag cell_flag);
  CellCache GetCache(CPos cell);
  void CellUpdated(CPos cell);
  void UpdateCellCost(CPos cell);
  void UpdateCellBlocking(CPos cell);

  LocomotorInfo info_;
  std::vector<std::function<void(CPos, short, short)>>
      vec_cell_cost_changed_;
  std::vector<LocomotorInfo::TerrainInfo> vec_terrain_infos_;
  sim::World* world_ = nullptr;
  std::vector<CPos> vec_dirty_cells_;
  std::vector<std::uint8_t> vec_dirty_bitmap_;
  bool shares_cell_ = false;

  using CellCostLayer = ora::map::CellLayer<short>;
  using BlockingCacheLayer = ora::map::CellLayer<CellCache>;
  std::vector<std::unique_ptr<CellCostLayer>> vec_cells_cost_;
  std::vector<std::unique_ptr<BlockingCacheLayer>> vec_blocking_cache_;

  sim::IActorMap* actor_map_ = nullptr;

  friend class HierarchicalPathFinder;
};

}
