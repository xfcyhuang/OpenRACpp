// golden_core — Phase 0 定点原语黄金对拍测试(C++26 版)
//
// 以与 tools/golden_gen/Program.cs 严格一致的输入序列(独立 LCG)驱动 ora:: 实现,
// 逐行生成文本并与 C# 生成的黄金文件比对;任何一行不一致即失败。
// 黄金数据由上游 OpenRA 原语的 C# 逐字副本生成,与被测语言无关——是 Phase 0 验收关卡。
//
// 用法: golden_core <golden_core.txt>
import std;
#include <cstdio>  // stderr 为宏,不穿越 import 边界(规范允许的并用正形态)

#include "core/cell_pos.hpp"
#include "core/cvec.hpp"
#include "core/exts_math.hpp"
#include "core/int2.hpp"
#include "core/mersenne_twister.hpp"
#include "core/rectangle.hpp"
#include "core/wdist.hpp"
#include "core/wangle.hpp"
#include "core/wpos.hpp"
#include "core/wrot.hpp"
#include "core/wvec.hpp"

namespace {

// ———— 黄金文件逐行读取(整个文件驻留内存) ————
std::string str_golden;                 // 黄金文件全文
std::size_t size_golden_pos{0};         // 当前读取偏移
std::size_t size_golden_line{0};        // 当前行号(1 起)
std::int32_t int4_failures{0};          // 差异计数
std::size_t size_total{0};              // 已比对行数

/// 取下一行(剥离 \n 与 \r);文件耗尽返回 false
bool NextGoldenLine(std::string_view& sv_line) {
  if (size_golden_pos >= str_golden.size())
    return false;

  const char* ptr_start{str_golden.data() + size_golden_pos};
  const char* ptr_nl{static_cast<const char*>(
      std::memchr(ptr_start, '\n', str_golden.size() - size_golden_pos))};
  std::size_t size_len{ptr_nl ? static_cast<std::size_t>(ptr_nl - ptr_start)
                              : str_golden.size() - size_golden_pos};
  if (size_len > 0 && ptr_start[size_len - 1] == '\r')
    size_len--;

  size_golden_pos += ptr_nl ? size_len + 1 : size_len;
  size_golden_line++;
  sv_line = std::string_view{ptr_start, size_len};
  return true;
}

/// 比对一行:与黄金行完全一致则通过;超 10 处差异中止
void Check(std::string_view sv_line) {
  size_total++;
  std::string_view sv_golden;
  if (!NextGoldenLine(sv_golden)) {
    std::println("FAIL line {}: golden 已耗尽,C++ 多输出: {}", size_golden_line, sv_line);
    int4_failures++;
    return;
  }

  if (sv_line != sv_golden) {
    std::println("FAIL line {}:\n  golden: {}\n  c++   : {}", size_golden_line, sv_golden, sv_line);
    if (++int4_failures > 10) {
      std::println("…差异过多,中止");
      std::exit(1);
    }
  }
}

// ———— 与 C# 侧一致的独立 LCG 输入流 ————
std::uint64_t uint8_lcg{0x9E3779B97F4A7C15ULL};
std::int32_t Rand() {
  uint8_lcg = uint8_lcg * 6364136223846793005ULL + 1442695040888963407ULL;
  return static_cast<std::int32_t>(static_cast<std::uint32_t>(uint8_lcg >> 33));  // [0, 2^31)
}

}  // namespace

int main(int argc, char** argv_argv) {
  if (argc != 2) {
    std::println(stderr, "用法: golden_core <golden_core.txt>");
    return 2;
  }

  // 读入黄金文件
  std::ifstream ifs_golden{argv_argv[1], std::ios::binary};
  if (!ifs_golden) {
    std::println(stderr, "无法打开 golden 文件: {}", argv_argv[1]);
    return 2;
  }
  std::ostringstream oss_buf{};
  oss_buf << ifs_golden.rdbuf();
  str_golden = std::move(oss_buf).str();

  using namespace ora;

  // 1) ISqrt:0..4096 全量 + 2^k 边界
  for (std::uint32_t uint4_n{0}; uint4_n <= 4096; uint4_n++)
    Check(std::format("ISQ n={} v={}", uint4_n, ISqrt(uint4_n)));
  for (std::int32_t int4_k{0}; int4_k <= 32; int4_k++) {
    const std::uint64_t uint8_p{int4_k == 32 ? std::numeric_limits<std::uint64_t>::max()
                                             : (1ULL << int4_k)};
    Check(std::format("ISQ64 n={} v={}", uint8_p - 1, ISqrt(uint8_p - 1)));
    Check(std::format("ISQ64 n={} v={}", uint8_p, ISqrt(uint8_p)));
    Check(std::format("ISQ64 n={} v={}", uint8_p + 1, ISqrt(uint8_p + 1)));
  }

  // 2) WAngle 全量 sin/cos/tan
  for (std::int32_t int4_a{0}; int4_a < 1024; int4_a++) {
    const WAngle wa_ang{int4_a};
    Check(std::format("WA a={} sin={} cos={} tan={} facing={}", int4_a, wa_ang.Sin(),
                      wa_ang.Cos(), wa_ang.Tan(), wa_ang.Facing()));
  }

  // 3) FromFacing / FromDegrees
  for (std::int32_t int4_f{-8}; int4_f <= 8; int4_f++)
    Check(std::format("WAF f={} a={}", int4_f, WAngle::FromFacing(int4_f).Angle));
  for (std::int32_t int4_d{-720}; int4_d <= 720; int4_d += 7)
    Check(std::format("WAD d={} a={}", int4_d, WAngle::FromDegrees(int4_d).Angle));

  // 4) ArcSin / ArcCos 全量
  for (std::int32_t int4_d{-1024}; int4_d <= 1024; int4_d++) {
    Check(std::format("ASIN d={} a={}", int4_d, WAngle::ArcSin(int4_d).Angle));
    Check(std::format("ACOS d={} a={}", int4_d, WAngle::ArcCos(int4_d).Angle));
  }

  // 5) ArcTan 网格(含轴与特殊分支)
  for (std::int32_t int4_y{-613}; int4_y <= 613; int4_y += 61)
    for (std::int32_t int4_x{-613}; int4_x <= 613; int4_x += 61)
      Check(std::format("ATAN y={} x={} a={}", int4_y, int4_x, WAngle::ArcTan(int4_y, int4_x).Angle));
  for (std::int32_t int4_y{-2000000}; int4_y <= 2000000; int4_y += 1000000)
    for (std::int32_t int4_x{-2000000}; int4_x <= 2000000; int4_x += 1000000)
      Check(std::format("ATAN y={} x={} a={}", int4_y, int4_x, WAngle::ArcTan(int4_y, int4_x).Angle));

  // 6) WAngle.Lerp
  {
    constexpr std::array<int, 6> arr_muls{0, 1, 5, 13, 999, 1024};
    constexpr std::array<int, 4> arr_divs{1, 7, 1000, 1024};
    for (std::int32_t int4_a{0}; int4_a < 1024; int4_a += 97)
      for (std::int32_t int4_b{0}; int4_b < 1024; int4_b += 89)
        for (const std::int32_t int4_m : arr_muls)
          for (const std::int32_t int4_d : arr_divs)
            Check(std::format("WAL a={} b={} m={} d={} r={}", int4_a, int4_b, int4_m, int4_d,
                              WAngle::Lerp(WAngle{int4_a}, WAngle{int4_b}, int4_m, int4_d).Angle));
  }

  // 7) MersenneTwister
  {
    constexpr std::array<int, 3> arr_seeds{0, 1, 20250929};
    for (const std::int32_t int4_s : arr_seeds) {
      MersenneTwister mt_rng{int4_s};
      for (std::int32_t int4_i{0}; int4_i < 3000; int4_i++)
        Check(std::format("MTU s={} i={} v={}", int4_s, int4_i, mt_rng.NextUint()));
      for (std::int32_t int4_i{0}; int4_i < 1000; int4_i++)
        Check(std::format("MTN s={} i={} v={}", int4_s, int4_i, mt_rng.Next()));
      for (std::int32_t int4_i{0}; int4_i < 200; int4_i++)
        Check(std::format("MTUL s={} i={} v={}", int4_s, int4_i, mt_rng.NextUlong()));
      for (std::int32_t int4_i{0}; int4_i < 500; int4_i++) {
        const std::int32_t int4_lo{Rand() % 2001 - 1000};
        const std::int32_t int4_hi{int4_lo + static_cast<std::int32_t>(Rand() % 3000)};
        Check(std::format("MTR s={} i={} lo={} hi={} v={}", int4_s, int4_i, int4_lo, int4_hi,
                          mt_rng.Next(int4_lo, int4_hi)));
      }
      Check(std::format("MTSTATE s={} last={} total={}", int4_s, mt_rng.Last, mt_rng.TotalCount));
    }

    constexpr std::array<int, 5> w5{1, 2, 3, 0, 7};
    constexpr std::array<int, 3> w3{0, 0, 0};
    constexpr std::array<int, 1> w1{5};
    constexpr std::array<int, 10> w10{10, 10, 10, 10, 10, 10, 10, 10, 10, 10};
    for (const std::int32_t int4_s : arr_seeds) {
      MersenneTwister mt_rng{int4_s + 1000};
      for (std::int32_t int4_i{0}; int4_i < 500; int4_i++) {
        const std::int32_t int4_v{mt_rng.PickWeighted(
            int4_i % 4 == 0 ? std::span<const int>{w5}
            : int4_i % 4 == 1 ? std::span<const int>{w3}
            : int4_i % 4 == 2 ? std::span<const int>{w1}
                              : std::span<const int>{w10})};
        Check(std::format("MTPW s={} i={} v={}", int4_s, int4_i, int4_v));
      }
    }
  }

  // 8) WDist/WVec.FromPDF
  {
    MersenneTwister mt_rng{7};
    constexpr std::array<int, 5> arr_samples{1, 2, 3, 5, 10};
    for (std::int32_t int4_i{0}; int4_i < 20; int4_i++)
      for (const std::int32_t int4_n : arr_samples)
        Check(std::format("PDF i={} n={} v={}", int4_i, int4_n,
                          WDist::FromPDF(mt_rng, int4_n).Length));

    mt_rng.Seed(77);
    for (std::int32_t int4_i{0}; int4_i < 20; int4_i++) {
      const WVec v_pdf{WVec::FromPDF(mt_rng, 3)};
      Check(std::format("PDFV i={} x={} y={} z={}", int4_i, v_pdf.X, v_pdf.Y, v_pdf.Z));
    }
  }

  // 9) WVec/WPos 随机运算(值域贴近真实游戏,避开 90° 渐近线邻域)
  for (std::int32_t int4_i{0}; int4_i < 4000; int4_i++) {
    const std::int32_t int4_ax{Rand() % 200001 - 100000};
    const std::int32_t int4_ay{Rand() % 200001 - 100000};
    const std::int32_t int4_az{Rand() % 200001 - 100000};
    const std::int32_t int4_bx{Rand() % 200001 - 100000};
    const std::int32_t int4_by{Rand() % 200001 - 100000};
    const std::int32_t int4_bz{Rand() % 200001 - 100000};
    const std::int32_t int4_s{Rand() % 201 - 100};
    const std::int32_t int4_d{static_cast<std::int32_t>(Rand() % 100) + 1};
    const std::int32_t int4_m{
        static_cast<std::int32_t>(static_cast<std::uint32_t>(Rand()) % static_cast<std::uint32_t>(int4_d))};
    std::int32_t int4_p{static_cast<std::int32_t>(Rand() % 1024)};
    if ((int4_p & 511) >= 254 && (int4_p & 511) <= 258)
      int4_p = (int4_p + 8) & 1023;  // 避开 90°/270° 渐近线邻域

    const WVec v_a{int4_ax, int4_ay, int4_az};
    const WVec v_b{int4_bx, int4_by, int4_bz};
    const WVec v_add{v_a + v_b};
    const WVec v_sub{v_a - v_b};
    const WVec v_neg{-v_a};
    const WVec v_div{v_a / int4_d};
    const WVec v_mul{int4_s * v_a};
    Check(std::format(
        "WV i={} addx={} addy={} addz={} subx={} suby={} subz={} negx={} negy={} negz={} "
        "divx={} divy={} divz={} mulx={} muly={} mulz={}",
        int4_i, v_add.X, v_add.Y, v_add.Z, v_sub.X, v_sub.Y, v_sub.Z, v_neg.X, v_neg.Y, v_neg.Z,
        v_div.X, v_div.Y, v_div.Z, v_mul.X, v_mul.Y, v_mul.Z));
    Check(std::format("WVL i={} dot={} lsq={} len={} hlsq={} hlen={} vlsq={} vlen={} yaw={}",
                      int4_i, WVec::Dot(v_a, v_b), v_a.LengthSquared(), v_a.Length(),
                      v_a.HorizontalLengthSquared(), v_a.HorizontalLength(),
                      v_a.VerticalLengthSquared(), v_a.VerticalLength(), v_a.Yaw().Angle));

    const WVec v_ler{WVec::Lerp(v_a, v_b, int4_m, int4_d)};
    const WVec v_lq{WVec::LerpQuadratic(v_a, v_b, WAngle{int4_p}, int4_m, int4_d)};
    Check(std::format("WVLQ i={} lx={} ly={} lz={} qx={} qy={} qz={}", int4_i, v_ler.X, v_ler.Y,
                      v_ler.Z, v_lq.X, v_lq.Y, v_lq.Z));

    const WPos p_a{int4_ax, int4_ay, int4_az};
    const WPos p_b{int4_bx, int4_by, int4_bz};
    const WPos p_ler{WPos::Lerp(p_a, p_b, int4_m, int4_d)};
    const WPos p_lq{WPos::LerpQuadratic(p_a, p_b, WAngle{int4_p}, int4_m, int4_d)};
    Check(std::format("WPLQ i={} lx={} ly={} lz={} qx={} qy={} qz={}", int4_i, p_ler.X, p_ler.Y,
                      p_ler.Z, p_lq.X, p_lq.Y, p_lq.Z));

    const WVec v_r1{v_a.Rotate(WRot{WAngle{int4_p / 3 * 3}, WAngle{(int4_p + 128) / 3 * 3},
                                    WAngle{(int4_p + 256) / 3 * 3}})};
    const WVec v_r2{v_a.Rotate(WRot{WAngle{int4_p}, WAngle{int4_p}, WAngle{int4_p}})};
    Check(std::format("WVR i={} rx={} ry={} rz={}", int4_i, v_r1.X, v_r2.Y, v_r2.Z));
  }

  // 10) WRot 全网格构造 + 矩阵 + Rotate/Neg + SLerp
  for (std::int32_t int4_r{0}; int4_r < 1024; int4_r += 64)
    for (std::int32_t int4_p{0}; int4_p < 1024; int4_p += 64)
      for (std::int32_t int4_y{0}; int4_y < 1024; int4_y += 64) {
        const WRot rot_q{WAngle{int4_r}, WAngle{int4_p}, WAngle{int4_y}};
        const Int32Matrix4x4 mtx_m{rot_q.AsMatrix()};
        Check(std::format("WR r={} p={} y={} q={},{},{},{} m={},{},{},{},{},{},{},{},{},{}",
                          int4_r, int4_p, int4_y, rot_q.x, rot_q.y, rot_q.z, rot_q.w, mtx_m.M11,
                          mtx_m.M12, mtx_m.M13, mtx_m.M21, mtx_m.M22, mtx_m.M23, mtx_m.M31,
                          mtx_m.M32, mtx_m.M33, mtx_m.M44));
      }

  for (std::int32_t int4_r{0}; int4_r < 1024; int4_r += 97)
    for (std::int32_t int4_p{0}; int4_p < 1024; int4_p += 131) {
      const WRot r_a{WAngle{int4_r}, WAngle{int4_p}, WAngle{300}};
      const WRot r_b{WAngle{700}, WAngle{(int4_p + 100) % 1024}, WAngle{(int4_r + 50) % 1024}};
      const WRot r_rr{r_a.Rotate(r_b)};
      const WRot r_neg{-r_a};
      Check(std::format("WRR r={} p={} q={},{},{},{} e={},{},{} nq={},{},{},{} ne={},{},{}",
                        int4_r, int4_p, r_rr.x, r_rr.y, r_rr.z, r_rr.w, r_rr.Roll.Angle,
                        r_rr.Pitch.Angle, r_rr.Yaw.Angle, r_neg.x, r_neg.y, r_neg.z, r_neg.w,
                        r_neg.Roll.Angle, r_neg.Pitch.Angle, r_neg.Yaw.Angle));
    }

  for (std::int32_t int4_i{0}; int4_i < 2000; int4_i++) {
    const std::int32_t int4_r{Rand() % 1024}, int4_p{Rand() % 1024}, int4_y{Rand() % 1024};
    const std::int32_t int4_r2{Rand() % 1024}, int4_p2{Rand() % 1024}, int4_y2{Rand() % 1024};
    const std::int32_t int4_m{Rand() % 100}, int4_d{Rand() % 100 + 1};
    const WRot r_a{WAngle{int4_r}, WAngle{int4_p}, WAngle{int4_y}};
    const WRot r_b{WAngle{int4_r2}, WAngle{int4_p2}, WAngle{int4_y2}};
    const WRot r_s{WRot::SLerp(r_a, r_b, int4_m, int4_d)};
    Check(std::format("WRS i={} q={},{},{},{} e={},{},{}", int4_i, r_s.x, r_s.y, r_s.z, r_s.w,
                      r_s.Roll.Angle, r_s.Pitch.Angle, r_s.Yaw.Angle));
  }

  // 11) CPos/CVec/MPos 网格
  {
    constexpr std::array<std::uint8_t, 3> arr_layers{0, 1, 255};
    for (std::int32_t int4_x{-2048}; int4_x <= 2047; int4_x += 131)
      for (std::int32_t int4_y{-2048}; int4_y <= 2047; int4_y += 131)
        for (const std::uint8_t uint1_layer : arr_layers) {
          const CPos cell_c{int4_x, int4_y, uint1_layer};
          Check(std::format("CP x={} y={} l={} bits={} rx={} ry={} rl={}", int4_x, int4_y,
                            uint1_layer, cell_c.Bits, cell_c.X(), cell_c.Y(), cell_c.Layer()));
        }

    for (std::int32_t int4_x{-40}; int4_x <= 40; int4_x += 3)
      for (std::int32_t int4_y{-40}; int4_y <= 40; int4_y += 3) {
        const CPos cell_c{int4_x, int4_y};
        const MPos m_r{cell_c.ToMPos(MapGridType::Rectangular)};
        const MPos m_i{cell_c.ToMPos(MapGridType::RectangularIsometric)};
        const CPos b_r{m_r.ToCPos(MapGridType::Rectangular)};
        const CPos b_i{m_i.ToCPos(MapGridType::RectangularIsometric)};
        Check(std::format("CPM x={} y={} ru={} rv={} iu={} iv={} bx={} by={} rx={} ry={}", int4_x,
                          int4_y, m_r.U, m_r.V, m_i.U, m_i.V, b_i.X(), b_i.Y(), b_r.X(), b_r.Y()));
      }
  }

  // 12) CVec 运算采样
  for (std::int32_t int4_i{0}; int4_i < 2000; int4_i++) {
    const std::int32_t int4_ax{Rand() % 4001 - 2000};
    const std::int32_t int4_ay{Rand() % 4001 - 2000};
    const std::int32_t int4_bx{Rand() % 4001 - 2000};
    const std::int32_t int4_by{Rand() % 4001 - 2000};
    const std::int32_t int4_s{static_cast<std::int32_t>(Rand() % 50) + 1};
    const CVec c_a{int4_ax, int4_ay};
    const CVec c_b{int4_bx, int4_by};
    const CVec c_add{c_a + c_b};
    const CVec c_sub{c_a - c_b};
    const CVec c_mul{int4_s * c_a};
    const CVec c_div{c_a / int4_s};
    const CVec c_mx{CVec::Max(c_a, c_b)};
    const CVec c_mn{CVec::Min(c_a, c_b)};
    Check(std::format(
        "CV i={} ax={} ay={} bx={} by={} s={} addx={} addy={} subx={} suby={} mulx={} muly={} "
        "divx={} divy={} maxx={} maxy={} minx={} miny={} dot={} lsq={} len={} sgx={} sgy={} "
        "absx={} absy={}",
        int4_i, int4_ax, int4_ay, int4_bx, int4_by, int4_s, c_add.X, c_add.Y, c_sub.X, c_sub.Y,
        c_mul.X, c_mul.Y, c_div.X, c_div.Y, c_mx.X, c_mx.Y, c_mn.X, c_mn.Y, CVec::Dot(c_a, c_b),
        c_a.LengthSquared(), c_a.Length(), c_a.Sign().X, c_a.Sign().Y, c_a.Abs().X, c_a.Abs().Y));
  }

  // golden 是否有剩余行
  std::string_view sv_tail;
  if (NextGoldenLine(sv_tail) && !sv_tail.empty()) {
    std::println("FAIL: C++ 输出已结束但 golden 还有剩余(行 {} 起): {}", size_golden_line, sv_tail);
    int4_failures++;
  }

  if (int4_failures == 0) {
    std::println("golden_core: PASS  ({} 行全部一致)", size_total);
    return 0;
  }
  std::println("golden_core: FAIL  ({} 处差异 / {} 行)", int4_failures, size_total);
  return 1;
}
