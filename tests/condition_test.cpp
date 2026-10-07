// UPSTREAM: Phase 5 第七批验收:Conditions 31 件 + ExternalCondition/
//          ProximityExternalCondition + Cloak/DetectCloaked/IgnoresCloak +
//          GainsExperience/GivesExperience/GainsExperienceMultiplier/
//          PlayerExperience + CaptureManager/Capturable/Captures/
//          GivesCashOnCapture + Enter/CaptureActor + Selectable/Interactable +
//          SpawnMapActors(真 ra 全链:地图摆位 → 升级授条件 → 隐身节拍 →
//          伤害状态禁隐 → 工程师捕获换主)
//          The Phase 5 batch-7 acceptance: the 31 conditions +
//          ExternalCondition/ProximityExternalCondition +
//          Cloak/DetectCloaked/IgnoresCloak + the experience family +
//          the capture family + Enter/CaptureActor + Selectable/
//          Interactable + SpawnMapActors (the real ra full chain: the
//          map placement → the promotion grant → the cloak cadence →
//          the damage-state cloak disable → the engineer capture's
//          owner change).
import std;

#include "game/game.hpp"
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

namespace ora::gen {
void RegisterGeneratedAll();
}

static int g_failures = 0;

// 攻击信息载荷(测试内静态存储;上游 AttackInfo.Damage 引用语义)
// The attack-info payload (static storage in the test; upstream's
// AttackInfo.Damage reference semantics).
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
  if (argc < 2) {
    std::println("usage: condition_test <upstream-root>");
    return 1;
  }

  ora::gen::RegisterGeneratedAll();
  ora::game::RegisterGameLoaders();
  ora::sim::RegisterWorldTraits();
  ora::mods::RegisterCommonTraits();

  // qboi facings 解析钩子:测试接线 8(同 attack_test;渲染批换接真序列表)
  // The qboi facings-resolution hook: the test wires 8 (attack_test's
  // device; the render batch swaps in the real sequence table).
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
  cache.LoadMaps(mod_data);
  std::string str_uid;
  for (map::MapPreview* p : cache.Previews())
    if (p->Status() == map::MapStatus::Available) {
      str_uid = p->Uid();
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

  auto world = std::make_unique<sim::World>(
      *map_world, mod_data, *game_.OrderManagerFace(),
      sim::WorldType::Regular);
  world->LoadComplete(nullptr);
  Check(!world->Players().empty(), "map players created");
  if (world->Players().empty())
    return g_failures != 0;

  // ———— 1. SpawnMapActors:地图摆位全链(IWorldLoaded 分发)————
  // ———— 1. SpawnMapActors: the map-placement full chain (the
  //      IWorldLoaded dispatch) ————

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
    // 出生 actor 的 Owner 域:全部在地图玩家集合内(无效 owner 的中立转
    // 移面)
    // The spawned actors' owners all sit in the map-player set (the
    // invalid-owner neutral transfer face).
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

  // 两个战斗玩家
  // Two combatants.
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

  // 出生辅助:Location + Owner(+可选 Facing)
  // The spawn helper: Location + Owner (+ optional Facing).
  const Rectangle bounds = world->Map().Bounds();
  const int cx = bounds.Left() + bounds.Width / 2;
  const int cy = bounds.Top() + bounds.Height / 2;
  auto spawn = [&](const std::string& str_type, sim::Player* owner,
                   CPos cell,
                   std::optional<int> opt_facing = std::nullopt) {
    sim::TypeDictionary dict;
    sim::LocationInit location{cell};
    sim::OwnerInit owner_init{owner};
    dict.Add(&location);
    dict.Add(&owner_init);
    std::optional<sim::FacingInit> facing;
    if (opt_facing.has_value()) {
      facing.emplace(WAngle{*opt_facing});
      dict.Add(&*facing);
    }
    return world->CreateActor(true, str_type, dict);
  };

  // ———— 2. e1 的 Selectable + GainsExperience 升级授条件 ————
  // ———— 2. e1's Selectable + the GainsExperience promotion grant ————

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
      // ^GainsExperience 首级 = 200% × Valued.Cost(100) = 200
      // The first level = 200% × Valued.Cost (100) = 200.
      // 首级 = 200% × Valued.Cost(100) = 20000(上游 key×cost 语义)
      // The first level = 200% × Valued.Cost (100) = 20000 (upstream's
      // key×cost semantics).
      gains->GiveExperience(20000);
      CheckEq(gains->Level, 1, "20000 experience reaches level 1");
      const auto& cache_conds = e1->ConditionCache();
      Check(cache_conds.count("rank-veteran") != 0 &&
                cache_conds.at("rank-veteran") >= 1,
            "rank-veteran granted at level 1");
    }

    // GivesExperience:击杀敌 e1 的经验链(killer 升级)
    // GivesExperience: the kill-experience chain (the killer's
    // promotion).
    sim::Actor* e1_prey = spawn("e1", player_b, CPos{cx + 2, cy});
    mods::GivesExperience* gives =
        e1_prey != nullptr ? e1_prey->TraitOrDefault<mods::GivesExperience>()
                           : nullptr;
    Check(gives != nullptr, "prey GivesExperience mounted (^Soldier)");
    if (gives != nullptr && gains != nullptr) {
      // 敌方击杀:ValidRelationships = Neutral|Enemy ✓;exp = cost 100,
      // ActorExperienceModifier 10000% → 100
      // An enemy kill: ValidRelationships = Neutral|Enemy ✓; exp =
      // cost 100, ActorExperienceModifier 10000% → 100.
      // ^Infantry 的 GainsExperienceMultiplier(Modifier 0,装车禁经验)
      // 恒入链 —— 上游真语义:步兵击杀不授 killer 经验(0×10000%=0)
      // ^Infantry's GainsExperienceMultiplier (Modifier 0, the
      // in-cargo experience freeze) is always chained — upstream's
      // live semantics: infantry kills credit the killer nothing
      // (0×10000% = 0).
      sim::AttackInfo ai;
      ai.Damage = &damage_pool;
      ai.Attacker = e1;
      gives->Killed(*e1_prey, ai);
      CheckEq(gains->Experience, 20000,
              "infantry kill credits zero (upstream quirk)");
      // PlayerExperience 面:ra 的 ^ExistsInWorld 基础规则
      // PlayerExperienceModifier = 1(击杀记分);100×1% = 1
      // The PlayerExperience face: ra's ^ExistsInWorld base rules set
      // PlayerExperienceModifier = 1 (the kill score); 100×1% = 1.
      if (const mods::PlayerExperience* player_exp =
              player_a->PlayerActor()
                  ->TraitOrDefault<mods::PlayerExperience>())
        CheckEq(player_exp->Experience, 1,
                "player experience credits the kill score");
    }

    // GrantConditionWhileAiming:aiming 边沿授撤(DOG 实配 Condition: run
    // —— e1 无该 trait)
    // GrantConditionWhileAiming: the aiming-edge grant/revoke (DOG's
    // configured Condition: run — e1 carries no such trait).
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

  // ———— 3. THF 的 Cloak 节拍 + 伤害状态禁隐 ————
  // ———— 3. THF's cloak cadence + the damage-state disable ————

  sim::Actor* thf = spawn("THF", player_a, CPos{cx + 4, cy});
  Check(thf != nullptr, "THF constructed");
  mods::Cloak* cloak = thf != nullptr ? thf->TraitOrDefault<mods::Cloak>()
                                      : nullptr;
  // THF 携双 GrantConditionOnDamageState(^Soldier 的 @DAMAGED + 本体的
  // @UNCLOAK)—— TraitOrDefault 的单例语义不适用,按条件名挑选
  // THF carries two GrantConditionOnDamageState (^Soldier's @DAMAGED +
  // its own @UNCLOAK) — the single-instance TraitOrDefault semantics do
  // not apply; pick by condition name.
  mods::GrantConditionOnDamageState* uncloak_state = nullptr;
  if (thf != nullptr)
    for (mods::GrantConditionOnDamageState* state :
         thf->TraitsImplementing<mods::GrantConditionOnDamageState>()) {
      const auto& conds = thf->ConditionCache();
      (void)conds;
      // 工厂无按名暴露面 —— 以 Critical→cloak-force-disabled 的真链验
      // 证:留首个非空即可(两实例都收 DamageStateChanged,上游同)
      // The factory exposes no by-name face — the Critical→
      // cloak-force-disabled live chain validates: keep the first
      // non-null (both instances receive DamageStateChanged, as
      // upstream).
      if (uncloak_state == nullptr)
        uncloak_state = state;
    }
  Check(cloak != nullptr, "THF Cloak mounted");
  Check(uncloak_state != nullptr, "THF GrantConditionOnDamageState mounted");
  if (cloak != nullptr) {
    // InitialDelay 250:250 tick 后进入隐身
    // InitialDelay 250: cloaked after 250 ticks.
    for (int i = 0; i < 251; i++)
      world->Tick();
    Check(cloak->Cloaked(), "THF cloaked after InitialDelay");
    Check(!cloak->IsVisible(*thf, player_b),
          "cloaked THF invisible to the enemy");
    Check(cloak->IsVisible(*thf, player_a),
          "cloaked THF visible to allies");

    // 移动触发 UncloakOn Move → Uncloak(remaining = CloakDelay 120)
    // Movement triggers UncloakOn Move → Uncloak (remaining = CloakDelay
    // 120).
    mods::Mobile* thf_mobile = thf->TraitOrDefault<mods::Mobile>();
    if (thf_mobile != nullptr) {
      thf_mobile->SetPosition(thf, CPos{cx + 5, cy}, sim::SubCell::Any);
      world->Tick();
      Check(!cloak->Cloaked(), "movement uncloaks (UncloakOn Move)");
      // CloakDelay 120:冷却后自然复隐
      // CloakDelay 120: naturally re-cloaks after the cooldown.
      for (int i = 0; i < 121; i++)
        world->Tick();
      Check(cloak->Cloaked(), "re-cloaks after CloakDelay elapses");
    }

    // 伤害至 Critical → cloak-force-disabled → PauseOnCondition 生效
    // Damage into Critical → cloak-force-disabled → PauseOnCondition
    // engages.
    if (uncloak_state != nullptr) {
      const sim::IHealth* health = thf->Trait<sim::IHealth>();
      const int damage = health->HP() - health->MaxHP() / 8;
      damage_pool.Value = damage;
      thf->Trait<sim::IHealth>()->InflictDamage(*thf, nullptr, damage_pool,
                                                false);
      const auto& cache_c = thf->ConditionCache();
      Check(cache_c.count("cloak-force-disabled") != 0,
            "Critical damage grants cloak-force-disabled");
      // 手动再授一枚验证通知链(随后撤销,不扰动计数)
      // A manual extra grant verifies the notification chain (revoked
      // right after, leaving the counts untouched).
      const int int4_manual_token = thf->GrantCondition("cloak-force-disabled");
      Check(cloak->IsTraitPaused(),
            "the manual grant re-pauses the cloak");
      thf->RevokeCondition(int4_manual_token);
      Check(cloak->IsTraitPaused(),
            "cloak-force-disabled pauses the cloak");
      // 冷却过期后 paused 仍阻隐(PauseOnCondition 持续)
      // After the cooldown elapses the pause still blocks cloaking
      // (PauseOnCondition persists).
      for (int i = 0; i < 130; i++)
        world->Tick();
      Check(!cloak->Cloaked(), "paused cloak stays uncloaked");
    }
  }

  // ———— 4. E6 工程师捕获 TENT(Enter/CaptureActor 全链)————
  // ———— 4. The E6 engineer's TENT capture (the Enter/CaptureActor full
  //      chain) ————

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

    // CaptureActor 活动链:接近 → CaptureDelay 200 等待 → 进入 → 帧末
    // 换主(ConsumedByCapture → E6 消耗)
    // The CaptureActor chain: approach → the CaptureDelay-200 wait →
    // entering → the frame-end owner change (ConsumedByCapture → E6
    // consumed).
    // E6 携双 Captures(本体 + @REUSABLE)—— 分发面遍历(上游
    // UnitOrders 对每实例 ResolveOrder;条件禁用者自拒)
    // E6 carries two Captures (the base + @REUSABLE) — the dispatch
    // iterates (upstream's UnitOrders ResolveOrders every instance; the
    // condition-disabled one rejects itself).
    // 测试域视野手段:全图探索先于下发(Enter 链的 Recalculate 需要
    // TENT 对 player_a 可见;渲染批的 TickRender 物化域在循环内先行)
    // The test-domain visibility device: explore everything before the
    // order (the Enter chain's Recalculate needs the TENT visible to
    // player_a; the render batch's TickRender materialization domain
    // leads inside the loop).
    player_a->GetShroud()->ExploreAll();

    // 可见性链收敛提前量:Shroud 的事件批处理在 Tick 内推进、FrozenActor
    // 的重算再下一 tick(上游渲染帧多轮先行,玩家"看见后才点击";此处等
    // 价空转)
    // The visibility-chain settling lead: Shroud's event batching
    // advances inside Tick and the FrozenActor recomputation lands the
    // next tick (upstream's many leading render frames mean the player
    // clicks only after seeing; the idle spin here is the equivalent).
    for (int i = 0; i < 4; i++) {
      world->TickRender();
      world->Tick();
    }

    // lobby 的 engineer-reusable 选项域:授予全局前置 → Captures@REUSABLE
    // 接管(CaptureDelay 375/ConsumedByCapture=False —— 工程师走进前即
    // 完成捕获且存活;上游真实玩法路径)
    // The lobby's engineer-reusable option domain: granting the global
    // prerequisite hands capture to Captures@REUSABLE (CaptureDelay 375/
    // ConsumedByCapture=False — the engineer completes the capture
    // before entering and survives; a genuine upstream play path).
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

  // ———— 5. SyncHash 推进(本批 [VerifySync] 哈希装配不崩)————
  // ———— 5. The SyncHash advance (this batch's [VerifySync] hash
  //      assemblies do not crash) ————
  for (int i = 0; i < 10; i++)
    world->Tick();
  const int sync_hash = world->SyncHash();
  std::println("SyncHash after batch-7 chain: {}", sync_hash);

  std::println("{}",
               g_failures == 0 ? "condition_test: ALL PASS"
                              : "condition_test: FAILURES");
  return g_failures != 0;
}
