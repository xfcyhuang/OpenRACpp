#pragma once
import std;

namespace ora::core {

template <class T, class U>
class CachedTransform {
 public:

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

template <class T, class U>
class PredictedCachedTransform {
 public:
  PredictedCachedTransform() = default;

  explicit PredictedCachedTransform(std::function<U(const T&)> fn_transform)
      : fn_transform_(std::move(fn_transform)) {}

  void Predict(U value_prediction) {
    b_predicted_ = true;
    output_prediction_ = std::move(value_prediction);
  }

  U Update(const T& input) {
    if ((b_predicted_ || b_initialized_) && input == input_last_)
      return b_predicted_ ? output_prediction_ : output_last_;

    b_predicted_ = false;
    b_initialized_ = true;
    input_last_ = input;
    output_last_ = fn_transform_(input);

    return output_last_;
  }

 private:
  std::function<U(const T&)> fn_transform_;
  bool b_initialized_ = false;
  bool b_predicted_ = false;
  T input_last_{};
  U output_last_{};
  U output_prediction_{};
};

}
