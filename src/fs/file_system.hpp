#pragma once
import std;

#include "fs/i_package.hpp"

namespace ora::fs {

class FileSystem final {
 public:

  explicit FileSystem(std::vector<std::unique_ptr<IPackageLoader>> packageLoaders = {});

  void SetPathResolver(std::function<std::string(const std::string&)> fn_resolve);

  void SetModPackageResolver(
      std::function<IReadOnlyPackage*(const std::string&)> fn_resolve_mod);

  bool TryParsePackage(std::span<const char> bytes, const std::string& filename,
                       std::unique_ptr<IReadOnlyPackage>& package);

  std::unique_ptr<IReadOnlyPackage> OpenPackage(const std::string& filename);

  void Mount(const std::string& name, const std::string& explicitName = "");

  void Mount(std::unique_ptr<IReadOnlyPackage> package, const std::string& explicitName = "");

  void MountBorrowed(IReadOnlyPackage* package, const std::string& explicitName = "");

  bool Unmount(const IReadOnlyPackage* package);
  void UnmountAll();

  std::vector<std::string> MountedPackageNames() const;

  std::vector<IReadOnlyPackage*> MountedPackages() const;

  std::optional<std::string> GetPrefix(const IReadOnlyPackage* package) const;

  std::vector<char> Open(const std::string& filename) const;
  bool TryGetPackageContaining(const std::string& path, IReadOnlyPackage*& package,
                               std::string& filename) const;
  bool TryOpen(const std::string& filename, std::vector<char>& bytes) const;
  bool Exists(const std::string& filename) const;

  static std::string ResolveCaseInsensitivePath(const std::string& path);

 private:

  std::optional<std::vector<char>> GetFromCache(const std::string& filename) const;

  std::vector<std::unique_ptr<IPackageLoader>> vec_packageLoaders_;
  std::function<std::string(const std::string&)> fn_pathResolver_;
  std::function<IReadOnlyPackage*(const std::string&)> fn_modResolver_;

  std::vector<std::unique_ptr<IReadOnlyPackage>> vec_owned_;
  std::vector<IReadOnlyPackage*> vec_mounted_;
  std::unordered_map<std::string, IReadOnlyPackage*> map_explicitMounts_;

  std::unordered_map<std::string, std::vector<IReadOnlyPackage*>> map_fileIndex_;
};

}
