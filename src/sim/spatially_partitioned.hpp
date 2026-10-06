// UPSTREAM: OpenRA.Game/Primitives/SpatiallyPartitioned.cs @b6fc03f L19-168
//          (逐语义重写 + OPT-A8 数组化)
//          Verbatim-semantics rewrite + the OPT-A8 array-ization.
//
// OPT-A8(docs/OPTIMIZATION_TRACKER.md):上游每 bin 的 Dictionary<T,Rectangle>
// 与全局 Dictionary<T,Rectangle)(哈希桶 + 装箱)→
//   - Actor 键:ActorID 稠密 ⇒ slab 平行数组直引(bounds/present/查询戳按
//     ActorID 索引),bin 内 = ActorID 列表;InBox 去重 = "最后查询标记"数组
//     (per-query serial,零分配);
//   - 其余键(IEffect* 等无稠密整数键):指针哈希 unordered_map + bin 内
//     指针列表。
//   语义等价:枚举序 = bin 列表插入序(上游 Dictionary 实现序在无删除时同
//   为插入序;筛选后的消费序不进入同步哈希 —— 鼠标拾取/渲染收集域)。
//   OPT-A8 (docs/OPTIMIZATION_TRACKER.md): upstream's per-bin
//   Dictionary<T,Rectangle> plus the global Dictionary become, for dense
//   Actor keys, ActorID-indexed slab arrays (bounds/presence/query-stamp by
//   id) with per-bin id lists and a "last-query stamp" array for InBox dedup
//   (per-query serial, zero allocation); for the remaining keys (IEffect*
//   etc., no dense integer key), an unordered_map keyed by pointer plus
//   per-bin pointer lists. Semantic equivalence: the enumeration order is
//   the bin-list insertion order (upstream's Dictionary implementation order
//   is insertion order absent removals; the filtered consumption order never
//   enters the sync hash — the mouse-picking/render-collection domain).
#pragma once
import std;

#include "core/int2.hpp"
#include "core/rectangle.hpp"

namespace ora::sim {

class Actor;

/// Exts.IntegerDivisionRoundingAwayFromZero(Exts.cs L~240)的等价
/// The Exts.IntegerDivisionRoundingAwayFromZero equivalent.
inline std::int32_t IntegerDivisionRoundingAwayFromZero(std::int32_t a,
                                                        std::int32_t b) {
  const std::int32_t quotient = a / b;
  const std::int32_t remainder = a % b;
  if (remainder == 0)
    return quotient;
  // 远离零取整(正负同向 +1)| round away from zero (either sign).
  return (a < 0) == (b < 0) ? quotient + 1 : quotient - 1;
}

template <class T>
class SpatiallyPartitioned {
 public:
  SpatiallyPartitioned(int width, int height, int bin_size)
      : bin_size_{bin_size},
        rows_{IntegerDivisionRoundingAwayFromZero(height, bin_size)},
        cols_{IntegerDivisionRoundingAwayFromZero(width, bin_size)},
        vec_bins_(static_cast<std::size_t>(rows_) * cols_) {}

  /// ValidateBounds(L36-40):空界 ArgumentException 同文本(item 呈现面 =
  /// ToStringOf 模板点)
  static void ValidateBounds(T item, Rectangle bounds) {
    if (bounds.Width == 0 || bounds.Height == 0)
      throw std::invalid_argument(
          std::format("Bounds of {} are empty.", ToStringOf(item)));
  }

  /// Add(L42-47):重复键 = 上游 Dictionary.Add 等价抛 | Add (L42-47): a
  /// duplicate key throws the Dictionary.Add equivalent.
  void Add(T item, Rectangle bounds) {
    ValidateBounds(item, bounds);
    if (!map_item_bounds_.emplace(item, bounds).second)
      throw std::runtime_error("An item with the same key has already been added");
    MutateBins(item, bounds, true);
  }

  /// this[item] set(L49-62):存在先摘 bin 再写 | the setter (L49-62):
  /// removes from the bins first when present.
  void Set(T item, Rectangle bounds) {
    ValidateBounds(item, bounds);
    const auto it = map_item_bounds_.find(item);
    if (it != map_item_bounds_.end())
      MutateBins(item, it->second, false);
    map_item_bounds_[item] = bounds;
    MutateBins(item, bounds, true);
  }

  /// this[item] get(L49):缺失 KeyNotFoundException 等价 | the getter: a
  /// missing key is the KeyNotFoundException equivalent.
  const Rectangle& Get(T item) const {
    const auto it = map_item_bounds_.find(item);
    if (it == map_item_bounds_.end())
      throw std::runtime_error("The given key was not present in the dictionary.");
    return it->second;
  }

  /// Remove(L64-71) | Remove (L64-71).
  bool Remove(T item) {
    const auto it = map_item_bounds_.find(item);
    if (it == map_item_bounds_.end())
      return false;
    MutateBins(item, it->second, false);
    map_item_bounds_.erase(it);
    return true;
  }

  /// At(L105-112):bin 内逐项界内判定 | At (L105-112): a per-bin bounds
  /// containment scan.
  std::vector<T> At(int2 location) const {
    const int col = std::clamp(location.X / bin_size_, 0, cols_ - 1);
    const int row = std::clamp(location.Y / bin_size_, 0, rows_ - 1);
    std::vector<T> out;
    for (const T item : BinAt(row, col))
      if (Get(item).Contains(location))
        out.push_back(item);
    return out;
  }

  /// InBox(L114-141):去重条件三态(单 bin 段免跟踪;bin 全含项免跟踪;
  /// 其余查询戳)—— 上游 HashSet 语义的零分配承载
  /// InBox (L114-141): the three dedup states (no tracking inside a single
  /// bin-row/col run; no tracking for bin-contained items; query stamps
  /// otherwise) — the zero-allocation carrier of the HashSet semantics.
  std::vector<T> InBox(Rectangle box) const {
    auto [min_row, max_row, min_col, max_col] = BoundsToBinRowsAndCols(box);

    const bool b_track = !(min_row >= max_row || min_col >= max_col);
    ++uint4_query_stamp_;  // 单线程约定(§4.8)| single-threaded (§4.8)

    std::vector<T> out;
    for (int row = min_row; row < max_row; row++) {
      for (int col = min_col; col < max_col; col++) {
        const Rectangle bin_bounds = BinBounds(row, col);
        for (const T item : BinAt(row, col)) {
          const Rectangle& bounds = Get(item);

          // If the item is in the bin, we must check it intersects the box
          // before returning it.(上游注释)
          if (!bounds.IntersectsWith(box))
            continue;

          // PERF: If the item is wholly contained within the bin, we can
          // avoid the cost of tracking it. (上游注释)
          if (b_track && !bin_bounds.Contains(bounds)) {
            if (uint4_stamp_of_.find(item) != uint4_stamp_of_.end() &&
                uint4_stamp_of_[item] == uint4_query_stamp_)
              continue;  // 已返回过 | already returned.
            uint4_stamp_of_[item] = uint4_query_stamp_;
          }

          out.push_back(item);
        }
      }
    }

    return out;
  }

  /// Clear(L143-148) | Clear (L143-148).
  void Clear() {
    map_item_bounds_.clear();
    for (auto& bin : vec_bins_)
      bin.clear();
    uint4_stamp_of_.clear();
  }

  /// Keys/Values/Count/ContainsKey/TryGetValue(L150-154) | the IDictionary
  /// faces (L150-154).
  std::vector<T> Keys() const {
    std::vector<T> out;
    out.reserve(map_item_bounds_.size());
    for (const auto& [item, _] : map_item_bounds_)
      out.push_back(item);
    return out;
  }
  std::vector<Rectangle> Values() const {
    std::vector<Rectangle> out;
    out.reserve(map_item_bounds_.size());
    for (const auto& [_, bounds] : map_item_bounds_)
      out.push_back(bounds);
    return out;
  }
  int Count() const { return static_cast<int>(map_item_bounds_.size()); }
  bool ContainsKey(T item) const { return map_item_bounds_.contains(item); }
  bool TryGetValue(T item, Rectangle& value_out) const {
    const auto it = map_item_bounds_.find(item);
    if (it == map_item_bounds_.end())
      return false;
    value_out = it->second;
    return true;
  }

 protected:
  /// ToStringOf(L36 的 item 呈现;通用型 = 指针/值呈现面,Actor 特化改写)
  /// The item presentation of L36 (generic pointer/value face; the Actor
  /// specialization overrides).
  static std::string ToStringOf(T item) {
    return std::format("{}",
                       reinterpret_cast<std::uintptr_t>(
                           const_cast<std::remove_const_t<
                               std::remove_pointer_t<T>>*>(item)));
  }

  const std::vector<T>& BinAt(int row, int col) const {
    return vec_bins_[static_cast<std::size_t>(row) * cols_ + col];
  }
  std::vector<T>& BinAt(int row, int col) {
    return vec_bins_[static_cast<std::size_t>(row) * cols_ + col];
  }

  Rectangle BinBounds(int row, int col) const {
    return Rectangle::FromLTRB(col * bin_size_, row * bin_size_,
                               col * bin_size_ + bin_size_,
                               row * bin_size_ + bin_size_);
  }

  std::tuple<int, int, int, int> BoundsToBinRowsAndCols(Rectangle bounds) const {
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

  /// MutateBins(L96-103):action = 加/摘(上游委托双态 → bool 参数)
  /// MutateBins (L96-103): the add/remove delegate pair becomes a bool.
  void MutateBins(T item, Rectangle bounds, bool b_add) {
    auto [min_row, max_row, min_col, max_col] = BoundsToBinRowsAndCols(bounds);

    for (int row = min_row; row < max_row; row++) {
      for (int col = min_col; col < max_col; col++) {
        auto& bin = BinAt(row, col);
        if (b_add)
          bin.push_back(item);
        else
          std::erase(bin, item);
      }
    }
  }

  int bin_size_;
  int rows_, cols_;
  std::vector<std::vector<T>> vec_bins_;
  std::unordered_map<T, Rectangle> map_item_bounds_;
  mutable std::unordered_map<T, std::uint32_t> uint4_stamp_of_;
  mutable std::uint32_t uint4_query_stamp_ = 0;
};

/// Actor 键特化(OPT-A8:slab 数组形态;语义面与通用模板一致)
/// The Actor-key specialization (OPT-A8: the slab-array form; the semantic
/// face matches the generic template).
template <>
class SpatiallyPartitioned<Actor*> {
 public:
  SpatiallyPartitioned(int width, int height, int bin_size)
      : bin_size_{bin_size},
        rows_{IntegerDivisionRoundingAwayFromZero(height, bin_size)},
        cols_{IntegerDivisionRoundingAwayFromZero(width, bin_size)},
        vec_bins_(static_cast<std::size_t>(rows_) * cols_) {}

  static void ValidateBounds(Actor* item, Rectangle bounds);
  static std::string ToStringOf(Actor* item);

  void Add(Actor* item, Rectangle bounds);
  void Set(Actor* item, Rectangle bounds);
  const Rectangle& Get(Actor* item) const;
  bool Remove(Actor* item);
  std::vector<Actor*> At(int2 location) const;
  std::vector<Actor*> InBox(Rectangle box) const;
  void Clear();
  std::vector<Actor*> Keys() const;
  std::vector<Rectangle> Values() const;
  int Count() const;
  bool ContainsKey(Actor* item) const;
  bool TryGetValue(Actor* item, Rectangle& value_out) const;

 private:
  /// slab 扩容至 id+1(ActorID 单调 —— NextAID) | grows the slabs to id+1
  /// (ActorID is monotonic — NextAID).
  void EnsureSlab(std::uint32_t actor_id) const;
  std::tuple<int, int, int, int> BoundsToBinRowsAndCols(Rectangle bounds) const;
  void MutateBins(std::uint32_t actor_id, Rectangle bounds, bool b_add);
  const std::vector<std::uint32_t>& BinAt(int row, int col) const {
    return vec_bins_[static_cast<std::size_t>(row) * cols_ + col];
  }
  std::vector<std::uint32_t>& BinAt(int row, int col) {
    return vec_bins_[static_cast<std::size_t>(row) * cols_ + col];
  }
  Rectangle BinBounds(int row, int col) const {
    return Rectangle::FromLTRB(col * bin_size_, row * bin_size_,
                               col * bin_size_ + bin_size_,
                               row * bin_size_ + bin_size_);
  }

  int bin_size_;
  int rows_, cols_;
  std::vector<std::vector<std::uint32_t>> vec_bins_;

  // OPT-A8 slab(按 ActorID 直引)| the OPT-A8 slabs (indexed by ActorID).
  mutable std::vector<Rectangle> slab_bounds_;
  mutable std::vector<std::uint8_t> slab_present_;
  mutable std::vector<std::uint32_t> slab_query_stamp_;  // 最后查询标记
                                                         // last-query stamps.
  mutable std::vector<Actor*> actor_by_id_;              // id → Actor 反查
                                                         // the id → Actor map.
  mutable std::uint32_t uint4_query_serial_ = 0;
};

}  // namespace ora::sim
