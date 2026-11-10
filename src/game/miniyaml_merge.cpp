module ora.game.miniyaml;

import std;

namespace ora::game {

namespace {

std::size_t index_of_key(const std::vector<MiniYamlNode>& vec_nodes, const std::string& str_key) {
  for (std::size_t i{0}; i < vec_nodes.size(); ++i)
    if (vec_nodes[i].str_key == str_key) return i;
  return vec_nodes.size();
}

std::size_t last_index_of_key(const std::vector<MiniYamlNode>& vec_nodes,
                              const std::string& str_key) {
  for (std::size_t i{vec_nodes.size()}; i-- > 0;)
    if (vec_nodes[i].str_key == str_key) return i;
  return vec_nodes.size();
}

std::size_t remove_all_with_key(std::vector<MiniYamlNode>& vec_nodes, const std::string& str_key) {
  std::size_t sz_removed{0};
  std::size_t sz_write{0};
  for (std::size_t sz_read{0}; sz_read < vec_nodes.size(); ++sz_read) {
    if (vec_nodes[sz_read].str_key == str_key) {
      ++sz_removed;
      continue;
    }
    if (sz_write != sz_read) vec_nodes[sz_write] = std::move(vec_nodes[sz_read]);
    ++sz_write;
  }
  vec_nodes.resize(sz_write);
  return sz_removed;
}

std::string location_to_string(const SourceLocation& loc_location) {
  return std::format("{}:{}", loc_location.str_name, loc_location.int4_line);
}

struct TreeEntry {
  std::string str_key;
  const MiniYamlNode* ptr_node;
};

const MiniYamlNode* tree_find(const std::vector<TreeEntry>& vec_tree, const std::string& str_key) {
  for (const TreeEntry& entry : vec_tree)
    if (entry.str_key == str_key) return entry.ptr_node;
  return nullptr;
}

struct InheritedEntry {
  std::string str_parent;
  SourceLocation loc_location;
};

std::optional<std::vector<MiniYamlNode>> weak_resolve_removals(
    const std::vector<MiniYamlNode>& vec_nodes) {
  std::optional<std::vector<MiniYamlNode>> opt_ret;
  for (std::size_t i{0}; i < vec_nodes.size(); ++i) {
    const MiniYamlNode& node{vec_nodes[i]};
    if (node.str_key.empty()) continue;
    if (node.str_key.starts_with('-')) {
      if (!opt_ret.has_value()) {
        opt_ret.emplace();
        for (std::size_t j{0}; j < i; ++j) opt_ret->push_back(vec_nodes[j]);
      }

      remove_all_with_key(*opt_ret, node.str_key.substr(1));
    } else if (opt_ret.has_value()) {
      opt_ret->push_back(node);
    }
  }
  return opt_ret;
}

std::optional<std::string> duplicate_view_keys_message(const std::vector<MiniYamlNode>& vec_view) {
  struct SeenKey {
    std::string str_key;
    std::string str_first_log;
  };
  struct DupKey {
    std::string str_key;
    std::string str_value_logs;
  };
  std::vector<SeenKey> vec_seen;
  std::vector<DupKey> vec_dups;

  for (const MiniYamlNode& node : vec_view) {
    if (node.str_key.empty()) continue;
    const std::string str_log{node.str_key + " (at " +
                              location_to_string(node.loc_location) + ")"};
    const SeenKey* ptr_seen{nullptr};
    for (const SeenKey& seen : vec_seen)
      if (seen.str_key == node.str_key) {
        ptr_seen = &seen;
        break;
      }
    if (ptr_seen == nullptr) {
      vec_seen.push_back({node.str_key, str_log});
      continue;
    }
    DupKey* ptr_dup{nullptr};
    for (DupKey& dup : vec_dups)
      if (dup.str_key == node.str_key) {
        ptr_dup = &dup;
        break;
      }
    if (ptr_dup == nullptr)
      vec_dups.push_back({node.str_key, ptr_seen->str_first_log + "," + str_log});
    else
      ptr_dup->str_value_logs += "," + str_log;
  }

  if (vec_dups.empty()) return std::nullopt;
  std::string str_msg{"MiniYaml.Merge, duplicate values found for the following keys: "};
  for (const DupKey& dup : vec_dups) str_msg += dup.str_key + ": [" + dup.str_value_logs + "]";
  return str_msg;
}

struct ValueNodes {
  std::string str_value;
  std::vector<MiniYamlNode> vec_children;
};

std::expected<std::vector<MiniYamlNode>, std::string> merge_partial_nodes(
    const std::vector<MiniYamlNode>& vec_existing, const std::vector<MiniYamlNode>& vec_override);

std::expected<ValueNodes, std::string> merge_partial_value(const MiniYamlNode* ptr_existing,
                                                           const MiniYamlNode* ptr_override) {
  if (ptr_existing != nullptr) {
    const auto opt_view_e{weak_resolve_removals(ptr_existing->vec_children)};
    if (opt_view_e.has_value())
      if (const auto opt_msg{duplicate_view_keys_message(*opt_view_e)})
        return std::unexpected(*opt_msg);
  }
  if (ptr_override != nullptr) {
    const auto opt_view_o{weak_resolve_removals(ptr_override->vec_children)};
    if (opt_view_o.has_value())
      if (const auto opt_msg{duplicate_view_keys_message(*opt_view_o)})
        return std::unexpected(*opt_msg);
  }

  if (ptr_existing == nullptr)
    return ValueNodes{ptr_override->str_value, ptr_override->vec_children};
  if (ptr_override == nullptr)
    return ValueNodes{ptr_existing->str_value, ptr_existing->vec_children};

  auto res_children{merge_partial_nodes(ptr_existing->vec_children, ptr_override->vec_children)};
  if (!res_children.has_value()) return std::unexpected(res_children.error());

  return ValueNodes{ptr_override->str_value.empty() ? ptr_existing->str_value
                                                    : ptr_override->str_value,
                    std::move(*res_children)};
}

std::expected<std::vector<MiniYamlNode>, std::string> merge_partial_nodes(
    const std::vector<MiniYamlNode>& vec_existing, const std::vector<MiniYamlNode>& vec_override) {
  if (vec_existing.empty()) return vec_override;
  if (vec_override.empty()) return vec_existing;

  std::vector<MiniYamlNode> vec_ret;
  vec_ret.reserve(vec_existing.size() + vec_override.size());
  std::set<std::string> set_plain_keys;

  const auto merge_node{[&](const MiniYamlNode& node) -> std::expected<void, std::string> {
    if (node.str_key.empty()) return {};
    if (node.str_key.starts_with('-')) {
      vec_ret.push_back(node);
      return {};
    }
    if (set_plain_keys.insert(node.str_key).second) {
      vec_ret.push_back(node);
      return {};
    }

    const std::size_t sz_prev{last_index_of_key(vec_ret, node.str_key)};
    const std::size_t sz_removal{last_index_of_key(vec_ret, "-" + node.str_key)};
    if (sz_removal < vec_ret.size() && sz_removal > sz_prev) {

      vec_ret.push_back(node);
      return {};
    }

    auto res_value{merge_partial_value(&vec_ret[sz_prev], &node)};
    if (!res_value.has_value()) return std::unexpected(res_value.error());
    MiniYamlNode node_merged{node};
    node_merged.str_value = std::move(res_value->str_value);
    node_merged.vec_children = std::move(res_value->vec_children);
    vec_ret[sz_prev] = std::move(node_merged);
    return {};
  }};

  for (const MiniYamlNode& node : vec_existing)
    if (const auto res{merge_node(node)}; !res.has_value()) return std::unexpected(res.error());
  for (const MiniYamlNode& node : vec_override)
    if (const auto res{merge_node(node)}; !res.has_value()) return std::unexpected(res.error());
  return vec_ret;
}

std::expected<std::vector<MiniYamlNode>, std::string> merge_self_partial(
    const std::vector<MiniYamlNode>& vec_nodes) {
  if (vec_nodes.empty()) return vec_nodes;

  std::set<std::string> set_keys;
  std::vector<MiniYamlNode> vec_ret;
  vec_ret.reserve(vec_nodes.size());
  for (const MiniYamlNode& node : vec_nodes) {
    if (node.str_key.empty()) continue;
    if (set_keys.insert(node.str_key).second) {
      vec_ret.push_back(node);
      continue;
    }
    const std::size_t sz_idx{index_of_key(vec_ret, node.str_key)};
    auto res_value{merge_partial_value(&vec_ret[sz_idx], &node)};
    if (!res_value.has_value()) return std::unexpected(res_value.error());
    MiniYamlNode node_merged{vec_ret[sz_idx]};
    node_merged.str_value = std::move(res_value->str_value);
    node_merged.vec_children = std::move(res_value->vec_children);
    vec_ret[sz_idx] = std::move(node_merged);
  }
  return vec_ret;
}

std::expected<std::vector<MiniYamlNode>, std::string> resolve_inherits(
    const std::vector<MiniYamlNode>& vec_nodes, const std::vector<TreeEntry>& vec_tree,
    std::vector<InheritedEntry> vec_inherited);

std::expected<void, std::string> merge_into_resolved(const MiniYamlNode& node_override,
                                                     std::vector<MiniYamlNode>& vec_existing,
                                                     std::set<std::string>& set_keys,
                                                     const std::vector<TreeEntry>& vec_tree,
                                                     const std::vector<InheritedEntry>& vec_inherited) {
  const MiniYamlNode* ptr_existing{nullptr};
  std::size_t sz_idx{0};
  if (!set_keys.insert(node_override.str_key).second) {
    sz_idx = index_of_key(vec_existing, node_override.str_key);
    ptr_existing = &vec_existing[sz_idx];
  }

  auto res_value{merge_partial_value(ptr_existing, &node_override)};
  if (!res_value.has_value()) return std::unexpected(res_value.error());

  auto res_nodes{resolve_inherits(res_value->vec_children, vec_tree, vec_inherited)};
  if (!res_nodes.has_value()) return std::unexpected(res_nodes.error());

  if (ptr_existing != nullptr) {
    MiniYamlNode node_merged{*ptr_existing};
    node_merged.str_value = std::move(res_value->str_value);
    node_merged.vec_children = std::move(*res_nodes);
    vec_existing[sz_idx] = std::move(node_merged);
  } else {
    MiniYamlNode node_merged{node_override};
    node_merged.str_value = std::move(res_value->str_value);
    node_merged.vec_children = std::move(*res_nodes);
    vec_existing.push_back(std::move(node_merged));
  }
  return {};
}

std::expected<std::vector<MiniYamlNode>, std::string> resolve_inherits(
    const std::vector<MiniYamlNode>& vec_nodes, const std::vector<TreeEntry>& vec_tree,
    std::vector<InheritedEntry> vec_inherited) {
  if (vec_nodes.empty()) return vec_nodes;

  std::vector<MiniYamlNode> vec_resolved;
  vec_resolved.reserve(vec_nodes.size());
  std::set<std::string> set_resolved_keys;

  for (const MiniYamlNode& node : vec_nodes) {
    if (node.str_key == "Inherits" || node.str_key.starts_with("Inherits@")) {
      const std::string& str_parent{node.str_value};
      const MiniYamlNode* ptr_parent{tree_find(vec_tree, str_parent)};
      if (ptr_parent == nullptr)
        return std::unexpected(std::format("{}: Parent type `{}` not found",
                                           location_to_string(node.loc_location), str_parent));

      for (const InheritedEntry& entry : vec_inherited)
        if (entry.str_parent == str_parent)
          return std::unexpected(std::format(
              "{}: Parent type `{}` was already inherited by this yaml tree at {} "
              "(note: may be from a derived tree)",
              location_to_string(node.loc_location), str_parent,
              location_to_string(entry.loc_location)));
      vec_inherited.push_back({str_parent, node.loc_location});

      auto res_parent{resolve_inherits(ptr_parent->vec_children, vec_tree, vec_inherited)};
      if (!res_parent.has_value()) return std::unexpected(res_parent.error());
      for (const MiniYamlNode& node_parent : *res_parent)
        if (const auto res{merge_into_resolved(node_parent, vec_resolved, set_resolved_keys,
                                               vec_tree, vec_inherited)};
            !res.has_value())
          return std::unexpected(res.error());
    } else if (node.str_key.starts_with('-')) {
      const std::string str_removed{node.str_key.substr(1)};
      if (remove_all_with_key(vec_resolved, str_removed) == 0)
        return std::unexpected(std::format("{}: There are no elements with key `{}` to remove",
                                           location_to_string(node.loc_location), str_removed));
      set_resolved_keys.erase(str_removed);
    } else {
      if (const auto res{merge_into_resolved(node, vec_resolved, set_resolved_keys, vec_tree,
                                             vec_inherited)};
          !res.has_value())
        return std::unexpected(res.error());
    }
  }
  return vec_resolved;
}

}

std::expected<std::vector<MiniYamlNode>, std::string> miniyaml_merge(
    const std::vector<std::vector<MiniYamlNode>>& vec_sources) {

  if (vec_sources.empty()) return std::vector<MiniYamlNode>{};

  std::vector<MiniYamlNode> vec_aggregated;
  for (std::size_t i{0}; i < vec_sources.size(); ++i) {
    auto res_self{merge_self_partial(vec_sources[i])};
    if (!res_self.has_value()) return std::unexpected(res_self.error());
    if (i == 0) {
      vec_aggregated = std::move(*res_self);
      continue;
    }
    auto res_merged{merge_partial_nodes(vec_aggregated, *res_self)};
    if (!res_merged.has_value()) return std::unexpected(res_merged.error());
    vec_aggregated = std::move(*res_merged);
  }

  std::vector<TreeEntry> vec_tree;
  vec_tree.reserve(vec_aggregated.size());
  for (const MiniYamlNode& node : vec_aggregated) {
    if (node.str_key.empty()) continue;
    if (tree_find(vec_tree, node.str_key) != nullptr)
      return std::unexpected("MiniYaml.Merge: duplicate top-level key '" + node.str_key + "'");
    vec_tree.push_back({node.str_key, &node});
  }

  struct ResolvedTop {
    std::string str_key;
    std::string str_value;
    std::vector<MiniYamlNode> vec_children;
  };
  std::vector<ResolvedTop> vec_resolved;
  vec_resolved.reserve(vec_tree.size());
  for (const TreeEntry& entry : vec_tree) {
    std::vector<InheritedEntry> vec_inherited{{entry.str_key, SourceLocation{"", 0}}};
    auto res_children{
        resolve_inherits(entry.ptr_node->vec_children, vec_tree, std::move(vec_inherited))};
    if (!res_children.has_value()) return std::unexpected(res_children.error());
    vec_resolved.push_back({entry.str_key, entry.ptr_node->str_value, std::move(*res_children)});
  }

  std::vector<MiniYamlNode> vec_top;
  vec_top.reserve(vec_resolved.size());
  for (ResolvedTop& top : vec_resolved)
    vec_top.push_back(MiniYamlNode{top.str_key, top.str_value, std::nullopt,
                                   SourceLocation{"", 0}, std::move(top.vec_children)});

  return resolve_inherits(vec_top, vec_tree, std::vector<InheritedEntry>{});
}

}
