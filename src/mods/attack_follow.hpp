// UPSTREAM: OpenRA.Mods.Common/Traits/Attack/AttackFollow.cs @b6fc03fc
//          L22-466 全文 + AttackTurreted.cs L18-51 全文(逐语义重写;
//          嵌套 AttackActivity 以 activities::AttackFollowActivity 承载)
//          The whole of AttackFollow.cs L22-466 + the whole of
//          AttackTurreted.cs L18-51 (verbatim-semantics rewrites; the
//          nested AttackActivity rides activities::AttackFollowActivity).
//
// 机制对照 / Mechanism mapping:
//  - C# 继承链 AttackFollow : AttackBase → AttackFollow :
//    AttackBaseCore<AttackFollow>(CRTP;AttackTurreted 再继承 AttackFollow
//    —— 基类接口集在 AttackTurreted 的 upcast 表重列,Classic 同法)
//    C#'s AttackFollow : AttackBase chain → AttackFollow :
//    AttackBaseCore<AttackFollow> (CRTP; AttackTurreted then derives from
//    AttackFollow — the base's interface set is re-listed in
//    AttackTurreted's upcast table, Classic's device).
//  - AttackFollowInfo 的 `new` Info 隐藏 → 基类切片拷贝 + 派生字段成员
//    (info 不可变;两份数据同值)
//    AttackFollowInfo's `new` Info hiding → the base-slice copy + the
//    derived-field member (info is immutable; both copies hold the same
//    values).
//  - Rearmable/Resupply/ReturnToBase/AircraftInfo 未移植:AttackActivity 的
//    补给分支不可达(rearmable == null / isAircraft = false 的上游等价
//    面;COVERAGE 登记)
//    Rearmable/Resupply/ReturnToBase/AircraftInfo unported: the
//    AttackActivity resupply branch is unreachable (upstream's
//    rearmable == null / isAircraft = false equivalent faces; registered
//    in COVERAGE).
#pragma once
import std;

#include "core/color.hpp"
#include "core/wdist.hpp"
#include "mods/affects_shroud.hpp"
#include "mods/attack_base.hpp"
#include "mods/auto_target.hpp"
#include "mods/mobile.hpp"
#include "mods/move_activities.hpp"
#include "sim/actor.hpp"
#include "sim/conditional_trait.hpp"
#include "sim/target.hpp"
#include "sim/trait_interfaces.hpp"

namespace ora::mods {

class Turreted;
class AttackFollow;

namespace activities {

/// AttackFollow.cs L231-464:AttackActivity(嵌套类;命名空间承载避免与
/// Activities/Attack 混淆)
/// AttackFollow.cs L231-464: AttackActivity (the nested class; carried in
/// the activities namespace to avoid the clash with Activities/Attack).
class AttackFollowActivity final : public sim::Activity,
                                   public IActivityNotifyStanceChanged {
 public:
  AttackFollowActivity(sim::Actor& self, AttackSource source,
                       const sim::Target& target, bool allow_move,
                       bool force_attack,
                       std::optional<core::Color> target_line_color);

  bool Tick(sim::Actor& self) override;
  void OnLastRun(sim::Actor& self) override;

  void StanceChanged(sim::Actor& self, AutoTarget* auto_target,
                     UnitStance old_stance, UnitStance new_stance) override;

 private:
  /// L460-463:HasArmamentsFor | L460-463: HasArmamentsFor.
  bool HasArmamentsFor(const sim::Target& target);

  AttackFollow* p_attack_;
  std::vector<RevealsShroud*> vec_reveals_shroud_;
  sim::IMove* p_move_ = nullptr;
  bool b_force_attack_;
  std::optional<core::Color> opt_target_line_color_;
  // Rearmable trait 未移植:恒 null(补给分支不可达;COVERAGE 登记)
  // The Rearmable trait is unported: constantly null (the resupply
  // branch is unreachable; registered in COVERAGE).
  // readonly Rearmable rearmable;
  AttackSource source_;
  bool b_is_aircraft_ = false;  // AircraftInfo 未注册 → 恒 false(同上)
  MoveCooldownHelper move_cooldown_helper_;

  sim::Target target_;
  sim::Target last_visible_target_;
  bool b_use_last_visible_target_ = false;
  WDist dist_last_visible_maximum_range_{0};
  WDist dist_last_visible_minimum_range_{0};
  core::BitSet<sim::TargetableType> bitset_last_visible_target_types_;
  sim::Player* p_last_visible_owner_ = nullptr;
  bool b_has_ticked_ = false;
  bool b_return_to_base_ = false;
};

}  // namespace activities


/// AttackFollowInfo 的解析面(L24-38)
/// The parsed face of AttackFollowInfo (L24-38).
struct AttackFollowInfoData : AttackBaseInfoData {
  bool b_opportunity_fire = true;      // L26
  bool b_persistent_targeting = true;  // L29
  WDist dist_range_margin{1024};       // L32 WDist.FromCells(1)
  bool b_abort_on_resupply = true;     // L35

  static AttackFollowInfoData Parse(const meta::RecordObject& rec_info);
};

/// AttackTurretedInfo 的解析面(AttackTurreted.cs L18-26)
/// The parsed face of AttackTurretedInfo (AttackTurreted.cs L18-26).
struct AttackTurretedInfoData : AttackFollowInfoData {
  std::vector<std::string> vec_turrets{"primary"};  // L22

  static AttackTurretedInfoData Parse(const meta::RecordObject& rec_info);
};

/// AttackFollow(L40-465)
class AttackFollow : public AttackBaseCore<AttackFollow>,
                     public sim::IObservesVariables,
                     public sim::INotifyCreated,
                     public sim::INotifyOwnerChanged,
                     public sim::IOverrideAutoTarget,
                     public INotifyStanceChanged {
 public:
  AttackFollow(ActorInitializer& init, const AttackFollowInfoData& info);

  ORA_TRAIT_INTERFACES(
      AttackFollow, OpenRA_Mods_Common_Traits_AttackFollow, AttackBaseFace,
      sim::ITick, sim::IIssueOrder, sim::IResolveOrder, sim::IOrderVoice,
      sim::ISync, sim::IObservesVariables, sim::INotifyCreated,
      sim::INotifyOwnerChanged, sim::IOverrideAutoTarget,
      INotifyStanceChanged)

  bool IsTraitEnabled() const override {
    return !sim::ConditionalTraitCore<AttackFollow>::IsTraitDisabled();
  }
  bool IsTraitDisabled() const override {
    return sim::ConditionalTraitCore<AttackFollow>::IsTraitDisabled();
  }

  void Created(Actor& self) override;
  std::vector<sim::VariableObserver> GetVariableObservers() override {
    return CollectObservers();
  }

  // ———— 请求/机会目标面(L43-44/53-71)————
  // ———— The requested/opportunity target faces (L43-44/53-71) ————

  const sim::Target& RequestedTarget() const { return target_requested_; }
  const sim::Target& OpportunityTarget() const {
    return target_opportunity_;
  }

  /// L53-58:SetRequestedTarget | L53-58: SetRequestedTarget.
  void SetRequestedTarget(const sim::Target& target,
                          bool is_force_attack = false,
                          sim::Activity* requested_target_preset = nullptr);

  /// L60-71:ClearRequestedTarget | L60-71: ClearRequestedTarget.
  void ClearRequestedTarget();

  /// L86-103:CanAimAtTarget(protected virtual)
  /// L86-103: CanAimAtTarget (protected virtual).
  virtual bool CanAimAtTarget(Actor& self, const sim::Target& target,
                              bool force_attack);

  /// L105-160:Tick | L105-160: Tick.
  void Tick(Actor& self) override;

  /// L162-170:GetAttackActivity
  /// L162-170: GetAttackActivity.
  sim::Activity* GetAttackActivity(
      Actor& self, AttackSource source, const sim::Target& new_target,
      bool allow_move, bool force_attack,
      std::optional<core::Color> target_line_color = std::nullopt) override;

  /// L172-178:OnResolveAttackOrder(响应性预置)
  /// L172-178: OnResolveAttackOrder (the responsiveness preemption).
  void OnResolveAttackOrder(Actor& self, sim::Activity* activity,
                            const sim::Target& target, bool queued,
                            bool force_attack) override;

  /// L179-185:OnStopOrder | L179-185: OnStopOrder.
  void OnStopOrder(Actor& self) override;

  /// L187-191:INotifyOwnerChanged
  /// L187-191: INotifyOwnerChanged.
  void OnOwnerChanged(Actor& self, Player& old_owner,
                      Player& new_owner) override;

  /// L193-209:IOverrideAutoTarget(optional 返回面)
  /// L193-209: IOverrideAutoTarget (the optional-return face).
  std::optional<sim::Target> TryGetAutoTargetOverride(
      Actor& self) override;

  /// L211-229:INotifyStanceChanged
  /// L211-229: INotifyStanceChanged.
  void StanceChanged(Actor& self, AutoTarget* auto_target,
                     UnitStance old_stance, UnitStance new_stance) override;

  void TraitEnabledHook(Actor& /*self*/) {}
  void TraitDisabledHook(Actor& /*self*/) {}
  void TraitResumedHook(Actor& /*self*/) {}
  void TraitPausedHook(Actor& /*self*/) {}

  const AttackFollowInfoData& FollowInfo() const { return info_follow_; }

 protected:
  Mobile* p_mobile_ = nullptr;      // L46
  AutoTarget* p_auto_target_ = nullptr;  // L47

 private:
  friend class sim::ConditionalTraitCore<AttackFollow>;

  AttackFollowInfoData info_follow_;  // L42 的派生 Info 面(基类持切片)

  sim::Target target_requested_ = sim::Target::Invalid();
  sim::Target target_opportunity_ = sim::Target::Invalid();
  bool b_requested_force_attack_ = false;        // L48
  sim::Activity* p_requested_target_preset_for_activity_ = nullptr;  // L49
  bool b_opportunity_force_attack_ = false;      // L50
  bool b_opportunity_target_is_persistent_target_ = false;           // L51
};

/// AttackTurreted(AttackTurreted.cs L27-50)
class AttackTurreted final : public AttackFollow {
 public:
  AttackTurreted(ActorInitializer& init,
                 const AttackTurretedInfoData& info);

  // upcast 表重列基类全接口集(上游 GetInterfaces() 含继承;漏列 = 分发断链)
  // The upcast table re-lists the base's full interface set (upstream's
  // GetInterfaces() includes inherited ones; omitting them breaks dispatch).
  ORA_TRAIT_INTERFACES(
      AttackTurreted, OpenRA_Mods_Common_Traits_AttackTurreted, AttackFollow,
      AttackBaseFace, sim::ITick, sim::IIssueOrder, sim::IResolveOrder,
      sim::IOrderVoice, sim::ISync, sim::IObservesVariables,
      sim::INotifyCreated, sim::INotifyOwnerChanged, sim::IOverrideAutoTarget,
      INotifyStanceChanged)

  /// L37-49:CanAttack(全炮塔就位判定)
  /// L37-49: CanAttack (the all-turrets-brought-to-bear check).
  bool CanAttack(Actor& self, const sim::Target& target) override;

  const AttackTurretedInfoData& TurretInfo() const { return info_turrets_; }

  std::span<Turreted* const> Turrets() const { return vec_turrets_; }

 private:
  AttackTurretedInfoData info_turrets_;  // L28-34 的 Turrets 面
  std::vector<Turreted*> vec_turrets_;   // L29/34
};

}  // namespace ora::mods
