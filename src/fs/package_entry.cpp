// UPSTREAM: OpenRA.Mods.Cnc/FileSystem/PackageEntry.cs @7d57605 L21-117
#include "fs/package_entry.hpp"

#include "formats/crc32.hpp"

namespace ora::fs {

namespace {

/// 大写化 + ASCII 字节化:.NET ToUpperInvariant + Encoding.ASCII 的等价域
/// (a-z 翻转;≥ 0x80 → '?',与 ASCII 编码的替换一致)。返回按 4 字节对
/// 填充后的字节数组与填充长度。
/// Uppercase + ASCII encode: the equivalent domain of .NET's
/// ToUpperInvariant + Encoding.ASCII (a-z flipped; >= 0x80 -> '?', matching
/// the ASCII encoder's replacement). Returns the padded byte array (aligned
/// to 4) and the padding length.
std::vector<std::uint8_t> UpperAsciiPadded(std::string_view str_name, std::size_t& st_padding) {
  const std::size_t st_len = str_name.size();
  st_padding = st_len % 4 != 0 ? 4 - st_len % 4 : 0;
  const std::size_t st_padded = st_len + st_padding;

  auto vec_out = std::vector<std::uint8_t>(st_padded, 0);
  for (std::size_t st_i = 0; st_i < st_len; st_i++) {
    const auto uint1_c = static_cast<unsigned char>(str_name[st_i]);
    if (uint1_c >= 'a' && uint1_c <= 'z')
      vec_out[st_i] = static_cast<std::uint8_t>(uint1_c - 'a' + 'A');
    else if (uint1_c >= 0x80)
      vec_out[st_i] = '?';
    else
      vec_out[st_i] = uint1_c;
  }
  return vec_out;
}

}  // namespace

std::uint32_t PackageEntry::HashFilename(std::string_view str_name, PackageHashType enum_type) {
  std::size_t st_padding = 0;
  auto vec_upper = UpperAsciiPadded(str_name, st_padding);
  const std::size_t st_padded_length = vec_upper.size();

  switch (enum_type) {
    case PackageHashType::Classic: {
      // L71-85:尾部填 NUL;逐 4 字节(小端 u32)rotate-1 累加。
      // L71-85: NUL padding; rotate-1 accumulate over each 4-byte
      // little-endian word.
      for (std::size_t st_p = 0; st_p < st_padding; st_p++)
        vec_upper[st_padded_length - 1 - st_p] = 0;

      auto uint4_result = 0u;
      for (std::size_t st_i = 0; st_i < st_padded_length; st_i += 4) {
        const std::uint32_t uint4_next = static_cast<std::uint32_t>(vec_upper[st_i]) |
                                         (static_cast<std::uint32_t>(vec_upper[st_i + 1]) << 8) |
                                         (static_cast<std::uint32_t>(vec_upper[st_i + 2]) << 16) |
                                         (static_cast<std::uint32_t>(vec_upper[st_i + 3]) << 24);
        uint4_result = ((uint4_result << 1) | (uint4_result >> 31)) + uint4_next;
      }
      return uint4_result;
    }

    case PackageHashType::CRC32: {
      // L87-101:upperPaddedName[length] = (char)(length - length/4*4),其余
      // 填充位复制该字节;标准 CRC-32。
      // L87-101: upperPaddedName[length] = (char)(length -
      // length/4*4); the remaining padding replicates that byte; the
      // standard CRC-32.
      const std::size_t st_length = str_name.size();
      const std::size_t st_rounded = st_length / 4 * 4;
      if (st_length != st_rounded && st_padding > 0) {
        const auto uint1_pad = static_cast<std::uint8_t>(st_length - st_rounded);
        vec_upper[st_length] = uint1_pad;
        for (std::size_t st_p = 1; st_p < st_padding; st_p++)
          vec_upper[st_length + st_p] = vec_upper[st_rounded];
      }

      const auto span_bytes = std::as_bytes(std::span<const std::uint8_t>{vec_upper});
      return fmt::CRC32::Calculate(span_bytes);
    }
  }

  // 上游 default:NotImplementedException —— enum 两值已穷尽,不可达。
  // Upstream default: NotImplementedException — both enum values are
  // covered, unreachable.
  throw std::runtime_error{"Unknown hash type"};
}

}  // namespace ora::fs
