// UPSTREAM: OpenRA.Mods.Common/Traits/Render/WithSpriteTurret.cs @b6fc03f
//          L21-144 全文
//          The whole of WithSpriteTurret.cs L21-144.
//
// 机制对照 / Mechanism mapping:
//  - RenderPreviewSprites(IActorPreview 面)随 Phase 6 预览装配
//    RenderPreviewSprites (the IActorPreview face) rides Phase 6's preview
//    assembly.
//  - TurretOffset 的 recoil 常量折叠:WVec(new WDist(-recoilDist), Zero,
//    Zero).Rotate(WorldOrientation) —— 运算逐字
//    TurretOffset's recoil arithmetic kept verbatim (WVec(new
//    WDist(-recoilDist), Zero, Zero).Rotate(WorldOrientation)).
#pragma once
import std;

#include "core/wvec.hpp"
#include "gfx/animation.hpp"
#include "meta/generic_record.hpp"
#include "mods/render_sprites.hpp"
#include "sim/actor.hpp"
#include "sim/conditional_trait.hpp"
#include "sim/trait_interfaces.hpp"

namespace ora::mods {

class Turreted;
class BodyOrientation;
class Armament;

/// WithSpriteTurretInfo 的解析面(L22-42)
/// The parsed face of WithSpriteTurretInfo (L22-42).
struct WithSpriteTurretInfoData {
  sim::ConditionalTraitData conditional;
  std::string str_sequence = "turret";  // L27
  std::string str_palette;              // L30
  bool b_is_player_palette = false;     // L33
  std::string str_turret = "primary";   // L36
  bool b_recoils = true;                // L39

  static WithSpriteTurretInfoData Parse(const meta::RecordObject& rec_info);
};

/// WithSpriteTurret(L75-143)
class WithSpriteTurret : public TraitBase,
                         public sim::ConditionalTraitCore<WithSpriteTurret>,
                         public sim::IObservesVariables,
                         public sim::INotifyCreated,
                         public sim::INotifyDamageStateChanged {
 public:
  WithSpriteTurret(Actor& self, const WithSpriteTurretInfoData& info);

  ORA_TRAIT_INTERFACES(
      WithSpriteTurret, OpenRA_Mods_Common_Traits_Render_WithSpriteTurret,
      sim::IObservesVariables, sim::INotifyCreated,
      sim::INotifyDamageStateChanged)

  bool IsTraitEnabled() const override {
    return !sim::ConditionalTraitCore<WithSpriteTurret>::IsTraitDisabled();
  }
  bool IsTraitDisabled() const override {
    return sim::ConditionalTraitCore<WithSpriteTurret>::IsTraitDisabled();
  }

  void Created(Actor& self) override {
    sim::ConditionalTraitCore<WithSpriteTurret>::CoreCreated(self);
  }
  std::vector<sim::VariableObserver> GetVariableObservers() override {
    return CollectObservers();
  }

  const WithSpriteTurretInfoData& Info() const { return info_; }

  /// TurretOffset(L103-113;虚钩)
  /// TurretOffset (L103-113; a virtual hook).
  virtual WVec TurretOffset(Actor& self);

  /// NormalizeSequence(L115-118)
  std::string NormalizeSequence(Actor& self, std::string_view str_sequence);

  void DamageStateChanged(Actor& self, const sim::AttackInfo& e) override;

  void PlayCustomAnimation(Actor& self, std::string_view str_name,
                           std::function<void()> fn_after = {});
  void CancelCustomAnimation(Actor& self);

  void TraitEnabledHook(Actor&) {}
  void TraitDisabledHook(Actor&) {}
  void TraitResumedHook(Actor&) {}
  void TraitPausedHook(Actor&) {}

  gfx::Animation& DefaultAnimation() { return *up_default_animation_; }

 protected:
  /// DamageStateChanged 虚钩(L120-124)
  /// The DamageStateChanged virtual hook (L120-124).
  virtual void DamageStateChangedInner(Actor& self);

  WithSpriteTurretInfoData info_;
  BodyOrientation* ptr_body_ = nullptr;
  Turreted* ptr_turreted_ = nullptr;
  std::vector<Armament*> vec_arms_;
  std::unique_ptr<gfx::Animation> up_default_animation_;
  std::optional<gfx::AnimationWithOffset> awo_default_;
};

}  // namespace ora::mods
