#pragma once
import std;

namespace ora::fmt {

class Blowfish {
 public:

  explicit Blowfish(std::span<const std::byte> vec_key);

  std::vector<std::uint32_t> Encrypt(std::span<const std::uint32_t> vec_data) const;

  std::vector<std::uint32_t> Decrypt(std::span<const std::uint32_t> vec_data) const;

 private:

  void EncryptBlock(std::uint32_t& uint4_a, std::uint32_t& uint4_b) const;

  void DecryptBlock(std::uint32_t& uint4_a, std::uint32_t& uint4_b) const;

  void Round(std::uint32_t& uint4_a, std::uint32_t uint4_b, int int4_n) const;

  std::uint32_t S(std::uint32_t uint4_x, int int4_i) const;

  std::uint32_t ConvertBFtoF(std::uint32_t uint4_x) const;

  static constexpr std::uint32_t SwapBytes(std::uint32_t uint4_i) {
    uint4_i = (uint4_i << 16) | (uint4_i >> 16);
    uint4_i = ((uint4_i << 8) & 0xff00ff00) | ((uint4_i >> 8) & 0x00ff00ff);
    return uint4_i;
  }

  std::array<std::uint32_t, 18> arr_p_{};
  std::array<std::array<std::uint32_t, 256>, 4> arr_s_{};
};

}
