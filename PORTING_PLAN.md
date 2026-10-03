# OpenRA → C++26 重写计划(OpenRACpp)

> 源码库:`Z:\source_code\OpenRA`,**基线锁定 commit `7d57605bca`**(2025 年 DEV 版,SDL2 + OpenGL 3.2Core/GLES3 + OpenAL + FreeType)。
> 目标:在 `Z:\source_code\OpenRACpp` 用 **C++26** 按 `Z:\source_code\cpp26.md` 规范完全重写引擎,数据/联机/回放兼容现有 mods、地图与 replay,并**持续与上游语义同步**(§8)。
> 工具链(锁定):**Clang 23.1.0**(llvm-mingw/UCRT/ld.lld,满足规范"Clang 21+")+ CMake ≥ 4.3 + Ninja;`-std=c++26 -fwrapv -fno-strict-aliasing`。
> 前身:本仓库 c260fc5..2850482 的 C23 Phase 0 成果(定点原语 + MersenneTwister,60,883 行黄金对拍全绿)**留在 git 历史中可回收**,黄金数据与对拍器直接复用,原语代码机械转换为 C++26。

---

## 1. 源码库理解总结

### 1.1 规模与构成(上游 @7d57605bca)

| 组件 | 文件数 | 行数 | 职责 |
|---|---|---|---|
| OpenRA.Game | 224 | 44,012 | 引擎核心:主循环、反射、Trait 系统、网络、MiniYaml、Widget 基础 |
| OpenRA.Mods.Common | 1,085 | 159,235 | 仿真 trait(501 文件/约 418 个 TraitInfo)、Widgets、AI、脚本绑定 |
| OpenRA.Mods.Cnc | 140 | 20,784 | C&C/RA/TS 专属:文件格式(mix/shp/vqa/vxl…)、Cnc trait |
| OpenRA.Mods.D2k | 23 | 3,551 | Dune 2000 mod |
| OpenRA.Server | 1 | 124 | 专用服务器入口(薄壳,实现在 Game/Server) |
| OpenRA.Utility | 1 | 178 | 工具入口(45 个子命令在各程序集) |
| OpenRA.Platforms.Default | 18 | 4,814 | SDL2/OpenGL/OpenAL/FreeType 平台实现 |
| OpenRA.Test | 22 | 4,553 | NUnit 单测 |
| glsl/ + mods/ | 12 + 759 | — | 着色器与规则数据(直接复用) |

**合计约 25.7 万行 C#**。C# 依赖多数本就是 C API 的 P/Invoke 封装(SDL2/OpenAL/FreeType/Lua),C++ 重写后直连原生库,封装层消失。

### 1.2 运行时核心语义(重写必须等价的靶子,锚点见附录)

1. **主循环**:单线程,双绝对时间戳 `nextLogic`/`nextRender` 节流(Game.cs:811-813);逻辑落后 >250ms 放弃追帧;每逻辑 tick 至少渲染一帧;输入在渲染帧末尾采集,全部包在 `Sync.RunUnsynced` 内;逻辑 tick 序:`delayedActions → OrderManager.TickImmediate → TryTick(锁步放行) → world.Tick → world.TickRender`。
2. **数据驱动反射链**:MiniYaml → `FieldLoader.Load` 按字段名反射赋值(约 40 种类型解析器,FieldLoader.cs:754-896)→ `ObjectCreator.CreateObject` 按字符串类名实例化。承载 trait/武器/widget/chrome/设置/地图规则,是全项目地基。
3. **Trait 系统**:`ActorInfo` 共享规则模板(TraitInfo 拓扑排序构造序,ActorInfo.cs:104-167);`TraitDictionary` 以接口类型为键,平行数组按 ActorID 有序 + 二分查找(TraitDictionary.cs:145-327)。
4. **仿真确定性**:全整数定点(1 cell = 1024 世界单位,整圆 1024 单位查表);双 RNG(SharedRandom 同步 / LocalRandom UI,MersenneTwister 纯整数);每 tick 按 ActorID 升序遍历;`SyncHash` = Σ n·ActorID·hash(actor 的 ISync trait)(World.cs:482-512),哈希函数由 Reflection.Emit 生成(Sync.cs:78-107)。唯一 decimal 使用点:抛物线插值 WPos.cs:58-72。
5. **网络锁步**:线协议 `Int32 长度 + Int32 客户端ID + Int32 帧号 + Orders[]`;Order 首字节 0xFF = orderstring + Int16 位掩码 + 按 flag 字段(含 Actor 代际);Ack/Ping/SyncHash/TickScale/Disconnect/Handshake 控制帧;`NetFrameInterval=3` 三帧捆包;服务器把 order 投影 `frame += OrderLatency`;大厅命令走 `Order("Command",…)` 带内通道。
6. **渲染管线**:三级合成 world FBO(调色板查色 + HSV 色移 + gl_FragDepth)→ screen FBO(像素风放大)→ 默认帧缓冲;调色板打包 256×N 纹理;SpriteRenderer 8 纹理槽批渲染;地形静态大 VB 只传脏行;GL 封送独立渲染线程(ThreadedGraphicsContext,Windows 窗口模式例外)。
7. **平台层**:20 文件实现 IPlatform/IGraphicsContext/IShader/ITexture/IFrameBuffer/IFont/ISoundEngine 接口族;OpenAL 256 音源池 + 同源实例限制;FreeType 手算结构体偏移(C++ 侧换回真结构体)。
8. **文件格式**(纯字节解析,可直译):mix(可选 Blowfish)、shpTD/D2/TS/remastered、tmpRA/TD/TS、pal、vqa、wsa、aud(IMA ADPCM)、voc/wav、vxl/hva、lzo/lcw/xor、dds/tga、png(自研 592 行)、idx、map.bin。地图 = zip,UID = SHA1(流拼接)(Map.cs:285-313)。
9. **UI**:Widget 树,布局用 IntegerExpression 表达式;事件从最深子节点倒序尝试后冒泡;`Logic:` 节点反射实例化;UI 每 40ms 独立节拍;62 widget 类 + 131 Logic 类。
10. **Lua 脚本**:Lua 5.2(Eluant),沙箱(50MB 内存/100 万指令/去 math.random),反射暴露全部 public 成员(17 全局表 + ~35 属性组 + Trigger 事件);脚本在同步 tick 内执行。
11. **AI**:ModularBot 只能经 `IBot.QueueOrder` 改变世界;11 个 bot 模块;FuzzyLogic 仅一处(Mamdani ~200 行)。
12. **静态全局状态**:Game.ModData/Settings/Renderer/Sound/OrderManager/worldRenderer/…(需显式生命周期管理);切 mod 时全量重建(Game.cs:474-532)。

### 1.3 语义边界与策略

数千个 trait/activity/widget 的逐行语义不可能预先穷尽。策略:**结构先行 + 按需精读 + 黄金对拍**;每模块移植时精读对应源文件,以对拍验证语义等价(§10)。

---

## 2. 移植目标与兼容性基线

| # | 目标 | 验收 |
|---|---|---|
| G1 | **数据兼容**:mods/ 759 个 yaml、地图(.oramap)、设置、fluent 资源零修改可用 | 解析 + resolved-rules 逐字节对拍 |
| G2 | **回放兼容**:C# 录制的 replay 在 C++ 版回放,逐帧 SyncHash 一致 | replaydiff 工具 = 0 差异 |
| G3 | **联机兼容**:C++ 客户端可加入 C# 服务器(线协议逐字节对齐) | 力争目标,以 G2 为硬性验收 |
| G4 | **行为等价**:相同输入产生相同世界状态演化(伤害/寻路/AI 决策序列) | replay 对拍 + 单测 |
| G5 | **工程质量**:无 UB、ASan/UBSan 干净、符合 cpp26.md 规范、CI 绿 | 全阶段持续 |

**范围裁剪**(一期):DiscordRichPresence、TagLibSharp 不移植;Utility 先做 `--map-hash --check-yaml --resolved-rules --extract --docs` 子集;GLES3/ANGLE 二期;master server 认证(RSA/PEM/GeoIP/UPnP/LAN beacon)三期。Remastered shp 加载器保留(纯格式解析,代价低)。

---

## 3. 总体策略

**自底向上 + 持续对拍 + 权威 schema 导出 + 上游同步闭环**:

1. **依赖倒置先行**:先移植无依赖叶子(定点原语、MiniYaml、文件格式),再逐层上移;每阶段结束存在**可运行的验收程序**。
2. **权威 schema 导出器**(压缩工期最大杠杆):利用上游 C# 程序集自带反射,写 `tools/schema_dumper`(跑在 .NET 上,非交付运行时依赖),把全部 TraitInfo/Widget/Settings 类的**字段元数据 + 类型注册表 + [VerifySync] 标记 + Lua API 表**导出为 C++26 `constexpr` 源码(`gen/`)。生成器只依赖 C# 元数据,天然正确;以静态断言防漂移。
3. **逐字节/逐帧对拍**:MiniYaml 规范化输出、resolved-rules、replay SyncHash、地图 UID、资产解码哈希、渲染截图差异 —— 全部以**上游 C# 版为黄金基准**。
4. **上游同步闭环**(§8):基线锁定 + 覆盖登记 + 黄金基准再生 + 阶段末批量 rebase。

---

## 4. C# → C++26 概念映射

### 4.1 类型系统与 OOP

| C# 概念 | C++26 方案 |
|---|---|
| class + virtual | `class` + `virtual`/`override`/`final` 直接对应(比 C23 方案直接得多) |
| interface(查询用) | 纯虚类;**TraitDictionary 键不用 RTTI**——每个 trait 接口给 `static constexpr uint16 iface_id`,编译期集中分配(C++26 静态反射 P2996 在 Clang 23 主线不可用,见 cpp26.md 注),保持平行数组 + ActorID 二分语义 |
| abstract / sealed | 纯虚函数 / `final` |
| 继承(单继承) | 单继承 + `[[no_unique_address]]` 零开销上转;多接口 trait 用多继承纯虚基类 |
| 泛型集合 | `std::vector` / `std::span` / `std::inplace_vector<N>`(规范:已知容量小集合栈优先);有序遍历场景用 `std::map`(等价 SortedDictionary 的稳定序) |
| Nullable | `std::optional<T>`;可空引用 = 指针/`std::optional<引用包装>` 按场景 |
| delegate / event | `std::function`(规范:严禁函数指针)+ 订阅表;**成对注销**硬规则(对应 InitializeMod 清空事件) |
| LINQ / 闭包 | `std::ranges`;闭包 = lambda(移动捕获大对象) |
| string | `std::string` / `std::string_view`(UTF-8);MiniYaml 字符串池保留 interning 语义 |
| enum | `enum class : int32_t`;**凡进 Order 序列化的枚举按 int32 语义显式读写** |
| `readonly`/`init` | `const` 成员 + 构造初始化约定 |
| 属性(property) | getter 成员函数;POD 数据类直接公有字段(配合描述表加载) |

**命名落地(对齐 cpp26.md,且服务语义同步)**:类型名**保留上游拼写**(`WPos`/`WAngle`/`World`/`Actor`/`TraitDictionary`),与 C# 一一对应是审计线索;函数 `snake_case` 或保留上游 `PascalCase` 方法名(推荐保留,便于对照源码);变量按规范加类型前缀(`wpos_origin`、`vec_actors`、`str_name`、`b_alive`、`uint4_id`);常量 `kCamelCase`;2 空格缩进 K&R;`#pragma once`。

### 4.2 反射链的替代(第一支柱)

C++26 契约与静态反射在 Clang 23 均未落地(cpp26.md 实测表),运行时按字符串加载**必须生成代码**:

- `gen/interfaces.h`:接口 ID 表(集中 `enum : uint16`,schema_dumper 导出,手写禁改);
- `gen/*_meta.hpp`:每个 Info/Widget/Settings 类的 `constexpr` 字段描述表:`struct FieldDesc { std::string_view name; FieldType type; ptrdiff_t offset; bool required; }`,配合约 40 种字段类型解析器(含 `"50%"`、逗号元组、表达式求值器)构成 `FieldLoader` 等价物;
- `gen/registry.cpp`:`std::map<std::string_view, factory>`(factory = 返回 `std::unique_ptr<…>` 的 `std::function`),等价 ObjectCreator;
- `gen/sync_hash.*`:每个 ISync 类型的哈希函数(字段集与 C# `[VerifySync]` 完全一致);
- `gen/lua_api.cpp`:`luaL_Reg` 绑定表(Phase 8 用)。

C++ 侧自有新代码可用聚合初始化 + structured bindings + concepts 做有限度的编译期字段遍历(仅自有叶子类型),但**对上游既有类型一律走导出表**,不双轨。

### 4.3 契约的过渡落地

cpp26.md 指出 `pre`/`post`/`contract_assert` 关键字 Clang 未实现。定义过渡宏(接口即契约,编译器支持后一行切换):

```cpp
#define ORA_PRE(cond)    assert(cond)          // Debug 断言;Release 侧另配 [[assume(cond)]] 提示(仅无副作用条件)
#define ORA_POST(r, cond) assert(cond)
#define ORA_CONTRACT_ASSERT(cond) assert(cond)
```

### 4.4 模块策略

- 标准库一律 `import std;`;自有代码按模块划分:`export module ora.core.geom;` 等(§5 布局一一对应);
- C 宏头(SDL2/GL/OpenAL/`<cassert>` 等,宏不穿越 import 边界)在各包装模块的 **global module fragment** 中 `#include`,再 export 类型化接口——平台差异只允许出现在 `ora.platform.*`;
- 第三方无模块支持的库同样经包装模块隔离;`import` 与 `#include` 同 TU 混用合法,但新代码默认 import。

### 4.5 内存模型(替代 GC,分区而治)

| 区域 | 策略 | 对应 C# 对象 |
|---|---|---|
| 进程级(引擎/ModData/Settings) | `std::unique_ptr` 显式所有权,严格按 `Game.Run` 退出序销毁(OrderManager → worldRenderer → ModData → Chrome → Sound → Renderer,Game.cs:914-922) | ModData、Renderer、Sound |
| 每局世界 | **World arena**:开局一次分配、整局 bump、结束整块释放;actor/trait/activity/effect 全在其中(规范允许的 arena 例外:容器内构造/析构由 arena 记录调用) | Actor、Trait、Activity、Effect |
| 帧临时 | 线性帧分配器(渲染收集列表、排序 key),帧末重置 | 每 tick 临时 List |
| 资产(Sheet/纹理/序列) | `std::shared_ptr` + ModData 级资产表;切 mod 统一释放 | Sheet、ITexture、序列 |
| 字符串 | MiniYaml interning 池 + 字面量 `string_view`;World 级 strtab | interned string |

纪律:trait/actor 销毁只发生在帧末队列(World.cs:453-455 既有语义 → 硬规则);ActorID + Generation 代数保留;禁止全局缓存 actor 裸指针;**同步路径禁 `shared_ptr` 原子引用计数**(跨线程计数不确定)——sim 内一律 arena 裸引用;CI 全程 ASan/UBSan。

### 4.6 错误处理(对齐 cpp26.md 第三章)

- 加载期可恢复错误:`std::expected<T, LoadError>`(YamlException/MissingFieldsException 等价物,`LoadError{code, str_msg, src_loc}`);
- 性能关键路径:`std::error_code` + `[[nodiscard]]`;
- 运行期致命(违反不变量):`abort` 带上下文 dump;析构/释放 `noexcept`。

### 4.7 确定性保障(移植生命线,与 C23 计划同源)

1. **符号溢出**:C# unchecked int 溢出 = 回绕;C++ signed 溢出是 UB。全工程强制 **`-fwrapv`**(写进 CMake 基线),关键运算可用 `uint32_t` 往返显式化;
2. **移位**:`>>` 负数 = 算术右移,启动自检 `-1 >> 1 == -1`;
3. **浮点**:同步路径本就无浮点(§1.2-4);残留 double 仅在 RunUnsynced(AI/渲染/音频);CI lint 检查 sim/ 无 `double/float`;唯一 decimal 点(WPos.LerpQuadratic)用 `__int128` 通分除法精确复刻(已验证,C23 成果可回收);
4. **SyncHash**:gen/ 生成的哈希注册表 + World.SyncHash 整数组合公式;
5. **RNG**:MersenneTwister 纯整数,逐行照抄 + 同种子序列对拍(已验证);
6. **遍历顺序**:SortedDictionary → `std::map`;LINQ OrderBy → `std::stable_sort`;凡"顺序影响结果"处精读确认;
7. **locale/编码**:解析路径禁 locale 依赖,`std::from_chars` 无 locale(规范亦要求)。

### 4.8 并发模型

| C# | C++26 |
|---|---|
| ThreadedGraphicsContext(渲染线程) | 保留架构:无锁 SPSC 命令队列封送 GL 调用;Windows 窗口模式主线程特例保留(Sdl2PlatformWindow.cs:346-353) |
| 音频流异步加载(Task) | `std::jthread` + 条件变量,语义照抄(挂静音 buffer → 后台填充 → 换 buffer) |
| 服务器单线程事件循环 + 每连接读线程 | 同构;`BlockingCollection` → 有界阻塞队列 |
| — | `std::execution` Sender/Receiver 在 libstdc++/libc++ 尚未落地(cpp26.md 实测),**一期不依赖**,以 jthread + 队列实现,接口留升级空间 |

### 4.9 输出与转换(对齐 cpp26.md)

`std::print`/`std::println` 替代 cout;`std::to_chars`/`from_chars` 替代 `to_string`/`stoi`(无异常无 locale,亦是确定性要求);仅输出用缓冲区传 `char*`/`string_view`。

---

## 5. 目标架构与代码布局

```
OpenRACpp/
├── CMakeLists.txt            # clang++ -std=c++26 -fwrapv -fno-strict-aliasing
│                             # -Wall -Wextra -Wpedantic;ORA_SANITIZE 开关;模块编译
├── PORTING_PLAN.md / UPSTREAM.baseline / docs/COVERAGE.md   # §8 同步机制
├── src/                      # 每目录 = C++20 命名模块 ora.<子域>.<名>
│   ├── core/                 # ora.core.*:定点原语(WPos/WVec/WDist/WAngle/WRot/CPos/MPos/int2/
│   │                         #   BitSet)、MersenneTwister、CRC32/SHA1、日志、arena/帧分配器
│   ├── yaml/                 # ora.yaml:MiniYaml 状态机/Merge/ResolveInherits/字符串池
│   ├── meta/                 # ora.meta:字段描述框架 + 约 40 种解析器(FieldLoader 等价)、
│   │                         #   FieldSaver 等价(设置 diff 保存)
│   ├── fs/                   # ora.fs:挂载/索引/大小写规整、Folder、Zip(miniz)、
│   │                         #   Mix(Blowfish)、Meg/Big/Pak/ISO9660
│   ├── formats/              # ora.fmt.*:shp*/tmp*/pal/vqa/wsa/aud/voc/wav/ogg/mp3/vxl/hva/
│   │                         #   idx/lcw/lzo/xor/dds/tga/png/geoip/map.bin
│   ├── gfx/                  # ora.gfx:Renderer 三级合成、Sheet/SheetBuilder、HardwarePalette、
│   │                         #   SpriteRenderer、TerrainSpriteLayer、WorldRenderer、Renderable 排序
│   ├── platform/             # ora.platform.*:SDL2/GL(glad)/OpenAL/FreeType 包装模块
│   │                         #   (global module fragment #include C 头)、ThreadedGraphicsContext
│   ├── sim/                  # ora.sim:Actor/ActorInfo/World/TraitDictionary/Activity/条件系统/
│   │                         #   SyncHash/Effects/Ruleset/WeaponInfo/OrderGenerator
│   ├── net/                  # ora.net:Order 序列化/OrderIO/OrderManager/Connection 三态
│   │                         #   (Echo/Network/Replay)/Session/ReplayRecorder
│   ├── server/               # ora.server:事件循环、ServerTraits、专用服务器入口
│   ├── ui/                   # ora.ui:Widget 框架/布局表达式/事件路由/chrome/控制器
│   ├── script/               # ora.script:Lua 宿主、沙箱、绑定层(gen/ 生成)
│   ├── audio/                # ora.audio:Sound 门面、音源池、流式播放
│   ├── l10n/                 # ora.l10n:Fluent 子集或绑定、FluentMessage 网络格式
│   ├── game/                 # ora.game:main、Game 编排、Settings/热键/LoadScreen/InstalledMods
│   └── mods/                 # ora.mods.common / cnc / d2k:trait/widget/logic/bot/script + 注册
├── gen/                      # schema_dumper 输出(constexpr 描述表/接口 ID/同步哈希/Lua API),纳管
├── tools/
│   ├── schema_dumper/        # C# 反射 → gen/(跑在 .NET 上,非运行时依赖)
│   ├── golden_gen/           # C# 黄金数据生成器(已有一版在 git 历史 2850482)
│   ├── replaydiff/           # replay 逐帧 SyncHash 对拍器
│   └── upstream_check/       # §8 漂移检测
├── tests/                    # 单测(Catch2)+ 黄金对拍夹具(golden_core.txt 可从历史回收)
├── third_party/              # SDL2、OpenAL-soft、freetype、lua、miniz、minimp3、stb_vorbis、
│                             #   glad、spng、BearSSL/mbedTLS(三期)
└── packaging/
```

**每文件头部强制标注**:`// UPSTREAM: OpenRA.Game/WPos.cs @7d57605 L59-99`(审计线索 + §8 覆盖登记的数据源)。

---

## 6. 第三方依赖选型

| C# 依赖 | C++ 替代 | 说明 |
|---|---|---|
| OpenRA-SDL2-CS | **SDL2 原生** | 直连,P/Invoke 层消失 |
| 手写 GL 绑定(OpenGL.cs 801 行) | **glad**(GL 3.2 Core + GLES3 双 profile) | 函数子集 = 两者交集 |
| OpenRA-OpenAL-CS | **OpenAL-soft** | 原生 |
| OpenRA-Freetype6 | **FreeType 原生** | 手算偏移换回真结构体 |
| Eluant(Lua 5.2) | **Lua 5.4**(回归测试兜底)或 5.3 | 5.2 停维护 |
| Linguini(Fluent) | 自研子集(未用 isolating,面小)或 fluent 绑定 | FluentMessage 网络格式必须保留 |
| SharpZipLib | **miniz** | zip 读写 |
| NVorbis / MP3Sharp | **stb_vorbis / minimp3** | 现成 C 库 |
| Pfim(TGA/DDS) | 照抄(~200 行) | 直接移植 |
| Png.cs(自研 592 行) | 照抄自研(含 Indexed8 写) | 保行为一致 |
| Mono.NAT / BeaconLib | libminiupnpc / 自研 UDP 广播(三期) | |
| HttpWebRequest / RSA/PEM | libcurl / BearSSL 或 mbedTLS(三期) | fingerprint=SHA1(n‖e) 必须一致 |
| FuzzyLogicLibrary | 自实现 Mamdani(~200 行) | 仅一处使用 |
| NUnit | **Catch2**(cpp26.md 推荐) | 移植 OpenRA.Test 核心用例 |

---

## 7. 上游语义同步机制(本项目特有要求)

> 原则:**语义同步不是"追 HEAD",而是"每个已移植模块随时可证明与某个上游 commit 等价"**。

### 7.1 基线锁定

- 仓库根 `UPSTREAM.baseline` 记录:`repo=Z:\source_code\OpenRA`、`commit=7d57605bca`、日期、mods/glsl 资产快照哈希;
- 一切黄金数据(golden_gen 输出)由**该 commit 的 C# 构建**生成,与数据一同入库,标注生成时的 commit;
- 上游工作区有本地改动(`M .gitignore`、未跟踪 `project.md`),以 commit 为准,忽略工作区脏文件。

### 7.2 覆盖登记表(COVERAGE)

- `docs/COVERAGE.md`:按上游文件路径列出"上游文件 → C++ 模块 → 状态(未开始/进行中/已对拍/已偏离+理由)→ 最后同步 commit";
- 数据源 = 扫描 `src/` 全部 `// UPSTREAM:` 头标注,由 `tools/upstream_check` 自动生成/校验,CI 运行;
- **已移植文件不允许没有 UPSTREAM 标注**(lint 强制)。

### 7.3 黄金基准再生

- `tools/golden_gen`(C# 工程,跑在 .NET 上):输入 = 上游源码构建,输出 = 定点原语全量值、MiniYaml 规范化输出、resolved-rules dump、replay SyncHash 序列、资产解码哈希、地图 UID;
- 上游更新后流程:**checkout 新 commit → 重新构建 → 重新生成全部黄金数据 → C++ 侧重跑对拍**,差异即"上游语义变化 ∪ 移植偏差",逐条归因。

### 7.4 上游更新 SOP(阶段末批量,不追 HEAD)

1. `git -C ../OpenRA fetch && git diff --stat <old> <new>` 按文件分类:未触及/已移植/新增/删除;
2. 已移植文件:精读 diff → 移植行为变更 → 更新该文件 UPSTREAM 标注与 COVERAGE 状态 → 重跑该模块对拍;
3. 新增文件:若属于已移植模块的邻接语义(如同目录新 trait),评估是否纳入本阶段范围;
4. 全绿后:更新 `UPSTREAM.baseline`,提交"sync upstream <old>..<new>";
5. 频率:每阶段末强制一次;上游 hotfix(如协议安全修复)可插入紧急同步。

### 7.5 偏离登记

凡 C++ 侧无法或有意不逐语义等价处(如三期裁剪项、bug 兼容决策),在 COVERAGE 标注 `已偏离` 并写理由;**未登记的偏离视为 bug**。

---

## 8. 分阶段实施计划

> 原则:阶段编号 = 依赖顺序;每阶段结束有可运行验收程序;每阶段末执行一次 §7.4 上游同步。
> 图例:【对拍】黄金比对;【单测】Catch2;【里程碑】人肉可玩/可验证。

### Phase 0 — C++26 工程骨架 + core 基础库(约 2~3 周)

- CMake 基线:clang++ `-std=c++26 -fwrapv -fno-strict-aliasing -Wall -Wextra -Wpedantic`,Ninja,`import std;` 模块编译探针(特性可用性以探针为准,如 pack 结构化绑定、inplace_vector、expected、print、byteswap constexpr);
- 已知限制落库:`std::execution`/contracts 不可用(cpp26.md 实测)→ §4.3/§4.8 过渡方案;Windows 侧 `<thread>` 可用性以探针为准;
- **回收 C23 成果**(git 2850482):定点原语、MT19937、60,883 行黄金数据、golden_gen —— 原语机械转 C++26(类 + constexpr 化 + 规范命名),对拍数据直接复用;
- core/ 补齐:BitSet、CRC32/SHA1、日志(`std::print` 后端)、arena/帧分配器;
- third_party 子树接入(SDL2/OpenAL/freetype/lua/miniz 源码式纳入);
- **验收**:探针 ctest 绿;定点原语 + MT 黄金对拍 100% 一致(复用历史数据);ASan+UBSan 干净;`UPSTREAM.baseline`/COVERAGE/upstream_check 就位。

### Phase 1 — MiniYaml + FileSystem(约 2~3 周)

- MiniYaml.cs(791 行)逐行移植:行状态机(4 空格或 1 tab、`\#` 转义、值裁剪与 `\ ` 保护)、`Merge/MergeSelfPartial`、`ResolveInherits`(循环检测)、`-Key` 弱删除顺序语义(MiniYaml.cs:613-647)、interning 池、SourceLocation;
- FileSystem:挂载顺序=覆盖优先级(FileSystem.cs:193-199)、大小写不敏感索引、Folder、ZipFile(miniz);
- **验收【对拍·第一关卡】**:759 个 mods yaml 解析 + `MiniYaml.ToString` 规范化输出逐文件 diff = 0;三 mod `--resolved-rules` 文本对拍一致。

### Phase 2 — 元数据框架 + 数据加载链(约 4~6 周)

- `meta/`:字段描述框架 + 全部 ~40 种解析器(含 `"50%"`、逗号元组、字典、BooleanExpression/IntegerExpression)、FieldSaver("只存与默认差异",Settings.cs:67-82);
- `tools/schema_dumper`:导出 `gen/`(接口 ID 表、traits/widgets/settings 描述表、synchash 注册表、Lua API 表);
- Manifest/ModData/Ruleset/ActorInfo/WeaponInfo 加载链(`TraitName@实例名`、拓扑构造序 ActorInfo.cs:104-167、`^` 抽象 actor 过滤);
- **验收【对拍】**:三 mod 全规则加载后逐 actor dump( trait 列表 + 全字段值)与 C# 深度解析一致;非法 yaml 报错行为对拍(缺 Required、未知字段告警)。R2 风险对冲:先打通 Health/Mobile/Armament 三个代表 trait 再铺开。

### Phase 3 — 仿真核心(约 6~8 周)

- Actor/ActorInfo/World/TraitDictionary(接口 ID + 平行数组二分,逐行照抄)/Activity(状态机,最可机械转换)/条件系统(GrantCondition 令牌,Actor.cs:560-613)/帧末任务队列/Effects;
- SyncHash:gen/ 注册表 + World 组合公式;
- Order 数据结构与序列化(**逐字节**:0xFF 位掩码 + Actor 代际;0x10 Ack;0x20 Ping;0x65 SyncHash;0x76 TickScale;0xFE Handshake);
- OrderManager 锁步(TryTick/ProcessOrders/双通道/NetFrameInterval 捆包/EchoConnection);
- **验收**:EchoConnection 单机 10⁶ tick ASan 无泄漏;TraitDictionary/Activity/条件系统单测(移植 OpenRA.Test 对应用例)。

### Phase 4 — 平台层 + 渲染(约 6~8 周,可与 Phase 5 并行)

- platform/:SDL2 窗口/输入(HiDPI、motion 合并、X1X2 转键、多击)/硬件光标;glad;Shader(`{VERSION}` 替换 + active-uniform 自动枚举,Shader.cs:107-131 原样移植);Texture(BGRA、GLES FBO 回读分支)/FBO/VB/IB;ThreadedGraphicsContext;OpenAL(音源池/实例限制/流式);FreeType;
- gfx/:三级合成、Sheet/SheetBuilder(shelf、1px margin、索引通道轮换)、HardwarePalette(256×N + RGBA16F)、SpriteRenderer(8 槽/BlendSpan)、TerrainSpriteLayer(静态 VB 脏行)、Renderable 稳定排序;glsl/ 原样复用;
- formats/:全部 Westwood 格式直译 + 单测(mods 资产做夹具);
- **验收**:离屏渲染 shellmap/若干地图与 C# 截图逐像素差异 ≤ 容差(报告差异分布);全部资产解码哈希对拍。

### Phase 5 — 主循环整合 + 核心 gameplay(约 8~10 周)

- game/:Game 编排(Initialize/InitializeMod 重建时序、StartGame、双时间戳 Loop、delayedActions)、Settings/热键/LoadScreen、InstalledMods;
- mods/common 核心 20 trait:Health/Mobile+Locomotor/Armament+AttackBase/AutoTarget/Targetable+HitShape+Armor/Selectable/BodyOrientation/RenderSprites+WithSpriteBody/Building+Buildable+Production+ProductionQueue/PlayerResources/Harvester+ResourceLayer+ResourceClaimLayer/Cloak/GainsExperience/Capturable/Conditions(34 个)/Shroud;
- 寻路全套:A* + 双向 + LaneBias、HierarchicalPathFinder(10×10 分块 + 增量脏更新)、四种 PathGraph、池化;Activities Move 系列;武器弹道 + 全部 Warhead;Map(map.bin、UID、CellLayer 投影);
- **里程碑**:调试渲染模式加载 skirmish 地图,控制台发 order 造兵/移动/战斗;
- **验收【对拍·重大关卡】**:C# 录制 replay 矩阵(各种族、AI 对战 30 分钟)逐帧 SyncHash = 完全一致。**此项通过 = 确定性移植成功。**

### Phase 6 — UI + 本地化(约 8~10 周)

- ui/:Widget 框架(树/IntegerExpression 布局/倒序冒泡/焦点)、chrome.yaml、62 widget + 131 Logic(字段表生成 + 手写行为)、WorldInteractionController/Viewport、ChromeMetrics、光标;
- l10n/:Fluent 落地 + FluentMessage;audio/:Sound 门面/通知/音乐流;
- **里程碑**:主菜单 → 本地大厅 → skirmish 全 UI 流程可玩;三 mod 可玩;
- **验收**:UI 冒烟脚本 + 截图抽样对拍。

### Phase 7 — 网络与服务器(约 6~8 周)

- net/:NetworkConnection(读线程/Ack/TickScale)、Session、ReplayRecorder/ReplayConnection、UnitOrders 分发、SyncReport;
- server/:单线程事件循环、每连接读线程、LobbyCommands 20 命令、PlayerPinger/MasterServerPinger、OrderLatency 投影、空帧注入(Server.cs:1444-1457);
- **验收【对拍】**:C++ 回放 C# 联机 replay(G3 前置);力争 C++ 客户端连 C# 服务器完成一局;desync 报告格式对拍。

### Phase 8 — Lua 脚本、AI、战役、Utility(约 6~8 周)

- script/:Lua 宿主 + 沙箱(内存/指令上限、去 random)+ gen/ 绑定层;AI:ModularBot/11 模块/小队状态机/自研 Mamdani;战役流程;d2k/ts 内容补全;
- Utility 子集:`--map-hash --check-yaml --resolved-rules --extract --docs`;
- **验收**:官方战役可通关;Lua API 与 `--lua-docs` 对拍;AI 对战 replay SyncHash 对拍(bot 在 RunUnsynced 内,order 序列应一致)。

### Phase 9 — 打磨与发布(约 4 周)

- 性能(帧分配器调优、trait 查询缓存、批次审计,对齐 C# 基准帧率);打包(Windows/Linux/macOS)、fuzz(MiniYaml/格式解析器)、完整 CI 矩阵;
- **验收**:三平台打包冒烟;8 小时 AI 对战 soak 无崩溃无泄漏。

### 时间线汇总(单人全职粗估,含每阶段末上游同步)

| 阶段 | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 合计 |
|---|---|---|---|---|---|---|---|---|---|---|---|
| 周数 | 2-3 | 2-3 | 4-6 | 6-8 | 6-8 | 8-10 | 8-10 | 6-8 | 6-8 | 4 | **52-68 周** |

> C++26 相比 C23 方案省去 OOP 手工映射(virtual/继承/泛型直接对应),Phase 3/5/6 机械转换量下降;但模块化构建与 Clang 特性缺口(§4.3/4.8)带来新摩擦。综合与 C23 估计持平;2-3 人并行(仿真/图形/UI-网络分线)可压至 8-10 个月。

---

## 9. 验证与质量保障体系

1. **黄金对拍矩阵**(CI 每夜,数据由基线 commit 的 C# 构建生成):MiniYaml 759 文件、resolved-rules 三 mod 全量、replay SyncHash 逐帧(Phase 5 起最高优先级)、资产解码哈希、地图 UID、渲染截图差异分布;
2. **单测**:Catch2,移植 OpenRA.Test 核心用例(MiniYaml/寻路/定点/格式);
3. **消毒器**:全程 ASan+UBSan(`ORA_SANITIZE=ON`);
4. **静态检查**:clang-tidy + 自定义 lint(sim/ 无浮点、UPSTREAM 标注强制、事件订阅成对、sim 内禁 shared_ptr 原子路径、规范命名前缀抽查);
5. **desync 排查**:`tools/replaydiff` 输出首个不一致帧的 actor/字段级 diff(复用 SyncReport 语义);
6. **同步审计**:`tools/upstream_check` 每 PR 运行:COVERAGE 与 UPSTREAM 标注一致性、黄金数据生成 commit 与 baseline 一致。

---

## 10. 风险登记册

| # | 风险 | 等级 | 对策 |
|---|---|---|---|
| R1 | 确定性破坏(UB/浮点/顺序)致 desync | 高 | §4.7 全套;replay 对拍前置于一切 gameplay 验收 |
| R2 | 元数据框架设计返工 | 高 | Phase 2 先跑通 3 个代表 trait;schema_dumper 消除手工漂移 |
| R3 | Clang 23 的 C++26 缺口(contracts/std::execution/反射) | 中 | cpp26.md 实测表为准;§4.3 过渡宏、§4.8 不依赖 execution;反射走生成器;Phase 0 探针前置锁定 |
| R4 | 25 万行规模进度失控 | 高 | 阶段验收制;每文件 UPSTREAM 标注 + 对拍;核心 20 trait 先出可玩版 |
| R5 | C++ 模块 + 第三方 C 头构建复杂(build 顺序/PCM 依赖) | 中 | 包装模块隔离;CMake 明确模块依赖图;必要时叶子格式层先 #include 后迁模块 |
| R6 | Lua 5.4 vs 5.2 脚本兼容 | 中 | 全 mods 脚本回归;必要时锁 5.3 |
| R7 | Fluent 无成熟 C++ 实现 | 中 | 自研子集(未用 isolating,面小) |
| R8 | 内存生命周期 bug | 高 | §4.5 分区 + ASan + 帧末销毁硬规则 + 订阅成对检查 |
| R9 | 联机跨版本(G3)不达 | 中 | G2 replay 对拍为硬验收,G3 力争 |
| R10 | 上游演进导致同步债务累积 | 中 | §7 阶段末批量 SOP;COVERAGE 可视化欠账;不追 HEAD |

---

## 11. 起步行动清单(本周)

1. 建 C++26 工程骨架:CMake + clang++ + Ninja + 模块探针(`import std;`、inplace_vector、expected、print、constexpr new、byteswap、`[[assume]]`),ctest 绿;
2. 从 git 历史(2850482)回收:`tools/golden_gen`、`tests/golden_core.txt`(60,883 行)、定点原语/MT 源码,机械转 C++26 并复跑对拍 = 0 diff;
3. 写入 `UPSTREAM.baseline`(7d57605bca)+ `tools/upstream_check` 骨架 + COVERAGE 初始表;
4. 搭 `tools/schema_dumper` 骨架:先导出接口 ID 表 + Health/Mobile/Armament 字段表,打通"yaml → C++ 结构体"最小闭环;
5. 移植 MiniYaml(Phase 1 核心),759 文件解析冒烟。

构建命令(Phase 0 目标形态):

```sh
cmake -S . -B build -G Ninja -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure      # 消毒器构建:加 -DORA_SANITIZE=ON
```

---

## 附:关键源码锚点速查(@7d57605bca)

| 机制 | 文件:行 |
|---|---|
| 主循环/双时间戳 | OpenRA.Game/Game.cs:771-897 |
| InitializeMod 重建时序 | Game.cs:474-532 |
| FieldLoader 解析分发 | OpenRA.Game/FieldLoader.cs:854-896 |
| ObjectCreator 实例化 | OpenRA.Game/ObjectCreator.cs:78-137 |
| TraitDictionary 二分查询 | OpenRA.Game/TraitDictionary.cs:145-327 |
| trait 拓扑构造序 | OpenRA.Game/GameRules/ActorInfo.cs:104-167 |
| World.Tick 顺序 | OpenRA.Game/World.cs:413-455 |
| SyncHash 公式 | World.cs:482-512;Sync.cs:78-107 |
| 定点原语 | WPos.cs / WDist.cs / WAngle.cs:59-99 / Map.cs:976 |
| Order 线格式 | Network/Order.cs:29-42,326-468;Server/ProtocolVersion.cs:16-80 |
| 锁步循环 | Network/OrderManager.cs:234-328 |
| 大厅命令表 | Mods.Common/ServerTraits/LobbyCommands.cs:163-190 |
| MiniYaml 合并/继承/弱删除 | MiniYaml.cs:459-564,613-647 |
| 渲染三级合成 | Renderer.cs:237-360 |
| 调色板纹理方案 | Graphics/HardwarePalette.cs |
| A*/HPF 寻路 | Mods.Common/Traits/World/PathFinder.cs;Pathfinder/* |
| Activities 状态机 | OpenRA.Game/Activities/Activity.cs:49-295 |
| Widget 反射加载 | OpenRA.Game/Widgets/WidgetLoader.cs:46-81;Widget.cs:322-332 |
| Lua 反射暴露 | Game/Scripting/ScriptObjectWrapper.cs:19-94;ScriptContext.cs:119-345 |
| 文件格式群 | Mods.Cnc/FileFormats/* |
| 地图格式/UID | Game/Map/Map.cs:160-313,708-765,784-888 |
| 服务器事件循环 | Game/Server/Server.cs:148,348-402,904-934,1444-1457 |
