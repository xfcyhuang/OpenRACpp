import std;

#include "game/game.hpp"

#include "render_sequences_fixture.hpp"
#include "meta/variable_expression.hpp"
#include "game/game_records.hpp"
#include "game/game_records.hpp"
#include "game/manifest.hpp"
#include "game/mod_data.hpp"
#include "map/map.hpp"
#include "map/map_cache.hpp"
#include "mods/body_orientation.hpp"
#include "mods/capture.hpp"
#include "mods/cloak.hpp"
#include "mods/conditions.hpp"
#include "mods/create_map_players.hpp"
#include "mods/experience.hpp"
#include "mods/mobile.hpp"
#include "mods/selectable.hpp"
#include "mods/spawn_map_actors.hpp"
#include "sim/actor.hpp"
#include "sim/actor_init.hpp"
#include "sim/frozen_actor_layer.hpp"
#include "sim/player.hpp"
#include "sim/shroud.hpp"
#include "sim/target.hpp"
#include "sim/trait_interfaces.hpp"
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

static ora::sim::Damage damage_pool{0};

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

int main(int argc, char** argv) {
  setvbuf(stdout, nullptr, _IONBF, 0);
  if (argc < 2) {
    std::println("usage: condition_test <upstream-root>");
    return 1;
  }

  ora::gen::RegisterGeneratedAll();
  ora::game::RegisterGameLoaders();
  ora::sim::RegisterWorldTraits();
  ora::mods::RegisterCommonTraits();

  ora::mods::SetBodyOrientationFacingsResolver(
      [](const ora::game::ActorInfo&, const std::string&) { return 8; });

  const char* str_upstream_root = argv[1];
  std::println("== condition_test ==");

  using namespace ora;
  game::InstalledMods mods_installed{std::string{str_upstream_root} + "/mods"};
  const game::Manifest* manifest = mods_installed.Find("ra");
  Check(manifest != nullptr, "ra mod found");
  if (manifest == nullptr)
    return g_failures != 0;
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
    return g_failures != 0;

  map::MapPreview& preview = cache.At(str_uid);
  auto map_world = preview.ToMap();
  Check(map_world != nullptr, "MapPreview.ToMap constructs");
  if (map_world == nullptr)
    return g_failures != 0;

  ora::testfx::InstallSyntheticSequences(*map_world, *manifest,
                                          mod_data, str_upstream_root);
  auto world = std::make_unique<sim::World>(
      *map_world, mod_data, *game_.OrderManagerFace(),
      sim::WorldType::Regular);
  world->LoadComplete(nullptr);
  Check(!world->Players().empty(), "map players created");
  if (world->Players().empty())
    return g_failures != 0;

  mods::SpawnMapActors* spawn_map_actors =
      world->WorldActor()->TraitOrDefault<mods::SpawnMapActors>();
  Check(spawn_map_actors != nullptr, "SpawnMapActors mounted on world actor");
  const std::size_t map_actor_definitions =
      world->Map().ActorDefinitions().size();
  Check(map_actor_definitions > 0, "map carries actor definitions");
  if (spawn_map_actors != nullptr) {
    CheckEq(spawn_map_actors->Actors().size(), map_actor_definitions,
            "every map actor spawned");
    Check(spawn_map_actors->LastMapActorID() > 0, "LastMapActorID recorded");

    bool owners_valid = true;
    for (const auto& [key, actor] : spawn_map_actors->Actors()) {
      (void)key;
      bool found = false;
      for (const sim::Player* p : world->Players())
        if (p == actor->Owner())
          found = true;
      owners_valid = owners_valid && found;
    }
    Check(owners_valid, "spawned actor owners all valid");
  }

  std::vector<sim::Player*> vec_combatants;
  for (sim::Player* p : world->Players())
    if (!p->NonCombatant())
      vec_combatants.push_back(p);
  while (vec_combatants.size() < 2) {
    std::size_t index = vec_combatants.size();
    if (index >= world->Players().size())
      index = 0;
    sim::Player* fallback = world->Players()[index];
    if (std::find(vec_combatants.begin(), vec_combatants.end(), fallback) !=
        vec_combatants.end())
      break;
    vec_combatants.push_back(fallback);
  }
  sim::Player* player_a = vec_combatants[0];
  sim::Player* player_b = vec_combatants[1];
  Check(player_a != player_b, "two distinct players");
  player_a->EnemyPlayersMask =
      player_a->EnemyPlayersMask.Union(player_b->PlayerMask);
  player_b->EnemyPlayersMask =
      player_b->EnemyPlayersMask.Union(player_a->PlayerMask);
  Check(player_a->RelationshipWith(player_b) ==
            sim::PlayerRelationship::Enemy,
        "A sees B as enemy (forced mask)");

  const Rectangle bounds = world->Map().Bounds();
  const int cx = bounds.Left() + bounds.Width / 2;
  const int cy = bounds.Top() + bounds.Height / 2;
  auto spawn = [&](const std::string& str_type, sim::Player* owner,
                   CPos cell,
                   std::optional<int> opt_facing = std::nullopt) {
    sim::TypeDictionary dict;
    sim::LocationInit location{cell};
    sim::OwnerInit owner_init{owner};

    sim::SkipMakeAnimsInit skip_make_anims;
    dict.Add(&location);
    dict.Add(&owner_init);
    dict.Add(&skip_make_anims);
    std::optional<sim::FacingInit> facing;
    if (opt_facing.has_value()) {
      facing.emplace(WAngle{*opt_facing});
      dict.Add(&*facing);
    }
    return world->CreateActor(true, str_type, dict);
  };

  sim::Actor* e1 = spawn("e1", player_a, CPos{cx, cy});
  Check(e1 != nullptr, "e1 constructed");
  if (e1 != nullptr) {
    mods::Selectable* selectable =
        e1->TraitOrDefault<mods::Selectable>();
    Check(selectable != nullptr, "e1 Selectable mounted");
    if (selectable != nullptr)
      CheckEq(std::string{selectable->Class()}, std::string{"E1"},
              "Selectable.Class defaults to the actor name");

    mods::GainsExperience* gains = e1->TraitOrDefault<mods::GainsExperience>();
    Check(gains != nullptr, "e1 GainsExperience mounted (^GainsExperience)");
    if (gains != nullptr) {
      Check(gains->CanGainLevel(), "e1 can gain levels");
      CheckEq(gains->Experience, 0, "experience starts at 0");

      gains->GiveExperience(20000);
      CheckEq(gains->Level, 1, "20000 experience reaches level 1");
      const auto& cache_conds = e1->ConditionCache();
      Check(cache_conds.count("rank-veteran") != 0 &&
                cache_conds.at("rank-veteran") >= 1,
            "rank-veteran granted at level 1");
    }

    sim::Actor* e1_prey = spawn("e1", player_b, CPos{cx + 2, cy});
    mods::GivesExperience* gives =
        e1_prey != nullptr ? e1_prey->TraitOrDefault<mods::GivesExperience>()
                           : nullptr;
    Check(gives != nullptr, "prey GivesExperience mounted (^Soldier)");
    if (gives != nullptr && gains != nullptr) {

      sim::AttackInfo ai;
      ai.Damage = &damage_pool;
      ai.Attacker = e1;
      gives->Killed(*e1_prey, ai);
      CheckEq(gains->Experience, 20000,
              "infantry kill credits zero (upstream quirk)");

      if (const mods::PlayerExperience* player_exp =
              player_a->PlayerActor()
                  ->TraitOrDefault<mods::PlayerExperience>())
        CheckEq(player_exp->Experience, 1,
                "player experience credits the kill score");
    }

    sim::Actor* dog = spawn("DOG", player_a, CPos{cx + 6, cy});
    Check(dog != nullptr, "DOG constructed");
    mods::GrantConditionWhileAiming* aiming =
        dog != nullptr
            ? dog->TraitOrDefault<mods::GrantConditionWhileAiming>()
            : nullptr;
    Check(aiming != nullptr, "DOG GrantConditionWhileAiming mounted");
    if (aiming != nullptr) {
      aiming->StartedAiming(*dog, nullptr);
      Check(dog->ConditionCache().count("run") != 0, "aiming grants run");
      aiming->StoppedAiming(*dog, nullptr);
      const auto& cache_run = dog->ConditionCache();
      Check(cache_run.count("run") == 0 || cache_run.at("run") == 0,
            "stopping aiming revokes run");
    }
  }

  sim::Actor* thf = spawn("THF", player_a, CPos{cx + 4, cy});
  Check(thf != nullptr, "THF constructed");
  mods::Cloak* cloak = thf != nullptr ? thf->TraitOrDefault<mods::Cloak>()
                                      : nullptr;

  mods::GrantConditionOnDamageState* uncloak_state = nullptr;
  if (thf != nullptr)
    for (mods::GrantConditionOnDamageState* state :
         thf->TraitsImplementing<mods::GrantConditionOnDamageState>()) {
      const auto& conds = thf->ConditionCache();
      (void)conds;

      if (uncloak_state == nullptr)
        uncloak_state = state;
    }
  Check(cloak != nullptr, "THF Cloak mounted");
  Check(uncloak_state != nullptr, "THF GrantConditionOnDamageState mounted");
  if (cloak != nullptr) {

    for (int i = 0; i < 251; i++)
      world->Tick();
    Check(cloak->Cloaked(), "THF cloaked after InitialDelay");
    Check(!cloak->IsVisible(*thf, player_b),
          "cloaked THF invisible to the enemy");
    Check(cloak->IsVisible(*thf, player_a),
          "cloaked THF visible to allies");

    mods::Mobile* thf_mobile = thf->TraitOrDefault<mods::Mobile>();
    if (thf_mobile != nullptr) {
      thf_mobile->SetPosition(thf, CPos{cx + 5, cy}, sim::SubCell::Any);
      world->Tick();
      Check(!cloak->Cloaked(), "movement uncloaks (UncloakOn Move)");

      for (int i = 0; i < 121; i++)
        world->Tick();
      Check(cloak->Cloaked(), "re-cloaks after CloakDelay elapses");
    }

    if (uncloak_state != nullptr) {
      const sim::IHealth* health = thf->Trait<sim::IHealth>();
      const int damage = health->HP() - health->MaxHP() / 8;
      damage_pool.Value = damage;
      thf->Trait<sim::IHealth>()->InflictDamage(*thf, nullptr, damage_pool,
                                                false);
      const auto& cache_c = thf->ConditionCache();
      Check(cache_c.count("cloak-force-disabled") != 0,
            "Critical damage grants cloak-force-disabled");

      const int int4_manual_token = thf->GrantCondition("cloak-force-disabled");
      Check(cloak->IsTraitPaused(),
            "the manual grant re-pauses the cloak");
      thf->RevokeCondition(int4_manual_token);
      Check(cloak->IsTraitPaused(),
            "cloak-force-disabled pauses the cloak");

      for (int i = 0; i < 130; i++)
        world->Tick();
      Check(!cloak->Cloaked(), "paused cloak stays uncloaked");
    }
  }

  sim::Actor* tent = spawn("TENT", player_b, CPos{cx + 4, cy + 7});
  Check(tent != nullptr, "TENT constructed");
  sim::Actor* e6 = spawn("E6", player_a, CPos{cx + 3, cy + 8});
  Check(e6 != nullptr, "E6 constructed");
  if (tent != nullptr && e6 != nullptr) {
    mods::CaptureManager* e6_manager =
        e6->TraitOrDefault<mods::CaptureManager>();
    mods::CaptureManager* tent_manager =
        tent->TraitOrDefault<mods::CaptureManager>();
    Check(e6_manager != nullptr, "E6 CaptureManager mounted");
    Check(tent_manager != nullptr, "TENT CaptureManager mounted");
    Check(e6_manager != nullptr && tent_manager != nullptr &&
              e6_manager->CanTarget(*tent_manager),
          "E6 can target the enemy TENT");

    player_a->GetShroud()->ExploreAll();

    for (int i = 0; i < 4; i++) {
      world->TickRender();
      world->Tick();
    }

    e6->GrantCondition("global-reusable-engineers");

    sim::Target tent_target = sim::Target::FromActor(tent);
    net::Order order{"CaptureActor", e6, tent_target, false};
    int captures_count = 0;
    for (mods::Captures* captures :
         e6->TraitsImplementing<mods::Captures>()) {
      captures->ResolveOrder(*e6, order);
      captures_count++;
    }
    Check(captures_count >= 1, "E6 Captures mounted");
    Check(e6->CurrentActivity() != nullptr,
          "CaptureActor queued on the engineer");

    for (int i = 0; i < 700; i++) {
      world->TickRender();
      world->Tick();
      if (tent->Owner() == player_a)
        break;
    }
    CheckEq(tent->Owner()->InternalName(), player_a->InternalName(),
            "engineer capture flips the TENT owner");
    Check(!e6->Disposed(),
          "the reusable engineer survives the capture");
  }

  for (int i = 0; i < 10; i++)
    world->Tick();
  const int sync_hash = world->SyncHash();
  std::println("SyncHash after batch-7 chain: {}", sync_hash);

  std::println("{}",
               g_failures == 0 ? "condition_test: ALL PASS"
                              : "condition_test: FAILURES");
  return g_failures != 0;
}
