// UPSTREAM: OpenRA.Game/GameRules/Ruleset.cs @b6fc03f(ruleset.hpp 的实现;
//          MergeOrDefault/SystemActors 补齐逐语义,见 hpp 头注)
//          Implementation of ruleset.hpp — MergeOrDefault and the
//          SystemActors backfill verbatim; see the hpp header notes.
import std;
#include "game/ruleset.hpp"
#include "game/mod_data.hpp"
#include "meta/field_loader.hpp"
#include "meta/parse.hpp"
#include "meta/type_registry.hpp"
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
    std::vector<std::pair<std::string, yaml::MiniYamlNode>> vec_modelSequences,
    const map::ITerrainInfo* terrain_info)
    : vec_actors_{std::move(vec_actors)},
      vec_weapons_{std::move(vec_weapons)},
      vec_voices_{std::move(vec_voices)},
      vec_notifications_{std::move(vec_notifications)},
      vec_music_{std::move(vec_music)},
      vec_modelSequences_{std::move(vec_modelSequences)},
      terrain_info_{terrain_info} {
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
  const auto& vec = borrowed_actors_ != nullptr ? borrowed_actors_->vec_actors_
                                                : vec_actors_;
  for (const auto& [str_name, rec_actor] : vec)
    if (str_name == str_key)
      return rec_actor.get();
  return nullptr;
}

const WeaponInfo* Ruleset::FindWeapon(std::string_view str_key) const {
  const auto& vec = borrowed_weapons_ != nullptr ? borrowed_weapons_->vec_weapons_
                                                 : vec_weapons_;
  for (const auto& [str_name, rec_weapon] : vec)
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

std::unique_ptr<Ruleset> Ruleset::LoadDefaultsForTileSet(
    ModData& mod_data, std::string_view str_tile_set) {
  // Ruleset.cs L167-173:默认规则(actor/weapon 等全部复用引用)+ 指定
  // tileset 的地形 —— C++ unique_ptr 所有权下以"借用视图"承载:除
  // TerrainInfo 外全部转发到默认规则集(与上游共享引用等价)
  // Ruleset.cs L167-173: the default rules (every table reused by
  // reference) + the named tileset's terrain — under C++ unique_ptr
  // ownership this rides a "borrowing view": everything but TerrainInfo
  // forwards to the default ruleset (the shared-reference equivalent).
  auto rs = std::make_unique<Ruleset>();
  const Ruleset* dr = &mod_data.DefaultRules();
  rs->borrowed_actors_ = dr;
  rs->borrowed_weapons_ = dr;
  rs->borrowed_voices_ = dr;
  rs->borrowed_notifications_ = dr;
  rs->borrowed_music_ = dr;
  rs->borrowed_modelSequences_ = dr;
  rs->terrain_info_ = &mod_data.GetTerrainInfo(str_tile_set);

  return rs;
}

std::unique_ptr<Ruleset> Ruleset::Load(
    ModData& mod_data, const MapFileSystemFace& map_files,
    std::string_view str_tile_set, const yaml::MiniYaml* map_rules,
    const yaml::MiniYaml* map_weapons, const yaml::MiniYaml* map_voices,
    const yaml::MiniYaml* map_notifications, const yaml::MiniYaml* map_music,
    const yaml::MiniYaml* map_model_sequences) {
  // Ruleset.cs L175-226 + MergeOrDefault(L98-116):
  //  - additional == null 且 defaults 非空 → 直接复用默认规则字典(上游引用
  //    共享;C++ 承载 = 按表借用视图)
  //  - additional 非 null → MiniYaml.Load(fileSystem, files, additional)
  //    (additional.Value 的逗号文件列表追加入 files,单池合并),过滤 '^'
  //    抽象项后小写键建表
  //  MergeOrDefault semantics: a null additional reuses the default
  //  dictionary (per-table borrowed views here); a non-null additional goes
  //  through MiniYaml.Load (its Value's comma file list appends to files,
  //  one pool through the merge), the '^' abstracts filtered, keys
  //  lower-cased.
  const Manifest& manifest = mod_data.ManifestRef();
  Ruleset& dr = mod_data.DefaultRules();
  yaml::StringPool pool;

  // MiniYaml.Load(fs, files, additional, pool) 的 map 面等价(manifest 文件
  // 经 map_files 打开 —— 地图内 yaml 可达)
  const auto mini_yaml_load = [&](const std::vector<std::string>& files,
                                  const yaml::MiniYaml* additional) {
    std::vector<std::string> all_files{files.begin(), files.end()};
    if (additional != nullptr && additional->Value != nullptr) {
      // FieldLoader.GetValue<string[]>:逗号分割、Trim、去空段
      for (const std::string_view part : meta::SplitCommaTrimmed(
               std::string_view{*additional->Value})) {
        if (!part.empty())
          all_files.emplace_back(part);
      }
    }

    std::vector<std::vector<yaml::MiniYamlNode>> sources;
    sources.reserve(all_files.size() + (additional != nullptr ? 1 : 0));
    for (const std::string& f : all_files) {
      const std::vector<char> bytes = map_files.fn_open(f);
      sources.push_back(yaml::MiniYaml::FromStream(
          std::string_view{bytes.data(), bytes.size()}, f, true, pool));
    }
    if (additional != nullptr && !additional->Nodes.empty())
      sources.push_back(additional->Nodes);

    return yaml::MiniYaml::Merge(std::move(sources));
  };

  const auto merge_actors = [&](const yaml::MiniYaml* additional) {
    std::vector<std::pair<std::string, std::unique_ptr<ActorInfo>>> out;
    for (const yaml::MiniYamlNode& node :
         mini_yaml_load(manifest.Rules(), additional)) {
      const std::string_view sv_key =
          node.Key != nullptr ? std::string_view{*node.Key} : std::string_view{};
      if (!sv_key.empty() && sv_key.front() == ActorInfo::kAbstractActorPrefix)
        continue;  // filterNode(L110-112)
      std::string key = ToLowerInvariant(sv_key);
      if (std::any_of(out.begin(), out.end(),
                      [&](const auto& p) { return p.first == key; }))
        continue;
      out.emplace_back(std::move(key),
                       std::make_unique<ActorInfo>(ToLowerInvariant(sv_key), node.Value));
    }
    return out;
  };
  const auto merge_weapons = [&](const yaml::MiniYaml* additional) {
    std::vector<std::pair<std::string, std::unique_ptr<WeaponInfo>>> out;
    for (const yaml::MiniYamlNode& node :
         mini_yaml_load(manifest.Weapons(), additional)) {
      const std::string_view sv_key =
          node.Key != nullptr ? std::string_view{*node.Key} : std::string_view{};
      std::string key = ToLowerInvariant(sv_key);
      if (std::any_of(out.begin(), out.end(),
                      [&](const auto& p) { return p.first == key; }))
        continue;
      out.emplace_back(std::move(key), std::make_unique<WeaponInfo>(node.Value));
    }
    return out;
  };
  const auto merge_sounds =
      [&](const std::vector<std::string>& files,
          const yaml::MiniYaml* additional) {
        std::vector<std::pair<std::string, std::unique_ptr<SoundInfo>>> out;
        for (const yaml::MiniYamlNode& node : mini_yaml_load(files, additional)) {
          const std::string_view sv_key =
              node.Key != nullptr ? std::string_view{*node.Key} : std::string_view{};
          std::string key = ToLowerInvariant(sv_key);
          if (std::any_of(out.begin(), out.end(),
                          [&](const auto& p) { return p.first == key; }))
            continue;
          out.emplace_back(std::move(key),
                           std::make_unique<SoundInfo>(node.Value));
        }
        return out;
      };
  const auto merge_music = [&](const yaml::MiniYaml* additional) {
    std::vector<std::pair<std::string, std::unique_ptr<MusicInfo>>> out;
    for (const yaml::MiniYamlNode& node :
         mini_yaml_load(manifest.Music(), additional)) {
      const std::string_view sv_key =
          node.Key != nullptr ? std::string_view{*node.Key} : std::string_view{};
      std::string key = ToLowerInvariant(sv_key);
      if (std::any_of(out.begin(), out.end(),
                      [&](const auto& p) { return p.first == key; }))
        continue;
      out.emplace_back(std::move(key),
                       std::make_unique<MusicInfo>(std::string{sv_key}, node.Value));
    }
    return out;
  };
  const auto merge_model_sequences = [&](const yaml::MiniYaml* additional) {
    std::vector<std::pair<std::string, yaml::MiniYamlNode>> out;
    for (const yaml::MiniYamlNode& node :
         mini_yaml_load(manifest.ModelSequences(), additional)) {
      const std::string_view sv_key =
          node.Key != nullptr ? std::string_view{*node.Key} : std::string_view{};
      std::string key = ToLowerInvariant(sv_key);
      if (std::any_of(out.begin(), out.end(),
                      [&](const auto& p) { return p.first == key; }))
        continue;
      out.emplace_back(std::move(key), node);
    }
    return out;
  };

  auto rs = std::make_unique<Ruleset>();
  // 按表双态:additional 空 → 借用默认表(上游 defaults 引用共享的等价);
  // 非空 → 全量建表
  // Per-table dual state: a null additional borrows the default table (the
  // defaults-reference-sharing equivalent); otherwise a full build.
  if (map_rules != nullptr)
    rs->vec_actors_ = merge_actors(map_rules);
  else
    rs->borrowed_actors_ = &dr;

  if (map_weapons != nullptr)
    rs->vec_weapons_ = merge_weapons(map_weapons);
  else
    rs->borrowed_weapons_ = &dr;

  if (map_voices != nullptr)
    rs->vec_voices_ = merge_sounds(manifest.Voices(), map_voices);
  else
    rs->borrowed_voices_ = &dr;

  if (map_notifications != nullptr)
    rs->vec_notifications_ =
        merge_sounds(manifest.Notifications(), map_notifications);
  else
    rs->borrowed_notifications_ = &dr;

  if (map_music != nullptr)
    rs->vec_music_ = merge_music(map_music);
  else
    rs->borrowed_music_ = &dr;

  if (map_model_sequences != nullptr)
    rs->vec_modelSequences_ = merge_model_sequences(map_model_sequences);
  else
    rs->borrowed_modelSequences_ = &dr;

  rs->terrain_info_ = &mod_data.GetTerrainInfo(str_tile_set);
  return rs;
}

bool Ruleset::AnyCustomYaml(const yaml::MiniYaml* yaml) {
  return yaml != nullptr &&
         (yaml->Value != nullptr || !yaml->Nodes.empty());
}

bool Ruleset::AnyFlaggedTraits(ModData& mod_data,
                               const std::vector<yaml::MiniYamlNode>& vec_actors) {
  (void)mod_data;
  for (const yaml::MiniYamlNode& actor_node : vec_actors) {
    if (actor_node.Value.Nodes.empty())
      continue;
    for (const yaml::MiniYamlNode& trait_node : actor_node.Value.Nodes) {
      std::string_view sv_key =
          trait_node.Key != nullptr ? std::string_view{*trait_node.Key}
                                    : std::string_view{};
      const std::size_t n_at = sv_key.find('@');
      if (n_at != std::string_view::npos)
        sv_key = sv_key.substr(0, n_at);
      const std::string str_trait_name{sv_key};
      const meta::RecordDesc* desc = meta::TypeRegistry::FindType(
          str_trait_name + "Info");
      if (desc != nullptr) {
        bool b_whitelisted = false;
        for (std::string_view str_iface : desc->interfaces)
          if (str_iface == "OpenRA.Traits.ILobbyCustomRulesIgnore") {
            b_whitelisted = true;
            break;
          }
        if (!b_whitelisted)
          return true;
      }
    }
  }
  return false;
}

bool Ruleset::DefinesUnsafeCustomRules(
    ModData& mod_data, const MapFileSystemFace& map_files,
    const yaml::MiniYaml* map_rules, const yaml::MiniYaml* map_weapons,
    const yaml::MiniYaml* map_voices, const yaml::MiniYaml* map_notifications,
    const yaml::MiniYaml* map_sequences) {
  if (AnyCustomYaml(map_weapons) || AnyCustomYaml(map_voices) ||
      AnyCustomYaml(map_notifications) || AnyCustomYaml(map_sequences))
    return true;

  if (map_rules == nullptr)
    return false;

  if (AnyFlaggedTraits(mod_data, map_rules->Nodes))
    return true;

  if (map_rules->Value != nullptr) {
    const std::vector<std::string> vec_map_files =
        meta::GetStringArrayValue("value", *map_rules->Value);
    for (const std::string& str_file : vec_map_files) {
      const std::vector<char> bytes = map_files.fn_open(str_file);
      const std::vector<yaml::MiniYamlNode> vec_nodes = yaml::MiniYaml::FromStream(
          std::string_view{bytes.data(), bytes.size()}, str_file, false,
          yaml::MiniYaml::GlobalPool());
      if (AnyFlaggedTraits(mod_data, vec_nodes))
        return true;
    }
  }

  return false;
}

}  // namespace ora::game
