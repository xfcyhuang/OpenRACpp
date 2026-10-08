// UPSTREAM: OpenRA.Mods.Common/Traits/Render/WithMakeAnimation.cs @b6fc03f
//          L20-203 全文 + WithMakeOverlay.cs L17-64 全文
//          The whole of WithMakeAnimation.cs L20-203 + WithMakeOverlay.cs
//          L17-64.
//
// 机制对照 / Mechanism mapping:
//  - PlayCustomAnimation 的 onComplete 闭包族 → std::function(帧末任务闭包
//    体逐字;token 的 grant/revoke 配对保真)
//    PlayCustomAnimation's onComplete closures → std::function (the
//    frame-end closure bodies verbatim; the token grant/revoke pairing
//    kept).
//  - FirstEnabledConditionalTraitOrDefault(Exts.cs)→ 首个未禁用元素的内联
//    扫描
//    FirstEnabledConditionalTraitOrDefault (Exts.cs) → the inline scan for
//    the first enabled element.
#pragma once
import std;

#include "gfx/animation.hpp"
#include "meta/generic_record.hpp"
#include "mods/with_sprite_body.hpp"
#include "sim/actor.hpp"
#include "sim/trait_interfaces.hpp"

namespace ora::mods {

class WithMakeOverlay;  // 前向(vec_overlays_ 成员)| forward (the
                        // vec_overlays_ member).

/// WithMakeAnimationInfo 的解析面(L21-35)
/// The parsed face of WithMakeAnimationInfo (L21-35).
struct WithMakeAnimationInfoData {
  std::string str_sequence = "make";  // L24
  std::string str_condition;          // L28
  std::vector<std::string> vec_body_names{"body"};  // L31

  static WithMakeAnimationInfoData Parse(const meta::RecordObject& rec_info);
};

/// WithMakeAnimation(L37-202)
class WithMakeAnimation final : public TraitBase,
                                public sim::INotifyCreated,
                                public sim::INotifyDeployTriggered {
 public:
  WithMakeAnimation(const ActorInitializer& init,
                    const WithMakeAnimationInfoData& info);

  ORA_TRAIT_INTERFACES(
      WithMakeAnimation, OpenRA_Mods_Common_Traits_Render_WithMakeAnimation,
      sim::INotifyCreated, sim::INotifyDeployTriggered)

  void Created(Actor& self) override;

  /// Forward(L61-85)/Reverse 双形态(L87-111/L113-130)
  /// Forward (L61-85) / the two Reverse forms (L87-111/L113-130).
  void Forward(Actor& self, std::function<void()> fn_on_complete);
  void Reverse(Actor& self, std::function<void()> fn_on_complete);
  void Reverse(Actor& self, sim::Activity* ptr_activity, bool b_queued = true);

  void Deploy(Actor& self, bool b_skip_make_anim) override;
  void Undeploy(Actor& self, bool b_skip_make_anim) override;

  const WithMakeAnimationInfoData& Info() const { return info_; }

 private:
  /// wsbs 的首个启用条件 trait(FirstEnabledConditionalTraitOrDefault)
  /// The wsbs' first enabled conditional trait
  /// (FirstEnabledConditionalTraitOrDefault).
  WithSpriteBody* FirstEnabledBody() const;

  WithMakeAnimationInfoData info_;
  std::vector<WithSpriteBody*> vec_wsbs_;
  bool b_skip_make_animation_ = false;
  std::vector<WithMakeOverlay*> vec_overlays_;
  int int4_token_ = Actor::InvalidConditionToken;
  bool b_deploy_notified_ = false;    // L135/138 的闭包闩锁(上游局部变量
                                      // 的闭包提升等价)
  bool b_undeploy_notified_ = false;  // 同上 | same (L171/174).
};

/// WithMakeOverlay(WithMakeOverlay.cs L35-64)
class WithMakeOverlay final : public TraitBase {
 public:
  WithMakeOverlay(Actor& self, std::string str_sequence,
                  std::string str_palette, bool b_is_player_palette);

  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_Render_WithMakeOverlay;
  gen::TypeId GetTraitTypeId() const override { return kTypeId; }
  static constexpr auto kTraitUpcasts =
      ora::sim::MakeTraitUpcasts<WithMakeOverlay>();
  std::span<const ora::sim::TraitUpcastEntry> TraitUpcasts() const override {
    return kTraitUpcasts;
  }

  void Forward();
  void Reverse();

 private:
  std::string str_sequence_;
  std::unique_ptr<gfx::Animation> up_overlay_;
  std::optional<gfx::AnimationWithOffset> awo_overlay_;
  bool b_visible_ = false;
};

}  // namespace ora::mods
