// UPSTREAM: OpenRA.Mods.Common/Traits/Carryall.cs + Carryable.cs +
//          AutoCarryall.cs + AutoCarryable.cs + CarryableHarvester.cs @e180e3225
#pragma once
import std;

#include "core/color.hpp"
#include "core/wangle.hpp"
#include "core/wdist.hpp"
#include "core/wpos.hpp"
#include "gfx/actor_preview.hpp"
#include "meta/generic_record.hpp"
#include "meta/variable_expression.hpp"
#include "mods/deploy_order_targeter.hpp"
#include "net/order.hpp"
#include "sim/actor.hpp"
#include "sim/actor_init.hpp"
#include "sim/conditional_trait.hpp"
#include "sim/trait_interfaces.hpp"

namespace ora::mods {

using sim::Actor;
using sim::Player;
using sim::TraitBase;

class Aircraft;
class Mobile;
class BodyOrientation;
class Carryall;
class AutoCarryall;

enum class LockResponse { Success, Pending, Failed };

using sim::IDelayCarryallPickup;

struct CarryableInfoData {
  sim::ConditionalTraitData conditional;
  std::string str_reserved_condition;
  std::string str_carried_condition;
  std::string str_locked_condition;
  WVec wvec_local_offset{0, 0, 0};

  static CarryableInfoData Parse(const meta::RecordObject& rec_info);
};

/// Carryable.cs L57-58:State(Free/Reserved/Locked)
enum class CarryableState { Free, Reserved, Locked };

class Carryable : public TraitBase,
                  public sim::ConditionalTraitCore<Carryable>,
                  public sim::IObservesVariables,
                  public sim::INotifyCreated {
 public:
  Carryable(const CarryableInfoData& info);

  ORA_TRAIT_INTERFACES(Carryable, OpenRA_Mods_Common_Traits_Carryable,
                       sim::IObservesVariables, sim::INotifyCreated)

  bool IsTraitEnabled() const override {
    return !sim::ConditionalTraitCore<Carryable>::IsTraitDisabled();
  }
  bool IsTraitDisabled() const override {
    return sim::ConditionalTraitCore<Carryable>::IsTraitDisabled();
  }
  void Created(sim::Actor& self) override;
  std::vector<sim::VariableObserver> GetVariableObservers() override {
    return CollectObservers();
  }

  const CarryableInfoData& Info() const { return info_; }

  Actor* Carrier() const { return p_carrier_; }
  bool Reserved() const { return state_ != CarryableState::Free; }

  virtual void Attached(Actor& self, Actor* carrier);
  virtual void Detached(Actor& self);
  virtual bool Reserve(Actor& self, Actor* carrier);
  virtual void UnReserve(Actor& self);
  virtual LockResponse LockForPickup(Actor& self, Actor* carrier);

 protected:
  Mobile* MobileTrait() const { return p_mobile_; }
  CarryableState StateValue() const { return state_; }
  bool IsAttached() const { return b_attached_; }

  CarryableInfoData info_;
  int int4_reserved_token = sim::Actor::InvalidConditionToken;
  int int4_carried_token = sim::Actor::InvalidConditionToken;
  int int4_locked_token = sim::Actor::InvalidConditionToken;

  std::vector<IDelayCarryallPickup*> vec_delay_pickups_;

  Actor* p_carrier_ = nullptr;
  Mobile* p_mobile_ = nullptr;
  CarryableState state_ = CarryableState::Free;
  bool b_attached_ = false;
};

struct AutoCarryableInfoData {
  CarryableInfoData carryable;
  WDist dist_min_distance = WDist::FromCells(6);

  static AutoCarryableInfoData Parse(const meta::RecordObject& rec_info);
};

class AutoCarryable final : public Carryable,
                            public sim::ICallForTransport {
 public:
  AutoCarryable(const AutoCarryableInfoData& info);

  ORA_TRAIT_INTERFACES(AutoCarryable,
                       OpenRA_Mods_Common_Traits_AutoCarryable, Carryable,
                       sim::IObservesVariables, sim::INotifyCreated,
                       sim::ICallForTransport)

  WDist MinimumDistance() const override {
    return auto_info_.dist_min_distance;
  }
  bool WantsTransport() const override {
    return opt_destination_.has_value() && !IsTraitDisabled();
  }

  void MovementCancelled(Actor& self) override {
    MovementCancelledInner();
  }
  void RequestTransport(Actor& self, CPos destination) override;

  std::optional<CPos> Destination() const { return opt_destination_; }

  void Detached(Actor& self) override;
  bool AutoReserve(Actor& self, Actor* carrier);
  LockResponse LockForPickup(Actor& self, Actor* carrier) override;

 private:
  bool IsValidAutoCarryDistance(Actor& self, CPos destination);
  void MovementCancelledInner();

  AutoCarryableInfoData auto_info_;
  bool b_auto_reserved_ = false;
  std::optional<CPos> opt_destination_;
};

struct CarryallInfoData {
  sim::ConditionalTraitData conditional;
  std::string str_initial_actor;
  int int4_before_load_delay = 0;
  int int4_before_unload_delay = 0;
  WVec wvec_local_offset{0, 0, 0};
  WDist dist_drop_range = WDist::FromCells(5);
  std::string str_unload_cursor{"deploy"};
  std::string str_unload_blocked_cursor{"deploy-blocked"};
  bool b_allow_drop_off = false;
  std::string str_drop_off_cursor{"ability"};
  std::string str_drop_off_blocked_cursor{"move-blocked"};
  std::string str_pick_up_cursor{"ability"};
  std::string str_carry_condition;
  std::vector<std::pair<std::string, std::string>> vec_carryable_conditions;
  std::string str_voice{"Action"};
  core::Color color_target_line = core::Color::FromArgbRaw(4294967040u);

  static CarryallInfoData Parse(const meta::RecordObject& rec_info);
};

/// Carryall.cs L91-96:CarryallState
enum class CarryallState { Idle, Reserved, Carrying };

class Carryall : public TraitBase,
                 public sim::ConditionalTraitCore<Carryall>,
                 public sim::IObservesVariables,
                 public sim::INotifyCreated,
                 public sim::INotifyKilled,
                 public sim::ISync,
                 public sim::ITick,
                 public sim::IRender,
                 public sim::INotifyActorDisposing,
                 public sim::IIssueOrder,
                 public sim::IResolveOrder,
                 public sim::IOrderVoice,
                 public sim::IIssueDeployOrder,
                 public sim::IAircraftCenterPositionOffset,
                 public sim::IOverrideAircraftLanding {
 public:
  Carryall(sim::ActorInitializer& init, const CarryallInfoData& info);

  ORA_TRAIT_INTERFACES(
      Carryall, OpenRA_Mods_Common_Traits_Carryall, sim::IObservesVariables,
      sim::INotifyCreated, sim::INotifyKilled, sim::ISync, sim::ITick,
      sim::IRender, sim::INotifyActorDisposing, sim::IIssueOrder,
      sim::IResolveOrder, sim::IOrderVoice, sim::IIssueDeployOrder,
      sim::IAircraftCenterPositionOffset, sim::IOverrideAircraftLanding)

  bool IsTraitEnabled() const override {
    return !sim::ConditionalTraitCore<Carryall>::IsTraitDisabled();
  }
  bool IsTraitDisabled() const override {
    return sim::ConditionalTraitCore<Carryall>::IsTraitDisabled();
  }
  void Created(sim::Actor& self) override;
  std::vector<sim::VariableObserver> GetVariableObservers() override {
    return CollectObservers();
  }

  const CarryallInfoData& Info() const { return info_; }

  Actor* CarryableActor() const { return p_carryable_; }
  CarryallState State() const { return carryall_state_; }
  WVec CarryableOffset() const { return wvec_carryable_offset_; }

  void Tick(Actor& self) override;
  void Disposing(Actor& self) override;
  void Killed(Actor& self, const sim::AttackInfo& e) override;

  virtual WVec OffsetForCarryable(Actor& self, Actor& carryable);
  WVec PositionOffset() const override;
  std::span<const std::string> LandableTerrainTypes() override;

  virtual bool AttachCarryable(Actor& self, Actor* carryable);
  virtual void DetachCarryable(Actor& self);
  bool ReserveCarryable(Actor& self, Actor* carryable);
  virtual void UnreserveCarryable(Actor& self);

  void Render(Actor& self, gfx::WorldRenderer& wr,
              std::vector<gfx::RenderItem>& vec_out) override;
  std::vector<Rectangle> ScreenBounds(Actor& self,
                                      gfx::WorldRenderer& wr) override;

  bool CanUnload();

  std::vector<sim::IOrderTargeter*> Orders() override;
  net::Order* IssueOrder(Actor& self, sim::IOrderTargeter* order,
                         const sim::Target& target, bool queued) override;
  net::Order IssueDeployOrder(Actor& self, bool queued) override;
  bool CanIssueDeployOrder(Actor& self, bool queued) override;
  void ResolveOrder(Actor& self, const net::Order& order) override;
  std::string VoicePhraseForOrder(Actor& self,
                                  const net::Order& order) override;

  class CarryallPickupOrderTargeter final : public sim::IOrderTargeter {
   public:
    CarryallPickupOrderTargeter(const CarryallInfoData& info)
        : info_{info} {}

    std::string OrderID() const override { return "PickupUnit"; }
    int OrderPriority() const override { return 5; }
    bool TargetOverridesSelection(Actor&, const sim::Target&,
                                  std::span<Actor* const>, CPos,
                                  sim::TargetModifiers) override {
      return true;
    }
    bool CanTarget(Actor& self, const sim::Target& target,
                   sim::TargetModifiers& modifiers,
                   std::string& cursor) override;
    bool IsQueued() const override { return b_is_queued_; }

   private:
    const CarryallInfoData& info_;
    bool b_is_queued_ = false;
  };

  class CarryallDeliverUnitTargeter final : public sim::IOrderTargeter {
   public:
    CarryallDeliverUnitTargeter(bool b_move_into_shroud,
                                const CarryallInfoData& info)
        : b_move_into_shroud_{b_move_into_shroud}, info_{info} {}

    std::string OrderID() const override { return "DeliverUnit"; }
    int OrderPriority() const override { return 6; }
    bool TargetOverridesSelection(Actor&, const sim::Target&,
                                  std::span<Actor* const>, CPos,
                                  sim::TargetModifiers) override {
      return true;
    }
    bool CanTarget(Actor& self, const sim::Target& target,
                   sim::TargetModifiers& modifiers,
                   std::string& cursor) override;
    bool IsQueued() const override { return b_is_queued_; }

   private:
    bool b_move_into_shroud_;
    const CarryallInfoData& info_;
    bool b_is_queued_ = false;
  };

 protected:
  Actor* p_carryable_ = nullptr;
  CarryallState carryall_state_ = CarryallState::Idle;

 private:
  CarryallInfoData info_;
  Actor& self_;
  Aircraft* aircraft_ = nullptr;
  BodyOrientation* body_ = nullptr;
  sim::IFacing* facing_ = nullptr;

  WAngle angle_cached_facing_{0};
  std::vector<std::unique_ptr<gfx::IActorPreview>> vec_carryable_preview_;
  std::optional<std::vector<std::string>> opt_landable_terrain_types_;
  int int4_carry_condition_token = sim::Actor::InvalidConditionToken;
  int int4_carryable_condition_token = sim::Actor::InvalidConditionToken;
  WVec wvec_carryable_offset_{0, 0, 0};
  std::unique_ptr<CarryallPickupOrderTargeter> ptr_pickup_targeter_;
  std::unique_ptr<sim::IOrderTargeter> ptr_unload_targeter_;
  std::unique_ptr<CarryallDeliverUnitTargeter> ptr_deliver_targeter_;
};

struct AutoCarryallInfoData {
  CarryallInfoData carryall;
  std::optional<expr::BooleanExpression> expr_auto_carry_condition;

  static AutoCarryallInfoData Parse(const meta::RecordObject& rec_info);
};

class AutoCarryall final : public Carryall,
                           public sim::INotifyBecomingIdle {
 public:
  AutoCarryall(sim::ActorInitializer& init, const AutoCarryallInfoData& info);

  ORA_TRAIT_INTERFACES(
      AutoCarryall, OpenRA_Mods_Common_Traits_AutoCarryall, Carryall,
      sim::IObservesVariables, sim::INotifyCreated, sim::INotifyKilled,
      sim::ISync, sim::ITick, sim::IRender, sim::INotifyActorDisposing,
      sim::IIssueOrder, sim::IResolveOrder, sim::IOrderVoice,
      sim::IIssueDeployOrder, sim::IAircraftCenterPositionOffset,
      sim::IOverrideAircraftLanding, sim::INotifyBecomingIdle)

  std::vector<sim::VariableObserver> GetVariableObservers() override;

  bool EnableAutoCarry() const { return b_enable_auto_carry_; }

  void OnBecomingIdle(Actor& self) override;

  bool RequestTransportNotify(Actor& self, Actor* carryable);

 private:
  bool AutoReserveCarryable(Actor& self, Actor* carryable);
  static bool IsBestAutoCarryallForCargo(Actor& self, Actor* candidate_cargo);
  void FindCarryableForTransport(Actor& self);
  void AutoCarryConditionsChanged(Actor& self,
                                  sim::ConditionCacheView conditions);

  AutoCarryallInfoData auto_info_;
  bool b_enable_auto_carry_ = true;
};

struct CarryableHarvesterInfoData {
  static CarryableHarvesterInfoData Parse(const meta::RecordObject& rec_info);
};

/// 上游两接口的 MovementCancelled(Actor) 同签名同转发体,C++ 单覆写
/// 同时满足两基类(语义等价:两处都是 foreach transports)
/// Upstream's two interfaces declare MovementCancelled(Actor) with the
/// same signature and body; a single C++ override satisfies both bases
/// (equivalent: both loop the transports).
class CarryableHarvester final : public TraitBase,
                                 public sim::INotifyCreated,
                                 public sim::INotifyHarvestAction,
                                 public sim::INotifyDockClientMoving {
 public:
  CarryableHarvester(const CarryableHarvesterInfoData& info);

  ORA_TRAIT_INTERFACES(CarryableHarvester,
                       OpenRA_Mods_Common_Traits_CarryableHarvester,
                       sim::INotifyCreated, sim::INotifyHarvestAction,
                       sim::INotifyDockClientMoving)

  const CarryableHarvesterInfoData& Info() const { return info_; }

  void Created(Actor& self) override;
  void MovingToResources(Actor& self, CPos target_cell) override;
  void MovementCancelled(Actor& self) override;
  void Harvested(Actor& self, const std::string& str_resource_type) override;
  void MovingToDock(Actor& self, Actor& host_actor,
                    sim::IDockHost* host) override;

 private:
  CarryableHarvesterInfoData info_;
  std::vector<sim::ICallForTransport*> vec_transports_;
};

}  // namespace ora::mods
