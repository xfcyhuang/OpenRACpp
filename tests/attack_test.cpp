import std;

#include "game/game.hpp"

#include "render_sequences_fixture.hpp"
#include "game/game_records.hpp"
#include "game/manifest.hpp"
#include "game/mod_data.hpp"
#include "map/map.hpp"
#include "map/map_cache.hpp"
#include "mods/affects_shroud.hpp"
#include "mods/armor.hpp"
#include "mods/body_orientation.hpp"
#include "mods/create_map_players.hpp"
#include "mods/attack_base.hpp"
#include "mods/auto_target.hpp"
#include "mods/hit_shape.hpp"
#include "mods/mobile.hpp"
#include "mods/projectiles.hpp"
#include "mods/warheads.hpp"
#include "sim/actor.hpp"
#include "sim/actor_init.hpp"
#include "sim/frozen_actor_layer.hpp"
#include "sim/player.hpp"
#include "sim/shroud.hpp"
#include "sim/sync_hash.hpp"
#include "sim/target.hpp"
#include "sim/world.hpp"
#include <cstdio>

namespace {
std::string_view TestMapFilter() {
  const char* str_env = std::getenv("ORA_TEST_MAP_FILTER");
  if (str_env != nullptr)
    return std::string_view{str_env};
  return "Siberian-Pass";
}
}

namespace ora::gen {
void RegisterGeneratedAll();
}

static int g_failures = 0;

template <class T>
void CheckEq(const T& a, const T& b, std::string_view what) {
  if (!(a == b)) {
    std::println("FAIL: {} ({} != {})", what, a, b);
    ++g_failures;
  }
}

void Check(bool ok, std::string_view what) {
  if (!ok) {
    std::println("FAIL: {}", what);
    ++g_failures;
  }
}

void TestRealRaAttackChain(const char* str_upstream_root) {
  using namespace ora;
  game::InstalledMods mods_installed{std::string{str_upstream_root} + "/mods"};
  const game::Manifest* manifest = mods_installed.Find("ra");
  Check(manifest != nullptr, "ra mod found");
  if (manifest == nullptr)
    return;
  game::ModData mod_data{*manifest, mods_installed, str_upstream_root};

  game::Game game_{game::Game::Deps{.ptr_mod_data = &mod_data}};
  game_.JoinLocal();

  map::MapCache cache{mod_data.ManifestRef(), mod_data.ModFiles()};
  cache.LoadMaps(mod_data, TestMapFilter());
  std::string str_uid;
  for (map::MapPreview* p : cache.Previews())
    if (p->Status() == map::MapStatus::Available) {
      str_uid = p->Uid();
      std::println("map: {} ({})", p->Title(), p->Path());
      break;
    }
  Check(!str_uid.empty(), "an available ra map exists");
  if (str_uid.empty())
    return;

  map::MapPreview& preview = cache.At(str_uid);
  auto map_world = preview.ToMap();
  Check(map_world != nullptr, "MapPreview.ToMap constructs");
  if (map_world == nullptr)
    return;

  ora::testfx::InstallSyntheticSequences(*map_world, *manifest,
                                          mod_data, str_upstream_root);
  auto world = std::make_unique<sim::World>(
      *map_world, mod_data, *game_.OrderManagerFace(), sim::WorldType::Regular);
  world->LoadComplete(nullptr);
  Check(!world->Players().empty(), "map players created");
  if (world->Players().empty())
    return;

  mods::InstallCommonSyncEffectHasher(*world);

  std::vector<sim::Player*> vec_combatants;
  for (sim::Player* p : world->Players())
    if (!p->NonCombatant())
      vec_combatants.push_back(p);

  while (vec_combatants.size() < 2) {
    std::size_t index = vec_combatants.size();
    if (index >= world->Players().size())
      index = 0;
    sim::Player* fallback = world->Players()[index];
    if (std::find(vec_combatants.begin(), vec_combatants.end(),
                  fallback) != vec_combatants.end())
      break;
    vec_combatants.push_back(fallback);
  }
  sim::Player* player_a = vec_combatants[0];
  sim::Player* player_b = vec_combatants[1];
  player_a->EnemyPlayersMask =
      player_a->EnemyPlayersMask.Union(player_b->PlayerMask);
  player_b->EnemyPlayersMask =
      player_b->EnemyPlayersMask.Union(player_a->PlayerMask);
  Check(player_a->RelationshipWith(player_b) == sim::PlayerRelationship::Enemy,
        "A sees B as enemy (forced mask)");

  sim::Shroud* shroud_a = player_a->GetShroud();
  Check(shroud_a != nullptr, "player A Shroud resolved (^Player rules)");
  sim::FrozenActorLayer* frozen_layer = player_a->GetFrozenActorLayer();
  Check(frozen_layer != nullptr,
        "player A FrozenActorLayer resolved (^Player rules)");
  if (shroud_a == nullptr)
    return;

  Check(shroud_a->FogEnabled(), "fog enabled by default (lobby default)");
  Check(!shroud_a->ExploreMapEnabled(),
        "explored-map disabled by default");

  const map::Map& map = world->Map();
  const Rectangle bounds = map.Bounds();
  CPos spawn{bounds.Left() + bounds.Width / 2,
             bounds.Top() + bounds.Height / 2};

  sim::TypeDictionary inits_probe;
  inits_probe.Add(new sim::OwnerInit(player_a));
  sim::Actor* probe = world->CreateActor("e1", inits_probe);
  mods::Mobile* probe_mobile = probe->Trait<mods::Mobile>();
  spawn = probe_mobile->NearestMoveableCell(spawn, 0, 12);
  world->Remove(probe);

  sim::TypeDictionary inits_a;
  inits_a.Add(new sim::OwnerInit(player_a));
  inits_a.Add(new sim::LocationInit(spawn));
  sim::Actor* attacker = world->CreateActor("e1", inits_a);
  mods::Mobile* attacker_mobile = attacker->Trait<mods::Mobile>();
  Check(attacker->IsInWorld(), "attacker in world");
  Check(attacker->Location() == spawn, "attacker spawned at LocationInit");

  mods::AttackFrontal* frontal = attacker->Trait<mods::AttackFrontal>();
  Check(frontal != nullptr, "e1 AttackFrontal resolved");
  CheckEq(frontal->Info().angle_facing_tolerance.Angle, 0,
          "FacingTolerance 0 (^Soldier)");
  mods::AutoTarget* auto_target = attacker->Trait<mods::AutoTarget>();
  Check(auto_target != nullptr, "e1 AutoTarget resolved");

  const mods::UnitStance stance_expected =
      player_a->IsBot() || !player_a->Playable()
          ? mods::UnitStance::AttackAnything
          : mods::UnitStance::Defend;
  CheckEq(static_cast<int>(auto_target->Stance()),
          static_cast<int>(stance_expected),
          "AutoTarget initial stance (AI/ Defend per player face)");
  Check(!attacker->TraitsImplementing<mods::HitShape>().empty(),
        "e1 HitShape resolved (^SpriteActor default Circle)");
  Check(!attacker->TraitsImplementing<mods::Armor>().empty(),
        "e1 Armor resolved (^Infantry Type None)");
  Check(attacker->TraitsImplementing<mods::Armor>()[0]->Info().str_type ==
            "None",
        "e1 armor type None");

  CheckEq(attacker->EnabledTargetablePositions().size(), std::size_t{1},
          "one enabled targetable position (HitShape)");
  CheckEq(attacker->GetTargetablePositions().size(), std::size_t{1},
          "GetTargetablePositions materialized");
  Check(attacker->GetTargetablePositions()[0] ==
            attacker->CenterPosition(),
        "default HitShape position == center");

  CPos far_cell = spawn + CVec{12, 0};
  if (!map.Contains(far_cell))
    far_cell = spawn + CVec{-12, 0};
  world->Tick();
  Check(shroud_a->IsVisible(attacker->Location()),
        "own cell visible (RevealsShroud source added)");
  Check(shroud_a->IsExplored(attacker->Location()),
        "own cell explored");
  Check(!shroud_a->IsVisible(far_cell),
        "12-cell-away cell not visible (range 4c0)");
  const int revealed_before = shroud_a->RevealedCells();
  Check(revealed_before > 0, "RevealedCells > 0 under fog");

  shroud_a->SetDisabled(true);
  world->Tick();
  Check(shroud_a->IsVisible(far_cell), "disabled shroud sees all");
  shroud_a->SetDisabled(false);
  world->Tick();
  Check(!shroud_a->IsVisible(far_cell), "re-enabled hides far cell again");

  CPos defender_cell =
      attacker_mobile->NearestMoveableCell(spawn + CVec{2, 0}, 0, 4);
  defender_cell =
      attacker_mobile->NearestMoveableCell(defender_cell, 0, 4);
  sim::TypeDictionary inits_b;
  inits_b.Add(new sim::OwnerInit(player_b));
  inits_b.Add(new sim::LocationInit(defender_cell));
  sim::Actor* defender = world->CreateActor("e1", inits_b);

  defender->Trait<mods::AutoTarget>()->SetStance(
      *defender, mods::UnitStance::HoldFire);
  sim::IHealth* defender_health = defender->Trait<sim::IHealth>();
  const int hp0 = defender_health->HP();
  CheckEq(hp0, 5000, "defender e1 HP 5000");

  Check(attacker->IsIdle(), "attacker idle before scan");

  {
    int scan_ticks = 0;
    while (attacker->IsIdle() && scan_ticks < 12) {
      world->Tick();
      ++scan_ticks;
    }
    Check(!attacker->IsIdle(),
          "AutoTarget idle scan queued an attack activity");
  }
  Check(auto_target->Aggressor != nullptr || !attacker->IsIdle(),
        "scan produced engagement");

  int ticks = 0;
  int hp_midpoint = hp0;
  bool saw_damage = false;
  while (ticks < 3000) {
    world->Tick();
    ++ticks;
    const int hp = defender_health->HP();
    if (!saw_damage && hp < hp0) {
      saw_damage = true;
      hp_midpoint = hp;
    }
    if (defender->IsDead())
      break;
  }
  Check(saw_damage, "defender HP decreased (warhead chain)");
  CheckEq(hp_midpoint, hp0 - 1500, "first hit leaves 3500 (1500 damage)");
  Check(defender->IsDead(), "defender killed within tick budget");
  std::println("attack: killed in {} ticks (HP {} → dead)", ticks, hp0);

  struct DummyFrozenCreator final : sim::ICreatesFrozenActors {
    void OnVisibilityChanged(sim::FrozenActor&) override {}
  } dummy_creator;

  const CPos ft_cell =
      attacker_mobile->NearestMoveableCell(spawn + CVec{0, 2}, 0, 4);
  sim::TypeDictionary inits_ft;
  inits_ft.Add(new sim::OwnerInit(player_a));
  inits_ft.Add(new sim::LocationInit(ft_cell));
  sim::Actor* freeze_target = world->CreateActor("e1", inits_ft);

  auto* frozen = world->Arena().Create<sim::FrozenActor>(
      *freeze_target, dummy_creator,
      std::vector<PPos>{ToPPos(ft_cell.ToMPos(map.Grid().Type))},
      *player_b, false);
  frozen->RefreshState();
  Check(frozen->IsValid(), "FrozenActor valid after RefreshState");
  frozen_layer->Add(frozen);
  Check(frozen_layer->FromID(freeze_target->ActorID()) == frozen,
        "FromID roundtrip");
  CheckEq(frozen_layer->FrozenActorsInCircle(
              *world, map.CenterOfCell(ft_cell), WDist{2 * 1024}, false)
              .size(),
          std::size_t{1},
          "FrozenActorsInCircle finds the actor (onlyVisible=false)");
  const int frozen_hash_before = frozen_layer->FrozenHash;
  world->Tick();
  Check(frozen_layer->FrozenHash != frozen_hash_before,
        "FrozenHash advances per tick");
  frozen->Invalidate();
  frozen_layer->Tick(*player_a->PlayerActor());

  Check(frozen_layer->FromID(freeze_target->ActorID()) == frozen,
        "live backing actor keeps the frozen entry");

  const game::WeaponInfo* weapon_cannon = nullptr;
  for (const auto& [name, weapon] :
       const_cast<sim::World&>(*world).Map().Rules().Weapons())
    if (name == "120mm") {
      weapon_cannon = weapon.get();
      break;
    }
  Check(weapon_cannon != nullptr, "120mm weapon found in rules");
  if (weapon_cannon != nullptr && weapon_cannon->rec_projectile != nullptr) {
    sim::IProjectileInfo* bullet_info =
        sim::ResolveProjectileInfo(*weapon_cannon->rec_projectile);
    Check(bullet_info != nullptr, "BulletInfo registered for 120mm");

    const CPos victim_cell = attacker_mobile->NearestMoveableCell(
        spawn + CVec{0, -2}, 0, 4);
    sim::TypeDictionary inits_bv;
    inits_bv.Add(new sim::OwnerInit(player_b));
    inits_bv.Add(new sim::LocationInit(victim_cell));
    sim::Actor* bullet_victim = world->CreateActor("e1", inits_bv);
    sim::IHealth* victim_health = bullet_victim->Trait<sim::IHealth>();
    const int bullet_hp0 = victim_health->HP();

    sim::ProjectileArgs args;
    args.weapon = weapon_cannon;
    args.source_actor = attacker;
    args.source = attacker->CenterPosition();
    args.passive_target = bullet_victim->CenterPosition();
    args.guided_target = sim::Target::FromActor(bullet_victim);
    world->Add(std::unique_ptr<sim::IEffect>(bullet_info->Create(args)));

    int bullet_ticks = 0;
    while (bullet_ticks < 200 && victim_health->HP() == bullet_hp0 &&
           !bullet_victim->IsDead()) {
      world->Tick();
      ++bullet_ticks;
    }
    Check(victim_health->HP() < bullet_hp0 || bullet_victim->IsDead(),
          "Bullet impact dealt damage");
    CheckEq(victim_health->HP(), bullet_hp0 - 1800,
          "Bullet damage 1800 (6000 × 30%)");
    std::println("bullet: impact after {} ticks", bullet_ticks);
  }

  Check(!attacker->SyncHashes().empty(),
        "attacker carries [VerifySync] trait hashes");
}

int main(int argc, char** argv) {
  setvbuf(stdout, nullptr, _IONBF, 0);
  if (argc < 2) {
    std::println("usage: attack_test <upstream-root>");
    return 1;
  }

  ora::gen::RegisterGeneratedAll();
  ora::game::RegisterGameLoaders();
  ora::sim::RegisterWorldTraits();
  ora::mods::RegisterCommonTraits();

  ora::mods::SetBodyOrientationFacingsResolver(
      [](const ora::game::ActorInfo&, const std::string&) { return 8; });

  TestRealRaAttackChain(argv[1]);

  if (g_failures == 0)
    std::println("attack_test: all passed");
  else
    std::println("attack_test: {} FAILURES", g_failures);
  return g_failures == 0 ? 0 : 1;
}
