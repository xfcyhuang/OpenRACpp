# OpenRA → C23 移植计划

> 源码库:`Z:\source_code\OpenRA`(约 2025 年基线,SDL2 + OpenGL 3.2Core/GLES3 + OpenAL + FreeType)
> 目标:在 `Z:\source_code\OpenRAC` 用 C23 标准重写整个引擎,数据/联机/回放兼容现有 mods、地图与 replay。
> **工具链(锁定)**:CMake ≥ 3.28 + Clang(`-std=gnu23`,GNU 方言)+ Ninja。本机基线已验证:clang 23.1.0(x86_64-w64-windows-gnu / llvm-mingw-UCRT 系,链接器 ld.lld)、CMake 4.3.2、Ninja 1.13.0。
> 本文档基于对源码库四大子系统(引擎核心、图形音频平台层、仿真/UI/AI/脚本、网络/服务器/资产管线)的全量调研,所有关键论断附文件与行号出处。

---

## 1. 源码库理解总结

### 1.1 规模与构成

| 组件 | 文件数 | 行数 | 职责 |
|---|---|---|---|
| OpenRA.Game | 224 | 44,012 | 引擎核心:主循环、反射、Trait 系统、网络、MiniYaml、Widget 基础 |
| OpenRA.Mods.Common | 1,085 | 159,235 | 仿真 trait(501 文件/约 418 个 TraitInfo)、Widgets、AI、脚本绑定 |
| OpenRA.Mods.Cnc | 140 | 20,784 | C&C/RA/TS 专属:文件格式(mix/shp/vqa/vxl…)、Cnc trait |
| OpenRA.Mods.D2k | 23 | 3,551 | Dune 2000 mod |
| OpenRA.Server | 1 | 124 | 专用服务器入口(薄壳) |
| OpenRA.Utility | 1 | 178 | 工具入口(45 个子命令在各程序集) |
| OpenRA.Platforms.Default | 18 | 4,814 | SDL2/OpenGL/OpenAL/FreeType 平台实现 |
| OpenRA.Test | 22 | 4,553 | NUnit 单测 |
| glsl/ | 12 | — | 着色器(可直接复用) |
| mods/ | 759 yaml | — | 规则/序列/热键/fluent/chrome 数据(直接复用) |

**合计约 25.7 万行 C# 代码**。依赖:`OpenRA-SDL2-CS`、`OpenRA-OpenAL-CS`、`OpenRA-Freetype6`(三者本来就是 C API 的 P/Invoke 封装)、`OpenRA-Eluant`(Lua 5.2)、`Linguini`(Project Fluent)、`Mono.NAT`(UPnP)、`SharpZipLib`、`NVorbis`/`MP3Sharp`/`TagLibSharp`、`Pfim`(TGA/DDS)、`DiscordRichPresence`、`rix0rrr.BeaconLib`(LAN 发现)、`OpenRA-FuzzyLogicLibrary`。

### 1.2 运行时架构(核心语义)

**主循环**(`OpenRA.Game/Game.cs`):单线程,双绝对时间戳 `nextLogic`/`nextRender` 节流(Game.cs:811-813)。逻辑 tick 间隔由 GameSpeed(`OrderManager.SuggestedTimestep`)决定,渲染间隔由图形设置决定;二者完全独立。逻辑落后超过 250ms 放弃追帧(843-844);每个逻辑 tick 强制至少渲染一帧(852-861);输入事件在**渲染帧末尾**由 `Renderer.EndFrame → Window.PumpInput` 采集(753),全部包在 `Sync.RunUnsynced` 中;逻辑 tick 顺序:`delayedActions → OrderManager.TickImmediate(收发包) → TryTick(锁步放行) → world.Tick → world.TickRender`。shellmap 世界与联机大厅的 OrderManager 可并存交替 tick(687-688)。

**数据驱动核心(反射链)**:磁盘 MiniYaml → `FieldLoader.Load` 按字段名反射赋值(FieldLoader.cs:754-796,支持约 40 种类型解析器:基元、`Color/Hotkey/WDist/WVec/WPos/WAngle/WRot/CPos/CVec/int2/Vector2/3/Rectangle/DateTime`、`HashSet/List/ImmutableArray/FrozenSet/FrozenDictionary/BitSet/Nullable`、一维数组、任意枚举、`BooleanExpression/IntegerExpression` 表达式、`TypeConverter` 兜底)→ `ObjectCreator.CreateObject` 按字符串类名实例化(ObjectCreator.cs:78-137)。该链承载了 trait、武器、widget、chrome、设置、地图规则——**是全项目的地基**。

**Trait 系统**:`ActorInfo` 是共享规则模板(含 `TraitInfo` 集合,拓扑排序构造顺序,ActorInfo.cs:104-167);每个 `Actor` 创建时实例化全部 trait,并把高频接口缓存为数组(Actor.cs:169-210)。`TraitDictionary` 以"接口类型"为键,`(actors[], traits[])` 平行数组按 ActorID 有序、二分查找(TraitDictionary.cs:145-327)——`T GetTrait<T>()` 按任意接口查询的性能基础。

**仿真确定性**:全整数定点运算——1 cell = 1024 世界单位(`WPos` = 3×int32,Map.cs:976)、整圆 1024 单位(`WAngle.Sin/Cos` 整数查表,WAngle.cs:59-99);双 RNG(`World.SharedRandom` 同步 / `LocalRandom` UI,MersenneTwister 纯整数);每 tick 顺序:ActorID 升序遍历 activities → `ITick` traits → effects → 帧末任务队列(World.cs:413-455)。`SyncHash` = Σ n·ActorID·hash(actor 的 ISync trait)(World.cs:482-512),哈希函数由 **Reflection.Emit** 生成(Sync.cs:78-107)。bot 与 UI 跑在 `Sync.RunUnsynced` 校验下。唯一 `decimal` 使用点:抛物线二次插值(WPos.cs:58-72)。

**网络锁步**:线协议 `Int32 长度 + Int32 客户端ID + Int32 帧号 + Orders[]`(ProtocolVersion.cs:16-80,Handshake=7 / Orders=21)。游戏指令 Order 首字节 0xFF:orderstring + Int16 位掩码 + 按 flag 的字段(含 Actor 代际 Generation,Order.cs:29-42,326-468);另含 Ack(0x10,本地回环省带宽优化)、Ping(0x20)、SyncHash(0x65)、TickScale(0x76 动态限速)、Disconnect(0xBF)、Handshake(0xFE,7-bit 前缀字符串 + MiniYaml blob)。`NetFrameInterval=3` 三帧捆包;服务器把 order 投影 `frame += OrderLatency`;大厅命令(`/slot` `/kick` 等 20 个)走 `Order("Command", …)` 带内通道;服务器文本通知发 FluentMessage(key+args)由客户端本地化。

**渲染管线**:三级合成 world FBO(调色板纹理查色 + HSV 色移 + gl_FragDepth 逐像素深度)→ screen FBO(像素风双线性放大)→ 默认帧缓冲(Renderer.cs:237-360)。调色板全部打包为 256×N 纹理 + RGBA16F 色移纹理(HardwarePalette)。`SpriteRenderer` 8 纹理槽批渲染、BlendSpan 允许同批次交错混合模式;地形走静态大 VB 只传脏行(TerrainSpriteLayer.cs:221-235);体素(VXL/HVA)由 mod 注册的 `IRenderer` 绘制。GL 调用封送独立渲染线程(ThreadedGraphicsContext,859 行;Windows 窗口模式例外走主线程)。GLSL 的 `{VERSION}` 按桌面 `#version 140` / GLES `#version 300 es` 替换。

**平台层**:20 个文件实现 `IPlatform/IPlatformWindow/IGraphicsContext/IShader/ITexture/IFrameBuffer/IFont/ISoundEngine/…` 接口;OpenAL 256 音源池 + 同源实例限制(5 帧内 2730 距离最多 3 例,音量按活跃数衰减);FreeType 6 个函数的 P/Invoke + 手算结构体偏移。

**文件格式**(纯字节解析,可直译):mix(C&C1/RA/TS/RA2,可选 Blowfish 加密头)、shpTD/shpD2/shpTS/remastered、tmpRA/TD/TS(地形)、pal(6bit→8bit)、vqa(码本+LCW+LZO+内嵌音频)、wsa(XOR delta)、aud(IMA ADPCM)、voc/wav、vxl/hva(体素)、lzo/lcw/xor 压缩、dds/tga、png(自研 592 行)、idx、GeoIP 二进制、map.bin(tile 格式 2)。地图 = zip 包,UID = yaml/bin/lua/map.png 流拼接 SHA1(Map.cs:285-313)。

**UI**:Widget 树,布局用 `IntegerExpression` 表达式(`WINDOW_WIDTH` 等宏);事件从最深子节点倒序尝试后冒泡(Widget.cs:436-457);`Logic:` 节点反射实例化 ChromeLogic 代码后置(Widget.cs:322-332);UI 每 40ms 独立节拍,整体在 RunUnsynced 内。62 个 widget 类 + 131 个 Logic 类。

**Lua 脚本**:Eluant(Lua 5.2),沙箱(50MB 内存上限、100 万指令上限、删除 `math.random`),反射暴露全部 public 方法/属性(17 个全局表 + 约 35 个属性组 + Trigger 全套事件回调);脚本在同步 tick 内执行,可改世界状态。

**AI**:ModularBot 只能通过 `IBot.QueueOrder` 发 order 改变世界;11 个 bot 模块;小队状态机;FuzzyLogic 仅一处(AttackOrFleeFuzzy 的 Mamdani 系统,~200 行可自实现)。

**静态全局状态**(C 移植需显式化的对象):`Game.ModData/Settings/Renderer/Sound/CursorManager/OrderManager/worldRenderer/server/CosmeticRandom`、`Ui.*`(Widget.cs:24-118)、`ChromeProvider/ChromeMetrics/FluentProvider/TextNotificationsManager`、`ObjectCreator.ResolvedAssemblies`、`FieldLoader.TypeLoadInfo`、`Sync.HashFunctions`、`Platform.*` 路径。生命周期:切 mod 时 ModData/OrderManager/worldRenderer/事件订阅全部重建(Game.cs:474-532)。

### 1.3 语义理解的边界与策略

25.7 万行代码中的关键机制与数据流已全部理清(上文 1.2 及各节行号引用),但**数千个 trait/活动/widget 的逐行语义**不可能预先穷尽阅读。本计划采用工程化策略保证语义等价:**结构先行(先移植机制骨架)+ 按需深读(移植每个模块时精读对应源文件)+ 黄金对拍(与 C# 版本逐字节/逐帧比对输出)**。对拍手段见第 8 节。

---

## 2. 移植目标与兼容性基线

### 2.1 目标

1. **G1 数据兼容**:现有 `mods/`(759 个 yaml)、地图(.oramap)、设置文件、fluent 资源**零修改**可用。
2. **G2 回放兼容**:C# 版录制的 replay 可在 C 版回放,逐帧 SyncHash 一致。
3. **G3 联机兼容**:C 客户端可加入 C# 版服务器(线协议逐字节对齐)。(G3 为力争目标,以 G2 为硬性验收。)
4. **G4 行为等价**:游戏规则(伤害公式、寻路结果、AI 决策序列)在相同输入下产生相同的世界状态演化。
5. **G5 工程质量**:无 UB、ASan/UBSan 干净、核心单测通过、可维护的 C23 代码风格。

### 2.2 范围裁剪(建议,待确认)

| 组件 | 处置 |
|---|---|
| DiscordRichPresence | 一期不移植(独立集成,不碰核心) |
| TagLibSharp(音频元数据) | 一期不移植 |
| Remastered shp 加载器 | 移植(纯格式解析,代价低) |
| Utility 45 个命令 | 按需子集:`--map-hash` `--check-yaml` `--resolved-rules` `--extract` `--docs` 优先 |
| ANGLE/GLES3 路径 | 二期(先保 GL 3.2 Core,代码保留 profile 分支) |
| 主服务器账号认证(RSA/PEM/GeoIP/UPnP/LAN beacon) | 三期(依赖 mbedTLS/libminiupnpc/libcurl) |

---

## 3. 总体策略

**自底向上 + 持续对拍 + 双向工具**:

1. **依赖倒置先行**:先移植"无依赖叶子"(MiniYaml、文件格式、定点原语),再逐层上移。每个阶段结束时存在一个**可运行的验收程序**,而不是长期不可运行的半成品。
2. **权威 schema 导出器**:利用现有 C# 程序集自带反射,写一个导出工具(挂在 OpenRA.Utility 下,或独立加载其 DLL),把全部 `*Info`/Widget/Settings 类的**字段元数据 + 类型注册表**导出为 C 源文件(字段描述表、接口表、类型 ID 表)。这把"418 个 TraitInfo + 62 widget + 40 种字段类型"的最大工作量从手写变成生成,且以编译期静态断言防止漂移。**生成器本身不依赖被移植代码的正确性,只依赖 C# 元数据,天然正确。**
3. **逐字节/逐帧对拍**:MiniYaml 解析树与 `--resolved-rules` 输出对拍;replay 逐帧 SyncHash 对拍;地图 UID/小地图像素对拍;渲染走"离屏 FBO 读回 + 逐像素差异"抽样对拍。
4. **每模块移植时精读源文件**:计划中的行号引用即"精读清单"的入口。

---

## 4. C# → C23 概念映射(核心技术决策)

### 4.1 类型系统与 OOP

| C# 概念 | C23 方案 |
|---|---|
| class + virtual | `struct` 首成员放公共头(内嵌 `vtable` 常量指针);虚方法 = 函数指针成员 |
| interface(查询用) | **接口 ID**(集中分配的 `enum : uint16`)。trait 结构体头部含"接口表"(接口 ID → 函数指针表数组)。`TraitDictionary` 键从 `Type` 改为接口 ID |
| abstract/sealed | vtable 中函数指针为 NULL 即抽象;C23 无 sealed,靠约定与注释 |
| 继承(单继承) | 结构体首成员内嵌基结构体(零开销上转);`offsetof` 下转仅限受控点 |
| 泛型集合 | 少量手写专用容器(actor 表、order 队列等);通用泛型用**宏模板**(`ORA_VEC(T)` / `ORA_MAP(K,V)`),配合 `_Generic` 做接口胶水 |
| Nullable | 指针 NULL;值类型 `Maybe<T>` 用 tagged struct(仅个别场景) |
| enum | C23 指定底层类型 `enum E : uint8_t`。**注意:凡进 Order 序列化的枚举在 C# 里是 int32,C 侧必须显式 `int32_t` 语义读写**(见 4.4) |
| delegate/event | 回调数组 `{fn, ctx}`;事件源持有数组,对象销毁时**必须**成对注销(对应 C# InitializeMod 清空事件,Game.cs:477-480;漏掉即 use-after-free) |
| LINQ/闭包 | 手写循环;闭包改为小型 ctx 结构体 |
| `readonly`/`init` | `const` 限定 + 初始化约定 |
| string | UTF-8 `char*`;MiniYaml 的字符串池语义保留(去重 interning);不可变约定靠纪律 |

### 4.2 反射的替代:类型注册表 + 字段描述宏

这是移植的**第一支柱**。三层构成:

**(a) 类型 ID 与注册表**(替代 ObjectCreator):

```c
/* 生成或手写:interfaces.h */
enum ora_iface : uint16_t { ORA_IF_INVALID, ORA_IF_ITick, ORA_IF_IResolveOrder, /* … */ };
enum ora_type_id : uint16_t { ORA_TYPE_INVALID, ORA_TYPE_TraitInfo, /* …每 Info/trait/widget 一项 */ };

/* 运行时注册表:名字 → {构造回调, 字段表, 接口表, 类型ID} */
typedef struct ora_type_info {
    const char *name;                 /* "Health" → 实际注册 "HealthInfo" */
    ora_type_id id;
    const ora_field_desc *fields;     /* 字段描述表 */
    uint16_t nfields;
    const ora_iface_entry *ifaces;    /* 接口 → 函数表 */
    void *(*construct)(void);         /* calloc + vtable 头 */
} ora_type_info;
```

mod 侧:每个 mod 一个 `mod_register()` 函数(静态注册全部类型);mod 动态库(`dlopen`/`LoadLibrary`)作为**可选**进阶能力,一期静态链接。

**(b) 字段描述表**(替代 FieldLoader 的反射元数据):

```c
typedef struct ora_field_desc {
    const char *yaml_name;      /* "HP" */
    ora_field_type type;        /* ORA_F_INT / ORA_F_WPOS / ORA_F_CPOS_LIST / ORA_F_ENUM / … */
    size_t offset;
    uint16_t flags;             /* REQUIRED / IGNORE / HAS_CUSTOM_LOADER */
    union { int64_t i; const void *p; } deflt;   /* 默认值(FieldSaver diff 保存需要) */
    bool (*custom)(void *obj, const ora_yaml_node *n);  /* LoadUsing 等价 */
} ora_field_desc;

#define ORA_FIELD(T, member, type_, ...) \
    { .yaml_name = #member, .type = (type_), .offset = offsetof(T, member), __VA_ARGS__ }

static const ora_field_desc health_info_fields[] = {
    ORA_FIELD(HealthInfo, HP, ORA_F_INT, .flags = ORA_FF_REQUIRED),
    ORA_FIELD(HealthInfo, DisplayHP, ORA_F_U32),
};
```

`ora_field_type` 枚举即 FieldLoader.cs:74-892 全部类型解析器的清单(§1.2),每个 tag 对应一个 `parse_*` 函数——**解析器集合与 C# 一一对应**,包括 `"50%"` 百分比语法、逗号元组、按子节点的字典、表达式求值。字段表可以手写(核心类)或由 **(d) 导出器生成**(批量类)。

**(c) 引擎消费侧不变**:`ruleset_load()` 依旧执行"MiniYaml 节点 → 名字查 type_info → construct → 按字段表灌值",与 C# 的 Ruleset/ActorInfo/WidgetLoader 流程逐行对应(含 `TraitName@实例名`、`Inherits:`、`-Key` 弱删除、缺 Required 字段报错语义)。

**(d) schema 导出器**(见第 3 节策略 2):用 C# 反射枚举目标程序集全部可序列化类型,生成 `gen/traits_meta.c/h`、`gen/interfaces.h`、`gen/widgets_meta.c`、`gen/settings_meta.c`。**生成代码纳入版本管理并在 CI 校验可重现**。手写与生成混用时以 `ORA_REGISTER` 宏统一挂表。

### 4.3 内存管理策略(替代 GC)

按生命周期分区,不搞万能方案:

| 区域 | 策略 | 对应 C# 对象 |
|---|---|---|
| 进程级(引擎/ModData/Settings) | 显式 create/destroy,严格按 `Game.Run` 退出序销毁:OrderManager → worldRenderer → ModData → Chrome → Sound → Renderer(Game.cs:914-922) | ModData、Renderer、Sound |
| 每局世界 | **World arena**:开局一次分配、整局 bump、结束整块释放;actor/trait/activity/effect 全在其中 | Actor、trait、Activity、IEffect |
| 帧临时 | 线性帧分配器(渲染收集的 Renderable 列表、排序 key 等),帧末重置 | 每 tick 的临时 List |
| 资产(Sheet/纹理/序列) | 引用计数 + ModData 级资产表;切 mod 时统一释放 | Sheet、ITexture、序列 |
| 字符串 | MiniYaml 池 + 静态字面量;World 级 strtab | interned string |

配套纪律:trait/actor 销毁只发生在帧末队列(World.cs:453-455 的既有语义,C 侧成为硬规则);ActorID + Generation 代数机制保留(Order 反序列化据此拒绝陈旧引用,Order.cs:95-240);指针一律从 World/ModData 根可达,禁止全局缓存 actor 指针。CI 跑 ASan。

### 4.4 确定性保障(移植的生命线)

1. **符号溢出**:C# 默认 unchecked int 溢出 = 回绕;C 中 signed 溢出是 UB。全工程强制 `-fwrapv`(或关键运算显式走 `uint32_t` 再转回)。**这是必须的编译选项,写进 CMake 基线。**
2. **移位/除法**:C# `>>` 负数 = 算术右移,C 为实现定义——用静态断言 + 启动自检(`-1 >> 1 == -1`);除法/取模两边都是向零截断,一致。
3. **浮点**:仿真路径本就无浮点(§1.2);残留 double 仅在 RunUnsynced(AI/bot/渲染/音频)。禁止在新代码向同步路径引入任何浮点;CI 加 lint 规则检查 sim/ 目录无 `double/float`。
4. **SyncHash**:Reflection.Emit(Sync.cs:78-107)→ 编译期生成:每个 ISync 类型手写/宏生成 `sync_hash(const void*)` 函数并注册到 `sync_hash_registry`。字段集与 C# `[VerifySync]` 标记完全一致(导出器顺带导出该标记)。
5. **哈希仅整数**:`World.SyncHash` 组合式全为整数运算,跨语言可复现 → **replay 对拍可行**(第 8 节)。
6. **RNG**:MersenneTwister.cs 是纯整数实现,逐行照抄 + 与 C# 版同种子输出序列对拍。
7. **遍历顺序**:C# `SortedDictionary<uint,Actor>` 与 LINQ 排序的稳定语义 → C 侧显式有序结构 + 稳定排序(WorldRenderer 的 Renderable key 排序等)。凡是"顺序影响结果"的容器,导出器/精读时逐一确认。

### 4.5 并发模型

| C# | C23 |
|---|---|
| ThreadedGraphicsContext(渲染线程) | 保留架构:无锁 SPSC 命令队列封送 GL 调用;Windows 窗口模式走主线程的特例保留(Sdl2PlatformWindow.cs:346-353) |
| 音频流异步加载(Task) | pthread/`thrd_t` + 条件变量,语义照抄(挂静音 buffer → 后台填充 → 换 buffer) |
| 服务器单线程事件循环 + 每连接读线程 | 同构;`BlockingCollection` → 有界阻塞队列 |
| 网络读线程 | 同构,socket 非阻塞/阻塞 + 队列 |

C23 线程库在本工具链不可用(见 Phase 0 已知限制),`ora_thread` 封装为主:Windows 用 Win32 API(`CreateThread`/`SRWLOCK`/`CONDITION_VARIABLE`),POSIX 用 pthread;平台差异只允许出现在 `core/ora_thread.h` 一个翻译单元。

### 4.6 错误处理

- C# 异常路径(YamlException/MissingFieldsException 等)在加载期 = `longjmp` 到模块加载锚点 + 日志;运行期致命错误 = 明确 `abort` 带上下文 dump。
- Result 风格(`ora_result { int code; const char *msg; }`)用于可恢复路径(文件缺失、坏资产)。

---

## 5. 目标架构与代码布局

```
OpenRAC/
├── CMakeLists.txt            # 工具链基线: clang + -std=gnu23 + ninja; -fwrapv -fno-strict-aliasing
├── PORTING_PLAN.md           # 本文档
├── src/
│   ├── core/                 # 原语:WPos/WVec/WDist/WAngle/WRot/CPos/MPos/int2、BitSet、
│   │                         # MersenneTwister、CRC32/SHA1、日志、内存(arena/帧分配器)、
│   │                         # 容器宏模板、ora_thread
│   ├── yaml/                 # MiniYaml:解析/FromLines 状态机/Merge/ResolveInherits/字符串池
│   ├── meta/                 # 类型注册表、字段描述框架、schema 加载器(FieldLoader 等价)、
│   │                         # FieldSaver 等价(设置 diff 保存)
│   ├── fs/                   # FileSystem:挂载/索引/大小写规整、Folder、Zip(miniz)、
│   │                         # Mix(含 Blowfish)、Meg/Big/Pak/ISO9660 等包
│   ├── formats/              # shp*/tmp*/pal/vqa/wsa/aud/voc/wav/ogg/mp3/vxl/hva/idx/
│   │                         # lcw/lzo/xor/dds/tga/png/geoip/map.bin
│   ├── gfx/                  # Renderer 编排、Sprite/Sheet/SheetBuilder、HardwarePalette、
│   │                         # TerrainSpriteLayer、WorldRenderer、Renderable 收集排序、
│   │                         # CursorManager、字体图集
│   ├── platform/             # SDL2 窗口/输入/光标、GL 加载(glad)、Shader、Texture/FBO/VB/IB、
│   │                         # ThreadedGraphicsContext、OpenAL 引擎、FreeType 字体
│   ├── sim/                  # Actor/ActorInfo/World/TraitDictionary/Activity/条件系统/
│   │                         # SyncHash/Effects/Ruleset/WeaponInfo/OrderGenerator
│   ├── net/                  # Order/OrderIO/OrderManager/Connection(Echo/Network/Replay)/
│   │                         # Session/ReplayRecorder/UnitOrders 分发
│   ├── server/               # Server 单线程事件循环、ServerTraits(lobby 命令/pinger)、
│   │                         # 专用服务器入口
│   ├── ui/                   # Widget 框架(树/布局表达式/事件路由)、chrome 加载、
│   │                         # WorldInteraction/Viewport 控制器
│   ├── script/               # Lua 宿主、沙箱、绑定层(生成)、ScriptTriggers 桥
│   ├── audio/                # Sound 门面、音源池策略、流式播放
│   ├── l10n/                 # Fluent 子集或 fluent-rs C 绑定、FluentMessage 网络格式
│   ├── game/                 # main、Game 全局编排(Initialize/InitializeMod/StartGame/Loop)、
│   │                         # Settings/HotkeyManager、LoadScreen、InstalledMods
│   └── mods/                 # common/cnc/d2k 的 trait/widget/logic/bot/script 实现 + 注册函数
├── gen/                      # schema 导出器输出(纳入版本管理)
├── tools/
│   ├── schema_dumper/        # C# 反射 → gen/ 代码(可跑在 .NET 上,非交付运行时依赖)
│   └── replaydiff/           # replay 逐帧 SyncHash 对拍器
├── tests/                    # 单测(原 OpenRA.Test 核心用例)+ 对拍夹具
├── third_party/              # SDL2、OpenAL-soft、freetype、lua、miniz、minimp3、
│                             # stb_vorbis/dr_mp3、libpng(或 spng)、glad、mbedTLS(三期)、
│                             # libminiupnpc(三期)、libcurl(三期)
└── packaging/
```

**命名/风格**:类型 `ora_` 前缀引擎侧、mod 侧 `ra_/cnc_` 模块前缀;`typedef struct X X;`;公共头自包含;每文件顶部注释标注对应 C# 源文件与行号区间(审计线索)。

---

## 6. 第三方依赖选型

| C# 依赖 | C 替代 | 说明 |
|---|---|---|
| SDL2-CS | **SDL2 原生库** | 直连,P/Invoke 层反而消失 |
| 手写 GL 绑定(OpenGL.cs 801 行) | **glad**(GL 3.2 Core + GLES3 双 profile) | 函数子集就是"GL3.2Core ∩ GLES3" |
| OpenAL-CS | **OpenAL-soft** | 原生 |
| Freetype6 绑定 | **FreeType 原生** | 手算偏移换回真结构体,更简单 |
| Eluant(Lua 5.2) | **Lua 5.4**(跑现有 mods 脚本回归)或 5.3 | 5.2 已停维护;地图脚本兼容性靠回归测试兜底 |
| Linguini(Fluent) | **fluent-rs + cbindgen 出 C ABI**;或自研子集(OpenRA 未用 isolating,特性面小) | FluentMessage 网络格式必须保留 |
| SharpZipLib | **miniz** | zip 读写 |
| NVorbis / MP3Sharp | **stb_vorbis / minimp3(dr_mp3)** | C 库现成 |
| Pfim(TGA/DDS) | 自照抄(两文件共 ~200 行) | 直接移植 |
| Png.cs(592 行自研) | libpng/spng 或照抄自研(含 Indexed8 写) | 倾向照抄自研以保行为 |
| Mono.NAT | libminiupnpc(三期) | SSDP+SOAP |
| BeaconLib | 自研 UDP 广播(~100 行) | LAN 发现 |
| HttpWebRequest | libcurl(三期) | master server / 认证 |
| RSA/SHA1/PEM | **BearSSL 或 mbedTLS**(三期) | fingerprint=SHA1(n‖e) 必须保持一致 |
| FuzzyLogicLibrary | 自实现 Mamdani(~200 行,仅一处使用) | |
| Discord/TagLib | 一期裁剪 | §2.2 |

---

## 7. 分阶段实施计划

> 原则:每阶段结束有**可运行验收程序**;阶段编号即依赖顺序;并行度允许(如 Phase 4/5 可并行)。
> 图例:【对拍】= 与 C# 版输出比对的黄金测试;【单测】= 单元测试;【里程碑】= 人肉可玩/可验证。

### Phase 0 — 工程骨架与基础库(约 2~3 周)

- CMake 工程 + 编译基线(**已落地**):`-std=gnu23`(CMAKE_C_EXTENSIONS ON)+ `-fwrapv -fno-strict-aliasing -Wall -Wextra -Wconversion -Wshadow`。工具链锁定 **CMake + Clang + Ninja**,本机 clang 23.1.0(x86_64-w64-windows-gnu,UCRT,ld.lld)。CI:同一 clang 主版本跨 Windows/Linux 双平台(Linux 侧用发行版 LLVM 或同版本容器镜像)。
- **C23 特性使用基线**(以 `tools/c23_probe.c` 探测通过为准,已验证):enum 底层类型、`constexpr`、`nullptr`、`true/false`、`[[attr]]`、`typeof`、`auto`、`static_assert`、复合字面量、指定初始化器、匿名结构/联合、`#embed`、`_BitInt`(按需);**禁用** VLA。
- **已知工具链限制**:`<threads.h>` 缺失(llvm-mingw 系不随附)→ `ora_thread` 封装层直接采用 Win32(CreateThread/SRWLOCK/CONDITION_VARIABLE)+ POSIX(pthread)双后端(§4.5),不依赖 C11 线程库。
- `core/` 基础库:定点原语(WPos/WVec/WDist/WAngle/WRot/CPos/MPos/int2/BitSet——照抄各 .cs 的整数实现)、MersenneTwister、CRC32/SHA1、日志、内存分配器(arena/帧分配器)、容器宏模板、`ora_thread`。
- 第三方子树接入(SDL2/OpenAL/freetype/lua/miniz)。
- **验收**:定点原语与 C# 版随机输入对拍(WAngle.Sin/Cos 查表全值扫描比对);MersenneTwister 同种子前 10⁶ 输出一致;ASan 干净。【单测】
- **当前状态**:`CMakeLists.txt` 与 `tools/c23_probe.c` 已就绪并通过(全部特性 + 确定性前提 + Win32 线程后端,ctest 绿)。

### Phase 1 — MiniYaml + FileSystem(约 2~3 周)

- MiniYaml.cs(791 行)逐行移植:行状态机(4 空格或 1 tab 缩进、`\#` 转义、值裁剪与 `\ ` 保护)、`Merge/MergeSelfPartial` 深合并、`ResolveInherits`(`Inherits:`/循环检测)、`-Key` 弱删除及其相对顺序语义(MiniYaml.cs:613-647)、字符串池、SourceLocation。
- FileSystem:挂载顺序=覆盖优先级(FileSystem.cs:193-199)、大小写不敏感索引、Folder、ZipFile(miniz)。
- **验收【对拍】**:
  - 解析全部 759 个 mods/ yaml,与 C# `MiniYaml.ToString` 规范化输出逐文件 diff = 0;
  - 与 C# Utility `--resolved-rules` 输出对拍(ra/cnc/d2k 全部规则)→ **这一条通过意味着 MiniYaml+合并+继承语义完全等价,是整个项目的第一个重大关卡**。

### Phase 2 — 元数据框架 + 数据加载链(约 4~6 周)

- `meta/`:类型注册表、字段描述框架、全部 ~40 种字段类型解析器(含 `"50%"`、逗号元组、按子节点字典、表达式求值器 BooleanExpression/IntegerExpression)、FieldSaver(含"只存与默认差异"逻辑,Settings.cs:67-82)。
- `tools/schema_dumper`:C# 侧导出器,生成 `gen/`(interfaces.h、traits_meta、widgets_meta、settings_meta、synchash 注册表——**含 [VerifySync] 标记导出**)。
- Manifest/ModData/Ruleset/ActorInfo/WeaponInfo 加载链(含 `TraitName@实例名`、拓扑排序构造序 ActorInfo.cs:104-167、`^` 抽象 actor 过滤)。
- **验收【对拍】**:C 版加载 ra/cnc/d2k 全部规则后,dump"每 actor 的 trait 列表 + 全部字段值",与 C# `--resolved-rules` 深度解析结果逐 actor 比对 = 一致;非法 yaml 的报错行为对拍(缺 Required 字段、未知字段告警等)。

### Phase 3 — 仿真核心(约 6~8 周)

- Actor/ActorInfo/World/TraitDictionary(接口 ID 化 + 平行数组二分,逐行照抄 TraitDictionary.cs)/Activity 基础设施(手写状态机,最可机械转换)/条件系统(GrantCondition 令牌,Actor.cs:560-613)/帧末任务队列/Effects 框架。
- SyncHash:编译期生成的哈希注册表 + World.SyncHash 组合公式。
- Order 数据结构与序列化(**逐字节对齐**:0xFF Fields 格式、位掩码、Actor 代际;0x10 Ack 等)。
- OrderManager 锁步循环(TryTick/ProcessOrders/双通道 immediate+queued/NetFrameInterval 捆包/EchoConnection 本地回环)。
- **验收**:EchoConnection 下单机 tick 驱动空世界跑 10⁶ tick 无泄漏(ASan);单测覆盖 TraitDictionary 全 API、Activity 链、条件系统(移植 OpenRA.Test 对应用例)。

### Phase 4 — 平台层 + 渲染(约 6~8 周,可与 Phase 5 并行)

- platform/:SDL2 窗口/输入(HiDPI、motion 合并、X1X2 转键、多击检测)/硬件光标;glad 加载;Shader(编译 + `{VERSION}` 替换 + active-uniform 自动枚举与采样器槽分配——Shader.cs:107-131 值得原样移植);Texture(BGRA、GLES 回读 FBO 路径保留分支)/FBO/VB/IB;ThreadedGraphicsContext;OpenAL 引擎(音源池/实例限制/流式);FreeType 字体。
- gfx/:Renderer 三级合成管线、Sheet/SheetBuilder(shelf 打包、1px margin、索引 sheet 通道轮换)、HardwarePalette(256×N 纹理 + RGBA16F 色移)、SpriteRenderer(8 槽、BlendSpan)、TerrainSpriteLayer(静态 VB + 脏行)、WorldRenderer 的 Renderable 收集与稳定排序、glsl/ 原样复用。
- formats/:全部 Westwood 格式解析器(纯字节流,直译)+ 独立解码单测(用 mods 资产做夹具)。
- **验收**:离屏渲染 mods/ra shellmap 与若干地图截图,与 C# 版逐像素差异 ≤ 容差(抗锯齿/驱动差异允许小面积差异,报告差异分布);格式解码与 C# 版对全部游戏资产输出哈希比对。

### Phase 5 — 主循环整合 + 核心 gameplay trait(约 8~10 周)

- game/:Game 编排(Initialize/InitializeMod 重建时序、StartGame、Loop 双时间戳节流、delayedActions→回调结构)、Settings/热键/LoadScreen、InstalledMods。
- mods/common 核心 trait(§调研的"核心 20 个"):Health/Mobile+Locomotor/Armament+AttackBase 系列/AutoTarget/Targetable+HitShape+Armor/Selectable/BodyOrientation/RenderSprites+WithSpriteBody/Building+Buildable+Production+ProductionQueue/PlayerResources/Harvester+ResourceLayer+ResourceClaimLayer/Cloak/GainsExperience/Capturable+Captures/Conditions(34 个)/Shroud/迷雾。
- 寻路全套:PathSearch A* + 双向 + 启发权重 + LaneBias、HierarchicalPathFinder(10×10 分块连通域 + 增量脏更新)、四种 PathGraph、池化。
- Activities 根 + Move 系列;武器弹道(Bullet/Missile/GravityBomb/InstantHit/LaserZap/…)+ 全部 Warhead;Map(map.bin、UID、等距投影 CellLayer/CPos↔MPos↔PPos)。
- **里程碑**:无正式 UI 的调试渲染模式下,加载 skirmish 地图,手动发 order(调试控制台)造兵、移动、战斗。
- **验收【对拍·重大关卡】**:C# 录制一批 replay(各种族、AI 对战 30 分钟),C 版逐帧 SyncHash 对拍 = 完全一致。**此项通过等于确定性移植成功。**

### Phase 6 — UI + 本地化 + 完整游戏体验(约 8~10 周)

- ui/:Widget 框架(树/IntegerExpression 布局/倒序冒泡路由/焦点)、chrome.yaml 加载、62 个 widget 类 + 131 个 Logic 类(schema_dumper 生成字段表 + 手写行为)、WorldInteractionController/Viewport 控制器、ChromeMetrics、光标。
- l10n/:Fluent 方案落地 + FluentMessage。
- audio/:Sound 门面、通知表、音乐/视频流。
- 里程碑:主菜单 → 大厅(本地)→ skirmish 全 UI 流程可玩;三个 mod 可玩。
- **验收**:UI 冒烟脚本(模拟输入走完主流程)+ 截图对拍抽样。

### Phase 7 — 网络与服务器(约 6~8 周)

- net/:NetworkConnection(读线程、Ack、TickScale)、Session、ReplayRecorder/ReplayConnection、UnitOrders 全命令分发、SyncReport。
- server/:单线程事件循环、每连接读线程、LobbyCommands 20 命令、PlayerPinger/MasterServerPinger、GameSpeed/OrderLatency 投影、空帧注入(Server.cs:1444-1457)。
- 专用服务器入口;GeoIP;UPnP/LAN beacon/master server/认证(视 §2.2 三期安排)。
- **验收【对拍】**:C 版回放 C# 录制的联机 replay(G3 前置);C 客户端连 C# 服务器完成一局(若达 G3);跨版本 desync 报告格式对拍。

### Phase 8 — Lua 脚本、AI、战役、Utility(约 6~8 周)

- script/:Lua 宿主 + 沙箱(内存/指令上限、去 random)+ **绑定层生成**(schema_dumper 顺带导出 Lua API 表 → `luaL_Reg` 生成;17 全局表 + 35 属性组 + Trigger 桥)。
- AI:ModularBot/11 模块/小队状态机/自研 Mamdani。
- 战役流程(MissionObjectives/媒体播放/触发器);d2k、ts 内容补全。
- Utility 子集:`--map-hash` `--check-yaml` `--resolved-rules` `--extract` `--docs`。
- **验收**:全部官方战役可通关;脚本 API 与 C# `--lua-docs` 输出对拍;AI 对战 replay 与 C# 版 SyncHash 对拍(bot 在 RunUnsynced 内,order 序列应一致)。

### Phase 9 — 打磨与发布(约 4 周)

- 性能:帧分配器调优、trait 查询缓存核对、批渲染批次审计(对齐 C# 基准帧率)。
- 打包(installer/AppImage/Steam 风格布局)、文档、完整 CI 矩阵、fuzz(MiniYaml/格式解析器)。
- **验收**:三平台(Windows/Linux/macOS)打包产物冒烟;长期 soak(8 小时 AI 对战)无崩溃无泄漏。

### 时间线汇总(单人全职粗估)

| 阶段 | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 合计 |
|---|---|---|---|---|---|---|---|---|---|---|---|
| 周数 | 2-3 | 2-3 | 4-6 | 6-8 | 6-8 | 8-10 | 8-10 | 6-8 | 6-8 | 4 | **52-68 周(约 1.0-1.3 人年,理想工具链下)** |

> 现实校正:以上为"全职、无返工、对拍一次通过"的下界。考虑调试、desync 排查、UI 细节返工,**单人实际预期 1.5-2 人年;2-3 人并行(仿真/图形/UI-网络分线)可压至 8-10 个月**。工作量主体在 Phase 5/6(Mods.Common 的 16 万行)与 Phase 2(反射框架),schema 导出器是压缩工期的关键杠杆。

---

## 8. 验证与质量保障体系

1. **黄金对拍矩阵**(持续,CI 每夜):
   - MiniYaml:759 文件规范化输出 diff;
   - resolved-rules/sequences/weapons:三 mod 全量;
   - replay SyncHash:逐帧(Phase 5 起为最高优先级关卡);
   - 资产解码哈希:全部 shp/tmp/pal/aud/vqa → 像素级 SHA;
   - 渲染:离屏截图差异分布报告;
   - 地图 UID:全 maps 目录 SHA1 比对。
2. **单测**:移植 OpenRA.Test 核心用例(MiniYaml、寻路、定点、格式);新代码行覆盖导向 ≥ 核心 70%。
3. **消毒器**:CI 全程 ASan+UBSan(clang,`-fsanitize=address,undefined`,CMake 开关 `ORA_SANITIZE=ON`)。
4. **静态分析**:clang-tidy + 自定义 lint(sim/ 无浮点、禁 VLA、事件订阅成对检查)。
5. **desync 排查工具**:`tools/replaydiff` 输出首个不一致帧的 actor/字段级 diff(复用 SyncReport 语义)。

---

## 9. 风险登记册

| # | 风险 | 等级 | 对策 |
|---|---|---|---|
| R1 | 确定性破坏(UB/浮点/顺序)导致 desync | 高 | §4.4 全套;replay 对拍前置于一切 gameplay 移植验收 |
| R2 | 反射框架设计返工 | 高 | Phase 2 原型先跑通 Health 等 5 个代表 trait 再铺开;schema 导出器消除手工漂移 |
| R3 | 工具链 C23 支持不齐(多编译器矩阵漂移) | 低(已消解) | 已锁定单一工具链 CMake+Clang(gnu23)+Ninja,探针验证通过;代价是放弃 MSVC/GCC 原生构建,CI 统一 LLVM 版本消除漂移 |
| R4 | 25 万行规模下进度失控 | 高 | 阶段验收制;每个 trait 移植=源文件行号注释+对拍;优先核心 20 trait 先出可玩版本 |
| R5 | Lua 5.4 与 Eluant(5.2)脚本兼容差异 | 中 | 现有 mods 全部脚本回归;必要时锁 5.3 |
| R6 | Fluent 无成熟 C 实现 | 中 | fluent-rs+cbindgen 或自研子集(项目未用 isolating,面小) |
| R7 | 内存生命周期 bug(use-after-free) | 高 | 分区策略(§4.3)+ ASan + 帧末销毁硬规则 + 事件注销静态检查 |
| R8 | GLES/ANGLE 路径移植后显示差异 | 低(二期) | profile 分支保留,首期锁定 GL 3.2 Core |
| R9 | 联机跨版本兼容(G3)不达 | 中 | 以 replay 对拍(G2)为硬验收,G3 降级为力争 |
| R10 | 第三方库许可证合规 | 低 | 全部选 MIT/Zlib/Public-Domain 系;SDL2 Zlib、OpenAL-soft LGPL(动态链接) |

---

## 10. 起步行动清单(本周)

1. ~~初始化 `OpenRAC` 仓库结构 + CMake C23 基线~~ **已完成**:`CMakeLists.txt`(cmake + clang/`gnu23` + ninja,含 `-fwrapv` 确定性基线与 ASan/UBSan 开关)+ `tools/c23_probe.c` 特性探针,构建与 ctest 全绿。
2. 移植 `core/` 定点原语与 MersenneTwister,完成与 C# 的首轮对拍。
3. 移植 MiniYaml 解析器(Phase 1 核心),接入 759 文件解析冒烟。
4. 搭 `tools/schema_dumper` 骨架:先导出 interfaces.h 与 Health/Mobile/Armament 三个 trait 的字段表,打通"yaml → C 结构体"最小闭环。
5. 建 `tools/replaydiff` 骨架(解析 replay 元数据 + 逐帧 SyncHash 提取),提前于 Phase 3 就绪。

构建命令(已验证可用):

```sh
cmake -S . -B build -G Ninja -DCMAKE_C_COMPILER=clang -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure          # 消毒器构建:加 -DORA_SANITIZE=ON
```

---

## 附:本计划引用的关键源码锚点速查

| 机制 | 文件:行 |
|---|---|
| 主循环/双时间戳 | OpenRA.Game/Game.cs:771-897 |
| InitializeMod 重建时序 | Game.cs:474-532 |
| FieldLoader 解析分发 | OpenRA.Game/FieldLoader.cs:854-892 |
| ObjectCreator 实例化 | OpenRA.Game/ObjectCreator.cs:78-137 |
| TraitDictionary 二分查询 | OpenRA.Game/TraitDictionary.cs:145-327 |
| trait 拓扑构造序 | OpenRA.Game/GameRules/ActorInfo.cs:104-167 |
| World.Tick 顺序 | OpenRA.Game/World.cs:413-455 |
| SyncHash 公式 | World.cs:482-512;Sync.cs:78-107(Reflection.Emit) |
| 定点原语 | WPos.cs / WDist.cs / WAngle.cs:59-99 / Map.cs:976 |
| Order 线格式 | Network/Order.cs:29-42,326-468;Server/ProtocolVersion.cs:16-80 |
| 锁步循环 | Network/OrderManager.cs:234-328 |
| 大厅命令表 | Mods.Common/ServerTraits/LobbyCommands.cs:163-190 |
| MiniYaml 合并/继承/弱删除 | MiniYaml.cs:459-564,613-647 |
| 渲染三级合成 | Renderer.cs:237-360 |
| 调色板纹理方案 | Graphics/HardwarePalette.cs |
| A*/HPF 寻路 | Mods.Common/Traits/World/PathFinder.cs;Pathfinder/PathSearch.cs;HierarchicalPathFinder.cs |
| Activities 状态机 | OpenRA.Game/Activities/Activity.cs:49-295 |
| Widget 反射加载 | OpenRA.Game/Widgets/WidgetLoader.cs:46-81;Widget.cs:322-332 |
| Lua 反射暴露 | Game/Scripting/ScriptObjectWrapper.cs:19-94;ScriptContext.cs:119-345 |
| 文件格式群 | Mods.Cnc/FileFormats/*(MixFile/VqaVideo/AudReader/VxlReader…) |
| 地图格式/UID | Game/Map/Map.cs:160-313,708-765,784-888 |
| 服务器事件循环 | Game/Server/Server.cs:148,348-402,904-934,1444-1457 |
