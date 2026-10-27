#pragma once
import std;

#include "core/color.hpp"
#include "meta/generic_record.hpp"
#include "sim/actor.hpp"
#include "sim/conditional_trait.hpp"
#include "sim/trait_interfaces.hpp"

namespace ora::mods {

using sim::Actor;
using sim::ActorInitializer;
using sim::TraitBase;

struct TooltipInfoData final : sim::ITooltipInfo {
  sim::ConditionalTraitData conditional;
  std::string str_name;
  std::string str_generic_name;
  bool b_generic_stance_prefix = true;
  std::string str_ally_prefix{"label-tooltip-prefix.ally"};
  std::string str_neutral_prefix;
  std::string str_enemy_prefix{"label-tooltip-prefix.enemy"};
  sim::PlayerRelationship kind_generic_visibility =
      sim::PlayerRelationship::None;
  bool b_show_owner_row = true;

  static TooltipInfoData Parse(const meta::RecordObject& rec_info);

  std::string TooltipForPlayerStance(
      sim::PlayerRelationship relationship) override;
  bool IsOwnerRowVisible() const override { return b_show_owner_row; }
};

class Tooltip final : public TraitBase,
                      public sim::ConditionalTraitCore<Tooltip>,
                      public sim::IObservesVariables,
                      public sim::ITooltip {
 public:
  Tooltip(const ActorInitializer& init, TooltipInfoData info);

  ORA_TRAIT_INTERFACES(Tooltip, OpenRA_Mods_Common_Traits_Tooltip,
                       sim::IObservesVariables, sim::ITooltip)

  bool IsTraitEnabled() const override {
    return !sim::ConditionalTraitCore<Tooltip>::IsTraitDisabled();
  }
  bool IsTraitDisabled() const override {
    return sim::ConditionalTraitCore<Tooltip>::IsTraitDisabled();
  }
  std::vector<sim::VariableObserver> GetVariableObservers() override {
    return CollectObservers();
  }

  const sim::ITooltipInfo* TooltipInfo() const override { return &info_; }
  sim::Player* Owner() const override {
    return ptr_self_->EffectiveOwner() != nullptr
               ? ptr_self_->EffectiveOwner()->Owner()
               : ptr_self_->Owner();
  }

 private:
  Actor* ptr_self_ = nullptr;
  TooltipInfoData info_;
};

}
