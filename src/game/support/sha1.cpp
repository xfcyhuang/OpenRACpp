#include "game/support/sha1.hpp"

namespace ora::game::support {

namespace {

constexpr std::uint32_t rol32(std::uint32_t u4_value, int int4_bits) {
  return (u4_value << int4_bits) | (u4_value >> (32 - int4_bits));
}

}

Sha1Digest sha1_digest(std::span<const std::uint8_t> span1u_data) {
  std::uint32_t vec4u_h[5] = {0x67452301u, 0xEFCDAB89u, 0x98BADCFEu, 0x10325476u, 0xC3D2E1F0u};

  const std::uint64_t u8_bit_len{static_cast<std::uint64_t>(span1u_data.size()) * 8u};
  const std::size_t sz_padded{((span1u_data.size() + 8) / 64 + 1) * 64};
  std::vector<std::uint8_t> vec1u_padded(sz_padded, 0u);
  for (std::size_t sz_i{0}; sz_i < span1u_data.size(); ++sz_i)
    vec1u_padded[sz_i] = span1u_data[sz_i];
  vec1u_padded[span1u_data.size()] = 0x80u;
  for (int int4_i{0}; int4_i < 8; ++int4_i)
    vec1u_padded[sz_padded - 1 - static_cast<std::size_t>(int4_i)]
        = static_cast<std::uint8_t>((u8_bit_len >> (8 * int4_i)) & 0xFFu);

  std::uint32_t vec4u_w[80]{};
  for (std::size_t sz_block{0}; sz_block < sz_padded; sz_block += 64) {
    for (int int4_t{0}; int4_t < 16; ++int4_t) {
      const std::size_t sz_off{sz_block + static_cast<std::size_t>(int4_t) * 4u};
      vec4u_w[int4_t] = (static_cast<std::uint32_t>(vec1u_padded[sz_off]) << 24)
          | (static_cast<std::uint32_t>(vec1u_padded[sz_off + 1]) << 16)
          | (static_cast<std::uint32_t>(vec1u_padded[sz_off + 2]) << 8)
          | static_cast<std::uint32_t>(vec1u_padded[sz_off + 3]);
    }
    for (int int4_t{16}; int4_t < 80; ++int4_t)
      vec4u_w[int4_t] = rol32(vec4u_w[int4_t - 3] ^ vec4u_w[int4_t - 8] ^ vec4u_w[int4_t - 14] ^ vec4u_w[int4_t - 16], 1);

    std::uint32_t u4_a{vec4u_h[0]}, u4_b{vec4u_h[1]}, u4_c{vec4u_h[2]}, u4_d{vec4u_h[3]}, u4_e{vec4u_h[4]};
    for (int int4_t{0}; int4_t < 80; ++int4_t) {
      std::uint32_t u4_f{};
      std::uint32_t u4_k{};
      if (int4_t < 20) {
        u4_f = (u4_b & u4_c) | ((~u4_b) & u4_d);
        u4_k = 0x5A827999u;
      } else if (int4_t < 40) {
        u4_f = u4_b ^ u4_c ^ u4_d;
        u4_k = 0x6ED9EBA1u;
      } else if (int4_t < 60) {
        u4_f = (u4_b & u4_c) | (u4_b & u4_d) | (u4_c & u4_d);
        u4_k = 0x8F1BBCDCu;
      } else {
        u4_f = u4_b ^ u4_c ^ u4_d;
        u4_k = 0xCA62C1D6u;
      }
      const std::uint32_t u4_tmp{rol32(u4_a, 5) + u4_f + u4_e + u4_k + vec4u_w[int4_t]};
      u4_e = u4_d;
      u4_d = u4_c;
      u4_c = rol32(u4_b, 30);
      u4_b = u4_a;
      u4_a = u4_tmp;
    }
    vec4u_h[0] += u4_a;
    vec4u_h[1] += u4_b;
    vec4u_h[2] += u4_c;
    vec4u_h[3] += u4_d;
    vec4u_h[4] += u4_e;
  }

  Sha1Digest digest_out;
  for (int int4_i{0}; int4_i < 5; ++int4_i)
    for (int int4_j{0}; int4_j < 4; ++int4_j)
      digest_out.vec1u_bytes[static_cast<std::size_t>(int4_i) * 4u + static_cast<std::size_t>(int4_j)]
          = static_cast<std::uint8_t>((vec4u_h[int4_i] >> (8 * (3 - int4_j))) & 0xFFu);
  return digest_out;
}

namespace {

std::string bytes_to_hex_lower(std::span<const std::uint8_t> span1u_data) {
  constexpr std::string_view kHexDigits{"0123456789abcdef"};
  std::string str_out;
  str_out.resize(span1u_data.size() * 2);
  for (std::size_t sz_i{0}; sz_i < span1u_data.size(); ++sz_i) {
    str_out[sz_i * 2] = kHexDigits[span1u_data[sz_i] >> 4];
    str_out[sz_i * 2 + 1] = kHexDigits[span1u_data[sz_i] & 0x0Fu];
  }
  return str_out;
}

}

std::string sha1_hex(std::span<const std::uint8_t> span1u_data) {
  return bytes_to_hex_lower(sha1_digest(span1u_data).vec1u_bytes);
}

std::string sha1_hex(std::string_view str_data) {
  return sha1_hex(std::span<const std::uint8_t>{
      reinterpret_cast<const std::uint8_t*>(str_data.data()), str_data.size()});
}

}
