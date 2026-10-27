#include "fs/folder.hpp"

#include "fs/file_system.hpp"
#include "fs/zip_file.hpp"

namespace ora::fs {

namespace {

namespace stdfs = std::filesystem;

std::string ReadAllBytesStr(const stdfs::path& path) {
  std::ifstream f{path, std::ios::binary};
  if (!f)
    return {};
  return std::string{std::istreambuf_iterator<char>{f}, std::istreambuf_iterator<char>{}};
}

}

Folder::Folder(const std::string& path) : str_name_{path} {
  std::error_code ec;
  if (!stdfs::exists(str_name_, ec))
    stdfs::create_directories(str_name_, ec);
}

std::vector<std::string> Folder::Contents() const {

  std::vector<std::string> names;
  std::error_code ec;
  for (const auto& it : stdfs::directory_iterator(str_name_, ec)) {
    if (it.is_regular_file(ec) || it.is_directory(ec))
      names.push_back(it.path().filename().generic_string());
  }
  std::sort(names.begin(), names.end());
  return names;
}

std::optional<std::vector<char>> Folder::GetStream(const std::string& filename) const {
  const stdfs::path combined = stdfs::path{str_name_} / stdfs::path{filename};
  std::error_code ec;
  if (!stdfs::is_regular_file(combined, ec))
    return std::nullopt;
  const std::string bytes = ReadAllBytesStr(combined);
  if (!stdfs::is_regular_file(combined, ec) && bytes.empty())
    return std::nullopt;
  return std::vector<char>{bytes.begin(), bytes.end()};
}

bool Folder::Contains(const std::string& filename) const {

  const stdfs::path combined = stdfs::path{str_name_} / stdfs::path{filename};
  const std::string generic = combined.generic_string();
  if (generic.rfind(str_name_, 0) != 0)
    return false;
  std::error_code ec;
  return stdfs::is_regular_file(combined, ec);
}

std::unique_ptr<IReadOnlyPackage> Folder::OpenPackage(const std::string& filename,
                                                      FileSystem& context) const {
  const stdfs::path resolvedPath = stdfs::path{str_name_} / stdfs::path{filename};
  std::error_code ec;
  if (stdfs::is_directory(resolvedPath, ec))
    return std::make_unique<Folder>(resolvedPath.generic_string());

  const std::optional<std::vector<char>> bytes = GetStream(filename);
  if (!bytes.has_value())
    return nullptr;

  std::unique_ptr<IReadOnlyPackage> package;
  if (context.TryParsePackage(*bytes, filename, package))
    return package;
  return nullptr;
}

void Folder::Update(const std::string& filename, std::span<const char> contents) {

  const std::string genericFilename = stdfs::path{filename}.generic_string();
  const stdfs::path filePath =
      genericFilename.rfind(str_name_, 0) == 0 ? stdfs::path{filename}
                                               : stdfs::path{str_name_} / stdfs::path{filename};
  if (filePath.has_parent_path()) {
    std::error_code ec;
    stdfs::create_directories(filePath.parent_path(), ec);
  }
  std::ofstream f{filePath, std::ios::binary | std::ios::trunc};
  if (f)
    f.write(contents.data(), static_cast<std::streamsize>(contents.size()));
}

void Folder::Delete(const std::string& filename) {
  const std::string genericFilename = stdfs::path{filename}.generic_string();
  const stdfs::path filePath =
      genericFilename.rfind(str_name_, 0) == 0 ? stdfs::path{filename}
                                               : stdfs::path{str_name_} / stdfs::path{filename};
  std::error_code ec;
  if (stdfs::is_directory(filePath, ec))
    stdfs::remove_all(filePath, ec);
  else if (stdfs::is_regular_file(filePath, ec))
    stdfs::remove(filePath, ec);
}

}
