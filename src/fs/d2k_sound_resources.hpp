#pragma once
import std;

#include "fs/i_package.hpp"

namespace ora::fs {

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

class D2kSoundResourcesLoader final : public IPackageLoader {
 public:
  bool TryParsePackage(std::span<const char> vec_bytes, const std::string& str_filename,
                       FileSystem& context,
                       std::unique_ptr<IReadOnlyPackage>& package) override;
};

}
