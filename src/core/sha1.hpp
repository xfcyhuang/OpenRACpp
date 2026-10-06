// UPSTREAM: NONE —— FIPS 180-1 SHA-1(上游经 System.Security.Cryptography.
// SHA1.HashData 消费,见 OpenRA.Game/CryptoUtil.cs L251-263;算法为公开标准,
// C++ 侧自给自足。标准测试向量:NIST FIPS 180-1 附录 + "abc" 空串经典向量)
// UPSTREAM: NONE — FIPS 180-1 SHA-1 (upstream consumes it through
// System.Security.Cryptography.SHA1.HashData, OpenRA.Game/CryptoUtil.cs
// L251-263; the algorithm is a public standard, self-provided on the C++
// side. Standard test vectors: the NIST FIPS 180-1 appendix plus the "abc"/
// empty-string classics).
#pragma once
import std;

namespace ora::sha1 {

/// SHA-1 上下文:64 字节分块,大端位序(算法规范;无上游对应物)
/// The SHA-1 context: 64-byte blocks, big-endian word order (algorithm
/// spec; no upstream counterpart).
class Sha1 final {
 public:
  Sha1() { Reset(); }

  void Reset() {
    h0_ = 0x67452301;
    h1_ = 0xEFCDAB89;
    h2_ = 0x98BADCFE;
    h3_ = 0x10325476;
    h4_ = 0xC3D2E1F0;
    size_buffered_ = 0;
    uint8_total_length_ = 0;
  }

  void Update(std::span<const unsigned char> bytes) {
    uint8_total_length_ += bytes.size();
    while (!bytes.empty()) {
      const std::size_t size_fill =
          std::min(bytes.size(), std::size_t{64} - size_buffered_);
      std::memcpy(arr_buffer_ + size_buffered_, bytes.data(), size_fill);
      size_buffered_ += size_fill;
      bytes = bytes.subspan(size_fill);
      if (size_buffered_ == 64) {
        ProcessBlock(arr_buffer_);
        size_buffered_ = 0;
      }
    }
  }

  /// 终结:0x80 填充 + 0 长度位 + 64 位大端总长(规范;流式与一次性同结果)
  /// Finalize: 0x80 pad + zero bits + the 64-bit big-endian total length
  /// (spec; streaming and one-shot agree).
  std::array<unsigned char, 20> Finish() {
    // 填充计算只影响本地副本(摘要后对象仍可续用与否不属契约;以本地
    // 长度快照处理 —— 与 .NET 一次性 HashData 的语义对齐)
    // Padding works on local state only (post-digest reuse is not part of
    // the contract; handled via a local length snapshot — aligned with
    // .NET's one-shot HashData semantics).
    const std::uint64_t uint8_bits =
        static_cast<std::uint64_t>(uint8_total_length_) * 8;

    unsigned char pad[72] = {};
    pad[0] = 0x80;
    const std::size_t size_rem = (size_buffered_ + 1) % 64;
    const std::size_t size_zeros =
        size_rem <= 56 ? 56 - size_rem : 120 - size_rem;
    const std::size_t size_pad = 1 + size_zeros + 8;
    for (int i = 0; i < 8; i++)
      pad[size_pad - 1 - i] =
          static_cast<unsigned char>((uint8_bits >> (i * 8)) & 0xFF);

    // 填充块走同一分块路径(不更新 uint8_total_length_ —— 长度只计载荷)
    // Padding blocks take the same block path (uint8_total_length_ is not
    // updated — it counts payload only).
    auto padded = std::span<const unsigned char>(pad, size_pad);
    while (!padded.empty()) {
      const std::size_t size_fill =
          std::min(padded.size(), std::size_t{64} - size_buffered_);
      std::memcpy(arr_buffer_ + size_buffered_, padded.data(), size_fill);
      size_buffered_ += size_fill;
      padded = padded.subspan(size_fill);
      if (size_buffered_ == 64) {
        ProcessBlock(arr_buffer_);
        size_buffered_ = 0;
      }
    }

    std::array<unsigned char, 20> out{};
    const std::uint32_t arr_h[5] = {h0_, h1_, h2_, h3_, h4_};
    for (int i = 0; i < 5; i++)
      for (int j = 0; j < 4; j++)
        out[i * 4 + j] =
            static_cast<unsigned char>((arr_h[i] >> (24 - j * 8)) & 0xFF);
    return out;
  }

 private:
  static std::uint32_t RotateLeft(std::uint32_t v, int n) {
    return (v << n) | (v >> (32 - n));
  }

  void ProcessBlock(const unsigned char* p_block) {
    std::array<std::uint32_t, 80> w{};
    for (int i = 0; i < 16; i++)
      w[i] = (static_cast<std::uint32_t>(p_block[i * 4]) << 24) |
             (static_cast<std::uint32_t>(p_block[i * 4 + 1]) << 16) |
             (static_cast<std::uint32_t>(p_block[i * 4 + 2]) << 8) |
             static_cast<std::uint32_t>(p_block[i * 4 + 3]);
    for (int i = 16; i < 80; i++)
      w[i] = RotateLeft(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);

    std::uint32_t a = h0_, b = h1_, c = h2_, d = h3_, e = h4_;
    for (int i = 0; i < 80; i++) {
      std::uint32_t f, k;
      if (i < 20) {
        f = (b & c) | ((~b) & d);
        k = 0x5A827999;
      } else if (i < 40) {
        f = b ^ c ^ d;
        k = 0x6ED9EBA1;
      } else if (i < 60) {
        f = (b & c) | (b & d) | (c & d);
        k = 0x8F1BBCDC;
      } else {
        f = b ^ c ^ d;
        k = 0xCA62C1D6;
      }
      const std::uint32_t temp =
          RotateLeft(a, 5) + f + e + k + w[i];  // uint32 回绕 | uint32 wrap
      e = d;
      d = c;
      c = RotateLeft(b, 30);
      b = a;
      a = temp;
    }

    h0_ += a;
    h1_ += b;
    h2_ += c;
    h3_ += d;
    h4_ += e;
  }

  std::uint32_t h0_, h1_, h2_, h3_, h4_;
  unsigned char arr_buffer_[64];
  std::size_t size_buffered_;
  std::size_t uint8_total_length_;
};

/// 一次性摘要 | the one-shot digest.
inline std::array<unsigned char, 20> HashData(std::span<const unsigned char> bytes) {
  Sha1 s;
  s.Update(bytes);
  return s.Finish();
}

/// 小写 hex(CryptoUtil.SHA1Hash 的输出形态) | lowercase hex (the output
/// form of CryptoUtil.SHA1Hash).
inline std::string HexOf(std::span<const unsigned char> digest) {
  static constexpr char kHexLower[] = "0123456789abcdef";
  std::string out;
  out.reserve(digest.size() * 2);
  for (const unsigned char b : digest) {
    out.push_back(kHexLower[b >> 4]);
    out.push_back(kHexLower[b & 0xF]);
  }
  return out;
}

}  // namespace ora::sha1
