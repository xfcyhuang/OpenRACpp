// UPSTREAM: OpenRA.Mods.Common/Traits/Health.cs @b6fc03f L19-261(逐语义重写)
//          Verbatim-semantics rewrite.
//
// 机制对照 / Mechanism mapping:
//  - InflictDamage 的手内联 decimal 修正链(L167-187)→
//    core/percent_modifiers.hpp 的 __int128 精确复刻(OPT-A1 已落地面;
//    modifier ≠ 100 收集 + span 直传,零 LINQ)
//    InflictDamage's hand-inlined decimal chain (L167-187) → the exact
//    __int128 replica in core/percent_modifiers.hpp (the OPT-A1 landed
//    face; non-100 modifiers collected + span-passed, zero LINQ).
//  - DamageState 阶梯的 `HP * 100L < MaxHP * 25L` 长整升位照抄
//    The DamageState ladder's `HP * 100L < MaxHP * 25L` long promotions
//    kept.
//  - RulesetLoaded(L33-36:无 HitShape 抛)→ 工厂面执行(值袋无
//    IRulesetLoaded 生命周期;时点差异 = COVERAGE 登记偏离)
//    RulesetLoaded (L33-36: the no-HitShape throw) → executed by the
//    factory (the value bag has no IRulesetLoaded lifecycle; the timing
//    difference is the registered COVERAGE deviation).
#pragma once
import std;

#include "sim/actor_init.hpp"
#include "sim/sync_hash.hpp"
#include "sim/trait_interfaces.hpp"

namespace ora::meta {
class RecordObject;
}

namespace ora::mods {

using sim::Actor;
using sim::ActorInitializer;
using sim::Damage;
using sim::DamageType;
using sim::IDamageModifier;
using sim::INotifyAppliedDamage;
using sim::INotifyDamage;
using sim::INotifyDamageStateChanged;
using sim::INotifyKilled;
using sim::ISync;
using sim::Player;
using sim::ValueActorInit;
using sim::ISingleInstanceInit;
using sim::ITick;
using sim::INotifyCreated;
using sim::INotifyOwnerChanged;
using sim::IHealth;
using sim::TraitBase;

/// Health 的运行时 Info 承载(HealthInfo 的四个字段;工厂面读值袋)
/// The runtime Info carrier of Health (HealthInfo's four fields; the
/// factory reads the bag).
struct HealthInfoData {
  int hp = 0;                             // L22
  bool notify_applied_damage = true;      // L25
  int editor_health_display_order = 2;    // L28

  static HealthInfoData Parse(const meta::RecordObject& rec_info);
};

/// Health(Health.cs L52-240) | Health (Health.cs L52-240).
class Health final : public TraitBase,
                     public IHealth,
                     public ISync,
                     public ITick,
                     public INotifyCreated,
                     public INotifyOwnerChanged {
 public:
  Health(ActorInitializer& init, const HealthInfoData& info);  // L65-76

  ORA_TRAIT_INTERFACES(Health, OpenRA_Mods_Common_Traits_Health, IHealth,
                       ISync, ITick, INotifyCreated, INotifyOwnerChanged)

  int HP() const override { return hp_; }          // L79([VerifySync])
  int MaxHP() const override { return max_hp_; }   // L80
  bool IsDead() const override { return hp_ <= 0; }  // L82
  bool RemoveOnDeath = true;              // L83(public 字段,上游同名)
                                          // (the public field, same name).
  int DisplayHP() const override { return display_hp_; }  // L63

  sim::DamageState DamageState() const override;   // L85-106

  void Resurrect(Actor& self, Actor* repairer);             // L126-156
  void InflictDamage(Actor& self, Actor* attacker, const Damage& damage,
                     bool ignore_modifiers) override;       // L158-226
  void Kill(Actor& self, Actor* attacker,
            const core::BitSet<DamageType>& damage_types) override;  // L228-231

  void Tick(Actor& self) override;                         // L233-239
  void Created(Actor& self) override;                      // L108-117
  void OnOwnerChanged(Actor& self, Player& old_owner,
                      Player& new_owner) override;         // L119-124

 private:
  HealthInfoData info_;                                    // L54 的 Info 面
  std::vector<INotifyDamageStateChanged*> vec_notify_damage_state_changed_;  // L55
  std::vector<INotifyDamage*> vec_notify_damage_;                            // L56
  std::vector<INotifyDamage*> vec_notify_damage_player_;                     // L57
  std::vector<IDamageModifier*> vec_damage_modifiers_;                       // L58
  std::vector<IDamageModifier*> vec_damage_modifiers_player_;                // L59
  std::vector<INotifyKilled*> vec_notify_killed_;                            // L60
  std::vector<INotifyKilled*> vec_notify_killed_player_;                     // L61
  int display_hp_ = 0;                     // L63
  int hp_ = 0;                             // L79
  int max_hp_ = 0;                         // L80
};

/// HealthInit(Health.cs L242-260;ValueActorInit<int> + ISingleInstanceInit)
/// HealthInit (Health.cs L242-260).
class HealthInit final : public ValueActorInit<int>,
                         public ISingleInstanceInit {
 public:
  ORA_INIT_TYPE(HealthInit, OpenRA_Mods_Common_Traits_HealthInit,
                gen::TypeId::OpenRA_ISingleInstanceInit)

  HealthInit(int value, bool allow_zero = false)
      : ValueActorInit<int>(value), allow_zero_{allow_zero} {}

  // ActorInit → TraitBase 的纯虚收尾(同 actor_init.hpp 注记)
  std::span<const sim::TraitUpcastEntry> TraitUpcasts() const override {
    return {};
  }

  /// Value 覆写(L249-259:负值/非允许零 → 1)
  /// The Value override (L249-259: negative / disallowed-zero → 1).
  int Value() const {
    const int v = ValueActorInit<int>::Value();
    if (v < 0 || (v == 0 && !allow_zero_))
      return 1;
    return v;
  }

 private:
  bool allow_zero_ = false;
};

}  // namespace ora::mods
