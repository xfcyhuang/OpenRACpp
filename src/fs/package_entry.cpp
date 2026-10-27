#include "fs/package_entry.hpp"

#include "formats/crc32.hpp"

namespace ora::fs {

namespace {

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

}

std::uint32_t PackageEntry::HashFilename(std::string_view str_name, PackageHashType enum_type) {
  std::size_t st_padding = 0;
  auto vec_upper = UpperAsciiPadded(str_name, st_padding);
  const std::size_t st_padded_length = vec_upper.size();

  switch (enum_type) {
    case PackageHashType::Classic: {

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

  throw std::runtime_error{"Unknown hash type"};
}

}
