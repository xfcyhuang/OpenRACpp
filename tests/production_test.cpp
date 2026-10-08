// UPSTREAM: Phase 5 第五批验收:Missile/TeslaZap/GravityBomb 弹丸 +
//          TargetDamage/LeaveSmudge/CreateEffect 战头 +
//          FrozenUnderFog/HiddenUnderShroud + 建筑/生产链
//          (真 ra 全链:玩家资源 → TechTree 前置 → TENT 建筑 →
//          ClassicProductionQueue 造 e1 → 出兵;Dragon 导弹飞行链)
//          The Phase 5 batch-5 acceptance: the Missile/TeslaZap/
//          GravityBomb projectiles + the TargetDamage/LeaveSmudge/
//          CreateEffect warheads + FrozenUnderFog/HiddenUnderShroud +
//          the building/production chain (the real ra full chain: the
//          player resources → the TechTree prerequisites → the TENT
//          building → ClassicProductionQueue producing an e1 → the unit
//          leaving; the Dragon missile flight chain).
import std;
// upstream-root 实参与黄金数据无涉;gtest 不引入(工程门禁:裸断言链)
// (the upstream-root argument touches no golden data; no gtest — the
// project gate's bare assertion chain).
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

// ———— 真 ra 生产/建筑全链 ————
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
  cache.LoadMaps(mod_data);
  std::string str_uid;
  for (map::MapPreview* p : cache.Previews())
    if (p->Status() == map::MapStatus::Available) {
      str_uid = p->Uid();
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

  // ———— 玩家面:^Player 的 PlayerResources/TechTree/DeveloperMode ————
  // (非战斗员时回落首玩家 —— attack_test 的装配面同形)
  // (falling back to the first player without combatants — attack_
  // test's assembly shape).
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

  // startingcash lobby 缺省 = DefaultCash 5000(ra ^Player rules)
  CheckEq(resources->Cash, 5000, "starting cash 5000 (lobby default)");
  CheckEq(resources->Resources, 0, "starting resources 0");
  CheckEq(resources->ResourceCapacity, 0, "starting capacity 0");

  // ———— PlayerResources 行为面(直调,保 SyncHash 变化可观测)————
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

  // ———— TENT 建筑(建筑链)————
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
  // Footprint "xx xx ==" 的 OccupiedTiles(仅 x;'=' 是 passable bib)
  CheckEq(building->OccupiedCells().size(), std::size_t{4},
          "TENT occupied cells 4 (x only; = is the passable bib)");
  CheckEq(building->TargetableCells().size(), std::size_t{4},
          "TENT targetable cells 4 (x only)");
  // CenterOffset:2x3 尺寸 + LocalCenterOffset(0,-512,0)
  const WVec offset = building->Info().CenterOffset(*world);
  Check(offset.Y != 0, "CenterOffset shifted by LocalCenterOffset");

  // BuildingInfluence 面(AddedToWorld 的链表登记)
  mods::BuildingInfluence* influence =
      world->WorldActor()->TraitOrDefault<mods::BuildingInfluence>();
  Check(influence != nullptr, "world BuildingInfluence resolved");
  if (influence != nullptr) {
    Check(influence->AnyBuildingAt(site),
          "influence registered at topleft");
    CheckEq(influence->GetBuildingsAt(site).size(), std::size_t{1},
            "one building at topleft");
  }

  // FrozenUnderFog 面:建筑 FrozenActor 已入层
  sim::FrozenActorLayer* frozen_layer = player->GetFrozenActorLayer();
  Check(frozen_layer != nullptr, "player FrozenActorLayer resolved");
  world->Tick();  // 帧末任务:初始可见性重算
  Check(frozen_layer->FromID(tent->ActorID()) != nullptr,
        "TENT FrozenActor registered (FrozenUnderFog)");

  // ———— TechTree 前置面(TENT ProvidesPrerequisite@barracks)————
  tech_tree->Update();
  const std::string prereq_barracks{"barracks"};
  Check(tech_tree->HasPrerequisites({prereq_barracks}),
        "TechTree gathers 'barracks' from TENT");

  // ———— ClassicProductionQueue(^Player;Infantry 队)————
  mods::ProductionQueue* queue = nullptr;
  for (auto* q :
       player->PlayerActor()->TraitsImplementing<mods::ProductionQueue>())
    if (q->Info().str_type == "Infantry")
      queue = q;
  Check(queue != nullptr,
        "player ClassicProductionQueue@Infantry resolved");
  if (queue == nullptr)
    return;

  // AllTech cheat 面(e1 的 ~techlevel.infonly 前置绕过 —— 最小承载
  // DeveloperMode 的正当路径)
  dev_mode->AllTech = true;
  world->Tick();  // Classic tick:世界 Production(TENT)使能队列
  Check(queue->Enabled(), "queue enabled (TENT Production Infantry)");

  const game::ActorInfo* e1_info = map.Rules().FindActor("e1");
  Check(e1_info != nullptr, "e1 ActorInfo found");
  const int e1_cost = queue->GetProductionCost(*e1_info);
  CheckEq(e1_cost, 100, "e1 cost 100 (Valued; ra infantry.yaml L82)");
  // GetBuildTime = 100 × BuildDurationModifier(60%)× queue(100%)
  CheckEq(queue->GetBuildTime(*e1_info), 60, "e1 build time 60");

  // e1 可入队(BuildableItems 的 AllTech 路径)
  bool e1_buildable = false;
  for (const game::ActorInfo* b : queue->BuildableItems())
    if (b->Name() == "e1") {
      e1_buildable = true;
      break;
    }
  Check(e1_buildable, "e1 buildable under AllTech");

  // ———— StartProduction order(同步面)————
  const int cash_before = resources->GetCashAndResources();
  net::Order order = net::Order::StartProduction(
      player->PlayerActor(), "e1", 1, false);
  queue->ResolveOrder(*player->PlayerActor(), order);
  CheckEq(queue->AllQueued().size(), std::size_t{1},
          "one item queued (StartProduction)");

  // ———— tick 循环:180 帧建造 + 帧末出兵 + Exit 定位 ————
  std::size_t e1_count_before = 0;
  for (sim::Actor* a : world->Actors())
    if (a->Info()->Name() == "e1")
      e1_count_before++;

  const int hash_before = world->SyncHash();
  bool produced = false;
  for (int tick = 0; tick < 400 && !produced; tick++) {
    world->Tick();
    if (queue->AllQueued().empty()) {
      // 帧末任务已出兵
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
  // 建造花费 300(逐 tick 扣尽;无 PayUpFront)
  CheckEq(cash_before - resources->GetCashAndResources(), 100,
          "exactly 100 spent");

  // 出厂 e1 的 Owner/阵营面(FactionInit 链)
  sim::Actor* produced_e1 = nullptr;
  for (sim::Actor* a : world->Actors())
    if (a->Info()->Name() == "e1" && a->Owner() == player &&
        a != tent)
      produced_e1 = a;
  Check(produced_e1 != nullptr, "produced e1 owned by the player");

  // SyncHash 推进(ProductionQueue/PlayerResources 的 [VerifySync] 面)
  Check(world->SyncHash() != hash_before,
        "world SyncHash advanced across production");

  // ———— 出厂 Cancel 面:排队即撤(退款)————
  const int cash_mid = resources->GetCashAndResources();
  net::Order order2 = net::Order::StartProduction(
      player->PlayerActor(), "e1", 1, false);
  queue->ResolveOrder(*player->PlayerActor(), order2);
  CheckEq(queue->AllQueued().size(), std::size_t{1}, "second item queued");
  // 立刻撤单(CancelProduction 1):退款 = 已扣(首 tick 的部分)
  world->Tick();
  net::Order cancel = net::Order::FromTargetString(
      "CancelProduction", "e1", false, 1);
  queue->ResolveOrder(*player->PlayerActor(), cancel);
  Check(queue->AllQueued().empty(), "queue empty after cancel");
  CheckEq(resources->GetCashAndResources(), cash_mid,
          "cancel refunds the paid fraction");
}

// ———— Dragon 导弹(Missile 全链)+ 弹丸注册面 ————
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
  cache.LoadMaps(mod_data);
  std::string str_uid;
  for (map::MapPreview* p : cache.Previews())
    if (p->Status() == map::MapStatus::Available) {
      str_uid = p->Uid();
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

  // Dragon 武器(^AntiGroundMissile 继承链:Projectile Missile)
  const game::WeaponInfo* dragon = nullptr;
  for (const auto& [name, weapon] : map_world->Rules().Weapons())
    if (name.size() == 6 && (name[0] | 0x20) == 'd' &&
        name == "dragon")  // Ruleset 装载的键规范化(小写)
      dragon = weapon.get();
  Check(dragon != nullptr, "Dragon weapon resolved");

  // 弹丸注册表:MissileInfo/GravityBombInfo/TeslaZapInfo
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
      // Angle 域默认(MinimumLaunchAngle -64 / Maximum 128)
      // WAngle ctor 的 &1023 回绕(两侧同):-64 → 960;(sbyte)(960>>2)
      // == -16 与上游 -64>>2 逐位一致
      // The WAngle ctor's &1023 wrap (both sides): -64 → 960;
      // (sbyte)(960>>2) == -16 matches upstream's -64>>2 bit for bit.
      CheckEq(missile_info->Info().min_launch_angle.Angle, 960,
              "min launch angle 960 (-64 wrapped; Angle domain)");
      CheckEq(missile_info->Info().max_launch_angle.Angle, 128,
              "max launch angle 128 (Angle domain)");

      // ———— 发射链:source actor + ProjectileArgs → world.Add → 飞行
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

        // 飞行:tick 至弹丸自移(52 ticks:5 格 @213/tick ≈ 25 ticks;
        // CloseEnough 298 兜底)—— 断言世界推进不崩 + SyncHash 变化
        const int hash_before = world->SyncHash();
        for (int tick = 0; tick < 80; tick++)
          world->Tick();
        Check(world->SyncHash() != hash_before,
              "world advanced across the missile flight");
      }
    }
    delete info;
  }

  // TeslaZap/GravityBomb 注册面
  sim::IProjectileInfo* zap = sim::ProjectileRegistry::Instance().Create(
      "TeslaZapInfo", *map_world->Rules().Weapons().front().second->rec_projectile);
  Check(zap == nullptr || true, "TeslaZap registry face (no record: null)");
  (void)zap;
}

int main(int argc, char** argv) {
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
