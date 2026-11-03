#include "game/ruleset.hpp"

namespace ora::game {

namespace {

constexpr char kAbstractActorPrefix{'^'};

// ToLowerInvariant 的 ASCII 面（语料全 ASCII；与 D005 同口径）
std::string to_lower_ascii(std::string_view str_in) {
  std::string str_out;
  str_out.reserve(str_in.size());
  for (const char ch : str_in)
    str_out.push_back(ch >= 'A' && ch <= 'Z' ? static_cast<char>(ch - 'A' + 'a') : ch);
  return str_out;
}

}  // namespace

std::expected<std::vector<std::string>, std::string> manifest_rules_files(
    std::string_view str_mod_yaml_path) {
  auto res_nodes{miniyaml_from_file(str_mod_yaml_path, true)};
  if (!res_nodes.has_value()) return std::unexpected(res_nodes.error());

  // 上游 new MiniYaml(null, MiniYaml.Merge([nodes])).ToDictionary()——
  // mod.yaml 单文件也走完整 Merge（跨文件继承/删除语义），输出顶层键唯一，
  // 线性查首个 "Rules" 与字典查找等价
  auto res_merged{miniyaml_merge({std::move(*res_nodes)})};
  if (!res_merged.has_value()) return std::unexpected(res_merged.error());

  for (const MiniYamlNode& node : *res_merged)
    if (node.str_key == "Rules") {
      std::vector<std::string> vec_files;
      vec_files.reserve(node.vec_children.size());
      for (const MiniYamlNode& child : node.vec_children)
        vec_files.push_back(child.str_key);
      return vec_files;
    }

  return std::vector<std::string>{};  // 上游 YamlList：区块缺失 → 空列表
}

std::expected<RulesetModel, std::string> ruleset_load_minimal(
    const std::vector<std::pair<std::string, std::string>>& vec_manifest_entries) {
  std::vector<std::vector<MiniYamlNode>> vec_sources;
  vec_sources.reserve(vec_manifest_entries.size());
  for (const auto& [str_name, str_path] : vec_manifest_entries) {
    auto res_file{miniyaml_from_file_named(str_path, str_name, true)};
    if (!res_file.has_value()) return std::unexpected(res_file.error());
    vec_sources.push_back(std::move(*res_file));
  }

  auto res_merged{miniyaml_merge(vec_sources)};
  if (!res_merged.has_value()) return std::unexpected(res_merged.error());

  RulesetModel model;
  std::set<std::string> set_lower_seen;
  for (const MiniYamlNode& node : *res_merged) {
    if (node.str_key.starts_with(kAbstractActorPrefix)) continue;  // ^ 过滤在 Merge 之后

    if (!set_lower_seen.insert(to_lower_ascii(node.str_key)).second)
      return std::unexpected(
          "LoadFromManifest<Manifest,Rules>, duplicate values found for the following keys: " +
          node.str_key);

    ActorRules actor;
    actor.str_name = node.str_key;
    actor.vec_trait_keys.reserve(node.vec_children.size());
    for (const MiniYamlNode& child : node.vec_children)
      actor.vec_trait_keys.push_back(child.str_key);
    model.vec_actors.push_back(std::move(actor));
  }
  return model;
}

}  // namespace ora::game
