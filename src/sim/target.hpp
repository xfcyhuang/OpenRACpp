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

  /// Target.cs L87:FromPos(p) | Target.cs L87: FromPos(p).
  static Target FromPos(const WPos& p) {
    Target t;
    t.type = TargetType::Terrain;
    t.terrain_center_position = p;
    t.vec_terrain_positions = std::vector<WPos>{p};
    return t;
  }

  /// Target.cs L87-88:FromCell(w, c, subCell)(CenterOfSubCell 依赖已随
  /// Map 批解除 —— D28)
  /// Target.cs L87-88: FromCell(w, c, subCell) (the CenterOfSubCell
  /// dependency unlocked with the Map batch — D28).
  static Target FromCell(const class World& w, CPos c,
                         SubCell sub_cell = SubCell::FullCell);

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

  /// Target.cs L110-125:IsValidFor(targeter)
  /// Target.cs L110-125: IsValidFor(targeter).
  bool IsValidFor(const Actor* targeter) const;

  /// Target.cs L131-152:RequiresForceFire(全有或全无)
  /// Target.cs L131-152: RequiresForceFire (all or nothing).
  bool RequiresForceFire() const;

  /// Target.cs L86:FromTargetPositions(t)(地形化快照)
  /// Target.cs L86: FromTargetPositions(t) (the terrain-ized snapshot).
  static Target FromTargetPositions(const Target& t) {
    Target r;
    r.type = TargetType::Terrain;
    r.terrain_center_position = t.terrain_center_position;
    r.vec_terrain_positions = t.vec_terrain_positions;
    return r;
  }

  /// TargetExtensions.cs L31-81:Recalculate(viewer, out targetIsHiddenActor)
  /// —— FrozenActor/可视面未移植段的等价分支(COVERAGE 登记):
  /// bot 视角的 FrozenActor→Invalid、Actor 可视(CanBeViewedByPlayer 真值
  /// 面)、ReplacedByActor 换代修复
  /// TargetExtensions.cs L31-81: Recalculate(viewer, out
  /// targetIsHiddenActor) — the equivalent branches of the not-yet-ported
  /// FrozenActor/visibility faces (registered in COVERAGE): the bot view's
  /// FrozenActor→Invalid, the Actor visibility (the CanBeViewedByPlayer
  /// true face), and the ReplacedByActor replacement fix.
  Target Recalculate(const Player* viewer, bool& b_target_is_hidden_actor) const;

  /// Target.cs L155-172
  WPos CenterPosition() const;

  /// Target.cs L175-194(Positions;Invalid → 空)
  const std::vector<WPos>& Positions() const;

  /// Target.cs L196-203:射程判定(2D 距离)
  /// Target.cs L196-203: range check (2D distance).
  bool IsInRange(const WPos& origin, const WDist& range) const;

  /// Target.cs L224-240:operator ==(第六批消费点:AttackFollow 的持久
  /// 机会目标判定)
  /// Target.cs L224-240: operator == (the batch-6 consumer:
  /// AttackFollow's persistent-opportunity-target check).
  friend bool operator==(const Target& me, const Target& other) {
    if (me.type != other.type)
      return false;

    switch (me.type) {
      case TargetType::Terrain:
        return me.terrain_center_position == other.terrain_center_position &&
               me.vec_terrain_positions == other.vec_terrain_positions &&
               ((!me.b_has_cell && !other.b_has_cell) ||
                (me.b_has_cell && other.b_has_cell &&
                 me.cell == other.cell)) &&
               ((!me.b_has_sub_cell && !other.b_has_sub_cell) ||
                (me.b_has_sub_cell && other.b_has_sub_cell &&
                 me.sub_cell == other.sub_cell));

      case TargetType::Actor:
        return me.ActorPtr == other.ActorPtr &&
               me.generation == other.generation;

      case TargetType::FrozenActor:
        return me.FrozenActorPtr == other.FrozenActorPtr;

      case TargetType::Invalid:
      default:
        return false;
    }
  }

  /// Target.cs L242-246:operator !=
  /// Target.cs L242-246: operator !=
  friend bool operator!=(const Target& me, const Target& other) {
    return !(me == other);
  }

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
