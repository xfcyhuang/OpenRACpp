// UPSTREAM: OpenRA.Mods.Common/Traits/Cloak.cs @b6fc03f L23-377 全文 +
//          DetectCloaked.cs L11-54 全文 + IgnoresCloak.cs L11-19 全文
//          The whole of Cloak.cs L23-377 + DetectCloaked.cs L11-54 +
//          IgnoresCloak.cs L11-19.
//
// 机制对照 / Mechanism mapping:
//  - IRenderModifier/IRadarColorModifier 的渲染物化面(WithAlpha/
//    WithTint/WithPalette/RadarColorOverride)随渲染批 —— Cloak 的同步
//    面(remainingTime/Cloaked/Uncloak 族/可见性/条件授予)全量保留;
//    隐匿时 ModifyRender 返回 SpriteRenderable.None 的分支由渲染批承载
//    The IRenderModifier/IRadarColorModifier render materialization
//    (WithAlpha/WithTint/WithPalette/RadarColorOverride) rides the render
//    batch — Cloak's synchronous faces (remainingTime/Cloaked/the
//    Uncloak family/visibility/condition granting) are kept in full;
//    the ModifyRender-returns-None branch lands with the render batch.
//  - 声音/SpriteEffect 注入面不构造(声音注入惯例;RNG 无涉 —— 声音走
//    Game.Sound 无随机)
//    The sound/SpriteEffect injection faces are not constructed (the
//    sound-injection convention; no RNG involvement — sounds go through
//    Game.Sound without randomness).
//  - 上游 otherCloaks = 同 CloakType 的其它 Cloak 实例(声音抑制判据);
//    C++ 侧同构缓存
//    Upstream's otherCloaks = the other Cloak instances sharing the
//    CloakType (the sound-suppression predicate); cached isomorphically.
#pragma once
import std;

#include "core/bitset.hpp"
#include "core/color.hpp"
#include "core/wangle.hpp"
#include "core/wdist.hpp"
#include "meta/generic_record.hpp"
#include "sim/actor.hpp"
#include "sim/conditional_trait.hpp"
#include "sim/trait_interfaces.hpp"

namespace ora::mods {

class Armament;   // INotifyAttack 签面前向 | the INotifyAttack forward.
struct Barrel;    // 同上 | same.

/// Cloak.cs L23-38:UncloakType 位域
/// Cloak.cs L23-38: the UncloakType bit set.
enum class UncloakType : std::int32_t {
  None = 0,
  Attack = 1,
  Move = 2,
  Load = 4,
  Unload = 8,
  Infiltrate = 16,
  Demolish = 32,
  Damage = 64,
  Heal = 128,
  SelfHeal = 256,
  Dock = 512,
  SupportPower = 1024,
};

inline bool HasUncloakFlag(UncloakType set, UncloakType flag) {
  return (static_cast<std::int32_t>(set) & static_cast<std::int32_t>(flag)) ==
         static_cast<std::int32_t>(flag);
}

/// Cloak.cs L41:DetectionType 位标签 | the DetectionType bit tag.
class DetectionType {};

/// Cloak.cs L43:CloakStyle
enum class CloakStyle : std::int32_t { None, Alpha, Color, Palette };

struct CloakInfoData {
  int int4_initial_delay = 10;             // L49
  int int4_cloak_delay = 30;               // L52
  std::int32_t int4_uncloak_on =           // L59
      static_cast<std::int32_t>(UncloakType::Attack) |
      static_cast<std::int32_t>(UncloakType::Unload) |
      static_cast<std::int32_t>(UncloakType::Infiltrate) |
      static_cast<std::int32_t>(UncloakType::Demolish) |
      static_cast<std::int32_t>(UncloakType::Dock);
  std::string str_cloak_sound;             // L62
  std::string str_uncloak_sound;           // L63
  core::BitSet<DetectionType> bitset_detection_types;  // L65
  std::string str_cloaked_condition;       // L69
  std::string str_cloak_type;              // L72
  sim::ConditionalTraitData conditional;

  static CloakInfoData Parse(const meta::RecordObject& rec);
};

/// Cloak(L114-377)
class Cloak final : public sim::TraitBase,
                    public sim::IObservesVariables,
                    public sim::INotifyCreated,
                    public sim::ITick,
                    public sim::ISync,
                    public sim::INotifyDamage,
                    public sim::INotifyAttack,
                    public sim::IVisibilityModifier,
                    public sim::INotifyDockClient,
                    public sim::INotifyDockHost,
                    public sim::INotifyLoadCargo,
                    public sim::INotifyUnloadCargo,
                    public sim::INotifyDemolition,
                    public sim::INotifyInfiltration,
                    public sim::INotifySupportPower,
                    private sim::ConditionalTraitCore<Cloak> {
 public:
  ORA_TRAIT_INTERFACES(Cloak, OpenRA_Mods_Common_Traits_Cloak,
                       sim::IObservesVariables, sim::INotifyCreated,
                       sim::ITick, sim::ISync, sim::INotifyDamage,
                       sim::INotifyAttack, sim::IVisibilityModifier,
                       sim::INotifyDockClient, sim::INotifyDockHost,
                       sim::INotifyLoadCargo, sim::INotifyUnloadCargo,
                       sim::INotifyDemolition, sim::INotifyInfiltration,
                       sim::INotifySupportPower)

  explicit Cloak(CloakInfoData info);
  ~Cloak() override;

  bool IsTraitEnabled() const override { return !ConditionalTraitCore::IsTraitDisabled(); }
  bool IsTraitDisabled() const override { return ConditionalTraitCore::IsTraitDisabled(); }
  bool IsTraitPaused() const { return ConditionalTraitCore::IsTraitPaused(); }

  /// L161:Cloaked 判定 | the Cloaked predicate.
  bool Cloaked() const {
    return !ConditionalTraitCore::IsTraitDisabled() &&
           !ConditionalTraitCore::IsTraitPaused() && RemainingTime <= 0;
  }

  /// L163-165:Uncloak 族 | the Uncloak family.
  void Uncloak() { UncloakFor(info_.int4_cloak_delay); }
  void UncloakFor(int time) {
    RemainingTime = std::max(RemainingTime, time);
  }

  const CloakInfoData& InfoData() const { return info_; }

  void Created(sim::Actor& self) override;                       // L142-159
  std::vector<sim::VariableObserver> GetVariableObservers() override {
    return CollectObservers();
  }
  void Tick(sim::Actor& self) override;                          // L219-288
  void Damaged(sim::Actor& self, const sim::AttackInfo& e) override;  // L171-181
  void Attacking(sim::Actor& self, const sim::Target& target,    // L167
                 Armament& armament, const Barrel& barrel) override;
  void PreparingAttack(sim::Actor&, const sim::Target&,          // L169
                       Armament&, const Barrel&) override {}
  bool IsVisible(sim::Actor& self, sim::Player* viewer) override;  // L297-305
  // INotifyDockClient.Docked/Undocked 与 INotifyDockHost 的同签名 ——
  // 上游两接口的实现体完全相同(isDocking + Uncloak),C++ 单个 override
  // 同时服务两接口,语义恰好一致(COVERAGE 登记:两接口面合并承载)
  // INotifyDockClient's Docked/Undocked share INotifyDockHost's exact
  // signatures — upstream's two implementation bodies are identical
  // (isDocking + Uncloak), so a single C++ override serves both
  // interfaces with exactly matching semantics (registered in COVERAGE:
  // the two interface faces carried merged).
  void Docked(sim::Actor& self, sim::Actor& host) override;      // L315-322/330-337
  void Undocked(sim::Actor& self, sim::Actor& host) override;    // L324-328/339-343
  void Loading(sim::Actor& self) override;                       // L345-349
  void Unloading(sim::Actor& self) override;                     // L351-355
  void Demolishing(sim::Actor& self) override;                   // L357-361
  void Infiltrating(sim::Actor& self) override;                  // L363-367
  void Charged(sim::Actor& self) override {}                     // L369
  void Activated(sim::Actor& self,                               // L371-375
                 const std::string& order_name) override;

  int RemainingTime = 0;  // [VerifySync] L122 remainingTime | the member.

 private:
  friend class sim::ConditionalTraitCore<Cloak>;
  void TraitEnabledHook(sim::Actor& self);   // L290-293
  void TraitDisabledHook(sim::Actor& self);  // L295(Uncloak)

  CloakInfoData info_;
  bool b_is_docking_ = false;
  std::vector<Cloak*> vec_other_cloaks_;
  std::optional<CPos> opt_last_pos_;
  bool b_was_cloaked_ = false;
  bool b_first_tick_ = true;
  int int4_cloaked_token_ = sim::Actor::InvalidConditionToken;
};

/// DetectCloaked(L35-54)
class DetectCloaked final : public sim::TraitBase,
                            public sim::IObservesVariables,
                            public sim::INotifyCreated,
                            private sim::ConditionalTraitCore<DetectCloaked> {
 public:
  ORA_TRAIT_INTERFACES(DetectCloaked, OpenRA_Mods_Common_Traits_DetectCloaked,
                       sim::IObservesVariables, sim::INotifyCreated)

  DetectCloaked(core::BitSet<DetectionType> detection_types, WDist range,
                sim::ConditionalTraitData conditional);
  ~DetectCloaked() override;

  bool IsTraitEnabled() const override { return !ConditionalTraitCore::IsTraitDisabled(); }
  bool IsTraitDisabled() const override { return ConditionalTraitCore::IsTraitDisabled(); }

  void Created(sim::Actor& self) override;  // L44-47
  std::vector<sim::VariableObserver> GetVariableObservers() override {
    return CollectObservers();
  }

  /// L49-54:Range(禁用恒零 + IDetectCloakedModifier 链)
  /// L49-54: Range (zero when disabled + the IDetectCloakedModifier
  /// chain).
  WDist Range() const;

  const core::BitSet<DetectionType>& DetectionTypes() const {
    return bitset_detection_types_;
  }

 private:
  friend class sim::ConditionalTraitCore<DetectCloaked>;
  core::BitSet<DetectionType> bitset_detection_types_;  // L21
  WDist range_;                                         // L23
  std::vector<sim::IDetectCloakedModifier*> vec_range_modifiers_;
};

/// IgnoresCloak(L18-19:空运行时类;Targetable 的 cloaks 过滤判据)
/// IgnoresCloak (L18-19: an empty runtime class; Targetable's cloaks
/// filter predicate).
class IgnoresCloak final : public sim::TraitBase {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_IgnoresCloak;
  gen::TypeId GetTraitTypeId() const override { return kTypeId; }
  static constexpr auto kTraitUpcasts =
      ora::sim::MakeTraitUpcasts<IgnoresCloak>();
  std::span<const ora::sim::TraitUpcastEntry> TraitUpcasts() const override {
    return kTraitUpcasts;
  }
};

}  // namespace ora::mods
