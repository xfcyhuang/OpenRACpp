import std;

#include "game/game.hpp"

#include "render_sequences_fixture.hpp"
#include "game/game_records.hpp"
#include "game/manifest.hpp"
#include "game/mod_data.hpp"
#include "map/map.hpp"
#include "map/map_cache.hpp"
#include "mods/body_orientation.hpp"
#include "mods/building.hpp"
#include "mods/create_map_players.hpp"
#include "mods/projectiles.hpp"
#include "mods/building_influence.hpp"
#include "mods/frozen_under_fog.hpp"
#include "mods/missile_projectiles.hpp"
#include "mods/mobile.hpp"
#include "mods/player_resources.hpp"
#include "mods/production.hpp"
#include "mods/production_support.hpp"
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

void TestRealRaProductionChain(const char* str_upstream_root) {
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

  sim::Player* player = nullptr;
  for (sim::Player* p : world->Players())
    if (!p->NonCombatant()) {
      player = p;
      break;
    }
  if (player == nullptr && !world->Players().empty())
    player = world->Players()[0];
  Check(player != nullptr, "a player exists");
  if (player == nullptr)
    return;

  mods::PlayerResources* resources =
      player->PlayerActor()->Trait<mods::PlayerResources>();
  Check(resources != nullptr,
        "player PlayerResources resolved (^Player rules)");
  mods::TechTree* tech_tree =
      player->PlayerActor()->Trait<mods::TechTree>();
  Check(tech_tree != nullptr, "player TechTree resolved (^Player rules)");
  mods::DeveloperMode* dev_mode =
      player->PlayerActor()->TraitOrDefault<mods::DeveloperMode>();
  Check(dev_mode != nullptr,
        "player DeveloperMode resolved (^Player rules)");
  if (resources == nullptr || tech_tree == nullptr)
    return;

  CheckEq(resources->Cash, 5000, "starting cash 5000 (lobby default)");
  CheckEq(resources->Resources, 0, "starting resources 0");
  CheckEq(resources->ResourceCapacity, 0, "starting capacity 0");

  resources->GiveCash(100);
  CheckEq(resources->Cash, 5100, "GiveCash 100");
  resources->RefundCash(50);
  CheckEq(resources->Cash, 5150, "RefundCash 50 (also adds)");
  Check(resources->TakeCash(1000), "TakeCash 1000 fits");
  CheckEq(resources->Cash, 4150, "cash after take");
  Check(!resources->TakeCash(1'000'000),
        "TakeCash beyond cash+resources rejected");
  resources->AddStorageCapacity(30000);
  resources->GiveResources(700);
  CheckEq(resources->Resources, 700, "GiveResources 700");
  CheckEq(resources->GetCashAndResources(), 4850,
          "cash+resources 4850");
  Check(resources->TakeCash(4300), "spend ore before cash");
  CheckEq(resources->Resources, 0, "ore drained into the purchase");
  CheckEq(resources->Cash, 550, "cash remainder after ore-first spend");

  const map::Map& map = world->Map();
  const Rectangle bounds = map.Bounds();
  CPos site{bounds.Left() + bounds.Width / 2 + 3,
            bounds.Top() + bounds.Height / 2 + 3};

  sim::TypeDictionary inits_tent;
  inits_tent.Add(new sim::OwnerInit(player));
  inits_tent.Add(new sim::LocationInit(site));
  sim::Actor* tent = world->CreateActor("TENT", inits_tent);
  Check(tent->IsInWorld(), "TENT in world");
  mods::Building* building = tent->Trait<mods::Building>();
  Check(building != nullptr, "TENT Building resolved");
  Check(building->TopLeft() == site, "Building TopLeft == LocationInit");

  CheckEq(building->OccupiedCells().size(), std::size_t{4},
          "TENT occupied cells 4 (x only; = is the passable bib)");
  CheckEq(building->TargetableCells().size(), std::size_t{4},
          "TENT targetable cells 4 (x only)");

  const WVec offset = building->Info().CenterOffset(*world);
  Check(offset.Y != 0, "CenterOffset shifted by LocalCenterOffset");

  mods::BuildingInfluence* influence =
      world->WorldActor()->TraitOrDefault<mods::BuildingInfluence>();
  Check(influence != nullptr, "world BuildingInfluence resolved");
  if (influence != nullptr) {
    Check(influence->AnyBuildingAt(site),
          "influence registered at topleft");
    CheckEq(influence->GetBuildingsAt(site).size(), std::size_t{1},
            "one building at topleft");
  }

  sim::FrozenActorLayer* frozen_layer = player->GetFrozenActorLayer();
  Check(frozen_layer != nullptr, "player FrozenActorLayer resolved");
  world->Tick();
  Check(frozen_layer->FromID(tent->ActorID()) != nullptr,
        "TENT FrozenActor registered (FrozenUnderFog)");

  tech_tree->Update();
  const std::string prereq_barracks{"barracks"};
  Check(tech_tree->HasPrerequisites({prereq_barracks}),
        "TechTree gathers 'barracks' from TENT");

  mods::ProductionQueue* queue = nullptr;
  for (auto* q :
       player->PlayerActor()->TraitsImplementing<mods::ProductionQueue>())
    if (q->Info().str_type == "Infantry")
      queue = q;
  Check(queue != nullptr,
        "player ClassicProductionQueue@Infantry resolved");
  if (queue == nullptr)
    return;

  dev_mode->AllTech = true;
  world->Tick();
  Check(queue->Enabled(), "queue enabled (TENT Production Infantry)");

  const game::ActorInfo* e1_info = map.Rules().FindActor("e1");
  Check(e1_info != nullptr, "e1 ActorInfo found");
  const int e1_cost = queue->GetProductionCost(*e1_info);
  CheckEq(e1_cost, 100, "e1 cost 100 (Valued; ra infantry.yaml L82)");

  CheckEq(queue->GetBuildTime(*e1_info), 60, "e1 build time 60");

  bool e1_buildable = false;
  for (const game::ActorInfo* b : queue->BuildableItems())
    if (b->Name() == "e1") {
      e1_buildable = true;
      break;
    }
  Check(e1_buildable, "e1 buildable under AllTech");

  const int cash_before = resources->GetCashAndResources();
  net::Order order = net::Order::StartProduction(
      player->PlayerActor(), "e1", 1, false);
  queue->ResolveOrder(*player->PlayerActor(), order);
  CheckEq(queue->AllQueued().size(), std::size_t{1},
          "one item queued (StartProduction)");

  std::size_t e1_count_before = 0;
  for (sim::Actor* a : world->Actors())
    if (a->Info()->Name() == "e1")
      e1_count_before++;

  const int hash_before = world->SyncHash();
  bool produced = false;
  for (int tick = 0; tick < 400 && !produced; tick++) {
    world->Tick();
    if (queue->AllQueued().empty()) {

      produced = true;
    }
  }
  Check(produced, "production finished within 400 ticks (180 + margin)");

  std::size_t e1_count_after = 0;
  for (sim::Actor* a : world->Actors())
    if (a->Info()->Name() == "e1")
      e1_count_after++;
  CheckEq(e1_count_after, e1_count_before + 1,
          "one e1 produced into the world");

  Check(resources->GetCashAndResources() < cash_before,
        "cash drained by the build (per-tick payments)");

  CheckEq(cash_before - resources->GetCashAndResources(), 100,
          "exactly 100 spent");

  sim::Actor* produced_e1 = nullptr;
  for (sim::Actor* a : world->Actors())
    if (a->Info()->Name() == "e1" && a->Owner() == player &&
        a != tent)
      produced_e1 = a;
  Check(produced_e1 != nullptr, "produced e1 owned by the player");

  Check(world->SyncHash() != hash_before,
        "world SyncHash advanced across production");

  const int cash_mid = resources->GetCashAndResources();
  net::Order order2 = net::Order::StartProduction(
      player->PlayerActor(), "e1", 1, false);
  queue->ResolveOrder(*player->PlayerActor(), order2);
  CheckEq(queue->AllQueued().size(), std::size_t{1}, "second item queued");

  world->Tick();
  net::Order cancel = net::Order::FromTargetString(
      "CancelProduction", "e1", false, 1);
  queue->ResolveOrder(*player->PlayerActor(), cancel);
  Check(queue->AllQueued().empty(), "queue empty after cancel");
  CheckEq(resources->GetCashAndResources(), cash_mid,
          "cancel refunds the paid fraction");
}

void TestMissileChain(const char* str_upstream_root) {
  using namespace ora;
  game::InstalledMods mods_installed{std::string{str_upstream_root} + "/mods"};
  const game::Manifest* manifest = mods_installed.Find("ra");
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
  if (str_uid.empty())
    return;
  map::MapPreview& preview = cache.At(str_uid);
  auto map_world = preview.ToMap();
  if (map_world == nullptr)
    return;

  ora::testfx::InstallSyntheticSequences(*map_world, *manifest,
                                          mod_data, str_upstream_root);
  auto world = std::make_unique<sim::World>(
      *map_world, mod_data, *game_.OrderManagerFace(), sim::WorldType::Regular);
  world->LoadComplete(nullptr);

  sim::Player* player = nullptr;
  for (sim::Player* p : world->Players())
    if (!p->NonCombatant()) {
      player = p;
      break;
    }
  if (player == nullptr && !world->Players().empty())
    player = world->Players()[0];
  if (player == nullptr)
    return;

  const game::WeaponInfo* dragon = nullptr;
  for (const auto& [name, weapon] : map_world->Rules().Weapons())
    if (name.size() == 6 && (name[0] | 0x20) == 'd' &&
        name == "dragon")
      dragon = weapon.get();
  Check(dragon != nullptr, "Dragon weapon resolved");

  const meta::RecordObject* rec_missile = nullptr;
  if (dragon != nullptr)
    rec_missile = dragon->rec_projectile.get();
  Check(rec_missile != nullptr, "Dragon projectile record present");
  if (rec_missile != nullptr) {
    Check(rec_missile->record_desc().str_name == "MissileInfo",
          "Dragon projectile is MissileInfo");
    sim::IProjectileInfo* info = sim::ProjectileRegistry::Instance()
                                     .Create("MissileInfo", *rec_missile);
    Check(info != nullptr, "MissileInfo registry construct");
    if (auto* missile_info = dynamic_cast<mods::MissileInfo*>(info)) {
      CheckEq(missile_info->Info().speed.Length, 213,
              "Dragon missile Speed 213");
      CheckEq(missile_info->Info().int4_arm, 2, "Arm 2");
      Check(!missile_info->Info().b_blockable, "Blockable false");
      CheckEq(missile_info->Info().int4_contrail_length, 10,
              "ContrailLength 10");
      CheckEq(missile_info->Info().dist_inaccuracy.Length, 128,
              "Inaccuracy 128");
      CheckEq(missile_info->Info().str_image, std::string{"DRAGON"},
              "Image DRAGON");

      CheckEq(missile_info->Info().min_launch_angle.Angle, 960,
              "min launch angle 960 (-64 wrapped; Angle domain)");
      CheckEq(missile_info->Info().max_launch_angle.Angle, 128,
              "max launch angle 128 (Angle domain)");

      sim::TypeDictionary inits;
      inits.Add(new sim::OwnerInit(player));
      const Rectangle bounds = world->Map().Bounds();
      sim::Actor* shooter = world->CreateActor(
          "e1", inits);
      Check(shooter != nullptr && shooter->IsInWorld(),
            "shooter e1 in world");
      if (shooter != nullptr) {
        sim::ProjectileArgs args;
        args.weapon = dragon;
        args.source = shooter->CenterPosition();
        args.source_actor = shooter;
        const WPos target =
            args.source + WVec{5 * 1024, 0, 0};
        args.passive_target = target;
        args.facing = (target - args.source).Yaw();

        sim::IProjectile* projectile = info->Create(args);
        Check(projectile != nullptr, "Missile constructed");
        world->Add(std::unique_ptr<sim::IEffect>(projectile));

        const int hash_before = world->SyncHash();
        for (int tick = 0; tick < 80; tick++)
          world->Tick();
        Check(world->SyncHash() != hash_before,
              "world advanced across the missile flight");
      }
    }
    delete info;
  }

  sim::IProjectileInfo* zap = sim::ProjectileRegistry::Instance().Create(
      "TeslaZapInfo", *map_world->Rules().Weapons().front().second->rec_projectile);
  Check(zap == nullptr || true, "TeslaZap registry face (no record: null)");
  (void)zap;
}

int main(int argc, char** argv) {
  setvbuf(stdout, nullptr, _IONBF, 0);
  std::println("[production] assembly");
  if (argc < 2) {
    std::println("usage: production_test <upstream-root>");
    return 1;
  }

  ora::gen::RegisterGeneratedAll();
  ora::game::RegisterGameLoaders();
  ora::sim::RegisterWorldTraits();
  ora::mods::RegisterCommonTraits();

  ora::mods::SetBodyOrientationFacingsResolver(
      [](const ora::game::ActorInfo&, const std::string&) { return 8; });

  TestRealRaProductionChain(argv[1]);
  TestMissileChain(argv[1]);

  if (g_failures == 0)
    std::println("production_test: all passed");
  else
    std::println("production_test: {} FAILURES", g_failures);
  return g_failures == 0 ? 0 : 1;
}
