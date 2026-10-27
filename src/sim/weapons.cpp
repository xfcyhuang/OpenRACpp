#include "sim/weapons.hpp"

#include "game/game_records.hpp"
#include "sim/actor.hpp"
#include "sim/player.hpp"
#include "sim/effects.hpp"
#include "sim/player.hpp"
#include "sim/world.hpp"

namespace ora::sim {

bool WeaponIsValidTarget(const game::WeaponInfo& weapon,
                         const core::BitSet<TargetableType>& target_types) {
  const auto valid = core::BitSet<TargetableType>::FromRawBits(
      static_cast<std::uint64_t>(weapon.int8_validTargets));
  const auto invalid = core::BitSet<TargetableType>::FromRawBits(
      static_cast<std::uint64_t>(weapon.int8_invalidTargets));
  return valid.Overlaps(target_types) && !invalid.Overlaps(target_types);
}

bool WeaponIsValidAgainst(const game::WeaponInfo& weapon, Actor& victim,
                          Actor* fired_by) {

  if (!weapon.b_canTargetSelf && &victim == fired_by)
    return false;

  return WeaponIsValidTarget(weapon, victim.GetEnabledTargetTypes());
}

bool WeaponIsValidAgainst(const game::WeaponInfo& weapon, const Target& target,
                          World& world, Actor* fired_by) {

  if (target.Type() == TargetType::Actor)
    return WeaponIsValidAgainst(weapon, *const_cast<Actor*>(target.ActorPtr),
                                fired_by);

  if (target.Type() == TargetType::FrozenActor) {

    return false;
  }

  if (target.Type() == TargetType::Terrain) {
    static const core::BitSet<TargetableType> kTargetTypeAir =
        [] {
          const std::string str_air[]{"Air"};
          return core::BitSet<TargetableType>(
              std::span<const std::string>{str_air});
        }();

    const WDist dat = world.Map().DistanceAboveTerrain(
        target.CenterPosition());
    const WDist air_threshold{
        weapon.int4_airThreshold};
    if (dat > air_threshold)
      return WeaponIsValidTarget(weapon, kTargetTypeAir);

    const CPos cell = world.Map().CellContaining(target.CenterPosition());
    if (!world.Map().Contains(cell))
      return false;

    const auto& cell_info = world.Map().GetTerrainInfo(cell);
    return WeaponIsValidTarget(
        weapon,
        core::BitSet<TargetableType>::FromRawBits(
            cell_info.uint8_target_types));
  }

  return false;
}

namespace {

class DelayedImpact final : public IEffect {
 public:
  DelayedImpact(int int4_delay, mods::IWarhead* wh, Target target,
                WarheadArgs args)
      : target_(std::move(target)),
        wh_(wh),
        args_(std::move(args)),
        int4_delay_(int4_delay) {}

  void Tick(World& world) override {
    if (--int4_delay_ <= 0)

      world.AddFrameEndTask([this, wh = wh_, target = target_,
                             args = args_](World& w) mutable {
        w.Remove(this);
        wh->DoImpact(target, args);
      });
  }

 private:
  Target target_;
  mods::IWarhead* wh_;
  WarheadArgs args_;
  int int4_delay_;
};

}

void WeaponImpact(const game::WeaponInfo& weapon, const Target& target,
                  WarheadArgs& args) {

  World& world = args.source_actor->world();
  for (mods::IWarhead* warhead : mods::ResolveWarheads(weapon.vec_warheads)) {
    if (warhead->Delay() > 0) {

      const Target delayed_target = target;
      const WarheadArgs delayed_args = args;
      world.AddFrameEndTask([warhead, delayed_target, delayed_args](
                                World& w) mutable {
        w.Add(std::make_unique<DelayedImpact>(warhead->Delay(), warhead,
                                              delayed_target, delayed_args));
      });
    } else {

      WarheadArgs per_wh_args = args;
      warhead->DoImpact(target, per_wh_args);
    }
  }
}

void WeaponImpact(const game::WeaponInfo& weapon, const Target& target,
                  Actor* fired_by) {

  WarheadArgs args;
  args.weapon = &weapon;
  args.source_actor = fired_by;
  args.weapon_target = target;

  if (fired_by->OccupiesSpace() != nullptr) {
    args.b_has_source = true;
    args.source = fired_by->CenterPosition();
  }

  WeaponImpact(weapon, target, args);
}

WarheadArgs::WarheadArgs(const ProjectileArgs& args) {

  weapon = args.weapon;
  vec_damage_modifiers = args.vec_damage_modifiers;
  impact_position = args.passive_target;
  b_has_source = true;
  source = args.source;
  source_actor = args.source_actor;
  weapon_target = args.guided_target;
}

ProjectileRegistry& ProjectileRegistry::Instance() {
  static ProjectileRegistry instance;
  return instance;
}

void ProjectileRegistry::Register(std::string str_name, CreateFn fn_create) {
  map_creators_[std::move(str_name)] = std::move(fn_create);
}

IProjectileInfo* ProjectileRegistry::Create(const std::string& str_name,
                                            const meta::RecordObject& rec_info) {
  const auto it = map_creators_.find(str_name);
  return it != map_creators_.end() ? it->second(rec_info) : nullptr;
}

IProjectileInfo* ResolveProjectileInfo(const meta::RecordObject& rec_projectile) {

  static std::map<const meta::RecordObject*, IProjectileInfo*> map_cache;
  const auto it = map_cache.find(&rec_projectile);
  if (it != map_cache.end())
    return it->second;

  IProjectileInfo* resolved = ProjectileRegistry::Instance().Create(
      std::string{rec_projectile.record_desc().str_name}, rec_projectile);
  map_cache.emplace(&rec_projectile, resolved);
  return resolved;
}

}

namespace ora::mods {

WarheadRegistry& WarheadRegistry::Instance() {
  static WarheadRegistry instance;
  return instance;
}

void WarheadRegistry::Register(std::string str_name, CreateFn fn_create) {
  map_creators_[std::move(str_name)] = std::move(fn_create);
}

IWarhead* WarheadRegistry::Create(const std::string& str_name,
                                  const meta::RecordObject& rec_info) {
  const auto it = map_creators_.find(str_name);
  return it != map_creators_.end() ? it->second(rec_info) : nullptr;
}

bool Warhead::IsValidAgainst(sim::Actor& victim, sim::Actor* fired_by) {

  if (!affects_parent && &victim == fired_by)
    return false;

  const sim::PlayerRelationship relationship =
      fired_by->Owner()->RelationshipWith(victim.Owner());
  if (!sim::HasRelationship(valid_relationships, relationship))
    return false;

  if (!IsValidTarget(victim.GetEnabledTargetTypes()))
    return false;

  return true;
}

bool Warhead::IsValidAgainstFrozen(const sim::Actor& ,
                                   const sim::Actor* ) {

  return false;
}

std::vector<IWarhead*> ResolveWarheads(
    const std::vector<std::shared_ptr<meta::RecordObject>>& vec_rec_warheads) {

  static std::map<const meta::RecordObject*, IWarhead*> map_cache;

  std::vector<IWarhead*> vec_result;
  vec_result.reserve(vec_rec_warheads.size());
  for (const auto& rec : vec_rec_warheads) {
    IWarhead* resolved = nullptr;
    const auto it = map_cache.find(rec.get());
    if (it != map_cache.end()) {
      resolved = it->second;
    } else {

      resolved = WarheadRegistry::Instance().Create(
          std::string{rec->record_desc().str_name}, *rec);
      map_cache.emplace(rec.get(), resolved);
    }

    if (resolved != nullptr)
      vec_result.push_back(resolved);
  }
  return vec_result;
}

}
