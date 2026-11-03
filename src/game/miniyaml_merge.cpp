#include "game/miniyaml_merge.hpp"

// 上游 OpenRA.Game/MiniYaml.cs Merge 管线（L408-698）语义等价移植。
// 行号注释均指上游文件。C++ 单层节点模型下，上游 MiniYaml(Value, Nodes)
// 二层结构对应节点的 (str_value, vec_children)；上游 null MiniYaml 对应
// "节点不存在"（空指针），其 Value 的 null 与空串等价（T04 惯例）。
//
// 保序生命线：tree/resolved/结果列表一律 vector 首现序（上游 .NET Dictionary
// 插入序的事实等价），线性查找；绝不用 map 遍历序决定输出。

namespace ora::game {

namespace {

// —— IndexOfKey / LastIndexOfKey（L652-668）——
// 返回 not-found 哨兵 = vec_nodes.size()（调用处以 < size() 判断命中）。
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

// List.RemoveAll(r => r.Key == key) 等价（返回删除数）
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

// 上游 SourceLocation.ToString（MiniYaml.cs:68）= $"{Name}:{Line}"；
// default（Name=null）插值为空串
std::string location_to_string(const SourceLocation& loc_location) {
  return std::format("{}:{}", loc_location.str_name, loc_location.int4_line);
}

// 上游 tree：Dictionary<string, MiniYaml>（键 → 顶层节点的 value/children 载体）。
// 首现序 vector + 指针（聚合输出在 tree 生命周期内不再变动）。
struct TreeEntry {
  std::string str_key;
  const MiniYamlNode* ptr_node;
};

const MiniYamlNode* tree_find(const std::vector<TreeEntry>& vec_tree, const std::string& str_key) {
  for (const TreeEntry& entry : vec_tree)
    if (entry.str_key == str_key) return entry.ptr_node;
  return nullptr;
}

// 循环检测沿递归传递的集合（上游 ImmutableDictionary，值语义：兄弟分支不共享）
struct InheritedEntry {
  std::string str_parent;
  SourceLocation loc_location;
};

// —— WeakResolveRemovals（L533-564）——
// 只用于同侧重复键检查的容错视图："-K 在 K 之后"的 K 从视图中消失、标记本身
// 也不进视图（"K, -K, K" 形态因此不算冲突）。实际合并在原始列表上进行：
// -K 标记经集合级 MergePartial 透传、由 ResolveInherits 强删除消费。
// 返回 nullopt = 无删除标记（视图与原列表相同，上游 ret ?? nodes 短路）。
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
      // 弱应用删除：移除视图中之前的同名；无命中不抛；标记本身不进视图
      remove_all_with_key(*opt_ret, node.str_key.substr(1));
    } else if (opt_ret.has_value()) {
      opt_ret->push_back(node);
    }
  }
  return opt_ret;
}

// —— IntoDictionaryWithConflictLog（Exts.cs:414-465）消息复刻 ——
// 对弱删除视图做同键检查，收集全部重复后一次报错（键段 = 首次冲突出现序，
// 值列表含首现值）。logValue = "{Key} (at {Location})"。
std::optional<std::string> duplicate_view_keys_message(const std::vector<MiniYamlNode>& vec_view) {
  struct SeenKey {
    std::string str_key;
    std::string str_first_log;
  };
  struct DupKey {
    std::string str_key;
    std::string str_value_logs;  // "v1,v2,..."，v1 为首现值
  };
  std::vector<SeenKey> vec_seen;
  std::vector<DupKey> vec_dups;

  for (const MiniYamlNode& node : vec_view) {
    if (node.str_key.empty()) continue;  // 上游丢弃 null key
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

// 上游 MiniYaml（Value + Nodes）的值语义载体
struct ValueNodes {
  std::string str_value;
  std::vector<MiniYamlNode> vec_children;
};

std::expected<std::vector<MiniYamlNode>, std::string> merge_partial_nodes(
    const std::vector<MiniYamlNode>& vec_existing, const std::vector<MiniYamlNode>& vec_override);

// —— MergePartial（MiniYaml 级，L566-595）——
// existing/override 为 null（节点不存在）时空指针。null 侧跳过视图检查；
// existing == null 返回 override 原样（其子层未做弱删除——上游返回原始对象）。
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

  // override 非空值胜出（上游 overrideNodes.Value ?? existingNodes.Value；
  // C++ 空串 = null）
  return ValueNodes{ptr_override->str_value.empty() ? ptr_existing->str_value
                                                    : ptr_override->str_value,
                    std::move(*res_children)};
}

// —— MergePartial（集合级，L597-650）——
// 任一侧空 → 另一侧原样。遍历 existing 全部再 override 全部，逐节点 MergeNode：
//   `-K` 删除标记透传（不在此消费）；首现 append；removal 更近时 append 不合并
//   （保"先应用 → 删除 → 再应用"次序）；否则与最后同名节点原位深层合并
//   （壳用新节点、位置用旧下标）。
std::expected<std::vector<MiniYamlNode>, std::string> merge_partial_nodes(
    const std::vector<MiniYamlNode>& vec_existing, const std::vector<MiniYamlNode>& vec_override) {
  if (vec_existing.empty()) return vec_override;
  if (vec_override.empty()) return vec_existing;

  std::vector<MiniYamlNode> vec_ret;
  vec_ret.reserve(vec_existing.size() + vec_override.size());
  std::set<std::string> set_plain_keys;

  const auto merge_node{[&](const MiniYamlNode& node) -> std::expected<void, std::string> {
    if (node.str_key.empty()) return {};  // 上游 null key 跳过
    if (node.str_key.starts_with('-')) {
      vec_ret.push_back(node);  // 删除节点原样透传
      return {};
    }
    if (set_plain_keys.insert(node.str_key).second) {
      vec_ret.push_back(node);  // 首现
      return {};
    }

    const std::size_t sz_prev{last_index_of_key(vec_ret, node.str_key)};
    const std::size_t sz_removal{last_index_of_key(vec_ret, "-" + node.str_key)};
    if (sz_removal < vec_ret.size() && sz_removal > sz_prev) {
      // 删除标记离此键更近：不合并（合并会让数据跳过删除），直接 append
      vec_ret.push_back(node);
      return {};
    }

    // 原位深层合并：node.WithValue(MergePartial(ret[prev].Value, node.Value))
    // （保留新节点的 key/comment/location，位置保留旧下标）
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

// —— MergeSelfPartial（L503-531）：文件内同层重复键合并 ——
// 首现保留位置；后现节点经 MiniYaml 级 MergePartial 合并进首现（壳用首现）。
// 不解析继承、不处理 '-'（上游 L505 注释明言）。
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
    const std::size_t sz_idx{index_of_key(vec_ret, node.str_key)};  // 正序首个
    auto res_value{merge_partial_value(&vec_ret[sz_idx], &node)};
    if (!res_value.has_value()) return std::unexpected(res_value.error());
    MiniYamlNode node_merged{vec_ret[sz_idx]};  // 壳用首现（original.WithValue）
    node_merged.str_value = std::move(res_value->str_value);
    node_merged.vec_children = std::move(res_value->vec_children);
    vec_ret[sz_idx] = std::move(node_merged);
  }
  return vec_ret;
}

std::expected<std::vector<MiniYamlNode>, std::string> resolve_inherits(
    const std::vector<MiniYamlNode>& vec_nodes, const std::vector<TreeEntry>& vec_tree,
    std::vector<InheritedEntry> vec_inherited);

// —— MergeIntoResolved（L437-457）：解析列表内同键合并 + 合并后立即递归解析继承 ——
// 上游 existingNodeKeys 为 HashSet（纯存在性）；existingNodes 索引取正序首个。
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

  // ★ 合并完成后立刻对该层做继承解析（与合并非两阶段）
  auto res_nodes{resolve_inherits(res_value->vec_children, vec_tree, vec_inherited)};
  if (!res_nodes.has_value()) return std::unexpected(res_nodes.error());

  if (ptr_existing != nullptr) {
    MiniYamlNode node_merged{*ptr_existing};  // 原位替换，壳保留 existing 的
    node_merged.str_value = std::move(res_value->str_value);
    node_merged.vec_children = std::move(*res_nodes);
    vec_existing[sz_idx] = std::move(node_merged);
  } else {
    MiniYamlNode node_merged{node_override};  // append，壳用 override 的
    node_merged.str_value = std::move(res_value->str_value);
    node_merged.vec_children = std::move(*res_nodes);
    vec_existing.push_back(std::move(node_merged));
  }
  return {};
}

// —— ResolveInherits（L459-501）：继承展开 + 强删除 + 循环检测 ——
// Inherits / Inherits@x：父名取节点【值】（@suffix 不参与查找）；父的每个已解析
// trait 依次合入当前列表（继承内容出现在 Inherits 语句处，Inherits 节点本身
// 不出现在结果里）。`-Key` 强删除按完整键（含 @ 后缀）精确匹配，无命中即报错。
std::expected<std::vector<MiniYamlNode>, std::string> resolve_inherits(
    const std::vector<MiniYamlNode>& vec_nodes, const std::vector<TreeEntry>& vec_tree,
    std::vector<InheritedEntry> vec_inherited) {
  if (vec_nodes.empty()) return vec_nodes;  // 上游空数组短路

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
      const std::string str_removed{node.str_key.substr(1)};  // 剥一个 '-'，完整键匹配
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

}  // namespace

std::expected<std::vector<MiniYamlNode>, std::string> miniyaml_merge(
    const std::vector<std::vector<MiniYamlNode>>& vec_sources) {
  // 上游 Merge（L408-435）总控
  if (vec_sources.empty()) return std::vector<MiniYamlNode>{};

  // 每源 MergeSelfPartial → 按清单序 Aggregate(集合级 MergePartial)
  //（单元素序列不调 func，直接作种子——LINQ Aggregate 语义）
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

  // null key 过滤 → tree（上游 ToDictionary：重复键报错；K,-K,K 顶层形态会走到
  // 这里——.NET 内部消息不可移植，用含键名的近似消息，ra 语料 0 触发，见 COVERAGE）
  std::vector<TreeEntry> vec_tree;
  vec_tree.reserve(vec_aggregated.size());
  for (const MiniYamlNode& node : vec_aggregated) {
    if (node.str_key.empty()) continue;
    if (tree_find(vec_tree, node.str_key) != nullptr)
      return std::unexpected("MiniYaml.Merge: duplicate top-level key '" + node.str_key + "'");
    vec_tree.push_back({node.str_key, &node});
  }

  // 逐键继承解析（种子含自身，防自继承；兄弟不共享）
  struct ResolvedTop {
    std::string str_key;
    std::string str_value;               // 顶层节点自身 Value 保留（上游 kv.Value.Value）
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

  // 顶层删除解析：节点键/值来自 resolved，Location/Comment 为 default
  //（上游 new MiniYamlNode(kv.Key, kv.Value)）；inherited 为空（无种子）
  std::vector<MiniYamlNode> vec_top;
  vec_top.reserve(vec_resolved.size());
  for (ResolvedTop& top : vec_resolved)
    vec_top.push_back(MiniYamlNode{top.str_key, top.str_value, std::nullopt,
                                   SourceLocation{"", 0}, std::move(top.vec_children)});

  return resolve_inherits(vec_top, vec_tree, std::vector<InheritedEntry>{});
}

}  // namespace ora::game
