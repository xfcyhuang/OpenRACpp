// UPSTREAM: OpenRA.Mods.Common/Projectiles/Missile.cs + GravityBomb.cs +
//          OpenRA.Mods.Cnc/Projectiles/TeslaZap.cs(实现部分)
//          The implementation half.
#include "mods/missile_projectiles.hpp"

#include "core/color.hpp"
#include "core/exts_math.hpp"
#include "gfx/animation.hpp"
#include "meta/field_loader.hpp"
#include "meta/generic_record.hpp"
#include "mods/blocks_projectiles.hpp"
#include "mods/actor_exts.hpp"
#include "mods/hit_shape.hpp"
#include "mods/projectiles.hpp"
#include "sim/actor.hpp"
#include "sim/player.hpp"
#include "sim/world.hpp"

namespace ora::mods {

namespace {

/// int 夹取(Exts.Clamp:val < min ? min : val > max ? max : val)
/// The int clamp (Exts.Clamp: val < min ? min : val > max ? max : val).
inline int ClampInt(int v, int lo, int hi) {
  return v < lo ? lo : (v > hi ? hi : v);
}

/// (sbyte) 强转的负判定(C# 的符号截断语义)
/// The (sbyte)-cast negativity test (C#'s sign-truncating semantics).
inline bool SByteNegative(int v) {
  return static_cast<std::int8_t>(v) < 0;
}

/// int 符号(Math.Sign:C# 语义 0→0)
/// The int sign (Math.Sign: C#'s 0→0 semantics).
inline int SignOf(int v) { return (v > 0) - (v < 0); }

std::optional<std::int64_t> RecInt(const meta::RecordObject& rec,
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
        // bool 载荷(true/false 的整型面;Blockable 等)
        // The bool payload (the integral face of true/false; Blockable
        // and friends).
        if (auto* b = std::get_if<bool>(&v.val))
          return *b ? std::int64_t{1} : std::int64_t{0};
      }
  }
  return std::nullopt;
}

std::optional<std::string> RecString(const meta::RecordObject& rec,
                                     std::string_view str_name) {
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

std::vector<std::string> RecStringArray(const meta::RecordObject& rec,
                                        std::string_view str_name) {
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

/// WVec 槽读取(GenericTuple 三整型分量)
/// The WVec slot read (GenericTuple's three integer components).
std::optional<WVec> RecWVec(const meta::RecordObject& rec,
                            std::string_view str_name) {
  if (const auto* generated =
          dynamic_cast<const meta::GeneratedRecord*>(&rec)) {
    const std::vector<const meta::FieldDesc*> fields =
        meta::CollectFields(generated->record_desc());
    for (std::size_t i = 0; i < fields.size(); i++)
      if (fields[i]->str_name == str_name) {
        const meta::GenericValue& v = generated->Slot(i);
        if (auto* tuple = std::get_if<meta::GenericTuple>(&v.val))
          return WVec{static_cast<std::int32_t>((*tuple).arr_ints[0]),
                      static_cast<std::int32_t>((*tuple).arr_ints[1]),
                      static_cast<std::int32_t>((*tuple).arr_ints[2])};
        return std::nullopt;
      }
  }
  return std::nullopt;
}

/// ImmutableArrayExtensions.Random(SharedRandom) 的等价 RNG 消耗
/// (projectiles.cpp 同形)
/// The equivalent RNG consumption of ImmutableArrayExtensions.Random(
/// SharedRandom) (projectiles.cpp's shape).
std::string RandomOf(sim::World& world,
                     const std::vector<std::string>& vec_xs) {
  if (vec_xs.empty())
    return {};
  const std::int32_t index = world.SharedRandom().Next(
      0, static_cast<std::int32_t>(vec_xs.size()));
  return vec_xs[static_cast<std::size_t>(index)];
}

}  // namespace

// ———— MissileInfoData::Parse ————

MissileInfoData MissileInfoData::Parse(const meta::RecordObject& rec_info) {
  MissileInfoData data;
  if (const auto v = RecString(rec_info, "Image"))
    data.str_image = *v;
  {
    const auto vec = RecStringArray(rec_info, "Sequences");
    if (!vec.empty())
      data.vec_sequences = vec;
  }
  if (const auto v = RecString(rec_info, "Palette"))
    data.str_palette = *v;
  if (const auto v = RecInt(rec_info, "IsPlayerPalette"))
    data.b_is_player_palette = *v != 0;
  if (const auto v = RecInt(rec_info, "Shadow"))
    data.b_shadow = *v != 0;
  if (const auto v = RecInt(rec_info, "ShadowColor"))
    data.shadow_color = core::Color::FromArgbRaw(
        static_cast<std::uint32_t>(*v));
  if (const auto v = RecInt(rec_info, "MinimumLaunchAngle"))
    data.min_launch_angle = WAngle{static_cast<std::int32_t>(*v)};
  if (const auto v = RecInt(rec_info, "MaximumLaunchAngle"))
    data.max_launch_angle = WAngle{static_cast<std::int32_t>(*v)};
  if (const auto v = RecInt(rec_info, "MinimumLaunchSpeed"))
    data.min_launch_speed = WDist{static_cast<std::int32_t>(*v)};
  if (const auto v = RecInt(rec_info, "MaximumLaunchSpeed"))
    data.max_launch_speed = WDist{static_cast<std::int32_t>(*v)};
  if (const auto v = RecInt(rec_info, "Speed"))
    data.speed = WDist{static_cast<std::int32_t>(*v)};
  if (const auto v = RecInt(rec_info, "Acceleration"))
    data.acceleration = WDist{static_cast<std::int32_t>(*v)};
  if (const auto v = RecInt(rec_info, "Arm"))
    data.int4_arm = static_cast<int>(*v);
  if (const auto v = RecInt(rec_info, "Blockable"))
    data.b_blockable = *v != 0;
  if (const auto v = RecInt(rec_info, "TerrainHeightAware"))
    data.b_terrain_height_aware = *v != 0;
  if (const auto v = RecInt(rec_info, "Width"))
    data.dist_width = WDist{static_cast<std::int32_t>(*v)};
  if (const auto v = RecInt(rec_info, "Inaccuracy"))
    data.dist_inaccuracy = WDist{static_cast<std::int32_t>(*v)};
  if (const auto v = RecInt(rec_info, "InaccuracyType"))
    data.inaccuracy_type =
        static_cast<InaccuracyType>(static_cast<std::int32_t>(*v));
  if (const auto v = RecInt(rec_info, "LockOnInaccuracy"))
    data.lock_on_inaccuracy = WDist{static_cast<std::int32_t>(*v)};
  if (const auto v = RecInt(rec_info, "LockOnProbability"))
    data.int4_lock_on_probability = static_cast<int>(*v);
  if (const auto v = RecInt(rec_info, "HorizontalRateOfTurn"))
    data.horizontal_rate_of_turn =
        WAngle{static_cast<std::int32_t>(*v)};
  if (const auto v = RecInt(rec_info, "VerticalRateOfTurn"))
    data.vertical_rate_of_turn = WAngle{static_cast<std::int32_t>(*v)};
  if (const auto v = RecInt(rec_info, "Gravity"))
    data.int4_gravity = static_cast<int>(*v);
  if (const auto v = RecInt(rec_info, "RangeLimit"))
    data.range_limit = WDist{static_cast<std::int32_t>(*v)};
  if (const auto v = RecInt(rec_info, "ExplodeWhenEmpty"))
    data.b_explode_when_empty = *v != 0;
  if (const auto v = RecInt(rec_info, "AirburstAltitude"))
    data.airburst_altitude = WDist{static_cast<std::int32_t>(*v)};
  if (const auto v = RecInt(rec_info, "CruiseAltitude"))
    data.cruise_altitude = WDist{static_cast<std::int32_t>(*v)};
  if (const auto v = RecInt(rec_info, "HomingActivationDelay"))
    data.int4_homing_activation_delay = static_cast<int>(*v);
  if (const auto v = RecString(rec_info, "TrailImage"))
    data.str_trail_image = *v;
  {
    const auto vec = RecStringArray(rec_info, "TrailSequences");
    if (!vec.empty())
      data.vec_trail_sequences = vec;
  }
  if (const auto v = RecString(rec_info, "TrailPalette"))
    data.str_trail_palette = *v;
  if (const auto v = RecInt(rec_info, "TrailUsePlayerPalette"))
    data.b_trail_use_player_palette = *v != 0;
  if (const auto v = RecInt(rec_info, "TrailInterval"))
    data.int4_trail_interval = static_cast<int>(*v);
  if (const auto v = RecInt(rec_info, "TrailWhenDeactivated"))
    data.b_trail_when_deactivated = *v != 0;
  if (const auto v = RecInt(rec_info, "ContrailLength"))
    data.int4_contrail_length = static_cast<int>(*v);
  if (const auto v = RecInt(rec_info, "ContrailDelay"))
    data.int4_contrail_delay = static_cast<int>(*v);
  if (const auto v = RecInt(rec_info, "ContrailZOffset"))
    data.int4_contrail_z_offset = static_cast<int>(*v);
  if (const auto v = RecInt(rec_info, "ContrailStartWidth"))
    data.dist_contrail_start_width =
        WDist{static_cast<std::int32_t>(*v)};
  if (const auto v = RecInt(rec_info, "ContrailEndWidth"))
    data.opt_contrail_end_width = WDist{static_cast<std::int32_t>(*v)};
  if (const auto v = RecInt(rec_info, "ContrailStartColor"))
    data.contrail_start_color = core::Color::FromArgbRaw(
        static_cast<std::uint32_t>(*v));
  if (const auto v = RecInt(rec_info, "ContrailStartColorUsePlayerColor"))
    data.b_contrail_start_color_use_player_color = *v != 0;
  if (const auto v = RecInt(rec_info, "ContrailStartColorAlpha"))
    data.int4_contrail_start_color_alpha = static_cast<int>(*v);
  if (const auto v = RecInt(rec_info, "ContrailEndColor"))
    data.opt_contrail_end_color = core::Color::FromArgbRaw(
        static_cast<std::uint32_t>(*v));
  if (const auto v = RecInt(rec_info, "ContrailEndColorUsePlayerColor"))
    data.b_contrail_end_color_use_player_color = *v != 0;
  if (const auto v = RecInt(rec_info, "ContrailEndColorAlpha"))
    data.int4_contrail_end_color_alpha = static_cast<int>(*v);
  if (const auto v = RecInt(rec_info, "Jammable"))
    data.b_jammable = *v != 0;
  if (const auto v = RecInt(rec_info, "JammedDiversionRange"))
    data.int4_jammed_diversion_range = static_cast<int>(*v);
  if (const auto v = RecString(rec_info, "BoundToTerrainType"))
    data.str_bound_to_terrain_type = *v;
  if (const auto v = RecInt(rec_info, "AllowSnapping"))
    data.b_allow_snapping = *v != 0;
  if (const auto v = RecInt(rec_info, "CloseEnough"))
    data.close_enough = WDist{static_cast<std::int32_t>(*v)};
  return data;
}

// ———— Missile(L193-981)————
// ———— Missile (L193-981) ————

Missile::Missile(const MissileInfoData& info, sim::ProjectileArgs& args)
    : info_{info}, args_{args} {
  // L250-313
  pos_ = args.source;
  int4_h_facing_ = args.facing.Facing();
  gravity_ = WVec{0, 0, -info_.int4_gravity};
  target_position_ = args.passive_target;
  const int limit = info_.range_limit != WDist{0}
                        ? info_.range_limit.Length
                        : args.weapon->int4_range;
  range_limit_ = WDist{ApplyPercentageModifiers(
      limit, args.vec_range_modifiers)};
  min_launch_speed_ = info_.min_launch_speed.Length > -1
                          ? info_.min_launch_speed.Length
                          : info_.speed.Length;
  max_launch_speed_ = info_.max_launch_speed.Length > -1
                          ? info_.max_launch_speed.Length
                          : info_.speed.Length;
  max_speed_ = info_.speed.Length;
  min_launch_angle_ = info_.min_launch_angle;
  max_launch_angle_ = info_.max_launch_angle;

  // Make sure the projectile on being spawned is approximately looking at
  // the correct direction.(上游注释)
  render_facing_ = args.facing;

  sim::World& world = args.source_actor->world();

  if (world.SharedRandom().Next(100) <= info_.int4_lock_on_probability)
    b_lock_on_ = true;

  const int inaccuracy =
      b_lock_on_ && info_.lock_on_inaccuracy.Length > -1
          ? info_.lock_on_inaccuracy.Length
          : info_.dist_inaccuracy.Length;
  if (inaccuracy > 0) {
    const std::int32_t max_inaccuracy_offset = GetProjectileInaccuracy(
        inaccuracy, info_.inaccuracy_type, args);
    offset_ = WVec::FromPDF(world.SharedRandom(), 2) *
              max_inaccuracy_offset / 1024;
  }

  DetermineLaunchSpeedAndAngle(world, int4_speed_, int4_v_facing_);

  velocity_ = WVec{0, -int4_speed_, 0}
                  .Rotate(WRot{WAngle::FromFacing(int4_v_facing_),
                               WAngle{0}, WAngle{0}})
                  .Rotate(WRot{WAngle{0}, WAngle{0},
                               WAngle::FromFacing(int4_h_facing_)});

  if (!info_.str_image.empty()) {
    // RNG 消耗序与上游一致(PlayRepeating(Sequences.Random(...)));
    // Animation 仅在注入面就绪时构造
    // The RNG consumption order matches upstream (PlayRepeating(
    // Sequences.Random(...))); the Animation is constructed only when the
    // injection face is wired.
    const std::string str_sequence = RandomOf(world, info_.vec_sequences);
    animation_ = MakeBulletAnimation(world, info_.str_image, str_sequence,
                                     [this]() { return render_facing_; });
  }

  // Contrail(视觉)不构造 —— RNG 无消耗;COVERAGE 登记
  // The (visual) contrail is not constructed — no RNG consumption;
  // registered in COVERAGE.

  str_trail_palette_ = info_.str_trail_palette;
  if (info_.b_trail_use_player_palette)
    str_trail_palette_ += args.source_actor->Owner()->InternalName();
}

int Missile::LoopRadiusOf(int speed, int rot) {
  // L315-322:loopRadius(w-units)= speed / angular speed;
  // pi = 314/100 → speed * 128 * 100 / (314 * rot)
  // L315-322: loopRadius (w-units) = speed / angular speed;
  // pi = 314/100 → speed * 128 * 100 / (314 * rot).
  return speed * 6400 / (157 * rot);
}

void Missile::DetermineLaunchSpeedAndAngleForIncline(
    int pred_clf_dist, int diff_clf_msl_hgt, int rel_tar_hor_dist,
    int& speed, int& v_facing) {
  // L324-363
  speed = max_launch_speed_;

  // Find smallest vertical facing, for which the missile will be able to
  // climb terrAltDiff w-units within hHeightChange w-units all the while
  // ending the ascent with vertical facing 0(上游注释)
  v_facing = max_launch_angle_.Angle >> 2;

  // Compute minimum speed necessary to both be able to face directly
  // upwards and have enough space to hit the target without passing it by
  // (and thus having to do horizontal loops)(上游注释)
  const int sin_v = WAngle::FromFacing(v_facing).Sin();
  const int min_speed =
      ClampInt(std::min(pred_clf_dist * 1024 / (1024 - sin_v),
                        (rel_tar_hor_dist + pred_clf_dist) * 1024 /
                            (2 * (2048 - sin_v))) *
                   info_.vertical_rate_of_turn.Facing() * 157 / 6400,
               min_launch_speed_, max_launch_speed_);

  if (SByteNegative(v_facing)) {
    speed = min_speed;
  } else if (!WillClimbWithinDistance(v_facing, int4_loop_radius_,
                                      pred_clf_dist, diff_clf_msl_hgt) &&
             !WillClimbAroundInclineTop(v_facing, int4_loop_radius_,
                                        pred_clf_dist, diff_clf_msl_hgt)) {
    // Find highest speed greater than the above minimum that allows the
    // missile to surmount the incline(上游注释)
    const int v_fac = v_facing;
    speed = BisectionSearch(
        min_speed, max_launch_speed_, [&](int spd) {
          const int lp_rds =
              LoopRadiusOf(spd, info_.vertical_rate_of_turn.Facing());
          return WillClimbWithinDistance(v_fac, lp_rds, pred_clf_dist,
                                         diff_clf_msl_hgt) ||
                 WillClimbAroundInclineTop(v_fac, lp_rds, pred_clf_dist,
                                           diff_clf_msl_hgt);
        });
  } else {
    // Find least vertical facing that will allow the missile to climb
    // terrAltDiff w-units within hHeightChange w-units all the while
    // ending the ascent with vertical facing 0(上游注释)
    v_facing = BisectionSearch(
                   std::max(static_cast<std::int8_t>(
                                min_launch_angle_.Angle >> 2),
                            static_cast<std::int8_t>(0)),
                   static_cast<std::int8_t>(max_launch_angle_.Angle >> 2),
                   [&](int v_fac) {
                     return !WillClimbWithinDistance(
                         v_fac, int4_loop_radius_, pred_clf_dist,
                         diff_clf_msl_hgt);
                   }) +
               1;
  }
}

void Missile::DetermineLaunchSpeedAndAngle(sim::World& world, int& speed,
                                           int& v_facing) {
  // L366-408
  speed = max_launch_speed_;
  int4_loop_radius_ =
      LoopRadiusOf(speed, info_.vertical_rate_of_turn.Facing());

  // Compute current distance from target position(上游注释)
  const WVec tar_dist_vec = target_position_ + offset_ - pos_;
  const int rel_tar_hor_dist = tar_dist_vec.HorizontalLength();

  int pred_clf_hgt = 0;
  int pred_clf_dist = 0;
  int last_ht_chg = 0;
  int last_ht = 0;

  if (info_.b_terrain_height_aware)
    InclineLookahead(world, rel_tar_hor_dist, pred_clf_hgt, pred_clf_dist,
                     last_ht_chg, last_ht);

  // Height difference between the incline height and missile height
  // (上游注释)
  const int diff_clf_msl_hgt = pred_clf_hgt - pos_.Z;

  // Incline coming up(上游注释)
  if (info_.b_terrain_height_aware && diff_clf_msl_hgt >= 0 &&
      pred_clf_dist > 0) {
    DetermineLaunchSpeedAndAngleForIncline(pred_clf_dist, diff_clf_msl_hgt,
                                           rel_tar_hor_dist, speed,
                                           v_facing);
  } else if (last_ht != 0) {
    v_facing = std::max(
        static_cast<std::int8_t>(min_launch_angle_.Angle >> 2),
        static_cast<std::int8_t>(0));
    speed = max_launch_speed_;
  } else {
    // Set vertical facing so that the missile faces its target(上游注释)
    const WVec v_dist{-tar_dist_vec.Z, -rel_tar_hor_dist, 0};
    v_facing = static_cast<std::int8_t>(v_dist.Yaw().Facing());

    // Do not accept -1 as valid vertical facing since it is usually a
    // numerical error and will lead to premature descent and crashing
    // into the ground(上游注释)
    if (v_facing == -1)
      v_facing = 0;

    // Make sure the chosen vertical facing adheres to prescribed bounds
    // (上游注释)
    v_facing = ClampInt(v_facing,
                        static_cast<std::int8_t>(
                            min_launch_angle_.Angle >> 2),
                        static_cast<std::int8_t>(
                            max_launch_angle_.Angle >> 2));
  }
}

bool Missile::WillClimbWithinDistance(int v_facing, int loop_radius,
                                      int pred_clf_dist,
                                      int diff_clf_msl_hgt) {
  // L413-427
  // Missile's horizontal distance from loop's center(上游注释)
  const int sin_v = WAngle::FromFacing(v_facing).Sin();
  const int cos_v = WAngle::FromFacing(v_facing).Cos();
  const int miss_dist = loop_radius * sin_v / 1024;

  // Missile's height below loop's top(上游注释)
  const int miss_hgt = loop_radius * (1024 - cos_v) / 1024;

  // Height that would be climbed without changing vertical facing for a
  // horizontal distance hHeightChange - missDist(上游注释)
  const int hgt_chg =
      (pred_clf_dist - miss_dist) * WAngle::FromFacing(v_facing).Tan() /
      1024;

  // Check if total manoeuvre height enough to overcome the incline's
  // height(上游注释)
  return hgt_chg + miss_hgt >= diff_clf_msl_hgt;
}

bool Missile::IsNearInclineTop(int v_facing, int loop_radius,
                               int pred_clf_dist) {
  // L432-435
  return v_facing >= 0 &&
         pred_clf_dist <=
             loop_radius *
                 (1024 - WAngle::FromFacing(v_facing).Sin()) / 1024;
}

bool Missile::WillClimbAroundInclineTop(int v_facing, int loop_radius,
                                        int pred_clf_dist,
                                        int diff_clf_msl_hgt) {
  // L440-451
  // Vector from missile's current position pointing to the loop's center
  // (上游注释)
  const WVec radius =
      WVec{loop_radius, 0, 0}
          .Rotate(WRot{WAngle{0}, WAngle{0},
                       WAngle::FromFacing(std::max(0, 64 - v_facing))});

  // Vector from loop's center to incline top + 64 hardcoded in height
  // buffer zone(上游注释)
  const WVec top_vector =
      WVec{pred_clf_dist, diff_clf_msl_hgt + 64, 0} - radius;

  // Check if incline top inside of the vertical loop(上游注释)
  return top_vector.Length() <= loop_radius;
}

template <class Pred>
int Missile::BisectionSearch(int lower_bound, int upper_bound,
                             Pred test_criterion) {
  // L453-470
  while (upper_bound - lower_bound > 1) {
    const int middle = (upper_bound + lower_bound) / 2;

    if (test_criterion(middle))
      lower_bound = middle;
    else
      upper_bound = middle;
  }

  return lower_bound;
}

void Missile::ChangeSpeed(int sign) {
  // L483-489
  int4_speed_ = ClampInt(
      int4_speed_ + sign * info_.acceleration.Length, 0, max_speed_);

  // Compute the vertical loop radius(上游注释)
  int4_loop_radius_ =
      LoopRadiusOf(int4_speed_, info_.vertical_rate_of_turn.Facing());
}

WVec Missile::FreefallTick() {
  // L491-501
  // Compute the projectile's freefall displacement(上游注释)
  const WVec move = velocity_ + gravity_ / 2;
  velocity_ = velocity_ + gravity_;
  const int vel_ratio = max_speed_ * 1024 / velocity_.Length();
  if (vel_ratio < 1024)
    velocity_ = velocity_ * vel_ratio / 1024;

  return move;
}

void Missile::InclineLookahead(sim::World& world, int dist_check,
                               int& pred_clf_hgt, int& pred_clf_dist,
                               int& last_ht_chg, int& last_ht) {
  // L505-548
  pred_clf_hgt = 0;   // Highest probed terrain height
  pred_clf_dist = 0;  // Distance from highest point
  last_ht_chg = 0;    // Distance from last time the height changes
  last_ht = 0;        // Height just before the last height change

  // NOTE: Might be desired to unhardcode the lookahead step size(上游注释)
  constexpr int kStepSize = 32;
  const WVec step =
      WVec{0, -kStepSize, 0}
          .Rotate(WRot{WAngle{0}, WAngle{0},
                       WAngle::FromFacing(int4_h_facing_)});  // Step vector
                                                             // of length 128

  // Probe terrain ahead of the missile
  // NOTE: Might be desired to unhardcode maximum lookahead distance
  // (上游注释)
  const int max_lookahead_distance = int4_loop_radius_ * 4;
  WPos pos_probe = pos_;
  int cur_dist = 0;
  const int tick_limit =
      std::min(max_lookahead_distance, dist_check) / kStepSize;
  int prev_ht = 0;

  // TODO: Make sure cell on map!!!(上游注释)
  for (int tick = 0; tick <= tick_limit; tick++) {
    pos_probe = pos_probe + step;
    if (!world.Map().Contains(world.Map().CellContaining(pos_probe)))
      break;

    const int ht = world.Map().Height().Get(
                       world.Map().CellContaining(pos_probe)) *
                   512;

    cur_dist += kStepSize;
    if (ht > pred_clf_hgt) {
      pred_clf_hgt = ht;
      pred_clf_dist = cur_dist;
    }

    if (prev_ht != ht) {
      last_ht_chg = cur_dist;
      last_ht = prev_ht;
      prev_ht = ht;
    }
  }
}

int Missile::IncreaseAltitude(int pred_clf_dist, int diff_clf_msl_hgt,
                              int rel_tar_hor_dist, int v_facing) {
  // L550-604
  int desired_v_facing = v_facing;

  // If missile is below incline top height and facing downwards, bring
  // back its vertical facing above zero as soon as possible(上游注释)
  if (SByteNegative(v_facing))
    desired_v_facing = info_.vertical_rate_of_turn.Facing();

  // Missile will climb around incline top if bringing vertical facing
  // down to zero on an arc of radius loopRadius(上游注释)
  else if (IsNearInclineTop(v_facing, int4_loop_radius_, pred_clf_dist) &&
           WillClimbAroundInclineTop(v_facing, int4_loop_radius_,
                                     pred_clf_dist, diff_clf_msl_hgt))
    desired_v_facing = 0;

  // Missile will not climb terrAltDiff w-units within hHeightChange
  // w-units all the while ending the ascent with vertical facing 0
  // (上游注释)
  else if (!WillClimbWithinDistance(v_facing, int4_loop_radius_,
                                    pred_clf_dist, diff_clf_msl_hgt)) {
    // Find smallest vertical facing, attainable in the next tick, for
    // which the missile will be able to climb terrAltDiff w-units within
    // hHeightChange w-units all the while ending the ascent with vertical
    // facing 0(上游注释)
    for (int v_fac = std::min(v_facing +
                                  info_.vertical_rate_of_turn.Facing() - 1,
                              63);
         v_fac >= v_facing; v_fac--)
      if (!WillClimbWithinDistance(v_fac, int4_loop_radius_,
                                   pred_clf_dist, diff_clf_msl_hgt) &&
          !(pred_clf_dist <=
                int4_loop_radius_ *
                    (1024 - WAngle::FromFacing(v_fac).Sin()) / 1024 &&
            WillClimbAroundInclineTop(v_fac, int4_loop_radius_,
                                      pred_clf_dist, diff_clf_msl_hgt))) {
        desired_v_facing = v_fac + 1;
        break;
      }
  }

  // Attained height after ascent as predicted from upper part of incline
  // surmounting manoeuvre(上游注释)
  const int pred_att_hght =
      int4_loop_radius_ *
          (1024 - WAngle::FromFacing(v_facing).Cos()) / 1024 -
      diff_clf_msl_hgt;

  // Should the missile be slowed down in order to make it more
  // maneuverable(上游注释)
  const int sin_v = WAngle::FromFacing(v_facing).Sin();
  const bool slow_down =
      info_.acceleration.Length != 0  // Possible to decelerate
      && ((desired_v_facing != 0      // Lower part of incline surmounting
                                   // manoeuvre
           // Incline will be hit before vertical facing attains 64
           // (上游注释)
           && (pred_clf_dist <=
                   int4_loop_radius_ * (1024 - sin_v) / 1024
               // When evaluating this the incline will be *not* be hit
               // before vertical facing attains 64. At current speed
               // target too close to hit without passing it by(上游注释)
               || rel_tar_hor_dist <=
                      2 * int4_loop_radius_ * (2048 - sin_v) / 1024 -
                          pred_clf_dist))

           ||
           (desired_v_facing == 0  // Upper part of incline surmounting
                                   // manoeuvre
            && rel_tar_hor_dist <=
                   int4_loop_radius_ * sin_v / 1024 +
                       ISqrt(pred_att_hght *
                             (2 * int4_loop_radius_ - pred_att_hght)))
               // Target too close to hit at current speed
           );

  if (slow_down)
    ChangeSpeed(-1);

  return desired_v_facing;
}

int Missile::HomingInnerTick(int pred_clf_dist, int diff_clf_msl_hgt,
                             int rel_tar_hor_dist, int last_ht_chg,
                             int last_ht, int rel_tar_hgt, int v_facing,
                             bool target_passed_by) {
  // L606-777
  int desired_v_facing;

  // Incline coming up -> attempt to reach the incline so that after
  // predClfDist the height above the terrain is positive but as close to 0
  // as possible. Also, never change horizontal facing and never travel
  // backwards. Possible techniques to avoid close cliffs are deceleration,
  // turning as sharply as possible to travel directly upwards and then
  // returning to zero vertical facing as low as possible while still not
  // hitting the high terrain. A last technique (and the preferred one,
  // normally used when the missile hasn't been fired near a cliff) is
  // simply finding the smallest vertical facing that allows for a smooth
  // climb to the new terrain's height and coming in at predClfDist at
  // exactly zero vertical facing(上游注释)
  if (info_.b_terrain_height_aware && diff_clf_msl_hgt >= 0 &&
      !b_allow_pass_by_)
    desired_v_facing =
        IncreaseAltitude(pred_clf_dist, diff_clf_msl_hgt, rel_tar_hor_dist,
                         v_facing);
  else if (rel_tar_hor_dist <= 3 * int4_loop_radius_ ||
           state_ == States::Hitting) {
    // No longer travel at cruise altitude(上游注释)
    state_ = States::Hitting;

    if (last_ht >= target_position_.Z)
      b_allow_pass_by_ = true;

    if (!b_allow_pass_by_ &&
        (last_ht < target_position_.Z || target_passed_by)) {
      // Aim for the target(上游注释)
      const WVec v_dist{-rel_tar_hgt, -rel_tar_hor_dist, 0};
      desired_v_facing =
          static_cast<std::int8_t>(v_dist.HorizontalLengthSquared()) != 0
              ? v_dist.Yaw().Facing()
              : v_facing;

      // Do not accept -1 as valid vertical facing since it is usually a
      // numerical error and will lead to premature descent and crashing
      // into the ground(上游注释)
      if (desired_v_facing == -1)
        desired_v_facing = 0;

      // If the target has been passed by, limit the absolute value of
      // vertical facing by the maximum vertical rate of turn. Do this
      // because the missile will be looping horizontally and thus needs
      // smaller vertical facings so as not to hit the ground prematurely
      // (上游注释)
      if (target_passed_by)
        desired_v_facing =
            ClampInt(desired_v_facing,
                     -info_.vertical_rate_of_turn.Facing(),
                     info_.vertical_rate_of_turn.Facing());
      else if (last_ht == 0) {
        // Before the target is passed by, missile speed should be changed
        // Target's height above loop's center(上游注释)
        const int cos_v = WAngle::FromFacing(v_facing).Cos();
        const int tar_hgt =
            ClampInt(int4_loop_radius_ * cos_v / 1024 - std::abs(rel_tar_hgt),
                     0, int4_loop_radius_);

        // Target's horizontal distance from loop's center(上游注释)
        const int tar_dist = ISqrt(int4_loop_radius_ * int4_loop_radius_ -
                                   tar_hgt * tar_hgt);

        // Missile's horizontal distance from loop's center(上游注释)
        const int sin_v = WAngle::FromFacing(v_facing).Sin();
        const int miss_dist = int4_loop_radius_ * sin_v / 1024;

        // If the current height does not permit the missile to hit the
        // target before passing it by, lower speed. Otherwise, increase
        // speed(上游注释)
        if (rel_tar_hor_dist <=
            tar_dist - SignOf(rel_tar_hgt) * miss_dist)
          ChangeSpeed(-1);
        else
          ChangeSpeed();
      }
    } else if (b_allow_pass_by_ ||
               (last_ht != 0 &&
                rel_tar_hor_dist - last_ht_chg < int4_loop_radius_)) {
      // Only activate this part if target too close to cliff(上游注释)
      b_allow_pass_by_ = true;

      // Vector from missile's current position pointing to the loop's
      // center(上游注释)
      WVec radius =
          WVec{int4_loop_radius_, 0, 0}
              .Rotate(WRot{WAngle{0}, WAngle{0},
                           WAngle::FromFacing(64 - v_facing)});

      // Vector from loop's center to incline top hardcoded in height
      // buffer zone(上游注释)
      WVec edge_vector = WVec{last_ht_chg, last_ht - pos_.Z, 0} - radius;

      if (!target_passed_by) {
        // Climb to critical height(上游注释)
        if (rel_tar_hor_dist > 2 * int4_loop_radius_) {
          // Target's distance from cliff(上游注释)
          int d1 = rel_tar_hor_dist - last_ht_chg;
          if (d1 < 0)
            d1 = 0;
          if (d1 > 2 * int4_loop_radius_)
            return 0;

          // Find critical height at which the missile must be once it is
          // at one loopRadius away from the target(上游注释)
          const int h1 = int4_loop_radius_ -
                         ISqrt(d1 * (2 * int4_loop_radius_ - d1)) -
                         (pos_.Z - last_ht);

          const int cos_v = WAngle::FromFacing(v_facing).Cos();
          if (h1 > int4_loop_radius_ * (1024 - cos_v) / 1024)
            desired_v_facing =
                WAngle::ArcTan(
                    ISqrt(h1 * (2 * int4_loop_radius_ - h1)),
                    int4_loop_radius_ - h1).Angle >>
                2;
          else
            desired_v_facing = 0;

          // TODO: deceleration checks!!!(上游注释)
        } else {
          // Avoid the cliff edge(上游注释)
          if (info_.b_terrain_height_aware &&
              edge_vector.Length() > int4_loop_radius_ &&
              last_ht > target_position_.Z) {
            int v_fac;
            for (v_fac = v_facing + 1;
                 v_fac <=
                 v_facing + info_.vertical_rate_of_turn.Facing() - 1;
                 v_fac++) {
              // Vector from missile's current position pointing to the
              // loop's center(上游注释)
              radius = WVec{int4_loop_radius_, 0, 0}
                           .Rotate(WRot{WAngle{0}, WAngle{0},
                                        WAngle::FromFacing(64 - v_fac)});

              // Vector from loop's center to incline top + 64 hardcoded
              // in height buffer zone(上游注释)
              edge_vector =
                  WVec{last_ht_chg, last_ht - pos_.Z, 0} - radius;
              if (edge_vector.Length() <= int4_loop_radius_)
                break;
            }

            desired_v_facing = v_fac;
          } else {
            // Aim for the target(上游注释)
            const WVec v_dist{-rel_tar_hgt, -rel_tar_hor_dist, 0};
            desired_v_facing =
                static_cast<std::int8_t>(
                    v_dist.HorizontalLengthSquared()) != 0
                    ? v_dist.Yaw().Facing()
                    : v_facing;
            if (desired_v_facing < 0 &&
                info_.vertical_rate_of_turn.Facing() <
                    static_cast<std::int8_t>(v_facing))
              desired_v_facing = 0;
          }
        }
      } else {
        // Aim for the target(上游注释)
        const WVec v_dist{-rel_tar_hgt, rel_tar_hor_dist, 0};
        desired_v_facing =
            static_cast<std::int8_t>(v_dist.HorizontalLengthSquared()) !=
                    0
                ? v_dist.Yaw().Facing()
                : v_facing;
        if (desired_v_facing < 0 &&
            info_.vertical_rate_of_turn.Facing() <
                static_cast<std::int8_t>(v_facing))
          desired_v_facing = 0;
      }
    } else {
      // Aim to attain cruise altitude as soon as possible while having
      // the absolute value of vertical facing bound by the maximum
      // vertical rate of turn(上游注释)
      const WVec v_dist{-diff_clf_msl_hgt - info_.cruise_altitude.Length,
                        -int4_speed_, 0};
      desired_v_facing =
          static_cast<std::int8_t>(v_dist.HorizontalLengthSquared()) != 0
              ? v_dist.Yaw().Facing()
              : v_facing;

      // If the missile is launched above CruiseAltitude, it has to descend
      // instead of climbing(上游注释)
      if (-diff_clf_msl_hgt > info_.cruise_altitude.Length)
        desired_v_facing = -desired_v_facing;

      desired_v_facing =
          ClampInt(desired_v_facing,
                   -info_.vertical_rate_of_turn.Facing(),
                   info_.vertical_rate_of_turn.Facing());

      ChangeSpeed();
    }
  } else {
    // Aim to attain cruise altitude as soon as possible while having the
    // absolute value of vertical facing bound by the maximum vertical rate
    // of turn(上游注释)
    const WVec v_dist{-diff_clf_msl_hgt - info_.cruise_altitude.Length,
                      -int4_speed_, 0};
    desired_v_facing =
        static_cast<std::int8_t>(v_dist.HorizontalLengthSquared()) != 0
            ? v_dist.Yaw().Facing()
            : v_facing;

    // If the missile is launched above CruiseAltitude, it has to descend
    // instead of climbing(上游注释)
    if (-diff_clf_msl_hgt > info_.cruise_altitude.Length)
      desired_v_facing = -desired_v_facing;

    desired_v_facing =
        ClampInt(desired_v_facing, -info_.vertical_rate_of_turn.Facing(),
                 info_.vertical_rate_of_turn.Facing());

    ChangeSpeed();
  }

  return desired_v_facing;
}

WVec Missile::HomingTick(sim::World& world, const WVec& tar_dist_vec,
                         int rel_tar_hor_dist) {
  // L779-837
  int pred_clf_hgt = 0;
  int pred_clf_dist = 0;
  int last_ht_chg = 0;
  int last_ht = 0;

  if (info_.b_terrain_height_aware)
    InclineLookahead(world, rel_tar_hor_dist, pred_clf_hgt, pred_clf_dist,
                     last_ht_chg, last_ht);

  // Height difference between the incline height and missile height
  // (上游注释)
  const int diff_clf_msl_hgt = pred_clf_hgt - pos_.Z;

  // Get underestimate of distance from target in next tick(上游注释;
  // 上游计算后未消费 —— 历史遗留,保留计算)
  // Get an underestimate of the distance to the target next tick
  // (upstream computes but never consumes it — the historical leftover,
  // kept verbatim).
  [[maybe_unused]] const int nxt_rel_tar_hor_dist =
      ClampInt(rel_tar_hor_dist - int4_speed_ -
                   info_.acceleration.Length,
               0, rel_tar_hor_dist);

  // Target height relative to the missile(上游注释)
  const int rel_tar_hgt = tar_dist_vec.Z;

  // Compute which direction the projectile should be facing(上游注释)
  const WVec vel_vec = tar_dist_vec + pred_vel_;
  int desired_h_facing =
      vel_vec.HorizontalLengthSquared() != 0
          ? vel_vec.Yaw().Facing()
          : int4_h_facing_;

  const int delta = NormalizeFacing(int4_h_facing_ - desired_h_facing);
  if (b_allow_pass_by_ && delta > 64 && delta < 192) {
    desired_h_facing = (desired_h_facing + 128) & 0xFF;
    b_target_passed_by_ = true;
  } else {
    b_target_passed_by_ = false;
  }

  const int desired_v_facing = HomingInnerTick(
      pred_clf_dist, diff_clf_msl_hgt, rel_tar_hor_dist, last_ht_chg,
      last_ht, rel_tar_hgt, int4_v_facing_, b_target_passed_by_);

  // The target has been passed by(上游注释)
  if (tar_dist_vec.HorizontalLength() <
      int4_speed_ * WAngle::FromFacing(int4_v_facing_).Cos() / 1024)
    b_target_passed_by_ = true;

  // Check whether the homing mechanism is jammed(JamsMissiles 空集 →
  // 恒 false;COVERAGE 登记)
  // Check whether the homing mechanism is jammed (the empty JamsMissiles
  // set → constantly false; registered in COVERAGE).
  constexpr bool jammed = false;
  if (jammed) {
    // 不可达(JamsMissiles 空集);RNG 无消耗
    // Unreachable (the empty JamsMissiles set); no RNG consumption.
  } else if (!args_.guided_target.IsValidFor(args_.source_actor)) {
    desired_h_facing = int4_h_facing_;
  }

  // Compute new direction the projectile will be facing(上游注释)
  int4_h_facing_ =
      TickFacingInt(int4_h_facing_, desired_h_facing,
                    info_.horizontal_rate_of_turn.Facing());
  int4_v_facing_ =
      TickFacingInt(int4_v_facing_, desired_v_facing,
                    info_.vertical_rate_of_turn.Facing());

  // Compute the projectile's guided displacement(上游注释)
  return WVec{0, -1024 * int4_speed_, 0}
             .Rotate(WRot{WAngle::FromFacing(int4_v_facing_), WAngle{0},
                          WAngle{0}})
             .Rotate(WRot{WAngle{0}, WAngle{0},
                          WAngle::FromFacing(int4_h_facing_)}) /
         1024;
}

void Missile::Tick(sim::World& world) {
  // L839-928
  int4_ticks_++;
  if (animation_ != nullptr)
    animation_->Tick();

  // Switch from freefall mode to homing mode(上游注释)
  if (int4_ticks_ == info_.int4_homing_activation_delay + 1) {
    state_ = States::Homing;
    int4_speed_ = velocity_.Length();

    // Compute the vertical loop radius(上游注释)
    int4_loop_radius_ =
        LoopRadiusOf(int4_speed_, info_.vertical_rate_of_turn.Facing());
  }

  // Switch from homing mode to freefall mode(上游注释)
  if (range_limit_.Length >= 0 && distance_covered_.Length > range_limit_.Length) {
    state_ = States::Freefall;
    velocity_ = WVec{0, -int4_speed_, 0}
                    .Rotate(WRot{WAngle::FromFacing(int4_v_facing_),
                                 WAngle{0}, WAngle{0}})
                    .Rotate(WRot{WAngle{0}, WAngle{0},
                                 WAngle::FromFacing(int4_h_facing_)});
  }

  // Check if target position should be updated (actor visible & locked
  // on)(上游注释)
  WPos new_tar_pos = target_position_;
  if (args_.guided_target.IsValidFor(args_.source_actor) && b_lock_on_) {
    new_tar_pos =
        (args_.weapon->b_targetActorCenter
             ? args_.guided_target.CenterPosition()
             : ClosestToIgnoringPath(
                   args_.guided_target.Positions(), args_.source)) +
        WVec{0, 0, info_.airburst_altitude.Length};
  }

  // Compute target's predicted velocity vector (assuming uniform circular
  // motion)(上游注释)
  const WAngle yaw1 = tar_vel_.HorizontalLengthSquared() != 0
                          ? tar_vel_.Yaw()
                          : WAngle::FromFacing(int4_h_facing_);
  tar_vel_ = new_tar_pos - target_position_;
  const WAngle yaw2 = tar_vel_.HorizontalLengthSquared() != 0
                          ? tar_vel_.Yaw()
                          : WAngle::FromFacing(int4_h_facing_);
  pred_vel_ = tar_vel_.Rotate(WRot::FromYaw(yaw2 - yaw1));
  target_position_ = new_tar_pos;

  // Compute current distance from target position(上游注释)
  const WVec tar_dist_vec = target_position_ + offset_ - pos_;
  const int rel_tar_dist = tar_dist_vec.Length();
  const int rel_tar_hor_dist = tar_dist_vec.HorizontalLength();

  WVec move;
  if (state_ == States::Freefall)
    move = FreefallTick();
  else
    move = HomingTick(world, tar_dist_vec, rel_tar_hor_dist);

  render_facing_ = WVec{move.X, move.Y - move.Z, 0}.Yaw();

  // Move the missile(上游注释)
  const WPos last_pos = pos_;
  if (info_.b_allow_snapping && state_ != States::Freefall &&
      rel_tar_dist < move.Length())
    pos_ = target_position_ + offset_;
  else
    pos_ = pos_ + move;

  // Check for walls or other blocking obstacles(上游注释)
  bool should_explode = false;
  if (info_.b_blockable) {
    WPos blocked_pos;
    if (BlocksProjectiles::AnyBlockingActorsBetween(
            world, args_.source_actor->Owner(), last_pos, pos_,
            info_.dist_width, blocked_pos)) {
      pos_ = blocked_pos;
      should_explode = true;
    }
  }

  // Create the sprite trail effect(SpriteEffect 视觉面不构造;RNG 消耗
  // 保持;COVERAGE 登记)
  // Create the sprite trail effect (the SpriteEffect visual face is not
  // constructed; the RNG consumption is preserved; registered in
  // COVERAGE).
  if (!info_.str_trail_image.empty() && --int4_ticks_to_next_smoke_ < 0 &&
      (state_ != States::Freefall || info_.b_trail_when_deactivated)) {
    [[maybe_unused]] const std::string str_sequence =
        RandomOf(world, info_.vec_trail_sequences);

    int4_ticks_to_next_smoke_ = info_.int4_trail_interval;
  }

  // Contrail.Update 随渲染批(无 RNG;COVERAGE 登记)
  // The Contrail update rides the render batch (no RNG; registered in
  // COVERAGE).

  distance_covered_ = WDist{distance_covered_.Length + int4_speed_};
  const CPos cell = world.Map().CellContaining(pos_);
  const WDist height = world.Map().DistanceAboveTerrain(pos_);
  should_explode |=
      height.Length < 0  // Hit the ground
      || rel_tar_dist < info_.close_enough.Length  // Within range
      || (info_.b_explode_when_empty && range_limit_.Length >= 0 &&
          distance_covered_.Length > range_limit_.Length)  // Ran out of
                                                           // fuel
      || !world.Map().Contains(
             cell)  // This also avoids an IndexOutOfRange below
      || (!info_.str_bound_to_terrain_type.empty() &&
          world.Map().GetTerrainInfo(cell).Type !=
              info_.str_bound_to_terrain_type)  // Hit incompatible
                                                // terrain
      || (height.Length < info_.airburst_altitude.Length &&
          rel_tar_hor_dist < info_.close_enough.Length);  // Airburst

  if (should_explode)
    Explode(world);
}

void Missile::Explode(sim::World& world) {
  // L930-948
  // ContrailFader 的帧末生成随渲染批(无 RNG;COVERAGE 登记)
  // The ContrailFader's frame-end spawn rides the render batch (no RNG;
  // registered in COVERAGE).
  world.AddFrameEndTask([this](sim::World& w) { w.Remove(this); });

  // Don't blow up in our launcher's face!(上游注释)
  if (int4_ticks_ <= info_.int4_arm)
    return;

  sim::WarheadArgs warhead_args{args_};
  warhead_args.impact_orientation =
      WRot{WAngle{0}, WAngle::FromFacing(int4_v_facing_),
           WAngle::FromFacing(int4_h_facing_)};
  warhead_args.impact_position = pos_;

  sim::WeaponImpact(*args_.weapon, sim::Target::FromPos(pos_),
                    warhead_args);
}

int Missile::SyncHashOf(const sim::ISync* s) {
  // gen/sync_gen.cpp Missile {pos, hFacing, vFacing}(int 成员直取值)
  // gen/sync_gen.cpp's Missile {pos, hFacing, vFacing} (the int members
  // hash as their raw values).
  const auto* missile = static_cast<const Missile*>(s);
  return sim::sync::CombineSyncHash(
      sim::sync::CombineSyncHash(
          sim::sync::CombineSyncHash(0, sim::sync::HashWPos(missile->pos_)),
          missile->int4_h_facing_),
      missile->int4_v_facing_);
}

sim::IProjectile* MissileInfo::Create(sim::ProjectileArgs& args) {
  // L189
  return new Missile(info_, args);
}

// ———— GravityBomb(L57-147)————
// ———— GravityBomb (L57-147) ————

GravityBombInfoData GravityBombInfoData::Parse(
    const meta::RecordObject& rec_info) {
  GravityBombInfoData data;
  if (const auto v = RecString(rec_info, "Image"))
    data.str_image = *v;
  {
    const auto vec = RecStringArray(rec_info, "Sequences");
    if (!vec.empty())
      data.vec_sequences = vec;
  }
  if (const auto v = RecString(rec_info, "OpenSequence"))
    data.str_open_sequence = *v;
  if (const auto v = RecString(rec_info, "Palette"))
    data.str_palette = *v;
  if (const auto v = RecInt(rec_info, "IsPlayerPalette"))
    data.b_is_player_palette = *v != 0;
  if (const auto v = RecInt(rec_info, "Shadow"))
    data.b_shadow = *v != 0;
  if (const auto v = RecInt(rec_info, "ShadowColor"))
    data.shadow_color =
        core::Color::FromArgbRaw(static_cast<std::uint32_t>(*v));
  if (const auto v = RecWVec(rec_info, "Velocity"))
    data.velocity = *v;
  if (const auto v = RecWVec(rec_info, "Acceleration"))
    data.acceleration = *v;
  return data;
}

GravityBomb::GravityBomb(const GravityBombInfoData& info,
                         sim::ProjectileArgs& args)
    : info_{info}, args_{args} {
  // L72-94
  pos_ = args.source;
  const WVec converted_velocity{info_.velocity.Y, -info_.velocity.X,
                                info_.velocity.Z};
  velocity_ = converted_velocity.Rotate(WRot::FromYaw(args.facing));
  acceleration_ =
      WVec{info_.acceleration.Y, -info_.acceleration.X,
           info_.acceleration.Z};

  if (!info_.str_image.empty()) {
    // OpenSequence 非空:上游 PlayThen(OpenSequence, 闭包) —— 闭包内的
    // Sequences.Random 未执行(视觉时序),此刻零 RNG 消耗;空:立即
    // PlayRepeating(Sequences.Random(...)) 一次消耗。两路均仅注入面
    // 就绪时构造 Animation
    // A non-empty OpenSequence: upstream PlayThen(OpenSequence, closure)
    // — the closure's Sequences.Random has not run (the visual timeline),
    // so zero RNG consumption now; empty: an immediate PlayRepeating(
    // Sequences.Random(...)) consumes once. Both paths construct the
    // Animation only when the injection face is wired.
    if (info_.str_open_sequence.empty()) {
      sim::World& world = args.source_actor->world();
      const std::string str_sequence =
          RandomOf(world, info_.vec_sequences);
      animation_ = MakeBulletAnimation(
          world, info_.str_image, str_sequence,
          [this]() { return args_.facing; });
    }
  }
}

void GravityBomb::Tick(sim::World& world) {
  // L96-117
  last_pos_ = pos_;
  pos_ = pos_ + velocity_;
  velocity_ = velocity_ + acceleration_;

  if (pos_.Z <= args_.passive_target.Z) {
    pos_ = pos_ + WVec{0, 0, args_.passive_target.Z - pos_.Z};
    world.AddFrameEndTask([this](sim::World& w) { w.Remove(this); });

    sim::WarheadArgs warhead_args{args_};
    warhead_args.impact_orientation =
        WRot{WAngle{0}, GetVerticalAngle(last_pos_, pos_), args_.facing};
    warhead_args.impact_position = pos_;

    sim::WeaponImpact(*args_.weapon, sim::Target::FromPos(pos_),
                      warhead_args);
  }

  if (animation_ != nullptr)
    animation_->Tick();
}

int GravityBomb::SyncHashOf(const sim::ISync* s) {
  // gen/sync_gen.cpp GravityBomb {pos, lastPos}
  // gen/sync_gen.cpp's GravityBomb {pos, lastPos}.
  const auto* bomb = static_cast<const GravityBomb*>(s);
  return sim::sync::CombineSyncHash(
      sim::sync::CombineSyncHash(0, sim::sync::HashWPos(bomb->pos_)),
      sim::sync::HashWPos(bomb->last_pos_));
}

sim::IProjectile* GravityBombInfo::Create(sim::ProjectileArgs& args) {
  // L54
  return new GravityBomb(info_, args);
}

// ———— TeslaZap(L58-98)————
// ———— TeslaZap (L58-98) ————

TeslaZapInfoData TeslaZapInfoData::Parse(
    const meta::RecordObject& rec_info) {
  TeslaZapInfoData data;
  if (const auto v = RecString(rec_info, "Image"))
    data.str_image = *v;
  if (const auto v = RecString(rec_info, "BrightSequence"))
    data.str_bright_sequence = *v;
  if (const auto v = RecString(rec_info, "DimSequence"))
    data.str_dim_sequence = *v;
  if (const auto v = RecString(rec_info, "Palette"))
    data.str_palette = *v;
  if (const auto v = RecInt(rec_info, "BrightZaps"))
    data.int4_bright_zaps = static_cast<int>(*v);
  if (const auto v = RecInt(rec_info, "DimZaps"))
    data.int4_dim_zaps = static_cast<int>(*v);
  if (const auto v = RecInt(rec_info, "Duration"))
    data.int4_duration = static_cast<int>(*v);
  if (const auto v = RecInt(rec_info, "DamageDuration"))
    data.int4_damage_duration = static_cast<int>(*v);
  if (const auto v = RecInt(rec_info, "TrackTarget"))
    data.b_track_target = *v != 0;
  if (const auto v = RecInt(rec_info, "ZOffset"))
    data.int4_z_offset = static_cast<int>(*v);
  return data;
}

TeslaZap::TeslaZap(const TeslaZapInfoData& info, sim::ProjectileArgs& args)
    : args_{args}, info_{info} {
  // L69-76
  int4_ticks_until_remove_ = info_.int4_duration;
  int4_damage_duration_ = info_.int4_damage_duration > info_.int4_duration
                              ? info_.int4_duration
                              : info_.int4_damage_duration;
  target_ = args.passive_target;
}

void TeslaZap::Tick(sim::World& world) {
  // L78-89
  if (int4_ticks_until_remove_-- <= 0)
    world.AddFrameEndTask(
        [this](sim::World& w) { w.Remove(this); });

  // Zap tracks target(上游注释)
  if (info_.b_track_target &&
      args_.guided_target.IsValidFor(args_.source_actor))
    target_ = args_.weapon->b_targetActorCenter
                  ? args_.guided_target.CenterPosition()
                  : ClosestToIgnoringPath(args_.guided_target.Positions(),
                                          args_.source);

  if (int4_damage_duration_-- > 0) {
    sim::WarheadArgs warhead_args{args_};
    sim::WeaponImpact(*args_.weapon, sim::Target::FromPos(target_),
                      warhead_args);
  }
}

int TeslaZap::SyncHashOf(const sim::ISync* s) {
  // gen/sync_gen.cpp TeslaZap {target}
  // gen/sync_gen.cpp's TeslaZap {target}.
  const auto* zap = static_cast<const TeslaZap*>(s);
  return sim::sync::CombineSyncHash(0,
                                    sim::sync::HashWPos(zap->target_));
}

sim::IProjectile* TeslaZapInfo::Create(sim::ProjectileArgs& args) {
  // L55
  return new TeslaZap(info_, args);
}

}  // namespace ora::mods
