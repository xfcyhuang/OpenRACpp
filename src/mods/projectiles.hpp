// UPSTREAM: OpenRA.Mods.Common/Projectiles/Bullet.cs @b6fc03f L26-399 +
//          InstantHit.cs L20-96(全文逐语义重写;Render/ContrailRenderable/
//          SpriteEffect 的视觉面随渲染批 —— RNG 消耗序保持)
//          The whole of Bullet.cs L26-399 + InstantHit.cs L20-96
//          (verbatim-semantics rewrites; the Render/ContrailRenderable/
//          SpriteEffect visual faces land with the render batch — the RNG
//          consumption order is preserved).
//
// 机制对照 / Mechanism mapping:
//  - 值袋 BulletInfo/InstantHitInfo 记录 → 本文件 Info 解析面;Create 经
//    ProjectileRegistry 注册(记录名即键)
//    The value-bag BulletInfo/InstantHitInfo records → this file's Info
//    parse faces; Create registers through the ProjectileRegistry (the
//    record name is the key).
//  - Animation(弹体图像)注入面:工厂函数注入(缺省 = 不构造;RNG 的
//    Sequences.Random 消耗保持 —— 确定性不依赖视觉装配)
//    The Animation (projectile image) face: an injected factory (the
//    default constructs nothing; the Sequences.Random RNG consumption is
//    preserved — determinism never depends on the visual assembly).
//  - Trail 的 SpriteEffect / Contrail 的 ContrailFader/ContrailRenderable:
//    视觉效果不构造(渲染批;RNG 消耗保持;COVERAGE 登记)
//    Trail's SpriteEffect / Contrail's ContrailFader/ContrailRenderable:
//    the visual effects are not constructed (the render batch; the RNG
//    consumption is preserved; registered in COVERAGE).
//  - Game.Sound.Play(BounceSound) → 声音注入面(同 Armament 形)
//    Game.Sound.Play (BounceSound) → the sound injection face (Armament's
//    shape).
#pragma once
import std;

#include "core/color.hpp"
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

/// BulletInfo 的解析面(L28-144)
/// The parsed face of BulletInfo (L28-144).
struct BulletInfoData {
  std::vector<WDist> vec_speed{WDist{17}};  // L31(双值 = 变速域)
  WDist dist_inaccuracy{0};                 // L34
  InaccuracyType inaccuracy_type = InaccuracyType::Maximum;  // L40
  std::string str_image;                    // L43
  std::vector<std::string> vec_sequences{"idle"};  // L47
  std::string str_palette{"effect"};        // L51
  bool b_is_player_palette = false;         // L54
  bool b_shadow = false;                    // L57
  core::Color shadow_color{
      core::Color::FromArgbRaw(0x8C000000u)};  // L60 FromArgb(140,0,0,0)
  std::string str_trail_image;              // L63
  std::vector<std::string> vec_trail_sequences{"idle"};  // L67
  int int4_trail_interval = 2;              // L70
  int int4_trail_delay = 1;                 // L73
  std::string str_trail_palette{"effect"};  // L77
  bool b_trail_use_player_palette = false;  // L80
  bool b_blockable = true;                  // L83
  WDist dist_width{1};                      // L86
  std::vector<WAngle> vec_launch_angle{WAngle{0}};  // L89
  int int4_bounce_count = 0;                // L93
  int int4_bounce_range_modifier = 60;      // L96
  std::string str_bounce_sound;             // L99
  std::vector<std::string> vec_invalid_bounce_terrain;  // L102
  sim::PlayerRelationship valid_bounce_blocker_relationships =
      sim::PlayerRelationship::Enemy | sim::PlayerRelationship::Neutral;
  WDist dist_airburst_altitude{0};          // L108
  // Contrail 族(L111-141)视觉面随渲染批;字段保留以锚定解析
  int int4_contrail_length = 0;             // L111
  int int4_contrail_delay = 1;              // L114
  int int4_contrail_z_offset = 2047;        // L117
  WDist dist_contrail_start_width{64};      // L120
  std::optional<WDist> opt_contrail_end_width;  // L123
  core::Color contrail_start_color;         // L126
  bool b_contrail_start_color_use_player_color = false;  // L129
  int int4_contrail_start_color_alpha = 255;  // L132
  std::optional<core::Color> opt_contrail_end_color;  // L135
  bool b_contrail_end_color_use_player_color = false;  // L138
  int int4_contrail_end_color_alpha = 0;    // L141

  static BulletInfoData Parse(const meta::RecordObject& rec_info);
};

/// BulletInfo(值袋面)→ IProjectileInfo | BulletInfo (the value-bag face)
/// → IProjectileInfo.
class BulletInfo final : public sim::IProjectileInfo {
 public:
  explicit BulletInfo(BulletInfoData data) : info_{std::move(data)} {}

  sim::IProjectile* Create(sim::ProjectileArgs& args) override;

  const BulletInfoData& Info() const { return info_; }

 private:
  BulletInfoData info_;
};

/// InstantHitInfo 的解析面(L21-43)
/// The parsed face of InstantHitInfo (L21-43).
struct InstantHitInfoData {
  WDist dist_inaccuracy{0};  // L24
  InaccuracyType inaccuracy_type = InaccuracyType::Maximum;  // L30
  bool b_blockable = false;  // L33
  WDist dist_width{1};       // L36
  WDist dist_blocker_scan_radius{-1};  // L40

  static InstantHitInfoData Parse(const meta::RecordObject& rec_info);
};

/// InstantHitInfo(值袋面)→ IProjectileInfo
/// InstantHitInfo (the value-bag face) → IProjectileInfo.
class InstantHitInfo final : public sim::IProjectileInfo {
 public:
  explicit InstantHitInfo(InstantHitInfoData data)
      : info_{std::move(data)} {}

  sim::IProjectile* Create(sim::ProjectileArgs& args) override;

  const InstantHitInfoData& Info() const { return info_; }

 private:
  InstantHitInfoData info_;
};

/// 弹丸图像的注入面(渲染批接线;缺省空 = 不构造 —— RNG 消耗仍在)
/// The projectile-image injection face (wired by the render batch; the
/// empty default constructs nothing — the RNG consumption remains).
void SetBulletAnimationFactory(
    std::function<gfx::Animation*(sim::World&, const std::string&,
                                  const std::string&,
                                  std::function<WAngle()>)> fn_factory);

/// 注入面的消费入口(Missile/GravityBomb 等跨文件构造;未接线 → nullptr)
/// The injection face's consumption entry (cross-file construction for
/// Missile/GravityBomb etc.; unwired → nullptr).
gfx::Animation* MakeBulletAnimation(
    sim::World& world, const std::string& str_image,
    const std::string& str_sequence, std::function<WAngle()> fn_facing);

/// 弹着音注入面(Game.Sound.Play 的 World 型;同 Armament 形)
/// The impact-sound injection face (Game.Sound.Play's World form;
/// Armament's shape).
void SetProjectileSoundPlayer(
    std::function<void(const std::string&, sim::World&, const WPos&)>
        fn_play);

/// 已知同步弹丸的效果哈希装配(World.SetSyncEffectHasher 的 dynamic_cast
/// 分派;含 Bullet —— 引擎装配/测试接线)
/// The known-synced-projectile effect-hash assembly (World.
/// SetSyncEffectHasher's dynamic_cast dispatch; covers Bullet — wired by
/// the engine assembly/tests).
void InstallCommonSyncEffectHasher(sim::World& world);

}  // namespace ora::mods
