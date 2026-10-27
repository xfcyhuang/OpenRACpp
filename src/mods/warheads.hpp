#pragma once
import std;

#include "core/bitset.hpp"
#include "core/wdist.hpp"
#include "core/wpos.hpp"
#include "core/wrot.hpp"
#include "mods/hit_shape.hpp"
#include "sim/target.hpp"
#include "sim/trait_interfaces.hpp"
#include "sim/weapons.hpp"

namespace ora::meta {
class RecordObject;
}

namespace ora::mods {

using sim::Actor;

enum class DamageCalculationType : std::int32_t {
  HitShape = 0,
  ClosestTargetablePosition = 1,
  CenterPosition = 2,
};

class DamageWarhead : public Warhead {
 public:
  int Damage = 0;
  core::BitSet<sim::DamageType> bitset_damage_types;
  std::vector<std::pair<std::string, int>> vec_versus;

  bool IsValidAgainst(sim::Actor& victim, sim::Actor* fired_by) override;

  void DoImpact(const sim::Target& target,
                sim::WarheadArgs& args) override;

 protected:

  virtual int DamageVersus(sim::Actor& victim, HitShape& shape,
                           const sim::WarheadArgs& args);

  virtual void InflictDamage(sim::Actor& victim, sim::Actor* fired_by,
                             HitShape& shape, sim::WarheadArgs& args);

  virtual void DoImpact(const WPos& pos, sim::Actor* fired_by,
                        sim::WarheadArgs& args) = 0;

  void ParseDamageFields(const meta::RecordObject& rec_info);
};

class SpreadDamageWarhead final : public DamageWarhead {
 public:
  WDist Spread{43};
  std::vector<int> vec_falloff{100, 37, 14, 5, 0};
  std::vector<WDist> vec_range;
  DamageCalculationType damage_calculation_type =
      DamageCalculationType::HitShape;

  std::vector<WDist> vec_effective_range;

  static std::unique_ptr<SpreadDamageWarhead> Parse(
      const meta::RecordObject& rec_info);

  void DoImpact(const WPos& pos, sim::Actor* fired_by,
                sim::WarheadArgs& args) override;

 private:

  int GetDamageFalloff(int distance) const;
};

class TargetDamageWarhead final : public DamageWarhead {
 public:
  WDist Spread{0};

  static std::unique_ptr<TargetDamageWarhead> Parse(
      const meta::RecordObject& rec_info);

  void DoImpact(const WPos& pos, sim::Actor* fired_by,
                sim::WarheadArgs& args) override;
};

class CreateEffectWarhead final : public Warhead {
 public:
  std::vector<std::string> vec_explosions;
  std::string str_image{"explosion"};
  std::string str_explosion_palette{"effect"};
  bool b_use_player_palette = false;
  bool b_force_display_at_ground_level = false;
  std::vector<std::string> vec_impact_sounds;
  int int4_impact_sound_chance = 100;
  bool b_impact_actors = true;
  WDist dist_inaccuracy{0};

  static std::unique_ptr<CreateEffectWarhead> Parse(
      const meta::RecordObject& rec_info);

  bool IsValidAgainst(sim::Actor& victim, sim::Actor* fired_by) override;

  void DoImpact(const sim::Target& target,
                sim::WarheadArgs& args) override;

 private:

  enum class ImpactActorType : std::int32_t {
    None = 0,
    Invalid = 1,
    Valid = 2,
  };

  ImpactActorType ActorTypeAtImpact(sim::World& world, const WPos& pos,
                                    sim::Actor* fired_by);

  bool IsValidAgainstTerrain(sim::World& world, const WPos& pos);
};

class LeaveSmudgeWarhead final : public Warhead {
 public:
  std::vector<int> vec_size{0, 0};
  std::vector<std::string> vec_smudge_type;
  int int4_chance = 100;

  static std::unique_ptr<LeaveSmudgeWarhead> Parse(
      const meta::RecordObject& rec_info);

  void DoImpact(const sim::Target& target,
                sim::WarheadArgs& args) override;
};

void SetWarheadSoundPlayer(
    std::function<void(const std::string&, sim::World&, const WPos&)>
        fn_play);

}
