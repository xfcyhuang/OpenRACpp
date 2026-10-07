// UPSTREAM: OpenRA.Mods.Common/Traits/Armament.cs @b6fc03f L22-431 实现部分
//          (the implementation half)。
#include "mods/armament.hpp"

#include "core/percent_modifiers.hpp"
#include "game/ruleset.hpp"
#include "meta/field_loader.hpp"
#include "meta/generic_record.hpp"
#include "mods/actor_exts.hpp"
#include "mods/body_orientation.hpp"
#include "sim/actor.hpp"
#include "sim/trait_registry.hpp"
#include "sim/world.hpp"

namespace ora::mods {

// ———— 声音门面注入面(Game.Sound.Play 的 World 型;引擎装配层接线,
//      Phase 6 前默认空实现)————
// ———— The sound-facade injection face (Game.Sound.Play's World form;
//      wired by the engine assembly, a no-op by default before Phase 6)
// ————
namespace {
std::function<void(std::span<const std::string>, sim::World&, WPos)>
    fn_sound_play_world_ = nullptr;
}  // namespace

void SetArmamentSoundPlayer(
    std::function<void(std::span<const std::string>, sim::World&, WPos)>
        fn_play) {
  fn_sound_play_world_ = std::move(fn_play);
}

// ———— ArmamentInfoData(工厂解析面:L28-111)————

ArmamentInfoData ArmamentInfoData::Parse(const meta::RecordObject& rec_info,
                                         const sim::World& world) {
  ArmamentInfoData data;

  if (const auto v = sim::RecordFieldString(rec_info, "Name"))
    data.str_name = std::string{*v};
  if (const auto v = sim::RecordFieldString(rec_info, "Weapon"))
    data.str_weapon = std::string{*v};
  if (const auto v = sim::RecordFieldString(rec_info, "Turret"))
    data.str_turret = std::string{*v};
  if (const auto v = sim::RecordFieldInt(rec_info, "FireDelay"))
    data.int4_fire_delay = static_cast<int>(*v);
  if (const auto v = sim::RecordFieldInt(rec_info, "Recoil"))
    data.dist_recoil = WDist{static_cast<std::int32_t>(*v)};
  if (const auto v = sim::RecordFieldInt(rec_info, "RecoilRecovery"))
    data.dist_recoil_recovery = WDist{static_cast<std::int32_t>(*v)};
  if (const auto v = sim::RecordFieldString(rec_info, "MuzzleSequence"))
    data.str_muzzle_sequence = std::string{*v};
  if (const auto v = sim::RecordFieldString(rec_info, "MuzzlePalette"))
    data.str_muzzle_palette = std::string{*v};
  if (const auto v = sim::RecordFieldString(rec_info, "ReloadingCondition"))
    data.str_reloading_condition = std::string{*v};
  if (const auto v = sim::RecordFieldString(rec_info, "Cursor"))
    data.str_cursor = std::string{*v};
  if (const auto v = sim::RecordFieldString(rec_info, "OutsideRangeCursor"))
    data.str_outside_range_cursor = std::string{*v};
  if (const auto v = sim::RecordFieldInt(rec_info, "AmmoUsage"))
    data.int4_ammo_usage = static_cast<int>(*v);

  // LocalOffset(WVec 三元组数组)与 LocalYaw(WAngle 数组)
  // LocalOffset (the WVec-triple array) and LocalYaw (the WAngle array).
  if (const auto* generated =
          dynamic_cast<const meta::GeneratedRecord*>(&rec_info)) {
    const std::vector<const meta::FieldDesc*> fields =
        meta::CollectFields(generated->record_desc());
    for (std::size_t i = 0; i < fields.size(); i++) {
      const std::string_view name = fields[i]->str_name;
      const meta::GenericValue& v = generated->Slot(i);
      if (name == "LocalOffset") {
        if (auto* list = std::get_if<std::vector<meta::GenericValue>>(&v.val))
          for (const auto& element : *list)
            if (auto* tuple = std::get_if<meta::GenericTuple>(&element.val)) {
              data.vec_local_offset.push_back(WVec{
                  static_cast<int>(tuple->arr_ints[0]),
                  static_cast<int>(tuple->arr_ints[1]),
                  static_cast<int>(tuple->arr_ints[2])});
            }
      } else if (name == "LocalYaw") {
        if (auto* list = std::get_if<std::vector<meta::GenericValue>>(&v.val))
          for (const auto& element : *list)
            if (auto* tuple = std::get_if<meta::GenericTuple>(&element.val))
              data.vec_local_yaw.push_back(
                  WAngle{static_cast<std::int32_t>(tuple->arr_ints[0])});
      } else if (name == "TargetRelationships") {
        if (auto* n = std::get_if<std::int64_t>(&v.val))
          data.target_relationships = static_cast<sim::PlayerRelationship>(
              static_cast<std::int32_t>(*n));
      } else if (name == "ForceTargetRelationships") {
        if (auto* n = std::get_if<std::int64_t>(&v.val))
          data.force_target_relationships =
              static_cast<sim::PlayerRelationship>(
                  static_cast<std::int32_t>(*n));
      }
    }
  }

  data.conditional = sim::ConditionalTraitData::Parse(rec_info);

  // RulesetLoaded(L92-110)的武器解析与校验 —— 工厂时点
  // RulesetLoaded's (L92-110) weapon resolution and validation — at
  // factory time.
  std::string weapon_to_lower = data.str_weapon;
  std::transform(weapon_to_lower.begin(), weapon_to_lower.end(),
                 weapon_to_lower.begin(),
                 [](unsigned char c) { return static_cast<char>(
                     std::tolower(c)); });

  const game::WeaponInfo* weapon_info = nullptr;
  for (const auto& [name, weapon] :
       const_cast<sim::World&>(world).Map().Rules().Weapons())
    if (name == weapon_to_lower) {
      weapon_info = weapon.get();
      break;
    }

  if (weapon_info == nullptr)
    throw yaml::YamlException(
        "Weapons Ruleset does not contain an entry '" + weapon_to_lower +
        "'");

  data.weapon_info = weapon_info;
  // ModifiedRange:IRangeModifierInfo 集空 = 上游无修正的同值直通
  // ModifiedRange: the empty IRangeModifierInfo set = upstream's
  // unmodified pass-through.
  data.dist_modified_range = WDist{weapon_info->int4_range};

  if (weapon_info->int4_burst > 1 &&
      weapon_info->vec_burstDelays.size() > 1 &&
      weapon_info->vec_burstDelays.size() !=
          static_cast<std::size_t>(weapon_info->int4_burst - 1))
    throw yaml::YamlException(
        "Weapon '" + weapon_to_lower +
        "' has an invalid number of BurstDelays, must be single entry or "
        "Burst - 1.");

  if (weapon_info->int4_reloadDelay <= 0)
    throw yaml::YamlException("Weapon '" + weapon_to_lower +
                              "' ReloadDelay value must not be equal to or "
                              "lower than 0");

  return data;
}

// ———— Armament(L113-431)————

Armament::Armament(ActorInitializer& init, const ArmamentInfoData& info)
    : ConditionalTraitCore<Armament>(info.conditional),
      info_(info),
      p_self_(&init.Self()),
      int4_barrel_count_(0) {
  // L141-165
  ActorPtr = &init.Self();

  Weapon = info_.weapon_info;
  Burst = Weapon->int4_burst;

  for (std::size_t i = 0; i < info_.vec_local_offset.size(); i++) {
    Barrel barrel;
    barrel.Offset = info_.vec_local_offset[i];
    barrel.Yaw = info_.vec_local_yaw.size() > i ? info_.vec_local_yaw[i]
                                                : WAngle{0};
    vec_barrels.push_back(barrel);
  }

  if (vec_barrels.empty()) {
    Barrel barrel;
    barrel.Offset = WVec{};
    barrel.Yaw = WAngle{0};
    vec_barrels.push_back(barrel);
  }

  const_cast<int&>(int4_barrel_count_) =
      static_cast<int>(vec_barrels.size());
}

WDist Armament::MaxRange() {
  // L167-170(OPT-A9:成员缓冲回填 + span 直传,零分配)
  // L167-170 (OPT-A9: the member buffer refilled + span-passed, zero
  // allocation).
  static std::vector<int> scratch;
  scratch.clear();
  for (auto* modifier : vec_range_trait_ptrs_)
    scratch.push_back(modifier->GetRangeModifier());
  return WDist{ApplyPercentageModifiers(
      Weapon->int4_range, std::span<const int>{scratch})};
}

void Armament::Created(Actor& self) {
  // L172-186(Turreted/Hovers 查询空集恒 null —— COVERAGE 登记)
  // L172-186 (the Turreted/Hovers queries stay null on the empty set —
  // registered in COVERAGE).
  p_turret_ = nullptr;
  p_hovers_ = nullptr;
  coords_ = self.Trait<BodyOrientation>();
  vec_notify_burst_complete_ =
      self.TraitsImplementing<sim::INotifyBurstComplete>();
  vec_notify_attacks_.clear();
  for (auto* a : self.TraitsImplementing<sim::INotifyAttack>())
    vec_notify_attacks_.emplace_back(&self, a);

  // OPT-A9:一次性解析修正 trait 指针(上游 Lazy IEnumerable 的缓存面)
  // OPT-A9: the modifier trait pointers resolved once (the cached face of
  // upstream's Lazy IEnumerable).
  vec_range_trait_ptrs_ = self.TraitsImplementing<sim::IRangeModifier>();
  vec_reload_trait_ptrs_ = self.TraitsImplementing<sim::IReloadModifier>();
  vec_damage_trait_ptrs_ =
      self.TraitsImplementing<sim::IFirepowerModifier>();
  vec_inaccuracy_trait_ptrs_ =
      self.TraitsImplementing<sim::IInaccuracyModifier>();

  // ConditionalTrait.Created
  CoreCreated(self);
}

void Armament::UpdateCondition(Actor& self) {
  // L198-209
  if (info_.str_reloading_condition.empty())
    return;

  const bool enabled = !IsTraitDisabled() && IsReloading();

  if (enabled && int4_condition_token_ == Actor::InvalidConditionToken)
    int4_condition_token_ =
        self.GrantCondition(info_.str_reloading_condition);
  else if (!enabled && int4_condition_token_ != Actor::InvalidConditionToken)
    int4_condition_token_ = self.RevokeCondition(int4_condition_token_);
}

void Armament::Tick(Actor& self) {
  // L211-242(protected Tick;ITick 委托)
  // L211-242 (the protected Tick; ITick delegates).
  // We need to disable conditions if IsTraitDisabled is true, so we have
  // to update conditions before the return below.
  UpdateCondition(self);

  if (IsTraitDisabled())
    return;

  if (int4_ticks_since_last_shot_ < Weapon->int4_reloadDelay)
    ++int4_ticks_since_last_shot_;

  if (FireDelay > 0)
    --FireDelay;

  Recoil = WDist{std::max(0, Recoil.Length -
                           info_.dist_recoil_recovery.Length)};

  for (auto& action : vec_delayed_actions_) {
    if (--action.ticks <= 0)
      RunDelayedAction(self, action);
  }

  std::erase_if(vec_delayed_actions_,
                [](const DelayedAction& a) { return a.ticks <= 0; });
}

void Armament::ScheduleDelayedAction(int t, int b, DelayedAction action) {
  // L244-250
  if (t > 0) {
    action.ticks = t;
    action.burst = b;
    vec_delayed_actions_.push_back(std::move(action));
  } else {
    RunDelayedAction(*p_self_, action);
  }
}

bool Armament::CanFire(Actor* self, const sim::Target& target) {
  // L252-268
  if (IsReloading() || ConditionalTraitCore<Armament>::IsTraitPaused())
    return false;

  // turret 未移植:turret != null 的 HasAchievedDesiredFacing 前置跳过
  // (空集等价;COVERAGE 登记)
  // Turret unported: the turret != null HasAchievedDesiredFacing
  // precondition skips (the empty-set equivalent; registered in COVERAGE).
  if (!target.IsInRange(self->CenterPosition(), MaxRange()) ||
      (Weapon->int4_minRange != 0 &&
       target.IsInRange(self->CenterPosition(),
                        WDist{Weapon->int4_minRange})))
    return false;

  if (!sim::WeaponIsValidAgainst(*Weapon, target, self->world(), self))
    return false;

  return true;
}

bool Armament::CheckFire(Actor* self, sim::IFacing* facing,
                         const sim::Target& target) {
  // L272-294
  if (!CanFire(self, target))
    return false;

  if (int4_ticks_since_last_shot_ >= Weapon->int4_reloadDelay)
    Burst = Weapon->int4_burst;

  int4_ticks_since_last_shot_ = 0;
  do {
    // If Weapon.Burst == 1, cycle through all LocalOffsets, otherwise use
    // the offset corresponding to current Burst
    int4_current_barrel_ %= int4_barrel_count_;
    const std::size_t barrel_index =
        Weapon->int4_burst == 1
            ? static_cast<std::size_t>(int4_current_barrel_)
            : static_cast<std::size_t>(
                  Burst % static_cast<int>(vec_barrels.size()));
    const Barrel& barrel = vec_barrels[barrel_index];
    int4_current_barrel_++;

    FireBarrel(self, facing, target, barrel);
    UpdateBurst(self, target);
  } while (FireDelay == 0 && CanFire(self, target));

  return true;
}

void Armament::FireBarrel(Actor* self, sim::IFacing* facing,
                          const sim::Target& target, const Barrel& barrel) {
  // L296-363
  (void)facing;
  for (auto& [notify_actor, notify] : vec_notify_attacks_)
    notify->PreparingAttack(*notify_actor, target, *this, barrel);

  const WPos muzzle_position = self->CenterPosition() +
                               MuzzleOffset(self, barrel);
  const WAngle muzzle_facing = MuzzleOrientation(self, barrel).Yaw;
  const WRot muzzle_orientation = WRot::FromYaw(muzzle_facing);

  WPos passive_target =
      Weapon->b_targetActorCenter
          ? target.CenterPosition()
          : ClosestToIgnoringPath(target.Positions(), muzzle_position);

  // FirstBurstTargetOffset(tuple_firstBurstTargetOffset = WVec 三元组)
  // FirstBurstTargetOffset (tuple_firstBurstTargetOffset = the WVec
  // triple).
  const auto& fbo = Weapon->tuple_firstBurstTargetOffset.arr_ints;
  const WVec initial_offset{static_cast<int>(fbo[0]),
                            static_cast<int>(fbo[1]),
                            static_cast<int>(fbo[2])};
  if (initial_offset != WVec{}) {
    // We want this to match Armament.LocalOffset, so we need to convert it
    // to forward, right, up
    const WVec converted{initial_offset.Y, -initial_offset.X,
                         initial_offset.Z};
    passive_target = passive_target + converted.Rotate(muzzle_orientation);
  }

  const auto& fbo2 = Weapon->tuple_followingBurstTargetOffset.arr_ints;
  const WVec following_offset{static_cast<int>(fbo2[0]),
                              static_cast<int>(fbo2[1]),
                              static_cast<int>(fbo2[2])};
  if (following_offset != WVec{}) {
    // We want this to match Armament.LocalOffset, so we need to convert it
    // to forward, right, up
    const WVec converted{following_offset.Y, -following_offset.X,
                         following_offset.Z};
    passive_target =
        passive_target +
        ((Weapon->int4_burst - Burst) * converted).Rotate(muzzle_orientation);
  }

  sim::ProjectileArgs args;
  args.weapon = Weapon;
  args.facing = muzzle_facing;
  args.fn_current_muzzle_facing = [self, this, &barrel]() {
    return MuzzleOrientation(self, barrel).Yaw;
  };

  // OPT-A9:成员缓冲回填(上游 damageModifiers.ToArray() 的零分配面)
  // OPT-A9: the member buffers refilled (the zero-allocation face of
  // upstream's damageModifiers.ToArray()).
  static thread_local std::vector<int> damage_scratch;
  damage_scratch.clear();
  for (auto* modifier : vec_damage_trait_ptrs_)
    damage_scratch.push_back(modifier->GetFirepowerModifier());
  args.vec_damage_modifiers = damage_scratch;

  static thread_local std::vector<int> inaccuracy_scratch;
  inaccuracy_scratch.clear();
  for (auto* modifier : vec_inaccuracy_trait_ptrs_)
    inaccuracy_scratch.push_back(modifier->GetInaccuracyModifier());
  args.vec_inaccuracy_modifiers = inaccuracy_scratch;

  static thread_local std::vector<int> range_scratch;
  range_scratch.clear();
  for (auto* modifier : vec_range_trait_ptrs_)
    range_scratch.push_back(modifier->GetRangeModifier());
  args.vec_range_modifiers = range_scratch;

  args.source = muzzle_position;
  args.fn_current_source = [self, this, &barrel]() {
    return self->CenterPosition() + MuzzleOffset(self, barrel);
  };
  args.source_actor = self;
  args.passive_target = passive_target;
  args.guided_target = target;

  // Lambdas can't use 'in' variables, so capture a copy for later
  const sim::Target delayed_target = target;
  DelayedAction action;
  action.kind = DelayedAction::Kind::FireProjectile;
  action.burst = Burst;
  action.args = std::move(args);
  action.barrel = &barrel;
  action.delayed_target_copy = delayed_target;
  ScheduleDelayedAction(info_.int4_fire_delay, Burst, std::move(action));
}

void Armament::UpdateBurst(Actor* self, const sim::Target& target) {
  // L365-392
  if (--Burst > 0) {
    if (Weapon->vec_burstDelays.size() == 1)
      FireDelay = Weapon->vec_burstDelays[0];
    else
      FireDelay = Weapon->vec_burstDelays[Weapon->int4_burst -
                                            (Burst + 1)];
  } else {
    // OPT-A9:reloadModifiers 的成员缓冲回填
    // OPT-A9: the reloadModifiers member-buffer refill.
    static thread_local std::vector<int> reload_scratch;
    reload_scratch.clear();
    for (auto* modifier : vec_reload_trait_ptrs_)
      reload_scratch.push_back(modifier->GetReloadModifier());
    FireDelay = ApplyPercentageModifiers(
        Weapon->int4_reloadDelay, std::span<const int>{reload_scratch});
    if (FireDelay <= 0)
      FireDelay = 1;

    Burst = Weapon->int4_burst;

    if (!Weapon->vec_afterFireSound.empty()) {
      DelayedAction action;
      action.kind = DelayedAction::Kind::PlayAfterFireSound;
      ScheduleDelayedAction(Weapon->int4_afterFireSoundDelay, Burst,
                            std::move(action));
    }

    for (auto* nbc : vec_notify_burst_complete_)
      nbc->FiredBurst(*self, target, *this);
  }
}

void Armament::RunDelayedAction(Actor& self, DelayedAction& action) {
  // FireBarrel 的延迟发射段(L343-362)/ UpdateBurst 的延迟音效段
  // (L383-387)—— 上游闭包体的展开
  // FireBarrel's delayed firing section (L343-362) / UpdateBurst's delayed
  // sound section (L383-387) — the expansion of upstream's closure bodies.
  if (action.kind == DelayedAction::Kind::FireProjectile) {
    sim::IProjectileInfo* projectile_info = nullptr;
    if (Weapon->rec_projectile != nullptr)
      projectile_info = sim::ResolveProjectileInfo(
          *Weapon->rec_projectile);

    if (projectile_info != nullptr) {
      sim::IProjectile* projectile = projectile_info->Create(action.args);
      if (projectile != nullptr)
        self.world().Add(std::unique_ptr<sim::IEffect>(
            static_cast<sim::IEffect*>(projectile)));

      if (!Weapon->vec_report.empty() && fn_sound_play_world_)
        fn_sound_play_world_(Weapon->vec_report, self.world(),
                             self.CenterPosition());

      if (action.burst == Weapon->int4_burst &&
          !Weapon->vec_startBurstReport.empty() && fn_sound_play_world_)
        fn_sound_play_world_(Weapon->vec_startBurstReport, self.world(),
                             self.CenterPosition());

      Recoil = info_.dist_recoil;
    }

    for (auto& [notify_actor, notify] : vec_notify_attacks_)
      notify->Attacking(*notify_actor, action.delayed_target_copy, *this,
                        *action.barrel);
  } else {
    if (!Weapon->vec_afterFireSound.empty() && fn_sound_play_world_)
      fn_sound_play_world_(Weapon->vec_afterFireSound, self.world(),
                           self.CenterPosition());
  }
}

WVec Armament::MuzzleOffset(Actor* self, const Barrel& b) {
  // L396-399
  return CalculateMuzzleOffset(self, b);
}

WVec Armament::CalculateMuzzleOffset(Actor* self, const Barrel& b) {
  // L401-418
  // Weapon offset in turret coordinates
  WVec local_offset = b.Offset + WVec{-static_cast<int>(Recoil.Length), 0,
                                      0};

  // Hovers 未移植:hovers != null 的 WorldVisualOffset 跳过(空集等价)
  // Hovers unported: the hovers != null WorldVisualOffset skips (the
  // empty-set equivalent).

  // Turret coordinates to body coordinates
  const WRot body_orientation =
      coords_->QuantizeOrientation(self->Orientation());
  // turret 未移植:localOffset = localOffset.Rotate(turret.WorldOrientation)
  // + turret.Offset.Rotate(bodyOrientation) 分支跳过
  // Turret unported: the localOffset =
  // localOffset.Rotate(turret.WorldOrientation) +
  // turret.Offset.Rotate(bodyOrientation) branch skips.
  local_offset = local_offset.Rotate(body_orientation);

  // Body coordinates to world coordinates
  return coords_->LocalToWorld(local_offset);
}

WRot Armament::MuzzleOrientation(Actor* self, const Barrel& b) {
  // L420-423
  return CalculateMuzzleOrientation(self, b);
}

WRot Armament::CalculateMuzzleOrientation(Actor* self, const Barrel& b) {
  // L425-428(turret ? turret.WorldOrientation : self.Orientation;
  // turret 恒 null → self.Orientation)
  // L425-428 (turret ? turret.WorldOrientation : self.Orientation;
  // turret stays null → self.Orientation).
  return WRot::FromYaw(b.Yaw).Rotate(self->Orientation());
}

}  // namespace ora::mods
