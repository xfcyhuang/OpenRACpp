module ora.game;

import std;

namespace ora::game {

namespace {

const MiniYamlNode* node_with_key(const std::vector<MiniYamlNode>& vec_nodes,
                                  std::string_view str_key) {
  for (const MiniYamlNode& node : vec_nodes)
    if (node.str_key == str_key) return &node;
  return nullptr;
}

std::optional<bool> parse_bool_tryparse(std::string_view str_value) {
  std::string str_trimmed{string_to_lower_ascii(str_value)};
  while (!str_trimmed.empty() &&
         (str_trimmed.front() == ' ' || (str_trimmed.front() >= 0x09 && str_trimmed.front() <= 0x0d)))
    str_trimmed.erase(str_trimmed.begin());
  while (!str_trimmed.empty() &&
         (str_trimmed.back() == ' ' || (str_trimmed.back() >= 0x09 && str_trimmed.back() <= 0x0d)))
    str_trimmed.pop_back();
  if (str_trimmed == "true") return true;
  if (str_trimmed == "false") return false;
  return false;
}

}

std::expected<music_info, std::string> music_info_from_yaml(std::string_view str_key,
                                                            const MiniYamlNode& value) {
  music_info info{};
  info.str_title = value.str_value;

  for (const MiniYamlNode& child : value.vec_children) {
    if (child.str_key == "Hidden") {
      info.b_hidden = parse_bool_tryparse(child.str_value).value_or(false);
    } else if (child.str_key == "VolumeModifier") {
      const std::optional<float> opt_volume{parse_float_value(child.str_value)};
      if (!opt_volume.has_value())
        return std::unexpected{std::format(
            "FieldLoader: Cannot parse `{}` into field `VolumeModifier` of type `Single`",
            child.str_value)};
      info.f4_volume_modifier = *opt_volume;
    }
  }

  std::string str_extension{"aud"};
  std::string str_base{std::string{str_key}};
  if (const MiniYamlNode* ptr_ext{node_with_key(value.vec_children, "Extension")})
    str_extension = ptr_ext->str_value;
  if (const MiniYamlNode* ptr_filename{node_with_key(value.vec_children, "Filename")})
    str_base = ptr_filename->str_value;
  info.str_filename = str_base + "." + str_extension;

  return info;
}

std::expected<std::vector<std::pair<std::string, music_info>>, std::string> load_music_infos(
    std::string_view str_path) {
  auto res_nodes{miniyaml_from_file(str_path, true)};
  if (!res_nodes.has_value()) return std::unexpected{res_nodes.error()};
  std::vector<std::pair<std::string, music_info>> vec_out;
  std::set<std::string> set_seen;
  for (const MiniYamlNode& node : *res_nodes) {
    std::string str_lowered{string_to_lower_ascii(node.str_key)};
    if (!set_seen.insert(str_lowered).second)
      return std::unexpected{std::format(
          "LoadFromManifest<Manifest,Music>, duplicate values found for the following keys: {}",
          str_lowered)};
    auto res_info{music_info_from_yaml(node.str_key, node)};
    if (!res_info.has_value()) return std::unexpected{res_info.error()};
    vec_out.emplace_back(std::move(str_lowered), std::move(*res_info));
  }
  return vec_out;
}

void music_infos_probe_exists(std::vector<std::pair<std::string, music_info>>& vec_tracks,
                              std::string_view str_dir) {
  for (auto& [str_key, info] : vec_tracks) {
    std::error_code ec_probe{};
    const bool b_present{
        std::filesystem::exists(std::filesystem::path{str_dir} / info.str_filename, ec_probe)};
    info.b_exists = !ec_probe && b_present;
  }
}

}
