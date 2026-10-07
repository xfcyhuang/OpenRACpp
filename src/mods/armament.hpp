// UPSTREAM: OpenRA.Mods.Common/Traits/Armament.cs @b6fc03f L22-431 全文
//          (逐语义重写)
//          The whole of Armament.cs L22-431 (verbatim-semantics rewrite).
//
// 机制对照 / Mechanism mapping:
//  - PausableConditionalTrait<ArmamentInfo> → sim::ConditionalTraitCore<
//    Armament> 组合(同 Mobile 头注)
//    PausableConditionalTrait<ArmamentInfo> → the
//    sim::ConditionalTraitCore<Armament> composition (see Mobile's header).
//  - OPT-A9 开火链零分配:上游每发 CheckFire 的 LINQ ToArray
//    (rangeModifiers/damageModifiers/inaccuracyModifiers)→ Created 一次性
//    解析 trait 指针 + 每发回填的成员定长缓冲(span 直传);
//    delayedActions 的闭包 Action<int> → 成员函数 + 载荷,零闭包分配
//    OPT-A9's zero-allocation firing chain: upstream's per-shot LINQ
//    ToArray (rangeModifiers/damageModifiers/inaccuracyModifiers) → the
//    trait pointers resolved once at Created + member fixed buffers
//    refilled per shot (span-passed); delayedActions' Action<int> closures
//    → member functions + payloads, zero closure allocation.
//  - ArmamentInfo.RulesetLoaded 的武器解析/校验(L92-110)→ 工厂解析面
//    (时点差异同 Health D 系;异常文本逐字;ModifiedRange 的
//    IRangeModifierInfo 集空 = 无修正上游等价)
//    ArmamentInfo.RulesetLoaded's weapon resolution/validation (L92-110) →
//    the factory-parse face (the timing difference like Health's D series;
//    exception texts verbatim; ModifiedRange's empty IRangeModifierInfo
//    set = upstream's no-modifier equivalent).
//  - Turreted/Hovers trait 面未移植:查询空集的上游等价分支(turret ==
//    null → 体朝向;hovers == null → 无悬浮偏移;COVERAGE 登记)
//    The Turreted/Hovers trait faces unported: the empty-query upstream-
//    equivalent branches (turret == null → the body orientation;
//    hovers == null → no hover offset; registered in COVERAGE).
//  - Game.Sound.Play:声音门面注入面(Deps;Phase 6 接线前 = 空实现的
//    上游不可观测面 —— 上游依赖 Sound 系统已初始化)
//    Game.Sound.Play: the sound-facade injection face (Deps; before the
//    Phase 6 wiring = the upstream-unobservable no-op face — upstream
//    presumes the Sound system initialized).
#pragma once
import std;

#include "core/wdist.hpp"
#include "game/game_records.hpp"
#include "sim/actor.hpp"
#include "sim/conditional_trait.hpp"
#include "sim/trait_interfaces.hpp"
#include "sim/weapons.hpp"

namespace ora::mods {

using sim::Actor;
using sim::ActorInitializer;
using sim::TraitBase;

/// Barrel(Armament.cs L22-26)
struct Barrel {
  WVec Offset;
  WAngle Yaw;
};

/// ArmamentInfo 的解析面(L28-111)
/// The parsed face of ArmamentInfo (L28-111).
struct ArmamentInfoData {
  std::string str_name{"primary"};            // L31
  std::string str_weapon;                     // L36 [FieldLoader.Require]
  std::string str_turret{"primary"};          // L39
  int int4_fire_delay = 0;                    // L42
  std::vector<WVec> vec_local_offset{};       // L46
  std::vector<WAngle> vec_local_yaw{};        // L49
  WDist dist_recoil{0};                       // L52
  WDist dist_recoil_recovery{9};              // L55
  std::string str_muzzle_sequence;            // L59
  std::string str_muzzle_palette{"effect"};   // L62
  std::string str_reloading_condition;        // L67
  sim::PlayerRelationship target_relationships =
      sim::PlayerRelationship::Enemy;         // L72
  sim::PlayerRelationship force_target_relationships =
      sim::PlayerRelationship::Enemy | sim::PlayerRelationship::Neutral |
      sim::PlayerRelationship::Ally;          // L73
  std::string str_cursor{"attack"};           // L80
  std::string str_outside_range_cursor{"attackoutsiderange"};  // L85
  int int4_ammo_usage = 1;                    // L88

  // RulesetLoaded 解析面(L92-110)—— 工厂时点
  // The RulesetLoaded resolution face (L92-110) — at factory time.
  const game::WeaponInfo* weapon_info = nullptr;
  WDist dist_modified_range{0};               // ModifiedRange
  sim::ConditionalTraitData conditional;

  static ArmamentInfoData Parse(const meta::RecordObject& rec_info,
                                const sim::World& world);
};

/// Armament(L113-431)
class Armament final : public TraitBase,
                       public sim::ConditionalTraitCore<Armament>,
                       public sim::IObservesVariables,
                       public sim::INotifyCreated,
                       public sim::ITick {
 public:
  Armament(ActorInitializer& init, const ArmamentInfoData& info);  // L141-165

  ORA_TRAIT_INTERFACES(Armament, OpenRA_Mods_Common_Traits_Armament,
                       sim::IObservesVariables, sim::INotifyCreated,
                       sim::ITick)

  bool IsTraitEnabled() const override {
    return !ConditionalTraitCore<Armament>::IsTraitDisabled();
  }
  bool IsTraitDisabled() const override {
    return ConditionalTraitCore<Armament>::IsTraitDisabled();
  }

  const game::WeaponInfo* Weapon = nullptr;   // L115

  /// Info 值面(上游 a.Info.Name / TargetRelationships / Cursor 族消费)
  /// The Info value face (upstream's a.Info.Name / TargetRelationships /
  /// Cursor-family consumers).
  const ArmamentInfoData& InfoData() const { return info_; }
  std::vector<Barrel> vec_barrels;            // L116 Barrels
  WDist Recoil;                               // L137
  int FireDelay = 0;                          // L138
  int Burst = 0;                              // L139
  Actor* ActorPtr = nullptr;                  // L430 Actor

  /// MaxRange(L167-170;rangeModifiers 每次求值 —— OPT-A9 成员缓冲)
  /// MaxRange (L167-170; rangeModifiers evaluated per call — OPT-A9's
  /// member buffer).
  WDist MaxRange();

  void Created(Actor& self) override;          // L172-186
  /// ConditionalTrait.GetVariableObservers(L57-63:base 序)
  /// ConditionalTrait.GetVariableObservers (L57-63: the base order).
  std::vector<sim::VariableObserver> GetVariableObservers() override {
    return CollectObservers();
  }
  void Tick(Actor& self) override;             // L211-242(protected Tick)
  bool CheckFire(Actor* self, sim::IFacing* facing,
                 const sim::Target& target);   // L272-294
  bool IsReloading() const {                   // L394
    return FireDelay > 0 || IsTraitDisabled();
  }
  WVec MuzzleOffset(Actor* self, const Barrel& b);        // L396-399
  WRot MuzzleOrientation(Actor* self, const Barrel& b);   // L420-423

  void TraitEnabledHook(Actor& /*self*/) {}
  void TraitDisabledHook(Actor& /*self*/) {}
  void TraitResumedHook(Actor& /*self*/) {}
  void TraitPausedHook(Actor& /*self*/) {}

 private:
  friend class sim::ConditionalTraitCore<Armament>;

  /// delayedActions 条目(L135 的 (Ticks, Burst, Action<int>);两种闭包体
  /// → kind 判别式,零闭包分配 —— OPT-A9)
  /// A delayedActions entry (L135's (Ticks, Burst, Action<int>); the two
  /// closure bodies → the kind discriminator, zero closure allocation —
  /// OPT-A9).
  struct DelayedAction {
    enum class Kind { FireProjectile, PlayAfterFireSound } kind;
    int ticks;
    int burst;
    sim::ProjectileArgs args;       // FireProjectile 载荷 | the payload
    const Barrel* barrel = nullptr; // FireProjectile 载荷 | the payload
    sim::Target delayed_target_copy;  // 上游闭包的 target 副本 | upstream's
                                      // closure's target copy
  };

  void UpdateCondition(Actor& self);           // L198-209
  void ScheduleDelayedAction(int t, int b,
                             DelayedAction action);  // L244-250
  /// 延迟动作执行体(Tick 内的 x.Func(x.Burst))
  /// The delayed-action execution body (Tick's x.Func(x.Burst)).
  void RunDelayedAction(Actor& self, DelayedAction& action);
  bool CanFire(Actor* self, const sim::Target& target);  // L252-268
  void FireBarrel(Actor* self, sim::IFacing* facing,
                  const sim::Target& target,
                  const Barrel& barrel);       // L296-363
  void UpdateBurst(Actor* self, const sim::Target& target);  // L365-392
  WVec CalculateMuzzleOffset(Actor* self, const Barrel& b);  // L401-418
  WRot CalculateMuzzleOrientation(Actor* self,
                                  const Barrel& b);           // L425-428

  ArmamentInfoData info_;                      // 基类的 Info 面
  Actor* p_self_ = nullptr;                    // L144 Actor = self

  // Turreted/Hovers 查询面(空集恒 null;COVERAGE 登记)
  // The Turreted/Hovers query faces (the empty set stays null; registered
  // in COVERAGE).
  void* p_turret_ = nullptr;
  void* p_hovers_ = nullptr;
  class BodyOrientation* coords_ = nullptr;
  std::vector<sim::INotifyBurstComplete*> vec_notify_burst_complete_;
  std::vector<std::pair<Actor*, sim::INotifyAttack*>> vec_notify_attacks_;

  int int4_condition_token_ = Actor::InvalidConditionToken;  // L124

  // ———— OPT-A9:一次性解析 trait 指针 + 每发回填的成员缓冲 ————
  // ———— OPT-A9: the trait pointers resolved once + the member buffers
  //      refilled per shot ————
  std::vector<sim::IRangeModifier*> vec_range_trait_ptrs_;
  std::vector<sim::IReloadModifier*> vec_reload_trait_ptrs_;
  std::vector<sim::IFirepowerModifier*> vec_damage_trait_ptrs_;
  std::vector<sim::IInaccuracyModifier*> vec_inaccuracy_trait_ptrs_;

  int int4_ticks_since_last_shot_ = 0;         // L131
  int int4_current_barrel_ = 0;                // L132
  const int int4_barrel_count_;                // L133

  std::vector<DelayedAction> vec_delayed_actions_;  // L135
};

}  // namespace ora::mods
