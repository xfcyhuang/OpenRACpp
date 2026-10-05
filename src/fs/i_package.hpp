// UPSTREAM: OpenRA.Game/FileSystem/IPackage.cs @b6fc03f L18-42(接口逐语义移植)
// 包(Package)= 文件系统的一个可挂载单元:目录(Folder)、zip、mix……
// A "package" is one mountable unit of the virtual file system: a directory
// (Folder), a zip, a mix, ...
// 设计决策(docs/COVERAGE.md):C# GetStream 返回惰性 Stream,此处返回
// std::optional<std::vector<char>>(立即读全)——上游消费方均为全量读取,
// 语义等价且免去流生命周期管理。
// Design decision (docs/COVERAGE.md): the C# GetStream returns a lazy Stream;
// here it returns std::optional<std::vector<char>> (read in full) — every
// upstream consumer reads the stream through, so this is semantically
// equivalent and avoids stream lifetime management.
#pragma once
import std;

namespace ora::fs {

class FileSystem;

/// IPackageLoader(IPackage.cs L18-26):按内容嗅探解析包;成功则接管数据,
/// 失败须保持无副作用
/// IPackageLoader (IPackage.cs L18-26): content-sniffing package parser; on
/// success it takes ownership of the data, on failure it must have no effect.
class IPackageLoader {
 public:
  virtual ~IPackageLoader() = default;
  virtual bool TryParsePackage(std::span<const char> bytes, const std::string& filename,
                               FileSystem& context,
                               std::unique_ptr<class IReadOnlyPackage>& package) = 0;
};

/// IReadOnlyPackage(IPackage.cs L28-35)
class IReadOnlyPackage {
 public:
  virtual ~IReadOnlyPackage() = default;
  virtual const std::string& Name() const = 0;
  /// 条目名列表(zip 内为 '/' 分隔原始名;目录条目含尾部 '/')
  /// Entry names ('/'-separated inside zips; directory entries keep the
  /// trailing '/').
  virtual std::vector<std::string> Contents() const = 0;
  virtual std::optional<std::vector<char>> GetStream(const std::string& filename) const = 0;
  virtual bool Contains(const std::string& filename) const = 0;
  virtual std::unique_ptr<IReadOnlyPackage> OpenPackage(const std::string& filename,
                                                        FileSystem& context) const = 0;
};

/// IReadWritePackage(IPackage.cs L37-41):可写包(设置保存、地图导出)
/// IReadWritePackage (IPackage.cs L37-41): writable package (settings saving,
/// map export).
class IReadWritePackage : public IReadOnlyPackage {
 public:
  virtual void Update(const std::string& filename, std::span<const char> contents) = 0;
  virtual void Delete(const std::string& filename) = 0;
};

}  // namespace ora::fs
