export module ora.game.sound;

import std;
import ora.game;
import ora.game.miniyaml;
import ora.game.support;
import ora.platforms;

export {
namespace ora::game {

enum class sound_tick_event : std::uint8_t { k_none, k_music_complete };

constexpr std::uint32_t kSfxNoPlayer{0xffffffffu};

enum class sound_type : std::uint8_t { world, ui };

struct sound_facade {
  ora::platforms::sound_engine engine;
  bool b_music_playing{false};
  std::optional<ora::platforms::sound> opt_music{};
  std::optional<music_info> opt_current_music{};
  std::optional<ora::platforms::sound> opt_video{};
  SoundSettingsModel* ptr_settings{};
  float fp_sound_volume_modifier{1.0f};
  bool b_disable_all_sounds{false};
  bool b_disable_world_sounds{false};
  std::uint32_t u4_local_player{kSfxNoPlayer};
  std::int32_t int4_current_tick{0};
  std::map<std::string, ora::platforms::sound_source> map_sources;
  std::map<std::uint32_t, ora::platforms::sound> map_current_sounds;
  std::map<std::string, ora::platforms::sound> map_current_notifications;
  std::vector<std::string> vec_log;
};

[[nodiscard]] sound_facade make_sound_facade(std::string_view str_device, bool b_mute);
[[nodiscard]] sound_facade make_dummy_facade();

[[nodiscard]] sound_facade make_sound_facade_settings(std::string_view str_device, bool b_mute,
                                                      SoundSettingsModel& settings);

[[nodiscard]] float sound_facade_sound_volume(const sound_facade& facade);
[[nodiscard]] float sound_facade_music_volume(const sound_facade& facade);
[[nodiscard]] float sound_facade_video_volume(const sound_facade& facade);
[[nodiscard]] float sound_facade_internal_sound_volume(const sound_facade& facade);

void sound_facade_set_sound_volume(sound_facade& facade, float f4_volume);
void sound_facade_set_music_volume(sound_facade& facade, float f4_volume);
void sound_facade_set_video_volume(sound_facade& facade, float f4_volume);
void sound_facade_set_sound_volume_modifier(sound_facade& facade, float f4_modifier);

[[nodiscard]] bool sound_facade_music_playing(const sound_facade& facade);

void sound_facade_play_music_then(sound_facade& facade, const music_info& info);
void sound_facade_play_music(sound_facade& facade, const music_info& info, bool b_loop);
void sound_facade_play_music_resume(sound_facade& facade);
void sound_facade_stop_music(sound_facade& facade);
void sound_facade_pause_music(sound_facade& facade);

[[nodiscard]] sound_tick_event sound_facade_tick(sound_facade& facade);

void facade_set_tick(sound_facade& facade, std::int32_t int4_tick);

[[nodiscard]] std::string sfx_fmt_float_r(float f4_value);

[[nodiscard]] const ora::platforms::sound_source& facade_source_get(sound_facade& facade,
                                                                   std::string_view str_name);

[[nodiscard]] std::optional<ora::platforms::sound> facade_play2d(
    sound_facade& facade, std::string_view str_name, bool b_loop, bool b_relative,
    ora::platforms::wpos pos, float f4_volume, bool b_attenuate_volume);

void facade_stop_sound(sound_facade& facade, const ora::platforms::sound* ptr_snd);

[[nodiscard]] std::optional<ora::platforms::sound> sound_play(sound_facade& facade,
                                                              sound_type kind,
                                                              std::string_view str_name,
                                                              ora::platforms::wpos pos,
                                                              float f4_volume_modifier,
                                                              bool b_loop);
[[nodiscard]] std::optional<ora::platforms::sound> sound_play(sound_facade& facade,
                                                              sound_type kind,
                                                              std::string_view str_name,
                                                              ora::platforms::wpos pos,
                                                              float f4_volume_modifier);
[[nodiscard]] std::optional<ora::platforms::sound> sound_play(sound_facade& facade,
                                                              sound_type kind,
                                                              std::string_view str_name,
                                                              ora::platforms::wpos pos);
[[nodiscard]] std::optional<ora::platforms::sound> sound_play(sound_facade& facade,
                                                              sound_type kind,
                                                              std::string_view str_name);

[[nodiscard]] std::optional<ora::platforms::sound> sound_play_to_player(
    sound_facade& facade, sound_type kind, std::uint32_t u4_player, std::string_view str_name);
[[nodiscard]] std::optional<ora::platforms::sound> sound_play_to_player(
    sound_facade& facade, sound_type kind, std::uint32_t u4_player, std::string_view str_name,
    ora::platforms::wpos pos);

}

namespace ora::game {

struct sfx_sound_rules {
  std::map<std::string, sound_info> map_voices;
  std::map<std::string, sound_info> map_notifications;
};

struct sfx_voiced {
  std::uint32_t u4_actor_id{};
  bool b_selected{};
};

[[nodiscard]] std::expected<bool, std::string> sound_play_predefined(
    sound_facade& facade, sfx_sound_rules& rules, support::MersenneTwister& rng_cosmetic,
    sound_type kind, std::uint32_t u4_player, std::optional<sfx_voiced> opt_voiced,
    std::string_view str_type, std::string_view str_definition, const std::string* ptr_variant,
    bool b_relative, ora::platforms::wpos pos, float f4_volume_modifier,
    bool b_attenuate_volume);

[[nodiscard]] std::expected<bool, std::string> sound_play_notification(
    sound_facade& facade, sfx_sound_rules& rules, support::MersenneTwister& rng_cosmetic,
    std::uint32_t u4_player, std::string_view str_type, std::string_view str_notification,
    const std::string* ptr_variant);


struct sfx_context {
  sound_facade* ptr_facade{};
  sfx_sound_rules* ptr_rules{};
  support::MersenneTwister* ptr_rng_cosmetic{};
  std::int32_t int4_tick{0};
  const std::vector<std::uint32_t>* ptr_selection{nullptr};
};

void sfx_context_set_tick(sfx_context& ctx, std::int32_t int4_tick);

[[nodiscard]] std::optional<std::string> sfx_random_or_default(
    const std::vector<std::string>& vec_items, support::MersenneTwister& rng_cosmetic);

struct sfx_impact_sounds {
  std::vector<std::string> vec_explosions;
  std::vector<std::string> vec_impact_sounds;
  std::int32_t int4_chance{100};
};

void sfx_on_weapon_fired(sfx_context& ctx, const std::vector<std::string>& vec_report,
                         ora::platforms::wpos pos_muzzle);

void sfx_on_warhead_impact(sfx_context& ctx, const sfx_impact_sounds& sounds,
                           ora::platforms::wpos pos_impact);

void sfx_on_building_placed(sfx_context& ctx, std::uint32_t u4_player,
                            const std::vector<std::string>& vec_build_sounds,
                            ora::platforms::wpos pos);

struct sfx_select_candidate {
  std::uint32_t u4_actor_id{};
  std::uint32_t u4_owner{};
  bool b_in_world{};
  bool b_has_voice{};
  std::string str_voice_type;
  std::string str_phrase;
  std::string str_faction;
};

void sfx_on_selection(sfx_context& ctx, std::span<const sfx_select_candidate> vec_candidates);

struct sfx_order_voice {
  std::uint32_t u4_actor_id{};
  std::uint32_t u4_owner{};
  std::string str_voice_type;
  bool b_has_phrase{};
  std::string str_phrase;
  std::string str_faction;
};

[[nodiscard]] bool sfx_play_voice_for_orders(sfx_context& ctx,
                                             std::span<const sfx_order_voice> vec_orders);

void sfx_on_production_ready(sfx_context& ctx, std::uint32_t u4_owner,
                             std::string_view str_faction, std::string_view str_audio);
void sfx_on_production_blocked(sfx_context& ctx, std::uint32_t u4_owner,
                               std::string_view str_faction, std::string_view str_audio);

}

namespace ora::game {

struct music_playlist_info {
  std::string str_starting_music{};
  std::string str_background_music{};
  bool b_allow_mute_background_music{false};
};

struct music_player {
  sound_facade* ptr_facade{};
  SoundSettingsModel* ptr_settings{};
  std::vector<std::pair<std::string, music_info>> vec_library;
  std::vector<std::pair<std::string, music_info>> vec_playlist;
  std::vector<std::pair<std::string, music_info>> vec_random;
  std::optional<std::string> str_current_song{};
  std::optional<std::string> str_current_background_song{};
  bool b_current_song_is_background{false};
  bool b_is_music_available{false};
  bool b_allow_mute_background_music{false};
};

[[nodiscard]] std::vector<std::pair<std::string, music_info>> shuffle_playlist(
    const std::vector<std::pair<std::string, music_info>>& vec_playlist,
    support::MersenneTwister& mt_cosmetic);

[[nodiscard]] music_player make_music_player(
    sound_facade& facade, SoundSettingsModel& settings,
    const std::vector<std::pair<std::string, music_info>>& vec_library,
    support::MersenneTwister& mt_cosmetic, const music_playlist_info& info);

[[nodiscard]] const music_info* music_player_find(const music_player& player,
                                                  std::string_view str_key);

[[nodiscard]] std::optional<std::string> music_player_next_song(const music_player& player);
[[nodiscard]] std::optional<std::string> music_player_prev_song(const music_player& player);

void music_player_play(music_player& player);
void music_player_play_track(music_player& player, std::string_view str_key);
void music_player_stop(music_player& player);
void music_player_tick(music_player& player);

}
}
