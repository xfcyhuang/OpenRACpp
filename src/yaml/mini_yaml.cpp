#include "yaml/mini_yaml.hpp"

#include "core/text.hpp"

namespace ora::yaml {

namespace {

constexpr int kSpacesPerLevel = 4;

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

std::string EscapeHash(std::string_view text) {
  return ReplaceAll(text, "#", "\\#");
}

bool KeyEquals(const std::string* a, const std::string* b) {
  if (a == nullptr || b == nullptr)
    return a == b;
  return *a == *b;
}

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

using Tree = std::vector<std::pair<const std::string*, MiniYaml>>;
using TreeIndex = std::unordered_map<std::string_view, std::size_t>;
using InheritedMap = std::unordered_map<std::string_view, SourceLocation>;

std::vector<MiniYamlNode> ResolveInherits(const MiniYaml& node, const Tree& tree,
                                          const TreeIndex& treeIndex, InheritedMap inherited);
MiniYaml MergePartialValue(const MiniYaml* existingNodes, const MiniYaml* overrideNodes);
std::vector<MiniYamlNode> MergePartialList(const std::vector<MiniYamlNode>& existingNodes,
                                           const std::vector<MiniYamlNode>& overrideNodes);

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
      continue;
    auto& values = dups.try_emplace(keyView).first->second;
    if (values.empty()) {
      dupOrder.push_back(keyView);
      values.push_back(it->second);
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

    if (node.Key->starts_with('-')) {
      ret.push_back(node);
      return;
    }

    if (plainKeys.insert(std::string_view{*node.Key}).second) {
      ret.push_back(node);
      return;
    }

    const std::size_t previousNodeIndex = LastIndexOfKey(ret, node.Key);
    const std::string removalKey = std::string{"-"} + *node.Key;
    const std::size_t previousRemovalNodeIndex =
        LastIndexOfKey(ret, &removalKey);
    if (previousRemovalNodeIndex != static_cast<std::size_t>(-1) &&
        previousRemovalNodeIndex > previousNodeIndex) {
      ret.push_back(node);
      return;
    }

    MiniYaml merged = MergePartialValue(&ret[previousNodeIndex].Value, &node.Value);
    ret[previousNodeIndex] = node.WithValue(std::move(merged));
  };

  for (const MiniYamlNode& node : existingNodes)
    mergeNode(node);
  for (const MiniYamlNode& node : overrideNodes)
    mergeNode(node);

  return ret;
}

MiniYaml MergePartialValue(const MiniYaml* existingNodes, const MiniYaml* overrideNodes) {

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

      const std::size_t originalIndex = IndexOfKey(ret, n.Key);
      MiniYaml merged = MergePartialValue(&ret[originalIndex].Value, &n.Value);
      ret[originalIndex] = ret[originalIndex].WithValue(std::move(merged));
    }
  }

  return ret;
}

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

}

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

  auto buildCompletedSubNode = [&](int level) {
    const int lastLevel = parsedLines.back().Level;
    while (static_cast<int>(result.size()) <= lastLevel)
      result.emplace_back();

    while (!parsedLines.empty() && parsedLines.back().Level >= level) {
      const ParsedLine& parent = parsedLines.back();
      std::size_t startOfRange = parsedLines.size() - 1;
      while (startOfRange > 0 && parsedLines[startOfRange - 1].Level == parent.Level)
        startOfRange--;

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
    std::string valueScratch;

    if (!line.empty()) {

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

  const MiniYaml nodes{nullptr, std::move(resolvedTop)};
  return ResolveInherits(nodes, tree, treeIndex, {});
}

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
    f << TrimEndNetWhiteSpace(line);
#ifdef _WIN32
    f << "\r\n";
#else
    f << '\n';
#endif
  }
}

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

}
