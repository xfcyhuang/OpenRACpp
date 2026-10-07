// UPSTREAM: OpenRA.Mods.Common/Projectiles/Bullet.cs + InstantHit.cs
//          (实现部分) | The implementation half.
#include "mods/projectiles.hpp"

#include "gfx/animation.hpp"
#include "meta/field_loader.hpp"
#include "meta/generic_record.hpp"
#include "mods/blocks_projectiles.hpp"
#include "mods/missile_projectiles.hpp"
#include "mods/hit_shape.hpp"
#include "sim/actor.hpp"
#include "sim/player.hpp"
#include "sim/world.hpp"

namespace ora::mods {

namespace {

std::function<gfx::Animation*(sim::World&, const std::string&,
                              const std::string&, std::function<WAngle()>)>
    fn_bullet_animation_factory_ = nullptr;

std::function<void(const std::string&, sim::World&, const WPos&)>
    fn_projectile_sound_play_ = nullptr;

/// 字符串数组槽读取 | the string-array slot reader.
std::vector<std::string> RecordFieldStringArray(
    const meta::RecordObject& rec, std::string_view str_name) {
  std::vector<std::string> vec_out;
  if (const auto* generated =
          dynamic_cast<const meta::GeneratedRecord*>(&rec)) {
    const std::vector<const meta::FieldDesc*> fields =
        meta::CollectFields(generated->record_desc());
    for (std::size_t i = 0; i < fields.size(); i++)
      if (fields[i]->str_name == str_name) {
        const meta::GenericValue& v = generated->Slot(i);
        if (auto* list = std::get_if<std::vector<meta::GenericValue>>(
                &v.val))
          for (const auto& element : *list)
            if (auto* s = std::get_if<std::string>(&element.val))
              vec_out.push_back(*s);
      }
  }
  return vec_out;
}

/// int 数组槽读取(WDist/WAngle 族) | the int-array slot reader (the
/// WDist/WAngle families).
std::vector<std::int64_t> RecordFieldIntArray(const meta::RecordObject& rec,
                                              std::string_view str_name) {
  std::vector<std::int64_t> vec_out;
  if (const auto* generated =
          dynamic_cast<const meta::GeneratedRecord*>(&rec)) {
    const std::vector<const meta::FieldDesc*> fields =
        meta::CollectFields(generated->record_desc());
    for (std::size_t i = 0; i < fields.size(); i++)
      if (fields[i]->str_name == str_name) {
        const meta::GenericValue& v = generated->Slot(i);
        if (auto* list = std::get_if<std::vector<meta::GenericValue>>(
                &v.val))
          for (const auto& element : *list)
            if (auto* n = std::get_if<std::int64_t>(&element.val))
              vec_out.push_back(*n);
      }
  }
  return vec_out;
}

std::optional<std::int64_t> RecordFieldIntOf(const meta::RecordObject& rec,
                                             std::string_view str_name) {
  if (const auto* generated =
          dynamic_cast<const meta::GeneratedRecord*>(&rec)) {
    const std::vector<const meta::FieldDesc*> fields =
        meta::CollectFields(generated->record_desc());
    for (std::size_t i = 0; i < fields.size(); i++)
      if (fields[i]->str_name == str_name) {
        const meta::GenericValue& v = generated->Slot(i);
        if (auto* n = std::get_if<std::int64_t>(&v.val))
          return *n;
      }
  }
  return std::nullopt;
}

std::optional<std::string> RecordFieldStringOf(
    const meta::RecordObject& rec, std::string_view str_name) {
  if (const auto* generated =
          dynamic_cast<const meta::GeneratedRecord*>(&rec)) {
    const std::vector<const meta::FieldDesc*> fields =
        meta::CollectFields(generated->record_desc());
    for (std::size_t i = 0; i < fields.size(); i++)
      if (fields[i]->str_name == str_name) {
        const meta::GenericValue& v = generated->Slot(i);
        if (auto* s = std::get_if<std::string>(&v.val))
          return *s;
      }
  }
  return std::nullopt;
}

/// ImmutableArrayExtensions.Random(world.SharedRandom) 的等价 RNG 消耗
/// The equivalent RNG consumption of ImmutableArrayExtensions.Random(
/// world.SharedRandom).
std::string RandomOf(sim::World& world,
                     const std::vector<std::string>& vec_xs) {
  if (vec_xs.empty())
    return {};
  const std::int32_t index = world.SharedRandom().Next(
      0, static_cast<std::int32_t>(vec_xs.size()));
  return vec_xs[static_cast<std::size_t>(index)];
}

}  // namespace

void SetBulletAnimationFactory(
    std::function<gfx::Animation*(sim::World&, const std::string&,
                                  const std::string&,
                                  std::function<WAngle()>)> fn_factory) {
  fn_bullet_animation_factory_ = std::move(fn_factory);
}

void SetProjectileSoundPlayer(
    std::function<void(const std::string&, sim::World&, const WPos&)>
        fn_play) {
  fn_projectile_sound_play_ = std::move(fn_play);
}

gfx::Animation* MakeBulletAnimation(
    sim::World& world, const std::string& str_image,
    const std::string& str_sequence, std::function<WAngle()> fn_facing) {
  if (fn_bullet_animation_factory_ == nullptr)
    return nullptr;
  return fn_bullet_animation_factory_(world, str_image, str_sequence,
                                      std::move(fn_facing));
}

// ———— BulletInfoData::Parse ————

BulletInfoData BulletInfoData::Parse(const meta::RecordObject& rec_info) {
  BulletInfoData data;

  {
    const std::vector<std::int64_t> vec_speed =
        RecordFieldIntArray(rec_info, "Speed");
    if (!vec_speed.empty()) {
      data.vec_speed.clear();
      for (const std::int64_t v : vec_speed)
        data.vec_speed.push_back(WDist{static_cast<std::int32_t>(v)});
    }
  }
  if (const auto v = RecordFieldIntOf(rec_info, "Inaccuracy"))
    data.dist_inaccuracy = WDist{static_cast<std::int32_t>(*v)};
  if (const auto v = RecordFieldIntOf(rec_info, "InaccuracyType"))
    data.inaccuracy_type =
        static_cast<InaccuracyType>(static_cast<std::int32_t>(*v));
  if (const auto v = RecordFieldStringOf(rec_info, "Image"))
    data.str_image = *v;
  {
    const std::vector<std::string> vec_sequences =
        RecordFieldStringArray(rec_info, "Sequences");
    if (!vec_sequences.empty())
      data.vec_sequences = vec_sequences;
  }
  if (const auto v = RecordFieldStringOf(rec_info, "Palette"))
    data.str_palette = *v;
  if (const auto v = RecordFieldIntOf(rec_info, "IsPlayerPalette"))
    data.b_is_player_palette = *v != 0;
  if (const auto v = RecordFieldIntOf(rec_info, "Shadow"))
    data.b_shadow = *v != 0;
  if (const auto v = RecordFieldIntOf(rec_info, "ShadowColor"))
    data.shadow_color =
        core::Color::FromArgbRaw(static_cast<std::uint32_t>(*v));
  if (const auto v = RecordFieldStringOf(rec_info, "TrailImage"))
    data.str_trail_image = *v;
  {
    const std::vector<std::string> vec_trail =
        RecordFieldStringArray(rec_info, "TrailSequences");
    if (!vec_trail.empty())
      data.vec_trail_sequences = vec_trail;
  }
  if (const auto v = RecordFieldIntOf(rec_info, "TrailInterval"))
    data.int4_trail_interval = static_cast<int>(*v);
  if (const auto v = RecordFieldIntOf(rec_info, "TrailDelay"))
    data.int4_trail_delay = static_cast<int>(*v);
  if (const auto v = RecordFieldStringOf(rec_info, "TrailPalette"))
    data.str_trail_palette = *v;
  if (const auto v = RecordFieldIntOf(rec_info, "TrailUsePlayerPalette"))
    data.b_trail_use_player_palette = *v != 0;
  if (const auto v = RecordFieldIntOf(rec_info, "Blockable"))
    data.b_blockable = *v != 0;
  if (const auto v = RecordFieldIntOf(rec_info, "Width"))
    data.dist_width = WDist{static_cast<std::int32_t>(*v)};
  {
    const std::vector<std::int64_t> vec_angles =
        RecordFieldIntArray(rec_info, "LaunchAngle");
    if (!vec_angles.empty()) {
      data.vec_launch_angle.clear();
      for (const std::int64_t v : vec_angles)
        data.vec_launch_angle.push_back(
            WAngle{static_cast<std::int32_t>(v)});
    }
  }
  if (const auto v = RecordFieldIntOf(rec_info, "BounceCount"))
    data.int4_bounce_count = static_cast<int>(*v);
  if (const auto v = RecordFieldIntOf(rec_info, "BounceRangeModifier"))
    data.int4_bounce_range_modifier = static_cast<int>(*v);
  if (const auto v = RecordFieldStringOf(rec_info, "BounceSound"))
    data.str_bounce_sound = *v;
  data.vec_invalid_bounce_terrain =
      RecordFieldStringArray(rec_info, "InvalidBounceTerrain");
  if (const auto v =
          RecordFieldIntOf(rec_info, "ValidBounceBlockerRelationships"))
    data.valid_bounce_blocker_relationships =
        static_cast<sim::PlayerRelationship>(
            static_cast<std::int32_t>(*v));
  if (const auto v = RecordFieldIntOf(rec_info, "AirburstAltitude"))
    data.dist_airburst_altitude = WDist{static_cast<std::int32_t>(*v)};
  if (const auto v = RecordFieldIntOf(rec_info, "ContrailLength"))
    data.int4_contrail_length = static_cast<int>(*v);
  if (const auto v = RecordFieldIntOf(rec_info, "ContrailDelay"))
    data.int4_contrail_delay = static_cast<int>(*v);
  if (const auto v = RecordFieldIntOf(rec_info, "ContrailZOffset"))
    data.int4_contrail_z_offset = static_cast<int>(*v);
  if (const auto v = RecordFieldIntOf(rec_info, "ContrailStartWidth"))
    data.dist_contrail_start_width =
        WDist{static_cast<std::int32_t>(*v)};
  if (const auto v = RecordFieldIntOf(rec_info, "ContrailEndWidth"))
    data.opt_contrail_end_width = WDist{static_cast<std::int32_t>(*v)};
  if (const auto v = RecordFieldIntOf(rec_info, "ContrailStartColor"))
    data.contrail_start_color =
        core::Color::FromArgbRaw(static_cast<std::uint32_t>(*v));
  if (const auto v =
          RecordFieldIntOf(rec_info, "ContrailStartColorUsePlayerColor"))
    data.b_contrail_start_color_use_player_color = *v != 0;
  if (const auto v = RecordFieldIntOf(rec_info, "ContrailStartColorAlpha"))
    data.int4_contrail_start_color_alpha = static_cast<int>(*v);
  if (const auto v = RecordFieldIntOf(rec_info, "ContrailEndColor"))
    data.opt_contrail_end_color =
        core::Color::FromArgbRaw(static_cast<std::uint32_t>(*v));
  if (const auto v =
          RecordFieldIntOf(rec_info, "ContrailEndColorUsePlayerColor"))
    data.b_contrail_end_color_use_player_color = *v != 0;
  if (const auto v = RecordFieldIntOf(rec_info, "ContrailEndColorAlpha"))
    data.int4_contrail_end_color_alpha = static_cast<int>(*v);

  return data;
}

// ———— Bullet(L146-398)————
// ———— Bullet (L146-398) ————

/// Bullet(Bullet.cs L146-398;ISync {pos,lastPos,target,source})
class Bullet final : public sim::IProjectile, public sim::ISync {
 public:
  Bullet(const BulletInfoData& info, sim::ProjectileArgs& args);

  void Tick(sim::World& world) override;

  /// gen/sync_gen.cpp Bullet 成员表的哈希(dynamic_cast 分派消费)
  /// The hash of gen/sync_gen.cpp's Bullet member table (consumed by the
  /// dynamic_cast dispatch).
  static int SyncHashOf(const sim::ISync* s);

 private:
  /// L232-245:GetEffectiveFacing | L232-245: GetEffectiveFacing.
  WAngle GetEffectiveFacing() const;

  /// L263-323:ShouldExplode | L263-323: ShouldExplode.
  bool ShouldExplode(sim::World& world);

  /// L366-377:Explode | L366-377: Explode.
  void Explode(sim::World& world);

  /// L379-397:AnyValidTargetsInRadius | L379-397:
  /// AnyValidTargetsInRadius.
  bool AnyValidTargetsInRadius(sim::World& world, const WPos& pos,
                               const WDist& radius, Actor* fired_by,
                               bool check_target_type);

  const BulletInfoData& info_;
  sim::ProjectileArgs args_;
  gfx::Animation* animation_ = nullptr;  // 注入面(缺省 null)
  WAngle angle_facing_;
  WAngle angle_;
  WDist speed_;
  std::string str_trail_palette_;

  // [VerifySync] L161-162
  WPos pos_, last_pos_, target_, source_;

  int int4_length_ = 0;
  int int4_ticks_ = 0, int4_smoke_ticks_ = 0;
  int int4_remaining_bounces_ = 0;
};

Bullet::Bullet(const BulletInfoData& info, sim::ProjectileArgs& args)
    : info_{info}, args_{args} {
  // L170-230
  pos_ = args.source;
  source_ = args.source;

  sim::World& world = args.source_actor->world();

  if (info_.vec_launch_angle.size() > 1)
    angle_ = WAngle{world.SharedRandom().Next(
        info_.vec_launch_angle[0].Angle,
        info_.vec_launch_angle[1].Angle)};
  else
    angle_ = info_.vec_launch_angle[0];

  if (info_.vec_speed.size() > 1)
    speed_ = WDist{world.SharedRandom().Next(info_.vec_speed[0].Length,
                                             info_.vec_speed[1].Length)};
  else
    speed_ = info_.vec_speed[0];

  target_ = args.passive_target;
  if (info_.dist_inaccuracy.Length > 0) {
    const std::int32_t max_inaccuracy_offset =
        GetProjectileInaccuracy(info_.dist_inaccuracy.Length,
                                info_.inaccuracy_type, args);
    target_ = target_ +
              WVec::FromPDF(world.SharedRandom(), 2) *
                  max_inaccuracy_offset / 1024;
  }

  if (info_.dist_airburst_altitude > WDist{0})
    target_ = target_ +
              WVec{0, 0, info_.dist_airburst_altitude.Length};

  angle_facing_ = (target_ - pos_).Yaw();
  int4_length_ = std::max((target_ - pos_).Length() / speed_.Length, 1);

  if (!info_.str_image.empty()) {
    // RNG 消耗序与上游一致(PlayRepeating(Sequences.Random(...)));
    // Animation 仅在注入面就绪时构造
    // The RNG consumption order matches upstream (PlayRepeating(
    // Sequences.Random(...))); the Animation is constructed only when the
    // injection face is wired.
    const std::string str_sequence =
        RandomOf(world, info_.vec_sequences);
    if (fn_bullet_animation_factory_ != nullptr)
      animation_ = fn_bullet_animation_factory_(
          world, info_.str_image, str_sequence,
          [this]() { return GetEffectiveFacing(); });
  }

  str_trail_palette_ = info_.str_trail_palette;
  if (info_.b_trail_use_player_palette)
    str_trail_palette_ += args.source_actor->Owner()->InternalName();

  int4_smoke_ticks_ = info_.int4_trail_delay;
  int4_remaining_bounces_ = info_.int4_bounce_count;
}

WAngle Bullet::GetEffectiveFacing() const {
  // L232-245
  const float at =
      static_cast<float>(int4_ticks_) /
      static_cast<float>(int4_length_ - 1);
  const float attitude =
      angle_.Tan() * (1 - 2 * at) / (4 * 1024);

  const float u = angle_facing_.Angle % 512 / 512.0f;
  const float scale = 2048 * u * (1 - u);

  const int effective = static_cast<int>(
      angle_facing_.Angle < 512
          ? angle_facing_.Angle - scale * attitude
          : angle_facing_.Angle + scale * attitude);

  return WAngle{effective};
}

void Bullet::Tick(sim::World& world) {
  // L247-261
  if (animation_ != nullptr)
    animation_->Tick();

  last_pos_ = pos_;
  pos_ = WPos::LerpQuadratic(source_, target_, angle_, int4_ticks_,
                             int4_length_);

  if (ShouldExplode(world)) {
    // ContrailFader 的帧末生成随渲染批(RNG 无消耗;COVERAGE 登记)
    // The ContrailFader's frame-end spawn rides the render batch (no RNG
    // consumption; registered in COVERAGE).
    Explode(world);
  }
}

bool Bullet::ShouldExplode(sim::World& world) {
  // L263-323
  // Check for walls or other blocking obstacles
  if (info_.b_blockable) {
    WPos blocked_pos;
    if (BlocksProjectiles::AnyBlockingActorsBetween(
            world, args_.source_actor->Owner(), last_pos_, pos_,
            info_.dist_width, blocked_pos)) {
      pos_ = blocked_pos;
      return true;
    }
  }

  if (!info_.str_trail_image.empty() && --int4_smoke_ticks_ < 0) {
    // RNG 消耗保持(SpriteEffect 的视觉面随渲染批;COVERAGE 登记)
    // The RNG consumption preserved (SpriteEffect's visual face rides the
    // render batch; registered in COVERAGE).
    [[maybe_unused]] const std::string str_sequence =
        RandomOf(world, info_.vec_trail_sequences);

    int4_smoke_ticks_ = info_.int4_trail_interval;
  }

  // Contrail 的 Update 随渲染批(无 RNG;COVERAGE 登记)
  // The Contrail update rides the render batch (no RNG; registered in
  // COVERAGE).

  const bool b_flight_length_reached = int4_ticks_++ >= int4_length_;
  const bool b_should_bounce = int4_remaining_bounces_ > 0;

  if (b_flight_length_reached && b_should_bounce) {
    const CPos cell = world.Map().CellContaining(pos_);
    if (!world.Map().Contains(cell))
      return true;

    const std::string& terrain_type =
        world.Map().GetTerrainInfo(cell).Type;
    if (std::find(info_.vec_invalid_bounce_terrain.begin(),
                  info_.vec_invalid_bounce_terrain.end(),
                  terrain_type) !=
        info_.vec_invalid_bounce_terrain.end())
      return true;

    if (AnyValidTargetsInRadius(world, pos_, info_.dist_width,
                                args_.source_actor, true))
      return true;

    target_ = target_ + (pos_ - source_) *
                            info_.int4_bounce_range_modifier / 100;
    const WDist dat = world.Map().DistanceAboveTerrain(target_);
    target_ = target_ + WVec{0, 0, -dat.Length};
    int4_length_ = std::max((target_ - pos_).Length() / speed_.Length, 1);

    int4_ticks_ = 0;
    source_ = pos_;
    if (fn_projectile_sound_play_ != nullptr &&
        !info_.str_bounce_sound.empty())
      fn_projectile_sound_play_(info_.str_bounce_sound, world, source_);
    int4_remaining_bounces_--;
  }

  // Flight length reached / exceeded
  if (b_flight_length_reached && !b_should_bounce)
    return true;

  // Driving into cell with higher height level
  if (world.Map().DistanceAboveTerrain(pos_).Length < 0)
    return true;

  // After first bounce, check for targets each tick
  if (int4_remaining_bounces_ < info_.int4_bounce_count &&
      AnyValidTargetsInRadius(world, pos_, info_.dist_width,
                              args_.source_actor, true))
    return true;

  return false;
}

void Bullet::Explode(sim::World& world) {
  // L366-377
  world.AddFrameEndTask([this](sim::World& w) { w.Remove(this); });

  sim::WarheadArgs warhead_args{args_};
  warhead_args.impact_orientation =
      WRot{WAngle{0}, GetVerticalAngle(last_pos_, pos_), args_.facing};
  warhead_args.impact_position = pos_;

  sim::WeaponImpact(*args_.weapon, sim::Target::FromPos(pos_),
                    warhead_args);
}

bool Bullet::AnyValidTargetsInRadius(sim::World& world, const WPos& pos,
                                     const WDist& radius, Actor* fired_by,
                                     bool check_target_type) {
  // L379-397
  for (Actor* victim : FindActorsOnCircle(world, pos, radius)) {
    if (check_target_type &&
        !sim::Target::FromActor(victim).IsValidFor(fired_by))
      continue;

    if (victim != args_.guided_target.ActorPtr &&
        !sim::HasRelationship(
            info_.valid_bounce_blocker_relationships,
            fired_by->Owner()->RelationshipWith(victim->Owner())))
      continue;

    // If the impact position is within any actor's HitShape, we have a
    // direct hit(上游注释)
    for (sim::ITargetablePositions* target_pos :
         victim->EnabledTargetablePositions())
      if (auto* h = dynamic_cast<HitShape*>(target_pos);
          h != nullptr &&
          h->DistanceFromEdge(*victim, pos).Length <= 0)
        return true;
  }

  return false;
}

int Bullet::SyncHashOf(const sim::ISync* s) {
  // gen/sync_gen.cpp Bullet {pos, lastPos, target, source}(Sync.cs 的
  // WPos 自定义哈希 = HashWPos)
  // gen/sync_gen.cpp's Bullet {pos, lastPos, target, source} (Sync.cs's
  // WPos custom hash = HashWPos).
  const auto* bullet = static_cast<const Bullet*>(s);
  return sim::sync::CombineSyncHash(
      sim::sync::CombineSyncHash(
          sim::sync::CombineSyncHash(
              sim::sync::CombineSyncHash(0,
                                         sim::sync::HashWPos(
                                             bullet->pos_)),
              sim::sync::HashWPos(bullet->last_pos_)),
          sim::sync::HashWPos(bullet->target_)),
      sim::sync::HashWPos(bullet->source_));
}

sim::IProjectile* BulletInfo::Create(sim::ProjectileArgs& args) {
  // L143
  return new Bullet(info_, args);
}

// ———— InstantHit(L45-95)————
// ———— InstantHit (L45-95) ————

/// InstantHit(InstantHit.cs L45-95)
class InstantHit final : public sim::IProjectile {
 public:
  InstantHit(const InstantHitInfoData& info, sim::ProjectileArgs& args)
      : info_{info}, args_{args} {
    // L52-67
    if (args.weapon->b_targetActorCenter)
      target_ = args.guided_target;
    else if (info_.dist_inaccuracy.Length > 0) {
      const std::int32_t max_inaccuracy_offset =
          GetProjectileInaccuracy(info_.dist_inaccuracy.Length,
                                  info_.inaccuracy_type, args);
      const WVec inaccuracy_offset =
          WVec::FromPDF(args.source_actor->world().SharedRandom(), 2) *
          max_inaccuracy_offset / 1024;
      target_ = sim::Target::FromPos(args.passive_target +
                                     inaccuracy_offset);
    } else {
      target_ = sim::Target::FromPos(args.passive_target);
    }
  }

  void Tick(sim::World& world) override {
    // L69-89
    // If GuidedTarget has become invalid due to getting killed the same
    // tick, we need to set target to args.PassiveTarget to prevent
    // target.CenterPosition below from crashing.(上游注释)
    if (target_.Type() == sim::TargetType::Invalid)
      target_ = sim::Target::FromPos(args_.passive_target);

    // Check for blocking actors
    if (info_.b_blockable) {
      WPos blocked_pos;
      if (BlocksProjectiles::AnyBlockingActorsBetween(
              world, args_.source_actor->Owner(), args_.source,
              target_.CenterPosition(), info_.dist_width, blocked_pos))
        target_ = sim::Target::FromPos(blocked_pos);
    }

    sim::WarheadArgs warhead_args{args_};
    warhead_args.impact_orientation =
        WRot{WAngle{0},
             GetVerticalAngle(args_.source, target_.CenterPosition()),
             args_.facing};
    warhead_args.impact_position = target_.CenterPosition();

    sim::WeaponImpact(*args_.weapon, target_, warhead_args);
    world.AddFrameEndTask(
        [this](sim::World& w) { w.Remove(this); });
  }

 private:
  const InstantHitInfoData& info_;
  sim::ProjectileArgs args_;
  sim::Target target_;
};

// ———— InstantHitInfoData::Parse ————

InstantHitInfoData InstantHitInfoData::Parse(
    const meta::RecordObject& rec_info) {
  InstantHitInfoData data;
  if (const auto v = RecordFieldIntOf(rec_info, "Inaccuracy"))
    data.dist_inaccuracy = WDist{static_cast<std::int32_t>(*v)};
  if (const auto v = RecordFieldIntOf(rec_info, "InaccuracyType"))
    data.inaccuracy_type =
        static_cast<InaccuracyType>(static_cast<std::int32_t>(*v));
  if (const auto v = RecordFieldIntOf(rec_info, "Blockable"))
    data.b_blockable = *v != 0;
  if (const auto v = RecordFieldIntOf(rec_info, "Width"))
    data.dist_width = WDist{static_cast<std::int32_t>(*v)};
  if (const auto v = RecordFieldIntOf(rec_info, "BlockerScanRadius"))
    data.dist_blocker_scan_radius = WDist{static_cast<std::int32_t>(*v)};
  return data;
}

sim::IProjectile* InstantHitInfo::Create(sim::ProjectileArgs& args) {
  // L42
  return new InstantHit(info_, args);
}

// ———— 同步效果哈希装配 ————

void InstallCommonSyncEffectHasher(sim::World& world) {
  world.SetSyncEffectHashResolver([](const sim::ISync* s) -> int {
    if (auto* bullet = dynamic_cast<const Bullet*>(s); bullet != nullptr)
      return Bullet::SyncHashOf(bullet);
    if (auto* missile =
            dynamic_cast<const Missile*>(s); missile != nullptr)
      return Missile::SyncHashOf(missile);
    if (auto* bomb =
            dynamic_cast<const GravityBomb*>(s); bomb != nullptr)
      return GravityBomb::SyncHashOf(bomb);
    if (auto* zap = dynamic_cast<const TeslaZap*>(s); zap != nullptr)
      return TeslaZap::SyncHashOf(zap);
    return 0;
  });
}

}  // namespace ora::mods
