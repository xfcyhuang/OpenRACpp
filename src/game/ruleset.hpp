// UPSTREAM: OpenRA.Game/GameRules/Ruleset.cs @b6fc03f L23-281(逐语义重写)+
//          OpenRA.Game/Primitives/ActorInfoDictionary.cs L18-55(SystemActors 补齐)
//          Full rewrite of Ruleset.cs + the ActorInfoDictionary SystemActors
//          backfill.
//
// 已登记偏离(docs/COVERAGE.md)/ Registered deviations:
//  - IRulesetLoaded 回调(actor/warhead/projectile 的 RulesetLoaded(this,…))
//    属行为面:生成类型无行为,Phase 5 起随各 trait 落地;对拍不受影响
//    (回调只做跨引用校验与非 yaml 属性缓存,默认规则集全部通过)
//    The IRulesetLoaded callbacks (RulesetLoaded(this,…) of actors/warheads/
//    projectiles) are behaviour: generated types carry none; they land trait
//    by trait from Phase 5. Dumps are unaffected (the callbacks only do
//    cross-reference validation and non-yaml property caching, all of which
//    pass on the default rulesets).
//  - ToDictionaryWithConflictLog 的重复键 debug 日志不落地(保留首个的行为
//    一致;日志属 debug 输出面)
//    The duplicate-key debug logging of ToDictionaryWithConflictLog is not
//    replicated (first-wins behaviour identical; the log is debug output).
//  - TerrainInfo/ITerrainInfo:Phase 4 随地形格式;LoadDefaults 传空
//    TerrainInfo/ITerrainInfo: arrives with the terrain formats in Phase 4;
//    LoadDefaults passes empty.
#pragma once
import std;

#include "game/actor_info.hpp"
#include "game/game_records.hpp"
#include "fs/file_system.hpp"
#include "terrain/terrain_info.hpp"

namespace ora::game {

/// Ruleset.Load 的地图文件系统面(Map.Open/TryOpen/Exists 的等价闭包快照;
/// ora_game 不依赖 ora_map 的分层约束 —— 依赖倒置为三函数面)
/// The map file-system face of Ruleset::Load (closures over Map.Open/...;
/// ora_game must not depend on ora_map — inverted into three function faces).
struct MapFileSystemFace {
  std::function<std::vector<char>(const std::string&)> fn_open;
  std::function<bool(const std::string&, std::vector<char>&)> fn_try_open;
  std::function<bool(const std::string&)> fn_exists;
};

/// Ruleset(Ruleset.cs L23)
class Ruleset final {
 public:
  /// SystemActors(Actor.cs L28-34):SystemActor 枚举名(小写)
  /// SystemActors (Actor.cs L28-34): the enum names, lower-cased.
  static constexpr std::array<std::string_view, 4> kSystemActors{
      "player", "editorplayer", "world", "editorworld"};

  /// Ruleset(...)(L33-94):装入已构造的各表;SystemActors 补空 actor;
  /// IRulesetLoaded 回调见文件头偏离注记
  /// Ruleset(...) (L33-94): takes over the constructed tables; backfills
  /// empty SystemActors; the IRulesetLoaded callbacks — see the deviation
  /// note in the header.
  Ruleset(std::vector<std::pair<std::string, std::unique_ptr<ActorInfo>>> vec_actors,
          std::vector<std::pair<std::string, std::unique_ptr<WeaponInfo>>> vec_weapons,
          std::vector<std::pair<std::string, std::unique_ptr<SoundInfo>>> vec_voices,
          std::vector<std::pair<std::string, std::unique_ptr<SoundInfo>>> vec_notifications,
          std::vector<std::pair<std::string, std::unique_ptr<MusicInfo>>> vec_music,
          std::vector<std::pair<std::string, yaml::MiniYamlNode>> vec_modelSequences,
          const map::ITerrainInfo* terrain_info = nullptr);

  /// ActorInfoDictionary 语义:按名查找(未含 SystemActors 补齐项;
  /// 借用视图态转发默认规则集 —— LoadDefaultsForTileSet 的共享引用等价)
  /// ActorInfoDictionary semantics: lookup by name (SystemActors backfill
  /// included; the borrowed-view state forwards to the default ruleset —
  /// the shared-reference equivalent of LoadDefaultsForTileSet).
  const ActorInfo* FindActor(std::string_view str_key) const;
  const WeaponInfo* FindWeapon(std::string_view str_key) const;

  /// Rules.TerrainInfo(L36 的表成员;非拥有 —— ModData 地形缓存所有)
  /// Rules.TerrainInfo (L36; non-owning — the ModData terrain cache owns it).
  /// 借用视图构造(L167-173 的 LoadDefaultsForTileSet 语义)
  Ruleset() = default;

  const map::ITerrainInfo& TerrainInfo() const { return *terrain_info_; }

  // 插入序枚举(dump/对拍遍历序)/ insertion-order enumeration (the
  // dump/comparison traversal order).
  const std::vector<std::pair<std::string, std::unique_ptr<ActorInfo>>>& Actors() const {
    return borrowed_actors_ != nullptr ? borrowed_actors_->vec_actors_
                                       : vec_actors_;
  }
  const std::vector<std::pair<std::string, std::unique_ptr<WeaponInfo>>>& Weapons() const {
    return borrowed_weapons_ != nullptr ? borrowed_weapons_->vec_weapons_
                                        : vec_weapons_;
  }
  const std::vector<std::pair<std::string, std::unique_ptr<SoundInfo>>>& Voices() const {
    return borrowed_voices_ != nullptr ? borrowed_voices_->vec_voices_
                                       : vec_voices_;
  }
  const std::vector<std::pair<std::string, std::unique_ptr<SoundInfo>>>& Notifications() const {
    return borrowed_notifications_ != nullptr
               ? borrowed_notifications_->vec_notifications_
               : vec_notifications_;
  }
  const std::vector<std::pair<std::string, std::unique_ptr<MusicInfo>>>& Music() const {
    return borrowed_music_ != nullptr ? borrowed_music_->vec_music_
                                      : vec_music_;
  }
  const std::vector<std::pair<std::string, yaml::MiniYamlNode>>& ModelSequences() const {
    return borrowed_modelSequences_ != nullptr
               ? borrowed_modelSequences_->vec_modelSequences_
               : vec_modelSequences_;
  }

  /// LoadDefaults(L118-164):manifest 规则文件加载 + '^' 抽象 actor 过滤
  /// LoadDefaults (L118-164): manifest rule-file loading with the '^'
  /// abstract-actor filter.
  static std::unique_ptr<Ruleset> LoadDefaults(class ModData& mod_data);

  /// Load(L175-278):manifest 文件 + 地图附加节点合并(文件经 map 面打开,
  /// 地图内 yaml 可达);terrainInfo = modData.DefaultTerrainInfo[tileSet]
  /// Load (L175-278): manifest files + the map's additional nodes merged
  /// (files open through the map face, so map-contained yaml resolves);
  /// terrainInfo = modData.DefaultTerrainInfo[tileSet].
  static std::unique_ptr<Ruleset> Load(
      ModData& mod_data, const MapFileSystemFace& map_files,
      std::string_view str_tile_set, const yaml::MiniYaml* map_rules,
      const yaml::MiniYaml* map_weapons, const yaml::MiniYaml* map_voices,
      const yaml::MiniYaml* map_notifications, const yaml::MiniYaml* map_music,
      const yaml::MiniYaml* map_model_sequences);

  /// LoadDefaultsForTileSet(L167-173):默认规则 + 指定 tileset 的地形
  /// LoadDefaultsForTileSet (L167-173): the default rules plus the named
  /// tileset's terrain.
  static std::unique_ptr<Ruleset> LoadDefaultsForTileSet(
      ModData& mod_data, std::string_view str_tile_set);

 private:
  std::vector<std::pair<std::string, std::unique_ptr<ActorInfo>>> vec_actors_;
  std::vector<std::pair<std::string, std::unique_ptr<WeaponInfo>>> vec_weapons_;
  std::vector<std::pair<std::string, std::unique_ptr<SoundInfo>>> vec_voices_;
  std::vector<std::pair<std::string, std::unique_ptr<SoundInfo>>> vec_notifications_;
  std::vector<std::pair<std::string, std::unique_ptr<MusicInfo>>> vec_music_;
  std::vector<std::pair<std::string, yaml::MiniYamlNode>> vec_modelSequences_;
  const map::ITerrainInfo* terrain_info_ = nullptr;
  // 按表借用视图(MergeOrDefault L109-110 的 defaults 引用共享等价;
  // LoadDefaultsForTileSet 全表借用,Load 按各 additional 双态)
  // Per-table borrowed views (the defaults-reference sharing of
  // MergeOrDefault L109-110; LoadDefaultsForTileSet borrows every table,
  // Load picks per additional).
  const Ruleset* borrowed_actors_ = nullptr;
  const Ruleset* borrowed_weapons_ = nullptr;
  const Ruleset* borrowed_voices_ = nullptr;
  const Ruleset* borrowed_notifications_ = nullptr;
  const Ruleset* borrowed_music_ = nullptr;
  const Ruleset* borrowed_modelSequences_ = nullptr;
};

}  // namespace ora::game
