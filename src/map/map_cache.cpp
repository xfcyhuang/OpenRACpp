import std;

#include "map/map_cache.hpp"

#include "game/manifest.hpp"
#include "game/mod_data.hpp"
#include "game/ruleset.hpp"
#include "meta/field_loader.hpp"
#include "terrain/map_grid.hpp"

namespace ora::map {

namespace {

constexpr std::int32_t kEmptyDelayMs = 50;
constexpr std::int32_t kMaxKeepAlive = 5000 / kEmptyDelayMs;

int Base64Digit(char ch) {
  if (ch >= 'A' && ch <= 'Z')
    return ch - 'A';
  if (ch >= 'a' && ch <= 'z')
    return ch - 'a' + 26;
  if (ch >= '0' && ch <= '9')
    return ch - '0' + 52;
  if (ch == '+')
    return 62;
  if (ch == '/')
    return 63;
  return -1;
}

std::optional<std::vector<std::byte>> DecodeBase64(std::string_view sv) {
  std::vector<std::byte> vec_out;
  vec_out.reserve(sv.size() / 4 * 3);
  int int_buf = 0;
  int int_bits = 0;
  for (char ch : sv) {
    if (ch == '=')
      break;
    const int int_digit = Base64Digit(ch);
    if (int_digit < 0)
      return std::nullopt;
    int_buf = (int_buf << 6) | int_digit;
    int_bits += 6;
    if (int_bits >= 8) {
      int_bits -= 8;
      vec_out.push_back(static_cast<std::byte>((int_buf >> int_bits) & 0xFF));
    }
  }
  return vec_out;
}

const yaml::MiniYaml* FindDictEntry(
    const std::vector<std::pair<std::string_view, const yaml::MiniYaml*>>& dict,
    std::string_view str_key) {
  for (const auto& [key, value] : dict)
    if (key == str_key)
      return value;
  return nullptr;
}

std::optional<yaml::MiniYaml> LoadRuleSection(
    const std::vector<std::pair<std::string_view, const yaml::MiniYaml*>>& dict,
    std::string_view str_section) {
  if (const yaml::MiniYaml* node = FindDictEntry(dict, str_section))
    return *node;
  return std::nullopt;
}

bool IsLoadableRuleDefinition(const yaml::MiniYamlNode& n) {
  const std::string_view sv_key =
      n.Key != nullptr ? std::string_view{*n.Key} : std::string_view{};
  if (!sv_key.empty() && sv_key.front() == '^')
    return true;
  std::string str_key{sv_key};
  std::ranges::transform(str_key, str_key.begin(), [](unsigned char ch) {
    return static_cast<char>(std::tolower(ch));
  });
  return str_key == "world" || str_key == "player";
}

std::shared_ptr<const game::ActorInfo> BorrowActorInfo(
    const game::ActorInfo* ptr) {
  return std::shared_ptr<const game::ActorInfo>(
      ptr, [](const game::ActorInfo*) {});
}

}  // namespace

MapPreview::MapPreview(game::ModData* mod_data, std::string str_uid,
                       std::optional<MapGridType> grid_type,
                       MapCache* ptr_cache)
    : ptr_mod_data_{mod_data},
      ptr_cache_{ptr_cache},
      str_uid_{std::move(str_uid)},
      opt_grid_type_{grid_type},
      ptr_inner_{std::make_shared<InnerData>()} {}

MapPreview::~MapPreview() = default;

std::shared_ptr<const MapPreview::InnerData> MapPreview::Snapshot() const {
  const std::lock_guard<std::mutex> lock{mtx_inner_};
  return ptr_inner_;
}

int MapPreview::MapFormat() const {
  return Snapshot()->int4_map_format;
}

MapStatus MapPreview::Status() const {
  return Snapshot()->kind_status;
}

MapClassification MapPreview::Class() const {
  return Snapshot()->kind_class;
}

const std::string& MapPreview::Uid() const {
  return str_uid_;
}

const std::string& MapPreview::Path() const {
  return str_path_;
}

const std::string& MapPreview::Title() const {
  return Snapshot()->str_title;
}

const std::string& MapPreview::Author() const {
  return Snapshot()->str_author;
}

const std::string& MapPreview::TileSet() const {
  return Snapshot()->str_tile_set;
}

const std::vector<std::string>& MapPreview::Categories() const {
  return Snapshot()->vec_categories;
}

MapVisibility MapPreview::Visibility() const {
  return Snapshot()->kind_visibility;
}

const Rectangle& MapPreview::Bounds() const {
  return Snapshot()->rect_bounds;
}

const std::vector<CPos>& MapPreview::SpawnPoints() const {
  return Snapshot()->vec_spawn_points;
}

const MapPlayers& MapPreview::Players() const {
  static const MapPlayers kEmpty{};
  const std::shared_ptr<const InnerData> data = Snapshot();
  return data->opt_players.has_value() ? *data->opt_players : kEmpty;
}

int MapPreview::PlayerCount() const {
  return Snapshot()->int4_player_count;
}

Size MapPreview::MapSize() const {
  return Snapshot()->size_;
}

bool MapPreview::HideSpawnPreviews() const {
  return Snapshot()->b_hide_spawn_previews;
}

const fmt::Png* MapPreview::Preview() const {
  return Snapshot()->ptr_preview.get();
}

std::int64_t MapPreview::ModifiedDate() const {
  return Snapshot()->int8_modified_date;
}

const std::optional<MapGenerationArgs>& MapPreview::GenerationArgs() const {
  return Snapshot()->opt_generation_args;
}

const std::optional<yaml::MiniYaml>& MapPreview::RuleDefinitions() const {
  return Snapshot()->opt_rule_definitions;
}

const std::optional<yaml::MiniYaml>& MapPreview::WeaponDefinitions() const {
  return Snapshot()->opt_weapon_definitions;
}

const std::optional<yaml::MiniYaml>& MapPreview::SequenceDefinitions() const {
  return Snapshot()->opt_sequence_definitions;
}

std::shared_ptr<const game::ActorInfo> MapPreview::WorldActorInfo() const {
  return Snapshot()->ptr_world_actor_info;
}

std::shared_ptr<const game::ActorInfo> MapPreview::PlayerActorInfo() const {
  return Snapshot()->ptr_player_actor_info;
}

std::string MapPreview::GetMessage(std::string_view str_key) const {
  std::string str_message;
  if (TryGetMessage(str_key, str_message))
    return str_message;
  return std::string{str_key};
}

bool MapPreview::TryGetMessage(std::string_view str_key,
                               std::string& out_message) const {
  (void)str_key;
  (void)out_message;
  return false;
}

std::uintptr_t MapPreview::GetMinimap() {
  const std::uintptr_t uint8_result =
      uint8_minimap_.load(std::memory_order_acquire);
  if (uint8_result != 0)
    return uint8_result;

  bool b_expected = false;
  if (b_generating_minimap_.compare_exchange_strong(b_expected, true)) {
    if (Status() == MapStatus::Available && ptr_cache_ != nullptr)
      ptr_cache_->CacheMinimap(*this);
    else
      b_generating_minimap_.store(false, std::memory_order_release);
  }

  return 0;
}

void MapPreview::SetMinimap(std::uintptr_t uint8_minimap) {
  uint8_minimap_.store(uint8_minimap, std::memory_order_release);
  b_generating_minimap_.store(false, std::memory_order_release);
}

bool MapPreview::DefinesUnsafeCustomRules() const {
  if (ptr_mod_data_ == nullptr)
    return false;
  const std::shared_ptr<const InnerData> data = Snapshot();
  const game::MapFileSystemFace face{
      .fn_open = [this](const std::string& str_filename) {
        return OpenFile(str_filename);
      },
      .fn_try_open = [this](const std::string& str_filename,
                            std::vector<char>& out) {
        return TryOpenFile(str_filename, out);
      },
      .fn_exists = [this](const std::string& str_filename) {
        return ExistsFile(str_filename);
      }};
  return game::Ruleset::DefinesUnsafeCustomRules(
      *ptr_mod_data_, face,
      data->opt_rule_definitions.has_value()
          ? &*data->opt_rule_definitions
          : nullptr,
      data->opt_weapon_definitions.has_value()
          ? &*data->opt_weapon_definitions
          : nullptr,
      data->opt_voice_definitions.has_value() ? &*data->opt_voice_definitions
                                              : nullptr,
      data->opt_notification_definitions.has_value()
          ? &*data->opt_notification_definitions
          : nullptr,
      data->opt_sequence_definitions.has_value()
          ? &*data->opt_sequence_definitions
          : nullptr);
}

std::unique_ptr<game::Ruleset> MapPreview::LoadRuleset() const {
  const std::shared_ptr<const InnerData> data = Snapshot();
  const game::MapFileSystemFace face{
      .fn_open = [this](const std::string& str_filename) {
        return OpenFile(str_filename);
      },
      .fn_try_open = [this](const std::string& str_filename,
                            std::vector<char>& out) {
        return TryOpenFile(str_filename, out);
      },
      .fn_exists = [this](const std::string& str_filename) {
        return ExistsFile(str_filename);
      }};
  return game::Ruleset::Load(
      *ptr_mod_data_, face, data->str_tile_set,
      data->opt_rule_definitions.has_value() ? &*data->opt_rule_definitions
                                             : nullptr,
      data->opt_weapon_definitions.has_value() ? &*data->opt_weapon_definitions
                                               : nullptr,
      data->opt_voice_definitions.has_value() ? &*data->opt_voice_definitions
                                              : nullptr,
      data->opt_notification_definitions.has_value()
          ? &*data->opt_notification_definitions
          : nullptr,
      data->opt_music_definitions.has_value() ? &*data->opt_music_definitions
                                              : nullptr,
      data->opt_model_sequence_definitions.has_value()
          ? &*data->opt_model_sequence_definitions
          : nullptr);
}

void MapPreview::SetOpenContext(fs::FileSystem* mod_files) {
  ptr_mod_files_ = mod_files;
}

void MapPreview::LoadPackage() const {
  if (ptr_owned_package_ == nullptr && ptr_parent_package_ != nullptr &&
      ptr_mod_files_ != nullptr)
    ptr_owned_package_ =
        ptr_parent_package_->OpenPackage(str_path_, *ptr_mod_files_);
}

void MapPreview::SetCustomRules(
    const std::vector<std::pair<std::string_view, const yaml::MiniYaml*>>& yaml,
    const std::vector<std::vector<yaml::MiniYamlNode>>* vec_mod_data_rules,
    InnerData& data_new) {
  data_new.opt_rule_definitions = LoadRuleSection(yaml, "Rules");
  data_new.opt_weapon_definitions = LoadRuleSection(yaml, "Weapons");
  data_new.opt_voice_definitions = LoadRuleSection(yaml, "Voices");
  data_new.opt_music_definitions = LoadRuleSection(yaml, "Music");
  data_new.opt_notification_definitions = LoadRuleSection(yaml, "Notifications");
  data_new.opt_sequence_definitions = LoadRuleSection(yaml, "Sequences");
  data_new.opt_model_sequence_definitions =
      LoadRuleSection(yaml, "ModelSequences");
  data_new.opt_fluent_message_definitions =
      LoadRuleSection(yaml, "FluentMessages");

  try {
    if (data_new.opt_rule_definitions.has_value()) {
      std::vector<std::vector<yaml::MiniYamlNode>> vec_sources;
      if (vec_mod_data_rules != nullptr) {
        for (const std::vector<yaml::MiniYamlNode>& file_nodes :
             *vec_mod_data_rules) {
          std::vector<yaml::MiniYamlNode> vec_file;
          for (const yaml::MiniYamlNode& n : file_nodes)
            if (IsLoadableRuleDefinition(n))
              vec_file.push_back(n);
          vec_sources.push_back(std::move(vec_file));
        }
      } else if (ptr_mod_data_ != nullptr) {
        for (const std::vector<yaml::MiniYamlNode>& file_nodes :
             ptr_mod_data_->GetRulesYaml()) {
          std::vector<yaml::MiniYamlNode> vec_file;
          for (const yaml::MiniYamlNode& n : file_nodes)
            if (IsLoadableRuleDefinition(n))
              vec_file.push_back(n);
          vec_sources.push_back(std::move(vec_file));
        }
      }

      const yaml::MiniYaml& rule_definitions = *data_new.opt_rule_definitions;
      if (rule_definitions.Value != nullptr) {
        const std::vector<std::string> vec_map_files =
            meta::GetStringArrayValue("value", *rule_definitions.Value);
        for (const std::string& str_file : vec_map_files) {
          const std::vector<char> bytes = OpenFile(str_file);
          const std::vector<yaml::MiniYamlNode> vec_nodes =
              yaml::MiniYaml::FromStream(
                  std::string_view{bytes.data(), bytes.size()}, str_file, false,
                  yaml::MiniYaml::GlobalPool());
          std::vector<yaml::MiniYamlNode> vec_file;
          for (const yaml::MiniYamlNode& n : vec_nodes)
            if (IsLoadableRuleDefinition(n))
              vec_file.push_back(n);
          vec_sources.push_back(std::move(vec_file));
        }
      }

      for (const yaml::MiniYamlNode& n : rule_definitions.Nodes)
        if (IsLoadableRuleDefinition(n))
          vec_sources.push_back({n});

      const std::vector<yaml::MiniYamlNode> vec_merged =
          yaml::MiniYaml::Merge(std::move(vec_sources));
      const yaml::MiniYamlNode* node_world = nullptr;
      const yaml::MiniYamlNode* node_player = nullptr;
      for (const yaml::MiniYamlNode& n : vec_merged) {
        const std::string_view sv_key =
            n.Key != nullptr ? std::string_view{*n.Key} : std::string_view{};
        if (sv_key == "world" && node_world == nullptr)
          node_world = &n;
        else if (sv_key == "player" && node_player == nullptr)
          node_player = &n;
      }
      if (node_world != nullptr && node_player != nullptr) {
        data_new.ptr_world_actor_info =
            std::make_shared<const game::ActorInfo>("world",
                                                    node_world->Value);
        data_new.ptr_player_actor_info =
            std::make_shared<const game::ActorInfo>("player",
                                                    node_player->Value);
        return;
      }
    }
  } catch (...) {
  }

  if (ptr_mod_data_ != nullptr) {
    data_new.ptr_world_actor_info =
        BorrowActorInfo(ptr_mod_data_->DefaultRules().FindActor("world"));
    data_new.ptr_player_actor_info =
        BorrowActorInfo(ptr_mod_data_->DefaultRules().FindActor("player"));
  }
}

bool MapPreview::UpdateFromMapWithoutOwningPackage(
    const fs::IReadOnlyPackage& p, const fs::IReadOnlyPackage& parent,
    MapClassification classification, std::optional<MapGridType> grid_type,
    const std::vector<std::vector<yaml::MiniYamlNode>>* vec_mod_data_rules) {
  const bool b_ok = UpdateFromMap(p, classification, grid_type,
                                  vec_mod_data_rules);
  ptr_parent_package_ = &parent;
  ptr_owned_package_.reset();
  return b_ok;
}

bool MapPreview::UpdateFromMap(
    const fs::IReadOnlyPackage& p, MapClassification classification,
    std::optional<MapGridType> grid_type,
    const std::vector<std::vector<yaml::MiniYamlNode>>* vec_mod_data_rules) {
  str_path_ = p.Name();

  auto yaml_stream = p.GetStream("map.yaml");
  if (!yaml_stream.has_value())
    return false;

  yaml::StringPool& pool = yaml::MiniYaml::GlobalPool();
  const std::vector<yaml::MiniYamlNode> nodes = yaml::MiniYaml::FromStream(
      std::string_view{yaml_stream->data(), yaml_stream->size()},
      std::format("{}:map.yaml", p.Name()), true, pool);
  yaml::MiniYaml root;
  root.Nodes = nodes;
  const auto yaml = root.ToDictionary();

  InnerData data_new = *Snapshot();
  data_new.kind_class = classification;
  if (grid_type.has_value())
    data_new.grid_type = *grid_type;
  else if (ptr_mod_data_ != nullptr)
    data_new.grid_type = ptr_mod_data_->GetOrCreateMapGrid().Type;

  const auto value_of = [&](const yaml::MiniYaml* v) -> std::string_view {
    if (v == nullptr || v->Value == nullptr)
      return {};
    return *v->Value;
  };

  if (const yaml::MiniYaml* temp = FindDictEntry(yaml, "MapFormat")) {
    const int format = meta::GetInt32Value("MapFormat", value_of(temp));
    if (format < Map::kSupportedMapFormat)
      return false;
  }

  if (const yaml::MiniYaml* temp = FindDictEntry(yaml, "Title"))
    data_new.str_title = std::string{value_of(temp)};

  if (const yaml::MiniYaml* temp = FindDictEntry(yaml, "Categories"))
    data_new.vec_categories =
        meta::GetStringArrayValue("Categories", value_of(temp));

  if (const yaml::MiniYaml* temp = FindDictEntry(yaml, "Tileset"))
    data_new.str_tile_set = std::string{value_of(temp)};

  if (const yaml::MiniYaml* temp = FindDictEntry(yaml, "Author"))
    data_new.str_author = std::string{value_of(temp)};

  if (const yaml::MiniYaml* temp = FindDictEntry(yaml, "Bounds"))
    data_new.rect_bounds = meta::GetRectangleValue("Bounds", value_of(temp));

  if (const yaml::MiniYaml* temp = FindDictEntry(yaml, "Visibility"))
    data_new.kind_visibility = static_cast<MapVisibility>(
        meta::GetEnumValue("Visibility", value_of(temp), "OpenRA.MapVisibility"));

  std::string requires_mod;
  if (const yaml::MiniYaml* temp = FindDictEntry(yaml, "RequiresMod"))
    requires_mod = std::string{value_of(temp)};

  if (const yaml::MiniYaml* temp = FindDictEntry(yaml, "MapFormat"))
    data_new.int4_map_format =
        meta::GetInt32Value("MapFormat", value_of(temp));

  if (ptr_mod_data_ != nullptr) {
    const game::Manifest& manifest = ptr_mod_data_->ManifestRef();
    const bool compatible = std::any_of(
        manifest.MapCompatibility().begin(),
        manifest.MapCompatibility().end(),
        [&](const std::string& c) { return c == requires_mod; });
    data_new.kind_status =
        compatible ? MapStatus::Available : MapStatus::Unavailable;
  }

  data_new.b_hide_spawn_previews = false;
  if (const yaml::MiniYaml* temp = FindDictEntry(yaml, "HideSpawnPreviews"))
    data_new.b_hide_spawn_previews =
        meta::GetBoolValue("HideSpawnPreviews", value_of(temp));

  try {
    const yaml::MiniYaml* actor_definitions = FindDictEntry(yaml, "Actors");
    data_new.vec_spawn_points.clear();
    if (actor_definitions != nullptr) {
      for (const yaml::MiniYamlNode& d : actor_definitions->Nodes) {
        const std::string_view sv_value =
            d.Value.Value != nullptr ? std::string_view{*d.Value.Value}
                                     : std::string_view{};
        if (sv_value == "mpspawn") {
          const yaml::MiniYamlNode* loc =
              d.Value.NodeWithKeyOrDefault("Location");
          if (loc != nullptr && loc->Value.Value != nullptr) {
            const int2 uv =
                meta::GetSizeValue("Location", *loc->Value.Value);
            data_new.vec_spawn_points.emplace_back(uv.X, uv.Y);
          }
        }
      }
    }
  } catch (...) {
    data_new.vec_spawn_points.clear();
    data_new.kind_status = MapStatus::Unavailable;
  }

  try {
    const yaml::MiniYaml* player_definitions = FindDictEntry(yaml, "Players");
    if (player_definitions != nullptr) {
      data_new.opt_players =
          MapPlayers{player_definitions->Nodes,
                     core::Color::FromArgbRaw(0xFF4B4B4B)};
      data_new.int4_player_count = 0;
      for (const auto& [_, pr] : data_new.opt_players->Players)
        if (pr.Playable)
          ++data_new.int4_player_count;
    }
  } catch (...) {
    data_new.kind_status = MapStatus::Unavailable;
  }

  SetCustomRules(yaml, vec_mod_data_rules, data_new);

  const bool b_load_preview_images =
      ptr_cache_ == nullptr || ptr_cache_->LoadPreviewImages;
  if (b_load_preview_images && p.Contains("map.png")) {
    auto png_stream = p.GetStream("map.png");
    if (png_stream.has_value()) {
      try {
        data_new.ptr_preview = std::make_shared<const fmt::Png>(
            std::span<const std::byte>{
                reinterpret_cast<const std::byte*>(png_stream->data()),
                png_stream->size()});
      } catch (...) {
        data_new.ptr_preview.reset();
      }
    }
  }

  if (!p.Name().empty()) {
    namespace fsstd = std::filesystem;
    std::error_code ec;
    if (const fsstd::directory_entry entry{p.Name(), ec}; !ec) {
      std::error_code ec_stamp;
      if (const auto tm = entry.last_write_time(ec_stamp); !ec_stamp)
        data_new.int8_modified_date = tm.time_since_epoch().count();
      else
        data_new.int8_modified_date =
            std::chrono::system_clock::now().time_since_epoch().count();
    } else {
      data_new.int8_modified_date =
          std::chrono::system_clock::now().time_since_epoch().count();
    }
  } else {
    data_new.int8_modified_date =
        std::chrono::system_clock::now().time_since_epoch().count();
  }

  const std::lock_guard<std::mutex> lock{mtx_inner_};
  ptr_inner_ = std::make_shared<const InnerData>(std::move(data_new));
  return true;
}

void MapPreview::UpdateFromGenerationArgs(const MapGenerationArgs& args) {
  InnerData data_new = *Snapshot();
  data_new.kind_class = MapClassification::Generated;
  data_new.kind_status = MapStatus::Generatable;
  data_new.str_title = args.str_title;
  data_new.str_author = args.str_author;
  data_new.str_tile_set = args.str_tileset;
  data_new.opt_generation_args = args;
  data_new.int4_map_format = Map::kCurrentMapFormat;
  data_new.kind_status = MapStatus::Unavailable;

  const std::lock_guard<std::mutex> lock{mtx_inner_};
  ptr_inner_ = std::make_shared<const InnerData>(std::move(data_new));
}

void MapPreview::Generate() {
  if (Class() != MapClassification::Generated ||
      Status() != MapStatus::Generatable)
    return;

  InnerData data_new = *Snapshot();
  data_new.kind_status = MapStatus::Generating;

  const std::lock_guard<std::mutex> lock{mtx_inner_};
  ptr_inner_ = std::make_shared<const InnerData>(std::move(data_new));
}

void MapPreview::BeginRemoteSearch() {
  InnerData data_new = *Snapshot();
  data_new.kind_class = MapClassification::Remote;
  data_new.kind_status = MapStatus::Searching;

  const std::lock_guard<std::mutex> lock{mtx_inner_};
  if (ptr_inner_->kind_class == MapClassification::Unknown ||
      ptr_inner_->kind_class == MapClassification::Remote)
    ptr_inner_ = std::make_shared<const InnerData>(std::move(data_new));
}

void MapPreview::CompleteRemoteSearch(
    const yaml::MiniYamlNode& node_yaml,
    const std::function<void(MapPreview&)>& fn_parse_metadata) {
  InnerData data_new = *Snapshot();
  data_new.kind_class = MapClassification::Remote;
  data_new.kind_status = MapStatus::Unavailable;

  const auto find_field = [&](std::string_view str_key) {
    for (const yaml::MiniYamlNode& n : node_yaml.Value.Nodes) {
      const std::string_view sv_key =
          n.Key != nullptr ? std::string_view{*n.Key} : std::string_view{};
      if (sv_key == str_key)
        return &n.Value;
    }
    return static_cast<const yaml::MiniYaml*>(nullptr);
  };
  const auto field_value = [&](const yaml::MiniYaml* f) -> std::string_view {
    if (f == nullptr || f->Value == nullptr)
      return {};
    return *f->Value;
  };

  const auto try_field_str = [&](std::string_view str_key,
                                 std::string& out) {
    const yaml::MiniYaml* f = find_field(str_key);
    if (f == nullptr || f->Value == nullptr || f->Value->empty())
      return false;
    out = std::string{*f->Value};
    return true;
  };

  try {
    std::string str_buf;
    if (const yaml::MiniYaml* f = find_field("downloading"))
      if (meta::GetBoolValue("downloading", field_value(f)))
        data_new.kind_status = MapStatus::DownloadAvailable;

    if (!try_field_str("title", data_new.str_title) ||
        !try_field_str("author", data_new.str_author) ||
        !try_field_str("tileset", data_new.str_tile_set)) {
      data_new.kind_status = MapStatus::Unavailable;
    } else {
      if (const yaml::MiniYaml* f = find_field("categories"))
        data_new.vec_categories =
            meta::GetStringArrayValue("categories", field_value(f));

      std::int32_t int4_players = 0;
      if (try_field_str("players", str_buf) &&
          meta::TryParseInt32Invariant(str_buf, int4_players))
        data_new.int4_player_count = int4_players;

      if (const yaml::MiniYaml* f = find_field("bounds"))
        data_new.rect_bounds =
            meta::GetRectangleValue("bounds", field_value(f));

      std::int32_t int4_map_format = 0;
      if (try_field_str("mapformat", str_buf) &&
          meta::TryParseInt32Invariant(str_buf, int4_map_format))
        data_new.int4_map_format = int4_map_format;

      if (const yaml::MiniYaml* f = find_field("hideSpawnPreviews"))
        data_new.b_hide_spawn_previews =
            meta::GetBoolValue("hideSpawnPreviews", field_value(f));

      if (const yaml::MiniYaml* f = find_field("spawnpoints")) {
        const std::vector<std::string_view> vec_parts =
            meta::SplitCommaTrimmed(field_value(f));
        std::vector<CPos> vec_points;
        bool b_ok = true;
        for (std::size_t i = 0; i + 1 < vec_parts.size(); i += 2) {
          std::int32_t int4_x = 0;
          std::int32_t int4_y = 0;
          if (!meta::TryParseInt32Invariant(vec_parts[i], int4_x) ||
              !meta::TryParseInt32Invariant(vec_parts[i + 1], int4_y)) {
            b_ok = false;
            break;
          }
          vec_points.emplace_back(int4_x, int4_y);
        }
        if (b_ok && !vec_parts.empty())
          data_new.vec_spawn_points = std::move(vec_points);
      }

      if (const yaml::MiniYaml* f = find_field("map_grid_type"))
        data_new.grid_type = static_cast<MapGridType>(meta::GetEnumValue(
            "map_grid_type", field_value(f), "OpenRA.MapGridType"));

      const bool b_preview_images =
          ptr_cache_ == nullptr || ptr_cache_->LoadPreviewImages;
      if (b_preview_images) {
        try {
          std::string str_minimap;
          if (try_field_str("minimap", str_minimap)) {
            const std::optional<std::vector<std::byte>> vec_decoded =
                DecodeBase64(str_minimap);
            if (vec_decoded.has_value())
              data_new.ptr_preview = std::make_shared<const fmt::Png>(
                  std::span<const std::byte>{vec_decoded->data(),
                                             vec_decoded->size()});
          }
        } catch (...) {
          data_new.ptr_preview.reset();
        }
      }

      std::string str_players_block;
      if (try_field_str("players_block", str_players_block)) {
        const std::optional<std::vector<std::byte>> vec_players =
            DecodeBase64(str_players_block);
        if (vec_players.has_value()) {
          data_new.opt_players =
              MapPlayers{yaml::MiniYaml::FromString(
                             std::string_view{
                                 reinterpret_cast<const char*>(
                                     vec_players->data()),
                                 vec_players->size()},
                             "players_block", false,
                             yaml::MiniYaml::GlobalPool()),
                         core::Color::FromArgbRaw(0xFF4B4B4B)};
        }
      }

      std::string str_rules;
      if (try_field_str("rules", str_rules)) {
        const std::optional<std::vector<std::byte>> vec_rules =
            DecodeBase64(str_rules);
        if (vec_rules.has_value()) {
          yaml::MiniYaml root;
          root.Nodes = yaml::MiniYaml::FromString(
              std::string_view{
                  reinterpret_cast<const char*>(vec_rules->data()),
                  vec_rules->size()},
              "rules", false, yaml::MiniYaml::GlobalPool());
          SetCustomRules(root.ToDictionary(), nullptr, data_new);
        }
      }

      std::string str_game_mod;
      if (try_field_str("game_mod", str_game_mod) &&
          ptr_mod_data_ != nullptr) {
        const game::Manifest& manifest = ptr_mod_data_->ManifestRef();
        const bool compatible = std::any_of(
            manifest.MapCompatibility().begin(),
            manifest.MapCompatibility().end(),
            [&](const std::string& c) { return c == str_game_mod; });
        if (!compatible)
          data_new.kind_status = MapStatus::Unavailable;
      }
    }
  } catch (...) {
    data_new.kind_status = MapStatus::Unavailable;
  }

  MapClassification kind_after;
  {
    const std::lock_guard<std::mutex> lock{mtx_inner_};
    kind_after = ptr_inner_->kind_class;
    if (kind_after == MapClassification::Remote)
      ptr_inner_ = std::make_shared<const InnerData>(std::move(data_new));
  }

  if (kind_after == MapClassification::Remote) {
    if (Snapshot()->ptr_preview != nullptr && ptr_cache_ != nullptr)
      ptr_cache_->CacheMinimap(*this);
    if (fn_parse_metadata != nullptr)
      fn_parse_metadata(*this);
  }
}

void MapPreview::Install(const std::string& str_map_repository_url) {
  (void)str_map_repository_url;
}

void MapPreview::Invalidate() {
  InnerData data_new = *Snapshot();
  data_new.kind_class = MapClassification::Unknown;
  data_new.kind_status = MapStatus::Unavailable;

  const std::lock_guard<std::mutex> lock{mtx_inner_};
  ptr_inner_ = std::make_shared<const InnerData>(std::move(data_new));
}

void MapPreview::Delete() {
  Invalidate();
  if (auto* ptr_read_write =
          dynamic_cast<fs::IReadWritePackage*>(
              const_cast<fs::IReadOnlyPackage*>(ptr_parent_package_)))
    ptr_read_write->Delete(str_path_);
}

std::vector<char> MapPreview::OpenFile(const std::string& filename) const {
  LoadPackage();
  if (filename.find('|') == std::string::npos &&
      ptr_owned_package_ != nullptr &&
      ptr_owned_package_->Contains(filename)) {
    auto stream = ptr_owned_package_->GetStream(filename);
    if (stream.has_value())
      return std::move(*stream);
  }
  if (ptr_mod_files_ != nullptr) {
    std::vector<char> bytes;
    if (ptr_mod_files_->TryOpen(filename, bytes))
      return bytes;
  }
  return {};
}

bool MapPreview::TryGetPackageContainingFile(
    const std::string& path, fs::IReadOnlyPackage*& out_package,
    std::string& out_filename) const {
  if (ptr_mod_files_ != nullptr)
    return ptr_mod_files_->TryGetPackageContaining(path, out_package,
                                                   out_filename);
  return false;
}

bool MapPreview::TryOpenFile(const std::string& filename,
                             std::vector<char>& out_bytes) const {
  if (filename.find('|') == std::string::npos) {
    LoadPackage();
    if (ptr_owned_package_ != nullptr) {
      auto stream = ptr_owned_package_->GetStream(filename);
      if (stream.has_value()) {
        out_bytes = std::move(*stream);
        return true;
      }
    }
  }
  if (ptr_mod_files_ != nullptr)
    return ptr_mod_files_->TryOpen(filename, out_bytes);
  return false;
}

bool MapPreview::ExistsFile(const std::string& filename) const {
  LoadPackage();
  if (filename.find('|') == std::string::npos &&
      ptr_owned_package_ != nullptr &&
      ptr_owned_package_->Contains(filename))
    return true;
  if (ptr_mod_files_ != nullptr)
    return ptr_mod_files_->Exists(filename);
  return false;
}

bool MapPreview::IsExternalFile(const std::string& filename) const {
  (void)filename;
  return false;
}

std::unique_ptr<Map> MapPreview::ToMap() {
  LoadPackage();
  if (ptr_owned_package_ == nullptr)
    return nullptr;

  Map::Params params;
  params.mod_data = ptr_mod_data_;
  return std::make_unique<Map>(params, *ptr_owned_package_);
}

MapCache::MapCache(const game::Manifest& manifest, fs::FileSystem& mod_files)
    : ptr_manifest_{&manifest}, ptr_mod_files_{&mod_files} {}

MapCache::~MapCache() {
  b_disposed_.store(true, std::memory_order_release);
  if (thread_loader_.joinable())
    thread_loader_.join();
}

MapPreview& MapCache::UnknownMap() {
  static MapPreview preview{nullptr, std::string{}, MapGridType::Rectangular,
                            nullptr};
  return preview;
}

void MapCache::LoadMaps(game::ModData& mod_data) {
  ptr_mod_data_ = &mod_data;
  grid_type_ = mod_data.GetOrCreateMapGrid().Type;

  if (ptr_manifest_->MapFolders().empty())
    return;

  for (const auto& [name_with_prefix, classification_str] :
       ptr_manifest_->MapFolders()) {
    std::string name{name_with_prefix};
    MapClassification classification = MapClassification::Unknown;
    if (!classification_str.empty()) {
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
      package = ptr_mod_files_->OpenPackage(name);
    } catch (...) {
      if (optional)
        continue;
      throw;
    }

    vec_map_locations_.emplace_back(package.get(), classification);
    vec_trackers_.push_back(std::make_unique<MapDirectoryTracker>(
        package.get(), classification));
    vec_owned_packages_.push_back(std::move(package));
  }

  std::optional<std::vector<std::vector<yaml::MiniYamlNode>>> opt_mod_data_rules;
  if (!ptr_manifest_->Rules().empty())
    opt_mod_data_rules = mod_data.GetRulesYaml();

  for (const auto& [package, classification] : vec_map_locations_)
    for (const std::string& map_path : package->Contents())
      LoadMapInternal(map_path, *package, classification, "", grid_type_,
                      opt_mod_data_rules.has_value() ? &*opt_mod_data_rules
                                                     : nullptr);

  str_last_modified_map_.clear();
}

void MapCache::UpdateMaps() {
  for (const std::unique_ptr<MapDirectoryTracker>& tracker : vec_trackers_)
    tracker->UpdateMaps(*this);
}

void MapCache::LoadMap(const std::string& str_map,
                       const fs::IReadOnlyPackage& package,
                       MapClassification classification,
                       const std::string& str_old_map) {
  LoadMapInternal(str_map, package, classification, str_old_map, std::nullopt,
                  nullptr);
}

MapPreview& MapCache::At(const std::string& str_uid) {
  for (const auto& [key, preview] : vec_previews_)
    if (key == str_uid)
      return *preview;

  vec_previews_.emplace_back(
      str_uid, std::make_unique<MapPreview>(ptr_mod_data_, str_uid, grid_type_,
                                            this));
  return *vec_previews_.back().second;
}

std::string MapCache::ChooseInitialMap(const std::string& str_initial_uid,
                                       MersenneTwister& random) {
  const MapPreview& initial =
      str_initial_uid.empty() ? UnknownMap() : At(str_initial_uid);
  if (!str_initial_uid.empty() && initial.Status() == MapStatus::Available &&
      HasFlag(initial.Visibility(), MapVisibility::Lobby) &&
      (initial.Class() == MapClassification::System ||
       initial.Class() == MapClassification::User))
    return str_initial_uid;

  const auto suitable = [&](const MapPreview& m) {
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

  std::vector<MapPreview*> suitable_maps;
  for (const auto& [_, p] : vec_previews_)
    if (suitable(*p))
      suitable_maps.push_back(p.get());
  if (!suitable_maps.empty()) {
    const int index = random.Next(
        static_cast<std::int32_t>(suitable_maps.size()));
    return suitable_maps[static_cast<std::size_t>(index)]->Uid();
  }

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

const std::vector<std::pair<const fs::IReadOnlyPackage*, MapClassification>>&
MapCache::MapLocations() const {
  return vec_map_locations_;
}

void MapCache::LoadMapInternal(
    const std::string& map_path, const fs::IReadOnlyPackage& package,
    MapClassification classification, const std::string& str_old_map,
    std::optional<MapGridType> grid_type,
    const std::vector<std::vector<yaml::MiniYamlNode>>* vec_mod_data_rules) {
  std::unique_ptr<fs::IReadOnlyPackage> map_package;
  try {
    map_package = package.OpenPackage(map_path, *ptr_mod_files_);
    if (map_package != nullptr) {
      const std::string str_uid = Map::ComputeUID(*map_package);
      MapPreview& preview = At(str_uid);
      preview.SetOpenContext(ptr_mod_files_);
      preview.UpdateFromMapWithoutOwningPackage(
          *map_package, package, classification, grid_type,
          vec_mod_data_rules);

      if (str_old_map != str_uid) {
        str_last_modified_map_ = str_uid;
        if (!str_old_map.empty()) {
          const auto it = std::find_if(
              vec_map_updates_.begin(), vec_map_updates_.end(),
              [&](const auto& pair_k) {
                return pair_k.first == str_old_map;
              });
          if (it != vec_map_updates_.end())
            it->second = str_uid;
          else
            vec_map_updates_.emplace_back(str_old_map, str_uid);
        }
      }
    }
  } catch (const std::exception& e) {
    std::cout << std::format("Failed to load map: {}\nDetails:\n{}", map_path,
                             e.what())
              << std::endl;
  }
}

void MapCache::CacheMinimap(MapPreview& preview) {
  bool launch_preview_loader_thread;
  {
    const std::lock_guard<std::mutex> lock{mtx_async_};
    vec_generate_minimap_.push_back(&preview);
    launch_preview_loader_thread = b_loader_shutdown_;
    b_loader_shutdown_ = false;
  }

  if (launch_preview_loader_thread) {
    if (fn_run_after_tick_ != nullptr) {
      fn_run_after_tick_([this] {
        if (thread_loader_.joinable())
          thread_loader_.join();
        thread_loader_ = std::thread([this] { LoadAsyncInternal(); });
      });
    } else {
      if (thread_loader_.joinable())
        thread_loader_.join();
      thread_loader_ = std::thread([this] { LoadAsyncInternal(); });
    }
  }
}

void MapCache::LoadAsyncInternal() {
  int keep_alive = kMaxKeepAlive;

  while (true) {
    std::vector<MapPreview*> vec_todo;
    {
      const std::lock_guard<std::mutex> lock{mtx_async_};
      for (MapPreview* p : vec_generate_minimap_)
        if (p->GetMinimap() == 0)
          vec_todo.push_back(p);
      vec_generate_minimap_.clear();
      if (keep_alive > 0)
        --keep_alive;
      if (keep_alive == 0 && vec_todo.empty()) {
        b_loader_shutdown_ = true;
        break;
      }
    }

    if (b_disposed_.load(std::memory_order_acquire)) {
      const std::lock_guard<std::mutex> lock{mtx_async_};
      b_loader_shutdown_ = true;
      break;
    }

    if (vec_todo.empty()) {
      std::this_thread::sleep_for(std::chrono::milliseconds{kEmptyDelayMs});
      continue;
    }
    keep_alive = kMaxKeepAlive;

    for (MapPreview* p : vec_todo) {
      if (p->Preview() != nullptr) {
        const std::function<void()> fn_work = [this, p] {
          if (fn_materialize_minimap_ != nullptr)
            fn_materialize_minimap_(*p);
        };
        if (fn_run_after_tick_ != nullptr)
          fn_run_after_tick_(fn_work);
        else
          fn_work();
      }

      std::this_thread::sleep_for(std::chrono::milliseconds{
          std::thread::hardware_concurrency() == 1 ? 25 : 5});
    }
  }

  if (fn_release_buffer_ != nullptr) {
    if (fn_run_after_tick_ != nullptr)
      fn_run_after_tick_(fn_release_buffer_);
    else
      fn_release_buffer_();
  }
}

}  // namespace ora::map
