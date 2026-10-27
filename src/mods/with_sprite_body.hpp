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

struct WithSpriteBodyInfoData {
  sim::ConditionalTraitData conditional;
  std::string str_start_sequence;
  std::string str_sequence = "idle";
  std::string str_name = "body";
  bool b_force_to_ground = false;
  std::string str_palette;
  bool b_is_player_palette = false;

  static WithSpriteBodyInfoData Parse(const meta::RecordObject& rec_info);
};

class WithSpriteBody : public TraitBase,
                       public sim::ConditionalTraitCore<WithSpriteBody>,
                       public sim::IObservesVariables,
                       public sim::INotifyCreated,
                       public sim::INotifyDamageStateChanged,
                       public sim::IAutoMouseBounds {
 public:

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

  std::string NormalizeSequence(Actor& self, std::string_view str_sequence);

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

  virtual void DamageStateChangedInner(Actor& self);

  WithSpriteBodyInfoData info_;
  std::function<WAngle()> fn_base_facing_;
  std::unique_ptr<gfx::Animation> up_default_animation_;
  std::optional<gfx::AnimationWithOffset> awo_default_;
  std::unique_ptr<gfx::Animation> up_bounds_animation_;
};

class WithFacingSpriteBody final : public WithSpriteBody {
 public:
  WithFacingSpriteBody(const ActorInitializer& init,
                       const WithSpriteBodyInfoData& info);

  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_Render_WithFacingSpriteBody;
  gen::TypeId GetTraitTypeId() const override { return kTypeId; }

  static constexpr auto kTraitUpcasts =
      ora::sim::MakeTraitUpcasts<WithFacingSpriteBody, WithSpriteBody,
                                 sim::IObservesVariables, sim::INotifyCreated,
                                 sim::INotifyDamageStateChanged,
                                 sim::IAutoMouseBounds>();
  std::span<const ora::sim::TraitUpcastEntry> TraitUpcasts() const override {
    return kTraitUpcasts;
  }
};

}
