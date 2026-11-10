module ora.game.sound;

import std;
import ora.game;
import ora.game.miniyaml;
import ora.game.support;
import ora.platforms;

namespace ora::game {

std::expected<bool, std::string> sound_play_predefined(
    sound_facade& facade, sfx_sound_rules& rules, support::MersenneTwister& rng_cosmetic,
    sound_type kind, std::uint32_t u4_player, std::optional<sfx_voiced> opt_voiced,
    std::string_view str_type, std::string_view str_definition, const std::string* ptr_variant,
    bool b_relative, ora::platforms::wpos pos, float f4_volume_modifier,
    bool b_attenuate_volume) {
  if (str_definition.empty() || facade.b_disable_all_sounds ||
      (facade.b_disable_world_sounds && kind == sound_type::world))
    return false;

  sound_info* ptr_rules{nullptr};
  if (opt_voiced.has_value()) {
    const auto it{rules.map_voices.find(std::string{str_type})};
    if (it == rules.map_voices.end())
      return std::unexpected{std::format(
          "The given key '{}' was not present in the dictionary.", str_type)};
    ptr_rules = &it->second;
  } else {
    const auto it{rules.map_notifications.find(std::string{str_type})};
    if (it == rules.map_notifications.end())
      return std::unexpected{std::format(
          "The given key '{}' was not present in the dictionary.", str_type)};
    ptr_rules = &it->second;
  }
  sound_info& rules_info{*ptr_rules};

  const std::uint32_t u4_id{opt_voiced.has_value() ? opt_voiced->u4_actor_id : 0u};

  sound_pool* ptr_pool{nullptr};
  if (opt_voiced.has_value()) {
    const auto it{rules_info.map_voice_pools.find(std::string{str_definition})};
    if (it == rules_info.map_voice_pools.end())
      return std::unexpected{std::format("Can't find {} in voice pool.", str_definition)};
    ptr_pool = &it->second;
  } else {
    const auto it{rules_info.map_notification_pools.find(std::string{str_definition})};
    if (it == rules_info.map_notification_pools.end())
      return std::unexpected{std::format("Can't find {} in notification pool.", str_definition)};
    ptr_pool = &it->second;
  }
  sound_pool& pool{*ptr_pool};

  std::string str_suffix{rules_info.str_default_variant};
  std::string str_prefix{rules_info.str_default_prefix};

  const std::optional<std::string> opt_clip{sound_pool_get_next(pool, rng_cosmetic)};
  if (!opt_clip.has_value() || opt_clip->empty()) return false;

  if (ptr_variant != nullptr) {
    if (const auto it{rules_info.map_variants.find(*ptr_variant)};
        it != rules_info.map_variants.end() && !it->second.empty() &&
        !rules_info.set_disable_variants.contains(std::string{str_definition})) {
      const std::size_t sz_idx{static_cast<std::size_t>(u4_id) % it->second.size()};
      str_suffix = it->second[sz_idx];
    }
    if (const auto it{rules_info.map_prefixes.find(*ptr_variant)};
        it != rules_info.map_prefixes.end() && !it->second.empty() &&
        !rules_info.set_disable_prefixes.contains(std::string{str_definition})) {
      const std::size_t sz_idx{static_cast<std::size_t>(u4_id) % it->second.size()};
      str_prefix = it->second[sz_idx];
    }
  }

  const std::string str_name{str_prefix + *opt_clip + str_suffix};
  const std::uint32_t u4_actor_id{
      opt_voiced.has_value() && opt_voiced->b_selected ? 0u : u4_id};

  if (!str_name.empty() &&
      (u4_player == kSfxNoPlayer || u4_player == facade.u4_local_player)) {
    const float f4_volume{sound_facade_internal_sound_volume(facade) * f4_volume_modifier *
                          pool.f4_volume_modifier};

    const auto play_sound{[&]() -> std::optional<ora::platforms::sound> {
      return facade_play2d(facade, str_name, false, b_relative, pos, f4_volume,
                           b_attenuate_volume);
    }};

    if (pool.kind == interrupt_type::overlap) {
      if (!play_sound().has_value()) return false;
    } else if (!opt_voiced.has_value()) {
      if (const auto it{facade.map_current_notifications.find(str_name)};
          it != facade.map_current_notifications.end() &&
          !ora::platforms::sound_complete(facade.engine, it->second)) {
        if (pool.kind == interrupt_type::interrupt)
          facade_stop_sound(facade, &it->second);
        else if (pool.kind == interrupt_type::do_not_play) return false;
      }
      std::optional<ora::platforms::sound> snd{play_sound()};
      if (!snd.has_value()) return false;
      facade.map_current_notifications.insert_or_assign(str_name, *snd);
    } else {
      if (const auto it{facade.map_current_sounds.find(u4_actor_id)};
          it != facade.map_current_sounds.end() &&
          !ora::platforms::sound_complete(facade.engine, it->second)) {
        if (pool.kind == interrupt_type::interrupt)
          facade_stop_sound(facade, &it->second);
        else if (pool.kind == interrupt_type::do_not_play) return false;
      }
      std::optional<ora::platforms::sound> snd{play_sound()};
      if (!snd.has_value()) return false;
      facade.map_current_sounds.insert_or_assign(u4_actor_id, *snd);
    }
  }

  return true;
}

std::expected<bool, std::string> sound_play_notification(
    sound_facade& facade, sfx_sound_rules& rules, support::MersenneTwister& rng_cosmetic,
    std::uint32_t u4_player, std::string_view str_type, std::string_view str_notification,
    const std::string* ptr_variant) {
  if (str_type.empty() || str_notification.empty()) return false;
  return sound_play_predefined(facade, rules, rng_cosmetic, sound_type::ui, u4_player,
                               std::nullopt, string_to_lower_ascii(str_type), str_notification,
                               ptr_variant, true, ora::platforms::wpos{}, 1.0f, false);
}


namespace {

std::expected<bool, std::string> bridge_play_voice(
    sfx_context& ctx, const sfx_voiced& voiced, std::string_view str_voice_type,
    std::string_view str_phrase, std::string_view str_faction, float f4_volume) {
  const std::string str_variant{str_faction};
  return sound_play_predefined(*ctx.ptr_facade, *ctx.ptr_rules, *ctx.ptr_rng_cosmetic,
                               sound_type::world, kSfxNoPlayer, voiced, str_voice_type,
                               str_phrase, &str_variant, true, ora::platforms::wpos{},
                               f4_volume, true);
}

bool selection_contains(const sfx_context& ctx, std::uint32_t u4_actor_id) {
  return ctx.ptr_selection != nullptr &&
         std::find(ctx.ptr_selection->begin(), ctx.ptr_selection->end(), u4_actor_id) !=
             ctx.ptr_selection->end();
}

}

void sfx_context_set_tick(sfx_context& ctx, std::int32_t int4_tick) {
  ctx.int4_tick = int4_tick;
  if (ctx.ptr_facade != nullptr) facade_set_tick(*ctx.ptr_facade, int4_tick);
}

std::optional<std::string> sfx_random_or_default(const std::vector<std::string>& vec_items,
                                                 support::MersenneTwister& rng_cosmetic) {
  if (vec_items.empty()) return std::nullopt;
  return vec_items[static_cast<std::size_t>(next(rng_cosmetic,
                                                static_cast<std::int32_t>(vec_items.size())))];
}

void sfx_on_weapon_fired(sfx_context& ctx, const std::vector<std::string>& vec_report,
                         ora::platforms::wpos pos_muzzle) {
  if (ctx.ptr_facade == nullptr || vec_report.empty()) return;
  const std::optional<std::string> opt_name{sfx_random_or_default(vec_report,
                                                                  *ctx.ptr_rng_cosmetic)};
  if (!opt_name.has_value()) return;
  static_cast<void>(sound_play(*ctx.ptr_facade, sound_type::world, *opt_name, pos_muzzle));
}

void sfx_on_warhead_impact(sfx_context& ctx, const sfx_impact_sounds& sounds,
                           ora::platforms::wpos pos_impact) {
  if (ctx.ptr_facade == nullptr) return;
  static_cast<void>(sfx_random_or_default(sounds.vec_explosions, *ctx.ptr_rng_cosmetic));
  const std::optional<std::string> opt_impact{sfx_random_or_default(sounds.vec_impact_sounds,
                                                                    *ctx.ptr_rng_cosmetic)};
  if (!opt_impact.has_value()) return;
  if (next(*ctx.ptr_rng_cosmetic, 0, 100) >= sounds.int4_chance) return;
  static_cast<void>(sound_play(*ctx.ptr_facade, sound_type::world, *opt_impact, pos_impact));
}

void sfx_on_building_placed(sfx_context& ctx, std::uint32_t u4_player,
                            const std::vector<std::string>& vec_build_sounds,
                            ora::platforms::wpos pos) {
  if (ctx.ptr_facade == nullptr) return;
  for (const std::string& str_sound : vec_build_sounds)
    static_cast<void>(sound_play_to_player(*ctx.ptr_facade, sound_type::world, u4_player,
                                           str_sound, pos));
}

void sfx_on_selection(sfx_context& ctx, std::span<const sfx_select_candidate> vec_candidates) {
  if (ctx.ptr_facade == nullptr) return;
  for (const sfx_select_candidate& candidate : vec_candidates) {
    if (candidate.u4_owner != ctx.ptr_facade->u4_local_player || !candidate.b_in_world)
      continue;
    if (!candidate.b_has_voice || candidate.str_phrase.empty()) continue;
    const sfx_voiced voiced{.u4_actor_id = candidate.u4_actor_id,
                            .b_selected = selection_contains(ctx, candidate.u4_actor_id)};
    static_cast<void>(bridge_play_voice(ctx, voiced, candidate.str_voice_type,
                                        candidate.str_phrase, candidate.str_faction, 1.0f));
    break; 
  }
}

bool sfx_play_voice_for_orders(sfx_context& ctx, std::span<const sfx_order_voice> vec_orders) {
  if (ctx.ptr_facade == nullptr) return false;
  for (const sfx_order_voice& order : vec_orders) {
    if (!order.b_has_phrase || order.str_phrase.empty()) continue;
    const sfx_voiced voiced{.u4_actor_id = order.u4_actor_id,
                            .b_selected = selection_contains(ctx, order.u4_actor_id)};
    const auto res{bridge_play_voice(ctx, voiced, order.str_voice_type, order.str_phrase,
                                     order.str_faction, 1.0f)};
    if (res.has_value() && *res) return true;
  }
  return false;
}

void sfx_on_production_ready(sfx_context& ctx, std::uint32_t u4_owner,
                             std::string_view str_faction, std::string_view str_audio) {
  if (ctx.ptr_facade == nullptr || str_audio.empty()) return;
  const std::string str_variant{str_faction};
  static_cast<void>(sound_play_notification(*ctx.ptr_facade, *ctx.ptr_rules,
                                            *ctx.ptr_rng_cosmetic, u4_owner, "Speech",
                                            str_audio, &str_variant));
}

void sfx_on_production_blocked(sfx_context& ctx, std::uint32_t u4_owner,
                               std::string_view str_faction, std::string_view str_audio) {
  if (ctx.ptr_facade == nullptr || str_audio.empty()) return;
  const std::string str_variant{str_faction};
  static_cast<void>(sound_play_notification(*ctx.ptr_facade, *ctx.ptr_rules,
                                            *ctx.ptr_rng_cosmetic, u4_owner, "Speech",
                                            str_audio, &str_variant));
}

}
