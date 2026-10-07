// UPSTREAM: OpenRA.Mods.Common/Traits/Turreted.cs 实现部分
//          The implementation half.
#include "mods/turreted.hpp"

#include "meta/field_loader.hpp"
#include "meta/generic_record.hpp"
#include "mods/attack_follow.hpp"
#include "sim/actor_init.hpp"
#include "mods/body_orientation.hpp"
#include "mods/util.hpp"
#include "sim/sync_hash.hpp"
#include "sim/trait_registry.hpp"
#include "sim/world.hpp"

namespace ora::mods {

// ———— TurretedInfoData(L21-127)————

TurretedInfoData TurretedInfoData::Parse(const meta::RecordObject& rec_info) {
  TurretedInfoData data;

  if (const auto* generated =
          dynamic_cast<const meta::GeneratedRecord*>(&rec_info)) {
    const std::vector<const meta::FieldDesc*> fields =
        meta::CollectFields(generated->record_desc());
    for (std::size_t i = 0; i < fields.size(); i++) {
      const std::string_view name = fields[i]->str_name;
      const meta::GenericValue& v = generated->Slot(i);
      if (name == "Turret") {
        if (auto* s = std::get_if<std::string>(&v.val))
          data.str_turret = *s;
      } else if (name == "TurnSpeed") {
        if (auto* n = std::get_if<std::int64_t>(&v.val))
          data.angle_turn_speed =
              WAngle{static_cast<std::int32_t>(*n)};
      } else if (name == "InitialFacing") {
        if (auto* n = std::get_if<std::int64_t>(&v.val))
          data.angle_initial_facing =
              WAngle{static_cast<std::int32_t>(*n)};
      } else if (name == "RealignDelay") {
        if (auto* n = std::get_if<std::int64_t>(&v.val))
          data.int4_realign_delay = static_cast<int>(*n);
      } else if (name == "Offset") {
        if (auto* list = std::get_if<std::vector<meta::GenericValue>>(
                &v.val)) {
          std::vector<int> vec_xyz;
          for (const auto& element : *list)
            if (auto* n = std::get_if<std::int64_t>(&element.val))
              vec_xyz.push_back(static_cast<int>(*n));
          if (vec_xyz.size() == 3)
            data.vec_offset = WVec{vec_xyz[0], vec_xyz[1], vec_xyz[2]};
        }
      } else if (name == "EditorTurretFacingDisplayOrder") {
        if (auto* n = std::get_if<std::int64_t>(&v.val))
          data.int4_editor_turret_facing_display_order =
              static_cast<int>(*n);
      }
    }
  }

  data.conditional = sim::ConditionalTraitData::Parse(rec_info);
  return data;
}

std::function<WAngle()> TurretedInfoData::WorldFacingFromInit(
    ActorInitializer& init, std::string_view info_instance_name,
    WAngle default_facing) {
  // L58-82
  // (Dynamic)TurretFacingInit is specified relative to the actor body.
  // We need to add the body facing to return an absolute world angle.
  // (上游注释)
  std::function<WAngle()> fn_body_facing;
  if (sim::FacingInit* facing_init =
          init.GetOrDefault<sim::FacingInit>()) {
    const WAngle facing = facing_init->Value();
    fn_body_facing = [facing]() { return facing; };
  }

  if (sim::TurretFacingInit* turret_facing_init =
          init.GetOrDefault<sim::TurretFacingInit>(
              info_instance_name)) {
    const WAngle facing = turret_facing_init->Value();
    if (fn_body_facing != nullptr)
      return [fn_body_facing, facing]() {
        return fn_body_facing() + facing;
      };
    return [facing]() { return facing; };
  }

  if (sim::DynamicTurretFacingInit* dynamic_facing_init =
          init.GetOrDefault<sim::DynamicTurretFacingInit>(
              info_instance_name)) {
    const std::function<WAngle()> dynamic = dynamic_facing_init->Value();
    if (fn_body_facing != nullptr)
      return [fn_body_facing, dynamic]() {
        return fn_body_facing() + dynamic();
      };
    return dynamic;
  }

  if (fn_body_facing != nullptr)
    return fn_body_facing;
  return [default_facing]() { return default_facing; };
}

std::function<WAngle()> TurretedInfoData::LocalFacingFromInit(
    ActorInitializer& init,
    std::string_view info_instance_name) const {
  // L89-103
  if (sim::TurretFacingInit* turret_facing_init =
          init.GetOrDefault<sim::TurretFacingInit>(
              info_instance_name)) {
    const WAngle facing = turret_facing_init->Value();
    return [facing]() { return facing; };
  }

  if (sim::DynamicTurretFacingInit* dynamic_facing_init =
          init.GetOrDefault<sim::DynamicTurretFacingInit>(
              info_instance_name))
    return dynamic_facing_init->Value();

  const WAngle initial = angle_initial_facing;
  return [initial]() { return initial; };
}

// ———— Turreted(L130-314)————

Turreted::Turreted(ActorInitializer& init, const TurretedInfoData& info,
                   std::string str_info_instance_name)
    : sim::ConditionalTraitCore<Turreted>(info.conditional),
      info_{info},
      str_info_instance_name_{std::move(str_info_instance_name)},
      rot_local_orientation_{WRot::None()} {
  // L175-179
  rot_local_orientation_ = WRot::FromYaw(
      info.LocalFacingFromInit(init, str_info_instance_name_)());
}

void Turreted::Created(Actor& self) {
  // L181-187
  // SingleOrDefault:多匹配即上游 Single 抛(文本逐字)
  // SingleOrDefault: a multi-match throws as upstream's Single does (the
  // text verbatim).
  AttackTurreted* match = nullptr;
  int match_count = 0;
  for (AttackTurreted* at : self.TraitsImplementing<AttackTurreted>())
    if (std::find(at->TurretInfo().vec_turrets.begin(),
                  at->TurretInfo().vec_turrets.end(),
                  info_.str_turret) !=
        at->TurretInfo().vec_turrets.end()) {
      match = at;
      match_count++;
    }
  if (match_count > 1)
    throw std::runtime_error(
        "Sequence contains more than one matching element");
  p_attack_ = match;

  p_facing_ = self.TraitOrDefault<sim::IFacing>();
  p_body_ = self.Trait<BodyOrientation>();

  CoreCreated(self);
}

WRot Turreted::WorldOrientation() const {
  // L153-165
  WRot world = p_facing_ != nullptr
                   ? LocalOrientation().Rotate(p_facing_->Orientation())
                   : LocalOrientation();
  if (QuantizedFacings() == 0)
    return world;

  // Quantize orientation to match a rendered sprite
  // Implies no pitch or roll(上游注释)
  return WRot::FromYaw(
      p_body_->QuantizeFacing(world.Yaw, QuantizedFacings()));
}

void Turreted::Tick(Actor& self) {
  // L194-224
  if (IsTraitDisabled())
    return;

  // NOTE: FaceTarget is called in AttackTurreted.CanAttack if the turret
  // has a target.(上游注释)
  if (p_attack_ != nullptr) {
    // Only realign while not attacking anything(上游注释)
    if (p_attack_->IsAiming()) {
      int4_realign_tick_ = 0;
      return;
    }

    if (int4_realign_tick_ < info_.int4_realign_delay)
      int4_realign_tick_++;
    else if (info_.int4_realign_delay > -1) {
      b_realign_desired_ = true;
      vec_desired_direction_ = WVec{0, 0, 0};
    }

    MoveTurret();
  } else {
    int4_realign_tick_ = 0;
    MoveTurret();
  }
}

WAngle Turreted::DesiredLocalFacing() const {
  // L226-246
  // A zero value means that we have a target, but it is on top of us
  // (上游注释)
  if (vec_desired_direction_ == WVec{0, 0, 0})
    return LocalOrientation().Yaw;

  if (p_facing_ == nullptr)
    return vec_desired_direction_.Yaw();

  // PERF: If the turret rotation axis is vertical we can directly take the
  // difference in facing/yaw(上游注释)
  const WRot orientation = p_facing_->Orientation();
  if (orientation.Pitch == WAngle{0} && orientation.Roll == WAngle{0})
    return vec_desired_direction_.Yaw() - orientation.Yaw;

  // If the turret rotation axis is not vertical we must transform the
  // target direction into the turrets local coordinate system(上游注释)
  return vec_desired_direction_.Rotate(-orientation).Yaw();
}

void Turreted::MoveTurret() {
  // L248-261
  const WAngle desired =
      b_realign_desired_ ? info_.angle_initial_facing : DesiredLocalFacing();
  if (desired == LocalOrientation().Yaw)
    return;

  SetLocalOrientation(LocalOrientation().WithYaw(TickFacing(
      LocalOrientation().Yaw, desired, info_.angle_turn_speed)));

  if (desired == LocalOrientation().Yaw) {
    b_realign_desired_ = false;
    vec_desired_direction_ = WVec{0, 0, 0};
  }
}

bool Turreted::FaceTarget(Actor& self, const sim::Target& target) {
  // L263-281
  if (IsTraitDisabled() || IsTraitPaused() || p_attack_ == nullptr ||
      p_attack_->IsTraitDisabled() || p_attack_->IsTraitPaused())
    return false;

  if (target.Type() == sim::TargetType::Invalid) {
    vec_desired_direction_ = WVec{0, 0, 0};
    return false;
  }

  const WPos turret_pos = self.CenterPosition() + Position(self);
  const WPos target_pos = p_attack_->GetTargetPosition(turret_pos, target);
  vec_desired_direction_ = target_pos - turret_pos;
  b_realign_desired_ = false;

  MoveTurret();
  return HasAchievedDesiredFacing();
}

bool Turreted::HasAchievedDesiredFacing() const {
  // L283-290
  const WAngle desired =
      b_realign_desired_ ? info_.angle_initial_facing : DesiredLocalFacing();
  return desired == LocalOrientation().Yaw;
}

WVec Turreted::Position(Actor& self) const {
  // L293-297
  const WRot body_orientation =
      p_body_->QuantizeOrientation(self.Orientation());
  return p_body_->LocalToWorld(Offset().Rotate(body_orientation));
}

void Turreted::ModifyDeathActorInit(Actor& /*self*/,
                                    sim::TypeDictionary& init) {
  // L299-302
  init.Add(new sim::TurretFacingInit(LocalOrientation().Yaw,
                                str_info_instance_name_));
}

void Turreted::ModifyActorPreviewInit(Actor& /*self*/,
                                      sim::TypeDictionary& inits) {
  // L304-307(闭包捕获 this —— actor 生存期内求值,与上游委托目标语义同)
  // L304-307 (the closure captures this — evaluated within the actor's
  // lifetime, the same as upstream's delegate target).
  Turreted* turreted = this;
  inits.Add(new sim::DynamicTurretFacingInit(
      [turreted]() { return turreted->LocalOrientation().Yaw; },
      str_info_instance_name_));
}

void Turreted::TraitDisabledHookImpl(Actor& self) {
  // L309-313
  if (p_attack_ != nullptr && p_attack_->IsAiming())
    p_attack_->OnStopOrder(self);
}

// ———— [VerifySync] 哈希注册(gen/sync_gen.cpp:Turreted {QuantizedFacings})————
// ———— The [VerifySync] hash registration (gen/sync_gen.cpp: Turreted
//      {QuantizedFacings}) ————

namespace {

int TurretedSyncHash(const sim::ISync* s) {
  const auto* turreted = static_cast<const Turreted*>(s);
  return sim::sync::CombineSyncHash(
      0, turreted->QuantizedFacings());
}

const bool b_turreted_sync_registered = [] {
  sim::RegisterSyncHashFunction("OpenRA.Mods.Common.Traits.Turreted",
                                &TurretedSyncHash);
  return true;
}();
[[maybe_unused]] const bool* b_turreted_sync_registered_anchor =
    &b_turreted_sync_registered;

}  // namespace

}  // namespace ora::mods
