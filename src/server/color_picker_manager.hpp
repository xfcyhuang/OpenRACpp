// UPSTREAM: OpenRA.Mods.Common/Traits/World/ColorPickerManager.cs
//          @e180e3225 L28-186(服务器消费面全文:IsInvalidColor 的 compuphase
//          感知度量 / MakeValid 的双色域钳制 + 双向扩幅搜索 + 400 次迭代
//          上限 / RandomPresetColor 的 Fisher-Yates 洗牌遍历与随机回落;
//          PresetColors 表 = 规则装载注入;ShowColorDropDown 的 UI 下拉
//          面随 Phase 6;注册为 trait Info 的 World 构造面随 Phase 6 装配
//          批 —— 本批承载值数据 + 三个纯函数)
//          ColorPickerManager.cs L28-186 (the whole server-consumed face:
//          IsInvalidColor's compuphase perceptual metric / MakeValid's
//          dual-range clamping + the bidirectional expanding search with the
//          400-iteration cap / RandomPresetColor's Fisher-Yates-shuffled
//          walk and the random fallback; the PresetColors table = a
//          rules-loading injection; ShowColorDropDown's UI face rides
//          Phase 6, as does the trait-Info world-construction face — this
//          batch carries the value data + the three pure functions).
#pragma once
import std;

#include "core/color.hpp"
#include "core/mersenne_twister.hpp"


namespace ora::server {

/// ColorPickerManagerInfo 的可注入值数据(Info 字段的服务面子集)
/// The injectable value data of ColorPickerManagerInfo (the
/// server-consumed subset of the Info fields).
struct ColorPickerManagerData {
  std::array<float, 2> arr_hsv_saturation_range{0.3f, 0.95f};  // L40-42
  std::array<float, 2> arr_hsv_value_range{0.3f, 0.95f};        // L45-47
  int SimilarityThreshold = 0x50;                               // L50
  std::vector<core::Color> vec_preset_colors;                  // L56
};

/// IsInvalidColor(L66-93):compuphase 感知距离 < 阈值 = 撞色
/// IsInvalidColor (L66-93): the compuphase perceptual distance below the
/// threshold = a clash.
bool ColorIsInvalid(const ColorPickerManagerData& data,
                    core::Color color,
                    std::span<const core::Color> vec_candidate_blockers);

/// RandomPresetColor(L117-133):洗牌遍历首个合法预设;尽 = 随机 HSV 的
/// MakeValid 回落
/// RandomPresetColor (L117-133): the first legal preset in the shuffled
/// walk; exhaustion = the random-HSV MakeValid fallback.
core::Color ColorRandomPresetColor(const ColorPickerManagerData& data,
                                   MersenneTwister& random,
                                   std::span<const core::Color> vec_terrain,
                                   std::span<const core::Color> vec_players);

/// MakeValid(L135-141):ToAhsv → 私有 MakeValid(h,s,v)
/// MakeValid (L135-141): ToAhsv → the private MakeValid(h,s,v).
core::Color ColorMakeValid(const ColorPickerManagerData& data,
                           core::Color color, MersenneTwister& random,
                           std::span<const core::Color> vec_terrain,
                           std::span<const core::Color> vec_players,
                           const std::function<void(std::string_view)>&
                               fn_on_error);

/// RandomValidColor(L168-177):三次 NextFloat 后入私有 MakeValid
/// RandomValidColor (L168-177): three NextFloat draws into the private
/// MakeValid.
core::Color ColorRandomValidColor(const ColorPickerManagerData& data,
                                  MersenneTwister& random,
                                  std::span<const core::Color> vec_terrain,
                                  std::span<const core::Color> vec_players);

}  // namespace ora::server
