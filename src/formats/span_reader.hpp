// UPSTREAM: NONE —— 自有形态适配件:对应 C# Stream 的随机访问读取面
// 上游格式加载器直接消费 System.IO.Stream(Position/Seek/ReadUInt16/
// ReadBytes…,StreamExtensions 扩展)。C++ 侧将文件字节物化为 span 后
// 以本读取器替换 Stream:位置 long 语义、小端多字节读取、区间读取返回
// span(零拷贝)。越界 = 上游 EndOfStreamException/IndexOutOfRange 的
// 等价抛点(std::runtime_error;仅坏数据可达 —— COVERAGE 登记)。
// UPSTREAM: NONE (an in-house shape adapter replacing the random-access
// read surface of C#'s Stream). Upstream format loaders consume a
// System.IO.Stream directly (Position/Seek/ReadUInt16/ReadBytes via the
// StreamExtensions). The C++ side materializes file bytes into a span and
// substitutes this reader for the Stream: long-typed position, little-
// endian multi-byte reads, and zero-copy subspan reads. Out-of-bounds
// access throws std::runtime_error at the equivalent points of upstream's
// EndOfStreamException/IndexOutOfRange (reachable only on malformed data —
// registered in COVERAGE).
#pragma once
import std;

namespace ora::fmt {

/// 只读字节区间 + 游标(上游 Stream 的局部替身;pos 上游为 long)。
/// A read-only byte span with a cursor (a local stand-in for the upstream
/// Stream; the position is a long like upstream's).
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

  /// 上游 s.Position += n 形态。
  /// The upstream s.Position += n form.
  void Skip(std::int64_t int8_count) { Seek(int8_pos_ + int8_count); }

  /// 上游 StreamExts.Peek(StreamExts.cs L60-68):窥视下一字节,流尽返
  /// 回 -1(不抛;VqaVideo 的偶对齐 `Peek() == 0` 消费依赖此 EOF 语义)。
  /// Upstream StreamExts.Peek (StreamExts.cs L60-68): peeks the next
  /// byte, returning -1 at end of stream without throwing (VqaVideo's
  /// even-alignment `Peek() == 0` consume relies on this EOF behavior).
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

  /// 上游 ReadInt32:位模式回绕解释。
  /// Upstream ReadInt32: the bit pattern reinterpreted.
  std::int32_t ReadInt32() { return static_cast<std::int32_t>(ReadUInt32()); }

  /// 读 count 字节返回区间(零拷贝;上游 ReadBytes(int) 分配新数组)。
  /// Reads count bytes as a subspan (zero-copy; upstream's ReadBytes(int)
  /// allocates a fresh array).
  std::span<const std::byte> ReadBytes(std::int64_t int8_count) {
    const auto span_result = vec_source_.subspan(static_cast<std::size_t>(int8_pos_), static_cast<std::size_t>(int8_count));
    int8_pos_ += int8_count;
    return span_result;
  }

  /// 上游 Stream.ReadBytes(byte[] dest, int offset, int count) 填充形态。
  /// The filling form of upstream's Stream.ReadBytes(byte[] dest, int
  /// offset, int count).
  void ReadInto(std::span<std::byte> vec_dest, std::size_t st_offset, std::size_t st_count) {
    std::ranges::copy(ReadBytes(static_cast<std::int64_t>(st_count)),
                      vec_dest.begin() + static_cast<std::ptrdiff_t>(st_offset));
  }

 private:
  std::span<const std::byte> vec_source_;
  std::int64_t int8_pos_ = 0;
};

}  // namespace ora::fmt
