#include "game/input/hotkey_registry.hpp"

#include "game/miniyaml_merge.hpp"

namespace ora::game::input {

namespace {

constexpr std::string_view kContextPrefix{"hotkey-context."};

const MiniYamlNode* find_child(const std::vector<MiniYamlNode>& vec_nodes,
                               std::string_view str_key) {
  for (const MiniYamlNode& node : vec_nodes)
    if (node.str_key == str_key) return &node;
  return nullptr;
}

std::vector<std::string> split_comma_list(std::string_view str_text) {
  std::vector<std::string> vec_out;
  std::size_t idx_begin{0};
  while (idx_begin <= str_text.size()) {
    const std::size_t idx_comma{str_text.find(',', idx_begin)};
    std::string_view str_token{str_text.substr(idx_begin, idx_comma == std::string_view::npos
                                                           ? std::string_view::npos
                                                           : idx_comma - idx_begin)};
    while (!str_token.empty() && (str_token.front() == ' ' || str_token.front() == '\t'))
      str_token.remove_prefix(1);
    while (!str_token.empty() && (str_token.back() == ' ' || str_token.back() == '\t'))
      str_token.remove_suffix(1);
    if (!str_token.empty()) vec_out.emplace_back(str_token);
    if (idx_comma == std::string_view::npos) break;
    idx_begin = idx_comma + 1;
  }
  return vec_out;
}

std::optional<bool> parse_bool(std::string_view str_text) {
  if (str_text == "True" || str_text == "true") return true;
  if (str_text == "False" || str_text == "false") return false;
  return std::nullopt;
}

// 上游 HotkeyDefinition ctor（HotkeyDefinition.cs:36-64）。C++ 编译目标 Windows →
// Platform 覆盖只认 "Windows" 子键；OSX/Linux 子键解析存在但必须忽略（笔记 §7-5）。
std::expected<HotkeyDefinitionModel, std::string> parse_definition(const MiniYamlNode& node) {
  HotkeyDefinitionModel def;
  def.str_name = node.str_key;

  if (!node.str_value.empty()) {
    auto res{hotkey_try_parse(node.str_value)};
    if (!res.has_value())
      return std::unexpected(std::format("hotkey {}: {}", node.str_key, res.error()));
    def.hk_default = *res;
  }

  if (const MiniYamlNode* node_desc{find_child(node.vec_children, "Description")})
    def.str_description = node_desc->str_value;

  if (const MiniYamlNode* node_types{find_child(node.vec_children, "Types")})
    def.vec_types = split_comma_list(node_types->str_value);

  if (const MiniYamlNode* node_contexts{find_child(node.vec_children, "Contexts")})
    for (const std::string& str_context : split_comma_list(node_contexts->str_value))
      def.vec_contexts.push_back(std::string(kContextPrefix) + str_context);

  if (const MiniYamlNode* node_platform{find_child(node.vec_children, "Platform")})
    if (const MiniYamlNode* node_override{
            find_child(node_platform->vec_children, "Windows")}) {
      auto res{hotkey_try_parse(node_override->str_value)};
      if (!res.has_value())
        return std::unexpected(std::format("hotkey {} platform: {}", node.str_key, res.error()));
      def.hk_default = *res;
    }

  if (const MiniYamlNode* node_readonly{find_child(node.vec_children, "Readonly")}) {
    const auto opt{parse_bool(node_readonly->str_value)};
    if (!opt.has_value())
      return std::unexpected(std::format("hotkey {}: bad Readonly value `{}'", node.str_key,
                                         node_readonly->str_value));
    def.b_readonly = *opt;
  }

  return def;
}

bool contexts_overlap(const std::vector<std::string>& vec_a, const std::vector<std::string>& vec_b) {
  for (const std::string& str_a : vec_a)
    for (const std::string& str_b : vec_b)
      if (str_a == str_b) return true;
  return false;
}

}

std::expected<HotkeyRegistryModel, std::string> hotkey_registry_load(
    const std::vector<std::vector<MiniYamlNode>>& vec_files) {
  auto res_merged{miniyaml_merge(vec_files)};
  if (!res_merged.has_value()) return std::unexpected(res_merged.error());

  HotkeyRegistryModel reg;
  for (const MiniYamlNode& node : *res_merged) {
    auto res_def{parse_definition(node)};
    if (!res_def.has_value()) return std::unexpected(res_def.error());

    if (const std::optional<std::size_t> idx{hotkey_registry_find(reg, node.str_key)}) {
      reg.vec_definitions[*idx] = std::move(*res_def);
      reg.vec_keys[*idx] = reg.vec_definitions[*idx].hk_default;
      continue;
    }
    reg.vec_definitions.push_back(std::move(*res_def));
    reg.vec_keys.push_back(reg.vec_definitions.back().hk_default);
  }

  hotkey_registry_recompute_duplicates(reg);
  return reg;
}

void hotkey_registry_apply_settings(HotkeyRegistryModel& reg,
                                    std::span<const MiniYamlNode> vec_keys_nodes) {
  for (const MiniYamlNode& node : vec_keys_nodes) {
    const std::optional<std::size_t> idx{hotkey_registry_find(reg, node.str_key)};
    if (!idx.has_value()) continue;
    if (reg.vec_definitions[*idx].b_readonly) continue;

    auto res{hotkey_try_parse(node.str_value)};
    if (!res.has_value()) {
      std::println("hotkeys: cannot parse `{}' for {} ({}); keeping default", node.str_value,
                   node.str_key, res.error());
      continue;
    }
    reg.vec_keys[*idx] = *res;
  }
  hotkey_registry_recompute_duplicates(reg);
}

std::optional<std::size_t> hotkey_registry_find(const HotkeyRegistryModel& reg,
                                                std::string_view str_name) {
  for (std::size_t i{0}; i < reg.vec_definitions.size(); ++i)
    if (reg.vec_definitions[i].str_name == str_name) return i;
  return std::nullopt;
}

HotkeyModel hotkey_resolve(const HotkeyRegistryModel& reg, std::string_view str_name) {
  if (const std::optional<std::size_t> idx{hotkey_registry_find(reg, str_name)})
    return reg.vec_keys[*idx];
  auto res{hotkey_try_parse(str_name)};
  if (!res.has_value()) return kHotkeyInvalid;
  return *res;
}

std::optional<std::size_t> hotkey_get_first_duplicate(const HotkeyRegistryModel& reg,
                                                      std::size_t idx_definition) {
  if (idx_definition >= reg.vec_definitions.size()) return std::nullopt;
  const HotkeyDefinitionModel& def{reg.vec_definitions[idx_definition]};
  const HotkeyModel& hk_value{reg.vec_keys[idx_definition]};

  for (std::size_t i{0}; i < reg.vec_keys.size(); ++i) {
    if (i == idx_definition) continue;
    if (!hotkey_equal(reg.vec_keys[i], hk_value)) continue;
    if (contexts_overlap(reg.vec_definitions[i].vec_contexts, def.vec_contexts)) return i;
  }
  return std::nullopt;
}

void hotkey_registry_recompute_duplicates(HotkeyRegistryModel& reg) {
  for (std::size_t i{0}; i < reg.vec_definitions.size(); ++i)
    reg.vec_definitions[i].b_has_duplicates =
        hotkey_get_first_duplicate(reg, i).has_value();
}

bool hotkey_set(HotkeyRegistryModel& reg, std::string_view str_name, const HotkeyModel& hk) {
  const std::optional<std::size_t> idx{hotkey_registry_find(reg, str_name)};
  if (!idx.has_value()) return false;
  if (reg.vec_definitions[*idx].b_readonly) return false;

  reg.vec_keys[*idx] = hk;

  std::size_t sz_write{0};
  for (std::size_t sz_read{0}; sz_read < reg.vec_settings.size(); ++sz_read) {
    if (reg.vec_settings[sz_read].first == str_name) continue;
    if (sz_write != sz_read) reg.vec_settings[sz_write] = std::move(reg.vec_settings[sz_read]);
    ++sz_write;
  }
  reg.vec_settings.resize(sz_write);
  // value != definition.Default —— hotkey_equal 的 UNKNOWN 永不等语义参与
  // （Set(name, Invalid) 对 Default=Invalid 的定义恒"不等" → 写 UNKNOWN None 节点，
  // 上游 quirk 照抄，登记 upstream.md）。
  if (!hotkey_equal(hk, reg.vec_definitions[*idx].hk_default))
    reg.vec_settings.emplace_back(std::string(str_name), hotkey_to_string(hk));

  // 全表重算。上游是条件重算（hadDuplicates||HasDuplicates 才扫其他定义），存在
  // 他人 flag 滞后 quirk；全表重算语义更强，偏离登记 COVERAGE.md。
  hotkey_registry_recompute_duplicates(reg);
  return true;
}

}
