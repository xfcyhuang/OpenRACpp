# upstream —— 上游注记表

对上游（基线见 [UPSTREAM.baseline](../UPSTREAM.baseline)）实现的调研结论、移植注意点与坑位登记于此，供后续阶段回查。

| 编号 | 日期 | 上游位置 | 注记 | 影响面（C++ 侧位置） |
|------|------|----------|------|----------------------|
| U001 | 2026-11-03 | OpenRA.Game/Support/MersenneTwister.cs:35-39 | `Next()` 的 XML 注释声称返回值介于 -0x7fffffff..0x7fffffff 且 0 双倍概率——**陈旧注释与代码不符**：`Last = (int)(y % int.MaxValue)` 中 C# `uint % int` 提升为 long 运算，结果恒非负 ∈ [0, 2147483646]；黄金数据 7 种子 × 200 值零负值实证 | src/game/support/mersenne_twister.cpp（`next_uint` 的 last 赋值，按代码而非注释移植）；tests/prng.cpp 的 next kind 对拍含非负断言 |
| U002 | 2026-11-03 | OpenRA.Game/Support/MersenneTwister.cs:77-79 | `Next(low, high)` 在 `diff <= 1` 时**不消耗任何随机数**直接返回 `low`（含 `Next(1)`、`Next(0)`、`Next(3,3)` 及 `diff` int 溢出回绕为负的病态情形）；上游大量 `Next(1)`/`Next(0, count)` 空列表调用依赖该短路保持序列对齐 | src/game/support/mersenne_twister.cpp（`next` 双参重载）；tests/prng.cpp 短路契约检查（total_count 零消耗断言，含 int.MinValue/int.MaxValue 回绕路径） |
| U003 | 2026-11-03 | OpenRA.Game/Support/MersenneTwister.cs:57-60 | `NextUlong()` 依赖 C# 左结合求值序：第一个 `NextUint()` 结果是**高 32 位**、第二个是低 32 位；C++ `\|` 两操作数不定序，必须手工分步取值再拼接 | src/game/support/mersenne_twister.cpp（`next_ulong` 分步 hi/lo）；tests/prng.cpp 的 u64 kind 对拍 |
| U004 | 2026-11-03 | OpenRA.Game/Support/Log.cs:49-54 | `Log` 静态构造启动的日志线程未设 `IsBackground=true`（前台线程），任何触发该静态构造却不调 `Log.Dispose()` 的进程都**永不退出**；上游自己的收口在 `OpenRA.Utility/Program.cs` Main 的 finally。oracle 宿主复刻该 finally（tools/golden_gen/Program.cs 顶层 `finally { Log.Dispose(); }`），否则 gen/verify 挂死、脚本整体超时 | tools/golden_gen/Program.cs（宿主收尾，非 C++ 移植体）；C++ 侧日志线程移植时应对齐后台语义或显式 join |
