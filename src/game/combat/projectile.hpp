#pragma once

import std;

#include "game/combat/combat_sim.hpp"
#include "game/combat/weapon.hpp"
#include "game/pathfinding/wangle.hpp"
#include "game/support/mersenne_twister.hpp"

namespace ora::game::combat {

// Projectile/Warhead 结算最小面（WeaponInfo.Impact / InstantHit / Bullet /
// DelayedImpact / SpreadDamageWarhead / DamageWarhead / CreateEffectWarhead，T12）。
// 效果动画（SpriteEffect）不入对拍面：RNG 消耗照做（CreateEffect 的 Inaccuracy
// 在效果创建之前），动画生命周期登记 upstream.md。

// InstantHit ctor（:52-67）：TargetActorCenter ? GuidedTarget : PassiveTarget
// （M1Carbine Inaccuracy=0 → 无散布、无 RNG）
[[nodiscard]] pathfinding::WPos instant_hit_target_pos(const WeaponDef& weapon,
                                                       pathfinding::WPos pos_passive);

// Bullet ctor（:170-230）：LaunchAngle/Speed 双值各 1 RNG；Inaccuracy>0 →
// WVec.FromPDF(r,2)（4 RNG）散布；length = max(dist/speed, 1)（int 除法）
[[nodiscard]] Effect bullet_create(Arena& arena, const WeaponDef& weapon,
                                   pathfinding::WPos pos_source, pathfinding::WPos pos_passive,
                                   std::uint32_t u4_owner);

// WeaponInfo.Impact（:235-249）：逐 warhead（yaml 序）——delay>0 → 帧尾加
// DelayedImpact；否则立即 do_impact
void weapon_impact(Arena& arena, const WeaponDef& weapon, pathfinding::WPos pos_impact,
                   std::uint32_t u4_owner);

// warhead 立即结算（DoImpact 最小面）：SpreadDamage（FindActorsOnCircle[effectiveRange[^1]
// 外扩面] → falloff 距离 → 链 2/链 3）/ CreateEffect（ActorTypeAtImpact 门闸 + Inaccuracy
// RNG 4）/ LeaveSmudge（不同步，跳过——登记 upstream）
void warhead_do_impact(Arena& arena, const WarheadDef& wh, pathfinding::WPos pos_impact,
                       std::uint32_t u4_owner);

// effects 段逐 effect tick（列表序）：
//   Bullet：LerpQuadratic 插值 + ticks++ >= length 爆炸（帧尾 remove + Impact）
//   DelayedImpact：--delay<=0 → 帧尾（remove + DoImpact）
void effects_tick(Arena& arena);

// WPos.LerpQuadratic（WPos.cs:58-72）：linear lerp + decimal 高度项
// （int64 乘积精确商，向零截断）
[[nodiscard]] pathfinding::WPos wpos_lerp_quadratic(pathfinding::WPos pos_a,
                                                    pathfinding::WPos pos_b,
                                                    std::int32_t int4_pitch,
                                                    std::int32_t int4_mul, std::int32_t int4_div);

// WVec.FromPDF(r, 2)：X/Y 各 WDist.FromPDF(2)（各 2 次 Next、和除 2 int 截断）→ 4 RNG
[[nodiscard]] pathfinding::WVec wvec_from_pdf2(support::MersenneTwister& rng);

// WDist.FromPDF(r, samples)：samples 次 Next(-1024,1024) 求和除 samples（int 截断）
[[nodiscard]] std::int32_t wdist_from_pdf(support::MersenneTwister& rng, std::int32_t int4_samples);

}  // namespace ora::game::combat
