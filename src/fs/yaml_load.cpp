#include "fs/file_system.hpp"
#include "yaml/mini_yaml.hpp"

#include "core/text.hpp"

namespace ora::yaml {

std::vector<MiniYamlNode> MiniYaml::Load(fs::FileSystem& fileSystem,
                                         std::span<const std::string> files,
                                         const MiniYaml* mapRules, StringPool& pool) {
  std::vector<std::string> allFiles{files.begin(), files.end()};
  if (mapRules != nullptr && mapRules->Value != nullptr) {

    for (const auto part : std::views::split(std::string_view{*mapRules->Value}, ',')) {
      const std::string_view p =
          TrimNetWhiteSpace(std::string_view{part.begin(), part.end()});
      if (!p.empty())
        allFiles.emplace_back(p);
    }
  }

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

}
