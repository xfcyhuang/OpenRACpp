// core_test — core 基础设施单测:percent_modifiers(decimal 语义精确复刻)与
// arena(FrameArena/WorldArena,PORTING_PLAN §4.5 内存分区)。
// 期望值来源:ApplyPercentageModifiers 的期望按 C# decimal 全精度手算
// (Util.cs L203-211:链式精确有理数 +(int) 向零截断);关键分歧用例
// {60,60}×3 区分"每步整数早截断"与"decimal 全精度"(1.08→1 而非 0)。
// 黄金数据级对拍(程序化边界矩阵)登记于 docs/OPTIMIZATION_TRACKER.md OPT-A1,
// 随 Phase 5 的 golden_gen 扩展落地。
// core_test — core infrastructure unit tests: percent_modifiers (exact
// reproduction of the decimal semantics) and arena (FrameArena/WorldArena,
// the PORTING_PLAN §4.5 memory regions).
// Expected values: hand-computed with full C# decimal precision for
// ApplyPercentageModifiers (Util.cs L203-211: exact rational chain + (int)
// truncation towards zero); the pivotal divergence case {60,60}×3 separates
// "per-step integer early truncation" from "full decimal precision"
// (1.08 → 1, not 0). A golden-data-grade differential (programmatic boundary
// matrix) is registered as OPT-A1 in docs/OPTIMIZATION_TRACKER.md and lands
// with the Phase 5 golden_gen extension.
import std;
#include <cstdio>  // stderr 为宏,不穿越 import 边界(规范允许的并用正形态) | stderr is a macro; the spec-sanctioned mixed form

#include "core/arena.hpp"
#include "core/percent_modifiers.hpp"

namespace {

std::int32_t int4_failures = 0;

#define ORA_CHECK(cond)                                                        \
  do {                                                                         \
    if (!(cond)) {                                                             \
      std::println(stderr, "FAIL {}:{} {}", __FILE__, __LINE__, #cond);       \
      ++int4_failures;                                                         \
    }                                                                          \
  } while (0)

// ———— ApplyPercentageModifiers:decimal 语义 ————
// ———— ApplyPercentageModifiers: decimal semantics ————

// span 尚无 initializer_list 直构,测试经此辅助转发(std::span(it, n) 构造)
// span cannot yet be directly constructed from a braced list; tests forward
// through this helper (std::span(it, n) constructor).
constexpr std::int32_t Apm(std::int32_t int4_number, std::initializer_list<std::int32_t> list_percentages) {
  return ora::ApplyPercentageModifiers(int4_number,
                                       std::span<const std::int32_t>(list_percentages.begin(), list_percentages.size()));
}

void TestPercentModifiers() {
  using ora::ApplyPercentageModifiers;

  // 恒等与空链 | identity and the empty chain
  ORA_CHECK(Apm(42, {}) == 42);
  ORA_CHECK(Apm(-42, {}) == -42);
  ORA_CHECK(Apm(42, {100}) == 42);

  // 单项截断(向零):(int)(5 × 0.5m) = 2;(int)(-5 × 0.5m) = -2
  // Single-item truncation (towards zero).
  ORA_CHECK(Apm(5, {50}) == 2);
  ORA_CHECK(Apm(-5, {50}) == -2);
  ORA_CHECK(Apm(3, {200}) == 6);
  ORA_CHECK(Apm(7, {-50}) == -3);
  ORA_CHECK(Apm(9, {-100}) == -9);

  // 链式全精度:3 × 0.6 × 0.6 = 1.08 → 1(每步早截断会得 3→1→0,是本函数的语义底线)
  // Chained full precision: 3 × 0.6 × 0.6 = 1.08 → 1 (per-step early truncation
  // would give 3→1→0 — the semantic floor of this function).
  ORA_CHECK(Apm(3, {60, 60}) == 1);

  // 7 × 0.5³ = 0.875 → 0;100 × 1.5 × 0.5 = 75;10 × 0.25³ = 0.15625 → 0
  ORA_CHECK(Apm(7, {50, 50, 50}) == 0);
  ORA_CHECK(Apm(100, {150, 50}) == 75);
  ORA_CHECK(Apm(10, {25, 25, 25}) == 0);

  // 实践形值域(上游调用点典型量级)| practical value domain (typical magnitudes at upstream call sites)
  ORA_CHECK(Apm(768, {85}) == 652);         // 768×0.85 = 652.8 → 652
  ORA_CHECK(Apm(5120, {110, 90}) == 5068);  // 5120×0.99×... = 5068.8 → 5068
  ORA_CHECK(Apm(-999, {33, 33, 33}) == -35);  // -999×0.035937 = -35.921 → -35

  // int32 边界 | int32 boundaries
  ORA_CHECK(Apm(std::numeric_limits<std::int32_t>::max(), {100}) ==
            std::numeric_limits<std::int32_t>::max());
  ORA_CHECK(Apm(std::numeric_limits<std::int32_t>::min(), {50}) == -1073741824);

  // constexpr 可用性(描述表/模板元编程场景)| constexpr usability (descriptor-table/metaprogramming contexts)
  static constexpr std::int32_t arr_pct_6060[]{60, 60};
  static_assert(ora::ApplyPercentageModifiers(3, std::span<const std::int32_t>(arr_pct_6060)) == 1);
  static constexpr std::int32_t arr_pct_50[]{50};
  static_assert(ora::ApplyPercentageModifiers(5, std::span<const std::int32_t>(arr_pct_50)) == 2);
}

// ———— FrameArena ————

struct PodItem {  // 平凡可析构的帧临时 | trivially destructible frame transient
  std::int32_t int4_x;
  std::int64_t int8_y;
};

void TestFrameArena() {
  ora::FrameArena arena_frame(1024);

  // 基本构造与数组 | basic construction and arrays
  PodItem* item_a = arena_frame.Create<PodItem>(std::int32_t{11}, std::int64_t{22});
  ORA_CHECK(item_a->int4_x == 11 && item_a->int8_y == 22);
  PodItem* vec_items = arena_frame.CreateArray<PodItem>(8);
  for (std::size_t i = 0; i < 8; ++i)
    vec_items[i] = PodItem{std::int32_t(i), std::int64_t(i) * 7};
  ORA_CHECK(vec_items[7].int8_y == 49);

  // 对齐保证 | alignment guarantees
  void* ptr_aligned = arena_frame.Allocate(16, 64);
  ORA_CHECK(reinterpret_cast<std::uintptr_t>(ptr_aligned) % 64 == 0);

  // Mark/Rewind 水位 | Mark/Rewind watermarks
  const std::size_t size_mark = arena_frame.Mark();
  [[maybe_unused]] PodItem* item_temp = arena_frame.Create<PodItem>(1, 2);
  ORA_CHECK(arena_frame.Mark() > size_mark);
  arena_frame.Rewind(size_mark);
  ORA_CHECK(arena_frame.Mark() == size_mark);
  ORA_CHECK(item_a->int4_x == 11);  // 回退不破坏既有对象 | rewind corrupts no live object

  // 超首块增长后的分配与 Reset 复用 | growth past the first chunk, then Reset reuse
  [[maybe_unused]] PodItem* vec_big = arena_frame.CreateArray<PodItem>(512);  // 512×16B > 1024B 首块 | exceeds the 1 KiB first chunk
  ORA_CHECK(arena_frame.size_used() > 1024);
  arena_frame.Reset();
  ORA_CHECK(arena_frame.size_used() == 0);
  PodItem* item_after = arena_frame.Create<PodItem>(5, 6);
  ORA_CHECK(item_after->int4_x == 5);
}

// ———— WorldArena ————

std::int32_t int4_dtor_log[16];
std::int32_t int4_dtor_count;

struct Tracked {  // 非平凡析构:验证登记/逆序/幂等 | non-trivial dtor: registration/reverse order/idempotence
  std::int32_t int4_tag;
  explicit Tracked(std::int32_t t) : int4_tag(t) {}
  ~Tracked() { int4_dtor_log[int4_dtor_count++] = int4_tag; }
};

struct Plain {  // 平凡可析构:零登记开销 | trivially destructible: zero registration
  std::int32_t int4_v;
};

void TestWorldArena() {
  int4_dtor_count = 0;
  ora::WorldArena arena_world(1024);

  // 平凡类型直接 bump | trivial types bump directly
  Plain* obj_plain = arena_world.Create<Plain>(7);
  ORA_CHECK(obj_plain->int4_v == 7);

  // 非平凡类型:构造序登记,reset 逆序析构 | non-trivial: registered in
  // construction order, destroyed in reverse at reset
  Tracked* obj_t1 = arena_world.Create<Tracked>(1);
  Tracked* obj_t2 = arena_world.Create<Tracked>(2);
  Tracked* obj_t3 = arena_world.Create<Tracked>(3);
  ORA_CHECK(obj_t1->int4_tag == 1 && obj_t3->int4_tag == 3);

  // 显式 Destroy:立即析构 + 幂等 + reset 不重复
  // Explicit Destroy: immediate, idempotent, not repeated at reset.
  arena_world.Destroy(obj_t2);
  ORA_CHECK(int4_dtor_count == 1 && int4_dtor_log[0] == 2);
  arena_world.Destroy(obj_t2);  // 双重销毁必须被登记表吸收 | double destruction must be absorbed by the record table
  ORA_CHECK(int4_dtor_count == 1);

  arena_world.Reset();  // 逆序:3 先于 1 | reverse order: 3 before 1
  ORA_CHECK(int4_dtor_count == 3);
  ORA_CHECK(int4_dtor_log[1] == 3 && int4_dtor_log[2] == 1);
  ORA_CHECK(arena_world.size_used() == 0);

  // Reset 后复用(水位归零,chunk 保留)| reuse after Reset (watermark zeroed, chunks kept)
  Tracked* obj_reuse = arena_world.Create<Tracked>(9);
  ORA_CHECK(obj_reuse->int4_tag == 9);
  arena_world.Reset();
  ORA_CHECK(int4_dtor_count == 4 && int4_dtor_log[3] == 9);

  // Release 后仍可继续使用(哨兵块)| still usable after Release (sentinel chunk)
  arena_world.Release();
  Plain* obj_final = arena_world.Create<Plain>(1);
  ORA_CHECK(obj_final->int4_v == 1);
}

}  // namespace

int main() {
  TestPercentModifiers();
  TestFrameArena();
  TestWorldArena();

  if (int4_failures != 0) {
    std::println(stderr, "core_test: {} 项失败 | {} failure(s)", int4_failures, int4_failures);
    return 1;
  }
  std::println("core_test: PASS(percent_modifiers + arena)");
  return 0;
}
