// UPSTREAM: OpenRA.Game/Traits/Target.cs @b6fc03f L18-293(仿真核心子集逐语义
//          重写;FrozenActor 依赖 Shroud/FrozenActorLayer,Phase 5 落地前以
//          前向指针承载 —— 上游 FrozenActorLayer 为 null 时同样落 Invalid 分支)
//          Verbatim-semantics rewrite of the sim-core subset; FrozenActor
//          depends on Shroud/FrozenActorLayer (Phase 5), carried here as a
//          forward-declared pointer — upstream falls into the Invalid branch
//          when FrozenActorLayer is null, which stays reachable.
//
// 机制对照 / Mechanism mapping:
//  - readonly struct + in 传参 → 值类型按值/const 引用传(C++ 结构体自然形态)
//    readonly struct + `in` parameters → a value type passed by value /
//    const-ref (the natural C++ shape).
//  - FromCell(World, CPos, SubCell):依赖 Map.CenterOfSubCell(Phase 5);
//    本期不提供,Order 反序列化的 TargetIsCell 分支经 World 桩换算(见
//    order.cpp,COVERAGE 登记)
//    FromCell(World, CPos, SubCell): depends on Map.CenterOfSubCell
//    (Phase 5); not provided this phase — the TargetIsCell branch of order
//    deserialization goes through a World stub conversion (order.cpp,
//    registered in COVERAGE).
//  - IsValidFor/RequiresForceFire/IsInRange:依赖 trait 查询(ISelectable/
//    ITargetable 的运行时面,Phase 5 trait 类);本期保留定义,Phase 5 接线
//    IsValidFor/RequiresForceFire/IsInRange: depend on trait queries (the
//    runtime surface of ISelectable/ITargetable, Phase 5); declared here,
//    wired in Phase 5.
#pragma once
import std;

#include "core/cell_pos.hpp"
#include "core/wdist.hpp"
#include "core/wpos.hpp"
#include "sim/trait_interfaces.hpp"

namespace ora::sim {

class Actor;
class FrozenActor;  // Phase 5(Shroud);序列化面只取 ID / Phase 5 (Shroud); serialization touches only its ID.

/// Target.cs L18
enum class TargetType : std::uint8_t {
  Invalid = 0,
  Actor = 1,
  Terrain = 2,
  FrozenActor = 3,
};

/// Target.cs L19-293(值语义)
/// Target.cs L19-293 (value semantics).
struct Target {
  /// Target.cs L21-22
  static const std::vector<Target> None;
  static Target Invalid() { return Target{}; }

  const Actor* ActorPtr = nullptr;        // Actor(只读;判定用)
  const FrozenActor* FrozenActorPtr = nullptr;  // FrozenActor

  TargetType type = TargetType::Invalid;
  WPos terrain_center_position;     // terrainCenterPosition
  std::vector<WPos> vec_terrain_positions;  // terrainPositions(null 区分:空 = null)
  bool b_has_cell = false;                // CPos? cell 的有值标记
  CPos cell{};
  bool b_has_sub_cell = false;            // SubCell? subCell 的有值标记
  SubCell sub_cell = SubCell::FullCell;
  int generation = 0;                     // Actor 代际

  /// Target.cs L33-44:Terrain(pos)
  static Target FromPos(const WPos& p) {
    Target t;
    t.type = TargetType::Terrain;
    t.terrain_center_position = p;
    t.vec_terrain_positions = std::vector<WPos>{p};
    return t;
  }

  /// Target.cs L59-70:Actor(a, generation)
  static Target FromActor(const Actor* a);

  /// Target.cs L88:FromFrozenActor(fa)
  static Target FromFrozenActor(const FrozenActor* fa) {
    Target t;
    t.type = TargetType::FrozenActor;
    t.FrozenActorPtr = fa;
    return t;
  }

  /// Target.cs L290(序列化专用,order 代码独用)
  /// Target.cs L290 (serialization-only, used by the orders code alone).
  static Target FromSerializedActor(Actor* a, int actor_generation);
  static Target FromSerializedTerrainPosition(
      const WPos& center_position,
      std::vector<WPos> terrain_positions) {
    Target t;
    t.type = TargetType::Terrain;
    t.terrain_center_position = center_position;
    t.vec_terrain_positions = std::move(terrain_positions);
    return t;
  }

  /// Target.cs L91-107:Type 的有效性判定(Actor 出世界/死亡/换代 → Invalid)
  /// Target.cs L91-107: the Type validity check (an Actor out of world /
  /// dead / generation-bumped degrades to Invalid).
  TargetType Type() const;

  /// Target.cs L155-172
  WPos CenterPosition() const;

  /// Target.cs L175-194(Positions;Invalid → 空)
  const std::vector<WPos>& Positions() const;

  /// Target.cs L196-203:射程判定(2D 距离)
  /// Target.cs L196-203: range check (2D distance).
  bool IsInRange(const WPos& origin, const WDist& range) const;

  /// Target.cs L292-293(序列化状态导出)
  /// Target.cs L292-293 (the serialization state export).
  struct SerializableState {
    TargetType type;
    const Actor* actor;
    int generation;
    bool has_cell;
    CPos cell;
    bool has_sub_cell;
    SubCell sub_cell;
    WPos pos;
    const std::vector<WPos>* terrain_positions;  // null 区分保留
  };
  SerializableState Serializable() const {
    return SerializableState{type,           ActorPtr,
                             generation,     b_has_cell,
                             cell,           b_has_sub_cell,
                             sub_cell,       terrain_center_position,
                             vec_terrain_positions.empty()
                                 ? nullptr
                                 : &vec_terrain_positions};
  }
};

/// Sync.cs HashTarget(L138-159)
int HashTarget(const Target& t);

}  // namespace ora::sim
