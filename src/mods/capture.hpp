// UPSTREAM: OpenRA.Mods.Common/Traits/CaptureManager.cs @b6fc03f
//          L21-293 全文 + Capturable.cs L15-50 全文 + Captures.cs L15-173
//          全文 + GivesCashOnCapture.cs L14-62 全文 +
//          Activities/Enter.cs L11-163 全文 + Activities/CaptureActor.cs
//          L11-135 全文
//          The whole of CaptureManager.cs L21-293 + Capturable.cs L15-50 +
//          Captures.cs L15-173 + GivesCashOnCapture.cs L14-62 +
//          Activities/Enter.cs L11-163 + Activities/CaptureActor.cs
//          L11-135.
//
// 机制对照 / Mechanism mapping:
//  - CaptureProgressBar/CapturableProgressBar/CapturableProgressBlink =
//    ICaptureProgressWatcher 的 UI 面(Phase 6);TransformOnCapture 依赖
//    Transform 活动系统(未移植)—— 两族随宿主批
//    CaptureProgressBar/CapturableProgressBar/CapturableProgressBlink are
//    ICaptureProgressWatcher's UI faces (Phase 6); TransformOnCapture
//    depends on the unported Transform system — both families ride their
//    host batches.
//  - Captures 的 CaptureOrderTargeter/voice/cursor/TargetLines = 输入批
//    面(Phase 6);ResolveOrder 的 CaptureActor 活动链全量
//    Captures' CaptureOrderTargeter/voice/cursor/TargetLines are the
//    input-batch faces (Phase 6); ResolveOrder's CaptureActor activity
//    chain is kept in full.
//  - GivesCashOnCapture 的 FloatingText 面省略(Phase 6);ChangeCash 记账
//    全量
//    GivesCashOnCapture's FloatingText face is omitted (Phase 6); the
//    ChangeCash bookkeeping is kept in full.
//  - Enter 的 TargetLineNodes = Phase 6;MoveCooldownHelper/让行/入内
//    状态机全量
//    Enter's TargetLineNodes ride Phase 6; the MoveCooldownHelper/yield/
//    enter state machine is kept in full.
#pragma once
import std;

#include "core/bitset.hpp"
#include "core/color.hpp"
#include "meta/generic_record.hpp"
#include "net/order.hpp"
#include "sim/actor.hpp"
#include "sim/conditional_trait.hpp"
#include "sim/sync_hash.hpp"
#include "sim/trait_interfaces.hpp"
#include "mods/move_activities.hpp"
#include "sim/target.hpp"

namespace ora::mods {

class CaptureManager;
class Capturable;
class Captures;
class Mobile;

/// CaptureManagerInfo(L39-53)的解析面
/// The parsed face of CaptureManagerInfo (L39-53).
struct CaptureManagerInfoData {
  std::string str_capturing_condition;    // L46
  std::string str_being_captured_condition;  // L49
  bool b_prevents_auto_target = true;     // L52

  static CaptureManagerInfoData Parse(const meta::RecordObject& rec);
};

/// CaptureManager(L58-293)
class CaptureManager final : public sim::TraitBase,
                             public sim::INotifyCreated,
                             public sim::INotifyCapture,
                             public sim::ITick,
                             public sim::IDisableEnemyAutoTarget {
 public:
  ORA_TRAIT_INTERFACES(CaptureManager,
                       OpenRA_Mods_Common_Traits_CaptureManager,
                       sim::INotifyCreated, sim::INotifyCapture, sim::ITick,
                       sim::IDisableEnemyAutoTarget)

  CaptureManager(sim::ActorInitializer& init, CaptureManagerInfoData info);
  ~CaptureManager() override;

  void Created(sim::Actor& self) override;                     // L107-118
  void OnCapture(sim::Actor& self, sim::Actor& captor,         // L158-162
                 sim::Player& old_owner, sim::Player& new_owner,
                 const core::BitSet<sim::CaptureType>& capture_types) override;
  void Tick(sim::Actor& self) override;                        // L251-264
  bool DisableEnemyAutoTarget(sim::Actor& self,                // L266-268
                              sim::Actor& attacker) override;

  /// L120-121:RefreshCapturable(Capturable 使能钩)
  /// L120-121: RefreshCapturable (the Capturable enable hook).
  void RefreshCapturable();

  /// L123-124:RefreshCaptures(Captures 使能钩)
  /// L123-124: RefreshCaptures (the Captures enable hook).
  void RefreshCaptures();

  /// L126-147:CanTarget(CaptureManager)
  bool CanTarget(const CaptureManager& target);

  /// L149-157:CanTarget(FrozenActor)—— FrozenActorInfo 无条件缓存的
  /// 上游语义(Capturable 恒启用假设)
  /// L149-157: CanTarget (FrozenActor) — upstream's condition-less
  /// FrozenActorInfo semantics (the all-Capturable-enabled assumption).
  bool CanTargetFrozen(const sim::FrozenActor& target);

  /// L170-188:ValidCapturesWithLowestSabotageThreshold(SabotageThreshold
  /// 升序 → CaptureDelay 升序的稳定择优)
  /// L170-188: ValidCapturesWithLowestSabotageThreshold (the
  /// SabotageThreshold-ascending then CaptureDelay-ascending stable pick).
  class Captures* ValidCapturesWithLowestSabotageThreshold(
      const CaptureManager& target);

  /// L198-249:StartCapture(CaptureActor 的进入前闸)
  /// L198-249: StartCapture (CaptureActor's pre-enter gate).
  bool StartCapture(CaptureManager& target_manager,
                    Captures*& captures_out);

  /// L213+ 的 Captures 友面(延迟 token)。
  void CancelCapture(sim::Actor* target, CaptureManager* target_manager);  // L213-249

  bool BeingCaptured = false;  // L101(帧末复位面)

  sim::Actor* Self() const { return ptr_self_; }
  const CaptureManagerInfoData& InfoData() const { return info_; }

 private:
  /// L126-147:CanTarget(Player, captureTypes)(关系三向判)
  /// L126-147: CanTarget (Player, captureTypes) (the three-way
  /// relationship test).
  bool CanTargetInternal(const sim::Player& target,
                         const core::BitSet<sim::CaptureType>& capture_types);

  sim::Actor* ptr_self_ = nullptr;
  CaptureManagerInfoData info_;
  sim::IMove* ptr_move_ = nullptr;
  std::vector<sim::ICaptureProgressWatcher*> vec_progress_watchers_;

  core::BitSet<sim::CaptureType> bitset_ally_captures_types_;
  core::BitSet<sim::CaptureType> bitset_neutral_captures_types_;
  core::BitSet<sim::CaptureType> bitset_enemy_captures_types_;
  core::BitSet<sim::CaptureType> bitset_capturable_types_;

  std::vector<Capturable*> vec_enabled_capturable_;
  std::vector<Captures*> vec_enabled_captures_;

  // 一次捕获进行中的状态 | the in-progress capture state.
  sim::Actor* ptr_current_target_ = nullptr;
  CaptureManager* ptr_current_target_manager_ = nullptr;
  int int4_current_target_delay_ = 0;
  int int4_current_target_total_ = 0;
  int int4_capturing_token_ = sim::Actor::InvalidConditionToken;
  int int4_being_captured_token_ = sim::Actor::InvalidConditionToken;
  bool b_entering_current_target_ = false;

  std::vector<sim::Actor*> vec_current_captors_;
};

/// CapturableInfo(L19-35)的解析面
/// The parsed face of CapturableInfo (L19-35).
struct CapturableInfoData {
  core::BitSet<sim::CaptureType> bitset_types;  // L24
  bool b_cancel_activity = false;          // L27
  sim::ConditionalTraitData conditional;

  static CapturableInfoData Parse(const meta::RecordObject& rec);
};

/// Capturable(L38-50)
class Capturable final : public sim::TraitBase,
                         public sim::IObservesVariables,
                         public sim::INotifyCreated,
                         public sim::INotifyCapture,
                         private sim::ConditionalTraitCore<Capturable> {
 public:
  ORA_TRAIT_INTERFACES(Capturable, OpenRA_Mods_Common_Traits_Capturable,
                       sim::IObservesVariables, sim::INotifyCreated,
                       sim::INotifyCapture)

  Capturable(sim::ActorInitializer& init, CapturableInfoData info);
  ~Capturable() override;

  bool IsTraitEnabled() const override { return !ConditionalTraitCore::IsTraitDisabled(); }
  bool IsTraitDisabled() const override { return ConditionalTraitCore::IsTraitDisabled(); }

  void Created(sim::Actor& self) override { ConditionalTraitCore::CoreCreated(self); }
  std::vector<sim::VariableObserver> GetVariableObservers() override {
    return CollectObservers();
  }
  void OnCapture(sim::Actor& self, sim::Actor& captor,  // L44-47
                 sim::Player& old_owner, sim::Player& new_owner,
                 const core::BitSet<sim::CaptureType>& capture_types) override;

  const CapturableInfoData& InfoData() const { return info_; }

 private:
  friend class sim::ConditionalTraitCore<Capturable>;
  void TraitEnabledHook(sim::Actor& self);   // L49
  void TraitDisabledHook(sim::Actor& self);  // L49

  CapturableInfoData info_;
  CaptureManager* ptr_capture_manager_ = nullptr;
};

/// CapturesInfo(L20-77)的解析面
/// The parsed face of CapturesInfo (L20-77).
struct CapturesInfoData {
  core::BitSet<sim::CaptureType> bitset_capture_types;  // L24
  int int4_sabotage_threshold = 0;                 // L28
  int int4_sabotage_hp_removal = 50;               // L32
  core::BitSet<sim::DamageType> bitset_sabotage_damage_types;  // L35
  int int4_capture_delay = 0;                      // L39
  bool b_consumed_by_capture = true;               // L42
  int int4_player_experience = 0;                  // L46
  sim::PlayerRelationship valid_relationships =    // L49
      sim::PlayerRelationship::Neutral | sim::PlayerRelationship::Enemy;
  sim::PlayerRelationship player_experience_relationships =  // L52
      sim::PlayerRelationship::Enemy;
  core::Color color_target_line;                   // L71
  sim::ConditionalTraitData conditional;

  static CapturesInfoData Parse(const meta::RecordObject& rec);
};

/// Captures(L80-173)
class Captures final : public sim::TraitBase,
                       public sim::IObservesVariables,
                       public sim::INotifyCreated,
                       public sim::IResolveOrder,
                       private sim::ConditionalTraitCore<Captures> {
 public:
  ORA_TRAIT_INTERFACES(Captures, OpenRA_Mods_Common_Traits_Captures,
                       sim::IObservesVariables, sim::INotifyCreated,
                       sim::IResolveOrder)

  Captures(sim::ActorInitializer& init, CapturesInfoData info);
  ~Captures() override;

  bool IsTraitEnabled() const override { return !ConditionalTraitCore::IsTraitDisabled(); }
  bool IsTraitDisabled() const override { return ConditionalTraitCore::IsTraitDisabled(); }

  void Created(sim::Actor& self) override { ConditionalTraitCore::CoreCreated(self); }
  std::vector<sim::VariableObserver> GetVariableObservers() override {
    return CollectObservers();
  }
  void ResolveOrder(sim::Actor& self,  // L143-151
                    const net::Order& order) override;

  const CapturesInfoData& InfoData() const { return info_; }
  CaptureManager* Manager() const { return ptr_capture_manager_; }

 private:
  friend class sim::ConditionalTraitCore<Captures>;
  void TraitEnabledHook(sim::Actor& self);   // L153
  void TraitDisabledHook(sim::Actor& self);  // L154

  CapturesInfoData info_;
  CaptureManager* ptr_capture_manager_ = nullptr;
};

/// GivesCashOnCaptureInfo(L16-30)的解析面
/// The parsed face of GivesCashOnCaptureInfo (L16-30).
struct GivesCashOnCaptureInfoData {
  int int4_amount = 0;                          // L19
  bool b_show_ticks = true;                     // L22
  core::BitSet<sim::CaptureType> bitset_capture_types;  // L25
  int int4_display_duration = 30;               // L28

  static GivesCashOnCaptureInfoData Parse(const meta::RecordObject& rec);
};

/// GivesCashOnCapture(L32-62)
class GivesCashOnCapture final : public sim::TraitBase,
                                 public sim::IObservesVariables,
                                 public sim::INotifyCreated,
                                 public sim::INotifyCapture,
                                 private sim::ConditionalTraitCore<GivesCashOnCapture> {
 public:
  ORA_TRAIT_INTERFACES(GivesCashOnCapture,
                       OpenRA_Mods_Common_Traits_GivesCashOnCapture,
                       sim::IObservesVariables, sim::INotifyCreated,
                       sim::INotifyCapture)

  GivesCashOnCapture(GivesCashOnCaptureInfoData info,
                     sim::ConditionalTraitData conditional);
  ~GivesCashOnCapture() override;

  bool IsTraitEnabled() const override { return !ConditionalTraitCore::IsTraitDisabled(); }
  bool IsTraitDisabled() const override { return ConditionalTraitCore::IsTraitDisabled(); }

  void Created(sim::Actor& self) override { ConditionalTraitCore::CoreCreated(self); }
  std::vector<sim::VariableObserver> GetVariableObservers() override {
    return CollectObservers();
  }
  void OnCapture(sim::Actor& self, sim::Actor& captor,  // L40-58
                 sim::Player& old_owner, sim::Player& new_owner,
                 const core::BitSet<sim::CaptureType>& capture_types) override;

 private:
  friend class sim::ConditionalTraitCore<GivesCashOnCapture>;
  GivesCashOnCaptureInfoData info_;
};

/// Enter.cs L16:EnterBehaviour
enum class EnterBehaviour : std::int32_t {
  Exit = 0,
  Suicide = 1,
  Dispose = 2,
};

/// Enter(Enter.cs L19-163;抽象活动基类 —— 接近/等待/进入/离场四态)
/// Enter (Enter.cs L19-163; the abstract activity base — the
/// approach/wait/enter/exit four states).
class Enter : public sim::Activity {
 public:
  /// targetLineColor 的可选面 | the optional targetLineColor face.
  Enter(sim::Actor& self, const sim::Target& target,
        std::optional<core::Color> target_line_color);

  ~Enter() override;

  bool Tick(sim::Actor& self) override;  // L52-158

 protected:
  /// L44:TickInner(子类状态更新面)
  /// L44: TickInner (the subclass state-update face).
  virtual void TickInner(sim::Actor& self, const sim::Target& target,
                         bool b_target_is_dead_or_hidden_actor) {
    (void)self;
    (void)target;
    (void)b_target_is_dead_or_hidden_actor;
  }

  /// L51:TryStartEnter(false = 等待)
  /// L51: TryStartEnter (false = wait).
  virtual bool TryStartEnter(sim::Actor& self, sim::Actor& target_actor) {
    (void)self;
    (void)target_actor;
    return true;
  }

  /// L57:OnEnterComplete
  virtual void OnEnterComplete(sim::Actor& self, sim::Actor& target_actor) {
    (void)self;
    (void)target_actor;
  }

 private:
  enum class EnterState { Approaching, Entering, Exiting, Finished };

  sim::IMove* ptr_move_ = nullptr;
  Mobile* ptr_mobile_ = nullptr;  // MoveToTarget 具体面(上游 IMove 的
                                   // Mobile 实现;Enter 族仅 Mobile 挂载)
                                   // the MoveToTarget concrete face
                                   // (upstream's IMove implementor; the
                                   // Enter family mounts on Mobile only).
  std::optional<core::Color> opt_target_line_color_;
  activities::MoveCooldownHelper move_cooldown_helper_;
  sim::Target target_;
  sim::Target target_last_visible_;
  bool b_use_last_visible_target_ = false;
  EnterState state_last_ = EnterState::Approaching;
};

/// CaptureActor(CaptureActor.cs L17-135)
class CaptureActor final : public Enter {
 public:
  CaptureActor(sim::Actor& self, const sim::Target& target,
               std::optional<core::Color> target_line_color);

 protected:
  void TickInner(sim::Actor& self, const sim::Target& target,  // L34-46
                 bool b_target_is_dead_or_hidden_actor) override;
  bool TryStartEnter(sim::Actor& self,                         // L48-78
                     sim::Actor& target_actor) override;
  void OnEnterComplete(sim::Actor& self,                       // L80-104
                       sim::Actor& target_actor) override;
  void OnLastRun(sim::Actor& self) override;                   // L106-110
  void OnActorDispose(sim::Actor& self) override;              // L112-116
  void Cancel(sim::Actor& self, bool keep_queue = false) override;  // L118-123

 private:
  /// L125-133:DoCapture(帧末任务:破坏/换主/通知/经验/消耗)
  /// L125-133: DoCapture (the frame-end task: sabotage/owner change/
  /// notifications/experience/consumption).
  void DoCapture(sim::Actor& self, Captures& captures);
  void CancelCapture() {
    ptr_manager_->CancelCapture(ptr_enter_actor_, ptr_enter_capture_manager_);
  }

  CaptureManager* ptr_manager_ = nullptr;
  sim::Actor* ptr_enter_actor_ = nullptr;
  CaptureManager* ptr_enter_capture_manager_ = nullptr;
};

}  // namespace ora::mods
