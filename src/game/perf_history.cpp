#include "game/perf_history.hpp"

namespace ora::game {

namespace {

const core::Color kPerfColors[] = {
    core::Color::FromArgb(255, 255, 0, 0),      // Red
    core::Color::FromArgb(255, 0, 128, 0),      // Green
    core::Color::FromArgb(255, 255, 165, 0),    // Orange
    core::Color::FromArgb(255, 255, 255, 0),    // Yellow
    core::Color::FromArgb(255, 255, 0, 255),    // Fuchsia
    core::Color::FromArgb(255, 0, 255, 0),      // Lime
    core::Color::FromArgb(255, 0, 255, 255),    // Cyan
    core::Color::FromArgb(255, 0, 0, 255),      // Blue
    core::Color::FromArgb(255, 255, 255, 255),  // White
    core::Color::FromArgb(255, 0, 128, 128),    // Teal
    core::Color::FromArgb(255, 255, 192, 203),  // Pink
    core::Color::FromArgb(255, 147, 112, 219),  // MediumPurple
    core::Color::FromArgb(255, 128, 128, 0),    // Olive
    core::Color::FromArgb(255, 100, 149, 237),  // CornflowerBlue
};

}  // namespace

void PerfItem::Tick(bool b_game_is_active) {
  arr_samples[sz_head++] = fp8_val;
  if (b_game_is_active) {
    fp8_active_game_total += fp8_val;
    int4_active_game_total_samples++;
  }

  if (sz_head == arr_samples.size())
    sz_head = 0;
  if (sz_head == sz_tail && ++sz_tail == arr_samples.size())
    sz_tail = 0;
  fp8_val = 0.0;
}

std::vector<double> PerfItem::Samples() const {
  std::vector<double> vec_out;
  std::size_t sz_n = sz_head;
  while (sz_n != sz_tail) {
    if (sz_n == 0)
      sz_n = arr_samples.size();
    --sz_n;
    vec_out.push_back(arr_samples[sz_n]);
  }
  return vec_out;
}

double PerfItem::Average(int int4_count) const {
  int int4_i = 0;
  std::size_t sz_n = sz_head;
  double fp8_sum = 0.0;
  while (int4_i < int4_count && sz_n != sz_tail) {
    if (sz_n == 0)
      sz_n = arr_samples.size();
    --sz_n;
    fp8_sum += arr_samples[sz_n];
    int4_i++;
  }
  return int4_i == 0 ? fp8_sum : fp8_sum / int4_i;
}

double PerfItem::LastValue() const {
  std::size_t sz_n = sz_head;
  if (sz_n == 0)
    sz_n = arr_samples.size();
  --sz_n;
  return arr_samples[sz_n];
}

void PerfItem::ResetSamples() {
  sz_head = 1;
  sz_tail = 0;
  fp8_val = 0.0;
  fp8_active_game_total = 0.0;
  int4_active_game_total_samples = 0;
}

PerfHistory& PerfHistory::Instance() {
  static PerfHistory instance_perf_history;
  return instance_perf_history;
}

std::vector<PerfItem*> PerfHistory::Items() {
  std::vector<PerfItem*> vec_view;
  vec_view.reserve(vec_items_.size());
  for (const auto& up_item : vec_items_)
    vec_view.push_back(up_item.get());
  return vec_view;
}

PerfItem& PerfHistory::Item(const std::string& str_name) {
  const auto it_found = map_lookup_.find(str_name);
  if (it_found != map_lookup_.end())
    return *it_found->second;

  core::Color color_next = kPerfColors[sz_next_color++];
  if (sz_next_color >= std::size(kPerfColors))
    sz_next_color = 0;
  vec_items_.push_back(std::make_unique<PerfItem>(str_name, color_next));
  PerfItem& item_new = *vec_items_.back();
  map_lookup_.emplace(str_name, &item_new);
  return item_new;
}

void PerfHistory::Increment(const std::string& str_name, double fp8_x) {
  Item(str_name).fp8_val += fp8_x;
}

void PerfHistory::Tick(bool b_game_is_active) {
  for (const auto& up_item : vec_items_)
    if (up_item->b_has_normal_tick)
      up_item->Tick(b_game_is_active);
}

void PerfHistory::Reset() {
  for (const auto& up_item : vec_items_)
    up_item->ResetSamples();
}

}  // namespace ora::game
