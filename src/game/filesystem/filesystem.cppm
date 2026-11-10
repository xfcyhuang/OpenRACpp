export module ora.game.filesystem;

import std;

export {
namespace ora::game::filesystem {

[[nodiscard]] std::expected<std::vector<std::uint8_t>, std::string> inflate_raw(
    std::span<const std::uint8_t> span1u_input);

}

namespace ora::game::filesystem {

struct ZipEntry {
  std::string str_name;
  std::uint16_t u2_method{};
  std::uint32_t u4_crc{};
  std::uint32_t u4_csize{};
  std::uint32_t u4_usize{};
  std::uint32_t u4_local_offset{};
};

struct ZipArchive {
  std::vector<ZipEntry> vec_entries;
  std::vector<std::uint8_t> vec1u_bytes;
};

[[nodiscard]] std::expected<ZipArchive, std::string> zip_read(std::span<const std::uint8_t> span1u_archive);

[[nodiscard]] std::expected<std::vector<std::uint8_t>, std::string> zip_entry_data(
    const ZipArchive& archive_zip, std::string_view str_name);

}
}
