// UPSTREAM: OpenRA.Mods.Common/Projectiles/Missile.cs @b6fc03f L26-982 +
//          GravityBomb.cs L20-147 + OpenRA.Mods.Cnc/Projectiles/TeslaZap.cs
//          L20-99(全文逐语义重写;Render/TeslaZapRenderable/Contrail 的
//          视觉面随渲染批 —— RNG 消耗序保持)
//          The whole of Missile.cs L26-982 + GravityBomb.cs L20-147 +
//          OpenRA.Mods.Cnc/Projectiles/TeslaZap.cs L20-99
//          (verbatim-semantics rewrites; the Render/TeslaZapRenderable/
//          Contrail visual faces land with the render batch — the RNG
//          consumption order is preserved).
//
// 机制对照 / Mechanism mapping:
//  - 值袋 MissileInfo/GravityBombInfo/TeslaZapInfo 记录 → 本文件 Info 解析
//    面;Create 经 ProjectileRegistry(记录名即键;同 Bullet 形)
//    The value-bag MissileInfo/GravityBombInfo/TeslaZapInfo records →
//    this file's Info parse faces; Create registers through the
//    ProjectileRegistry (the record name is the key; Bullet's shape).
//  - Animation 注入面:复用 projectiles.hpp 的 SetBulletAnimationFactory
//    (缺省 = 不构造;Sequences.Random 的 SharedRandom 消耗保持 —— 上游
//    Missile/GravityBomb 的 anim 构造序 = ctor 尾部,RNG 序不依赖视觉装配;
//    GravityBomb 的 OpenSequence→PlayThen 链 = 注入面之上的渲染批语义,
//    RNG 只消耗一次 Random,登记 COVERAGE)
//    The Animation injection face: reuses projectiles.hpp's
//    SetBulletAnimationFactory (the default constructs nothing; the
//    Sequences.Random SharedRandom consumption is preserved — upstream's
//    Missile/GravityBomb anim construction sits at the ctor tail, so the
//    RNG order never depends on the visual assembly; GravityBomb's
//    OpenSequence→PlayThen chain is a render-batch semantics atop the
//    injection face, with exactly one Random consumed, registered in
//    COVERAGE).
//  - Contrail/ContrailFader/TeslaZapRenderable/SpriteEffect(trail):视觉
//    效果不构造(渲染批;Missile 的 trail RNG 消耗保持;COVERAGE 登记)
//    The Contrail/ContrailFader/TeslaZapRenderable/SpriteEffect (trail):
//    the visual effects are not constructed (the render batch; Missile's
//    trail RNG consumption is preserved; registered in COVERAGE).
//  - JamsMissiles trait 未移植:world.ActorsWithTrait<JamsMissiles>() 的
//    空集 = Any() 恒假(jammed 恒 false;上游无该 trait 世界同值;
//    COVERAGE 登记)
//    The JamsMissiles trait is unported: world.ActorsWithTrait<
//    JamsMissiles>()'s empty set = Any() constantly false (jammed stays
//    false; the same value upstream gives in a world without the trait;
//    registered in COVERAGE).
#pragma once
import std;

#include "core/wangle.hpp"
#include "core/wdist.hpp"
#include "core/wpos.hpp"
#include "core/wrot.hpp"
#include "mods/util.hpp"
#include "sim/target.hpp"
#include "sim/trait_interfaces.hpp"
#include "sim/weapons.hpp"

namespace ora::meta {
class RecordObject;
}

namespace ora::gfx {
class Animation;  // 弹体图像注入面 | the projectile-image injection face.
}

namespace ora::mods {

using sim::Actor;

// ———— MissileInfo(L30-187 的解析面)————
// ———— MissileInfo (the parse face of L30-187) ————

struct MissileInfoData {
  std::string str_image;                              // L30
  std::vector<std::string> vec_sequences{"idle"};     // L34
  std::string str_palette{"effect"};                  // L38
  bool b_is_player_palette = false;                   // L41
  bool b_shadow = false;                              // L44
  core::Color shadow_color{
      core::Color::FromArgbRaw(0x8C000000u)};         // L47
  WAngle min_launch_angle{-64};                    // L50(Angle 域)
  WAngle max_launch_angle{128};                    // L53(Angle 域)
  WDist min_launch_speed{-1};                      // L56
  WDist max_launch_speed{-1};                      // L59
  WDist speed{384};                                // L62
  WDist acceleration{5};                           // L65
  int int4_arm = 0;                                // L68
  bool b_blockable = true;                         // L71
  bool b_terrain_height_aware = false;             // L74
  WDist dist_width{1};                             // L77
  WDist dist_inaccuracy{0};                        // L80
  InaccuracyType inaccuracy_type = InaccuracyType::Absolute;  // L86
  WDist lock_on_inaccuracy{-1};                    // L89
  int int4_lock_on_probability = 100;              // L92
  WAngle horizontal_rate_of_turn{20};              // L95(Angle 域)
  WAngle vertical_rate_of_turn{24};                // L98(Angle 域)
  int int4_gravity = 10;                              // L101
  WDist range_limit{0};                               // L104
  bool b_explode_when_empty = true;                   // L107
  WDist airburst_altitude{0};                         // L110
  WDist cruise_altitude{512};                         // L113
  int int4_homing_activation_delay = 0;               // L116
  std::string str_trail_image;                        // L119
  std::vector<std::string> vec_trail_sequences{"idle"};  // L123
  std::string str_trail_palette{"effect"};            // L127
  bool b_trail_use_player_palette = false;            // L130
  int int4_trail_interval = 2;                        // L133
  bool b_trail_when_deactivated = false;              // L136
  int int4_contrail_length = 0;                       // L139
  int int4_contrail_delay = 1;                        // L142
  int int4_contrail_z_offset = 2047;                  // L145
  WDist dist_contrail_start_width{64};                // L148
  std::optional<WDist> opt_contrail_end_width;        // L151
  core::Color contrail_start_color;                   // L154
  bool b_contrail_start_color_use_player_color = false;  // L157
  int int4_contrail_start_color_alpha = 255;          // L160
  std::optional<core::Color> opt_contrail_end_color;  // L163
  bool b_contrail_end_color_use_player_color = false;  // L166
  int int4_contrail_end_color_alpha = 0;              // L169
  bool b_jammable = true;                             // L172
  int int4_jammed_diversion_range = 20;               // L175
  std::string str_bound_to_terrain_type;              // L178
  bool b_allow_snapping = false;                      // L182
  WDist close_enough{298};                            // L187

  static MissileInfoData Parse(const meta::RecordObject& rec_info);
};

/// MissileInfo(值袋面)→ IProjectileInfo
/// MissileInfo (the value-bag face) → IProjectileInfo.
class MissileInfo final : public sim::IProjectileInfo {
 public:
  explicit MissileInfo(MissileInfoData data) : info_{std::move(data)} {}

  sim::IProjectile* Create(sim::ProjectileArgs& args) override;

  const MissileInfoData& Info() const { return info_; }

 private:
  MissileInfoData info_;
};

// ———— GravityBombInfo(L22-53 的解析面)————
// ———— GravityBombInfo (the parse face of L22-53) ————

struct GravityBombInfoData {
  std::string str_image;                          // L25
  std::vector<std::string> vec_sequences{"idle"};  // L29
  std::string str_open_sequence;                  // L33
  std::string str_palette{"effect"};              // L37
  bool b_is_player_palette = false;               // L40
  bool b_shadow = false;                          // L43
  core::Color shadow_color{
      core::Color::FromArgbRaw(0x8C000000u)};     // L46
  WVec velocity{0, 0, 0};                         // L49
  WVec acceleration{0, 0, -15};                   // L52

  static GravityBombInfoData Parse(const meta::RecordObject& rec_info);
};

/// GravityBombInfo(值袋面)→ IProjectileInfo
/// GravityBombInfo (the value-bag face) → IProjectileInfo.
class GravityBombInfo final : public sim::IProjectileInfo {
 public:
  explicit GravityBombInfo(GravityBombInfoData data)
      : info_{std::move(data)} {}

  sim::IProjectile* Create(sim::ProjectileArgs& args) override;

  const GravityBombInfoData& Info() const { return info_; }

 private:
  GravityBombInfoData info_;
};

// ———— TeslaZapInfo(L21-54 的解析面)————
// ———— TeslaZapInfo (the parse face of L21-54) ————

struct TeslaZapInfoData {
  std::string str_image{"litning"};   // L23
  std::string str_bright_sequence{"bright"};  // L27
  std::string str_dim_sequence{"dim"};        // L31
  std::string str_palette{"effect"};          // L35
  int int4_bright_zaps = 1;                   // L38
  int int4_dim_zaps = 2;                      // L41
  int int4_duration = 2;                      // L44
  int int4_damage_duration = 1;               // L47
  bool b_track_target = true;                 // L50
  int int4_z_offset = 0;                      // L53

  static TeslaZapInfoData Parse(const meta::RecordObject& rec_info);
};

/// TeslaZapInfo(值袋面)→ IProjectileInfo
/// TeslaZapInfo (the value-bag face) → IProjectileInfo.
class TeslaZapInfo final : public sim::IProjectileInfo {
 public:
  explicit TeslaZapInfo(TeslaZapInfoData data) : info_{std::move(data)} {}

  sim::IProjectile* Create(sim::ProjectileArgs& args) override;

  const TeslaZapInfoData& Info() const { return info_; }

 private:
  TeslaZapInfoData info_;
};

// ———— 弹丸类(InstallCommonSyncEffectHasher 的 dynamic_cast 分派需要
//      完整类型;同 Bullet 的 .cpp 内类不同,此处三件哈希需跨文件消费)————
// ———— The projectile classes (InstallCommonSyncEffectHasher's
//      dynamic_cast dispatch needs complete types; unlike Bullet's
//      in-.cpp class, the three hashes here are consumed across files)
// ————

/// Missile(Missile.cs L193-981;ISync {pos, hFacing, vFacing})
class Missile final : public sim::IProjectile, public sim::ISync {
 public:
  Missile(const MissileInfoData& info, sim::ProjectileArgs& args);

  void Tick(sim::World& world) override;

  /// gen/sync_gen.cpp Missile 成员表的哈希 {pos(WPos), hFacing(int),
  /// vFacing(int)}
  /// The hash of gen/sync_gen.cpp's Missile member table {pos(WPos),
  /// hFacing(int), vFacing(int)}.
  static int SyncHashOf(const sim::ISync* s);

 private:
  // L315-322:loopRadius = speed / angular speed(pi = 314/100)
  // L315-322: loopRadius = speed / angular speed (pi = 314/100).
  static int LoopRadiusOf(int speed, int rot);

  // L324-363
  void DetermineLaunchSpeedAndAngleForIncline(int pred_clf_dist,
                                              int diff_clf_msl_hgt,
                                              int rel_tar_hor_dist,
                                              int& speed, int& v_facing);
  // L366-408
  void DetermineLaunchSpeedAndAngle(sim::World& world, int& speed,
                                    int& v_facing);
  // L413-427
  static bool WillClimbWithinDistance(int v_facing, int loop_radius,
                                      int pred_clf_dist,
                                      int diff_clf_msl_hgt);
  // L432-435
  static bool IsNearInclineTop(int v_facing, int loop_radius,
                               int pred_clf_dist);
  // L440-451
  static bool WillClimbAroundInclineTop(int v_facing, int loop_radius,
                                        int pred_clf_dist,
                                        int diff_clf_msl_hgt);
  // L453-470(testCriterion → 模板谓词)
  // L453-470 (testCriterion → a template predicate).
  template <class Pred>
  static int BisectionSearch(int lower_bound, int upper_bound, Pred test);

  // L483-489
  void ChangeSpeed(int sign = 1);
  // L491-501
  WVec FreefallTick();
  // L505-548
  void InclineLookahead(sim::World& world, int dist_check,
                        int& pred_clf_hgt, int& pred_clf_dist,
                        int& last_ht_chg, int& last_ht);
  // L550-604
  int IncreaseAltitude(int pred_clf_dist, int diff_clf_msl_hgt,
                       int rel_tar_hor_dist, int v_facing);
  // L606-777
  int HomingInnerTick(int pred_clf_dist, int diff_clf_msl_hgt,
                      int rel_tar_hor_dist, int last_ht_chg, int last_ht,
                      int rel_tar_hgt, int v_facing, bool target_passed_by);
  // L779-837
  WVec HomingTick(sim::World& world, const WVec& tar_dist_vec,
                  int rel_tar_hor_dist);
  // L930-948
  void Explode(sim::World& world);

  enum class States { Freefall, Homing, Hitting };

  const MissileInfoData& info_;
  sim::ProjectileArgs args_;
  gfx::Animation* animation_ = nullptr;  // 注入面(缺省 null)

  WVec gravity_{};
  int min_launch_speed_ = 0;
  int max_launch_speed_ = 0;
  int max_speed_ = 0;
  WAngle min_launch_angle_{};
  WAngle max_launch_angle_{};

  int int4_ticks_ = 0;
  int int4_ticks_to_next_smoke_ = 0;
  std::string str_trail_palette_;

  States state_ = States::Freefall;
  bool b_target_passed_by_ = false;
  bool b_lock_on_ = false;
  bool b_allow_pass_by_ = false;  // L225 TODO 注释随行

  WPos target_position_{};
  WVec offset_{};

  WVec tar_vel_{};
  WVec pred_vel_{};

  // [VerifySync] L233-248
  WPos pos_{};
  WVec velocity_{};
  int int4_speed_ = 0;
  int int4_loop_radius_ = 0;
  WDist distance_covered_{0};
  WDist range_limit_{0};

  WAngle render_facing_{};
  int int4_h_facing_ = 0;  // [VerifySync]
  int int4_v_facing_ = 0;  // [VerifySync]
};

/// GravityBomb(GravityBomb.cs L57-147;ISync {pos, lastPos})
class GravityBomb final : public sim::IProjectile, public sim::ISync {
 public:
  GravityBomb(const GravityBombInfoData& info, sim::ProjectileArgs& args);

  void Tick(sim::World& world) override;

  /// gen/sync_gen.cpp GravityBomb 成员表的哈希 {pos, lastPos}
  /// The hash of gen/sync_gen.cpp's GravityBomb member table
  /// {pos, lastPos}.
  static int SyncHashOf(const sim::ISync* s);

 private:
  const GravityBombInfoData& info_;
  sim::ProjectileArgs args_;
  gfx::Animation* animation_ = nullptr;  // 注入面(缺省 null)
  WVec acceleration_{};

  // [VerifySync] L69-70
  WPos pos_, last_pos_;

  WVec velocity_{};
};

/// TeslaZap(TeslaZap.cs L58-98;ISync {target})
class TeslaZap final : public sim::IProjectile, public sim::ISync {
 public:
  TeslaZap(const TeslaZapInfoData& info, sim::ProjectileArgs& args);

  void Tick(sim::World& world) override;

  /// gen/sync_gen.cpp TeslaZap 成员表的哈希 {target(WPos)}
  /// The hash of gen/sync_gen.cpp's TeslaZap member table
  /// {target(WPos)}.
  static int SyncHashOf(const sim::ISync* s);

 private:
  sim::ProjectileArgs args_;
  const TeslaZapInfoData& info_;
  int int4_ticks_until_remove_ = 0;
  int int4_damage_duration_ = 0;

  WPos target_{};  // [VerifySync] L67
};

}  // namespace ora::mods
