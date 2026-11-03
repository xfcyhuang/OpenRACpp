# PROTOTYPE：手写 x64 汇编确定性哈希对拍（wayfinder 工单 #23）

**一次性原型，位于 `prototype/asm-sync-hash` 分支，永不并入主分支。**
回答的问题：手写 x64 汇编 + llvm-mingw 构建 + ctest + oracle 黄金对拍，三条通路能否一次打通。

## 内容

- `sync_hash_proto.S`：上游 `OpenRA.Game/Sync.cs`（基线 `1f594e320`）三个纯函数的 x64 汇编实现
  - `HashInt2/HashCVec = ((X*5)^(Y*3))/4`（C# int32：乘法回绕、**除法向零截断**）
  - `HashPlayer = (int)(ActorID<<16)*0x567`
  - 外加 `naive_sar` 反面教材变体
- `sync_hash_proto.h`：C ABI 声明（`extern "C"`）
- `../../tests/prototype_asm_sync_hash.cpp`：黄金对拍（int2 15 行 + player 10 行 + 陷阱断言 15 行）

## 黄金值生成（oracle）

```sh
cd <临时目录>/oracle_hash_proto && dotnet run -c Release   # .NET 10，CSV 清单见测试文件头注
```

生成器 `Program.cs` 的表达式逐字取自上游 `Sync.cs`（纯算术表达式，语义在语言而非库）。
生产级黄金对拍应由 `tools/golden_gen`（dotnet 无头宿主）直接调用上游程序集产出。

## 暴露的陷阱（写进 asm 单元的注释里了）

1. **截断除法**：C# `/` 向零截断，x86 `sar` 向下取整——负数差 1，直接 desync。
   修正：`cdq; and edx,3; add eax,edx; sar eax,2`。
2. **Win64 ABI 高位垃圾**：32 位参数的 RCX/RDX 高 32 位未定义，只能用 32 位操作。
3. （承自研究票 #22）非叶子函数必写 `.seh_*`；call 前 `sub rsp,0x28`。本文件全是叶子函数，豁免。
