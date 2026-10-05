// UPSTREAM: OpenRA.Mods.Common/FileFormats/FastByteReader.cs @b6fc03f L16-43(全文)
// 快速字节读取器:LCW/XOR/RLE0 解码的内环热路径(每字节一次方法调用,
// 无虚分发)。ReadWord 返回 int(x | next<<8),上游原样。
// 形态适配:byte[] + int offset → span + size_t;越界读上游抛 CLR
// IndexOutOfRangeException,C++ 抛 std::runtime_error(仅非法输入可达,
// 合法资产不触发 —— COVERAGE 登记)。
// The fast byte reader: the hot inner-loop path of the LCW/XOR/RLE0
// decoders (one method call per byte, no virtual dispatch). ReadWord
// returns an int (x | next<<8), exactly as upstream. Shape adaptation:
// byte[] + int offset → span + size_t; out-of-bounds reads throw CLR's
// IndexOutOfRangeException upstream and std::runtime_error here (reachable
// only on malformed input — registered in COVERAGE).
#pragma once
import std;

namespace ora::fmt {

class FastByteReader {
 public:
  FastByteReader(std::span<const std::byte> vec_src, std::size_t st_offset = 0)
      : vec_src_(vec_src), st_offset_(st_offset) {}

  bool Done() const { return st_offset_ >= vec_src_.size(); }

  /// 越界 = 上游 CLR IndexOutOfRangeException 的等价抛点(仅坏数据可达)。
  /// Out of bounds = the equivalent throw site of upstream's CLR
  /// IndexOutOfRangeException (reachable only on malformed data).
  std::byte ReadByte() {
    if (st_offset_ >= vec_src_.size()) [[unlikely]]
      throw std::runtime_error("Index was outside the bounds of the array.");
    return vec_src_[st_offset_++];
  }

  /// x | (下一字节 << 8);int 语义照上游。
  /// x | (next byte << 8); the upstream int semantics.
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

}  // namespace ora::fmt
