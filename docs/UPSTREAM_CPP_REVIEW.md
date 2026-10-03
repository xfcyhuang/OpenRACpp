# 上游代码审阅:C++ 特性优化点与陈旧模块改造清单

> 审阅对象:上游 OpenRA @`7d57605bca`(Z:/source_code/OpenRA),为 OpenRACpp Phase 4-8 移植提供输入。
> 方法:核心热路径(Game/World/Actor/TraitDictionary/Sync/Order/OrderManager/Activity/Widget/Script/Server)逐文件精读 + 三路并行扫描(数据反射链、渲染平台层、Mods.Common 仿真大面/寻路/武器)。
> 所有条目均给出上游 file:line 证据。分三类:**A. C++ 语言特性可优化**(C# 做不到或代价高)、**B. 陈旧模块可改造**、**C. 确定性移植风险警示**。
> 原则:凡进入同步路径的改动必须保持语义等价(replay SyncHash 对拍);结构性重设计需登记 COVERAGE 偏离项。

---

## 0. 与现有计划的衔接(已覆盖、勿重复立项)

以下方向已在 PORTING_PLAN/gen/ 路线上,审阅证实其正确性,不重复展开:

- 反射链 → `gen/` constexpr 描述表 + TypeId 键空间(interfaces_gen.h,6048 类型):对应上游 FieldLoader/ObjectCreator/TypeDictionary 的全套运行时反射;
- SyncHash 的 Reflection.Emit → sync_gen.cpp 编译期生成(84 类型);
- `import std;`/from_chars/无 locale 解析、arena 内存分区、`std::expected` 错误处理。

---

## 一、A 类:利用 C++ 语言特性的优化机会(按收益排序)

### A1. 数值修正热路径的 decimal → __int128 精确十进制(改动小、命中面最广)⚠️ 兼 C 类风险

上游证据:`OpenRA.Mods.Common/Util.cs:203-211`:

```csharp
public static int ApplyPercentageModifiers(int number, IEnumerable<int> percentages) {
    var a = (decimal)number;
    foreach (var p in percentages) a *= p / 100m;   // 128 位软十进制,无硬件支持
    return (int)a;
}
```

- 调用面:**22 个文件**(grep 证实),含 `Armament.MaxRange()`(每次射程询问)、`Mobile.MovementSpeedForCell`(每次跨格)、`Health`/`DamageWarhead`/`SpreadDamageWarhead`(每次命中)、`RevealsShroud.Range`(每 tick)、`Missile`/`Aircraft`/`Power` 等 —— 全部同步路径。
- 参数是 `IEnumerable<int>`:调用侧普遍 `.Append(x)`/`.ToArray()`(LINQ 链分配),如 `Armament.cs:167-170` 每次 `MaxRange()` 都 `rangeModifiers.ToArray()`。
- **关键数学事实**:`100 = 2²×5²`,`p/100m` 是精确有限小数;int 范围内链式乘积的有效数字远小于 decimal 的 28 位上限,故该函数**等价于精确有理数运算 + 最终 (int) 截断**。C++ 用 `__int128` 累积分子的分子(分母恒为 100^k),链长 ≤4 时 |分子| ≤ 2×10⁹×100⁴ ≈ 2×10¹⁷ < 2⁶³,无溢出。**必须**黄金对拍验证边界值舍入(见 C 类警示)。
- C++ 形态:`int apply_percentage_modifiers(int64_t n, std::span<const int> ps)` + 调用侧 `inplace_vector<int,8>`/`static_vector` 传参,零分配零间接。

### A2. 寻路 A* 内环:每节点展开一次堆分配 + 每次搜索全图 memset

上游证据:
- `OpenRA.Mods.Common/Pathfinder/DensePathGraph.cs:123` — `var validNeighbors = new List<GraphConnection>(...)`,`GetConnections` 被 `PathSearch.Expand()`(PathSearch.cs:254)对每个弹出节点调用一次 → 千节点搜索 = 千次分配;
- `Pathfinder/CellInfoLayerPool.cs:35-51` + `OpenRA.Game/Map/CellLayer.cs:37-44` — 池仅 4 层,每层取出时 `layer.Clear()` 做 O(地图格数) memset,CellInfo 28B/格,256×256 图 ≈ 1.8MB/次搜索;
- `OpenRA.Game/Map/CellLayer.cs:56-90` — 索引器每次访问做 GridType 分支 + isometric `(x-y)/2` 除法 + setter 事件空调用检查;
- `OpenRA.Game/Primitives/PriorityQueue.cs:29-158` — 二叉堆"层级加倍"索引布局 + `Array.Resize`,每搜索 new 不复用。

C++ 方案:
- **世代标记(generation stamping)**:每层附 `u32 epoch[]`,`++curEpoch` 免清零 —— 收益最大的单项;
- 邻居迭代改回调风格 `template<class F> void for_each_neighbor(CPos, F&&)`,邻居 ≤10,栈上 `small_vector<GraphConnection,10>`;
- 状态压缩 `{u32 costSoFar; u32 costTotal:30, status:2; u32 prevIndex}`,格点 u32 索引代替 CPos(12B);
- 网格类型编译期化 `template<GridType>`,索引 `constexpr` + always_inline,"带事件的写"与"纯写"分离 API;
- 平坦 4 叉堆 + `clear()` 只重置 size,放入每 Locomotor 复用的搜索上下文;
- 启发函数值类型策略对象(替代 `DefaultCostEstimator` 闭包 + 每次的 LINQ `Min`,PathSearch.cs:149)。

### A3. TraitDictionary / 接口查询:哈希+二分+枚举器 → 列式 TypeId 数组

上游证据:`OpenRA.Game/TraitDictionary.cs:45-58`(容器按 `Dictionary<Type,...>` 反射实例化)、`100-119`(查询返回 `IEnumerable` + 手写枚举器)、`127-135`(`ApplyToActorsWithTrait` 走 `Action` 委托,无法内联);调用面:grep `TraitsImplementing<` 331 处、`TraitOrDefault<` 200 处、`.Trait<` 438 处。`OpenRA.Game/Primitives/TypeDictionary.cs:41-54` 的 `Add` 每次反射枚举 `GetInterfaces()+BaseTypes()` —— **每个运行时 actor 的每个 trait 都执行**(真·每帧路径,区别于加载期反射)。

C++ 侧已有 iface_id 方向;审阅补充:
- 常用接口查询结果在 Actor 构造时物化为紧凑数组 —— 上游 `Actor.cs:179-195` 已对 17 个接口这么做(16 连 `is` 类型测试),C++ 侧用 gen/ 上行转换表零成本等价,且**可把范围扩到全部 INotify* 热接口**(受击/所有者变更/队列等);
- `ApplyToActorsWithTraitTimed<ITick>`(World.cs:448,每 tick)从委托调用改为模板参数 + 可内联 lambda。

### A4. 条件系统:三个 string 字典 → intern 后的紧凑计数

上游证据:`OpenRA.Game/Actor.cs:89-109, 560-615` — `Dictionary<string,ConditionState>`(内含 `HashSet<int> Tokens` + 委托 List)+ `Dictionary<int,string>` + `Dictionary<string,int>`,每次 Grant/Revoke 2 次字符串哈希 + HashSet 增删;观察者签名 `VariableObserverNotifier(Actor, IReadOnlyDictionary<string,int>)` 把整本字典传给每个观察者。AutoTarget 换姿态、Harvester 空载等高频触发。

C++ 方案:条件名在 Ruleset 加载期 intern 为 `u32 ConditionId`;每 Actor 活跃计数用小数组(按该 actor 涉及的 condition 局部索引);观察者改传**增量 span**(`span<const CondDelta>`)而非全字典。⚠️ 观察者遍历顺序与通知时序需保持语义(见 C 类)。

### A5. 渲染命令封送:消息队列+装箱+锁 → 值类型命令缓冲 + 无锁 SPSC(Phase 4 最大重设计点)

上游证据:`OpenRA.Platforms.Default/ThreadedGraphicsContext.cs`:
- `477-505` — 每个 GL 调用 = 1 次装箱(`Post(Action<object>, object)`,值元组/枚举装箱)+ `lock` + `Monitor.Pulse`;`SpriteRenderer.Flush`(SpriteRenderer.cs:78-88)每 BlendSpan 发 2 条 → 每帧几十到几百次;
- `599-630` — 顶点数据三段拷贝(写入托管数组 → Array.Copy 进池数组 → GCHandle pin → glBufferSubData);`SetData(ref T[] ...)` 的"数组换所有权"技巧返回可能含垃圾的池数组;
- `719-755` — 纹理上传按 LOH 阈值 85000 字节分支,小纹理每次 new+copy;
- `210, 298-309` — 每个 Message 常驻一个 AutoResetEvent 内核句柄;
- `103-123` — 启动路径 `MakeGenericMethod().Invoke()` 反射构造泛型 VB(消息通道是 object 所致);
- `Sdl2PlatformWindow.cs:345-353` — **Windows 窗口模式整体禁用渲染线程**(最小化/恢复兼容性坑),Post/Send 同线程空转仍付装箱/锁成本。

C++ 方案:`struct GfxCmd { CmdKind; union/variant }` 值类型命令 + 双缓冲 command list + `std::atomic::wait/notify`;`glBufferStorage` + persistent mapping + `glFenceSync` 顶点直写;模板化 `create_vb<V>(span<const V>)`;统一线程模型消除 Windows 特例。

### A6. GL/着色器层:手写绑定与字符串 uniform

上游证据:
- `OpenGL.cs:240-506, 666-669` — 801 行手写 delegate 绑定 + `Marshal.GetDelegateForFunctionPointer`;每调用后 `CheckGLError()`(未启用 KHR_debug 时退化为高频 `glGetError()`,每帧数百次驱动往返);
- `Shader.cs:134-145` — 全局单 VAO,每次 Bind 重播 `glVertexAttribPointer`;`184-218` — uniform 按字符串名(每次 `glUseProgram` + 字典查找),`ThreadedShader` 再叠 Post+元组分配;`147-172` — 每次 flush 逐采样器 `glIsTexture` 驱动查询;
- `Sdl2GraphicsContext.cs:207-269` — blend 状态无缓存全量重设;`FrameBuffer.cs:90,108` — 每次 FBO 绑定/解绑内嵌 `glFlush()`。

C++ 方案:glad2 直连;每 (vertex format, shader) 一个 VAO 或 attribBinding 模型;uniform 编译期枚举 + UBO;纹理句柄 RAII + 绑定 diff;release 构建剔除错误轮询。

### A7. 渲染收集与批处理:每帧对象分配洪水 → SOA + 帧 arena

上游证据:
- `WorldRenderer.cs:141-175` — 每帧每可绘制体 new `SpriteRenderable`(类);`WithAlpha/WithTint` 以 new 对象实现不可变修改(SpriteRenderable.cs:63-91);`319-323` — overlay 分组 `GroupBy(prs => prs.GetType())` LINQ + RTTI(注释自认 HACK);
- `Vertex.cs:17-48` — 48B 胖顶点,quad 4 顶点重复 C/tint(Util.FastCreateQuad 每精灵 192B);索引用 uint32(Util.cs:26-43);
- `TerrainSpriteLayer.cs:55-88` — 全图 AOS 顶点数组(256² 图 ≈12.6MB)+ `PaletteReference[]` 对象数组;任一调色板变化 → O(地图) 顶点重写 + 全部行标脏;`112-149` — 每 cell `new Vector3[4]` 临时数组;
- `HardwarePalette.cs:132-154` — 每帧无条件全量重传调色板纹理(即使无 modifier 无变化);`SpriteRenderer.cs:164-176` + `PaletteReference.cs:26` — 每 RGBA 精灵 2-4 次字符串哈希查 `HasColorShift`。

C++ 方案:渲染收集 SOA(`vector<RenderItem>` 全 POD,帧 arena 重置);`with_alpha` 值语义;type_id 计数分段替代 GroupBy;instanced quad(≈36-40B/精灵)+ u16 索引;地形层拆 palette_index(u16)/tint(u8x4) 独立数组;调色板 dirty 行/指纹上传;palette 句柄缓存 bool。

### A8. 空间索引与高频查询:对象键 Dictionary → ActorID 数组

上游证据:
- `OpenRA.Game/Primitives/SpatiallyPartitioned.cs` — bins 是 `Dictionary<T,Rectangle>`(T=Actor 引用键),跨 bin 查询 `new HashSet<T>` 去重,`MutateBins` 走静态 Action 委托;
- `Traits/World/ScreenMap.cs:146-208` — 鼠标/渲染空间查询全走 `Where/Select` LINQ 链;
- `ActorMap.cs:644-671`(`ActorsInBox` yield 迭代器)+ `WorldUtils.cs:69-75`(再叠 Where)—— 21 处调用方;
- **关键事实:ActorID 是稠密递增 uint** —— 上游所有 `Dictionary<Actor,...>`/`HashSet<Actor>` 在 C++ 侧都可以换成 arena slab 数组直接索引,哈希彻底消失。

C++ 方案:`for_each_actor_in_circle(pos, r, f)` 模板回调(bin 遍历内联);跨 bin 去重用"最后查询标记"数组代替 HashSet;`ScreenMap` 的增删集合(addOrUpdateActors 等)换 id 数组。

### A9. 武器/攻击链的每次开火分配

上游证据:`Armament.cs:167-170, 322-362`(每次 `MaxRange()` → `ToArray` + decimal;每次开火 3 个 modifier 数组 + 2 委托 + 闭包延迟动作);`DamageWarhead.cs:78-88`/`SpreadDamageWarhead.cs:106-122`(每次命中 2 迭代器 + 闭包 + `new WarheadArgs`/`AttackInfo` class);`AutoTarget.cs:355-470`(每单位每 3-8 tick:ToList + Concat + 嵌套闭包工厂 + 二次枚举);`Activities/Attack.cs:192-207`(每 tick 重算 min/max 射程)。

C++ 方案:modifier 在条件授予/撤销时失效重算(缓存 `static_vector<int,8>`);`AttackInfo`/`WarheadArgs` 改 struct 按值;AutoTarget 扫描单层手写循环 + 复用候选缓冲;目标未变时复用 armament 选择结果。

### A10. 资源/杂项类型紧化

- `ResourceLayer.cs:106-195` — 格内容含 string 引用(`Content[c].Type == resource.Type` 字符串比较,每格)→ u8 资源索引 + `{u8 type; u8 density}` 2B/格;事件 `Action<CPos,string>` → 索引;
- `Harvester.cs:217-229` — `Info.Resources.Contains(resourceType)`(ImmutableArray<string> 线性扫)→ 256 位掩码;`FindAndDeliverResources.cs:198-234` 的采金搜索是零启发式 Dijkstra 泛洪(450-1800 格/次)+ 每格双闭包;
- `HPF.cs:106-139` — `dirtyGridIndexes:HashSet<int>`(稠密小整数!)、`cellsWithBlockingActor:HashSet<CPos>`、`localCellToAbstractCell:Dictionary<CPos,CPos>`(每 grid 仅 100 格)→ 位图/CellLayer<u32>;
- `HPF.cs:204-243, 1133, 1241` — 抽象图 `Dictionary<CPos,List<GraphConnection>>` + `GetConnections` 每次 `ToList()` → 抽象节点编号化 + CSR 邻接;
- `Locomotor.cs:422-466` — `dirtyCells:HashSet<CPos>` 每次 Remove 探测;`UpdateCellBlocking` 内 `PerfSample` 每次打 2 次 QPC → 编译期开关。

### A11. 数据结构/解析的散点(加载期,影响首载时间)

上游证据与方案(节选自反射链审阅):
- `FieldLoader.cs:754-796` — 每 trait `ToDictionary()` 字符串字典 + 每字段反射 `SetValue` → 排序索引 + `lower_bound` 双指针匹配 + 成员指针直赋;
- `FieldLoader.cs:68-69, 167-177` — 上游手工维护 `BoxedTrue/BoxedFalse/BoxedInts[33]` 装箱缓存(装箱协议的自供自证)→ 强类型 `parse<T>(string_view)` 无堆分配;
- `Exts.cs:549-566` — `%` 百分比解析每值分配两个字符串 → `from_chars` 解析 `remove_suffix` 子区间;
- `Ruleset.cs:115/127` — 规则键每加载一次 `ToLowerInvariant()` 分配 → 大小写不敏感比较/intern 规范形;
- `MiniYaml.cs:408-435` — Merge 顶层 LINQ 管道 + 防环用 `ImmutableDictionary.Add` 函数式持久化(每 actor 键分配)→ vector + 小集合线性查;`489-495/524/636-646` — 键查找 O(n²)(上游已手写循环替 LINQ 但仍平方)→ 临时 flat 索引;
- `MiniYaml.cs:566-595` — `MergePartial` 递归核心每次:全局锁 + 整本 scratch 字典灌入再丢弃 + 错误格式化闭包 → 排序 + adjacent_find 查重,错误消息惰性;
- `Map.cs:294-312` — UID 计算用 N-1 层 `MergedStream` 装饰器(每字节穿 N 层虚 Read)→ SHA1 增量 update。

---

## 二、B 类:陈旧模块与遗留代码(可直接改造/删除)

| # | 模块 | 上游证据 | 改造建议 |
|---|---|---|---|
| B1 | **强制 2 的幂纹理 + 三级 pow2 FBO 合成** | `Texture.cs:84-85`、`FrameBuffer.cs:32-33`、`Renderer.cs:164-360` | GL3.2 core 完整支持 NPOT;1920×1080 窗口被迫用 2048² 双 FBO + 每帧 3 次全屏 clear + 2 次全屏拷贝。改 NPOT 精确视口 + 单级 blit;`BeginFrame` 对默认 FB 的 Clear 被 EndFrame 全屏 quad 覆盖,属冗余 |
| B2 | **FreeType 结构体手算偏移** | `FreeTypeFont.cs:27-36`(注释自认 "HACK: raw pointer offsets") | 9 个偏移按 32/64 位硬编码,FreeType 升级即静默读错内存。C++ 直接 `#include <ft2build.h>` 真类型 |
| B3 | **自研 PNG 592 行** | `Png.cs:288-387, 417-447, 506-585` | unfilter 逐字节标量循环;`Palette` 是 GDI `Color[]`。PORTING_PLAN 决定照抄保行为一致 —— 审阅建议维持照抄(对拍优先),但解码循环可 SIMD 化后以资产解码哈希对拍验证 |
| B4 | **DPI/窗口平台 hack** | `Sdl2PlatformWindow.cs:124-134`(裸 P/Invoke)、`181-215`(Linux 启动 `/usr/bin/xrdb -query` 子进程解析文本取 DPI!)、`237-265`(XChangeProperty 设 KDE 图标) | `SetProcessDpiAwarenessContext`;SDL 显示 API;平台差异收进 `ora::platform` |
| B5 | **窗口属性 getter 全加锁** | `Sdl2PlatformWindow.cs:42-120`(每个 getter `lock (syncObject)`),scissor 每次开 = 3 次锁 | `atomic<u64>` 打包双 u32 快照,getter 无锁 |
| B6 | **`TypeDescriptor.GetConverter` 兜底**(.NET 1.x 遗产) | `FieldLoader.cs:877-888`、`FieldSaver.cs:147-157` | C++ 类型表全覆盖,未注册类型直接编译错 |
| B7 | **magic string 运行时协议** | 类名拼接 `xxx+"Info"/"Warhead"/"Loader"/"Widget"`(ActorInfo.cs:84、WeaponInfo.cs:153/166、ObjectCreator.cs:151、WidgetLoader.cs:80);Yaml 语义串 `"Inherits"/"^-abstract"/"-remove"/"Defaults"`(MiniYaml.cs:470/489、WeaponInfo.cs:141 自认 HACK) | gen/ 注册表静态键;解析器词法层产出 `enum NodeKind` 后续 switch |
| B8 | **全局可变静态委托改错误策略** | `FieldLoader.cs:55-59`、`Settings.cs:442-475`(存旧值→替换→finally 恢复)、`ObjectCreator.cs:76`(注释:"HACK: The linter does not want to crash") | 显式 `ParseContext`/`expected<T,ParseError>` 参数化,消灭时序依赖 |
| B9 | **裸 Task+轮询加载屏** | `Ruleset.cs:149-163, 211-223`(`new Task(...).Start(); while(!Wait(40))` 复制粘贴两处) | `jthread` + 原子进度 + `condition_variable::wait_for` |
| B10 | **程序集泄漏 workaround** | `ObjectCreator.cs:23-64`(对整个 mod DLL 求 SHA1 判断是否已载 + 永久字典,.NET 不能卸载程序集) | C++ 静态注册表,整套机制不存在 |
| B11 | **sim 内自认的 HACK(逐条修正机会)** | `Mobile.cs:261/770-772/865`("This entire method is a hack"、NearestMoveableCell 应被正规寻路取代);`ResourceLayer.cs:159-161`("should not be lerping to 9... too disruptive to fix") | 重写时按注释指向修正,登记 COVERAGE 偏离 |
| B12 | **三处手工镜像的阻塞判定** | `Locomotor.cs:328-331/460-462` 与 `HPF.cs:656-690` 靠注释互相提醒同步 | 收敛为单一函数/表驱动 |
| B13 | **阻塞判定逻辑散点** | `GraphConnection` 构造函数内运行时参数校验(每邻居每展开执行);`PerfSample` 常驻热路径 | assert/debug-only;采样编译期开关 |
| B14 | **阻塞判定之外的每帧 LINQ 残留** | `World.cs:490-508`(SyncHash 对 `ActorsHavingTrait<ISync>()` 的惰性枚举)、OrderManager.cs:202/308(`pendingOrders.All(p=>...)` 每帧闭包)、Server.cs:1430-1460(`Conns.SingleOrDefault` 每帧 per player + 每包事件对象分配) | 手写循环 + 事件 variant |
| B15 | **Settings diff 用字符串比较** | `Settings.cs:67-82`(`FormatValue` 两次格式化成字符串再比较检测差异) | 按字段值比较 |
| B16 | **OnlyFor 工具类** | `PriorityArray.cs`(递归 BubbleUp)仅 MapGenerator 使用 | 不移植 |
| B17 | **UI 层散点** | `Widget.cs:242`(每实例一个 `Func<bool> IsVisible` 闭包)、`329-330`(Logic 反射实例化)、`IntegerExpression.Evaluate(ReadOnlyDictionary)` 每布局字符串变量名查字典 | 成员函数指针/位标志;gen/ 表;变量 intern |
| B18 | **Lua 绑定全反射** | `ScriptObjectWrapper.cs:32-45`(`GetConstructor(argTypes).Invoke`)、`47-62`(`WrappableMembers(GetType())` 反射枚举) | gen/lua_api.cpp 编译期绑定表(Phase 8 已计划,审阅补充:热路径是每脚本对象成员访问的字符串字典 + 委托) |

**上游已做对的优化(照抄,勿倒退)**:MiniYaml stringPool 驻留(MiniYaml.cs:203-211)、`ReadAllLinesAsMemory` 池化行读取、YamlValue ref-struct、Shroud 的 ProjectedCellLayer 计数器设计(Shroud.cs:159-243,SoA 友好)、渲染排序 key 打包 `(zKey<<32)|index`(WorldRenderer.cs:163-168)、TraitDictionary 平行数组 + BinarySearchMany、`CellStatus` 惰性删除配合优先队列、sim 主循环零显式 GC 干预(grep 证实)。

---

## 三、C 类:确定性移植风险警示(优化前必须对拍)

1. **decimal 语义**(A1):PORTING_PLAN §4.7 断言"唯一 decimal 使用点 WPos.cs:58-72"**不完整** —— `ApplyPercentageModifiers` 22 文件全是 decimal 同步运算。C# decimal 链在此语境下是精确十进制(100=2²×5² 保证 p/100 有限),`__int128` 分子精确复刻可行,但**必须**用黄金对拍覆盖:链长>2、负百分比、乘积超 int32、截断边界(|a| 恰在整数边界)等用例。
2. **条件系统改造**(A4):Grant/Revoke 顺序 → notifier 调用顺序 → trait 内部状态(如速度修正缓存)必须保持遍历序;`conditionCache` 传观察者的**全字典语义**若改增量 span,观察者实现(约 131 处 `IObservesVariables`)读的是整表 —— 保险方案:保留"按 id 索引的全量只读数组"视图,仅消灭字符串与字典。
3. **SyncHash 相关**:TraitDictionary 列式化后 `ActorsWithTrait<ISync>` 的遍历序 = 平行数组的 ActorID 升序,必须保持;`Actor.SyncHashes` 数组序 = 构造序(Trait 拓扑序),列式存储不能重排。
4. **世代标记寻路**(A2):只需保证最终路径选择与上游一致 —— 上游 PQ 的"层级加倍"堆在**相等代价**时的弹出顺序可能不同,对拍需覆盖平局路径场景(或精确复刻其堆序)。
5. **调色板 dirty 行**(A7):`TerrainSpriteLayer.UpdatePaletteIndices` 上游全图重写 = 全行重传;dirty 行方案渲染结果应逐像素一致(渲染对拍关卡)。

---

## 四、落地建议(映射 Phase)

| Phase | 应纳入的审阅结论 |
|---|---|
| Phase 4(平台+渲染) | A5/A6/A7、B1/B2/B4/B5;调色板 dirty 化(C-5 对拍) |
| Phase 5(主循环+gameplay) | **A1(decimal,__int128 精确复刻 + 对拍)**、A2(寻路全套)、A3/A4(扩展 Actor 构造期接口物化)、A8/A9/A10、B11/B12/B13 |
| Phase 6(UI) | B17;Widget 加载走 gen/ 表 |
| Phase 7(网络/服务器) | B14(Server 事件 variant、SingleOrDefault) |
| Phase 8(Lua) | B18 |
| PORTING_PLAN 修订 | §4.7 decimal 断言更新;§4.5 落实 arena(Phase 0 遗留项)承接 A7/A9 的帧分配器需求 |

> 本文档由 2026-10-04 上游审阅生成;证据锚点均基于 @7d57605bca 工作区实际读取。
