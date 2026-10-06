# OpenRACpp

**中文** | [English](#english)

## 简介

[OpenRA](https://github.com/OpenRA/OpenRA) 游戏引擎的 **C++26 语义重写**。

按 OpenRA 上游源码（基线 commit `b6fc03fcfa`）的代码语义逐行重写，数据、地图与回放格式兼容上游。以**语义等价**为第一原则（黄金对拍验证），在此基础上做 C++ 侧有意优化；无法逐语义等价处登记于 [docs/COVERAGE.md](docs/COVERAGE.md)（未登记的偏离视为 bug）。

配套文档:[PORTING_PLAN.md](PORTING_PLAN.md)（分阶段移植计划与状态）、[docs/COVERAGE.md](docs/COVERAGE.md)（偏离登记全文）、[docs/UPSTREAM_CPP_REVIEW.md](docs/UPSTREAM_CPP_REVIEW.md)（优化点审阅证据）、[UPSTREAM.baseline](UPSTREAM.baseline)（上游基线与同步流程）。

## 进度总览

**整体完成度约 45%**（按 PORTING_PLAN 各阶段工作量加权估算）。数据/引擎/渲染/平台层（前半程）已完成并经黄金对拍锁定；当前处于 **Phase 5 的 gameplay 大面**——Mobile 与武器链落地后，即可开启 skirmish 可玩里程碑与 replay SyncHash 对拍两个关键关卡。

| 阶段 | 内容 | 状态 | 完成度 |
|---|---|---|---|
| Phase 0 | C++26 工程骨架 + 定点原语 | ✅ 完成（2026-10-03） | 100% |
| Phase 1 | MiniYaml + 文件系统 | ✅ 完成（2026-10-03） | 100% |
| Phase 2 | 元数据框架 + 数据加载链 | ✅ 完成（2026-10-03） | 100% |
| Phase 3 | 仿真核心 + Order/锁步 | ✅ 完成（2026-10-03） | 100% |
| Phase 4 | 平台层 + 渲染 + 文件格式 + 音频 + UI 框架起步 + Game 主循环骨架（15 批） | ✅ 完成（2026-10-06） | 100% |
| Phase 5 | 主循环整合 + 核心 gameplay（已完成 Map/World/寻路/Health 等 2 批） | 🔨 进行中（2026-10-07 起） | ~30% |
| Phase 6 | UI + 本地化 | 🌱 起步（框架内核已就位） | ~10% |
| Phase 7 | 网络与服务器 | 🌱 起步（锁步内核已就位） | ~20% |
| Phase 8 | Lua 脚本 + AI + 战役 + Utility | ⬜ 未开始 | ~0% |
| Phase 9 | 打磨与发布 | ⬜ 未开始 | 0% |

### 已达成的验证关卡

- **MiniYaml 逐字节对拍**：上游 mods/ 全部 759 个 yaml × 双模式（丢弃/保留注释），`FromStream→WriteToString` 与 C# oracle **逐字节 100% 一致**（13.5MB 黄金数据）。
- **定点原语黄金对拍**：与 C# 版 **60,883 行逐行一致**（全角度三角学、MT19937 全序列、decimal 截断语义等）。
- **三 mod 规则深解析**：ra 80,274 / cnc 49,416 / d2k 35,833 行**逐字节快照回归**；上游 `--check-yaml` exit=0 佐证。
- **资产解码黄金对拍**：mods 全部 .shp/.pal/.aud/.wav/.vqa/.wsa/.vxl/.hva/.idx/.voc/.r8 资产 + 合成夹具，`tests/golden_formats.txt` **6,737 行逐行一致**；字形 **216 个逐字节一致**（与 C# oracle 加载同一 freetype6.dll）。
- **锁步确定性**：EchoConnection 单机 **10⁶ tick 双构建（ASan+UBSan/Release）通过，无泄漏无 desync**。
- **持续门禁**：ctest **20/20** 双构建（ASan+UBSan 与 Release）全绿；`import std;` 严控（**324 文件**）；UPSTREAM 溯源标注（**312 条**）；偏离登记 **D1~D119**。

### 当前焦点（Phase 5 第三批）

Mobile + IPositionable/BodyOrientation 一族（Locomotor 挂点换实）+ Move 活动 + Weapons/Warheads 起步（Armament/Projectile，OPT-A9 开火链零分配）。其后的关键里程碑：**skirmish 地图控制台发 order 造兵/移动/战斗** → **C# 录制 replay 逐帧 SyncHash 对拍（确定性移植总关卡）**。

## 各阶段完成情况

### Phase 0 — C++26 工程骨架 + 定点原语 ✅

clang `-std=c++26` + `import std;`（std.cppm 预编译 PCM）+ CMake/Ninja + ctest；确定性选项 `-fwrapv -fno-strict-aliasing`。`src/core/` 12 个头文件：定点原语（WPos/WVec/WAngle/WRot/WDist/CPos/CVec/MPos/int2/Rectangle/Int32Matrix4x4/ISqrt）与 MersenneTwister，逐语义重写、`constexpr` 全面化、文件头带 `// UPSTREAM:` 溯源。基础设施增补：`percent_modifiers.hpp`（C# decimal 修正链以 `__int128` 精确复刻，OPT-A1）、`arena.hpp`（FrameArena/WorldArena 内存分区，§4.5）。

### Phase 1 — MiniYaml + 文件系统 ✅

- `src/yaml/`：MiniYaml.cs（791 行）逐语义重写——行状态机（4 空格/1 tab 层级、`\#` 转义、`\ ` 空白守护）、字符串池 interning、`Merge`/继承解析/`-Key` 弱删除、规范化序列化、可变 Builder；`text.hpp` 复刻 .NET `char.IsWhiteSpace`/`Trim` 的 UTF-8 语义。上游 MiniYamlTest.cs 28 用例逐字移植全绿。
- `src/fs/`：FileSystem（挂载序 = 覆盖优先级、`'|'` 显式挂载、大小写规整）、Folder、ZipFile（SharpZipLib→miniz，含子目录视图与嵌套 zip）、`MiniYaml::Load`。

### Phase 2 — 元数据框架 + 数据加载链 ✅

`tools/schema_dumper --gen` 权威导出：**680 类型（601 可加载）+ 37 枚举 + 9 BitSet 标签 + TypeId 键空间（6048 全名）+ [VerifySync] 成员表（84 类型）**。`src/meta/` GenericValue/GeneratedRecord 值袋 + 双向 typed 内存变换。加载链全通：Manifest → ModData → Ruleset → ActorInfo（`@` 实例名/接口依赖拓扑序）→ WeaponInfo；**24 个 LoadUsing loader 逐语义移植**。

### Phase 3 — 仿真核心 + Order/锁步 ✅

- `src/sim/`：TraitDictionary（平行数组二分，载荷 = TypeId 上行转换表）、Actor（条件系统/Initialize 观察者链/Dispose 帧末幂等）、World（Tick 序/SyncHash n 连续公式）、Activity 状态机、Sync 哈希协议（含 bool 字段 IL 可达语义）、Target/Player/Effects/TypeDictionary/ActorInitializer。
- `src/net/`：Order 逐字节序列化（逐位断言 + 往返恒等）、OrderPacket/OrderIO、EchoConnection、OrderManager 锁步全文（TryTick 三段/IsNetFrame 节流/帧号校验）、UnitOrders 可运行子集。

### Phase 4 — 平台层 + 渲染 + 文件格式 + 音频（15 批）✅

- **平台层**：`third_party/` SDL2 2.32.10 / OpenAL-soft 1.24.3 / FreeType 2.14.1（与 C# oracle 加载同一 DLL——字形逐字节恒等的前提）；gl_loader（78 入口表驱动，KHR_debug 回调）、sdl2_window（几何打包 atomic 快照，getter 无锁）、sdl2_input（事件泵：motion 合并/X1X2 伪键盘/多击检测）、al_loader（25 入口直连）、openal_sound_engine（OpenAlSoundEngine.cs 全文：池 256/实例限 3/设备表怪癖 bug 兼容）、freetype_font（**OPT-B2 真结构体 + LP64 ABI 手术**，六偏移 static_assert 锁定）、sdl2_hardware_cursor。
- **渲染管线**（`src/gfx/`）：gfx_command（**值类型命令 + SPSC 无锁字节环**替代装箱消息队列，OPT-A5）+ render_thread（统一线程模型）；shader/texture（OPT-A6：uniform 整数化 + RAII）；Sheet/SheetBuilder/Palette 家族 + HardwarePalette（**OPT-A7 调色板 dirty 行**，增量 == 全量逐字节）；vertex_buffer（**glBufferStorage 持久映射三槽 + fence + VAO 缓存**）；sprite_renderer + renderer（**OPT-B1 单级合成**：每帧 3 clear + 2 blit → 1 + 1）；WorldRenderer/渲染收集（**RenderItem POD + 帧 arena + (zkey<<32)|i 全序排序**）；TerrainSpriteLayer（**分离数组组合等价**）；Viewport 全量（缩放矩阵/滚动夹取/Ramp 角多边形命中）；SpriteCache/SequenceSet/Animation/CursorManager/ChromeProvider。
- **Westwood 文件格式族**（`src/formats/`，六编全收）：编解码器（LCW/XOR/RLE0/LZO/CRC32）+ SHP TD/D2/TS + TMP TD/RA/TS + **Png 自研**（上游 592 行逐语义照抄）+ Tga/Dds（Pfim 逐语义）+ ShpRemastered + mix 包（Blowfish/RSA 密钥提供者）+ aud/wav（IMA/WS/MS-ADPCM）+ vqa/wsa + vxl/hva/idx + voc/r8/png_sheet + ogg(stb_vorbis)/mp3(minimp3)。
- **声音**（`src/sound/`）：五格式加载链名分派 + Sound 门面（Sound.cs 全文：通知池打断策略/音乐视频声道/Play2D 所有权表）。
- **UI 框架起步**（`src/ui/`）：widget（Widget.cs 全文：焦点三态/输入冒泡/LoadFieldOrProperty 虚链）、ui 门面（窗口栈双态所有权/悬停三态迁移）、chrome_metrics、widget_loader（名字分派注册表 + 异常文本逐字）。
- **Game 主循环骨架**（`src/game/game.cpp`）：RunTime 时钟/InnerLogicTick 双 TickTime 节拍（全经 Sync.RunUnsynced 门禁）/Loop 双时间表（MaxLogicTicksBehind=250 截断/MinReplayFps=10 强制帧）/Run 清理；Phase 5-8 依赖面 → Deps/钩子注入。

### Phase 5 — 主循环整合 + 核心 gameplay 🔨（~30%）

**第一批（2026-10-06）：Map 全量 + World 接线**

- `ora_terrain`（CellRamp 三角化/TilesByDistance/DefaultTerrain 双格式）+ `ora_map`（cell_region 零分配枚举器、cell_layer 双坐标索引 + 等距预滤怪癖、map_players 全 22 字段、**Map.cs 全量**：map.bin 三段读/SHA-1 UID 内容寻址/投影族/坐标换算族、MapCache 装载链）。
- **OPT-A8 落地**：SpatiallyPartitioned 的 Actor 键 slab 数组化（查询戳去重零分配）。
- Selection 全量；**D26/D27**：TraitRegistry（Info 名 → 工厂 → WorldArena 内构造）+ **World 全量重写**（系统 actor 解析/trait+actor 全入 arena/LoadComplete 与 Dispose 全序）；Game::StartGame 双入口装配。

**第二批（2026-10-07）：ActorMap/ControlGroups + Player 创建链 + UnitOrderGenerator + 寻路全套 + Health/Locomotor**

- core 增 priority_queue（"层级加倍"堆**原样保留**——平局弹出序进入寻路结果，对拍前提）+ long_bitset（全量位集）。
- sim 增 **ActorMap 全文**（影响层链表/位置缓存/Cell+Proximity 触发器）、ControlGroups 全文、**Player 全量重写**（ResolveFaction 随机阵营展开/关系掩码/FactionInfo 值袋解析）、trait_interfaces 批接口（IActorMap/INotifyDamage 族/IIssueOrder/BlockedByActor 等）、World 接线补全（DefaultOrderGenerator 注册表校验逐字/AddToMaps 全序/FogObscures 注入）。
- `src/mods/` 新族：create_map_players 全文、health 全文（**decimal 链走 OPT-A1 + 栈上定长缓冲 OPT-A9**；[VerifySync] HP 哈希注册）、unit_order_generator 全文（两轮目标器回退/InputOverridesSelection）、**寻路九件**：图类型校验逐字 + **OPT-A2 世代标记层池**（取出即 ++epoch 免 O(地图) 清零）+ 稠密图成员暂存缓冲 + PathSearch 全文 + **Locomotor 全文**（CellCache 三集合/CanMoveFreelyInto 阶梯/代价表 + 阻挡缓存）+ **HPF 全文**（BuildGrid 泛洪/AbstractGraphWithInsertedEdges/单源双向 + 多源单向/AbstractNodeForCost 高速公路延迟汇入）+ PathFinder 全文。
- 验收：新 path_test **41 检查全绿**（纯逻辑矩阵 + **真实 ra 地形全链**：代价表 → 全高墙 NoPath → 开口绕行 → 邻近快路径 → HPF 域查询）。

**剩余（~70%）**：Mobile + IPositionable/BodyOrientation（Locomotor 挂点换实）+ Move 活动族；武器链（Armament/AttackBase/AutoTarget/Warheads/Projectiles）；其余核心 trait 约 40+（Building/Production/PlayerResources/Harvester/Cloak/GainsExperience/Capturable 族/Conditions 34 个/Selectable 行为面/Render·WithSpriteBody 族/SpawnMapActors/Shroud+FrozenActorLayer/HitShape 行为面）；Settings/FieldSaver；MapPreview 异步面；SyncReport；录像录制。

### Phase 6 — UI + 本地化 🌱（~10%）

已完成：Widget 框架内核（树/焦点/冒泡/注册表）、Ui 门面、ChromeMetrics、WidgetLoader、光标管理器、声音门面。**未完成**：62 个 chrome widget + 131 个 Logic、Fluent 本地化（FluentProvider/FluentMessage）、WorldInteractionController/DefaultInputHandler 输入装配、chrome.yaml 资产接线。里程碑：主菜单 → 大厅 → skirmish 全 UI 流程可玩。

### Phase 7 — 网络与服务器 🌱（~20%）

已完成：Order 线协议/OrderManager 锁步/UnitOrders 分发/EchoConnection、Session 最小面。**未完成**：NetworkConnection 实体（读线程/Ack/TickScale）、Session 完整序列化与大厅协议、ReplayRecorder/ReplayConnection、SyncReport、Server（单线程事件循环/LobbyCommands 20 命令/Pinger 族）。验收：C++ 回放 C# 联机 replay；力争 C++ 客户端连 C# 服务器完成一局。

### Phase 8 — Lua 脚本、AI、战役、Utility ⬜

Lua 宿主 + 沙箱（内存/指令上限）+ gen/ 编译期绑定表；AI（ModularBot/11 模块/小队状态机/Mamdani 模糊推理）；战役流程；d2k/ts 内容补全；Utility 子集（`--map-hash --check-yaml --resolved-rules --extract --docs`）。验收：官方战役可通关；`--lua-docs` 对拍；AI 对战 replay SyncHash 对拍。

### Phase 9 — 打磨与发布 ⬜

性能调优、打包发布、剩余平台兼容。

## 工程门禁

- **双构建**：ASan+UBSan 与 Release，ctest 20/20 全绿。
- **`import std;` 严控**：禁传统 std 头引入（`tools/std_import_check.py`，324 文件 PASS；白名单仅第三方 C 头）。
- **函数级裁剪**：`-ffunction-sections -fdata-sections` + `--gc-sections`。
- **UPSTREAM 溯源**：每个移植文件带上游 file:line 标注（`tools/upstream_check.py`，312 条 PASS）。
- **黄金对拍体系**：yaml（759 文件）/定点原语（60,883 行）/规则深解析（三 mod）/资产解码（6,737 行）/字形（216 个），oracle 双跑确定性验证。
- **双语注释**：全部代码中英双语注释。

## 与上游的差异（优化点与偏离登记）

### 有意优化（C++ 侧）

| 层面 | 上游 C# | C++ 侧 |
|---|---|---|
| 数据反射链 | `FieldLoader`/`ObjectCreator` 运行时反射 + `TypeDescriptor` 兜底 | `gen/` 编译期描述表（680 可加载类型，schema_dumper 从 C# 反射权威导出）+ 注册表工厂，无运行时反射 |
| 类型/接口查询 | `Dictionary<Type,…>` 哈希 + `GetInterfaces()` 反射枚举 | TypeId 键空间（6048 类型全名 → `uint16`）+ 平行数组二分 |
| SyncHash | `Reflection.Emit` 运行时 IL + `ConcurrentCache` 委托链 | `gen/sync_gen.cpp` 编译期生成（84 类型），零反射零委托间接 |
| 数值修正链 | C# `decimal`（128 位软十进制） | `__int128` 精确复刻（整除 = `(int)decimal` 向零截断；等价域论证见 `src/core/percent_modifiers.hpp`） |
| 内存管理 | GC（每帧 LINQ/闭包/装箱分配） | 分区 arena：`FrameArena`（帧末重置）+ `WorldArena`（整局 bump + 析构登记）；同步路径禁 `shared_ptr` |
| 集合参数 | `IEnumerable<T>`（LINQ 链 + 枚举器分配） | `std::span` 直传 / 插入序 vector / 回调模板 |
| 渲染提交 | 装箱消息队列 + 每调用 lock | 值类型命令 + SPSC 无锁字节环 + 统一渲染线程 |
| 空间索引 | `Dictionary<Actor,…>` 哈希桶 + HashSet 去重 | ActorID slab 数组直引 + per-query 查询戳（OPT-A8） |
| 寻路 | 每搜索 O(地图) 层清零 + 每展开 List 分配 | 世代标记层池（取出 ++epoch）+ 成员暂存缓冲；堆序保真（OPT-A2） |
| 枚举/异常 | 字符串 switch / 异常类型分散 | `enum class` + 集中 `YamlException`（消息文本仍逐字对齐） |

完整审阅证据（上游 file:line 锚点，40+ 项）见 [docs/UPSTREAM_CPP_REVIEW.md](docs/UPSTREAM_CPP_REVIEW.md)。

### 与上游不同步处（偏离登记摘要）

当前登记 **D1~D119**（全文见 [docs/COVERAGE.md](docs/COVERAGE.md)），按模块：

- **yaml/fs（D1~D9）**：异常类型统一 YamlException（消息逐字）、惰性枚举物化 vector、null/"" 键合流——合法输入下行为等价或不可观测；
- **meta/加载链（D10~D24）**：TypeConverter 兜底未实现（字段类型已全覆盖）、字典字段插入序 vector、三 mod 解析快照 C++ 侧固化（D24，工具恢复后可再对拍）；
- **sim/net（D25~D34）**：Initialize 观察者去重形态（受端幂等）、trait 工厂 + WorldArena 所有权（**D26/D27 已落地**）、Target.FromCell 换算（D28 已随 Map 解除）、UI 命令族静默（D29，Phase 6/7）、SyncReport 未接（D30，上游默认关闭）；
- **平台/渲染（D35~D57）**：命令缓冲替代装箱队列、统一线程模型、NPOT、KHR_debug、持久映射 VB/VAO 缓存/blend diff/单级合成；输入层形态适配；
- **文件格式族（D58~D95）**：Stream→SpanReader、zlib miniz、Save deflate 依实现、Pfim 双路径统一、BC4/5/DX10 拒绝、正则手写复刻、哈希域 ASCII、声音惰性工厂物化、ogg/mp3 后端替换（样点值非位精确规范，元数据等价）、vxl 行主序 + presence 标志、FreeType LP64 宏窗编入等；
- **精灵/UI/声音门面（D96~D100）**：帧与文件字节共持有（GC 显式等价）、名字分派注册表、Deps 注入面、.NET 异常文本等价抛；
- **主循环/UI（D101~D106）**：upper_bound 插入位（同稳定序）、子 widget unique_ptr 所有权、Mediator type_index 桶、ChromeLogic/WidgetTypeRegistry 双注册表、Ui::ResetAll 排水序反转（ASan 实证防悬垂）、Game 骨架 Deps/钩子面；
- **Map/World（D107~D113）**：DefaultTerrain 模板双视图、事件 → 回调表、MapPreview 异步面随 Phase 6、OPT-A8、TraitRegistry 工厂 + Ruleset 按表借用视图 + StartGame 装配；
- **第二批（D114~D119）**：priority_queue 三态 Compare 静态协议 + LongBitSet 进程级分配域、ActorMap 的 GC/集合/形状求值形态、Player 的 Shroud/FrozenActorLayer 解析面随其批、OPT-A2 语义论证（世代标记/暂存缓冲/插入序保真）、Health 工厂时点 + Session/input 最小承载面。

**尚未移植（Phase 5-8 范围）**：Mobile/IPositionable 一族、Move 活动、Weapons/Warheads/Projectiles、其余约 40 个核心 trait、Shroud/FrozenActorLayer、62 个 chrome widget + 131 Logic、Fluent、WorldInteractionController、NetworkConnection 实体、Session 完整协议、Replay、Server、Lua、AI、战役、Utility 子命令——详见上方各阶段"剩余"节。

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

The engine is rewritten line-by-line to match the exact semantics of the upstream OpenRA source code (baseline commit `b6fc03fcfa`), keeping data, map, and replay formats compatible with upstream. **Semantic equivalence is the first principle** (verified by golden differentials); deliberate C++-side optimizations are made on top, and every non-equivalence is registered in [docs/COVERAGE.md](docs/COVERAGE.md) (an unregistered deviation is treated as a bug).

Companion documents: [PORTING_PLAN.md](PORTING_PLAN.md) (the phased plan and status), [docs/COVERAGE.md](docs/COVERAGE.md) (the full deviation registry), [docs/UPSTREAM_CPP_REVIEW.md](docs/UPSTREAM_CPP_REVIEW.md) (the optimization-review evidence), [UPSTREAM.baseline](UPSTREAM.baseline) (the upstream baseline and sync procedure).

## Progress at a Glance

**Overall completion ≈ 45%** (weighted by the PORTING_PLAN phase budgets). The data/engine/rendering/platform layers (the first half) are complete and locked in by golden differentials; the project is currently in **Phase 5's gameplay surface** — once Mobile and the weapon chain land, the two critical gates (a playable skirmish milestone and the replay SyncHash differential) open up.

| Phase | Scope | Status | Done |
|---|---|---|---|
| Phase 0 | C++26 skeleton + fixed-point primitives | ✅ complete (2026-10-03) | 100% |
| Phase 1 | MiniYaml + file system | ✅ complete (2026-10-03) | 100% |
| Phase 2 | Metadata framework + data-loading chain | ✅ complete (2026-10-03) | 100% |
| Phase 3 | Simulation core + orders/lockstep | ✅ complete (2026-10-03) | 100% |
| Phase 4 | Platform + rendering + file formats + audio + the UI-framework start + the game main loop (15 batches) | ✅ complete (2026-10-06) | 100% |
| Phase 5 | Main-loop integration + core gameplay (Map/World/pathfinding/Health done, 2 batches) | 🔨 in progress (since 2026-10-07) | ~30% |
| Phase 6 | UI + localization | 🌱 started (the framework core is in place) | ~10% |
| Phase 7 | Network + server | 🌱 started (the lockstep core is in place) | ~20% |
| Phase 8 | Lua scripting + AI + campaigns + Utility | ⬜ not started | ~0% |
| Phase 9 | Polish + release | ⬜ not started | 0% |

### Verification gates achieved

- **MiniYaml byte-exact differential**: all 759 upstream mods yaml × both modes (discard/keep comments), `FromStream→WriteToString` **100% byte-identical** to the C# oracle (13.5 MB of golden data).
- **Fixed-point golden differential**: **60,883 lines matching the C# output line for line** (full-circle trigonometry, full MT19937 sequences, decimal truncation semantics, and more).
- **The three-mod deep rules parse**: ra 80,274 / cnc 49,416 / d2k 35,833 lines as **byte-for-byte frozen snapshots**; upstream `--check-yaml` exit=0 corroborates.
- **The asset-decode golden differential**: all mods .shp/.pal/.aud/.wav/.vqa/.wsa/.vxl/.hva/.idx/.voc/.r8 assets + synthetic fixtures, `tests/golden_formats.txt` **6,737 lines matching line for line**; **216 glyphs byte-identical** (the same freetype6.dll the C# oracle loads).
- **Lockstep determinism**: EchoConnection **10⁶ ticks on both builds (ASan+UBSan / Release), leak-free and desync-free**.
- **Standing gates**: ctest **20/20** green on both builds (ASan+UBSan and Release); strict `import std;` (**324 files**); UPSTREAM provenance tags (**312 entries**); the deviation registry **D1–D119**.

### Current focus (Phase 5, third installment)

Mobile + the IPositionable/BodyOrientation family (plugging the Locomotor hooks in) + the Move activities + the Weapons/Warheads start (Armament/Projectile, OPT-A9's zero-allocation firing chain). The milestones after that: **issuing build/move/fight orders on a skirmish map** → **the frame-by-frame replay SyncHash differential against a C#-recorded replay (the determinism capstone)**.

## Phase Details

### Phase 0 — the C++26 skeleton + fixed-point primitives ✅

clang `-std=c++26` + `import std;` (the precompiled std.cppm PCM) + CMake/Ninja + ctest; the determinism flags `-fwrapv -fno-strict-aliasing`. 12 headers in `src/core/`: the fixed-point primitives (WPos/WVec/WAngle/WRot/WDist/CPos/CVec/MPos/int2/Rectangle/Int32Matrix4x4/ISqrt) and MersenneTwister, rewritten statement-by-statement, fully `constexpr`, each file carrying a `// UPSTREAM:` provenance tag. Infrastructure additions: `percent_modifiers.hpp` (the C# decimal modifier chain reproduced exactly with `__int128`, OPT-A1) and `arena.hpp` (the FrameArena/WorldArena memory regions, §4.5).

### Phase 1 — MiniYaml + the file system ✅

- `src/yaml/`: a statement-by-statement rewrite of MiniYaml.cs (791 lines) — the line state machine (4-space/1-tab levels, `\#` escaping, `\ ` whitespace guards), string-pool interning, merge/inheritance resolution/`-Key` weak removals, normalized serialization, the mutable builders; `text.hpp` replicates .NET's `char.IsWhiteSpace`/`Trim` over UTF-8. All 28 upstream MiniYamlTest.cs cases ported with verbatim assertion texts.
- `src/fs/`: FileSystem (mount order as override priority, `'|'` explicit mounts, case-insensitive resolution), Folder, ZipFile (SharpZipLib→miniz, with subfolder views and nested zips), `MiniYaml::Load`.

### Phase 2 — the metadata framework + the data-loading chain ✅

`tools/schema_dumper --gen` authoritative export: **680 types (601 loadable) + 37 enums + 9 BitSet tagsets + the TypeId key space (6,048 full names) + the [VerifySync] member tables (84 types)**. `src/meta/` GenericValue/GeneratedRecord value bags with two-way typed memory transforms. The full loading chain: Manifest → ModData → Ruleset → ActorInfo (`@` instance names / interface-dependency topological order) → WeaponInfo; **all 24 LoadUsing loaders ported semantically**.

### Phase 3 — the simulation core + orders/lockstep ✅

- `src/sim/`: TraitDictionary (parallel arrays with binary search, payloads keyed by the TypeId upcast tables), Actor (the condition system / Initialize observer chains / idempotent frame-end Dispose), World (the tick ordering / the n-consecutive SyncHash formula), an Activity state machine, the Sync hash protocol (including the IL-reachable bool semantics), Target/Player/Effects/TypeDictionary/ActorInitializer.
- `src/net/`: byte-exact Order serialization (bit assertions + round-trip identity), OrderPacket/OrderIO, EchoConnection, the full OrderManager lockstep (three-stage TryTick / IsNetFrame throttling / frame validation), a runnable UnitOrders subset.

### Phase 4 — platform + rendering + file formats + audio (15 batches) ✅

- **The platform layer**: `third_party/` SDL2 2.32.10 / OpenAL-soft 1.24.3 / FreeType 2.14.1 (the same DLLs the C# oracle loads — the precondition for byte-exact glyphs); gl_loader (78 entry points table-driven, the KHR_debug callback), sdl2_window (a packed atomic geometry snapshot, lock-free getters), sdl2_input (the event pump: motion coalescing / X1X2 pseudo-keys / multi-tap detection), al_loader (25 entry points direct), openal_sound_engine (the whole of OpenAlSoundEngine.cs: the 256 pool / the 3-instance cap / the device-list quirk kept bug-compatible), freetype_font (**OPT-B2 real structs + an LP64 ABI surgery**, six offsets pinned by static_asserts), sdl2_hardware_cursor.
- **The render pipeline** (`src/gfx/`): gfx_command (**value-type commands + an SPSC lock-free byte ring** replacing the boxed message queue, OPT-A5) + render_thread (the unified threading model); shader/texture (OPT-A6: integer uniforms + RAII); the Sheet/SheetBuilder/Palette family + HardwarePalette (**OPT-A7 palette dirty rows**, incremental == full byte-for-byte); vertex_buffer (**glBufferStorage persistent-mapped triple-slot VBs + fences + the VAO cache**); sprite_renderer + renderer (**OPT-B1 single-pass compositing**: 3 clears + 2 blits → 1 + 1); WorldRenderer/the render collection (**the RenderItem POD + a frame arena + (zkey<<32)|i total-order sorting**); TerrainSpriteLayer (**the split-array composition equivalence**); the full Viewport (the zoom matrix / scroll clamping / the ramp-corner polygon hit); SpriteCache/SequenceSet/Animation/CursorManager/ChromeProvider.
- **The Westwood format family** (`src/formats/`, all six installments): the codecs (LCW/XOR/RLE0/LZO/CRC32) + SHP TD/D2/TS + TMP TD/RA/TS + the **in-house Png** (upstream's 592 lines kept verbatim) + Tga/Dds (Pfim semantics) + ShpRemastered + the mix package (Blowfish/the RSA key provider) + aud/wav (IMA/WS/MS-ADPCM) + vqa/wsa + vxl/hva/idx + voc/r8/png_sheet + ogg (stb_vorbis)/mp3 (minimp3).
- **Sound** (`src/sound/`): the five-format load chain with name dispatch + the Sound facade (the whole of Sound.cs: the notification-pool interrupt policies / the music and video channels / the Play2D ownership list).
- **The UI-framework start** (`src/ui/`): widget (the whole of Widget.cs: the focus trio / input bubbling / the LoadFieldOrProperty virtual chain), the Ui facade (the window stack's dual-state ownership / the hover three-state transitions), chrome_metrics, widget_loader (name-dispatch registries + verbatim exception texts).
- **The game main-loop skeleton** (`src/game/game.cpp`): the RunTime clock / InnerLogicTick's dual TickTime pacing (all under the Sync.RunUnsynced gate) / Loop's dual schedule (the MaxLogicTicksBehind=250 cutoff / the MinReplayFps=10 forced frame) / Run's cleanup; the Phase 5-8 dependency faces ride Deps/hooks.

### Phase 5 — main-loop integration + core gameplay 🔨 (~30%)

**First installment (2026-10-06): the full Map + the World wiring**

- `ora_terrain` (CellRamp triangulation / TilesByDistance / DefaultTerrain's dual formats) + `ora_map` (zero-allocation cell_region enumerators, cell_layer's dual indexing + the isometric pre-filter quirk, all 22 map_players fields, **the whole of Map.cs**: the three-segment map.bin read / the SHA-1 content-addressed UID / the projection family / the coordinate conversions, the MapCache loading chain).
- **OPT-A8 landed**: SpatiallyPartitioned's Actor key as slab arrays (query-stamp dedup, zero allocation).
- Selection in full; **D26/D27**: TraitRegistry (an Info name → factory → constructed inside the WorldArena) + **the World rewritten in full** (system-actor resolution / every trait and actor in the arena / the LoadComplete and Dispose orders); Game::StartGame's dual-entry assembly.

**Second installment (2026-10-07): ActorMap/ControlGroups + the player-creation chain + UnitOrderGenerator + the full pathfinding suite + Health/Locomotor**

- core gains priority_queue (the "levels-doubled" heap **kept verbatim** — the tie pop order enters path results, the differential precondition) + long_bitset (the full bit-set).
- sim gains the **whole of ActorMap** (influence linked lists / position caches / Cell+Proximity triggers), the whole of ControlGroups, a **full Player rewrite** (ResolveFaction's random-faction expansion / the relationship masks / FactionInfo parsed from the bag), the batch trait_interfaces (IActorMap / the INotifyDamage family / IIssueOrder / BlockedByActor and more), and the completed World wiring (the DefaultOrderGenerator registry checks verbatim / the full AddToMaps order / the FogObscures injection).
- A new `src/mods/` family: the whole of create_map_players, the whole of health (**the decimal chain via OPT-A1 + a stack fixed buffer OPT-A9**; the [VerifySync] HP hash registration), the whole of unit_order_generator (the two-round targeter fallback / InputOverridesSelection), and the **nine pathfinding files**: the graph-type checks verbatim + the **OPT-A2 generation-stamped layer pool** (checkout is ++epoch with no O(map) clear) + the dense graph's member scratch buffer + the whole of PathSearch + the whole of **Locomotor** (the CellCache trio / CanMoveFreelyInto's ladder / the cost table + blocking cache) + the whole of **HPF** (BuildGrid flood fill / AbstractGraphWithInsertedEdges / single-source bidirectional + multi-source unidirectional / AbstractNodeForCost's highway-merge delay) + the whole of PathFinder.
- Acceptance: the new path_test **all 41 checks green** (pure-logic matrices + the **real-ra-terrain full chain**: the cost table → the full-height wall NoPath → the gap detour → the adjacency fast path → the HPF domain queries).

**Remaining (~70%)**: Mobile + IPositionable/BodyOrientation (plugging the Locomotor hooks in) + the Move activities; the weapon chain (Armament/AttackBase/AutoTarget/Warheads/Projectiles); ~40 more core traits (Building/Production/PlayerResources/Harvester/Cloak/GainsExperience/the Capturable family/the 34 Conditions traits/the Selectable behavior face/the Render·WithSpriteBody family/SpawnMapActors/Shroud+FrozenActorLayer/the HitShape behavior face); Settings/FieldSaver; the MapPreview async face; SyncReport; replay recording.

### Phase 6 — UI + localization 🌱 (~10%)

Done: the Widget-framework core (tree/focus/bubbling/registries), the Ui facade, ChromeMetrics, WidgetLoader, the cursor manager, the sound facade. **Remaining**: the 62 chrome widgets + 131 Logics, Fluent localization (FluentProvider/FluentMessage), WorldInteractionController/the input assembly, the chrome.yaml asset wiring. Milestone: main menu → lobby → a playable skirmish UI flow.

### Phase 7 — network + server 🌱 (~20%)

Done: the Order wire protocol / the OrderManager lockstep / the UnitOrders dispatch / EchoConnection / the minimal Session face. **Remaining**: the real NetworkConnection (the read thread / Acks / TickScale), full Session serialization + the lobby protocol, ReplayRecorder/ReplayConnection, SyncReport, the Server (a single-threaded event loop / the 20 LobbyCommands / the pinger family). Acceptance: a C++ replay of a C# multiplayer game; the stretch goal — a C++ client finishing a match on a C# server.

### Phase 8 — Lua scripting, AI, campaigns, Utility ⬜

The Lua host + sandbox (memory/instruction caps) + the gen/ compile-time binding tables; the AI (ModularBot / 11 modules / squad state machines / Mamdani fuzzy inference); campaign flow; d2k/ts content completion; the Utility subset (`--map-hash --check-yaml --resolved-rules --extract --docs`). Acceptance: official campaigns completable; the `--lua-docs` differential; an AI-vs-AI replay SyncHash differential.

### Phase 9 — polish + release ⬜

Performance tuning, packaging, remaining platform compatibility.

## Project Gates

- **Dual builds**: ASan+UBSan and Release; ctest 20/20 green on both.
- **Strict `import std;`**: no classic std-header includes (`tools/std_import_check.py`; 324 files PASS; the whitelist covers only third-party C headers).
- **Function-level dead-code elimination**: `-ffunction-sections -fdata-sections` + `--gc-sections`.
- **UPSTREAM provenance**: every ported file carries upstream file:line tags (`tools/upstream_check.py`; 312 entries PASS).
- **The golden-differential system**: yaml (759 files) / the fixed-point primitives (60,883 lines) / the deep rules parse (three mods) / asset decoding (6,737 lines) / glyphs (216), with the oracle's cross-run determinism verified.
- **Bilingual comments** throughout.

## Differences from upstream (optimizations & registered deviations)

### Deliberate optimizations (C++ side)

| Area | Upstream C# | C++ side |
|---|---|---|
| Data reflection chain | `FieldLoader`/`ObjectCreator` runtime reflection + a `TypeDescriptor` fallback | `gen/` compile-time descriptor tables (680 loadable types, exported authoritatively from C# reflection by schema_dumper) + registry factories; no runtime reflection |
| Type/interface queries | `Dictionary<Type,…>` hashing + `GetInterfaces()` reflection | the TypeId key space (6,048 full names → `uint16`) + parallel-array binary search |
| SyncHash | `Reflection.Emit` runtime IL + a `ConcurrentCache` delegate chain | `gen/sync_gen.cpp` generated at compile time (84 types); zero reflection, zero delegate indirection |
| Percentage modifiers | the C# `decimal` chain (128-bit soft decimal) | exact `__int128` reproduction (integer division = `(int)decimal` truncation towards zero; the equivalence-domain argument lives in `src/core/percent_modifiers.hpp`) |
| Memory management | GC (per-frame LINQ/closure/boxing allocations) | partitioned arenas: `FrameArena` (frame-end reset) + `WorldArena` (per-world bump + destructor records); no `shared_ptr` on the synced path |
| Collection parameters | `IEnumerable<T>` (LINQ chains + enumerator allocations) | `std::span` direct / insertion-ordered vectors / callback templates |
| Render submission | a boxed message queue + a lock per call | value-type commands + an SPSC lock-free byte ring + a unified render thread |
| Spatial indexes | `Dictionary<Actor,…>` hash buckets + HashSet dedup | ActorID slab arrays + per-query stamps (OPT-A8) |
| Pathfinding | an O(map) layer clear per search + a List allocation per expansion | generation-stamped layer pools (checkout ++epoch) + member scratch buffers; the heap order kept (OPT-A2) |
| Enums / exceptions | string switches / scattered exception types | `enum class` + a unified `YamlException` (message texts still match verbatim) |

The full review evidence (upstream file:line anchors, 40+ items) lives in [docs/UPSTREAM_CPP_REVIEW.md](docs/UPSTREAM_CPP_REVIEW.md).

### Not-yet-synced with upstream (the registered-deviation summary)

Currently **D1–D119** (full texts in [docs/COVERAGE.md](docs/COVERAGE.md)), by module:

- **yaml/fs (D1–D9)**: exceptions unified into YamlException (texts verbatim), lazy enumerations materialized, null/"" key coalescing — behavior-equivalent for valid inputs;
- **meta/loading chain (D10–D24)**: the TypeConverter fallback not implemented (field types fully covered), insertion-ordered dictionary fields, the three-mod parse snapshots frozen on the C++ side (D24; re-differential once the tool is restored);
- **sim/net (D25–D34)**: the Initialize observer-dedup shape (idempotent receivers), the trait factory + WorldArena ownership (**D26/D27 landed**), Target.FromCell's conversion (D28 unlocked with Map), the UI command families silenced (D29, Phase 6/7), SyncReport unwired (D30, off upstream by default);
- **platform/render (D35–D57)**: the command buffer, the unified threading model, NPOT, KHR_debug, persistent-mapped VBs / the VAO cache / blend diff / single-pass compositing; the input-layer shape adaptations;
- **file formats (D58–D95)**: Stream→SpanReader, zlib via miniz, Save's implementation-defined deflate, the Pfim dual-path unification, the BC4/5/DX10 rejection, the hand-written regex reproductions, the ASCII hash domains, the materialized sound lazy factories, the ogg/mp3 backend swaps (sample values are not a bit-exact specification; metadata equivalent), vxl's row-major + presence flags, the FreeType LP64 macro window, and more;
- **sprite/UI/sound facades (D96–D100)**: frames co-owning the file bytes (the explicit GC equivalent), the name-dispatch registries, the Deps injection faces, the .NET exception-text equivalent throws;
- **main loop/UI (D101–D106)**: the upper_bound insertion point (the same stable order), child widgets as unique_ptr ownership, the Mediator's type_index buckets, the ChromeLogic/WidgetTypeRegistry dual registries, Ui::ResetAll's drain-order reversal (ASan-proven dangling prevention), the Game skeleton's Deps/hook faces;
- **map/world (D107–D113)**: DefaultTerrain's template dual views, events → callback lists, the MapPreview async faces deferred to Phase 6, OPT-A8, the TraitRegistry factory + Ruleset's per-table borrowed views + the StartGame assembly;
- **the second installment (D114–D119)**: priority_queue's three-way Compare static protocol + LongBitSet's process-level allocation domain, ActorMap's GC/collection/shape-evaluation forms, Player's Shroud/FrozenActorLayer resolution faces riding their batches, the OPT-A2 semantic argument (generation stamps / scratch buffers / order fidelity), Health's factory timing + the Session/input minimal carriers.

**Not yet ported (the Phase 5-8 scope)**: the Mobile/IPositionable family, the Move activities, Weapons/Warheads/Projectiles, ~40 more core traits, Shroud/FrozenActorLayer, the 62 chrome widgets + 131 Logics, Fluent, WorldInteractionController, the real NetworkConnection, the full Session protocol, replay, the server, Lua, AI, campaigns, and the Utility subcommands — see each phase's "remaining" section above.

## License & Attribution

This project is a derivative work of OpenRA and is released under the **GPL-3.0**, the same license as upstream. See [LICENSE](LICENSE).

- Upstream copyright: Copyright (c) OpenRA Developers and Contributors
- Rewrite code in this project: Copyright (c) 2026 xfcyhuang

OpenRA, Command & Conquer, Red Alert, and Dune 2000 are trademarks of their respective owners; this project is not affiliated with them.
