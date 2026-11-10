export module ora.game;

import std;
import ora.game.filesystem;
import ora.game.miniyaml;
import ora.game.support;

export {
namespace ora::game {

int game_tag();

std::string_view game_name();

}

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

namespace ora::game {

struct TerrainTile {
  std::uint16_t u2_type{};
  std::uint8_t u1_index{};
};

struct ResourceTile {
  std::uint8_t u1_type{};
  std::uint8_t u1_density{};
};

struct MapBin {
  int int4_width{};
  int int4_height{};
  std::uint32_t u4_tiles_offset{};
  std::uint32_t u4_heights_offset{};
  std::uint32_t u4_resources_offset{};
  std::vector<TerrainTile> vec_tiles;
  std::vector<ResourceTile> vec_resources;

  [[nodiscard]] const TerrainTile& tile_at(int int4_i, int int4_j) const {
    return vec_tiles[static_cast<std::size_t>(int4_i) * static_cast<std::size_t>(int4_height) + int4_j];
  }

  [[nodiscard]] const ResourceTile& resource_at(int int4_i, int int4_j) const {
    return vec_resources[static_cast<std::size_t>(int4_i) * static_cast<std::size_t>(int4_height) + int4_j];
  }
};

[[nodiscard]] std::expected<MapBin, std::string> map_bin_parse(
    std::span<const std::uint8_t> span1u_data, int int4_expected_width, int int4_expected_height);

}

namespace ora::game {

struct TerrainTemplate {
  std::uint16_t u2_id{};
  int int4_size_x{1};
  int int4_size_y{1};
  bool b_pickany{false};
  std::vector<std::string> vec_str_images{};
  std::vector<std::optional<std::uint8_t>> vec_opt1u_frame_terrain{};

  [[nodiscard]] bool contains_frame(int int4_index) const {
    return int4_index >= 0 && static_cast<std::size_t>(int4_index) < vec_opt1u_frame_terrain.size();
  }
};

struct Tileset {
  std::string str_id;
  std::string str_name;
  int int4_tile_size_x{24};
  int int4_tile_size_y{24};
  std::vector<std::string> vec_str_types;
  std::vector<TerrainTemplate> vec_templates;
  std::uint8_t u1_default_walkable{};

  [[nodiscard]] const TerrainTemplate* find_template(std::uint16_t u2_id) const {
    const auto it_hit{std::find_if(vec_templates.begin(), vec_templates.end(),
        [&](const TerrainTemplate& tpl) { return tpl.u2_id == u2_id; })};
    return it_hit == vec_templates.end() ? nullptr : &*it_hit;
  }

  [[nodiscard]] std::expected<std::uint8_t, std::string> terrain_index_of_type(std::string_view str_type) const {
    const auto it_hit{std::lower_bound(vec_str_types.begin(), vec_str_types.end(), str_type)};
    if (it_hit == vec_str_types.end() || *it_hit != str_type)
      return std::unexpected(std::format("Tileset '{}' lacks terrain type '{}'", str_id, str_type));
    return static_cast<std::uint8_t>(it_hit - vec_str_types.begin());
  }

  [[nodiscard]] bool tile_is_valid(const TerrainTile& tile) const {
    const TerrainTemplate* tpl{find_template(tile.u2_type)};
    if (tpl == nullptr) return false;
    if (!tpl->contains_frame(tile.u1_index)) return false;
    return tpl->vec_opt1u_frame_terrain[tile.u1_index].has_value();
  }

  [[nodiscard]] std::uint8_t terrain_index_of_tile(const TerrainTile& tile) const {
    const TerrainTemplate* tpl{find_template(tile.u2_type)};
    const std::optional<std::uint8_t> opt1u_terrain{tpl != nullptr && tpl->contains_frame(tile.u1_index)
        ? tpl->vec_opt1u_frame_terrain[tile.u1_index]
        : std::optional<std::uint8_t>{}};
    if (opt1u_terrain.has_value() && *opt1u_terrain != 255u) return *opt1u_terrain;
    return u1_default_walkable;
  }

  [[nodiscard]] TerrainTile default_terrain_tile() const {
    return TerrainTile{vec_templates.front().u2_id, 0};
  }
};

[[nodiscard]] std::expected<Tileset, std::string> tileset_load(
    std::string_view str_yaml_text, std::string_view str_source_name);

}

namespace ora::game {

enum class interrupt_type : std::uint8_t { do_not_play, interrupt, overlap };

struct sound_pool {
  float f4_volume_modifier{1.0f};
  interrupt_type kind{interrupt_type::do_not_play};
  std::vector<std::string> vec_clips;
  std::vector<std::string> vec_live_clips;
};

[[nodiscard]] std::optional<std::string> sound_pool_get_next(sound_pool& pool,
                                                             support::MersenneTwister& mt_rng);

struct sound_info {
  std::map<std::string, std::vector<std::string>> map_variants;
  std::map<std::string, std::vector<std::string>> map_prefixes;
  std::map<std::string, std::vector<std::string>> map_voices;
  std::map<std::string, std::vector<std::string>> map_notifications;
  std::string str_default_variant{".aud"};
  std::string str_default_prefix{};
  std::set<std::string> set_disable_variants;
  std::set<std::string> set_disable_prefixes;
  std::map<std::string, sound_pool> map_voice_pools;
  std::map<std::string, sound_pool> map_notification_pools;
};

[[nodiscard]] std::expected<sound_info, std::string> sound_info_from_yaml(
    const MiniYamlNode& node);

[[nodiscard]] std::vector<std::string> parse_string_csv(std::string_view str_value);

[[nodiscard]] std::optional<float> parse_float_value(std::string_view str_value);

[[nodiscard]] std::optional<interrupt_type> parse_interrupt_type(std::string_view str_value);

[[nodiscard]] std::expected<std::vector<std::pair<std::string, sound_info>>, std::string>
load_sound_infos(std::string_view str_path);

[[nodiscard]] std::string string_to_lower_ascii(std::string_view str_value);

}

namespace ora::game {

struct music_info {
  std::string str_filename;
  std::string str_title;
  bool b_hidden{false};
  float f4_volume_modifier{1.0f};
  std::int32_t int4_length{0};
  bool b_exists{false};
};

[[nodiscard]] std::expected<music_info, std::string> music_info_from_yaml(
    std::string_view str_key, const MiniYamlNode& value);

[[nodiscard]] std::expected<std::vector<std::pair<std::string, music_info>>, std::string>
load_music_infos(std::string_view str_path);

void music_infos_probe_exists(std::vector<std::pair<std::string, music_info>>& vec_tracks,
                              std::string_view str_dir);

}

namespace ora::game {

struct MapHeader {
  int int4_format{};
  std::string str_requires_mod;
  std::string str_title;
  std::string str_author;
  std::string str_tileset;
  int int4_size_x{};
  int int4_size_y{};
  int int4_bounds_x{};
  int int4_bounds_y{};
  int int4_bounds_w{};
  int int4_bounds_h{};
  std::uint32_t u4_visibility{};
  std::vector<std::string> vec_str_categories;
  std::size_t sz_players{};
  std::size_t sz_actors{};
};

[[nodiscard]] std::expected<MapHeader, std::string> map_header_parse(
    std::span<const std::uint8_t> span1u_yaml_bytes, std::string_view str_source_name);

[[nodiscard]] std::expected<int, std::string> map_scan_format(
    std::span<const std::uint8_t> span1u_yaml_bytes);

[[nodiscard]] std::expected<std::string, std::string> map_uid(const filesystem::ZipArchive& archive_zip);

[[nodiscard]] std::expected<std::string, std::string> map_info_dump(
    std::span<const std::uint8_t> span1u_oramap_bytes, std::string_view str_tileset_dir);

}

namespace ora::game {

[[nodiscard]] std::expected<std::vector<std::string>, std::string> manifest_rules_files(
    std::string_view str_mod_yaml_path);

struct ActorRules {
  std::string str_name;
  std::vector<std::string> vec_trait_keys;
};

struct RulesetModel {
  std::vector<ActorRules> vec_actors;
};

[[nodiscard]] std::expected<RulesetModel, std::string> ruleset_load_minimal(
    const std::vector<std::pair<std::string, std::string>>& vec_manifest_entries);

}
}
