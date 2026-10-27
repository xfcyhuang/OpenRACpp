#pragma once
import std;

#include "meta/generic_record.hpp"
#include "mods/render_sprites.hpp"
#include "sim/actor.hpp"
#include "sim/conditional_trait.hpp"
#include "sim/trait_interfaces.hpp"

namespace ora::mods {

struct WithDeathAnimationInfoData {
  sim::ConditionalTraitData conditional;
  std::string str_death_sequence = "die";
  std::string str_death_sequence_palette = "player";
  bool b_death_palette_is_player_palette = true;
  bool b_use_death_type_suffix = true;
  std::string str_crushed_sequence;
  std::string str_crushed_sequence_palette = "effect";
  bool b_crushed_palette_is_player_palette = false;

  std::vector<std::pair<std::string, std::vector<std::string>>>
      vec_death_types;
  std::string str_fallback_sequence;
  int int4_delay = 0;

  static WithDeathAnimationInfoData Parse(const meta::RecordObject& rec_info);
};

class WithDeathAnimation final : public TraitBase,
                                 public sim::ConditionalTraitCore<WithDeathAnimation>,
                                 public sim::IObservesVariables,
                                 public sim::INotifyKilled,
                                 public sim::INotifyCrushed {
 public:
  WithDeathAnimation(const ActorInitializer& init,
                     WithDeathAnimationInfoData info);

  ORA_TRAIT_INTERFACES(
      WithDeathAnimation, OpenRA_Mods_Common_Traits_Render_WithDeathAnimation,
      sim::IObservesVariables, sim::INotifyKilled, sim::INotifyCrushed)

  bool IsTraitEnabled() const override {
    return !sim::ConditionalTraitCore<WithDeathAnimation>::IsTraitDisabled();
  }
  bool IsTraitDisabled() const override {
    return sim::ConditionalTraitCore<WithDeathAnimation>::IsTraitDisabled();
  }
  std::vector<sim::VariableObserver> GetVariableObservers() override {
    return CollectObservers();
  }

  void Killed(Actor& self, const sim::AttackInfo& e) override;
  void OnCrush(Actor& self, Actor& crusher,
               const core::BitSet<sim::CrushClass>& crush_classes) override;
  void WarnCrush(Actor& self, Actor& crusher,
                 const core::BitSet<sim::CrushClass>& crush_classes) override {
    (void)self;
    (void)crusher;
    (void)crush_classes;
  }

  void SpawnDeathAnimation(Actor& self, WPos pos, std::string_view str_image,
                           std::string_view str_sequence,
                           std::string_view str_palette, int int4_delay);

  const WithDeathAnimationInfoData& Info() const { return info_; }

 private:
  WithDeathAnimationInfoData info_;
  RenderSprites* ptr_render_sprites_ = nullptr;
  bool b_crushed_ = false;
};

}
