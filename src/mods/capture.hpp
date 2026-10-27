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

struct CaptureManagerInfoData {
  std::string str_capturing_condition;
  std::string str_being_captured_condition;
  bool b_prevents_auto_target = true;

  static CaptureManagerInfoData Parse(const meta::RecordObject& rec);
};

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

  void Created(sim::Actor& self) override;
  void OnCapture(sim::Actor& self, sim::Actor& captor,
                 sim::Player& old_owner, sim::Player& new_owner,
                 const core::BitSet<sim::CaptureType>& capture_types) override;
  void Tick(sim::Actor& self) override;
  bool DisableEnemyAutoTarget(sim::Actor& self,
                              sim::Actor& attacker) override;

  void RefreshCapturable();

  void RefreshCaptures();

  bool CanTarget(const CaptureManager& target);

  bool CanTargetFrozen(const sim::FrozenActor& target);

  class Captures* ValidCapturesWithLowestSabotageThreshold(
      const CaptureManager& target);

  bool StartCapture(CaptureManager& target_manager,
                    Captures*& captures_out);

  void CancelCapture(sim::Actor* target, CaptureManager* target_manager);

  bool BeingCaptured = false;

  sim::Actor* Self() const { return ptr_self_; }
  const CaptureManagerInfoData& InfoData() const { return info_; }

 private:

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

  sim::Actor* ptr_current_target_ = nullptr;
  CaptureManager* ptr_current_target_manager_ = nullptr;
  int int4_current_target_delay_ = 0;
  int int4_current_target_total_ = 0;
  int int4_capturing_token_ = sim::Actor::InvalidConditionToken;
  int int4_being_captured_token_ = sim::Actor::InvalidConditionToken;
  bool b_entering_current_target_ = false;

  std::vector<sim::Actor*> vec_current_captors_;
};

struct CapturableInfoData {
  core::BitSet<sim::CaptureType> bitset_types;
  bool b_cancel_activity = false;
  sim::ConditionalTraitData conditional;

  static CapturableInfoData Parse(const meta::RecordObject& rec);
};

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
  void OnCapture(sim::Actor& self, sim::Actor& captor,
                 sim::Player& old_owner, sim::Player& new_owner,
                 const core::BitSet<sim::CaptureType>& capture_types) override;

  const CapturableInfoData& InfoData() const { return info_; }

 private:
  friend class sim::ConditionalTraitCore<Capturable>;
  void TraitEnabledHook(sim::Actor& self);
  void TraitDisabledHook(sim::Actor& self);

  CapturableInfoData info_;
  CaptureManager* ptr_capture_manager_ = nullptr;
};

struct CapturesInfoData {
  core::BitSet<sim::CaptureType> bitset_capture_types;
  int int4_sabotage_threshold = 0;
  int int4_sabotage_hp_removal = 50;
  core::BitSet<sim::DamageType> bitset_sabotage_damage_types;
  int int4_capture_delay = 0;
  bool b_consumed_by_capture = true;
  int int4_player_experience = 0;
  sim::PlayerRelationship valid_relationships =
      sim::PlayerRelationship::Neutral | sim::PlayerRelationship::Enemy;
  sim::PlayerRelationship player_experience_relationships =
      sim::PlayerRelationship::Enemy;
  core::Color color_target_line;
  sim::ConditionalTraitData conditional;

  static CapturesInfoData Parse(const meta::RecordObject& rec);
};

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
  void ResolveOrder(sim::Actor& self,
                    const net::Order& order) override;

  const CapturesInfoData& InfoData() const { return info_; }
  CaptureManager* Manager() const { return ptr_capture_manager_; }

 private:
  friend class sim::ConditionalTraitCore<Captures>;
  void TraitEnabledHook(sim::Actor& self);
  void TraitDisabledHook(sim::Actor& self);

  CapturesInfoData info_;
  CaptureManager* ptr_capture_manager_ = nullptr;
};

struct GivesCashOnCaptureInfoData {
  int int4_amount = 0;
  bool b_show_ticks = true;
  core::BitSet<sim::CaptureType> bitset_capture_types;
  int int4_display_duration = 30;

  static GivesCashOnCaptureInfoData Parse(const meta::RecordObject& rec);
};

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
  void OnCapture(sim::Actor& self, sim::Actor& captor,
                 sim::Player& old_owner, sim::Player& new_owner,
                 const core::BitSet<sim::CaptureType>& capture_types) override;

 private:
  friend class sim::ConditionalTraitCore<GivesCashOnCapture>;
  GivesCashOnCaptureInfoData info_;
};

enum class EnterBehaviour : std::int32_t {
  Exit = 0,
  Suicide = 1,
  Dispose = 2,
};

class Enter : public sim::Activity {
 public:

  Enter(sim::Actor& self, const sim::Target& target,
        std::optional<core::Color> target_line_color);

  ~Enter() override;

  bool Tick(sim::Actor& self) override;

 protected:

  virtual void TickInner(sim::Actor& self, const sim::Target& target,
                         bool b_target_is_dead_or_hidden_actor) {
    (void)self;
    (void)target;
    (void)b_target_is_dead_or_hidden_actor;
  }

  virtual bool TryStartEnter(sim::Actor& self, sim::Actor& target_actor) {
    (void)self;
    (void)target_actor;
    return true;
  }

  virtual void OnEnterComplete(sim::Actor& self, sim::Actor& target_actor) {
    (void)self;
    (void)target_actor;
  }

 private:
  enum class EnterState { Approaching, Entering, Exiting, Finished };

  sim::IMove* ptr_move_ = nullptr;
  Mobile* ptr_mobile_ = nullptr;

  std::optional<core::Color> opt_target_line_color_;
  activities::MoveCooldownHelper move_cooldown_helper_;
  sim::Target target_;
  sim::Target target_last_visible_;
  bool b_use_last_visible_target_ = false;
  EnterState state_last_ = EnterState::Approaching;
};

class CaptureActor final : public Enter {
 public:
  CaptureActor(sim::Actor& self, const sim::Target& target,
               std::optional<core::Color> target_line_color);

 protected:
  void TickInner(sim::Actor& self, const sim::Target& target,
                 bool b_target_is_dead_or_hidden_actor) override;
  bool TryStartEnter(sim::Actor& self,
                     sim::Actor& target_actor) override;
  void OnEnterComplete(sim::Actor& self,
                       sim::Actor& target_actor) override;
  void OnLastRun(sim::Actor& self) override;
  void OnActorDispose(sim::Actor& self) override;
  void Cancel(sim::Actor& self, bool keep_queue = false) override;

 private:

  void DoCapture(sim::Actor& self, Captures& captures);
  void CancelCapture() {
    ptr_manager_->CancelCapture(ptr_enter_actor_, ptr_enter_capture_manager_);
  }

  CaptureManager* ptr_manager_ = nullptr;
  sim::Actor* ptr_enter_actor_ = nullptr;
  CaptureManager* ptr_enter_capture_manager_ = nullptr;
};

}
