import std;

#include <algorithm>

#include "game/input/hotkey.hpp"
#include "game/input/hotkey_registry.hpp"
#include "game/miniyaml.hpp"
#include "game/settings.hpp"
#include "test_util.hpp"
using ora::test::check;

#ifndef ORA_SETTINGS_GOLDEN_DIR
#define ORA_SETTINGS_GOLDEN_DIR "tests/golden/settings"
#endif

namespace {

namespace wg = ora::game;
namespace wig = ora::game::input;

std::string read_bytes(const std::string& str_path) {
  std::ifstream fs{str_path, std::ios::binary};
  if (!fs) return {};
  std::ostringstream ss;
  ss << fs.rdbuf();
  return ss.str();
}

void write_text_file(const std::string& str_path, std::string_view str_text) {
  std::ofstream fs{str_path, std::ios::binary | std::ios::trunc};
  fs.write(str_text.data(), static_cast<std::streamsize>(str_text.size()));
}

bool is_hex_char(char ch) {
  return (ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f') || (ch >= 'A' && ch <= 'F');
}

bool is_uuid_span(std::string_view str_span) {
  constexpr std::array<std::size_t, 5> kSegs{8, 4, 4, 4, 12};
  std::size_t sz_pos{0};
  for (std::size_t i{0}; i < kSegs.size(); ++i) {
    if (sz_pos + kSegs[i] > str_span.size()) return false;
    for (std::size_t j{0}; j < kSegs[i]; ++j)
      if (!is_hex_char(str_span[sz_pos + j])) return false;
    sz_pos += kSegs[i];
    if (i + 1 < kSegs.size()) {
      if (sz_pos >= str_span.size() || str_span[sz_pos] != '-') return false;
      ++sz_pos;
    }
  }
  return sz_pos == str_span.size();
}

std::string normalize_uuid(std::string str_text) {
  std::size_t sz_pos{0};
  while ((sz_pos = str_text.find("\tUUID: ", sz_pos)) != std::string::npos) {
    if ((sz_pos == 0 || str_text[sz_pos - 1] == '\n') && sz_pos + 7 + 36 <= str_text.size() &&
        is_uuid_span(std::string_view{str_text}.substr(sz_pos + 7, 36))) {
      str_text.replace(sz_pos + 7, 36, "<UUID>");
      sz_pos += 7 + 6;
      continue;
    }
    ++sz_pos;
  }
  return str_text;
}

std::string golden_text(std::string_view str_name) {
  return read_bytes(std::format("{}/{}", ORA_SETTINGS_GOLDEN_DIR, str_name));
}

const std::string kTempDir{
    (std::filesystem::temp_directory_path() / "ora-settings-test").string()};

std::string temp_path(std::string_view str_name) {
  return (std::filesystem::path{kTempDir} / str_name).string();
}

constexpr std::string_view kInputUnknown{
    "# top level user comment\n"
    "Player:\n\tName: Tester2\n\n"
    "UnknownSection:\n\tSomeKey: SomeValue\n\tNested:\n\t\tDeep: 5\n\n"
    "Sound:\n\tMute: True\n\tUnknownInKnown: keepme\n\tSoundVolume: 0.4\n"
    "\tMusicVolume: 0.3 # trailing comment\n\n"
    "Server: # trailing on section\n\tName: x\n"};

constexpr std::string_view kInputBlankSplit{"Sound:\n\tMute: True\n\n\tMusicVolume: 0.3\n"};

constexpr std::string_view kInputBadval{
    "Sound:\n\tSoundVolume: garbage\n\tMute: notabool\n"
    "Game:\n\tMouseScroll: garbage\n\tSelectionDeadzone: notanint\n"
    "Graphics:\n\tMode: garbage\n\tWindowedSize: notanint2\n"
    "Player:\n\tColor: nothex\n\tCustomColors: C820C8, zz\n"};

constexpr std::string_view kInputKeys{"Keys:\n\tSell: Q Ctrl\n\tAutocomplete: F2\n\n"
                                      "Sound:\n\tMute: True\n"};

void test_fresh_default() {
  auto res_model{wg::settings_load(temp_path("missing.yaml"))};
  check(res_model.has_value(), "load: missing file yields defaults");
  if (!res_model.has_value()) return;
  check(res_model->vec_yaml.size() == 6,
        "load: missing file yields six empty section nodes (GetOrCreate append)");

  const auto& player{res_model->player};
  check(player.str_name == "Commander" && player.color.u4_argb == 0xFFC82020u &&
            player.str_last_server == "localhost:1234" && player.vec_custom_colors.empty(),
        "defaults: Player table (Name/Color/LastServer/CustomColors)");
  const auto& game{res_model->game};
  check(game.str_platform == "Default" && game.b_viewport_edge_scroll &&
            game.int4_viewport_edge_scroll_margin == 5 &&
            game.mouse_control_style == wg::MouseControlStyle::kModern &&
            game.mouse_scroll == wg::MouseScrollType::kJoystick &&
            game.fp_viewport_edge_scroll_step == 30.0f && game.fp_zoom_speed == 0.04f &&
            game.int4_selection_deadzone == 24 &&
            game.status_bars == wg::StatusBarsType::kStandard &&
            game.target_lines == wg::TargetLinesType::kManual &&
            game.b_allow_downloading && game.str_auth_profile == "player.oraid" &&
            game.zoom_modifier == wg::Modifiers::kNone && game.b_fetch_news &&
            game.mp_game_filters ==
                (wg::MpGameFilters::kWaiting | wg::MpGameFilters::kEmpty |
                 wg::MpGameFilters::kProtected | wg::MpGameFilters::kStarted) &&
            game.b_enable_discord_service &&
            game.text_notification_pool_filters ==
                (wg::TextNotificationPoolFilters::kFeedback |
                 wg::TextNotificationPoolFilters::kTransients),
        "defaults: Game table spot checks");
  const auto& sound{res_model->sound};
  check(sound.fp_sound_volume == 0.5f && sound.fp_music_volume == 0.5f &&
            sound.fp_video_volume == 0.5f && !sound.b_shuffle && !sound.b_repeat &&
            sound.str_device.empty() && sound.b_cash_ticks && !sound.b_mute &&
            !sound.b_mute_background_music,
        "defaults: Sound table");
  const auto& graphics{res_model->graphics};
  check(graphics.mode == wg::WindowMode::kPseudoFullscreen && graphics.b_vsync &&
            graphics.fullscreen_size.int4_x == 0 && graphics.fullscreen_size.int4_y == 0 &&
            graphics.windowed_size.int4_x == 1024 && graphics.windowed_size.int4_y == 768 &&
            graphics.viewport_distance == wg::WorldViewport::kMedium &&
            graphics.fp_ui_scale == 1.0f && graphics.int4_max_framerate == 60 &&
            graphics.gl_profile == wg::GlProfile::kAutomatic,
        "defaults: Graphics table spot checks");
  const auto& server{res_model->server};
  check(server.int4_listen_port == 1234 && server.b_advertise_online &&
            server.int4_nat_port_mapping_lifetime == 36000 && server.str_map.empty() &&
            server.vec_ban.empty() && server.str_timestamp_format == "yyyy-MM-ddTHH:mm:ss" &&
            server.int4_vote_kicker_cooldown == 120000,
        "defaults: Server table spot checks");
  const auto& debug{res_model->debug};
  check(debug.int4_samples == 25 && debug.b_check_version && debug.b_send_system_information &&
            debug.str_uuid.size() == 36 && debug.fp_long_tick_threshold_ms == 1.0f,
        "defaults: Debug table spot checks (UUID is 36-char guid shape)");

  auto res_save{wg::settings_save(*res_model, temp_path("fresh.yaml"))};
  check(res_save.has_value(), "save: fresh default writes");
  check(normalize_uuid(read_bytes(temp_path("fresh.yaml"))) == golden_text("fresh-default.yaml"),
        "golden: fresh-default.yaml matches oracle (Debug.UUID only + trailing spacer)");
}

void test_all_types_and_roundtrip() {
  auto res_model{wg::settings_load(temp_path("missing2.yaml"))};
  check(res_model.has_value(), "all-types: model loads");
  if (!res_model.has_value()) return;
  wg::SettingsModel& sm{*res_model};
  sm.player.str_name = "Tester";
  sm.player.color.u4_argb = 0xFFC820C8u;
  sm.player.vec_custom_colors = {wg::SettingsColorModel{0xFFC820C8u},
                                 wg::SettingsColorModel{0xFF00FF80u}};
  sm.game.mouse_scroll = wg::MouseScrollType::kStandard;
  sm.game.mp_game_filters = wg::MpGameFilters::kWaiting | wg::MpGameFilters::kProtected;
  sm.game.zoom_modifier = wg::Modifiers::kShift;
  sm.sound.fp_sound_volume = 0.75f;
  sm.sound.str_device = "OpenAL Soft on Speakers";
  sm.sound.b_mute = true;
  sm.graphics.mode = wg::WindowMode::kWindowed;
  sm.graphics.windowed_size = wg::SettingsInt2Model{1600, 900};
  sm.graphics.fp_ui_scale = 1.25f;
  sm.server.str_name = "My # Server";
  sm.server.vec_ban = {"1.2.3.4", "5.6.7.8"};
  sm.server.vec_profile_id_whitelist = {1, 3, 4, 5};
  sm.debug.b_perf_text = true;
  sm.debug.str_uuid = "021de054-4aa4-470e-8e3c-dc006dc1312b";

  auto res_save{wg::settings_save(sm, temp_path("all-types.yaml"))};
  check(res_save.has_value(), "all-types: save writes");
  const std::string str_file{read_bytes(temp_path("all-types.yaml"))};
  check(normalize_uuid(str_file) == golden_text("all-types.yaml"),
        "golden: all-types.yaml matches oracle (12-type formats, section order, spacers)");
  check(str_file.find("Name: My \\# Server") != std::string::npos,
        "all-types: `#` written escaped");
  check(str_file.find("MPGameFilters: Waiting, Protected") != std::string::npos,
        "all-types: flags enum declaration order");
  check(str_file.find("WindowedSize: 1600,900") != std::string::npos,
        "all-types: int2 without spaces");
  check(str_file.find("ProfileIDWhitelist: 1, 3, 4, 5") != std::string::npos,
        "all-types: int set joined with ', ' (ascending insert = FrozenSet iteration order)");
  check(str_file.find("UIScrollSpeed") == std::string::npos &&
            str_file.find("MusicVolume") == std::string::npos &&
            str_file.find("ListenPort") == std::string::npos,
        "all-types: default-valued keys are omitted (only-non-default save semantics)");

  auto res_reload{wg::settings_load(temp_path("all-types.yaml"))};
  check(res_reload.has_value(), "roundtrip: reload of saved file");
  if (!res_reload.has_value()) return;
  auto res_resave{wg::settings_save(*res_reload, temp_path("roundtrip.yaml"))};
  check(res_resave.has_value(), "roundtrip: resave writes");
  check(read_bytes(temp_path("roundtrip.yaml")) == str_file,
        "roundtrip: write-read-write byte identity (raw, UUID passes through)");
  check(normalize_uuid(read_bytes(temp_path("roundtrip.yaml"))) == golden_text("roundtrip.yaml"),
        "golden: roundtrip.yaml matches oracle (identical bytes to all-types)");
}

void test_unknown_preserved() {
  write_text_file(temp_path("unknown.yaml"), kInputUnknown);
  auto res_model{wg::settings_load(temp_path("unknown.yaml"))};
  check(res_model.has_value(), "unknown: loads");
  if (!res_model.has_value()) return;
  wg::MiniYamlNode* ptr_unknown{wg::settings_find_section(*res_model, "UnknownSection")};
  check(ptr_unknown != nullptr && ptr_unknown->vec_children.size() == 2 &&
            ptr_unknown->vec_children[1].str_key == "Nested" &&
            ptr_unknown->vec_children[1].vec_children.size() == 1 &&
            ptr_unknown->vec_children[1].vec_children[0].str_value == "5",
        "unknown: unknown section subtree preserved (SomeKey + Nested.Deep)");
  check(res_model->sound.b_mute && res_model->sound.fp_sound_volume == 0.4f,
        "unknown: known fields loaded");
  check(wg::settings_find_section(*res_model, "UnknownSection") ==
                wg::settings_find_section(*res_model, "UnknownSection"),
        "unknown: find_section stable");

  auto res_save{wg::settings_save(*res_model, temp_path("unknown-out.yaml"))};
  check(res_save.has_value(), "unknown: save writes");
  const std::string str_out{normalize_uuid(read_bytes(temp_path("unknown-out.yaml")))};
  check(str_out == golden_text("unknown-preserved.yaml"),
        "golden: unknown-preserved.yaml matches oracle");
  check(str_out.find("# top level user comment") == std::string::npos,
        "unknown: top-level standalone comment dropped (no children filter)");
  check(str_out.find("Server: # trailing on section") != std::string::npos,
        "unknown: section-line trailing comment kept");
  check(str_out.find("MusicVolume: 0.3 # trailing comment") == std::string::npos &&
            str_out.find("MusicVolume: 0.3") != std::string::npos,
        "unknown: known-field trailing comment lost (Commit rebuild)");
  check(str_out.find("UnknownInKnown: keepme") != std::string::npos,
        "unknown: unknown key inside known section kept in place");
  const std::size_t idx_sound{str_out.find("Sound:")};
  const std::size_t idx_volume{str_out.find("SoundVolume: 0.4")};
  const std::size_t idx_music{str_out.find("MusicVolume: 0.3")};
  const std::size_t idx_mute{str_out.find("Mute: True")};
  check(idx_sound != std::string::npos && idx_volume > idx_sound && idx_music > idx_volume &&
            idx_mute > idx_music,
        "unknown: Commit re-adds known fields after untouched unknown keys (declaration order)");
}

void test_blank_split() {
  write_text_file(temp_path("blank.yaml"), kInputBlankSplit);
  auto res_model{wg::settings_load(temp_path("blank.yaml"))};
  check(res_model.has_value(), "blank-split: loads");
  if (!res_model.has_value()) return;
  check(res_model->sound.b_mute && res_model->sound.fp_music_volume == 0.5f,
        "blank-split: MusicVolume below blank line NOT loaded (stays default 0.5)");
  auto res_save{wg::settings_save(*res_model, temp_path("blank-out.yaml"))};
  check(res_save.has_value(), "blank-split: save writes");
  const std::string str_out{normalize_uuid(read_bytes(temp_path("blank-out.yaml")))};
  check(str_out == golden_text("blank-split.yaml"),
        "golden: blank-split.yaml matches oracle (double blank line + null-key subtree)");
}

void test_badval_substituted() {
  write_text_file(temp_path("badval.yaml"), kInputBadval);
  std::vector<std::string> vec_warnings;
  wg::SettingsLoadOptions opts{};
  opts.ptr_warnings = &vec_warnings;
  auto res_model{wg::settings_load(temp_path("badval.yaml"), opts)};
  check(res_model.has_value(), "badval: loads (bad values warn + substitute, never fail)");
  if (!res_model.has_value()) return;

  check(vec_warnings.size() == 8, "badval: 8 warnings emitted");
  std::string str_warnings;
  for (const std::string& str_line : vec_warnings) str_warnings += str_line + "\n";
  check(str_warnings == golden_text("badval-warnings.txt"),
        "golden: badval-warnings.txt matches oracle line by line (order = section ctor order)");

  check(res_model->sound.fp_sound_volume == 0.0f,
        "zero-table: float bad value -> 0 (not declared default 0.5)");
  check(!res_model->sound.b_mute, "zero-table: bool bad value -> false");
  check(res_model->game.mouse_scroll == wg::MouseScrollType::kDisabled,
        "zero-table: enum bad value -> first member (Disabled, not Joystick)");
  check(res_model->game.int4_selection_deadzone == 0, "zero-table: int bad value -> 0");
  check(res_model->graphics.mode == wg::WindowMode::kWindowed,
        "zero-table: WindowMode bad value -> first member (Windowed, not PseudoFullscreen)");
  check(res_model->graphics.windowed_size.int4_x == 0 && res_model->graphics.windowed_size.int4_y == 0,
        "zero-table: int2 bad value -> (0,0)");
  check(res_model->player.color.u4_argb == 0u, "zero-table: Color bad value -> 00000000");
  check(res_model->player.vec_custom_colors.size() == 2 &&
            res_model->player.vec_custom_colors[0].u4_argb == 0xFFC820C8u &&
            res_model->player.vec_custom_colors[1].u4_argb == 0u,
        "zero-table: bad color ARRAY element -> zero element, good element kept");

  auto res_save{wg::settings_save(*res_model, temp_path("badval-out.yaml"))};
  check(res_save.has_value(), "badval: save writes");
  const std::string str_out{normalize_uuid(read_bytes(temp_path("badval-out.yaml")))};
  check(str_out == golden_text("badval-substituted.yaml"),
        "golden: badval-substituted.yaml matches oracle (zero values flushed, Mute key gone)");
  check(str_out.find("Mute:") == std::string::npos,
        "badval: substituted False == default -> key absent from file");
  check(str_out.find("SoundVolume: 0") != std::string::npos &&
            str_out.find("Mode: Windowed") != std::string::npos,
        "badval: zero values != defaults -> written back (self-cleaning file)");
}

void test_cliargs() {
  std::vector<std::string> vec_args{std::string{"Sound.Mute=True"},
                                    std::string{"Game.ZoomModifier=Shift"}};
  wg::SettingsLoadOptions opts{};
  opts.vec_args = vec_args;
  auto res_model{wg::settings_load(temp_path("missing3.yaml"), opts)};
  check(res_model.has_value(), "cliargs: loads");
  if (!res_model.has_value()) return;
  check(res_model->sound.b_mute && res_model->game.zoom_modifier == wg::Modifiers::kShift,
        "cliargs: overrides applied on fresh model");
  auto res_save{wg::settings_save(*res_model, temp_path("cliargs-out.yaml"))};
  check(res_save.has_value(), "cliargs: save writes");
  check(normalize_uuid(read_bytes(temp_path("cliargs-out.yaml"))) == golden_text("cliargs.yaml"),
        "golden: cliargs.yaml matches oracle (Game before Sound = section creation order)");

  std::vector<std::string> vec_args2{std::string{"Sound.Mute=notabool"}};
  wg::SettingsLoadOptions opts2{};
  opts2.vec_args = vec_args2;
  std::vector<std::string> vec_warnings;
  opts2.ptr_warnings = &vec_warnings;
  auto res_bad{wg::settings_load(temp_path("missing4.yaml"), opts2)};
  check(res_bad.has_value() && !res_bad->sound.b_mute && vec_warnings.size() == 1 &&
            vec_warnings[0].find("Cannot parse `notabool` into field `Mute`") != std::string::npos,
        "cliargs: bad CLI value warns + substitutes zero (upstream window covers CLI loop)");
}

void test_keys_rebind() {
  write_text_file(temp_path("keys.yaml"), kInputKeys);
  auto res_model{wg::settings_load(temp_path("keys.yaml"))};
  check(res_model.has_value(), "keys: loads");
  if (!res_model.has_value()) return;
  wg::MiniYamlNode* ptr_keys{wg::settings_find_section(*res_model, "Keys")};
  check(ptr_keys != nullptr && ptr_keys->vec_children.size() == 2 &&
            ptr_keys->vec_children[0].str_key == "Sell" &&
            ptr_keys->vec_children[0].str_value == "Q Ctrl",
        "keys: Keys section loaded untouched (no-field section, unknown-key semantics)");

  auto res_save{wg::settings_save(*res_model, temp_path("keys-out.yaml"))};
  check(res_save.has_value(), "keys: save writes");
  check(normalize_uuid(read_bytes(temp_path("keys-out.yaml"))) == golden_text("keys-rebind.yaml"),
        "golden: keys-rebind.yaml matches oracle (Commit no-op on no-field section)");

  wig::HotkeyRegistryModel reg;
  reg.vec_definitions.push_back(wig::HotkeyDefinitionModel{
      .str_name = "Sell", .hk_default = {wig::Keycode::kZ, wig::ModifierKeys::kNone},
      .str_description = {}, .vec_types = {}, .vec_contexts = {"player"},
      .b_readonly = false, .b_has_duplicates = false});
  reg.vec_keys.push_back(reg.vec_definitions[0].hk_default);
  reg.vec_definitions.push_back(wig::HotkeyDefinitionModel{
      .str_name = "Autocomplete", .hk_default = {wig::Keycode::kTab, wig::ModifierKeys::kNone},
      .str_description = {}, .vec_types = {}, .vec_contexts = {"chat"},
      .b_readonly = true, .b_has_duplicates = false});
  reg.vec_keys.push_back(reg.vec_definitions[1].hk_default);

  std::vector<wg::MiniYamlNode> vec_nodes;
  for (const wg::MiniYamlNode& node_child : ptr_keys->vec_children) vec_nodes.push_back(node_child);
  auto res_apply{wig::hotkey_registry_apply_settings(reg, vec_nodes)};
  check(res_apply.has_value(), "keys: override applies (D059: valid values)");
  check(wig::hotkey_resolve(reg, "Sell").key == wig::Keycode::kQ &&
            wig::hotkey_resolve(reg, "Sell").mods == wig::ModifierKeys::kCtrl,
        "keys: Sell overridden to Q Ctrl");
  check(wig::hotkey_resolve(reg, "Autocomplete").key == wig::Keycode::kTab,
        "keys: readonly Autocomplete override skipped");

  std::vector<wg::MiniYamlNode> vec_bad;
  wg::MiniYamlNode node_bad;
  node_bad.str_key = "Sell";
  node_bad.str_value = "GARBAGE_KEYS";
  vec_bad.push_back(node_bad);
  auto res_bad{wig::hotkey_registry_apply_settings(reg, vec_bad)};
  check(!res_bad.has_value() &&
            res_bad.error() ==
                "FieldLoader: Cannot parse `GARBAGE_KEYS` into field `Sell` of type `OpenRA.Hotkey`",
        "D059: bad Keys value = error refuse, upstream message replicated");
  check(wig::hotkey_resolve(reg, "Sell").key == wig::Keycode::kQ,
        "D059: registry unchanged after refused load (validate-then-apply)");

  std::vector<wg::MiniYamlNode> vec_readonly_bad;
  wg::MiniYamlNode node_readonly_bad;
  node_readonly_bad.str_key = "Autocomplete";
  node_readonly_bad.str_value = "GARBAGE_KEYS";
  vec_readonly_bad.push_back(node_readonly_bad);
  wg::MiniYamlNode node_unknown_bad;
  node_unknown_bad.str_key = "NoSuchHotkey";
  node_unknown_bad.str_value = "!";
  vec_readonly_bad.push_back(node_unknown_bad);
  check(wig::hotkey_registry_apply_settings(reg, vec_readonly_bad).has_value(),
        "D059: readonly/unknown-name nodes skipped before parse (upstream loop shape)");

  check(wig::hotkey_set(reg, "Sell", {wig::Keycode::kW, wig::ModifierKeys::kNone}),
        "keys: rebind Sell=W None");
  wg::settings_keys_assign(*res_model, reg.vec_settings);
  ptr_keys = wg::settings_find_section(*res_model, "Keys");
  check(ptr_keys != nullptr && ptr_keys->vec_children.size() == 2 &&
            ptr_keys->vec_children[1].str_key == "Sell" &&
            ptr_keys->vec_children[1].str_value == "W None" &&
            ptr_keys->vec_children[0].str_key == "Autocomplete",
        "keys: assign = RemoveAll+append per entry (upstream Set semantics, order preserved)");
  auto res_save2{wg::settings_save(*res_model, temp_path("keys-out2.yaml"))};
  check(res_save2.has_value(), "keys: resave writes");
  const std::string str_out2{read_bytes(temp_path("keys-out2.yaml"))};
  check(str_out2.find("\tSell: W None\r\n") != std::string::npos &&
            str_out2.find("\tAutocomplete: F2\r\n") != std::string::npos,
        "keys: rebind persisted to file alongside file-loaded nodes");
}

void test_float_formats() {
  constexpr std::array<float, 27> kValues{
      0.5f,   0.75f,   30.f,          50.f,        1.f,        0.04f,  0.1f,
      1.25f,  2.5f,    16777216.f,    1e-8f,       0.0001f,    0.00001f, 0.000001f,
      100000.f, 1000000.f, 10000000.f, 100000000.f, 1000000000.f, 123456.789f, 0.1f + 0.2f,
      -0.5f,  3.4028235e38f, 1e-5f,   1234.5678f,  99999999.f, 999999999.f};
  std::string str_out;
  for (const float fp_value : kValues) str_out += wg::settings_format_float(fp_value) + "\n";
  check(str_out == golden_text("float-formats.txt"),
        "golden: float-formats.txt matches oracle (invariant shortest round-trip anchors)");

  auto res_model{wg::settings_load(temp_path("missing5.yaml"))};
  check(res_model.has_value(), "float: model loads");
  if (!res_model.has_value()) return;
  res_model->debug.fp_long_tick_threshold_ms = 1e-8f;
  auto res_save{wg::settings_save(*res_model, temp_path("float-anchor.yaml"))};
  check(res_save.has_value() &&
            read_bytes(temp_path("float-anchor.yaml")).find("LongTickThresholdMs: 1E-08") !=
                std::string::npos,
        "float: 1E-08 anchor via save path (scientific form through public seam)");
}

void test_error_paths() {
  write_text_file(temp_path("dup.yaml"), "Sound:\n\tMute: True\n\tMute: False\n");
  auto res_dup{wg::settings_load(temp_path("dup.yaml"))};
  check(!res_dup.has_value() &&
            res_dup.error().starts_with("Duplicate key 'Mute' in "),
        "error: duplicate key in known section = load refused (upstream InvalidDataException)");

  write_text_file(temp_path("nullkey.yaml"),
                  "Sound:\n\tMute: True\n\t# standalone nested comment\n\tMusicVolume: 0.3\n");
  auto res_null{wg::settings_load(temp_path("nullkey.yaml"))};
  check(!res_null.has_value() &&
            res_null.error() == "Value cannot be null. (Parameter 'key')",
        "error: standalone comment inside known section = load refused (upstream ArgumentNullException, probe 4b)");

  write_text_file(temp_path("indent.yaml"), "Sound:\n\t\tMute: True\n");
  auto res_indent{wg::settings_load(temp_path("indent.yaml"))};
  check(!res_indent.has_value() &&
            res_indent.error().starts_with("Bad indent in miniyaml at "),
        "error: bad indent propagates from miniyaml parse");

  write_text_file(temp_path("unknown-section-comment.yaml"),
                  "UnknownSection:\n\t# nested comment is fine here\n\tKey: 1\n");
  auto res_unknown_comment{wg::settings_load(temp_path("unknown-section-comment.yaml"))};
  check(res_unknown_comment.has_value(),
        "error: nested comment inside UNKNOWN section survives (only known sections validate)");
}

}

int main() {
  std::error_code ec{};
  std::filesystem::remove_all(kTempDir, ec);
  std::filesystem::create_directories(kTempDir);

  test_fresh_default();
  test_all_types_and_roundtrip();
  test_unknown_preserved();
  test_blank_split();
  test_badval_substituted();
  test_cliargs();
  test_keys_rebind();
  test_float_formats();
  test_error_paths();

  return ora::test::test_exit_code("settings");
}
