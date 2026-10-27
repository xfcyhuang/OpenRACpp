#include "formats/blowfish_key_provider.hpp"

namespace ora::fmt {

namespace {

constexpr std::string_view kPublicKeyString =
    "AihRvNoIbTn85FZRYNZRcT+i6KpU+maCsEqr3Q5q+LDB5tH7Tz2qQ38V";

std::vector<std::uint8_t> FromBase64(std::string_view str_in) {
  constexpr std::string_view sv_alphabet =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  const auto value_of = [&](char chr_c) -> unsigned {
    const std::size_t st_pos = sv_alphabet.find(chr_c);
    return st_pos == std::string_view::npos ? 0u : static_cast<unsigned>(st_pos);
  };

  std::vector<std::uint8_t> vec_out;
  unsigned uint_acc = 0;
  int int4_bits = 0;
  for (const char chr_c : str_in) {
    uint_acc = (uint_acc << 6) | value_of(chr_c);
    int4_bits += 6;
    if (int4_bits >= 8) {
      int4_bits -= 8;
      vec_out.push_back(static_cast<std::uint8_t>((uint_acc >> int4_bits) & 0xFF));
    }
  }
  return vec_out;
}

void InitBigNum(std::uint32_t* arr_n, std::uint32_t uint4_val, std::uint32_t uint4_len) {
  for (std::uint32_t uint4_i = 0; uint4_i < uint4_len; uint4_i++)
    arr_n[uint4_i] = 0;
  arr_n[0] = uint4_val;
}

void MoveKeyToBig(std::uint32_t* arr_n, const std::uint8_t* arr_key, std::uint32_t uint4_klen,
                  std::uint32_t uint4_blen) {
  const std::uint8_t uint1_sign = (arr_key[0] & 0x80) != 0 ? 0xff : 0x00;

  auto* pn = reinterpret_cast<std::uint8_t*>(arr_n);
  auto int4_i = static_cast<std::int64_t>(uint4_blen) * 4;
  for (; int4_i > uint4_klen; int4_i--)
    pn[int4_i - 1] = uint1_sign;
  for (; int4_i > 0; int4_i--)
    pn[int4_i - 1] = arr_key[uint4_klen - int4_i];
}

void KeyToBigNum(std::uint32_t* arr_n, const std::uint8_t* arr_key, std::uint32_t uint4_len) {
  std::uint32_t uint4_keylen;
  std::uint32_t uint4_j = 0;

  if (arr_key[uint4_j] != 2)
    return;
  uint4_j++;

  if ((arr_key[uint4_j] & 0x80) != 0) {
    uint4_keylen = 0;
    for (std::uint32_t uint4_i = 0; uint4_i < (arr_key[uint4_j] & 0x7f); uint4_i++)
      uint4_keylen = (uint4_keylen << 8) | arr_key[uint4_j + uint4_i + 1];
    uint4_j += (arr_key[uint4_j] & 0x7f) + 1;
  } else {
    uint4_keylen = arr_key[uint4_j];
    uint4_j++;
  }

  if (uint4_keylen <= uint4_len * 4)
    MoveKeyToBig(arr_n, arr_key + uint4_j, uint4_keylen, uint4_len);
}

std::uint32_t LenBigNum(const std::uint32_t* arr_n, std::uint32_t uint4_len) {
  if (uint4_len == 0)
    return 0;

  auto uint4_i = uint4_len;
  while (arr_n[--uint4_i] == 0)
    if (uint4_i == 0)
      return 0;

  return uint4_i + 1;
}

std::uint32_t BitLenBigNum(const std::uint32_t* arr_n, std::uint32_t uint4_len) {
  const std::uint32_t uint4_ddlen = LenBigNum(arr_n, uint4_len);
  if (uint4_ddlen == 0)
    return 0;
  std::uint32_t uint4_bitlen = uint4_ddlen * 32;
  std::uint32_t uint4_mask = 0x80000000;
  while ((uint4_mask & arr_n[uint4_ddlen - 1]) == 0) {
    uint4_mask >>= 1;
    uint4_bitlen--;
  }
  return uint4_bitlen;
}

int CompareBigNum(const std::uint32_t* arr_n1, const std::uint32_t* arr_n2, std::uint32_t uint4_len) {
  while (uint4_len > 0) {
    --uint4_len;
    if (arr_n1[uint4_len] < arr_n2[uint4_len])
      return -1;
    if (arr_n1[uint4_len] > arr_n2[uint4_len])
      return 1;
  }
  return 0;
}

void MoveBigNum(std::uint32_t* arr_dest, const std::uint32_t* arr_src, std::uint32_t uint4_len) {
  std::memcpy(arr_dest, arr_src, uint4_len * 4);
}

void ShrBigNum(std::uint32_t* arr_n, int int4_bits, int int4_len) {
  int int4_i;
  const int int4_i2 = int4_bits / 32;

  if (int4_i2 > 0) {
    for (int4_i = 0; int4_i < int4_len - int4_i2; int4_i++)
      arr_n[int4_i] = arr_n[int4_i + int4_i2];
    for (; int4_i < int4_len; int4_i++)
      arr_n[int4_i] = 0;
    int4_bits %= 32;
  }

  if (int4_bits == 0)
    return;
  for (int4_i = 0; int4_i < int4_len - 1; int4_i++)
    arr_n[int4_i] = (arr_n[int4_i] >> int4_bits) | (arr_n[int4_i + 1] << (32 - int4_bits));
  arr_n[int4_i] >>= int4_bits;
}

void ShlBigNum(std::uint32_t* arr_n, int int4_bits, int int4_len) {
  int int4_i;
  const int int4_i2 = int4_bits / 32;

  if (int4_i2 > 0) {
    for (int4_i = int4_len - 1; int4_i > int4_i2; int4_i--)
      arr_n[int4_i] = arr_n[int4_i - int4_i2];
    for (; int4_i > 0; int4_i--)
      arr_n[int4_i] = 0;
    int4_bits %= 32;
  }

  if (int4_bits == 0)
    return;
  for (int4_i = int4_len - 1; int4_i > 0; int4_i--)
    arr_n[int4_i] = (arr_n[int4_i] << int4_bits) | (arr_n[int4_i - 1] >> (32 - int4_bits));
  arr_n[0] <<= int4_bits;
}

std::uint32_t SubBigNum(std::uint32_t* arr_dest, const std::uint32_t* arr_src1,
                        const std::uint32_t* arr_src2, std::uint32_t uint4_carry, int int4_len) {
  std::uint32_t uint4_i1, uint4_i2;

  int4_len += int4_len;

  auto* ps1 = reinterpret_cast<const std::uint16_t*>(arr_src1);
  auto* ps2 = reinterpret_cast<const std::uint16_t*>(arr_src2);
  auto* pd = reinterpret_cast<std::uint16_t*>(arr_dest);

  while (--int4_len != -1) {
    uint4_i1 = *ps1++;
    uint4_i2 = *ps2++;
    *pd++ = static_cast<std::uint16_t>(uint4_i1 - uint4_i2 - uint4_carry);
    if (((uint4_i1 - uint4_i2 - uint4_carry) & 0x10000) != 0)
      uint4_carry = 1;
    else
      uint4_carry = 0;
  }

  return uint4_carry;
}

void InvertBigNum(std::uint32_t* arr_n1, const std::uint32_t* arr_n2, std::uint32_t uint4_len) {
  auto arr_n_tmp = std::array<std::uint32_t, 64>{};

  InitBigNum(arr_n_tmp.data(), 0, uint4_len);
  InitBigNum(arr_n1, 0, uint4_len);
  auto int4_n_two_bit_len = static_cast<int>(BitLenBigNum(arr_n2, uint4_len));
  auto uint4_bit = 1u << (int4_n_two_bit_len % 32);
  auto int4_j = (int4_n_two_bit_len + 32) / 32 - 1;
  const auto uint4_n_two_byte_len =
      static_cast<std::uint32_t>((int4_n_two_bit_len - 1) / 32) * 4;
  arr_n_tmp[uint4_n_two_byte_len / 4] |= 1u << ((int4_n_two_bit_len - 1) & 0x1f);

  while (int4_n_two_bit_len > 0) {
    int4_n_two_bit_len--;
    ShlBigNum(arr_n_tmp.data(), 1, static_cast<int>(uint4_len));
    if (CompareBigNum(arr_n_tmp.data(), arr_n2, uint4_len) != -1) {
      SubBigNum(arr_n_tmp.data(), arr_n_tmp.data(), arr_n2, 0, static_cast<int>(uint4_len));
      arr_n1[int4_j] |= uint4_bit;
    }

    uint4_bit >>= 1;
    if (uint4_bit == 0) {
      int4_j--;
      uint4_bit = 0x80000000;
    }
  }

  InitBigNum(arr_n_tmp.data(), 0, uint4_len);
}

void IncrementBigNum(std::uint32_t* arr_n, std::uint32_t uint4_len) {
  std::uint32_t uint4_i = 0;
  while ((++arr_n[uint4_i] == 0) && (--uint4_len > 0))
    uint4_i++;
}

void MulBignumWord(std::uint16_t* pn1, const std::uint32_t* arr_n2, std::uint32_t uint4_mul,
                   std::uint32_t uint4_len) {
  const auto* pn2 = reinterpret_cast<const std::uint16_t*>(arr_n2);

  std::uint32_t uint4_tmp = 0;
  for (std::uint32_t uint4_i = 0; uint4_i < uint4_len; uint4_i++) {
    uint4_tmp = uint4_mul * *pn2 + *pn1 + uint4_tmp;
    *pn1 = static_cast<std::uint16_t>(uint4_tmp);
    pn1++;
    pn2++;
    uint4_tmp >>= 16;
  }

  *pn1 += static_cast<std::uint16_t>(uint4_tmp);
}

void MulBigNum(std::uint32_t* arr_dest, const std::uint32_t* arr_src1, const std::uint32_t* arr_src2,
               std::uint32_t uint4_len) {
  auto* psrc2 = reinterpret_cast<const std::uint16_t*>(arr_src2);
  auto* pdest = reinterpret_cast<std::uint16_t*>(arr_dest);

  InitBigNum(arr_dest, 0, uint4_len * 2);
  for (std::uint32_t uint4_i = 0; uint4_i < uint4_len * 2; uint4_i++)
    MulBignumWord(pdest++, arr_src1, *psrc2++, uint4_len * 2);
}

void NotBigNum(std::uint32_t* arr_n, std::uint32_t uint4_len) {
  for (std::uint32_t uint4_i = 0; uint4_i < uint4_len; uint4_i++)
    arr_n[uint4_i] = ~arr_n[uint4_i];
}

void NegBigNum(std::uint32_t* arr_n, std::uint32_t uint4_len) {
  NotBigNum(arr_n, uint4_len);
  IncrementBigNum(arr_n, uint4_len);
}

void DecBigNum(std::uint32_t* arr_n, std::uint32_t uint4_len) {
  std::uint32_t uint4_i = 0;
  while ((--arr_n[uint4_i] == 0xffffffff) && (--uint4_len > 0))
    uint4_i++;
}

}

void BlowfishKeyProvider::InitPublicKey() {
  InitBigNum(rec_pubkey_.arr_keyTwo.data(), 0x10001, 64);

  const std::vector<std::uint8_t> vec_key = FromBase64(kPublicKeyString);
  KeyToBigNum(rec_pubkey_.arr_keyOne.data(), vec_key.data(), 64);
  rec_pubkey_.uint4_len = BitLenBigNum(rec_pubkey_.arr_keyOne.data(), 64) - 1;
}

void BlowfishKeyProvider::InitTwoDw(const std::uint32_t* arr_n, std::uint32_t uint4_len) {
  MoveBigNum(arr_globOne_.data(), arr_n, uint4_len);
  uint4_globOneBitLen_ = BitLenBigNum(arr_globOne_.data(), uint4_len);
  uint4_globOneLenXTwo_ = (uint4_globOneBitLen_ + 15) / 16;

  MoveBigNum(arr_globOneHigh_.data(),
             arr_globOne_.data() + LenBigNum(arr_globOne_.data(), uint4_len) - 2, 2);
  uint4_globOneHighBitLen_ = BitLenBigNum(arr_globOneHigh_.data(), 2) - 32;
  ShrBigNum(arr_globOneHigh_.data(), static_cast<int>(uint4_globOneHighBitLen_), 2);
  InvertBigNum(arr_globOneHighInv_.data(), arr_globOneHigh_.data(), 2);
  ShrBigNum(arr_globOneHighInv_.data(), 1, 2);
  uint4_globOneHighBitLen_ = (uint4_globOneHighBitLen_ + 15) % 16 + 1;
  IncrementBigNum(arr_globOneHighInv_.data(), 2);
  if (BitLenBigNum(arr_globOneHighInv_.data(), 2) > 32) {
    ShrBigNum(arr_globOneHighInv_.data(), 1, 2);
    uint4_globOneHighBitLen_--;
  }

  uint4_globOneHighInvLow_ = static_cast<std::uint16_t>(arr_globOneHighInv_[0]);
  uint4_globOneHighInvHigh_ = static_cast<std::uint16_t>(arr_globOneHighInv_[0] >> 16);
}

std::uint32_t BlowfishKeyProvider::GetMulWord(const std::uint32_t* ptr_n) {
  const auto* wn = reinterpret_cast<const std::uint16_t*>(ptr_n);

  const std::uint64_t uint8_a = ((((wn[-1] ^ 0xffff) & 0xffff) * 1ULL * uint4_globOneHighInvLow_ + 0x10000) >> 1);
  const std::uint64_t uint8_b = (((wn[-2] ^ 0xffff) * 1ULL * uint4_globOneHighInvHigh_ + uint4_globOneHighInvHigh_) >> 1);
  const std::uint64_t uint8_c = ((uint8_a + uint8_b + 1) >> 16) +
                                ((((wn[-1] ^ 0xffff) & 0xffff) * 1ULL * uint4_globOneHighInvHigh_) >> 1) +
                                (((*wn ^ 0xffff) * 1ULL * uint4_globOneHighInvLow_) >> 1) + 1;
  const std::uint64_t uint8_d = (uint8_c >> 14) + uint4_globOneHighInvHigh_ * 1ULL * (*wn ^ 0xffff) * 2;
  const auto uint8_i = static_cast<std::uint32_t>(uint8_d >> static_cast<int>(uint4_globOneHighBitLen_));

  if (uint8_i > 0xffff)
    return 0xffff;
  return uint8_i & 0xffff;
}

void BlowfishKeyProvider::CalcBigNum(std::uint32_t* arr_n1, const std::uint32_t* arr_n2,
                                     const std::uint32_t* arr_n3, std::uint32_t uint4_len) {
  std::uint32_t uint4_glob_two_x_two, uint4_len_diff;

  auto* g1 = arr_globOne_.data();
  auto* g2 = arr_globTwo_.data();

  MulBigNum(g2, arr_n2, arr_n3, uint4_len);
  g2[uint4_len * 2] = 0;
  uint4_glob_two_x_two = LenBigNum(g2, uint4_len * 2 + 1) * 2;
  if (uint4_glob_two_x_two >= uint4_globOneLenXTwo_) {
    IncrementBigNum(g2, uint4_len * 2 + 1);
    NegBigNum(g2, uint4_len * 2 + 1);
    uint4_len_diff = uint4_glob_two_x_two + 1 - uint4_globOneLenXTwo_;
    auto* esi = reinterpret_cast<std::uint16_t*>(g2) + (1 + uint4_glob_two_x_two - uint4_globOneLenXTwo_);
    auto* edi = reinterpret_cast<std::uint16_t*>(g2) + (uint4_glob_two_x_two + 1);
    for (; uint4_len_diff != 0; uint4_len_diff--) {
      edi--;
      const auto uint4_tmp = GetMulWord(reinterpret_cast<std::uint32_t*>(edi));
      esi--;
      if (uint4_tmp > 0) {
        MulBignumWord(esi, g1, uint4_tmp, 2 * uint4_len);
        if ((*edi & 0x8000) == 0 &&
            SubBigNum(reinterpret_cast<std::uint32_t*>(esi), reinterpret_cast<std::uint32_t*>(esi), g1,
                      0, static_cast<int>(uint4_len)) != 0)
          (*edi)--;
      }
    }

    NegBigNum(g2, uint4_len);
    DecBigNum(g2, uint4_len);
  }

  MoveBigNum(arr_n1, g2, uint4_len);
}

void BlowfishKeyProvider::ClearTempVars(std::uint32_t uint4_len) {
  InitBigNum(arr_globOne_.data(), 0, uint4_len);
  InitBigNum(arr_globTwo_.data(), 0, uint4_len);
  InitBigNum(arr_globOneHighInv_.data(), 0, 4);
  InitBigNum(arr_globOneHigh_.data(), 0, 4);
  uint4_globOneBitLen_ = 0;
  uint4_globOneHighBitLen_ = 0;
  uint4_globOneLenXTwo_ = 0;
  uint4_globOneHighInvLow_ = 0;
  uint4_globOneHighInvHigh_ = 0;
}

void BlowfishKeyProvider::CalcKey(std::uint32_t* arr_n1, const std::uint32_t* arr_n2,
                                  const std::uint32_t* arr_n3, const std::uint32_t* arr_n4,
                                  std::uint32_t uint4_len) {
  auto arr_n_tmp = std::array<std::uint32_t, 64>{};

  const std::uint32_t* pn3 = arr_n3;

  InitBigNum(arr_n1, 1, uint4_len);
  const std::uint32_t uint4_n4_len = LenBigNum(arr_n4, uint4_len);
  InitTwoDw(arr_n4, uint4_n4_len);
  auto int4_n3_bitlen = static_cast<int>(BitLenBigNum(arr_n3, uint4_n4_len));
  const auto uint4_n3_len = static_cast<std::uint32_t>((int4_n3_bitlen + 31) / 32);
  auto uint4_bit_mask = (1u << ((int4_n3_bitlen - 1) % 32)) >> 1;
  pn3 += uint4_n3_len - 1;
  int4_n3_bitlen--;
  MoveBigNum(arr_n1, arr_n2, uint4_n4_len);
  while (--int4_n3_bitlen != -1) {
    if (uint4_bit_mask == 0) {
      uint4_bit_mask = 0x80000000;
      pn3--;
    }

    CalcBigNum(arr_n_tmp.data(), arr_n1, arr_n1, uint4_n4_len);
    if ((*pn3 & uint4_bit_mask) != 0)
      CalcBigNum(arr_n1, arr_n_tmp.data(), arr_n2, uint4_n4_len);
    else
      MoveBigNum(arr_n1, arr_n_tmp.data(), uint4_n4_len);
    uint4_bit_mask >>= 1;
  }

  InitBigNum(arr_n_tmp.data(), 0, uint4_n4_len);
  ClearTempVars(uint4_len);
}

std::array<std::uint8_t, 256> BlowfishKeyProvider::ProcessPredata(std::span<const std::byte> vec_src) {
  auto arr_dest = std::array<std::uint8_t, 256>{};
  auto arr_n2 = std::array<std::uint32_t, 64>{};
  auto arr_n3 = std::array<std::uint32_t, 64>{};

  const auto int4_a = static_cast<int>((rec_pubkey_.uint4_len - 1) / 8);
  auto int4_pre_len = (55 / int4_a + 1) * (int4_a + 1);
  auto int4_src_offset = 0;
  auto int4_dest_offset = 0;

  while (int4_a + 1 <= int4_pre_len) {
    InitBigNum(arr_n2.data(), 0, 64);

    std::memcpy(arr_n2.data(), vec_src.data() + int4_src_offset, static_cast<std::size_t>(int4_a) + 1);
    CalcKey(arr_n3.data(), arr_n2.data(), rec_pubkey_.arr_keyTwo.data(),
            rec_pubkey_.arr_keyOne.data(), 64);
    std::memcpy(arr_dest.data() + int4_dest_offset, arr_n3.data(), static_cast<std::size_t>(int4_a));

    int4_pre_len -= int4_a + 1;
    int4_src_offset += int4_a + 1;
    int4_dest_offset += int4_a;
  }

  return arr_dest;
}

std::array<std::uint8_t, 56> BlowfishKeyProvider::DecryptKey(std::span<const std::byte> vec_src) {
  InitPublicKey();
  const auto arr_dest = ProcessPredata(vec_src);

  auto arr_key = std::array<std::uint8_t, 56>{};
  std::ranges::copy_n(arr_dest.begin(), 56, arr_key.begin());
  return arr_key;
}

}
