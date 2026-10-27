#pragma once
import std;

namespace ora::fmt {

class FastByteReader {
 public:
  FastByteReader(std::span<const std::byte> vec_src, std::size_t st_offset = 0)
      : vec_src_(vec_src), st_offset_(st_offset) {}

  bool Done() const { return st_offset_ >= vec_src_.size(); }

  std::byte ReadByte() {
    if (st_offset_ >= vec_src_.size()) [[unlikely]]
      throw std::runtime_error("Index was outside the bounds of the array.");
    return vec_src_[st_offset_++];
  }

  std::int32_t ReadWord() {
    const auto uint1_x = static_cast<std::int32_t>(ReadByte());
    return uint1_x | (static_cast<std::int32_t>(ReadByte()) << 8);
  }

  void CopyTo(std::span<std::byte> vec_dest, std::size_t st_offset, std::int32_t int4_count) {
    const auto count_size = static_cast<std::size_t>(int4_count);
    std::ranges::copy(vec_src_.subspan(st_offset_, count_size), vec_dest.begin() + static_cast<std::ptrdiff_t>(st_offset));
    st_offset_ += count_size;
  }

  std::int32_t Remaining() const { return static_cast<std::int32_t>(vec_src_.size() - st_offset_); }

 private:
  std::span<const std::byte> vec_src_;
  std::size_t st_offset_;
};

}
