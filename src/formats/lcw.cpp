// UPSTREAM: OpenRA.Mods.Cnc/FileFormats/LCWCompression.cs @b6fc03f L16-167
// 实现文件:解码五 case / ReplicatePrevious / CountSame / WriteCopyBlocks /
// Encode 逐句照抄;byte[] → span。
// Implementation file: the five decoder cases / ReplicatePrevious /
// CountSame / WriteCopyBlocks / Encode copied statement by statement;
// byte[] → span.
#include "formats/lcw.hpp"

#include "formats/fast_byte_reader.hpp"

namespace ora::fmt::lcw {

namespace {

/// L18-24。srcIndex > destIndex 为坏数据(上游 NotImplementedException,
/// 消息逐字)。距离 1 的行内重复展开是 RLE 压缩的一部分。
/// L18-24. srcIndex > destIndex means malformed data (upstream
/// NotImplementedException, message verbatim). The distance-1 inline
/// repeat expansion is part of the RLE compression.
void ReplicatePrevious(std::span<std::byte> vec_dest, std::int32_t int4_dest_index, std::int32_t int4_src_index,
                       std::int32_t int4_count) {
  if (int4_src_index > int4_dest_index)
    throw std::runtime_error("srcIndex > destIndex " + std::to_string(int4_src_index) + " " +
                             std::to_string(int4_dest_index));

  for (auto int4_i = 0; int4_i < int4_count; int4_i++) {
    if (int4_dest_index - int4_src_index == 1)
      vec_dest[static_cast<std::size_t>(int4_dest_index + int4_i)] = vec_dest[static_cast<std::size_t>(int4_dest_index - 1)];
    else
      vec_dest[static_cast<std::size_t>(int4_dest_index + int4_i)] =
          vec_dest[static_cast<std::size_t>(int4_src_index + int4_i)];
  }
}

/// 越界写检查:上游 case4/3/5 直写数组抛 IndexOutOfRangeException,
/// case2 有显式提前返回;C++ 统一为该守卫(仅坏数据可达)。
/// The out-of-bounds-write guard: upstream's cases 4/3/5 write the array
/// directly and throw IndexOutOfRangeException, while case2 returns early
/// explicitly; C++ funnels both into this guard (malformed data only).
void CheckDestBounds(std::span<std::byte> vec_dest, std::int32_t int4_end) {
  if (int4_end > static_cast<std::int32_t>(vec_dest.size())) [[unlikely]]
    throw std::runtime_error("Index was outside the bounds of the array.");
}

}  // namespace

std::int32_t DecodeInto(std::span<const std::byte> vec_src, std::span<std::byte> vec_dest, std::int32_t int4_src_offset,
                        bool b_reverse) {
  auto ctx = FastByteReader{vec_src, static_cast<std::size_t>(int4_src_offset)};
  auto int4_dest_index = std::int32_t{0};
  while (true) {
    const auto uint1_i = static_cast<std::int32_t>(ctx.ReadByte());
    if ((uint1_i & 0x80) == 0) {
      // case 2
      const auto uint1_second = static_cast<std::int32_t>(ctx.ReadByte());
      const auto int4_count = ((uint1_i & 0x70) >> 4) + 3;
      const auto int4_rpos = ((uint1_i & 0xF) << 8) + uint1_second;

      if (int4_dest_index + int4_count > static_cast<std::int32_t>(vec_dest.size()))
        return int4_dest_index;

      ReplicatePrevious(vec_dest, int4_dest_index, int4_dest_index - int4_rpos, int4_count);
      int4_dest_index += int4_count;
    } else if ((uint1_i & 0x40) == 0) {
      // case 1
      auto int4_count = uint1_i & 0x3F;
      if (int4_count == 0)
        return int4_dest_index;

      CheckDestBounds(vec_dest, int4_dest_index + int4_count);
      ctx.CopyTo(vec_dest, static_cast<std::size_t>(int4_dest_index), int4_count);
      int4_dest_index += int4_count;
    } else {
      const auto int4_count3 = uint1_i & 0x3F;
      if (int4_count3 == 0x3E) {
        // case 4
        const auto int4_count = ctx.ReadWord();
        const auto uint1_color = ctx.ReadByte();

        CheckDestBounds(vec_dest, int4_dest_index + int4_count);
        for (auto int4_end = int4_dest_index + int4_count; int4_dest_index < int4_end; int4_dest_index++)
          vec_dest[static_cast<std::size_t>(int4_dest_index)] = static_cast<std::byte>(uint1_color);
      } else {
        // count3 == 0x3F 为 case 5,否则 case 3
        // count3 == 0x3F is case 5, else case 3
        const auto int4_count = int4_count3 == 0x3F ? ctx.ReadWord() : int4_count3 + 3;
        auto int4_src_index = b_reverse ? int4_dest_index - ctx.ReadWord() : ctx.ReadWord();
        if (int4_src_index >= int4_dest_index)
          throw std::runtime_error("srcIndex >= destIndex " + std::to_string(int4_src_index) + " " +
                                   std::to_string(int4_dest_index));

        CheckDestBounds(vec_dest, int4_dest_index + int4_count);
        for (auto int4_end = int4_dest_index + int4_count; int4_dest_index < int4_end; int4_dest_index++)
          vec_dest[static_cast<std::size_t>(int4_dest_index)] =
              vec_dest[static_cast<std::size_t>(int4_src_index++)];
      }
    }
  }
}

namespace {

/// L68-81。
std::int32_t CountSame(std::span<const std::byte> vec_src, std::int32_t int4_offset, std::int32_t int4_max_count) {
  int4_max_count = std::min(static_cast<std::int32_t>(vec_src.size()) - int4_offset, int4_max_count);
  if (int4_max_count <= 0)
    return 0;

  auto st_offset = static_cast<std::size_t>(int4_offset);
  const auto uint1_first = vec_src[st_offset++];
  auto int4_count = std::int32_t{1};

  while (int4_count < int4_max_count && vec_src[st_offset++] == uint1_first)
    int4_count++;

  return int4_count;
}

/// L83-93。
void WriteCopyBlocks(std::span<const std::byte> vec_src, std::int32_t int4_offset, std::int32_t int4_count,
                     std::vector<std::byte>& vec_output) {
  while (int4_count > 0) {
    const auto int4_write_now = std::min(int4_count, 0x3F);
    vec_output.push_back(static_cast<std::byte>(0x80 | int4_write_now));
    const auto span_chunk = vec_src.subspan(static_cast<std::size_t>(int4_offset), static_cast<std::size_t>(int4_write_now));
    vec_output.insert(vec_output.end(), span_chunk.begin(), span_chunk.end());

    int4_count -= int4_write_now;
    int4_offset += int4_write_now;
  }
}

}  // namespace

std::vector<std::byte> Encode(std::span<const std::byte> vec_src) {
  auto vec_ms = std::vector<std::byte>{};
  auto int4_offset = std::int32_t{0};
  const auto int4_left = static_cast<std::int32_t>(vec_src.size());
  auto int4_block_start = std::int32_t{0};

  while (int4_offset < int4_left) {
    const auto int4_repeat_count = CountSame(vec_src, int4_offset, 0xFFFF);
    if (int4_repeat_count >= 4) {
      // 先写此前未写出的部分 | first write what hasn't been written yet
      WriteCopyBlocks(vec_src, int4_block_start, int4_offset - int4_block_start, vec_ms);

      // Command 4: 单字节重复 n 次 | Command 4: repeat byte n times
      vec_ms.push_back(static_cast<std::byte>(0xFE));

      // Low byte
      vec_ms.push_back(static_cast<std::byte>(int4_repeat_count & 0xFF));

      // High byte
      vec_ms.push_back(static_cast<std::byte>(int4_repeat_count >> 8));

      // 被重复的值 | the value to repeat
      vec_ms.push_back(vec_src[static_cast<std::size_t>(int4_offset)]);

      int4_offset += int4_repeat_count;
      int4_block_start = int4_offset;
    } else
      int4_offset++;
  }

  // 写出剩余部分 | write whatever remains
  WriteCopyBlocks(vec_src, int4_block_start, int4_offset - int4_block_start, vec_ms);

  // 终止符 | the terminator
  vec_ms.push_back(static_cast<std::byte>(0x80));

  return vec_ms;
}

}  // namespace ora::fmt::lcw
