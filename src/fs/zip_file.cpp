#include "fs/zip_file.hpp"

#include <miniz.h>

#include "fs/file_system.hpp"

namespace ora::fs {

namespace {

class ZipFolder final : public IReadOnlyPackage {
 public:
  ZipFolder(const ReadOnlyZipFile* parent, std::string path) : parent_{parent} {
    if (!path.empty() && path.back() == '/')
      path.pop_back();
    str_name_ = std::move(path);
  }

  const std::string& Name() const override { return str_name_; }

  std::optional<std::vector<char>> GetStream(const std::string& filename) const override {

    return parent_->GetStream(str_name_ + "/" + filename);
  }

  std::vector<std::string> Contents() const override {
    std::vector<std::string> result;
    for (const std::string& entry : parent_->Contents()) {
      if (entry.rfind(str_name_, 0) == 0 && entry != str_name_) {
        const std::string_view filename =
            std::string_view{entry}.substr(str_name_.size() + 1);

        std::size_t dirLevels = 0;
        for (const auto part : std::views::split(filename, '/'))
          if (!part.empty())
            dirLevels++;
        if (dirLevels == 1)
          result.push_back(std::string{filename});
      }
    }
    return result;
  }

  bool Contains(const std::string& filename) const override {
    return parent_->Contains(str_name_ + "/" + filename);
  }

  std::unique_ptr<IReadOnlyPackage> OpenPackage(const std::string& filename,
                                                FileSystem& context) const override {
    return parent_->OpenPackage(str_name_ + "/" + filename, context);
  }

 private:
  const ReadOnlyZipFile* parent_;
  std::string str_name_;
};

}

void DestroyZipReader(void* ptr_reader) {
  auto* reader = static_cast<mz_zip_archive*>(ptr_reader);
  mz_zip_reader_end(reader);
  delete reader;
}

ReadOnlyZipFile::ReadOnlyZipFile(std::vector<char> bytes, std::string filename)
    : vec_bytes_{std::move(bytes)}, str_name_{std::move(filename)} {
  auto* reader = new mz_zip_archive{};
  if (!mz_zip_reader_init_mem(reader, vec_bytes_.data(), vec_bytes_.size(), 0)) {
    delete reader;
    throw std::runtime_error{"zip reader init failed: " + str_name_};
  }
  up_reader_ = {reader, &DestroyZipReader};
}

std::vector<std::string> ReadOnlyZipFile::Contents() const {

  auto* reader = static_cast<mz_zip_archive*>(up_reader_.get());
  std::vector<std::string> names;
  const mz_uint count = mz_zip_reader_get_num_files(reader);
  std::string buf;
  for (mz_uint i = 0; i < count; i++) {
    if (mz_zip_reader_is_file_a_directory(reader, i))
      continue;
    const mz_uint len = mz_zip_reader_get_filename(reader, i, nullptr, 0);
    buf.resize(len);
    mz_zip_reader_get_filename(reader, i, buf.data(), len);
    names.emplace_back(buf.data());
  }
  return names;
}

std::optional<std::vector<char>> ReadOnlyZipFile::GetStream(
    const std::string& filename) const {
  auto* reader = static_cast<mz_zip_archive*>(up_reader_.get());
  const mz_int64 index = mz_zip_reader_locate_file(reader, filename.c_str(), nullptr,
                                                   MZ_ZIP_FLAG_CASE_SENSITIVE);
  if (index < 0)
    return std::nullopt;
  std::size_t size = 0;
  void* data = mz_zip_reader_extract_to_heap(reader, static_cast<mz_uint>(index), &size, 0);
  if (data == nullptr)
    return std::nullopt;
  std::vector<char> out{static_cast<const char*>(data), static_cast<const char*>(data) + size};
  mz_free(data);
  return out;
}

bool ReadOnlyZipFile::Contains(const std::string& filename) const {
  auto* reader = static_cast<mz_zip_archive*>(up_reader_.get());
  return mz_zip_reader_locate_file(reader, filename.c_str(), nullptr,
                                   MZ_ZIP_FLAG_CASE_SENSITIVE) >= 0;
}

std::unique_ptr<IReadOnlyPackage> ReadOnlyZipFile::OpenPackage(const std::string& filename,
                                                               FileSystem& context) const {

  auto* reader = static_cast<mz_zip_archive*>(up_reader_.get());
  mz_int64 index = mz_zip_reader_locate_file(reader, filename.c_str(), nullptr,
                                             MZ_ZIP_FLAG_CASE_SENSITIVE);
  if (index < 0) {
    index = mz_zip_reader_locate_file(reader, (filename + "/").c_str(), nullptr,
                                      MZ_ZIP_FLAG_CASE_SENSITIVE);
  }
  if (index < 0)
    return nullptr;

  if (mz_zip_reader_is_file_a_directory(reader, static_cast<mz_uint>(index)))
    return std::make_unique<ZipFolder>(this, filename);

  const std::optional<std::vector<char>> bytes = GetStream(filename);
  if (!bytes.has_value())
    return nullptr;

  std::unique_ptr<IReadOnlyPackage> package;
  if (context.TryParsePackage(*bytes, filename, package))
    return package;
  return nullptr;
}

bool ZipFileLoader::TryParsePackage(std::span<const char> bytes, const std::string& filename,
                                    [[maybe_unused]] FileSystem& context,
                                    std::unique_ptr<IReadOnlyPackage>& package) {

  constexpr std::uint32_t kZipSignature = 0x04034b50;
  if (bytes.size() < 4) {
    package.reset();
    return false;
  }
  std::uint32_t readSignature = 0;
  std::memcpy(&readSignature, bytes.data(), 4);
  if (readSignature != kZipSignature) {
    package.reset();
    return false;
  }

  package = std::make_unique<ReadOnlyZipFile>(std::vector<char>{bytes.begin(), bytes.end()},
                                              filename);
  return true;
}

}
