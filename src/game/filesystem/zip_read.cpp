#include "game/filesystem/inflate.hpp"
#include "game/filesystem/zip_read.hpp"

namespace ora::game::filesystem {

namespace {

constexpr std::uint32_t kLocalHeaderSig{0x04034B50u};
constexpr std::uint32_t kCentralDirSig{0x02014B50u};
constexpr std::uint32_t kEocdSig{0x06054B50u};

[[nodiscard]] std::uint16_t u16_le(std::span<const std::uint8_t> span1u_data, std::size_t sz_off) {
  return static_cast<std::uint16_t>(span1u_data[sz_off] | (span1u_data[sz_off + 1] << 8));
}

[[nodiscard]] std::uint32_t u32_le(std::span<const std::uint8_t> span1u_data, std::size_t sz_off) {
  return static_cast<std::uint32_t>(span1u_data[sz_off]) | (static_cast<std::uint32_t>(span1u_data[sz_off + 1]) << 8)
      | (static_cast<std::uint32_t>(span1u_data[sz_off + 2]) << 16)
      | (static_cast<std::uint32_t>(span1u_data[sz_off + 3]) << 24);
}

[[nodiscard]] std::expected<std::size_t, std::string> find_eocd(std::span<const std::uint8_t> span1u_archive) {
  if (span1u_archive.size() < 22) return std::unexpected(std::string{"zip 尺寸不足 EOCD"});
  const std::size_t sz_scan_floor{span1u_archive.size() >= 22 + 65535u ? span1u_archive.size() - 22u - 65535u : 0u};
  for (std::size_t sz_pos{span1u_archive.size() - 22u};; --sz_pos) {
    if (u32_le(span1u_archive, sz_pos) == kEocdSig) return sz_pos;
    if (sz_pos == sz_scan_floor) break;
  }
  return std::unexpected(std::string{"zip 未找到 EOCD 签名"});
}

}

std::expected<ZipArchive, std::string> zip_read(std::span<const std::uint8_t> span1u_archive) {
  const auto res_eocd{find_eocd(span1u_archive)};
  if (!res_eocd) return std::unexpected(res_eocd.error());
  const std::size_t sz_eocd{*res_eocd};

  const std::uint16_t u2_entry_count{u16_le(span1u_archive, sz_eocd + 10)};
  const std::uint32_t u4_cd_offset{u32_le(span1u_archive, sz_eocd + 16)};
  if (u4_cd_offset >= span1u_archive.size() || u4_cd_offset >= sz_eocd)
    return std::unexpected(std::string{"zip 中央目录偏移越界"});

  ZipArchive archive_out;
  archive_out.vec1u_bytes.assign(span1u_archive.begin(), span1u_archive.end());
  archive_out.vec_entries.reserve(u2_entry_count);

  std::size_t sz_pos{u4_cd_offset};
  for (std::uint16_t u2_i{0}; u2_i < u2_entry_count; ++u2_i) {
    if (sz_pos + 46 > span1u_archive.size()) return std::unexpected(std::string{"zip 中央目录截断"});
    if (u32_le(span1u_archive, sz_pos) != kCentralDirSig)
      return std::unexpected(std::string{"zip 中央目录签名不符"});

    const std::uint16_t u2_flags{u16_le(span1u_archive, sz_pos + 8)};
    if ((u2_flags & 0x0008u) != 0u) return std::unexpected(std::string{"zip 数据描述符（flag bit3）不支持"});

    ZipEntry entry_next{};
    entry_next.u2_method = u16_le(span1u_archive, sz_pos + 10);
    entry_next.u4_crc = u32_le(span1u_archive, sz_pos + 16);
    entry_next.u4_csize = u32_le(span1u_archive, sz_pos + 20);
    entry_next.u4_usize = u32_le(span1u_archive, sz_pos + 24);
    entry_next.u4_local_offset = u32_le(span1u_archive, sz_pos + 42);

    if (entry_next.u4_csize == 0xFFFFFFFFu || entry_next.u4_usize == 0xFFFFFFFFu
        || entry_next.u4_local_offset == 0xFFFFFFFFu)
      return std::unexpected(std::string{"zip64 扩展不支持"});

    const std::uint16_t u2_name_len{u16_le(span1u_archive, sz_pos + 28)};
    const std::uint16_t u2_extra_len{u16_le(span1u_archive, sz_pos + 30)};
    const std::uint16_t u2_comment_len{u16_le(span1u_archive, sz_pos + 32)};
    if (sz_pos + 46 + static_cast<std::size_t>(u2_name_len) + u2_extra_len + u2_comment_len
        > span1u_archive.size())
      return std::unexpected(std::string{"zip 中央目录条目截断"});

    entry_next.str_name.assign(reinterpret_cast<const char*>(span1u_archive.data() + sz_pos + 46),
        u2_name_len);
    sz_pos += 46u + u2_name_len + u2_extra_len + u2_comment_len;

    if (!entry_next.str_name.empty() && entry_next.str_name.back() == '/') continue;
    archive_out.vec_entries.push_back(std::move(entry_next));
  }

  return archive_out;
}

std::expected<std::vector<std::uint8_t>, std::string> zip_entry_data(
    const ZipArchive& archive_zip, std::string_view str_name) {
  const auto it_hit{std::find_if(archive_zip.vec_entries.begin(), archive_zip.vec_entries.end(),
      [&](const ZipEntry& entry) { return entry.str_name == str_name; })};
  if (it_hit == archive_zip.vec_entries.end())
    return std::unexpected(std::format("zip 条目不存在：{}", str_name));

  const std::span<const std::uint8_t> span1u_archive{archive_zip.vec1u_bytes};
  const std::size_t sz_local{it_hit->u4_local_offset};
  if (sz_local + 30 > span1u_archive.size()) return std::unexpected(std::string{"zip local header 截断"});
  if (u32_le(span1u_archive, sz_local) != kLocalHeaderSig)
    return std::unexpected(std::string{"zip local header 签名不符"});

  const std::uint16_t u2_name_len{u16_le(span1u_archive, sz_local + 26)};
  const std::uint16_t u2_extra_len{u16_le(span1u_archive, sz_local + 28)};
  const std::size_t sz_data{sz_local + 30u + u2_name_len + u2_extra_len};
  if (sz_data + it_hit->u4_csize > span1u_archive.size())
    return std::unexpected(std::string{"zip 条目数据截断"});

  const std::span<const std::uint8_t> span1u_comp{span1u_archive.data() + sz_data, it_hit->u4_csize};
  if (it_hit->u2_method == 0u) {
    return std::vector<std::uint8_t>{span1u_comp.begin(), span1u_comp.end()};
  }
  if (it_hit->u2_method == 8u) {
    const auto res_inflated{inflate_raw(span1u_comp)};
    if (!res_inflated) return std::unexpected(res_inflated.error());
    if (res_inflated->size() != it_hit->u4_usize)
      return std::unexpected(std::format("zip 条目解压尺寸不符：{} != {}", res_inflated->size(), it_hit->u4_usize));
    return res_inflated;
  }
  return std::unexpected(std::format("zip 压缩方法 {} 不支持", it_hit->u2_method));
}

}
