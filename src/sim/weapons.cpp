// UPSTREAM: OpenRA.Game/GameRules/WeaponInfo.cs @b6fc03f L178-268(仿真面)+
//          OpenRA.Game/Effects/DelayedImpact.cs L15-43 +
//          OpenRA.Mods.Common/Warheads/Warhead.cs L59-96(IsValidAgainst 面)
//          The sim face of WeaponInfo.cs L178-268 + DelayedImpact.cs
//          L15-43 + Warhead.cs L59-96 (the IsValidAgainst faces).
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
  // L210-218
  if (!weapon.b_canTargetSelf && &victim == fired_by)
    return false;

  return WeaponIsValidTarget(weapon, victim.GetEnabledTargetTypes());
}

bool WeaponIsValidAgainst(const game::WeaponInfo& weapon, const Target& target,
                          World& world, Actor* fired_by) {
  // L184-207
  if (target.Type() == TargetType::Actor)
    return WeaponIsValidAgainst(weapon, *const_cast<Actor*>(target.ActorPtr),
                                fired_by);

  if (target.Type() == TargetType::FrozenActor) {
    // FrozenActor 面随 Shroud 批:上游走 IsValidAgainst(FrozenActor, …)
    // 的 victim.IsValid 前置 —— 本批无 FrozenActor 实例,恒落假分支
    // (COVERAGE 登记)
    // The FrozenActor face rides the Shroud batch: upstream goes through
    // IsValidAgainst(FrozenActor, …)'s victim.IsValid precondition — no
    // FrozenActor instances exist this batch, so the false branch always
    // lands (registered in COVERAGE).
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

/// DelayedImpact(DelayedImpact.cs L15-43):Delay 递减到 0 → 帧末摘除 +
/// DoImpact
/// DelayedImpact (DelayedImpact.cs L15-43): when the delay counts down to
/// zero, a frame-end removal + DoImpact.
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
      // 上游帧末闭包经 GC 保活;C++ 的 World::Remove 在帧末任务内即析构
      // 效果对象 —— 成员载荷按值快照(wh/target/args 的拷贝捕获),
      // this 仅作 Remove 的对象引用(任务执行时仍在效果表,有效)
      // Upstream's frame-end closure is kept alive by the GC; C++'s
      // World::Remove destroys the effect right inside the frame-end
      // task — the member payload is snapshotted by value (wh/target/
      // args captured by copy), with `this` serving only as the Remove
      // reference (still in the effect table at task time, valid).
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

}  // namespace

void WeaponImpact(const game::WeaponInfo& weapon, const Target& target,
                  WarheadArgs& args) {
  // L221-230
  World& world = args.source_actor->world();
  for (mods::IWarhead* warhead : mods::ResolveWarheads(weapon.vec_warheads)) {
    if (warhead->Delay() > 0) {
      // 上游闭包按值捕获 target/args(L226 注记;帧末构造 DelayedImpact)
      // upstream's closure captures target/args by value (the L226 note;
      // the DelayedImpact is constructed at frame end).
      const Target delayed_target = target;
      const WarheadArgs delayed_args = args;
      world.AddFrameEndTask([warhead, delayed_target, delayed_args](
                                World& w) mutable {
        w.Add(std::make_unique<DelayedImpact>(warhead->Delay(), warhead,
                                              delayed_target, delayed_args));
      });
    } else {
      // 上游 DoImpact(in Target, WarheadArgs args) 按值 —— 每 warhead 一份
      // 独立拷贝(受端可变不外泄)
      // upstream's DoImpact(in Target, WarheadArgs args) is by value — one
      // independent copy per warhead (callee mutations stay local).
      WarheadArgs per_wh_args = args;
      warhead->DoImpact(target, per_wh_args);
    }
  }
}

void WeaponImpact(const game::WeaponInfo& weapon, const Target& target,
                  Actor* fired_by) {
  // L233-250
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
  // L44-52
  weapon = args.weapon;
  vec_damage_modifiers = args.vec_damage_modifiers;
  impact_position = args.passive_target;
  b_has_source = true;
  source = args.source;
  source_actor = args.source_actor;
  weapon_target = args.guided_target;
}

// ———— ProjectileRegistry(进程级单例;TraitRegistry 同形)————
// ———— ProjectileRegistry (the process-level singleton; the TraitRegistry
//      shape) ————

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
  // 一记录一构造的进程级缓存(确定性;产物为无状态数据对象)
  // The process-level one-record-one-construct cache (deterministic; the
  // products are stateless data objects).
  static std::map<const meta::RecordObject*, IProjectileInfo*> map_cache;
  const auto it = map_cache.find(&rec_projectile);
  if (it != map_cache.end())
    return it->second;

  // 记录名(如 "BulletInfo")→ 注册表
  // The record name (e.g. "BulletInfo") → the registry.
  IProjectileInfo* resolved = ProjectileRegistry::Instance().Create(
      std::string{rec_projectile.record_desc().str_name}, rec_projectile);
  map_cache.emplace(&rec_projectile, resolved);
  return resolved;
}

}  // namespace ora::sim

namespace ora::mods {

// ———— WarheadRegistry(同 ProjectileRegistry)————
// ———— WarheadRegistry (same as the ProjectileRegistry) ————

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
  // L59-79
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

bool Warhead::IsValidAgainstFrozen(const sim::Actor& /*victim*/,
                                   const sim::Actor* /*fired_by*/) {
  // L82-96:FrozenActor 面随 Shroud 批(上游 victim.IsValid 前置 → 本批恒假)
  // L82-96: the FrozenActor face rides the Shroud batch (upstream's
  // victim.IsValid precondition → always false this batch).
  return false;
}

std::vector<IWarhead*> ResolveWarheads(
    const std::vector<std::shared_ptr<meta::RecordObject>>& vec_rec_warheads) {
  // 一记录一构造的进程级缓存
  // The process-level one-record-one-construct cache.
  static std::map<const meta::RecordObject*, IWarhead*> map_cache;

  std::vector<IWarhead*> vec_result;
  vec_result.reserve(vec_rec_warheads.size());
  for (const auto& rec : vec_rec_warheads) {
    IWarhead* resolved = nullptr;
    const auto it = map_cache.find(rec.get());
    if (it != map_cache.end()) {
      resolved = it->second;
    } else {
      // 记录名(如 "DamageWarhead" 记录 → 名 "…Warhead" 键)—— 上游键 =
      // "{值}Warhead",而值袋记录名已含 Warhead 后缀,直接按记录名查
      // The record name (e.g. the "DamageWarhead" record → the
      // "…Warhead" key) — upstream's key is "{value}Warhead", and the
      // value-bag record name already carries the Warhead suffix, so the
      // record name is the key.
      resolved = WarheadRegistry::Instance().Create(
          std::string{rec->record_desc().str_name}, *rec);
      map_cache.emplace(rec.get(), resolved);
    }

    // 未注册 → 跳过(上游 CreateObject null 后 continue)
    // Unregistered → skip (upstream's CreateObject-null-then-continue).
    if (resolved != nullptr)
      vec_result.push_back(resolved);
  }
  return vec_result;
}

}  // namespace ora::mods
