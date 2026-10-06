// UPSTREAM: OpenRA.Game/Map/PlayerReference.cs @b6fc03f L15-65(逐语义重写)+
//          OpenRA.Game/Map/MapPlayers.cs L12-82(全量;ToMiniYaml 属编辑面
//          随编辑器批) | Verbatim-semantics rewrite; MapPlayers.ToMiniYaml
//          is an editor-face item landing with the editor batch.
//
// 机制对照 / Mechanism mapping:
//  - PlayerReference 字段加载(FieldLoader.Load)→ 手写逐键(全默认;
//    Color 默认 = Game.ModData.GetOrCreate<DefaultPlayer>().Color —— C++
//    侧经注入的默认色回调承载,缺省红警默认色 HSL(145±)…… 实际为
//    DefaultPlayer 的 Color 字段;本批以注入面缺省中性色 + 登记偏离)
//    The PlayerReference field loading (FieldLoader.Load) is hand-written
//    per key (all defaulted); the Color default (Game.ModData's
//    DefaultPlayer.Color) rides an injected default-color callback.
#pragma once
import std;

#include "core/color.hpp"
#include "map/cell_region.hpp"
#include "yaml/mini_yaml.hpp"

namespace ora::map {

/// PlayerReference(PlayerReference.cs L15)
/// PlayerReference (PlayerReference.cs L15).
struct PlayerReference {
  std::string Name{};                  // L17
  std::string Palette{};               // L18
  std::string Bot{};                   // L19(null 承载为空串 | null as "")
  std::string StartingUnitsClass{};    // L20(null 同上 | ditto)
  bool AllowBots{true};                // L21
  bool Playable{false};                // L22
  bool Required{false};                // L23
  bool OwnsWorld{false};               // L24
  bool Spectating{false};              // L25
  bool NonCombatant{false};            // L26

  bool LockFaction{false};             // L28
  std::string Faction{};               // L29

  bool LockColor{false};               // L31
  core::Color Color{core::Color::FromArgbRaw(0xFF4B4B4B)};  // L32(DefaultPlayer
                                                            // 默认;注入面覆盖)
                                                            // (the DefaultPlayer
                                                            // default; the
                                                            // injection face
                                                            // overrides)

  CPos HomeLocation{CPos::Zero()};     // L39
  bool LockSpawn{false};               // L41
  int Spawn{0};                        // L47
  bool LockTeam{false};                // L49
  int Team{0};                         // L50
  bool LockHandicap{false};            // L52
  int Handicap{0};                     // L53

  std::vector<std::string> Allies{};   // L55
  std::vector<std::string> Enemies{};  // L56

  PlayerReference() = default;
  explicit PlayerReference(const yaml::MiniYaml& my,
                           core::Color color_default_fallback);

  std::string ToString() const { return Name; }  // L61
};

/// MapPlayers(MapPlayers.cs L12)
/// MapPlayers (MapPlayers.cs L12).
class MapPlayers final {
 public:
  static constexpr int kMaximumPlayerCount = 63;  // L17

  std::vector<std::pair<std::string, PlayerReference>> Players{};  // 插入序字典
                                                                   // insertion-ordered

  MapPlayers() = default;

  /// MapPlayers(playerDefinitions)(L23-27):键取 PlayerReference.Name
  /// (L25 的 ToDictionary(player => player.Name))
  /// MapPlayers (playerDefinitions) (L23-27): keyed by PlayerReference.Name.
  MapPlayers(std::span<const yaml::MiniYamlNode> player_definitions,
             core::Color color_default_fallback);

  /// PlayerReference 查找(字典语义;缺失 null) | the lookup (a missing
  /// key yields null).
  const PlayerReference* Find(std::string_view name) const;

  /// ToMiniYaml(L339-367)属编辑/存档面 —— 随编辑器批落地
  /// ToMiniYaml (L339-367) belongs to the editor/save faces — lands with
  /// the editor batch.
};

}  // namespace ora::map
