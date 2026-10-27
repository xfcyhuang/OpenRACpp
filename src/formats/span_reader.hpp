#pragma once
import std;

namespace ora::fmt {

class SpanReader {
 public:
  explicit SpanReader(std::span<const std::byte> vec_source) : vec_source_(vec_source) {}

  std::int64_t Position() const { return int8_pos_; }
  std::int64_t Length() const { return static_cast<std::int64_t>(vec_source_.size()); }

  void Seek(std::int64_t int8_pos) {
    if (int8_pos < 0 || int8_pos > Length()) [[unlikely]]
      throw std::runtime_error("Attempted to seek outside the source span.");
    int8_pos_ = int8_pos;
  }

  void Skip(std::int64_t int8_count) { Seek(int8_pos_ + int8_count); }

  std::int32_t Peek() const {
    return int8_pos_ >= Length() ? -1 : static_cast<std::int32_t>(
                                           static_cast<std::uint8_t>(vec_source_[static_cast<std::size_t>(int8_pos_)]));
  }

  std::uint8_t ReadUInt8() { return static_cast<std::uint8_t>(vec_source_.at(static_cast<std::size_t>(int8_pos_++))); }

  std::uint16_t ReadUInt16() {
    const auto uint1_lo = ReadUInt8();
    return static_cast<std::uint16_t>(uint1_lo | (ReadUInt8() << 8));
  }

  std::uint32_t ReadUInt32() {
    const auto uint2_lo = ReadUInt16();
    return static_cast<std::uint32_t>(uint2_lo | (ReadUInt16() << 16));
  }

  std::int32_t ReadInt32() { return static_cast<std::int32_t>(ReadUInt32()); }

  float ReadSingle() { return std::bit_cast<float>(ReadUInt32()); }

  std::span<const std::byte> ReadBytes(std::int64_t int8_count) {
    const auto span_result = vec_source_.subspan(static_cast<std::size_t>(int8_pos_), static_cast<std::size_t>(int8_count));
    int8_pos_ += int8_count;
    return span_result;
  }

  void ReadInto(std::span<std::byte> vec_dest, std::size_t st_offset, std::size_t st_count) {
    std::ranges::copy(ReadBytes(static_cast<std::int64_t>(st_count)),
                      vec_dest.begin() + static_cast<std::ptrdiff_t>(st_offset));
  }

 private:
  std::span<const std::byte> vec_source_;
  std::int64_t int8_pos_ = 0;
};

}
