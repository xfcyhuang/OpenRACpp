#include "game/filesystem/inflate.hpp"

namespace ora::game::filesystem {

namespace {

constexpr std::size_t kWindowSize{32768};
constexpr std::size_t kMaxOutputBytes{1u << 28};

struct BitReader {
  std::span<const std::uint8_t> span1u_data;
  std::size_t sz_bit_pos{0};

  [[nodiscard]] bool bits_left(std::size_t sz_count) const {
    return sz_bit_pos + sz_count <= span1u_data.size() * 8u;
  }

  [[nodiscard]] std::uint32_t read_bits(int int4_count) {
    std::uint32_t u4_value{0};
    for (int int4_i{0}; int4_i < int4_count; ++int4_i) {
      const std::size_t sz_byte{sz_bit_pos >> 3};
      const std::uint32_t u4_bit{(span1u_data[sz_byte] >> (sz_bit_pos & 7u)) & 1u};
      u4_value |= u4_bit << int4_i;
      ++sz_bit_pos;
    }
    return u4_value;
  }

  void align_to_byte() {
    sz_bit_pos = (sz_bit_pos + 7u) & ~static_cast<std::size_t>(7u);
  }
};

struct HuffmanTable {
  std::array<std::uint16_t, 16> vec2u_count{};
  std::array<std::uint16_t, 288> vec2u_symbol{};

  [[nodiscard]] std::expected<int, std::string> decode(BitReader& reader_bits) const {
    int int4_code{0};
    int int4_first{0};
    int int4_index{0};
    for (int int4_len{1}; int4_len <= 15; ++int4_len) {
      if (!reader_bits.bits_left(1)) return std::unexpected(std::string{"deflate 码流截断"});
      int4_code |= static_cast<int>(reader_bits.read_bits(1));
      const int int4_count{vec2u_count[static_cast<std::size_t>(int4_len)]};
      if (int4_code - int4_first < int4_count)
        return vec2u_symbol[static_cast<std::size_t>(int4_index + int4_code - int4_first)];
      int4_index += int4_count;
      int4_first = (int4_first + int4_count) << 1;
      int4_code <<= 1;
    }
    return std::unexpected(std::string{"deflate Huffman 码越界"});
  }
};

[[nodiscard]] HuffmanTable build_huffman_table(std::span<const int> vec4_lengths) {
  HuffmanTable table_out;
  for (const int int4_len : vec4_lengths)
    if (int4_len > 0) ++table_out.vec2u_count[static_cast<std::size_t>(int4_len)];
  std::array<std::uint16_t, 16> vec2u_offsets{};
  std::uint16_t u2_next{0};
  for (int int4_len{1}; int4_len <= 15; ++int4_len) {
    vec2u_offsets[static_cast<std::size_t>(int4_len)] = u2_next;
    u2_next += table_out.vec2u_count[static_cast<std::size_t>(int4_len)];
  }
  for (std::size_t sz_sym{0}; sz_sym < vec4_lengths.size(); ++sz_sym)
    if (vec4_lengths[sz_sym] > 0)
      table_out.vec2u_symbol[vec2u_offsets[static_cast<std::size_t>(vec4_lengths[sz_sym])]++]
          = static_cast<std::uint16_t>(sz_sym);
  return table_out;
}

constexpr std::array<int, 29> kLengthBase{3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43,
    51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258};
constexpr std::array<int, 29> kLengthExtra{0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4,
    4, 4, 5, 5, 5, 5, 0};
constexpr std::array<int, 30> kDistBase{1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385,
    513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577};
constexpr std::array<int, 30> kDistExtra{0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9,
    10, 10, 11, 11, 12, 12, 13, 13};
constexpr std::array<int, 19> kCodeLengthOrder{16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};

[[nodiscard]] std::expected<void, std::string> inflate_stored(BitReader& reader_bits,
    std::vector<std::uint8_t>& vec1u_out) {
  reader_bits.align_to_byte();
  const std::size_t sz_byte_pos{reader_bits.sz_bit_pos >> 3};
  if (sz_byte_pos + 4 > reader_bits.span1u_data.size()) return std::unexpected(std::string{"deflate stored 块头截断"});
  const std::uint16_t u2_len{static_cast<std::uint16_t>(reader_bits.span1u_data[sz_byte_pos]
      | (reader_bits.span1u_data[sz_byte_pos + 1] << 8))};
  const std::uint16_t u2_nlen{static_cast<std::uint16_t>(reader_bits.span1u_data[sz_byte_pos + 2]
      | (reader_bits.span1u_data[sz_byte_pos + 3] << 8))};
  if (static_cast<std::uint16_t>(~u2_len) != u2_nlen) return std::unexpected(std::string{"deflate stored LEN/NLEN 不符"});
  if (sz_byte_pos + 4 + u2_len > reader_bits.span1u_data.size())
    return std::unexpected(std::string{"deflate stored 数据截断"});
  if (vec1u_out.size() + u2_len > kMaxOutputBytes) return std::unexpected(std::string{"deflate 输出超限"});
  for (std::size_t sz_i{0}; sz_i < u2_len; ++sz_i)
    vec1u_out.push_back(reader_bits.span1u_data[sz_byte_pos + 4 + sz_i]);
  reader_bits.sz_bit_pos = (sz_byte_pos + 4 + u2_len) * 8u;
  return {};
}

[[nodiscard]] std::expected<void, std::string> read_dynamic_tables(BitReader& reader_bits,
    HuffmanTable& table_lit, HuffmanTable& table_dist) {
  const int int4_hlit{static_cast<int>(reader_bits.read_bits(5)) + 257};
  const int int4_hdist{static_cast<int>(reader_bits.read_bits(5)) + 1};
  const int int4_hclen{static_cast<int>(reader_bits.read_bits(4)) + 4};
  if (!reader_bits.bits_left(static_cast<std::size_t>(int4_hclen) * 3u))
    return std::unexpected(std::string{"deflate 码长表截断"});

  std::array<int, 19> vec4_cl_lengths{};
  for (int int4_i{0}; int4_i < int4_hclen; ++int4_i)
    vec4_cl_lengths[static_cast<std::size_t>(kCodeLengthOrder[static_cast<std::size_t>(int4_i)])]
        = static_cast<int>(reader_bits.read_bits(3));
  const HuffmanTable table_cl{build_huffman_table(vec4_cl_lengths)};

  std::vector<int> vec4_lengths(static_cast<std::size_t>(int4_hlit + int4_hdist), 0);
  std::size_t sz_pos{0};
  while (sz_pos < vec4_lengths.size()) {
    const auto res_sym{table_cl.decode(reader_bits)};
    if (!res_sym) return std::unexpected(res_sym.error());
    const int int4_sym{*res_sym};
    if (int4_sym < 16) {
      vec4_lengths[sz_pos++] = int4_sym;
    } else {
      int int4_repeat{0};
      int int4_value{0};
      if (int4_sym == 16) {
        if (sz_pos == 0) return std::unexpected(std::string{"deflate 码长重复无前驱"});
        int4_repeat = 3 + static_cast<int>(reader_bits.read_bits(2));
        int4_value = vec4_lengths[sz_pos - 1];
      } else if (int4_sym == 17) {
        int4_repeat = 3 + static_cast<int>(reader_bits.read_bits(3));
      } else {
        int4_repeat = 11 + static_cast<int>(reader_bits.read_bits(7));
      }
      if (sz_pos + static_cast<std::size_t>(int4_repeat) > vec4_lengths.size())
        return std::unexpected(std::string{"deflate 码长重复越界"});
      for (int int4_i{0}; int4_i < int4_repeat; ++int4_i) vec4_lengths[sz_pos++] = int4_value;
    }
  }
  if (vec4_lengths[256] == 0) return std::unexpected(std::string{"deflate 缺 EOB 码"});

  table_lit = build_huffman_table(std::span<const int>{vec4_lengths.data(), static_cast<std::size_t>(int4_hlit)});
  table_dist = build_huffman_table(std::span<const int>{vec4_lengths.data() + int4_hlit,
      static_cast<std::size_t>(int4_hdist)});
  return {};
}

[[nodiscard]] std::expected<void, std::string> inflate_block(BitReader& reader_bits, const HuffmanTable& table_lit,
    const HuffmanTable& table_dist, std::vector<std::uint8_t>& vec1u_out) {
  for (;;) {
    const auto res_sym{table_lit.decode(reader_bits)};
    if (!res_sym) return std::unexpected(res_sym.error());
    const int int4_sym{*res_sym};
    if (int4_sym < 256) {
      if (vec1u_out.size() + 1 > kMaxOutputBytes) return std::unexpected(std::string{"deflate 输出超限"});
      vec1u_out.push_back(static_cast<std::uint8_t>(int4_sym));
    } else if (int4_sym == 256) {
      return {};
    } else {
      const int int4_len_idx{int4_sym - 257};
      if (int4_len_idx >= 29) return std::unexpected(std::string{"deflate length 码越界"});
      const int int4_extra_bits{kLengthExtra[static_cast<std::size_t>(int4_len_idx)]};
      if (!reader_bits.bits_left(static_cast<std::size_t>(int4_extra_bits)))
        return std::unexpected(std::string{"deflate length 额外位截断"});
      const int int4_length{kLengthBase[static_cast<std::size_t>(int4_len_idx)]
          + static_cast<int>(reader_bits.read_bits(int4_extra_bits))};

      const auto res_dist{table_dist.decode(reader_bits)};
      if (!res_dist) return std::unexpected(res_dist.error());
      const int int4_dist_idx{*res_dist};
      if (int4_dist_idx >= 30) return std::unexpected(std::string{"deflate distance 码越界"});
      const int int4_dist_extra{kDistExtra[static_cast<std::size_t>(int4_dist_idx)]};
      if (!reader_bits.bits_left(static_cast<std::size_t>(int4_dist_extra)))
        return std::unexpected(std::string{"deflate distance 额外位截断"});
      const int int4_distance{kDistBase[static_cast<std::size_t>(int4_dist_idx)]
          + static_cast<int>(reader_bits.read_bits(int4_dist_extra))};

      if (static_cast<std::size_t>(int4_distance) > vec1u_out.size() || int4_distance < 1)
        return std::unexpected(std::string{"deflate distance 越过窗口前缘"});
      if (vec1u_out.size() + static_cast<std::size_t>(int4_length) > kMaxOutputBytes)
        return std::unexpected(std::string{"deflate 输出超限"});
      const std::size_t sz_src{vec1u_out.size() - static_cast<std::size_t>(int4_distance)};
      for (int int4_i{0}; int4_i < int4_length; ++int4_i)
        vec1u_out.push_back(vec1u_out[sz_src + static_cast<std::size_t>(int4_i)]);
    }
  }
}

}

std::expected<std::vector<std::uint8_t>, std::string> inflate_raw(
    std::span<const std::uint8_t> span1u_input) {
  std::vector<std::uint8_t> vec1u_out;
  if (span1u_input.empty()) return std::unexpected(std::string{"deflate 输入为空"});
  BitReader reader_bits{span1u_input, 0};

  const HuffmanTable table_fixed_lit{[] {
    std::array<int, 288> vec4_lengths{};
    for (int int4_i{0}; int4_i < 144; ++int4_i) vec4_lengths[static_cast<std::size_t>(int4_i)] = 8;
    for (int int4_i{144}; int4_i < 256; ++int4_i) vec4_lengths[static_cast<std::size_t>(int4_i)] = 9;
    for (int int4_i{256}; int4_i < 280; ++int4_i) vec4_lengths[static_cast<std::size_t>(int4_i)] = 7;
    for (int int4_i{280}; int4_i < 288; ++int4_i) vec4_lengths[static_cast<std::size_t>(int4_i)] = 8;
    return build_huffman_table(vec4_lengths);
  }()};
  const HuffmanTable table_fixed_dist{[] {
    std::array<int, 30> vec4_lengths{};
    for (int int4_i{0}; int4_i < 30; ++int4_i) vec4_lengths[static_cast<std::size_t>(int4_i)] = 5;
    return build_huffman_table(vec4_lengths);
  }()};

  bool b_final{false};
  while (!b_final) {
    if (!reader_bits.bits_left(3)) return std::unexpected(std::string{"deflate 块头截断"});
    b_final = reader_bits.read_bits(1) != 0u;
    const std::uint32_t u4_btype{reader_bits.read_bits(2)};
    if (u4_btype == 0u) {
      const auto res_stored{inflate_stored(reader_bits, vec1u_out)};
      if (!res_stored) return std::unexpected(res_stored.error());
    } else if (u4_btype == 1u) {
      const auto res_block{inflate_block(reader_bits, table_fixed_lit, table_fixed_dist, vec1u_out)};
      if (!res_block) return std::unexpected(res_block.error());
    } else if (u4_btype == 2u) {
      HuffmanTable table_lit{};
      HuffmanTable table_dist{};
      const auto res_tables{read_dynamic_tables(reader_bits, table_lit, table_dist)};
      if (!res_tables) return std::unexpected(res_tables.error());
      const auto res_block{inflate_block(reader_bits, table_lit, table_dist, vec1u_out)};
      if (!res_block) return std::unexpected(res_block.error());
    } else {
      return std::unexpected(std::string{"deflate BTYPE=3 保留块"});
    }
  }

  static_cast<void>(kWindowSize);
  return vec1u_out;
}

}
