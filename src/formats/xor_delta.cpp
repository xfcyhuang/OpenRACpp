// UPSTREAM: OpenRA.Mods.Cnc/FileFormats/XORDeltaCompression.cs @7d57605 L16-82
// 实现文件:六 case 逐句照抄;byte[] → span。
// Implementation file: the six cases copied statement by statement;
// byte[] → span.
#include "formats/xor_delta.hpp"

#include "formats/fast_byte_reader.hpp"

namespace ora::fmt::xor_delta {

namespace {

/// 越界写守卫(上游直写数组抛 IndexOutOfRangeException;坏数据专用)。
/// The out-of-bounds-write guard (upstream writes the array directly and
/// throws IndexOutOfRangeException; malformed data only).
void CheckDestBounds(std::span<std::byte> vec_dest, std::int32_t int4_end) {
  if (int4_end > static_cast<std::int32_t>(vec_dest.size())) [[unlikely]]
    throw std::runtime_error("Index was outside the bounds of the array.");
}

}  // namespace

std::int32_t DecodeInto(std::span<const std::byte> vec_src, std::span<std::byte> vec_dest, std::int32_t int4_src_offset) {
  auto ctx = FastByteReader{vec_src, static_cast<std::size_t>(int4_src_offset)};
  auto int4_dest_index = std::int32_t{0};

  while (true) {
    const auto uint1_i = static_cast<std::int32_t>(ctx.ReadByte());
    if ((uint1_i & 0x80) == 0) {
      auto int4_count = uint1_i & 0x7F;
      if (int4_count == 0) {
        // case 6
        int4_count = static_cast<std::int32_t>(ctx.ReadByte());
        const auto uint1_value = ctx.ReadByte();
        CheckDestBounds(vec_dest, int4_dest_index + int4_count);
        for (auto int4_end = int4_dest_index + int4_count; int4_dest_index < int4_end; int4_dest_index++)
          vec_dest[static_cast<std::size_t>(int4_dest_index)] ^= static_cast<std::byte>(uint1_value);
      } else {
        // case 5
        CheckDestBounds(vec_dest, int4_dest_index + int4_count);
        for (auto int4_end = int4_dest_index + int4_count; int4_dest_index < int4_end; int4_dest_index++)
          vec_dest[static_cast<std::size_t>(int4_dest_index)] ^= ctx.ReadByte();
      }
    } else {
      auto int4_count = uint1_i & 0x7F;
      if (int4_count == 0) {
        int4_count = ctx.ReadWord();
        if (int4_count == 0)
          return int4_dest_index;

        if ((int4_count & 0x8000) == 0) {
          // case 2
          int4_dest_index += int4_count & 0x7FFF;
        } else if ((int4_count & 0x4000) == 0) {
          // case 3
          CheckDestBounds(vec_dest, int4_dest_index + (int4_count & 0x3FFF));
          for (auto int4_end = int4_dest_index + (int4_count & 0x3FFF); int4_dest_index < int4_end; int4_dest_index++)
            vec_dest[static_cast<std::size_t>(int4_dest_index)] ^= ctx.ReadByte();
        } else {
          // case 4
          const auto uint1_value = ctx.ReadByte();
          CheckDestBounds(vec_dest, int4_dest_index + (int4_count & 0x3FFF));
          for (auto int4_end = int4_dest_index + (int4_count & 0x3FFF); int4_dest_index < int4_end; int4_dest_index++)
            vec_dest[static_cast<std::size_t>(int4_dest_index)] ^= static_cast<std::byte>(uint1_value);
        }
      } else {
        // case 1
        int4_dest_index += int4_count;
      }
    }
  }
}

}  // namespace ora::fmt::xor_delta
