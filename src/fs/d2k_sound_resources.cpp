// UPSTREAM: OpenRA.Mods.D2k/PackageLoaders/D2kSoundResources.cs @b6fc03f
//          L20-94(全文逐语义)
#include "fs/d2k_sound_resources.hpp"

#include "formats/span_reader.hpp"

namespace ora::fs {

D2kSoundResources::D2kSoundResources(std::vector<char> vec_bytes, std::string str_filename)
    : vec_bytes_{std::move(vec_bytes)}, str_name_{std::move(str_filename)} {
  fmt::SpanReader reader{std::as_bytes(std::span<const char>{vec_bytes_})};

  const auto uint4_header_length = reader.ReadUInt32();
  while (reader.Position() < static_cast<std::int64_t>(uint4_header_length) + 4) {
    // ReadASCIIZ + 偏移/长度对(L42-45);Dictionary.Add 的重键抛 = 上游
    // ArgumentException 等价(仅坏数据可达)。
    // ReadASCIIZ + the offset/length pair (L42-45); Dictionary.Add's
    // duplicate-key throw = the upstream ArgumentException equivalent
    // (reachable only on malformed data).
    auto str_name = std::string{};
    while (const auto uint1_c = reader.ReadUInt8())
      str_name.push_back(static_cast<char>(uint1_c));
    const auto uint4_offset = reader.ReadUInt32();
    const auto uint4_length = reader.ReadUInt32();
    if (!map_index_.emplace(str_name, Entry{uint4_offset, uint4_length}).second)
      throw std::runtime_error{"An item with the same key has already been added. Key: " + str_name};
  }
}

std::vector<std::string> D2kSoundResources::Contents() const {
  auto vec_names = std::vector<std::string>{};
  vec_names.reserve(map_index_.size());
  for (const auto& [str_name, entry] : map_index_)
    vec_names.push_back(str_name);
  return vec_names;
}

std::optional<std::vector<char>> D2kSoundResources::GetStream(
    const std::string& str_filename) const {
  const auto it = map_index_.find(str_filename);
  if (it == map_index_.end())
    return std::nullopt;

  // SegmentStream.CreateWithoutOwningStream(s, offset, length):绝对偏移。
  // SegmentStream.CreateWithoutOwningStream(s, offset, length): the
  // absolute offset.
  const auto& entry = it->second;
  if (static_cast<std::int64_t>(entry.uint4_offset) + entry.uint4_length >
      static_cast<std::int64_t>(vec_bytes_.size()))
    throw std::runtime_error{"Invalid .rs entry range"};
  return std::vector<char>{vec_bytes_.data() + entry.uint4_offset,
                           vec_bytes_.data() + entry.uint4_offset + entry.uint4_length};
}

bool D2kSoundResources::Contains(const std::string& str_filename) const {
  return map_index_.contains(str_filename);
}

std::unique_ptr<IReadOnlyPackage> D2kSoundResources::OpenPackage(
    [[maybe_unused]] const std::string& str_filename, [[maybe_unused]] FileSystem& context) const {
  // Not implemented(上游 L65-69 同)
  // Not implemented (as upstream L65-69).
  return nullptr;
}

bool D2kSoundResourcesLoader::TryParsePackage(std::span<const char> vec_bytes,
                                              const std::string& str_filename,
                                              [[maybe_unused]] FileSystem& context,
                                              std::unique_ptr<IReadOnlyPackage>& package) {
  if (!std::ranges::ends_with(
          std::string_view{str_filename}, std::string_view{".rs"},
          [](char chr_a, char chr_b) { return std::tolower(static_cast<unsigned char>(chr_a)) == chr_b; })) {
    package.reset();
    return false;
  }

  package = std::make_unique<D2kSoundResources>(std::vector<char>{vec_bytes.begin(), vec_bytes.end()},
                                                str_filename);
  return true;
}

}  // namespace ora::fs
