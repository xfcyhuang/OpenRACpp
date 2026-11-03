# llvm-mingw 汇编工具链能力核查

> 工单：[#22](https://github.com/xfcyhuang/OpenRACpp/issues/22)（wayfinder #19 子票，阻塞原型票 #23）
> 日期：2026-11-03。方法：本机最小实证（可复现命令全部记录在文内与附录）+ 官方文档核查。
> 结论一句话：**llvm-mingw Clang 23 集成汇编器 + lld 完全可以承载手写 Intel 语法 x64 汇编单元，推荐 `.S` 后缀 + 内建 `ASM` 语言接入 CMake；两条铁律是「非叶子函数必须写 `.seh_*` unwind 元数据」与「call 前必须留 0x28 影子空间」，违反任一条均实测导致进程崩溃。**

## 1. 实证环境

| 项 | 值 |
|---|---|
| 工具链 | llvm-mingw（解包于 `D:\software\LLVM`，PATH 首位；同机另有 winget 版 22.1.6 未用） |
| clang | `clang version 23.1.0`，默认目标 `x86_64-w64-windows-gnu`（`D:/software/LLVM/bin/x86_64-w64-windows-gnu.cfg`），x64 默认 SEH |
| 链接器 | `ld.lld`（llvm-mingw 唯一链接器；显式 `-fuse-ld=lld` 亦通过） |
| CRT | UCRT（sysroot `D:/software/LLVM/x86_64-w64-mingw32/`，`crt2.o` 等） |
| 产物格式 | COFF-x86-64 对象 / PE 可执行（`llvm-readobj --file-headers` 确认） |
| CMake / Ninja | cmake 4.3.2 / ninja 1.13.0 |
| NASM | 本机未安装 → NASM 部分为文档判断，标注「未实证」 |

## 2. 能力矩阵

| # | 能力项 | 结论 | 证据/命令 |
|---|---|---|---|
| 1 | `.intel_syntax noprefix`（Intel 语法 + 无 `%` 前缀） | ✅ 可行 | 全部示例源码即证；见 GNU as 手册 [i386 变体](https://sourceware.org/binutils/docs/as/i386_002dVariations.html)（集成汇编器兼容此约定） |
| 2 | `.S` 后缀（走 C 预处理器，cpp 宏可用） | ✅ 可行 | `#define VAL 7` 在 `.S` 中正确展开为 `mov eax, 7` |
| 3 | `.asm` 后缀 | ⚠️ 有限制 | `clang -c foo.asm` 能汇编（rc=0，符号正常），但**不走 cpp**：`#define VAL 7` 静默失效，`VAL` 变成未定义符号引用（链接期或运行期才暴露）。**结论：项目内一律用 `.S`；若必须 `.asm` 需加 `-x assembler` 且不得使用 cpp 宏** |
| 4 | 与 C 目标混链 | ✅ 可行 | `clang -c main.c` + `clang -fuse-ld=lld main.o asmfuncs.o` |
| 5 | 与 C++ 目标混链 | ✅ 可行 | `clang++ -c helper.cpp` 与 `.S` 目标同链；注意 C++ 侧函数要 `extern "C"`，否则 name-mangle 导致 `ld.lld: undefined symbol`（本实证首跑即踩到，属源码问题非工具链问题） |
| 6 | 与 UCRT/mingw CRT 混链 | ✅ 可行 | 汇编单元所在程序调用 `printf`/`malloc` 等正常 |
| 7 | 与 SDL2 混链 | ⚠️ 未实证 | 本机 sysroot 无 SDL2。判断：与第 4/5/6 项同走 lld 的 COFF/PE 链接路径，机制上无差别；Phase 3 引入 SDL2 后补一次冒烟即可 |
| 8 | lld 链接（`-fuse-ld=lld`） | ✅ 可行 | 全部实证产物如此链接 |
| 9 | DWARF 调试信息（`-g`） | ✅ 可行 | PE 内 `.debug_abbrev/aranges/info/line/loc/ranges/str` 段齐全（`llvm-readobj --sections`） |
| 10 | 符号化回溯（崩溃地址 → 函数 + `.S` 行号） | ✅ 可行 | `llvm-symbolizer --obj=demo_g.exe 0x140001720` → `crash_write_null` + `asmfuncs.S:73:0`；`llvm-addr2line -e demo_g.exe -f 0x140001720` 同。注意 PE 要传**完整 VA**（默认基址 `0x140000000` + RVA），传 RVA 只得 `??` |
| 11 | 调试器（lldb）帧级调试 | ✅ 可行（带限制） | lldb 停在崩溃帧并显示 `.S` 源码行（`frame #0 ... crash_write_null at asmfuncs.S:73`）。⚠️ 本机 lldb 的**批处理模式**（`-o run -o bt`、stdin 管道）在 run 后不再执行/回显后续命令，交互式不受影响；CI 中应依赖 symbolizer/addr2line 而非 lldb 批处理 |
| 12 | SEH 异常穿越汇编帧（叶子，无 unwind 元数据） | ✅ 可行 | x64 展开器对无 `.pdata` 条目的帧按叶子规则推断（返回地址 = `[RSP]`），纯叶子（不改 RSP、无 call）实测可被 `__try/__except` 正常捕获 |
| 13 | SEH 异常穿越汇编帧（非叶子，写 `.seh_*`） | ✅ 可行 | `.seh_proc/.seh_stackalloc/.seh_endprologue/.seh_endproc` → `llvm-readobj --unwind` 出现该函数条目，`__except` 实测捕获成功 |
| 14 | SEH 异常穿越汇编帧（非叶子，**无** unwind 元数据） | ❌ 不可行 | 实测：进程直接崩溃（退出码 0xC0000005），`__except` 救不回。这是手写汇编第一杀手：**任何有 prologue（`sub rsp`/`push`）或含 `call` 的汇编函数必须写 `.seh_*`** |
| 15 | 影子空间（0x28 = 0x20 home 区 + 8 对齐垫） | ✅ 可行（正确实现） | `apply`（`sub rsp,40` → `call rdx` → `add rsp,40`）实测通过。❌ 缺失实测：call 前 4 个栈参数写 `[rsp+0x20..0x38]` 直接踩烂调用方栈帧 → 进程崩溃 |
| 16 | 非易失寄存器保存（RBX/RBP/RDI/RSI/R12-15/XMM6-15） | ✅ 可行 | `keep_xmm6` 保存/恢复 XMM6 实测正确（调用后 C 侧浮点状态无损） |
| 17 | 数据段放置（`.section .rdata`）+ RIP 相对寻址 | ✅ 可行 | `nth` 经 `lea rax,[rip+table]` 读 `.rdata` 表实测正确 |
| 18 | CMake 内建 `ASM` 语言直接吃 `.S` | ✅ 可行 | `project(... C CXX ASM)` + `add_executable(demo main.c helper.cpp asmfuncs.S)`；构建日志 `Building ASM object` |
| 19 | CMake `add_custom_command` 接 `.asm` | ✅ 可行 | 自定义命令 `clang --target=... -x assembler -c extra.asm -o extra.obj`；构建日志 `assembling extra.asm` |
| 20 | ctest 接入 | ✅ 可行 | `enable_testing()` + `add_test`，2/2 Passed |
| 21 | 内联汇编（Intel 语法）+ `__builtin` 帮手 | ✅ 可行 | `__asm(".intel_syntax noprefix\n lea eax,[ecx+edx]\n .att_syntax prefix\n" ...)` 与 `__builtin_popcount/_bswap32/_clz` 同单元共存，`-O2` 下实测正确。热点代码可优先选内联汇编而非独立 `.S` |
| 22 | 反汇编 Intel 输出 | ✅ 可行 | `llvm-objdump -d --x86-asm-syntax=intel demo.exe`（默认输出是 AT&T） |
| 23 | DWARF dump 工具完备性 | ⚠️ 有限制 | 本机 llvm-mingw 的 `llvm-objdump --dwarf=` / `llvm-readobj --debug-*` 被裁剪不可用，也无 `llvm-dwarfdump`；符号化以 `llvm-symbolizer`/`llvm-addr2line` 为准（够用） |
| 24 | NASM 备选 | ⚠️ 未实证（文档判断） | 见 §5 |

## 3. 已知限制与坑清单（按杀伤力排序）

1. **非叶子汇编函数不写 `.seh_*` → 任何穿越该帧的异常直接杀死进程**（实证 #14）。ASAN/UBSAN 报错、CRT 错误路径、系统回调、SDL 内部异常都可能穿帧。规则：有 `sub rsp`/`push`/`call` 就必须 `.seh_proc` 全套；纯叶子（不改 RSP、无 call）可豁免（实证 #12）。
2. **call 前不留 0x28 影子空间 → 栈损坏崩溃**（实证 #15）。第 5+ 个参数由调用方写 `[rsp+0x20]` 起，没 sub 就直接写进调用方帧。叶子函数不 call 他人则无需影子空间。
3. **`.asm` 后缀不走 cpp**（实证 #3）：`#define`/`#include` 静默失效且能汇编通过，错误推迟到链接或运行期。统一用 `.S`。
4. **C/C++ 混链符号名**：C++ 侧引用汇编函数需 `extern "C"`，否则 mangle 后 `ld.lld` 报 undefined symbol。
5. **PE 符号化要传完整 VA**（默认基址 `0x140000000` + RVA）：`llvm-symbolizer --obj=x.exe 0x140001720`，传 `0x1720` 只得 `??`。运行时地址需先减去 ASLR 实际基址换算 RVA。
6. **lldb 批处理模式缺陷**（本机实证 #11）：`-b -o run -o bt` 与 stdin 管道在进程 stop 后不回显后续命令；交互式正常。CI 崩溃分析用 vectored exception handler 打印 RIP + `llvm-symbolizer` 的组合（本报告正是一例）。
7. `llvm-objdump`/`llvm-readobj` 的 DWARF dump 选项被 llvm-mingw 裁剪（#23），查段用 `llvm-readobj --sections`，符号化用 `llvm-symbolizer`。
8. mingw 目标 `__try/__except` 需 `-fms-extensions` 编译 C 源（x64 SEH 本身默认开启）。

## 4. 最小构建配方（原型票 #23 可直接抄）

### 4.1 命令行（手工构建 + 运行）

```bash
CLANG=D:/software/LLVM/bin/clang.exe
CXX=D:/software/LLVM/bin/clang++.exe

# 汇编（.S 走 cpp + 集成汇编器；--target 可省——clang 默认即 x86_64-w64-windows-gnu）
"$CLANG" --target=x86_64-w64-windows-gnu -g -c asmfuncs.S -o asmfuncs.o
"$CLANG" --target=x86_64-w64-windows-gnu -g -c main.c     -o main.o
"$CXX"   --target=x86_64-w64-windows-gnu -g -c helper.cpp  -o helper.o
"$CXX"   --target=x86_64-w64-windows-gnu -g -fuse-ld=lld \
         main.o asmfuncs.o helper.o -o demo.exe
./demo.exe
```

### 4.2 汇编单元模板（含两条铁律的落地写法）

```asm
# xxx.S —— Intel 语法 x64 汇编单元模板（MS x64 调用约定）
    .intel_syntax noprefix
    .text

# ---- 叶子函数：无 prologue、无 call → 无需影子空间、无需 .seh ----
# int add2(int a, int b) —— a=ECX, b=EDX，返回 EAX
    .globl add2
    .def add2; .scl 2; .type 32; .endef
add2:
    lea   eax, [rcx + rdx]
    ret

# ---- 非叶子函数：影子空间(0x28) + .seh unwind 元数据，二者缺一不可 ----
# int apply(int x, int (*fn)(int)) —— RCX=x, RDX=fn，返回 fn(x)
    .globl apply
    .def apply; .scl 2; .type 32; .endef
    .seh_proc apply
apply:
    sub   rsp, 40                 # 0x28：32 字节影子空间 + 8 对齐垫（进入时 RSP%16==8）
    .seh_stackalloc 40
    .seh_endprologue
    call  rdx                     # RCX 已是 x
    add   rsp, 40
    ret
    .seh_endproc

# ---- 非易失寄存器（XMM6-15/RBX/RBP/RDI/RSI/R12-15）先保存后使用 ----
# double keep_xmm6(double x)
    .globl keep_xmm6
    .def keep_xmm6; .scl 2; .type 32; .endef
    .seh_proc keep_xmm6
keep_xmm6:
    sub   rsp, 24                 # 16 保存槽 + 8 对齐
    .seh_stackalloc 24
    .seh_endprologue
    movsd qword ptr [rsp], xmm6   # 保存
    movsd xmm6, xmm0
    addsd xmm6, xmm6
    addsd xmm0, xmm6
    movsd xmm6, qword ptr [rsp]   # 恢复
    add   rsp, 24
    ret
    .seh_endproc

# ---- 只读数据：.rdata 段 + RIP 相对寻址 ----
    .section .rdata,"dr"
table:
    .long 10, 20, 30, 40
    .text
    .globl nth
    .def nth; .scl 2; .type 32; .endef
nth:                             # int nth(int i) —— RCX=i
    lea   rax, [rip + table]
    mov   eax, dword ptr [rax + rcx*4]
    ret

# ---- SSE2 + 循环（只碰 volatile 寄存器：xmm0/xmm1、r9）----
# double dot(int n, const double* a, const double* b)
    .globl dot
    .def dot; .scl 2; .type 32; .endef
dot:
    xorpd xmm0, xmm0
    xor   r9d, r9d
    test  rcx, rcx
    jle   2f
1:  movsd xmm1, qword ptr [rdx + r9*8]
    mulsd xmm1, qword ptr [r8 + r9*8]
    addsd xmm0, xmm1
    inc   r9
    cmp   r9, rcx
    jl    1b
2:  ret
```

### 4.3 C 侧调用（含 extern "C" 与回溯辅助）

```c
/* main.c */
#include <stdio.h>
int add2(int a, int b);
int apply(int x, int (*fn)(int));
double keep_xmm6(double x);
int nth(int i);
double dot(int n, const double* a, const double* b);
int cpp_gate(int a, int b);          /* helper.cpp 提供，须 extern "C" */

static int doubler(int x) { return x * 2; }

int main(void) {
    printf("add2=%d\n", add2(3, 4));
    double a[4] = {1,2,3,4}, b[4] = {4,3,2,1};
    printf("dot=%.1f\n", dot(4, a, b));
    printf("apply=%d\n", apply(21, doubler));
    printf("keep_xmm6=%.1f nth=%d cpp=%d\n", keep_xmm6(2.0), nth(2), cpp_gate(3,4));
    return 0;
}
```

```cpp
// helper.cpp —— C++ 目标与 .S 混链
extern "C" int add2(int a, int b);
extern "C" int cpp_gate(int a, int b) { return add2(a, b) + 100; }
```

### 4.4 CMake 接入（与现有 preset 体系对齐）+ ctest

```cmake
# 推荐：内建 ASM 语言（零自定义命令）
project(OpenRACpp C CXX ASM)          # 现有 project(...) 追加 ASM 即可
add_executable(ora_asm_smoke
    src/asm/main.c
    src/asm/asmfuncs.S)               # .S 直接进源列表，Ninja 自动派生规则

enable_testing()
add_test(NAME asm_smoke COMMAND ora_asm_smoke)
```

配置命令（编译器三件套都指 llvm-mingw）：

```bash
cmake -G Ninja -S . -B build/base \
  -DCMAKE_C_COMPILER=D:/software/LLVM/bin/clang.exe \
  -DCMAKE_CXX_COMPILER=D:/software/LLVM/bin/clang++.exe \
  -DCMAKE_ASM_COMPILER=D:/software/LLVM/bin/clang.exe
cmake --build build/base && ctest --test-dir build/base --output-on-failure
```

若因命名规范必须 `.asm` 后缀，用自定义命令（记得 `-x assembler`，且文件内不得用 cpp 宏）：

```cmake
add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/extra.obj
  COMMAND ${CMAKE_C_COMPILER} --target=x86_64-w64-windows-gnu -x assembler
          -c ${CMAKE_CURRENT_SOURCE_DIR}/extra.asm
          -o ${CMAKE_CURRENT_BINARY_DIR}/extra.obj
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/extra.asm
  VERBATIM)
add_executable(demo2 main2.c ${CMAKE_CURRENT_BINARY_DIR}/extra.obj)
```

### 4.5 崩溃回溯配方（CI 友好，不依赖 lldb 批处理）

```c
/* 程序内自报故障地址：向量异常处理器 + llvm-symbolizer 离线解析 */
static LONG WINAPI fault_printer(EXCEPTION_POINTERS* ep) {
    fprintf(stderr, "FAULT rip=%p code=0x%lX\n",
            (void*)ep->ContextRecord->Rip, ep->ExceptionRecord->ExceptionCode);
    return EXCEPTION_CONTINUE_SEARCH;   /* 交由系统默认处理（进程终止） */
}
/* main 入口处：AddVectoredExceptionHandler(1, fault_printer); */
```

```bash
# 解析（完整 VA = 0x140000000 + 偏移；0x1720 来自上面打印的 rip 减去 ASLR 基址）
llvm-symbolizer --obj=demo.exe 0x140001720
# → crash_write_null + D:/.../asmfuncs.S:73:0
```

## 5. NASM 备选（未实证，文档判断）

本机未安装 NASM（`where nasm` 无结果）。据官方文档判断：

- NASM `-f win64` 产出 Microsoft Win64 COFF 对象，与 lld 的 COFF x64 后端兼容，可与 clang 产物混链（NASM 手册第 9 章：win32/win64 支持 `-g` 启用 CodeView 调试信息）。
- 差异与风险：NASM 语法自成一体（非 GNU as 兼容），`default rel`、`proc` 宏等与集成汇编器写法不同；其调试信息为 CodeView 而非 DWARF，与本工具链 lldb/symbolizer 的 DWARF 路径不互通（lldb 对 mingw 目标读 DWARF）。
- 判断：**既然集成汇编器全项可用，NASM 只在「已有 NASM 语法的上游汇编资产要直接复用」时才有意义**；新写汇编一律用 `.S` + 集成汇编器。若未来需要 NASM，先跑本报告 §4.1 同款冒烟再定。

## 6. 对原型票 #23 的直接输入

1. 语法与后缀：`.intel_syntax noprefix` + `.S` 后缀（要 cpp）；禁用 `.asm`。
2. 两铁律进代码评审清单：非叶子必 `.seh_*`；call 前必 `sub rsp,0x28`（对齐后）。
3. CMake：现有 `project(... C CXX)` 追加 `ASM`，`.S` 直接进 `add_executable`/`add_library` 源列表；preset 无需改动（`CMAKE_ASM_COMPILER` 传 clang）。
4. 验证：ctest 普通冒烟 + 「故意崩溃 → vectored handler 打印 RIP → llvm-symbolizer 解析行号」作为汇编单元的最低调试保障。
5. 与 SDL2 混链待 Phase 3 首次引入时补一条冒烟记录（机制同 CRT 混链，预期无障碍）。

## 附录：实证记录摘要

全部实验在 `%TEMP%` 一次性目录完成（`D:\Temp\ora_asm_ghcW`），未在仓库工作区留构建残料。关键输出：

```
$ ./demo.exe
add2(3,4)=7            cpp_gate(3,4)=107      dot=20.0
apply(21,doubler)=42   keep_xmm6(2.0)=6.0     nth(2)=30
add2(5,6)=11 dot2=10.0   OK                   exit=0

$ ./demo_g.exe crash   # -g 构建
FAULT addr=00007FF70E361720 rip=00007FF70E361720 code=0xC0000005
$ llvm-symbolizer --obj=demo_g.exe 0x140001720
crash_write_null
D:/Temp/ora_asm_ghcW/asmfuncs.S:73:0

# SEH 展开矩阵
叶子帧 + 无 .seh            → __except 捕获成功 ✅
非叶子帧 + .seh_proc 全套   → __except 捕获成功 ✅
非叶子帧 + 无 .seh          → 进程崩溃(0xC0000005)，__except 无法捕获 ❌
call 前无 sub rsp,0x28      → 栈参数写入踩烂调用方帧，进程崩溃 ❌

# CMake/ctest（双路线）
100% tests passed, 0 tests failed out of 2 → asm_builtin_language / asm_custom_command 均 Passed
```

### 参考来源

- [NASM 手册第 9 章：输出格式](https://www.nasm.us/doc/nasm09.html)（win64/-g/CodeView）
- [GNU as 手册：i386 变体](https://sourceware.org/binutils/docs/as/i386_002dVariations.html)（`.intel_syntax` 指令）
- [maskray：Assemblers](https://maskray.me/blog/assemblers)（集成汇编器与 `-x86-asm-syntax=intel` 行为）
- [LLVM issue #109157](https://github.com/llvm/llvm-project/issues/109157)（`--x86-asm-syntax=intel` 相关回归，输入语法应以文件内 `.intel_syntax noprefix` 为准）
