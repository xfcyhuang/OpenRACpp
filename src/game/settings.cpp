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

constexpr FieldDesc kGameSettingsFields[] = {
    {.str_name = "Platform", .type = FieldType::String},
    {.str_name = "ViewportEdgeScroll", .type = FieldType::Bool},
    {.str_name = "ViewportEdgeScrollMargin", .type = FieldType::Int32},
    {.str_name = "LockMouseWindow", .type = FieldType::Bool},
    {.str_name = "MouseControlStyle", .type = FieldType::Enum,
     .str_type_name = "OpenRA.MouseControlStyle"},
    {.str_name = "MouseScroll", .type = FieldType::Enum,
     .str_type_name = "OpenRA.MouseScrollType"},
    {.str_name = "ViewportEdgeScrollStep", .type = FieldType::Float},
    {.str_name = "UIScrollSpeed", .type = FieldType::Float},
    {.str_name = "ZoomSpeed", .type = FieldType::Float},
    {.str_name = "SelectionDeadzone", .type = FieldType::Int32},
    {.str_name = "MouseScrollDeadzone", .type = FieldType::Int32},
    {.str_name = "UseAlternateScrollButton", .type = FieldType::Bool},
    {.str_name = "HideReplayChat", .type = FieldType::Bool},
    {.str_name = "StatusBars", .type = FieldType::Enum,
     .str_type_name = "OpenRA.StatusBarsType"},
    {.str_name = "TargetLines", .type = FieldType::Enum,
     .str_type_name = "OpenRA.TargetLinesType"},
    {.str_name = "UsePlayerStanceColors", .type = FieldType::Bool},
    {.str_name = "AllowDownloading", .type = FieldType::Bool},
    {.str_name = "AuthProfile", .type = FieldType::String},
    {.str_name = "ZoomModifier", .type = FieldType::Enum,
     .str_type_name = "OpenRA.Modifiers"},
    {.str_name = "FetchNews", .type = FieldType::Bool},
    {.str_name = "IntroductionPromptVersion", .type = FieldType::Int32},
    {.str_name = "MPGameFilters", .type = FieldType::Enum,
     .str_type_name = "OpenRA.MPGameFilters"},
    {.str_name = "PauseShellmap", .type = FieldType::Bool},
    {.str_name = "EnableDiscordService", .type = FieldType::Bool},
    {.str_name = "TextNotificationPoolFilters", .type = FieldType::Enum,
     .str_type_name = "OpenRA.TextNotificationPoolFilters"},
};

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

const std::vector<GenericValue>& PlayerSettingsDefaults() {
  static const std::vector<GenericValue> vec_defaults = {
      GenericValue::Of(std::string{"Commander"}),

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
      GenericValue::Of(static_cast<std::int64_t>(1)),
      GenericValue::Of(static_cast<std::int64_t>(3)),
      GenericValue::Of(30.0f),
      GenericValue::Of(50.0f),
      GenericValue::Of(0.04f),
      GenericValue::Of(static_cast<std::int64_t>(24)),
      GenericValue::Of(static_cast<std::int64_t>(8)),
      GenericValue::Of(false),
      GenericValue::Of(false),
      GenericValue::Of(static_cast<std::int64_t>(0)),
      GenericValue::Of(static_cast<std::int64_t>(1)),
      GenericValue::Of(false),
      GenericValue::Of(true),
      GenericValue::Of(std::string{"player.oraid"}),
      GenericValue::Of(static_cast<std::int64_t>(0)),
      GenericValue::Of(true),
      GenericValue::Of(static_cast<std::int64_t>(0)),
      GenericValue::Of(static_cast<std::int64_t>(15)),
      GenericValue::Of(false),
      GenericValue::Of(true),
      GenericValue::Of(static_cast<std::int64_t>(3)),
  };
  return vec_defaults;
}

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

constexpr FieldDesc kSoundSettingsFields[] = {
    {.str_name = "SoundVolume", .type = FieldType::Float, .b_required = false},
    {.str_name = "MusicVolume", .type = FieldType::Float, .b_required = false},
    {.str_name = "VideoVolume", .type = FieldType::Float, .b_required = false},
    {.str_name = "Shuffle", .type = FieldType::Bool, .b_required = false},
    {.str_name = "Repeat", .type = FieldType::Bool, .b_required = false},
    {.str_name = "Device", .type = FieldType::String, .b_required = false},
    {.str_name = "CashTicks", .type = FieldType::Bool, .b_required = false},
    {.str_name = "Mute", .type = FieldType::Bool, .b_required = false},
    {.str_name = "MuteBackgroundMusic", .type = FieldType::Bool,
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

const std::vector<GenericValue>& SoundSettingsDefaults() {
  static const std::vector<GenericValue> vec_defaults = {
      GenericValue::Of(0.5f),
      GenericValue::Of(0.5f),
      GenericValue::Of(0.5f),
      GenericValue::Of(false),
      GenericValue::Of(false),
      GenericValue::Of(std::string{}),
      GenericValue::Of(true),
      GenericValue::Of(false),
      GenericValue::Of(false),
  };
  return vec_defaults;
}

constexpr meta::RecordDesc kSoundSettingsDesc = {
    .str_name = "SoundSettings",
    .str_full_name = "OpenRA.SoundSettings",
    .str_base = "",
    .fields = kSoundSettingsFields,
    .requires_types = {},
    .not_before_types = {},
    .interfaces = {},
};

constexpr meta::RecordDesc kDebugSettingsDesc = {
    .str_name = "DebugSettings",
    .str_full_name = "OpenRA.DebugSettings",
    .str_base = "",
    .fields = kDebugSettingsFields,
    .requires_types = {},
    .not_before_types = {},
    .interfaces = {},
};

constexpr FieldDesc kServerSettingsFields[] = {
    {.str_name = "Name", .type = FieldType::String, .b_required = false},
    {.str_name = "ListenPort", .type = FieldType::Int32, .b_required = false},
    {.str_name = "AdvertiseOnline", .type = FieldType::Bool, .b_required = false},
    {.str_name = "AdvertiseOnLocalNetwork", .type = FieldType::Bool,
     .b_required = false},
    {.str_name = "Password", .type = FieldType::String, .b_required = false},
    {.str_name = "DiscoverNatDevices", .type = FieldType::Bool,
     .b_required = false},
    {.str_name = "NatPortMappingLifetime", .type = FieldType::Int32,
     .b_required = false},
    {.str_name = "Map", .type = FieldType::String, .b_required = false},
    {.str_name = "Ban", .type = FieldType::ImmutableArray, .b_required = false},
    {.str_name = "RequireAuthentication", .type = FieldType::Bool,
     .b_required = false},
    {.str_name = "ProfileIDWhitelist", .type = FieldType::ImmutableArray,
     .b_required = false},
    {.str_name = "ProfileIDBlacklist", .type = FieldType::ImmutableArray,
     .b_required = false},
    {.str_name = "EnableSingleplayer", .type = FieldType::Bool,
     .b_required = false},
    {.str_name = "EnableMapGeneration", .type = FieldType::Bool,
     .b_required = false},
    {.str_name = "QueryMapRepository", .type = FieldType::Bool,
     .b_required = false},
    {.str_name = "EnableSyncReports", .type = FieldType::Bool,
     .b_required = false},
    {.str_name = "TimestampFormat", .type = FieldType::String,
     .b_required = false},
    {.str_name = "ShareAnonymizedIPs", .type = FieldType::Bool,
     .b_required = false},
    {.str_name = "EnableGeoIP", .type = FieldType::Bool, .b_required = false},
    {.str_name = "RecordReplays", .type = FieldType::Bool, .b_required = false},
    {.str_name = "EnableLintChecks", .type = FieldType::Bool,
     .b_required = false},
    {.str_name = "MapPool", .type = FieldType::ImmutableArray,
     .b_required = false},
    {.str_name = "FloodLimitJoinCooldown", .type = FieldType::Int32,
     .b_required = false},
    {.str_name = "FloodLimitInterval", .type = FieldType::Int32,
     .b_required = false},
    {.str_name = "FloodLimitMessageCount", .type = FieldType::Int32,
     .b_required = false},
    {.str_name = "FloodLimitCooldown", .type = FieldType::Int32,
     .b_required = false},
    {.str_name = "EnableVoteKick", .type = FieldType::Bool, .b_required = false},
    {.str_name = "VoteKickTimer", .type = FieldType::Int32, .b_required = false},
    {.str_name = "VoteKickerCooldown", .type = FieldType::Int32,
     .b_required = false},
};

const std::vector<GenericValue>& ServerSettingsDefaults() {
  static const std::vector<GenericValue> vec_defaults = {
      GenericValue::Of(std::string{}),
      GenericValue::Of(static_cast<std::int64_t>(1234)),
      GenericValue::Of(true),
      GenericValue::Of(true),
      GenericValue::Of(std::string{}),
      GenericValue::Of(false),
      GenericValue::Of(static_cast<std::int64_t>(36000)),
      GenericValue::Of(std::string{}),
      GenericValue::Of(std::vector<GenericValue>{}),
      GenericValue::Of(false),
      GenericValue::Of(std::vector<GenericValue>{}),
      GenericValue::Of(std::vector<GenericValue>{}),
      GenericValue::Of(false),
      GenericValue::Of(true),
      GenericValue::Of(true),
      GenericValue::Of(false),
      GenericValue::Of(std::string{"yyyy-MM-ddTHH:mm:ss"}),
      GenericValue::Of(true),
      GenericValue::Of(true),
      GenericValue::Of(false),
      GenericValue::Of(true),
      GenericValue::Of(std::vector<GenericValue>{}),
      GenericValue::Of(static_cast<std::int64_t>(5000)),
      GenericValue::Of(static_cast<std::int64_t>(5000)),
      GenericValue::Of(static_cast<std::int64_t>(5)),
      GenericValue::Of(static_cast<std::int64_t>(15000)),
      GenericValue::Of(true),
      GenericValue::Of(static_cast<std::int64_t>(30000)),
      GenericValue::Of(static_cast<std::int64_t>(120000)),
  };
  return vec_defaults;
}

constexpr meta::RecordDesc kServerSettingsDesc = {
    .str_name = "ServerSettings",
    .str_full_name = "OpenRA.ServerSettings",
    .str_base = "",
    .fields = kServerSettingsFields,
    .requires_types = {},
    .not_before_types = {},
    .interfaces = {},
};

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

constexpr meta::EnumMemberDesc kEnumMouseControlStyle[] = {
    {.str_name = "Classic", .int4_value = 0},
    {.str_name = "Modern", .int4_value = 1},
    {.str_name = "OtherRTS", .int4_value = 2},
};
constexpr meta::EnumMemberDesc kEnumMouseScrollType[] = {
    {.str_name = "Disabled", .int4_value = 0},
    {.str_name = "Standard", .int4_value = 1},
    {.str_name = "Inverted", .int4_value = 2},
    {.str_name = "Joystick", .int4_value = 3},
};
constexpr meta::EnumMemberDesc kEnumStatusBarsType[] = {
    {.str_name = "Standard", .int4_value = 0},
    {.str_name = "DamageShow", .int4_value = 1},
    {.str_name = "AlwaysShow", .int4_value = 2},
};
constexpr meta::EnumMemberDesc kEnumTargetLinesType[] = {
    {.str_name = "Disabled", .int4_value = 0},
    {.str_name = "Manual", .int4_value = 1},
    {.str_name = "Automatic", .int4_value = 2},
};
constexpr meta::EnumMemberDesc kEnumMPGameFilters[] = {
    {.str_name = "None", .int4_value = 0},
    {.str_name = "Waiting", .int4_value = 1},
    {.str_name = "Empty", .int4_value = 2},
    {.str_name = "Protected", .int4_value = 4},
    {.str_name = "Started", .int4_value = 8},
    {.str_name = "Incompatible", .int4_value = 16},
};
constexpr meta::EnumMemberDesc kEnumModifiers[] = {
    {.str_name = "None", .int4_value = 0},
    {.str_name = "Shift", .int4_value = 1},
    {.str_name = "Alt", .int4_value = 2},
    {.str_name = "Ctrl", .int4_value = 4},
    {.str_name = "Meta", .int4_value = 8},
};
constexpr meta::EnumMemberDesc kEnumTextNotificationPoolFilters[] = {
    {.str_name = "None", .int4_value = 0},
    {.str_name = "Feedback", .int4_value = 1},
    {.str_name = "Transients", .int4_value = 2},
};

void RegisterGameSettingsEnums() {
  meta::RegisterEnum("OpenRA.MouseControlStyle", kEnumMouseControlStyle);
  meta::RegisterEnum("OpenRA.MouseScrollType", kEnumMouseScrollType);
  meta::RegisterEnum("OpenRA.StatusBarsType", kEnumStatusBarsType);
  meta::RegisterEnum("OpenRA.TargetLinesType", kEnumTargetLinesType);
  meta::RegisterEnum("OpenRA.MPGameFilters", kEnumMPGameFilters);
  meta::RegisterEnum("OpenRA.Modifiers", kEnumModifiers);
  meta::RegisterEnum("OpenRA.TextNotificationPoolFilters",
                     kEnumTextNotificationPoolFilters);
}

constexpr FieldDesc kAutoSaveSettingsFields[] = {
    {.str_name = "AutoSaveInterval", .type = FieldType::Int32,
     .b_required = false},
    {.str_name = "AutoSaveMaxFileCount", .type = FieldType::Int32,
     .b_required = false},
};

const std::vector<GenericValue>& AutoSaveSettingsDefaults() {
  static const std::vector<GenericValue> vec_defaults = {
      GenericValue::Of(static_cast<std::int64_t>(0)),
      GenericValue::Of(static_cast<std::int64_t>(10)),
  };
  return vec_defaults;
}

constexpr meta::RecordDesc kAutoSaveSettingsDesc = {
    .str_name = "AutoSaveSettings",
    .str_full_name = "OpenRA.Mods.Common.Traits.AutoSaveSettings",
    .str_base = "",
    .fields = kAutoSaveSettingsFields,
    .requires_types = {},
    .not_before_types = {},
    .interfaces = {},
};

}

const meta::RecordDesc* SettingsDescOf(std::string_view str_section) {
  if (str_section == "Player")
    return &kPlayerSettingsDesc;
  if (str_section == "Game")
    return &kGameSettingsDesc;
  if (str_section == "Graphics")
    return &kGraphicSettingsDesc;
  if (str_section == "Sound")
    return &kSoundSettingsDesc;
  if (str_section == "Debug")
    return &kDebugSettingsDesc;
  if (str_section == "Server")
    return &kServerSettingsDesc;
  if (str_section == "AutoSave")
    return &kAutoSaveSettingsDesc;
  return nullptr;
}

SettingsModule::SettingsModule(const meta::RecordDesc& desc,
                               std::string str_key, bool b_shared)
    : str_key_{std::move(str_key)}, b_shared_{b_shared} {
  const std::vector<GenericValue>* ptr_defaults = nullptr;
  if (&desc == &kPlayerSettingsDesc)
    ptr_defaults = &PlayerSettingsDefaults();
  else if (&desc == &kGameSettingsDesc) {
    RegisterGameSettingsEnums();
    ptr_defaults = &GameSettingsDefaults();
  } else if (&desc == &kGraphicSettingsDesc) {
    RegisterGraphicSettingsEnums();
    ptr_defaults = &GraphicSettingsDefaults();
  } else if (&desc == &kSoundSettingsDesc)
    ptr_defaults = &SoundSettingsDefaults();
  else if (&desc == &kDebugSettingsDesc)
    ptr_defaults = &DebugSettingsDefaults();
  else if (&desc == &kServerSettingsDesc)
    ptr_defaults = &ServerSettingsDefaults();
  else if (&desc == &kAutoSaveSettingsDesc)
    ptr_defaults = &AutoSaveSettingsDefaults();
  else {

    static const std::vector<GenericValue> vec_empty{};
    ptr_defaults = &vec_empty;
  }

  up_record_ = std::make_unique<meta::GeneratedRecord>(desc, *ptr_defaults);
  up_defaults_ = std::make_unique<meta::GeneratedRecord>(desc, *ptr_defaults);
}

void SettingsModule::Load(const yaml::MiniYaml& yaml_section) {

  meta::Load(up_record_.get(), yaml_section);
}

std::vector<yaml::MiniYamlNode> SettingsModule::Commit(
    yaml::StringPool& pool) const {

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

std::optional<std::size_t> SettingsModule::SlotIndexOf(
    std::string_view str_field) const {
  const std::vector<const FieldDesc*> vec_fields =
      meta::CollectFields(up_record_->record_desc());
  for (std::size_t sz_i = 0; sz_i < vec_fields.size(); ++sz_i)
    if (vec_fields[sz_i]->str_name == str_field)
      return sz_i;
  return std::nullopt;
}

meta::FieldType SettingsModule::SlotType(std::size_t sz_slot) const {
  const std::vector<const FieldDesc*> vec_fields =
      meta::CollectFields(up_record_->record_desc());
  return vec_fields.at(sz_slot)->type;
}

bool SettingsModule::GetBool(std::size_t sz_slot) const {
  return std::get<bool>(up_record_->Slot(sz_slot).val);
}

void SettingsModule::SetBool(std::size_t sz_slot, bool b_value) {
  up_record_->Slot(sz_slot) = GenericValue::Of(b_value);
}

float SettingsModule::GetFloat(std::size_t sz_slot) const {
  return std::get<float>(up_record_->Slot(sz_slot).val);
}

void SettingsModule::SetFloat(std::size_t sz_slot, float fp4_value) {
  up_record_->Slot(sz_slot) = GenericValue::Of(fp4_value);
}

std::int64_t SettingsModule::GetInt(std::size_t sz_slot) const {
  return std::get<std::int64_t>(up_record_->Slot(sz_slot).val);
}

void SettingsModule::SetInt(std::size_t sz_slot, std::int64_t int8_value) {
  up_record_->Slot(sz_slot) = GenericValue::Of(int8_value);
}

const std::string& SettingsModule::GetString(std::size_t sz_slot) const {
  return std::get<std::string>(up_record_->Slot(sz_slot).val);
}

void SettingsModule::SetString(std::size_t sz_slot, std::string str_value) {
  up_record_->Slot(sz_slot) = GenericValue::Of(std::move(str_value));
}

int2 SettingsModule::GetInt2(std::size_t sz_slot) const {
  const meta::GenericTuple& tuple =
      std::get<meta::GenericTuple>(up_record_->Slot(sz_slot).val);
  return int2{static_cast<std::int32_t>(tuple.arr_ints[0]),
              static_cast<std::int32_t>(tuple.arr_ints[1])};
}

void SettingsModule::SetInt2(std::size_t sz_slot, int2 int2_value) {
  meta::GenericTuple tuple{};
  tuple.arr_ints[0] = int2_value.X;
  tuple.arr_ints[1] = int2_value.Y;
  tuple.uint1_count = 2;
  up_record_->Slot(sz_slot) = GenericValue::Of(std::move(tuple));
}

const meta::GenericValue& SettingsModule::DefaultSlot(
    std::size_t sz_slot) const {
  return up_defaults_->Slot(sz_slot);
}

bool SettingsModule::DefaultBool(std::size_t sz_slot) const {
  return std::get<bool>(up_defaults_->Slot(sz_slot).val);
}

float SettingsModule::DefaultFloat(std::size_t sz_slot) const {
  return std::get<float>(up_defaults_->Slot(sz_slot).val);
}

std::int64_t SettingsModule::DefaultInt(std::size_t sz_slot) const {
  return std::get<std::int64_t>(up_defaults_->Slot(sz_slot).val);
}

const std::string& SettingsModule::DefaultString(
    std::size_t sz_slot) const {
  return std::get<std::string>(up_defaults_->Slot(sz_slot).val);
}

meta::GeneratedRecord SettingsModule::CloneRecord() const {
  return *up_record_;
}

namespace {

yaml::MiniYaml CopyYamlDeep(const yaml::MiniYaml& yaml_src,
                            std::deque<std::string>& deque_strings) {
  const std::string* ptr_value = nullptr;
  if (yaml_src.Value != nullptr) {
    deque_strings.emplace_back(*yaml_src.Value);
    ptr_value = &deque_strings.back();
  }
  std::vector<yaml::MiniYamlNode> vec_nodes;
  vec_nodes.reserve(yaml_src.Nodes.size());
  for (const auto& node : yaml_src.Nodes) {
    deque_strings.emplace_back(node.Key != nullptr ? *node.Key
                                                   : std::string{});
    const std::string* ptr_key = &deque_strings.back();
    vec_nodes.emplace_back(ptr_key, CopyYamlDeep(node.Value, deque_strings),
                           node.Comment);
  }
  return yaml::MiniYaml{ptr_value, std::move(vec_nodes)};
}

yaml::MiniYaml InternYamlDeep(const yaml::MiniYaml& yaml_src,
                              yaml::StringPool& pool) {
  const std::string* ptr_value = nullptr;
  if (yaml_src.Value != nullptr)
    ptr_value = pool.Intern(*yaml_src.Value);
  std::vector<yaml::MiniYamlNode> vec_nodes;
  vec_nodes.reserve(yaml_src.Nodes.size());
  for (const auto& node : yaml_src.Nodes)
    vec_nodes.emplace_back(
        pool.Intern(node.Key != nullptr ? std::string_view{*node.Key}
                                        : std::string_view{}),
        InternYamlDeep(node.Value, pool), node.Comment);
  return yaml::MiniYaml{ptr_value, std::move(vec_nodes)};
}

}

void RawSettingsSection::Load(const yaml::MiniYaml& yaml_section) {
  vec_nodes_.clear();
  vec_nodes_.reserve(yaml_section.Nodes.size());
  for (const auto& node : yaml_section.Nodes) {
    deque_strings_.emplace_back(node.Key != nullptr ? *node.Key
                                                    : std::string{});
    const std::string* ptr_key = &deque_strings_.back();
    vec_nodes_.emplace_back(ptr_key, CopyYamlDeep(node.Value, deque_strings_),
                            node.Comment);
  }
}

std::vector<yaml::MiniYamlNode> RawSettingsSection::Commit(
    yaml::StringPool& pool) const {
  std::vector<yaml::MiniYamlNode> vec_nodes;
  vec_nodes.reserve(vec_nodes_.size());
  for (const auto& node : vec_nodes_)
    vec_nodes.emplace_back(
        pool.Intern(node.Key != nullptr ? std::string_view{*node.Key}
                                        : std::string_view{}),
        InternYamlDeep(node.Value, pool), node.Comment);
  return vec_nodes;
}

void RawSettingsSection::RemoveNodesNamed(std::string_view str_name) {
  std::erase_if(vec_nodes_, [&](const yaml::MiniYamlNode& node) {
    return node.Key != nullptr && *node.Key == str_name;
  });
}

void RawSettingsSection::AddNode(const std::string& str_name,
                                 const std::string& str_value) {
  deque_strings_.emplace_back(str_name);
  const std::string* ptr_key = &deque_strings_.back();
  deque_strings_.emplace_back(str_value);
  vec_nodes_.emplace_back(ptr_key, yaml::MiniYaml{&deque_strings_.back()});
}

namespace {

std::unique_ptr<Settings>& InstanceSlot() {
  static std::unique_ptr<Settings> up_settings = std::make_unique<Settings>();
  return up_settings;
}

}

Settings& Settings::Instance() {
  return *InstanceSlot();
}

void Settings::ResetInstanceForTest() {
  InstanceSlot() = std::make_unique<Settings>();
}

Settings::Settings() {

  if (const meta::RecordDesc* ptr_desc = SettingsDescOf("Player"))
    vec_modules_.push_back(
        std::make_unique<SettingsModule>(*ptr_desc, "Player", true));
  if (const meta::RecordDesc* ptr_desc = SettingsDescOf("Game"))
    vec_modules_.push_back(
        std::make_unique<SettingsModule>(*ptr_desc, "Game", true));
  if (const meta::RecordDesc* ptr_desc = SettingsDescOf("Graphics"))
    vec_modules_.push_back(
        std::make_unique<SettingsModule>(*ptr_desc, "Graphics", true));
  if (const meta::RecordDesc* ptr_desc = SettingsDescOf("Sound"))
    vec_modules_.push_back(
        std::make_unique<SettingsModule>(*ptr_desc, "Sound", true));
  if (const meta::RecordDesc* ptr_desc = SettingsDescOf("Debug"))
    vec_modules_.push_back(
        std::make_unique<SettingsModule>(*ptr_desc, "Debug", true));
  if (const meta::RecordDesc* ptr_desc = SettingsDescOf("Server"))
    vec_modules_.push_back(
        std::make_unique<SettingsModule>(*ptr_desc, "Server", true));
}

void Settings::Load(const std::vector<yaml::MiniYamlNode>& vec_nodes,
                    yaml::StringPool& pool) {
  for (const auto& node : vec_nodes) {
    if (node.Key == nullptr)
      continue;
    bool b_consumed = false;
    for (auto& module : vec_modules_)
      if (*node.Key == module->Key()) {
        module->Load(node.Value);
        b_consumed = true;
      }
    for (auto& section : vec_raw_sections_)
      if (*node.Key == section->Key()) {
        section->Load(node.Value);
        b_consumed = true;
      }
    if (!b_consumed)
      vec_pending_nodes_.push_back(node);
    (void)pool;
  }
}

std::vector<yaml::MiniYamlNode> Settings::Save(
    yaml::StringPool& pool) const {
  std::vector<yaml::MiniYamlNode> vec_nodes;
  for (const auto& module : vec_modules_) {
    std::vector<yaml::MiniYamlNode> vec_section = module->Commit(pool);
    if (vec_section.empty())
      continue;
    vec_nodes.emplace_back(
        pool.Intern(module->Key()),
        yaml::MiniYaml{nullptr, std::move(vec_section)});
  }
  for (const auto& section : vec_raw_sections_) {
    std::vector<yaml::MiniYamlNode> vec_section = section->Commit(pool);
    if (vec_section.empty())
      continue;
    vec_nodes.emplace_back(
        pool.Intern(section->Key()),
        yaml::MiniYaml{nullptr, std::move(vec_section)});
  }
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

SettingsModule* Settings::ModuleOrCreate(std::string_view str_key) {
  if (SettingsModule* ptr_module = Module(str_key))
    return ptr_module;

  const meta::RecordDesc* ptr_desc = SettingsDescOf(str_key);
  if (ptr_desc == nullptr)
    return nullptr;

  auto up_module = std::make_unique<SettingsModule>(*ptr_desc,
                                                    std::string{str_key},
                                                    true);
  for (const auto& node : vec_pending_nodes_) {
    if (node.Key != nullptr && *node.Key == str_key)
      up_module->Load(node.Value);
  }
  vec_modules_.push_back(std::move(up_module));
  return vec_modules_.back().get();
}

RawSettingsSection& Settings::RawSection(std::string_view str_key) {
  for (auto& section : vec_raw_sections_)
    if (section->Key() == str_key)
      return *section;

  auto up_section = std::make_unique<RawSettingsSection>(std::string{str_key});
  for (const auto& node : vec_pending_nodes_) {
    if (node.Key != nullptr && *node.Key == str_key)
      up_section->Load(node.Value);
  }
  vec_raw_sections_.push_back(std::move(up_section));
  return *vec_raw_sections_.back();
}

}
