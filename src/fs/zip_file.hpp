// UPSTREAM: OpenRA.Game/FileSystem/ZipFile.cs @7d57605 L20-261(只读路径逐语义重写)
// ReadWriteZipFile(更新/删除条目)推迟到 Phase 6 地图保存(miniz writer 实现),
// 已登记 docs/COVERAGE.md。
// The read-only paths are rewritten statement-by-statement; ReadWriteZipFile
// (entry update/delete) is deferred to Phase 6 map saving (via the miniz
// writer), as registered in docs/COVERAGE.md.
// SharpZipLib → miniz:条目名按 zip 内原始存储('/' 分隔);大小写敏感匹配
// 与 SharpZipLib GetEntry 一致。
// SharpZipLib → miniz: entry names are the raw stored names ('/'-separated);
// case-sensitive lookup matches SharpZipLib GetEntry.
#pragma once
import std;

#include "fs/i_package.hpp"

namespace ora::fs {

/// ReadOnlyZipFile(ZipFile.cs L24-95):内存 zip 读取包
/// ReadOnlyZipFile (ZipFile.cs L24-95): in-memory zip reader package.
class ReadOnlyZipFile final : public IReadOnlyPackage {
 public:
  /// 接管 bytes 的所有权(等价 C# 构造函数接管 Stream 并整体解压到内存)
  /// Takes ownership of bytes (equivalent to the C# ctor taking over the
  /// stream and extracting into memory on demand).
  ReadOnlyZipFile(std::vector<char> bytes, std::string filename);
  ~ReadOnlyZipFile() override;

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
  void* reader_ = nullptr;  // mz_zip_archive*(实现细节,头文件不外泄 C 类型)
};

/// ZipFileLoader(ZipFile.cs L20):按本地文件头签名 0x04034b50 嗅探
/// ZipFileLoader (ZipFile.cs L20): sniffs the local-file-header signature
/// 0x04034b50.
class ZipFileLoader final : public IPackageLoader {
 public:
  bool TryParsePackage(std::span<const char> bytes, const std::string& filename,
                       FileSystem& context,
                       std::unique_ptr<IReadOnlyPackage>& package) override;
};

}  // namespace ora::fs
