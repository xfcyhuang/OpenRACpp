import std;

#include "game/game.hpp"

#include "render_sequences_fixture.hpp"
#include "game/game_records.hpp"
#include "game/manifest.hpp"
#include "game/mod_data.hpp"
#include "map/map.hpp"
#include "map/map_cache.hpp"
#include "mods/attack_follow.hpp"
#include "mods/body_orientation.hpp"
#include "mods/building.hpp"
#include "mods/create_map_players.hpp"
#include "mods/dock_client.hpp"
#include "mods/dock_host.hpp"
#include "mods/harvester.hpp"
#include "mods/health.hpp"
#include "mods/mobile.hpp"
#include "mods/player_resources.hpp"
#include "mods/power.hpp"
#include "mods/production.hpp"
#include "mods/production_support.hpp"
#include "mods/resource_layer.hpp"
#include "mods/stores_resources.hpp"
#include "mods/turreted.hpp"
#include "sim/actor.hpp"
#include "sim/actor_init.hpp"
#include "sim/player.hpp"
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

struct TestWorld {
  ora::game::InstalledMods mods_installed;
  ora::game::ModData mod_data;
  ora::game::Game game_;

  std::unique_ptr<ora::map::Map> map_world;
  std::unique_ptr<ora::sim::World> world;
  ora::map::MapCache cache;
  std::string str_uid;
  std::string str_root;
  ora::sim::Player* player = nullptr;

  explicit TestWorld(const char* str_upstream_root, int combatants = 1)
      : mods_installed{std::string{str_upstream_root} + "/mods"},
        str_root{str_upstream_root},
        mod_data{*mods_installed.Find("ra"), mods_installed,
                 str_upstream_root},
        game_{ora::game::Game::Deps{
            .ptr_mod_data = &mod_data}},
        cache{mod_data.ManifestRef(), mod_data.ModFiles()} {
    game_.JoinLocal();
    cache.LoadMaps(mod_data, TestMapFilter());

    for (ora::map::MapPreview* p : cache.Previews()) {
      if (p->Status() != ora::map::MapStatus::Available)
        continue;
      int count = 0;
      for (const auto& [name, reference] : p->Players().Players)
        if (!reference.NonCombatant)
          count++;
      if (count >= combatants) {
        str_uid = p->Uid();
        std::println("map: {} ({})", p->Title(), p->Path());
      std::println("map: {} ({})", p->Title(), p->Path());
        break;
      }
    }
  }

  bool Build() {
    if (str_uid.empty())
      return false;
    map_world = cache.At(str_uid).ToMap();
    if (map_world == nullptr)
      return false;
    ora::testfx::InstallSyntheticSequences(
        *map_world, mod_data.ManifestRef(), mod_data, str_root.c_str());
    world = std::make_unique<ora::sim::World>(
        *map_world, mod_data, *game_.OrderManagerFace(),
        ora::sim::WorldType::Regular);
    world->LoadComplete(nullptr);
    for (ora::sim::Player* p : world->Players())
      if (!p->NonCombatant()) {
        player = p;
        break;
      }
    if (player == nullptr && !world->Players().empty())
      player = world->Players()[0];
    return player != nullptr;
  }
};

void TestPowerChain(const char* str_upstream_root) {
  using namespace ora;
  std::println("[resource] power-chain");
  TestWorld tw{str_upstream_root};
  Check(tw.Build(), "power test world built");
  if (tw.world == nullptr)
    return;
  sim::World& world = *tw.world;
  sim::Player* player = tw.player;

  mods::PowerManager* pm =
      player->PlayerActor()->Trait<mods::PowerManager>();
  Check(pm != nullptr, "player PowerManager resolved (^Player rules)");
  if (pm == nullptr)
    return;
  CheckEq(pm->PowerProvided, 0, "power provided starts 0");
  CheckEq(pm->PowerDrained, 0, "power drained starts 0");
  CheckEq(static_cast<int>(pm->GetPowerState()),
          static_cast<int>(sim::PowerState::Normal),
          "power state Normal at 0/0");

  const Rectangle bounds = world.Map().Bounds();
  const CPos powr_site{bounds.Left() + bounds.Width / 2 - 6,
                       bounds.Top() + bounds.Height / 2 - 6};
  sim::TypeDictionary inits_powr;
  inits_powr.Add(new sim::OwnerInit(player));
  inits_powr.Add(new sim::LocationInit(powr_site));
  sim::Actor* powr = world.CreateActor("POWR", inits_powr);
  Check(powr != nullptr && powr->IsInWorld(), "POWR in world");
  CheckEq(pm->PowerProvided, 100, "POWR provides 100");

  for (int i = 0; i < 3; i++) {
    sim::TypeDictionary inits_tent;
    inits_tent.Add(new sim::OwnerInit(player));
    inits_tent.Add(new sim::LocationInit(
        CPos{bounds.Left() + bounds.Width / 2 + 2 + i * 3,
             bounds.Top() + bounds.Height / 2 + 3}));
    world.CreateActor("TENT", inits_tent);
  }
  CheckEq(pm->PowerDrained, 60, "three TENTs drain 60");
  CheckEq(pm->ExcessPower(), 40, "excess 40");
  CheckEq(static_cast<int>(pm->GetPowerState()),
          static_cast<int>(sim::PowerState::Normal),
          "power state Normal at 100/60");

  for (int i = 0; i < 3; i++) {
    sim::TypeDictionary inits_tent;
    inits_tent.Add(new sim::OwnerInit(player));
    inits_tent.Add(new sim::LocationInit(
        CPos{bounds.Left() + bounds.Width / 2 + 2 + i * 3,
             bounds.Top() + bounds.Height / 2 + 6}));
    world.CreateActor("TENT", inits_tent);
  }
  CheckEq(pm->PowerDrained, 120, "six TENTs drain 120");
  CheckEq(static_cast<int>(pm->GetPowerState()),
          static_cast<int>(sim::PowerState::Low),
          "power state Low at 100/120");

  mods::DeveloperMode* dev_mode =
      player->PlayerActor()->Trait<mods::DeveloperMode>();
  Check(dev_mode != nullptr, "player DeveloperMode resolved");
  if (dev_mode != nullptr) {
    dev_mode->UnlimitedPower = true;
    world.Tick();
    CheckEq(pm->PowerProvided, 0, "unlimited power zeroes provided");
    CheckEq(pm->PowerDrained, 0, "unlimited power zeroes drained");
    CheckEq(static_cast<int>(pm->GetPowerState()),
            static_cast<int>(sim::PowerState::Normal),
            "power state Normal under unlimited power");
    dev_mode->UnlimitedPower = false;
    world.Tick();
    CheckEq(pm->PowerProvided, 100, "power rebuilt after cheat off");
    CheckEq(pm->PowerDrained, 120, "drain rebuilt after cheat off");
  }

  if (dev_mode != nullptr)
    dev_mode->AllTech = true;
  world.Tick();
  mods::ProductionQueue* queue = nullptr;
  for (auto* q :
       player->PlayerActor()->TraitsImplementing<mods::ProductionQueue>())
    if (q->Info().str_type == "Infantry")
      queue = q;
  Check(queue != nullptr, "player ClassicProductionQueue@Infantry resolved");
  if (queue != nullptr) {
    net::Order order = net::Order::StartProduction(
        player->PlayerActor(), "e1", 1, false);
    queue->ResolveOrder(*player->PlayerActor(), order);
    CheckEq(queue->AllQueued().size(), std::size_t{1},
            "one e1 queued under low power");
    world.Tick();
    if (!queue->AllQueued().empty()) {
      const int remaining = queue->AllQueued()[0]->RemainingTime();
      const int actual = queue->AllQueued()[0]->RemainingTimeActual();
      CheckEq(actual, remaining * 300 / 100,
              "low-power build time ×3 (LowPowerModifier 300)");

      net::Order cancel = net::Order::FromTargetString(
          "CancelProduction", "e1", false, 1);
      queue->ResolveOrder(*player->PlayerActor(), cancel);
    }
  }

  pm->TriggerPowerOutage(5);
  CheckEq(pm->PowerOutageRemainingTicks, 5, "outage ticks set");
  for (int i = 0; i < 5; i++)
    world.Tick();
  CheckEq(pm->PowerOutageRemainingTicks, 0, "outage ticks elapse");
}

void TestTurretChain(const char* str_upstream_root) {
  using namespace ora;
  std::println("[resource] turret-chain");
  TestWorld tw{str_upstream_root};
  Check(tw.Build(), "turret test world built");
  if (tw.world == nullptr)
    return;
  sim::World& world = *tw.world;
  sim::Player* player = tw.player;

  sim::Player* enemy = nullptr;
  for (sim::Player* p : world.Players())
    if (p != player) {
      enemy = p;
      break;
    }
  Check(enemy != nullptr, "an opposing player exists");
  if (enemy == nullptr)
    return;

  const Rectangle bounds = world.Map().Bounds();
  const CPos center{bounds.Left() + bounds.Width / 2,
                    bounds.Top() + bounds.Height / 2};

  sim::TypeDictionary inits_tank;
  inits_tank.Add(new sim::OwnerInit(player));
  inits_tank.Add(new sim::LocationInit(center));
  sim::Actor* tank = world.CreateActor("2tnk", inits_tank);
  Check(tank != nullptr && tank->IsInWorld(), "2tnk in world");
  if (tank == nullptr)
    return;

  mods::Turreted* turret = tank->Trait<mods::Turreted>();
  Check(turret != nullptr, "2tnk Turreted resolved");
  mods::AttackTurreted* attack =
      tank->Trait<mods::AttackTurreted>();
  Check(attack != nullptr, "2tnk AttackTurreted resolved");
  if (turret == nullptr || attack == nullptr)
    return;
  CheckEq(attack->Turrets().size(), std::size_t{1},
          "AttackTurreted binds one Turreted");
  CheckEq(turret->Name(), std::string{"primary"},
          "Turreted name 'primary'");
  CheckEq(turret->Info().angle_turn_speed.Angle, 20,
          "2tnk Turreted TurnSpeed 20");

  sim::TypeDictionary inits_e1;
  inits_e1.Add(new sim::OwnerInit(enemy));
  inits_e1.Add(new sim::LocationInit(
      CPos{center.X() + 2, center.Y()}));
  sim::Actor* e1 = world.CreateActor("e1", inits_e1);
  Check(e1 != nullptr && e1->IsInWorld(), "enemy e1 in world");
  if (e1 == nullptr)
    return;
  mods::Health* e1_health = e1->Trait<mods::Health>();
  Check(e1_health != nullptr, "e1 Health resolved");
  if (e1_health == nullptr)
    return;
  const int hp0 = e1_health->HP();

  net::Order attack_order = net::Order(
      "ForceAttack", tank, sim::Target::FromActor(e1), false);
  tank->ResolveOrder(attack_order);

  bool damaged = false;
  bool aiming_seen = false;
  bool killed = false;
  for (int tick = 0; tick < 400; tick++) {
    if (tick % 100 == 0)
      std::println("[resource] turret-chain tick {}", tick);
    world.Tick();
    if (attack->IsAiming())
      aiming_seen = true;
    if (!damaged && e1_health->HP() < hp0)
      damaged = true;
    if (e1->IsDead()) {
      killed = true;
      break;
    }
  }
  Check(aiming_seen, "AttackTurreted aimed (turret tracking)");
  Check(damaged, "90mm damaged the e1 (projectile impact)");
  Check(killed, "e1 killed within 400 ticks");

  if (killed) {
    for (int tick = 0; tick < 80; tick++)
      world.Tick();
    Check(turret->HasAchievedDesiredFacing(),
          "turret realigned after the target died");
  }
}

void TestResourceChain(const char* str_upstream_root) {
  using namespace ora;
  std::println("[resource] harvest-chain");
  TestWorld tw{str_upstream_root};
  Check(tw.Build(), "resource test world built");
  if (tw.world == nullptr)
    return;
  sim::World& world = *tw.world;
  sim::Player* player = tw.player;
  const map::Map& map = world.Map();
  const Rectangle bounds = map.Bounds();

  mods::ResourceLayer* layer =
      world.WorldActor()->Trait<mods::ResourceLayer>();
  Check(layer != nullptr, "world ResourceLayer resolved");
  mods::ResourceClaimLayer* claims =
      world.WorldActor()->Trait<mods::ResourceClaimLayer>();
  Check(claims != nullptr, "world ResourceClaimLayer resolved");
  if (layer == nullptr || claims == nullptr)
    return;

  const int max_density = layer->GetMaxDensity("Ore");
  CheckEq(static_cast<int>(max_density), 12, "ra Ore MaxDensity 12");
  CheckEq(static_cast<int>(layer->GetMaxDensity("NoSuchType")), 0,
          "unknown type max density 0");

  CPos clear_cell{bounds.Left() + bounds.Width / 2 + 8,
                  bounds.Top() + bounds.Height / 2 + 8};

  for (const CPos cell : map.AllCells()) {
    if (map.Contains(cell) &&
        map.GetTerrainInfo(cell).Type == "Clear" &&
        map.Ramp().Get(cell) == 0 &&
        layer->GetResource(cell).str_type.empty() &&
        world.ActorMapFace()->GetActorsAt(cell).empty()) {
      clear_cell = cell;
      break;
    }
  }
  Check(layer->CanAddResource("Ore", clear_cell, 1),
        "Clear terrain accepts Ore");
  CheckEq(layer->AddResource("Ore", clear_cell, 5), 5,
          "AddResource 5 on empty cell adds 5");

  CheckEq(static_cast<int>(layer->GetResource(clear_cell).uint1_density), 6,
          "density 6 after add 5 (clamp(1) floor)");
  CheckEq(layer->AddResource("Ore", clear_cell, 200), 6,
          "density clamps to MaxDensity 12 (adds 6)");
  Check(!layer->CanAddResource("Ore", clear_cell, 1),
        "full cell rejects more");
  CheckEq(layer->RemoveResource("Ore", clear_cell, 3), 3,
          "RemoveResource 3");

  CheckEq(layer->RemoveResource("Ore", clear_cell, 9), 9,
          "removing the exact density returns 9 (cell empties)");
  Check(layer->GetResource(clear_cell).str_type.empty(),
        "cell empty after full removal");
  CheckEq(static_cast<int>(map.CustomTerrain().Get(clear_cell)), 255,
          "custom terrain cleared to 255");

  sim::TypeDictionary inits_a, inits_b;
  inits_a.Add(new sim::OwnerInit(player));
  inits_b.Add(new sim::OwnerInit(player));
  sim::Actor* harv_a = world.CreateActor(
      "HARV", inits_a);
  Check(claims->TryClaimCell(*harv_a, clear_cell),
        "first claim succeeds");
  Check(claims->TryClaimCell(*harv_a, clear_cell),
        "self re-claim succeeds");
  Check(claims->CanClaimCell(*harv_a, CPos{clear_cell.X() + 1,
                                           clear_cell.Y() + 1}),
        "unclaimed cell claimable");
  claims->RemoveClaim(*harv_a);
  Check(claims->CanClaimCell(*harv_a, clear_cell),
        "claim released");

  CPos proc_site{bounds.Left() + bounds.Width / 2,
                 bounds.Top() + bounds.Height / 2};
  bool site_found = false;
  for (int dy = 0; dy < 40 && !site_found; dy++) {
    for (int dx = 0; dx < 40 && !site_found; dx++) {
      const CPos site{bounds.Left() + bounds.Width / 2 + dx,
                      bounds.Top() + bounds.Height / 2 + dy};
      bool b_ok = map.Contains(site);
      for (int y = 0; b_ok && y < 4; y++)
        for (int x = 0; b_ok && x < 3; x++) {
          const CPos c{site.X() + x, site.Y() + y};
          b_ok = map.Contains(c) &&
                 map.GetTerrainInfo(c).Type != "Water";
        }
      if (b_ok) {
        proc_site = site;
        site_found = true;
      }
    }
  }
  Check(site_found, "a PROC site found");

  sim::TypeDictionary inits_proc;
  inits_proc.Add(new sim::OwnerInit(player));
  inits_proc.Add(new sim::LocationInit(proc_site));
  sim::Actor* proc = world.CreateActor("PROC", inits_proc);
  Check(proc != nullptr && proc->IsInWorld(), "PROC in world");
  mods::DockHost* dock = proc->Trait<mods::DockHost>();
  Check(dock != nullptr, "PROC DockHost resolved");
  if (proc == nullptr || dock == nullptr)
    return;
  Check(dock->GetDockType().RawBits() != 0,
        "DockHost type carries bits (Unload)");

  mods::PlayerResources* resources =
      player->PlayerActor()->Trait<mods::PlayerResources>();
  Check(resources != nullptr, "player PlayerResources resolved");
  CheckEq(resources->ResourceCapacity, 2000,
          "PROC StoresPlayerResources adds 2000 capacity");

  sim::TypeDictionary inits_harv;
  inits_harv.Add(new sim::OwnerInit(player));
  CPos harv_site{-1, -1};
  bool harv_site_found = false;
  for (int r = 2; r < 12 && !harv_site_found; r++) {
    for (int dy = -r; dy <= r && !harv_site_found; dy++) {
      for (int dx = -r; dx <= r && !harv_site_found; dx++) {
        const CPos c{proc_site.X() + dx, proc_site.Y() + dy};
        if (map.Contains(c) &&
            map.GetTerrainInfo(c).Type == "Clear" &&
            map.Ramp().Get(c) == 0 &&
            layer->GetResource(c).str_type.empty() && c != clear_cell) {
          harv_site = c;
          harv_site_found = true;
        }
      }
    }
  }
  Check(harv_site_found, "a HARV site near the PROC found");
  inits_harv.Add(new sim::LocationInit(harv_site));
  sim::Actor* harv = world.CreateActor("HARV", inits_harv);
  Check(harv != nullptr && harv->IsInWorld(), "HARV in world");
  if (harv == nullptr)
    return;
  mods::Harvester* harvester = harv->Trait<mods::Harvester>();
  Check(harvester != nullptr, "HARV Harvester resolved");
  Check(harvester->IsEmpty(), "harvester starts empty");
  Check(harvester->GetDockClientManager() != nullptr,
        "HARV DockClientManager resolved");

  for (int i = 0; i < 3; i++) {
    const CPos c{harv_site.X() + 1, harv_site.Y() + i};
    if (layer->CanAddResource("Ore", c, 1))
      layer->AddResource("Ore", c, 12);
  }

  const int resources0 = resources->Resources + resources->Cash;
  bool harvested = false;
  bool docked = false;
  bool credited = false;
  for (int tick = 0; tick < 6000; tick++) {
    world.Tick();

    if (!harvested && !harvester->IsEmpty())
      harvested = true;

    if (!docked && harvester->GetDockClientManager()->ReservedHost() !=
                       nullptr)
      docked = true;

    if (!credited && resources->Resources + resources->Cash >
                         resources0) {
      credited = true;
      break;
    }

    if (harv->IsDead())
      break;
  }
  Check(harvested, "HARV harvested injected ore");
  Check(docked, "HARV reserved the PROC dock");
  Check(credited, "refinery credited the player account");

  mods::StoresResources* stores = harv->Trait<mods::StoresResources>();
  Check(stores != nullptr, "HARV StoresResources resolved");
  if (stores != nullptr && credited) {
    Check(stores->ContentsSum() <= stores->Capacity(),
          "cargo within capacity");
  }
}

int main(int argc, char** argv) {
  setvbuf(stdout, nullptr, _IONBF, 0);
  const char* str_upstream_root = argc > 1 ? argv[1] : ".";

  ora::gen::RegisterGeneratedAll();
  ora::game::RegisterGameLoaders();
  ora::sim::RegisterWorldTraits();
  ora::mods::RegisterCommonTraits();

  ora::mods::SetBodyOrientationFacingsResolver(
      [](const ora::game::ActorInfo&, const std::string&) { return 8; });

  const std::string_view str_stage = argc > 2 ? std::string_view{argv[2]}
                                              : std::string_view{"all"};
  if (str_stage == "all" || str_stage == "power")
    TestPowerChain(str_upstream_root);
  if (str_stage == "all" || str_stage == "turret")
    TestTurretChain(str_upstream_root);
  if (str_stage == "all" || str_stage == "harvest")
    TestResourceChain(str_upstream_root);

  if (g_failures == 0)
    std::println("resource_test: all checks passed");
  else
    std::println("resource_test: {} checks failed", g_failures);
  return g_failures == 0 ? 0 : 1;
}
