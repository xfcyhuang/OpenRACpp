module;
#include <cctype>
#include <cstring>
module ora.game;

import std;
import ora.game.miniyaml;

namespace ora::game {

namespace {

enum class FieldTypeId {
  kBool,
  kInt,
  kFloat,
  kString,
  kColor,
  kInt2,
  kWindowMode,
  kMouseControlStyle,
  kMouseScrollType,
  kStatusBarsType,
  kTargetLinesType,
  kWorldViewport,
  kGlProfile,
  kModifiers,
  kMpGameFilters,
  kTextNotificationPoolFilters,
  kStringSet,
  kIntSet,
  kColorArray,
};

template <typename Ptr>
struct FieldEntry {
  std::string_view str_name;
  FieldTypeId ft;
  std::string_view str_dotnet_type;
  Ptr ptr_member;
};

constexpr std::string_view kTypeBool{"System.Boolean"};
constexpr std::string_view kTypeInt{"System.Int32"};
constexpr std::string_view kTypeFloat{"System.Single"};
constexpr std::string_view kTypeString{"System.String"};
constexpr std::string_view kTypeColor{"OpenRA.Primitives.Color"};
constexpr std::string_view kTypeInt2{"OpenRA.int2"};
constexpr std::string_view kTypeWindowMode{"OpenRA.WindowMode"};
constexpr std::string_view kTypeMouseControlStyle{"OpenRA.MouseControlStyle"};
constexpr std::string_view kTypeMouseScrollType{"OpenRA.MouseScrollType"};
constexpr std::string_view kTypeStatusBarsType{"OpenRA.StatusBarsType"};
constexpr std::string_view kTypeTargetLinesType{"OpenRA.TargetLinesType"};
constexpr std::string_view kTypeWorldViewport{"OpenRA.WorldViewport"};
constexpr std::string_view kTypeGlProfile{"OpenRA.GLProfile"};
constexpr std::string_view kTypeModifiers{"OpenRA.Modifiers"};
constexpr std::string_view kTypeMpGameFilters{"OpenRA.MPGameFilters"};
constexpr std::string_view kTypeTextNotificationPoolFilters{"OpenRA.TextNotificationPoolFilters"};
constexpr std::string_view kTypeStringSet{"System.Collections.Frozen.FrozenSet`1[System.String]"};
constexpr std::string_view kTypeIntSet{"System.Collections.Frozen.FrozenSet`1[System.Int32]"};
constexpr std::string_view kTypeColorArray{
    "System.Collections.Immutable.ImmutableArray`1[OpenRA.Primitives.Color]"};

struct EnumMember {
  int int4_value;
  std::string_view str_name;
};

constexpr EnumMember kMembersWindowMode[]{
    {0, "Windowed"}, {1, "Fullscreen"}, {2, "PseudoFullscreen"}};
constexpr EnumMember kMembersMouseControlStyle[]{
    {0, "Classic"}, {1, "Modern"}, {2, "OtherRTS"}};
constexpr EnumMember kMembersMouseScrollType[]{
    {0, "Disabled"}, {1, "Standard"}, {2, "Inverted"}, {3, "Joystick"}};
constexpr EnumMember kMembersStatusBarsType[]{
    {0, "Standard"}, {1, "DamageShow"}, {2, "AlwaysShow"}};
constexpr EnumMember kMembersTargetLinesType[]{
    {0, "Disabled"}, {1, "Manual"}, {2, "Automatic"}};
constexpr EnumMember kMembersWorldViewport[]{
    {0, "Native"}, {1, "Close"}, {2, "Medium"}, {3, "Far"}};
constexpr EnumMember kMembersGlProfile[]{
    {0, "Automatic"}, {1, "ANGLE"}, {2, "Modern"}, {3, "Embedded"}};
constexpr EnumMember kMembersModifiers[]{
    {0, "None"}, {1, "Shift"}, {2, "Alt"}, {4, "Ctrl"}, {8, "Meta"}};
constexpr EnumMember kMembersMpGameFilters[]{
    {0, "None"}, {1, "Waiting"}, {2, "Empty"}, {4, "Protected"}, {8, "Started"},
    {16, "Incompatible"}};
constexpr EnumMember kMembersTextNotificationPoolFilters[]{
    {0, "None"}, {1, "Feedback"}, {2, "Transients"}};

std::span<const EnumMember> enum_members_of(FieldTypeId ft, bool& b_flags) {
  b_flags = false;
  switch (ft) {
    case FieldTypeId::kWindowMode:
      return kMembersWindowMode;
    case FieldTypeId::kMouseControlStyle:
      return kMembersMouseControlStyle;
    case FieldTypeId::kMouseScrollType:
      return kMembersMouseScrollType;
    case FieldTypeId::kStatusBarsType:
      return kMembersStatusBarsType;
    case FieldTypeId::kTargetLinesType:
      return kMembersTargetLinesType;
    case FieldTypeId::kWorldViewport:
      return kMembersWorldViewport;
    case FieldTypeId::kGlProfile:
      return kMembersGlProfile;
    case FieldTypeId::kModifiers:
      return kMembersModifiers;
    case FieldTypeId::kMpGameFilters:
      b_flags = true;
      return kMembersMpGameFilters;
    case FieldTypeId::kTextNotificationPoolFilters:
      b_flags = true;
      return kMembersTextNotificationPoolFilters;
    default:
      return {};
  }
}

constexpr auto kPlayerTable{std::tuple{
    FieldEntry{"Name", FieldTypeId::kString, kTypeString, &PlayerSettingsModel::str_name},
    FieldEntry{"Color", FieldTypeId::kColor, kTypeColor, &PlayerSettingsModel::color},
    FieldEntry{"LastServer", FieldTypeId::kString, kTypeString,
               &PlayerSettingsModel::str_last_server},
    FieldEntry{"CustomColors", FieldTypeId::kColorArray, kTypeColorArray,
               &PlayerSettingsModel::vec_custom_colors},
}};

constexpr auto kGameTable{std::tuple{
    FieldEntry{"Platform", FieldTypeId::kString, kTypeString, &GameSettingsModel::str_platform},
    FieldEntry{"ViewportEdgeScroll", FieldTypeId::kBool, kTypeBool,
               &GameSettingsModel::b_viewport_edge_scroll},
    FieldEntry{"ViewportEdgeScrollMargin", FieldTypeId::kInt, kTypeInt,
               &GameSettingsModel::int4_viewport_edge_scroll_margin},
    FieldEntry{"LockMouseWindow", FieldTypeId::kBool, kTypeBool,
               &GameSettingsModel::b_lock_mouse_window},
    FieldEntry{"MouseControlStyle", FieldTypeId::kMouseControlStyle, kTypeMouseControlStyle,
               &GameSettingsModel::mouse_control_style},
    FieldEntry{"MouseScroll", FieldTypeId::kMouseScrollType, kTypeMouseScrollType,
               &GameSettingsModel::mouse_scroll},
    FieldEntry{"ViewportEdgeScrollStep", FieldTypeId::kFloat, kTypeFloat,
               &GameSettingsModel::fp_viewport_edge_scroll_step},
    FieldEntry{"UIScrollSpeed", FieldTypeId::kFloat, kTypeFloat,
               &GameSettingsModel::fp_ui_scroll_speed},
    FieldEntry{"ZoomSpeed", FieldTypeId::kFloat, kTypeFloat, &GameSettingsModel::fp_zoom_speed},
    FieldEntry{"SelectionDeadzone", FieldTypeId::kInt, kTypeInt,
               &GameSettingsModel::int4_selection_deadzone},
    FieldEntry{"MouseScrollDeadzone", FieldTypeId::kInt, kTypeInt,
               &GameSettingsModel::int4_mouse_scroll_deadzone},
    FieldEntry{"UseAlternateScrollButton", FieldTypeId::kBool, kTypeBool,
               &GameSettingsModel::b_use_alternate_scroll_button},
    FieldEntry{"HideReplayChat", FieldTypeId::kBool, kTypeBool,
               &GameSettingsModel::b_hide_replay_chat},
    FieldEntry{"StatusBars", FieldTypeId::kStatusBarsType, kTypeStatusBarsType,
               &GameSettingsModel::status_bars},
    FieldEntry{"TargetLines", FieldTypeId::kTargetLinesType, kTypeTargetLinesType,
               &GameSettingsModel::target_lines},
    FieldEntry{"UsePlayerStanceColors", FieldTypeId::kBool, kTypeBool,
               &GameSettingsModel::b_use_player_stance_colors},
    FieldEntry{"AllowDownloading", FieldTypeId::kBool, kTypeBool,
               &GameSettingsModel::b_allow_downloading},
    FieldEntry{"AuthProfile", FieldTypeId::kString, kTypeString,
               &GameSettingsModel::str_auth_profile},
    FieldEntry{"ZoomModifier", FieldTypeId::kModifiers, kTypeModifiers,
               &GameSettingsModel::zoom_modifier},
    FieldEntry{"FetchNews", FieldTypeId::kBool, kTypeBool, &GameSettingsModel::b_fetch_news},
    FieldEntry{"IntroductionPromptVersion", FieldTypeId::kInt, kTypeInt,
               &GameSettingsModel::int4_introduction_prompt_version},
    FieldEntry{"MPGameFilters", FieldTypeId::kMpGameFilters, kTypeMpGameFilters,
               &GameSettingsModel::mp_game_filters},
    FieldEntry{"PauseShellmap", FieldTypeId::kBool, kTypeBool,
               &GameSettingsModel::b_pause_shellmap},
    FieldEntry{"EnableDiscordService", FieldTypeId::kBool, kTypeBool,
               &GameSettingsModel::b_enable_discord_service},
    FieldEntry{"TextNotificationPoolFilters", FieldTypeId::kTextNotificationPoolFilters,
               kTypeTextNotificationPoolFilters,
               &GameSettingsModel::text_notification_pool_filters},
}};

constexpr auto kSoundTable{std::tuple{
    FieldEntry{"SoundVolume", FieldTypeId::kFloat, kTypeFloat,
               &SoundSettingsModel::fp_sound_volume},
    FieldEntry{"MusicVolume", FieldTypeId::kFloat, kTypeFloat,
               &SoundSettingsModel::fp_music_volume},
    FieldEntry{"VideoVolume", FieldTypeId::kFloat, kTypeFloat,
               &SoundSettingsModel::fp_video_volume},
    FieldEntry{"Shuffle", FieldTypeId::kBool, kTypeBool, &SoundSettingsModel::b_shuffle},
    FieldEntry{"Repeat", FieldTypeId::kBool, kTypeBool, &SoundSettingsModel::b_repeat},
    FieldEntry{"Device", FieldTypeId::kString, kTypeString, &SoundSettingsModel::str_device},
    FieldEntry{"CashTicks", FieldTypeId::kBool, kTypeBool, &SoundSettingsModel::b_cash_ticks},
    FieldEntry{"Mute", FieldTypeId::kBool, kTypeBool, &SoundSettingsModel::b_mute},
    FieldEntry{"MuteBackgroundMusic", FieldTypeId::kBool, kTypeBool,
               &SoundSettingsModel::b_mute_background_music},
}};

constexpr auto kGraphicsTable{std::tuple{
    FieldEntry{"Mode", FieldTypeId::kWindowMode, kTypeWindowMode, &GraphicSettingsModel::mode},
    FieldEntry{"VSync", FieldTypeId::kBool, kTypeBool, &GraphicSettingsModel::b_vsync},
    FieldEntry{"FullscreenSize", FieldTypeId::kInt2, kTypeInt2,
               &GraphicSettingsModel::fullscreen_size},
    FieldEntry{"WindowedSize", FieldTypeId::kInt2, kTypeInt2,
               &GraphicSettingsModel::windowed_size},
    FieldEntry{"CursorDouble", FieldTypeId::kBool, kTypeBool,
               &GraphicSettingsModel::b_cursor_double},
    FieldEntry{"ViewportDistance", FieldTypeId::kWorldViewport, kTypeWorldViewport,
               &GraphicSettingsModel::viewport_distance},
    FieldEntry{"UIScale", FieldTypeId::kFloat, kTypeFloat, &GraphicSettingsModel::fp_ui_scale},
    FieldEntry{"CapFramerate", FieldTypeId::kBool, kTypeBool,
               &GraphicSettingsModel::b_cap_framerate},
    FieldEntry{"MaxFramerate", FieldTypeId::kInt, kTypeInt,
               &GraphicSettingsModel::int4_max_framerate},
    FieldEntry{"CapFramerateToGameFps", FieldTypeId::kBool, kTypeBool,
               &GraphicSettingsModel::b_cap_framerate_to_game_fps},
    FieldEntry{"DisableGLDebugMessageCallback", FieldTypeId::kBool, kTypeBool,
               &GraphicSettingsModel::b_disable_gl_debug_message_callback},
    FieldEntry{"DisableHardwareCursors", FieldTypeId::kBool, kTypeBool,
               &GraphicSettingsModel::b_disable_hardware_cursors},
    FieldEntry{"VideoDisplay", FieldTypeId::kInt, kTypeInt,
               &GraphicSettingsModel::int4_video_display},
    FieldEntry{"GLProfile", FieldTypeId::kGlProfile, kTypeGlProfile,
               &GraphicSettingsModel::gl_profile},
}};

constexpr auto kServerTable{std::tuple{
    FieldEntry{"Name", FieldTypeId::kString, kTypeString, &ServerSettingsModel::str_name},
    FieldEntry{"ListenPort", FieldTypeId::kInt, kTypeInt, &ServerSettingsModel::int4_listen_port},
    FieldEntry{"AdvertiseOnline", FieldTypeId::kBool, kTypeBool,
               &ServerSettingsModel::b_advertise_online},
    FieldEntry{"AdvertiseOnLocalNetwork", FieldTypeId::kBool, kTypeBool,
               &ServerSettingsModel::b_advertise_on_local_network},
    FieldEntry{"Password", FieldTypeId::kString, kTypeString, &ServerSettingsModel::str_password},
    FieldEntry{"DiscoverNatDevices", FieldTypeId::kBool, kTypeBool,
               &ServerSettingsModel::b_discover_nat_devices},
    FieldEntry{"NatPortMappingLifetime", FieldTypeId::kInt, kTypeInt,
               &ServerSettingsModel::int4_nat_port_mapping_lifetime},
    FieldEntry{"Map", FieldTypeId::kString, kTypeString, &ServerSettingsModel::str_map},
    FieldEntry{"Ban", FieldTypeId::kStringSet, kTypeStringSet, &ServerSettingsModel::vec_ban},
    FieldEntry{"RequireAuthentication", FieldTypeId::kBool, kTypeBool,
               &ServerSettingsModel::b_require_authentication},
    FieldEntry{"ProfileIDWhitelist", FieldTypeId::kIntSet, kTypeIntSet,
               &ServerSettingsModel::vec_profile_id_whitelist},
    FieldEntry{"ProfileIDBlacklist", FieldTypeId::kIntSet, kTypeIntSet,
               &ServerSettingsModel::vec_profile_id_blacklist},
    FieldEntry{"EnableSingleplayer", FieldTypeId::kBool, kTypeBool,
               &ServerSettingsModel::b_enable_singleplayer},
    FieldEntry{"EnableMapGeneration", FieldTypeId::kBool, kTypeBool,
               &ServerSettingsModel::b_enable_map_generation},
    FieldEntry{"QueryMapRepository", FieldTypeId::kBool, kTypeBool,
               &ServerSettingsModel::b_query_map_repository},
    FieldEntry{"EnableSyncReports", FieldTypeId::kBool, kTypeBool,
               &ServerSettingsModel::b_enable_sync_reports},
    FieldEntry{"TimestampFormat", FieldTypeId::kString, kTypeString,
               &ServerSettingsModel::str_timestamp_format},
    FieldEntry{"ShareAnonymizedIPs", FieldTypeId::kBool, kTypeBool,
               &ServerSettingsModel::b_share_anonymized_ips},
    FieldEntry{"EnableGeoIP", FieldTypeId::kBool, kTypeBool,
               &ServerSettingsModel::b_enable_geo_ip},
    FieldEntry{"RecordReplays", FieldTypeId::kBool, kTypeBool,
               &ServerSettingsModel::b_record_replays},
    FieldEntry{"EnableLintChecks", FieldTypeId::kBool, kTypeBool,
               &ServerSettingsModel::b_enable_lint_checks},
    FieldEntry{"MapPool", FieldTypeId::kStringSet, kTypeStringSet,
               &ServerSettingsModel::vec_map_pool},
    FieldEntry{"FloodLimitJoinCooldown", FieldTypeId::kInt, kTypeInt,
               &ServerSettingsModel::int4_flood_limit_join_cooldown},
    FieldEntry{"FloodLimitInterval", FieldTypeId::kInt, kTypeInt,
               &ServerSettingsModel::int4_flood_limit_interval},
    FieldEntry{"FloodLimitMessageCount", FieldTypeId::kInt, kTypeInt,
               &ServerSettingsModel::int4_flood_limit_message_count},
    FieldEntry{"FloodLimitCooldown", FieldTypeId::kInt, kTypeInt,
               &ServerSettingsModel::int4_flood_limit_cooldown},
    FieldEntry{"EnableVoteKick", FieldTypeId::kBool, kTypeBool,
               &ServerSettingsModel::b_enable_vote_kick},
    FieldEntry{"VoteKickTimer", FieldTypeId::kInt, kTypeInt,
               &ServerSettingsModel::int4_vote_kick_timer},
    FieldEntry{"VoteKickerCooldown", FieldTypeId::kInt, kTypeInt,
               &ServerSettingsModel::int4_vote_kicker_cooldown},
}};

constexpr auto kDebugTable{std::tuple{
    FieldEntry{"PerfText", FieldTypeId::kBool, kTypeBool, &DebugSettingsModel::b_perf_text},
    FieldEntry{"PerfGraph", FieldTypeId::kBool, kTypeBool, &DebugSettingsModel::b_perf_graph},
    FieldEntry{"Samples", FieldTypeId::kInt, kTypeInt, &DebugSettingsModel::int4_samples},
    FieldEntry{"CheckVersion", FieldTypeId::kBool, kTypeBool,
               &DebugSettingsModel::b_check_version},
    FieldEntry{"SendSystemInformation", FieldTypeId::kBool, kTypeBool,
               &DebugSettingsModel::b_send_system_information},
    FieldEntry{"SystemInformationVersionPrompt", FieldTypeId::kInt, kTypeInt,
               &DebugSettingsModel::int4_system_information_version_prompt},
    FieldEntry{"UUID", FieldTypeId::kString, kTypeString, &DebugSettingsModel::str_uuid},
    FieldEntry{"DisplayDeveloperSettings", FieldTypeId::kBool, kTypeBool,
               &DebugSettingsModel::b_display_developer_settings},
    FieldEntry{"BotDebug", FieldTypeId::kBool, kTypeBool, &DebugSettingsModel::b_bot_debug},
    FieldEntry{"LuaDebug", FieldTypeId::kBool, kTypeBool, &DebugSettingsModel::b_lua_debug},
    FieldEntry{"EnableDebugCommandsInReplays", FieldTypeId::kBool, kTypeBool,
               &DebugSettingsModel::b_enable_debug_commands_in_replays},
    FieldEntry{"EnableSimulationPerfLogging", FieldTypeId::kBool, kTypeBool,
               &DebugSettingsModel::b_enable_simulation_perf_logging},
    FieldEntry{"LongTickThresholdMs", FieldTypeId::kFloat, kTypeFloat,
               &DebugSettingsModel::fp_long_tick_threshold_ms},
    FieldEntry{"SyncCheckUnsyncedCode", FieldTypeId::kBool, kTypeBool,
               &DebugSettingsModel::b_sync_check_unsynced_code},
    FieldEntry{"SyncCheckBotModuleCode", FieldTypeId::kBool, kTypeBool,
               &DebugSettingsModel::b_sync_check_bot_module_code},
}};

bool is_ascii_space_char(char ch) {
  return ch == ' ' || ch == '\t' || ch == '\n' || ch == '\v' || ch == '\f' || ch == '\r';
}

std::string_view trim_ascii_view(std::string_view str_in) {
  while (!str_in.empty() && is_ascii_space_char(str_in.front())) str_in.remove_prefix(1);
  while (!str_in.empty() && is_ascii_space_char(str_in.back())) str_in.remove_suffix(1);
  return str_in;
}

bool ieq_ascii(std::string_view lhs, std::string_view rhs) {
  if (lhs.size() != rhs.size()) return false;
  for (std::size_t i{0}; i < lhs.size(); ++i)
    if (std::tolower(static_cast<unsigned char>(lhs[i])) !=
        std::tolower(static_cast<unsigned char>(rhs[i])))
      return false;
  return true;
}

std::string format_int_value(std::int32_t int4_value) {
  char buf[16]{};
  const auto res{std::to_chars(buf, buf + sizeof(buf), int4_value)};
  return std::string{buf, res.ptr};
}

std::string format_float_value(float fp_value) {
  char buf[64]{};
  const auto res{std::to_chars(buf, buf + sizeof(buf), fp_value, std::chars_format::scientific)};
  const std::string_view str_sci{buf, res.ptr};
  const std::size_t idx_e{str_sci.find('e')};
  std::string_view str_mantissa{str_sci.substr(0, idx_e)};
  const bool b_neg{str_mantissa.starts_with('-')};
  if (b_neg) str_mantissa.remove_prefix(1);
  std::string str_digits{str_mantissa};
  str_digits.erase(std::remove(str_digits.begin(), str_digits.end(), '.'), str_digits.end());
  int int4_exp{};
  const char* ptr_exp_begin{str_sci.data() + idx_e + 1};
  if (ptr_exp_begin != str_sci.data() + str_sci.size() && *ptr_exp_begin == '+')
    ++ptr_exp_begin;
  const auto res_exp{std::from_chars(ptr_exp_begin, str_sci.data() + str_sci.size(), int4_exp)};
  if (res_exp.ec != std::errc{}) return {};
  const std::size_t sz_len{str_digits.size()};
  const std::int64_t int8_point{static_cast<std::int64_t>(int4_exp) + 1};
  std::string str_out;
  if (b_neg) str_out += '-';
  if (int4_exp >= 9 || int4_exp <= -5) {
    str_out += str_digits.substr(0, 1);
    if (sz_len > 1) {
      str_out += '.';
      str_out += str_digits.substr(1);
    }
    str_out += 'E';
    str_out += int4_exp >= 0 ? '+' : '-';
    const std::int32_t int4_mag{int4_exp >= 0 ? int4_exp : -int4_exp};
    if (int4_mag < 10) str_out += '0';
    str_out += format_int_value(int4_mag);
    return str_out;
  }
  if (int8_point >= static_cast<std::int64_t>(sz_len)) {
    str_out += str_digits;
    str_out.append(static_cast<std::size_t>(int8_point - static_cast<std::int64_t>(sz_len)), '0');
  } else if (int8_point <= 0) {
    str_out += "0.";
    str_out.append(static_cast<std::size_t>(-int8_point), '0');
    str_out += str_digits;
  } else {
    str_out += str_digits.substr(0, static_cast<std::size_t>(int8_point));
    str_out += '.';
    str_out += str_digits.substr(static_cast<std::size_t>(int8_point));
  }
  return str_out;
}

std::string format_color_value(std::uint32_t u4_argb) {
  const std::uint8_t u1_r{static_cast<std::uint8_t>((u4_argb >> 16) & 0xFFu)};
  const std::uint8_t u1_g{static_cast<std::uint8_t>((u4_argb >> 8) & 0xFFu)};
  const std::uint8_t u1_b{static_cast<std::uint8_t>(u4_argb & 0xFFu)};
  const std::uint8_t u1_a{static_cast<std::uint8_t>((u4_argb >> 24) & 0xFFu)};
  if (u1_a == 255) return std::format("{:02X}{:02X}{:02X}", u1_r, u1_g, u1_b);
  return std::format("{:02X}{:02X}{:02X}{:02X}", u1_r, u1_g, u1_b, u1_a);
}

std::string format_int2_value(const SettingsInt2Model& v) {
  return std::format("{},{}", v.int4_x, v.int4_y);
}

std::string format_bool_value(bool b_value) { return b_value ? "True" : "False"; }

std::string format_enum_value(FieldTypeId ft, int int4_value) {
  bool b_flags{false};
  const std::span<const EnumMember> vec_members{enum_members_of(ft, b_flags)};
  if (b_flags) {
    if (int4_value == 0) return std::string{vec_members[0].str_name};
    int int4_rest{int4_value};
    std::string str_out;
    for (const EnumMember& m : vec_members) {
      if (m.int4_value == 0) continue;
      if ((int4_rest & m.int4_value) == m.int4_value) {
        if (!str_out.empty()) str_out += ", ";
        str_out += m.str_name;
        int4_rest &= ~m.int4_value;
      }
    }
    if (int4_rest != 0) {
      if (!str_out.empty()) str_out += ", ";
      str_out += format_int_value(int4_rest);
    }
    return str_out;
  }
  for (const EnumMember& m : vec_members)
    if (m.int4_value == int4_value) return std::string{m.str_name};
  return format_int_value(int4_value);
}

template <typename VecT, typename Fn>
std::string join_with(const VecT& vec, Fn&& fn) {
  std::string str_out;
  for (std::size_t i{0}; i < vec.size(); ++i) {
    if (i > 0) str_out += ", ";
    str_out += fn(vec[i]);
  }
  return str_out;
}

void enum_storage_write(void* ptr_field, int int4_value) {
  std::memcpy(ptr_field, &int4_value, sizeof(int4_value));
}

int enum_storage_read(const void* ptr_field) {
  int int4_value{};
  std::memcpy(&int4_value, ptr_field, sizeof(int4_value));
  return int4_value;
}

std::string format_field_value(const void* ptr_field, FieldTypeId ft) {
  switch (ft) {
    case FieldTypeId::kBool:
      return format_bool_value(*static_cast<const bool*>(ptr_field));
    case FieldTypeId::kInt:
      return format_int_value(*static_cast<const std::int32_t*>(ptr_field));
    case FieldTypeId::kFloat:
      return format_float_value(*static_cast<const float*>(ptr_field));
    case FieldTypeId::kString:
      return *static_cast<const std::string*>(ptr_field);
    case FieldTypeId::kColor:
      return format_color_value(static_cast<const SettingsColorModel*>(ptr_field)->u4_argb);
    case FieldTypeId::kInt2:
      return format_int2_value(*static_cast<const SettingsInt2Model*>(ptr_field));
    case FieldTypeId::kWindowMode:
    case FieldTypeId::kMouseControlStyle:
    case FieldTypeId::kMouseScrollType:
    case FieldTypeId::kStatusBarsType:
    case FieldTypeId::kTargetLinesType:
    case FieldTypeId::kWorldViewport:
    case FieldTypeId::kGlProfile:
    case FieldTypeId::kModifiers:
    case FieldTypeId::kMpGameFilters:
    case FieldTypeId::kTextNotificationPoolFilters:
      return format_enum_value(ft, enum_storage_read(ptr_field));
    case FieldTypeId::kStringSet:
      return join_with(*static_cast<const std::vector<std::string>*>(ptr_field),
                       [](const std::string& str) { return str; });
    case FieldTypeId::kIntSet:
      return join_with(*static_cast<const std::vector<std::int32_t>*>(ptr_field),
                       [](std::int32_t int4_value) { return format_int_value(int4_value); });
    case FieldTypeId::kColorArray:
      return join_with(*static_cast<const std::vector<SettingsColorModel>*>(ptr_field),
                       [](const SettingsColorModel& color) {
                         return format_color_value(color.u4_argb);
                       });
  }
  return {};
}

void emit_field_warning(const SettingsLoadOptions& opts, std::string_view str_value,
                        std::string_view str_field, std::string_view str_type,
                        std::string_view str_ret) {
  const std::string str_line{std::format(
      "FieldLoader: Cannot parse `{}` into field `{}` of type `{}`; substituting default `{}`",
      str_value, str_field, str_type, str_ret)};
  if (opts.ptr_warnings != nullptr)
    opts.ptr_warnings->push_back(str_line);
  else
    std::println("{}", str_line);
}

bool parse_int32_value(std::string_view str_value, std::int32_t& int4_out) {
  std::string_view str{trim_ascii_view(str_value)};
  if (!str.empty() && str.front() == '+') str.remove_prefix(1);
  const auto res{std::from_chars(str.data(), str.data() + str.size(), int4_out)};
  return res.ec == std::errc{} && res.ptr == str.data() + str.size();
}

bool parse_bool_value(std::string_view str_value, bool& b_out) {
  const std::string_view str{trim_ascii_view(str_value)};
  if (ieq_ascii(str, "true")) {
    b_out = true;
    return true;
  }
  if (ieq_ascii(str, "false")) {
    b_out = false;
    return true;
  }
  return false;
}

bool parse_float_value(std::string_view str_value, float& fp_out) {
  std::string str{trim_ascii_view(str_value)};
  float fp_mult{1.0f};
  if (str.contains('%')) {
    str.erase(std::remove(str.begin(), str.end(), '%'), str.end());
    fp_mult = 0.01f;
  }
  std::string_view str_trimmed{trim_ascii_view(str)};
  if (!str_trimmed.empty() && str_trimmed.front() == '+') str_trimmed.remove_prefix(1);
  const auto res{std::from_chars(str_trimmed.data(), str_trimmed.data() + str_trimmed.size(),
                                 fp_out, std::chars_format::general)};
  if (res.ec != std::errc{} || res.ptr != str_trimmed.data() + str_trimmed.size()) return false;
  fp_out *= fp_mult;
  return true;
}

bool parse_hex_byte(std::string_view str_pair, std::uint8_t& u1_out) {
  if (str_pair.size() != 2) return false;
  std::uint8_t u1_value{};
  for (const char ch : str_pair) {
    int int4_digit{-1};
    if (ch >= '0' && ch <= '9')
      int4_digit = ch - '0';
    else if (ch >= 'a' && ch <= 'f')
      int4_digit = ch - 'a' + 10;
    else if (ch >= 'A' && ch <= 'F')
      int4_digit = ch - 'A' + 10;
    if (int4_digit < 0) return false;
    u1_value = static_cast<std::uint8_t>(u1_value * 16 + int4_digit);
  }
  u1_out = u1_value;
  return true;
}

bool parse_color_value(std::string_view str_value, std::uint32_t& u4_out) {
  const std::string_view str{trim_ascii_view(str_value)};
  if (str.size() != 6 && str.size() != 8) return false;
  std::uint8_t u1_r{};
  std::uint8_t u1_g{};
  std::uint8_t u1_b{};
  std::uint8_t u1_a{255};
  if (!parse_hex_byte(str.substr(0, 2), u1_r) || !parse_hex_byte(str.substr(2, 2), u1_g) ||
      !parse_hex_byte(str.substr(4, 2), u1_b))
    return false;
  if (str.size() == 8 && !parse_hex_byte(str.substr(6, 2), u1_a)) return false;
  u4_out = (static_cast<std::uint32_t>(u1_a) << 24) | (static_cast<std::uint32_t>(u1_r) << 16) |
           (static_cast<std::uint32_t>(u1_g) << 8) | static_cast<std::uint32_t>(u1_b);
  return true;
}

bool parse_int2_value(std::string_view str_value, int& int4_x, int& int4_y) {
  std::array<std::string_view, 3> arr_tokens{};
  std::size_t sz_count{0};
  std::size_t sz_pos{0};
  while (sz_pos <= str_value.size() && sz_count < arr_tokens.size() + 1) {
    const std::size_t idx_comma{str_value.find(',', sz_pos)};
    const std::string_view str_token{trim_ascii_view(str_value.substr(
        sz_pos, idx_comma == std::string_view::npos ? std::string_view::npos
                                                    : idx_comma - sz_pos))};
    if (!str_token.empty()) {
      if (sz_count >= arr_tokens.size()) return false;
      arr_tokens[sz_count++] = str_token;
    }
    if (idx_comma == std::string_view::npos) break;
    sz_pos = idx_comma + 1;
  }
  if (sz_count != 2) return false;
  if (!parse_int32_value(arr_tokens[0], int4_x) || !parse_int32_value(arr_tokens[1], int4_y))
    return false;
  return true;
}

const EnumMember* lookup_enum_member(std::span<const EnumMember> vec_members,
                                     std::string_view str_token) {
  for (const EnumMember& m : vec_members)
    if (ieq_ascii(m.str_name, str_token)) return &m;
  return nullptr;
}

bool parse_enum_value(FieldTypeId ft, std::string_view str_value, int& int4_out) {
  bool b_flags{false};
  const std::span<const EnumMember> vec_members{enum_members_of(ft, b_flags)};
  if (!b_flags) {
    const std::string_view str_token{trim_ascii_view(str_value)};
    if (str_token.empty()) return false;
    if (const EnumMember* ptr_named{lookup_enum_member(vec_members, str_token)}) {
      int4_out = ptr_named->int4_value;
      return true;
    }
    return parse_int32_value(str_token, int4_out);
  }
  int int4_acc{0};
  std::size_t sz_pos{0};
  while (sz_pos <= str_value.size()) {
    const std::size_t idx_comma{str_value.find(',', sz_pos)};
    const std::string_view str_token{trim_ascii_view(str_value.substr(
        sz_pos, idx_comma == std::string_view::npos ? std::string_view::npos
                                                    : idx_comma - sz_pos))};
    if (str_token.empty()) return false;
    if (const EnumMember* ptr_named{lookup_enum_member(vec_members, str_token)})
      int4_acc |= ptr_named->int4_value;
    else {
      int int4_token{};
      if (!parse_int32_value(str_token, int4_token)) return false;
      int4_acc |= int4_token;
    }
    if (idx_comma == std::string_view::npos) break;
    sz_pos = idx_comma + 1;
  }
  int4_out = int4_acc;
  return true;
}

void parse_string_set_value(std::string_view str_value, std::vector<std::string>& vec_out) {
  vec_out.clear();
  std::size_t sz_pos{0};
  while (sz_pos <= str_value.size()) {
    const std::size_t idx_comma{str_value.find(',', sz_pos)};
    const std::string_view str_token{trim_ascii_view(str_value.substr(
        sz_pos, idx_comma == std::string_view::npos ? std::string_view::npos
                                                    : idx_comma - sz_pos))};
    if (!str_token.empty()) vec_out.emplace_back(str_token);
    if (idx_comma == std::string_view::npos) break;
    sz_pos = idx_comma + 1;
  }
}

void parse_int_set_value(std::string_view str_value, std::string_view str_field,
                         const SettingsLoadOptions& opts,
                         std::vector<std::int32_t>& vec_out) {
  vec_out.clear();
  std::size_t sz_pos{0};
  while (sz_pos <= str_value.size()) {
    const std::size_t idx_comma{str_value.find(',', sz_pos)};
    const std::string_view str_token{trim_ascii_view(str_value.substr(
        sz_pos, idx_comma == std::string_view::npos ? std::string_view::npos
                                                    : idx_comma - sz_pos))};
    if (!str_token.empty()) {
      std::int32_t int4_element{};
      if (!parse_int32_value(str_token, int4_element)) {
        emit_field_warning(opts, str_token, str_field, kTypeInt, "0");
        int4_element = 0;
      }
      vec_out.push_back(int4_element);
    }
    if (idx_comma == std::string_view::npos) break;
    sz_pos = idx_comma + 1;
  }
}

void parse_color_array_value(std::string_view str_value, std::string_view str_field,
                             const SettingsLoadOptions& opts,
                             std::vector<SettingsColorModel>& vec_out) {
  vec_out.clear();
  std::size_t sz_pos{0};
  while (sz_pos <= str_value.size()) {
    const std::size_t idx_comma{str_value.find(',', sz_pos)};
    const std::string_view str_token{trim_ascii_view(str_value.substr(
        sz_pos, idx_comma == std::string_view::npos ? std::string_view::npos
                                                    : idx_comma - sz_pos))};
    if (!str_token.empty()) {
      std::uint32_t u4_element{};
      if (!parse_color_value(str_token, u4_element)) {
        emit_field_warning(opts, str_token, str_field, kTypeColor,
                           format_color_value(0));
        u4_element = 0;
      }
      vec_out.push_back(SettingsColorModel{u4_element});
    }
    if (idx_comma == std::string_view::npos) break;
    sz_pos = idx_comma + 1;
  }
}

bool parse_field_value(void* ptr_field, FieldTypeId ft, std::string_view str_value,
                       std::string_view str_field, const SettingsLoadOptions& opts) {
  switch (ft) {
    case FieldTypeId::kBool: {
      bool b_value{};
      if (!parse_bool_value(str_value, b_value)) return false;
      *static_cast<bool*>(ptr_field) = b_value;
      return true;
    }
    case FieldTypeId::kInt: {
      std::int32_t int4_value{};
      if (!parse_int32_value(str_value, int4_value)) return false;
      *static_cast<std::int32_t*>(ptr_field) = int4_value;
      return true;
    }
    case FieldTypeId::kFloat: {
      float fp_value{};
      if (!parse_float_value(str_value, fp_value)) return false;
      *static_cast<float*>(ptr_field) = fp_value;
      return true;
    }
    case FieldTypeId::kString:
      *static_cast<std::string*>(ptr_field) = std::string{str_value};
      return true;
    case FieldTypeId::kColor: {
      std::uint32_t u4_value{};
      if (!parse_color_value(str_value, u4_value)) return false;
      static_cast<SettingsColorModel*>(ptr_field)->u4_argb = u4_value;
      return true;
    }
    case FieldTypeId::kInt2: {
      int int4_x{};
      int int4_y{};
      if (!parse_int2_value(str_value, int4_x, int4_y)) return false;
      static_cast<SettingsInt2Model*>(ptr_field)->int4_x = int4_x;
      static_cast<SettingsInt2Model*>(ptr_field)->int4_y = int4_y;
      return true;
    }
    case FieldTypeId::kWindowMode:
    case FieldTypeId::kMouseControlStyle:
    case FieldTypeId::kMouseScrollType:
    case FieldTypeId::kStatusBarsType:
    case FieldTypeId::kTargetLinesType:
    case FieldTypeId::kWorldViewport:
    case FieldTypeId::kGlProfile:
    case FieldTypeId::kModifiers:
    case FieldTypeId::kMpGameFilters:
    case FieldTypeId::kTextNotificationPoolFilters: {
      int int4_value{};
      if (!parse_enum_value(ft, str_value, int4_value)) return false;
      enum_storage_write(ptr_field, int4_value);
      return true;
    }
    case FieldTypeId::kStringSet:
      parse_string_set_value(str_value, *static_cast<std::vector<std::string>*>(ptr_field));
      return true;
    case FieldTypeId::kIntSet:
      parse_int_set_value(str_value, str_field, opts,
                          *static_cast<std::vector<std::int32_t>*>(ptr_field));
      return true;
    case FieldTypeId::kColorArray:
      parse_color_array_value(str_value, str_field, opts,
                              *static_cast<std::vector<SettingsColorModel>*>(ptr_field));
      return true;
  }
  return false;
}

void assign_field_zero(void* ptr_field, FieldTypeId ft) {
  switch (ft) {
    case FieldTypeId::kBool:
      *static_cast<bool*>(ptr_field) = false;
      return;
    case FieldTypeId::kInt:
      *static_cast<std::int32_t*>(ptr_field) = 0;
      return;
    case FieldTypeId::kFloat:
      *static_cast<float*>(ptr_field) = 0.0f;
      return;
    case FieldTypeId::kString:
      static_cast<std::string*>(ptr_field)->clear();
      return;
    case FieldTypeId::kColor:
      static_cast<SettingsColorModel*>(ptr_field)->u4_argb = 0;
      return;
    case FieldTypeId::kInt2:
      *static_cast<SettingsInt2Model*>(ptr_field) = SettingsInt2Model{};
      return;
    case FieldTypeId::kWindowMode:
    case FieldTypeId::kMouseControlStyle:
    case FieldTypeId::kMouseScrollType:
    case FieldTypeId::kStatusBarsType:
    case FieldTypeId::kTargetLinesType:
    case FieldTypeId::kWorldViewport:
    case FieldTypeId::kGlProfile:
    case FieldTypeId::kModifiers:
    case FieldTypeId::kMpGameFilters:
    case FieldTypeId::kTextNotificationPoolFilters:
      enum_storage_write(ptr_field, 0);
      return;
    case FieldTypeId::kStringSet:
      static_cast<std::vector<std::string>*>(ptr_field)->clear();
      return;
    case FieldTypeId::kIntSet:
      static_cast<std::vector<std::int32_t>*>(ptr_field)->clear();
      return;
    case FieldTypeId::kColorArray:
      static_cast<std::vector<SettingsColorModel>*>(ptr_field)->clear();
      return;
  }
}

std::vector<std::pair<std::string, std::string>> parse_cli_args(
    std::span<const std::string> vec_args) {
  std::vector<std::pair<std::string, std::string>> vec_out;
  for (const std::string& str_raw : vec_args) {
    const std::size_t idx_eq{str_raw.find('=')};
    if (idx_eq == std::string::npos || idx_eq == 0) continue;
    std::string str_key{str_raw.substr(0, idx_eq)};
    std::string str_value{str_raw.substr(idx_eq + 1)};
    bool b_replaced{false};
    for (auto& [str_existing_key, str_existing_value] : vec_out) {
      if (str_existing_key != str_key) continue;
      str_existing_value = std::move(str_value);
      b_replaced = true;
      break;
    }
    if (!b_replaced) vec_out.emplace_back(std::move(str_key), std::move(str_value));
  }
  return vec_out;
}

std::size_t section_index_get_or_create(SettingsModel& sm, std::string_view str_key) {
  for (std::size_t i{0}; i < sm.vec_yaml.size(); ++i)
    if (sm.vec_yaml[i].str_key == str_key) return i;
  sm.vec_yaml.push_back(MiniYamlNode{std::string{str_key}, {}, {}, {}, {}});
  return sm.vec_yaml.size() - 1;
}

template <typename ModelT, typename TableT>
std::optional<std::string> settings_load_section(MiniYamlNode& node_section, ModelT& model,
                                                 const TableT& vec_table,
                                                 std::string_view str_section_key,
                                                 const std::vector<std::pair<std::string, std::string>>& vec_args,
                                                 const SettingsLoadOptions& opts) {
  for (std::size_t i{0}; i < node_section.vec_children.size(); ++i) {
    const MiniYamlNode& node_child{node_section.vec_children[i]};
    if (node_child.str_key.empty())
      return std::string{"Value cannot be null. (Parameter 'key')"};
    for (std::size_t j{0}; j < i; ++j) {
      if (node_section.vec_children[j].str_key != node_child.str_key) continue;
      return std::format("Duplicate key '{}' in {}:{}", node_child.str_key,
                         node_child.loc_location.str_name, node_child.loc_location.int4_line);
    }
  }

  std::optional<std::string> opt_error;
  auto fold_load = [&](const auto& entry) {
    if (opt_error.has_value()) return;
    for (const MiniYamlNode& node_child : node_section.vec_children) {
      if (node_child.str_key != entry.str_name) continue;
      void* ptr_field{static_cast<void*>(&(model.*entry.ptr_member))};
      if (!parse_field_value(ptr_field, entry.ft, node_child.str_value, entry.str_name, opts)) {
        assign_field_zero(ptr_field, entry.ft);
        emit_field_warning(opts, node_child.str_value, entry.str_name, entry.str_dotnet_type,
                           format_field_value(ptr_field, entry.ft));
      }
      break;
    }
  };
  std::apply([&](const auto&... entries) { (fold_load(entries), ...); }, vec_table);
  if (opt_error.has_value()) return opt_error;

  auto fold_cli = [&](const auto& entry) {
    if (opt_error.has_value()) return;
    const std::string str_arg_name{std::format("{}.{}", str_section_key, entry.str_name)};
    for (const auto& [str_arg_key, str_arg_value] : vec_args) {
      if (str_arg_key != str_arg_name) continue;
      void* ptr_field{static_cast<void*>(&(model.*entry.ptr_member))};
      if (!parse_field_value(ptr_field, entry.ft, str_arg_value, entry.str_name, opts)) {
        assign_field_zero(ptr_field, entry.ft);
        emit_field_warning(opts, str_arg_value, entry.str_name, entry.str_dotnet_type,
                           format_field_value(ptr_field, entry.ft));
      }
      break;
    }
  };
  std::apply([&](const auto&... entries) { (fold_cli(entries), ...); }, vec_table);
  return opt_error;
}

template <typename ModelT, typename TableT>
void settings_commit_section(MiniYamlNode& node_section, const ModelT& model,
                             const TableT& vec_table, const ModelT& defaults) {
  auto fold = [&](const auto& entry) {
    std::erase_if(node_section.vec_children, [&](const MiniYamlNode& node_child) {
      return node_child.str_key == entry.str_name;
    });
    const void* ptr_current{static_cast<const void*>(&(model.*entry.ptr_member))};
    const void* ptr_default{static_cast<const void*>(&(defaults.*entry.ptr_member))};
    const std::string str_current{format_field_value(ptr_current, entry.ft)};
    const std::string str_default{format_field_value(ptr_default, entry.ft)};
    if (str_current != str_default)
      node_section.vec_children.push_back(
          MiniYamlNode{std::string{entry.str_name}, str_current, {}, {}, {}});
  };
  std::apply([&](const auto&... entries) { (fold(entries), ...); }, vec_table);
}

}

std::string settings_new_uuid() {
  static std::uint32_t u4_state{0x9E3779B9u};
  const auto next{[]() {
    std::uint32_t x{u4_state};
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    u4_state = x;
    return x;
  }};
  const std::uint32_t u4_n1{next()};
  const std::uint32_t u4_n2{next()};
  const std::uint32_t u4_n3{next()};
  const std::uint32_t u4_n4{next()};
  const std::uint32_t u4_n5{next()};
  const std::uint32_t u4_n6{next()};
  return std::format("{:08x}-{:04x}-{:04x}-{:04x}-{:04x}{:08x}", u4_n1, u4_n2 & 0xFFFFu,
                     u4_n3 & 0xFFFFu, u4_n4 & 0xFFFFu, u4_n5 & 0xFFFFu, u4_n6);
}

std::expected<SettingsModel, std::string> settings_load(std::string_view str_path,
                                                        const SettingsLoadOptions& opts) {
  SettingsModel sm{};
  const std::vector<std::pair<std::string, std::string>> vec_args{parse_cli_args(opts.vec_args)};
  {
    std::error_code ec{};
    if (std::filesystem::exists(str_path, ec)) {
      auto res_nodes{miniyaml_from_file(str_path, false)};
      if (!res_nodes.has_value()) return std::unexpected(res_nodes.error());
      sm.vec_yaml = std::move(*res_nodes);
    }
  }

  const auto load_at{[&](std::size_t& sz_idx, std::string_view str_key, auto& model,
                         const auto& vec_table) -> std::optional<std::string> {
    sz_idx = section_index_get_or_create(sm, str_key);
    return settings_load_section(sm.vec_yaml[sz_idx], model, vec_table, str_key, vec_args, opts);
  }};
  if (auto opt_error{load_at(sm.sz_player, "Player", sm.player, kPlayerTable)})
    return std::unexpected(std::move(*opt_error));
  if (auto opt_error{load_at(sm.sz_game, "Game", sm.game, kGameTable)})
    return std::unexpected(std::move(*opt_error));
  if (auto opt_error{load_at(sm.sz_sound, "Sound", sm.sound, kSoundTable)})
    return std::unexpected(std::move(*opt_error));
  if (auto opt_error{load_at(sm.sz_graphics, "Graphics", sm.graphics, kGraphicsTable)})
    return std::unexpected(std::move(*opt_error));
  if (auto opt_error{load_at(sm.sz_server, "Server", sm.server, kServerTable)})
    return std::unexpected(std::move(*opt_error));
  if (auto opt_error{load_at(sm.sz_debug, "Debug", sm.debug, kDebugTable)})
    return std::unexpected(std::move(*opt_error));
  return sm;
}

std::expected<void, std::string> settings_save(SettingsModel& sm, std::string_view str_path,
                                               bool b_commit_modules) {
  if (b_commit_modules) {
    const auto commit_at{[&](std::size_t& sz_idx, std::string_view str_key, const auto& model,
                             const auto& vec_table) {
      if (sz_idx >= sm.vec_yaml.size()) sz_idx = section_index_get_or_create(sm, str_key);
      settings_commit_section(sm.vec_yaml[sz_idx], model, vec_table,
                              std::decay_t<decltype(model)>{});
    }};
    commit_at(sm.sz_player, "Player", sm.player, kPlayerTable);
    commit_at(sm.sz_game, "Game", sm.game, kGameTable);
    commit_at(sm.sz_sound, "Sound", sm.sound, kSoundTable);
    commit_at(sm.sz_graphics, "Graphics", sm.graphics, kGraphicsTable);
    commit_at(sm.sz_server, "Server", sm.server, kServerTable);
    commit_at(sm.sz_debug, "Debug", sm.debug, kDebugTable);
  }

  std::vector<MiniYamlNode> vec_out;
  for (const MiniYamlNode& node : sm.vec_yaml) {
    if (node.vec_children.empty()) continue;
    vec_out.push_back(node);
    vec_out.emplace_back();
  }
  return miniyaml_write_to_file(str_path, vec_out);
}

MiniYamlNode* settings_find_section(SettingsModel& sm, std::string_view str_key) {
  for (MiniYamlNode& node : sm.vec_yaml)
    if (node.str_key == str_key) return &node;
  return nullptr;
}

MiniYamlNode& settings_get_or_create_section(SettingsModel& sm, std::string_view str_key) {
  return sm.vec_yaml[section_index_get_or_create(sm, str_key)];
}

void settings_keys_assign(SettingsModel& sm,
                          std::span<const std::pair<std::string, std::string>> vec_entries) {
  MiniYamlNode& node{sm.vec_yaml[section_index_get_or_create(sm, "Keys")]};
  for (const auto& [str_name, str_value] : vec_entries) {
    std::erase_if(node.vec_children, [&str_name](const MiniYamlNode& node_child) {
      return node_child.str_key == str_name;
    });
    node.vec_children.push_back(MiniYamlNode{str_name, str_value, {}, {}, {}});
  }
}

std::string settings_format_float(float fp_value) { return format_float_value(fp_value); }

}
