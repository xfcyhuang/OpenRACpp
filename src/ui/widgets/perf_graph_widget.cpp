import std;

#include "ui/widgets/perf_graph_widget.hpp"

#include "core/vector_n.hpp"
#include "game/perf_history.hpp"
#include "gfx/renderer.hpp"
#include "gfx/sprite_font.hpp"
#include "gfx/sprite_renderer.hpp"
#include "ui/ui.hpp"

namespace ora::ui {

PerfGraphWidget::PerfGraphWidget() {
  gfx::Renderer* ptr_renderer = Ui::Instance().RendererDeps();
  if (ptr_renderer != nullptr)
    ptr_font_ = ptr_renderer->TryGetFont("Tiny");
  if (ptr_font_ == nullptr)
    ptr_font_ = Ui::Instance().TryGetFontDeps("Tiny");
  if (ptr_font_ == nullptr)
    return;

  int4_dot_width = GetTextWidth(".");
  int4_next_dot_advance = GetTextWidth("..") - int4_dot_width;
}

int PerfGraphWidget::GetTextWidth(const std::string& str_text) {
  const auto it_found = map_text_width_cache_.find(str_text);
  if (it_found != map_text_width_cache_.end())
    return it_found->second;
  const int int4_width = ptr_font_->Measure(str_text).X;
  map_text_width_cache_.emplace(str_text, int4_width);
  return int4_width;
}

void PerfGraphWidget::SixCharacterFormatFloat(std::string& str_output,
                                              double fp8_value) {
  if (std::isnan(fp8_value)) {
    str_output += "NaN   ";
    return;
  }

  if (std::isinf(fp8_value)) {
    str_output += fp8_value > 0 ? "+Inf  " : "-Inf  ";
    return;
  }

  if (fp8_value < 0) {
    str_output += "<0    ";
    return;
  }

  long long int8_value = static_cast<long long>(fp8_value);

  int int4_front_place_multiplier = 1;
  while (int4_front_place_multiplier <= int8_value && int4_front_place_multiplier < 100000)
    int4_front_place_multiplier *= 10;
  fp8_value += 0.000005 * int4_front_place_multiplier;
  int8_value = static_cast<long long>(fp8_value);

  if (int8_value >= 1000000) {
    str_output += "TooBig";
    return;
  }

  long long int8_part_value = static_cast<long long>(fp8_value);
  long long int8_digit = int8_part_value / 1000000;
  if (int8_value >= 1000000)
    str_output += static_cast<char>(int8_digit + '0');

  for (long long int8_p = 1000000; int8_p >= 10;) {
    const long long int8_n = int8_p / 10;

    if (int8_value >= int8_n) {
      int8_part_value -= int8_digit * int8_p;
      int8_digit = int8_part_value / int8_n;
      str_output += static_cast<char>(int8_digit + '0');
    }

    int8_p = int8_n;
  }

  if (int8_value >= 1000000)
    return;

  if (int8_value < 100000)
    str_output += '.';

  if (int8_value >= 10000)
    return;

  long long int8_fraction_value =
      static_cast<long long>(fp8_value * 100000) - 100000 * int8_value;
  for (long long int8_p = 10000;;) {
    int8_digit = int8_fraction_value / int8_p;
    str_output += static_cast<char>(int8_digit + '0');
    const long long int8_n = int8_p / 10;

    if (int8_value >= int8_n)
      return;

    int8_fraction_value -= int8_digit * int8_p;
    int8_p = int8_n;
  }
}

void PerfGraphWidget::Draw() {
  if (ptr_font_ == nullptr)
    return;

  gfx::Renderer* ptr_renderer = Ui::Instance().RendererDeps();
  if (ptr_renderer == nullptr)
    return;
  gfx::RgbaColorRenderer& cr = ptr_renderer->UIRgbaColorRenderer();
  const Rectangle rect = RenderBounds();
  const core::Vector2 vec_origin{static_cast<float>(rect.Right()),
                                 static_cast<float>(rect.Bottom())};
  const core::Vector2 vec_basis{
      -static_cast<float>(rect.Width / 100),
      -static_cast<float>(rect.Height / 100)};
  const core::Color color_white = core::Color::FromArgb(255, 255, 255, 255);

  const core::Vector3 vec_points_frame[3] = {
      core::Vector3{static_cast<float>(rect.Left()), static_cast<float>(rect.Top()), 0.0f},
      core::Vector3{static_cast<float>(rect.Left()), static_cast<float>(rect.Bottom()),
                    0.0f},
      core::Vector3{static_cast<float>(rect.Right()), static_cast<float>(rect.Bottom()),
                    0.0f}};
  cr.DrawLine(vec_points_frame, 1.0f, color_white);

  cr.DrawLine(core::Vector2{vec_origin.X + 100.0f * vec_basis.X, vec_origin.Y},
              core::Vector2{vec_origin.X + 100.0f * vec_basis.X,
                            vec_origin.Y + 100.0f * vec_basis.Y},
              1.0f, color_white);

  int int4_k = 0;
  std::vector<game::PerfItem*> vec_items = game::PerfHistory::Instance().Items();
  for (const game::PerfItem* ptr_item : vec_items) {
    std::vector<double> vec_samples = ptr_item->Samples();
    std::vector<core::Vector3> vec_line;
    vec_line.reserve(vec_samples.size());
    for (std::size_t sz_i = 0; sz_i < vec_samples.size(); ++sz_i)
      vec_line.push_back(core::Vector3{
          vec_origin.X + static_cast<float>(sz_i) * vec_basis.X,
          vec_origin.Y + static_cast<float>(vec_samples[sz_i]) * vec_basis.Y, 0.0f});
    cr.DrawLine(vec_line, 1.0f, ptr_item->C());

    const core::Vector2 vec_u{static_cast<float>(rect.Left()),
                              static_cast<float>(rect.Top())};

    cr.DrawLine(core::Vector2{vec_u.X + 10, vec_u.Y + 10 * int4_k + 5},
                core::Vector2{vec_u.X + 12, vec_u.Y + 10 * int4_k + 5}, 1.0f,
                ptr_item->C());
    cr.DrawLine(core::Vector2{vec_u.X + 10, vec_u.Y + 10 * int4_k + 4},
                core::Vector2{vec_u.X + 12, vec_u.Y + 10 * int4_k + 4}, 1.0f,
                ptr_item->C());

    ++int4_k;
  }

  int4_k = 0;
  int int4_max_extra_dot_space = 0;
  int int4_max_name_length = 0;
  for (const game::PerfItem* ptr_item : vec_items) {
    int4_max_extra_dot_space =
        std::max(int4_max_extra_dot_space, GetTextWidth(ptr_item->Name()));
    int4_max_name_length =
        std::max(int4_max_name_length, static_cast<int>(ptr_item->Name().size()));
  }
  (void)int4_max_name_length;

  if (str_builder_.capacity() < static_cast<std::size_t>(
                                    std::max(20, int4_max_extra_dot_space + 3)))
    str_builder_.reserve(
        static_cast<std::size_t>(std::max(20, int4_max_extra_dot_space + 3)));
  int4_max_extra_dot_space = std::max(0, int4_max_extra_dot_space - int4_dot_width);
  const int int4_column_two_start =
      int4_max_extra_dot_space + 3 * int4_next_dot_advance + 2;

  const int int4_samples = Ui::Instance().DebugSamples();
  for (const game::PerfItem* ptr_item : vec_items) {
    const int int4_name_width = GetTextWidth(ptr_item->Name());
    const int int4_dots_needed =
        (int4_max_extra_dot_space - int4_name_width) / int4_next_dot_advance + 3;
    str_builder_ += ptr_item->Name();
    str_builder_.append(static_cast<std::size_t>(int4_dots_needed), '.');
    ptr_font_->DrawText(str_builder_,
                        core::Vector2{static_cast<float>(rect.Left() + 18),
                                      static_cast<float>(rect.Top() + 10 * int4_k - 3)},
                        color_white);
    str_builder_.clear();
    SixCharacterFormatFloat(str_builder_, ptr_item->Average(int4_samples));
    str_builder_ += " : ";
    SixCharacterFormatFloat(str_builder_, ptr_item->ActiveGameTotalAverage());
    ptr_font_->DrawText(str_builder_,
                        core::Vector2{
                            static_cast<float>(rect.Left() + 18 + int4_column_two_start),
                            static_cast<float>(rect.Top() + 10 * int4_k - 3)},
                        color_white);
    str_builder_.clear();
    ++int4_k;
  }
}

}  // namespace ora::ui
