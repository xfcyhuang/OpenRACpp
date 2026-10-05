// UPSTREAM: OpenRA.Mods.Cnc/FileFormats/Blowfish.cs @7d57605 L14-409(全文逐语义)
// 标准 Blowfish(ECB,63 字节密钥上限任意长):18 项 P 盒 + 4×256 S 盒逐值
// 照搬;密钥扩展与 16 轮 Feistel 控制流逐行等价(x 交错标志、Encrypt/Decrypt
// 的 P[0]/P[17] 首尾异或、输出 a/b 交换)。RunCipher 的 SwapBytes 大端字序
// 装卸照抄 —— 数据数组按小端 uint 读入后整体字节交换,与上游跨平台一致。
// 消费方:mix 加密头(TS/RA2 格式)。
// The standard Blowfish (ECB, any key length up to 63 bytes): the 18-entry
// P array + the 4×256 S boxes copied value for value; the key schedule and
// the 16-round Feistel control flow equivalent line for line (the x
// alternating flag, the leading/trailing P[0]/P[17] XORs of Encrypt/Decrypt,
// the output a/b swap). RunCipher's SwapBytes big-endian load/store is kept
// verbatim — data words are read as little-endian uints then byte-swapped
// whole, matching upstream across platforms. Consumer: the encrypted mix
// header (the TS/RA2 format).
#pragma once
import std;

namespace ora::fmt {

/// Blowfish(Blowfish.cs sealed class):块密码本体,按上游保持小写方法名
/// 语义。Encrypt/Decrypt 作用于 uint 数组(两 uint = 一块,大端装卸)。
/// Blowfish (the Blowfish.cs sealed class): the block cipher itself.
/// Encrypt/Decrypt operate on uint arrays (two uints per block, loaded and
/// stored big-endian).
class Blowfish {
 public:
  /// key 任意非空长度(上游按 key.Length 循环取字节)。
  /// Any non-empty key length (upstream cycles over key.Length bytes).
  explicit Blowfish(std::span<const std::byte> vec_key);

  /// RunCipher + Encrypt 委托(Blowfish.cs L46/L51-69):返回新数组。
  /// RunCipher with the Encrypt delegate (Blowfish.cs L46/L51-69): returns
  /// a fresh array.
  std::vector<std::uint32_t> Encrypt(std::span<const std::uint32_t> vec_data) const;
  /// RunCipher + Decrypt 委托(Blowfish.cs L47/L51-69)。
  /// RunCipher with the Decrypt delegate (Blowfish.cs L47/L51-69).
  std::vector<std::uint32_t> Decrypt(std::span<const std::uint32_t> vec_data) const;

 private:
  /// 上游实例方法 Encrypt(ref uint, ref uint)(L71-89)。
  /// The upstream instance Encrypt(ref uint, ref uint) (L71-89).
  void EncryptBlock(std::uint32_t& uint4_a, std::uint32_t& uint4_b) const;
  /// 上游实例方法 Decrypt(ref uint, ref uint)(L91-109)。
  /// The upstream instance Decrypt(ref uint, ref uint) (L91-109).
  void DecryptBlock(std::uint32_t& uint4_a, std::uint32_t& uint4_b) const;
  /// Round(ref a, b, n)(L121-124)。
  /// Round(ref a, b, n) (L121-124).
  void Round(std::uint32_t& uint4_a, std::uint32_t uint4_b, int int4_n) const;
  /// S(x, i)(L111-113)。
  /// S(x, i) (L111-113).
  std::uint32_t S(std::uint32_t uint4_x, int int4_i) const;
  /// ConvertBFtoF(x)(L116-118)。
  /// ConvertBFtoF(x) (L116-118).
  std::uint32_t ConvertBFtoF(std::uint32_t uint4_x) const;

  /// SwapBytes(i)(L126-131):32 位字节反转。
  /// SwapBytes(i) (L126-131): the 32-bit byte reversal.
  static constexpr std::uint32_t SwapBytes(std::uint32_t uint4_i) {
    uint4_i = (uint4_i << 16) | (uint4_i >> 16);
    uint4_i = ((uint4_i << 8) & 0xff00ff00) | ((uint4_i >> 8) & 0x00ff00ff);
    return uint4_i;
  }

  std::array<std::uint32_t, 18> arr_p_{};              // lookupMfromP / the P array
  std::array<std::array<std::uint32_t, 256>, 4> arr_s_{};  // lookupMfromS / the S boxes
};

}  // namespace ora::fmt
