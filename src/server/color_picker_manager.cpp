// UPSTREAM: OpenRA.Mods.Common/Traits/World/ColorPickerManager.cs
//          @e180e3225 L66-186(实现体;IsInvalidColor/MakeValid/
//          RandomPresetColor 的 RNG 消耗序逐字保真)
//          The implementation body (L66-186; IsInvalidColor/MakeValid/
//          RandomPresetColor with the RNG consumption order kept verbatim).
#include "server/color_picker_manager.hpp"

namespace ora::server {

namespace {

float Lerp(float fp4_a, float fp4_b, float fp4_t) {
  return fp4_a + fp4_t * (fp4_b - fp4_a);
}

constexpr std::string_view kPlayerColorTerrain = "notification-player-color-terrain";
constexpr std::string_view kPlayerColorPlayer = "notification-player-color-player";
constexpr std::string_view kInvalidPlayerColor = "notification-invalid-player-color";

core::Color MakeValidHsv(const ColorPickerManagerData& data, float fp4_hue,
                         float fp4_sat, float fp4_val, MersenneTwister& random,
                         std::span<const core::Color> vec_terrain,
                         std::span<const core::Color> vec_players,
                         const std::function<void(std::string_view)>&
                             fn_on_error) {
  fp4_sat = std::clamp(fp4_sat, data.arr_hsv_saturation_range[0],
                       data.arr_hsv_saturation_range[1]);
  fp4_val = std::clamp(fp4_val, data.arr_hsv_value_range[0],
                       data.arr_hsv_value_range[1]);

  std::string_view sv_error;
  auto color = core::Color::FromAhsv(fp4_hue, fp4_sat, fp4_val);
  if (ColorIsInvalid(data, color, vec_terrain))
    sv_error = kPlayerColorTerrain;
  else if (ColorIsInvalid(data, color, vec_players))
    sv_error = kPlayerColorPlayer;
  else
    return color;

  for (int int4_i = 2; int4_i < 402; int4_i++) {
    color = core::Color::FromAhsv(
        fp4_hue + (int4_i % 2 == 0 ? -1.0f : 1.0f) * (int4_i / 2) * 0.1f *
                      (0.2f + random.NextFloat()),
        Lerp(data.arr_hsv_saturation_range[0], data.arr_hsv_saturation_range[1],
             random.NextFloat()),
        Lerp(data.arr_hsv_value_range[0], data.arr_hsv_value_range[1],
             random.NextFloat()));

    if (!ColorIsInvalid(data, color, vec_terrain) &&
        !ColorIsInvalid(data, color, vec_players)) {
      if (fn_on_error)
        fn_on_error(sv_error);
      return color;
    }
  }

  if (fn_on_error)
    fn_on_error(kInvalidPlayerColor);
  const float fp4_random_sat =
      Lerp(data.arr_hsv_saturation_range[0], data.arr_hsv_saturation_range[1],
           random.NextFloat());
  const float fp4_random_val =
      Lerp(data.arr_hsv_value_range[0], data.arr_hsv_value_range[1],
           random.NextFloat());
  return core::Color::FromAhsv(random.NextFloat(), fp4_random_sat,
                               fp4_random_val);
}

}  // namespace

bool ColorIsInvalid(const ColorPickerManagerData& data, core::Color color,
                    std::span<const core::Color> vec_candidate_blockers) {
  for (const core::Color candidate : vec_candidate_blockers) {
    const int int4_rmean = (color.R() + candidate.R()) >> 1;
    const int int4_rdelta = color.R() - candidate.R();
    const int int4_gdelta = color.G() - candidate.G();
    const int int4_bdelta = color.B() - candidate.B();

    const int int4_weight_r = ((512 + int4_rmean) * int4_rdelta * int4_rdelta) >> 8;
    const int int4_weight_g = 4 * int4_gdelta * int4_gdelta;
    const int int4_weight_b = ((767 - int4_rmean) * int4_bdelta * int4_bdelta) >> 8;

    if (std::sqrt(static_cast<double>(int4_weight_r + int4_weight_g +
                                     int4_weight_b)) <
        static_cast<double>(data.SimilarityThreshold))
      return true;
  }

  return false;
}

core::Color ColorRandomPresetColor(
    const ColorPickerManagerData& data, MersenneTwister& random,
    std::span<const core::Color> vec_terrain,
    std::span<const core::Color> vec_players) {
  // Shuffle(Util.cs L149-162)的 Fisher-Yates 惰性枚举:逐位洗牌逐位产出
  // Shuffle's (Util.cs L149-162) lazy Fisher-Yates: per-position shuffle and
  // per-position yield.
  auto vec_items = data.vec_preset_colors;
  for (std::size_t sz_i = 0; sz_i + 1 < vec_items.size(); sz_i++) {
    const std::size_t sz_j =
        static_cast<std::size_t>(random.Next(static_cast<int>(vec_items.size()) -
                                             static_cast<int>(sz_i)));
    std::swap(vec_items[sz_i], vec_items[sz_i + sz_j]);
    if (!ColorIsInvalid(data, vec_items[sz_i], vec_terrain) &&
        !ColorIsInvalid(data, vec_items[sz_i], vec_players))
      return vec_items[sz_i];
  }

  if (!vec_items.empty()) {
    const core::Color last = vec_items.back();
    if (!ColorIsInvalid(data, last, vec_terrain) &&
        !ColorIsInvalid(data, last, vec_players))
      return last;
  }

  const float fp4_random_hue = random.NextFloat();
  const float fp4_random_sat =
      Lerp(data.arr_hsv_saturation_range[0], data.arr_hsv_saturation_range[1],
           random.NextFloat());
  const float fp4_random_val =
      Lerp(data.arr_hsv_value_range[0], data.arr_hsv_value_range[1],
           random.NextFloat());
  return MakeValidHsv(data, fp4_random_hue, fp4_random_sat, fp4_random_val,
                      random, vec_terrain, vec_players, nullptr);
}

core::Color ColorMakeValid(
    const ColorPickerManagerData& data, core::Color color,
    MersenneTwister& random, std::span<const core::Color> vec_terrain,
    std::span<const core::Color> vec_players,
    const std::function<void(std::string_view)>& fn_on_error) {
  const auto [int4_a, fp4_h, fp4_s, fp4_v] = color.ToAhsv();
  (void)int4_a;
  return MakeValidHsv(data, fp4_h, fp4_s, fp4_v, random, vec_terrain,
                      vec_players, fn_on_error);
}

}  // namespace ora::server
