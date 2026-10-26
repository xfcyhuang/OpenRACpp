// UPSTREAM: OpenRA.Game/Traits/TraitsInterfaces.cs @1f594e320 L571-612(
//          ILobbyOptions 接口 + LobbyOption/LobbyBooleanOption 值面全
//          文;map.GetMessage 的本地化名 = 上游直返 key 的回落面 ——
//          FluentProvider 随 Phase 6,LobbyOptions(map) 的逐 trait 装配
//          面(PlayerResources/MapOptions 等 11 宿主)随其宿主 trait 批,
//          本批以 LobbyCommandsDeps.fn_lobby_options 注入)
//          TraitsInterfaces.cs L571-612 (the ILobbyOptions interface + the
//          whole LobbyOption/LobbyBooleanOption value face; map.GetMessage's
//          localized names = the key-returning fallback face — the
//          FluentProvider rides Phase 6, and the per-trait assembly of
//          LobbyOptions(map) (the eleven hosts PlayerResources/MapOptions/
//          and friends) rides those hosts' own batches, injected here via
//          LobbyCommandsDeps.fn_lobby_options).
#pragma once
import std;

#include "map/map_cache.hpp"


namespace ora::server {

/// LobbyOption(TraitsInterfaces.cs L576-605)的值面(Id/Values/
/// DefaultValue/IsLocked 进服务器消费;Name/Description/IsVisible/
/// DisplayOrder 为 UI 值)
/// The value face of LobbyOption (TraitsInterfaces.cs L576-605) (the
/// server consumes Id/Values/DefaultValue/IsLocked; Name/Description/
/// IsVisible/DisplayOrder are UI values).
struct LobbyOptionData {
  std::string Id;
  std::string Name;
  std::string Description;
  std::vector<std::pair<std::string, std::string>> vec_values;
  std::string DefaultValue;
  bool IsLocked = false;
  bool IsVisible = true;
  int DisplayOrder = 0;
  /// 上游 `o is LobbyBooleanOption` 的接口判别承载
  /// The interface-discrimination carrier of upstream's `o is
  /// LobbyBooleanOption`.
  bool IsBooleanOption = false;

  /// Values.ContainsKey(value)
  bool HasValue(std::string_view str_value) const {
    for (const auto& [k, v] : vec_values)
      if (k == str_value)
        return true;
    return false;
  }

  const std::string* FindValueLabel(std::string_view str_value) const {
    for (const auto& [k, v] : vec_values)
      if (k == str_value)
        return &v;
    return nullptr;
  }

  /// Label(TraitsInterfaces.cs L601-604):Values[value]
  std::string Label(std::string_view str_value) const {
    const std::string* label = FindValueLabel(str_value);
    return label ? *label : std::string{};
  }
};

/// LobbyBooleanOption(TraitsInterfaces.cs L607-620):True/False →
/// Enabled/Disabled 的值表;Label 小写化
/// LobbyBooleanOption (TraitsInterfaces.cs L607-620): the True/False →
/// Enabled/Disabled value table; Label lower-cased.
LobbyOptionData MakeLobbyBooleanOption(std::string_view str_id,
                                       std::string_view str_name,
                                       std::string_view str_description,
                                       bool b_visible, int int4_display_order,
                                       bool b_default_value, bool b_locked);

}  // namespace ora::server
