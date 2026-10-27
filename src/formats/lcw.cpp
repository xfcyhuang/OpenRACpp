#include "formats/lcw.hpp"

#include "formats/fast_byte_reader.hpp"

namespace ora::fmt::lcw {

namespace {

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

void CheckDestBounds(std::span<std::byte> vec_dest, std::int32_t int4_end) {
  if (int4_end > static_cast<std::int32_t>(vec_dest.size())) [[unlikely]]
    throw std::runtime_error("Index was outside the bounds of the array.");
}

}

std::int32_t DecodeInto(std::span<const std::byte> vec_src, std::span<std::byte> vec_dest, std::int32_t int4_src_offset,
                        bool b_reverse) {
  auto ctx = FastByteReader{vec_src, static_cast<std::size_t>(int4_src_offset)};
  auto int4_dest_index = std::int32_t{0};
  while (true) {
    const auto uint1_i = static_cast<std::int32_t>(ctx.ReadByte());
    if ((uint1_i & 0x80) == 0) {

      const auto uint1_second = static_cast<std::int32_t>(ctx.ReadByte());
      const auto int4_count = ((uint1_i & 0x70) >> 4) + 3;
      const auto int4_rpos = ((uint1_i & 0xF) << 8) + uint1_second;

      if (int4_dest_index + int4_count > static_cast<std::int32_t>(vec_dest.size()))
        return int4_dest_index;

      ReplicatePrevious(vec_dest, int4_dest_index, int4_dest_index - int4_rpos, int4_count);
      int4_dest_index += int4_count;
    } else if ((uint1_i & 0x40) == 0) {

      auto int4_count = uint1_i & 0x3F;
      if (int4_count == 0)
        return int4_dest_index;

      CheckDestBounds(vec_dest, int4_dest_index + int4_count);
      ctx.CopyTo(vec_dest, static_cast<std::size_t>(int4_dest_index), int4_count);
      int4_dest_index += int4_count;
    } else {
      const auto int4_count3 = uint1_i & 0x3F;
      if (int4_count3 == 0x3E) {

        const auto int4_count = ctx.ReadWord();
        const auto uint1_color = ctx.ReadByte();

        CheckDestBounds(vec_dest, int4_dest_index + int4_count);
        for (auto int4_end = int4_dest_index + int4_count; int4_dest_index < int4_end; int4_dest_index++)
          vec_dest[static_cast<std::size_t>(int4_dest_index)] = static_cast<std::byte>(uint1_color);
      } else {

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

}

std::vector<std::byte> Encode(std::span<const std::byte> vec_src) {
  auto vec_ms = std::vector<std::byte>{};
  auto int4_offset = std::int32_t{0};
  const auto int4_left = static_cast<std::int32_t>(vec_src.size());
  auto int4_block_start = std::int32_t{0};

  while (int4_offset < int4_left) {
    const auto int4_repeat_count = CountSame(vec_src, int4_offset, 0xFFFF);
    if (int4_repeat_count >= 4) {

      WriteCopyBlocks(vec_src, int4_block_start, int4_offset - int4_block_start, vec_ms);

      vec_ms.push_back(static_cast<std::byte>(0xFE));

      vec_ms.push_back(static_cast<std::byte>(int4_repeat_count & 0xFF));

      vec_ms.push_back(static_cast<std::byte>(int4_repeat_count >> 8));

      vec_ms.push_back(vec_src[static_cast<std::size_t>(int4_offset)]);

      int4_offset += int4_repeat_count;
      int4_block_start = int4_offset;
    } else
      int4_offset++;
  }

  WriteCopyBlocks(vec_src, int4_block_start, int4_offset - int4_block_start, vec_ms);

  vec_ms.push_back(static_cast<std::byte>(0x80));

  return vec_ms;
}

}
