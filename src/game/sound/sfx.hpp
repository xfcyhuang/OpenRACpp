#pragma once

import std;

#include "game/sound/sound.hpp"
#include "game/sound_info.hpp"

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
