#pragma once

import std;

namespace ora::game::pathfinding {

struct CPos {
  std::int32_t int4_x{};
  std::int32_t int4_y{};

  friend bool operator==(const CPos& a, const CPos& b) {
    return a.int4_x == b.int4_x && a.int4_y == b.int4_y;
  }
};

struct GraphConnection {
  CPos cell;
  std::int32_t int4_cost{};
};

struct LevelOrderQueue {
  std::vector<GraphConnection> vec_items;
  std::int32_t int4_level{0};
  std::int32_t int4_index{0};
};

void queue_add(LevelOrderQueue& q, const GraphConnection& item);
bool queue_empty(const LevelOrderQueue& q);
GraphConnection queue_pop(LevelOrderQueue& q);

}  // namespace ora::game::pathfinding
