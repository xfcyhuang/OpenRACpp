#pragma once
import std;

#include "fs/i_package.hpp"

namespace ora::fs {

class ReadOnlyZipFile final : public IReadOnlyPackage {
 public:

  ReadOnlyZipFile(std::vector<char> bytes, std::string filename);

  ReadOnlyZipFile(const ReadOnlyZipFile&) = delete;
  ReadOnlyZipFile& operator=(const ReadOnlyZipFile&) = delete;

  const std::string& Name() const override { return str_name_; }
  std::vector<std::string> Contents() const override;
  std::optional<std::vector<char>> GetStream(const std::string& filename) const override;
  bool Contains(const std::string& filename) const override;
  std::unique_ptr<IReadOnlyPackage> OpenPackage(const std::string& filename,
                                                FileSystem& context) const override;

 private:
  std::vector<char> vec_bytes_;
  std::string str_name_;
  std::unique_ptr<void, void (*)(void*)> up_reader_{
      nullptr, nullptr};
};

class ZipFileLoader final : public IPackageLoader {
 public:
  bool TryParsePackage(std::span<const char> bytes, const std::string& filename,
                       FileSystem& context,
                       std::unique_ptr<IReadOnlyPackage>& package) override;
};

}
