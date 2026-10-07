// UPSTREAM: OpenRA.Mods.Common/Traits/Turreted.cs @b6fc03f L19-333(全文逐语
//          义重写;IActorPreviewInitInfo 的预览面/IEditorActorOptions 的编辑
//          器 UI 面随 Phase 6 —— ValueActorInit/TurretFacingInit 的 init 面
//          本批全承载)
//          The whole of Turreted.cs L19-333 (a verbatim-semantics rewrite;
//          IActorPreviewInitInfo's preview face / IEditorActorOptions's
//          editor-UI face land with Phase 6 — the ValueActorInit/
//          TurretFacingInit init faces are carried in full this batch).
//
// 机制对照 / Mechanism mapping:
//  - PausableConditionalTrait<TurretedInfo> → sim::ConditionalTraitCore<
//    Turreted>(四钩子静态派发;同 Mobile 形)
//    PausableConditionalTrait<TurretedInfo> → sim::ConditionalTraitCore<
//    Turreted> (the four statically dispatched hooks; Mobile's shape).
//  - WorldFacingFromInit 的 Func<WAngle> 闭包族 → std::function<WAngle()>
//    (捕获 facing/dynamicInit 值副本;上游闭包语义保真)
//    WorldFacingFromInit's Func<WAngle> closure family →
//    std::function<WAngle()> (value-captured facing/dynamicInit; the
//    upstream closure semantics kept).
//  - Created 的 SingleOrDefault(AttackTurreted 匹配)→ 首匹配 + 多匹配抛
//    "Sequence contains more than one matching element"(上游 Single 语义)
//    Created's SingleOrDefault (the AttackTurreted match) → the first
//    match + the multi-match throw of "Sequence contains more than one
//    matching element" (upstream Single's semantics).
//  - QuantizedFacings 的 ArgumentOutOfRangeException 消息文本逐字(.NET
//    三段格式)
//    QuantizedFacings' ArgumentOutOfRangeException keeps the verbatim
//    .NET three-segment message text.
#pragma once
import std;

#include "core/wangle.hpp"
#include "core/wrot.hpp"
#include "core/wvec.hpp"
#include "meta/generic_record.hpp"
#include "sim/actor.hpp"
#include "sim/actor_init.hpp"
#include "sim/conditional_trait.hpp"
#include "sim/trait_interfaces.hpp"

namespace ora::mods {

class AttackTurreted;
class BodyOrientation;

using sim::Actor;
using sim::ActorInitializer;
using sim::TraitBase;

/// TurretedInfo 的解析面(L21-127)
/// The parsed face of TurretedInfo (L21-127).
struct TurretedInfoData {
  std::string str_turret{"primary"};  // L23
  WAngle angle_turn_speed{512};       // L26
  WAngle angle_initial_facing{0};     // L28
  int int4_realign_delay = 40;        // L31
  WVec vec_offset{0, 0, 0};           // L34
  int int4_editor_turret_facing_display_order = 4;  // L37
  sim::ConditionalTraitData conditional;

  static TurretedInfoData Parse(const meta::RecordObject& rec_info);

  /// L58-82:WorldFacingFromInit(静态;info 实例名携带)
  /// L58-82: WorldFacingFromInit (static; carries the info instance name).
  static std::function<WAngle()> WorldFacingFromInit(
      ActorInitializer& init, std::string_view info_instance_name,
      WAngle default_facing);

  /// L89-103:LocalFacingFromInit
  /// L89-103: LocalFacingFromInit.
  std::function<WAngle()> LocalFacingFromInit(
      ActorInitializer& init,
      std::string_view info_instance_name) const;
};

/// Turreted(L130-314)
class Turreted : public TraitBase,
                 public sim::ConditionalTraitCore<Turreted>,
                 public sim::IObservesVariables,
                 public sim::INotifyCreated,
                 public sim::ITick,
                 public sim::IDeathActorInitModifier,
                 public sim::IActorPreviewInitModifier,
                 public sim::ISync {
 public:
  Turreted(ActorInitializer& init, const TurretedInfoData& info,
           std::string str_info_instance_name);

  ORA_TRAIT_INTERFACES(Turreted, OpenRA_Mods_Common_Traits_Turreted,
                       sim::IObservesVariables, sim::INotifyCreated,
                       sim::ITick, sim::IDeathActorInitModifier,
                       sim::IActorPreviewInitModifier, sim::ISync)

  bool IsTraitEnabled() const override {
    return !sim::ConditionalTraitCore<Turreted>::IsTraitDisabled();
  }
  bool IsTraitDisabled() const override {
    return sim::ConditionalTraitCore<Turreted>::IsTraitDisabled();
  }

  void Created(Actor& self) override;
  std::vector<sim::VariableObserver> GetVariableObservers() override {
    return CollectObservers();
  }

  /// L137-147:[VerifySync] QuantizedFacings(0 拒绝 —— .NET 异常文本逐字)
  /// L137-147: the [VerifySync] QuantizedFacings (zero rejected — the
  /// .NET exception text verbatim).
  int QuantizedFacings() const { return int4_quantized_facings_; }
  void SetQuantizedFacings(int value) {
    if (value == 0)
      throw std::out_of_range(
          "Expected nonzero facings for turret. (Parameter 'value')\n"
          "Actual value was 0.");
    int4_quantized_facings_ = value;
  }

  /// L153-165:WorldOrientation
  /// L153-165: WorldOrientation.
  WRot WorldOrientation() const;

  /// L167:LocalOrientation | L167: LocalOrientation.
  WRot LocalOrientation() const { return rot_local_orientation_; }
  void SetLocalOrientation(WRot value) { rot_local_orientation_ = value; }

  /// L172-173:Offset/Name | L172-173: Offset/Name.
  WVec Offset() const { return info_.vec_offset + vec_local_offset_; }
  const std::string& Name() const { return info_.str_turret; }

  /// L194-224:Tick(protected virtual 的 ITick 面)
  /// L194-224: Tick (the ITick face of the protected virtual).
  void Tick(Actor& self) override;

  /// L263-281:FaceTarget | L263-281: FaceTarget.
  bool FaceTarget(Actor& self, const sim::Target& target);

  /// L283-290:HasAchievedDesiredFacing(virtual)
  /// L283-290: HasAchievedDesiredFacing (virtual).
  virtual bool HasAchievedDesiredFacing() const;

  /// L293-297:Position | L293-297: Position.
  WVec Position(Actor& self) const;

  /// L299-302:IDeathActorInitModifier
  /// L299-302: IDeathActorInitModifier.
  void ModifyDeathActorInit(Actor& self,
                            sim::TypeDictionary& init) override;

  /// L304-307:IActorPreviewInitModifier
  /// L304-307: IActorPreviewInitModifier.
  void ModifyActorPreviewInit(Actor& self,
                              sim::TypeDictionary& inits) override;

  /// 子类的炮塔相对位移面(L170)
  /// The subclass face of the turret's body-relative offset (L170).
  void SetLocalOffset(WVec value) { vec_local_offset_ = value; }

  void TraitEnabledHook(Actor& /*self*/) {}
  void TraitDisabledHookImpl(Actor& self);
  void TraitDisabledHook(Actor& self) { TraitDisabledHookImpl(self); }
  void TraitResumedHook(Actor& /*self*/) {}
  void TraitPausedHook(Actor& /*self*/) {}

  const TurretedInfoData& Info() const { return info_; }

 protected:
  /// L226-246:DesiredLocalFacing | L226-246: DesiredLocalFacing.
  WAngle DesiredLocalFacing() const;

  /// L248-261:MoveTurret | L248-261: MoveTurret.
  void MoveTurret();

 private:
  friend class sim::ConditionalTraitCore<Turreted>;

  const TurretedInfoData info_;
  std::string str_info_instance_name_;  // init 按名匹配面(GetOrDefault(info))

  AttackTurreted* p_attack_ = nullptr;  // L132
  sim::IFacing* p_facing_ = nullptr;    // L133
  BodyOrientation* p_body_ = nullptr;   // L134
  int int4_quantized_facings_ = 0;      // L137

  WVec vec_desired_direction_{0, 0, 0};  // L149
  int int4_realign_tick_ = 0;            // L150
  bool b_realign_desired_ = false;       // L151
  WRot rot_local_orientation_;           // L167
  WVec vec_local_offset_{0, 0, 0};       // L170
};

}  // namespace ora::mods
