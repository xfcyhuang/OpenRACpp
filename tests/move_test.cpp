import std;

#include "game/game.hpp"

#include "render_sequences_fixture.hpp"
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

void TestBodyOrientationInfoData() {
  using namespace ora;
  mods::BodyOrientationInfoData data;

  const WAngle pitch = data.angle_camera_pitch;
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

  CheckEq(data.QuantizeFacing(WAngle{0}, 8).Angle, 0, "QuantizeFacing 0");
  CheckEq(data.QuantizeFacing(WAngle{64}, 8).Angle, 128,
          "QuantizeFacing 64 → 128(最近)→ 128 (nearest)");

  CheckEq(data.QuantizeFacing(WAngle{1000}, 8).Angle, 0,
          "QuantizeFacing 1000 wraps to 0");
  CheckEq(data.QuantizeFacing(WAngle{777}, 0).Angle, 777,
          "QuantizeFacing facings=0 禁用 | disabled");

  const WRot q = data.QuantizeOrientation(WRot{WAngle{10}, WAngle{20},
                                              WAngle{100}},
                                          8);
  CheckEq(q.Roll.Angle, 0, "QuantizeOrientation roll zeroed");
  CheckEq(q.Pitch.Angle, 0, "QuantizeOrientation pitch zeroed");
  CheckEq(q.Yaw.Angle, 128, "QuantizeOrientation yaw quantized");
}

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
  Check(world->WorldActor() != nullptr, "WorldActor created");

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

  CheckEq(mobile->FromCell().X(), 0, "Mobile ctor FromCell.X zero");
  Check(mobile->FromCell() == mobile->ToCell(), "ctor FromCell == ToCell");
  CheckEq(mobile->Info().int4_speed, 54, "e1 Mobile Speed 54 (^Infantry)");
  Check(mobile->Info().str_locomotor == "foot", "e1 locomotor foot");
  Check(mobile->Info().b_always_turn_in_place, "e1 AlwaysTurnInPlace");
  Check(mobile->Locomotor() != nullptr, "runtime locomotor resolved");

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

  world->Tick();
  Check(mobile->CurrentMovementTypes() == sim::MovementType::None,
        "movement types cleared at rest");
  std::println("move: {} ticks {} cells (speed {})", ticks,
               (dest - spawn).Length(), speed);

  auto armaments = e1->TraitsImplementing<mods::Armament>();
  CheckEq(armaments.size(), std::size_t{2}, "e1 has two Armaments");
  if (armaments.empty())
    return;
  mods::Armament* armament = armaments[0];
  Check(armament->Weapon != nullptr, "weapon resolved");
  CheckEq(armament->Weapon->int4_range, 5 * 1024, "M1Carbine Range 5c0");
  CheckEq(armament->Weapon->int4_reloadDelay, 20, "M1Carbine ReloadDelay 20");
  CheckEq(armament->Weapon->int4_burst, 1, "M1Carbine Burst 1(^HeavyMG 无覆写"
          " | no override)");

  const CPos far_cell = mobile->NearestMoveableCell(dest + CVec{20, 0}, 0, 4);
  sim::Target far_target = sim::Target::FromPos(map.CenterOfCell(far_cell));
  Check(!armament->CheckFire(e1, mobile, far_target),
        "CheckFire rejects out-of-range");

  const CPos near_cell = mobile->NearestMoveableCell(dest + CVec{2, 0}, 0, 4);
  sim::Target near_target = sim::Target::FromPos(map.CenterOfCell(near_cell));
  Check(armament->CheckFire(e1, mobile, near_target),
        "CheckFire accepts in-range terrain target");
  Check(armament->IsReloading(), "IsReloading after first shot");
  CheckEq(armament->FireDelay, 20, "FireDelay = reload delay (Burst 1)");
  CheckEq(armament->Burst, 1, "Burst reset to weapon burst");

  Check(!armament->CheckFire(e1, mobile, near_target),
        "CheckFire rejects while reloading");

  for (int i = 0; i < 21; i++)
    world->Tick();
  CheckEq(armament->FireDelay, 0, "FireDelay counts down to zero");
  Check(armament->CheckFire(e1, mobile, near_target),
        "CheckFire accepts after reload");

  CheckEq(armament->Recoil.Length, 0, "e1 has no recoil override");

  Check(!e1->SyncHashes().empty(), "e1 carries [VerifySync] trait hashes");
}

void TestConditionalCore() {
  using namespace ora;

  sim::ConditionalTraitData unconditional;
  Check(unconditional.b_enabled_by_default, "no-condition enabled by default");
  Check(!unconditional.b_paused_by_default, "no-condition not paused");
}

int main(int argc, char** argv) {
  setvbuf(stdout, nullptr, _IONBF, 0);
  if (argc < 2) {
    std::println("usage: move_test <upstream-root>");
    return 1;
  }

  ora::gen::RegisterGeneratedAll();
  ora::game::RegisterGameLoaders();
  ora::sim::RegisterWorldTraits();
  ora::mods::RegisterCommonTraits();

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
