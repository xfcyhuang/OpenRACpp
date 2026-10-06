// UPSTREAM: OpenRA.Mods.Common/Traits/World/Locomotor.cs @b6fc03f L24-526
//          (逐语义重写 + Mobile 挂点的部分覆盖面)
//          Verbatim-semantics rewrite + the partial-coverage Mobile hooks.
//
// 机制对照 / Mechanism mapping:
//  - CellCache(L133:readonly record struct)→ POD;LongBitSet<PlayerBitMask>
//    的 Immovable/Crushable 集合照抄(世界掩码语义随玩家创建链本批到位)
//    The CellCache (L133) → a POD; the Immovable/Crushable
//    LongBitSet<PlayerBitMask> sets kept (the world-mask semantics arrive
//    with this batch's player-creation chain).
//  - Mobile 挂点(L336-337/484-486:OccupiesSpace as Mobile 的可移动判定、
//    IsBlockedBy 的 allied/moving 让行):Mobile trait 随下一批 —— 部分覆盖
//    装配面下 `as Mobile` 恒 null → otherIsMovable/otherIsMoving 恒 false,
//    ITemporaryBlocker(经 world.RulesContainTemporaryBlocker 门控)与
//    Building 的 TransitOnlyCells 判定同面(COVERAGE 登记;代码形态保留
//    挂点注释,Mobile 批即插即用)
//    The Mobile hooks (L336-337/484-486): the Mobile trait lands next
//    batch — under the partial-coverage assembly face `as Mobile` is always
//    null → otherIsMovable/otherIsMoving stay false, and the same face
//    applies to the ITemporaryBlocker (gated by
//    world.RulesContainTemporaryBlocker) and the Building TransitOnlyCells
//    checks (in COVERAGE; the code shapes keep hook comments so the Mobile
//    batch is plug-in).
//  - dirtyCells(L144:HashSet<CPos>)→ 插入序 vector + 位图判重(集面语义:
//    Remove 探测即重算)
//    The dirtyCells (L144) → an insertion-ordered vector + a bitmap for
//    membership (the Remove-probe-recomputes semantics).
//  - CellCostChanged 事件 → 回调表 | the CellCostChanged event → callbacks.
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

/// CellFlag(Locomotor.cs L25-34) | CellFlag (Locomotor.cs L25-34).
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
  // PERF: Enum.HasFlag is slower and requires allocations.(上游注释)
  return (static_cast<std::uint8_t>(c) & static_cast<std::uint8_t>(cell_flag)) ==
         static_cast<std::uint8_t>(cell_flag);
}

/// CustomMovementLayerType(Locomotor.cs L51-57) | CustomMovementLayerType
/// (Locomotor.cs L51-57).
namespace CustomMovementLayerType {
inline constexpr std::uint8_t kTunnel = 1;
inline constexpr std::uint8_t kSubterranean = 2;
inline constexpr std::uint8_t kJumpjet = 3;
inline constexpr std::uint8_t kElevatedBridge = 4;
}  // namespace CustomMovementLayerType

/// LocomotorInfo 的运行时承载(Locomotor.cs L62-129;Info 走值袋,工厂面
/// 经 TraitRegistry 读 TerrainSpeeds 值袋)
/// The runtime carrier of LocomotorInfo (Locomotor.cs L62-129; the Info
/// rides the value bag, the factory reads the TerrainSpeeds bag through
/// the TraitRegistry).
class LocomotorInfo {
 public:
  /// TerrainInfo(L106-124) | TerrainInfo (L106-124).
  struct TerrainInfo {
    static TerrainInfo Impassable() { return TerrainInfo{}; }

    TerrainInfo() = default;
    TerrainInfo(int speed, short cost) : speed_{speed}, cost_{cost} {}

    short Cost() const { return cost_; }    // L110
    int Speed() const { return speed_; }    // L111

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

  const std::string& Name() const { return str_name_; }        // L64
  int WaitAverage() const { return wait_average_; }            // L66
  int WaitSpread() const { return wait_spread_; }              // L68
  bool SharesCell() const { return shares_cell_; }             // L71
  bool MoveIntoShroud() const { return move_into_shroud_; }    // L74
  const core::BitSet<sim::CrushClass>& Crushes() const { return crushes_; }
  const core::BitSet<sim::DamageType>& CrushDamageTypes() const {
    return crush_damage_types_;
  }
  /// TerrainSpeeds 字典(L84;插入序)| the TerrainSpeeds dictionary (L84).
  const std::vector<std::pair<std::string, TerrainInfo>>& TerrainSpeeds()
      const {
    return vec_terrain_speeds_;
  }
  bool DisableDomainPassabilityCheck() const {  // L126
    return b_disable_domain_passability_check_;
  }

  /// 地形索引表(ctor 的 L158-162 展开;WorldLoaded 前 = 恒 Impassable)
  /// The terrain-index table (the ctor's L158-162 expansion; Impassable
  /// before WorldLoaded).
  const TerrainInfo& TerrainInfoAt(std::size_t index) const {
    return vec_terrain_infos_[index];
  }

 private:
  std::string str_name_{"default"};  // L64
  int wait_average_ = 40;            // L66
  int wait_spread_ = 10;             // L68
  bool shares_cell_ = false;         // L71
  bool move_into_shroud_ = true;     // L74
  core::BitSet<sim::CrushClass> crushes_;           // L77
  core::BitSet<sim::DamageType> crush_damage_types_;  // L80
  std::vector<TerrainInfo> vec_terrain_infos_;      // ctor 展开面
  std::vector<std::pair<std::string, TerrainInfo>> vec_terrain_speeds_;  // L84
  bool b_disable_domain_passability_check_ = false;  // L126
};

/// LocomotorInfo 的值袋解析(TraitRegistry 工厂面;TerrainSpeeds 的
/// GenericDict 槽位约定:arr_ints[0]=speed, arr_ints[1]=cost —— loaders.cpp
/// 的 RegisterLoadSpeeds 契约)
/// The LocomotorInfo bag parse (the TraitRegistry factory face; the
/// TerrainSpeeds GenericDict slot convention: arr_ints[0]=speed,
/// arr_ints[1]=cost — loaders.cpp's RegisterLoadSpeeds contract).
LocomotorInfo ParseLocomotorInfo(const meta::RecordObject& rec_info);

/// Locomotor(Locomotor.cs L131-526;IWorldLoaded) | Locomotor
/// (Locomotor.cs L131-526; IWorldLoaded).
class Locomotor final : public TraitBase, public IWorldLoaded {
 public:
  Locomotor(sim::Actor& self, LocomotorInfo info);  // L152-163

  ORA_TRAIT_INTERFACES(Locomotor, OpenRA_Mods_Common_Traits_Locomotor,
                       IWorldLoaded)

  const LocomotorInfo& Info() const { return info_; }  // L135

  /// CellCostChanged 事件(L140) | the CellCostChanged event (L140).
  void AddCellCostChangedListener(
      std::function<void(CPos, short, short)> fn) {
    vec_cell_cost_changed_.push_back(std::move(fn));
  }

  short MovementCostForCell(CPos cell) const {  // L165-168
    return MovementCostForCell(cell, nullptr);
  }
  int MovementSpeedForCell(CPos cell) const;    // L186-192
  short MovementCostToEnterCell(sim::Actor* actor, CPos dest_node,
                                sim::BlockedByActor check,
                                sim::Actor* ignore_actor,
                                bool ignore_self = false,
                                sim::SubCell sub_cell = sim::SubCell::FullCell);   // L194-204
  short MovementCostToEnterCell(sim::Actor* actor, CPos src_node, CPos dest_node,
                                sim::BlockedByActor check,
                                sim::Actor* ignore_actor,
                                bool ignore_self = false);  // L206-216
  bool CanStayInCell(CPos cell);               // L299-305
  sim::SubCell GetAvailableSubCell(sim::Actor* self, CPos cell,
                                   sim::BlockedByActor check,
                                   sim::SubCell preferred_sub_cell = sim::SubCell::Any,
                                   sim::Actor* ignore_actor = nullptr);  // L307-326

  void WorldLoaded(sim::World& w, gfx::WorldRenderer* wr) override;  // L378-420

 private:
  /// CellCache(L133) | CellCache (L133).
  struct CellCache {
    PlayerMaskSet immovable{};
    CellFlag cell_flag = CellFlag::HasFreeSpace;
    PlayerMaskSet crushable{};
  };

  short MovementCostForCell(CPos cell, const CPos* from_cell) const;  // L170-184
  bool CanMoveFreelyInto(sim::Actor* actor, CPos cell, sim::SubCell sub_cell,
                         sim::BlockedByActor check, sim::Actor* ignore_actor,
                         bool ignore_self);  // L219-297
  bool IsBlockedBy(sim::Actor* actor, sim::Actor* other_actor,
                   sim::Actor* ignore_actor, CPos cell,
                   sim::BlockedByActor check, CellFlag cell_flag);  // L331-376
  CellCache GetCache(CPos cell);     // L422-430
  void CellUpdated(CPos cell);       // L432-435
  void UpdateCellCost(CPos cell);    // L437-458
  void UpdateCellBlocking(CPos cell);  // L463-524

  LocomotorInfo info_;                                     // L135
  std::vector<std::function<void(CPos, short, short)>>
      vec_cell_cost_changed_;                              // L140 事件
  std::vector<LocomotorInfo::TerrainInfo> vec_terrain_infos_;  // L142
  sim::World* world_ = nullptr;                            // L143
  std::vector<CPos> vec_dirty_cells_;                      // L144(插入序 + 判重)
  std::vector<std::uint8_t> vec_dirty_bitmap_;
  bool shares_cell_ = false;                               // L145

  using CellCostLayer = ora::map::CellLayer<short>;
  using BlockingCacheLayer = ora::map::CellLayer<CellCache>;
  std::vector<std::unique_ptr<CellCostLayer>> vec_cells_cost_;      // L147
  std::vector<std::unique_ptr<BlockingCacheLayer>> vec_blocking_cache_;  // L148

  sim::IActorMap* actor_map_ = nullptr;                    // L150

  friend class HierarchicalPathFinder;
};

}  // namespace ora::mods::pathfinding
