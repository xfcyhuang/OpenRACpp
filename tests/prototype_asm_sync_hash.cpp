// PROTOTYPE（wayfinder 工单 #23）—— 一次性原型测试，勿并入主分支。
// 黄金值来源：.NET 10 实跑上游 Sync.HashInt2/HashPlayer 同表达式（生成器见原型目录 README），
// CSV 清单 2026-11-03 生成，基线 1f594e320。
import std;
#include <limits>

#include "prototype/asm_sync_hash/sync_hash_proto.h"

namespace {

struct Int2Row {
  int x;
  int y;
  int expected;  // oracle：((x*5)^(y*3))/4，C# int32 语义
};

constexpr Int2Row kInt2Rows[] = {
    {0, 0, 0},
    {1, 1, 1},
    {2, 3, 0},
    {-1, 0, -1},  // 截断陷阱：天真 sar 得 -2
    {-3, -5, 0},
    {7, -13, -1},  // 截断陷阱：天真 sar 得 -2
    {-1, -1, 1},
    {-13, 7, -21},  // 截断陷阱：天真 sar 得 -22
    {std::numeric_limits<int>::max(), std::numeric_limits<int>::min(), -1},
    {std::numeric_limits<int>::min(), std::numeric_limits<int>::max(), 0},
    {123456789, -987654321, 451670305},
    {-987654321, 123456789, -202562323},
    {0x55555555, static_cast<int>(0xAAAAAAAB), -357913942},
    {100, -7, -120},
    {-100, 7, -121},
};

struct PlayerRow {
  unsigned int actor_id;
  int expected;  // oracle：(int)(id<<16)*0x567
};

constexpr PlayerRow kPlayerRows[] = {
    {0u, 0},
    {1u, 90636288},
    {2u, 181272576},
    {4660u, 1458307072},
    {4294967295u, -90636288},
    {2147483648u, 0},
    {2147483647u, -90636288},
    {3735928559u, 2099838976},
    {65535u, -90636288},
    {65536u, 0},
};

}  // namespace

int main() {
  int int4_failures{0};
  int int4_int2_checks{0};
  int int4_player_checks{0};
  int int4_trap_checks{0};

  // 1) HashInt2 黄金对拍
  for (const auto& row : kInt2Rows) {
    const int got = ora_proto_hash_int2(row.x, row.y);
    ++int4_int2_checks;
    if (got != row.expected) {
      ++int4_failures;
      std::println("asm_sync_hash_proto: check failed: hash_int2({}, {}) = {} 期望 {}", row.x, row.y, got, row.expected);
    }
  }

  // 2) HashPlayer 黄金对拍
  for (const auto& row : kPlayerRows) {
    const int got = ora_proto_hash_player(row.actor_id);
    ++int4_player_checks;
    if (got != row.expected) {
      ++int4_failures;
      std::println("asm_sync_hash_proto: check failed: hash_player({}) = {} 期望 {}", row.actor_id, got, row.expected);
    }
  }

  // 3) 陷阱确实存在：天真 sar 在被除数为负且不被 4 整除时必须与 C# 语义分叉（否则陷阱证明失效）；
  //    被除数为负但恰被 4 整除时 trunc==floor，两实现一致属正常。
  for (const auto& row : kInt2Rows) {
    const int naive = ora_proto_hash_int2_naive_sar(row.x, row.y);
    const int cs = ora_proto_hash_int2(row.x, row.y);
    ++int4_trap_checks;
    const int dividend = (row.x * 5) ^ (row.y * 3);  // 与 C# 同为 int32 回绕（-fwrapv）
    const bool b_must_diverge = dividend < 0 && (dividend & 3) != 0;
    if (b_must_diverge && naive == cs) {
      ++int4_failures;
      std::println("asm_sync_hash_proto: check failed: naive_sar({}, {}) 应与 C# 语义分叉（naive={} cs={}）", row.x, row.y, naive, cs);
    }
    if (!b_must_diverge && naive != cs) {
      ++int4_failures;
      std::println("asm_sync_hash_proto: check failed: naive_sar({}, {}) 该输入两实现应一致（naive={} cs={}）", row.x, row.y, naive, cs);
    }
  }

  // 展示陷阱实例（surface the state）
  std::println("asm_sync_hash_proto: 截断除法陷阱实例 hash_int2(-1,0)：C#={} 天真sar={} 调整后={}",
               ora_proto_hash_int2(-1, 0), ora_proto_hash_int2_naive_sar(-1, 0), ora_proto_hash_int2(-1, 0));

  const int int4_total = int4_int2_checks + int4_player_checks + int4_trap_checks;
  if (int4_failures != 0) {
    std::println("asm_sync_hash_proto: {}/{} checks failed", int4_failures, int4_total);
    return 1;
  }
  std::println("asm_sync_hash_proto: all {} checks passed (int2 {} + player {} + trap {})",
               int4_total, int4_int2_checks, int4_player_checks, int4_trap_checks);
  return 0;
}
