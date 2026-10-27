import std;
#include <cstdio>

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

constexpr std::int32_t Apm(std::int32_t int4_number, std::initializer_list<std::int32_t> list_percentages) {
  return ora::ApplyPercentageModifiers(int4_number,
                                       std::span<const std::int32_t>(list_percentages.begin(), list_percentages.size()));
}

void TestPercentModifiers() {
  using ora::ApplyPercentageModifiers;

  ORA_CHECK(Apm(42, {}) == 42);
  ORA_CHECK(Apm(-42, {}) == -42);
  ORA_CHECK(Apm(42, {100}) == 42);

  ORA_CHECK(Apm(5, {50}) == 2);
  ORA_CHECK(Apm(-5, {50}) == -2);
  ORA_CHECK(Apm(3, {200}) == 6);
  ORA_CHECK(Apm(7, {-50}) == -3);
  ORA_CHECK(Apm(9, {-100}) == -9);

  ORA_CHECK(Apm(3, {60, 60}) == 1);

  ORA_CHECK(Apm(7, {50, 50, 50}) == 0);
  ORA_CHECK(Apm(100, {150, 50}) == 75);
  ORA_CHECK(Apm(10, {25, 25, 25}) == 0);

  ORA_CHECK(Apm(768, {85}) == 652);
  ORA_CHECK(Apm(5120, {110, 90}) == 5068);
  ORA_CHECK(Apm(-999, {33, 33, 33}) == -35);

  ORA_CHECK(Apm(std::numeric_limits<std::int32_t>::max(), {100}) ==
            std::numeric_limits<std::int32_t>::max());
  ORA_CHECK(Apm(std::numeric_limits<std::int32_t>::min(), {50}) == -1073741824);

  static constexpr std::int32_t arr_pct_6060[]{60, 60};
  static_assert(ora::ApplyPercentageModifiers(3, std::span<const std::int32_t>(arr_pct_6060)) == 1);
  static constexpr std::int32_t arr_pct_50[]{50};
  static_assert(ora::ApplyPercentageModifiers(5, std::span<const std::int32_t>(arr_pct_50)) == 2);
}

struct PodItem {
  std::int32_t int4_x;
  std::int64_t int8_y;
};

void TestFrameArena() {
  ora::FrameArena arena_frame(1024);

  PodItem* item_a = arena_frame.Create<PodItem>(std::int32_t{11}, std::int64_t{22});
  ORA_CHECK(item_a->int4_x == 11 && item_a->int8_y == 22);
  PodItem* vec_items = arena_frame.CreateArray<PodItem>(8);
  for (std::size_t i = 0; i < 8; ++i)
    vec_items[i] = PodItem{std::int32_t(i), std::int64_t(i) * 7};
  ORA_CHECK(vec_items[7].int8_y == 49);

  void* ptr_aligned = arena_frame.Allocate(16, 64);
  ORA_CHECK(reinterpret_cast<std::uintptr_t>(ptr_aligned) % 64 == 0);

  const std::size_t size_mark = arena_frame.Mark();
  [[maybe_unused]] PodItem* item_temp = arena_frame.Create<PodItem>(1, 2);
  ORA_CHECK(arena_frame.Mark() > size_mark);
  arena_frame.Rewind(size_mark);
  ORA_CHECK(arena_frame.Mark() == size_mark);
  ORA_CHECK(item_a->int4_x == 11);

  [[maybe_unused]] PodItem* vec_big = arena_frame.CreateArray<PodItem>(512);
  ORA_CHECK(arena_frame.size_used() > 1024);
  arena_frame.Reset();
  ORA_CHECK(arena_frame.size_used() == 0);
  PodItem* item_after = arena_frame.Create<PodItem>(5, 6);
  ORA_CHECK(item_after->int4_x == 5);
}

std::int32_t int4_dtor_log[16];
std::int32_t int4_dtor_count;

struct Tracked {
  std::int32_t int4_tag;
  explicit Tracked(std::int32_t t) : int4_tag(t) {}
  ~Tracked() { int4_dtor_log[int4_dtor_count++] = int4_tag; }
};

struct Plain {
  std::int32_t int4_v;
};

void TestWorldArena() {
  int4_dtor_count = 0;
  ora::WorldArena arena_world(1024);

  Plain* obj_plain = arena_world.Create<Plain>(7);
  ORA_CHECK(obj_plain->int4_v == 7);

  Tracked* obj_t1 = arena_world.Create<Tracked>(1);
  Tracked* obj_t2 = arena_world.Create<Tracked>(2);
  Tracked* obj_t3 = arena_world.Create<Tracked>(3);
  ORA_CHECK(obj_t1->int4_tag == 1 && obj_t3->int4_tag == 3);

  arena_world.Destroy(obj_t2);
  ORA_CHECK(int4_dtor_count == 1 && int4_dtor_log[0] == 2);
  arena_world.Destroy(obj_t2);
  ORA_CHECK(int4_dtor_count == 1);

  arena_world.Reset();
  ORA_CHECK(int4_dtor_count == 3);
  ORA_CHECK(int4_dtor_log[1] == 3 && int4_dtor_log[2] == 1);
  ORA_CHECK(arena_world.size_used() == 0);

  Tracked* obj_reuse = arena_world.Create<Tracked>(9);
  ORA_CHECK(obj_reuse->int4_tag == 9);
  arena_world.Reset();
  ORA_CHECK(int4_dtor_count == 4 && int4_dtor_log[3] == 9);

  arena_world.Release();
  Plain* obj_final = arena_world.Create<Plain>(1);
  ORA_CHECK(obj_final->int4_v == 1);
}

}

int main() {
  setvbuf(stdout, nullptr, _IONBF, 0);
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
