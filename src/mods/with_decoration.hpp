// UPSTREAM: OpenRA.Mods.Common/Traits/Render/WithDecorationBase.cs @b6fc03f
//          L18-146 + WithDecoration.cs L18-71 装饰族基座:泛型基 → CRTP
//          双参(DerivedT + InfoT);FrozenDictionary<BooleanExpression,…>
//          → 插入序对(键序 = 首命中序)。
//          The decoration-family base: the generic base → the two-parameter
//          CRTP (DerivedT + InfoT); FrozenDictionary<BooleanExpression,…>
//          → insertion-ordered pairs (the key order = the first-hit one).
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

/// WithDecorationBase.cs L18:BlinkState
/// WithDecorationBase.cs L18: BlinkState.
enum class BlinkState : std::uint8_t {
  Off = 0,
  On = 1,
};

/// WithDecorationBaseInfo(L20-55)的解析面
/// The parsed face of WithDecorationBaseInfo (L20-55).
struct WithDecorationBaseInfoData {
  sim::ConditionalTraitData conditional;
  std::string str_position = "TopLeft";  // L24
  sim::PlayerRelationship valid_relationships =
      sim::PlayerRelationship::Ally;     // L27
  bool b_requires_selection = false;     // L30
  int2 int2_margin{};                    // L33
  /// Offsets(L37-39):[条件表达式 → 屏幕偏移] 插入序对
  /// Offsets (L37-39): the [condition expression → screen offset]
  /// insertion-ordered pairs.
  std::vector<std::pair<expr::BooleanExpression, int2>> vec_offsets;
  int int4_blink_interval = 5;  // L42
  /// BlinkPattern(L45-46)
  std::vector<BlinkState> vec_blink_pattern;
  /// BlinkPatterns(L49-51):[条件表达式 → 模式] 插入序对
  /// BlinkPatterns (L49-51): the [condition expression → pattern]
  /// insertion-ordered pairs.
  std::vector<std::pair<expr::BooleanExpression,
                        std::vector<BlinkState>>> vec_blink_patterns;

  static WithDecorationBaseInfoData Parse(const meta::RecordObject& rec_info);

  /// ConsumedConditions(L53-55):Offsets/BlinkPatterns 键变量的并集(去重)
  /// ConsumedConditions (L53-55): the deduplicated union of the
  /// Offsets/BlinkPatterns key variables.
  std::vector<std::string> ConsumedConditionVariables() const;
};

/// WithDecorationBase<InfoType>(L57-146):装饰 trait 的 CRTP 基座
/// WithDecorationBase<InfoType> (L57-146): the decoration traits' CRTP
/// base.
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

  /// IObservesVariables:base(requires)→ Offsets → BlinkPatterns 序
  /// IObservesVariables: the base (requires) → Offsets → BlinkPatterns
  /// order.
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

  /// IDecoration(L104)
  bool RequiresSelection() const override {
    return info_.base.b_requires_selection;
  }

  /// IDecoration.RenderDecoration(L106-111)
  void RenderDecoration(Actor& self, gfx::WorldRenderer& wr,
                        sim::ISelectionDecorations& container,
                        std::vector<gfx::RenderItem>& vec_out) override;

  const InfoT& InfoData() const { return info_; }

 protected:
  /// ShouldRender(L71-93)
  virtual bool ShouldRender(Actor& self);

  /// RenderDecoration(L102):屏幕位处的具体绘制
  /// RenderDecoration (L102): the concrete draw at the screen position.
  virtual void RenderDecorationAt(Actor& self, gfx::WorldRenderer& wr,
                                  int2 int2_screen_pos,
                                  std::vector<gfx::RenderItem>& vec_out) = 0;

  InfoT info_;
  int2 int2_conditional_offset_{};
  std::vector<BlinkState> vec_blink_pattern_;

 private:
  /// OffsetConditionChanged(L117-127):首个命中键的偏移胜
  /// OffsetConditionChanged (L117-127): the first matching key's offset
  /// wins.
  void OffsetConditionChanged(Actor& self,
                              sim::ConditionCacheView conditions);

  /// BlinkConditionsChanged(L129-141)
  void BlinkConditionsChanged(Actor& self,
                              sim::ConditionCacheView conditions);
};

/// ShouldRender(L71-93)
template <class DerivedT, class InfoT>
bool WithDecorationBase<DerivedT, InfoT>::ShouldRender(Actor& self) {
  if (self.world().FogObscures(self))
    return false;

  // WorldTick/BlinkInterval 对模式长度取模
  // WorldTick/BlinkInterval modulo the pattern length.
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

/// IDecoration.RenderDecoration(L106-111)
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

/// WithDecorationInfo(L27-50)的解析面
/// The parsed face of WithDecorationInfo (L27-50).
struct WithDecorationInfoData {
  WithDecorationBaseInfoData base;
  std::string str_image;                    // L31
  std::string str_sequence;                 // L35([FieldLoader.Require])
  std::string str_palette = "chrome";       // L39
  bool b_is_player_palette = false;         // L43

  static WithDecorationInfoData Parse(const meta::RecordObject& rec_info);
};

/// WithDecoration(L52-71):通用 sprite 装饰
/// WithDecoration (L52-71): the generic sprite decoration.
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
  /// GetPalette(L61-64)
  gfx::PaletteReference* GetPalette(Actor& self, gfx::WorldRenderer& wr);

  void RenderDecorationAt(Actor& self, gfx::WorldRenderer& wr,
                          int2 int2_screen_pos,
                          std::vector<gfx::RenderItem>& vec_out) override;

 private:
  std::string str_image_;
  std::unique_ptr<gfx::Animation> up_anim_;
};

}  // namespace ora::mods
