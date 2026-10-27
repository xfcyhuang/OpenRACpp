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

struct HealthInfoData {
  int hp = 0;
  bool notify_applied_damage = true;
  int editor_health_display_order = 2;

  static HealthInfoData Parse(const meta::RecordObject& rec_info);
};

class Health final : public TraitBase,
                     public IHealth,
                     public ISync,
                     public ITick,
                     public INotifyCreated,
                     public INotifyOwnerChanged {
 public:
  Health(ActorInitializer& init, const HealthInfoData& info);

  ORA_TRAIT_INTERFACES(Health, OpenRA_Mods_Common_Traits_Health, IHealth,
                       ISync, ITick, INotifyCreated, INotifyOwnerChanged)

  int HP() const override { return hp_; }
  int MaxHP() const override { return max_hp_; }
  bool IsDead() const override { return hp_ <= 0; }
  bool RemoveOnDeath = true;

  int DisplayHP() const override { return display_hp_; }

  sim::DamageState DamageState() const override;

  void Resurrect(Actor& self, Actor* repairer);
  void InflictDamage(Actor& self, Actor* attacker, const Damage& damage,
                     bool ignore_modifiers) override;
  void Kill(Actor& self, Actor* attacker,
            const core::BitSet<DamageType>& damage_types) override;

  void Tick(Actor& self) override;
  void Created(Actor& self) override;
  void OnOwnerChanged(Actor& self, Player& old_owner,
                      Player& new_owner) override;

 private:
  HealthInfoData info_;
  std::vector<INotifyDamageStateChanged*> vec_notify_damage_state_changed_;
  std::vector<INotifyDamage*> vec_notify_damage_;
  std::vector<INotifyDamage*> vec_notify_damage_player_;
  std::vector<IDamageModifier*> vec_damage_modifiers_;
  std::vector<IDamageModifier*> vec_damage_modifiers_player_;
  std::vector<INotifyKilled*> vec_notify_killed_;
  std::vector<INotifyKilled*> vec_notify_killed_player_;
  int display_hp_ = 0;
  int hp_ = 0;
  int max_hp_ = 0;
};

class HealthInit final : public ValueActorInit<int>,
                         public ISingleInstanceInit {
 public:
  ORA_INIT_TYPE(HealthInit, OpenRA_Mods_Common_Traits_HealthInit,
                gen::TypeId::OpenRA_ISingleInstanceInit)

  HealthInit(int value, bool allow_zero = false)
      : ValueActorInit<int>(value), allow_zero_{allow_zero} {}

  std::span<const sim::TraitUpcastEntry> TraitUpcasts() const override {
    return {};
  }

  int Value() const {
    const int v = ValueActorInit<int>::Value();
    if (v < 0 || (v == 0 && !allow_zero_))
      return 1;
    return v;
  }

 private:
  bool allow_zero_ = false;
};

}
