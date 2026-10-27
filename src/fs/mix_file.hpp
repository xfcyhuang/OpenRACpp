#pragma once
import std;

#include "fs/i_package.hpp"
#include "fs/package_entry.hpp"

namespace ora::fs {

class MixFile final : public IReadOnlyPackage {
 public:

  MixFile(std::vector<char> vec_bytes, std::string str_filename,
          std::span<const std::string> vec_global_filenames);
  ~MixFile() override = default;

  MixFile(const MixFile&) = delete;
  MixFile& operator=(const MixFile&) = delete;

  const std::string& Name() const override { return str_name_; }
  std::vector<std::string> Contents() const override;
  std::optional<std::vector<char>> GetStream(const std::string& str_filename) const override;
  bool Contains(const std::string& str_filename) const override;
  std::unique_ptr<IReadOnlyPackage> OpenPackage(const std::string& str_filename,
                                                FileSystem& context) const override;

  std::vector<std::pair<std::string, PackageEntry>> AbsoluteIndex() const;

  bool IsCncMix() const { return b_isCncMix_; }
  bool IsEncrypted() const { return b_isEncrypted_; }
  std::int64_t DataStart() const { return int8_dataStart_; }

 private:

  std::unordered_map<std::string, PackageEntry> ParseIndex(
      const std::unordered_map<std::uint32_t, PackageEntry>& map_entries,
      std::span<const std::string> vec_global_filenames);

  std::span<const char> ContentOf(const PackageEntry& entry) const;

  std::vector<char> vec_bytes_;
  std::string str_name_;
  std::unordered_map<std::string, PackageEntry> map_index_;
  std::int64_t int8_dataStart_ = 0;
  bool b_isCncMix_ = false;
  bool b_isEncrypted_ = false;
};

class MixLoader final : public IPackageLoader {
 public:
  bool TryParsePackage(std::span<const char> vec_bytes, const std::string& str_filename,
                       FileSystem& context,
                       std::unique_ptr<IReadOnlyPackage>& package) override;

 private:

  std::unique_ptr<std::vector<std::string>> ptr_globalFilenames_;
};

}
