// UPSTREAM: OpenRA.Game/GameRules/Ruleset.cs @7d57605 L23-281(逐语义重写)+
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

namespace ora::game {

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
          std::vector<std::pair<std::string, yaml::MiniYamlNode>> vec_modelSequences);

  /// ActorInfoDictionary 语义:按名查找(未含 SystemActors 补齐项)
  /// ActorInfoDictionary semantics: lookup by name (SystemActors backfill
  /// included).
  const ActorInfo* FindActor(std::string_view str_key) const;
  const WeaponInfo* FindWeapon(std::string_view str_key) const;

  // 插入序枚举(dump/对拍遍历序)/ insertion-order enumeration (the
  // dump/comparison traversal order).
  const std::vector<std::pair<std::string, std::unique_ptr<ActorInfo>>>& Actors() const {
    return vec_actors_;
  }
  const std::vector<std::pair<std::string, std::unique_ptr<WeaponInfo>>>& Weapons() const {
    return vec_weapons_;
  }
  const std::vector<std::pair<std::string, std::unique_ptr<SoundInfo>>>& Voices() const {
    return vec_voices_;
  }
  const std::vector<std::pair<std::string, std::unique_ptr<SoundInfo>>>& Notifications() const {
    return vec_notifications_;
  }
  const std::vector<std::pair<std::string, std::unique_ptr<MusicInfo>>>& Music() const {
    return vec_music_;
  }
  const std::vector<std::pair<std::string, yaml::MiniYamlNode>>& ModelSequences() const {
    return vec_modelSequences_;
  }

  /// LoadDefaults(L118-164):manifest 规则文件加载 + '^' 抽象 actor 过滤
  /// LoadDefaults (L118-164): manifest rule-file loading with the '^'
  /// abstract-actor filter.
  static std::unique_ptr<Ruleset> LoadDefaults(class ModData& mod_data);

 private:
  std::vector<std::pair<std::string, std::unique_ptr<ActorInfo>>> vec_actors_;
  std::vector<std::pair<std::string, std::unique_ptr<WeaponInfo>>> vec_weapons_;
  std::vector<std::pair<std::string, std::unique_ptr<SoundInfo>>> vec_voices_;
  std::vector<std::pair<std::string, std::unique_ptr<SoundInfo>>> vec_notifications_;
  std::vector<std::pair<std::string, std::unique_ptr<MusicInfo>>> vec_music_;
  std::vector<std::pair<std::string, yaml::MiniYamlNode>> vec_modelSequences_;
};

}  // namespace ora::game
