// UPSTREAM: OpenRA.Mods.Cnc/FileFormats/BlowfishKeyProvider.cs @7d57605 L19-491
// (全文逐语义;"direct C port" 的上游原话保持 —— 逐控制流照抄)
// mix 加密头的 80 字节密钥块 → 56 字节 Blowfish 密钥:固定公钥(42 字节
// DER,内嵌 base64)的 63..0 位 RSA 指数运算,小字节序 uint32 大数 + 16
// 位 limb 乘减 + 除法倒数的 Knuth D 商估计(GetMulWord)。所有指针算术
// (ushort*/uint* 视图、esi/edi 游标、GetMulWord 的 wn-1/wn-2 回看)按
// 上游 unsafe 代码逐行对译;数组定长 64/130/4 与上游一致。
// The mix encrypted header's 80-byte key block → the 56-byte Blowfish key:
// an RSA exponentiation with the fixed 42-byte DER public key (base64
// inlined) over little-endian uint32 bignums with 16-bit limb mul/sub and
// Knuth-D quotient estimation via the reciprocal (GetMulWord). All pointer
// arithmetic (the ushort*/uint* views, the esi/edi cursors, GetMulWord's
// wn-1/wn-2 lookbehind) is translated line for line from the upstream
// unsafe code; array lengths 64/130/4 match upstream.
#pragma once
import std;

namespace ora::fmt {

/// BlowfishKeyProvider(BlowfishKeyProvider.cs sealed class):仅
/// DecryptKey 公开。每实例独立(上游 readonly 成员字段同)。
/// BlowfishKeyProvider (the BlowfishKeyProvider.cs sealed class): only
/// DecryptKey is public. Each instance is independent (matching the
/// upstream readonly member fields).
class BlowfishKeyProvider final {
 public:
  /// DecryptKey(L485-489):src = mix 头的 80 字节密钥块;返回前 56 字节。
  /// DecryptKey (L485-489): src = the 80-byte key block from the mix
  /// header; returns the first 56 bytes.
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

}  // namespace ora::fmt
