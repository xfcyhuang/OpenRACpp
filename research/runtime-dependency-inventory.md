# 上游运行时依赖盘点（wayfinder #20）

**问题**：上游七程序集对 .NET 运行时服务的依赖有多深、多广？每项给出文件数/调用点数量级，并标注哪些进入了 SyncHash / 字节恒等这类可观测面。

- 基准：`OpenRA/OpenRA @ 1f594e320`（见 `UPSTREAM.baseline`），只读参照 `Z:\source_code\OpenRA`。
- 范围：OpenRA.Game 224 文件 / 44,039 行；OpenRA.Mods.Common 1085 / 159,311；OpenRA.Mods.Cnc 140 / 20,784；OpenRA.Mods.D2k 23 / 3,551；OpenRA.Platforms.Default 18 / 4,814；OpenRA.Server 1 / 124；OpenRA.Utility 1 / 178。合计 **1492 文件 / 232,801 行**。
- 布局说明：Server / Utility 程序集只有薄入口 `Program.cs`，其核心（`OpenRA.Game/Server/`、`OpenRA.Game/UtilityCommands/`、`OpenRA.Game/Network/`）物理上位于 OpenRA.Game 内，本盘点按文件物理位置统计。
- 方法：rg 正则计数（`rg -l` 计文件数、`--count-matches` 求和），配合对 `ObjectCreator` / `FieldLoader` / `Sync` / `TraitDictionary` / `FieldSaver` / `SyncReport` / `VariableExpression` 等核心文件的全文精读。计数是数量级参照，非精确值；含 BCL 同名方法（如 `List.Contains`）的口径已在条目内注明。
- 风险标注：🔴 = 直接卡 SyncHash / 字节恒等 / 黄金对拍判据；🟡 = 影响行为等价但不直接进判据；⚪ = 平台/性能层，判据外。

---

## 1. 异常

**量化**：`throw` 986 处（318 文件）；`catch` 249 处（116 文件）；`finally` 37 处；空 catch 块 16 处（13 文件）；裸 rethrow 14 处。自定义异常类仅 **6 个**（`YamlException`、`MissingFieldsException`、`NoSuchCommandException`、`SheetOverflowException`、`MapGenerationException`、`DevException`）。throw 类型分布前五：YamlException 167、InvalidOperationException 153、InvalidDataException 153、ArgumentException 111、LuaException 91。

**catch 做控制流的证据**：

- `FieldSaver.FormatValue`：`catch (InvalidOperationException) { return ""; }` —— 用异常兜底 ImmutableArray 默认值序列化。
- `InstallerUtils` / `MapCommand` / `Sdl2PlatformWindow`：`catch { return false; }` —— 探测式控制流。
- `OpenRA.Mods.Common/Traits/World/SmudgeLayer.cs:78`：**地图加载循环内的空 catch**，坏 smudge 条目被静默跳过 —— 哪些条目被吞决定了地图加载产物内容（🟡 靠近字节恒等面：地图往返/规则 lint）。
- `Sync.RunUnsynced`（`OpenRA.Game/Sync.cs`）：用 `InvalidOperationException` 做「非同步代码不得改状态」的运行时哨兵 —— 异常即断言机制。
- `ObjectCreator.LoadAssembly` 依赖 `AppDomain.AssemblyResolve` 事件兜底程序集解析（.NET 加载器语义）。

**汇编路线必须复刻什么**：错误路径的**外部可见效应**（哪条数据被拒、哪条被吞、日志/错误消息文本、进程退出码），而不是 CLR 异常机制本身；6 个自定义异常 + 错误码/跳转即可承载。

风险：🟡（SmudgeLayer 类空 catch 改变数据接受面；异常消息文本进 debug 日志，若黄金对拍比对日志则升 🔴）。

## 2. 委托 / 事件 / lambda

**量化**：`event` 声明 28 处（18 文件）；`Func<` 513 处（177 文件）；`Action` 702 处（240 文件）；lambda 箭头 `=>` 6974 处（1044 文件，占文件数 70%）；`+=` 订阅 383 处；显式 `delegate` 类型声明 89 处。`Delegate.CreateDelegate` 1 处（FieldLoader 的 `[LoadUsing]` 按名取加载函数）；`Marshal.GetDelegateForFunctionPointer` 1 处（OpenGL 绑定）。

**位置判断**：event 集中在 UI/生命周期层（`Game.OnQuit`、`BeforeGameStart`、`ObservableCollection.OnAdd/OnRemove`、`Viewport.ViewportTick`、`CellLayer.CellEntryChanged`）——全部在确定性模拟面之外。热路径上的多播已被上游自己消解：`Actor` 构造时把 `IResolveOrder` / `IRender` 等接口 trait 物化为**接口数组**缓存（`Actor.cs:150-215`，PERF 注释），`TraitDictionary.ApplyToAll` 也是数组直调而非委托。

**汇编路线必须复刻什么**：函数指针表 + 统一调用约定；`Func/Action` 闭包对应「捕获环境结构体 + 函数指针对」，接口数组缓存模式与汇编天然契合（vtable 槽）。

风险：⚪（模拟路径无事件依赖）。

## 3. LINQ

**量化**：`using System.Linq` 630 文件（42%）。调用点：`Select` 559、`Where` 577、`Any` 349、`First/FirstOrDefault` 434、`SelectMany` 149、`OrderBy` 145 + `OrderByDescending` 43、`ToDictionary` 88、`ToList/ToArray` 805、`Contains` 855（含 string/List 同名方法）、`Except/Intersect/Union/Concat` 162、`GroupBy` 27、`Distinct` 41、`Single` 49、`Aggregate` 9。

**进可观测面的语义点**：

- **OrderBy 是稳定排序**（🔴）：gameplay 决策直接消费排序结果——`AttackBase.cs:457/495`（按 `IsTraitPaused` 排序选 armament）、`AutoCarryable.cs:74` / `AutoCarryall.cs:139`（按距离平方选携带目标）、`CaptureManager.cs:162`（`OrderBy(...).ThenBy(...)` 链）、BotModules 多处。排序稳定性差异会改变目标选择 → 帧状态 → SyncHash。C++ 必须用 stable_sort 并复刻 ThenBy 多键语义。
- **ToDictionary / Dictionary 构造的插入序**（🔴，见 §8）：LINQ `ToDictionary` 产出的字典迭代序 = 插入序，进 yaml 回写。
- 延迟执行 + 多次枚举语义（🟡）：`EnabledTargetablePositions = ...SelectMany(...)` 等惰性序列每帧重算，重新实现若改成物化列表会改变分配与迭代成本，但**不改变语义**（无副作用选择器为主）。

**汇编路线必须复刻什么**：一小批「语义兼容集合算法库」——稳定多键排序、then-by、group-by（首见序）、distinct（首见序）、except/intersect/union（序语义按 .NET 文档逐个钉死）、延迟枚举协议（或编译期展开为循环）。

风险：🔴（排序稳定性与集合算法的迭代序语义直接改变 gameplay 决策与 yaml 输出）。

## 4. 反射 / 特性 / 运行时代码生成

**量化**：`typeof(` 334（98 文件）；`.GetType()` 118（49）；`GetProperty/GetField/GetMethod(s)` 85（43）；`GetCustomAttributes` 54（24）；`Activator.CreateInstance` 8（4）；`MakeGenericMethod/MakeGenericType` 17（6）；`BindingFlags` 64（14）；`.Invoke(` 115（57，含委托 Invoke）；`Assembly.GetTypes` 9。特性标注 `[Desc]/[Serialize 系]/[VerifySync]/[LoadUsing]` 等约 3383 处（692 文件）。

**四大装配链（精读结论）**：

1. **ObjectCreator**（`OpenRA.Game/ObjectCreator.cs`）：按类名跨程序集查 Type（缓存）→ `[UseCtor]` 特性选构造器 → 反射 `Invoke` 构造。供 trait/widget/规则/utility 命令装配（`GetTypesImplementing<T>` + `Activator`）。同时实现自定义 `AssemblyLoader` + `AppDomain.AssemblyResolve`（.NET 程序集加载语义，mod DLL 热插拔）。
2. **FieldLoader**（`OpenRA.Game/FieldLoader.cs`）：yaml → 对象字段绑定。`ConcurrentCache<Type, FieldLoadInfo[]>` 缓存反射字段表；`FrozenDictionary<Type, delegate>` 分发解析；泛型集合（`List<>`/`HashSet<>`/`Dictionary<,>`/`ImmutableArray<>`/`FrozenSet<>`/`FrozenDictionary<,>`/`BitSet<>`/`Nullable<>`）经 `MakeGenericType` + `MakeGenericMethod(...).Invoke` 反射构造；兜底 `TypeDescriptor.GetConverter`；`[LoadUsing]` 经 `Delegate.CreateDelegate` 按名绑定静态加载函数；装箱缓存 `BoxedInts`（0..32）证明此路径被认真优化过（规则/地图/actor 初始化全走它）。
3. **TraitDictionary**（`OpenRA.Game/TraitDictionary.cs`）：`typeof(TraitContainer<>).MakeGenericType(t)` 反射建容器（每 trait 类型一次，缓存）；**每次 `AddTrait`（每个 actor 创建）都反射 `t.GetInterfaces()` + `t.BaseTypes()` 枚举接口/基类链** —— 这是反射进 actor 创建热路径的直接证据（接口枚举结果稳定，等价于编译期可知的信息）。
4. **WidgetLoader**（`OpenRA.Game/Widgets/WidgetLoader.cs:80`）：`ObjectCreator.CreateObject<Widget>(widgetType + "Widget", args)` —— UI yaml 装配复用同一反射机制。

**运行时代码生成仅 3 处**：

- **`Sync.GenerateHashFunc`**（`OpenRA.Game/Sync.cs:78-107`）🔴：`DynamicMethod` + `ILGenerator` 为每个 `ISync` 类型生成哈希函数——扫描 `[VerifySync]` 字段/属性，Emit `Ldfld/Call` + `XOR` 链 + 类型专用哈希（`HashInt2 = ((X*5)^(Y*3))/4`、`CPos.Bits`、`Actor.ActorID<<16`、`Player/Target ... *0x567`、bool→`0xaaa/0x555` 常量陷阱）。这是 **SyncHash 可观测面的直接组成部分**。
- `SyncReport`（`OpenRA.Game/Network/SyncReport.cs:243-277`）：Expression tree `Compile()` 生成 desync 报告 getter（诊断输出，判据外）。
- `VariableExpression`（`OpenRA.Game/Support/VariableExpression.cs:663`）：Expression tree 编译 yaml 条件/整数表达式（`BooleanExpression`/`IntegerExpression` 字段），求值结果驱动 gameplay 条件（🟡）。

**汇编路线必须复刻什么**：反射机制的**外部行为**——「字符串类名 → 构造实例 → 按 yaml 键写字段」三段式装配，可用编译期注册表（每类型一条 {名字, 字段表, 构造 thunk}）等价承载，**不需要** System.Reflection 等价物；`AddTrait` 的接口枚举换成编译期接口位掩码；三处代码生成换成 C++ 模板/宏生成的等价函数。**哈希公式本身必须逐位复刻**（含 0xaaa/0x555、*0x567、<<16、XOR 链与字段遍历顺序——顺序即结果）。

风险：🔴（Sync.Emit 的哈希算法与字段序；FieldLoader 的解析接受集决定规则加载成败 → 地图/规则字节恒等）；🟡（VariableExpression 求值语义）。

## 5. 泛型

**量化**：泛型类/接口声明 41 个；泛型方法 201 处；`where` 约束 116 处；使用面：`IEnumerable<` 1218（498 文件）、`Dictionary<` 852（317）、`List<` 711（285）、`HashSet/FrozenSet/ImmutableArray/FrozenDictionary<` 1009（368）。接口总数 355 个（68 文件）；`TraitInfo` 派生 296 个（292 文件）。

**泛型 + 虚方法组合点**：`TraitInfo<T> : TraitInfo` 覆写 `Create(ActorInitializer)`（每 trait 一个虚构造工厂）；`TraitContainer<T> : ITraitContainer`（反射实例化的泛型容器 + 接口多态）；`Cache<T,U>` / `ConcurrentCache<,>`（泛型记忆化容器，FieldLoader/Sync 哈希缓存）。

**汇编路线必须复刻什么**：实例化形状有限（容器类约几十种、`TraitInfo<T>` 每 trait 一份）——按「每实例化一份手写特化」处理可行；虚分派与泛型实例化需正交设计（vtable + 特化体一一对应），不必实现运行时泛型。

风险：🟡（特化数量大但机械；漏一个接口形状会导致 trait 装配缺项 → 间接进 SyncHash）。

## 6. GC / 对象生命周期

**量化**：`new` 表达式约 7910 处（1198 文件）；`class` 声明 2544 vs `struct` 95（确定性模拟的基元 WPos/CPos/CVec/WVec/WDist/WAngle/WRot/int2 全是值类型）；finalizer 仅 **6 个**（`Sdl2HardwareCursor`、`Sdl2GraphicsContext`、`OpenAlSoundEngine`、`OpenAlSoundSource`、`ObjectCreator`、`DisposableAction`）；`: IDisposable` 64 处；`using` 语句 258 处；显式 `GC.*` 22 处（含 Server 入口 `GC.Collect()`）；`stackalloc` 56 处（FieldLoader 解析的热优化）。

**分配面判断**：大分配集中在加载期（地图 tile、素材、规则树）；帧内分配主要是 effects/projectiles/订单缓冲。上游已做过去 GC 化优化：TraitDictionary 手写枚举器避免 yield、`CollectionsMarshal.AsSpan(List)` 直取 span、装箱缓存（`BoxedTrue/BoxedFalse/BoxedInts`）、int2/float 装箱规避。对象图无循环引用（Actor 独占 traits，World 独占 Actor 表），`Actor` 引用语义靠 `ActorID`（uint）+ 字典而非指针恒等。

**汇编路线必须复刻什么**：**不需要复制式 GC**——需要 arena/pool 分配器 + 明确所有权；`IDisposable/using/finalizer` 对应确定性析构调用点；对象标识一律改走 ID（上游本就如此）。风险点是任何依赖引用相等（ReferenceEquals）做去重的地方（检索到的 `ConditionalWeakTable` 无使用；低风险）。

风险：🟡（分配时机/地址不影响可观测面；遗漏 Dispose 语义会在平台层泄漏，判据外 ⚪）。

## 7. 格式化 / 解析 / CultureInfo

**量化**：`$"...`" 插值 1216 处（375 文件，日志/UI 为主）；`string.Format` 9；带格式串 `ToString("...")` 16；`ToString(CultureInfo...)` 23；**`Exts.ToStringInvariant` 封装 56 处**；`int.Parse/TryParse` 40、`float` 5、`double` 1（几乎全走 `Exts.TryParse*Invariant`，用 `NumberStyles + NumberFormatInfo.InvariantInfo`）；`CultureInfo.InvariantCulture` 48 处 vs `CurrentCulture` 仅 **7 处**（dedicated server 时间戳、UI 显示）；`NumberStyles/NumberFormatInfo` 120。

**进可观测面的点**：

- **数值解析接受集**（🔴 字节恒等的输入侧）：规则/地图 yaml 全部经 `FieldLoader` → `Exts.TryParseInt32Invariant`（`NumberStyles.Integer`）与 `TryParseFloatOrPercentInvariant`（`NumberStyles.Float` + `%` 后缀乘 0.01f）。C++ 解析器必须逐字符等价复刻 .NET Invariant 解析的接受集（前导/尾随空白、正负号、指数、千分位逗号在 Float 集合内、`NaN/Infinity` 等特例）——多接受或拒绝一个字符串都会让规则加载产生不同成败路径。
- **float 最短往返格式化**（🔴 字节恒等的输出侧）：`FieldSaver.FormatValue` → `TypeConverter.ConvertToInvariantString` / `v.ToString()`（.NET 默认 = 最短往返表示）；`Vector2/3` 插值同理。规则 dump、地图 yaml 回写、`GameInformation`/`ReplayMetadata` 序列化全走此路径。C++ 需 `std::to_chars` shortest round-trip 并**验证与 .NET 输出逐字节一致**（两者算法语义同源，但需对拍确认边界值）。
- **DateTime 固定格式**（🔴 回放元数据）：`"yyyy-MM-dd HH-mm-ss"` Invariant（FieldLoader.ParseDateTime / FieldSaver），`ReplayMetadata.Write` 进 .orarep 尾部。
- `decimal` 10 处：FieldLoader 解析域 + 经济数值（`ParseDecimal` 同走 Invariant + % 缩放）。
- 判断：**文化敏感面没有进可观测面**（CurrentCulture 7 处都在判据外），这是上游替我们做好的隔离。

**汇编路线必须复刻什么**：int/float/decimal 的 Invariant 解析接受集、float 最短往返输出、`%` 百分比缩放（×0.01f/×0.01m）、DateTime 固定格式。这是字节恒等判据下**精度风险最高**的一节（IEEE-754 位级 + 文本表示双重恒等）。

风险：🔴。

## 8. 哈希与字典迭代序

**量化**：`GetHashCode` 覆写 26 处（24 文件）——**全部是确定性整数组合**（`WPos: X^Y^Z`、`CPos: Bits`、`int2: X^Y`、`Polygon: ((h<<5)+h)^v` 等），无一是 string 哈希；`string.GetHashCode` 显式调用 **0 处**（.NET 每进程随机化种子因此**不进可观测面**——关键好消息）；`StringComparer.Ordinal(IgnoreCase)` 15；`IEqualityComparer<>` 自定义 3；`.Keys/.Values` 303（137 文件）。

**迭代序进可观测面的证据链**：

- **SyncHash**（🔴，`World.cs:482-512`）：`ret += n++ * (1+ActorID) * hash` —— **迭代顺序号 n 直接做乘子**，任何容器序变化都会改 SyncHash。逐项核对：`World.actors = SortedDictionary<uint, Actor>`（按 ActorID 有序——上游刻意选定）；`ActorsHavingTrait<ISync>()` 走 TraitContainer 的**按 ActorID 有序平行数组**（二分插入维护）；`World.Players = Player[]`（创建时定序）；`SharedRandom.Last`。全部可复刻。
- **字节恒等 / yaml 回写**（🔴）：`FieldSaver.Save` 对字典字段 `foreach (kvp in dict)` 按插入序输出节点；`Rules.Actors = ActorInfoDictionary`（内部普通 `Dictionary`，构造自 yaml 节点序 → 插入序）。.NET `Dictionary` 迭代序 ≈ 插入序（删除/重插可打破，需按实际操作序列钉死）。
- `OrderBy` 产出的序列序见 §3（🔴）。

**汇编路线必须复刻什么**：「插入序字典」数据结构（vector 保序 + 哈希索引）替代 .NET Dictionary，并把上面四条迭代序来源（ActorID 有序、容器有序数组、数组序、插入序）逐一钉死为规范。`string.GetHashCode` 不可用也不需要——凡进判据的哈希全是手写公式（§4 Sync.Hash + §本节覆写）。

风险：🔴（迭代序是 SyncHash 与 yaml 字节恒等的共同底座）。

## 9. unsafe 与 P/Invoke

**unsafe：12 个文件真实使用**（`rg -l '\bunsafe\b'` 命中 15 个，其中 `ScriptContext.cs`、`D2kActorPreviewPlaceBuildingPreview.cs`、`ExtractLuaDocsCommand.cs` 只是字符串里含 "unsafe"）：

| 文件 | 用途 |
|---|---|
| OpenRA.Mods.Cnc/FileFormats/BlowfishKeyProvider.cs | 指针算术实现大数运算（CnC mix 密钥推导） |
| OpenRA.Mods.Cnc/FileFormats/LZOCompression.cs | LZO1x 解压，纯指针流操作 |
| OpenRA.Mods.D2k/SpriteLoaders/R8Loader.cs | R8 图像格式像素解码 |
| OpenRA.Game/Graphics/CursorManager.cs | 光标位图 byte[]→int[] 重解释拷贝 |
| OpenRA.Mods.Common/Widgets/RadarWidget.cs | 雷达小地图逐像素填充 |
| OpenRA.Mods.Common/Widgets/ColorMixerWidget.cs | HSV 调色板生成 |
| OpenRA.Mods.Common/Widgets/HueSliderWidget.cs | 色相条像素生成 |
| OpenRA.Platforms.Default/OpenGL.cs | GL 字符串指针 `(sbyte*)`、扩展函数表 |
| OpenRA.Platforms.Default/Shader.cs | 着色器二进制上传 |
| OpenRA.Platforms.Default/Texture.cs | 纹理像素上传 `fixed` |
| OpenRA.Platforms.Default/FreeTypeFont.cs | FreeType 字形位图 |
| OpenRA.Platforms.Default/Sdl2Input.cs | `Marshal.Copy` SDL 文本输入事件 |

（工单预估 11 个；差异为口径，本表为 `unsafe` 块/修饰符精确命中。）

**P/Invoke：源码内 `[DllImport]` 9 处**（工单预估 11；差异为口径）：

- `FreeTypeFont.cs`：freetype6 ×5（`FT_Init_FreeType`、`FT_New_Memory_Face`、`FT_Set_Char_Size`、`FT_Load_Char`、`FT_Done_Face`，Cdecl）。
- `Sdl2PlatformWindow.cs`：`user32.dll` ×1（`GetWindowRect`）+ `libX11` ×3（`XInitThreads`、`XOpenDisplay`、`XCloseDisplay`，Linux 侧）。
- 另两类非 DllImport 的原生绑定：`OpenGL.cs:668` 经 `SDL_GL_GetProcAddress` + `Marshal.GetDelegateForFunctionPointer` 建 GL 函数表；SDL2 全套经 NuGet 包 SDL2#（程序集外），Lua 经 OpenRA-Eluant（程序集外）。

**汇编路线必须复刻什么**：无需复刻任何运行时——unsafe/P-Invoke 在 C++/汇编侧就是原生域本身；只需照签名直连 freetype6/SDL2/OpenGL 与系统调用（Windows 优先项下 libX11 三处不适用）。

风险：⚪（LZO/Blowfish 属于格式语义，随 §7 的解析面一并管理）。

## 10. 其他隐性运行时依赖

- **yield 迭代器状态机**：`yield return` 546 处（254 文件）。热路径已被上游手写枚举器替换（TraitDictionary 的 PERF 注释明言 "using yield is slower"）。**汇编复刻**：枚举器协议（一个状态机约定或编译期展开为循环）——数量大但机械。🟡
- **async/await**：仅 4 文件 5 方法，其中 `Nat.cs` 是 `async void`（fire-and-forget UPnP 发现）——全在联网/平台层，**不进确定性模拟**。`Task` 25、`Thread` 26、`lock` 104、`ConcurrentCache/ConcurrentDictionary` 25（FieldLoader/Sync 缓存为并发安全）。**汇编复刻**：同步原语 + 加载期并行；模拟单线程无需异步运行时。⚪
- **DateTime 67 / TimeSpan 28**：绝大多数是日志/计时/元数据；进判据的只有 §7 的回放元数据固定格式。⚪/🔴（随 §7）
- **Regex 22 处（8 文件）**：地图名过滤（`Map.cs`）、fluent lint、命令行参数（`Arguments.cs`）、ShpRemastered 加载器、Steam 路径解析等。多为工具/平台路径；`ShpRemasteredLoader` 与 `MapCommand`（utility）间接靠字节恒等。**汇编复刻**：8 处正则可手写为等价匹配器（模式简单），避免携带 Regex 引擎。🟡
- **枚举反射**：`Enum.TryParse`（FieldLoader.ParseEnum，大小写不敏感 + 未定义数值也接受——必须复刻的接受集 🔴）、`Enum.GetValues<T>`（启动期）。🟡
- **.NET 8+ 优化容器**：`FrozenDictionary/FrozenSet`（FieldLoader 分发表、Sync.CustomHashFunctions、Ruleset 容器）、`ImmutableArray<>`（MiniYaml 节点、规则字段）、`CollectionsMarshal.AsSpan`（TraitDictionary 热路径）。语义上等价于只读字典/数组，C++ 用普通容器 + 冻结标志承载。⚪
- **程序集加载器语义**：`Support/AssemblyLoader`（自实现 ALC）+ `Microsoft.Extensions.DependencyModel` 包（枚举程序集清单）——mod DLL 热插拔机制。C++ 侧对应「mod 共享库 dlopen + 注册表导出符号」替代。⚪
- **第三方运行时库**（csproj）：OpenRA-Eluant（Lua）、Linguini.Bundle（Fluent 本地化——**翻译字符串进 UI/日志，不进 SyncHash**）、SharpZipLib（zip 包读取，进地图/素材字节恒等链）、Mono.NAT、OpenRA-FuzzyLogicLibrary（AI 模糊逻辑）、MP3Sharp/NVorbis/TagLibSharp/Pfim（音视频/图像解码）、DiscordRichPresence、rix0rrr.BeaconLib、NuGet.CommandLine。**汇编复刻**：算法库逐个替换为 C++ 等价实现（SharpZipLib=zip 格式、Eluant=Lua 虚拟机（Phase 8 已列）、Linguini=Fluent 求值器）。🟡（解码器输出逐字节恒等即通过）
- **BinaryWriter 订单序列化**（`Order.cs:345` / `OrderIO.cs`）：7-bit 编码字符串 + little-endian 定宽数值，`SerializeSync((Frame, SyncHash, DefeatState))` 把每帧 SyncHash 打进订单流（🔴 回放与网络可观测面）——C++ 直接按位复刻。🔴

---

## 总表

| # | 依赖 | 规模（文件/调用点） | 进可观测面 | 汇编路线必须复刻什么（一句） | 风险 |
|---|---|---|---|---|---|
| 1 | 异常 | 318/986 throw；116/249 catch；6 自定义异常 | 错误路径取舍（SmudgeLayer 空 catch）；RunUnsynced 哨兵 | 错误路径的外部可见效应（消息/取舍/退出），非 CLR EH 机制 | 🟡 |
| 2 | 委托/事件 | 1044/6974 lambda；28 event；1215 Func/Action | 否（event 全在 UI/生命周期层） | 函数指针 + 接口数组缓存（上游已消解热路径多播） | ⚪ |
| 3 | LINQ | 630 文件；万级调用 | OrderBy 稳定排序进 AI/瞄准决策 | 语义兼容集合算法库（稳定多键排序、首见序 distinct/group…） | 🔴 |
| 4 | 反射/特性 | 98/334 typeof；692/3383 特性；3 处运行时代码生成 | Sync.Hash 的 IL Emit 哈希公式与字段序；FieldLoader 接受集 | 装配行为三段式（名→构造→字段绑定）用编译期注册表承载；哈希公式逐位复刻 | 🔴 |
| 5 | 泛型 | 41 泛型类；201 泛型方法；1218 IEnumerable | 间接（漏形状 → 装配缺项） | 每实例化一份手写特化 + vtable 正交 | 🟡 |
| 6 | GC | 7910 new；6 finalizer；258 using | 否（对象标识走 ActorID） | arena/pool + 所有权 + 确定性析构，无需 GC | 🟡 |
| 7 | 格式化/解析 | 1216 插值；48 Invariant vs 7 Current | 解析接受集（规则加载成败）；float 最短往返（yaml 输出）；DateTime 固定格式（回放元数据） | .NET Invariant 接受集逐字符复刻 + to_chars shortest 对拍 | 🔴 |
| 8 | 哈希/迭代序 | 26 GetHashCode 覆写；0 string.GetHashCode | SyncHash 的 n++ 乘子序；FieldSaver 字典插入序 | 插入序字典 + 四条迭代序来源钉死为规范 | 🔴 |
| 9 | unsafe/P-Invoke | 12 文件 unsafe；9 DllImport + GL/SDL 函数表 | 否 | 原生域直连（freetype6/SDL2/GL），零复刻 | ⚪ |
| 10 | 其他 | 546 yield；5 async；22 Regex；枚举/冻结容器/第三方库 | Enum.TryParse 接受集；BinaryWriter 订单位序；解码器输出 | 枚举器协议展开；同步原语；8 处正则手写；解码库 C++ 替身 | 🟡/🔴 |

## 五条最重要结论

1. **上游不需要 CLR，但需要一套「.NET 集合与数值语义兼容层」**。没有运行时类型生成进判据（Sync.Emit 的机制可换、公式必须留）、没有 string 哈希随机化、没有文化敏感格式化、没有 GC 语义进可观测面——四大「经典 .NET 确定性陷阱」上游全部天然或刻意避开。汇编路线真正要手工复刻的运行时，收敛为四件硬货：**插入序字典、稳定排序集合算法、.NET Invariant 数值解析/格式化接受集、编译期装配注册表**。
2. **SyncHash 的可复刻性已被上游自己保证**：`SortedDictionary<uint,Actor>` + 有序 trait 数组 + `Player[]` + 确定性手写哈希公式，全部是刻意设计（`n++*(1+ActorID)*hash` 的顺序敏感乘子反而把任何迭代序偏离放大为 desync，对拍时立刻暴露）。风险不在「算不准」，在「哪个容器在哪条路径上换序」——建议在替代模型设计中把「迭代序规范」单列一页。
3. **反射是深度依赖但不是运行时依赖**：反射的产物（按名构造 + yaml 字段绑定 + 接口/基类枚举）在编译期全部可知，可用注册表/宏生成等价替换；唯一必须逐位保留的是它参与产出的哈希公式、字段处理顺序与解析接受集。工时大头在 ~300 个 TraitInfo × 字段表的注册表生成，而非运行时机制。
4. **字节恒等的最高精度风险在数值文本化**：float 最短往返格式化与 `NumberStyles.Float/Integer` 接受集（含 `%` 缩放、千分位、指数、未定义枚举值）同时约束输入与输出两侧；建议黄金对拍第一批样本就覆盖 float/decimal 边界值（0.1、1e-7、-0、NaN、3.4028235E38、`50%` 等）。
5. **平台与异步面可以整体后置**：unsafe/P-Invoke（freetype6、SDL2、GL、LZO、Blowfish）在汇编路线下是原生域；async/await、事件、Regex、线程/锁全部落在联网/UI/工具层，不阻塞 M1 沙盒对局的确定性验证——与 PORTING_PLAN 的垂直切片顺序（Phase 5 才上确定性关卡）正交不冲突。

---

*统计口径备注：计数来自 rg 正则（`-g '*.cs'`，七程序集目录，排除 obj/），行数来自 `cat | wc -l`；「调用点」为匹配次数求和，含少量注释/字符串命中与 BCL 同名方法（已在节内注明），作数量级参照。精读文件：ObjectCreator.cs、FieldLoader.cs、FieldSaver.cs、Sync.cs、TraitDictionary.cs、Actor.cs（150-240）、World.cs（470-530）、SyncReport.cs、VariableExpression.cs、ReplayRecorder.cs、ActorInfoDictionary.cs、Exts.cs（524-575）、TypeDictionary.cs、WidgetLoader.cs、ScriptContext.cs、Server/Utility Program.cs。*
