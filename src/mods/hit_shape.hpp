#pragma once
import std;

#include "core/bitset.hpp"
#include "core/wdist.hpp"
#include "core/wpos.hpp"
#include "core/wrot.hpp"
#include "mods/body_orientation.hpp"
#include "mods/hit_shapes.hpp"
#include "sim/actor.hpp"
#include "sim/conditional_trait.hpp"
#include "sim/trait_interfaces.hpp"

namespace ora::meta {
class RecordObject;
}

namespace ora::mods {

using sim::Actor;
using sim::ActorInitializer;
using sim::TraitBase;

class ArmorType {};

struct HitShapeInfoData {
  std::string str_turret;
  std::vector<WVec> vec_targetable_offsets{WVec{0, 0, 0}};
  bool b_use_targetable_cells_offsets = false;
  core::BitSet<ArmorType> bitset_armor_types;
  std::unique_ptr<IHitShape> ptr_type;
  sim::ConditionalTraitData conditional;

  static HitShapeInfoData Parse(const meta::RecordObject& rec_info);
};

class HitShape final : public TraitBase,
                       public sim::ConditionalTraitCore<HitShape>,
                       public sim::IObservesVariables,
                       public sim::INotifyCreated,
                       public sim::ITargetablePositions {
 public:
  HitShape(ActorInitializer& init, HitShapeInfoData&& info);

  ORA_TRAIT_INTERFACES(HitShape, OpenRA_Mods_Common_Traits_HitShape,
                       sim::IObservesVariables, sim::INotifyCreated,
                       sim::ITargetablePositions)

  bool IsTraitEnabled() const override {
    return !ConditionalTraitCore<HitShape>::IsTraitDisabled();
  }
  bool IsTraitDisabled() const override {
    return ConditionalTraitCore<HitShape>::IsTraitDisabled();
  }

  void Created(Actor& self) override;

  std::vector<sim::VariableObserver> GetVariableObservers() {
    return CollectObservers();
  }

  std::vector<WPos> TargetablePositions(Actor& self) override;

  WDist DistanceFromEdge(Actor& self, const WPos& pos);

  const HitShapeInfoData& Info() const { return info_; }

  void TraitEnabledHook(Actor& ) {}
  void TraitDisabledHook(Actor& ) {}
  void TraitResumedHook(Actor& ) {}
  void TraitPausedHook(Actor& ) {}

 private:
  friend class sim::ConditionalTraitCore<HitShape>;

  WVec CalculateTargetableOffset(Actor& self, const WVec& offset);

  std::vector<WPos> MaterializeTargetablePositions(Actor& self);

  HitShapeInfoData info_;
  BodyOrientation* coords_ = nullptr;
  sim::ITargetableCells* targetable_cells_ = nullptr;

  struct CacheKey {

    bool b_has_cells = false;
    std::vector<std::pair<CPos, sim::SubCell>> vec_targetable_cells;
    bool b_has_center = false;
    WPos center{};
    bool b_has_orientation = false;
    WRot orientation{WRot::None()};

    bool operator==(const CacheKey&) const = default;
  };
  CacheKey cache_input_;
  bool b_cache_valid_ = false;
  std::vector<WPos> vec_cached_targetable_positions_;
};

}
