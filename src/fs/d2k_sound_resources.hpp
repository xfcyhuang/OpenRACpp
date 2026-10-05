// UPSTREAM: OpenRA.Mods.D2k/PackageLoaders/D2kSoundResources.cs @b6fc03f
//          L20-94(全文逐语义)
// d2k 声音资源包(.rs):u32 头长界定目录区,条目 = ASCIIZ 名 + u32 偏移 +
// u32 长度;内容按文件绝对偏移切取。OpenPackage 上游即 "Not implemented"
// (返回 null)。
// The d2k sound-resources package (.rs): a u32 header length bounds the
// directory area; entries = an ASCIIZ name + u32 offset + u32 length, with
// content sliced at absolute file offsets. OpenPackage is upstream's
// "Not implemented" (returns null).
#pragma once
import std;

#include "fs/i_package.hpp"

namespace ora::fs {

/// D2kSoundResources(D2kSoundResources.cs L22-80 嵌套类)。
class D2kSoundResources final : public IReadOnlyPackage {
 public:
  D2kSoundResources(std::vector<char> vec_bytes, std::string str_filename);
  ~D2kSoundResources() override = default;

  D2kSoundResources(const D2kSoundResources&) = delete;
  D2kSoundResources& operator=(const D2kSoundResources&) = delete;

  const std::string& Name() const override { return str_name_; }
  std::vector<std::string> Contents() const override;
  std::optional<std::vector<char>> GetStream(const std::string& str_filename) const override;
  bool Contains(const std::string& str_filename) const override;
  std::unique_ptr<IReadOnlyPackage> OpenPackage(const std::string& str_filename,
                                                FileSystem& context) const override;

 private:
  struct Entry {
    std::uint32_t uint4_offset{};
    std::uint32_t uint4_length{};
  };

  std::vector<char> vec_bytes_;
  std::string str_name_;
  std::unordered_map<std::string, Entry> map_index_;
};

/// D2kSoundResourcesLoader(D2kSoundResources.cs L20):".rs" 后缀嗅探。
/// D2kSoundResourcesLoader (D2kSoundResources.cs L20): the ".rs" suffix
/// sniff.
class D2kSoundResourcesLoader final : public IPackageLoader {
 public:
  bool TryParsePackage(std::span<const char> vec_bytes, const std::string& str_filename,
                       FileSystem& context,
                       std::unique_ptr<IReadOnlyPackage>& package) override;
};

}  // namespace ora::fs
