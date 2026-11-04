#pragma once

import std;

#include "game/miniyaml.hpp"

namespace ora::game::combat {

// WeaponInfo/ProjectileInfo/Warhead 的 yaml 最小解析面（T12/issue #13）。
// 语义：上游 WeaponInfo(MiniYaml) ctor（FieldLoader：缺键 → 字段默认值保留）
// + Projectile/Warhead 子节点（值 = 类型名 + "Info"/"Warhead" 反射构造）。
// 对拍面 = tests/golden/combat/duels.txt 头部（weapons loaded/weapon/warheads 行）。

enum class WarheadKind : std::uint8_t {
  kSpreadDamage,
  kCreateEffect,
  kLeaveSmudge,
};

struct WarheadDef {
  WarheadKind kind{WarheadKind::kSpreadDamage};
  std::string str_kind_name;  // 反射类名（"SpreadDamageWarhead" 等，dump 对拍）
  int int4_delay{};           // Warhead.Delay（0 = 立即；>0 → DelayedImpact 帧尾链）
  int int4_inaccuracy{};      // CreateEffect Inaccuracy.Length（无该字段 = 0）
  int int4_damage{};          // SpreadDamage Damage
  std::vector<std::pair<std::string, int>> vec_versus;  // 保序（合并树序 = dump 序）
  std::vector<int> vec_falloff;
  int int4_spread{};
  std::vector<std::string> vec_valid_targets;    // 默认 Ground,Water
  std::vector<std::string> vec_invalid_targets;  // 默认空
};

enum class ProjectileKind : std::uint8_t {
  kNone,
  kInstantHit,
  kBullet,
};

struct ProjectileDef {
  ProjectileKind kind{ProjectileKind::kNone};
  std::string str_kind_name;          // 反射类名（"InstantHitInfo"/"BulletInfo"）
  int int4_speed{};                   // Bullet Speed（单值；双值剧本不涉及）
  std::vector<int> vec_launch_angle;  // Bullet LaunchAngle（单/双值，双值耗 1 RNG）
  int int4_inaccuracy{};              // InstantHit/Bullet Inaccuracy（ctor 耗 4 RNG）
  bool b_blockable{};                 // Blockable（对射剧本无 BlocksProjectiles actor，恒 false 面）
  int int4_contrail_length{};         // Bullet ContrailLength（>0 → 爆炸帧尾 ContrailFader）
};

struct WeaponDef {
  std::string str_name_lower;
  int int4_range{};
  int int4_min_range{};
  int int4_reload_delay{1};
  int int4_burst{1};
  std::vector<int> vec_burst_delays{5};
  bool b_target_actor_center{};
  ProjectileDef projectile;
  std::vector<WarheadDef> vec_warheads;  // yaml 节点序（结算序）
  std::vector<std::string> vec_valid_targets{"Ground", "Water"};
  std::vector<std::string> vec_invalid_targets;
};

struct WeaponTable {
  std::vector<WeaponDef> vec_weapons;  // 合并树序（含 ^ 抽象节点，与 oracle 同口径）

  [[nodiscard]] const WeaponDef* find(std::string_view str_name_lower) const {
    for (const WeaponDef& def : vec_weapons)
      if (def.str_name_lower == str_name_lower) return &def;
    return nullptr;
  }
};

// 上游 MiniYaml.Load(modFiles, Manifest.Weapons, null) 等价：逐文件解析（保清单序，
// Location 名 = manifest 条目）→ miniyaml_merge（MergeSelfPartial/继承解析/删除）→
// 逐顶层节点 WeaponDef 构造（含 ^ 抽象）。含 WeaponInfo ctor 的单源自合并（幂等）。
[[nodiscard]] std::expected<WeaponTable, std::string> weapon_table_load(
    const std::vector<std::string>& vec_file_texts,
    const std::vector<std::string>& vec_file_names);

// WDist yaml 字面量（"5c0"/"171"/"1c138"）；失败 nullopt（上游 TryParse 语义）
[[nodiscard]] std::optional<int> wdist_parse(std::string_view str_value);

}  // namespace ora::game::combat
