#pragma once
import std;

#include "core/wdist.hpp"
#include "mods/affects_shroud.hpp"
#include "mods/armament.hpp"
#include "mods/attack_base.hpp"
#include "mods/auto_target.hpp"
#include "mods/mobile.hpp"
#include "mods/move_activities.hpp"
#include "sim/activity.hpp"
#include "sim/target.hpp"

namespace ora::mods::activities {

class Attack : public sim::Activity,
               public IActivityNotifyStanceChanged {
 public:

  enum class AttackStatus : std::int32_t {
    UnableToAttack = 0,
    NeedsToTurn = 1,
    NeedsToMove = 2,
    Attacking = 4,
  };

  Attack(sim::Actor& self, const sim::Target& target, bool allow_movement,
         bool force_attack,
         std::optional<core::Color> target_line_color = std::nullopt);

  bool Tick(sim::Actor& self) override;
  void OnLastRun(sim::Actor& self) override;

  void StanceChanged(sim::Actor& self, AutoTarget* auto_target,
                     UnitStance old_stance, UnitStance new_stance) override;

 protected:

  virtual sim::Target RecalculateTarget(sim::Actor& self,
                                        bool& b_target_is_hidden_actor);

  virtual AttackStatus TickAttack(sim::Actor& self,
                                   AttackBaseFace* attack);

  virtual void DoAttack(sim::Actor& self, AttackBaseFace* attack,
                        const std::vector<Armament*>& vec_armaments);

  bool HasArmamentsFor(const sim::Target& target);

  std::vector<AttackBaseFace*> vec_attack_traits_;
  std::vector<RevealsShroud*> vec_reveals_shroud_;
  sim::IMove* move_ = nullptr;
  Mobile* mobile_ = nullptr;
  sim::IFacing* facing_ = nullptr;
  sim::IPositionable* positionable_ = nullptr;
  bool b_force_attack_ = false;
  std::optional<core::Color> opt_target_line_color_;
  MoveCooldownHelper move_cooldown_helper_;

  sim::Target target_;
  sim::Target last_visible_target_;
  WDist dist_last_visible_maximum_range_{0};
  core::BitSet<sim::TargetableType>
      bitset_last_visible_target_types_;
  sim::Player* p_last_visible_owner_ = nullptr;
  bool b_use_last_visible_target_ = false;

  WDist dist_min_range_{0};
  WDist dist_max_range_{0};
  AttackStatus attack_status_ = AttackStatus::UnableToAttack;
};

}
