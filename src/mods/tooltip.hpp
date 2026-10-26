// UPSTREAM: OpenRA.Mods.Common/Traits/Tooltip.cs @1f594e320
//          L16-102(TooltipInfoBase/TooltipInfo 的字段面与
//          TooltipForPlayerStance 的 Fluent 三前缀链 + Tooltip 运行时)
//          L16-102 (the TooltipInfoBase/TooltipInfo field face with
//          TooltipForPlayerStance's Fluent three-prefix chain, plus the
//          Tooltip runtime).
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

/// TooltipInfoBase + TooltipInfo(L16-84)的解析面;ITooltipInfo 的实现者
/// The parsed face of TooltipInfoBase + TooltipInfo (L16-84); the
/// ITooltipInfo implementer.
struct TooltipInfoData final : sim::ITooltipInfo {
  sim::ConditionalTraitData conditional;
  std::string str_name;                 // L20([FieldLoader.Require])
  std::string str_generic_name;         // L37
  bool b_generic_stance_prefix = true;  // L40
  std::string str_ally_prefix{"label-tooltip-prefix.ally"};   // L44
  std::string str_neutral_prefix;                             // L47
  std::string str_enemy_prefix{"label-tooltip-prefix.enemy"}; // L50
  sim::PlayerRelationship kind_generic_visibility =
      sim::PlayerRelationship::None;                            // L55
  bool b_show_owner_row = true;      // L58

  static TooltipInfoData Parse(const meta::RecordObject& rec_info);

  std::string TooltipForPlayerStance(
      sim::PlayerRelationship relationship) override;
  bool IsOwnerRowVisible() const override { return b_show_owner_row; }
};

/// Tooltip(L86-101):ITooltip 的最小运行时(条件 trait 核 + 伪装主行)
/// Tooltip (L86-101): the minimal ITooltip runtime (the conditional core
/// plus the disguised owner row).
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

}  // namespace ora::mods
