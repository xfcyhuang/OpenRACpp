// UPSTREAM: OpenRA.Game/Primitives/CachedTransform.cs @e180e3225 L15-41(全文)。
// 输入相等(Equals)则回放缓存输出;T 的空值对判 = monostate 形态由调用方
// 保证(本仓消费面的 T 均为值语义)。C# Func → std::function。
// The whole of CachedTransform.cs L15-41. An equal input (Equals) replays
// the cached output; T's null-pair test arrives as value semantics (every T
// consumed in this repo is a value type). C#'s Func becomes std::function.
#pragma once
import std;

namespace ora::core {

template <class T, class U>
class CachedTransform {
 public:
  /// U 为 CachedTransform 自身的嵌套承载(checkbox 的双层缓存)需要缺省构造
  /// (fn 空 = Update 首访必重建)。
  /// A default constructor for the nested U = CachedTransform carrier
  /// (checkbox's two-layer cache; a null fn means Update's first hit always
  /// rebuilds).
  CachedTransform() = default;

  explicit CachedTransform(std::function<U(const T&)> fn_transform)
      : fn_transform_(std::move(fn_transform)) {}

  U Update(const T& input) {
    if (b_initialized_ && input == input_last_)
      return output_last_;

    input_last_ = input;
    output_last_ = fn_transform_(input);
    b_initialized_ = true;

    return output_last_;
  }

 private:
  std::function<U(const T&)> fn_transform_;
  bool b_initialized_ = false;
  T input_last_{};
  U output_last_{};
};

}  // namespace ora::core
