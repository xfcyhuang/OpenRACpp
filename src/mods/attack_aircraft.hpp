// UPSTREAM: OpenRA.Mods.Common/Traits/Air/AttackAircraft.cs +
//          AttackBomber.cs @1f594e320 + Activities/Air/FlyAttack.cs
#pragma once
import std;

#include "core/color.hpp"
#include "core/wdist.hpp"
#include "core/wpos.hpp"
#include "meta/generic_record.hpp"
#include "mods/attack_follow.hpp"
#include "sim/actor.hpp"
#include "sim/conditional_trait.hpp"
#include "sim/target.hpp"
#include "sim/trait_interfaces.hpp"

namespace ora::mods {

class Aircraft;
struct AircraftInfoData;
class Rearmable;

enum class AirAttackType : std::int32_t { Default, Hover, Strafe };

struct AttackAircraftInfoData : AttackFollowInfoData {
  AirAttackType kind_attack_type = AirAttackType::Default;
  WDist dist_strafe_run_length{0};

  static AttackAircraftInfoData Parse(const meta::RecordObject& rec_info);
};

class AttackAircraft final : public AttackFollow {
 public:
  AttackAircraft(ActorInitializer& init, const AttackAircraftInfoData& info);

  ORA_TRAIT_INTERFACES(AttackAircraft,
                       OpenRA_Mods_Common_Traits_AttackAircraft, AttackBaseFace,
                       sim::ITick, sim::IIssueOrder, sim::IResolveOrder,
                       sim::IOrderVoice, sim::ISync, sim::IObservesVariables,
                       sim::INotifyCreated, sim::INotifyOwnerChanged,
                       sim::IOverrideAutoTarget, INotifyStanceChanged)

  const AttackAircraftInfoData& AttackAircraftInfo() const {
    return attack_aircraft_info_;
  }

  sim::Activity* GetAttackActivity(
      Actor& self, AttackSource source, const sim::Target& new_target,
      bool allow_move, bool force_attack,
      std::optional<core::Color> target_line_color) override;

 protected:
  bool CanAttack(Actor& self, const sim::Target& target) override;

 private:
  AttackAircraftInfoData attack_aircraft_info_;
  const meta::RecordObject* ptr_aircraft_rec_ = nullptr;
};

struct AttackBomberInfoData : AttackBaseInfoData {
  static AttackBomberInfoData Parse(const meta::RecordObject& rec_info);
};

class AttackBomber final : public AttackBaseCore<AttackBomber>,
                           public sim::INotifyRemovedFromWorld {
 public:
  AttackBomber(ActorInitializer& init, const AttackBomberInfoData& info);

  ORA_TRAIT_INTERFACES(AttackBomber, OpenRA_Mods_Common_Traits_AttackBomber,
                       AttackBaseFace, sim::ITick, sim::IIssueOrder,
                       sim::IResolveOrder, sim::IOrderVoice, sim::ISync,
                       sim::INotifyRemovedFromWorld)

  using OnRemovedFromWorldHandler = std::function<void(Actor&)>;
  using OnEnteredAttackRangeHandler = std::function<void(Actor&)>;
  using OnExitedAttackRangeHandler = std::function<void(Actor&)>;

  void AddOnRemovedFromWorld(OnRemovedFromWorldHandler handler);
  void AddOnEnteredAttackRange(OnEnteredAttackRangeHandler handler);
  void AddOnExitedAttackRange(OnExitedAttackRangeHandler handler);

  void Tick(Actor& self) override;
  void RemovedFromWorld(Actor& self) override;

  void SetTarget(WPos pos);

  sim::Activity* GetAttackActivity(
      Actor& self, AttackSource source, const sim::Target& new_target,
      bool allow_move, bool force_attack,
      std::optional<core::Color> target_line_color) override;

  const sim::Target& BomberTarget() const { return target_; }
  bool InAttackRange() const { return b_in_attack_range_; }

 private:
  AttackBomberInfoData info_;
  sim::Target target_;
  bool b_in_attack_range_ = false;
  bool b_facing_target_ = true;
  std::vector<OnRemovedFromWorldHandler> vec_on_removed_from_world_;
  std::vector<OnEnteredAttackRangeHandler> vec_on_entered_attack_range_;
  std::vector<OnExitedAttackRangeHandler> vec_on_exited_attack_range_;
};

namespace activities {

class FlyAttack final : public sim::Activity,
                        public IActivityNotifyStanceChanged {
 public:
  FlyAttack(sim::Actor& self, AttackSource source, const sim::Target& target,
            bool force_attack, std::optional<core::Color> target_line_color);

  bool Tick(sim::Actor& self) override;
  void OnLastRun(sim::Actor& self) override;
  void StanceChanged(sim::Actor& self, AutoTarget* auto_target,
                     UnitStance old_stance, UnitStance new_stance) override;

 private:
  bool HasArmamentsFor(const sim::Target& target);

  Aircraft* aircraft_ = nullptr;
  AttackAircraft* attack_aircraft_ = nullptr;
  Rearmable* rearmable_ = nullptr;
  AttackSource source_;
  bool b_force_attack_;
  std::optional<core::Color> opt_target_line_color_;
  WDist dist_strafe_distance_{0};
  sim::Target target_;
  sim::Target last_visible_target_;
  WDist dist_last_visible_maximum_range_{0};
  core::BitSet<sim::TargetableType> bitset_last_visible_target_types_;
  sim::Player* p_last_visible_owner_ = nullptr;
  bool b_use_last_visible_target_ = false;
  bool b_has_ticked_ = false;
  bool b_return_to_base_ = false;
};

class FlyAttackRun final : public sim::Activity {
 public:
  FlyAttackRun(const sim::Target& t, WDist exit_range,
               AttackAircraft& attack);

  void OnFirstRun(sim::Actor& self) override;
  bool Tick(sim::Actor& self) override;

 private:
  AttackAircraft& attack_;
  WDist dist_exit_range_;
  sim::Target target_;
  bool b_target_is_visible_actor_ = false;
};

class StrafeAttackRun final : public sim::Activity {
 public:
  StrafeAttackRun(AttackAircraft& attack_aircraft, Aircraft& aircraft,
                  const sim::Target& t, WDist exit_range);

  void OnFirstRun(sim::Actor& self) override;
  bool Tick(sim::Actor& self) override;

 private:
  AttackAircraft& attack_aircraft_;
  Aircraft& aircraft_;
  WDist dist_exit_range_;
  sim::Target target_;
};

}  // namespace activities

}  // namespace ora::mods
