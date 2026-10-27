#pragma once
import std;

#include "core/cell_pos.hpp"
#include "core/wdist.hpp"
#include "core/wpos.hpp"
#include "sim/trait_interfaces.hpp"

namespace ora::sim {

class Actor;
class FrozenActor;

enum class TargetType : std::uint8_t {
  Invalid = 0,
  Actor = 1,
  Terrain = 2,
  FrozenActor = 3,
};

struct Target {

  static const std::vector<Target> None;
  static Target Invalid() { return Target{}; }

  const Actor* ActorPtr = nullptr;
  const FrozenActor* FrozenActorPtr = nullptr;

  TargetType type = TargetType::Invalid;
  WPos terrain_center_position;
  std::vector<WPos> vec_terrain_positions;
  bool b_has_cell = false;
  CPos cell{};
  bool b_has_sub_cell = false;
  SubCell sub_cell = SubCell::FullCell;
  int generation = 0;

  static Target FromPos(const WPos& p) {
    Target t;
    t.type = TargetType::Terrain;
    t.terrain_center_position = p;
    t.vec_terrain_positions = std::vector<WPos>{p};
    return t;
  }

  static Target FromCell(const class World& w, CPos c,
                         SubCell sub_cell = SubCell::FullCell);

  static Target FromActor(const Actor* a);

  static Target FromFrozenActor(const FrozenActor* fa) {
    Target t;
    t.type = TargetType::FrozenActor;
    t.FrozenActorPtr = fa;
    return t;
  }

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

  TargetType Type() const;

  bool IsValidFor(const Actor* targeter) const;

  bool RequiresForceFire() const;

  static Target FromTargetPositions(const Target& t) {
    Target r;
    r.type = TargetType::Terrain;
    r.terrain_center_position = t.terrain_center_position;
    r.vec_terrain_positions = t.vec_terrain_positions;
    return r;
  }

  Target Recalculate(const Player* viewer, bool& b_target_is_hidden_actor) const;

  WPos CenterPosition() const;

  const std::vector<WPos>& Positions() const;

  bool IsInRange(const WPos& origin, const WDist& range) const;

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

  friend bool operator!=(const Target& me, const Target& other) {
    return !(me == other);
  }

  struct SerializableState {
    TargetType type;
    const Actor* actor;
    int generation;
    bool has_cell;
    CPos cell;
    bool has_sub_cell;
    SubCell sub_cell;
    WPos pos;
    const std::vector<WPos>* terrain_positions;
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

int HashTarget(const Target& t);

}
