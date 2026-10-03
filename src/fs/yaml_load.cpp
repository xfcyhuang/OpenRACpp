// UPSTREAM: OpenRA.Game/MiniYaml.cs @7d57605 L684-698(Load 逐语义移植;
// 与 FileSystem 同处一编译单元以解 yaml→fs 的依赖方向)
// The Load entry point ported statement-by-statement (kept in the fs
// translation unit so the yaml→fs dependency stays one-directional).
// mapRules.Value 的附加文件列表按 FieldLoader.ParseArray 语义解析
// (FieldLoader.cs L593-617:逗号分割 + 逐段 trim + 去空段)。
// The extra file list in mapRules.Value parses with FieldLoader.ParseArray
// semantics (FieldLoader.cs L593-617: comma split + per-part trim + drop
// empty parts).
#include "fs/file_system.hpp"
#include "yaml/mini_yaml.hpp"

#include "core/text.hpp"

namespace ora::yaml {

std::vector<MiniYamlNode> MiniYaml::Load(fs::FileSystem& fileSystem,
                                         std::span<const std::string> files,
                                         const MiniYaml* mapRules, StringPool& pool) {
  std::vector<std::string> allFiles{files.begin(), files.end()};
  if (mapRules != nullptr && mapRules->Value != nullptr) {
    // FieldLoader.GetValue<string[]>:逗号分割、去空白、去空段
    // FieldLoader.GetValue<string[]>: comma split, trim, drop empties.
    for (const auto part : std::views::split(std::string_view{*mapRules->Value}, ',')) {
      const std::string_view p =
          TrimNetWhiteSpace(std::string_view{part.begin(), part.end()});
      if (!p.empty())
        allFiles.emplace_back(p);
    }
  }

  // 单一字符串池贯穿本次加载(MiniYaml.cs L692)
  // A single string pool threads through this load (MiniYaml.cs L692).
  std::vector<std::vector<MiniYamlNode>> yaml;
  yaml.reserve(allFiles.size() + (mapRules != nullptr ? 1 : 0));
  for (const std::string& s : allFiles) {
    const std::vector<char> bytes = fileSystem.Open(s);
    yaml.push_back(MiniYaml::FromStream(std::string_view{bytes.data(), bytes.size()}, s,
                                        true, pool));
  }

  if (mapRules != nullptr && !mapRules->Nodes.empty())
    yaml.push_back(mapRules->Nodes);

  return MiniYaml::Merge(std::move(yaml));
}

}  // namespace ora::yaml
