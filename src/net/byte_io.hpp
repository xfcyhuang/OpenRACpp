#pragma once
import std;

namespace ora::net {

class ByteWriter {
 public:
  void WriteByte(std::uint8_t v) { vec_out_.push_back(v); }
  void Write(std::uint8_t v) { WriteByte(v); }

  void Write(std::int16_t v) { WriteScalar(v); }
  void Write(std::uint16_t v) { WriteScalar(v); }
  void Write(std::int32_t v) { WriteScalar(v); }
  void Write(std::uint32_t v) { WriteScalar(v); }
  void Write(std::int64_t v) { WriteScalar(v); }
  void Write(std::uint64_t v) { WriteScalar(v); }
  void Write(float v) {
    std::uint32_t bits;
    std::memcpy(&bits, &v, 4);
    Write(bits);
  }

  void Write(std::string_view sv) {
    Write7BitEncoded64(sv.size());
    vec_out_.insert(vec_out_.end(), sv.begin(), sv.end());
  }

  void WriteBytes(const std::vector<std::uint8_t>& v) {
    vec_out_.insert(vec_out_.end(), v.begin(), v.end());
  }

  const std::vector<std::uint8_t>& Bytes() const { return vec_out_; }
  std::vector<std::uint8_t> TakeBytes() { return std::move(vec_out_); }
  std::size_t Size() const { return vec_out_.size(); }

 private:
  template <class T>
  void WriteScalar(T v) {
    for (unsigned i = 0; i < sizeof(T); ++i)
      vec_out_.push_back(
          static_cast<std::uint8_t>((static_cast<std::uint64_t>(v) >> (i * 8)) &
                                    0xFF));
  }

  void Write7BitEncoded64(std::uint64_t v) {
    while (v >= 0x80) {
      vec_out_.push_back(static_cast<std::uint8_t>(v | 0x80));
      v >>= 7;
    }
    vec_out_.push_back(static_cast<std::uint8_t>(v));
  }

  std::vector<std::uint8_t> vec_out_;
};

class ByteReader {
 public:
  ByteReader(const std::uint8_t* data, std::size_t size)
      : p_data_(data), sz_size_(size) {}
  explicit ByteReader(const std::vector<std::uint8_t>& v)
      : ByteReader(v.data(), v.size()) {}

  std::size_t Position() const { return sz_pos_; }
  std::size_t Length() const { return sz_size_; }
  std::int64_t Remaining() const {
    return static_cast<std::int64_t>(sz_size_ - sz_pos_);
  }

  std::uint8_t ReadByte() {
    Ensure(1);
    return p_data_[sz_pos_++];
  }

  std::int16_t ReadInt16() { return ReadScalar<std::int16_t>(); }
  std::uint16_t ReadUInt16() { return ReadScalar<std::uint16_t>(); }
  std::int32_t ReadInt32() { return ReadScalar<std::int32_t>(); }
  std::uint32_t ReadUInt32() { return ReadScalar<std::uint32_t>(); }
  std::int64_t ReadInt64() { return ReadScalar<std::int64_t>(); }
  std::uint64_t ReadUInt64() { return ReadScalar<std::uint64_t>(); }
  float ReadSingle() {
    const auto bits = ReadUInt32();
    float v;
    std::memcpy(&v, &bits, 4);
    return v;
  }

  std::string ReadString() {
    const auto len = Read7BitEncoded64();
    if (len > static_cast<std::uint64_t>(Remaining()))
      throw std::runtime_error("End of stream");
    std::string out;
    out.resize(len);
    for (std::uint64_t i = 0; i < len; ++i)
      out[i] = static_cast<char>(ReadByte());
    return out;
  }

  void ReadBytes(std::size_t count, std::vector<std::uint8_t>& out) {
    Ensure(count);
    out.reserve(out.size() + count);
    for (std::size_t i = 0; i < count; ++i)
      out.push_back(p_data_[sz_pos_++]);
  }

 private:
  void Ensure(std::size_t n) const {
    if (sz_pos_ + n > sz_size_)
      throw std::runtime_error("End of stream");
  }

  template <class T>
  T ReadScalar() {
    Ensure(sizeof(T));
    std::uint64_t v = 0;
    for (unsigned i = 0; i < sizeof(T); ++i)
      v |= static_cast<std::uint64_t>(p_data_[sz_pos_++]) << (i * 8);
    return static_cast<T>(v);
  }

  std::uint64_t Read7BitEncoded64() {
    std::uint64_t v = 0;
    int shift = 0;
    for (;;) {
      const auto b = ReadByte();
      v |= static_cast<std::uint64_t>(b & 0x7F) << shift;
      if ((b & 0x80) == 0)
        break;
      shift += 7;
    }
    return v;
  }

  const std::uint8_t* p_data_;
  std::size_t sz_size_;
  std::size_t sz_pos_ = 0;
};

}
