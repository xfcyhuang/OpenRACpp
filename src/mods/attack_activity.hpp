// UPSTREAM: OpenRA.Mods.Common/Activities/Attack.cs @b6fc03f L22-283 全文
//          (逐语义重写;TargetLineNodes 渲染面随 Phase 6 —— 活动基类未引入)
//          The whole of Attack.cs L22-283 (verbatim-semantics rewrite;
//          TargetLineNodes' render face lands with Phase 6 — not carried
//          by the Activity base).
//
// 机制对照 / Mechanism mapping:
//  - attackTraits 的 IEnumerable<AttackBase>(非禁用过滤)→ 构造期物化
//    向量;armaments 的 LINQ 物化(ToList)→ 复用调用点局部 vector
//    attackTraits' IEnumerable<AttackBase> (the non-disabled filter) →
//    the construction-time materialized vector; armaments' LINQ ToList →
//    a reused call-site local vector.
//  - StanceChanged 的 autoTarget.HasValidTargetPriority 消费 = AutoTarget
//    具体类(本批挂点换实)
//    StanceChanged's autoTarget.HasValidTargetPriority consumption = the
//    AutoTarget concrete class (this batch's wiring).
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

/// Attack.cs L23-282(non-turreted attack)
class Attack : public sim::Activity,
               public IActivityNotifyStanceChanged {
 public:
  /// Attack.cs L25-26:AttackStatus([Flags])
  /// Attack.cs L25-26: AttackStatus ([Flags]).
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
  /// L91-94:RecalculateTarget(virtual)
  /// L91-94: RecalculateTarget (virtual).
  virtual sim::Target RecalculateTarget(sim::Actor& self,
                                        bool& b_target_is_hidden_actor);

  /// L164-252:TickAttack(virtual)
  /// L164-252: TickAttack (virtual).
  virtual AttackStatus TickAttack(sim::Actor& self,
                                   AttackBaseFace* attack);

  /// L254-259:DoAttack(virtual)
  /// L254-259: DoAttack (virtual).
  virtual void DoAttack(sim::Actor& self, AttackBaseFace* attack,
                        const std::vector<Armament*>& vec_armaments);

  /// L278-281:HasArmamentsFor | L278-281: HasArmamentsFor.
  bool HasArmamentsFor(const sim::Target& target);

  std::vector<AttackBaseFace*> vec_attack_traits_;  // L28
  std::vector<RevealsShroud*> vec_reveals_shroud_;  // L29
  sim::IMove* move_ = nullptr;                      // L30
  Mobile* mobile_ = nullptr;                        // L31
  sim::IFacing* facing_ = nullptr;                  // L32
  sim::IPositionable* positionable_ = nullptr;      // L33
  bool b_force_attack_ = false;                     // L34
  std::optional<core::Color> opt_target_line_color_;  // L35
  MoveCooldownHelper move_cooldown_helper_;           // L36

  sim::Target target_;                        // L38
  sim::Target last_visible_target_;           // L39
  WDist dist_last_visible_maximum_range_{0};  // L40
  core::BitSet<sim::TargetableType>
      bitset_last_visible_target_types_;      // L41
  sim::Player* p_last_visible_owner_ = nullptr;  // L42
  bool b_use_last_visible_target_ = false;       // L43

  WDist dist_min_range_{0};   // L45
  WDist dist_max_range_{0};   // L46
  AttackStatus attack_status_ = AttackStatus::UnableToAttack;  // L47
};

}  // namespace ora::mods::activities
