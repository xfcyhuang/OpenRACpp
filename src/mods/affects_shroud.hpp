// UPSTREAM: OpenRA.Mods.Common/Traits/AffectsShroud.cs @b6fc03f L18-174 +
//          RevealsShroud.cs L18-72(全文逐语义重写)
//          The whole of AffectsShroud.cs L18-174 + RevealsShroud.cs
//          L18-72.
//
// 机制对照 / Mechanism mapping:
//  - abstract AffectsShroud + abstract Info → AffectsShroudBase<Derived>
//    模板基(CRTP;ConditionalTraitCore 同构);Derived 提供
//    AddCellsToPlayerShroud/RemoveCellsFromPlayerShroud 并可覆写 Range
//    The abstract AffectsShroud + abstract Info → the AffectsShroudBase<
//    Derived> template base (CRTP; the ConditionalTraitCore isomorph);
//    the Derived class supplies AddCellsToPlayerShroud/
//    RemoveCellsFromPlayerShroud and may override Range.
//  - ProjectedCells 的 HashSet<PPos> 暂存(Footprint 型)→ vector 去重并
//    (UnionWith 语义:无序并集;排序仅影响遍历序,AddSource 对格序不敏感)
//    ProjectedCells' HashSet<PPos> staging (the Footprint form) → a
//    dedup-merged vector (UnionWith semantics: an unordered union; the
//    order affects traversal alone, and AddSource is cell-order
//    insensitive).
//  - VisibilityType 枚举:gen/enums_gen.cpp 已注册,int 槽承载
//    The VisibilityType enum: registered in gen/enums_gen.cpp, carried by
//    an int slot.
#pragma once
import std;

#include "core/wdist.hpp"
#include "core/wpos.hpp"
#include "map/map.hpp"
#include "sim/actor.hpp"
#include "sim/conditional_trait.hpp"
#include "sim/player.hpp"
#include "sim/shroud.hpp"
#include "sim/trait_interfaces.hpp"

namespace ora::meta {
class RecordObject;
}

namespace ora::mods {

using sim::Actor;
using sim::Player;

/// AffectsShroud.cs L30-33 的 VisibilityType(gen 枚举同步)
/// AffectsShroud.cs L30-33's VisibilityType (mirroring the gen enum).
enum class VisibilityType : std::int32_t {
  Footprint = 0,
  CenterPosition = 1,
  GroundPosition = 2,
};

/// AffectsShroudInfo 的解析面(L18-33;RevealsShroudInfo 的基段)
/// The parsed face of AffectsShroudInfo (L18-33; the base segment of
/// RevealsShroudInfo).
struct AffectsShroudInfoData {
  WDist dist_min_range{0};    // L20
  WDist dist_range{0};        // L21
  int int4_max_height_delta = -1;  // L25
  WDist dist_move_recalculation_threshold{256};  // L28
  VisibilityType type = VisibilityType::Footprint;  // L32
  sim::ConditionalTraitData conditional;

  static AffectsShroudInfoData Parse(const meta::RecordObject& rec_info);
};

/// AffectsShroud(L35-173;模板基 —— IObservesVariables/INotifyCreated 由
/// Derived 承载,ConditionalTrait 协议同 Armament 形)
/// AffectsShroud (L35-173; the template base — IObservesVariables/
/// INotifyCreated ride the Derived class, the ConditionalTrait protocol
/// in Armament's shape).
template <class Derived>
class AffectsShroudBase : public sim::TraitBase,
                          public sim::ConditionalTraitCore<Derived>,
                          public sim::ISync,
                          public sim::INotifyAddedToWorld,
                          public sim::INotifyRemovedFromWorld,
                          public sim::INotifyMoving,
                          public sim::INotifyCenterPositionChanged,
                          public sim::ITick {
 public:
  AffectsShroudBase(const AffectsShroudInfoData& info)
      : sim::ConditionalTraitCore<Derived>(info.conditional), info_{info} {}

  // ———— [VerifySync] 三成员(L43-49)————
  // ———— The three [VerifySync] members (L43-49). ————
  CPos cached_location_;                 // L43
  WDist cached_range_{0};                // L45
  bool cached_trait_disabled_ = false;   // L48

  /// L155:Range(virtual;Derived 覆写)| L155: Range (virtual; the
  /// Derived override wins).
  virtual WDist Range() const {
    return cached_trait_disabled_ ? WDist{0} : info_.dist_range;
  }

  void AddedToWorld(Actor& self) override;      // L136-147
  void RemovedFromWorld(Actor& self) override;  // L149-153
  void MovementTypeChanged(Actor& self,
                           sim::MovementType type) override;  // L157-172
  void CenterPositionChanged(Actor& self, std::uint8_t old_layer,
                             std::uint8_t new_layer) override;  // L89-107
  void Tick(Actor& self) override;               // L109-124

 protected:
  const AffectsShroudInfoData& BaseInfo() const { return info_; }

  /// Derived 的协议面(L53-54) | Derived's protocol faces (L53-54).
  void CallAddCellsToPlayerShroud(Actor& self, Player& player,
                                  const std::vector<PPos>& vec_uv) {
    static_cast<Derived*>(this)->AddCellsToPlayerShroud(self, player,
                                                        vec_uv);
  }
  void CallRemoveCellsFromPlayerShroud(Actor& self, Player& player) {
    static_cast<Derived*>(this)->RemoveCellsFromPlayerShroud(self, player);
  }

 private:
  /// L63-87:ProjectedCells | L63-87: ProjectedCells.
  std::vector<PPos> ProjectedCells(Actor& self);

  /// L126-134:UpdateShroudCells | L126-134: UpdateShroudCells.
  void UpdateShroudCells(Actor& self);

  const AffectsShroudInfoData info_;
  WPos pos_cached_{};
};

/// RevealsShroudInfo 的解析面(RevealsShroud.cs L18-27 的增字段)
/// The parsed face of RevealsShroudInfo (RevealsShroud.cs L18-27's added
/// fields).
struct RevealsShroudInfoData : AffectsShroudInfoData {
  sim::PlayerRelationship valid_relationships =
      sim::PlayerRelationship::Ally;  // L21
  bool b_reveal_generated_shroud = true;  // L24

  static RevealsShroudInfoData Parse(const meta::RecordObject& rec_info);
};

/// RevealsShroud.cs L29-72
class RevealsShroud final : public AffectsShroudBase<RevealsShroud>,
                            public sim::IObservesVariables,
                            public sim::INotifyCreated {
 public:
  RevealsShroud(const RevealsShroudInfoData& info);

  ORA_TRAIT_INTERFACES(RevealsShroud,
                       OpenRA_Mods_Common_Traits_RevealsShroud, sim::ISync,
                       sim::IObservesVariables, sim::INotifyCreated,
                       sim::INotifyAddedToWorld,
                       sim::INotifyRemovedFromWorld, sim::INotifyMoving,
                       sim::INotifyCenterPositionChanged, sim::ITick)

  bool IsTraitEnabled() const override {
    return !sim::ConditionalTraitCore<RevealsShroud>::IsTraitDisabled();
  }
  bool IsTraitDisabled() const override {
    return sim::ConditionalTraitCore<RevealsShroud>::IsTraitDisabled();
  }

  /// Created(L43-48):修正 trait 一次解析 + 核 Created
  /// Created (L43-48): the modifier traits resolved once + the core's
  /// Created.
  void Created(Actor& self) override;

  std::vector<sim::VariableObserver> GetVariableObservers() override {
    return CollectObservers();
  }

  /// L50-56:AddCellsToPlayerShroud(ValidRelationships 过滤 + AddSource)
  /// L50-56: AddCellsToPlayerShroud (the ValidRelationships filter +
  /// AddSource).
  void AddCellsToPlayerShroud(Actor& self, Player& p,
                              const std::vector<PPos>& vec_uv);

  /// L58:RemoveCellsFromPlayerShroud
  /// L58: RemoveCellsFromPlayerShroud.
  void RemoveCellsFromPlayerShroud(Actor& self, Player& p);

  /// L60-70:Range(rangeModifiers 链)
  /// L60-70: Range (the rangeModifiers chain).
  WDist Range() const override;

 private:
  friend class sim::ConditionalTraitCore<RevealsShroud>;

  RevealsShroudInfoData reveals_info_;  // 增字段段 | the added-field
                                        // segment
  sim::ShroudSourceType source_type_ =
      sim::ShroudSourceType::Visibility;  // L39-41(RevealGeneratedShroud
                                          // 决定)
  std::vector<sim::IRevealsShroudModifier*>
      vec_range_modifier_ptrs_;  // L33 rangeModifiers(Created 缓存)
};

}  // namespace ora::mods
