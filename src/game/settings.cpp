// UPSTREAM: OpenRA.Game/Settings.cs @e180e3225 L52-340(实现部分;头注见
//          settings.hpp)
//          The implementation half of Settings.cs L52-340 (the header note
//          lives in settings.hpp).
#include "game/settings.hpp"

#include "meta/dump_format.hpp"
#include "meta/field_loader.hpp"
#include "meta/field_saver.hpp"
#include "meta/parse.hpp"

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

/// GraphicSettings 字段表(Settings.cs L237-280;全字段;枚举经本文件的
/// WindowMode/GLProfile/WorldViewport 注册表)
/// The GraphicSettings field table (Settings.cs L237-280; every field; the
/// enums go through this file's WindowMode/GLProfile/WorldViewport registry
/// entries).
constexpr FieldDesc kGraphicSettingsFields[] = {
    {.str_name = "Mode", .type = FieldType::Enum, .b_required = false,
     .str_loader = {}, .str_type_name = "OpenRA.WindowMode"},
    {.str_name = "VSync", .type = FieldType::Bool, .b_required = false},
    {.str_name = "FullscreenSize", .type = FieldType::Int2, .b_required = false},
    {.str_name = "WindowedSize", .type = FieldType::Int2, .b_required = false},
    {.str_name = "CursorDouble", .type = FieldType::Bool, .b_required = false},
    {.str_name = "ViewportDistance", .type = FieldType::Enum, .b_required = false,
     .str_type_name = "OpenRA.WorldViewport"},
    {.str_name = "UIScale", .type = FieldType::Float, .b_required = false},
    {.str_name = "CapFramerate", .type = FieldType::Bool, .b_required = false},
    {.str_name = "MaxFramerate", .type = FieldType::Int32, .b_required = false},
    {.str_name = "CapFramerateToGameFps", .type = FieldType::Bool, .b_required = false},
    {.str_name = "DisableGLDebugMessageCallback", .type = FieldType::Bool,
     .b_required = false},
    {.str_name = "DisableHardwareCursors", .type = FieldType::Bool, .b_required = false},
    {.str_name = "VideoDisplay", .type = FieldType::Int32, .b_required = false},
    {.str_name = "GLProfile", .type = FieldType::Enum, .b_required = false,
     .str_type_name = "OpenRA.GLProfile"},
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

/// GraphicSettings 默认值(Settings.cs L237-280 字面量;枚举槽 = 底层值:
/// WindowMode.PseudoFullscreen=2 / WorldViewport.Medium=2 / GLProfile.
/// Automatic=0)
/// The GraphicSettings defaults (Settings.cs L237-280's literals; enum slots
/// = the underlying values: WindowMode.PseudoFullscreen=2 /
/// WorldViewport.Medium=2 / GLProfile.Automatic=0).
const std::vector<GenericValue>& GraphicSettingsDefaults() {
  const auto fn_tuple2 = [](std::int64_t int8_a, std::int64_t int8_b) {
    meta::GenericTuple t{};
    t.arr_ints[0] = int8_a;
    t.arr_ints[1] = int8_b;
    t.uint1_count = 2;
    return GenericValue::Of(t);
  };
  static const std::vector<GenericValue> vec_defaults = {
      GenericValue::Of(static_cast<std::int64_t>(2)),
      GenericValue::Of(true),
      fn_tuple2(0, 0),
      fn_tuple2(1024, 768),
      GenericValue::Of(false),
      GenericValue::Of(static_cast<std::int64_t>(2)),
      GenericValue::Of(1.0f),
      GenericValue::Of(false),
      GenericValue::Of(static_cast<std::int64_t>(60)),
      GenericValue::Of(false),
      GenericValue::Of(false),
      GenericValue::Of(false),
      GenericValue::Of(static_cast<std::int64_t>(0)),
      GenericValue::Of(static_cast<std::int64_t>(0)),
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

/// DebugSettings 字段表(Settings.cs L188-234;全字段手写 —— schema_dumper
/// 未导出 Settings 扩展,同 Player/Game/Graphics 段先例)
/// The DebugSettings field table (Settings.cs L188-234; every field
/// hand-written — schema_dumper exports no Settings extension, the
/// Player/Game/Graphics precedent).
constexpr FieldDesc kDebugSettingsFields[] = {
    {.str_name = "PerfText", .type = FieldType::Bool, .b_required = false},
    {.str_name = "PerfGraph", .type = FieldType::Bool, .b_required = false},
    {.str_name = "Samples", .type = FieldType::Int32, .b_required = false},
    {.str_name = "CheckVersion", .type = FieldType::Bool, .b_required = false},
    {.str_name = "SendSystemInformation", .type = FieldType::Bool,
     .b_required = false},
    {.str_name = "SystemInformationVersionPrompt", .type = FieldType::Int32,
     .b_required = false},
    {.str_name = "UUID", .type = FieldType::String, .b_required = false},
    {.str_name = "DisplayDeveloperSettings", .type = FieldType::Bool,
     .b_required = false},
    {.str_name = "BotDebug", .type = FieldType::Bool, .b_required = false},
    {.str_name = "LuaDebug", .type = FieldType::Bool, .b_required = false},
    {.str_name = "EnableDebugCommandsInReplays", .type = FieldType::Bool,
     .b_required = false},
    {.str_name = "EnableSimulationPerfLogging", .type = FieldType::Bool,
     .b_required = false},
    {.str_name = "LongTickThresholdMs", .type = FieldType::Float,
     .b_required = false},
    {.str_name = "SyncCheckUnsyncedCode", .type = FieldType::Bool,
     .b_required = false},
    {.str_name = "SyncCheckBotModuleCode", .type = FieldType::Bool,
     .b_required = false},
};

constexpr meta::RecordDesc kGraphicSettingsDesc = {
    .str_name = "GraphicSettings",
    .str_full_name = "OpenRA.GraphicSettings",
    .str_base = "",
    .fields = kGraphicSettingsFields,
    .requires_types = {},
    .not_before_types = {},
    .interfaces = {},
};

/// DebugSettings 默认值(Settings.cs L188-234 字面量;UUID 的 Guid.NewGuid()
/// → 进程级首构造随机 128 位 hex)
/// The DebugSettings defaults (Settings.cs L188-234's literals; UUID's
/// Guid.NewGuid() becomes a random 128-bit hex minted once per process).
const std::vector<GenericValue>& DebugSettingsDefaults() {
  static const std::string str_uuid = [] {
    std::array<char, 36> arr_out{};
    std::random_device device_random;
    const auto fn_hex = [&](std::size_t sz_from, std::size_t sz_count) {
      for (std::size_t sz_i = 0; sz_i < sz_count; ++sz_i) {
        const std::uint32_t uint4_nibble = device_random() & 0xF;
        arr_out[sz_from + sz_i] =
            static_cast<char>(uint4_nibble < 10 ? '0' + uint4_nibble : 'a' + uint4_nibble - 10);
      }
    };
    fn_hex(0, 8);
    arr_out[8] = '-';
    fn_hex(9, 4);
    arr_out[13] = '-';
    fn_hex(14, 4);
    arr_out[18] = '-';
    fn_hex(19, 4);
    arr_out[23] = '-';
    fn_hex(24, 12);
    return std::string(arr_out.data(), arr_out.size());
  }();
  static const std::vector<GenericValue> vec_defaults = {
      GenericValue::Of(false),
      GenericValue::Of(false),
      GenericValue::Of(static_cast<std::int64_t>(25)),
      GenericValue::Of(true),
      GenericValue::Of(true),
      GenericValue::Of(static_cast<std::int64_t>(0)),
      GenericValue::Of(str_uuid),
      GenericValue::Of(false),
      GenericValue::Of(false),
      GenericValue::Of(false),
      GenericValue::Of(false),
      GenericValue::Of(false),
      GenericValue::Of(1.0f),
      GenericValue::Of(false),
      GenericValue::Of(false),
  };
  return vec_defaults;
}

constexpr meta::RecordDesc kDebugSettingsDesc = {
    .str_name = "DebugSettings",
    .str_full_name = "OpenRA.DebugSettings",
    .str_base = "",
    .fields = kDebugSettingsFields,
    .requires_types = {},
    .not_before_types = {},
    .interfaces = {},
};

/// Settings 段的三枚举(PlatformInterfaces.cs L18-24 的 GLProfile/WindowMode
/// 与 Settings.cs L50 的 WorldViewport;schema_dumper 未导出 —— 引擎核心
/// 消费域手写注册)
/// The Graphics section's three enums (PlatformInterfaces.cs L18-24's
/// GLProfile/WindowMode and Settings.cs L50's WorldViewport; not exported by
/// schema_dumper — hand-registered for the engine-core consumer domain).
constexpr meta::EnumMemberDesc kEnumWindowMode[] = {
    {.str_name = "Windowed", .int4_value = 0},
    {.str_name = "Fullscreen", .int4_value = 1},
    {.str_name = "PseudoFullscreen", .int4_value = 2},
};
constexpr meta::EnumMemberDesc kEnumGLProfile[] = {
    {.str_name = "Automatic", .int4_value = 0},
    {.str_name = "ANGLE", .int4_value = 1},
    {.str_name = "Modern", .int4_value = 2},
    {.str_name = "Embedded", .int4_value = 3},
};
constexpr meta::EnumMemberDesc kEnumWorldViewport[] = {
    {.str_name = "Native", .int4_value = 0},
    {.str_name = "Close", .int4_value = 1},
    {.str_name = "Medium", .int4_value = 2},
    {.str_name = "Far", .int4_value = 3},
};

void RegisterGraphicSettingsEnums() {
  meta::RegisterEnum("OpenRA.WindowMode", kEnumWindowMode);
  meta::RegisterEnum("OpenRA.GLProfile", kEnumGLProfile);
  meta::RegisterEnum("OpenRA.WorldViewport", kEnumWorldViewport);
}

}  // namespace

const meta::RecordDesc* SettingsDescOf(std::string_view str_section) {
  if (str_section == "Player")
    return &kPlayerSettingsDesc;
  if (str_section == "Game")
    return &kGameSettingsDesc;
  if (str_section == "Graphics")
    return &kGraphicSettingsDesc;
  if (str_section == "Debug")
    return &kDebugSettingsDesc;
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
  else if (&desc == &kGraphicSettingsDesc) {
    RegisterGraphicSettingsEnums();
    ptr_defaults = &GraphicSettingsDefaults();
  } else if (&desc == &kDebugSettingsDesc)
    ptr_defaults = &DebugSettingsDefaults();
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
  // 上游 [YamlNode("Player"/"Game"/"Graphics")] 三段(shared: true)
  // Upstream's [YamlNode("Player"/"Game"/"Graphics")] sections (shared:
  // true).
  if (const meta::RecordDesc* ptr_desc = SettingsDescOf("Player"))
    vec_modules_.push_back(
        std::make_unique<SettingsModule>(*ptr_desc, "Player", true));
  if (const meta::RecordDesc* ptr_desc = SettingsDescOf("Game"))
    vec_modules_.push_back(
        std::make_unique<SettingsModule>(*ptr_desc, "Game", true));
  if (const meta::RecordDesc* ptr_desc = SettingsDescOf("Graphics"))
    vec_modules_.push_back(
        std::make_unique<SettingsModule>(*ptr_desc, "Graphics", true));
  if (const meta::RecordDesc* ptr_desc = SettingsDescOf("Debug"))
    vec_modules_.push_back(
        std::make_unique<SettingsModule>(*ptr_desc, "Debug", true));
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
