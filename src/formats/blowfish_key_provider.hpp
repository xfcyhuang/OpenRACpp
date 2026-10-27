#pragma once
import std;

namespace ora::fmt {

class BlowfishKeyProvider final {
 public:

  std::array<std::uint8_t, 56> DecryptKey(std::span<const std::byte> vec_src);

 private:
  struct PublicKey {
    std::array<std::uint32_t, 64> arr_keyOne{};
    std::array<std::uint32_t, 64> arr_keyTwo{};
    std::uint32_t uint4_len{};
  };

  void InitPublicKey();
  std::array<std::uint8_t, 256> ProcessPredata(std::span<const std::byte> vec_src);
  void InitTwoDw(const std::uint32_t* arr_n, std::uint32_t uint4_len);
  void CalcBigNum(std::uint32_t* arr_n1, const std::uint32_t* arr_n2, const std::uint32_t* arr_n3,
                  std::uint32_t uint4_len);
  void CalcKey(std::uint32_t* arr_n1, const std::uint32_t* arr_n2, const std::uint32_t* arr_n3,
               const std::uint32_t* arr_n4, std::uint32_t uint4_len);
  void ClearTempVars(std::uint32_t uint4_len);
  std::uint32_t GetMulWord(const std::uint32_t* ptr_n);

  PublicKey rec_pubkey_{};
  std::array<std::uint32_t, 64> arr_globOne_{};
  std::uint32_t uint4_globOneBitLen_ = 0, uint4_globOneLenXTwo_ = 0;
  std::array<std::uint32_t, 130> arr_globTwo_{};
  std::array<std::uint32_t, 4> arr_globOneHigh_{};
  std::array<std::uint32_t, 4> arr_globOneHighInv_{};
  std::uint32_t uint4_globOneHighBitLen_ = 0;
  std::uint32_t uint4_globOneHighInvLow_ = 0, uint4_globOneHighInvHigh_ = 0;
};

}
