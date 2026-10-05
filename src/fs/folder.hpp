// UPSTREAM: OpenRA.Game/FileSystem/Folder.cs @b6fc03f L19-110(逐语义重写)
// 目录包:Contents 顺序影响哈希(上游注释 L34),此处按字节序排序——
// mods 的 ASCII 文件名集合下与 C# .Order()(culture 序)等价,已登记 COVERAGE。
// Directory package: the Contents order matters for hashing (upstream comment
// L34); we sort bytewise — equivalent to the C# .Order() (culture order) over
// the ASCII filename set of the mods, as registered in COVERAGE.
#pragma once
import std;

#include "fs/i_package.hpp"

namespace ora::fs {

/// Folder(Folder.cs L19):以真实目录为包;目录不存在则创建
/// Folder (Folder.cs L19): a real filesystem directory as a package; the
/// directory is created if missing.
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

}  // namespace ora::fs
