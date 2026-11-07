#include "sound_info.hpp"

namespace ora::game {

namespace {

const MiniYamlNode* node_with_key(const std::vector<MiniYamlNode>& vec_nodes,
                                  std::string_view str_key) {
  for (const MiniYamlNode& node : vec_nodes)
    if (node.str_key == str_key) return &node;
  return nullptr;
}

std::optional<bool> parse_frozen_dictionary(const std::vector<MiniYamlNode>& vec_nodes,
                                            std::map<std::string, std::vector<std::string>>& map_out,
                                            std::string& str_error) {
  for (const MiniYamlNode& node : vec_nodes) {
    if (map_out.contains(node.str_key)) {
      str_error = std::format("FieldLoader: duplicate key `{}` in dictionary field", node.str_key);
      return std::nullopt;
    }
    map_out.emplace(node.str_key, parse_string_csv(node.str_value));
  }
  return true;
}

std::optional<sound_pool> parse_notification_pool(const MiniYamlNode& node,
                                                  std::string& str_error) {
  sound_pool pool{};
  if (const MiniYamlNode* ptr_volume{node_with_key(node.vec_children, "VolumeModifier")}) {
    const std::optional<float> opt_volume{parse_float_value(ptr_volume->str_value)};
    if (!opt_volume.has_value()) {
      str_error = std::format(
          "FieldLoader: Cannot parse `{}` into field `VolumeModifier` of type `Single`",
          ptr_volume->str_value);
      return std::nullopt;
    }
    pool.f4_volume_modifier = *opt_volume;
  }
  if (const MiniYamlNode* ptr_interrupt{node_with_key(node.vec_children, "InterruptType")}) {
    const std::optional<interrupt_type> opt_kind{
        parse_interrupt_type(ptr_interrupt->str_value)};
    if (!opt_kind.has_value()) {
      str_error = std::format(
          "FieldLoader: Cannot parse `{}` into field `InterruptType` of type `InterruptType`",
          ptr_interrupt->str_value);
      return std::nullopt;
    }
    pool.kind = *opt_kind;
  }
  pool.vec_clips = parse_string_csv(node.str_value);
  return pool;
}

}

std::string string_to_lower_ascii(std::string_view str_value) {
  std::string str_out;
  str_out.reserve(str_value.size());
  for (const char chr_c : str_value) {
    const char chr_lower{chr_c >= 'A' && chr_c <= 'Z'
                             ? static_cast<char>(chr_c - 'A' + 'a')
                             : chr_c};
    str_out.push_back(chr_lower);
  }
  return str_out;
}

std::vector<std::string> parse_string_csv(std::string_view str_value) {
  std::vector<std::string> vec_out;
  std::size_t sz_pos{0};
  while (sz_pos <= str_value.size()) {
    const std::size_t idx_comma{str_value.find(',', sz_pos)};
    const std::size_t sz_end{idx_comma == std::string_view::npos ? str_value.size() : idx_comma};
    std::string_view str_part{str_value.substr(sz_pos, sz_end - sz_pos)};
    while (!str_part.empty() &&
           (str_part.front() == ' ' || (str_part.front() >= 0x09 && str_part.front() <= 0x0d)))
      str_part.remove_prefix(1);
    while (!str_part.empty() &&
           (str_part.back() == ' ' || (str_part.back() >= 0x09 && str_part.back() <= 0x0d)))
      str_part.remove_suffix(1);
    if (!str_part.empty()) vec_out.emplace_back(str_part);
    if (idx_comma == std::string_view::npos) break;
    sz_pos = idx_comma + 1;
  }
  return vec_out;
}

std::optional<float> parse_float_value(std::string_view str_value) {
  std::string str_buffer;
  std::string_view str_raw{str_value};
  float f4_mult{1.0f};
  if (str_raw.find('%') != std::string_view::npos) {
    for (const char chr_c : str_raw)
      if (chr_c != '%') str_buffer.push_back(chr_c);
    str_raw = str_buffer;
    f4_mult = 0.01f;
  }
  std::size_t sz_pos{0};
  while (sz_pos < str_raw.size() &&
         (str_raw[sz_pos] == ' ' || (str_raw[sz_pos] >= 0x09 && str_raw[sz_pos] <= 0x0d)))
    ++sz_pos;
  std::size_t sz_end{str_raw.size()};
  while (sz_end > sz_pos &&
         (str_raw[sz_end - 1u] == ' ' || (str_raw[sz_end - 1u] >= 0x09 && str_raw[sz_end - 1u] <= 0x0d)))
    --sz_end;
  str_raw = str_raw.substr(sz_pos, sz_end - sz_pos);
  float f4_out{0.0f};
  const auto res_chars{std::from_chars(str_raw.data(), str_raw.data() + str_raw.size(), f4_out)};
  if (res_chars.ec != std::errc{} || res_chars.ptr != str_raw.data() + str_raw.size())
    return std::nullopt;
  f4_out *= f4_mult;
  return f4_out;
}

std::optional<interrupt_type> parse_interrupt_type(std::string_view str_value) {
  std::string str_trimmed{string_to_lower_ascii(str_value)};
  while (!str_trimmed.empty() &&
         (str_trimmed.front() == ' ' || (str_trimmed.front() >= 0x09 && str_trimmed.front() <= 0x0d)))
    str_trimmed.erase(str_trimmed.begin());
  while (!str_trimmed.empty() &&
         (str_trimmed.back() == ' ' || (str_trimmed.back() >= 0x09 && str_trimmed.back() <= 0x0d)))
    str_trimmed.pop_back();
  if (str_trimmed == "donotplay") return interrupt_type::do_not_play;
  if (str_trimmed == "interrupt") return interrupt_type::interrupt;
  if (str_trimmed == "overlap") return interrupt_type::overlap;
  if (str_trimmed == "0") return interrupt_type::do_not_play;
  if (str_trimmed == "1") return interrupt_type::interrupt;
  if (str_trimmed == "2") return interrupt_type::overlap;
  return std::nullopt;
}

std::optional<std::string> sound_pool_get_next(sound_pool& pool,
                                               support::MersenneTwister& mt_rng) {
  if (pool.vec_live_clips.empty()) pool.vec_live_clips = pool.vec_clips;
  if (pool.vec_live_clips.empty()) return std::nullopt;
  const std::int32_t int4_i{next(mt_rng, static_cast<std::int32_t>(pool.vec_live_clips.size()))};
  const std::size_t sz_i{static_cast<std::size_t>(int4_i)};
  std::string str_clip{pool.vec_live_clips[sz_i]};
  pool.vec_live_clips.erase(pool.vec_live_clips.begin() + static_cast<std::ptrdiff_t>(sz_i));
  return str_clip;
}

std::expected<sound_info, std::string> sound_info_from_yaml(const MiniYamlNode& node) {
  sound_info info{};
  std::string str_error{};

  for (const MiniYamlNode& child : node.vec_children) {
    if (child.str_key == "Variants" || child.str_key == "Prefixes" ||
        child.str_key == "Voices" || child.str_key == "Notifications") {
      std::map<std::string, std::vector<std::string>>* ptr_map{&info.map_variants};
      if (child.str_key == "Prefixes") ptr_map = &info.map_prefixes;
      if (child.str_key == "Voices") ptr_map = &info.map_voices;
      if (child.str_key == "Notifications") ptr_map = &info.map_notifications;
      if (!parse_frozen_dictionary(child.vec_children, *ptr_map, str_error).has_value())
        return std::unexpected{str_error};
    } else if (child.str_key == "DefaultVariant") {
      info.str_default_variant = child.str_value;
    } else if (child.str_key == "DefaultPrefix") {
      info.str_default_prefix = child.str_value;
    } else if (child.str_key == "DisableVariants" || child.str_key == "DisablePrefixes") {
      std::set<std::string>& set_out{child.str_key == "DisableVariants"
                                         ? info.set_disable_variants
                                         : info.set_disable_prefixes};
      for (const std::string& str_entry : parse_string_csv(child.str_value))
        set_out.insert(str_entry);
    }
  }

  for (const auto& [str_key, vec_clips] : info.map_voices) {
    sound_pool pool{};
    pool.f4_volume_modifier = 1.0f;
    pool.kind = interrupt_type::do_not_play;
    pool.vec_clips = vec_clips;
    info.map_voice_pools.emplace(str_key, std::move(pool));
  }

  if (const MiniYamlNode* ptr_notifications{node_with_key(node.vec_children, "Notifications")}) {
    for (const MiniYamlNode& pool_node : ptr_notifications->vec_children) {
      if (info.map_notification_pools.contains(pool_node.str_key))
        return std::unexpected{std::format(
            "FieldLoader: duplicate key `{}` in dictionary field", pool_node.str_key)};
      auto opt_pool{parse_notification_pool(pool_node, str_error)};
      if (!opt_pool.has_value()) return std::unexpected{str_error};
      info.map_notification_pools.emplace(pool_node.str_key, std::move(*opt_pool));
    }
  }

  return info;
}

std::expected<std::vector<std::pair<std::string, sound_info>>, std::string> load_sound_infos(
    std::string_view str_path) {
  auto res_nodes{miniyaml_from_file(str_path, true)};
  if (!res_nodes.has_value()) return std::unexpected{res_nodes.error()};
  std::vector<std::pair<std::string, sound_info>> vec_out;
  std::set<std::string> set_seen;
  for (const MiniYamlNode& node : *res_nodes) {
    std::string str_lowered{string_to_lower_ascii(node.str_key)};
    if (!set_seen.insert(str_lowered).second)
      return std::unexpected{std::format(
          "LoadFromManifest<Manifest,Voices>, duplicate values found for the following keys: {}",
          str_lowered)};
    auto res_info{sound_info_from_yaml(node)};
    if (!res_info.has_value()) return std::unexpected{res_info.error()};
    vec_out.emplace_back(std::move(str_lowered), std::move(*res_info));
  }
  return vec_out;
}

}
