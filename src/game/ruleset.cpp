// UPSTREAM: OpenRA.Game/GameRules/Ruleset.cs @b6fc03f(ruleset.hpp 的实现;
//          MergeOrDefault/SystemActors 补齐逐语义,见 hpp 头注)
//          Implementation of ruleset.hpp — MergeOrDefault and the
//          SystemActors backfill verbatim; see the hpp header notes.
import std;
#include "game/ruleset.hpp"
#include "game/mod_data.hpp"
#include "yaml/mini_yaml.hpp"

namespace ora::game {

namespace {

/// 键小写化(k.Key.ToLowerInvariant)
/// Lower-case the key (k.Key.ToLowerInvariant).
std::string ToLowerInvariant(std::string_view sv) {
  std::string str_ret{sv};
  std::ranges::transform(str_ret, str_ret.begin(),
                         [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
  return str_ret;
}

/// MergeOrDefault 的公共骨架(Ruleset.cs L98-116):manifest 文件序列加载 +
/// '^' 抽象项过滤 + 小写键字典(重复键保留首个 —— 上游仅 debug 日志)
/// The MergeOrDefault skeleton (Ruleset.cs L98-116): manifest-file-sequence
/// loading + the '^' abstract filter + a lower-cased key dictionary
/// (first-wins duplicates — upstream only logs them in debug).
template <class TValue, class TMake>
std::vector<std::pair<std::string, TValue>> MergeOrDefault(
    yaml::StringPool& pool, fs::FileSystem& fileSystem,
    const std::vector<std::string>& vec_files, bool b_filterAbstract, TMake fn_make) {
  std::vector<std::pair<std::string, TValue>> vec_ret;
  const std::vector<yaml::MiniYamlNode> vec_nodes =
      yaml::MiniYaml::Load(fileSystem, vec_files, nullptr, pool);

  for (const yaml::MiniYamlNode& node : vec_nodes) {
    const std::string_view sv_key =
        node.Key != nullptr ? std::string_view{*node.Key} : std::string_view{};
    if (b_filterAbstract && !sv_key.empty() && sv_key.front() == ActorInfo::kAbstractActorPrefix)
      continue;

    std::string str_key = ToLowerInvariant(sv_key);
    if (std::any_of(vec_ret.begin(), vec_ret.end(),
                    [&](const auto& pair_k) { return pair_k.first == str_key; }))
      continue;  // 重复键保留首个(上游 debug 日志面)/ first wins.
    vec_ret.emplace_back(std::move(str_key), fn_make(node, sv_key));
  }
  return vec_ret;
}

}  // namespace

Ruleset::Ruleset(
    std::vector<std::pair<std::string, std::unique_ptr<ActorInfo>>> vec_actors,
    std::vector<std::pair<std::string, std::unique_ptr<WeaponInfo>>> vec_weapons,
    std::vector<std::pair<std::string, std::unique_ptr<SoundInfo>>> vec_voices,
    std::vector<std::pair<std::string, std::unique_ptr<SoundInfo>>> vec_notifications,
    std::vector<std::pair<std::string, std::unique_ptr<MusicInfo>>> vec_music,
    std::vector<std::pair<std::string, yaml::MiniYamlNode>> vec_modelSequences)
    : vec_actors_{std::move(vec_actors)},
      vec_weapons_{std::move(vec_weapons)},
      vec_voices_{std::move(vec_voices)},
      vec_notifications_{std::move(vec_notifications)},
      vec_music_{std::move(vec_music)},
      vec_modelSequences_{std::move(vec_modelSequences)} {
  // ActorInfoDictionary 构造器(L28-35):SystemActors 缺失项补空 ActorInfo
  // The ActorInfoDictionary ctor (L28-35): backfill missing SystemActors
  // with empty ActorInfos.
  for (const std::string_view sv_system : kSystemActors) {
    const bool b_present = std::any_of(
        vec_actors_.begin(), vec_actors_.end(),
        [&](const auto& pair_a) { return pair_a.first == sv_system; });
    if (!b_present)
      vec_actors_.emplace_back(std::string{sv_system},
                               std::make_unique<ActorInfo>(std::string{sv_system}));
  }

  // IRulesetLoaded 回调(Ruleset.cs L50-93):见 ruleset.hpp 偏离注记
  // The IRulesetLoaded callbacks (Ruleset.cs L50-93): see the deviation note
  // in ruleset.hpp.
}

const ActorInfo* Ruleset::FindActor(std::string_view str_key) const {
  for (const auto& [str_name, rec_actor] : vec_actors_)
    if (str_name == str_key)
      return rec_actor.get();
  return nullptr;
}

const WeaponInfo* Ruleset::FindWeapon(std::string_view str_key) const {
  for (const auto& [str_name, rec_weapon] : vec_weapons_)
    if (str_name == str_key)
      return rec_weapon.get();
  return nullptr;
}

std::unique_ptr<Ruleset> Ruleset::LoadDefaults(ModData& mod_data) {
  const Manifest& manifest = mod_data.ManifestRef();
  fs::FileSystem& fileSystem = mod_data.ModFiles();
  yaml::StringPool pool_rules;

  auto vec_actors = MergeOrDefault<std::unique_ptr<ActorInfo>>(
      pool_rules, fileSystem, manifest.Rules(), true,
      [](const yaml::MiniYamlNode& node, std::string_view sv_key) {
        // 小写键同时是 ActorInfo.name(上游 k.Key.ToLowerInvariant 二用)
        // The lower-cased key doubles as ActorInfo.name (upstream feeds
        // k.Key.ToLowerInvariant to both).
        return std::make_unique<ActorInfo>(ToLowerInvariant(sv_key), node.Value);
      });
  auto vec_weapons = MergeOrDefault<std::unique_ptr<WeaponInfo>>(
      pool_rules, fileSystem, manifest.Weapons(), false,
      [](const yaml::MiniYamlNode& node, std::string_view) {
        return std::make_unique<WeaponInfo>(node.Value);
      });
  auto vec_voices = MergeOrDefault<std::unique_ptr<SoundInfo>>(
      pool_rules, fileSystem, manifest.Voices(), false,
      [](const yaml::MiniYamlNode& node, std::string_view) {
        return std::make_unique<SoundInfo>(node.Value);
      });
  auto vec_notifications = MergeOrDefault<std::unique_ptr<SoundInfo>>(
      pool_rules, fileSystem, manifest.Notifications(), false,
      [](const yaml::MiniYamlNode& node, std::string_view) {
        return std::make_unique<SoundInfo>(node.Value);
      });
  auto vec_music = MergeOrDefault<std::unique_ptr<MusicInfo>>(
      pool_rules, fileSystem, manifest.Music(), false,
      [](const yaml::MiniYamlNode& node, std::string_view sv_key) {
        return std::make_unique<MusicInfo>(std::string{sv_key}, node.Value);
      });
  auto vec_modelSequences = MergeOrDefault<yaml::MiniYamlNode>(
      pool_rules, fileSystem, manifest.ModelSequences(), false,
      [](const yaml::MiniYamlNode& node, std::string_view) { return node; });

  return std::make_unique<Ruleset>(
      std::move(vec_actors), std::move(vec_weapons), std::move(vec_voices),
      std::move(vec_notifications), std::move(vec_music),
      std::move(vec_modelSequences));
}

}  // namespace ora::game
