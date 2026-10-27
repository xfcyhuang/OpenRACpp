#pragma once
import std;

#include "core/color.hpp"
#include "core/wangle.hpp"
#include "core/wdist.hpp"
#include "mods/armament.hpp"
#include "sim/actor.hpp"
#include "sim/conditional_trait.hpp"
#include "sim/target.hpp"
#include "sim/trait_interfaces.hpp"

namespace ora::meta {
class RecordObject;
}

namespace ora::mods {

using sim::Actor;
using sim::ActorInitializer;
using sim::TraitBase;

enum class AttackSource : std::int32_t {
  Default = 0,
  AutoTarget = 1,
  AttackMove = 2,
};

class AttackBaseFace {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_AttackBase;
  virtual ~AttackBaseFace() = default;

  virtual bool IsAiming() const = 0;
  virtual void SetIsAiming(bool value) = 0;

  virtual bool IsTraitDisabled() const { return false; }
  virtual bool IsTraitPaused() const = 0;
  virtual bool AttackRequiresEnteringCell() const = 0;

  virtual sim::PlayerRelationship UnforcedAttackTargetStances() = 0;
  virtual WDist GetMaximumRange() = 0;
  virtual void AttackTarget(const sim::Target& target, AttackSource source,
                            bool queued, bool allow_move,
                            bool force_attack = false,
                            std::optional<core::Color> target_line_color =
                                std::nullopt) = 0;

  virtual bool TargetFrozenActors() const = 0;
  virtual WAngle FacingTolerance() const = 0;
  virtual std::vector<Armament*> ChooseArmamentsForTarget(
      const sim::Target& t, bool force_attack) = 0;

  virtual bool HasAnyValidWeapons(
      const sim::Target& t, bool check_for_center_targeting_weapons = false,
      bool reloading_is_invalid = false) = 0;
  virtual bool TargetInFiringArc(Actor& self, const sim::Target& target,
                                 WAngle facing_tolerance) = 0;

  virtual WDist GetMinimumRange() = 0;
  virtual WDist GetMinimumRangeVersusTarget(const sim::Target& target) = 0;
  virtual WDist GetMaximumRangeVersusTarget(const sim::Target& target) = 0;

  virtual WPos GetTargetPosition(const WPos& pos,
                                 const sim::Target& target) = 0;
  virtual void OnStopOrder(Actor& self) = 0;
};

struct AttackBaseInfoData {
  std::vector<std::string> vec_armaments{"primary",
                                         "secondary"};
  std::string str_cursor;
  std::string str_outside_range_cursor;
  core::Color target_line_color{
      core::Color::FromArgbRaw(0xFFDC143C)};
  bool b_attack_requires_entering_cell = false;
  bool b_target_frozen_actors = false;
  bool b_force_fire_ignores_actors = false;
  bool b_outside_range_requires_force_fire = false;
  std::string str_voice{"Action"};
  WAngle angle_facing_tolerance{512};
  bool b_target_terrain_without_force_fire = false;
  sim::ConditionalTraitData conditional;

  static AttackBaseInfoData Parse(const meta::RecordObject& rec_info);
};

template <class Derived>
class AttackBaseCore : public TraitBase,
                       public AttackBaseFace,
                       public sim::ConditionalTraitCore<Derived>,
                       public sim::ITick,
                       public sim::IIssueOrder,
                       public sim::IResolveOrder,
                       public sim::IOrderVoice,
                       public sim::ISync {
 public:
  AttackBaseCore(Actor& self, const AttackBaseInfoData& info);

  bool b_is_aiming = false;

  bool IsAiming() const override { return b_is_aiming; }

  bool IsTraitPaused() const override {
    return sim::ConditionalTraitCore<Derived>::IsTraitPaused();
  }

  void SetIsAiming(bool value) override { b_is_aiming = value; }
  bool AttackRequiresEnteringCell() const override {
    return info_.b_attack_requires_entering_cell;
  }

  std::span<Armament* const> Armaments() const {
    return vec_armaments_;
  }

  virtual sim::Activity* GetAttackActivity(
      Actor& self, AttackSource source, const sim::Target& new_target,
      bool allow_move, bool force_attack,
      std::optional<core::Color> target_line_color = std::nullopt) = 0;

  bool TargetInFiringArc(Actor& self, const sim::Target& target,
                         WAngle facing_tolerance) override;

  virtual bool CanAttack(Actor& self, const sim::Target& target);

  virtual void DoAttack(Actor& self, const sim::Target& target);

  std::vector<sim::IOrderTargeter*> Orders() override;

  std::unique_ptr<net::Order> IssueOrder(
      Actor& self, sim::IOrderTargeter* order,
      const sim::Target& target, bool queued) override;

  void ResolveOrder(Actor& self, const net::Order& order) override;

  virtual void OnStopOrder(Actor& self) override;

  std::string VoicePhraseForOrder(Actor& self,
                                  const net::Order& order) override;

  bool HasAnyValidWeapons(
      const sim::Target& t,
      bool check_for_center_targeting_weapons = false,
      bool reloading_is_invalid = false) override;

  virtual WPos GetTargetPosition(const WPos& pos,
                                 const sim::Target& target) override;

  WDist GetMinimumRange() override;

  WDist GetMaximumRange() override;

  WDist GetMinimumRangeVersusTarget(
      const sim::Target& target) override;

  WDist GetMaximumRangeVersusTarget(
      const sim::Target& target) override;

  std::vector<Armament*> ChooseArmamentsForTarget(
      const sim::Target& t, bool force_attack) override;

  void AttackTarget(const sim::Target& target, AttackSource source,
                    bool queued, bool allow_move, bool force_attack = false,
                    std::optional<core::Color> target_line_color =
                        std::nullopt) override;

  virtual void OnResolveAttackOrder(Actor& self, sim::Activity* activity,
                                    const sim::Target& target, bool queued,
                                    bool force_attack) {
    [[maybe_unused]] auto _ = std::tie(self, activity, target, queued,
                                       force_attack);
  }

  bool IsReachableTarget(const sim::Target& target, bool allow_move);

  sim::PlayerRelationship UnforcedAttackTargetStances() override;

  void Tick(Actor& self) override;

  void CoreAttackBaseCreated(Actor& self);

  const AttackBaseInfoData& Info() const { return info_; }

  bool TargetFrozenActors() const override {
    return info_.b_target_frozen_actors;
  }
  WAngle FacingTolerance() const override {
    return info_.angle_facing_tolerance;
  }

 protected:
  sim::IFacing* facing_ = nullptr;
  sim::IPositionable* positionable_ = nullptr;
  std::vector<sim::INotifyAiming*> vec_notify_aiming_;
  const AttackBaseInfoData info_;

  bool b_was_aiming_ = false;

 private:

  class AttackOrderTargeter final : public sim::IOrderTargeter {
   public:
    AttackOrderTargeter(AttackBaseCore& ab, int priority)
        : ab_{ab}, int4_priority_{priority} {}

    std::string OrderID() const override { return str_order_id_; }
    int OrderPriority() const override { return int4_priority_; }
    bool TargetOverridesSelection(
        Actor& , const sim::Target& ,
        std::span<Actor* const> , CPos ,
        sim::TargetModifiers ) override {
      return true;
    }
    bool CanTarget(Actor& self, const sim::Target& target,
                   sim::TargetModifiers& modifiers,
                   std::string& cursor) override;
    bool IsQueued() const override { return b_is_queued_; }

   private:
    bool CanTargetActor(Actor& self, const sim::Target& target,
                        sim::TargetModifiers& modifiers,
                        std::string& cursor);
    bool CanTargetLocation(Actor& self, CPos location,
                           sim::TargetModifiers modifiers,
                           std::string& cursor);

    AttackBaseCore& ab_;
    int int4_priority_;
    std::string str_order_id_{"Attack"};
    bool b_is_queued_ = false;
  };

  Actor* p_self_ = nullptr;
  std::vector<Armament*> vec_armaments_;
  std::vector<std::unique_ptr<AttackOrderTargeter>>
      vec_order_targeters_;
};

class AttackFrontal final
    : public AttackBaseCore<AttackFrontal>,
      public sim::IObservesVariables,
      public sim::INotifyCreated {
 public:
  AttackFrontal(ActorInitializer& init, AttackBaseInfoData&& info);

  ORA_TRAIT_INTERFACES(AttackFrontal,
                       OpenRA_Mods_Common_Traits_AttackFrontal, AttackBaseFace,
                       sim::ITick, sim::IIssueOrder, sim::IResolveOrder,
                       sim::IOrderVoice, sim::ISync,
                       sim::IObservesVariables, sim::INotifyCreated)

  bool IsTraitEnabled() const override {
    return !sim::ConditionalTraitCore<AttackFrontal>::IsTraitDisabled();
  }
  bool IsTraitDisabled() const override {
    return sim::ConditionalTraitCore<AttackFrontal>::IsTraitDisabled();
  }

  void Created(Actor& self) override;

  std::vector<sim::VariableObserver> GetVariableObservers() override {
    return CollectObservers();
  }

  bool CanAttack(Actor& self, const sim::Target& target) override;

  sim::Activity* GetAttackActivity(
      Actor& self, AttackSource source, const sim::Target& new_target,
      bool allow_move, bool force_attack,
      std::optional<core::Color> target_line_color = std::nullopt) override;

  void TraitEnabledHook(Actor& ) {}
  void TraitDisabledHook(Actor& ) {}
  void TraitResumedHook(Actor& ) {}
  void TraitPausedHook(Actor& ) {}

 private:
  friend class sim::ConditionalTraitCore<AttackFrontal>;
  friend class AttackBaseCore<AttackFrontal>;
};

}
