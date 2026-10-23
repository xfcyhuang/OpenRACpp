// UPSTREAM: OpenRA.Mods.Common/Traits/Cargo.cs + ParaDrop.cs @e180e3225
#pragma once
import std;

#include "core/wangle.hpp"
#include "core/wdist.hpp"
#include "meta/generic_record.hpp"
#include "mods/deploy_order_targeter.hpp"
#include "sim/actor.hpp"
#include "sim/actor_init.hpp"
#include "sim/conditional_trait.hpp"
#include "sim/trait_interfaces.hpp"

namespace ora::mods {

using sim::Actor;
using sim::Player;
using sim::TraitBase;

class Passenger;
class Aircraft;
class Cargo;

struct CargoInfoData {
  sim::ConditionalTraitData conditional;
  int int4_max_weight = 0;
  std::vector<std::string> vec_types;
  std::vector<std::string> vec_initial_units;
  bool b_eject_on_sell = true;
  bool b_eject_on_death = false;
  std::vector<std::string> vec_unload_terrain_types;
  std::string str_unload_voice{"Action"};
  WDist dist_load_range = WDist::FromCells(5);
  WAngle angle_passenger_facing{512};
  int int4_after_load_delay = 8;
  int int4_before_unload_delay = 8;
  int int4_after_unload_delay = 25;
  int int4_between_unload_delay = 0;
  std::string str_unload_cursor{"deploy"};
  std::string str_unload_blocked_cursor{"deploy-blocked"};
  std::string str_loading_condition;
  std::string str_loaded_condition;
  std::vector<std::pair<std::string, std::string>> vec_passenger_conditions;

  static CargoInfoData Parse(const meta::RecordObject& rec_info);
};

class Cargo final : public TraitBase,
                    public sim::ConditionalTraitCore<Cargo>,
                    public sim::IObservesVariables,
                    public sim::IIssueOrder,
                    public sim::IResolveOrder,
                    public sim::IOrderVoice,
                    public sim::IIssueDeployOrder,
                    public sim::INotifyOwnerChanged,
                    public sim::INotifySold,
                    public sim::INotifyActorDisposing,
                    public sim::INotifyCreated,
                    public sim::INotifyKilled,
                    public sim::ITransformActorInitModifier {
 public:
  Cargo(sim::ActorInitializer& init, const CargoInfoData& info,
        std::string str_info_instance_name);

  ORA_TRAIT_INTERFACES(Cargo, OpenRA_Mods_Common_Traits_Cargo,
                       sim::IObservesVariables, sim::IIssueOrder,
                       sim::IResolveOrder, sim::IOrderVoice,
                       sim::IIssueDeployOrder, sim::INotifyOwnerChanged,
                       sim::INotifySold, sim::INotifyActorDisposing,
                       sim::INotifyCreated, sim::INotifyKilled,
                       sim::ITransformActorInitModifier)

  bool IsTraitEnabled() const override {
    return !sim::ConditionalTraitCore<Cargo>::IsTraitDisabled();
  }
  bool IsTraitDisabled() const override {
    return sim::ConditionalTraitCore<Cargo>::IsTraitDisabled();
  }
  void Created(sim::Actor& self) override;
  std::vector<sim::VariableObserver> GetVariableObservers() override {
    return CollectObservers();
  }

  const CargoInfoData& Info() const { return info_; }

  std::span<Actor* const> Passengers() const { return vec_cargo_; }
  int PassengerCount() const {
    return static_cast<int>(vec_cargo_.size());
  }

  std::vector<sim::IOrderTargeter*> Orders() override;
  std::unique_ptr<net::Order> IssueOrder(
      Actor& self, sim::IOrderTargeter* order,
      const sim::Target& target, bool queued) override;
  net::Order IssueDeployOrder(Actor& self, bool queued) override;
  bool CanIssueDeployOrder(Actor& self, bool queued) override;
  void ResolveOrder(Actor& self, const net::Order& order) override;
  std::string VoicePhraseForOrder(Actor& self,
                                  const net::Order& order) override;

  std::vector<CPos> CurrentAdjacentCells();
  bool CanUnload(sim::BlockedByActor check = sim::BlockedByActor::None);
  bool CanLoad(Actor& a);
  bool ReserveSpace(Actor& a);
  void UnreserveSpace(Actor& a);
  bool HasSpace(int weight) const {
    return int4_total_weight_ + int4_reserved_weight_ + weight <=
           info_.int4_max_weight;
  }
  bool IsEmpty() const { return vec_cargo_.empty(); }
  Actor* Peek() { return vec_cargo_.back(); }
  Actor* Unload(Actor& self, Actor* passenger = nullptr);
  void Load(Actor& self, Actor& a);

  void Selling(Actor& self) override {}
  void Sold(Actor& self) override;
  void Disposing(Actor& self) override;
  void Killed(Actor& self, const sim::AttackInfo& e) override;
  void OnOwnerChanged(Actor& self, Player& old_owner,
                      Player& new_owner) override;
  void ModifyTransformActorInit(Actor& self,
                                sim::TypeDictionary& init) override;

  static int GetWeight(Actor& a);

 private:
  void LockForPickup(Actor& self);
  void ReleaseLock(Actor& self);
  void SetPassengerFacing(Actor& passenger);
  void SpawnPassenger(Actor* passenger);

  CargoInfoData info_;
  std::string str_info_instance_name_;
  Actor& self_;
  std::vector<Actor*> vec_cargo_;
  std::vector<Actor*> vec_reserves_;
  std::map<std::string, std::vector<int>> map_passenger_tokens_;
  sim::IFacing* ptr_facing_ = nullptr;
  bool b_check_terrain_type = false;
  std::unique_ptr<DeployOrderTargeter> ptr_order_targeter_;

  int int4_total_weight_ = 0;
  int int4_reserved_weight_ = 0;
  Aircraft* ptr_aircraft_ = nullptr;
  int int4_loading_token = sim::Actor::InvalidConditionToken;
  std::vector<int> vec_loaded_tokens_;
  bool b_take_off_after_load = false;
  bool b_initialised = false;

  enum class State { Free, Locked };
  State state_ = State::Free;
};

struct ParaDropInfoData {
  WDist dist_drop_range = WDist::FromCells(4);
  int int4_drop_interval = 5;
  std::string str_chute_sound;

  static ParaDropInfoData Parse(const meta::RecordObject& rec_info);
};

class ParaDrop final : public TraitBase,
                       public sim::ITick,
                       public sim::ISync,
                       public sim::INotifyRemovedFromWorld {
 public:
  using OnRemovedFromWorldHandler = std::function<void(Actor&)>;
  using OnEnteredDropRangeHandler = std::function<void(Actor&)>;
  using OnExitedDropRangeHandler = std::function<void(Actor&)>;

  ParaDrop(Actor& self, const ParaDropInfoData& info);

  ORA_TRAIT_INTERFACES(ParaDrop, OpenRA_Mods_Common_Traits_ParaDrop,
                       sim::ITick, sim::ISync, sim::INotifyRemovedFromWorld)

  void AddOnRemovedFromWorld(OnRemovedFromWorldHandler handler) {
    vec_on_removed_from_world_.push_back(std::move(handler));
  }
  void AddOnEnteredDropRange(OnEnteredDropRangeHandler handler) {
    vec_on_entered_drop_range_.push_back(std::move(handler));
  }
  void AddOnExitedDropRange(OnExitedDropRangeHandler handler) {
    vec_on_exited_drop_range_.push_back(std::move(handler));
  }

  void SetLZ(CPos lz, bool check_landing_cell);

  void Tick(Actor& self) override;
  void RemovedFromWorld(Actor& self) override;

 private:
  friend int ParaDropSyncHash(const sim::ISync* s);

  ParaDropInfoData info_;
  Actor& self_;
  Cargo& cargo_;

  std::vector<OnRemovedFromWorldHandler> vec_on_removed_from_world_;
  std::vector<OnEnteredDropRangeHandler> vec_on_entered_drop_range_;
  std::vector<OnExitedDropRangeHandler> vec_on_exited_drop_range_;

  bool b_in_drop_range_ = false;
  sim::Target target_{};
  int int4_drop_delay_ = 0;
  bool b_check_for_suitable_cell_ = false;
};

}  // namespace ora::mods
