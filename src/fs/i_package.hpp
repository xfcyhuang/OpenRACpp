#pragma once
import std;

namespace ora::fs {

class FileSystem;

class IPackageLoader {
 public:
  virtual ~IPackageLoader() = default;
  virtual bool TryParsePackage(std::span<const char> bytes, const std::string& filename,
                               FileSystem& context,
                               std::unique_ptr<class IReadOnlyPackage>& package) = 0;
};

class IReadOnlyPackage {
 public:
  virtual ~IReadOnlyPackage() = default;
  virtual const std::string& Name() const = 0;

  virtual std::vector<std::string> Contents() const = 0;
  virtual std::optional<std::vector<char>> GetStream(const std::string& filename) const = 0;
  virtual bool Contains(const std::string& filename) const = 0;
  virtual std::unique_ptr<IReadOnlyPackage> OpenPackage(const std::string& filename,
                                                        FileSystem& context) const = 0;
};

class IReadWritePackage : public IReadOnlyPackage {
 public:
  virtual void Update(const std::string& filename, std::span<const char> contents) = 0;
  virtual void Delete(const std::string& filename) = 0;
};

}
