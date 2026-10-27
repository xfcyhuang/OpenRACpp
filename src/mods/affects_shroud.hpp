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

enum class VisibilityType : std::int32_t {
  Footprint = 0,
  CenterPosition = 1,
  GroundPosition = 2,
};

struct AffectsShroudInfoData {
  WDist dist_min_range{0};
  WDist dist_range{0};
  int int4_max_height_delta = -1;
  WDist dist_move_recalculation_threshold{256};
  VisibilityType type = VisibilityType::Footprint;
  sim::ConditionalTraitData conditional;

  static AffectsShroudInfoData Parse(const meta::RecordObject& rec_info);
};

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

  CPos cached_location_;
  WDist cached_range_{0};
  bool cached_trait_disabled_ = false;

  virtual WDist Range() const {
    return cached_trait_disabled_ ? WDist{0} : info_.dist_range;
  }

  void AddedToWorld(Actor& self) override;
  void RemovedFromWorld(Actor& self) override;
  void MovementTypeChanged(Actor& self,
                           sim::MovementType type) override;
  void CenterPositionChanged(Actor& self, std::uint8_t old_layer,
                             std::uint8_t new_layer) override;
  void Tick(Actor& self) override;

 protected:
  const AffectsShroudInfoData& BaseInfo() const { return info_; }

  void CallAddCellsToPlayerShroud(Actor& self, Player& player,
                                  const std::vector<PPos>& vec_uv) {
    static_cast<Derived*>(this)->AddCellsToPlayerShroud(self, player,
                                                        vec_uv);
  }
  void CallRemoveCellsFromPlayerShroud(Actor& self, Player& player) {
    static_cast<Derived*>(this)->RemoveCellsFromPlayerShroud(self, player);
  }

 private:

  std::vector<PPos> ProjectedCells(Actor& self);

  void UpdateShroudCells(Actor& self);

  const AffectsShroudInfoData info_;
  WPos pos_cached_{};
};

struct RevealsShroudInfoData : AffectsShroudInfoData {
  sim::PlayerRelationship valid_relationships =
      sim::PlayerRelationship::Ally;
  bool b_reveal_generated_shroud = true;

  static RevealsShroudInfoData Parse(const meta::RecordObject& rec_info);
};

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

  void Created(Actor& self) override;

  std::vector<sim::VariableObserver> GetVariableObservers() override {
    return CollectObservers();
  }

  void AddCellsToPlayerShroud(Actor& self, Player& p,
                              const std::vector<PPos>& vec_uv);

  void RemoveCellsFromPlayerShroud(Actor& self, Player& p);

  WDist Range() const override;

 private:
  friend class sim::ConditionalTraitCore<RevealsShroud>;

  RevealsShroudInfoData reveals_info_;

  sim::ShroudSourceType source_type_ =
      sim::ShroudSourceType::Visibility;

  std::vector<sim::IRevealsShroudModifier*>
      vec_range_modifier_ptrs_;
};

}
