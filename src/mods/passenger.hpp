// UPSTREAM: OpenRA.Mods.Common/Traits/Passenger.cs +
//          Orders/EnterAlliedActorTargeter.cs @1f594e320
#pragma once
import std;

#include "core/color.hpp"
#include "core/wdist.hpp"
#include "meta/generic_record.hpp"
#include "meta/variable_expression.hpp"
#include "sim/actor.hpp"
#include "sim/actor_init.hpp"
#include "sim/trait_interfaces.hpp"

namespace ora::mods {

using sim::Actor;
using sim::TraitBase;

class Cargo;

struct PassengerInfoData {
  std::string str_cargo_type;
  std::string str_custom_pip_type;
  int int4_weight = 1;
  std::string str_cargo_condition;
  std::vector<std::pair<std::string, std::string>> vec_cargo_conditions;
  std::string str_voice{"Action"};
  core::Color color_target_line = core::Color::FromArgb(0, 128, 0);
  std::optional<expr::BooleanExpression> expr_require_force_move;
  std::string str_enter_cursor{"enter"};
  std::string str_enter_blocked_cursor{"enter-blocked"};

  static PassengerInfoData Parse(const meta::RecordObject& rec_info);
};

class Passenger final : public TraitBase,
                        public sim::IIssueOrder,
                        public sim::IResolveOrder,
                        public sim::IOrderVoice,
                        public sim::INotifyRemovedFromWorld,
                        public sim::INotifyEnteredCargo,
                        public sim::INotifyExitedCargo,
                        public sim::INotifyKilled,
                        public sim::IObservesVariables {
 public:
  explicit Passenger(const PassengerInfoData& info);

  ORA_TRAIT_INTERFACES(Passenger, OpenRA_Mods_Common_Traits_Passenger,
                       sim::IIssueOrder, sim::IResolveOrder, sim::IOrderVoice,
                       sim::INotifyRemovedFromWorld, sim::INotifyEnteredCargo,
                       sim::INotifyExitedCargo, sim::INotifyKilled,
                       sim::IObservesVariables)

  const PassengerInfoData& Info() const { return info_; }

  Actor* Transport() const { return p_transport_; }
  void SetTransport(Actor* actor) { p_transport_ = actor; }

  Cargo* ReservedCargo() const { return ptr_reserved_cargo_; }

  std::vector<sim::IOrderTargeter*> Orders() override;
  std::unique_ptr<net::Order> IssueOrder(
      Actor& self, sim::IOrderTargeter* order,
      const sim::Target& target, bool queued) override;
  std::string VoicePhraseForOrder(Actor& self,
                                  const net::Order& order) override;
  void ResolveOrder(Actor& self, const net::Order& order) override;

  void OnEnteredCargo(Actor& self, Actor& cargo) override;
  void OnExitedCargo(Actor& self, Actor& cargo) override;

  bool Reserve(Actor& self, Cargo& cargo);
  void Unreserve(Actor& self);

  virtual void OnBeforeAddedToWorld(Actor& actor);
  virtual void OnEjectedFromKilledCargo(Actor& self);

  void Killed(Actor& self, const sim::AttackInfo& e) override;
  void RemovedFromWorld(Actor& self) override;
  std::vector<sim::VariableObserver> GetVariableObservers() override;

  bool IsCorrectCargoType(Actor& target);
  bool CanEnter(Cargo* cargo);
  bool CanEnter(Actor& target);

  class EnterTransportTargeter final : public sim::IOrderTargeter {
   public:
    EnterTransportTargeter(Passenger& passenger,
                           const PassengerInfoData& info)
        : passenger_{passenger}, info_{info} {}

    std::string OrderID() const override { return "EnterTransport"; }
    int OrderPriority() const override { return 5; }
    bool TargetOverridesSelection(Actor& self, const sim::Target& target,
                                  std::span<Actor* const> actors_at, CPos xy,
                                  sim::TargetModifiers modifiers) override;
    bool CanTarget(Actor& self, const sim::Target& target,
                   sim::TargetModifiers& modifiers,
                   std::string& cursor) override;
    bool IsQueued() const override { return b_is_queued_; }

   private:
    Passenger& passenger_;
    const PassengerInfoData& info_;
    bool b_is_queued_ = false;
  };

 private:
  bool IsCorrectCargoType(Actor& target, sim::TargetModifiers modifiers);
  void RequireForceMoveConditionChanged(Actor& self,
                                        sim::ConditionCacheView conditions);

  PassengerInfoData info_;
  Actor* p_transport_ = nullptr;
  bool b_require_force_move_ = false;
  int int4_any_cargo_token = sim::Actor::InvalidConditionToken;
  int int4_specific_cargo_token = sim::Actor::InvalidConditionToken;
  Cargo* ptr_reserved_cargo_ = nullptr;
  std::unique_ptr<EnterTransportTargeter> ptr_order_targeter_;
};

}  // namespace ora::mods
