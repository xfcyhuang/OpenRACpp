# COVERAGE —— 偏离登记表

有意不与上游语义等价之处登记于此；**未登记的偏离视为 bug**（见 [ADR-0001](adr/0001-semantic-equivalence-first.md)）。对 `cpp26.md` 的规范例外（如 [ADR-0003](adr/0003-bare-ctest-no-framework.md) 裸 ctest）亦在此留痕。

| 编号 | 日期 | 位置（程序集/文件） | 偏离内容 | 原因 | 验证方式 | 恢复/回收计划 |
|------|------|---------------------|----------|------|----------|----------------|
| D001 | 2026-11-03 | CMakeLists.txt（hardened 编排） | `-pie` 链接旗标换为 MinGW 等价集 `-Wl,--dynamicbase -Wl,--nxcompat -Wl,--high-entropy-va` | clang 23 对 mingw PE 目标报 `-pie` unused 告警且不传导给 ld.lld；cpp26.md §一·五注记指定该等价集 | hardened 编排 ctest 全绿 + 链接行旗标核验 | 工具链支持 `-pie` 无告警时回收 |
| D002 | 2026-11-03 | src/CMakeLists.txt（ora_std 目标） | std.cppm 目标单独加 `-Wno-reserved-module-identifier` | 用户侧 `export module std;` 触发 clang 23 保留名告警；std 垫片是该保留名的唯一合法用例，零告警验收所必需 | 三编排构建零告警 | 换用 libc++ 官方 std 模块或告警语义变化时回收 |
