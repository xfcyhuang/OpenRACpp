#pragma once
import std;

#include "core/bitset.hpp"
#include "core/wdist.hpp"
#include "core/wpos.hpp"
#include "core/wrot.hpp"
#include "meta/generic_record.hpp"
#include "sim/effects.hpp"
#include "sim/target.hpp"
#include "sim/trait_interfaces.hpp"

namespace ora::game {
class WeaponInfo;
}

namespace ora::sim {

class Actor;
class World;

class WarheadArgs;

bool WeaponIsValidTarget(const game::WeaponInfo& weapon,
                         const core::BitSet<TargetableType>& target_types);

bool WeaponIsValidAgainst(const game::WeaponInfo& weapon, const Target& target,
                          World& world, Actor* fired_by);

bool WeaponIsValidAgainst(const game::WeaponInfo& weapon, Actor& victim,
                          Actor* fired_by);

void WeaponImpact(const game::WeaponInfo& weapon, const Target& target,
                  WarheadArgs& args);

void WeaponImpact(const game::WeaponInfo& weapon, const Target& target,
                  Actor* fired_by);

struct ProjectileArgs {
  const game::WeaponInfo* weapon = nullptr;
  std::vector<int> vec_damage_modifiers;
  std::vector<int> vec_inaccuracy_modifiers;
  std::vector<int> vec_range_modifiers;
  WAngle facing;
  std::function<WAngle()> fn_current_muzzle_facing;
  WPos source;
  std::function<WPos()> fn_current_source;
  Actor* source_actor = nullptr;
  WPos passive_target;
  Target guided_target;
};

class WarheadArgs {
 public:

  explicit WarheadArgs(const ProjectileArgs& args);

  WarheadArgs(const WarheadArgs& args) = default;
  WarheadArgs& operator=(const WarheadArgs& args) = default;

  WarheadArgs() = default;

  const game::WeaponInfo* weapon = nullptr;
  std::vector<int> vec_damage_modifiers;
  bool b_has_source = false;
  WPos source{};
  WRot impact_orientation = WRot::None();
  WPos impact_position;
  Actor* source_actor = nullptr;
  Target weapon_target;
};

class IProjectile : public IEffect {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_GameRules_IProjectile;
};

class IProjectileInfo {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_GameRules_IProjectileInfo;
  virtual ~IProjectileInfo() = default;
  virtual IProjectile* Create(ProjectileArgs& args) = 0;
};

class ProjectileRegistry {
 public:
  static ProjectileRegistry& Instance();

  using CreateFn = std::function<IProjectileInfo*(
      const meta::RecordObject& rec_info)>;

  void Register(std::string str_name, CreateFn fn_create);

  IProjectileInfo* Create(const std::string& str_name,
                          const meta::RecordObject& rec_info);

 private:
  std::map<std::string, CreateFn> map_creators_;
};

IProjectileInfo* ResolveProjectileInfo(const meta::RecordObject& rec_projectile);

}

namespace ora::mods {

class IWarhead;

class IWarhead {
 public:
  static constexpr gen::TypeId kTypeId = gen::TypeId::OpenRA_Traits_IWarhead;
  virtual ~IWarhead() = default;
  virtual int Delay() const = 0;
  virtual void DoImpact(const sim::Target& target,
                        sim::WarheadArgs& args) = 0;
};

class Warhead : public IWarhead {
 public:
  core::BitSet<sim::TargetableType> valid_types;
  core::BitSet<sim::TargetableType> invalid_types;
  sim::PlayerRelationship valid_relationships =
      sim::PlayerRelationship::Ally | sim::PlayerRelationship::Neutral |
      sim::PlayerRelationship::Enemy;
  bool affects_parent = false;
  WDist air_threshold{128};
  int delay = 0;

  int Delay() const override { return delay; }

  bool IsValidTarget(
      const core::BitSet<sim::TargetableType>& target_types) const {
    return valid_types.Overlaps(target_types) &&
           !invalid_types.Overlaps(target_types);
  }

  virtual bool IsValidAgainst(sim::Actor& victim, sim::Actor* fired_by);

  static bool IsValidAgainstFrozen(const sim::Actor& victim,
                                   const sim::Actor* fired_by);

  void DoImpact(const sim::Target& target,
                sim::WarheadArgs& args) override = 0;
};

class WarheadRegistry {
 public:
  static WarheadRegistry& Instance();

  using CreateFn =
      std::function<IWarhead*(const meta::RecordObject& rec_info)>;

  void Register(std::string str_name, CreateFn fn_create);

  IWarhead* Create(const std::string& str_name,
                   const meta::RecordObject& rec_info);

 private:
  std::map<std::string, CreateFn> map_creators_;
};

std::vector<IWarhead*> ResolveWarheads(
    const std::vector<std::shared_ptr<meta::RecordObject>>& vec_rec_warheads);

}
