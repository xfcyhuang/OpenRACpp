// Phase 5 第二批验收:优先队列/LongBitSet/图类型/稀疏图 A*(含双向)/
// 启发式 + ActorMap 影响面与触发器 + ControlGroups + Player 关系面与
// ResolveFaction 确定性 + 真实 ra 地形上的 Locomotor/PathFinder/HPF 全链
// (合成夹具 + argv[1] 的上游 mod 根;无渲染)
// Phase 5 batch-2 acceptance: the priority queue / LongBitSet / graph
// types / the sparse-graph A* (with the bidirectional form) / the
// heuristic + the ActorMap influence faces and triggers + ControlGroups +
// the Player relationship faces and ResolveFaction determinism + the full
// Locomotor/PathFinder/HPF chain over real ra terrain (synthetic fixtures
// + the upstream mod root in argv[1]; no rendering).
import std;

#include "core/long_bitset.hpp"
#include "core/priority_queue.hpp"
#include "game/manifest.hpp"
#include "game/mod_data.hpp"
#include "map/map.hpp"
#include "mods/pathfinding/path_graph.hpp"
#include "mods/pathfinding/locomotor.hpp"
#include "mods/pathfinding/path_finder.hpp"
#include "mods/pathfinding/path_search.hpp"
#include "sim/actor.hpp"
#include "sim/actor_map.hpp"
#include "sim/control_groups.hpp"
#include "sim/player.hpp"
#include "sim/world.hpp"

using namespace ora;

namespace ora::gen {
void RegisterGeneratedAll();  // gen/gen_all.cpp(与其他测试同型的显式注册)
}
                              // (the explicit registration, the other
                              // tests' shape.)

namespace {

int g_failures = 0;

void Check(bool ok, const std::string& what) {
  if (ok) {
    std::println("ok: {}", what);
  } else {
    ++g_failures;
    std::println("FAIL: {}", what);
  }
}

template <class T>
void CheckEq(T a, T b, const std::string& what) {
  Check(a == b, std::format("{} ({} == {})", what, a, b));
}

template <class Fn>
bool Throws(std::string_view prefix, Fn&& fn) {
  try {
    fn();
    return false;
  } catch (const std::exception& e) {
    return prefix.empty() || std::string_view{e.what()}.starts_with(prefix);
  }
}

// ———— PriorityQueue ————
void TestPriorityQueue() {
  std::println("--- PriorityQueue ---");
  // 三态 Compare 协议(上游 IComparer<T> 形;CostComparer 同构)
  // The three-way Compare protocol (the upstream IComparer<T> shape;
  // CostComparer is isomorphic).
  struct IntComparer {
    static int Compare(int a, int b) { return a < b ? -1 : (a > b ? 1 : 0); }
  };
  using PQ = ora::PriorityQueue<int, IntComparer>;
  PQ pq{};
  Check(pq.Empty(), "fresh queue empty");

  // 乱序入队 → 弹出为非降序(上游"层级加倍"堆的全序语义)
  // Out-of-order adds → a non-descending pop order (the total-order
  // semantics of upstream's levels-doubled heap).
  for (int v : {5, 1, 4, 1, 3})
    pq.Add(v);
  CheckEq(pq.Peek(), 1, "Peek is the minimum");
  std::vector<int> popped;
  while (!pq.Empty())
    popped.push_back(pq.Pop());
  CheckEq<size_t>(popped.size(), 5, "five pops");
  for (std::size_t i = 1; i < popped.size(); i++)
    Check(popped[i - 1] <= popped[i], "pop order non-descending");

  Check(Throws("PriorityQueue empty.", [&] {
    PQ empty{};
    (void)empty.Peek();
  }), "empty Peek throws verbatim");
}

// ———— LongBitSet ————
void TestLongBitSet() {
  std::println("--- LongBitSet ---");
  class TagA {};
  class TagB {};
  ora::LongBitSet<TagA>::Reset();

  const std::string a = "alpha", b = "beta", c = "gamma";
  const ora::LongBitSet<TagA> set_a{std::span{&a, 1}};
  const ora::LongBitSet<TagA> set_b{std::span{&b, 1}};
  const ora::LongBitSet<TagA> set_c{std::span{&c, 1}};
  Check(!set_a.IsEmpty() && set_a.Bits() != set_b.Bits(),
        "distinct strings get distinct bits");
  CheckEq(set_a.Bits(), std::int64_t{1}, "first allocation = bit 1");

  Check(set_a.Union(set_b).Overlaps(set_a), "union overlaps");
  Check(!set_a.Overlaps(set_b), "disjoint sets don't overlap");
  Check(set_a.Union(set_b).IsSupersetOf(set_a), "union superset");
  Check(set_a.Intersect(set_b).IsEmpty(), "intersect empty");
  Check(set_a.SetEquals(set_a), "SetEquals self");
  Check(set_a.Except(set_a).IsEmpty(), "except self empty");
  CheckEq(set_a.Union(set_b).Except(set_b).Bits(), set_a.Bits(),
          "except roundtrip");

  Check(Throws("Trying to allocate bit index outside of index 64.", [&] {
    ora::LongBitSet<TagB>::Reset();
    for (int i = 0; i < 65; i++) {
      const std::string s = std::format("s{}", i);
      (void)ora::LongBitSet<TagB>{std::span{&s, 1}};
    }
  }), "the 65th allocation overflows verbatim");

  ora::LongBitSet<TagA>::Reset();
  const ora::LongBitSet<TagA> set_a2{std::span{&a, 1}};
  CheckEq(set_a2.Bits(), std::int64_t{1},
          "Reset rewinds the allocator");
}

// ———— 图类型与 Grid ————
void TestGraphTypes() {
  std::println("--- GraphTypes/Grid ---");
  using namespace ora::mods::pathfinding;

  Check(Throws("cost cannot be negative",
               [] { GraphConnection{CPos{0, 0}, -1}; }),
        "negative connection cost throws");
  Check(Throws("cost cannot be used for an unreachable path",
               [] { GraphConnection{CPos{0, 0}, kPathCostForInvalidPath}; }),
        "unreachable connection cost throws");
  Check(Throws("source and destination must refer to different cells",
               [] { GraphEdge{CPos{1, 2}, CPos{1, 2}, 3}; }),
        "self-loop edge throws");

  const CellInfo info{CellStatus::Open, 5, 9, CPos{1, 1}};
  CheckEq(info.cost_so_far, 5, "CellInfo fields");
  Check(Throws("The default CellInfo is the only such CellInfo",
               [] { CellInfo{CellStatus::Unvisited, 0, 0, CPos{}}; }),
        "Unvisited CellInfo ctor throws");

  const Grid grid{CPos{2, 3}, CPos{7, 9}, false};
  CheckEq(grid.Width(), 5, "Grid width");
  Check(grid.Contains(CPos{3, 4}), "Grid contains inside");
  Check(!grid.Contains(CPos{1, 4}), "Grid excludes left");
  Check(!grid.Contains(CPos{3, 9}), "Grid excludes bottom (exclusive)");
  const Grid layered{CPos{2, 3, 1}, CPos{7, 9, 1}, true};
  Check(!layered.Contains(CPos{3, 4}), "single-layer grid rejects layer 0");
  Check(layered.Contains(CPos{3, 4, 1}), "single-layer grid accepts layer 1");
  Check(grid.IntersectsLine(CPos{0, 5}, CPos{9, 5}), "line crosses grid");
  Check(!grid.IntersectsLine(CPos{3, 4}, CPos{6, 8}),
        "contained line does not intersect");
}

// ———— 稀疏图 A*(ToTargetCellOverGraph 面)————
void TestSparseGraphSearch(mods::pathfinding::Locomotor* locomotor) {
  std::println("--- SparseGraph A* ---");
  using namespace ora::mods::pathfinding;

  // 5 格直线图:0..4,等代价 10 | a 5-cell line graph, uniform cost 10.
  SparsePathGraph::EdgesFn line_edges = [](CPos cell) {
    const int x = cell.X();
    std::vector<GraphConnection> out;
    if (x > 0)
      out.push_back(GraphConnection{CPos{x - 1, 0}, 10});
    if (x < 4)
      out.push_back(GraphConnection{CPos{x + 1, 0}, 10});
    return out;
  };

  auto search2 = PathSearch::ToTargetCellOverGraph(
      line_edges, locomotor, CPos{0, 0}, CPos{4, 0});
  const std::vector<CPos> path = search2->FindPath();
  // 返回路径 = target→source(上游注释)| the returned path is
  // target→source (the upstream comment).
  CheckEq<size_t>(path.size(), 5, "line path length");
  Check(path.front() == CPos{4, 0} && path.back() == CPos{0, 0},
        "path runs target to source");

  // 不可达图 | an unreachable graph.
  SparsePathGraph::EdgesFn dead_edges = [](CPos) {
    return std::vector<GraphConnection>{};
  };
  auto search3 = PathSearch::ToTargetCellOverGraph(dead_edges, locomotor,
                                                   CPos{0, 0}, CPos{1, 0});
  Check(search3->FindPath().empty(), "unreachable yields NoPath");

  // ExpandAll 计数 | the ExpandAll count.
  auto search4 = PathSearch::ToTargetCellOverGraph(line_edges, locomotor,
                                                   CPos{2, 0}, CPos{4, 0});
  CheckEq<size_t>(search4->ExpandAll().size(), 5,
                  "ExpandAll visits the whole reachable line");

  // 双向搜索:两半相接 | the bidirectional search: two halves meeting.
  auto left = PathSearch::ToTargetCellOverGraph(line_edges, locomotor,
                                                CPos{0, 0}, CPos{4, 0});
  auto right = PathSearch::ToTargetCellOverGraph(line_edges, locomotor,
                                                 CPos{4, 0}, CPos{0, 0});
  const std::vector<CPos> bidi = PathSearch::FindBidiPath(*left, *right);
  Check(!bidi.empty() && bidi.front() == CPos{0, 0} &&
            bidi.back() == CPos{4, 0},
        "bidi path runs source to target");
}

using sim::Actor;
using sim::ActorInitializer;
using sim::TypeDictionary;

// ———— 测试桩:固定格占位 trait(上游 Immobile 的 IOccupySpace 面)————
class StubOccupy final : public sim::TraitBase,
                         public sim::IOccupySpace,
                         public sim::INotifyAddedToWorld,
                         public sim::INotifyRemovedFromWorld {
 public:
  ORA_TRAIT_INTERFACES(StubOccupy, OpenRA_Mods_Common_Traits_Immobile,
                       IOccupySpace, INotifyAddedToWorld,
                       INotifyRemovedFromWorld)

  explicit StubOccupy(std::vector<std::pair<CPos, sim::SubCell>> cells)
      : cells_{std::move(cells)} {}

  WPos CenterPosition() const override {
    return WPos{cells_[0].first.X() * 1024 + 512,
                cells_[0].first.Y() * 1024 + 512, 0};
  }
  CPos TopLeft() const override { return cells_[0].first; }
  std::vector<std::pair<CPos, sim::SubCell>> OccupiedCells() const override {
    return cells_;
  }

  // 上游 Immobile 的登记面(INotifyAddedToWorld → World.AddToMaps)
  // The upstream Immobile registration face (INotifyAddedToWorld →
  // World.AddToMaps).
  void AddedToWorld(Actor& self) override {
    self.world().AddToMaps(&self, this);
  }
  void RemovedFromWorld(Actor& self) override {
    self.world().RemoveFromMaps(&self, this);
  }

 private:
  std::vector<std::pair<CPos, sim::SubCell>> cells_;
};

// ———— 世界夹具:合成地图 + ActorMap/Locomotor/PathFinder 装配 ————
struct WorldFixture {
  std::unique_ptr<map::Map> map_world;
  std::unique_ptr<sim::World> world;
  sim::ActorMap* actor_map = nullptr;
  mods::pathfinding::Locomotor* locomotor = nullptr;
  mods::pathfinding::PathFinder* path_finder = nullptr;
  std::uint8_t water_index = 255;

  static std::unique_ptr<WorldFixture> Make(game::ModData& mod_data,
                                            const map::ITerrainInfo& terrain);
};

std::unique_ptr<WorldFixture> WorldFixture::Make(
    game::ModData& mod_data, const map::ITerrainInfo& terrain) {
  auto fx = std::make_unique<WorldFixture>();

  // 12×12 合成地图(全 Clear;CustomTerrain 造墙)| a 12×12 synthetic map
  // (all Clear; CustomTerrain builds walls).
  fx->map_world = std::make_unique<map::Map>(
      map::Map::Params{.mod_data = &mod_data}, terrain, ora::Size{12, 12});
  // 编辑器构造的地图 Bounds 为退化值(上游同型)—— 导入路径显式设全域
  // (an editor-constructed map carries the degenerate Bounds (the
  // upstream shape) — the import path sets the full area explicitly.)
  fx->map_world->SetBounds(PPos{0, 0}, PPos{11, 11});
  // TEMPERAT 的 Water 索引(造墙面)| TEMPERAT's Water index (the wall
  // face).
  for (std::size_t i = 0; i < terrain.TerrainTypes().size(); i++)
    if (terrain.TerrainTypes()[i].Type == "Water")
      fx->water_index = static_cast<std::uint8_t>(i);

  fx->world = std::make_unique<sim::World>(
      sim::WorldSimParams{.ptr_map = fx->map_world.get()});

  // 世界系统 actor:ActorMap + Locomotor + PathFinder(上游形态;测试
  // trait 为堆分配的一次性夹具 —— 进程生命期,登记于 COVERAGE 测试面)
  // The world system actor: ActorMap + Locomotor + PathFinder (the
  // upstream shape; the test traits are one-shot heap fixtures — process
  // lifetime, registered as the COVERAGE test face).
  WorldFixture* fx_raw = fx.get();  // 工厂按值捕获裸指针( unique_ptr 局部
                                    // 的引用捕获会随 Make 返回悬垂)
                                    // (the factory captures the raw pointer
                                    // by value — a by-reference capture of
                                    // the local unique_ptr would dangle
                                    // after Make returns.)
  fx->world->SetTraitFactory(
      [fx_raw](const std::string& name,
               sim::ActorInitializer& init) -> std::vector<sim::TraitBase*> {
        if (name == "sysworld") {
          fx_raw->actor_map = new sim::ActorMap(init.Self().world(), 4);
          mods::pathfinding::LocomotorInfo info{
              "default", 40, 10, false, true, {}, {}, {},
              {{"Clear", {100, 100}}}, false};
          fx_raw->locomotor = new mods::pathfinding::Locomotor(
              init.Self(), std::move(info));
          fx_raw->path_finder =
              new mods::pathfinding::PathFinder(init.Self(), 125);
          return {fx_raw->actor_map, fx_raw->locomotor,
                  fx_raw->path_finder};
        }
        if (name == "e1")
          return {new StubOccupy{{{CPos{2, 2}, sim::SubCell::FullCell}}}};
        if (name == "e2")
          return {new StubOccupy{{{CPos{7, 7}, sim::SubCell::FullCell}}}};
        return {};
      });

  TypeDictionary dict;
  Actor* world_actor = fx->world->CreateActor(true, "sysworld", dict);
  Check(world_actor != nullptr, "system world actor created");
  // 该 actor 即世界 actor(上游 World ctor 的解析面)—— WorldActor 指针
  // 装订后 pf.WorldLoaded 的 Locomotor 表查询才可达
  // (this actor IS the world actor — the binding makes pf.WorldLoaded's
  // Locomotor-table lookup reachable.)
  fx->world->SetWorldActor(world_actor);

  // WorldLoaded 装配(loco 先于 pf —— pf.WorldLoaded 读世界 Locomotor 表)
  // WorldLoaded assembly (loco before pf — pf.WorldLoaded reads the world's
  // Locomotor table).
  fx->world->SetActorMapFace(fx->actor_map);
  // wr 不被两者触碰(传 null 指针)| wr is untouched by both (null passed).
  fx->locomotor->WorldLoaded(*fx->world, nullptr);
  fx->path_finder->WorldLoaded(*fx->world, nullptr);
  fx->path_finder->SetActorLocomotorResolver(
      [fx_raw](Actor&) { return fx_raw->locomotor; });

  return fx;
}

void TestWorldFaces(std::unique_ptr<WorldFixture> fx_ptr) {
  std::println("--- WorldFixture: ActorMap/ControlGroups/Pathfinding ---");
  std::unique_ptr<WorldFixture> fx = std::move(fx_ptr);
  auto& world = *fx->world;

  Check(world.ActorMapFace() == fx->actor_map, "ActorMap face wired");

  // ———— 影响面:stub actor 进图/出图 ————
  TypeDictionary dict;
  Actor* e1 = world.CreateActor(true, "e1", dict);
  Check(e1 != nullptr && e1->IsInWorld(), "e1 created in world");
  auto at = fx->actor_map->GetActorsAt(CPos{2, 2});
  CheckEq(at.size(), std::size_t{1}, "GetActorsAt finds e1");
  Check(at.size() == 1 && at[0] == e1, "GetActorsAt returns e1");
  Check(fx->actor_map->AnyActorsAt(CPos{2, 2}), "AnyActorsAt true");

  // 子格面:FullCell 占用阻塞 First 子格 | the subcell face: a FullCell
  // occupancy blocks the First subcell.
  Check(!fx->actor_map->HasFreeSubCell(CPos{2, 2}), "no free subcell");
  Check(fx->actor_map->FreeSubCell(CPos{2, 2}, sim::SubCell::Any) ==
            sim::SubCell::Invalid,
        "FreeSubCell invalid when full");

  // 空格子 | an empty cell.
  Check(fx->actor_map->HasFreeSubCell(CPos{5, 5}), "empty cell free");
  Check(fx->actor_map->FreeSubCell(CPos{5, 5}, sim::SubCell::Any) ==
            static_cast<sim::SubCell>(fx->map_world->Grid().DefaultSubCell()),
        "FreeSubCell default on empty node-free cell");

  // 盒查询(位置缓存在下一 tick 落位 —— 上游同型)| the box query (the
  // position cache lands at the next tick — the upstream shape).
  fx->actor_map->Tick(*e1);
  auto in_box = fx->actor_map->ActorsInBox(WPos{0, 0, 0}, WPos{4096, 4096, 0});
  CheckEq(in_box.size(), std::size_t{1}, "ActorsInBox finds e1");

  // 出图 → 影响摘除 | leaving the world removes the influence.
  world.Remove(e1);
  Check(fx->actor_map->GetActorsAt(CPos{2, 2}).empty(),
        "influence removed after Remove");
  // (上游 RemoveInfluence 由 Actor.Dispose 帧末任务走 —— 此处直测 Remove
  // 面的脏格路径;Dispose 链由 map_test 的世界析构覆盖)
  // (upstream RemoveInfluence rides the Actor.Dispose frame-end task — the
  // Remove face's dirty path is tested directly here; the Dispose chain is
  // covered by map_test's world teardown.)

  // ———— CellTrigger:进入/退出 ————
  int entered = 0, exited = 0;
  const int trigger_id = fx->actor_map->AddCellTrigger(
      {CPos{7, 7}}, [&](Actor&) { ++entered; }, [&](Actor&) { ++exited; });
  CheckEq(trigger_id, 0, "first trigger id");
  CheckEq(fx->actor_map->TriggerPositions().size(), std::size_t{1},
          "TriggerPositions");

  Actor* e2 = world.CreateActor(true, "e2", dict);
  Check(fx->actor_map->GetActorsAt(CPos{7, 7}).size() == 1,
        "e2 occupies the trigger cell");
  fx->actor_map->Tick(*e2);  // ITick 面(脏触发器在此推进)| the ITick face.
  CheckEq(entered, 1, "cell trigger entered");
  CheckEq(exited, 0, "no exit yet");

  fx->actor_map->RemoveCellTrigger(trigger_id);
  fx->actor_map->Tick(*e2);
  CheckEq(entered, 1, "removed trigger does not re-enter");

  // ———— ControlGroups ————
  sim::ControlGroups groups{world};
  CheckEq(groups.Groups().size(), std::size_t{10}, "default group count");
  groups.AddToControlGroup(e2, 3);
  CheckEq(groups.GetControlGroupForActor(e2).value_or(-1), 3,
          "group 3 assigned");
  auto members = groups.GetActorsInControlGroup(3);
  CheckEq(members.size(), std::size_t{1}, "group 3 member count");
  groups.RemoveFromControlGroup(e2);
  Check(!groups.GetControlGroupForActor(e2).has_value(),
         "group cleared after removal");
  groups.AddToControlGroup(e2, 4);
  groups.Tick(*e2);  // e2 无 Owner 且 LocalPlayer 为空:Owner(null) !=
                     // LocalPlayer(null) 为假 → 保留(上游比较语义)
                     // (e2 has no owner and LocalPlayer is null: the
                     // comparison is false → retained, the upstream
                     // comparison semantics.)
  Check(groups.GetControlGroupForActor(e2).has_value(),
        "null-owner actor retained by Tick");

  // ———— Locomotor/PathFinder/HPF 全链(真实 ra 地形)————
  // 代价面:Clear 格可走 | the cost face: Clear cells walkable.
  CheckEq(fx->locomotor->MovementCostForCell(CPos{2, 2}),
          static_cast<short>(100), "Clear cell cost 100");

  // 墙:整列 CustomTerrain → Water(不在 Locomotor 速度表 → 不可达)
  // The wall: a full CustomTerrain column → Water (absent from the
  // Locomotor's speeds → unreachable).
  const std::uint8_t water_index = fx->water_index;
  Check(water_index != 255, "TEMPERAT has Water");
  for (int y = 0; y < 12; y++)
    fx->map_world->CustomTerrainRef().Set(
        ora::MPos{4, y}.ToCPos(ora::MapGridType::Rectangular), water_index);
  // 地形变化面:CellEntryChanged 监听器驱动 UpdateCellCost
  // (Locomotor.WorldLoaded 挂载)| the terrain-change face: the
  // CellEntryChanged listeners drive UpdateCellCost (wired in
  // Locomotor.WorldLoaded).
  CheckEq(fx->locomotor->MovementCostForCell(CPos{4, 2}),
          mods::pathfinding::kMovementCostForUnreachableCell,
          "wall cell unreachable");

  auto* locomotor_ptr = fx->locomotor;
  Check(fx->path_finder->PathExistsForLocomotor(locomotor_ptr, CPos{1, 1},
                                                CPos{3, 3}),
        "path exists within the west half");

  // 全高墙 → 东西不连通 | the full-height wall → east/west disconnected.
  Check(!fx->path_finder->PathExistsForLocomotor(locomotor_ptr, CPos{1, 1},
                                                 CPos{6, 1}),
        "no path across the full wall");
  const CPos src_west{1, 1};
  auto no_path = fx->path_finder->FindPathToTargetCell(
      nullptr, std::span{&src_west, 1}, CPos{6, 1},
      sim::BlockedByActor::None);
  Check(no_path.empty(), "NoPath across the wall");

  // 开口 → 绕行路径 | a gap → a detouring path.
  fx->map_world->CustomTerrainRef().Set(ora::MPos{4, 0}.ToCPos(ora::MapGridType::Rectangular), 255);
  Check(fx->path_finder->PathExistsForLocomotor(locomotor_ptr, CPos{1, 1},
                                                CPos{6, 1}),
        "path exists through the gap");
  auto detour = fx->path_finder->FindPathToTargetCell(
      nullptr, std::span{&src_west, 1}, CPos{6, 1},
      sim::BlockedByActor::None);
  Check(!detour.empty(), "detour path found");
  if (!detour.empty()) {
    Check(detour.front() == CPos{6, 1} && detour.back() == CPos{1, 1},
          "detour runs target to source");
    Check(detour.size() > 5,
          "detour is longer than the straight line (manhattan 5)");
  }

  // 单格快路径(L177-206 的邻近分支)| the single-cell fast path (the
  // adjacency branch of L178-216).
  auto adjacent = fx->path_finder->FindPathToTargetCell(
      nullptr, std::span{&src_west, 1}, CPos{2, 1},
      sim::BlockedByActor::None);
  CheckEq(adjacent.size(), std::size_t{2}, "adjacent path = {target, source}");

  // HPF 域查询面 | the HPF domain-query face.
  Check(fx->path_finder->PathMightExistForLocomotorBlockedByImmovable(
            locomotor_ptr, CPos{1, 1}, CPos{3, 3}),
        "Immovable HPF path exists");
}

// ———— Player 关系面 / ResolveFaction ————
void TestPlayerFaces() {
  std::println("--- Player faces ---");
  sim::Player p1{"P1", "p1", 0};
  sim::Player p2{"P2", "p2", 1};

  Check(p1.RelationshipWith(&p1) == sim::PlayerRelationship::Ally,
        "self is ally");
  Check(p1.RelationshipWith(nullptr) == sim::PlayerRelationship::Ally,
        "null other is ally (non-combatant default false)");
  Check(!p1.IsAlliedWith(&p2), "unrelated players are not allied");

  // 掩码并集 → 盟友 | the mask union → allies.
  p1.AlliedPlayersMask = p1.AlliedPlayersMask.Union(p2.PlayerMask);
  Check(p1.IsAlliedWith(&p2), "mask union makes allies");
  p1.AlliedPlayersMask = {};
  p1.EnemyPlayersMask = p1.EnemyPlayersMask.Union(p2.PlayerMask);
  Check(p1.RelationshipWith(&p2) == sim::PlayerRelationship::Enemy,
        "enemy via mask");

  // Spectating → ally(观察者视角)| Spectating → ally (the observer view).
  p2.SetWinState(sim::WinState::Won);
  Check(p2.Spectating(), "won player spectates");
  Check(p1.RelationshipWith(&p2) == sim::PlayerRelationship::Ally,
        "spectator is ally to combatants (other side)");
}

void TestResolveFaction() {
  std::println("--- ResolveFaction ---");
  using sim::FactionInfoData;

  FactionInfoData allies;
  allies.InternalName = "allies";
  FactionInfoData england;
  england.InternalName = "england";
  england.RandomFactionMembers = {"allies", "soviet"};
  FactionInfoData soviet;
  soviet.InternalName = "soviet";
  FactionInfoData secret;
  secret.InternalName = "secret";
  secret.Selectable = false;
  const std::vector<FactionInfoData> infos{allies, england, soviet, secret};

  MersenneTwister r1{7};
  // 可选面过滤:secret 不可选 | the selectable filter: secret excluded.
  const auto picked =
      sim::Player::ResolveFaction("nonexistent", infos, r1, true);
  Check(picked.InternalName != "secret", "non-selectable faction excluded");

  // 确定性:同种子同结果 | determinism: the same seed, the same result.
  MersenneTwister r2{7}, r3{7};
  const auto f2 = sim::Player::ResolveFaction("x", infos, r2, true);
  const auto f3 = sim::Player::ResolveFaction("x", infos, r3, true);
  Check(f2.InternalName == f3.InternalName, "ResolveFaction deterministic");

  // RandomFactionMembers 展开 | the RandomFactionMembers expansion.
  const auto expanded =
      sim::Player::ResolveFaction("england", infos, r2, true);
  Check(expanded.InternalName == "allies" || expanded.InternalName == "soviet",
        "random-member expansion resolves to a concrete faction");

  // requireSelectable=false 命中 secret | requireSelectable=false hits
  // secret.
  CheckEq(sim::Player::ResolveFaction("secret", infos, r2, false).InternalName,
          std::string{"secret"}, "non-selectable exact match");

  Check(Throws("Unknown faction:", [&] {
    FactionInfoData loop;
    loop.InternalName = "loop";
    loop.RandomFactionMembers = {"missing"};
    std::vector<FactionInfoData> one{loop};
    MersenneTwister r{1};
    (void)sim::Player::ResolveFaction("loop", one, r, true);
  }), "missing random member throws verbatim");
}

}  // namespace

int main(int argc, char** argv) {
  gen::RegisterGeneratedAll();  // 生成注册表(枚举/BitSet/类型工厂)
                                // (the generated registries.)
  game::RegisterGameLoaders();  // LoadUsing 加载器(HitShape/LocomotorInfo)
                                // (the LoadUsing loaders — HitShape/
                                // LocomotorInfo).

  if (argc > 1) {
    // 真实 mod 链(ra 规则 + TEMPERAT 地形)
    // The real-mod chain (ra rules + the TEMPERAT tileset).
    try {
      game::InstalledMods mods_installed{std::string{argv[1]} + "/mods"};
      const game::Manifest* manifest = mods_installed.Find("ra");
      Check(manifest != nullptr, "ra mod found");
      if (manifest != nullptr) {
        game::ModData mod_data{*manifest, mods_installed, argv[1]};
        const map::ITerrainInfo& terrain = mod_data.GetTerrainInfo("TEMPERAT");
        auto fx = WorldFixture::Make(mod_data, terrain);
        TestSparseGraphSearch(fx->locomotor);
        TestWorldFaces(std::move(fx));
      }
    } catch (const std::exception& e) {
      ++g_failures;
      std::println("FAIL: real-mod chain raised: {}", e.what());

  }

  if (g_failures == 0)
    std::println("\npath_test: all passed");
  else
    std::println("\npath_test: {} FAILURES", g_failures);
  return g_failures == 0 ? 0 : 1;
}
}
