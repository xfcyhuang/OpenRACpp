// UPSTREAM: OpenRA.Game/Primitives/SpatiallyPartitioned.cs @b6fc03f
//          (spatially_partitioned.hpp 的 Actor* 特化实现;OPT-A8 slab 形态)
//          The SpatiallyPartitioned<Actor*> specialization implementation
//          (the OPT-A8 slab form).
import std;

#include "sim/spatially_partitioned.hpp"

#include "sim/actor.hpp"

namespace ora::sim {

void SpatiallyPartitioned<Actor*>::ValidateBounds(Actor* item,
                                                  Rectangle bounds) {
  if (bounds.Width == 0 || bounds.Height == 0)
    throw std::invalid_argument(
        std::format("Bounds of {} are empty.", ToStringOf(item)));
}

std::string SpatiallyPartitioned<Actor*>::ToStringOf(Actor* item) {
  // 上游 $"Bounds of {item} are empty." 的 item 插值 = Actor.ToString =
  // DebugName(Actor.cs L390-397)
  return item != nullptr ? item->DebugName() : std::string{"null"};
}

void SpatiallyPartitioned<Actor*>::EnsureSlab(std::uint32_t actor_id) const {
  const std::size_t size_needed = static_cast<std::size_t>(actor_id) + 1;
  if (slab_bounds_.size() < size_needed) {
    slab_bounds_.resize(size_needed);
    slab_present_.resize(size_needed, 0);
    slab_query_stamp_.resize(size_needed, 0);
    actor_by_id_.resize(size_needed, nullptr);
  }
}

void SpatiallyPartitioned<Actor*>::Add(Actor* item, Rectangle bounds) {
  // Add(L42-47):全局表重复键 = Dictionary.Add 等价抛
  ValidateBounds(item, bounds);
  EnsureSlab(item->ActorID());
  if (slab_present_[item->ActorID()] != 0)
    throw std::runtime_error("An item with the same key has already been added");
  slab_bounds_[item->ActorID()] = bounds;
  slab_present_[item->ActorID()] = 1;
  actor_by_id_[item->ActorID()] = item;
  MutateBins(item->ActorID(), bounds, true);
}

void SpatiallyPartitioned<Actor*>::Set(Actor* item, Rectangle bounds) {
  // this[item] set(L49-62):存在先摘 bin 再写
  ValidateBounds(item, bounds);
  EnsureSlab(item->ActorID());
  const std::uint32_t id = item->ActorID();
  if (slab_present_[id] != 0)
    MutateBins(id, slab_bounds_[id], false);
  slab_bounds_[id] = bounds;
  slab_present_[id] = 1;
  actor_by_id_[id] = item;
  MutateBins(id, bounds, true);
}

const Rectangle& SpatiallyPartitioned<Actor*>::Get(Actor* item) const {
  // 缺失 = KeyNotFoundException 等价(仅已注册且在场者可查询 —— ScreenMap
  // 的调用序保证;与通用模板一致)
  const std::uint32_t id = item->ActorID();
  if (id >= slab_bounds_.size() || slab_present_[id] == 0)
    throw std::runtime_error("The given key was not present in the dictionary.");
  return slab_bounds_[id];
}

bool SpatiallyPartitioned<Actor*>::Remove(Actor* item) {
  // Remove(L64-71)
  const std::uint32_t id = item->ActorID();
  if (id >= slab_bounds_.size() || slab_present_[id] == 0)
    return false;
  MutateBins(id, slab_bounds_[id], false);
  slab_present_[id] = 0;
  return true;
}

std::vector<Actor*> SpatiallyPartitioned<Actor*>::At(int2 location) const {
  // At(L105-112):bin 内逐项界内判定
  const int col = std::clamp(location.X / bin_size_, 0, cols_ - 1);
  const int row = std::clamp(location.Y / bin_size_, 0, rows_ - 1);
  std::vector<Actor*> out;
  for (const std::uint32_t id : BinAt(row, col))
    if (slab_bounds_[id].Contains(location))
      out.push_back(actor_by_id_[id]);
  return out;
}

std::vector<Actor*> SpatiallyPartitioned<Actor*>::InBox(Rectangle box) const {
  // InBox(L114-141):三态去重(单 bin 段免跟踪;bin 全含项免跟踪;其余
  // 最后查询标记)—— 上游 HashSet 的零分配承载
  auto [min_row, max_row, min_col, max_col] = BoundsToBinRowsAndCols(box);

  const bool b_track = !(min_row >= max_row || min_col >= max_col);
  ++uint4_query_serial_;  // 单线程约定(§4.8)| single-threaded (§4.8)

  std::vector<Actor*> out;
  for (int row = min_row; row < max_row; row++) {
    for (int col = min_col; col < max_col; col++) {
      const Rectangle bin_bounds = BinBounds(row, col);
      for (const std::uint32_t id : BinAt(row, col)) {
        const Rectangle& bounds = slab_bounds_[id];

        if (!bounds.IntersectsWith(box))
          continue;

        if (b_track && !bin_bounds.Contains(bounds)) {
          if (slab_query_stamp_[id] == uint4_query_serial_)
            continue;  // 已返回过 | already returned.
          slab_query_stamp_[id] = uint4_query_serial_;
        }

        out.push_back(actor_by_id_[id]);
      }
    }
  }

  return out;
}

void SpatiallyPartitioned<Actor*>::Clear() {
  // Clear(L143-148)
  std::fill(slab_present_.begin(), slab_present_.end(), 0);
  for (auto& bin : vec_bins_)
    bin.clear();
}

std::vector<Actor*> SpatiallyPartitioned<Actor*>::Keys() const {
  std::vector<Actor*> out;
  for (std::uint32_t id = 0; id < slab_present_.size(); id++)
    if (slab_present_[id] != 0)
      out.push_back(actor_by_id_[id]);
  return out;
}

std::vector<Rectangle> SpatiallyPartitioned<Actor*>::Values() const {
  std::vector<Rectangle> out;
  for (std::uint32_t id = 0; id < slab_present_.size(); id++)
    if (slab_present_[id] != 0)
      out.push_back(slab_bounds_[id]);
  return out;
}

int SpatiallyPartitioned<Actor*>::Count() const {
  int count = 0;
  for (const std::uint8_t present : slab_present_)
    count += present != 0 ? 1 : 0;
  return count;
}

bool SpatiallyPartitioned<Actor*>::ContainsKey(Actor* item) const {
  const std::uint32_t id = item->ActorID();
  return id < slab_present_.size() && slab_present_[id] != 0;
}

bool SpatiallyPartitioned<Actor*>::TryGetValue(Actor* item,
                                               Rectangle& value_out) const {
  const std::uint32_t id = item->ActorID();
  if (id >= slab_present_.size() || slab_present_[id] == 0)
    return false;
  value_out = slab_bounds_[id];
  return true;
}

void SpatiallyPartitioned<Actor*>::MutateBins(std::uint32_t actor_id,
                                              Rectangle bounds, bool b_add) {
  // MutateBins(L96-103)
  auto [min_row, max_row, min_col, max_col] = BoundsToBinRowsAndCols(bounds);

  for (int row = min_row; row < max_row; row++) {
    for (int col = min_col; col < max_col; col++) {
      auto& bin = BinAt(row, col);
      if (b_add)
        bin.push_back(actor_id);
      else
        std::erase(bin, actor_id);
    }
  }
}

std::tuple<int, int, int, int>
SpatiallyPartitioned<Actor*>::BoundsToBinRowsAndCols(Rectangle bounds) const {
  // L83-94
  const int top = std::min(bounds.Top(), bounds.Bottom());
  const int bottom = std::max(bounds.Top(), bounds.Bottom());
  const int left = std::min(bounds.Left(), bounds.Right());
  const int right = std::max(bounds.Left(), bounds.Right());

  const int min_row = std::max(0, top / bin_size_);
  const int min_col = std::max(0, left / bin_size_);
  const int max_row =
      std::min(rows_, IntegerDivisionRoundingAwayFromZero(bottom, bin_size_));
  const int max_col =
      std::min(cols_, IntegerDivisionRoundingAwayFromZero(right, bin_size_));
  return {min_row, max_row, min_col, max_col};
}

}  // namespace ora::sim
