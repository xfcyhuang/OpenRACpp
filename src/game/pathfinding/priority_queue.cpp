module ora.game.pathfinding;

import std;

namespace ora::game::pathfinding {

namespace {

std::size_t slot_index(std::int32_t int4_level, std::int32_t int4_index) {
  return static_cast<std::size_t>((1 << int4_level) - 1 + int4_index);
}

std::size_t slot_above(std::int32_t int4_level, std::int32_t int4_index) {
  return static_cast<std::size_t>((1 << (int4_level - 1)) - 1 + (int4_index >> 1));
}

std::size_t slot_last(LevelOrderQueue& q) {
  std::int32_t int4_level{q.int4_level};
  std::int32_t int4_index{q.int4_index};
  if (--int4_index < 0) int4_index = (1 << --int4_level) - 1;
  return slot_index(int4_level, int4_index);
}

void bubble_into(LevelOrderQueue& q, std::int32_t int4_into_level, std::int32_t int4_into_index,
                 const GraphConnection& item) {
  while (true) {
    const std::int32_t int4_down_level{int4_into_level + 1};
    std::int32_t int4_down_index{int4_into_index << 1};

    if (int4_down_level > q.int4_level ||
        (int4_down_level == q.int4_level && int4_down_index >= q.int4_index)) {
      q.vec_items[slot_index(int4_into_level, int4_into_index)] = item;
      return;
    }

    GraphConnection item_down{q.vec_items[slot_index(int4_down_level, int4_down_index)]};
    if (int4_down_level < q.int4_level ||
        (int4_down_level == q.int4_level && int4_down_index < q.int4_index - 1)) {
      const GraphConnection item_down_right{q.vec_items[slot_index(int4_down_level, int4_down_index + 1)]};
      if (item_down.int4_cost >= item_down_right.int4_cost) {
        item_down = item_down_right;
        ++int4_down_index;
      }
    }

    if (item.int4_cost <= item_down.int4_cost) {
      q.vec_items[slot_index(int4_into_level, int4_into_index)] = item;
      return;
    }

    q.vec_items[slot_index(int4_into_level, int4_into_index)] = item_down;
    int4_into_level = int4_down_level;
    int4_into_index = int4_down_index;
  }
}

}

void queue_add(LevelOrderQueue& q, const GraphConnection& item) {
  std::int32_t int4_add_level{q.int4_level};
  std::int32_t int4_add_index{q.int4_index};

  while (int4_add_level >= 1) {
    const GraphConnection item_above{q.vec_items[slot_above(int4_add_level, int4_add_index)]};
    if (item_above.int4_cost > item.int4_cost) {
      q.vec_items[slot_index(int4_add_level, int4_add_index)] = item_above;
      --int4_add_level;
      int4_add_index >>= 1;
    } else {
      break;
    }
  }

  const std::size_t sz_slot{slot_index(int4_add_level, int4_add_index)};
  if (sz_slot >= q.vec_items.size()) q.vec_items.resize(sz_slot + 1);
  q.vec_items[sz_slot] = item;

  if (++q.int4_index >= 1 << q.int4_level) {
    q.int4_index = 0;
    ++q.int4_level;
    const std::size_t sz_count{2 * (std::size_t{1} << q.int4_level)};
    if (sz_count - 1 >= q.vec_items.size()) q.vec_items.resize(sz_count);
  }
}

bool queue_empty(const LevelOrderQueue& q) {
  return q.int4_level == 0;
}

GraphConnection queue_pop(LevelOrderQueue& q) {
  const GraphConnection item_root{q.vec_items[slot_index(0, 0)]};
  bubble_into(q, 0, 0, q.vec_items[slot_last(q)]);
  if (--q.int4_index < 0) q.int4_index = (1 << --q.int4_level) - 1;
  return item_root;
}

}
