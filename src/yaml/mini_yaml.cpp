// UPSTREAM: OpenRA.Game/MiniYaml.cs @7d57605 L22-791(全文件逐语义重写)
// 依赖的 BCL 语义在对应位置标注:
//  - StreamExts.cs L200-248 ReadAllLinesAsMemory(流式行切分,含 BOM/行尾 \r)
//  - MiniYaml.cs L405 String.Split(["\r\n","\n"])(字符串行切分,保留末尾空行)
//  - Exts.cs L414-467 IntoDictionaryWithConflictLog(合并冲突消息格式)
// C# IEnumerable 惰性产出在此物化为 vector(FromStreamAsEnumerable 并发流测试
// 不移植,属实现细节;docs/COVERAGE.md 已登记)。
// BCL semantics relied upon are annotated at the corresponding sites:
//  - StreamExts.cs L200-248 ReadAllLinesAsMemory (streaming line splitting, incl. BOM/trailing \r)
//  - MiniYaml.cs L405 String.Split(["\r\n","\n"]) (string line splitting, keeps the trailing empty line)
//  - Exts.cs L414-467 IntoDictionaryWithConflictLog (merge conflict log message format)
// C# IEnumerable lazy production is materialized into a vector here (the
// FromStreamAsEnumerable concurrent-streaming test is not ported, being an
// implementation detail; recorded in docs/COVERAGE.md).
#include "yaml/mini_yaml.hpp"

#include "core/text.hpp"

namespace ora::yaml {

namespace {

constexpr int kSpacesPerLevel = 4;  // MiniYaml.cs L110

// ———— 行切分 ————
// ———— Line splitting ————

// ReadAllLinesAsMemory 语义:按 '\n' 切行,行尾紧邻的单个 '\r' 剥离;
// 末段无 '\n' 时仅当非空才作为末行(流以 '\n' 结尾不产生空行)。
// ReadAllLinesAsMemory semantics: split on '\n' and strip a single '\r' that
// immediately precedes the line break; a trailing segment without '\n' becomes
// the last line only if non-empty (a stream ending in '\n' yields no empty line).
std::vector<std::string_view> SplitLinesStream(std::string_view bytes) {
  std::vector<std::string_view> lines;
  std::size_t start = 0;
  while (true) {
    const std::size_t nl = bytes.find('\n', start);
    if (nl == std::string_view::npos)
      break;
    std::size_t end = nl;
    if (end > start && bytes[end - 1] == '\r')
      end--;
    lines.push_back(bytes.substr(start, end - start));
    start = nl + 1;
  }
  if (start < bytes.size())
    lines.push_back(bytes.substr(start));
  return lines;
}

// String.Split(["\r\n", "\n"], None) 语义:切法相同,末段(含空串)总是保留。
// String.Split(["\r\n", "\n"], None) semantics: identical splitting, but the
// final segment (empty string included) is always kept.
std::vector<std::string_view> SplitLinesString(std::string_view text) {
  std::vector<std::string_view> lines;
  std::size_t start = 0;
  while (true) {
    const std::size_t nl = text.find('\n', start);
    if (nl == std::string_view::npos)
      break;
    std::size_t end = nl;
    if (end > start && text[end - 1] == '\r')
      end--;
    lines.push_back(text.substr(start, end - start));
    start = nl + 1;
  }
  lines.push_back(text.substr(start));
  return lines;
}

// StreamReader 默认构造:识别并剥离 UTF-8 BOM
// StreamReader default construction: detect and strip the UTF-8 BOM
std::string_view StripUtf8Bom(std::string_view bytes) {
  if (bytes.size() >= 3 && static_cast<unsigned char>(bytes[0]) == 0xEF &&
      static_cast<unsigned char>(bytes[1]) == 0xBB && static_cast<unsigned char>(bytes[2]) == 0xBF)
    return bytes.substr(3);
  return bytes;
}

std::string ReplaceAll(std::string_view text, std::string_view from, std::string_view to) {
  std::string ret;
  ret.reserve(text.size());
  std::size_t start = 0;
  while (true) {
    const std::size_t pos = text.find(from, start);
    if (pos == std::string_view::npos)
      break;
    ret.append(text.substr(start, pos - start));
    ret.append(to);
    start = pos + from.size();
  }
  ret.append(text.substr(start));
  return ret;
}

std::string EscapeHash(std::string_view text) {  // ToLines 的 Value.Replace("#", "\\#") | Value.Replace("#", "\\#") in ToLines
  return ReplaceAll(text, "#", "\\#");
}

// ———— 键比较(null 语义 = C# string ==:null 仅等于 null)————
// ———— Key comparison (null semantics = C# string ==: null only equals null) ————

bool KeyEquals(const std::string* a, const std::string* b) {
  if (a == nullptr || b == nullptr)
    return a == b;
  return *a == *b;
}

// 已登记偏离:合并键集合里 null 键以 "" 表示(上游解析器不会产生 "" 键)
// Registered deviation: null keys in the merged key set are represented as ""
// (the upstream parser never produces "" keys)
std::string_view KeyViewOrNull(const std::string* key) {
  return key != nullptr ? std::string_view{*key} : std::string_view{};
}

std::size_t IndexOfKey(const std::vector<MiniYamlNode>& nodes, const std::string* key) {
  for (std::size_t i = 0; i < nodes.size(); i++)
    if (KeyEquals(nodes[i].Key, key))
      return i;
  return static_cast<std::size_t>(-1);
}

std::size_t LastIndexOfKey(const std::vector<MiniYamlNode>& nodes, const std::string* key) {
  for (std::size_t i = nodes.size(); i-- > 0;)
    if (KeyEquals(nodes[i].Key, key))
      return i;
  return static_cast<std::size_t>(-1);
}

// List.RemoveAll(r => r.Key == removed):返回移除数
// List.RemoveAll(r => r.Key == removed): returns the removal count
std::size_t RemoveAllByKey(std::vector<MiniYamlNode>& nodes, std::string_view removed) {
  std::size_t removedCount = 0;
  for (std::size_t i = 0; i < nodes.size();) {
    if (nodes[i].Key != nullptr && std::string_view{*nodes[i].Key} == removed) {
      nodes.erase(nodes.begin() + static_cast<std::ptrdiff_t>(i));
      removedCount++;
    } else {
      i++;
    }
  }
  return removedCount;
}

// ———— Merge 家族(MiniYaml.cs L437-650)————
// ———— Merge family (MiniYaml.cs L437-650) ————

using Tree = std::vector<std::pair<const std::string*, MiniYaml>>;  // C# Dictionary(插入序) | C# Dictionary (insertion order)
using TreeIndex = std::unordered_map<std::string_view, std::size_t>;
using InheritedMap = std::unordered_map<std::string_view, SourceLocation>;

std::vector<MiniYamlNode> ResolveInherits(const MiniYaml& node, const Tree& tree,
                                          const TreeIndex& treeIndex, InheritedMap inherited);
MiniYaml MergePartialValue(const MiniYaml* existingNodes, const MiniYaml* overrideNodes);
std::vector<MiniYamlNode> MergePartialList(const std::vector<MiniYamlNode>& existingNodes,
                                           const std::vector<MiniYamlNode>& overrideNodes);

// IntoDictionaryWithConflictLog(Exts.cs L414-467,调用点 MiniYaml.cs L576-579):
// 同层重复键 → YamlException,消息逐字一致:
// "MiniYaml.Merge, duplicate values found for the following keys: K: [v1,v2]"
// (逐键组拼接,组间无分隔;null 键跳过;组序 = 首次冲突发现序)
// IntoDictionaryWithConflictLog (Exts.cs L414-467, call site MiniYaml.cs L576-579):
// duplicate keys at the same level → YamlException, message verbatim:
// "MiniYaml.Merge, duplicate values found for the following keys: K: [v1,v2]"
// (concatenated per key group with no separator between groups; null keys
// skipped; group order = order of first conflict discovery)
void ConflictLogDupKeys(const std::vector<MiniYamlNode>& nodes, std::string_view debugName) {
  std::unordered_map<std::string_view, std::string> firstLog;
  std::unordered_map<std::string_view, std::vector<std::string>> dups;
  std::vector<std::string_view> dupOrder;
  for (const MiniYamlNode& n : nodes) {
    if (n.Key == nullptr)
      continue;
    const std::string_view keyView{*n.Key};
    const std::string logValue = std::format("{} (at {})", *n.Key, n.Location.ToString());
    const auto [it, inserted] = firstLog.emplace(keyView, logValue);
    if (inserted)
      continue;  // 首次出现:不记 | first occurrence: not recorded
    auto& values = dups.try_emplace(keyView).first->second;
    if (values.empty()) {
      dupOrder.push_back(keyView);
      values.push_back(it->second);  // 先记录首个(已插入字典)的值 | record the first (already-inserted) value first
    }
    values.push_back(logValue);
  }
  if (dupOrder.empty())
    return;

  std::string msg{std::format("{}, duplicate values found for the following keys: ", debugName)};
  for (const std::string_view key : dupOrder) {
    msg += key;
    msg += ": [";
    const auto& values = dups.at(key);
    for (std::size_t i = 0; i < values.size(); i++) {
      if (i > 0)
        msg += ',';
      msg += values[i];
    }
    msg += ']';
  }
  throw YamlException{std::move(msg)};
}

// WeakResolveRemovals(MiniYaml.cs L533-564):弱删除——无可删键时不抛;
// null 键节点:首个移除节点之前的原样保留(前缀),之后被丢弃(照抄上游行为)
// WeakResolveRemovals (MiniYaml.cs L533-564): weak removal — no throw when
// there is nothing to remove; null-key nodes: kept verbatim before the first
// removal node (prefix), discarded afterwards (upstream behavior copied verbatim)
std::vector<MiniYamlNode> WeakResolveRemovals(const std::vector<MiniYamlNode>& nodes) {
  if (nodes.empty())
    return {};

  std::optional<std::vector<MiniYamlNode>> ret;
  for (std::size_t i = 0; i < nodes.size(); i++) {
    const MiniYamlNode& node = nodes[i];
    if (node.Key == nullptr)
      continue;

    if (node.Key->starts_with('-')) {
      if (!ret.has_value()) {
        ret.emplace();
        ret->insert(ret->end(), nodes.begin(),
                    nodes.begin() + static_cast<std::ptrdiff_t>(i));
      }
      RemoveAllByKey(*ret, node.Key->substr(1));
    } else if (ret.has_value()) {
      ret->push_back(node);
    }
  }

  return ret.has_value() ? std::move(*ret) : nodes;
}

// MergePartial(IReadOnlyCollection, IReadOnlyCollection)(MiniYaml.cs L597-650)
std::vector<MiniYamlNode> MergePartialList(const std::vector<MiniYamlNode>& existingNodes,
                                           const std::vector<MiniYamlNode>& overrideNodes) {
  if (existingNodes.empty())
    return overrideNodes;
  if (overrideNodes.empty())
    return existingNodes;

  std::vector<MiniYamlNode> ret;
  ret.reserve(existingNodes.size() + overrideNodes.size());
  std::unordered_set<std::string_view> plainKeys;

  auto mergeNode = [&](const MiniYamlNode& node) {
    if (node.Key == nullptr)
      return;

    // 移除节点('-Key')无条件追加到结果
    // Removal nodes ('-Key') are appended to the result unconditionally
    if (node.Key->starts_with('-')) {
      ret.push_back(node);
      return;
    }

    // 无先前同键节点:新键,追加
    // No previous node with the same key: a new key, append
    if (plainKeys.insert(std::string_view{*node.Key}).second) {
      ret.push_back(node);
      return;
    }

    // 前面有比上次同键节点更近的移除节点:不合并,追加
    //(保证应用序:旧节点 → 移除 → 新节点)
    // A removal node nearer than the previous same-key node: do not merge, append
    // (preserves application order: old node → removal → new node)
    const std::size_t previousNodeIndex = LastIndexOfKey(ret, node.Key);
    const std::string removalKey = std::string{"-"} + *node.Key;
    const std::size_t previousRemovalNodeIndex =
        LastIndexOfKey(ret, &removalKey);  // 临时串仅作比较键(值相等) | temporary string used only as the comparison key (value equality)
    if (previousRemovalNodeIndex != static_cast<std::size_t>(-1) &&
        previousRemovalNodeIndex > previousNodeIndex) {
      ret.push_back(node);
      return;
    }

    // 前有同键节点且中间无移除:原地合并
    // A previous same-key node exists with no removal in between: merge in place
    MiniYaml merged = MergePartialValue(&ret[previousNodeIndex].Value, &node.Value);
    ret[previousNodeIndex] = node.WithValue(std::move(merged));
  };

  for (const MiniYamlNode& node : existingNodes)
    mergeNode(node);
  for (const MiniYamlNode& node : overrideNodes)
    mergeNode(node);

  return ret;
}

// MergePartial(MiniYaml, MiniYaml)(MiniYaml.cs L566-595)
MiniYaml MergePartialValue(const MiniYaml* existingNodes, const MiniYaml* overrideNodes) {
  // 先弱删除,再做同层重复键冲突检查(顺序照抄)
  // Weak removal first, then the same-level duplicate-key conflict check (order copied verbatim)
  if (existingNodes != nullptr)
    ConflictLogDupKeys(WeakResolveRemovals(existingNodes->Nodes), "MiniYaml.Merge");
  if (overrideNodes != nullptr)
    ConflictLogDupKeys(WeakResolveRemovals(overrideNodes->Nodes), "MiniYaml.Merge");

  if (existingNodes == nullptr)
    return *overrideNodes;
  if (overrideNodes == nullptr)
    return *existingNodes;

  return MiniYaml{overrideNodes->Value != nullptr ? overrideNodes->Value : existingNodes->Value,
                  MergePartialList(existingNodes->Nodes, overrideNodes->Nodes)};
}

// MergeSelfPartial(MiniYaml.cs L507-531):同源内重复顶层键合并
// MergeSelfPartial (MiniYaml.cs L507-531): merges duplicate top-level keys within a single source
std::vector<MiniYamlNode> MergeSelfPartial(const std::vector<MiniYamlNode>& existingNodes) {
  if (existingNodes.empty())
    return {};

  std::unordered_set<std::string_view> keys;
  std::vector<MiniYamlNode> ret;
  ret.reserve(existingNodes.size());
  for (const MiniYamlNode& n : existingNodes) {
    if (n.Key == nullptr)
      continue;

    if (keys.insert(std::string_view{*n.Key}).second) {
      ret.push_back(n);
    } else {
      // 同键节点已存在:新节点合并到旧节点上
      // A node with the same key already exists: the new node is merged into the old one
      const std::size_t originalIndex = IndexOfKey(ret, n.Key);
      MiniYaml merged = MergePartialValue(&ret[originalIndex].Value, &n.Value);
      ret[originalIndex] = ret[originalIndex].WithValue(std::move(merged));
    }
  }

  return ret;
}

// MergeIntoResolved(MiniYaml.cs L437-457)
void MergeIntoResolved(const MiniYamlNode& overrideNode, std::vector<MiniYamlNode>& existingNodes,
                       std::unordered_set<std::string_view>& existingNodeKeys, const Tree& tree,
                       const TreeIndex& treeIndex, InheritedMap& inherited) {
  const bool isNew = existingNodeKeys.insert(KeyViewOrNull(overrideNode.Key)).second;
  const std::size_t existingNodeIndex =
      isNew ? 0 : IndexOfKey(existingNodes, overrideNode.Key);
  const MiniYamlNode* existingNode = isNew ? nullptr : &existingNodes[existingNodeIndex];

  MiniYaml value = MergePartialValue(existingNode != nullptr ? &existingNode->Value : nullptr,
                                     &overrideNode.Value);
  std::vector<MiniYamlNode> nodes = ResolveInherits(value, tree, treeIndex, inherited);
  if (value.Nodes != nodes)
    value.Nodes = std::move(nodes);

  if (existingNode != nullptr)
    existingNodes[existingNodeIndex] = existingNode->WithValue(std::move(value));
  else
    existingNodes.push_back(overrideNode.WithValue(std::move(value)));
}

// ResolveInherits(MiniYaml.cs L459-501)。inherited 按值传入 =
// C# ImmutableDictionary 的"传引用 + 不可变重绑"语义(嵌套调用的增改不回漏)
// ResolveInherits (MiniYaml.cs L459-501). Passing inherited by value =
// the C# ImmutableDictionary "pass by reference + immutable rebinding" semantics
// (insertions/updates in nested calls do not leak back out)
std::vector<MiniYamlNode> ResolveInherits(const MiniYaml& node, const Tree& tree,
                                          const TreeIndex& treeIndex, InheritedMap inherited) {
  if (node.Nodes.empty())
    return node.Nodes;

  std::vector<MiniYamlNode> resolved;
  resolved.reserve(node.Nodes.size());
  std::unordered_set<std::string_view> resolvedKeys;

  for (const MiniYamlNode& n : node.Nodes) {
    if (n.Key != nullptr && (*n.Key == "Inherits" || n.Key->starts_with("Inherits@"))) {
      const std::string* parentName = n.Value.Value;
      if (parentName == nullptr)
        throw YamlException{std::format("{}: Parent type `{}` not found", n.Location.ToString(),
                                        std::string_view{})};
      const TreeIndex::const_iterator treeIt = treeIndex.find(std::string_view{*parentName});
      if (treeIt == treeIndex.end())
        throw YamlException{std::format("{}: Parent type `{}` not found", n.Location.ToString(),
                                        std::string_view{*parentName})};

      const MiniYaml& parent = tree[treeIt->second].second;
      const auto [insIt, inserted] =
          inherited.emplace(std::string_view{*parentName}, n.Location);
      if (!inserted)
        throw YamlException{std::format(
            "{}: Parent type `{}` was already inherited by this yaml tree at {} "
            "(note: may be from a derived tree)",
            n.Location.ToString(), *parentName, insIt->second.ToString())};

      for (const MiniYamlNode& r : ResolveInherits(parent, tree, treeIndex, inherited))
        MergeIntoResolved(r, resolved, resolvedKeys, tree, treeIndex, inherited);
    } else if (n.Key != nullptr && n.Key->starts_with('-')) {
      // 已登记偏离:C# 对 null 键在首个条件即 NullReferenceException(上游潜在
      // bug),此处 null 键落入 else 分支,合法输入下行为一致
      // Registered deviation: C# hits NullReferenceException on the first
      // condition for null keys (a latent upstream bug); here null keys fall
      // into the else branch — identical behavior for valid input
      const std::string_view removed = std::string_view{*n.Key}.substr(1);
      if (RemoveAllByKey(resolved, removed) == 0)
        throw YamlException{std::format("{}: There are no elements with key `{}` to remove",
                                        n.Location.ToString(), removed)};
      resolvedKeys.erase(removed);
    } else {
      MergeIntoResolved(n, resolved, resolvedKeys, tree, treeIndex, inherited);
    }
  }

  return resolved;
}

}  // namespace

// ———— StringPool / 查询 ————
// ———— StringPool / queries ————

// MiniYaml 构造与值操作(MiniYamlNode 已完整)/
// MiniYaml construction and value ops (MiniYamlNode is complete down here).
MiniYaml::MiniYaml() = default;
MiniYaml::MiniYaml(const std::string* str_v) : Value{str_v} {}
MiniYaml::MiniYaml(const std::string* str_v, std::vector<MiniYamlNode> nodes)
    : Value{str_v}, Nodes{std::move(nodes)} {}

bool MiniYaml::operator==(const MiniYaml& o) const { return Value == o.Value && Nodes == o.Nodes; }

MiniYaml MiniYaml::WithValue(const std::string* str_v) const {
  return MiniYaml{str_v, Nodes};
}
MiniYaml MiniYaml::WithNodes(std::vector<MiniYamlNode> nodes) const {
  return MiniYaml{Value, std::move(nodes)};
}
MiniYaml MiniYaml::WithNodesAppended(const std::vector<MiniYamlNode>& nodes) const {
  auto newNodes = Nodes;
  newNodes.insert(newNodes.end(), nodes.begin(), nodes.end());
  return MiniYaml{Value, std::move(newNodes)};
}

StringPool& MiniYaml::GlobalPool() {
  static StringPool pool;
  return pool;
}

const MiniYamlNode& MiniYaml::NodeWithKey(std::string_view key) const {
  const MiniYamlNode* result = NodeWithKeyOrDefault(key);
  if (result == nullptr)
    throw YamlException{std::format("No node with key '{}'", key)};
  return *result;
}

const MiniYamlNode* MiniYaml::NodeWithKeyOrDefault(std::string_view key) const {
  // PERF: 避免 LINQ(MiniYaml.cs L151)
  // PERF: avoid LINQ (MiniYaml.cs L151)
  bool first = true;
  const MiniYamlNode* result = nullptr;
  for (const MiniYamlNode& node : Nodes) {
    if (node.Key == nullptr || std::string_view{*node.Key} != key)
      continue;

    if (!first)
      throw YamlException{std::format("Duplicate key '{}' in {}", *node.Key,
                                      node.Location.ToString())};

    first = false;
    result = &node;
  }

  return result;
}

std::vector<std::pair<std::string_view, const MiniYaml*>> MiniYaml::ToDictionary() const {
  std::vector<std::pair<std::string_view, const MiniYaml*>> ret;
  ret.reserve(Nodes.size());
  std::unordered_set<std::string_view> seen;
  for (const MiniYamlNode& y : Nodes) {
    const std::string_view keyView = KeyViewOrNull(y.Key);
    if (!seen.insert(keyView).second)
      throw YamlException{std::format("Duplicate key '{}' in {}", keyView,
                                      y.Location.ToString())};
    ret.emplace_back(keyView, &y.Value);
  }
  return ret;
}

// ———— 解析 ————
// ———— Parsing ————

std::vector<MiniYamlNode> MiniYaml::FromLines(std::span<const std::string_view> lines,
                                              std::string_view name,
                                              bool discardCommentsAndWhitespace,
                                              StringPool& pool) {
  const std::string* namePtr = pool.Intern(name);

  struct ParsedLine {
    int Level;
    const std::string* Key;
    const std::string* Value;
    const std::string* Comment;
    int LineNo;
  };

  std::vector<std::vector<MiniYamlNode>> result(1);
  std::vector<ParsedLine> parsedLines;

  // BuildCompletedSubNode(MiniYaml.cs L360-390)
  auto buildCompletedSubNode = [&](int level) {
    const int lastLevel = parsedLines.back().Level;
    while (static_cast<int>(result.size()) <= lastLevel)
      result.emplace_back();

    while (!parsedLines.empty() && parsedLines.back().Level >= level) {
      const ParsedLine& parent = parsedLines.back();
      std::size_t startOfRange = parsedLines.size() - 1;
      while (startOfRange > 0 && parsedLines[startOfRange - 1].Level == parent.Level)
        startOfRange--;

      // 同组的非末位兄弟(其子树已在先前的构造中消化,照抄上游不带子节点)
      // Non-last siblings in the group (their subtrees were already consumed by
      // earlier construction; like upstream, no child nodes are attached)
      for (std::size_t i = startOfRange; i + 1 < parsedLines.size(); i++) {
        const ParsedLine& sibling = parsedLines[i];
        result[static_cast<std::size_t>(parent.Level)].emplace_back(
            sibling.Key, MiniYaml{sibling.Value}, sibling.Comment,
            SourceLocation{namePtr, sibling.LineNo});
      }

      std::vector<MiniYamlNode>* childNodes =
          static_cast<std::size_t>(parent.Level) + 1 < result.size()
              ? &result[static_cast<std::size_t>(parent.Level) + 1]
              : nullptr;
      result[static_cast<std::size_t>(parent.Level)].emplace_back(
          parent.Key,
          MiniYaml{parent.Value,
                   childNodes != nullptr ? std::move(*childNodes) : std::vector<MiniYamlNode>{}},
          parent.Comment, SourceLocation{namePtr, parent.LineNo});
      if (childNodes != nullptr)
        childNodes->clear();

      parsedLines.erase(parsedLines.begin() + static_cast<std::ptrdiff_t>(startOfRange),
                        parsedLines.end());
    }
  };

  int lineNo = 0;
  for (const std::string_view line : lines) {
    ++lineNo;

    std::size_t keyStart = 0;
    int level = 0;
    int spaces = 0;
    bool textStart = false;

    std::string_view key;
    std::string_view value;
    std::string_view comment;
    bool hasComment = false;
    std::string valueScratch;  // "\\#"→"#" 替换结果(存活至入池) | "\\#"→"#" replacement result (kept alive until interning)

    if (!line.empty()) {
      // 前导空白扫描:每 4 空格(跨 tab 累计)或每 tab = 1 级
      // Leading-whitespace scan: every 4 spaces (count accumulates across tabs) or each tab = 1 level
      char currChar = line[keyStart];
      while (!(currChar == '\n' || currChar == '\r') && keyStart < line.size() && !textStart) {
        currChar = line[keyStart];
        switch (currChar) {
          case ' ':
            spaces++;
            if (spaces >= kSpacesPerLevel) {
              spaces = 0;
              level++;
            }
            keyStart++;
            break;
          case '\t':
            level++;
            keyStart++;
            break;
          default:
            textStart = true;
            break;
        }
      }

      // 提取 <key>: <value>#<comment>('#' 可经 \# 转义进值;键两端总裁剪;
      // 值两端裁剪可由 \ 守护)
      // Extract <key>: <value>#<comment> ('#' can be escaped into the value via
      // \#; key ends are always trimmed; value-end trimming can be guarded by \)
      auto keyLength = static_cast<std::ptrdiff_t>(line.size()) -
                       static_cast<std::ptrdiff_t>(keyStart);
      std::ptrdiff_t valueStart = -1;
      std::size_t valueLength = 0;
      std::ptrdiff_t commentStart = -1;
      for (std::size_t i = 0; i < line.size(); i++) {
        if (valueStart < 0 && line[i] == ':') {
          valueStart = static_cast<std::ptrdiff_t>(i) + 1;
          keyLength = static_cast<std::ptrdiff_t>(i) - static_cast<std::ptrdiff_t>(keyStart);
          valueLength = line.size() - i - 1;
        }

        if (commentStart < 0 && line[i] == '#' && (i == 0 || line[i - 1] != '\\')) {
          commentStart = static_cast<std::ptrdiff_t>(i) + 1;
          if (static_cast<std::ptrdiff_t>(i) <=
              static_cast<std::ptrdiff_t>(keyStart) + keyLength)
            keyLength = static_cast<std::ptrdiff_t>(i) - static_cast<std::ptrdiff_t>(keyStart);
          else
            valueLength =
                static_cast<std::size_t>(static_cast<std::ptrdiff_t>(i) - valueStart);
          break;
        }
      }

      if (keyLength > 0)
        key = TrimNetWhiteSpace(
            line.substr(keyStart, static_cast<std::size_t>(keyLength)));

      if (valueStart >= 0) {
        const std::string_view trimmed = TrimNetWhiteSpace(
            line.substr(static_cast<std::size_t>(valueStart), valueLength));
        if (!trimmed.empty())
          value = trimmed;
      }

      if (commentStart >= 0 && !discardCommentsAndWhitespace) {
        comment = line.substr(static_cast<std::size_t>(commentStart));
        hasComment = true;
      }

      if (value.size() > 1) {
        // 剥离 \ 与空格/tab 组成的首尾空白守护
        // Strip the \ guards around leading/trailing whitespace (\ + space/tab)
        const std::size_t trimLeading =
            value[0] == '\\' && (value[1] == ' ' || value[1] == '\t') ? 1 : 0;
        const std::size_t trimTrailing =
            value[value.size() - 1] == '\\' &&
                    (value[value.size() - 2] == ' ' || value[value.size() - 2] == '\t')
                ? 1
                : 0;
        if (trimLeading + trimTrailing > 0)
          value = value.substr(trimLeading, value.size() - trimLeading - trimTrailing);

        // 值内的 \# 还原为 #
        // Restore \# inside the value back to #
        if (value.find("\\#") != std::string_view::npos) {
          valueScratch = ReplaceAll(value, "\\#", "#");
          value = valueScratch;
        }
      }
    }

    if (!key.empty() || !discardCommentsAndWhitespace) {
      if (!parsedLines.empty() && parsedLines.back().Level < level - 1)
        throw YamlException{std::format("Bad indent in miniyaml at {}:{}",
                                        std::string_view{*namePtr}, lineNo)};

      while (!parsedLines.empty() && parsedLines.back().Level > level)
        buildCompletedSubNode(level);

      const std::string* keyString = key.empty() ? nullptr : pool.Intern(key);
      const std::string* valueString = value.empty() ? nullptr : pool.Intern(value);
      // 空 comment('#' 结尾)须入池为 "" 以支持再序列化(MiniYaml.cs L340-342)
      // An empty comment (line ending in '#') must be interned as "" to support
      // re-serialization (MiniYaml.cs L340-342)
      const std::string* commentString = hasComment ? pool.Intern(comment) : nullptr;

      parsedLines.push_back(ParsedLine{level, keyString, valueString, commentString, lineNo});
    }
  }

  if (!parsedLines.empty())
    buildCompletedSubNode(0);

  return std::move(result[0]);
}

std::vector<MiniYamlNode> MiniYaml::FromFile(const std::string& path,
                                             bool discardCommentsAndWhitespace, StringPool& pool) {
  std::ifstream f{path, std::ios::binary};
  if (!f)
    throw YamlException{std::format("Could not find file '{}'.", path)};
  std::string bytes((std::istreambuf_iterator<char>{f}), std::istreambuf_iterator<char>{});
  return FromStream(bytes, path, discardCommentsAndWhitespace, pool);
}

std::vector<MiniYamlNode> MiniYaml::FromStream(std::string_view bytes, std::string_view name,
                                               bool discardCommentsAndWhitespace,
                                               StringPool& pool) {
  return FromLines(SplitLinesStream(StripUtf8Bom(bytes)), name, discardCommentsAndWhitespace,
                   pool);
}

std::vector<MiniYamlNode> MiniYaml::FromString(std::string_view text, std::string_view name,
                                               bool discardCommentsAndWhitespace,
                                               StringPool& pool) {
  return FromLines(SplitLinesString(text), name, discardCommentsAndWhitespace, pool);
}

// ———— Merge(MiniYaml.cs L408-435)————

std::vector<MiniYamlNode> MiniYaml::Merge(std::vector<std::vector<MiniYamlNode>> sources) {
  if (sources.empty())
    return {};

  // Aggregate(MergePartial):首源为种子,后续逐源合并(先各做 MergeSelfPartial)
  // Aggregate(MergePartial): the first source is the seed, later sources are
  // merged in one by one (each pre-merged via MergeSelfPartial)
  std::vector<MiniYamlNode> aggregated = MergeSelfPartial(sources.front());
  for (std::size_t i = 1; i < sources.size(); i++)
    aggregated = MergePartialList(aggregated, MergeSelfPartial(sources[i]));

  Tree tree;
  TreeIndex treeIndex;
  for (const MiniYamlNode& n : aggregated) {
    if (n.Key == nullptr)
      continue;
    if (!treeIndex.emplace(std::string_view{*n.Key}, tree.size()).second)
      // C# ToDictionary 抛 ArgumentException(消息文本随 .NET 版本,取经典文本)
      // C# ToDictionary throws ArgumentException (message text varies across
      // .NET versions; the classic text is used)
      throw YamlException{"An item with the same key has already been added."};
    tree.emplace_back(n.Key, n.Value);
  }

  // 继承自父到子跟踪,不从子回漏到父的兄弟(MiniYaml.cs L425)
  // Inheritance is tracked from parent to child and never leaks from a child
  // back into the parent's siblings (MiniYaml.cs L425)
  std::vector<MiniYamlNode> resolvedTop;
  for (const auto& [keyPtr, value] : tree) {
    InheritedMap inherited;
    inherited.emplace(std::string_view{*keyPtr}, SourceLocation{});
    std::vector<MiniYamlNode> children = ResolveInherits(value, tree, treeIndex, std::move(inherited));
    resolvedTop.emplace_back(keyPtr, MiniYaml{value.Value, std::move(children)}, nullptr);
  }

  // 解析顶层弱删除(如整块移除 actor),经由 '-' 分支走一遍
  // Resolve top-level weak removals (e.g. removing a whole actor block) by
  // running them through the '-' branch once
  const MiniYaml nodes{nullptr, std::move(resolvedTop)};  // C# 值为 "",仅作容器 | C# value is "", acting only as a container
  return ResolveInherits(nodes, tree, treeIndex, {});
}

// ———— 序列化(MiniYaml.cs L670-682 / MiniYamlExts L22-59)————
// ———— Serialization (MiniYaml.cs L670-682 / MiniYamlExts L22-59) ————

void MiniYaml::ToLines(std::vector<std::string>& lines, const std::string* key,
                       const std::string* comment) const {
  const bool hasKey = key != nullptr && !key->empty();
  const bool hasValue = Value != nullptr && !Value->empty();
  const bool hasComment = comment != nullptr;

  std::string line;
  if (hasKey) {
    line += *key;
    line += ':';
  }
  if (hasValue) {
    line += ' ';
    line += EscapeHash(*Value);
  }
  if (hasComment) {
    if (hasKey || hasValue)
      line += ' ';
    line += '#';
    line += *comment;
  }
  lines.push_back(std::move(line));

  const std::size_t mark = lines.size();
  ora::yaml::ToLines(std::span<const MiniYamlNode>{Nodes}, lines);
  for (std::size_t i = mark; i < lines.size(); i++)
    lines[i].insert(lines[i].begin(), '\t');
}

void ToLines(std::span<const MiniYamlNode> y, std::vector<std::string>& lines) {
  for (const MiniYamlNode& kv : y)
    kv.Value.ToLines(lines, kv.Key, kv.Comment);
}

std::string WriteToString(std::span<const MiniYamlNode> y) {
  // 去掉全部尾部换行后补一个 EOF 换行(MiniYaml.cs L31)
  // Strip all trailing newlines, then append a single EOF newline (MiniYaml.cs L31)
  std::vector<std::string> lines;
  ToLines(y, lines);
  std::string ret;
  for (std::size_t i = 0; i < lines.size(); i++) {
    if (i > 0)
      ret += '\n';
    ret += lines[i];
  }
  while (!ret.empty() && ret.back() == '\n')
    ret.pop_back();
  ret += '\n';
  return ret;
}

void WriteToFile(std::span<const MiniYamlNode> y, const std::string& filename) {
  std::vector<std::string> lines;
  ToLines(y, lines);
  std::ofstream f{filename, std::ios::binary | std::ios::trunc};
  if (!f)
    throw YamlException{std::format("Could not write file '{}'.", filename)};
  for (const std::string& line : lines) {
    f << TrimEndNetWhiteSpace(line);  // 每行 TrimEnd() | TrimEnd() per line
#ifdef _WIN32
    f << "\r\n";  // File.WriteAllLines 用 Environment.NewLine | File.WriteAllLines uses Environment.NewLine
#else
    f << '\n';
#endif
  }
}

// ———— 构建器(MiniYaml.cs L701-784)————
// ———— Builders (MiniYaml.cs L701-784) ————

MiniYamlBuilder::MiniYamlBuilder() = default;
MiniYamlBuilder::MiniYamlBuilder(const std::string* str_v) : Value{str_v} {}
MiniYamlBuilder::MiniYamlBuilder(const std::string* str_v, std::vector<MiniYamlNodeBuilder> nodes)
    : Value{str_v}, Nodes{std::move(nodes)} {}

MiniYamlBuilder::MiniYamlBuilder(const MiniYaml& yaml) : Value{yaml.Value} {
  Nodes.reserve(yaml.Nodes.size());
  for (const MiniYamlNode& n : yaml.Nodes)
    Nodes.emplace_back(n);
}

MiniYaml MiniYamlBuilder::Build() const {
  std::vector<MiniYamlNode> nodes;
  nodes.reserve(Nodes.size());
  for (const MiniYamlNodeBuilder& n : Nodes)
    nodes.push_back(n.Build());
  return MiniYaml{Value, std::move(nodes)};
}

void MiniYamlBuilder::ToLines(std::vector<std::string>& lines, const std::string* key,
                              const std::string* comment) const {
  const bool hasKey = key != nullptr && !key->empty();
  const bool hasValue = Value != nullptr && !Value->empty();
  const bool hasComment = comment != nullptr;

  std::string line;
  if (hasKey) {
    line += *key;
    line += ':';
  }
  if (hasValue) {
    line += ' ';
    line += EscapeHash(*Value);
  }
  if (hasComment) {
    if (hasKey || hasValue)
      line += ' ';
    line += '#';
    line += *comment;
  }
  lines.push_back(std::move(line));

  for (const MiniYamlNodeBuilder& kv : Nodes) {
    const std::size_t mark = lines.size();
    kv.Value.ToLines(lines, kv.Key, kv.Comment);
    for (std::size_t i = mark; i < lines.size(); i++)
      lines[i].insert(lines[i].begin(), '\t');
  }
}

const MiniYamlNodeBuilder* MiniYamlBuilder::NodeWithKeyOrDefault(std::string_view key) const {
  // C# SingleOrDefault:多于一个匹配时抛 InvalidOperationException(同文本)
  // C# SingleOrDefault: throws InvalidOperationException when more than one
  // element matches (same text)
  const MiniYamlNodeBuilder* found = nullptr;
  for (const MiniYamlNodeBuilder& n : Nodes) {
    if (n.Key == nullptr || std::string_view{*n.Key} != key)
      continue;
    if (found != nullptr)
      throw YamlException{"Sequence contains more than one matching element"};
    found = &n;
  }
  return found;
}

MiniYamlNodeBuilder::MiniYamlNodeBuilder(const MiniYamlNode& node)
    : Location{node.Location}, Key{node.Key}, Value{node.Value}, Comment{node.Comment} {}

}  // namespace ora::yaml
