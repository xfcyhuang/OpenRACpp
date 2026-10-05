// UPSTREAM: OpenRA.Mods.Cnc/FileFormats/LZOCompression.cs @7d57605 L49-291
// 实现文件:LZO1xDecompress 逐控制流照抄(标签 first_literal_run/
// match/match_done/eof_found 与 gtFirstLiteralRun/gtMatchDone 布尔保留);
// unsafe 指针 → uint8_t*;未对齐 32/16 位读写 → memcpy。
// Implementation file: LZO1xDecompress copied control flow for control
// flow (the first_literal_run/match/match_done/eof_found labels and the
// gtFirstLiteralRun/gtMatchDone booleans kept); unsafe pointers →
// uint8_t*; unaligned 32/16-bit accesses → memcpy.
#include "formats/lzo.hpp"

namespace ora::fmt::lzo {

namespace {

// 未对齐读(上游 *(uint*)p / *(ushort*)p;按小端字节序位拼 —— memcpy 在
// 本工程固定小端目标上与位拼逐字节等价,且无别名/对齐 UB)。
// Unaligned reads (upstream's *(uint*)p / *(ushort*)p; little-endian byte
// assembly — byte-for-byte equal to memcpy on this fixed little-endian
// target, without aliasing/alignment UB).
inline std::uint32_t ReadU32(const std::uint8_t* ptr_p) {
  return static_cast<std::uint32_t>(ptr_p[0]) | (static_cast<std::uint32_t>(ptr_p[1]) << 8) |
         (static_cast<std::uint32_t>(ptr_p[2]) << 16) | (static_cast<std::uint32_t>(ptr_p[3]) << 24);
}

inline void WriteU32(std::uint8_t* ptr_p, std::uint32_t uint4_v) {
  ptr_p[0] = static_cast<std::uint8_t>(uint4_v);
  ptr_p[1] = static_cast<std::uint8_t>(uint4_v >> 8);
  ptr_p[2] = static_cast<std::uint8_t>(uint4_v >> 16);
  ptr_p[3] = static_cast<std::uint8_t>(uint4_v >> 24);
}

inline std::uint16_t ReadU16(const std::uint8_t* ptr_p) {
  return static_cast<std::uint16_t>(static_cast<std::uint16_t>(ptr_p[0]) | (static_cast<std::uint16_t>(ptr_p[1]) << 8));
}

/// L271-274(MatchNext:字面量尾拷贝后预读下一控制字节)。
/// L271-274 (MatchNext: copies the literal tail, then pre-reads the next
/// control byte).
void MatchNext(std::uint8_t*& op_ref, const std::uint8_t*& ip_ref, std::uint32_t& t_ref) {
  do {
    *op_ref++ = *ip_ref++;
  } while (--t_ref > 0);
  t_ref = *ip_ref++;
}

/// L276-279(CopyMatch)。
/// L276-279 (CopyMatch).
void CopyMatch(std::uint8_t*& op_ref, const std::uint8_t*& m_pos_ref, std::uint32_t& t_ref) {
  *op_ref++ = *m_pos_ref++;
  *op_ref++ = *m_pos_ref++;
  do {
    *op_ref++ = *m_pos_ref++;
  } while (--t_ref > 0);
}

/// L51-279。指针与 t 的推进次序逐句对照 C# 版。
/// L51-279. Pointer and t advancement ordered statement by statement
/// against the C# version.
int LZO1xDecompress(const std::uint8_t* ptr_in, std::uint32_t uint4_in_len, std::uint8_t* ptr_out,
                    std::uint32_t& uint4_out_len) {
  std::uint8_t* op;
  const std::uint8_t* ip;
  std::uint32_t t;
  const std::uint8_t* mPos;
  const auto ip_end = ptr_in + uint4_in_len;
  uint4_out_len = 0;
  op = ptr_out;
  ip = ptr_in;
  auto gt_first_literal_run = false;
  auto gt_match_done = false;
  if (*ip > 17) {
    t = static_cast<std::uint32_t>(*ip++ - 17);
    if (t < 4)
      MatchNext(op, ip, t);
    else {
      do {
        *op++ = *ip++;
      } while (--t > 0);
      gt_first_literal_run = true;
    }
  }

  while (true) {
    if (gt_first_literal_run) {
      gt_first_literal_run = false;
      goto first_literal_run;
    }

    t = *ip++;
    if (t >= 16)
      goto match;

    if (t == 0) {
      while (*ip == 0) {
        t += 255;
        ip++;
      }

      t += static_cast<std::uint32_t>(15 + *ip++);
    }

    WriteU32(op, ReadU32(ip));
    op += 4;
    ip += 4;
    if (--t > 0) {
      if (t >= 4) {
        do {
          WriteU32(op, ReadU32(ip));
          op += 4;
          ip += 4;
          t -= 4;
        } while (t >= 4);

        if (t > 0)
          do {
            *op++ = *ip++;
          } while (--t > 0);
      } else
        do {
          *op++ = *ip++;
        } while (--t > 0);
    }

  first_literal_run:
    t = *ip++;
    if (t >= 16)
      goto match;

    mPos = op - (1 + 0x0800);
    mPos -= t >> 2;
    mPos -= *ip++ << 2;

    *op++ = *mPos++;
    *op++ = *mPos++;
    *op++ = *mPos;
    gt_match_done = true;

  match:
    do {
      if (gt_match_done) {
        gt_match_done = false;
        goto match_done;
      }

      if (t >= 64) {
        mPos = op - 1;
        mPos -= (t >> 2) & 7;
        mPos -= *ip++ << 3;
        t = (t >> 5) - 1;

        CopyMatch(op, mPos, t);
        goto match_done;
      } else if (t >= 32) {
        t &= 31;
        if (t == 0) {
          while (*ip == 0) {
            t += 255;
            ip++;
          }

          t += static_cast<std::uint32_t>(31 + *ip++);
        }

        mPos = op - 1;
        mPos -= ReadU16(ip) >> 2;
        ip += 2;
      } else if (t >= 16) {
        mPos = op;
        mPos -= (t & 8) << 11;
        t &= 7;
        if (t == 0) {
          while (*ip == 0) {
            t += 255;
            ip++;
          }

          t += static_cast<std::uint32_t>(7 + *ip++);
        }

        mPos -= ReadU16(ip) >> 2;
        ip += 2;
        if (mPos == op)
          goto eof_found;
        mPos -= 0x4000;
      } else {
        mPos = op - 1;
        mPos -= t >> 2;
        mPos -= *ip++ << 2;
        *op++ = *mPos++;
        *op++ = *mPos;
        goto match_done;
      }

      if (t >= 2 * 4 - (3 - 1) && op - mPos >= 4) {
        WriteU32(op, ReadU32(mPos));
        op += 4;
        mPos += 4;
        t -= 4 - (3 - 1);
        do {
          WriteU32(op, ReadU32(mPos));
          op += 4;
          mPos += 4;
          t -= 4;
        } while (t >= 4);

        if (t > 0)
          do {
            *op++ = *mPos++;
          } while (--t > 0);
      } else {
        // copy_match:
        *op++ = *mPos++;
        *op++ = *mPos++;
        do {
          *op++ = *mPos++;
        } while (--t > 0);
      }

    match_done:
      t = static_cast<std::uint32_t>(ip[-2] & 3);
      if (t == 0)
        break;

      // match_next:
      *op++ = *ip++;
      if (t > 1) {
        *op++ = *ip++;
        if (t > 2) {
          *op++ = *ip++;
        }
      }

      t = *ip++;
    } while (true);
  }

eof_found:
  uint4_out_len = static_cast<std::uint32_t>(op - ptr_out);
  return ip == ip_end ? 0 : (ip < ip_end ? (-8) : (-4));
}

}  // namespace

std::int32_t DecodeInto(std::span<const std::byte> vec_src, std::size_t st_src_offset, std::size_t st_src_length,
                        std::span<std::byte> vec_dest, std::size_t st_dest_offset, std::uint32_t& uint4_dest_length) {
  return LZO1xDecompress(reinterpret_cast<const std::uint8_t*>(vec_src.data()) + st_src_offset,
                         static_cast<std::uint32_t>(st_src_length),
                         reinterpret_cast<std::uint8_t*>(vec_dest.data()) + st_dest_offset, uint4_dest_length);
}

}  // namespace ora::fmt::lzo
