#pragma once
import std;

namespace ora {

template <class T, class Comparer>
class PriorityQueue final {
 public:
  PriorityQueue() { vec_items_.resize(1); }

  void Add(const T& item) {
    int add_level = level_;
    int add_index = index_;

    while (add_level >= 1) {
      const T& above = vec_items_[static_cast<std::size_t>(
          AboveIndex(add_level, add_index))];
      if (Comparer::Compare(above, item) > 0) {
        vec_items_[static_cast<std::size_t>(Index(add_level, add_index))] =
            above;
        --add_level;
        add_index >>= 1;
      } else {
        break;
      }
    }

    vec_items_[static_cast<std::size_t>(Index(add_level, add_index))] = item;

    if (++index_ >= 1 << level_) {
      index_ = 0;
      const int count = 2 * (1 << ++level_);
      if (count - 1 >= static_cast<int>(vec_items_.size()))
        vec_items_.resize(static_cast<std::size_t>(count));
    }
  }

  bool Empty() const { return level_ == 0; }

  const T& Peek() const {
    if (level_ <= 0 && index_ <= 0)
      throw std::runtime_error("PriorityQueue empty.");
    return vec_items_[static_cast<std::size_t>(Index(0, 0))];
  }

  T Pop() {
    const T ret = Peek();
    BubbleInto(0, 0, vec_items_[static_cast<std::size_t>(IndexLast())]);
    if (--index_ < 0)
      index_ = (1 << --level_) - 1;
    return ret;
  }

 private:
  static constexpr int Index(int level, int index) {
    return (1 << level) - 1 + index;
  }

  static constexpr int AboveIndex(int level, int index) {
    return (1 << (level - 1)) - 1 + (index >> 1);
  }

  int IndexLast() const {
    int last_level = level_;
    int last_index = index_;

    if (--last_index < 0)
      last_index = (1 << --last_level) - 1;

    return Index(last_level, last_index);
  }

  void BubbleInto(int into_level, int into_index, const T& val) {
    while (true) {
      const int down_level = into_level + 1;
      int down_index = into_index << 1;

      if (down_level > level_ || (down_level == level_ && down_index >= index_)) {
        vec_items_[static_cast<std::size_t>(Index(into_level, into_index))] =
            val;
        return;
      }

      T down = vec_items_[static_cast<std::size_t>(Index(down_level, down_index))];
      if (down_level < level_ ||
          (down_level == level_ && down_index < index_ - 1)) {
        const T& down_right =
            vec_items_[static_cast<std::size_t>(Index(down_level, down_index + 1))];
        if (Comparer::Compare(down, down_right) >= 0) {
          down = down_right;
          ++down_index;
        }
      }

      if (Comparer::Compare(val, down) <= 0) {
        vec_items_[static_cast<std::size_t>(Index(into_level, into_index))] =
            val;
        return;
      }

      vec_items_[static_cast<std::size_t>(Index(into_level, into_index))] = down;
      into_level = down_level;
      into_index = down_index;
    }
  }

  std::vector<T> vec_items_;
  int level_ = 0;
  int index_ = 0;
};

}
