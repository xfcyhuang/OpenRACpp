// UPSTREAM: OpenRA.Game/Map/MapPreview.cs @b6fc03f + OpenRA.Game/Map/
//          MapCache.cs @b6fc03f(map_cache.hpp 的实现) | The implementation of
//          map_cache.hpp.
import std;

#include "map/map_cache.hpp"

#include "game/manifest.hpp"
#include "game/mod_data.hpp"
#include "meta/field_loader.hpp"
#include "terrain/map_grid.hpp"

namespace ora::map {


MapPreview::MapPreview(game::ModData* mod_data, std::string str_uid,
                       std::optional<MapGridType> grid_type)
    : ptr_mod_data_{mod_data}, str_uid_{std::move(str_uid)},
      opt_grid_type_{grid_type} {
  // L340-349 的 Unknown 初态 | the Unknown initial state (L340-349).
  kind_status_ = MapStatus::Unavailable;
  kind_class_ = MapClassification::Unknown;
  kind_visibility_ = MapVisibility::Lobby;
  rect_bounds_ = Rectangle::FromLTRB(0, 0, 0, 0);
}

void MapPreview::UpdateFromMapWithoutOwningPackage(
    const fs::IReadOnlyPackage& p, const fs::IReadOnlyPackage& parent,
    MapClassification classification, std::optional<MapGridType> grid_type) {
  // L352-358
  UpdateFromMap(p, classification, grid_type);
  ptr_parent_package_ = &parent;
  ptr_owned_package_.reset();
}

void MapPreview::UpdateFromMap(const fs::IReadOnlyPackage& p,
                               MapClassification classification,
                               std::optional<MapGridType> grid_type) {
  // L364-470
  str_path_ = p.Name();

  auto yaml_stream = p.GetStream("map.yaml");
  if (!yaml_stream.has_value())
    throw std::runtime_error("Required file map.yaml not present in this map");

  yaml::StringPool& pool = yaml::MiniYaml::GlobalPool();
  const std::vector<yaml::MiniYamlNode> nodes = yaml::MiniYaml::FromStream(
      std::string_view{yaml_stream->data(), yaml_stream->size()},
      std::format("{}:map.yaml", p.Name()), true, pool);
  yaml::MiniYaml root;
  root.Nodes = nodes;
  const auto yaml = root.ToDictionary();

  kind_class_ = classification;
  if (grid_type.has_value())
    grid_type_ = *grid_type;
  else if (ptr_mod_data_ != nullptr)
    grid_type_ = ptr_mod_data_->GetOrCreateMapGrid().Type;

  const auto find = [&](std::string_view key) -> const yaml::MiniYaml* {
    for (const auto& [k, v] : yaml)
      if (k == key)
        return v;
    return nullptr;
  };
  const auto value_of = [&](const yaml::MiniYaml* v) -> std::string_view {
    if (v == nullptr || v->Value == nullptr)
      return {};
    return *v->Value;
  };

  if (const yaml::MiniYaml* temp = find("MapFormat")) {
    const int format =
        meta::GetInt32Value("MapFormat", value_of(temp));
    if (format < Map::kSupportedMapFormat)
      throw std::runtime_error(
          std::format("Map format {} is not supported.", format));
  }

  if (const yaml::MiniYaml* temp = find("Title"))
    str_title_ = std::string{value_of(temp)};

  if (const yaml::MiniYaml* temp = find("Categories"))
    vec_categories_ = meta::GetStringArrayValue("Categories", value_of(temp));

  if (const yaml::MiniYaml* temp = find("Tileset"))
    str_tile_set_ = std::string{value_of(temp)};

  if (const yaml::MiniYaml* temp = find("Author"))
    str_author_ = std::string{value_of(temp)};

  if (const yaml::MiniYaml* temp = find("Bounds"))
    rect_bounds_ = meta::GetRectangleValue("Bounds", value_of(temp));

  if (const yaml::MiniYaml* temp = find("Visibility"))
    kind_visibility_ = static_cast<MapVisibility>(meta::GetEnumValue(
        "Visibility", value_of(temp), "OpenRA.MapVisibility"));

  std::string requires_mod;
  if (const yaml::MiniYaml* temp = find("RequiresMod"))
    requires_mod = std::string{value_of(temp)};

  if (ptr_mod_data_ != nullptr) {
    // MapCompatibility.Contains(requiresMod)(L433)
    const game::Manifest& manifest = ptr_mod_data_->ManifestRef();
    const bool compatible = std::any_of(
        manifest.MapCompatibility().begin(),
        manifest.MapCompatibility().end(),
        [&](const std::string& c) { return c == requires_mod; });
    kind_status_ =
        compatible ? MapStatus::Available : MapStatus::Unavailable;
  }

  if (const yaml::MiniYaml* temp = find("HideSpawnPreviews"))
    b_hide_spawn_previews_ =
        meta::GetBoolValue("HideSpawnPreviews", value_of(temp));

  try {
    // Actor definitions may change if the map format changes(上游注释;
    // mpspawn 的 LocationInit 解析 —— ActorReference 机制在 SpawnMapActors
    // 批,此处直读等价字段)
    const yaml::MiniYaml* actor_definitions = find("Actors");
    vec_spawn_points_.clear();
    if (actor_definitions != nullptr) {
      for (const yaml::MiniYamlNode& d : actor_definitions->Nodes) {
        if (value_of_is(d, "mpspawn")) {
          const yaml::MiniYamlNode* loc = d.Value.NodeWithKeyOrDefault("Location");
          if (loc != nullptr && loc->Value.Value != nullptr) {
            const int2 uv = meta::GetSizeValue("Location", *loc->Value.Value);
            vec_spawn_points_.emplace_back(uv.X, uv.Y);
          }
        }
      }
    }
  } catch (...) {
    vec_spawn_points_.clear();
    kind_status_ = MapStatus::Unavailable;
  }

  try {
    const yaml::MiniYaml* player_definitions = find("Players");
    if (player_definitions != nullptr) {
      players_ = MapPlayers{player_definitions->Nodes,
                            core::Color::FromArgbRaw(0xFF4B4B4B)};
      int4_player_count_ = 0;
      for (const auto& [_, pr] : players_.Players)
        if (pr.Playable)
          ++int4_player_count_;
    }
  } catch (...) {
    kind_status_ = MapStatus::Unavailable;
  }

  // SetCustomRules / Preview(Png)/ ModifiedDate:随 Phase 6 批(见文件头)
}

bool MapPreview::value_of_is(const yaml::MiniYamlNode& node,
                             std::string_view value) {
  return node.Value.Value != nullptr && *node.Value.Value == value;
}

std::unique_ptr<Map> MapPreview::ToMap() {
  // L566-573:惰性打开(自有包优先,否则父包重开)
  std::unique_ptr<fs::IReadOnlyPackage> reopened;
  const fs::IReadOnlyPackage* p = ptr_owned_package_.get();
  if (p == nullptr) {
    if (ptr_parent_package_ == nullptr || ptr_mod_files_ == nullptr)
      throw std::runtime_error(
          "MapPreview.ToMap requires an open context (assembled by MapCache)");
    reopened = ptr_parent_package_->OpenPackage(str_path_, *ptr_mod_files_);
    if (reopened == nullptr)
      return nullptr;
    p = reopened.get();
  }

  Map::Params params;
  params.mod_data = ptr_mod_data_;
  return std::make_unique<Map>(params, *p);
}

// ———— MapCache ————

MapCache::MapCache(const game::Manifest& manifest, fs::FileSystem& mod_files)
    : ptr_manifest_{&manifest}, ptr_mod_files_{&mod_files} {}

MapPreview& MapCache::UnknownMap() {
  // L29:static readonly UnknownMap = new MapPreview(null, null, Rectangular,
  // null) —— 进程单例(C# 静态字段的等价)
  static MapPreview preview{nullptr, std::string{},
                            MapGridType::Rectangular};
  return preview;
}

void MapCache::LoadMaps(game::ModData& mod_data) {
  // L84-135
  ptr_mod_data_ = &mod_data;
  grid_type_ = mod_data.GetOrCreateMapGrid().Type;

  // Utility mod that does not support maps(上游注释)
  if (ptr_manifest_->MapFolders().empty())
    return;

  // Enumerate map directories(上游注释;MapDirectoryTracker 随 Phase 6 批)
  for (const auto& [name_with_prefix, classification_str] :
       ptr_manifest_->MapFolders()) {
    std::string name{name_with_prefix};
    MapClassification classification = MapClassification::Unknown;
    if (!classification_str.empty()) {
      // Enum.Parse<MapClassification>(kv.Value)
      if (classification_str == "System")
        classification = MapClassification::System;
      else if (classification_str == "User")
        classification = MapClassification::User;
      else if (classification_str == "Remote")
        classification = MapClassification::Remote;
      else if (classification_str == "Generated")
        classification = MapClassification::Generated;
      else
        classification = MapClassification::Unknown;
    }

    const bool optional = !name.empty() && name.front() == '~';
    if (optional)
      name.erase(0, 1);

    std::unique_ptr<fs::IReadOnlyPackage> package;
    try {
      // 上游的 SupportDir 建目录分支随 Phase 6 平台批
      package = ptr_mod_files_->OpenPackage(name);
    } catch (...) {
      if (optional)
        continue;
      throw;
    }

    vec_map_locations_.emplace_back(package.get(), classification);
    vec_owned_packages_.push_back(std::move(package));
  }

  // PERF: Load the mod YAML once outside the loop(上游注释;modDataRules
  // 面属 SetCustomRules,随 Phase 6 批)
  for (const auto& [package, classification] : vec_map_locations_)
    for (const std::string& map_path : package->Contents())
      LoadMapInternal(map_path, *package, classification, "", grid_type_);

  // We only want to track maps in runtime, not at loadtime(上游注释)
  str_last_modified_map_.clear();
}

MapPreview& MapCache::At(const std::string& str_uid) {
  // this[uid](L418-425):UpdateMaps 的监视面随 Phase 6;缺项懒建
  for (const auto& [key, preview] : vec_previews_)
    if (key == str_uid)
      return *preview;

  vec_previews_.emplace_back(str_uid, std::make_unique<MapPreview>(
                                          ptr_mod_data_, str_uid, grid_type_));
  return *vec_previews_.back().second;
}

std::string MapCache::ChooseInitialMap(const std::string& str_initial_uid,
                                       MersenneTwister& random) {
  // L398-416
  const MapPreview& initial =
      str_initial_uid.empty() ? UnknownMap() : At(str_initial_uid);
  if (!str_initial_uid.empty() && initial.Status() == MapStatus::Available &&
      HasFlag(initial.Visibility(), MapVisibility::Lobby) &&
      (initial.Class() == MapClassification::System ||
       initial.Class() == MapClassification::User))
    return str_initial_uid;

  const auto suitable = [&](const MapPreview& m) {
    // IsSuitableInitialMap(L378-396)
    if (m.Status() != MapStatus::Available ||
        !HasFlag(m.Visibility(), MapVisibility::Lobby))
      return false;
    if (std::find(m.Categories().begin(), m.Categories().end(),
                  "Conquest") == m.Categories().end())
      return false;
    for (const auto& [_, pr] : m.Players().Players)
      if (!pr.AllowBots)
        return false;
    if (m.Bounds().Width > 128 || m.Bounds().Height > 128)
      return false;
    return true;
  };

  // RandomOrDefault(suitable)
  std::vector<MapPreview*> suitable_maps;
  for (const auto& [_, p] : vec_previews_)
    if (suitable(*p))
      suitable_maps.push_back(p.get());
  if (!suitable_maps.empty()) {
    // Exts.RandomOrDefault:随机抽取;空集返回 default
    const int index =
        random.Next(static_cast<std::int32_t>(suitable_maps.size()));
    return suitable_maps[static_cast<std::size_t>(index)]->Uid();
  }

  // FirstOrDefault(可用 + Lobby + System/User)
  for (const auto& [_, p] : vec_previews_) {
    if (p->Status() == MapStatus::Available &&
        HasFlag(p->Visibility(), MapVisibility::Lobby) &&
        (p->Class() == MapClassification::System ||
         p->Class() == MapClassification::User))
      return p->Uid();
  }

  return std::string{};
}

std::string MapCache::GetUpdatedMap(const std::string& str_uid) {
  // L337-351
  if (str_uid.empty())
    return {};
  std::string uid = str_uid;
  while (At(uid).Status() != MapStatus::Available) {
    const auto it = std::find_if(
        vec_map_updates_.begin(), vec_map_updates_.end(),
        [&](const auto& pair_k) { return pair_k.first == uid; });
    if (it == vec_map_updates_.end())
      return {};
    uid = it->second;
  }
  return uid;
}

std::string MapCache::PickLastModifiedMap(MapVisibility visibility) {
  // L58-69
  if (str_last_modified_map_.empty())
    return {};
  MapPreview& preview = At(str_last_modified_map_);
  if (preview.Status() == MapStatus::Available &&
      HasFlag(preview.Visibility(), visibility) &&
      str_last_loaded_last_modified_map_ != str_last_modified_map_) {
    str_last_loaded_last_modified_map_ = str_last_modified_map_;
    return str_last_loaded_last_modified_map_;
  }
  return {};
}

std::vector<MapPreview*> MapCache::Previews() const {
  std::vector<MapPreview*> out;
  out.reserve(vec_previews_.size());
  for (const auto& [_, p] : vec_previews_)
    out.push_back(p.get());
  return out;
}

void MapCache::LoadMapInternal(const std::string& map_path,
                               const fs::IReadOnlyPackage& package,
                               MapClassification classification,
                               const std::string& str_old_map,
                               std::optional<MapGridType> grid_type) {
  // L142-176
  std::unique_ptr<fs::IReadOnlyPackage> map_package;
  try {
    map_package = package.OpenPackage(map_path, *ptr_mod_files_);
    if (map_package != nullptr) {
      const std::string str_uid = Map::ComputeUID(*map_package);
      MapPreview& preview = At(str_uid);
      preview.SetOpenContext(ptr_mod_files_);
      preview.UpdateFromMapWithoutOwningPackage(*map_package, package,
                                                classification, grid_type);

      if (str_old_map != str_uid) {
        str_last_modified_map_ = str_uid;
        if (!str_old_map.empty()) {
          // mapUpdates[oldMap] = uid
          const auto it = std::find_if(
              vec_map_updates_.begin(), vec_map_updates_.end(),
              [&](const auto& pair_k) { return pair_k.first == str_old_map; });
          if (it != vec_map_updates_.end())
            it->second = str_uid;
          else
            vec_map_updates_.emplace_back(str_old_map, str_uid);
        }
      }
    }
  } catch (const std::exception& e) {
    // L166-175:控制台 + Log 面(日志面不落地 —— D 系惯例;控制台面保留)
    std::cout << std::format("Failed to load map: {}\nDetails:\n{}", map_path,
                             e.what())
              << std::endl;
  }
}

}  // namespace ora::map
