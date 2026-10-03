// UPSTREAM: OpenRA.Game/FileSystem/FileSystem.cs @7d57605 L20-303(逐语义重写)
//          (IReadOnlyFileSystem 同文件 L20-27)
// 挂载顺序 = 覆盖优先级:后挂载的包在文件索引中排在列表尾部,查找取最后一个
// 包含该文件的包(GetFromCache 的 LastOrDefault,L193-199)。
// Mount order is override priority: later-mounted packages append to the tail
// of each per-file package list, and lookup takes the LAST package containing
// the file (GetFromCache's LastOrDefault, L193-199).
// 已登记偏离(docs/COVERAGE.md)/ Registered deviations (docs/COVERAGE.md):
//  1. '$' mod 引用挂载与 installedMods/Manifest 依赖推迟 Phase 2
//     the '$' mod-reference mount and the installedMods/Manifest dependency
//     are deferred to Phase 2;
//  2. ResolveCaseInsensitivePath 基于 std::filesystem 枚举等价实现
//     ResolveCaseInsensitivePath is reimplemented equivalently over
//     std::filesystem enumeration.
#pragma once
import std;

#include "fs/i_package.hpp"

namespace ora::fs {

/// FileSystem(FileSystem.cs L29):按挂载序组织的只读虚拟文件系统;
/// IReadOnlyFileSystem 接口方法(Open/TryOpen/Exists/…)直接以成员提供
/// FileSystem (FileSystem.cs L29): the read-only virtual file system ordered
/// by mount sequence; the IReadOnlyFileSystem surface (Open/TryOpen/Exists/…)
/// is provided directly as members.
class FileSystem final {
 public:
  /// packageLoaders 额外加载器(zip 内建加载器自动追加在末尾)
  /// Extra package loaders (the built-in zip loader is appended last).
  explicit FileSystem(std::vector<std::unique_ptr<IPackageLoader>> packageLoaders = {});

  bool TryParsePackage(std::span<const char> bytes, const std::string& filename,
                       std::unique_ptr<IReadOnlyPackage>& package);

  /// 打开(可能嵌套的)包:目录优先,其次 '|' 显式前缀父包,最后按内容嗅探
  /// Opens a (possibly nested) package: directories first, then the '|'
  /// explicit-prefix parent, then content sniffing.
  std::unique_ptr<IReadOnlyPackage> OpenPackage(const std::string& filename);

  /// 以路径/文件名挂载;失败抛异常(除非 name 以 '~' 前缀标记为可选,吞错)
  /// Mount by path/filename; throws on failure (unless the name carries the
  /// '~' optional prefix, in which case errors are swallowed).
  void Mount(const std::string& name, const std::string& explicitName = "");
  /// 具名挂载已打开的包(所有权移交 FileSystem)
  /// Mounts an already-opened package (ownership transfers to the FileSystem).
  void Mount(std::unique_ptr<IReadOnlyPackage> package, const std::string& explicitName = "");

  bool Unmount(const IReadOnlyPackage* package);
  void UnmountAll();

  std::vector<std::string> MountedPackageNames() const;

  /// 找不到抛异常 / throws when not found.
  std::vector<char> Open(const std::string& filename) const;
  bool TryGetPackageContaining(const std::string& path, IReadOnlyPackage*& package,
                               std::string& filename) const;
  bool TryOpen(const std::string& filename, std::vector<char>& bytes) const;
  bool Exists(const std::string& filename) const;

  /// 上游静态方法(L276-297):把路径按真实大小写逐级解析;失败返回空
  /// Upstream static (L276-297): resolves a path segment by segment against
  /// the real on-disk casing; empty on failure.
  static std::string ResolveCaseInsensitivePath(const std::string& path);

 private:
  /// GetFromCache(L193-199):文件索引中最后一个包含 filename 的包
  /// GetFromCache (L193-199): the last package in the file index that
  /// contains filename.
  std::optional<std::vector<char>> GetFromCache(const std::string& filename) const;

  std::vector<std::unique_ptr<IPackageLoader>> vec_packageLoaders_;

  /// 所有权 + 挂载序;vec_mounted_ 展开重复挂载(每引用一次一个槽,
  /// 对应 C# Dictionary<IReadOnlyPackage,int> 的计数)
  /// Ownership + mount order; vec_mounted_ expands re-mounts (one slot per
  /// reference, mirroring the C# Dictionary<IReadOnlyPackage,int> counts).
  std::vector<std::unique_ptr<IReadOnlyPackage>> vec_owned_;
  std::vector<IReadOnlyPackage*> vec_mounted_;
  std::unordered_map<std::string, IReadOnlyPackage*> map_explicitMounts_;
  /// 文件名 → 按挂载序的包列表
  /// filename → packages in mount order.
  std::unordered_map<std::string, std::vector<IReadOnlyPackage*>> map_fileIndex_;
};

}  // namespace ora::fs
