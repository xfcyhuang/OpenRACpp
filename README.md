# OpenRACpp

**中文** | [English](#english)

## 简介

[OpenRA](https://github.com/OpenRA/OpenRA) 游戏引擎的 **C++26 语义重写**。

按 OpenRA 上游源码（基线 commit `7d57605bca`）的代码语义完全重写，数据、地图与回放格式兼容上游。

## 状态

**Phase 3 完成**(仿真核心 + Order/锁步;2026-10-03):

- `src/sim/` 10 文件:TraitDictionary(平行数组二分,载荷 = gen 导出的 TypeId 上行转换表)、Actor(条件系统/Initialize 观察者链/Dispose 帧末幂等)、World(Tick 序/SyncHash n 连续公式)、Activity 状态机逐行、Sync 哈希协议(含 bool 字段 IL 可达语义)、Target/Player/Effects/TypeDictionary/ActorInitializer
- `src/net/` 8 文件:Order 逐字节序列化(构造期望字节逐位断言 + 往返恒等)、OrderPacket/OrderIO、EchoConnection、OrderManager 锁步全文(TryTick 三段/IsNetFrame 节流/帧号校验)、UnitOrders 可运行子集
- 验收:EchoConnection 单机 **10⁶ tick 双构建(ASan+UBSan/Release)通过,无泄漏无 desync**;全套 ctest 11/11 双构建;UPSTREAM 标注 85 条校验通过;偏离 D25~D34 登记于 docs/COVERAGE.md

**Phase 2 完成**(元数据框架 + 数据加载链;2026-10-03):

- `tools/schema_dumper --gen` 权威导出:680 类型(601 可加载)+ 37 枚举 + 9 BitSet 标签 + TypeId 键空间(6048 类型)+ [VerifySync] 成员表(84 类型);`src/meta/` GenericValue/GeneratedRecord 值袋 + 双向 typed 内存变换
- 加载链全通:Manifest → ModData → Ruleset → ActorInfo(`@` 实例名/接口依赖拓扑序)→ WeaponInfo;24 个 LoadUsing loader 逐语义移植
- 验收:三 mod 全量深度解析 dump **逐字节快照回归**(ra 80,274 / cnc 49,416 / d2k 35,833 行);上游 `--check-yaml` exit=0 佐证;偏离 D10~D24 登记

**Phase 1 完成**(MiniYaml + 文件系统;2026-10-03):

- `src/yaml/`:上游 MiniYaml.cs(791 行)逐语义重写——行状态机(4 空格/1 tab 层级、`\#` 转义、`\ ` 空白守护)、字符串池 interning、`Merge`/继承解析/`-Key` 弱删除、规范化序列化、可变 Builder;`src/core/text.hpp` 复刻 .NET `char.IsWhiteSpace`/`Trim` 的 UTF-8 语义
- `src/fs/`:FileSystem(挂载顺序 = 覆盖优先级、`'|'` 显式挂载、大小写规整解析)、Folder(读写)、ZipFile(只读,SharpZipLib→**miniz**,含子目录视图与嵌套 zip)、`MiniYaml::Load`
- **第一关卡验收:上游 mods/ 全部 759 个 yaml × 双模式(丢弃/保留注释),`FromStream→WriteToString` 输出与 C# oracle 逐字节 100% 一致**(13.5MB 黄金数据,`tests/golden_yaml.txt`);上游 MiniYamlTest.cs 28 用例断言文本逐字移植全绿;fs_test 24 断言全绿
- 工程门禁:`import std;` 严控(禁传统 std 头引入,`tools/std_import_check.py` 强制)、函数级裁剪(`-ffunction-sections -fdata-sections` + `--gc-sections`)、UPSTREAM 溯源标注(`tools/upstream_check.py`)、全代码中英双语注释
- 双构建(ASan+UBSan 与 Release)ctest 全绿;偏离 9 项登记于 docs/COVERAGE.md

**Phase 0 完成**(C++26 工程骨架 + 定点原语):

- 工程基线:clang `-std=c++26` + `import std;`(std.cppm 预编译 PCM)+ CMake/Ninja + ctest;确定性选项 `-fwrapv -fno-strict-aliasing`
- `src/core/` 12 个头文件:定点原语(WPos/WVec/WAngle/WRot/WDist/CPos/CVec/MPos/int2/Rectangle/Int32Matrix4x4/ISqrt)与 MersenneTwister,全部按上游源码逐语义重写,`constexpr` 全面化,文件头带 `// UPSTREAM:` 溯源标注
- 验收:与 C# 版黄金数据 **60,883 行逐行对拍 100% 一致**(覆盖全角度三角学、ArcSin/ArcCos 全量、MT19937 全序列、LerpQuadratic 的 decimal 截断语义等),ASan+UBSan 与 Release 双构建全绿
- 上游同步机制就位:`UPSTREAM.baseline`(基线 commit `7d57605bca`)+ `docs/COVERAGE.md` 覆盖登记 + `tools/upstream_check.py` 校验器

**core 基础设施增补**(2026-10-04,随上游审阅落地):

- `src/core/percent_modifiers.hpp`:上游 `ApplyPercentageModifiers` 的 C# decimal(128 位软十进制,22 个 sim 调用文件)以 `__int128` 精确复刻——分子累积 × 分母 `100^k`,整除 = `(int)decimal` 向零截断
- `src/core/arena.hpp`:PORTING_PLAN §4.5 内存分区落地——FrameArena(帧临时,仅平凡可析构)/ WorldArena(整局,析构逆序登记、Destroy 幂等、Reset 复用)
- `tests/core_test.cpp`:decimal 语义断言(含早截断分歧底线用例)+ arena 生命周期断言;双构建 ctest 12/12

**Phase 4 第一批完成**(平台层骨架 + 渲染命令缓冲;2026-10-04):

- `third_party/SDL2`:官方 2.32.10 MinGW x64 开发包接入
- `src/platform/`:gl_types(103 个 GL 常量逐值对照)+ gl_loader(78 入口表驱动直连,KHR_debug 回调替代 glGetError 轮询)+ sdl2_window(窗口三模式 + **几何打包 atomic 快照**,getter 无锁)
- `src/gfx/`:gfx_command(**值类型命令 + 内联载荷 + SPSC 无锁字节环**,替代上游装箱消息队列)+ render_thread(**统一线程模型**:渲染线程永远存在并独占 GL 上下文,消费端绑定状态 diff)
- 验收(platform_test):SPSC 双线程 **200,000 条**序号完整性压测 + 桌面 GL 集成 —— **NPOT FBO**(333×257)直接 FRAMEBUFFER_COMPLETE、清屏与着色器三角形的 glReadPixels 像素断言;无桌面环境自动 SKIP;双构建 ctest 13/13;偏离 D35~D40 登记

**Phase 4 第三批完成**(Sheet/SheetBuilder/Sprite + Palette 家族 + HardwarePalette;2026-10-05):

- `src/gfx/` 六件:sprite(四枚举 + Sprite 预计算 1/128 inset 归一化坐标 + SpriteWithSecondaryData)、vertex(48B 逐字段 + combined 属性契约)、gfx_util(**FastCreateQuad 位域打包逐位 = combined.vert 注释契约**、FastCopyIntoChannel 全路径、uint32 快速整数预乘、旋转/包围盒/NextPowerOf2 + Vector2/3 渲染运算)、sheet(**shelf 打包 + Indexed 四通道轮换 + dirty 全量/子区域自动切换** + 缓冲转移复用)、palette(Immutable/Mutable/Remap/PaletteReference,字节流构造的 <<2|>>6 语义)、hardware_palette(**OPT-A7:调色板 dirty 行** —— 单行变化逐行 SetSubData 增量,过半退全量;OPT-C5 逐字节读回断言)
- 验收(gfx_test):纯逻辑(shelf 几何/通道轮换/dirtyRegion 并集/palette 字节流边界/预乘边界/位域全位/拷贝全路径)+ GL 集成(Sheet 全量与子区域上传读回、缓冲转移 GL 路径、调色板**增量 == 全量参考逐字节**);双构建 ctest 14/14;修出两个真 bug(palette 扩容清零丢已写行、GetData 未绑定自身读错纹理);偏离 D46~D50 登记

**Phase 4 第二批完成**(输入层 + GL 资源封装;2026-10-05):

- `src/platform/`:keycode.hpp(238 条枚举逐值照搬,SDL 头对照断言)+ sdl2_input(事件泵逐事件:修饰符采样、**motion 合并**、X1X2 转伪键盘、滚轮/UTF-8 文本、退出上报)+ MultiTapDetection/TapHistory(三槽 250ms/位移 4 多击检测,时钟注入可测);sdl2_window 增焦点/挂起原子状态
- `src/gfx/`:shader(**{VERSION} 替换 + 链接后 active-uniform 枚举入整数表(OPT-A6:无字符串热路径)+ sampler 单元分配**,Set*/Location 整型化 API)+ texture(**BGRA 上传/UNPACK 行打包/RGBA16F/读回/ScaleFilter,RAII 异步删除**);gfx_command 增 13 命令
- 验收(platform_test):纯逻辑(多击时序/距离边界、修饰符位组合、坐标舍入边界、Keycode×SDL 对照)+ 合成事件泵(双击 1→2、三条 motion 合一、滚轮/文本/退出)+ GL 集成(**NPOT BGRA 逐字节往返、子区域行距位图、白纹理 × 红 uniform 采样链像素断言、RAII 后管线照常**);双构建 ctest 13/13;修出第一批三个真 bug(SPSC 失配对齐/上下文泄漏/payload 越界);偏离 D41~D45 登记

下一批次(Phase 4 续):SpriteRenderer(8 槽/BlendSpan/持久映射 VB + VAO 缓存)与三级合成 Renderer(单级合成)、OpenAL/FreeType、硬件光标、Westwood 文件格式全家。

## 与上游的差异(优化点与偏离登记)

本项目以**语义等价**为第一原则(黄金对拍验证),在此基础上于 C++ 侧做有意优化;凡无法或有意不逐语义等价处,在 [docs/COVERAGE.md](docs/COVERAGE.md) 登记偏离(**未登记的偏离视为 bug**)。

### 有意优化(C++ 侧)

| 层面 | 上游 C# | C++ 侧 |
|---|---|---|
| 数据反射链 | `FieldLoader`/`ObjectCreator` 运行时反射 + `TypeDescriptor` 转换器兜底 | `gen/` 编译期描述表(680 可加载类型,由 schema_dumper 从 C# 反射权威导出)+ 注册表工厂,无运行时反射 |
| 类型/接口查询 | `Dictionary<Type,…>` 哈希 + `GetInterfaces()` 反射枚举 | TypeId 键空间(6048 类型全名 → `uint16`,gen 导出)+ 平行数组二分 |
| SyncHash | `Reflection.Emit` 运行时生成 IL 哈希 + `ConcurrentCache` 委托链 | `gen/sync_gen.cpp` 编译期生成(84 类型),零反射零委托间接 |
| 数值修正链 | `ApplyPercentageModifiers` 的 C# `decimal`(软十进制,比 int64 慢一个数量级) | `__int128` 精确复刻(整除 = `(int)decimal` 向零截断;等价域论证见 `src/core/percent_modifiers.hpp` 文件头) |
| 内存管理 | GC(每帧 LINQ/闭包/装箱分配) | 分区 arena:`FrameArena`(帧末重置)+ `WorldArena`(整局 bump + 析构登记);同步路径禁 `shared_ptr` |
| 集合参数 | `IEnumerable<int>`(LINQ 链 + 枚举器分配) | `std::span` 直传(首批落地点即数值修正链) |
| 枚举/异常 | 字符串 switch / 异常类型分散 | `enum class` + 集中 `YamlException`(消息文本仍逐字对齐) |

后续大项(渲染命令缓冲替代装箱消息队列、寻路世代标记免清零、条件系统 intern 化、空间索引 ActorID 数组化等,共 40+ 项)按 Phase 随移植同批落地;完整审阅证据(上游 file:line 锚点)见 [docs/UPSTREAM_CPP_REVIEW.md](docs/UPSTREAM_CPP_REVIEW.md)。

### 与上游不同步处(偏离登记摘要)

当前登记 **D1~D50**(全文见 [docs/COVERAGE.md](docs/COVERAGE.md)),按模块:

- **yaml/fs(D1~D9)**:异常类型统一为 YamlException(消息文本逐字一致)、惰性枚举物化为 vector、null/"" 键合流等——合法输入下行为等价或不可观测;
- **meta/加载链(D10~D24)**:TypeConverter 兜底未实现(实际字段类型已全覆盖,不可达)、字典字段为插入序 vector(dump 协议按键排序)、三 mod 解析快照以 C++ 侧固化(D24:C# `--dump` 工具受 ALC 程序集副本环境制约,恢复后可再对拍校准)等;
- **sim/net(D25~D34)**:Initialize 观察者去重形态差异(受端幂等)、**trait 工厂与所有权待 Phase 5 随 World arena 接线(D26/D27)**、`Target.FromCell` 以 square 网格公式桩换算(D28,Phase 5 接 Map)、UI/大厅命令族静默吞并(D29,Phase 6/7 接线)、SyncReport 未接(D30,上游默认关闭)等;
- **平台/渲染(第一批 D35~D40、第二批 D41~D45、第三批 D46~D50)**:命令缓冲替代装箱消息队列、统一线程模型、NPOT、KHR_debug、纹理生命周期契约(替代 glIsTexture 驱逐)等;输入层形态适配(退出上报/时钟注入/X1X2 IsRepeat);**主循环、mods 运行时 trait、UI、服务器、Lua 脚本(Phase 5-8 范围)尚未移植**——见上方状态节。

## 许可证与归属

本项目是 OpenRA 的衍生作品，按上游许可证以 **GPL-3.0** 发布，见 [LICENSE](LICENSE)。

- 上游版权所有：Copyright (c) OpenRA Developers and Contributors
- 本项目的重写代码：Copyright (c) 2026 xfcyhuang

OpenRA、Command & Conquer、Red Alert 及 Dune 2000 相关商标归各自权利人所有，本项目与这些权利人无从属关系。

---

<a name="english"></a>

**[中文](#opencpp-1)** | English

## Overview

A **C++26 semantic rewrite** of the [OpenRA](https://github.com/OpenRA/OpenRA) game engine.

The engine is rewritten from scratch to match the exact semantics of the upstream OpenRA source code (baseline commit `7d57605bca`), while keeping data, map, and replay formats compatible with upstream.

## Status

**Phase 3 complete** (simulation core + orders/lockstep; 2026-10-03):

- `src/sim/`, 10 files: TraitDictionary (parallel arrays with binary search, payloads keyed by the gen-exported TypeId upcast tables), Actor (condition system / Initialize observer chains / idempotent frame-end Dispose), World (tick ordering / the n-consecutive SyncHash formula), a line-by-line Activity state machine, the Sync hash protocol (including the IL-reachable bool semantics), Target/Player/Effects/TypeDictionary/ActorInitializer
- `src/net/`, 8 files: byte-exact Order serialization (expected-byte bit assertions + round-trip identity), OrderPacket/OrderIO, EchoConnection, the full OrderManager lockstep (three-stage TryTick / IsNetFrame throttling / frame validation), and a runnable UnitOrders subset
- Acceptance: EchoConnection **10⁶ ticks on both builds (ASan+UBSan / Release), leak-free and desync-free**; the full ctest suite 11/11 on both builds; 85 UPSTREAM tags validated; deviations D25–D34 registered

**Phase 2 complete** (metadata framework + the data-loading chain; 2026-10-03):

- `tools/schema_dumper --gen` authoritative export: 680 types (601 loadable) + 37 enums + 9 BitSet tagsets + the TypeId key space (6048 full type names) + the [VerifySync] member tables (84 types); `src/meta/` GenericValue/GeneratedRecord value bags with two-way typed memory transforms
- The full loading chain: Manifest → ModData → Ruleset → ActorInfo (`@` instance names / interface-dependency topological order) → WeaponInfo; all 24 LoadUsing loaders ported semantically
- Acceptance: three-mod deep-parse dumps **compared byte-for-byte as frozen snapshots** (ra 80,274 / cnc 49,416 / d2k 35,833 lines); upstream `--check-yaml` exit=0 as corroboration; deviations D10–D24 registered

**Phase 1 complete** (MiniYaml + file system; 2026-10-03):

- `src/yaml/`: a statement-by-statement rewrite of upstream MiniYaml.cs (791 lines) — the line state machine (4-space/1-tab levels, `\#` escaping, `\ ` whitespace guards), string-pool interning, merge/inheritance resolution/`-Key` weak removals, normalized serialization, and the mutable builders; `src/core/text.hpp` replicates the .NET `char.IsWhiteSpace`/`Trim` semantics over UTF-8
- `src/fs/`: FileSystem (mount order as override priority, `'|'` explicit mounts, case-insensitive path resolution), Folder (read/write), ZipFile (read-only, SharpZipLib→**miniz**, with the subfolder view and nested zips), and `MiniYaml::Load`
- **First-gate acceptance: all 759 upstream mods yaml files × both modes (discard/keep comments) match the C# oracle byte-for-byte on `FromStream→WriteToString` output** (13.5 MB golden data in `tests/golden_yaml.txt`); all 28 upstream MiniYamlTest.cs cases ported with verbatim assertion texts; fs_test's 24 assertions all green
- Project gates: strict `import std;` enforcement (no classic std-header includes, forced by `tools/std_import_check.py`), function-level dead-code elimination (`-ffunction-sections -fdata-sections` + `--gc-sections`), UPSTREAM provenance tags (`tools/upstream_check.py`), bilingual (Chinese/English) comments throughout
- Both build flavors (ASan+UBSan and Release) pass ctest; 9 registered deviations in docs/COVERAGE.md

**Phase 0 complete** (C++26 skeleton + fixed-point primitives):

- Toolchain baseline: clang `-std=c++26` + `import std;` (precompiled std.cppm PCM) + CMake/Ninja + ctest; determinism flags `-fwrapv -fno-strict-aliasing`
- 12 headers in `src/core/`: fixed-point primitives (WPos/WVec/WAngle/WRot/WDist/CPos/CVec/MPos/int2/Rectangle/Int32Matrix4x4/ISqrt) and MersenneTwister, rewritten statement-by-statement from upstream sources, fully `constexpr`, each file carrying a `// UPSTREAM:` provenance tag
- Acceptance: **60,883 lines of golden differential testing match the C# output 100%** (covering full-circle trigonometry, exhaustive ArcSin/ArcCos, full MT19937 sequences, and the decimal truncation semantics of LerpQuadratic); clean under both ASan+UBSan and Release builds
- Upstream sync mechanism in place: `UPSTREAM.baseline` (commit `7d57605bca`), coverage registry in `docs/COVERAGE.md`, and the `tools/upstream_check.py` validator

**Core infrastructure additions** (2026-10-04, landed alongside the upstream review):

- `src/core/percent_modifiers.hpp`: the upstream `ApplyPercentageModifiers` C# `decimal` chain (128-bit soft decimal, 22 sim call files) reproduced exactly with `__int128` — accumulated numerator over denominator `100^k`, integer division = `(int)decimal` truncation towards zero
- `src/core/arena.hpp`: the PORTING_PLAN §4.5 memory regions — FrameArena (frame-transient, trivially destructible only) / WorldArena (per-world, reverse-order destructor records, idempotent Destroy, Reset reuse)
- `tests/core_test.cpp`: decimal-semantics assertions (including the early-truncation divergence floor case) + arena lifecycle assertions; ctest 12/12 on both builds

**Phase 4 first batch complete** (platform-layer skeleton + the render command buffer; 2026-10-04):

- `third_party/SDL2`: the official 2.32.10 MinGW x64 dev package
- `src/platform/`: gl_types (103 GL constants value-checked one by one) + gl_loader (78 entry points loaded table-driven; the KHR_debug callback replaces glGetError polling) + sdl2_window (three window modes + a **packed atomic geometry snapshot**, lock-free getters)
- `src/gfx/`: gfx_command (**value-type commands + inline payloads over an SPSC lock-free byte ring**, replacing the upstream boxed message queue) + render_thread (a **unified threading model**: the render thread always exists and solely owns the GL context; the consumer state-diffs bindings)
- Acceptance (platform_test): a two-thread **200,000-record** sequence-integrity stress test plus desktop-GL integration — a **NPOT FBO** (333×257) passing FRAMEBUFFER_COMPLETE directly, and glReadPixels pixel assertions for the clear and a shader triangle; auto-SKIP on headless hosts; ctest 13/13 on both builds; deviations D35–D40 registered

**Phase 4 third batch complete** (Sheet/SheetBuilder/Sprite + the Palette family + HardwarePalette; 2026-10-05):

- Six files in `src/gfx/`: sprite (four enums + Sprite with precomputed 1/128-inset normalized coordinates + SpriteWithSecondaryData), vertex (the 48-byte layout field by field + the combined attribute contract), gfx_util (**FastCreateQuad's bitfield packing bit-for-bit per the combined.vert contract**, every FastCopyIntoChannel path, the fast integer uint32 premultiply, rotation/bounds/NextPowerOf2, plus the Vector2/3 rendering arithmetic), sheet (**shelf packing + the Indexed four-channel rotation + the dirty full/sub-rectangle switch** + buffer-transfer reuse), palette (Immutable/Mutable/Remap/PaletteReference with the byte-stream <<2|>>6 semantics), hardware_palette (**OPT-A7: palette dirty rows** — single-row changes upload incrementally via per-row SetSubData, falling back to a full upload past half-dirty; byte-exact OPT-C5 readback assertions)
- Acceptance (gfx_test): pure logic (shelf geometry/channel rotation/dirtyRegion unions/palette byte-stream boundaries/premultiply boundaries/every bitfield bit/every copy path) + GL integration (full and sub-rectangle sheet uploads with readback, the buffer-transfer GL path, the palette **incremental == full-reference byte-for-byte** check); ctest 14/14 on both builds; two real bugs fixed (the palette growth zeroing written rows; GetData reading the wrong texture unbound); deviations D46–D50 registered

**Phase 4 second batch complete** (the input layer + GL resource wrappers; 2026-10-05):

- `src/platform/`: keycode.hpp (the 238-entry enum reproduced value-for-value, asserted against the SDL headers) + sdl2_input (the event pump event by event: modifier sampling, **motion coalescing**, X1X2 as pseudo-keyboard, wheel/UTF-8 text, exit reporting) + MultiTapDetection/TapHistory (the three-slot 250ms/displacement-4 multi-tap detection with an injectable clock); focus/suspend atomics added to sdl2_window
- `src/gfx/`: shader (the **{VERSION} substitution plus post-link active-uniform enumeration into an integer table (OPT-A6: no string hot path) plus sampler-unit assignment**, integer Set*/Location APIs) + texture (**BGRA uploads/UNPACK row packing/RGBA16F/readback/ScaleFilter with RAII asynchronous deletes**); 13 commands added to gfx_command
- Acceptance (platform_test): pure logic (multi-tap timing/distance boundaries, modifier bit combinations, coordinate-rounding boundaries, Keycode×SDL cross-checks) + the synthetic event pump (double-click 1→2, three motions coalesced to one, wheel/text/exit) + GL integration (**NPOT BGRA byte-exact round-trips, sub-rectangle pitched bitmaps, a white-texture × red-uniform sampling chain with pixel assertions, a healthy pipeline after RAII teardown**); ctest 13/13 on both builds; three first-batch bugs fixed on the way (the misaligned SPSC record / the leaked GL context / the payload over-read); deviations D41–D45 registered

Next batch (Phase 4 continued): SpriteRenderer (8 slots/BlendSpan/persistently-mapped VBs + the VAO cache) and the composite Renderer (single-stage), OpenAL/FreeType, hardware cursors, and the full Westwood file-format family.

## Differences from upstream (optimizations & registered deviations)

Semantic equivalence is this project's first principle (verified by golden differentials); on top of that, deliberate C++-side optimizations are made. Wherever exact semantic equivalence is impossible or intentionally forgone, a deviation is registered in [docs/COVERAGE.md](docs/COVERAGE.md) (**an unregistered deviation is treated as a bug**).

### Deliberate optimizations (C++ side)

| Area | Upstream C# | C++ side |
|---|---|---|
| Data reflection chain | `FieldLoader`/`ObjectCreator` runtime reflection + `TypeDescriptor` converter fallback | `gen/` compile-time descriptor tables (680 loadable types, exported authoritatively from C# reflection by schema_dumper) + registry factories; no runtime reflection |
| Type/interface queries | `Dictionary<Type,…>` hashing + `GetInterfaces()` reflection | the TypeId key space (6048 full names → `uint16`, gen-exported) + parallel-array binary search |
| SyncHash | `Reflection.Emit` runtime IL generation + a `ConcurrentCache` delegate chain | `gen/sync_gen.cpp` generated at compile time (84 types), zero reflection, zero delegate indirection |
| Percentage modifiers | the C# `decimal` chain of `ApplyPercentageModifiers` (soft decimal, an order of magnitude slower than int64) | exact `__int128` reproduction (integer division = `(int)decimal` truncation towards zero; the equivalence-domain argument is in the `src/core/percent_modifiers.hpp` header) |
| Memory management | GC (per-frame LINQ/closure/boxing allocations) | partitioned arenas: `FrameArena` (frame-end reset) + `WorldArena` (per-world bump + destructor records); no `shared_ptr` on the synced path |
| Collection parameters | `IEnumerable<int>` (LINQ chains + enumerator allocations) | direct `std::span` (the numeric-modifier chain is the first landing point) |
| Enums / exceptions | string switches / scattered exception types | `enum class` + a unified `YamlException` (message texts still match verbatim) |

The bigger items ahead (a render command buffer replacing the boxing message queue, generation-stamped pathfinding state, condition-name interning, ActorID-array spatial indexes, and 40+ more) land per phase together with their ports; the full review evidence (upstream file:line anchors) is in [docs/UPSTREAM_CPP_REVIEW.md](docs/UPSTREAM_CPP_REVIEW.md).

### Not-yet-synced with upstream (registered-deviation summary)

Currently **D1–D50** (full texts in [docs/COVERAGE.md](docs/COVERAGE.md)), by module:

- **yaml/fs (D1–D9)**: exception types unified into YamlException (message texts verbatim), lazy enumerations materialized into vectors, null/"" key coalescing, etc. — behavior-equivalent or unobservable for valid inputs;
- **meta/loading chain (D10–D24)**: the TypeConverter fallback not implemented (actual field types are fully covered, unreachable), dictionary fields as insertion-ordered vectors (dump protocol sorts by key), the three-mod parse snapshots frozen on the C++ side (D24: the C# `--dump` tool is constrained by the ALC assembly-copy environment; re-differential once restored), etc.;
- **sim/net (D25–D34)**: the Initialize observer-dedup shape differs (receiving ends are idempotent), **the trait factory and ownership await Phase 5 wiring into the World arena (D26/D27)**, `Target.FromCell` stubbed with the square-grid formula (D28, Map lands in Phase 5), UI/lobby command families silently swallowed (D29, wired in Phase 6/7), SyncReport not wired (D30, off by default upstream), etc.;
- **Platform/render (first batch D35–D40, second batch D41–D45, third batch D46–D50)**: the command buffer replacing the boxing message queue, the unified threading model, NPOT, KHR_debug, the texture lifetime contract (replacing the glIsTexture eviction), etc., plus input-layer shape adaptations (exit reporting / clock injection / X1X2 IsRepeat); **the main loop, runtime mods traits, UI, server, and Lua scripting (Phases 5–8) are not ported yet** — see the status section above.

## License & Attribution

This project is a derivative work of OpenRA and is released under the **GPL-3.0**, the same license as upstream. See [LICENSE](LICENSE).

- Upstream copyright: Copyright (c) OpenRA Developers and Contributors
- Rewrite code in this project: Copyright (c) 2026 xfcyhuang

OpenRA, Command & Conquer, Red Alert, and Dune 2000 are trademarks of their respective owners; this project is not affiliated with them.
