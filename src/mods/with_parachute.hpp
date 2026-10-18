// UPSTREAM: OpenRA.Mods.Common/Traits/Render/WithParachute.cs @e180e3225
#pragma once
import std;

#include "core/color.hpp"
#include "core/wvec.hpp"
#include "gfx/animation.hpp"
#include "meta/generic_record.hpp"
#include "sim/actor.hpp"
#include "sim/actor_init.hpp"
#include "sim/conditional_trait.hpp"
#include "sim/trait_interfaces.hpp"

namespace ora::mods {

using sim::Actor;
using sim::TraitBase;

class RenderSprites;
class BodyOrientation;

struct WithParachuteInfoData {
  sim::ConditionalTraitData conditional;
  std::string str_image;
  std::string str_opening_sequence;
  std::string str_sequence;
  std::string str_closing_sequence;
  std::string str_palette{"player"};
  bool b_is_player_palette = true;
  WVec wvec_offset{0, 0, 384};
  std::string str_shadow_image;
  std::string str_shadow_sequence;
  core::Color color_shadow = core::Color::FromArgb(140, 0, 0, 0);
  WVec wvec_shadow_offset{0, 128, 0};
  int int4_shadow_z_offset = 0;

  static WithParachuteInfoData Parse(const meta::RecordObject& rec_info);
};

class WithParachute final : public TraitBase,
                            public sim::ConditionalTraitCore<WithParachute>,
                            public sim::IObservesVariables,
                            public sim::INotifyCreated,
                            public sim::ITick,
                            public sim::IRender {
 public:
  WithParachute(const sim::ActorInitializer& init,
                WithParachuteInfoData info);

  ORA_TRAIT_INTERFACES(
      WithParachute, OpenRA_Mods_Common_Traits_Render_WithParachute,
      sim::IObservesVariables, sim::INotifyCreated, sim::ITick, sim::IRender)

  bool IsTraitEnabled() const override {
    return !sim::ConditionalTraitCore<WithParachute>::IsTraitDisabled();
  }
  bool IsTraitDisabled() const override {
    return sim::ConditionalTraitCore<WithParachute>::IsTraitDisabled();
  }
  std::vector<sim::VariableObserver> GetVariableObservers() override {
    return CollectObservers();
  }

  void Created(sim::Actor& self) override;
  void Tick(sim::Actor& self) override;
  void Render(sim::Actor& self, gfx::WorldRenderer& wr,
              std::vector<gfx::RenderItem>& vec_out) override;
  std::vector<Rectangle> ScreenBounds(sim::Actor& self,
                                      gfx::WorldRenderer& wr) override;

  void TraitEnabledHook(sim::Actor& self);
  void TraitDisabledHook(sim::Actor& self);

  const WithParachuteInfoData& Info() const { return info_; }
  bool RenderProlongedForTest() const { return b_render_prolonged_; }

 private:
  WithParachuteInfoData info_;
  std::optional<gfx::Animation> opt_shadow_;
  std::optional<gfx::Animation> opt_overlay_;
  std::optional<gfx::AnimationWithOffset> opt_anim_;
  BodyOrientation* ptr_body_ = nullptr;
  RenderSprites* ptr_render_sprites_ = nullptr;
  core::Vector3 vec_shadow_color_{1.0f, 1.0f, 1.0f};
  float fp4_shadow_alpha_ = 1.0f;
  bool b_render_prolonged_ = false;
};

}  // namespace ora::mods
