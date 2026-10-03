// cpp26_probe — C++26 特性探针(PORTING_PLAN Phase 0)
// 验证本工具链(clang 23.1.0 / llvm-mingw / libc++)的规范可用性。
// 本机实测结论(2026-10-03):
//   ✅ import std;(需预编译 share/libc++/v1/std.cppm → std.pcm,经 -fmodule-file 引用)
//   ✅ expected / print / println / constexpr byteswap / constexpr new / [[assume]]
//   ❌ 结构化绑定引入 pack(P1061):报 "pack declaration outside of template",不可用
//   ❌ std::inplace_vector(P0843)此 libc++ 版本未提供——后续阶段需要时自实现或回退
import std;

// constexpr 动态分配(C++20 起,含 P2747 非分配 placement new)
constexpr int constexpr_new_probe() {
  int* ptr_x{new int{7}};  // 编译期 new
  int val_x{*ptr_x};
  delete ptr_x;
  return val_x;
}

int main() {
  std::expected<int, int> maybe_val{5};  // C++23 expected

  std::print("probe: expected={}\n", maybe_val.value());

  constexpr bool b_swapped{std::byteswap<std::uint16_t>(0x1234) == 0x3412};  // constexpr 字节序
  constexpr int val_cnew{constexpr_new_probe()};
  static_assert(b_swapped && val_cnew == 7);

  [[assume(b_swapped)]];  // C++23/26 优化提示

  return 0;
}
