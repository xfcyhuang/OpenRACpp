#pragma once
import std;

namespace ora::fs {
class FileSystem;

}

namespace ora::yaml {

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

struct SourceLocation {
  const std::string* Name{};
  int Line{};

  bool operator==(const SourceLocation&) const = default;

  std::string ToString() const {
    return std::format("{}:{}", Name != nullptr ? std::string_view{*Name} : std::string_view{},
                       Line);
  }
};

struct MiniYamlNode;

class MiniYaml {
 public:
  const std::string* Value{};
  std::vector<MiniYamlNode> Nodes{};

  MiniYaml();
  explicit MiniYaml(const std::string* str_v);
  MiniYaml(const std::string* str_v, std::vector<MiniYamlNode> nodes);

  bool operator==(const MiniYaml&) const;
  MiniYaml WithValue(const std::string* str_v) const;
  MiniYaml WithNodes(std::vector<MiniYamlNode> nodes) const;
  MiniYaml WithNodesAppended(const std::vector<MiniYamlNode>& nodes) const;

  const MiniYamlNode& NodeWithKey(std::string_view key) const;

  const MiniYamlNode* NodeWithKeyOrDefault(std::string_view key) const;

  std::vector<std::pair<std::string_view, const MiniYaml*>> ToDictionary() const;

  static std::vector<MiniYamlNode> FromFile(const std::string& path,
                                            bool discardCommentsAndWhitespace = true,
                                            StringPool& pool = GlobalPool());

  static std::vector<MiniYamlNode> FromStream(std::string_view bytes, std::string_view name,
                                              bool discardCommentsAndWhitespace = true,
                                              StringPool& pool = GlobalPool());

  static std::vector<MiniYamlNode> FromString(std::string_view text, std::string_view name,
                                              bool discardCommentsAndWhitespace = true,
                                              StringPool& pool = GlobalPool());

  static std::vector<MiniYamlNode> Merge(std::vector<std::vector<MiniYamlNode>> sources);

  static std::vector<MiniYamlNode> Load(fs::FileSystem& fileSystem,
                                        std::span<const std::string> files,
                                        const MiniYaml* mapRules, StringPool& pool);

  void ToLines(std::vector<std::string>& lines, const std::string* key,
               const std::string* comment) const;

  static StringPool& GlobalPool();

 private:

  static std::vector<MiniYamlNode> FromLines(std::span<const std::string_view> lines,
                                             std::string_view name,
                                             bool discardCommentsAndWhitespace, StringPool& pool);
};

struct MiniYamlNode {
  SourceLocation Location{};
  const std::string* Key{};
  MiniYaml Value;
  const std::string* Comment{};

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

  std::string ToString() const {
    return std::format("{{YamlNode: {} @ {}}}",
                       Key != nullptr ? std::string_view{*Key} : std::string_view{},
                       Location.ToString());
  }
};

std::string WriteToString(std::span<const MiniYamlNode> y);
void WriteToFile(std::span<const MiniYamlNode> y, const std::string& filename);
void ToLines(std::span<const MiniYamlNode> y, std::vector<std::string>& lines);

struct MiniYamlNodeBuilder;

class MiniYamlBuilder {
 public:
  const std::string* Value{};
  std::vector<MiniYamlNodeBuilder> Nodes{};

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

}
