#pragma once
import std;

#include "fs/i_package.hpp"

namespace ora::fs {

class Folder final : public IReadWritePackage {
 public:
  explicit Folder(const std::string& path);

  const std::string& Name() const override { return str_name_; }
  std::vector<std::string> Contents() const override;
  std::optional<std::vector<char>> GetStream(const std::string& filename) const override;
  bool Contains(const std::string& filename) const override;
  std::unique_ptr<IReadOnlyPackage> OpenPackage(const std::string& filename,
                                                FileSystem& context) const override;

  void Update(const std::string& filename, std::span<const char> contents) override;
  void Delete(const std::string& filename) override;

 private:
  std::string str_name_;
};

}
