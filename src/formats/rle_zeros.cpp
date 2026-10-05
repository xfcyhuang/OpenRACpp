// UPSTREAM: OpenRA.Mods.Common/FileFormats/RLEZerosCompression.cs @7d57605 L19-36
// 实现文件:逐句照抄;byte[] → span。
// Implementation file: copied statement by statement; byte[] → span.
#include "formats/rle_zeros.hpp"

#include "formats/fast_byte_reader.hpp"

namespace ora::fmt::rle_zeros {

void DecodeInto(std::span<const std::byte> vec_src, std::span<std::byte> vec_dest, std::size_t st_dest_index) {
  auto reader = FastByteReader{vec_src};

  while (!reader.Done()) {
    const auto uint1_cmd = static_cast<std::uint8_t>(reader.ReadByte());
    if (uint1_cmd == 0) {
      const auto uint1_count = static_cast<std::uint8_t>(reader.ReadByte());
      const auto st_count = static_cast<std::size_t>(uint1_count);
      if (st_dest_index + st_count > vec_dest.size()) [[unlikely]]
        throw std::runtime_error("Index was outside the bounds of the array.");
      std::ranges::fill(vec_dest.subspan(st_dest_index, st_count), std::byte{0});
      st_dest_index += st_count;
    } else {
      if (st_dest_index >= vec_dest.size()) [[unlikely]]
        throw std::runtime_error("Index was outside the bounds of the array.");
      vec_dest[st_dest_index++] = static_cast<std::byte>(uint1_cmd);
    }
  }
}

}  // namespace ora::fmt::rle_zeros
