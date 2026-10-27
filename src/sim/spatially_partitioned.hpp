#pragma once
import std;

#include "core/int2.hpp"
#include "core/rectangle.hpp"

namespace ora::sim {

class Actor;

inline std::int32_t IntegerDivisionRoundingAwayFromZero(std::int32_t a,
                                                        std::int32_t b) {
  const std::int32_t quotient = a / b;
  const std::int32_t remainder = a % b;
  if (remainder == 0)
    return quotient;

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

  static void ValidateBounds(T item, Rectangle bounds) {
    if (bounds.Width == 0 || bounds.Height == 0)
      throw std::invalid_argument(
          std::format("Bounds of {} are empty.", ToStringOf(item)));
  }

  void Add(T item, Rectangle bounds) {
    ValidateBounds(item, bounds);
    if (!map_item_bounds_.emplace(item, bounds).second)
      throw std::runtime_error("An item with the same key has already been added");
    MutateBins(item, bounds, true);
  }

  void Set(T item, Rectangle bounds) {
    ValidateBounds(item, bounds);
    const auto it = map_item_bounds_.find(item);
    if (it != map_item_bounds_.end())
      MutateBins(item, it->second, false);
    map_item_bounds_[item] = bounds;
    MutateBins(item, bounds, true);
  }

  const Rectangle& Get(T item) const {
    const auto it = map_item_bounds_.find(item);
    if (it == map_item_bounds_.end())
      throw std::runtime_error("The given key was not present in the dictionary.");
    return it->second;
  }

  bool Remove(T item) {
    const auto it = map_item_bounds_.find(item);
    if (it == map_item_bounds_.end())
      return false;
    MutateBins(item, it->second, false);
    map_item_bounds_.erase(it);
    return true;
  }

  std::vector<T> At(int2 location) const {
    const int col = std::clamp(location.X / bin_size_, 0, cols_ - 1);
    const int row = std::clamp(location.Y / bin_size_, 0, rows_ - 1);
    std::vector<T> out;
    for (const T item : BinAt(row, col))
      if (Get(item).Contains(location))
        out.push_back(item);
    return out;
  }

  std::vector<T> InBox(Rectangle box) const {
    auto [min_row, max_row, min_col, max_col] = BoundsToBinRowsAndCols(box);

    const bool b_track = !(min_row >= max_row || min_col >= max_col);
    ++uint4_query_stamp_;

    std::vector<T> out;
    for (int row = min_row; row < max_row; row++) {
      for (int col = min_col; col < max_col; col++) {
        const Rectangle bin_bounds = BinBounds(row, col);
        for (const T item : BinAt(row, col)) {
          const Rectangle& bounds = Get(item);

          if (!bounds.IntersectsWith(box))
            continue;

          if (b_track && !bin_bounds.Contains(bounds)) {
            if (uint4_stamp_of_.find(item) != uint4_stamp_of_.end() &&
                uint4_stamp_of_[item] == uint4_query_stamp_)
              continue;
            uint4_stamp_of_[item] = uint4_query_stamp_;
          }

          out.push_back(item);
        }
      }
    }

    return out;
  }

  void Clear() {
    map_item_bounds_.clear();
    for (auto& bin : vec_bins_)
      bin.clear();
    uint4_stamp_of_.clear();
  }

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

  mutable std::vector<Rectangle> slab_bounds_;
  mutable std::vector<std::uint8_t> slab_present_;
  mutable std::vector<std::uint32_t> slab_query_stamp_;

  mutable std::vector<Actor*> actor_by_id_;

  mutable std::uint32_t uint4_query_serial_ = 0;
};

}
