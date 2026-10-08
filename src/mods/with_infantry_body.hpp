// UPSTREAM: OpenRA.Mods.Common/Traits/Render/WithInfantryBody.cs @b6fc03f
//          L21-225 全文
//          The whole of WithInfantryBody.cs L21-225.
//
// 机制对照 / Mechanism mapping:
//  - Game.CosmeticRandom(PlayStandAnimation 的 stand 择一)→
//    world.LocalRandom()(同 conditions 批的 LocalRandom 域形态 —— 非同步
//    消耗不进 SyncHash;偏离面与 GrantConditionOnDamageState 同登记)
//    Game.CosmeticRandom (PlayStandAnimation's stand pick) →
//    world.LocalRandom() (the conditions batch's LocalRandom-domain shape —
//    a non-synced consumption never entering SyncHash; the deviation rides
//    GrantConditionOnDamageState's registry).
//  - RenderPreviewSprites(IActorPreview 面)随 Phase 6 预览装配
//    RenderPreviewSprites (the IActorPreview face) rides Phase 6's preview
//    assembly.
//  - AttackSequences 的 FrozenDictionary → 插入序 vector pair(遍历序 =
//    键查找,无序依赖)
//    AttackSequences' FrozenDictionary → an insertion-ordered pair vector
//    (the traversal = key lookup, no order dependence).
#pragma once
import std;

#include "core/wangle.hpp"
#include "gfx/animation.hpp"
#include "meta/generic_record.hpp"
#include "mods/render_sprites.hpp"
#include "sim/actor.hpp"
#include "sim/conditional_trait.hpp"
#include "sim/trait_interfaces.hpp"

namespace ora::mods {

/// WithInfantryBodyInfo 的解析面(L22-50)
/// The parsed face of WithInfantryBodyInfo (L22-50).
struct WithInfantryBodyInfoData {
  sim::ConditionalTraitData conditional;
  int int4_min_idle_delay = 30;   // L23
  int int4_max_idle_delay = 110;  // L24
  std::string str_move_sequence = "run";   // L27
  std::string str_default_attack_sequence;  // L30
  std::vector<std::pair<std::string, std::vector<std::string>>>
      vec_attack_sequences;                     // L36
  std::vector<std::string> vec_idle_sequences;  // L39
  std::vector<std::string> vec_stand_sequences{"stand"};  // L42
  std::string str_palette;          // L46
  bool b_is_player_palette = false;  // L49

  static WithInfantryBodyInfoData Parse(const meta::RecordObject& rec_info);
};

/// WithInfantryBody(L70-224):步兵动画状态机(五态)
/// WithInfantryBody (L70-224): the infantry animation state machine (the
/// five states).
class WithInfantryBody : public TraitBase,
                         public sim::ConditionalTraitCore<WithInfantryBody>,
                         public sim::IObservesVariables,
                         public sim::INotifyCreated,
                         public sim::ITick,
                         public sim::INotifyAttack,
                         public sim::INotifyIdle {
 public:
  WithInfantryBody(const ActorInitializer& init,
                   const WithInfantryBodyInfoData& info);

  ORA_TRAIT_INTERFACES(
      WithInfantryBody, OpenRA_Mods_Common_Traits_Render_WithInfantryBody,
      sim::IObservesVariables, sim::INotifyCreated, sim::ITick,
      sim::INotifyAttack, sim::INotifyIdle)

  bool IsTraitEnabled() const override {
    return !sim::ConditionalTraitCore<WithInfantryBody>::IsTraitDisabled();
  }
  bool IsTraitDisabled() const override {
    return sim::ConditionalTraitCore<WithInfantryBody>::IsTraitDisabled();
  }

  void Created(Actor& self) override;
  std::vector<sim::VariableObserver> GetVariableObservers() override {
    return CollectObservers();
  }

  const WithInfantryBodyInfoData& Info() const { return info_; }

  /// PlayStandAnimation(L127-137)
  void PlayStandAnimation(Actor& self);

  void PreparingAttack(Actor& self, const sim::Target& target,
                       Armament& armament,
                       const Barrel& barrel) override;
  void Attacking(Actor& self, const sim::Target& target, Armament& armament,
                 const Barrel& barrel) override {}

  void Tick(Actor& self) override;
  void TickIdle(Actor& self) override;

  void TraitEnabledHook(Actor&) {}
  void TraitDisabledHook(Actor&) {}
  void TraitResumedHook(Actor&) {}
  void TraitPausedHook(Actor&) {}

  gfx::Animation& DefaultAnimation() { return *up_default_animation_; }

  /// AnimationState(L217-224)
  /// AnimationState (L217-224).
  enum class AnimationState {
    Idle = 0,
    Attacking = 1,
    Moving = 2,
    Waiting = 3,
    IdleAnimating = 4,
  };

 protected:
  /// GetDisplayInfo(L85-88)/NormalizeInfantrySequence(L112-120)/
  /// AllowIdleAnimation(L122-125)/Attacking(L139-160)/Tick(L176-196)的
  /// 虚钩族(上游 protected virtual)
  /// The protected-virtual hook family (L85-88/112-120/122-125/139-160/
  /// 176-196).
  virtual const WithInfantryBodyInfoData& GetDisplayInfo() const {
    return info_;
  }
  virtual std::string NormalizeInfantrySequence(
      Actor& self, std::string_view str_base_sequence);
  virtual bool AllowIdleAnimation(Actor& self);
  virtual void AttackingInner(Actor& self, Armament& armament,
                              const Barrel* ptr_barrel);
  virtual void TickInner(Actor& self);

  WithInfantryBodyInfoData info_;
  std::unique_ptr<gfx::Animation> up_default_animation_;
  std::optional<gfx::AnimationWithOffset> awo_default_;
  sim::IMove* ptr_move_ = nullptr;
  sim::IRenderInfantrySequenceModifier* ptr_rsm_ = nullptr;

  bool b_dirty_ = false;
  std::string str_idle_sequence_;
  int int4_idle_delay_ = 0;
  AnimationState state_animation_ = AnimationState::Waiting;
  bool b_was_modifying_ = false;

 private:
  bool IsModifyingSequence() const {
    return ptr_rsm_ != nullptr && ptr_rsm_->IsModifyingSequence();
  }
};

}  // namespace ora::mods
