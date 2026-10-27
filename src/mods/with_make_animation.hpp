#pragma once
import std;

#include "gfx/animation.hpp"
#include "meta/generic_record.hpp"
#include "mods/with_sprite_body.hpp"
#include "sim/actor.hpp"
#include "sim/trait_interfaces.hpp"

namespace ora::mods {

class WithMakeOverlay;

struct WithMakeAnimationInfoData {
  std::string str_sequence = "make";
  std::string str_condition;
  std::vector<std::string> vec_body_names{"body"};

  static WithMakeAnimationInfoData Parse(const meta::RecordObject& rec_info);
};

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

  void Forward(Actor& self, std::function<void()> fn_on_complete);
  void Reverse(Actor& self, std::function<void()> fn_on_complete);
  void Reverse(Actor& self, sim::Activity* ptr_activity, bool b_queued = true);

  void Deploy(Actor& self, bool b_skip_make_anim) override;
  void Undeploy(Actor& self, bool b_skip_make_anim) override;

  const WithMakeAnimationInfoData& Info() const { return info_; }

 private:

  WithSpriteBody* FirstEnabledBody() const;

  WithMakeAnimationInfoData info_;
  std::vector<WithSpriteBody*> vec_wsbs_;
  bool b_skip_make_animation_ = false;
  std::vector<WithMakeOverlay*> vec_overlays_;
  int int4_token_ = Actor::InvalidConditionToken;
  bool b_deploy_notified_ = false;

  bool b_undeploy_notified_ = false;
};

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

}
