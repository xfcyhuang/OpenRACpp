// UPSTREAM: OpenRA.Mods.Common/Traits/Health.cs @b6fc03f(实现部分)
//          The implementation half of Health.cs.
import std;

#include "mods/health.hpp"

#include "core/percent_modifiers.hpp"
#include "meta/generic_record.hpp"
#include "sim/actor.hpp"
#include "sim/trait_registry.hpp"
#include "sim/world.hpp"

namespace ora::mods {

// ———— HealthInfoData(值袋槽位 → 运行时 Info)————
HealthInfoData HealthInfoData::Parse(const meta::RecordObject& rec_info) {
  HealthInfoData data;
  if (auto v = sim::RecordFieldInt(rec_info, "HP"))
    data.hp = static_cast<int>(*v);
  if (auto v = sim::RecordFieldInt(rec_info, "NotifyAppliedDamage"))
    data.notify_applied_damage = *v != 0;
  if (auto v = sim::RecordFieldInt(rec_info, "EditorHealthDisplayOrder"))
    data.editor_health_display_order = static_cast<int>(*v);
  return data;
}

namespace {

/// Health 的 [VerifySync] 哈希(gen/sync_gen.cpp 成员表:{HP};Sync.cs 的
/// GenerateHashFunc 组合协议 = 0 XOR hash 成员)
/// Health's [VerifySync] hash (the gen/sync_gen.cpp member table {HP}; the
/// GenerateHashFunc combination protocol = 0 XOR the member hash).
int HealthSyncHash(const sim::ISync* s) {
  const auto* health = static_cast<const Health*>(s);
  return sim::sync::CombineSyncHash(0, health->HP());
}

const bool b_registered_sync_hash = [] {
  sim::RegisterSyncHashFunction("OpenRA.Mods.Common.Traits.Health",
                                &HealthSyncHash);
  return true;
}();

}  // namespace

// ———— ctor(L65-76)————
Health::Health(ActorInitializer& init, const HealthInfoData& info)
    : info_{info} {
  // MaxHP = HP = info.HP > 0 ? info.HP : 1(L68)| (L68).
  max_hp_ = info.hp > 0 ? info.hp : 1;
  hp_ = max_hp_;

  // Cast to long to avoid overflow when multiplying by the health(上游
  // 注释)| (upstream comment)
  auto* health_init = init.GetOrDefault<HealthInit>();
  if (health_init != nullptr)
    hp_ = static_cast<int>(static_cast<std::int64_t>(health_init->Value()) *
                           max_hp_ / 100);

  display_hp_ = hp_;  // L75
}

// ———— DamageState(L85-106)————
sim::DamageState Health::DamageState() const {
  if (hp_ == max_hp_)
    return sim::DamageState::Undamaged;

  if (hp_ <= 0)
    return sim::DamageState::Dead;

  if (static_cast<std::int64_t>(hp_) * 100 <
      static_cast<std::int64_t>(max_hp_) * 25)
    return sim::DamageState::Critical;

  if (static_cast<std::int64_t>(hp_) * 100 <
      static_cast<std::int64_t>(max_hp_) * 50)
    return sim::DamageState::Heavy;

  if (static_cast<std::int64_t>(hp_) * 100 <
      static_cast<std::int64_t>(max_hp_) * 75)
    return sim::DamageState::Medium;

  return sim::DamageState::Light;
}

// ———— INotifyCreated.Created(L108-117)————
void Health::Created(Actor& self) {
  vec_notify_damage_state_changed_ =
      self.TraitsImplementing<INotifyDamageStateChanged>();
  vec_notify_damage_ = self.TraitsImplementing<INotifyDamage>();
  vec_notify_damage_player_ =
      self.Owner()->PlayerActor()->TraitsImplementing<INotifyDamage>();
  vec_damage_modifiers_ = self.TraitsImplementing<IDamageModifier>();
  vec_damage_modifiers_player_ =
      self.Owner()->PlayerActor()->TraitsImplementing<IDamageModifier>();
  vec_notify_killed_ = self.TraitsImplementing<INotifyKilled>();
  vec_notify_killed_player_ =
      self.Owner()->PlayerActor()->TraitsImplementing<INotifyKilled>();
}

// ———— INotifyOwnerChanged.OnOwnerChanged(L119-124)————
void Health::OnOwnerChanged(Actor&, Player& old_owner, Player& new_owner) {
  (void)old_owner;
  vec_notify_damage_player_ =
      new_owner.PlayerActor()->TraitsImplementing<INotifyDamage>();
  vec_damage_modifiers_player_ =
      new_owner.PlayerActor()->TraitsImplementing<IDamageModifier>();
  vec_notify_killed_player_ =
      new_owner.PlayerActor()->TraitsImplementing<INotifyKilled>();
}

// ———— Resurrect(L126-156)————
void Health::Resurrect(Actor& self, Actor* repairer) {
  if (!IsDead())
    return;

  hp_ = max_hp_;

  const Damage damage{-max_hp_};
  sim::AttackInfo ai;
  ai.Attacker = repairer;
  ai.Damage = &damage;
  ai.damage_state = DamageState();
  ai.previous_damage_state = sim::DamageState::Dead;

  for (auto* nd : vec_notify_damage_)
    nd->Damaged(self, ai);
  for (auto* nd : vec_notify_damage_player_)
    nd->Damaged(self, ai);

  for (auto* nd : vec_notify_damage_state_changed_)
    nd->DamageStateChanged(self, ai);

  if (info_.notify_applied_damage && repairer != nullptr &&
      repairer->IsInWorld() && !repairer->IsDead()) {
    for (auto* nd : repairer->TraitsImplementing<INotifyAppliedDamage>())
      nd->AppliedDamage(*repairer, self, ai);
    for (auto* nd :
         repairer->Owner()->PlayerActor()->TraitsImplementing<INotifyAppliedDamage>())
      nd->AppliedDamage(*repairer, self, ai);
  }
}

// ———— InflictDamage(L158-226)————
void Health::InflictDamage(Actor& self, Actor* attacker, const Damage& damage,
                           bool ignore_modifiers) {
  // Overkill! Don't count extra hits as more kills!(上游注释)
  if (IsDead())
    return;

  const sim::DamageState old_state = DamageState();

  Damage effective_damage = damage;

  // Apply any damage modifiers(上游注释段;手内联 decimal → OPT-A1 面)
  // Apply any damage modifiers (upstream; the hand-inlined decimal → the
  // OPT-A1 face).
  if (!ignore_modifiers && damage.Value > 0) {
    // OPT-A9:修正值收集于栈上定长缓冲(span 直传,零分配)
    // OPT-A9: the modifier values collect in a stack fixed buffer (span
    // passed, zero allocation).
    std::int32_t modifiers[16];
    std::size_t count = 0;
    for (auto* dm : vec_damage_modifiers_) {
      const int modifier = dm->GetDamageModifier(attacker, damage);
      if (modifier != 100)
        modifiers[count++] = modifier;
    }
    for (auto* dm : vec_damage_modifiers_player_) {
      const int modifier = dm->GetDamageModifier(attacker, damage);
      if (modifier != 100)
        modifiers[count++] = modifier;
    }

    effective_damage = Damage{ApplyPercentageModifiers(
                                  damage.Value,
                                  std::span{modifiers, count}),
                              damage.DamageTypes};
  }

  // (HP - damage.Value).Clamp(0, MaxHP)(L189)| (L189).
  hp_ = std::clamp(hp_ - effective_damage.Value, 0, max_hp_);

  sim::AttackInfo ai;
  ai.Attacker = attacker;
  ai.Damage = &effective_damage;
  ai.damage_state = DamageState();
  ai.previous_damage_state = old_state;

  for (auto* nd : vec_notify_damage_)
    nd->Damaged(self, ai);
  for (auto* nd : vec_notify_damage_player_)
    nd->Damaged(self, ai);

  if (ai.damage_state != old_state)
    for (auto* nd : vec_notify_damage_state_changed_)
      nd->DamageStateChanged(self, ai);

  if (info_.notify_applied_damage && attacker != nullptr &&
      attacker->IsInWorld() && !attacker->IsDead()) {
    for (auto* nd : attacker->TraitsImplementing<INotifyAppliedDamage>())
      nd->AppliedDamage(*attacker, self, ai);
    for (auto* nd :
         attacker->Owner()->PlayerActor()->TraitsImplementing<INotifyAppliedDamage>())
      nd->AppliedDamage(*attacker, self, ai);
  }

  if (hp_ == 0) {
    for (auto* nd : vec_notify_killed_)
      nd->Killed(self, ai);
    for (auto* nd : vec_notify_killed_player_)
      nd->Killed(self, ai);

    if (RemoveOnDeath)
      self.Dispose();
  }
}

// ———— Kill(L228-231)————
void Health::Kill(Actor& self, Actor* attacker,
                  const core::BitSet<sim::DamageType>& damage_types) {
  InflictDamage(self, attacker, Damage{max_hp_, damage_types}, true);
}

// ———— ITick.Tick(L233-239)————
void Health::Tick(Actor&) {
  if (hp_ >= display_hp_)
    display_hp_ = hp_;
  else
    display_hp_ = (2 * display_hp_ + hp_) / 3;
}

}  // namespace ora::mods
