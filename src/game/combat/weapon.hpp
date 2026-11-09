#pragma once

import std;

#include "game/miniyaml.hpp"

namespace ora::game::combat {

enum class WarheadKind : std::uint8_t {
  kSpreadDamage,
  kCreateEffect,
  kLeaveSmudge,
};

struct WarheadDef {
  WarheadKind kind{WarheadKind::kSpreadDamage};
  std::string str_kind_name;
  int int4_delay{};
  int int4_inaccuracy{};
  int int4_damage{};
  std::vector<std::pair<std::string, int>> vec_versus;
  std::vector<int> vec_falloff;
  int int4_spread{};
  std::vector<std::string> vec_valid_targets;
  std::vector<std::string> vec_invalid_targets;
  std::vector<std::string> vec_explosions;
  std::vector<std::string> vec_impact_sounds;
  int int4_impact_sound_chance{100};
};

enum class ProjectileKind : std::uint8_t {
  kNone,
  kInstantHit,
  kBullet,
};

struct ProjectileDef {
  ProjectileKind kind{ProjectileKind::kNone};
  std::string str_kind_name;
  int int4_speed{};
  std::vector<int> vec_launch_angle;
  int int4_inaccuracy{};
  bool b_blockable{};
  int int4_contrail_length{};
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
  std::vector<WarheadDef> vec_warheads;
  std::vector<std::string> vec_valid_targets{"Ground", "Water"};
  std::vector<std::string> vec_invalid_targets;
  std::vector<std::string> vec_report;
};

struct WeaponTable {
  std::vector<WeaponDef> vec_weapons;

  [[nodiscard]] const WeaponDef* find(std::string_view str_name_lower) const {
    for (const WeaponDef& def : vec_weapons)
      if (def.str_name_lower == str_name_lower) return &def;
    return nullptr;
  }
};

[[nodiscard]] std::expected<WeaponTable, std::string> weapon_table_load(
    const std::vector<std::string>& vec_file_texts,
    const std::vector<std::string>& vec_file_names);

[[nodiscard]] std::optional<int> wdist_parse(std::string_view str_value);

}
