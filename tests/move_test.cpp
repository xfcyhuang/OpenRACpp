// UPSTREAM: Phase 5 第三批验收:Mobile/BodyOrientation/Move 活动族/Armament
//          (纯逻辑矩阵 + 真实 ra 规则/地图全链:构造 → 移动到站 → 开火节拍)
//          The Phase 5 batch-3 acceptance: Mobile/BodyOrientation/the move
//          activity family/Armament (pure-logic matrices + the real ra
//          rules/map full chain: construct → move to arrival → the firing
//          cadence).
import std;
// upstream-root 实参与黄金数据无涉;gtest 不引入(工程门禁:裸断言链)
// (the upstream-root argument touches no golden data; no gtest — the
// project gate's bare assertion chain).
#include "game/game.hpp"
#include "game/game_records.hpp"
#include "game/manifest.hpp"
#include "game/mod_data.hpp"
#include "map/map.hpp"
#include "map/map_cache.hpp"
#include "mods/armament.hpp"
#include "mods/create_map_players.hpp"
#include "mods/body_orientation.hpp"
#include "mods/mobile.hpp"
#include "mods/move_activities.hpp"
#include "sim/actor.hpp"
#include "sim/actor_init.hpp"
#include "sim/player.hpp"
#include "sim/sync_hash.hpp"
#include "sim/target.hpp"
#include "sim/world.hpp"

namespace ora::gen {
void RegisterGeneratedAll();  // gen/gen_all.cpp(库内定义 | defined in the
                              // ora_gen library)
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

// ———— A. BodyOrientation 纯逻辑矩阵(BodyOrientation.cs L31-62)————
void TestBodyOrientationInfoData() {
  using namespace ora;
  mods::BodyOrientationInfoData data;

  // LocalToWorld(L31-43):经典透视 fudge
  const WAngle pitch = data.angle_camera_pitch;  // 40°
  const WVec fudged = data.LocalToWorld(WVec{1024, 512, 256});
  CheckEq(fudged.X, 512, "LocalToWorld fudge .X = vec.Y");
  CheckEq(fudged.Y, -pitch.Sin() * 1024 / 1024,
          "LocalToWorld fudge .Y = -sin(40°)*vec.X/1024");
  CheckEq(fudged.Z, 256, "LocalToWorld fudge .Z = vec.Z");

  data.b_use_classic_perspective_fudge = false;
  const WVec rotated = data.LocalToWorld(WVec{1024, 512, 256});
  CheckEq(rotated.X, 512, "LocalToWorld 90° .X");
  CheckEq(rotated.Y, -1024, "LocalToWorld 90° .Y");
  CheckEq(rotated.Z, 256, "LocalToWorld 90° .Z");

  // QuantizeFacing(L57-62):8 facings,step 128
  CheckEq(data.QuantizeFacing(WAngle{0}, 8).Angle, 0, "QuantizeFacing 0");
  CheckEq(data.QuantizeFacing(WAngle{64}, 8).Angle, 128,
          "QuantizeFacing 64 → 128(最近)→ 128 (nearest)");
  // IndexFacing(1000,8):a=(1000+64)&1023=40,40/128=0 → 0(最近朝向回绕)
  // IndexFacing(1000, 8): a=(1000+64)&1023=40, 40/128=0 → 0 (nearest
  // facing after wrap).
  CheckEq(data.QuantizeFacing(WAngle{1000}, 8).Angle, 0,
          "QuantizeFacing 1000 wraps to 0");
  CheckEq(data.QuantizeFacing(WAngle{777}, 0).Angle, 777,
          "QuantizeFacing facings=0 禁用 | disabled");

  // QuantizeOrientation(L45-55):量化后 Roll/Pitch 归零
  const WRot q = data.QuantizeOrientation(WRot{WAngle{10}, WAngle{20},
                                              WAngle{100}},
                                          8);
  CheckEq(q.Roll.Angle, 0, "QuantizeOrientation roll zeroed");
  CheckEq(q.Pitch.Angle, 0, "QuantizeOrientation pitch zeroed");
  CheckEq(q.Yaw.Angle, 128, "QuantizeOrientation yaw quantized");
}

// ———— B. 真 ra 链:Mobile 构造/移动到站/Armament 开火节拍 ————
void TestRealRaChain(const char* str_upstream_root) {
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

  auto world = std::make_unique<sim::World>(
      *map_world, mod_data, *game_.OrderManagerFace(), sim::WorldType::Regular);
  Check(world->WorldActor() != nullptr, "WorldActor created");
  // LoadComplete(L260-298):Locomotor/PathFinder 的 WorldLoaded 建表面
  // (移动/寻路查询的前提;wr 空 = 无头测试面)
  // LoadComplete (L260-298): Locomotor/PathFinder's WorldLoaded builds
  // their tables (the precondition of move/path queries; a null wr = the
  // headless test face).
  world->LoadComplete(nullptr);
  Check(!world->Players().empty(), "map players created");
  if (world->Players().empty())
    return;

  sim::Player* owner = nullptr;
  for (sim::Player* p : world->Players())
    if (!p->NonCombatant()) {
      owner = p;
      break;
    }
  if (owner == nullptr)
    owner = world->Players()[0];

  // ———— Mobile 构造链(e1:^Infantry → Mobile Speed 54/foot/
  //      AlwaysTurnInPlace;BodyOrientation + Armament@PRIMARY M1Carbine)
  //      ————
  sim::TypeDictionary inits;
  inits.Add(new sim::OwnerInit(owner));
  mods::Mobile* mobile = nullptr;
  sim::Actor* e1 = nullptr;
  try {
    e1 = world->CreateActor("e1", inits);
    mobile = e1->Trait<mods::Mobile>();
  } catch (const std::exception& ex) {
    std::println("FAIL: e1 construction threw: {}", ex.what());
    ++g_failures;
    return;
  }
  Check(mobile != nullptr, "e1 has Mobile");
  if (mobile == nullptr)
    return;

  // 未带 LocationInit:FromCell == ToCell == CPos.Zero(构造期无位置)
  // Without a LocationInit: FromCell == ToCell == CPos.Zero (no position at
  // construction).
  CheckEq(mobile->FromCell().X(), 0, "Mobile ctor FromCell.X zero");
  Check(mobile->FromCell() == mobile->ToCell(), "ctor FromCell == ToCell");
  CheckEq(mobile->Info().int4_speed, 54, "e1 Mobile Speed 54 (^Infantry)");
  Check(mobile->Info().str_locomotor == "foot", "e1 locomotor foot");
  Check(mobile->Info().b_always_turn_in_place, "e1 AlwaysTurnInPlace");
  Check(mobile->Locomotor() != nullptr, "runtime locomotor resolved");

  // 找一个可站立格(地图中心外扩)并 SetPosition
  const map::Map& map = world->Map();
  const Rectangle bounds = map.Bounds();
  CPos spawn{bounds.Left() + bounds.Width / 2,
             bounds.Top() + bounds.Height / 2};
  spawn = mobile->NearestMoveableCell(spawn, 0, 12);
  Check(mobile->CanStayInCell(spawn), "spawn cell stayable");
  mobile->SetPosition(e1, spawn, sim::SubCell::Any);
  Check(e1->Location() == spawn, "Location == spawn after SetPosition");
  CheckEq(mobile->OccupiedCells().size(), std::size_t{1},
          "stationary OccupiedCells single entry");
  const int speed = mobile->MovementSpeedForCell(spawn);
  Check(speed > 0, "MovementSpeedForCell > 0 on spawn terrain");

  // ———— Move 活动全链:3 格直移 → 到站 ————
  CPos dest = spawn + CVec{3, 0};
  dest = mobile->NearestMoveableCell(dest, 0, 6);
  sim::Activity* move = mobile->MoveTo(dest, 0, nullptr, false, std::nullopt);
  Check(move != nullptr, "MoveTo returns activity");
  e1->QueueActivity(move);

  int ticks = 0;
  bool arrived = false;
  sim::MoveResult move_result = sim::MoveResult::InProgress;
  while (ticks < 3000) {
    world->Tick();
    ++ticks;
    if (e1->IsIdle()) {
      move_result = mobile->MoveResult;
      arrived = e1->Location() == dest &&
                move_result == sim::MoveResult::CompleteDestinationReached;
      break;
    }
  }
  Check(ticks < 3000, "move completes within tick budget");
  CheckEq(static_cast<int>(move_result),
          static_cast<int>(sim::MoveResult::CompleteDestinationReached),
          "MoveResult CompleteDestinationReached");
  Check(e1->Location() == dest, "arrival cell == destination");
  Check(mobile->FromCell() == mobile->ToCell(), "FromCell == ToCell at rest");
  Check(!mobile->IsMovingBetweenCells(), "not moving between cells at rest");
  // 到站 tick 的位移仍计 Horizontal(上游同形);下一 tick UpdateMovement
  // 清 None
  // The arrival tick's displacement still counts Horizontal (upstream's
  // same shape); the next tick's UpdateMovement clears it to None.
  world->Tick();
  Check(mobile->CurrentMovementTypes() == sim::MovementType::None,
        "movement types cleared at rest");
  std::println("move: {} ticks {} cells (speed {})", ticks,
               (dest - spawn).Length(), speed);

  // ———— Armament 开火节拍(M1Carbine:ReloadDelay 20 / Range 5c0 /
  //      InstantHit / Burst 1)————
  auto armaments = e1->TraitsImplementing<mods::Armament>();
  CheckEq(armaments.size(), std::size_t{2}, "e1 has two Armaments");
  if (armaments.empty())
    return;
  mods::Armament* armament = armaments[0];  // PRIMARY
  Check(armament->Weapon != nullptr, "weapon resolved");
  CheckEq(armament->Weapon->int4_range, 5 * 1024, "M1Carbine Range 5c0");
  CheckEq(armament->Weapon->int4_reloadDelay, 20, "M1Carbine ReloadDelay 20");
  CheckEq(armament->Weapon->int4_burst, 1, "M1Carbine Burst 1(^HeavyMG 无覆写"
          " | no override)");

  // 面外目标(20 格外)→ CheckFire 拒绝
  const CPos far_cell = mobile->NearestMoveableCell(dest + CVec{20, 0}, 0, 4);
  sim::Target far_target = sim::Target::FromPos(map.CenterOfCell(far_cell));
  Check(!armament->CheckFire(e1, mobile, far_target),
        "CheckFire rejects out-of-range");

  // 面内地形目标(2 格)→ 开火;Burst 1 → FireDelay = 20(reload)
  const CPos near_cell = mobile->NearestMoveableCell(dest + CVec{2, 0}, 0, 4);
  sim::Target near_target = sim::Target::FromPos(map.CenterOfCell(near_cell));
  Check(armament->CheckFire(e1, mobile, near_target),
        "CheckFire accepts in-range terrain target");
  Check(armament->IsReloading(), "IsReloading after first shot");
  CheckEq(armament->FireDelay, 20, "FireDelay = reload delay (Burst 1)");
  CheckEq(armament->Burst, 1, "Burst reset to weapon burst");

  // 装填期再开火被拒
  Check(!armament->CheckFire(e1, mobile, near_target),
        "CheckFire rejects while reloading");

  // 20 tick 后 FireDelay 归零 → 再开火成立
  for (int i = 0; i < 21; i++)
    world->Tick();
  CheckEq(armament->FireDelay, 0, "FireDelay counts down to zero");
  Check(armament->CheckFire(e1, mobile, near_target),
        "CheckFire accepts after reload");

  // 第四批起弹丸注册表含 InstantHit/Bullet:CheckFire 面内目标即真实发弹
  // (InstantHit 同 tick 落地;战头链随 attack_test 验收)。e1 无 Recoil
  // 覆写 → 后座 0(上游同值)
  // From batch 4 the projectile registry carries InstantHit/Bullet: the
  // in-range CheckFire really fires (InstantHit lands the same tick; the
  // warhead chain is accepted by attack_test). e1 overrides no Recoil →
  // zero recoil (upstream's same value).
  CheckEq(armament->Recoil.Length, 0, "e1 has no recoil override");

  // ———— SyncHash 注册面(Mobile/BodyOrientation)————
  Check(!e1->SyncHashes().empty(), "e1 carries [VerifySync] trait hashes");
}

// ———— C. 条件 trait 面(PauseOnCondition/RequiresCondition)————
void TestConditionalCore() {
  using namespace ora;
  // 无条件:初始即启用(ConditionalTraitData 的默认面)
  // Unconditional: enabled at construction (ConditionalTraitData's
  // default face).
  sim::ConditionalTraitData unconditional;
  Check(unconditional.b_enabled_by_default, "no-condition enabled by default");
  Check(!unconditional.b_paused_by_default, "no-condition not paused");
}

int main(int argc, char** argv) {
  if (argc < 2) {
    std::println("usage: move_test <upstream-root>");
    return 1;
  }

  ora::gen::RegisterGeneratedAll();
  ora::game::RegisterGameLoaders();
  ora::sim::RegisterWorldTraits();
  ora::mods::RegisterCommonTraits();

  // qboi facings 解析钩子:测试接线 8(ra 步兵序列 8 朝向;渲染批换接
  // 真序列表 —— COVERAGE 登记)
  // The qboi facings-resolution hook: the test wires 8 (ra infantry
  // sequences carry 8 facings; the render batch swaps in the real sequence
  // table — registered in COVERAGE).
  ora::mods::SetBodyOrientationFacingsResolver(
      [](const ora::game::ActorInfo&, const std::string&) { return 8; });

  TestBodyOrientationInfoData();
  TestConditionalCore();
  TestRealRaChain(argv[1]);

  if (g_failures == 0)
    std::println("move_test: all passed");
  else
    std::println("move_test: {} FAILURES", g_failures);
  return g_failures == 0 ? 0 : 1;
}
