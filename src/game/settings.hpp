#pragma once

import std;

#include "game/miniyaml.hpp"

namespace ora::game {

enum class WindowMode { kWindowed, kFullscreen, kPseudoFullscreen };
enum class MouseControlStyle { kClassic, kModern, kOtherRts };
enum class MouseScrollType { kDisabled, kStandard, kInverted, kJoystick };
enum class StatusBarsType { kStandard, kDamageShow, kAlwaysShow };
enum class TargetLinesType { kDisabled, kManual, kAutomatic };
enum class WorldViewport { kNative, kClose, kMedium, kFar };
enum class GlProfile { kAutomatic, kAngle, kModern, kEmbedded };
enum class Modifiers : int { kNone = 0, kShift = 1, kAlt = 2, kCtrl = 4, kMeta = 8 };
enum class MpGameFilters : int {
  kNone = 0,
  kWaiting = 1,
  kEmpty = 2,
  kProtected = 4,
  kStarted = 8,
  kIncompatible = 16
};
enum class TextNotificationPoolFilters : int { kNone = 0, kFeedback = 1, kTransients = 2 };

[[nodiscard]] constexpr MpGameFilters operator|(MpGameFilters lhs, MpGameFilters rhs) {
  return static_cast<MpGameFilters>(std::to_underlying(lhs) | std::to_underlying(rhs));
}

[[nodiscard]] constexpr TextNotificationPoolFilters operator|(
    TextNotificationPoolFilters lhs, TextNotificationPoolFilters rhs) {
  return static_cast<TextNotificationPoolFilters>(std::to_underlying(lhs) |
                                                  std::to_underlying(rhs));
}

struct SettingsColorModel {
  std::uint32_t u4_argb{};
};

struct SettingsInt2Model {
  int int4_x{};
  int int4_y{};
};

[[nodiscard]] std::string settings_new_uuid();

struct PlayerSettingsModel {
  std::string str_name{"Commander"};
  SettingsColorModel color{0xFFC82020u};
  std::string str_last_server{"localhost:1234"};
  std::vector<SettingsColorModel> vec_custom_colors;
};

struct GameSettingsModel {
  std::string str_platform{"Default"};
  bool b_viewport_edge_scroll{true};
  int int4_viewport_edge_scroll_margin{5};
  bool b_lock_mouse_window{};
  MouseControlStyle mouse_control_style{MouseControlStyle::kModern};
  MouseScrollType mouse_scroll{MouseScrollType::kJoystick};
  float fp_viewport_edge_scroll_step{30.0f};
  float fp_ui_scroll_speed{50.0f};
  float fp_zoom_speed{0.04f};
  int int4_selection_deadzone{24};
  int int4_mouse_scroll_deadzone{8};
  bool b_use_alternate_scroll_button{};
  bool b_hide_replay_chat{};
  StatusBarsType status_bars{StatusBarsType::kStandard};
  TargetLinesType target_lines{TargetLinesType::kManual};
  bool b_use_player_stance_colors{};
  bool b_allow_downloading{true};
  std::string str_auth_profile{"player.oraid"};
  Modifiers zoom_modifier{Modifiers::kNone};
  bool b_fetch_news{true};
  int int4_introduction_prompt_version{};
  MpGameFilters mp_game_filters{MpGameFilters::kWaiting | MpGameFilters::kEmpty |
                                MpGameFilters::kProtected | MpGameFilters::kStarted};
  bool b_pause_shellmap{};
  bool b_enable_discord_service{true};
  TextNotificationPoolFilters text_notification_pool_filters{
      TextNotificationPoolFilters::kFeedback | TextNotificationPoolFilters::kTransients};
};

struct SoundSettingsModel {
  float fp_sound_volume{0.5f};
  float fp_music_volume{0.5f};
  float fp_video_volume{0.5f};
  bool b_shuffle{};
  bool b_repeat{};
  std::string str_device{};
  bool b_cash_ticks{true};
  bool b_mute{};
  bool b_mute_background_music{};
};

struct GraphicSettingsModel {
  WindowMode mode{WindowMode::kPseudoFullscreen};
  bool b_vsync{true};
  SettingsInt2Model fullscreen_size{};
  SettingsInt2Model windowed_size{1024, 768};
  bool b_cursor_double{};
  WorldViewport viewport_distance{WorldViewport::kMedium};
  float fp_ui_scale{1.0f};
  bool b_cap_framerate{};
  int int4_max_framerate{60};
  bool b_cap_framerate_to_game_fps{};
  bool b_disable_gl_debug_message_callback{};
  bool b_disable_hardware_cursors{};
  int int4_video_display{};
  GlProfile gl_profile{GlProfile::kAutomatic};
};

struct ServerSettingsModel {
  std::string str_name{};
  int int4_listen_port{1234};
  bool b_advertise_online{true};
  bool b_advertise_on_local_network{true};
  std::string str_password{};
  bool b_discover_nat_devices{};
  int int4_nat_port_mapping_lifetime{36000};
  std::string str_map{};
  std::vector<std::string> vec_ban;
  bool b_require_authentication{};
  std::vector<std::int32_t> vec_profile_id_whitelist;
  std::vector<std::int32_t> vec_profile_id_blacklist;
  bool b_enable_singleplayer{};
  bool b_enable_map_generation{true};
  bool b_query_map_repository{true};
  bool b_enable_sync_reports{};
  std::string str_timestamp_format{"yyyy-MM-ddTHH:mm:ss"};
  bool b_share_anonymized_ips{true};
  bool b_enable_geo_ip{true};
  bool b_record_replays{};
  bool b_enable_lint_checks{true};
  std::vector<std::string> vec_map_pool;
  int int4_flood_limit_join_cooldown{5000};
  int int4_flood_limit_interval{5000};
  int int4_flood_limit_message_count{5};
  int int4_flood_limit_cooldown{15000};
  bool b_enable_vote_kick{true};
  int int4_vote_kick_timer{30000};
  int int4_vote_kicker_cooldown{120000};
};

struct DebugSettingsModel {
  bool b_perf_text{};
  bool b_perf_graph{};
  int int4_samples{25};
  bool b_check_version{true};
  bool b_send_system_information{true};
  int int4_system_information_version_prompt{};
  std::string str_uuid{settings_new_uuid()};
  bool b_display_developer_settings{};
  bool b_bot_debug{};
  bool b_lua_debug{};
  bool b_enable_debug_commands_in_replays{};
  bool b_enable_simulation_perf_logging{};
  float fp_long_tick_threshold_ms{1.0f};
  bool b_sync_check_unsynced_code{};
  bool b_sync_check_bot_module_code{};
};

struct SettingsModel {
  std::vector<MiniYamlNode> vec_yaml;
  std::size_t sz_player{};
  std::size_t sz_game{};
  std::size_t sz_sound{};
  std::size_t sz_graphics{};
  std::size_t sz_server{};
  std::size_t sz_debug{};
  PlayerSettingsModel player;
  GameSettingsModel game;
  SoundSettingsModel sound;
  GraphicSettingsModel graphics;
  ServerSettingsModel server;
  DebugSettingsModel debug;
};

struct SettingsLoadOptions {
  std::span<const std::string> vec_args{};
  std::vector<std::string>* ptr_warnings{};
};

[[nodiscard]] std::expected<SettingsModel, std::string> settings_load(
    std::string_view str_path, const SettingsLoadOptions& opts = {});

[[nodiscard]] std::expected<void, std::string> settings_save(
    SettingsModel& sm, std::string_view str_path, bool b_commit_modules = true);

[[nodiscard]] MiniYamlNode* settings_find_section(SettingsModel& sm, std::string_view str_key);

[[nodiscard]] MiniYamlNode& settings_get_or_create_section(SettingsModel& sm,
                                                           std::string_view str_key);

void settings_keys_assign(SettingsModel& sm,
                          std::span<const std::pair<std::string, std::string>> vec_entries);

[[nodiscard]] std::string settings_format_float(float fp_value);

}
