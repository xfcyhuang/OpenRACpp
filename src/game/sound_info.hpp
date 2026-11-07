#pragma once

import std;

#include "game/miniyaml.hpp"
#include "game/support/mersenne_twister.hpp"

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
