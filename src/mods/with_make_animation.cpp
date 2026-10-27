#include "mods/with_make_animation.hpp"

#include "sim/actor_init.hpp"
#include "sim/trait_registry.hpp"
#include "sim/world.hpp"

namespace ora::mods {

WithMakeAnimationInfoData WithMakeAnimationInfoData::Parse(
    const meta::RecordObject& rec_info) {
  WithMakeAnimationInfoData data;
  if (const auto s = sim::RecordFieldString(rec_info, "Sequence"))
    data.str_sequence = std::string{*s};
  if (const auto s = sim::RecordFieldString(rec_info, "Condition"))
    data.str_condition = std::string{*s};
  if (const auto* gv = sim::RecordFieldValue(rec_info, "BodyNames"))
    if (const auto* arr = std::get_if<std::vector<meta::GenericValue>>(&gv->val)) {
      data.vec_body_names.clear();
      for (const auto& element : *arr)
        if (const auto* str = std::get_if<std::string>(&element.val))
          data.vec_body_names.push_back(*str);
    }
  return data;
}

WithMakeAnimation::WithMakeAnimation(const ActorInitializer& init,
                                     const WithMakeAnimationInfoData& info)
    : info_{info} {

  Actor& self = init.Self();
  for (auto* w : self.TraitsImplementing<WithSpriteBody>())
    for (const auto& name : info.vec_body_names)
      if (w->Info().str_name == name) {
        vec_wsbs_.push_back(w);
        break;
      }
  b_skip_make_animation_ =
      init.Contains<sim::SkipMakeAnimsInit>({});
}

WithSpriteBody* WithMakeAnimation::FirstEnabledBody() const {

  for (auto* wsb : vec_wsbs_)
    if (!wsb->IsTraitDisabled())
      return wsb;
  return nullptr;
}

void WithMakeAnimation::Created(Actor& self) {

  for (auto* overlay : self.TraitsImplementing<WithMakeOverlay>())
    vec_overlays_.push_back(overlay);
  if (!b_skip_make_animation_)
    Forward(self, [] {});
}

void WithMakeAnimation::Forward(Actor& self,
                                std::function<void()> fn_on_complete) {

  if (int4_token_ == Actor::InvalidConditionToken)
    int4_token_ = self.GrantCondition(info_.str_condition);

  WithSpriteBody* ptr_wsb = FirstEnabledBody();

  if (ptr_wsb == nullptr)
    return;

  ptr_wsb->PlayCustomAnimation(self, info_.str_sequence,
                               [this, &self, fn_on_complete]() {
    self.world().AddFrameEndTask([this, &self, fn_on_complete](sim::World&) {
      if (int4_token_ != Actor::InvalidConditionToken)
        int4_token_ = self.RevokeCondition(int4_token_);

      fn_on_complete();
    });
  });

  for (auto* overlay : vec_overlays_)
    overlay->Forward();
}

void WithMakeAnimation::Reverse(Actor& self,
                                std::function<void()> fn_on_complete) {

  if (int4_token_ == Actor::InvalidConditionToken)
    int4_token_ = self.GrantCondition(info_.str_condition);

  WithSpriteBody* ptr_wsb = FirstEnabledBody();

  if (ptr_wsb == nullptr)
    return;

  ptr_wsb->PlayCustomAnimationBackwards(
      self, info_.str_sequence, [this, &self, fn_on_complete]() {
        self.world().AddFrameEndTask([this, &self, fn_on_complete](sim::World&) {
          if (int4_token_ != Actor::InvalidConditionToken)
            int4_token_ = self.RevokeCondition(int4_token_);

          fn_on_complete();
        });
      });

  for (auto* overlay : vec_overlays_)
    overlay->Reverse();
}

void WithMakeAnimation::Reverse(Actor& self, sim::Activity* ptr_activity,
                                bool b_queued) {

  Reverse(self, [this, &self, ptr_activity, b_queued]() {

    if (WithSpriteBody* ptr_wsb = FirstEnabledBody())
      ptr_wsb->DefaultAnimation().PlayFetchIndex(info_.str_sequence,
                                                 []() { return 0; });

    int4_token_ = self.GrantCondition(info_.str_condition);

    self.QueueActivity(b_queued, ptr_activity);
  });

  for (auto* overlay : vec_overlays_)
    overlay->Reverse();
}

void WithMakeAnimation::Deploy(Actor& self, bool b_skip_make_anim) {

  b_deploy_notified_ = false;

  if (b_skip_make_anim) {
    for (auto* n : self.TraitsImplementing<sim::INotifyDeployComplete>())
      n->FinishedDeploy(self);

    return;
  }

  for (auto* wsb : vec_wsbs_) {
    if (wsb->IsTraitDisabled())
      continue;

    wsb->PlayCustomAnimation(self, info_.str_sequence,
                             [this, &self]() {
                               if (b_deploy_notified_)
                                 return;

                               for (auto* n : self.TraitsImplementing<
                                        sim::INotifyDeployComplete>()) {
                                 n->FinishedDeploy(self);
                                 b_deploy_notified_ = true;
                               }
                             });
  }

  for (auto* overlay : vec_overlays_)
    overlay->Forward();
}

void WithMakeAnimation::Undeploy(Actor& self, bool b_skip_make_anim) {

  b_undeploy_notified_ = false;

  if (b_skip_make_anim) {
    for (auto* n : self.TraitsImplementing<sim::INotifyDeployComplete>())
      n->FinishedUndeploy(self);

    return;
  }

  for (auto* wsb : vec_wsbs_) {
    if (wsb->IsTraitDisabled())
      continue;

    wsb->PlayCustomAnimationBackwards(self, info_.str_sequence,
                                       [this, &self]() {
                                         if (b_undeploy_notified_)
                                           return;

                                         for (auto* n :
                                              self.TraitsImplementing<
                                                  sim::INotifyDeployComplete>()) {
                                           n->FinishedUndeploy(self);
                                           b_undeploy_notified_ = true;
                                         }
                                       });
  }

  for (auto* overlay : vec_overlays_)
    overlay->Reverse();
}

WithMakeOverlay::WithMakeOverlay(Actor& self, std::string str_sequence,
                                 std::string str_palette,
                                 bool b_is_player_palette) {

  auto* rs = self.Trait<RenderSprites>();
  up_overlay_ = std::make_unique<gfx::Animation>(
      RenderSprites::MakeAnimationDeps(self), rs->GetImage(self));
  up_overlay_->Play(str_sequence);
  str_sequence_ = std::move(str_sequence);

  awo_overlay_ = gfx::AnimationWithOffset{
      *up_overlay_, {}, [this]() { return !b_visible_; }};
  rs->Add(*awo_overlay_, str_palette.empty() ? std::string_view{}
                                            : std::string_view{str_palette},
          b_is_player_palette);
}

void WithMakeOverlay::Forward() {

  b_visible_ = true;
  up_overlay_->PlayThen(str_sequence_, [this]() { b_visible_ = false; });
}

void WithMakeOverlay::Reverse() {

  b_visible_ = true;
  up_overlay_->PlayBackwardsThen(str_sequence_, [this]() { b_visible_ = false; });
}

}
