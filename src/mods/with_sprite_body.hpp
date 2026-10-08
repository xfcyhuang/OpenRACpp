// UPSTREAM: OpenRA.Mods.Common/Traits/Render/WithSpriteBody.cs @b6fc03f
//          L21-151 全文 + WithFacingSpriteBody.cs L19-39 全文
//          The whole of WithSpriteBody.cs L21-151 + WithFacingSpriteBody.cs
//          L19-39.
//
// 机制对照 / Mechanism mapping:
//  - PausableConditionalTrait<WithSpriteBodyInfo> → CRTP 核 + IObservables
//    (blocks_projectiles.hpp 同形)
//    PausableConditionalTrait<WithSpriteBodyInfo> → the CRTP core +
//    IObservesVariables (blocks_projectiles.hpp's shape).
//  - PlayCustomAnimation 的 after 回调闭包 → std::function 成员链(Animation
//    的 PlayThen after 面直接承载;闭包体逐字)
//    PlayCustomAnimation's after-callback closure → the std::function
//    (Animation's PlayThen-after face carries it; the body verbatim).
//  - RenderPreviewSprites(IActorPreview 面)随 Phase 6 预览装配
//    RenderPreviewSprites (the IActorPreview face) rides Phase 6's preview
//    assembly.
#pragma once
import std;

#include "core/wangle.hpp"
#include "core/wvec.hpp"
#include "gfx/animation.hpp"
#include "meta/generic_record.hpp"
#include "mods/render_sprites.hpp"
#include "sim/actor.hpp"
#include "sim/conditional_trait.hpp"
#include "sim/trait_interfaces.hpp"

namespace ora::gfx {
class WorldRenderer;
}

namespace ora::mods {

/// WithSpriteBodyInfo 的解析面(L22-44;Pausable 条件面并入)
/// The parsed face of WithSpriteBodyInfo (L22-44; the Pausable conditional
/// face folded in).
struct WithSpriteBodyInfoData {
  sim::ConditionalTraitData conditional;
  std::string str_start_sequence;    // L26
  std::string str_sequence = "idle";  // L30
  std::string str_name = "body";      // L34
  bool b_force_to_ground = false;     // L37
  std::string str_palette;            // L40
  bool b_is_player_palette = false;   // L43

  static WithSpriteBodyInfoData Parse(const meta::RecordObject& rec_info);
};

/// WithSpriteBody(L64-151)
class WithSpriteBody : public TraitBase,
                       public sim::ConditionalTraitCore<WithSpriteBody>,
                       public sim::IObservesVariables,
                       public sim::INotifyCreated,
                       public sim::INotifyDamageStateChanged,
                       public sim::IAutoMouseBounds {
 public:
  /// (init, info) 与 (init, info, baseFacing) 两构造的合并形态(上游
  /// protected 重载;子类 WithFacingSpriteBody 传 facing 闭包)
  /// The merged form of the two upstream constructors (the protected
  /// overload; WithFacingSpriteBody passes the facing closure).
  WithSpriteBody(const ActorInitializer& init,
                 const WithSpriteBodyInfoData& info,
                 std::function<WAngle()> fn_base_facing = {});

  ORA_TRAIT_INTERFACES(
      WithSpriteBody, OpenRA_Mods_Common_Traits_Render_WithSpriteBody,
      sim::IObservesVariables, sim::INotifyCreated,
      sim::INotifyDamageStateChanged, sim::IAutoMouseBounds)

  bool IsTraitEnabled() const override {
    return !sim::ConditionalTraitCore<WithSpriteBody>::IsTraitDisabled();
  }
  bool IsTraitDisabled() const override {
    return sim::ConditionalTraitCore<WithSpriteBody>::IsTraitDisabled();
  }

  void Created(Actor& self) override { CoreCreated(self); }
  std::vector<sim::VariableObserver> GetVariableObservers() override {
    return CollectObservers();
  }

  const WithSpriteBodyInfoData& Info() const { return info_; }

  /// NormalizeSequence(L94-97)
  std::string NormalizeSequence(Actor& self, std::string_view str_sequence);

  /// PlayCustomAnimation(L108-115)/PlayCustomAnimationRepeating(L117-120)/
  /// PlayCustomAnimationBackwards(L122-129)/CancelCustomAnimation(L131-134)
  void PlayCustomAnimation(Actor& self, std::string_view str_name,
                           std::function<void()> fn_after = {});
  void PlayCustomAnimationRepeating(Actor& self, std::string_view str_name);
  void PlayCustomAnimationBackwards(Actor& self, std::string_view str_name,
                                    std::function<void()> fn_after = {});
  void CancelCustomAnimation(Actor& self);

  void DamageStateChanged(Actor& self,
                          const sim::AttackInfo& e) override;

  Rectangle AutoMouseoverBounds(Actor& self,
                                gfx::WorldRenderer* wr) override;

  void TraitEnabledHook(Actor& self);
  void TraitDisabledHook(Actor&) {}
  void TraitResumedHook(Actor&) {}
  void TraitPausedHook(Actor&) {}

  gfx::Animation& DefaultAnimation() { return *up_default_animation_; }

 protected:
  /// DamageStateChanged 虚钩(L136-140;子类 WithDeathAnimation 族扩展)
  /// The DamageStateChanged virtual hook (L136-140; the WithDeathAnimation
  /// family extends it).
  virtual void DamageStateChangedInner(Actor& self);

  WithSpriteBodyInfoData info_;
  std::function<WAngle()> fn_base_facing_;
  std::unique_ptr<gfx::Animation> up_default_animation_;
  std::optional<gfx::AnimationWithOffset> awo_default_;
  std::unique_ptr<gfx::Animation> up_bounds_animation_;
};

/// WithFacingSpriteBody(WithFacingSpriteBody.cs L35-39):facing 面的
/// 窄化子类(上游仅换 baseFacing 源)
/// WithFacingSpriteBody (WithFacingSpriteBody.cs L35-39): the narrowed
/// subclass (upstream swaps only the baseFacing source).
class WithFacingSpriteBody final : public WithSpriteBody {
 public:
  WithFacingSpriteBody(const ActorInitializer& init,
                       const WithSpriteBodyInfoData& info);

  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_Render_WithFacingSpriteBody;
  gen::TypeId GetTraitTypeId() const override { return kTypeId; }
  // 子类注册表必须复列基类接口集(上游 GetInterfaces 含继承面 —— 第五批
  // Classic 子类的教训:漏列即断 Created/Tick 等分发)
  // The subclass registry must re-list the base's interface set (upstream's
  // GetInterfaces includes the inherited faces — the batch-5 Classic
  // subclass lesson: omitting them severs the Created/Tick dispatch).
  static constexpr auto kTraitUpcasts =
      ora::sim::MakeTraitUpcasts<WithFacingSpriteBody, WithSpriteBody,
                                 sim::IObservesVariables, sim::INotifyCreated,
                                 sim::INotifyDamageStateChanged,
                                 sim::IAutoMouseBounds>();
  std::span<const ora::sim::TraitUpcastEntry> TraitUpcasts() const override {
    return kTraitUpcasts;
  }
};

}  // namespace ora::mods
