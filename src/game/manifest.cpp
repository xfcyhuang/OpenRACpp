// UPSTREAM: OpenRA.Game/Manifest.cs @7d57605(manifest.hpp 的实现;
//          Include 展开/Merge/各节解析,见 hpp 头注)
//          Implementation of manifest.hpp — the Include expansion/Merge/
//          section parsing; see the hpp header notes.
import std;
#include "game/manifest.hpp"
#include "meta/field_loader.hpp"

namespace ora::game {

// ———— ModMetadata / RendererConstants 描述表 ————

namespace {

// 说明:ModMetadata 的 C# 字段名 Title/Version/…与 C++ 成员名不同 —— yaml 键
// 兼容面要求描述表携带 C# 名,故手写条目(不走 ORA_FIELD 的 #Member 拼名)
// NB: the C# field names (Title/Version/…) differ from the C++ members — the
// yaml-key compatibility surface requires the C# names in the descriptors,
// so the entries are written out (not the #Member spelling of ORA_FIELD).
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

}  // namespace

const meta::RecordDesc& ModMetadata::record_desc() const { return kDesc_ModMetadata; }
const meta::RecordDesc& RendererConstants::record_desc() const {
  return kDesc_RendererConstants;
}

// ———— Manifest ————

namespace {

/// YamlList(Manifest.cs L190-196):节点 Key 列表(缺节 = 空)
/// YamlList (Manifest.cs L190-196): the node-key list (a missing section
/// yields empty).
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

}  // namespace

Manifest::Manifest(std::string str_mod_id, fs::IReadOnlyPackage& package)
    : str_id_{std::move(str_mod_id)}, pkg_package_{&package} {
  // mod.yaml 解析(L97):位置名 = "{package.Name}:mod.yaml"
  // mod.yaml parsing (L97): location name = "{package.Name}:mod.yaml".
  const std::optional<std::vector<char>> bytes_mod =
      package.GetStream("mod.yaml");
  if (!bytes_mod.has_value())
    throw yaml::YamlException(std::format("{}: File `mod.yaml` not found.", package.Name()));

  const std::string str_name{package.Name() + ":mod.yaml"};
  vec_nodes_ = yaml::MiniYaml::FromStream(
      std::string_view{bytes_mod->data(), bytes_mod->size()}, str_name, true,
      pool_stringPool_);

  // Include 展开(L98-111):倒序就地替换
  // Include expansion (L98-111): in-place replacement, in reverse order.
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

    // 倒序替换保持索引稳定(与上游 RemoveAt+InsertRange 等价)
    // Replace in reverse to keep indices stable (equivalent to the upstream
    // RemoveAt+InsertRange).
    vec_nodes_.erase(vec_nodes_.begin() + static_cast<std::ptrdiff_t>(int4_i));
    vec_nodes_.insert(vec_nodes_.begin() + static_cast<std::ptrdiff_t>(int4_i),
                      std::make_move_iterator(vec_included.begin()),
                      std::make_move_iterator(vec_included.end()));
  }

  // Merge 继承覆盖(L114):new MiniYaml(null, Merge([nodes])).ToDictionary()
  // The inheritance merge (L114).
  yaml_merged_ = yaml::MiniYaml{nullptr, yaml::MiniYaml::Merge({vec_nodes_})};
  const std::vector<std::pair<std::string_view, const yaml::MiniYaml*>> vec_yaml =
      yaml_merged_.ToDictionary();

  const auto find_node = [&vec_yaml](std::string_view sv_key) -> const yaml::MiniYaml* {
    for (const auto& [sv_k, yaml_v] : vec_yaml)
      if (sv_k == sv_key)
        return yaml_v;
    return nullptr;
  };

  // Metadata(L116):FieldLoader.Load<ModMetadata>(yaml["Metadata"])(缺节 =
  // KeyNotFound;上游同)
  // Metadata (L116): FieldLoader.Load<ModMetadata>(yaml["Metadata"]) (a
  // missing section = KeyNotFound, as upstream).
  const yaml::MiniYamlNode& node_metadata = yaml_merged_.NodeWithKey("Metadata");
  meta::Load(&rec_metadata_, node_metadata.Value);

  // MapFolders(L119 + L198-204):my.Value 字典
  // MapFolders (L119 + L198-204): the my.Value dictionary.
  if (const yaml::MiniYaml* yaml_mapFolders = find_node("MapFolders")) {
    for (const yaml::MiniYamlNode& node : yaml_mapFolders->Nodes) {
      vec_mapFolders_.emplace_back(
          node.Key != nullptr ? *node.Key : std::string{},
          node.Value.Value != nullptr ? *node.Value.Value : std::string{});
    }
  }

  // FileSystem(L121-122):必需节
  // FileSystem (L121-122): the required section.
  yaml_fileSystem_ = find_node("FileSystem");
  if (yaml_fileSystem_ == nullptr)
    throw yaml::YamlException("`FileSystem` section is not defined.");
  if (yaml_fileSystem_->Value != nullptr)
    str_fileSystemLoader_ = *yaml_fileSystem_->Value;

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
  vec_tileSets_ = YamlListOf(vec_yaml, "TileSets");
  vec_chromeMetrics_ = YamlListOf(vec_yaml, "ChromeMetrics");
  vec_serverTraits_ = YamlListOf(vec_yaml, "ServerTraits");
}

}  // namespace ora::game
