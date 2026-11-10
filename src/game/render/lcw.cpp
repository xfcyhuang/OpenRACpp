module ora.game.render;

import std;

namespace ora::game::render {

namespace {

class ByteReader {
public:
  ByteReader(std::span<const std::uint8_t> span1u_src, std::size_t sz_offset)
      : span1u_src_{span1u_src}, sz_offset_{sz_offset} {}

  [[nodiscard]] std::expected<std::uint8_t, std::string> read_byte() {
    if (sz_offset_ >= span1u_src_.size())
      return std::unexpected("lcw: src 流耗尽");
    return span1u_src_[sz_offset_++];
  }

  [[nodiscard]] std::expected<int, std::string> read_word() {
    const auto res_lo{read_byte()};
    if (!res_lo) return std::unexpected(res_lo.error());
    const auto res_hi{read_byte()};
    if (!res_hi) return std::unexpected(res_hi.error());
    return static_cast<int>(*res_lo) | (static_cast<int>(*res_hi) << 8);
  }

  [[nodiscard]] std::optional<std::string> copy_bytes(
      std::vector<std::uint8_t>& vec1u_dest, std::size_t sz_dest_index, std::size_t sz_count) {
    if (sz_offset_ + sz_count > span1u_src_.size() || sz_dest_index + sz_count > vec1u_dest.size())
      return std::string{"lcw: src 流耗尽"};
    for (std::size_t sz_i{0}; sz_i < sz_count; ++sz_i)
      vec1u_dest[sz_dest_index + sz_i] = span1u_src_[sz_offset_ + sz_i];
    sz_offset_ += sz_count;
    return std::nullopt;
  }

private:
  std::span<const std::uint8_t> span1u_src_;
  std::size_t sz_offset_;
};

}

std::expected<std::size_t, std::string> lcw_decode_into(
    std::span<const std::uint8_t> span1u_src, std::size_t sz_src_offset,
    std::vector<std::uint8_t>& vec1u_dest) {
  if (sz_src_offset > span1u_src.size())
    return std::unexpected("lcw: src_offset 越界");

  ByteReader reader{span1u_src, sz_src_offset};
  std::size_t sz_dest_index{0};
  while (true) {
    const auto res_i{reader.read_byte()};
    if (!res_i) return std::unexpected(res_i.error());
    const std::uint8_t u1_i{*res_i};

    if ((u1_i & 0x80) == 0) {
      const auto res_second{reader.read_byte()};
      if (!res_second) return std::unexpected(res_second.error());
      const int int4_count{((u1_i & 0x70) >> 4) + 3};
      const int int4_rpos{((u1_i & 0xF) << 8) + *res_second};

      if (int4_rpos > static_cast<int>(sz_dest_index))
        return std::unexpected(
            std::format("lcw: case2 rpos {} > destIndex {}", int4_rpos, sz_dest_index));
      if (static_cast<std::ptrdiff_t>(sz_dest_index) + int4_count
          > static_cast<std::ptrdiff_t>(vec1u_dest.size()))
        return sz_dest_index;

      const std::size_t sz_src_index{sz_dest_index - static_cast<std::size_t>(int4_rpos)};
      for (int int4_k{0}; int4_k < int4_count; ++int4_k) {
        if (sz_dest_index - sz_src_index == 1)
          vec1u_dest[sz_dest_index + static_cast<std::size_t>(int4_k)] =
              vec1u_dest[sz_dest_index - 1];
        else
          vec1u_dest[sz_dest_index + static_cast<std::size_t>(int4_k)] =
              vec1u_dest[sz_src_index + static_cast<std::size_t>(int4_k)];
      }
      sz_dest_index += static_cast<std::size_t>(int4_count);
    } else if ((u1_i & 0x40) == 0) {
      const int int4_count{u1_i & 0x3F};
      if (int4_count == 0) return sz_dest_index;

      const auto res_copy{reader.copy_bytes(vec1u_dest, sz_dest_index,
          static_cast<std::size_t>(int4_count))};
      if (res_copy) return std::unexpected(*res_copy);
      sz_dest_index += static_cast<std::size_t>(int4_count);
    } else {
      const int int4_count3{u1_i & 0x3F};
      if (int4_count3 == 0x3E) {
        const auto res_count{reader.read_word()};
        if (!res_count) return std::unexpected(res_count.error());
        const auto res_color{reader.read_byte()};
        if (!res_color) return std::unexpected(res_color.error());
        if (sz_dest_index + static_cast<std::size_t>(*res_count) > vec1u_dest.size())
          return std::unexpected("lcw: case4 RLE 越界");
        for (int int4_end{static_cast<int>(sz_dest_index) + *res_count};
            static_cast<int>(sz_dest_index) < int4_end; ++sz_dest_index)
          vec1u_dest[sz_dest_index] = *res_color;
      } else {
        int int4_count{};
        if (int4_count3 == 0x3F) {
          const auto res_word{reader.read_word()};
          if (!res_word) return std::unexpected(res_word.error());
          int4_count = *res_word;
        } else {
          int4_count = int4_count3 + 3;
        }
        const auto res_src_index{reader.read_word()};
        if (!res_src_index) return std::unexpected(res_src_index.error());
        if (static_cast<std::size_t>(*res_src_index) >= sz_dest_index)
          return std::unexpected(
              std::format("lcw: srcIndex >= destIndex {} {}", *res_src_index, sz_dest_index));
        if (sz_dest_index + static_cast<std::size_t>(int4_count) > vec1u_dest.size())
          return std::unexpected("lcw: case3/5 复制越界");

        std::size_t sz_src_index{static_cast<std::size_t>(*res_src_index)};
        for (int int4_end{static_cast<int>(sz_dest_index) + int4_count};
            static_cast<int>(sz_dest_index) < int4_end; ++sz_dest_index) {
          vec1u_dest[sz_dest_index] = vec1u_dest[sz_src_index];
          ++sz_src_index;
        }
      }
    }
  }
}

}
