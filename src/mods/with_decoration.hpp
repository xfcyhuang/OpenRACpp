#pragma once
import std;

#include "core/color.hpp"
#include "core/int2.hpp"
#include "gfx/animation.hpp"
#include "gfx/renderable.hpp"
#include "meta/generic_record.hpp"
#include "meta/variable_expression.hpp"
#include "sim/actor.hpp"
#include "sim/conditional_trait.hpp"
#include "sim/trait_interfaces.hpp"
#include "sim/world.hpp"

namespace ora::gfx {
class WorldRenderer;
}

namespace ora::mods {

using sim::Actor;
using sim::ActorInitializer;
using sim::TraitBase;

enum class BlinkState : std::uint8_t {
  Off = 0,
  On = 1,
};

struct WithDecorationBaseInfoData {
  sim::ConditionalTraitData conditional;
  std::string str_position = "TopLeft";
  sim::PlayerRelationship valid_relationships =
      sim::PlayerRelationship::Ally;
  bool b_requires_selection = false;
  int2 int2_margin{};

  std::vector<std::pair<expr::BooleanExpression, int2>> vec_offsets;
  int int4_blink_interval = 5;

  std::vector<BlinkState> vec_blink_pattern;

  std::vector<std::pair<expr::BooleanExpression,
                        std::vector<BlinkState>>> vec_blink_patterns;

  static WithDecorationBaseInfoData Parse(const meta::RecordObject& rec_info);

  std::vector<std::string> ConsumedConditionVariables() const;
};

template <class DerivedT, class InfoT>
class WithDecorationBase : public TraitBase,
                           public sim::ConditionalTraitCore<DerivedT>,
                           public sim::IObservesVariables,
                           public sim::IDecoration {
 public:
  WithDecorationBase(const ActorInitializer& init, InfoT info_data)
      : sim::ConditionalTraitCore<DerivedT>(info_data.base.conditional),
        info_{std::move(info_data)} {
    (void)init;
    vec_blink_pattern_ = info_.base.vec_blink_pattern;
  }

  bool IsTraitEnabled() const override {
    return !sim::ConditionalTraitCore<DerivedT>::IsTraitDisabled();
  }
  bool IsTraitDisabled() const override {
    return sim::ConditionalTraitCore<DerivedT>::IsTraitDisabled();
  }

  std::vector<sim::VariableObserver> GetVariableObservers() override {
    std::vector<sim::VariableObserver> vec_observers =
        this->CollectObservers();
    for (const auto& [expr, offset] : info_.base.vec_offsets) {
      sim::VariableObserver observer;
      observer.fn_notifier = [this](Actor& self,
                                    sim::ConditionCacheView conditions) {
        OffsetConditionChanged(self, conditions);
      };
      observer.vec_variables.assign(expr.Variables().begin(),
                                    expr.Variables().end());
      vec_observers.push_back(std::move(observer));
      (void)offset;
    }
    for (const auto& [expr, pattern] : info_.base.vec_blink_patterns) {
      sim::VariableObserver observer;
      observer.fn_notifier = [this](Actor& self,
                                    sim::ConditionCacheView conditions) {
        BlinkConditionsChanged(self, conditions);
      };
      observer.vec_variables.assign(expr.Variables().begin(),
                                    expr.Variables().end());
      vec_observers.push_back(std::move(observer));
      (void)pattern;
    }
    return vec_observers;
  }

  bool RequiresSelection() const override {
    return info_.base.b_requires_selection;
  }

  void RenderDecoration(Actor& self, gfx::WorldRenderer& wr,
                        sim::ISelectionDecorations& container,
                        std::vector<gfx::RenderItem>& vec_out) override;

  const InfoT& InfoData() const { return info_; }

 protected:

  virtual bool ShouldRender(Actor& self);

  virtual void RenderDecorationAt(Actor& self, gfx::WorldRenderer& wr,
                                  int2 int2_screen_pos,
                                  std::vector<gfx::RenderItem>& vec_out) = 0;

  InfoT info_;
  int2 int2_conditional_offset_{};
  std::vector<BlinkState> vec_blink_pattern_;

 private:

  void OffsetConditionChanged(Actor& self,
                              sim::ConditionCacheView conditions);

  void BlinkConditionsChanged(Actor& self,
                              sim::ConditionCacheView conditions);
};

template <class DerivedT, class InfoT>
bool WithDecorationBase<DerivedT, InfoT>::ShouldRender(Actor& self) {
  if (self.world().FogObscures(self))
    return false;

  if (!vec_blink_pattern_.empty()) {
    const std::size_t sz_index = static_cast<std::size_t>(
        self.world().WorldTick() / info_.base.int4_blink_interval %
        static_cast<std::int32_t>(vec_blink_pattern_.size()));
    if (vec_blink_pattern_[sz_index] != BlinkState::On)
      return false;
  }

  if (self.world().RenderPlayer() != nullptr) {
    const sim::PlayerRelationship relationship =
        self.Owner()->RelationshipWith(self.world().RenderPlayer());
    if (!sim::HasRelationship(info_.base.valid_relationships, relationship))
      return false;
  }

  return true;
}

template <class DerivedT, class InfoT>
void WithDecorationBase<DerivedT, InfoT>::RenderDecoration(
    Actor& self, gfx::WorldRenderer& wr,
    sim::ISelectionDecorations& container,
    std::vector<gfx::RenderItem>& vec_out) {
  if (IsTraitDisabled() || self.IsDead() || !self.IsInWorld() ||
      !ShouldRender(self))
    return;

  const int2 int2_screen_pos =
      container.GetDecorationOrigin(self, wr, info_.base.str_position,
                                    info_.base.int2_margin) +
      int2_conditional_offset_;
  RenderDecorationAt(self, wr, int2_screen_pos, vec_out);
}

template <class DerivedT, class InfoT>
void WithDecorationBase<DerivedT, InfoT>::OffsetConditionChanged(
    Actor& self, sim::ConditionCacheView conditions) {
  (void)self;
  int2_conditional_offset_ = int2{};
  for (const auto& [expr, offset] : info_.base.vec_offsets)
    if (expr.Evaluate(sim::ConditionSymbols(conditions))) {
      int2_conditional_offset_ = offset;
      break;
    }
}

template <class DerivedT, class InfoT>
void WithDecorationBase<DerivedT, InfoT>::BlinkConditionsChanged(
    Actor& self, sim::ConditionCacheView conditions) {
  (void)self;
  vec_blink_pattern_ = info_.base.vec_blink_pattern;
  for (const auto& [expr, pattern] : info_.base.vec_blink_patterns)
    if (expr.Evaluate(sim::ConditionSymbols(conditions))) {
      vec_blink_pattern_ = pattern;
      return;
    }
}

struct WithDecorationInfoData {
  WithDecorationBaseInfoData base;
  std::string str_image;
  std::string str_sequence;
  std::string str_palette = "chrome";
  bool b_is_player_palette = false;

  static WithDecorationInfoData Parse(const meta::RecordObject& rec_info);
};

class WithDecoration final
    : public WithDecorationBase<WithDecoration, WithDecorationInfoData>,
      public sim::ITick {
 public:
  WithDecoration(const ActorInitializer& init, WithDecorationInfoData info);

  ORA_TRAIT_INTERFACES(WithDecoration,
                       OpenRA_Mods_Common_Traits_Render_WithDecoration,
                       sim::IObservesVariables, sim::IDecoration, sim::ITick)

  void Tick(Actor& self) override { up_anim_->Tick(); }

  gfx::Animation& DecorationAnimation() { return *up_anim_; }

 protected:

  gfx::PaletteReference* GetPalette(Actor& self, gfx::WorldRenderer& wr);

  void RenderDecorationAt(Actor& self, gfx::WorldRenderer& wr,
                          int2 int2_screen_pos,
                          std::vector<gfx::RenderItem>& vec_out) override;

 private:
  std::string str_image_;
  std::unique_ptr<gfx::Animation> up_anim_;
};

}
