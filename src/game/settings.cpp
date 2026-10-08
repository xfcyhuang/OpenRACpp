// UPSTREAM: OpenRA.Game/Settings.cs @b6fc03f L52-340(实现部分;头注见
//          settings.hpp)
//          The implementation half of Settings.cs L52-340 (the header note
//          lives in settings.hpp).
#include "game/settings.hpp"

#include "meta/dump_format.hpp"
#include "meta/field_loader.hpp"
#include "meta/field_saver.hpp"

namespace ora::game {

namespace {

using meta::FieldDesc;
using meta::FieldType;
using meta::GenericValue;

/// PlayerSettings 字段表(Settings.cs L309-315;默认值取上游字面量)
/// The PlayerSettings field table (Settings.cs L309-315; the defaults are
/// upstream's literals).
constexpr FieldDesc kPlayerSettingsFields[] = {
    {.str_name = "Name",
     .type = FieldType::String,
     .b_required = false,
     .str_loader = {}},
    {.str_name = "Color",
     .type = FieldType::Color,
     .b_required = false,
     .str_loader = {}},
    {.str_name = "LastServer",
     .type = FieldType::String,
     .b_required = false,
     .str_loader = {}},
    {.str_name = "CustomColors",
     .type = FieldType::ImmutableArray,
     .b_required = false,
     .str_loader = {}},
};

/// GameSettings 字段表(Settings.cs L319-341;核心标量子集)
/// The GameSettings field table (Settings.cs L319-341; the core scalar
/// subset).
constexpr FieldDesc kGameSettingsFields[] = {
    {.str_name = "Platform", .type = FieldType::String},
    {.str_name = "ViewportEdgeScroll", .type = FieldType::Bool},
    {.str_name = "ViewportEdgeScrollMargin", .type = FieldType::Int32},
    {.str_name = "LockMouseWindow", .type = FieldType::Bool},
    {.str_name = "MouseControlStyle", .type = FieldType::Enum},
    {.str_name = "MouseScroll", .type = FieldType::Enum},
    {.str_name = "ViewportEdgeScrollStep", .type = FieldType::Float},
    {.str_name = "UIScrollSpeed", .type = FieldType::Float},
    {.str_name = "ZoomSpeed", .type = FieldType::Float},
    {.str_name = "SelectionDeadzone", .type = FieldType::Int32},
    {.str_name = "MouseScrollDeadzone", .type = FieldType::Int32},
    {.str_name = "UseAlternateScrollButton", .type = FieldType::Bool},
    {.str_name = "HideReplayChat", .type = FieldType::Bool},
    {.str_name = "StatusBars", .type = FieldType::Enum},
    {.str_name = "TargetLines", .type = FieldType::Enum},
    {.str_name = "UsePlayerStanceColors", .type = FieldType::Bool},
};

/// PlayerSettings/GameSettings 的默认值(展开序 = 字段序)
/// The PlayerSettings/GameSettings defaults (the expansion order = the
/// field order).
const std::vector<GenericValue>& PlayerSettingsDefaults() {
  static const std::vector<GenericValue> vec_defaults = {
      GenericValue::Of(std::string{"Commander"}),
      // Color.FromArgb(200, 32, 32) 的打包((A<<24)+(R<<16)+(G<<8)+B)
      // The Color.FromArgb(200, 32, 32) packing.
      GenericValue::Of(static_cast<std::int64_t>(0xFFC82020)),
      GenericValue::Of(std::string{"localhost:1234"}),
      GenericValue::Of(std::vector<GenericValue>{}),
  };
  return vec_defaults;
}

const std::vector<GenericValue>& GameSettingsDefaults() {
  static const std::vector<GenericValue> vec_defaults = {
      GenericValue::Of(std::string{"Default"}),
      GenericValue::Of(true),
      GenericValue::Of(static_cast<std::int64_t>(5)),
      GenericValue::Of(false),
      GenericValue::Of(static_cast<std::int64_t>(0)),
      GenericValue::Of(static_cast<std::int64_t>(0)),
      GenericValue::Of(30.0f),
      GenericValue::Of(50.0f),
      GenericValue::Of(0.04f),
      GenericValue::Of(static_cast<std::int64_t>(24)),
      GenericValue::Of(static_cast<std::int64_t>(8)),
      GenericValue::Of(false),
      GenericValue::Of(false),
      GenericValue::Of(static_cast<std::int64_t>(0)),
      GenericValue::Of(static_cast<std::int64_t>(0)),
      GenericValue::Of(false),
  };
  return vec_defaults;
}

constexpr meta::RecordDesc kPlayerSettingsDesc = {
    .str_name = "PlayerSettings",
    .str_full_name = "OpenRA.PlayerSettings",
    .str_base = "",
    .fields = kPlayerSettingsFields,
    .requires_types = {},
    .not_before_types = {},
    .interfaces = {},
};

constexpr meta::RecordDesc kGameSettingsDesc = {
    .str_name = "GameSettings",
    .str_full_name = "OpenRA.GameSettings",
    .str_base = "",
    .fields = kGameSettingsFields,
    .requires_types = {},
    .not_before_types = {},
    .interfaces = {},
};

}  // namespace

const meta::RecordDesc* SettingsDescOf(std::string_view str_section) {
  if (str_section == "Player")
    return &kPlayerSettingsDesc;
  if (str_section == "Game")
    return &kGameSettingsDesc;
  return nullptr;
}

// ———— SettingsModule ————

SettingsModule::SettingsModule(const meta::RecordDesc& desc,
                               std::string str_key, bool b_shared)
    : str_key_{std::move(str_key)}, b_shared_{b_shared} {
  const std::vector<GenericValue>* ptr_defaults = nullptr;
  if (&desc == &kPlayerSettingsDesc)
    ptr_defaults = &PlayerSettingsDefaults();
  else if (&desc == &kGameSettingsDesc)
    ptr_defaults = &GameSettingsDefaults();
  else {
    // 未知段:空字段承载(上游 Activator.CreateInstance 的缺省面)
    // An unknown section: the empty carrier (upstream's
    // Activator.CreateInstance default face).
    static const std::vector<GenericValue> vec_empty{};
    ptr_defaults = &vec_empty;
  }

  up_record_ = std::make_unique<meta::GeneratedRecord>(desc, *ptr_defaults);
  up_defaults_ = std::make_unique<meta::GeneratedRecord>(desc, *ptr_defaults);
}

void SettingsModule::Load(const yaml::MiniYaml& yaml_section) {
  // FieldLoader.Load(module, node.Value.Build())的值袋面(FieldLoader::Load
  // 主循环:节点键 → 字段槽)
  // The value-bag face of FieldLoader.Load (its main loop: the node keys →
  // the field slots).
  meta::Load(up_record_.get(), yaml_section);
}

std::vector<yaml::MiniYamlNode> SettingsModule::Commit(
    yaml::StringPool& pool) const {
  // Commit(L69-80):serialized != defaultSerialized 才存;键序 = 字段序
  // (差异集重建 = 上游的 RemoveAll + Add 序)
  // Commit (L69-80): saved only when serialized != defaultSerialized; the
  // key order = the field order (the difference rebuild = upstream's
  // RemoveAll + Add sequence).
  std::vector<yaml::MiniYamlNode> vec_nodes;
  const std::vector<const FieldDesc*> vec_fields =
      meta::CollectFields(up_record_->record_desc());
  for (std::size_t i = 0; i < vec_fields.size(); ++i) {
    const FieldDesc& desc = *vec_fields[i];
    const std::string str_serialized =
        meta::SaveFormatValue(desc, up_record_->Slot(i));
    const std::string str_default =
        meta::SaveFormatValue(desc, up_defaults_->Slot(i));
    if (str_serialized != str_default)
      vec_nodes.emplace_back(
          pool.Intern(desc.str_name),
          yaml::MiniYaml{pool.Intern(str_serialized)});
  }

  return vec_nodes;
}

// ———— Settings ————

Settings::Settings() {
  // 上游 [YamlNode("Player"/"Game")] 两段(shared: true)
  // Upstream's [YamlNode("Player"/"Game")] sections (shared: true).
  if (const meta::RecordDesc* ptr_desc = SettingsDescOf("Player"))
    vec_modules_.push_back(
        std::make_unique<SettingsModule>(*ptr_desc, "Player", true));
  if (const meta::RecordDesc* ptr_desc = SettingsDescOf("Game"))
    vec_modules_.push_back(
        std::make_unique<SettingsModule>(*ptr_desc, "Game", true));
}

void Settings::Load(const std::vector<yaml::MiniYamlNode>& vec_nodes,
                    yaml::StringPool& pool) {
  for (const auto& node : vec_nodes) {
    if (node.Key == nullptr)
      continue;
    for (auto& module : vec_modules_)
      if (*node.Key == module->Key())
        module->Load(node.Value);
    (void)pool;
  }
}

std::vector<yaml::MiniYamlNode> Settings::Save(
    yaml::StringPool& pool) const {
  std::vector<yaml::MiniYamlNode> vec_nodes;
  for (const auto& module : vec_modules_)
    vec_nodes.emplace_back(
        pool.Intern(module->Key()),
        yaml::MiniYaml{nullptr, module->Commit(pool)});
  return vec_nodes;
}

SettingsModule* Settings::Module(std::string_view str_key) {
  for (auto& module : vec_modules_)
    if (module->Key() == str_key)
      return module.get();
  return nullptr;
}

const SettingsModule* Settings::Module(std::string_view str_key) const {
  for (const auto& module : vec_modules_)
    if (module->Key() == str_key)
      return module.get();
  return nullptr;
}

}  // namespace ora::game
