module ora.game.render;

import std;

namespace ora::game::render {

namespace {

[[nodiscard]] bool src_take(std::span<const std::uint8_t> span1u_src, std::size_t& sz_offset,
    std::size_t sz_count) {
  if (sz_offset + sz_count > span1u_src.size() || sz_offset + sz_count < sz_offset)
    return false;
  sz_offset += sz_count;
  return true;
}

[[nodiscard]] std::optional<std::string> xor_span(std::span<const std::uint8_t> span1u_src,
    std::size_t& sz_offset, std::vector<std::uint8_t>& vec1u_dest, std::size_t& sz_dest_index,
    std::size_t sz_count, bool b_same_value) {
  if (sz_dest_index + sz_count > vec1u_dest.size())
    return std::string{"xor: delta 越界"};
  std::uint8_t u1_value{0};
  if (b_same_value) {
    if (sz_offset >= span1u_src.size()) return std::string{"xor: src 流耗尽"};
    u1_value = span1u_src[sz_offset];
    ++sz_offset;
  } else if (!src_take(span1u_src, sz_offset, sz_count)) {
    return std::string{"xor: src 流耗尽"};
  }
  for (std::size_t sz_k{0}; sz_k < sz_count; ++sz_k) {
    if (!b_same_value) {
      vec1u_dest[sz_dest_index + sz_k] ^= span1u_src[sz_offset - sz_count + sz_k];
    } else {
      vec1u_dest[sz_dest_index + sz_k] ^= u1_value;
    }
  }
  sz_dest_index += sz_count;
  return std::nullopt;
}

}

std::expected<std::size_t, std::string> xor_delta_decode_into(
    std::span<const std::uint8_t> span1u_src, std::size_t sz_src_offset,
    std::vector<std::uint8_t>& vec1u_dest) {
  if (sz_src_offset > span1u_src.size()) return std::unexpected("xor: src_offset 越界");

  std::size_t sz_src{sz_src_offset};
  std::size_t sz_dest_index{0};
  while (true) {
    if (sz_src >= span1u_src.size()) return std::unexpected("xor: src 流耗尽");
    const std::uint8_t u1_i{span1u_src[sz_src]};
    ++sz_src;

    if ((u1_i & 0x80) == 0) {
      const int int4_count{u1_i & 0x7F};
      if (int4_count == 0) {
        if (sz_src >= span1u_src.size()) return std::unexpected("xor: src 流耗尽");
        const std::uint8_t u1_count{span1u_src[sz_src]};
        ++sz_src;
        const auto res_xor{xor_span(span1u_src, sz_src, vec1u_dest, sz_dest_index, u1_count, true)};
        if (res_xor) return std::unexpected(*res_xor);
      } else {
        const auto res_xor{xor_span(span1u_src, sz_src, vec1u_dest, sz_dest_index,
            static_cast<std::size_t>(int4_count), false)};
        if (res_xor) return std::unexpected(*res_xor);
      }
    } else {
      const int int4_count{u1_i & 0x7F};
      if (int4_count == 0) {
        if (sz_src + 1 >= span1u_src.size()) return std::unexpected("xor: src 流耗尽");
        const int int4_word{static_cast<int>(span1u_src[sz_src])
            | (static_cast<int>(span1u_src[sz_src + 1]) << 8)};
        sz_src += 2;
        if (int4_word == 0) return sz_dest_index;

        if ((int4_word & 0x8000) == 0) {
          sz_dest_index += static_cast<std::size_t>(int4_word & 0x7FFF);
          if (sz_dest_index > vec1u_dest.size()) return std::unexpected("xor: skip 越界");
        } else if ((int4_word & 0x4000) == 0) {
          const auto res_xor{xor_span(span1u_src, sz_src, vec1u_dest, sz_dest_index,
              static_cast<std::size_t>(int4_word & 0x3FFF), false)};
          if (res_xor) return std::unexpected(*res_xor);
        } else {
          const auto res_xor{xor_span(span1u_src, sz_src, vec1u_dest, sz_dest_index,
              static_cast<std::size_t>(int4_word & 0x3FFF), true)};
          if (res_xor) return std::unexpected(*res_xor);
        }
      } else {
        sz_dest_index += static_cast<std::size_t>(int4_count);
        if (sz_dest_index > vec1u_dest.size()) return std::unexpected("xor: skip 越界");
      }
    }
  }
}

}
