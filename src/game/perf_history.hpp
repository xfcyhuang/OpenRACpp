#pragma once
import std;

#include "core/color.hpp"

namespace ora::game {

class PerfItem {
 public:
  PerfItem(std::string str_name_new, core::Color color_c_new)
      : str_name(std::move(str_name_new)), color_c(color_c_new) {}

  const std::string& Name() const { return str_name; }
  const core::Color& C() const { return color_c; }

  void Tick(bool b_game_is_active = true);

  std::vector<double> Samples() const;

  double Average(int int4_count) const;

  double ActiveGameTotalAverage() const {
    return fp8_active_game_total / static_cast<double>(int4_active_game_total_samples);
  }

  int ActiveGameTotalSamplesCount() const { return int4_active_game_total_samples; }

  double LastValue() const;

  void ResetSamples();

  double fp8_val = 0.0;
  bool b_has_normal_tick = true;

 private:
  static constexpr std::size_t sz_samples_count = 100;

  std::string str_name;
  core::Color color_c;
  std::array<double, sz_samples_count> arr_samples{};
  std::size_t sz_head = 1;
  std::size_t sz_tail = 0;
  double fp8_active_game_total = 0.0;
  int int4_active_game_total_samples = 0;
};

class PerfHistory {
 public:
  static PerfHistory& Instance();

  std::vector<PerfItem*> Items();
  PerfItem& Item(const std::string& str_name);

  void Increment(const std::string& str_name, double fp8_x);

  void Tick(bool b_game_is_active = true);
  void Reset();

 private:
  PerfHistory() = default;

  std::vector<std::unique_ptr<PerfItem>> vec_items_;
  std::map<std::string, PerfItem*> map_lookup_;
  std::size_t sz_next_color = 0;
};

}  // namespace ora::game
