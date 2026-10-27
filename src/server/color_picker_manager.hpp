#pragma once
import std;

#include "core/color.hpp"
#include "core/mersenne_twister.hpp"

namespace ora::server {

struct ColorPickerManagerData {
  std::array<float, 2> arr_hsv_saturation_range{0.3f, 0.95f};
  std::array<float, 2> arr_hsv_value_range{0.3f, 0.95f};
  int SimilarityThreshold = 0x50;
  std::vector<core::Color> vec_preset_colors;
};

bool ColorIsInvalid(const ColorPickerManagerData& data,
                    core::Color color,
                    std::span<const core::Color> vec_candidate_blockers);

core::Color ColorRandomPresetColor(const ColorPickerManagerData& data,
                                   MersenneTwister& random,
                                   std::span<const core::Color> vec_terrain,
                                   std::span<const core::Color> vec_players);

core::Color ColorMakeValid(const ColorPickerManagerData& data,
                           core::Color color, MersenneTwister& random,
                           std::span<const core::Color> vec_terrain,
                           std::span<const core::Color> vec_players,
                           const std::function<void(std::string_view)>&
                               fn_on_error);

core::Color ColorRandomValidColor(const ColorPickerManagerData& data,
                                  MersenneTwister& random,
                                  std::span<const core::Color> vec_terrain,
                                  std::span<const core::Color> vec_players);

}
