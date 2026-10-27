import std;
#include "game/manifest.hpp"
#include "meta/field_loader.hpp"
#include "meta/parse.hpp"

namespace ora::game {

namespace {

constexpr std::array<meta::FieldDesc, 6> kFields_ModMetadata{{
    {.str_name = "Title", .type = meta::FieldType::String, .b_required = false, .str_loader = {}, .off_offset = __builtin_offsetof(ModMetadata, str_title), .elem = nullptr, .key = nullptr, .value = nullptr, .str_type_name = "System.String"},
    {.str_name = "Version", .type = meta::FieldType::String, .b_required = false, .str_loader = {}, .off_offset = __builtin_offsetof(ModMetadata, str_version), .elem = nullptr, .key = nullptr, .value = nullptr, .str_type_name = "System.String"},
    {.str_name = "Website", .type = meta::FieldType::String, .b_required = false, .str_loader = {}, .off_offset = __builtin_offsetof(ModMetadata, str_website), .elem = nullptr, .key = nullptr, .value = nullptr, .str_type_name = "System.String"},
    {.str_name = "WebIcon32", .type = meta::FieldType::String, .b_required = false, .str_loader = {}, .off_offset = __builtin_offsetof(ModMetadata, str_webIcon32), .elem = nullptr, .key = nullptr, .value = nullptr, .str_type_name = "System.String"},
    {.str_name = "WindowTitle", .type = meta::FieldType::String, .b_required = false, .str_loader = {}, .off_offset = __builtin_offsetof(ModMetadata, str_windowTitle), .elem = nullptr, .key = nullptr, .value = nullptr, .str_type_name = "System.String"},
    {.str_name = "Hidden", .type = meta::FieldType::Bool, .b_required = false, .str_loader = {}, .off_offset = __builtin_offsetof(ModMetadata, b_hidden), .elem = nullptr, .key = nullptr, .value = nullptr, .str_type_name = "System.Boolean"},
}};

constexpr meta::RecordDesc kDesc_ModMetadata{
    .str_name = "OpenRA.ModMetadata",
    .str_base = {},
    .fields = kFields_ModMetadata,
    .requires_types = {},
    .not_before_types = {},
    .interfaces = {},
};

constexpr std::array<meta::FieldDesc, 6> kFields_RendererConstants{{
    {.str_name = "FontSheetSize", .type = meta::FieldType::Int32, .b_required = false, .str_loader = {}, .off_offset = __builtin_offsetof(RendererConstants, int4_fontSheetSize), .elem = nullptr, .key = nullptr, .value = nullptr, .str_type_name = "System.Int32"},
    {.str_name = "CursorSheetSize", .type = meta::FieldType::Int32, .b_required = false, .str_loader = {}, .off_offset = __builtin_offsetof(RendererConstants, int4_cursorSheetSize), .elem = nullptr, .key = nullptr, .value = nullptr, .str_type_name = "System.Int32"},
    {.str_name = "MapPreviewSheetSize", .type = meta::FieldType::Int32, .b_required = false, .str_loader = {}, .off_offset = __builtin_offsetof(RendererConstants, int4_mapPreviewSheetSize), .elem = nullptr, .key = nullptr, .value = nullptr, .str_type_name = "System.Int32"},
    {.str_name = "SequenceBgraSheetSize", .type = meta::FieldType::Int32, .b_required = false, .str_loader = {}, .off_offset = __builtin_offsetof(RendererConstants, int4_sequenceBgraSheetSize), .elem = nullptr, .key = nullptr, .value = nullptr, .str_type_name = "System.Int32"},
    {.str_name = "SequenceIndexedSheetSize", .type = meta::FieldType::Int32, .b_required = false, .str_loader = {}, .off_offset = __builtin_offsetof(RendererConstants, int4_sequenceIndexedSheetSize), .elem = nullptr, .key = nullptr, .value = nullptr, .str_type_name = "System.Int32"},
    {.str_name = "VertexBatchSize", .type = meta::FieldType::Int32, .b_required = false, .str_loader = {}, .off_offset = __builtin_offsetof(RendererConstants, int4_vertexBatchSize), .elem = nullptr, .key = nullptr, .value = nullptr, .str_type_name = "System.Int32"},
}};

constexpr meta::RecordDesc kDesc_RendererConstants{
    .str_name = "OpenRA.RendererConstants",
    .str_base = {},
    .fields = kFields_RendererConstants,
    .requires_types = {},
    .not_before_types = {},
    .interfaces = {},
};

}

const meta::RecordDesc& ModMetadata::record_desc() const { return kDesc_ModMetadata; }
const meta::RecordDesc& RendererConstants::record_desc() const {
  return kDesc_RendererConstants;
}

namespace {

std::vector<std::string> YamlListOf(
    const std::vector<std::pair<std::string_view, const yaml::MiniYaml*>>& vec_yaml,
    std::string_view str_key) {
  std::vector<std::string> vec_ret;
  for (const auto& [sv_k, yaml_v] : vec_yaml)
    if (sv_k == str_key)
      for (const yaml::MiniYamlNode& node : yaml_v->Nodes)
        vec_ret.emplace_back(*node.Key);
  return vec_ret;
}

}

Manifest::Manifest(std::string str_mod_id, fs::IReadOnlyPackage& package)
    : str_id_{std::move(str_mod_id)}, pkg_package_{&package} {

  const std::optional<std::vector<char>> bytes_mod =
      package.GetStream("mod.yaml");
  if (!bytes_mod.has_value())
    throw yaml::YamlException(std::format("{}: File `mod.yaml` not found.", package.Name()));

  const std::string str_name{package.Name() + ":mod.yaml"};
  vec_nodes_ = yaml::MiniYaml::FromStream(
      std::string_view{bytes_mod->data(), bytes_mod->size()}, str_name, true,
      pool_stringPool_);

  for (std::size_t int4_i = vec_nodes_.size(); int4_i-- > 0;) {
    const std::string_view sv_key =
        vec_nodes_[int4_i].Key != nullptr ? std::string_view{*vec_nodes_[int4_i].Key}
                                          : std::string_view{};
    if (sv_key != "Include")
      continue;

    const std::string str_filename =
        vec_nodes_[int4_i].Value.Value != nullptr
            ? std::string{*vec_nodes_[int4_i].Value.Value}
            : std::string{};
    const std::optional<std::vector<char>> bytes_include =
        package.GetStream(str_filename);
    if (!bytes_include.has_value())
      throw yaml::YamlException(std::format(
          "{}: File `{}` not found.", vec_nodes_[int4_i].Location.ToString(),
          str_filename));

    const std::string str_include_name{package.Name() + ":" + str_filename};
    std::vector<yaml::MiniYamlNode> vec_included = yaml::MiniYaml::FromStream(
        std::string_view{bytes_include->data(), bytes_include->size()},
        str_include_name, true, pool_stringPool_);

    vec_nodes_.erase(vec_nodes_.begin() + static_cast<std::ptrdiff_t>(int4_i));
    vec_nodes_.insert(vec_nodes_.begin() + static_cast<std::ptrdiff_t>(int4_i),
                      std::make_move_iterator(vec_included.begin()),
                      std::make_move_iterator(vec_included.end()));
  }

  yaml_merged_ = yaml::MiniYaml{nullptr, yaml::MiniYaml::Merge({vec_nodes_})};
  const std::vector<std::pair<std::string_view, const yaml::MiniYaml*>> vec_yaml =
      yaml_merged_.ToDictionary();

  const auto find_node = [&vec_yaml](std::string_view sv_key) -> const yaml::MiniYaml* {
    for (const auto& [sv_k, yaml_v] : vec_yaml)
      if (sv_k == sv_key)
        return yaml_v;
    return nullptr;
  };

  const yaml::MiniYamlNode& node_metadata = yaml_merged_.NodeWithKey("Metadata");
  meta::Load(&rec_metadata_, node_metadata.Value);

  if (const yaml::MiniYaml* yaml_renderer = find_node("RendererConstants"))
    meta::Load(&rec_rendererConstants_, *yaml_renderer);

  if (const yaml::MiniYaml* yaml_mapFolders = find_node("MapFolders")) {
    for (const yaml::MiniYamlNode& node : yaml_mapFolders->Nodes) {
      vec_mapFolders_.emplace_back(
          node.Key != nullptr ? *node.Key : std::string{},
          node.Value.Value != nullptr ? *node.Value.Value : std::string{});
    }
  }

  yaml_fileSystem_ = find_node("FileSystem");
  if (yaml_fileSystem_ == nullptr)
    throw yaml::YamlException("`FileSystem` section is not defined.");
  if (yaml_fileSystem_->Value != nullptr)
    str_fileSystemLoader_ = *yaml_fileSystem_->Value;

  if (const yaml::MiniYaml* yaml_formats = find_node("PackageFormats")) {
    if (yaml_formats->Value != nullptr && !yaml_formats->Value->empty())
      vec_packageFormats_.push_back(*yaml_formats->Value);
    for (const yaml::MiniYamlNode& node : yaml_formats->Nodes)
      if (node.Key != nullptr)
        vec_packageFormats_.push_back(*node.Key);
  }

  const auto parse_format_list = [&](const char* chr_key, std::vector<std::string>& vec_out) {
    if (const yaml::MiniYaml* yaml_node = find_node(chr_key)) {
      if (yaml_node->Value != nullptr)
        vec_out = meta::GetStringArrayValue(chr_key, *yaml_node->Value);
    }
  };
  parse_format_list("SoundFormats", vec_soundFormats_);
  parse_format_list("SpriteFormats", vec_spriteFormats_);

  if (const yaml::MiniYaml* yaml_entry = find_node("DefaultOrderGenerator")) {
    if (yaml_entry->Value != nullptr)
      str_defaultOrderGenerator_ = *yaml_entry->Value;
  }

  if (const yaml::MiniYaml* yaml_entry = find_node("SpriteSequenceFormat")) {
    if (yaml_entry->Value != nullptr)
      str_spriteSequenceFormat_ = *yaml_entry->Value;
  }

  if (const yaml::MiniYaml* yaml_entry = find_node("TerrainFormat")) {
    if (yaml_entry->Value != nullptr)
      str_terrainFormat_ = *yaml_entry->Value;
  }

  vec_mapCompatibility_.push_back(str_id_);
  if (const yaml::MiniYaml* yaml_entry = find_node("SupportsMapsFrom")) {
    if (yaml_entry->Value != nullptr) {
      for (const std::string_view part : meta::SplitComma(*yaml_entry->Value))
        vec_mapCompatibility_.emplace_back(part);
    }
  }

  vec_rules_ = YamlListOf(vec_yaml, "Rules");
  vec_sequences_ = YamlListOf(vec_yaml, "Sequences");
  vec_modelSequences_ = YamlListOf(vec_yaml, "ModelSequences");
  vec_cursors_ = YamlListOf(vec_yaml, "Cursors");
  vec_chrome_ = YamlListOf(vec_yaml, "Chrome");
  vec_chromeLayout_ = YamlListOf(vec_yaml, "ChromeLayout");
  vec_weapons_ = YamlListOf(vec_yaml, "Weapons");
  vec_voices_ = YamlListOf(vec_yaml, "Voices");
  vec_notifications_ = YamlListOf(vec_yaml, "Notifications");
  vec_music_ = YamlListOf(vec_yaml, "Music");
  vec_fluentMessages_ = YamlListOf(vec_yaml, "FluentMessages");
  vec_tileSets_ = YamlListOf(vec_yaml, "TileSets");
  vec_chromeMetrics_ = YamlListOf(vec_yaml, "ChromeMetrics");
  vec_serverTraits_ = YamlListOf(vec_yaml, "ServerTraits");
  vec_hotkeys_ = YamlListOf(vec_yaml, "Hotkeys");
  vec_missions_ = YamlListOf(vec_yaml, "Missions");
}

const yaml::MiniYaml* Manifest::GlobalModDataNode(
    std::string_view str_key) const {
  static constexpr std::array<std::string_view, 33> kReservedModuleNames{
      "Include", "Metadata", "FileSystem", "MapFolders", "Rules",
      "Sequences", "ModelSequences", "Cursors", "Chrome", "Assemblies",
      "ChromeLayout", "Weapons", "Voices", "Notifications", "Music",
      "FluentMessages", "TileSets", "ChromeMetrics", "Missions", "Hotkeys",
      "ServerTraits", "LoadScreen", "DefaultOrderGenerator",
      "SupportsMapsFrom", "SoundFormats", "SpriteFormats", "VideoFormats",
      "SpriteSequenceFormat", "TerrainFormat", "RequiresMods",
      "PackageFormats", "AllowUnusedFluentMessagesInExternalPackages",
      "RendererConstants"};
  for (std::string_view str_reserved : kReservedModuleNames)
    if (str_reserved == str_key)
      return nullptr;
  return MergedNode(str_key);
}

}
