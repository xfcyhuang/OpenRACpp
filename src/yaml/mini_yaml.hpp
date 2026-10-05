// UPSTREAM: OpenRA.Game/MiniYaml.cs @b6fc03f L22-791(全文件逐语义重写)
//          Full-file statement-by-statement rewrite of upstream MiniYaml.cs.
// 已登记偏离(docs/COVERAGE.md)/ Registered deviations (docs/COVERAGE.md):
//  1. 异常类型统一为 YamlException(C# 分散使用 YamlException/InvalidDataException/
//     ArgumentException),消息文本逐字一致
//     All error types are unified into YamlException (C# scatters
//     YamlException/InvalidDataException/ArgumentException); message texts are verbatim.
//  2. C# ResolveInherits 对 null 键节点会 NullReferenceException(上游潜在 bug),
//     C++ 侧 null 键走 else 分支(与上游合法输入下的行为一致)
//     C# ResolveInherits NullReferenceExceptions on null-key nodes (latent upstream
//     bug); the C++ side routes null keys to the else branch (identical behaviour
//     for all valid upstream inputs).
//  3. ToDictionary/合并键集合中 null 键与 "" 键视为同键(上游解析器不会产生 "" 键)
//     In ToDictionary/merge key-sets, a null key is conflated with "" (the upstream
//     parser never produces "" keys).
//
// 字符串模型 / String model:
// C# MiniYaml 的 string 字段可为 null 且引用池化;此处统一为 `const std::string*`
// —— nullptr 即 C# null,非空指针指向 StringPool 内的池化字符串(地址稳定、内容即
// 等价)。参与 Merge 的节点必须来自同一 StringPool(上游 MiniYaml.Load 同样以单池
// 贯穿一次加载,见 MiniYaml.cs L692)。
// C# MiniYaml string fields are nullable, pooled references; here they become
// `const std::string*` — nullptr is C# null, a non-null pointer targets a pooled
// string inside a StringPool (stable address, content equivalence). Nodes fed into
// Merge must come from one StringPool (upstream MiniYaml.Load threads a single pool
// through a load as well, see MiniYaml.cs L692).
#pragma once
import std;

namespace ora::fs {
class FileSystem;  // MiniYaml::Load 的文件系统参数(实现见 src/fs/yaml_load.cpp)
                   // The filesystem parameter of MiniYaml::Load (implemented in
                   // src/fs/yaml_load.cpp).
}  // namespace ora::fs

namespace ora::yaml {

/// 字符串池(MiniYaml.cs L203-211 的 stringPool 参数):Key/Value/Comment 与
/// SourceLocation.Name 全部入池去重;Intern 即 C# 闭包 GetOrAdd。
/// String pool (the stringPool parameter of MiniYaml.cs L203-211): keys, values,
/// comments and SourceLocation.Name are all interned; Intern is the C# GetOrAdd
/// closure.
class StringPool {
 public:
  const std::string* Intern(std::string_view sv) {
    return &*pool_.emplace(sv).first;
  }

 private:
  std::unordered_set<std::string> pool_;
};

class YamlException : public std::runtime_error {
 public:
  using std::runtime_error::runtime_error;
};

/// MiniYamlNode.SourceLocation(MiniYaml.cs L63-69);Name 为池化指针,nullptr 即 C# null
/// MiniYamlNode.SourceLocation (MiniYaml.cs L63-69); Name is a pooled pointer,
/// nullptr is C# null.
struct SourceLocation {
  const std::string* Name{};
  int Line{};

  bool operator==(const SourceLocation&) const = default;

  /// C# ToString:$"{Name}:{Line}"(Name 为 null 时插值为空串)
  /// C# ToString: $"{Name}:{Line}" (a null Name interpolates as empty).
  std::string ToString() const {
    return std::format("{}:{}", Name != nullptr ? std::string_view{*Name} : std::string_view{},
                       Line);
  }
};

struct MiniYamlNode;

class MiniYaml {
 public:
  const std::string* Value{};  // nullptr = C# null(区别于池内 "")/ nullptr = C# null (distinct from pooled "")
  std::vector<MiniYamlNode> Nodes{};

  // 定义在 .cpp(MiniYamlNode 此处尚不完整)/ Defined in mini_yaml.cpp
  // (MiniYamlNode is still incomplete here).
  MiniYaml();
  explicit MiniYaml(const std::string* str_v);
  MiniYaml(const std::string* str_v, std::vector<MiniYamlNode> nodes);

  bool operator==(const MiniYaml&) const;
  MiniYaml WithValue(const std::string* str_v) const;
  MiniYaml WithNodes(std::vector<MiniYamlNode> nodes) const;
  MiniYaml WithNodesAppended(const std::vector<MiniYamlNode>& nodes) const;

  /// 抛 YamlException("No node with key '{key}'")——C# InvalidDataException 同文本
  /// Throws YamlException("No node with key '{key}'") — same text as the C#
  /// InvalidDataException.
  const MiniYamlNode& NodeWithKey(std::string_view key) const;
  /// 重复键抛 YamlException("Duplicate key '{key}' in {location}")
  /// Throws YamlException("Duplicate key '{key}' in {location}") on duplicate keys.
  const MiniYamlNode* NodeWithKeyOrDefault(std::string_view key) const;

  /// ToDictionary(MiniYaml.cs L169-192):保持 C# Dictionary 的插入序;
  /// 键为节点键的视图(节点存活期间有效),重复键抛 YamlException 同文本
  /// ToDictionary (MiniYaml.cs L169-192): preserves C# Dictionary insertion
  /// order; keys are views over node keys (valid while the nodes live);
  /// duplicate keys throw YamlException with the same text.
  std::vector<std::pair<std::string_view, const MiniYaml*>> ToDictionary() const;

  // ———— 解析入口(MiniYaml.cs L393-406)/ Parse entry points (MiniYaml.cs L393-406) ————
  /// FromFile:name(位置标注用)= path 本身
  /// FromFile: the name (for location tagging) is the path itself.
  static std::vector<MiniYamlNode> FromFile(const std::string& path,
                                            bool discardCommentsAndWhitespace = true,
                                            StringPool& pool = GlobalPool());
  /// FromStream:字节流版(等价 C# Stream 语义——UTF-8 BOM 剥离、行尾 '\r' 剥离、
  /// 末尾 '\n' 不产生空行),name 独立于实际来源路径
  /// FromStream: byte-stream variant (equivalent to the C# Stream semantics —
  /// UTF-8 BOM stripped, a trailing '\r' before '\n' stripped, a final '\n'
  /// yields no empty last line); name is independent of the source path.
  static std::vector<MiniYamlNode> FromStream(std::string_view bytes, std::string_view name,
                                              bool discardCommentsAndWhitespace = true,
                                              StringPool& pool = GlobalPool());
  /// FromString:String.Split(["\r\n","\n"]) 语义(末尾保留空行)
  /// FromString: String.Split(["\r\n","\n"]) semantics (trailing empty line kept).
  static std::vector<MiniYamlNode> FromString(std::string_view text, std::string_view name,
                                              bool discardCommentsAndWhitespace = true,
                                              StringPool& pool = GlobalPool());

  /// Merge(MiniYaml.cs L408-435):跨源合并 + 继承解析 + 顶层弱删除
  /// Merge (MiniYaml.cs L408-435): cross-source merge + inheritance resolution +
  /// top-level weak removals.
  static std::vector<MiniYamlNode> Merge(std::vector<std::vector<MiniYamlNode>> sources);

  /// Load(MiniYaml.cs L684-698):manifest 文件序列(可带地图附加文件列表)+
  /// 可选地图规则节点,单池贯穿解析后 Merge;实现位于 src/fs/yaml_load.cpp
  /// Load (MiniYaml.cs L684-698): the manifest file sequence (plus optional
  /// extra map files listed in mapRules) and the optional map-rules node,
  /// parsed through one pool and merged; implemented in src/fs/yaml_load.cpp.
  static std::vector<MiniYamlNode> Load(fs::FileSystem& fileSystem,
                                        std::span<const std::string> files,
                                        const MiniYaml* mapRules, StringPool& pool);

  /// 单节点序列化(MiniYaml.cs L670-682):首行 + 子行(每行 '\t' 前缀)
  /// Single-node serialization (MiniYaml.cs L670-682): the head line plus child
  /// lines (each prefixed with '\t').
  void ToLines(std::vector<std::string>& lines, const std::string* key,
               const std::string* comment) const;

  /// 便捷池(单文件工具/测试;引擎加载路径应自持 StringPool)
  /// Convenience pool (single-file tools/tests; engine load paths should own
  /// their StringPool).
  static StringPool& GlobalPool();

 private:
  /// FromLines(MiniYaml.cs L203-391):逐行状态机
  /// FromLines (MiniYaml.cs L203-391): the line-by-line state machine.
  static std::vector<MiniYamlNode> FromLines(std::span<const std::string_view> lines,
                                             std::string_view name,
                                             bool discardCommentsAndWhitespace, StringPool& pool);
};

struct MiniYamlNode {
  SourceLocation Location{};
  const std::string* Key{};
  MiniYaml Value;
  const std::string* Comment{};

  // 值相等(字符串按指针——池化不变量下二者等价);供 vector 比较
  // Value equality (strings compare by pointer — equivalent under the pooling
  // invariant); used by vector comparison.
  bool operator==(const MiniYamlNode&) const = default;

  MiniYamlNode() = default;
  MiniYamlNode(const std::string* str_key, MiniYaml yaml_v, const std::string* str_c = nullptr)
      : Key{str_key}, Value{std::move(yaml_v)}, Comment{str_c} {}
  MiniYamlNode(const std::string* str_key, MiniYaml yaml_v, const std::string* str_c,
               SourceLocation loc)
      : MiniYamlNode{str_key, std::move(yaml_v), str_c} {
    Location = loc;
  }

  MiniYamlNode WithValue(MiniYaml yaml_v) const {
    return MiniYamlNode{Key, std::move(yaml_v), Comment, Location};
  }

  /// C# ToString:$"{{YamlNode: {Key} @ {Location}}}"
  std::string ToString() const {
    return std::format("{{YamlNode: {} @ {}}}",
                       Key != nullptr ? std::string_view{*Key} : std::string_view{},
                       Location.ToString());
  }
};

// ———— MiniYamlExts(MiniYaml.cs L22-59)/ MiniYamlExts (MiniYaml.cs L22-59) ————

std::string WriteToString(std::span<const MiniYamlNode> y);
void WriteToFile(std::span<const MiniYamlNode> y, const std::string& filename);
void ToLines(std::span<const MiniYamlNode> y, std::vector<std::string>& lines);

// ———— 可变构建器(MiniYaml.cs L701-784;地图编辑/设置保存用)————
// ———— Mutable builders (MiniYaml.cs L701-784; map editing / settings saving) ————

struct MiniYamlNodeBuilder;

class MiniYamlBuilder {
 public:
  const std::string* Value{};
  std::vector<MiniYamlNodeBuilder> Nodes{};

  // 定义在 .cpp(MiniYamlNodeBuilder 此处尚不完整)
  // Defined in mini_yaml.cpp (MiniYamlNodeBuilder is still incomplete here).
  MiniYamlBuilder();
  explicit MiniYamlBuilder(const std::string* str_v);
  MiniYamlBuilder(const std::string* str_v, std::vector<MiniYamlNodeBuilder> nodes);
  explicit MiniYamlBuilder(const MiniYaml& yaml);

  MiniYaml Build() const;
  void ToLines(std::vector<std::string>& lines, const std::string* key,
               const std::string* comment) const;

  const MiniYamlNodeBuilder* NodeWithKeyOrDefault(std::string_view key) const;
};

struct MiniYamlNodeBuilder {
  SourceLocation Location{};
  const std::string* Key{};
  MiniYamlBuilder Value;
  const std::string* Comment{};

  MiniYamlNodeBuilder() = default;
  MiniYamlNodeBuilder(const std::string* str_key, MiniYamlBuilder yaml_v,
                      const std::string* str_c = nullptr)
      : Key{str_key}, Value{std::move(yaml_v)}, Comment{str_c} {}
  MiniYamlNodeBuilder(const std::string* str_key, MiniYamlBuilder yaml_v,
                      const std::string* str_c, SourceLocation loc)
      : MiniYamlNodeBuilder{str_key, std::move(yaml_v), str_c} {
    Location = loc;
  }

  explicit MiniYamlNodeBuilder(const MiniYamlNode& node);

  MiniYamlNode Build() const {
    return MiniYamlNode{Key, Value.Build(), Comment, Location};
  }
};

}  // namespace ora::yaml
