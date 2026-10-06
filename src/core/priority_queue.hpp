// UPSTREAM: OpenRA.Game/Primitives/PriorityQueue.cs @b6fc03f(逐语义重写)
//          Verbatim-semantics rewrite.
//
// 机制对照 / Mechanism mapping:
//  - "层级加倍"二叉最小堆原样保留(而非改写为平坦堆):相同优先级条目的
//    弹出次序属于可观测行为(寻路平局路径的选择进入 replay SyncHash,
//    docs/OPTIMIZATION_TRACKER.md OPT-A2/C4 的警示项)—— 对拍优先于
//    局部重写;TComparer 结构比较器 → 模板比较器类型(可内联,同零虚化)
//    The "levels-doubled" binary min-heap is kept verbatim (rather than
//    flattened): the pop order of equal-priority entries is observable
//    (tied path choices enter the replay SyncHash — the OPT-A2/C4 warning
//    in the optimization tracker) — the differential gate outranks a local
//    rewrite; the struct comparer becomes a template comparator type
//    (inlinable, equally devirtualized).
//  - TComparer 的三态 Compare 协议(IComparer<T>.Compare 的 -1/0/1)以
//    静态成员承载(Comparer::Compare(x, y);CostComparer 同形)—— 上游
//    "实例 comparer" 为无状态结构,C++ 静态化即等价
//    The TComparer three-way Compare protocol (IComparer<T>.Compare's
//    -1/0/1) is carried as a static member (Comparer::Compare(x, y);
//    CostComparer shares the shape) — upstream's comparer instance is a
//    stateless struct, so the C++ static form is equivalent.
//  - Add 的扩容语义:Array.Resize → reserve/resize(vector 同倍增界);空堆
//    Peek/Pop 的 InvalidOperationException 同文本
//    Add's growth: Array.Resize → vector reserve/resize (the same doubling
//    bound); empty Peek/Pop keep the InvalidOperationException text.
#pragma once
import std;

namespace ora {

/// PriorityQueue<T, TComparer>(仅消费 Add/Empty/Peek/Pop 面)
/// PriorityQueue<T, TComparer> (only the Add/Empty/Peek/Pop faces are
/// consumed).
template <class T, class Comparer>
class PriorityQueue final {
 public:
  PriorityQueue() { vec_items_.resize(1); }

  /// Add(L60-87) | Add (L60-87).
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

  /// Empty(L89):level == 0 | Empty (L89): level == 0.
  bool Empty() const { return level_ == 0; }

  /// Peek(L106-112):空堆 "PriorityQueue empty." 同文本
  /// Peek (L106-112): the empty-heap throw keeps the text.
  const T& Peek() const {
    if (level_ <= 0 && index_ <= 0)
      throw std::runtime_error("PriorityQueue empty.");
    return vec_items_[static_cast<std::size_t>(Index(0, 0))];
  }

  /// Pop(L114-121) | Pop (L114-121).
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

  /// IndexLast(L95-104) | IndexLast (L95-104).
  int IndexLast() const {
    int last_level = level_;
    int last_index = index_;

    if (--last_index < 0)
      last_index = (1 << --last_level) - 1;

    return Index(last_level, last_index);
  }

  /// BubbleInto(L123-157) | BubbleInto (L123-157).
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

}  // namespace ora
