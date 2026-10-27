#include "fs/mix_file.hpp"

#include "formats/blowfish.hpp"
#include "formats/blowfish_key_provider.hpp"
#include "formats/span_reader.hpp"
#include "formats/xcc_database.hpp"
#include "fs/file_system.hpp"

namespace ora::fs {

namespace {

using ByteSpan = std::span<const std::byte>;

ByteSpan AsBytes(std::span<const char> vec) {
  return std::as_bytes(vec);
}

std::vector<PackageEntry> ParseHeader(fmt::SpanReader& reader, std::int64_t int8_offset,
                                      std::int64_t& int8_header_end) {
  reader.Seek(int8_offset);
  const auto uint2_num_files = reader.ReadUInt16();
  reader.ReadUInt32();

  auto vec_items = std::vector<PackageEntry>{};
  vec_items.reserve(uint2_num_files);
  for (auto int4_i = 0; int4_i < uint2_num_files; int4_i++)
    vec_items.emplace_back(reader);

  int8_header_end = int8_offset + 6 + static_cast<std::int64_t>(uint2_num_files) * PackageEntry::int4_size;
  return vec_items;
}

std::vector<std::uint32_t> ReadBlocks(fmt::SpanReader& reader, std::int64_t int8_offset, int int4_count) {
  if (int8_offset < 0)
    throw std::runtime_error("Non-negative number required. (Parameter 'offset')");

  if (int4_count < 0)
    throw std::runtime_error("Non-negative number required. (Parameter 'count')");

  if (int8_offset + int4_count * 2 > reader.Length())
    throw std::runtime_error(std::format("Bytes to read {} and offset {} greater than stream length {}.",
                                         static_cast<long long>(int4_count) * 2,
                                         static_cast<long long>(int8_offset), reader.Length()));

  reader.Seek(int8_offset);

  auto vec_ret = std::vector<std::uint32_t>(2 * static_cast<std::size_t>(int4_count));
  for (auto& uint4_v : vec_ret)
    uint4_v = reader.ReadUInt32();

  return vec_ret;
}

std::vector<std::byte> Decrypt(const std::vector<std::uint32_t>& vec_blocks,
                               const fmt::Blowfish& fish_blowfish) {
  const auto vec_decrypted = fish_blowfish.Decrypt(vec_blocks);

  auto vec_out = std::vector<std::byte>(vec_decrypted.size() * 4);
  for (std::size_t st_i = 0; st_i < vec_decrypted.size(); st_i++)
    for (auto int4_b = 0; int4_b < 4; int4_b++)
      vec_out[st_i * 4 + static_cast<std::size_t>(int4_b)] =
          static_cast<std::byte>(vec_decrypted[st_i] >> (8 * int4_b));

  return vec_out;
}

std::vector<std::byte> DecryptHeader(fmt::SpanReader& reader, std::int64_t int8_offset,
                                     std::int64_t& int8_header_end) {
  reader.Seek(int8_offset);

  const ByteSpan vec_keyblock = reader.ReadBytes(80);
  auto arr_key_src = std::array<std::byte, 80>{};
  std::ranges::copy(vec_keyblock, arr_key_src.begin());
  fmt::BlowfishKeyProvider provider{};
  const auto arr_blowfish_key = provider.DecryptKey(arr_key_src);
  const fmt::Blowfish fish_blowfish{
      std::span<const std::byte>{reinterpret_cast<const std::byte*>(arr_blowfish_key.data()),
                                 arr_blowfish_key.size()}};

  auto vec_first = Decrypt(ReadBlocks(reader, int8_offset + 80, 1), fish_blowfish);
  fmt::SpanReader reader_first{vec_first};
  const auto uint2_num_files = reader_first.ReadUInt16();

  const auto int4_block_count =
      static_cast<int>((13 + static_cast<long long>(uint2_num_files) * PackageEntry::int4_size) / 8);
  int8_header_end = int8_offset + 80 + static_cast<std::int64_t>(int4_block_count) * 8;

  return Decrypt(ReadBlocks(reader, int8_offset + 80, int4_block_count), fish_blowfish);
}

}

MixFile::MixFile(std::vector<char> vec_bytes, std::string str_filename,
                 std::span<const std::string> vec_global_filenames)
    : vec_bytes_{std::move(vec_bytes)}, str_name_{std::move(str_filename)} {
  fmt::SpanReader reader{AsBytes(vec_bytes_)};

  b_isCncMix_ = reader.ReadUInt16() != 0;

  b_isEncrypted_ = false;
  if (!b_isCncMix_)
    b_isEncrypted_ = (reader.ReadUInt16() & 0x2) != 0;

  std::vector<PackageEntry> vec_entries;
  if (b_isEncrypted_) {
    const auto vec_header = DecryptHeader(reader, 4, int8_dataStart_);
    fmt::SpanReader reader_header{vec_header};
    std::int64_t int8_unused = 0;
    vec_entries = ParseHeader(reader_header, 0, int8_unused);
  } else {
    vec_entries = ParseHeader(reader, b_isCncMix_ ? 0 : 4, int8_dataStart_);
  }

  auto map_by_hash = std::unordered_map<std::uint32_t, PackageEntry>{};
  map_by_hash.reserve(vec_entries.size() * 2);
  for (const PackageEntry& entry : vec_entries)
    map_by_hash.emplace(entry.uint4_hash, entry);

  map_index_ = ParseIndex(map_by_hash, vec_global_filenames);
}

std::unordered_map<std::string, PackageEntry> MixFile::ParseIndex(
    const std::unordered_map<std::uint32_t, PackageEntry>& map_entries,
    std::span<const std::string> vec_global_filenames) {

  auto set_names = std::vector<std::string>{};
  auto name_index = std::unordered_map<std::string, std::size_t>{};
  const auto add_name = [&](const std::string& str_name) {
    if (name_index.emplace(str_name, set_names.size()).second)
      set_names.push_back(str_name);
  };
  for (const std::string& str_name : vec_global_filenames)
    add_name(str_name);

  const std::uint32_t uint4_db_name_classic =
      PackageEntry::HashFilename("local mix database.dat", PackageHashType::Classic);
  const std::uint32_t uint4_db_name_crc =
      PackageEntry::HashFilename("local mix database.dat", PackageHashType::CRC32);
  for (const auto& [uint4_key, entry] : map_entries) {
    if (uint4_key == uint4_db_name_classic || uint4_key == uint4_db_name_crc) {
      const fmt::XccLocalDatabase database{AsBytes(ContentOf(entry))};
      for (const std::string& str_e : database.Entries())
        add_name(str_e);
      break;
    }
  }

  auto map_classic = std::unordered_map<std::string, PackageEntry>{};
  auto map_crc = std::unordered_map<std::string, PackageEntry>{};

  for (const std::string& str_filename : set_names) {
    const std::uint32_t uint4_classic =
        PackageEntry::HashFilename(str_filename, PackageHashType::Classic);
    const std::uint32_t uint4_crc = PackageEntry::HashFilename(str_filename, PackageHashType::CRC32);

    if (const auto it = map_entries.find(uint4_classic); it != map_entries.end())
      map_classic.emplace(str_filename, it->second);

    if (const auto it = map_entries.find(uint4_crc); it != map_entries.end())
      map_crc.emplace(str_filename, it->second);
  }

  const auto& map_best =
      map_crc.size() > map_classic.size() ? map_crc : map_classic;

  (void)(map_entries.size() - map_best.size());

  return map_best;
}

std::span<const char> MixFile::ContentOf(const PackageEntry& entry) const {

  const std::int64_t int8_start = int8_dataStart_ + entry.uint4_offset;
  if (int8_start < 0 || int8_start + static_cast<std::int64_t>(entry.uint4_length) >
                            static_cast<std::int64_t>(vec_bytes_.size()))
    throw std::runtime_error{"Invalid mix entry range"};
  return {vec_bytes_.data() + int8_start, entry.uint4_length};
}

std::vector<std::string> MixFile::Contents() const {
  auto vec_names = std::vector<std::string>{};
  vec_names.reserve(map_index_.size());
  for (const auto& [str_name, entry] : map_index_)
    vec_names.push_back(str_name);
  return vec_names;
}

std::optional<std::vector<char>> MixFile::GetStream(const std::string& str_filename) const {
  const auto it = map_index_.find(str_filename);
  if (it == map_index_.end())
    return std::nullopt;

  const auto span_content = ContentOf(it->second);
  return std::vector<char>{span_content.begin(), span_content.end()};
}

bool MixFile::Contains(const std::string& str_filename) const {
  return map_index_.contains(str_filename);
}

std::vector<std::pair<std::string, PackageEntry>> MixFile::AbsoluteIndex() const {
  auto vec_index = std::vector<std::pair<std::string, PackageEntry>>{};
  vec_index.reserve(map_index_.size());
  for (const auto& [str_name, entry] : map_index_)
    vec_index.emplace_back(str_name,
                           PackageEntry{entry.uint4_hash,
                                        static_cast<std::uint32_t>(entry.uint4_offset + int8_dataStart_),
                                        entry.uint4_length});
  return vec_index;
}

std::unique_ptr<IReadOnlyPackage> MixFile::OpenPackage(const std::string& str_filename,
                                                       FileSystem& context) const {

  const std::optional<std::vector<char>> vec_child = GetStream(str_filename);
  if (!vec_child.has_value())
    return nullptr;

  std::unique_ptr<IReadOnlyPackage> package;
  if (context.TryParsePackage(*vec_child, str_filename, package))
    return package;
  return nullptr;
}

bool MixLoader::TryParsePackage(std::span<const char> vec_bytes, const std::string& str_filename,
                                FileSystem& context,
                                std::unique_ptr<IReadOnlyPackage>& package) {
  if (!std::ranges::ends_with(
          std::string_view{str_filename}, std::string_view{".mix"},
          [](char chr_a, char chr_b) { return std::tolower(static_cast<unsigned char>(chr_a)) == chr_b; })) {
    package.reset();
    return false;
  }

  if (ptr_globalFilenames_ == nullptr) {
    auto vec_database = std::vector<char>{};
    if (context.TryOpen("global mix database.dat", vec_database)) {

      auto vec_entries = std::vector<std::string>{};
      auto set_seen = std::unordered_set<std::string>{};
      for (const std::string& str_e :
           fmt::XccGlobalDatabase{AsBytes(vec_database)}.Entries())
        if (set_seen.insert(str_e).second)
          vec_entries.push_back(str_e);
      ptr_globalFilenames_ = std::make_unique<std::vector<std::string>>(std::move(vec_entries));
    } else {
      ptr_globalFilenames_ = std::make_unique<std::vector<std::string>>();
    }
  }

  package = std::make_unique<MixFile>(std::vector<char>{vec_bytes.begin(), vec_bytes.end()},
                                      str_filename, *ptr_globalFilenames_);
  return true;
}

}
