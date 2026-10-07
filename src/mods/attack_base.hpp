// UPSTREAM: OpenRA.Mods.Common/Traits/Attack/AttackBase.cs @b6fc03f
//          L23-526 全文(逐语义重写;AttackFrontal.cs L18-48 同文件承载)
//          The whole of AttackBase.cs L23-526 + AttackFrontal.cs L18-48
//          (verbatim-semantics rewrite).
//
// 机制对照 / Mechanism mapping:
//  - abstract AttackBase + PausableConditionalTrait → AttackBaseCore<
//    Derived> 模板基(CRTP;ConditionalTraitCore 同构 —— 与 AffectsShroud
//    同法);具体类(AttackFrontal/后续 AttackFollow 族)挂 TypeId
//    The abstract AttackBase + PausableConditionalTrait → the
//    AttackBaseCore<Derived> template base (CRTP; the ConditionalTraitCore
//    isomorph — the same device as AffectsShroud); the concrete classes
//    (AttackFrontal / the later AttackFollow family) carry the TypeId.
//  - getArmaments 的 Func<IEnumerable<Armament>> 闭包 → Created 一次性
//    过滤物化(上游 InitializeGetArmaments 的 ToArray 缓存同值)
//    getArmaments' Func<IEnumerable<Armament>> closure → the one-shot
//    filtered materialization at Created (the same value as upstream's
//    InitializeGetArmaments ToArray cache).
//  - OrderBy(IsTraitPaused).ThenByDescending(MaxRange).First 的稳定序 →
//    单遍择优(键 (paused 升序, range 降序);平键首见者胜 = 稳定序等价)
//    OrderBy(IsTraitPaused).ThenByDescending(MaxRange).First's stable
//    order → a single-pass pick (the key (paused ascending, range
//    descending); the first-seen wins ties — the stable-order
//    equivalent).
//  - 上游 Cursor 的 string ?? 回退:空串 = null(游标名非空域)
//    Upstream's string ?? cursor fallback: an empty string = null (the
//    cursor-name domain is non-empty).
//  - OnStopOrder 的 Resupply/ReturnToBase 活动判:两类活动未移植 → 恒 false
//    的上游等价分支(COVERAGE 登记)
//    OnStopOrder's Resupply/ReturnToBase activity checks: both activity
//    classes are unported → upstream's constantly-false equivalent branch
//    (registered in COVERAGE).
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

/// AttackBase 的接口面(上游以具体类 AttackBase 充当接口 —— AutoTarget 的
/// TraitsImplementing<AttackBase> 查询族;TypeId 即上游 AttackBase 键)。
/// 第六批引入:AttackFollow 族落地后 AutoTarget/Move 族不再硬绑 AttackFrontal
/// The interface face of AttackBase (upstream's concrete class doubles as
/// the interface — AutoTarget's TraitsImplementing<AttackBase> query
/// family; the TypeId is upstream's AttackBase key itself). Introduced
/// with batch 6: once the AttackFollow family lands, AutoTarget/the move
/// family stop being hard-wired to AttackFrontal.
/// AttackBase.cs L23:AttackSource
/// AttackBase.cs L23: AttackSource.
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

  /// IsAiming(L78 的 [VerifySync] 面;Turreted 的 realign 判据;活动侧的
  /// 公开写 = 上游 public set)
  /// IsAiming (the [VerifySync] face at L78; Turreted's realign
  /// predicate; the activities' public write = upstream's public set).
  virtual bool IsAiming() const = 0;
  virtual void SetIsAiming(bool value) = 0;

  /// TraitBase::IsTraitDisabled 的面投影(具体类的 override 同时支配
  /// 两条基路径)
  /// The face projection of TraitBase::IsTraitDisabled (the concrete
  /// class's override dominates both base paths).
  virtual bool IsTraitDisabled() const { return false; }
  virtual bool IsTraitPaused() const = 0;
  virtual bool AttackRequiresEnteringCell() const = 0;

  /// AutoTarget.ScanForTarget 的消费面
  /// The consumption faces of AutoTarget.ScanForTarget.
  virtual sim::PlayerRelationship UnforcedAttackTargetStances() = 0;
  virtual WDist GetMaximumRange() = 0;
  virtual void AttackTarget(const sim::Target& target, AttackSource source,
                            bool queued, bool allow_move,
                            bool force_attack = false,
                            std::optional<core::Color> target_line_color =
                                std::nullopt) = 0;

  /// ChooseTarget 的消费面(ab.Info.TargetFrozenActors/FacingTolerance →
  /// 值访问器;ChooseArmamentsForTarget/TargetInFiringArc)
  /// ChooseTarget's consumption faces (ab.Info.TargetFrozenActors/
  /// FacingTolerance → value accessors; ChooseArmamentsForTarget/
  /// TargetInFiringArc).
  virtual bool TargetFrozenActors() const = 0;
  virtual WAngle FacingTolerance() const = 0;
  virtual std::vector<Armament*> ChooseArmamentsForTarget(
      const sim::Target& t, bool force_attack) = 0;

  /// AutoTarget.Damaged 的还击判定面
  /// The retaliation predicate face of AutoTarget.Damaged.
  virtual bool HasAnyValidWeapons(
      const sim::Target& t, bool check_for_center_targeting_weapons = false,
      bool reloading_is_invalid = false) = 0;
  virtual bool TargetInFiringArc(Actor& self, const sim::Target& target,
                                 WAngle facing_tolerance) = 0;

  /// AttackFollow 活动族的范围消费面
  /// The range consumption faces of the AttackFollow activity family.
  virtual WDist GetMinimumRange() = 0;
  virtual WDist GetMinimumRangeVersusTarget(const sim::Target& target) = 0;
  virtual WDist GetMaximumRangeVersusTarget(const sim::Target& target) = 0;

  /// Turreted::FaceTarget 的目标位消费面
  /// The target-position consumption face of Turreted::FaceTarget.
  virtual WPos GetTargetPosition(const WPos& pos,
                                 const sim::Target& target) = 0;
  virtual void OnStopOrder(Actor& self) = 0;
};


/// AttackBaseInfo 的解析面(L25-71)
/// The parsed face of AttackBaseInfo (L25-71).
struct AttackBaseInfoData {
  std::vector<std::string> vec_armaments{"primary",
                                         "secondary"};  // L28
  std::string str_cursor;                             // L32
  std::string str_outside_range_cursor;               // L35
  core::Color target_line_color{
      core::Color::FromArgbRaw(0xFFDC143C)};           // L39 Crimson
  bool b_attack_requires_entering_cell = false;       // L42
  bool b_target_frozen_actors = false;                // L45
  bool b_force_fire_ignores_actors = false;           // L48
  bool b_outside_range_requires_force_fire = false;   // L51
  std::string str_voice{"Action"};                    // L54
  WAngle angle_facing_tolerance{512};                 // L57
  bool b_target_terrain_without_force_fire = false;   // L60
  sim::ConditionalTraitData conditional;

  static AttackBaseInfoData Parse(const meta::RecordObject& rec_info);
};

/// AttackBase(L73-525;模板基)。IObservesVariables/INotifyCreated 由
/// Derived 承载(ConditionalTrait 协议同 Armament 形)
/// AttackBase (L73-525; the template base). IObservesVariables/
/// INotifyCreated ride the Derived class (the ConditionalTrait protocol in
/// Armament's shape).
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

  // ———— [VerifySync] L78-79 ————
  bool b_is_aiming = false;  // IsAiming

  /// AttackBaseFace.IsAiming | AttackBaseFace.IsAiming.
  bool IsAiming() const override { return b_is_aiming; }

  /// AttackBaseFace.IsTraitPaused(条件核转发)
  /// AttackBaseFace.IsTraitPaused (the condition-core forwarding).
  bool IsTraitPaused() const override {
    return sim::ConditionalTraitCore<Derived>::IsTraitPaused();
  }

  void SetIsAiming(bool value) override { b_is_aiming = value; }
  bool AttackRequiresEnteringCell() const override {
    return info_.b_attack_requires_entering_cell;
  }

  /// L81:Armaments(过滤物化;见头注) | L81: Armaments (the filtered
  /// materialization; see the header).
  std::span<Armament* const> Armaments() const {
    return vec_armaments_;
  }

  /// L230-231:GetAttackActivity(纯虚)| L230-231: GetAttackActivity
  /// (pure virtual).
  virtual sim::Activity* GetAttackActivity(
      Actor& self, AttackSource source, const sim::Target& new_target,
      bool allow_move, bool force_attack,
      std::optional<core::Color> target_line_color = std::nullopt) = 0;

  /// L134-147:TargetInFiringArc | L134-147: TargetInFiringArc.
  bool TargetInFiringArc(Actor& self, const sim::Target& target,
                         WAngle facing_tolerance) override;

  /// L149-165:CanAttack(virtual) | L149-165: CanAttack (virtual).
  virtual bool CanAttack(Actor& self, const sim::Target& target);

  /// L167-174:DoAttack(virtual) | L167-174: DoAttack (virtual).
  virtual void DoAttack(Actor& self, const sim::Target& target);

  /// L176-188:IIssueOrder.Orders | L176-188: IIssueOrder.Orders.
  std::vector<sim::IOrderTargeter*> Orders() override;

  /// L190-196:IIssueOrder.IssueOrder
  net::Order* IssueOrder(Actor& self, sim::IOrderTargeter* order,
                         const sim::Target& target, bool queued) override;

  /// L198-211:IResolveOrder.ResolveOrder
  void ResolveOrder(Actor& self, const net::Order& order) override;

  /// L214-223:OnStopOrder(virtual;Resupply/ReturnToBase 判恒 false ——
  /// 活动未移植,COVERAGE 登记)
  /// L214-223: OnStopOrder (virtual; the Resupply/ReturnToBase checks stay
  /// false — the activities are unported, registered in COVERAGE).
  virtual void OnStopOrder(Actor& self) override;

  /// L225-228:IOrderVoice.VoicePhraseForOrder
  std::string VoicePhraseForOrder(Actor& self,
                                  const net::Order& order) override;

  /// L233-251:HasAnyValidWeapons
  bool HasAnyValidWeapons(
      const sim::Target& t,
      bool check_for_center_targeting_weapons = false,
      bool reloading_is_invalid = false) override;

  /// L253-256:GetTargetPosition(virtual)
  /// L253-256: GetTargetPosition (virtual).
  virtual WPos GetTargetPosition(const WPos& pos,
                                 const sim::Target& target) override;

  /// L258-279:GetMinimumRange | L258-279: GetMinimumRange.
  WDist GetMinimumRange() override;

  /// L281-302:GetMaximumRange | L281-302: GetMaximumRange.
  WDist GetMaximumRange() override;

  /// L304-328:GetMinimumRangeVersusTarget
  WDist GetMinimumRangeVersusTarget(
      const sim::Target& target) override;

  /// L330-362:GetMaximumRangeVersusTarget
  WDist GetMaximumRangeVersusTarget(
      const sim::Target& target) override;

  /// L365-385:ChooseArmamentsForTarget
  std::vector<Armament*> ChooseArmamentsForTarget(
      const sim::Target& t, bool force_attack) override;

  /// L387-398:AttackTarget | L387-398: AttackTarget.
  void AttackTarget(const sim::Target& target, AttackSource source,
                    bool queued, bool allow_move, bool force_attack = false,
                    std::optional<core::Color> target_line_color =
                        std::nullopt) override;

  /// L400:OnResolveAttackOrder(virtual 空体)
  /// L400: OnResolveAttackOrder (the virtual empty body).
  virtual void OnResolveAttackOrder(Actor& self, sim::Activity* activity,
                                    const sim::Target& target, bool queued,
                                    bool force_attack) {
    [[maybe_unused]] auto _ = std::tie(self, activity, target, queued,
                                       force_attack);
  }

  /// L402-406:IsReachableTarget | L402-406: IsReachableTarget.
  bool IsReachableTarget(const sim::Target& target, bool allow_move);

  /// L408-417:UnforcedAttackTargetStances
  sim::PlayerRelationship UnforcedAttackTargetStances() override;

  /// L109-124:ITick.Tick(protected Tick 的接口面)
  void Tick(Actor& self) override;

  /// Created(L98-107;Derived 的 Created 调此 + CoreCreated)
  /// Created (L98-107; the Derived's Created calls this + CoreCreated).
  void CoreAttackBaseCreated(Actor& self);

  const AttackBaseInfoData& Info() const { return info_; }

  /// AttackBaseFace 的 Info 值访问器(ab.Info.TargetFrozenActors 等)
  /// AttackBaseFace's Info value accessors (ab.Info.TargetFrozenActors
  /// and friends).
  bool TargetFrozenActors() const override {
    return info_.b_target_frozen_actors;
  }
  WAngle FacingTolerance() const override {
    return info_.angle_facing_tolerance;
  }

 protected:
  sim::IFacing* facing_ = nullptr;              // L83
  sim::IPositionable* positionable_ = nullptr;  // L84
  std::vector<sim::INotifyAiming*> vec_notify_aiming_;  // L85
  const AttackBaseInfoData info_;

  bool b_was_aiming_ = false;  // L90

 private:
  /// L419-524:AttackOrderTargeter(嵌套;IOrderTargeter)
  class AttackOrderTargeter final : public sim::IOrderTargeter {
   public:
    AttackOrderTargeter(AttackBaseCore& ab, int priority)
        : ab_{ab}, int4_priority_{priority} {}

    std::string OrderID() const override { return str_order_id_; }
    int OrderPriority() const override { return int4_priority_; }
    bool TargetOverridesSelection(
        Actor& /*self*/, const sim::Target& /*target*/,
        std::span<Actor* const> /*actors_at*/, CPos /*xy*/,
        sim::TargetModifiers /*modifiers*/) override {
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

  Actor* p_self_ = nullptr;  // L88
  std::vector<Armament*> vec_armaments_;  // L86 的物化(见头注)
  std::vector<std::unique_ptr<AttackOrderTargeter>>
      vec_order_targeters_;  // Orders() 的载体(生命周期随 trait)
};

/// AttackFrontal.cs L18-48
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

  /// L34-40:CanAttack 覆写(基判定 + TargetInFiringArc)
  /// L34-40: the CanAttack override (the base check + TargetInFiringArc).
  bool CanAttack(Actor& self, const sim::Target& target) override;

  /// L42-46:GetAttackActivity → Activities.Attack
  sim::Activity* GetAttackActivity(
      Actor& self, AttackSource source, const sim::Target& new_target,
      bool allow_move, bool force_attack,
      std::optional<core::Color> target_line_color = std::nullopt) override;

  void TraitEnabledHook(Actor& /*self*/) {}
  void TraitDisabledHook(Actor& /*self*/) {}
  void TraitResumedHook(Actor& /*self*/) {}
  void TraitPausedHook(Actor& /*self*/) {}

 private:
  friend class sim::ConditionalTraitCore<AttackFrontal>;
  friend class AttackBaseCore<AttackFrontal>;
};

}  // namespace ora::mods
