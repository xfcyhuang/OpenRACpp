# UPSTREAM 溯源登记

代码文件不承载注记(编码规范:代码文件零注释);本文件是 `// UPSTREAM:` 标注的统一承载。
增量迁移:注记随各批改写对应文件时迁入本表并从代码删除;基线 commit 见 `UPSTREAM.baseline`;校验器 `tools/upstream_check.py`。

## src/game/game.hpp

- 上游:`OpenRA.Game/Game.cs @1f594e320` + `OpenRA.Game/Support/Program.cs L14-19`
- 批注:主循环骨架逐语义(RunTime 时钟/延迟动作队列/JoinInner·JoinLocal/InnerLogicTick 的双 TickTime 节拍/LogicTick 的连接态事件/RenderTick 的 prepare→world→UI→flip 全序/Loop 的逻辑-渲染双时间表与追帧截断/Run 的 finally 清理)。Phase 5-8 依赖面走 Deps/钩子注入:Renderer/Sound/CursorManager/ChromeProvider/WorldRenderer+Viewport/Ui、Settings.Graphics 值面、NetworkConnection 的连接态观测(Phase 7)、world.OrderGenerator(Phase 5)、PerfHistory/PerfSample/Benchmark/截图(Phase 5+)、TextNotificationsManager/UnitOrders.Clear(第十六批客户端接线:JoinInner 双 Clear + LobbyInfoChanged 事件转发)—— 不在场的编排点全部 no-op。第二十六批扩面:OpenWindow×2/LoadWidget 三转发(L148-176)+ TakeScreenshot 置旗(L691/696,RenderTick L756-759 消费)+ WidgetLoader 的 Game 持有(ModData.WidgetLoader 的 D105 族分层偏离)。

## src/game/game.cpp

- 上游:`OpenRA.Game/Game.cs @1f594e320`(game.hpp 的实现)

## src/ui/ui.hpp

- 上游:`OpenRA.Game/Widgets/Widget.cs @1f594e320 L24-194`(Ui 静态门面逐语义:窗口栈/焦点三指针/输入分发/ResetAll/Tick/Draw/Mediator 转发)
- 批注:C# static 走进程级单例;窗口栈条目在 widget 被 HideChild 摘出 Root 时接管所有权(可见栈顶由 Root 持有);WidgetLoader/Renderer.Resolution/Game.RunTime/ModData 走 Deps 注入;LastMousePos/LastMoveRunTime 写 gfx::Viewport 同名静态(Viewport.cs L134-135)。第二十六批扩面:WireModData(Game.InitializeMod 的重建接线,Ui.modData 静态直赋的注入等价)+ Deps 的 fn_game_load_widget(Game.LoadWidget 分层注入,缺省回落 Ui.LoadWidget)/fn_take_screenshot。

## src/ui/ui.cpp

- 上游:`OpenRA.Game/Widgets/Widget.cs @1f594e320 L24-194`(ui.hpp 的实现)

## src/ui/widget.hpp

- 上游:`OpenRA.Game/Widgets/Widget.cs @1f594e320`(Widget 树逐语义:Ui 门面/ChromeLogic/WidgetBounds/Widget/ContainerWidget/InputWidget/WidgetArgs/Mediator)
- 批注:C# static Ui 走进程级单例(Widget 的 Outer 族经其取焦点态);子 widget 所有权归父 unique_ptr,RemoveChild/HideChild 转移出树;Mediator 的 TypeDictionary 反射接口索引换 type_index 桶 + NotificationHandlerSet 混入;PostInit 的 ObjectCreator.CreateObject 换名字分派注册表(未知名 `Cannot locate type: {name}` 逐字);FieldLoader.LoadFieldOrProperty 换 LoadFieldOrProperty 虚方法链式回退;Initialize 的 Renderer.Resolution 与 ChromeMetrics 经 WidgetArgs(windowSize/chromeMetrics)注入。第二十六批扩面:WidgetArgValue 增 `game::PlayerProfile*`(profile)与 `std::function<int(int)>`(negotiateWidth)两槽。

## src/mods/create_map_players.cpp

- 上游:`OpenRA.Mods.Common/Traits/World/CreateMapPlayers.cs @1f594e320`(实现部分)+ 各批 mods trait 的注册表装配

## src/mods/load_widget_at_game_start.hpp / .cpp

- 上游:`OpenRA.Mods.Common/Traits/World/LoadWidgetAtGameStart.cs @1f594e320`(全文)
- 批注:TraitLocation World|EditorWorld;IWorldLoaded/INotifyGameLoading/INotifyGameLoaded 三接口;Game.LoadWidget 走 Ui::Deps.fn_game_load_widget 分层注入(缺省回落 Ui.LoadWidget);GameLoaded 的 optionsButton.OnClick 经快照后入 RunUnsynced(第二十二批 UAF 教训)。

## src/mods/hotkey_logics.hpp / .cpp

- 上游:`OpenRA.Mods.Common/Widgets/Logic/SingleHotkeyBaseLogic.cs L17-36` + `MusicHotkeyLogic.cs L19-83` + `ScreenshotHotkeyLogic.cs L18-31` + `MuteHotkeyLogic.cs L18-49`(全文)
- 批注:SingleHotkeyBaseLogic 的 namedKey/logicArgs/GLOBAL_KEYHANDLER 惯例;MusicHotkey 的四键分派与 StopMusic/PauseOrResumeMusic;Screenshot 的 Game.TakeScreenshot 走 Ui::Deps.fn_take_screenshot;Mute 的 Settings.Sound.Mute 翻转 + Fluent 双反馈行。

## src/mods/local_profile_logics.hpp / .cpp

- 上游:`OpenRA.Mods.Common/Widgets/Logic/LoadLocalPlayerProfileLogic.cs L16-30` + `LocalProfileLogic.cs L18-93` + `PlayerProfileBadgesLogic.cs L17-65`(全文)
- 批注:LoadLocalPlayerProfile 的 minimalProfile = CurrentWindow 非空;LocalProfile 的 LinkState 可见域/LINK_BUTTON 转发 SETTINGS_BUTTON/RefreshBadges 的 RunAfterTick + 订阅退订(C++ 侧加 alive 令牌防 RunAfterTick 悬垂);Badges 的模板摘除保活 + negotiateWidth 宽度方程 + TruncateText;game::PlayerBadge 与 ui::PlayerBadge 的字段桥(str_icon 三元 → opt_icon 三元)。

## tests/render_test.cpp

- 上游:`OpenRA.Mods.Common/LoadScreens/BlankLoadScreen.cs`(第二十六批节的 InitializeMod→LoadShellMap→MAINMENU 装配链;chrome 树 = mods/common/chrome/mainmenu.yaml + mods/common/chrome/playerprofile.yaml)

## src/core/action_queue.hpp

UPSTREAM: OpenRA.Game/Primitives/ActionQueue.cs @1f594e320(线程安全延迟动作
队列逐语义:时间升序稳定插入、锁内切片/锁外执行;C# BinarySearch+推进循环
的等值段末插入 = upper_bound)
Verbatim-semantics rewrite of the thread-safe delayed-action queue
(ascending-time stable insertion; the C# BinarySearch-then-advance
insertion lands on upper_bound).

## src/core/arena.hpp

UPSTREAM: NONE —— PORTING_PLAN §4.5 自有内存基础设施:C# GC 的分区替代,上游无对应物;NONE = C++ 侧自有设计,见 tools/upstream_check.py 登记形式
两个分配器对应 PORTING_PLAN §4.5 的两个区域:
  FrameArena —— 帧临时区(渲染收集列表/排序 key/每 tick 临时),线性 bump,
    帧末 reset 归零;只接受平凡可析构类型(reset 不调析构)。
  WorldArena —— 每局世界区(actor/trait/activity/effect),chunk 链 bump +
    析构记录(规范允许的 arena 例外:非平凡析构由登记表逆序调用);
    destroy() 支持帧末显式销毁(幂等),reset() 保留 chunk 复用,release()
    整体归还 OS。
对齐契约:所有 chunk 基址 64 对齐(AlignedBuffer),chunk 内相对偏移对齐到
请求的 2 的幂对齐后,绝对地址即满足该对齐(请求 ≤ 64,断言强制)。
单线程约定:sim 主线程与渲染线程各持实例,不做任何加锁(§4.8 并发模型);
同步路径禁 shared_ptr 的硬规则由"arena 裸引用"承载(§4.5)。
Two allocators for two PORTING_PLAN §4.5 regions:
  FrameArena — the frame-transient area (render collection lists / sort keys /
    per-tick temporaries): linear bump, reset to zero at frame end; only
    trivially destructible types are accepted (reset runs no destructors).
  WorldArena — the per-world area (actor/trait/activity/effect): chunk-chained
    bump allocation with destructor records (the guideline-sanctioned arena
    exception: non-trivial destructors are invoked in reverse registration
    order); destroy() supports idempotent frame-end teardown, reset() keeps
    chunks for reuse, release() returns everything to the OS.
Alignment contract: every chunk base is 64-aligned (AlignedBuffer), so an
in-chunk relative offset aligned to a requested power-of-two alignment yields
an absolutely aligned address (requests are capped at 64 by assertion).
Single-threaded by convention: the sim thread and the render thread each own
their instances, with no locking whatsoever (§4.8 concurrency model); the
"no shared_ptr on the synced path" hard rule is carried by bare arena
references (§4.5).

## src/core/arguments.hpp

UPSTREAM: OpenRA.Game/Support/Arguments.cs @1f594e320 L17-36

## src/core/bitset.hpp

UPSTREAM: OpenRA.Game/Primitives/BitSet.cs @1f594e320 L20-173(全类型逐语义重写)
         Full-type statement-by-statement rewrite of upstream BitSet.cs.
已登记偏离(docs/COVERAGE.md)/ Registered deviations (docs/COVERAGE.md):
 1. 位图用 uint64_t 承载(C# BigInteger 无界);每标签 64 个不同字符串即 abort
    (实际 mods 中 TargetableType 约 20 种,不可达),越界即程序错误
    The bit index is carried in uint64_t (C# uses unbounded BigInteger);
    the 65th distinct string per tag aborts (unreachable in practice —
    TargetableType tops out around 20 entries).
 2. 线程模型:分配器按 magic-static 初始化,加载为单线程(C# 侧加锁仅为
    防御性;引擎规则加载发生在主线程)
    Threading: allocators initialize via magic statics; rule loading is
    single-threaded (the C# lock is defensive; engine rule loading happens
    on the main thread).

语义要点 / Semantic notes:
 - 位按"字符串首次出现序"分配(1,2,4,8,…),GetStrings 按分配序枚举——
   这决定了 BitSet::ToString() 的输出顺序,是对拍兼容面
   Bits are allocated in first-appearance order (1,2,4,8,…), and GetStrings
   enumerates in allocation order — this defines BitSet::ToString() output,
   which is part of the comparison surface.
 - 每个 Tag 类型独立命名空间(上游 BitSetAllocator<T> 的静态状态)
   Each Tag type is an independent namespace (the static state of the
   upstream BitSetAllocator<T>).

## src/core/cached_transform.hpp

UPSTREAM: OpenRA.Game/Primitives/CachedTransform.cs @1f594e320 L15-41(全文)
+ PredictedCachedTransform.cs L15-45(全文)。
输入相等(Equals)则回放缓存输出;T 的空值对判 = monostate 形态由调用方
保证(本仓消费面的 T 均为值语义)。C# Func → std::function。
The whole of CachedTransform.cs L15-41 + PredictedCachedTransform.cs
L15-45. An equal input (Equals) replays the cached output; T's null-pair
test arrives as value semantics (every T consumed in this repo is a value
type). C#'s Func becomes std::function.

## src/core/cell_pos.hpp

UPSTREAM: OpenRA.Game/Map/MapGrid.cs @1f594e320 L20(地图网格类型枚举)
         OpenRA.Game/MPos.cs @1f594e320 L17-93(MPos/PPos)
         OpenRA.Game/CPos.cs @1f594e320 L19-146(除 Lua 脚本绑定接口)
单元格坐标族:CPos(单元格位,12/12/8 位打包)、MPos(地图位)、PPos(投影位)。
Cell coordinate family: CPos (cell position, 12/12/8-bit packed), MPos (map position), PPos (projected position).
CPos.ToMPos 与 MPos.ToCPos 互相依赖,故三类型合置于本文件并按依赖序定义。
CPos.ToMPos and MPos.ToCPos are mutually dependent, so all three types live in this file, defined in dependency order.
字段/方法名保留 C# 原名以保持审计对照;C# Lua 绑定不移植(Phase 8)。
Field/method names keep the C# originals for audit cross-reference; C# Lua bindings are not ported (Phase 8).

## src/core/color.hpp

UPSTREAM: OpenRA.Game/Primitives/Color.cs @1f594e320 L20-232,374(值类型核心 + TryParse/ToString
         + L84-160 的 HSV/线性 gamma 转换,随 Phase 4 第十三批移植)
         Value-type core + TryParse/ToString + the L84-160 HSV/linear-gamma
         conversions (ported with the Phase 4 thirteenth batch).

语义要点 / Semantic notes:
 - TryParse:Trim 后长度必须恰为 6(RRGGBB)或 8(RRGGBBAA),十六进制大小写均可
   TryParse: after Trim the length must be exactly 6 (RRGGBB) or 8
   (RRGGBBAA); hex digits are case-insensitive.
 - ToString:A==255 输出 RRGGBB,否则 RRGGBBAA,大写十六进制
   ToString: RRGGBB when A==255, otherwise RRGGBBAA, uppercase hex.

## src/core/cvec.hpp

UPSTREAM: OpenRA.Game/CVec.cs @1f594e320 L20-146(除 Lua 脚本绑定接口)
单元格向量(细胞坐标差)。字段/方法名保留 C# 原名以保持审计对照。
Cell vector (difference of cell coordinates). Field/method names keep the C# originals for audit cross-reference.
Lua Scripting 接口(#region Scripting interface)属 Phase 8 脚本绑定层,此处不移植。
The Lua Scripting interface (#region Scripting interface) belongs to the Phase 8 script-binding layer and is not ported here.

## src/core/exts_math.hpp

UPSTREAM: OpenRA.Game/Exts.cs @1f594e320 L282-373(ISqrt 家族与 √2 近似)
整数平方根:逐位算法,无浮点;三种舍入模式与上游逐一对应。
负数输入上游抛 InvalidOperationException,此处以契约断言承接(ORA_PRE 过渡形态)。
Integer square root: bit-by-bit algorithm, no floating point; the three rounding modes match upstream one-to-one.
Upstream throws InvalidOperationException on negative input; here it is handled by a contract assertion (ORA_PRE transitional form).

## src/core/int2.hpp

UPSTREAM: OpenRA.Game/Primitives/int2.cs @1f594e320 L20-94(全类型)
二维整数向量。字段/方法名保留 C# 原名(X/Y/Sign/Abs/…)以保持逐行审计对照,
2D integer vector. Field/method names keep the C# originals (X/Y/Sign/Abs/...) for line-by-line audit cross-reference;
因此公有字段不适用 cpp26.md 的变量前缀规范;局部变量仍按规范带类型前缀。
therefore public fields are exempt from the cpp26.md variable prefix convention; local variables still carry type prefixes per the convention.
上游 FromVector/ToVector2(System.Numerics 浮点)为渲染域成员,随 Phase 4
第十三批移植(CursorManager/ChromeProvider 消费)。
Upstream's FromVector/ToVector2 (System.Numerics floats) are render-domain
members, ported with the Phase 4 thirteenth batch (consumed by
CursorManager/ChromeProvider).
Clamp(Rectangle) 因与 Rectangle 相互依赖,实现在 rectangle.hpp。
Clamp(Rectangle) is mutually dependent with Rectangle, so it is implemented in rectangle.hpp.

## src/core/int32_matrix4x4.hpp

UPSTREAM: OpenRA.Game/Primitives/Int32Matrix4x4.cs @1f594e320 L16-66(全类型)
32 位整数 4×4 矩阵,WRot 的旋转矩阵表示(定点数,1024 == 1.0)。
字段名保留 C# 原名(M11..M44)以保持审计对照;ToString 上游为字符串拼接,移植为 format。
32-bit integer 4x4 matrix, the rotation-matrix representation of WRot (fixed-point, 1024 == 1.0).
Field names keep the C# originals (M11..M44) for audit cross-reference; upstream ToString is string
concatenation, ported as format.

## src/core/long_bitset.hpp

UPSTREAM: OpenRA.Game/Primitives/LongBitSet.cs @1f594e320(逐语义重写)
         Verbatim-semantics rewrite.

机制对照 / Mechanism mapping:
 - LongBitSetAllocator<T> 的 Cache<string,long> + nextBits 位分配器:
   键序非契约(仅存在性/位查询消费)→ std::map 插入序承载;溢出
   OverflowException "Trying to allocate bit index outside of index 64."
   同文本;Reset() 清表回卷(World 构造的 LongBitSet<PlayerBitMask>.Reset()
   锚点 —— C++ 进程级单例即分配域,World 复用同表;上游每世界复位由
   位身份仅在世界内消费保证等价 —— COVERAGE 登记)
   The LongBitSetAllocator<T> Cache<string,long> + nextBits allocator:
   the key order is non-contractual (only membership/bit queries are
   consumed) → a std::map; the overflow keeps the
   "Trying to allocate bit index outside of index 64." text; Reset()
   clears and rewinds (the World ctor's LongBitSet<PlayerBitMask>.Reset()
   anchor — the C++ process singleton is the allocation domain, world
   reuse of the same table is equivalent because bit identities are only
   consumed within one world — registered in COVERAGE).
 - LongBitSet<T> 的 readonly struct 位集合族:逐运算照抄(Union/Intersect/
   Except/SymmetricExcept/Overlaps/IsSubsetOf/IsSupersetOf/SetEquals/
   IsProperSubsetOf/IsProperSupersetOf/IsEmpty/Contains/ToString)
   The readonly-struct bit-set family of LongBitSet<T>: every operation
   copied verbatim.

## src/core/mersenne_twister.hpp

UPSTREAM: OpenRA.Game/Support/MersenneTwister.cs @1f594e320 L17-153(全类型)
MT19937(OpenRA 变体)——仿真确定性的根基:任何数值/顺序变化都会导致 desync。
Last/TotalCount 为上游公有字段,参与 SyncHash,字段名保留原名。
无参构造(Environment.TickCount 种子)不移植:同步路径必须显式播种。
ShuffleInPlace 为泛型方法,保留模板形式。
MT19937 (OpenRA variant) -- the foundation of simulation determinism: any change in values or ordering causes a desync.
Last/TotalCount are upstream public fields that take part in SyncHash; field names keep the originals.
The parameterless constructor (Environment.TickCount seed) is not ported: synchronized paths must seed explicitly.
ShuffleInPlace is a generic method, kept in template form.

## src/core/percent_modifiers.hpp

UPSTREAM: OpenRA.Mods.Common/Util.cs @1f594e320 L203-211(ApplyPercentageModifiers)
百分比修正链的精确十进制语义:上游以 C# decimal(128 位软十进制)逐项乘 p/100m,
最后 (int) 向零截断。100=2²×5² 保证 p/100 恒为有限小数;实践值域(number ≤ 10 位、
百分比 ≤ 4 位、链长 ≤ 4,见 Armament/Health/Mobile 等 22 个调用点)内 decimal 乘法
永不触发 28 位有效数字舍入,故该函数等价于"精确有理数 number·Πpᵢ/100^k + 向零截断"。
此处以 __int128 累积分母 100^k 的分子精确复刻;理论极端值域(分子超 __int128、
链长 > 18、商超 int32)上游抛 OverflowException,此处以契约断言承接,实践不可达。
调用面差异:参数由 IEnumerable<int>(上游 LINQ 链 + 每调用 ToArray 分配)改为
span 直传 —— 上游 22 处调用点(Armament.MaxRange/MovementSpeedForCell/伤害/
视野等)全部同步路径,见 docs/OPTIMIZATION_TRACKER.md OPT-A1。
Exact decimal semantics of the percentage-modifier chain: upstream multiplies
p/100m item by item in C# decimal (128-bit soft decimal) and finally truncates
with (int) towards zero. Since 100 = 2²×5², p/100 is always a finite decimal,
and within practical ranges (number ≤ 10 digits, percentages ≤ 4 digits,
chain length ≤ 4 — see the 22 call sites in Armament/Health/Mobile etc.) the
decimal products never hit the 28-significant-digit rounding, so the function
equals "exact rational number·Πpᵢ/100^k truncated towards zero". Here an
__int128 numerator over the fixed denominator 100^k reproduces it exactly;
the theoretical extreme range (numerator beyond __int128, chain > 18, quotient
beyond int32 — upstream throws OverflowException) is caught by contract
assertions and is unreachable in practice. The parameter is a plain span
instead of IEnumerable<int> (upstream: LINQ chains + per-call ToArray
allocations); all 22 call sites are on the synced path. See
docs/OPTIMIZATION_TRACKER.md OPT-A1.

## src/core/polygon.hpp

UPSTREAM: OpenRA.Game/Primitives/Polygon.cs @1f594e320 L18-119(逐语义重写)+
         OpenRA.Game/Exts.cs L~458-496(PolygonContains/LinesIntersect/
         WindingDirectionTest)
         Verbatim-semantics rewrite.

## src/core/priority_queue.hpp

UPSTREAM: OpenRA.Game/Primitives/PriorityQueue.cs @1f594e320(逐语义重写)
         Verbatim-semantics rewrite.

机制对照 / Mechanism mapping:
 - "层级加倍"二叉最小堆原样保留(而非改写为平坦堆):相同优先级条目的
   弹出次序属于可观测行为(寻路平局路径的选择进入 replay SyncHash,
   docs/OPTIMIZATION_TRACKER.md OPT-A2/C4 的警示项)—— 对拍优先于
   局部重写;TComparer 结构比较器 → 模板比较器类型(可内联,同零虚化)
   The "levels-doubled" binary min-heap is kept verbatim (rather than
   flattened): the pop order of equal-priority entries is observable
   (tied path choices enter the replay SyncHash — the OPT-A2/C4 warning
   in the optimization tracker) — the differential gate outranks a local
   rewrite; the struct comparer becomes a template comparator type
   (inlinable, equally devirtualized).
 - TComparer 的三态 Compare 协议(IComparer<T>.Compare 的 -1/0/1)以
   静态成员承载(Comparer::Compare(x, y);CostComparer 同形)—— 上游
   "实例 comparer" 为无状态结构,C++ 静态化即等价
   The TComparer three-way Compare protocol (IComparer<T>.Compare's
   -1/0/1) is carried as a static member (Comparer::Compare(x, y);
   CostComparer shares the shape) — upstream's comparer instance is a
   stateless struct, so the C++ static form is equivalent.
 - Add 的扩容语义:Array.Resize → reserve/resize(vector 同倍增界);空堆
   Peek/Pop 的 InvalidOperationException 同文本
   Add's growth: Array.Resize → vector reserve/resize (the same doubling
   bound); empty Peek/Pop keep the InvalidOperationException text.

## src/core/rectangle.hpp

UPSTREAM: OpenRA.Game/Primitives/Rectangle.cs @1f594e320 L17-129(除 Size 依赖部分)
轴对齐整数矩形。字段/方法名保留 C# 原名以保持审计对照。
Axis-aligned integer rectangle. Field/method names keep the C# originals for audit cross-reference.
未移植:Location/Size 属性与 int2,Size 构造(依赖 Size 类型,后续图形阶段补);
Not ported: Location/Size properties and the int2,Size constructor (depend on the Size type, to be added in the later graphics stage);
       Clamp(Vector2) 随第十四批 Viewport 落地(浮点,渲染域)。
       Clamp(Vector2) lands with the fourteenth batch's Viewport (float, rendering domain).
       int2::Clamp(Rectangle) 的定义在本文件尾部。
       int2::Clamp(Rectangle) is defined at the end of this file.

## src/core/size.hpp

UPSTREAM: OpenRA.Game/Primitives/Size.cs @1f594e320 L17-58(值类型逐语义;
ToInt2/LatticeToInt2/ToRectangle 等图形面消费点暂无 —— 随消费批扩展)
UPSTREAM: OpenRA.Game/Primitives/Size.cs @1f594e320 L17-58 (the value-type
semantics; the graphics-side consumers ToInt2/LatticeToInt2/ToRectangle
have no call sites yet — extended with their consuming batches).

## src/core/text.hpp

UPSTREAM: OpenRA.Game/MiniYaml.cs @1f594e320 L295,L299(.Trim() 调用点的 BCL 语义支撑)
复刻 .NET 的 char.IsWhiteSpace / MemoryExtensions.Trim:MiniYaml/FieldLoader 的
Replicates .NET char.IsWhiteSpace / MemoryExtensions.Trim: the key/value trimming of MiniYaml/FieldLoader
键值裁剪依赖该语义(Unicode 空白集含 NBSP/U+1680/U+2000-200A/U+3000 等),
depends on this semantics (the Unicode whitespace set includes NBSP/U+1680/U+2000-200A/U+3000 etc.),
字节级处理会在多字节空白上与 C# 产生偏差,故按 UTF-8 码点判定。
byte-level handling would diverge from C# on multi-byte whitespace, so the check is done per UTF-8 code point.
无效 UTF-8 序列按"每字节一个 U+FFFD"处理(mods 数据为合法 UTF-8,仅为兜底;
Invalid UTF-8 sequences are handled as "one U+FFFD per byte" (mods data is valid UTF-8, this is only a fallback;
C# StreamReader 的替换策略在连续无效字节上可能吞多字节,该角落记为已知偏离)。
C# StreamReader's replacement strategy may swallow multiple bytes on consecutive invalid bytes; that corner is recorded as a known deviation).

## src/core/vector_n.hpp

UPSTREAM: OpenRA.Game/FieldLoader.cs @1f594e320 L531-566(ParseVector2/ParseVector3
         的 X/Y/Z 数据语义与往返格式;System.Numerics.Vector2/3 的最小等价物)
         The X/Y/Z data semantics and round-trip format of
         ParseVector2/ParseVector3 (a minimal equivalent of
         System.Numerics.Vector2/3).

## src/core/wangle.hpp

UPSTREAM: OpenRA.Game/WAngle.cs @1f594e320 L24-269(除 Lua 脚本绑定接口)
一维定点角:1024 单位 = 360°。整数三角学:查表 + 无分支位技巧,
所有分支化写法(掩码取绝对值、瀑布二分)逐行照抄上游,任何"化简"都可能破坏对拍。
表数据照抄上游 L224-269(CosineTable: short[257],TanTable: int[257])。
RendererRadians/RendererDegrees 为浮点,仅渲染域。
1D fixed-point angle: 1024 units = 360°. Integer trigonometry: table lookup + branchless bit tricks;
every branch-shaped formulation (mask-based absolute value, waterfall binary search) is copied line by line
from upstream — any "simplification" could break differential testing.
Table data copied verbatim from upstream L224-269 (CosineTable: short[257], TanTable: int[257]).
RendererRadians/RendererDegrees are floating-point, rendering domain only.

## src/core/wdist.hpp

UPSTREAM: OpenRA.Game/WDist.cs @1f594e320 L24-115(除 Lua 脚本绑定接口)
一维世界距离:1024 单位 = 1 cell。字段/方法名保留 C# 原名以保持审计对照。
FromPDF 依赖 MersenneTwister,故本文件 include mersenne_twister.hpp。
ToString/TryParse 供数据加载链(FieldLoader)使用,数字转换走 std::from/to_chars。
1D world distance: 1024 units = 1 cell. Field/method names keep the original C# names for audit cross-reference.
FromPDF depends on MersenneTwister, so this file includes mersenne_twister.hpp.
ToString/TryParse serve the data-loading chain (FieldLoader); numeric conversion goes through std::from/to_chars.

## src/core/wpos.hpp

UPSTREAM: OpenRA.Game/WPos.cs @1f594e320 L20-79(除 Lua 脚本绑定接口)
         OpenRA.Game/WPos.cs @1f594e320 L143-168(IEnumerableExtensions.Average)
三维世界坐标(点,区别于 WVec 向量)。
LerpQuadratic 与 WVec 版的关键差异:offset 不先截断,(offset + Z) 在 decimal 域
相加后统一向零截断再 Clamp 到 int 范围——向零截断在跨零时不满足平移不变,
不能写成 trunc(offset)+z,必须通分后一次除法(WPos.cs L68-69)。
3D world coordinate (a point, as opposed to a WVec vector).
Key difference from the WVec version of LerpQuadratic: offset is not truncated first; (offset + Z) is
added in the decimal domain, then truncated toward zero once and clamped to the int range — truncation
toward zero is not translation-invariant across zero, so it cannot be written as trunc(offset)+z; the
terms must be brought to a common denominator for a single division (WPos.cs L68-69).

## src/core/wrot.hpp

UPSTREAM: OpenRA.Game/WRot.cs @1f594e320 L19-221(全类型)
三维世界旋转:公开欧拉角(Roll/Pitch/Yaw)+ 内部整数四元数(1024 == 1.0)。
上游 x/y/z/w 为 private;C++ 侧公开——黄金对拍与后续序列化需要读取四元数分量,
只读约束以命名与注释约定(修改四元数而不重导欧拉角会破坏不变量)。
WVec::Rotate(WRot) 的定义在本文件尾部(依赖 WRot 完整类型)。
3D world rotation: public Euler angles (Roll/Pitch/Yaw) + internal integer quaternion (1024 == 1.0).
Upstream x/y/z/w are private; exposed on the C++ side -- golden-differential testing and later
serialization need to read the quaternion components, so the read-only constraint is upheld by
naming and comment convention (mutating the quaternion without re-deriving the Euler angles breaks the invariant).
WVec::Rotate(WRot) is defined at the end of this file (requires the complete WRot type).

## src/core/wvec.hpp

UPSTREAM: OpenRA.Game/WVec.cs @1f594e320 L20-109(除 Lua 脚本绑定接口)
三维世界向量:全整数定点,1024 单位 = 1 cell。
LerpQuadratic 上游用 decimal(96 位十进制整数)防溢出;此处以 __int128 精确整数除法
复刻,商向零截断与 decimal 一致——理论差异仅当商的小数展开连续 12+ 个 9 并触发
decimal 第 28 位进位时出现(golden 对拍覆盖)。
Rotate(WRot) 因与 WRot 相互依赖,声明于此、定义在 wrot.hpp。
3D world vector: all-integer fixed-point, 1024 units = 1 cell.
Upstream LerpQuadratic guards against overflow with decimal (96-bit decimal integer); here it is
reproduced with exact __int128 integer division — the quotient truncates toward zero exactly as decimal
does; the only theoretical divergence appears when the quotient's fractional expansion has 12+ consecutive
9s that trigger a carry into decimal's 28th digit (covered by golden differential testing).
Rotate(WRot) and WRot are mutually dependent, so it is declared here and defined in wrot.hpp.

## src/fluent/fluent_bundle.cpp

UPSTREAM: OpenRA.Game/FluentBundle.cs L87-201 @1f594e320 —— 求值承载
(Linguini.Bundle.TryGetAttrMessage 的等价面;选择子 = 精确数字键 →
CLDR 类别 → 缺省变体;未解析变量按 Fluent 规范回退为其源文本
"{ $name }"并计错误)。
The evaluation carrier of OpenRA.Game/FluentBundle.cs L87-201 (the
equivalent face of Linguini.Bundle.TryGetAttrMessage; a selector = the
exact numeric key, then the CLDR category, then the default variant; an
unresolved variable falls back to its source text "{ $name }" per the
Fluent spec and counts as an error).

## src/fluent/fluent_bundle.hpp

UPSTREAM: OpenRA.Game/FluentBundle.cs L87-201 @1f594e320(逐语义重写;
Linguini.Bundle 的承载面 = 自研 Fluent 子集)。args 的 name/value 对校验
与异常吞并 → false + 错误报告(上游 ArgumentException/ArgumentNullException
由 catch(Exception) 转 Log.Write + false;错误路径文本仅在 debug 日志可
观察,不进入返回值面)。
A verbatim-semantics rewrite of OpenRA.Game/FluentBundle.cs L87-201 (the
carrier face of Linguini.Bundle = the self-written Fluent subset). The
args' name/value-pair validation and the exception swallowing become
false + an error report (upstream's ArgumentException/ArgumentNullException
reach the catch(Exception) → Log.Write + false; the error-path texts are
observable only in the debug log, never through the return face).

## src/fluent/fluent_provider.cpp

UPSTREAM: OpenRA.Game/FluentProvider.cs @1f594e320 —— 单例承载实现
(Initialize 的 map 文件表/base64 文本装载 = FluentBundle.cs L129-137 的
调用序;GetMessage 的 mod → map → 键回落序)。
The singleton-carrier implementation of OpenRA.Game/FluentProvider.cs
(Initialize's map file-list/base64-text loading = FluentBundle.cs's
L129-137 call order; GetMessage's mod → map → key fallback order).

## src/fluent/fluent_provider.hpp

UPSTREAM: OpenRA.Game/FluentProvider.cs @1f594e320(逐语义重写)。C#
static → 进程级单例(上游 Lock SyncObject → mutex);Initialize 的
`fileSystem is Map` 类型判 → 显式 map 定义节点出参(C++ 侧 Map 为具体
类且住 ora_map,本库不得上依 —— 由调用方(ModData.InitializeLoaders)
判型传入;偏离登记见 COVERAGE)。map 级 bundle 仅作 mod 级未中后的备
查(上游注释:禁止地图覆写 UI 级字符串键)。
A verbatim-semantics rewrite of OpenRA.Game/FluentProvider.cs. The C#
static becomes the process singleton (upstream's Lock SyncObject → a
mutex); Initialize's `fileSystem is Map` type test becomes an explicit
map-definitions out-parameter (the C++ Map is a concrete class living in
ora_map, which this library must not depend on — the caller
(ModData.InitializeLoaders) does the type test and passes the node; see
the COVERAGE registry). The map-level bundle only backs up mod-level
misses (upstream comment: maps must not overwrite UI string keys).

## src/fluent/fluent_syntax.cpp

UPSTREAM: OpenRA.Game/FluentBundle.cs @1f594e320 —— Fluent 资源解析的
自研承载(Linguini.Syntax 的等价面;空白规范 = 公共缩进扣除 + 行尾修剪
+ placeable 邻接修剪,Fluent 规范 Patterns 节;语料实测差异由
golden_fluent 对拍钉住)。
The self-written carrier of the Fluent resource parse (the Linguini.Syntax
equivalent face; the whitespace normalization = the common-indent deduction
+ the per-line trailing trim + the placeable-adjacent trim, the Fluent
spec's Patterns section; measured divergences pinned by golden_fluent).

## src/fluent/fluent_syntax.hpp

UPSTREAM: OpenRA.Game/FluentBundle.cs @1f594e320(经 Linguini.Bundle/
Linguini.Syntax 的 Fluent 0.x 子集自研承载 —— 上游依赖 Linguini 0.8.1 的
解析与求值,本侧按 Fluent 语法规范 + mods 语料实测面逐语义重写;语料
未用的面(term 引用/内建函数/消息引用)按解析错误落 junk,与上游
Linguini 报错误但继续装载后续条目的可观察行为对齐)。
Self-written carrier of the Fluent 0.x subset that stands in for the
upstream Linguini.Bundle/Linguini.Syntax dependency — parsed and evaluated
per the Fluent syntax specification and the mods corpus's measured face;
corpus-unused faces (term references/builtin functions/message references)
fall to junk as parse errors, matching the observable shape of upstream
Linguini logging errors but continuing with the remaining entries.

机制对照 / Mechanism mapping:
 - 资源(Linguini Syntax.Ast.Resource):条目表(消息/注释/junk 跳过);
   解析错误经 ErrorSink 报告(上游 ParseError → Log.Write("debug"))
   The resource (Linguini Syntax.Ast.Resource): the entry table
   (messages/comments/junk skipped); parse errors surface through the
   ErrorSink (upstream's ParseError → Log.Write("debug")).
 - 消息(Syntax.Ast.Message):主值 + 属性行(.name = …);选择子
   ({$var -> [key] … *[key] …})为其内嵌递归形态
   The message (Syntax.Ast.Message): the main value + attribute lines
   (.name = …); the selector ({$var -> [key] … *[key] …}) is its nested
   recursive form.
 - 空白规范:公共缩进扣除 + 逐行尾空白修剪 + placeable 邻接修剪
   (Fluent 规范 Patterns 节;Linguini 实测差异由 golden_fluent 对拍钉住)
   Whitespace normalization: the common-indent deduction + the per-line
   trailing trim + the placeable-adjacent trim (the Fluent spec's
   Patterns section; measured Linguini divergences pinned by the
   golden_fluent differential).

## src/formats/aud_reader.cpp

UPSTREAM: OpenRA.Mods.Cnc/FileFormats/AudReader.cs @1f594e320 + AudLoader.cs
的嗅探/try 面:逐句照抄;两解码流(ReadOnlyAdapterStream 的
BufferData 循环)物化为整段输出 —— Queue<byte> 逐字节 Enqueue 改
vector 追加,上游 return true(流尽)改循环退出,数据面逐字节等价。
[UPSTREAM continued] AudReader.cs + AudLoader.cs's sniff/try surface, copied
statement by statement; the two decode streams (the ReadOnlyAdapterStream
BufferData loops) materialize into one output — the byte-by-byte
Queue.Enqueue becomes a vector append and upstream's return true
(stream exhausted) becomes loop exit; the data plane stays byte-for-byte
equivalent.

## src/formats/aud_reader.hpp

UPSTREAM: OpenRA.Mods.Cnc/FileFormats/AudReader.cs @1f594e320 L20-205(全文)
+ OpenRA.Mods.Cnc/AudioLoaders/AudLoader.cs L20-73(IsAud 嗅探与
TryParseSound 的 catch 面)。
Westwood .aud 音频:12 字节头(sampleRate/dataSize/outputSize/flags/
format)+ 块序列(每块 8 字节头:compressedSize u16、outputSize u16、
0xdeaf 魔数)。两解码路径:IMA ADPCM(DecodeImaAdpcmSample 逐样点,
index/currentSample 跨块持久,outputSize 奇数时末半字节按
baseOffset < outputSize 截)与 Westwood 压缩(块级解码,
outputBuffer 只增复用 —— 解码短写时 OutputSize 尾段为上一块残留,
上游 EnsureArraySize 语义照抄)。
形态偏离(D74):上游 LoadSound 返回惰性工厂(Func<Stream> +
ReadOnlyAdapterStream 逐块 Queue<byte>);C++ 侧物化为整段 PCM
vector(消费端 Sound/OpenAL 批次本就整读)。字节序输出等价。
[UPSTREAM continued] the whole of AudReader.cs plus the AudLoader.cs L20-73 sniff
and TryParseSound catch surface. Westwood .aud audio: a 12-byte header
(sampleRate/dataSize/outputSize/flags/format) followed by chunks (each
with an 8-byte header: compressedSize u16, outputSize u16, the 0xdeaf
magic). Two decode paths: IMA ADPCM (DecodeImaAdpcmSample per sample
with index/currentSample persisting across chunks, an odd outputSize
truncating the final half-nibble via baseOffset < outputSize) and
Westwood compressed (chunk-level decoding with a grow-only output
buffer — a short decode leaves the tail of OutputSize holding the
previous chunk's residue, upstream's EnsureArraySize semantics kept).
Shape deviation (D74): upstream's LoadSound returns a lazy factory
(Func<Stream> + ReadOnlyAdapterStream enqueuing byte by byte per
chunk); the C++ side materializes the whole PCM into a vector (the
Sound/OpenAL consumer batch reads it whole anyway). The byte output is
equivalent.

## src/formats/blowfish.cpp

UPSTREAM: OpenRA.Mods.Cnc/FileFormats/Blowfish.cs @1f594e320 L14-409(全文逐语义)

## src/formats/blowfish.hpp

UPSTREAM: OpenRA.Mods.Cnc/FileFormats/Blowfish.cs @1f594e320 L14-409(全文逐语义)
标准 Blowfish(ECB,63 字节密钥上限任意长):18 项 P 盒 + 4×256 S 盒逐值
照搬;密钥扩展与 16 轮 Feistel 控制流逐行等价(x 交错标志、Encrypt/Decrypt
的 P[0]/P[17] 首尾异或、输出 a/b 交换)。RunCipher 的 SwapBytes 大端字序
装卸照抄 —— 数据数组按小端 uint 读入后整体字节交换,与上游跨平台一致。
消费方:mix 加密头(TS/RA2 格式)。
The standard Blowfish (ECB, any key length up to 63 bytes): the 18-entry
P array + the 4×256 S boxes copied value for value; the key schedule and
the 16-round Feistel control flow equivalent line for line (the x
alternating flag, the leading/trailing P[0]/P[17] XORs of Encrypt/Decrypt,
the output a/b swap). RunCipher's SwapBytes big-endian load/store is kept
verbatim — data words are read as little-endian uints then byte-swapped
whole, matching upstream across platforms. Consumer: the encrypted mix
header (the TS/RA2 format).

## src/formats/blowfish_key_provider.cpp

UPSTREAM: OpenRA.Mods.Cnc/FileFormats/BlowfishKeyProvider.cs @1f594e320 L19-491

## src/formats/blowfish_key_provider.hpp

UPSTREAM: OpenRA.Mods.Cnc/FileFormats/BlowfishKeyProvider.cs @1f594e320 L19-491
(全文逐语义;"direct C port" 的上游原话保持 —— 逐控制流照抄)
mix 加密头的 80 字节密钥块 → 56 字节 Blowfish 密钥:固定公钥(42 字节
DER,内嵌 base64)的 63..0 位 RSA 指数运算,小字节序 uint32 大数 + 16
位 limb 乘减 + 除法倒数的 Knuth D 商估计(GetMulWord)。所有指针算术
(ushort*/uint* 视图、esi/edi 游标、GetMulWord 的 wn-1/wn-2 回看)按
上游 unsafe 代码逐行对译;数组定长 64/130/4 与上游一致。
The mix encrypted header's 80-byte key block → the 56-byte Blowfish key:
an RSA exponentiation with the fixed 42-byte DER public key (base64
inlined) over little-endian uint32 bignums with 16-bit limb mul/sub and
Knuth-D quotient estimation via the reciprocal (GetMulWord). All pointer
arithmetic (the ushort*/uint* views, the esi/edi cursors, GetMulWord's
wn-1/wn-2 lookbehind) is translated line for line from the upstream
unsafe code; array lengths 64/130/4 match upstream.

## src/formats/crc32.hpp

UPSTREAM: OpenRA.Game/FileFormats/CRC32.cs @1f594e320 L19-133(全文)
查表 CRC32:初始与终结均折叠进 polynomial 参数(Calculate 的
poly=0xFFFFFFFF 默认形态即标准 CRC-32;static_assert 以标准测试向量
"123456789" → 0xCBF43926 锁表)。Update/Finish 链式形态供 PNG/mix
分段输入。消费方:mix 头、oracle 黄金哈希、Phase 5+ 的资产校验。
Table-driven CRC32: the initial state and the final XOR both fold into
the polynomial parameter (Calculate with the poly=0xFFFFFFFF default is
the standard CRC-32; a static_assert pins the table with the standard
test vector "123456789" → 0xCBF43926). The Update/Finish chaining form
serves segmented inputs for PNG/mix. Consumers: the mix header, the
oracle golden hashes, and Phase 5+ asset validation.

## src/formats/dds.cpp

UPSTREAM: NONE —— Pfim v0.11.3 逐语义移植(见 dds.hpp 头注的文件清单);
上游 OpenRA 消费面 DdsLoader.cs @1f594e320。RGB565 插值为 float 逐字
(Lerp + (byte)(x+0.5f) 截断);mip 尺寸的 Width/2^i 走 double 截断;
CompressedDds 统一走 InMemoryDecode 控制流(D65);D66 = BC4/5/DX10
系显式拒绝。
UPSTREAM: NONE — the verbatim-semantics port of Pfim v0.11.3 (see
dds.hpp's header note for the file list); the OpenRA consumer face is
DdsLoader.cs @1f594e320. The RGB565 interpolation keeps the float math
verbatim (Lerp + the (byte)(x+0.5f) truncation); mip dimensions use the
double-truncating Width/2^i; CompressedDds unifies on the InMemoryDecode
control flow (D65); D66 = the explicit rejection of the BC4/5/DX10
family.

## src/formats/dds.hpp

UPSTREAM: NONE —— Pfim v0.11.3(src/Pfim/dds/{Dds,DdsHeader,
UncompressedDds,CompressedDds,Dxt1Dds,Dxt3Dds,Dxt5Dds,Bc5Dds}.cs)逐语义
移植;上游 OpenRA 经 OpenRA.Mods.Common/SpriteLoaders/DdsLoader.cs
@1f594e320 间接消费(IsDds 的魔数判定逐字取自 DdsLoader.cs L24-31)。
DDS 子集:124 字节头 + 32 字节像素格式(ThreeCC 分派)、非压缩 RGB
(8/16/24/32bpp + 位掩码 R/B 交换 + Rgba16 半字节交换 + mip 链)、
DXT1/3/5 块解码(RGB565 浮点插值逐字 + 3 位 alpha 梯度)。Data = 全
mip 链拼接缓冲(上游 DdsFrame.Data 同为全数组)。
偏离(D66):FourCC BC4/BC4s/BC5/BC5s/ATI1/ATI2/DX10(BC6h/BC7)未
移植 —— 显式抛 "FourCC: X not supported."(上游 BC5 系可解 Rgb24;
mods 零 dds 资产,oracle 向量覆盖已移植路径)。缓冲流与 MemoryStream
快路径统一走内存路径(D65)。
The DDS subset: the 124-byte header + the 32-byte pixel format (the
FourCC dispatch), the uncompressed RGB paths (8/16/24/32bpp + the
bitmask R/B swap + the Rgba16 nibble swap + the mip chain), and the
DXT1/3/5 block decoders (the RGB565 float interpolation verbatim + the
3-bit alpha gradient). Data is the whole mip-chain buffer (upstream's
DdsFrame.Data is the same full array). Deviation (D66): the BC4/BC4s/
BC5/BC5s/ATI1/ATI2/DX10 (BC6h/BC7) FourCCs are not ported — an explicit
"FourCC: X not supported." throw (upstream's BC5 family would decode to
Rgb24; the mods carry zero dds assets; the oracle vectors cover the
ported paths). Buffered/fast paths unified on memory (D65).

## src/formats/embedded_sprite_palette.hpp

UPSTREAM: OpenRA.Mods.Common/SpriteLoaders/EmbeddedSpritePalette.cs
@1f594e320 L16-35(全文逐语义)
帧级调色板协商:帧专属字典命中优先,否则文件级兜底;两者皆无 → false。
形态:Dictionary<int,uint[]> → 有序 map(仅 TryGetValue 语义,无序敏);
uint[](null)→ optional<vector>(帧级字典的 null 值条目上游返回 false,
C++ 空 vector 视为"有"—— 加载路径不可达,值恒为构造产物)。消费方:
PaletteFromEmbeddedSpritePalette(Phase 5 traits 接线)。
The per-frame palette negotiation: a frame-specific dictionary hit wins,
else the file-level fallback; neither → false. Shape: Dictionary<int,
uint[]> → an ordered map (only TryGetValue semantics, order-insensitive);
uint[] (null) → optional<vector> (a null-valued frame-dictionary entry
returns false upstream while an empty C++ vector counts as present —
unreachable on loader paths, where values are always constructed). See
the Chinese note above. Consumer: PaletteFromEmbeddedSpritePalette
(wired with the Phase 5 traits).

## src/formats/fast_byte_reader.hpp

UPSTREAM: OpenRA.Mods.Common/FileFormats/FastByteReader.cs @1f594e320 L16-43(全文)
快速字节读取器:LCW/XOR/RLE0 解码的内环热路径(每字节一次方法调用,
无虚分发)。ReadWord 返回 int(x | next<<8),上游原样。
形态适配:byte[] + int offset → span + size_t;越界读上游抛 CLR
IndexOutOfRangeException,C++ 抛 std::runtime_error(仅非法输入可达,
合法资产不触发 —— COVERAGE 登记)。
The fast byte reader: the hot inner-loop path of the LCW/XOR/RLE0
decoders (one method call per byte, no virtual dispatch). ReadWord
returns an int (x | next<<8), exactly as upstream. Shape adaptation:
byte[] + int offset → span + size_t; out-of-bounds reads throw CLR's
IndexOutOfRangeException upstream and std::runtime_error here (reachable
only on malformed input — registered in COVERAGE).

## src/formats/hva_reader.cpp

UPSTREAM: OpenRA.Mods.Cnc/FileFormats/HvaReader.cs @1f594e320 + Util.cs
L131-255(MatrixInverse 子集)—— 逐句照抄(Stream → SpanReader)。
[UPSTREAM continued] HvaReader.cs + the Util.cs L131-255 MatrixInverse
subset, copied statement by statement (Stream → SpanReader).

## src/formats/hva_reader.hpp

UPSTREAM: OpenRA.Mods.Cnc/FileFormats/HvaReader.cs @1f594e320(全文)+
OpenRA.Mods.Cnc/Util.cs L131-255 的 MatrixInverse 子集(HVA 唯一消费面;
Util 余部随 ModelRenderer 批次移植)。
Westwood HVA 骨骼动画:16 字节名 + FrameCount/LimbCount u32 + 16×LimbCount
肢体名跳过 + 每帧每肢体 12 个 f32 —— 经转置表 ids 写入 16 元列主序矩阵
(末行/列 0,0,0,1),并逐矩阵 MatrixInverse 可逆性校验(det == 0 抛)。
形态适配:Stream → SpanReader;MatrixInverse 返回 null → std::optional。
[UPSTREAM continued] HvaReader.cs in full plus the MatrixInverse subset of
Util.cs L131-255 (HVA's only consumer; the rest of Util lands with the
ModelRenderer batch). The Westwood HVA skeletal animation: a 16-byte name +
FrameCount/LimbCount u32s + a 16×LimbCount limb-name skip + 12 f32 per
frame per limb — written through the transpose table ids into 16-element
column-major matrices (the final row/column 0,0,0,1), each validated for
invertibility by MatrixInverse (det == 0 throws). Shape adaptation:
Stream → SpanReader; MatrixInverse's null return → std::optional.

## src/formats/idx_reader.cpp

UPSTREAM: OpenRA.Mods.Cnc/FileFormats/IdxReader.cs + IdxEntry.cs
@1f594e320 —— 逐句照抄(Stream → SpanReader)。
[UPSTREAM continued] IdxReader.cs + IdxEntry.cs copied statement by
statement (Stream → SpanReader).

## src/formats/idx_reader.hpp

UPSTREAM: OpenRA.Mods.Cnc/FileFormats/IdxReader.cs @1f594e320(全文)+
IdxEntry.cs L16-45(全文)。RA2 声音索引:16 字节名 + "GABA" 魔数头 +
u32==2 校验 + SoundCount 条目(名 16B/偏移/长度/采样率/标志/块大小)。
IdxEntry 怪癖照抄:NUL 位 pos==0 不截断(16 个 NUL 原样入名);pos==-1
(无 NUL 的 16 字符名)上游 name[..-1] 抛 ArgumentOutOfRangeException
(双行默认消息,.NET 10 实测),此处等价抛。
[UPSTREAM continued] IdxReader.cs in full plus IdxEntry.cs L16-45. The
RA2 sound index: 16-byte name + the "GABA" magic header + a u32==2 check +
SoundCount entries (name 16B/offset/length/sample rate/flags/chunk size).
IdxEntry quirks kept: a NUL at pos 0 does not truncate (16 NULs stay in
the name); pos == -1 (a 16-char name with no NUL) makes upstream's
name[..-1] throw ArgumentOutOfRangeException (the two-line default
message, verified against .NET 10), thrown here as the equivalent.

## src/formats/ima_adpcm.cpp

UPSTREAM: OpenRA.Mods.Common/FileFormats/ImaAdpcmReader.cs @1f594e320
L18-31(表)与 L33-85(两函数):逐句照抄;byte[]/Span → span。
[UPSTREAM continued] ImaAdpcmReader.cs L18-31 (the tables) and L33-85 (both
functions): copied statement by statement; byte[]/Span → span.

## src/formats/ima_adpcm.hpp

UPSTREAM: OpenRA.Mods.Common/FileFormats/ImaAdpcmReader.cs @1f594e320 L16-87(全文)
IMA ADPCM 单样点解码 + 4 字节组解码:IndexAdjust 8 项 / StepTable 89 项
逐值照搬;delta = StepTable[index]*b/4 + StepTable[index]/8 的两次整除
均为 C# int 除(向零截断);current/index 的饱和边界 32767/-32768 与
0/88 逐行保留。AudReader(IMA 流)与 WavReader 的 WavStreamImaAdpcm 消费。
[UPSTREAM continued] the whole of ImaAdpcmReader.cs — the single-sample decode plus
the 4-byte-group decode: the 8-entry IndexAdjust and the 89-entry
StepTable copied value for value; delta = StepTable[index]*b/4 +
StepTable[index]/8 keeps both C# int divisions (truncation towards
zero); the saturation bounds of current/index (32767/-32768, 0/88) stay
line by line. Consumed by AudReader (the IMA stream) and WavReader's
WavStreamImaAdpcm.

## src/formats/lcw.cpp

UPSTREAM: OpenRA.Mods.Cnc/FileFormats/LCWCompression.cs @1f594e320 L16-167
实现文件:解码五 case / ReplicatePrevious / CountSame / WriteCopyBlocks /
Encode 逐句照抄;byte[] → span。
Implementation file: the five decoder cases / ReplicatePrevious /
CountSame / WriteCopyBlocks / Encode copied statement by statement;
byte[] → span.

## src/formats/lcw.hpp

UPSTREAM: OpenRA.Mods.Cnc/FileFormats/LCWCompression.cs @1f594e320 L16-167(全文)
Lempel-Castle-Welch 算法(aka Format80)。解码五 case 逐分支照抄:
  case1 = 0x80|n 之后的 n 字节原样拷贝(n==0 即终止标记);
  case2 = 回拷 ((i&0x70)>>4)+3 字节自 destIndex-rpos;
  case3/5 = 自绝对/相对 srcIndex 拷 count 字节;case4 = 单字节重复。
Encode = 上游"quick and dirty v2"(raw copy + RLE;命令 4 与终止符
0x80)。异常消息逐字(NotImplementedException 文本保留)。
形态适配:byte[] → span;MemoryStream → std::vector<std::byte>。
The Lempel-Castle-Welch algorithm (aka Format80). The decoder's five
cases are copied branch for branch: case1 copies n raw bytes following
0x80|n (n==0 is the terminator); case2 replicates ((i&0x70)>>4)+3 bytes
from destIndex-rpos; case3/5 copy count bytes from the absolute/relative
srcIndex; case4 repeats a single byte. Encode is the upstream "quick and
dirty v2" (raw copy + RLE; command 4 and the 0x80 terminator). Exception
messages verbatim (NotImplementedException texts kept). Shape
adaptation: byte[] → span; MemoryStream → std::vector<std::byte>.

## src/formats/lzo.cpp

UPSTREAM: OpenRA.Mods.Cnc/FileFormats/LZOCompression.cs @1f594e320 L49-291
实现文件:LZO1xDecompress 逐控制流照抄(标签 first_literal_run/
match/match_done/eof_found 与 gtFirstLiteralRun/gtMatchDone 布尔保留);
unsafe 指针 → uint8_t*;未对齐 32/16 位读写 → memcpy。
Implementation file: LZO1xDecompress copied control flow for control
flow (the first_literal_run/match/match_done/eof_found labels and the
gtFirstLiteralRun/gtMatchDone booleans kept); unsafe pointers →
uint8_t*; unaligned 32/16-bit accesses → memcpy.

## src/formats/lzo.hpp

UPSTREAM: OpenRA.Mods.Cnc/FileFormats/LZOCompression.cs @1f594e320 L49-291(minilzo 2.06 的 C# 移植,逐控制流照抄)
LZO1X 解压(minilzo 子集)。上游由 Frank Razenberg 自 minilzo 源码
预处理输出机械移植(goto 语义差异以 gt* 布尔改写);C++ 侧原生支持
goto,控制流与标签按 C# 版逐分支对照落地(含 MatchNext 后的双重读
等移植行为 —— 与 C# 运行事实一致优先)。
形态适配:unsafe byte* → uint8_t*;*(uint*)p 未对齐读写 → memcpy
(等价、无 UB);wrkmem 参数未用(上游亦未用,不移植)。
消费方:TS mix 文件、VQA/WSA 视频、Gen2 地图导入(后两者随后续批)。
LZO1X decompression (the minilzo subset). Upstream is Frank Razenberg's
mechanical port of the preprocessed minilzo 2.06 source (goto-semantics
differences rewritten with gt* booleans); C++ natively supports goto, so
the control flow and labels land branch-for-branch against the C#
version (including ported behaviors like the double read after
MatchNext — matching the C# runtime facts first). Shape adaptation:
unsafe byte* → uint8_t*; the unaligned *(uint*)p accesses → memcpy
(equivalent, no UB); the unused wrkmem parameter stays unported
(upstream never uses it). Consumers: TS mix files, VQA/WSA video, and
the Gen2 map import (the latter with later batches).

## src/formats/mp3_loader.cpp

UPSTREAM: OpenRA.Mods.Common/AudioLoaders/Mp3Loader.cs @1f594e320 L19-120
         (mp3_loader.hpp 的实现;后端偏离 D95 见头注)
         Implementation of mp3_loader.hpp; the backend deviation D95 is
         documented in the hpp header.

## src/formats/mp3_loader.hpp

UPSTREAM: OpenRA.Mods.Common/AudioLoaders/Mp3Loader.cs @1f594e320 L19-120(形态逐语义;
         解码后端替换)
MP3 音频:IsMp3 嗅探逐句 —— 首 3 字节 ASCII "ID3"(ID3v2 元数据头)或首
u16(小端)== 0xfbff(MPEG 帧同步字节 FF FB)。解码输出 16 位 PCM。
偏离(D95,COVERAGE 登记):解码后端 MP3Sharp(JavaLayer 系)→ minimp3
(third_party/Minimp3,CC0)。MP3 样点值非位精确规范,PCM 字节不保证与上游
一致;元数据面(声道/位深/采样率)等价;LengthInSeconds 上游经 TagLib 精确
解析(首猜 = 文件大小 × 8 / (2·ch·rate)),C++ 以解码样点数/rate 计(更接近
TagLib 精确值;仅 UI 时长展示可观察)。上游 mods 无 mp3 资产,黄金不含 mp3
段。PCM 与 D74 同族物化。
[UPSTREAM continued] Mp3Loader.cs L19-120 with verbatim shape but a
replaced decode backend. The IsMp3 sniff, statement by statement: the
leading three ASCII bytes "ID3" (an ID3v2 metadata header) or the leading
u16 (little-endian) == 0xfbff (the MPEG frame-sync bytes FF FB). The
decode output is 16-bit PCM. Deviation (D95, registered in COVERAGE): the
decode backend MP3Sharp (the JavaLayer family) → minimp3
(third_party/Minimp3, CC0). MP3 sample values are not a bit-exact
specification, so the PCM bytes are not guaranteed to match upstream; the
metadata surface (channels/bits/rate) is equivalent; upstream's
LengthInSeconds comes from TagLib's exact parse (with the first guess =
filesize × 8 / (2·ch·rate)), while C++ computes decoded-samples/rate
(closer to TagLib's exact value; observable only in the UI duration
display). The upstream mods ship no mp3 assets and the golden carries no
mp3 section. The PCM materializes like the D74 family.

## src/formats/ogg_loader.cpp

UPSTREAM: OpenRA.Mods.Common/AudioLoaders/OggLoader.cs @1f594e320 L19-150
         (ogg_loader.hpp 的实现;后端偏离 D94 见头注)
         Implementation of ogg_loader.hpp; the backend deviation D94 is
         documented in the hpp header.

本翻译单元为 stb_vorbis 的唯一实现包含(其余处如需原型,先
#define STB_VORBIS_HEADER_ONLY 再包含同文件)。
This translation unit is stb_vorbis's sole implementation include (any
other site wanting the prototypes includes the same file behind
#define STB_VORBIS_HEADER_ONLY).

## src/formats/ogg_loader.hpp

UPSTREAM: OpenRA.Mods.Common/AudioLoaders/OggLoader.cs @1f594e320 L19-150(形态逐语义;
         解码后端替换)
Ogg/Vorbis 音频:NVorbis 的容器头解析与 PCM 流面。OggStream.Read 的换算
逐句:count = bytes/2(16 位样点),count -= count % channels(奇数消费
截齐),float 样点 × 32767 后 (short) 截断 —— NVorbis 的 ClipSamples =
夹到 [-1,1] 再截断。
偏离(D94,COVERAGE 登记):解码后端 NVorbis(C#)→ stb_vorbis(third_party/
StbVorbis,公有领域/MIT)。Ogg/Vorbis 样点值非位精确规范,PCM 字节不保证
与上游一致;元数据面(声道/位深/采样率/时长)等价。上游 mods 无 ogg 资产,
黄金不含 ogg 段。PCM 与 D74 同族物化为整段 vector。
[UPSTREAM continued] OggLoader.cs L19-150 with verbatim shape but a
replaced decode backend. The conversion of OggStream.Read, statement by
statement: count = bytes/2 (16-bit samples), count -= count % channels
(the odd-consumer truncation), and the float samples × 32767 truncated by
the (short) cast — NVorbis's ClipSamples = clamping to [-1,1] before the
truncation. Deviation (D94, registered in COVERAGE): the decode backend
NVorbis (C#) → stb_vorbis (third_party/StbVorbis, public domain/MIT).
Ogg/Vorbis sample values are not a bit-exact specification, so the PCM
bytes are not guaranteed to match upstream; the metadata surface
(channels/bits/rate/duration) is equivalent. The upstream mods ship no ogg
assets and the golden carries no ogg section. The PCM materializes into a
whole vector like the D74 family.

## src/formats/png.cpp

UPSTREAM: OpenRA.Game/FileFormats/Png.cs @1f594e320 L29-592(实现半;头文件为契约半)
形态适配汇总见 png.hpp 头注。zlib:miniz mz_uncompress / mz_compress
(third_party/miniz;import std 门禁白名单)。
See png.hpp's header note for the shape-adaptation summary. zlib: miniz's
mz_uncompress / mz_compress (third_party/miniz; whitelisted by the
import-std gate).

## src/formats/png.hpp

UPSTREAM: OpenRA.Game/FileFormats/Png.cs @1f594e320 L29-592(全文逐语义)
自研 PNG 编解码器(OPT-B3:上游自研 592 行,PORTING_PLAN 决定照抄保行为
一致,含 Indexed8 读写):块循环(IHDR/PLTE/tRNS/IDAT/tEXt/IEND/未知块
跳过)、PngIdatStream 的 IDAT 链缝合、zlib 解压 + 五滤波反解(None/Sub/
Up/Average/Paeth)+ 位深 1/2/4 解包、tEXt 键值收集、Save 的块序(IHDR→
PLTE→tRNS(任一 alpha>0)→IDAT(行滤波 0)→tEXt→IEND)与 CRC32 链式写。
形态适配(COVERAGE 登记):
  - zlib 层 .NET ZLibStream → miniz(mz_uncompress/mz_compress):解压
    输出逐字节等价;坏压缩流的异常消息/时机不逐字(D63);Save 的 deflate
    字节依实现(D64)——黄金以"结构 CRC + IDAT 解压 CRC"替代字节对拍;
  - IDAT 链缝合流式 → 整段物化后单次解压(输出等价,D63);
  - Color[](GDI)→ PngColor(RGBA 字节对);Dictionary<string,string> →
    插入序 vector(键更新保位,同 Dictionary 语义);
  - Encoding.ASCII 的 >0x7F → '?' 替换语义保留。
The in-house PNG codec (OPT-B3: upstream's own 592 lines, kept verbatim in
behavior per PORTING_PLAN, Indexed8 read/write included): the chunk loop
(IHDR/PLTE/tRNS/IDAT/tEXt/IEND + unknown-chunk skipping), PngIdatStream's
IDAT-chain stitching, zlib decompression + the five unfilters (None/Sub/
Up/Average/Paeth) + the 1/2/4-bit unpacking, tEXt collection, and Save's
chunk order (IHDR→PLTE→tRNS when any alpha>0→IDAT with filter-0 rows→
tEXt→IEND) with chained CRC32 writes. Shape adaptations (registered in
COVERAGE): .NET ZLibStream → miniz (decode output byte-equal; corrupt-
stream exception texts/timing differ, D63; Save's deflate bytes are
implementation-defined, D64 — the golden uses a structural CRC + an
IDAT-decompressed CRC instead of byte comparison); the streamed IDAT
stitching → materialize-then-inflate once (output-equivalent, D63);
Color[] (GDI) → PngColor (RGBA bytes); Dictionary → an insertion-ordered
vector (key updates keep position, same as Dictionary); the ASCII
>0x7F→'?' replacement semantics kept.

## src/formats/png_sheet_loader.cpp

UPSTREAM: OpenRA.Mods.Common/SpriteLoaders/PngSheetLoader.cs @1f594e320 L36-164
         (png_sheet_loader.hpp 的实现;头注的形态适配说明适用)
         Implementation of png_sheet_loader.hpp; the shape-adaptation
         notes of the hpp header apply.

## src/formats/png_sheet_loader.hpp

UPSTREAM: OpenRA.Mods.Common/SpriteLoaders/PngSheetLoader.cs @1f594e320 L36-164(全文逐语义)
PNG 精灵表:整图或切片。帧区来源两路 —— 手工 Frame[i] 嵌入元数据
("x,y,w,h;offsetX,offsetY",tEXt 键)优先;否则按 FrameSize/FrameAmount/
Offset 切片(默认整图单帧,FrameAmount 单独出现时水平等分)。逐帧自源
数据按行距拷贝。越界帧区/非法尺寸抛 InvalidDataException(消息逐字,
Size.ToString = "W,H")。
形态适配:Stream → span(Png 解码复用 src/formats/png.cpp);上游
TypeDictionary metadata(PngSheetMetadata + EmbeddedSpritePalette)暂不
出参 —— 引擎消费面(SpriteCache/FrameLoader)对 metadata 仅透传不读,
mods 序列批再接(COVERAGE 登记);Stream 重读(加载器约定 TryParse 后
流位置复位)由 span 天然满足。
[UPSTREAM continued] the whole of PngSheetLoader.cs L36-164, verbatim
semantics. A PNG sprite sheet: the whole image or slices. Frame regions
come two ways — manual Frame[i] embedded metadata ("x,y,w,h;offsetX,
offsetY" over tEXt keys) first; otherwise the FrameSize/FrameAmount/Offset
slicing (the whole image as one frame by default; a lone FrameAmount splits
horizontally). Each frame copies row-by-row from the source data.
Out-of-bounds regions / illegal sizes raise InvalidDataException (messages
verbatim; Size.ToString = "W,H"). Shape adaptations: Stream → span (the
Png decode reuses src/formats/png.cpp); upstream's TypeDictionary metadata
(PngSheetMetadata + EmbeddedSpritePalette) leaves as no output for now —
the engine consumers (SpriteCache/FrameLoader) only pass metadata through
without reading it, and the mods-sequences batch wires it (registered in
COVERAGE); the Stream re-read convention (loaders leave the stream
position reset after TryParse) holds naturally over a span.

## src/formats/r8_loader.cpp

UPSTREAM: OpenRA.Mods.D2k/SpriteLoaders/R8Loader.cs @1f594e320 L24-229
         (r8_loader.hpp 的实现;头注的形态适配说明适用)
         Implementation of r8_loader.hpp; the shape-adaptation notes of
         the hpp header apply.

## src/formats/r8_loader.hpp

UPSTREAM: OpenRA.Mods.D2k/SpriteLoaders/R8Loader.cs @1f594e320 L24-229(全文逐语义)
Dune 2000 的 R8/R16 图像:帧头(1 字节 type,前导 0 跳过)+ w/h/x/y 四
i32 + imageHandle u32 + paletteHandle i32 + bpp u8(8/16)+ frameW/frameH
两 u8 + 对齐 u8。bpp16 数据为 RGB555,展开成 BGRA32(0xFF alpha,通道
左移对齐);bpp8 为索引数据。type==1 且 paletteHandle!=0 时帧尾随 512
字节调色板(RGB555 同式展开,索引 0 重映射为透明);type==2 沿用上一帧
调色板。RemappableFrame 将索引数据按需展开为 BGRA:shroud→fog 的位运算
半调(~((c>>1)&0x7F7F7F))、索引 1 → 影色(140<<24)、玩家色 240..255 的
PlayerColorRemap 重建(HSV 域)。
形态适配:Stream → SpanReader;byte[] 惰性 Data → 首次调用物化并缓存;
uint[] Palette → optional<vector<uint32>>;IsR8 的越界读(上游 EOS 异常逃
出 TryParse 的 catch 面)= 文件长度前置检查归 false。
[UPSTREAM continued] the whole of R8Loader.cs L24-229, verbatim
semantics. The Dune 2000 R8/R16 image: a frame header (a 1-byte type with
leading zeros skipped) + four i32s (w/h/x/y) + imageHandle u32 +
paletteHandle i32 + bpp u8 (8/16) + two u8s (frameW/frameH) + one
alignment u8. bpp16 data is RGB555 expanded to BGRA32 (0xFF alpha with
channel left-shifts); bpp8 is indexed. When type==1 and paletteHandle!=0 a
512-byte palette trails the frame (the same RGB555 expansion, index 0
remapped to transparent); type==2 reuses the previous frame's palette.
RemappableFrame expands the indexed data to BGRA on demand: the
shroud-to-fog bitwise halving (~((c>>1)&0x7F7F7F)), index 1 → the shadow
color (140<<24), and the PlayerColorRemap rebuild over the player range
240..255 (HSV domain). Shape adaptations: Stream → SpanReader; the lazy
byte[] Data → materialized on first call then cached; uint[] Palette →
optional<vector<uint32>>; IsR8's out-of-bounds read (upstream's EOS
exception escaping the TryParse catch surface) = a length pre-check
returning false.

## src/formats/rle_zeros.cpp

UPSTREAM: OpenRA.Mods.Common/FileFormats/RLEZerosCompression.cs @1f594e320 L19-36
实现文件:逐句照抄;byte[] → span。
Implementation file: copied statement by statement; byte[] → span.

## src/formats/rle_zeros.hpp

UPSTREAM: OpenRA.Mods.Common/FileFormats/RLEZerosCompression.cs @1f594e320 L16-37(全文)
零值游程编码(aka Format2):cmd==0 时后跟 count 字节零(Array.Clear
语义,destIndex 起清 count 字节);否则 cmd 本身即一个字面字节。
ShpD2/ShpTS(Format3 扫描线)消费。越界(零段与字面量两分支)= 上游 IndexOutOfRange 的
runtime_error 等价抛点(坏数据专用)。
Run-length encoded sequences of zeros (aka Format2): cmd==0 is followed
by a count of zeroed bytes (Array.Clear semantics, clearing count bytes
from destIndex); otherwise cmd itself is one literal byte. Consumed by
ShpD2 and ShpTS (Format3 scanlines). Out of bounds throws
std::runtime_error at the equivalent point of upstream's
IndexOutOfRangeException (malformed data only).

## src/formats/shp_d2.cpp

UPSTREAM: OpenRA.Mods.Cnc/SpriteLoaders/ShpD2Loader.cs @1f594e320 L17-172
实现文件:ShpD2Frame 头解析/查表/LCW 预解压/RLE0、IsShpD2、ParseFrames
逐句照抄;Stream → SpanReader。
Implementation file: the ShpD2Frame header parse / lookup table / LCW
pre-decode / RLE0, IsShpD2, and ParseFrames copied statement by
statement; Stream → SpanReader.

## src/formats/shp_d2.hpp

UPSTREAM: OpenRA.Mods.Cnc/SpriteLoaders/ShpD2Loader.cs @1f594e320 L17-172(全文)
Dune II 的 SHP 图像:帧头(flags/宽/高/数据长)+ 可选调色板查表
(PaletteTable/VariableLengthTable;无表时默认 256 项恒等表改四项)+
可选 LCW 预解压 + RLE0(Format2)扫描线。帧偏移表支持 2/4 字节两型
(首偏移第三字节非零 = 2 字节型)。判定三重(帧数/末偏移 + 2 == 文件
长/首帧 flags ∈ {≤3, 5})逐行照抄。
The Dune II SHP image: a frame header (flags/width/height/data length)
+ an optional palette lookup table (PaletteTable/VariableLengthTable;
without one, a 256-entry identity table with four entries rewritten) +
an optional LCW pre-decode + RLE0 (Format2) scanlines. The frame
offset table comes in 2- and 4-byte flavors (a non-zero third byte of
the first offset means the 2-byte flavor). The three-part probe (image
count / final offset + 2 == file length / first frame's flags ∈ {≤3,
5}) is copied line by line.

## src/formats/shp_remastered.cpp

UPSTREAM: OpenRA.Mods.Cnc/SpriteLoaders/ShpRemasteredLoader.cs @1f594e320
L24-121(实现半;头文件为契约半)。两条上游正则 boost::regex 消费点
直调(D235;原等价手写复刻 D67 由本批取代):
  - FilenameRegex ^(?<prefix>.+?[\-_])(?<frame>\d{4})\.tga$ 的惰性
    .+? = boost perl 语义的最左匹配(前缀含分隔符);
  - MetaRegex 的 {"size":[w,h],"crop":[l,t,r,b]} 六命名组捕获,
    regex_match 全串恰匹配。
boost::regex 替代 std/手写的选型理由:其匹配引擎性能优于 std::regex
(../cpp26.md §二 std 优先规则的性能豁免)。容器实现:ora::fs::
ReadOnlyZipFile(条目序 = 中央目录序)。
The implementation half (the contract half is in the header). The two
upstream regexes are consumed by direct boost::regex calls at the use
sites (D235; superseding the former equivalent hand-written parsers of
D67): FilenameRegex ^(?<prefix>.+?[\-_])(?<frame>\d{4})\.tga$ with its
lazy .+? = the leftmost match of boost's perl semantics (the prefix
includes the separator); MetaRegex's six named capture groups over
{"size":[w,h],"crop":[l,t,r,b]} with regex_match as the exact
full-string match. boost::regex over std/hand-written per the
performance exemption of ../cpp26.md §2: its matching engine outperforms
std::regex. The container: ora::fs::ReadOnlyZipFile (entry order =
central directory order).

## src/formats/shp_remastered.hpp

UPSTREAM: OpenRA.Mods.Cnc/SpriteLoaders/ShpRemasteredLoader.cs @1f594e320
L24-121(全文逐语义)
Remastered SHP = zip 容器:条目名正则 ^(?<prefix>.+?[\-_])(?<frame>
\d{4})\.tga$ 的惰性前缀(最短 .+? 后随 -/_ 分隔)、帧数 = max(帧号+1)、
缺号帧 = 空白 TgaFrame、meta 条目 {"size":[w,h],"crop":[l,t,r,b]} 的
画框/裁剪窗口、前缀不一致即抛。SharpZipLib ZipFile → ora::fs::
ReadOnlyZipFile(miniz;条目枚举序 = 中央目录序,同 SharpZipLib 事实序);
两条正则 boost::regex 消费点直调(性能选型,取代 D67 手写复刻;
COVERAGE 登记 D235)。
The Remastered SHP is a zip container: the entry-name regex ^(?<prefix>.
+?[\-_])(?<frame>\d{4})\.tga$ with its lazy prefix (the shortest .+?
followed by a -/_ separator), frame count = max(frame+1), gap frames as
blank TgaFrames, the meta entries' {"size":[w,h],"crop":[l,t,r,b]}
canvas/crop window, and the prefix-mismatch throw. SharpZipLib ZipFile →
ora::fs::ReadOnlyZipFile (miniz; the entry enumeration order = the
central-directory order, the same de-facto order as SharpZipLib); the
two regexes are consumed by direct boost::regex calls at the use sites
(a performance selection superseding the D67 hand-written parsers;
registered as D235).

## src/formats/shp_td.cpp

UPSTREAM: OpenRA.Mods.Cnc/SpriteLoaders/ShpTDLoader.cs @1f594e320 L17-331
实现文件:IsShpTD / ShpTDSprite 构造(ImageHeader 表 + 引用链 +
Decompress)/ TrimmedFrame 收边,逐句照抄;Stream → SpanReader。
Implementation file: IsShpTD / the ShpTDSprite construction (the
ImageHeader table + reference chain + Decompress) and the
TrimmedFrame wrapper, copied statement by statement; Stream →
SpanReader.

## src/formats/shp_td.hpp

UPSTREAM: OpenRA.Mods.Cnc/SpriteLoaders/ShpTDLoader.cs @1f594e320 L17-331(全文)
Command & Conquer TD/RA 的 SHP 图像:帧头表(偏移低 24 位 + 格式高
8 位)+ 三压缩格式(XORPrev/XORLCW/LCW)+ 引用解压链(XOR 帧引用
兄弟帧数据)+ TrimmedFrame 收边(非零像素包围盒裁剪,偶数行列对齐,
半像素偏移防御)。递归深度防线(Format20/40 infinite loop)与异常
消息逐字。
形态适配:Stream → SpanReader;Frames 的 IReadOnlyList → vector<
unique_ptr<ISpriteFrame>>;ShpTDSprite.Write(Utility 面)随 Phase 8;
headers.ToDictionary 的重复键 ArgumentException → runtime_error 等价
抛点(合法文件无重复 —— COVERAGE 登记)。
The Command & Conquer TD/RA SHP image: a frame-header table (offset in
the low 24 bits + format in the high 8) with three compression formats
(XORPrev/XORLCW/LCW), a reference decompression chain (XOR frames borrow
sibling frame data), and the TrimmedFrame bounds-trimming wrapper
(non-zero-pixel bounding box, even row/column alignment, half-pixel
offset guard). The recursion-depth guard ("Format20/40 headers contain
infinite loop") and exception messages are verbatim. Shape adaptation:
Stream → SpanReader; the IReadOnlyList Frames → vector<unique_ptr<
ISpriteFrame>>; ShpTDSprite.Write (the Utility face) lands with Phase 8;
headers.ToDictionary's duplicate-key ArgumentException → an equivalent
std::runtime_error throw (valid files carry no duplicates — registered
in COVERAGE).

## src/formats/shp_ts.cpp

UPSTREAM: OpenRA.Mods.Common/SpriteLoaders/ShpTSLoader.cs @1f594e320 L17-163
实现文件:ShpTSFrame 头解析与三种扫描线格式、IsShpTS 判定循环、
ParseFrames 逐句照抄;Stream → SpanReader。
Implementation file: the ShpTSFrame header parse with its three
scanline formats, the IsShpTS probe loop, and ParseFrames copied
statement by statement; Stream → SpanReader.

## src/formats/shp_ts.hpp

UPSTREAM: OpenRA.Mods.Common/SpriteLoaders/ShpTSLoader.cs @1f594e320 L17-163(全文)
Tiberian Sun 的 SHP 图像:24 字节/帧头表(x/y/宽/高/格式/文件偏移),
奇数宽高向上取偶(半像素偏移防御,Offset 记录取整差);三种格式:
Format3 = RLE0(Format2)压缩扫描线(每行 u16 前缀长);Format2 =
未压缩长度前缀扫描线;Format1/0 = 未压缩整宽行。判定循环容忍前导
零尺寸伪帧(IsShpTS 的 do-while 逐行照抄)。
The Tiberian Sun SHP image: a 24-byte-per-frame header table (x/y/
width/height/format/file offset) with odd widths and heights rounded up
(the half-pixel-offset defense; Offset records the rounding delta);
three formats: Format3 = RLE0 (Format2) compressed scanlines (each row
carrying a u16 length prefix); Format2 = uncompressed length-prefixed
scanlines; Formats 1/0 = an uncompressed full-width row. The probe
loop tolerates leading zero-sized bogus frames (IsShpTS's do-while
copied line by line).

## src/formats/span_reader.hpp

UPSTREAM: NONE —— 自有形态适配件:对应 C# Stream 的随机访问读取面
上游格式加载器直接消费 System.IO.Stream(Position/Seek/ReadUInt16/
ReadBytes…,StreamExtensions 扩展)。C++ 侧将文件字节物化为 span 后
以本读取器替换 Stream:位置 long 语义、小端多字节读取、区间读取返回
span(零拷贝)。越界 = 上游 EndOfStreamException/IndexOutOfRange 的
等价抛点(std::runtime_error;仅坏数据可达 —— COVERAGE 登记)。
UPSTREAM: NONE (an in-house shape adapter replacing the random-access
read surface of C#'s Stream). Upstream format loaders consume a
System.IO.Stream directly (Position/Seek/ReadUInt16/ReadBytes via the
StreamExtensions). The C++ side materializes file bytes into a span and
substitutes this reader for the Stream: long-typed position, little-
endian multi-byte reads, and zero-copy subspan reads. Out-of-bounds
access throws std::runtime_error at the equivalent points of upstream's
EndOfStreamException/IndexOutOfRange (reachable only on malformed data —
registered in COVERAGE).

## src/formats/targa.cpp

UPSTREAM: NONE —— Pfim v0.11.3 逐语义移植(targa/ 四件 + Targa.cs 骨架);
上游 OpenRA 消费面 TgaLoader.cs @1f594e320。适配汇总见 targa.hpp 头注。
忠实保留的上游怪癖:TopLeft-RLE 行紧排(不按 stride 跳衬垫)、
15bpp 的 PixelDepthBytes=1 截断、色图应用 newLen = depthBytes×DataLen
(非宽高积)。缓冲流/快路径统一(D65)。
UPSTREAM: NONE — the verbatim-semantics port of Pfim v0.11.3 (the four
targa/ files + the Targa.cs skeleton); the OpenRA consumer face is
TgaLoader.cs @1f594e320. See targa.hpp's header note. Upstream quirks kept
faithfully: the TopLeft-RLE tight row packing (no stride skips), the
PixelDepthBytes=1 truncation at 15bpp, and the color-map application's
newLen = depthBytes×DataLen (not width×height). Buffered/fast paths
unified (D65).

## src/formats/targa.hpp

UPSTREAM: NONE —— Pfim v0.11.3(src/Pfim/targa/{Targa,TargaHeader,
UncompressedTarga,CompressedTarga}.cs)逐语义移植;上游 OpenRA 经
OpenRA.Mods.Common/SpriteLoaders/TgaLoader.cs @1f594e320 间接消费(IsTga
的字节判定逐字取自 TgaLoader.cs L24-53)。
真彩 Targa 解码:18 字节头(图像类型/色图规格/原点/描述字节的 4-5 位
定向)、非压缩与 RLE 两解码器 × 四向原点(BottomLeft/BottomRight/
TopRight 归一到自下而上填充,TopLeft 自上而下)、4 字节对齐 stride 的
行尾零衬垫、色图应用(RGB565/888/8888 查表展开)。Pfim 的 Allocator/
Rent 形态 → vector;缓冲流与 MemoryStream 快路径在合法输入下输出一致,
C++ 统一走内存路径(D65)。
The true-color Targa decoder: the 18-byte header (image type / color-map
spec / origin / the descriptor bits 4-5 for orientation), the
uncompressed and RLE decoders × the four orientations (BottomLeft/
BottomRight/TopRight normalize to bottom-up filling, TopLeft fills
top-down), the 4-byte-aligned stride with zero row padding, and the
color-map application (RGB565/888/8888 table expansion). Pfim's
Allocator/Rent shape → vector; the buffered and MemoryStream fast paths
produce identical output on valid input — C++ unifies on the memory path
(D65).

## src/formats/tmp_ra.cpp

UPSTREAM: OpenRA.Mods.Cnc/SpriteLoaders/TmpRALoader.cs @1f594e320 L17-102
实现文件:TmpRAFrame / IsTmpRA / ParseFrames 逐句照抄;Stream →
SpanReader。
Implementation file: TmpRAFrame / IsTmpRA / ParseFrames copied
statement by statement; Stream → SpanReader.

## src/formats/tmp_ra.hpp

UPSTREAM: OpenRA.Mods.Cnc/SpriteLoaders/TmpRALoader.cs @1f594e320 L17-102(全文)
Red Alert 的 TMP 地形 tile 集:与 TmpTD 同构,头布局多 4 字节
(imgStart@16、魔数 @26 u16 == 0x2c73)、indexStart@36。空 tile 语义
同 TD。
The Red Alert TMP terrain tile set: isomorphic to TmpTD with a
4-byte-larger header layout (imgStart@16, the magic @26 u16 ==
0x2c73, indexStart@36). Empty-tile semantics as in TD.

## src/formats/tmp_td.cpp

UPSTREAM: OpenRA.Mods.Cnc/SpriteLoaders/TmpTDLoader.cs @1f594e320 L17-99
实现文件:TmpTDFrame / IsTmpTD / ParseFrames 逐句照抄;Stream →
SpanReader。
Implementation file: TmpTDFrame / IsTmpTD / ParseFrames copied
statement by statement; Stream → SpanReader.

## src/formats/tmp_td.hpp

UPSTREAM: OpenRA.Mods.Cnc/SpriteLoaders/TmpTDLoader.cs @1f594e320 L17-99(全文)
Command & Conquer TD 的 TMP 地形 tile 集:头(w/h/imgStart/indexEnd/
indexStart)+ 单字节 tile 索引(255 = 空 tile)。魔数:@16 u32 == 0
且 @20 u32 == 0x0D1AFFFF。空 tile 的 Data = 空、Size = (0,0)、
FrameSize = tile 尺寸。
The Command & Conquer TD TMP terrain tile set: the header (w/h/
imgStart/indexEnd/indexStart) + a one-byte tile index (255 = an empty
tile). Magic: @16 u32 == 0 and @20 u32 == 0x0D1AFFFF. An empty tile's
Data is empty, its Size (0,0), and its FrameSize the tile size.

## src/formats/tmp_ts.cpp

UPSTREAM: OpenRA.Mods.Cnc/SpriteLoaders/TmpTSLoader.cs @1f594e320 L17-200
实现文件:TmpTSFrame(菱形展开/extra 回填)/ TmpTSDepthFrame /
UnpackTileData / IsTmpTS / ParseFrames 逐句照抄;Stream → SpanReader。
索引 .at():上游数组负/越界索引抛 IndexOutOfRange 的等价抛点(合法
文件不可达)。
Implementation file: TmpTSFrame (the diamond unpack / extra refill) /
TmpTSDepthFrame / UnpackTileData / IsTmpTS / ParseFrames copied
statement by statement; Stream → SpanReader. The .at() indexing is the
equivalent throw point of upstream's IndexOutOfRange on negative or
out-of-bounds array indices (unreachable for valid files).

## src/formats/tmp_ts.hpp

UPSTREAM: OpenRA.Mods.Cnc/SpriteLoaders/TmpTSLoader.cs @1f594e320 L17-200(全文)
Tiberian Sun 的 TMP 地形 tile 集:菱形 tile(UnpackTileData:行宽自 4
起 +4/-4 递变,展开进矩形帧)+ 悬崖附加数据(flags&1 时 bounds 与主
帧联合、extra 两层主/深度回填)+ 深度通道作为第二组帧(tiles[k+
stride] = TmpTSDepthFrame,共享 parent 的 DepthData/Size/Offset)。
判定:tileW*tileH/2+52 出现于首非空 tile 头 @+12。
The Tiberian Sun TMP terrain tile set: diamond tiles (UnpackTileData:
the row width steps +4/-4 from 4, unpacked into rectangular frames) +
cliff extra data (with flags&1 the bounds union with the main frame
and a two-pass main/depth extra refill) + the depth channel exposed as
a second frame set (tiles[k+stride] = TmpTSDepthFrame sharing the
parent's DepthData/Size/Offset). Probe: tileW*tileH/2+52 appears at
@+12 of the first non-empty tile header.

## src/formats/video.hpp

UPSTREAM: OpenRA.Game/Graphics/Video.cs @1f594e320 L14-36(IVideo)+
VideoLoader.cs L16-31(IVideoLoader/GetVideo 链)。
视频契约:逐帧 BGRA 帧数据 + 帧推进/复位 + 可选音频(22050Hz 16bit
PCM,由具体格式解码为非交错字节流)。C# 属性面映射为 const 方法面;
CurrentFrameData/AudioData 的 byte[] 返回改为 span(零拷贝直传,项目
集合参数优化主轴);AudioData 的上游 null(WsaVideo)以空 span 表示。
[UPSTREAM continued] Video.cs L14-36 (IVideo) plus VideoLoader.cs L16-31
(the IVideoLoader/GetVideo chain). The video contract: per-frame BGRA
frame data with frame advance/reset plus optional audio (22050Hz 16bit
PCM decoded by each format into a non-interleaved byte stream). The C#
property surface maps to a const method surface; CurrentFrameData/
AudioData return spans instead of byte[] (zero-copy direct passing, the
project's collection-parameter optimization axis); upstream's null
AudioData (WsaVideo) becomes an empty span.

## src/formats/voc_loader.cpp

UPSTREAM: OpenRA.Mods.Cnc/AudioLoaders/VocLoader.cs @1f594e320 L19-394
         (voc_loader.hpp 的实现;头注的形态适配说明适用)
         Implementation of voc_loader.hpp; the shape-adaptation notes of
         the hpp header apply.

## src/formats/voc_loader.hpp

UPSTREAM: OpenRA.Mods.Cnc/AudioLoaders/VocLoader.cs @1f594e320 L19-394(全文逐语义)
Creative Voice File(.voc):26 字节头(20 字节描述 + datablockOffset u16 +
version u16 + id u16,id = ~version + 0x1234 校验)+ 块序列(1 字节 code +
3 字节 LE 长度)。声音块(code 1)携 freqDiv/codec 与绝对数据偏移;前一块
为附加信息块(code 8)时其采样率覆盖本块并自列表移除(struct 按值相等
Remove 的首例语义照抄);静默块(code 3)/循环块(code 6/7)入列表但读取
端只消费 code 1/9 块 —— 静默不产样点(上游 UpdateBlockIfNeeded 的可观测
语义);code 9 走 "Unhandled code in voc file" 抛(上游 default 分支)。
采样率 = 256-除数公式(a5/a6 → 11025、d2/d3 → 22050 特例;256 为非法除数;
code 8 的 16 位除数走 256000000/(65536-freqDiv),其 65536 非法检查对
u16 读数恒假 —— 上游不可达检查照抄)。混合采样率抛 "Voc file contains
chunks with different sample rate"(code 3 不参与校验)。
形态适配(D93 同族):Stream → SpanReader;VocStream 惰性流 → 整段 PCM
物化(与 D74 的 aud 同形态);EndOfStreamException 的块尾净结束 =
Position 检查。
[UPSTREAM continued] the whole of VocLoader.cs L19-394, verbatim
semantics. The Creative Voice File (.voc): a 26-byte header (a 20-byte
description + datablockOffset u16 + version u16 + id u16, with the
id = ~version + 0x1234 check) followed by the block sequence (a 1-byte
code + a 3-byte LE length). Sound blocks (code 1) carry freqDiv/codec and
an absolute data offset; when the previous block is an extra-info block
(code 8) its sample rate overrides this block's and the block removes
itself from the list (the first-occurrence Remove-by-struct-value
semantics kept); silence (code 3) and repeat blocks (code 6/7) enter the
list but the reader consumes code 1/9 blocks only — silence produces no
samples (the observable semantics of upstream's UpdateBlockIfNeeded);
code 9 takes the "Unhandled code in voc file" throw (upstream's default
branch). The sample rate = the 256-divisor formula (a5/a6 → 11025 and
d2/d3 → 22050 special cases; 256 the illegal divisor; code 8's 16-bit
divisor goes through 256000000/(65536-freqDiv), whose illegal-65536 check
is always false for a u16 read — upstream's unreachable check kept);
mixed sample rates throw "Voc file contains chunks with different sample
rate" (code 3 exempt from the check). Shape adaptations (the D93 family):
Stream → SpanReader; VocStream's lazy stream → the whole PCM materialized
(the same shape as aud's D74); EndOfStreamException's clean block-tail
ending = a Position check.

## src/formats/vqa_video.cpp

UPSTREAM: OpenRA.Mods.Cnc/FileFormats/VqaVideo.cs @1f594e320(全文)+
VqaLoader.cs:逐句照抄。异常面:三条消息逐字(Invalid vqa×2 /
Unknown sub-chunk / Vqa uses unknown Subtype)+ 参数-less 的
NotSupportedException/IndexOutOfRangeException 以 .NET 全名 + 默认消息
等价抛(仅坏数据可达)。Seek 越端:FileStream 允许 Position>Length 而
读取时抛;SpanReader 在 Seek 即抛 —— 时机差异不可观测于合法数据。
[UPSTREAM continued] VqaVideo.cs in full plus VqaLoader.cs, copied
statement by statement. The exception surface: four verbatim messages
(the two Invalid-vqa texts / Unknown sub-chunk / Vqa uses unknown
Subtype) with the parameterless NotSupportedException/
IndexOutOfRangeException thrown as the .NET full name plus the default
message (malformed data only). A Seek past end: FileStream allows
Position>Length and throws at read time; SpanReader throws at the Seek —
the timing difference is unobservable for valid data.

## src/formats/vqa_video.hpp

UPSTREAM: OpenRA.Mods.Cnc/FileFormats/VqaVideo.cs @1f594e320 L19-545(全文)
+ OpenRA.Mods.Cnc/VideoLoaders/VqaLoader.cs L18-61(IsWestwoodVqa 嗅探
与 TryParseVideo 面)。
Westwood VQA 视频:FORM/WVQA/VQHD 头 + FINF 帧偏移表(0x40000000 标志
位与 <<1);帧体 = VQFR 子块串(CPL0 调色板、CBFZ/CBF0 全量码本、
CBP0/CBPZ 分块码本(chunkBufferParts 块拼齐后于"下一帧"应用 —— 非
压缩 Clone / 压缩 LCW 解入)、VPTZ/VPRZ/VPTR 帧指令(LCW / 反向 LCW /
裸拷贝);非 HQ 8 位调色板展开((mod==0x0f) ? px : cbf[(mod*256+px)*8
+…])与 HQ 16 位码本(RGB555 拆 3 字节)两条 DecodeFrameData 路径;
音频 = SND0/SND2 块收集(SND2 走 IMA ADPCM,立体声左右道交错,奇长
度 +=2 怪癖照抄)。偶字节对齐的 `Peek() == 0` 消费依赖 StreamExts.Peek
的 EOF -1 语义。
形态适配(D78):Stream → SpanReader(构造期物化全文件 span);cbf 字
段在上游被多次重指(ctor 数组 / cbp.Clone / cbfBuffer / CBF0 新读),
C++ 以 heap 存储 + span 别名等价复刻(CBF0 照上游 ReadBytes 拷入 heap
—— 后续 CBFZ 的 Array.Clear(cbf) 会经别名写它,不可零拷贝指源);
LoadFrame 尾的 IndexOutOfRange / 参数-less NotSupportedException /
InvalidDataException 以带类型前缀的逐字消息等价抛,登记于 COVERAGE。
[UPSTREAM continued] the whole of VqaVideo.cs plus the VqaLoader.cs L18-61
sniff and TryParseVideo surface. Westwood VQA video: the FORM/WVQA/VQHD
header and the FINF frame-offset table (the 0x40000000 flag bit and the
<<1); a frame's body is a VQFR subchunk stream (the CPL0 palette, the
CBFZ/CBF0 full codebooks, the CBP0/CBPZ partial codebooks assembled over
chunkBufferParts chunks and applied "the frame after" — an uncompressed
Clone or an LCW decode into cbf), and the VPTZ/VPRZ/VPTR frame
instructions (LCW / reverse LCW / a bare copy); two DecodeFrameData
paths, the non-HQ 8-bit palette expansion ((mod==0x0f) ? px : cbf
[(mod*256+px)*8+…]) and the HQ 16-bit codebook (RGB555 split into 3
bytes); audio collected from SND0/SND2 chunks (SND2 runs IMA ADPCM,
stereo interleaves left/right, the odd-length +=2 quirk kept). The
even-alignment `Peek() == 0` consume relies on StreamExts.Peek's EOF -1.
Shape adaptation (D78): Stream → SpanReader (the whole file
materialized as a span at construction); upstream's cbf field is
re-pointed several times (the ctor array / cbp.Clone / cbfBuffer / a
fresh CBF0 read), reproduced equivalently as heap storage plus a span
alias (CBF0 copies into the heap like upstream's ReadBytes — the later
CBFZ Array.Clear(cbf) writes through the alias, so a zero-copy source
alias is impossible); the
IndexOutOfRange/parameterless NotSupportedException/InvalidDataException
throws around LoadFrame keep verbatim or equivalent messages, registered
in COVERAGE.

## src/formats/vxl_reader.cpp

UPSTREAM: OpenRA.Mods.Cnc/FileFormats/VxlReader.cs @1f594e320 —— 逐句照抄
(Stream → SpanReader;两遍列扫描保留:先计数后建图)。
[UPSTREAM continued] VxlReader.cs copied statement by statement (Stream →
SpanReader; the two column passes kept: count first, then build the map).

## src/formats/vxl_reader.hpp

UPSTREAM: OpenRA.Mods.Cnc/FileFormats/VxlReader.cs @1f594e320(全文)。
Westwood VXL 体素:16 字节头("Voxel Animation" 前缀)+ 尺寸域(LimbCount/
bodySize)+ 770 保留;每肢体 28 字节头(Name 16 + 12 保留);肢体足注位于
802 + 28*LimbCount + bodySize(limbDataOffset u32 + 8 保留 + Scale f32 +
48 保留 + Bounds 6×f32 + Size 3 字节 + NormalType 1 字节);体素数据按
limbDataOffset 回跳,每列 = baseSize 个 i32 列偏移 + 等长跳过表,列内
do-while 游程(skip 字节 + count 字节 + count×(color,normal) + 重复
count)。VoxelMap = Dictionary<byte,VxlElement>[x,y](空列 = -1)。
形态适配:Stream → SpanReader;Dictionary[,] → 行主序 vector(空 = 无
该列;插入序 = 上游 Add 序 = z 升序)。byte z 的模 256 回绕照抄。
[UPSTREAM continued] VxlReader.cs in full. The Westwood VXL voxel format:
a 16-byte header (the "Voxel Animation" prefix) + size fields (LimbCount/
bodySize) + 770 reserved bytes; per-limb 28-byte headers (Name 16 + 12
reserved); the limb footers at 802 + 28*LimbCount + bodySize (a
limbDataOffset u32 + 8 reserved + Scale f32 + 48 reserved + Bounds 6×f32 +
Size 3 bytes + NormalType 1 byte); voxel data reached by limbDataOffset
seek-back, each column = baseSize i32 column offsets + an equal-length
skip table, a column's body a do-while run list (skip byte + count byte +
count×(color,normal) + a duplicate count). VoxelMap =
Dictionary<byte,VxlElement>[x,y] (an empty column = -1). Shape
adaptation: Stream → SpanReader; Dictionary[,] → a row-major vector
(empty = no column; insertion order = upstream's Add order = ascending z).
The byte z's mod-256 wrap kept.

## src/formats/wav_reader.cpp

UPSTREAM: OpenRA.Mods.Common/FileFormats/WavReader.cs @1f594e320 +
WavLoader.cs 的嗅探/try 面:逐句照抄;两个 ReadOnlyAdapterStream
(WavStreamImaAdpcm/WavStreamMsAdpcm)的 BufferData 循环物化为整段
输出(Queue<byte> 逐字节 Enqueue 改 vector 追加,return true 改循环
退出),数据面逐字节等价。blockAlign 为 0 的除零 / 负 blockDataSize
的 new byte[] 在上游抛于工厂调用点,此处等价抛(仅坏数据可达)。
[UPSTREAM continued] WavReader.cs + WavLoader.cs's sniff/try surface, copied
statement by statement; the two ReadOnlyAdapterStream BufferData loops
(WavStreamImaAdpcm/WavStreamMsAdpcm) materialize into one output (the
byte-by-byte Queue.Enqueue becomes a vector append, return true becomes
loop exit), byte-for-byte equivalent on the data plane. A zero
blockAlign's division / a negative blockDataSize's new byte[] throw
upstream at the factory call point; here they throw the equivalent
(malformed data only).

## src/formats/wav_reader.hpp

UPSTREAM: OpenRA.Mods.Common/FileFormats/WavReader.cs @1f594e320 L20-305
(全文)+ OpenRA.Mods.Common/AudioLoaders/WavLoader.cs L20-49(IsWave 嗅
探与 TryParseSound 的 catch 面)。
RIFF/WAVE 解析:块循环(奇地址对齐衬垫、fmt/fact/data/LIST 及未知块
跳过、data 块尺寸超端按余量钳制);非 PCM 时 sampleBits 恒报 16;
channels 限 1/2(NotSupportedException 消息逐字)。三解码路径:PCM 直
通(data 段切片)、IMA ADPCM(块级 predictor/index 状态 + 交错;
outputSize = uncompressedSize × channels × 2,fact 缺失时
uncompressedSize = -1 的提前截断怪癖照抄)、MS ADPCM(bpred/idelta/
s1/s2 块头 + 高半字节恒左声道、idelta 下限 16)。
形态偏离(D74):上游返回惰性工厂(Func<Stream> + 两个
ReadOnlyAdapterStream 的 Queue<byte>);C++ 物化为整段 PCM vector,
字节输出等价;TryParse 内的解码失败折入 false(上游在流读取时才抛,
仅坏数据可达)。
[UPSTREAM continued] the whole of WavReader.cs plus WavLoader.cs L20-49 (the IsWave
sniff and the TryParseSound catch surface). RIFF/WAVE parsing: the chunk
loop (odd-address alignment padding; fmt/fact/data plus LIST and unknown
chunks skipped; a data chunk size beyond EOF clamped to the remainder);
sampleBits reports 16 for anything but PCM; channels limited to 1/2 (the
NotSupportedException message verbatim). Three decode paths: PCM
passthrough (a slice of the data chunk), IMA ADPCM (per-block
predictor/index state plus interleaving; outputSize = uncompressedSize ×
channels × 2, with the missing-fact uncompressedSize = -1
early-truncation quirk kept), and MS ADPCM (the bpred/idelta/s1/s2 block
header; the high nibble always the left channel; the idelta floor of
16). Shape deviation (D74): upstream returns a lazy factory (Func<Stream>
+ two ReadOnlyAdapterStreams enqueueing Queue<byte>); the C++ side
materializes the whole PCM into a vector with equivalent byte output, and
a decode failure inside TryParse folds into false (upstream throws later
at stream read time — malformed data only).

## src/formats/westwood_compressed.cpp

UPSTREAM: OpenRA.Mods.Common/FileFormats/WestwoodCompressedReader.cs
@1f594e320 L20-84:逐句照抄;byte[]/Span → span;output 越界守卫 = 上游
IndexOutOfRange 的等价抛点(D61 先例)。Clamp(byte.MinValue,
byte.MaxValue) 对 int 差分恒在 0..255 内截,无需分支。
[UPSTREAM continued] WestwoodCompressedReader.cs L20-84, copied statement by
statement; byte[]/Span → span; the output-bounds guard throws at
upstream's IndexOutOfRangeException points (the D61 precedent). Clamp
over the byte domain always lands in 0..255 for these int deltas, no
branch needed.

## src/formats/westwood_compressed.hpp

UPSTREAM: OpenRA.Mods.Common/FileFormats/WestwoodCompressedReader.cs
@1f594e320 L18-86(全文)
Westwood 块压缩音频解码( AUD 的 WS 编码):五分支按命令字节高 2 位
分派 —— 00:每码字节 4 个 2 位差分(±2/±1);01:每码字节 2 个 4 位
差分(±9..±8 表);10 且 count&0x20:单样点跳变((sbyte)(count<<3)>>3
的符号扩展);10:count+1 个字面字节(样点取末字节);11:count+1 个
当前样点重复。sample 初值 0x80,差分后按 byte 域饱和。input == output
长度时直通拷贝。AudReader 的 WestwoodCompressedAudStream 消费;越界写
= 上游 IndexOutOfRange 的等价抛点(坏数据专用)。
[UPSTREAM continued] the whole of WestwoodCompressedReader.cs — the Westwood
block-compressed audio decoder (the AUD "WS" coding): five branches on
the top 2 bits of the command byte — 00: four 2-bit deltas per code
byte (±2/±1); 01: two 4-bit deltas per code byte (the ±9..±8 table);
10 with count&0x20: a single-sample jump (the sign extension of
(sbyte)(count<<3)>>3); 10: count+1 literal bytes (the sample takes the
last byte); 11: count+1 repeats of the current sample. The sample
starts at 0x80 and saturates to the byte domain after each delta. An
input of the same length as output passes straight through. Consumed by
AudReader's WestwoodCompressedAudStream; out-of-bounds writes throw at
the equivalent points of upstream's IndexOutOfRangeException (malformed
data only).

## src/formats/wsa_video.cpp

UPSTREAM: OpenRA.Mods.Cnc/FileFormats/WsaVideo.cs @1f594e320(全文)+
WsaLoader.cs:逐句照抄。帧体 = LCW 解压 + XOR delta;LoadFrame 每帧
的 new byte[] 以成员 vector 每帧重置零代替(上游中间帧恒新零数组,
字节面等价;仅复用存储)。flags≠1 的 paletteBytes null → NRE 等价抛
(仅坏数据可达)。
[UPSTREAM continued] WsaVideo.cs in full plus WsaLoader.cs, copied
statement by statement. A frame's body = the LCW decompression + the XOR
delta; LoadFrame's per-frame new byte[]s become member vectors reset to
zero each frame (upstream's intermediate is always a fresh zeroed array —
byte-equal, only the storage reused). A flags≠1 null paletteBytes becomes
the NRE's equivalent throw (malformed data only).

## src/formats/wsa_video.hpp

UPSTREAM: OpenRA.Mods.Cnc/FileFormats/WsaVideo.cs @1f594e320 L18-156(全文)
+ OpenRA.Mods.Cnc/VideoLoaders/WsaLoader.cs L18-70(IsWsa 嗅探与
TryParseVideo 面)。
Westwood WSA 动画:10 字节头(frames/x/y/width/height/delta/flags)+
frames+2 个 u32 绝对偏移(flags==1 时先 768 字节调色板且偏移 +768;
调色板 <<2 后高位复制到低两位)。每帧 = Format80(LCW)解出中间帧,
再 Format40(XOR delta)作用于上一帧索引图,末按 totalFrameWidth 行
距展开 BGRA。Framerate 恒 15;无音频。
形态适配(D79):Stream → SpanReader;LoadFrame 每帧的 new byte[]
(intermediate/current)以成员 vector 每帧重置零代替(字节面等价);
flags≠1 时上游 paletteBytes 为 null → 展开处 NRE 的等价抛;IsWsa 的
`width <= 0` 对 ushort 恒假(上游怪癖照抄)。
[UPSTREAM continued] the whole of WsaVideo.cs plus the WsaLoader.cs L18-70
sniff and TryParseVideo surface. Westwood WSA animation: a 10-byte header
(frames/x/y/width/height/delta/flags) plus frames+2 absolute u32
offsets (with flags==1 reading a 768-byte palette first and adding 768
to the offsets; the palette shifts left by 2 then copies its high bits
into the low two). Each frame = Format80 (LCW) decoded into an
intermediate, then Format40 (the XOR delta) applied over the previous
frame's index map, finally expanded to BGRA on the totalFrameWidth row
stride. Framerate is a constant 15; no audio. Shape adaptation (D79):
Stream → SpanReader; LoadFrame's per-frame new byte[]s (intermediate/
current) become member vectors reset to zero each frame (byte-equal);
with flags≠1 upstream's paletteBytes is null → the NRE's equivalent
throw at the expansion; IsWsa's `width <= 0` is always false for a
ushort (the upstream quirk kept).

## src/formats/xcc_database.cpp

UPSTREAM: OpenRA.Mods.Cnc/FileFormats/XccLocalDatabase.cs @1f594e320 L19-69
         OpenRA.Mods.Cnc/FileFormats/XccGlobalDatabase.cs @1f594e320 L18-60

## src/formats/xcc_database.hpp

UPSTREAM: OpenRA.Mods.Cnc/FileFormats/XccLocalDatabase.cs @1f594e320 L19-69
         OpenRA.Mods.Cnc/FileFormats/XccGlobalDatabase.cs @1f594e320 L18-60
Xcc 数据库两件:local(mix 内嵌的文件名表,48 字节头 + count + NUL 串)
与 global(重复块 [int32 count + (name\0 comment\0)*] 至流尾)。解析逐
语义;Local 另带 Data() 写出器(上游同),供 mix 夹具构造与 oracle 对拍。
The two Xcc databases: local (the filename table embedded in a mix — a
48-byte header + count + NUL strings) and global (repeated blocks of
[int32 count + (name\0 comment\0)*] until end of stream). Parsing is
statement-by-statement; Local also carries the Data() writer (as
upstream), serving mix-fixture construction and the oracle differential.

## src/formats/xor_delta.cpp

UPSTREAM: OpenRA.Mods.Cnc/FileFormats/XORDeltaCompression.cs @1f594e320 L16-82
实现文件:六 case 逐句照抄;byte[] → span。
Implementation file: the six cases copied statement by statement;
byte[] → span.

## src/formats/xor_delta.hpp

UPSTREAM: OpenRA.Mods.Cnc/FileFormats/XORDeltaCompression.cs @1f594e320 L14-82(全文)
与既有数据异或合并的增量流(aka Format40)。六 case 逐分支照抄:
case1/2 跳过;case3/5 逐字节 XOR;case4/6 单值 XOR。终止 = 0x80 0x00
0x0000(word 0)。SHPTD 的 XORPrev/XORLCW 帧引用它。
形态适配:byte[] → span;dest 写越界处统一 runtime_error(上游抛
IndexOutOfRangeException,坏数据专用路径)。
A delta stream XORed into existing data (aka Format40). The six cases
are copied branch for branch: cases 1/2 skip; cases 3/5 XOR byte by
byte; cases 4/6 XOR with a single value. Termination = 0x80 0x00 0x0000
(a zero word). SHPTD's XORPrev/XORLCW frames reference it. Shape
adaptation: byte[] → span; out-of-bounds dest writes throw
std::runtime_error uniformly (upstream throws IndexOutOfRangeException;
a malformed-data only path).

## src/fs/d2k_sound_resources.cpp

UPSTREAM: OpenRA.Mods.D2k/PackageLoaders/D2kSoundResources.cs @1f594e320
         L20-94(全文逐语义)

## src/fs/d2k_sound_resources.hpp

UPSTREAM: OpenRA.Mods.D2k/PackageLoaders/D2kSoundResources.cs @1f594e320
         L20-94(全文逐语义)
d2k 声音资源包(.rs):u32 头长界定目录区,条目 = ASCIIZ 名 + u32 偏移 +
u32 长度;内容按文件绝对偏移切取。OpenPackage 上游即 "Not implemented"
(返回 null)。
The d2k sound-resources package (.rs): a u32 header length bounds the
directory area; entries = an ASCIIZ name + u32 offset + u32 length, with
content sliced at absolute file offsets. OpenPackage is upstream's
"Not implemented" (returns null).

## src/fs/file_system.cpp

UPSTREAM: OpenRA.Game/FileSystem/FileSystem.cs @1f594e320 L20-303(逐语义重写)
全文件仅经 file_system.hpp 的 import std; 引入标准库(工程门禁:
禁止 #include <标准头>)。
All standard-library entities come via import std; in file_system.hpp
(project gate: no #include <std headers>).

## src/fs/file_system.hpp

UPSTREAM: OpenRA.Game/FileSystem/FileSystem.cs @1f594e320 L20-303(逐语义重写)
         (IReadOnlyFileSystem 同文件 L20-27)
挂载顺序 = 覆盖优先级:后挂载的包在文件索引中排在列表尾部,查找取最后一个
包含该文件的包(GetFromCache 的 LastOrDefault,L193-199)。
Mount order is override priority: later-mounted packages append to the tail
of each per-file package list, and lookup takes the LAST package containing
the file (GetFromCache's LastOrDefault, L193-199).
已登记偏离(docs/COVERAGE.md)/ Registered deviations (docs/COVERAGE.md):
 1. '$' mod 引用挂载与 installedMods/Manifest 依赖推迟 Phase 2
    the '$' mod-reference mount and the installedMods/Manifest dependency
    are deferred to Phase 2;
 2. ResolveCaseInsensitivePath 基于 std::filesystem 枚举等价实现
    ResolveCaseInsensitivePath is reimplemented equivalently over
    std::filesystem enumeration.

## src/fs/folder.cpp

UPSTREAM: OpenRA.Game/FileSystem/Folder.cs @1f594e320 L19-110(逐语义重写)
标准库一律经 folder.hpp 的 import std;(工程门禁:禁止 #include <标准头>)
All standard-library entities come via import std; in folder.hpp (project
gate: no #include <std headers>).

## src/fs/folder.hpp

UPSTREAM: OpenRA.Game/FileSystem/Folder.cs @1f594e320 L19-110(逐语义重写)
目录包:Contents 顺序影响哈希(上游注释 L34),此处按字节序排序——
mods 的 ASCII 文件名集合下与 C# .Order()(culture 序)等价,已登记 COVERAGE。
Directory package: the Contents order matters for hashing (upstream comment
L34); we sort bytewise — equivalent to the C# .Order() (culture order) over
the ASCII filename set of the mods, as registered in COVERAGE.

## src/fs/i_package.hpp

UPSTREAM: OpenRA.Game/FileSystem/IPackage.cs @1f594e320 L18-42(接口逐语义移植)
包(Package)= 文件系统的一个可挂载单元:目录(Folder)、zip、mix……
A "package" is one mountable unit of the virtual file system: a directory
(Folder), a zip, a mix, ...
设计决策(docs/COVERAGE.md):C# GetStream 返回惰性 Stream,此处返回
std::optional<std::vector<char>>(立即读全)——上游消费方均为全量读取,
语义等价且免去流生命周期管理。
Design decision (docs/COVERAGE.md): the C# GetStream returns a lazy Stream;
here it returns std::optional<std::vector<char>> (read in full) — every
upstream consumer reads the stream through, so this is semantically
equivalent and avoids stream lifetime management.

## src/fs/mix_file.cpp

UPSTREAM: OpenRA.Mods.Cnc/FileSystem/MixFile.cs @1f594e320 L24-248(逐语义重写)

## src/fs/mix_file.hpp

UPSTREAM: OpenRA.Mods.Cnc/FileSystem/MixFile.cs @1f594e320 L24-248(逐语义重写)
mix 包:三格式(C&C 无标志头 / RA-TS 带 u16 标志 / 加密头 = 80 字节
密钥块 RSA 解密 + Blowfish 块解密)+ 哈希 → 文件名解析(local mix
database.dat 内嵌库 + global 库双哈希命中计数择优)。
形态适配(D 登记):字节全量驻留(GetStream 返回整读向量 = i_package
契约);条目序 = 上游 Dictionary/HashSet 序,本就非契约(进程随机),
Contents 顺序同样不作承诺。
The mix package: three formats (the C&C flagless header / the RA-TS u16
flags / the encrypted header = the 80-byte keyblock RSA-decrypted +
Blowfish block-decrypted) + hash → filename resolution (the embedded
"local mix database.dat" + the global database, the better-scoring of the
Classic/CRC32 hit counts). Shape adaptations (registered as deviations):
bytes resident in full (GetStream returns whole-read vectors per the
i_package contract); entry ordering mirrors the upstream Dictionary/
HashSet orders, which were never contractual (process-randomized) —
Contents order is likewise unspecified.

## src/fs/package_entry.cpp

UPSTREAM: OpenRA.Mods.Cnc/FileSystem/PackageEntry.cs @1f594e320 L21-117

## src/fs/package_entry.hpp

UPSTREAM: OpenRA.Mods.Cnc/FileSystem/PackageEntry.cs @1f594e320 L21-117
mix 目录项 + 双哈希:Classic(RA1/TD 的 rotate-add 32 位)与 CRC32(TS)。
HashFilename 逐语义:大写化 + 4 字节对齐填充;Classic 填 NUL,CRC32 填
(length % 4) 字节值并复制尾对齐字节。上游 ToString 的 Names 反查表仅
调试用,未移植(不可观测)。已登记偏离:哈希域 = ASCII 文件名(上游
.NET ToUpperInvariant + ASCII 编码把非 ASCII 折为 '?',且按 UTF-16 字符
计长;真实 mix 文件名全为 ASCII,两者等价)。
The mix directory entry + its two hashes: Classic (RA1/TD's rotate-add
32-bit) and CRC32 (TS). HashFilename statement-by-statement: uppercased +
padded to a 4-byte boundary; Classic pads NUL, CRC32 pads the (length % 4)
byte value and replicates the last aligned byte. The upstream ToString's
Names reverse table is debug-only and not ported (unobservable).
Registered deviation: the hash domain is ASCII filenames (upstream's .NET
ToUpperInvariant + ASCII encoding folds non-ASCII to '?', and lengths
count UTF-16 chars; real mix filenames are all ASCII, so the two agree).

## src/fs/yaml_load.cpp

UPSTREAM: OpenRA.Game/MiniYaml.cs @1f594e320 L684-698(Load 逐语义移植;
与 FileSystem 同处一编译单元以解 yaml→fs 的依赖方向)
The Load entry point ported statement-by-statement (kept in the fs
translation unit so the yaml→fs dependency stays one-directional).
mapRules.Value 的附加文件列表按 FieldLoader.ParseArray 语义解析
(FieldLoader.cs L593-617:逗号分割 + 逐段 trim + 去空段)。
The extra file list in mapRules.Value parses with FieldLoader.ParseArray
semantics (FieldLoader.cs L593-617: comma split + per-part trim + drop
empty parts).

## src/fs/zip_file.cpp

UPSTREAM: OpenRA.Game/FileSystem/ZipFile.cs @1f594e320 L20-261(只读路径逐语义重写)

## src/fs/zip_file.hpp

UPSTREAM: OpenRA.Game/FileSystem/ZipFile.cs @1f594e320 L20-261(只读路径逐语义重写)
ReadWriteZipFile(更新/删除条目)推迟到 Phase 6 地图保存(miniz writer 实现),
已登记 docs/COVERAGE.md。
The read-only paths are rewritten statement-by-statement; ReadWriteZipFile
(entry update/delete) is deferred to Phase 6 map saving (via the miniz
writer), as registered in docs/COVERAGE.md.
SharpZipLib → miniz:条目名按 zip 内原始存储('/' 分隔);大小写敏感匹配
与 SharpZipLib GetEntry 一致。
SharpZipLib → miniz: entry names are the raw stored names ('/'-separated);
case-sensitive lookup matches SharpZipLib GetEntry.

## src/game/actor_info.cpp

UPSTREAM: OpenRA.Game/GameRules/ActorInfo.cs @1f594e320(actor_info.hpp
         的实现;LoadTraitInfo/拓扑序逐语义,见 hpp 头注)
         Implementation of actor_info.hpp — LoadTraitInfo and the
         topological order verbatim; see the hpp header notes.

## src/game/actor_info.hpp

UPSTREAM: OpenRA.Game/GameRules/ActorInfo.cs @1f594e320 L18-201(逐语义重写)
         + OpenRA.Game/Primitives/TypeDictionary.cs L30-141(本文件所需的
         最小子集:接口名过滤查询)
         Full rewrite of ActorInfo.cs + the minimal TypeDictionary subset
         needed here (interface-name filtered queries).

机制对照 / Mechanism mapping:
 - traits 存储:C# TypeDictionary(接口类型键 + List)→ 此处以
   vector<unique_ptr<RecordObject>> + 接口名线性过滤(RecordDesc::
   interfaces);Phase 3 的 TraitDictionary(接口 ID + 平行数组二分)落地时
   替换存储
   Trait storage: the C# TypeDictionary (interface-type keys + lists)
   becomes a vector of Records plus linear interface-name filtering
   (RecordDesc::interfaces); the Phase 3 TraitDictionary (interface IDs +
   parallel-array bisection) replaces the storage when it lands.
 - PrerequisitesOf/OptionalPrerequisitesOf(L169-185):反射扫 Requires<>/
   NotBefore<> 泛型接口 → RecordDesc::requires_types/not_before_types
   (schema_dumper 导出)
   PrerequisitesOf/OptionalPrerequisitesOf (L169-185): the reflection scan
   for the Requires<>/NotBefore<> generic interfaces → the exported
   requires_types/not_before_types.
 - InstanceName:C# 反射写字段([FieldLoader.Ignore]);值袋侧无该字段,
   与 trait 并列存放(instance 视图;dump 输出用)
   InstanceName: upstream writes it via reflection (a [FieldLoader.Ignore]
   field); the value bag has no such field, so it sits alongside the trait
   (an instance view; used by dump output).

## src/game/fonts.cpp

UPSTREAM: OpenRA.Game/Fonts.cs @1f594e320(fonts.hpp 的实现)
The implementation of fonts.hpp.

## src/game/fonts.hpp

UPSTREAM: OpenRA.Game/Fonts.cs @1f594e320 L15-38(全文)。Fonts =
IGlobalModData,FontList 由 LoadUsing(LoadFonts) 从 mod.yaml "Fonts" 节点
逐键装载(FontData = FieldLoader.Load 三字段)。C# FrozenDictionary →
插入序 vector<pair>;GetOrCreate 的 C++ 承载 = ModData::GetOrCreateFonts
(mod_data.hpp,MergedNode("Fonts") 合并树优先,缺省空表)。
The whole of Fonts.cs L15-38. Fonts is an IGlobalModData whose FontList
loads through LoadUsing(LoadFonts) from the mod.yaml "Fonts" node, one
FontData per key (FieldLoader.Load's three fields). The C#
FrozenDictionary becomes an insertion-ordered vector<pair>; the
GetOrCreate carrier is ModData::GetOrCreateFonts (mod_data.hpp, the
MergedNode("Fonts") merged tree first, the empty table as the lazy
default).

## src/game/game_records.cpp

UPSTREAM: OpenRA.Game/GameRules/WeaponInfo.cs @1f594e320(game_records.hpp
         的实现;WeaponInfo/SoundInfo/MusicInfo,见 hpp 头注)
         Implementation of game_records.hpp — WeaponInfo/SoundInfo/
         MusicInfo; see the hpp header notes.

## src/game/game_records.hpp

UPSTREAM: OpenRA.Game/GameRules/WeaponInfo.cs @1f594e320 L74-175(加载链部分)+
         SoundInfo.cs L19-77 + MusicInfo.cs L16-47
         The loading-chain parts of WeaponInfo/SoundInfo/MusicInfo.

机制对照 / Mechanism mapping:
 - WeaponInfo:手写 RecordObject(非 601 生成集;两处 LoadUsing 的
   LoadProjectile/LoadWarheads 按上游逐语义注册 —— Projectile = 键 "Projectile"
   值节点的 "{值}Info" 工厂产物;Warheads = 键前缀 "Warhead" 的 "{值}Warhead"
   工厂产物,L147-175)
   WeaponInfo: a hand-written RecordObject (outside the generated 601;
   the two LoadUsing loaders LoadProjectile/LoadWarheads registered
   verbatim — Projectile = the "{value}Info" factory product of the
   "Projectile" key's value node; Warheads = the "{value}Warhead" factory
   products of keys prefixed "Warhead", L147-175).
 - SoundInfo:手写 RecordObject + ORA_FIELD 描述表(Lazy 音频池字段
   [VoicePools/NotificationsPools] 上游无解析器,描述表省略 = 等价跳过)
   SoundInfo: a hand-written RecordObject + ORA_FIELD descriptors (the
   Lazy audio-pool fields [VoicePools/NotificationsPools] have no upstream
   parser; omitting them from the table skips identically).
 - MusicInfo:上游不走 FieldLoader(手写从 yaml 读 Title/Hidden/…),
   同样手写
   MusicInfo: upstream bypasses FieldLoader (hand-reads
   Title/Hidden/…); likewise hand-written here.

## src/game/game_speed.cpp

UPSTREAM: OpenRA.Game/GameSpeed.cs @1f594e320(game_speed.hpp 的实现)
         The implementation of game_speed.hpp.

## src/game/game_speed.hpp

UPSTREAM: OpenRA.Game/GameSpeed.cs @1f594e320 L17-62(逐语义重写)
         Verbatim-semantics rewrite.

机制对照 / Mechanism mapping:
 - GameSpeed 的 [FieldLoader.Require] → 缺字段 MissingFieldsException 等价
   (LoadSpeeds 的包装异常文本逐字)
   The [FieldLoader.Require] of GameSpeed → the MissingFieldsException
   equivalent (LoadSpeeds' wrapper exception text verbatim).

## src/game/graphics_boot.cpp

UPSTREAM: OpenRA.Game/Renderer.cs L88-122 + Game.cs L827-829/L901-904
         (graphics_boot.hpp 的实现)。

## src/game/graphics_boot.hpp

UPSTREAM: OpenRA.Game/Renderer.cs @1f594e320 L88-122(Renderer ctor 的
         GraphicSettings 消费:GetResolution + CreateWindow 六参)+
         OpenRA.Game/Game.cs L827-829(Loop 的帧率帽)+ L901-904(Run 的
         MaxFramerate<1 修正)+ Sdl2PlatformWindow.cs L173-174/234
         (videoDisplay 域校验与居中)。
         GraphicSettings' startup consumption distilled for the embedder:
         GetResolution + the CreateWindow six parameters, the Loop
         framerate cap, and Run's MaxFramerate<1 fixup, plus the
         videoDisplay domain check and centering.

形态适配:C++ 无 Game.Initialize 单体装配 —— 本文件是调用方(未来 main/
测试)从 Settings 到 Sdl2Window::Desc 的装配纯函数族。GLProfile 的
Automatic/ANGLE 档映射 Modern(C++ 窗口域无 ANGLE 探测链;上游
supportedProfiles 回退 supported[0] 的简化;登记 COVERAGE)。
Shape adaptation: C++ has no monolithic Game.Initialize assembly — this
file is the caller's (the future main/tests) pure-function family mapping
Settings onto Sdl2Window::Desc. GLProfile's Automatic/ANGLE tiers map to
Modern (the C++ window domain has no ANGLE probe chain; a simplification
of upstream's supportedProfiles[0] fallback; registered in COVERAGE).

## src/game/load_screen.hpp

UPSTREAM: OpenRA.Game/ModData.cs @1f594e320 L236-252

## src/game/loaders.cpp

UPSTREAM: OpenRA.Game/FieldLoader.cs @1f594e320(24 个 [FieldLoader.LoadUsing]
         加载器的逐语义移植,schema_dumper --scan 清单;各 loader 的源文件
         行号在对应函数注记):
         The verbatim-semantics port of all 24 [FieldLoader.LoadUsing]
         loaders (the schema_dumper --scan list):
 - Locomotor.cs LoadSpeeds ×4(Locomotor/Subterranean/Jumpjet;10000/speed
   整除、PathingCost 覆盖、speed≤0 跳过)
 - Building.cs LoadFootprint ×3(Building/EnergyWall/D2kBuilding)
 - ResourceLayer.cs LoadResourceTypes ×8(各 ResourceTypeInfo 记录)
 - SmudgeLayer.cs LoadInitialSmudges(逐项 try-catch 吞错)
 - HitShape.cs LoadShape(缺省 CircleShape;异常包 "HitShape {shape}: ")
 - SupportPowerBotModule.cs LoadDecisions
 - MapGeneratorBase.cs LoadOptions/LoadFluentReferences ×10(4 类选项;
   Fluent = Label + MultiChoice 的 Choice Labels)
 - WeaponInfo.cs LoadProjectile/LoadWarheads(手写类路径,void* 注册表)

值袋承载 / Value-bag carriers:
 - TerrainInfo/(ResourceTypeInfo/SupportPowerDecision/IHitShape/Option)等
   嵌套记录 → GenericValue 的 rec 槽(GeneratedRecord + gen/ 描述表)
 - 元组型载荷(TerrainInfo.Cost/Speed、MapSmudge.Type/Depth)→ GenericTuple /
   字符串+int 混合 → 专用槽序(见各 loader 注释;dump 协议同步约定)

## src/game/manifest.cpp

UPSTREAM: OpenRA.Game/Manifest.cs @1f594e320(manifest.hpp 的实现;
         Include 展开/Merge/各节解析,见 hpp 头注)
         Implementation of manifest.hpp — the Include expansion/Merge/
         section parsing; see the hpp header notes.

## src/game/manifest.hpp

UPSTREAM: OpenRA.Game/Manifest.cs @1f594e320 L21-206(逐语义重写)
         Full statement-by-statement rewrite of upstream Manifest.cs.

机制对照 / Mechanism mapping:
 - Include 展开(L98-111):倒序就地替换为文件内容
   The Include expansion (L98-111): in-place replacement by file contents
   in reverse order.
 - Merge + ToDictionary(L114):继承覆盖合并后的节点字典
   Merge + ToDictionary (L114): the node dictionary after inheritance
   merging.
 - ModMetadata(RendererConstants)经 FieldLoader 加载(手写类 + ORA_FIELD
   描述表;ModMetadata.cs L21-38 / L40-48)
   ModMetadata (RendererConstants) loads through FieldLoader (hand-written
   classes + ORA_FIELD descriptor tables).
 - 持有性:Manifest 拥有整个 MiniYaml 节点树(视图在其存活期内稳定;
   上游 string 池化等价物 = 节点树内 const std::string*)
   Ownership: the Manifest owns the whole MiniYaml tree (views stay valid
   for its lifetime; the upstream pooled strings = the in-tree
   const std::string*).

## src/game/mod_credits.cpp

UPSTREAM: OpenRA.Mods.Common/ModCredits.cs @1f594e320(值面装载)。

## src/game/mod_credits.hpp

UPSTREAM: OpenRA.Mods.Common/ModCredits.cs @1f594e320 L12-23(全文)+
         ModData.cs L71-95 的 GlobalModData 装载链对 ModCredits 的投影
         (FieldLoader.Load 三字段;缺省 = CreateBasic)。
         The whole ModCredits value face + the ModCredits projection of
         ModData.cs L71-95's GlobalModData loading chain
         (FieldLoader.Load over the three fields; the default is the
         CreateBasic form).

## src/game/mod_data.cpp

UPSTREAM: OpenRA.Game/ModData.cs @1f594e320(mod_data.hpp 的实现;
         Phase 2 最小集挂载链,见 hpp 头注)
         Implementation of mod_data.hpp — the Phase 2 minimal mount
         chain; see the hpp header notes.

## src/game/mod_data.hpp

UPSTREAM: OpenRA.Game/ModData.cs @1f594e320 L29-233(Phase 2 最小集)+
         OpenRA.Mods.Common/FileSystem/{Default,ContentInstallerFileSystem}Loader.cs
         (FileSystem 节点的挂载语义)
         The Phase 2 minimal ModData + the mount semantics of the
         FileSystem-section loaders.

Phase 2 范围裁剪(其余成员随各阶段落地)/ Phase 2 scope (the remaining
members land with their phases):WidgetLoader/MapCache/各格式 Loader/
Hotkeys/Cursors/Fluent/LoadScreen/GlobalModData 属 Phase 4-6。
WidgetLoader/MapCache/format loaders/Hotkeys/Cursors/Fluent/LoadScreen/
GlobalModData belong to Phases 4-6.

## src/game/platform.cpp

UPSTREAM: OpenRA.Game/Platform.cs @1f594e320(platform.hpp 的实现;
         ResolvePath 逐语义,见 hpp 头注)
         Implementation of platform.hpp — the verbatim ResolvePath; see
         the hpp header notes.

## src/game/platform.hpp

UPSTREAM: OpenRA.Game/Platform.cs @1f594e320 L288-312(ResolvePath)+
         OpenRA.Game/Game.cs L98-110(EngineDir/BinDir/SupportDir 常量来源)
         ResolvePath + the EngineDir/BinDir/SupportDir constants.

'^EngineDir|^SupportDir|^BinDir' 前缀替换;引擎目录由宿主注入(上游 = 编译期
常量目录,工具/测试以仓库根/工作目录代入)。
The '^EngineDir|^SupportDir|^BinDir' prefix rewriting; the engine directory
is injected by the host (upstream = compile-time constant directories;
tools/tests substitute the repository root / working directory).

## src/game/ruleset.cpp

UPSTREAM: OpenRA.Game/GameRules/Ruleset.cs @1f594e320(ruleset.hpp 的实现;
         MergeOrDefault/SystemActors 补齐逐语义,见 hpp 头注)
         Implementation of ruleset.hpp — MergeOrDefault and the
         SystemActors backfill verbatim; see the hpp header notes.

## src/game/ruleset.hpp

UPSTREAM: OpenRA.Game/GameRules/Ruleset.cs @1f594e320 L23-281(逐语义重写)+
         OpenRA.Game/Primitives/ActorInfoDictionary.cs L18-55(SystemActors 补齐)
         Full rewrite of Ruleset.cs + the ActorInfoDictionary SystemActors
         backfill.

已登记偏离(docs/COVERAGE.md)/ Registered deviations:
 - IRulesetLoaded 回调(actor/warhead/projectile 的 RulesetLoaded(this,…))
   属行为面:生成类型无行为,Phase 5 起随各 trait 落地;对拍不受影响
   (回调只做跨引用校验与非 yaml 属性缓存,默认规则集全部通过)
   The IRulesetLoaded callbacks (RulesetLoaded(this,…) of actors/warheads/
   projectiles) are behaviour: generated types carry none; they land trait
   by trait from Phase 5. Dumps are unaffected (the callbacks only do
   cross-reference validation and non-yaml property caching, all of which
   pass on the default rulesets).
 - ToDictionaryWithConflictLog 的重复键 debug 日志不落地(保留首个的行为
   一致;日志属 debug 输出面)
   The duplicate-key debug logging of ToDictionaryWithConflictLog is not
   replicated (first-wins behaviour identical; the log is debug output).
 - TerrainInfo/ITerrainInfo:Phase 4 随地形格式;LoadDefaults 传空
   TerrainInfo/ITerrainInfo: arrives with the terrain formats in Phase 4;
   LoadDefaults passes empty.

## src/game/settings.cpp

UPSTREAM: OpenRA.Game/Settings.cs @1f594e320 L52-340(实现部分;头注见
         settings.hpp)
         The implementation half of Settings.cs L52-340 (the header note
         lives in settings.hpp).

## src/game/settings.hpp

UPSTREAM: OpenRA.Game/Settings.cs @1f594e320 L52-340 的起步承载面(
         SettingsModule 的 Commit 差异保存协议 + PlayerSettings/
         GameSettings/GraphicSettings 三段手写字段表;其余段与 UI 绑定面
         随 Phase 6/7)
         The starter face of Settings.cs L52-340 (SettingsModule's
         save-differences Commit protocol + the hand-written field tables
         of PlayerSettings/GameSettings/GraphicSettings; the remaining
         sections and the UI bindings ride Phase 6/7).

机制对照 / Mechanism mapping:
 - Settings 具体类的反射字段 → 手写 RecordDesc 描述表(值袋承载,
   GeneratedRecord;schema_dumper 的 Settings 扩展随需再生成 —— 登记)
   The concrete Settings classes' reflective fields → the hand-written
   RecordDesc tables (a value-bag GeneratedRecord; schema_dumper's
   Settings export regenerates on demand — registered).
 - Commit 的"默认值不存 + 擦旧再增"(L69-80)→ 差异节点重建(等价:
   节点集 = 差异集;键序 = 字段序)
   Commit's "defaults unsaved + remove-then-add" (L69-80) → the
   difference-node rebuild (equivalent: the node set = the difference
   set; the key order = the field order).

## src/game/web_services.cpp

UPSTREAM: OpenRA.Mods.Common/WebServices.cs @1f594e320 L15-60 +
OpenRA.Game/Support/HttpQueryBuilder.cs L16-59(实现半;契约半见头文件)
The implementation half (the contract half is in the header).

## src/game/web_services.hpp

UPSTREAM: OpenRA.Mods.Common/WebServices.cs @1f594e320 L15-60 +
OpenRA.Game/Support/HttpQueryBuilder.cs L16-59(值面 + CheckModVersion)
五服务 URL 的值面(上游 readonly 字面量;此处可注入覆写 = 测试/宿主
面)+ CheckModVersion 的三态查询(GET {VersionCheck}?protocol=1&engine=
&mod=&version= → "outdated"/"unknown"/"playtest"/其他=Latest);上游
Task.Run + Game.RunAfterTick 的赋值序 → detach 线程 + 可注入 sink(缺
省 = 立即置,观察序等价)。
The five-service-URL value face (upstream's readonly literals; here
injectable for override = the test/host face) + CheckModVersion's
three-state query (GET {VersionCheck}?protocol=1&engine=&mod=&version=
→ "outdated"/"unknown"/"playtest"/anything-else=Latest); upstream's
Task.Run + Game.RunAfterTick assignment ordering → a detached thread +
an injectable sink (defaulting to immediate assignment, observably
equivalent ordering).

## src/gfx/animation.cpp

UPSTREAM: OpenRA.Game/Graphics/Animation.cs @1f594e320 L21-261 +
          OpenRA.Game/Graphics/AnimationWithOffset.cs @1f594e320 L17-60
         (animation.hpp 的实现;头注的形态适配说明适用)
         Implementation of animation.hpp; the shape-adaptation notes of
         the hpp header apply.

## src/gfx/animation.hpp

UPSTREAM: OpenRA.Game/Graphics/Animation.cs @1f594e320 L21-261(全文逐语义)+
          OpenRA.Game/Graphics/AnimationWithOffset.cs @1f594e320 L17-60(全文)
动画状态机:Play*(五族 — Repeating/Then/BackwardsThen/FetchIndex/FetchDirection)
装配 tickFunc 委托,Tick(t) 的 timeUntilNextFrame 负债循环(while <= 0 追帧)
与 tickAlways 旁路逐句照抄;CurrentFrame 的 backwards 反转索引;Render 家
族的 shadow 两件套(高度 = map.DistanceAboveTerrain(pos).Length 的地面下
投 + ShadowZOffset)与 RenderUI 的 int2.FromVector/(int) 截断算式逐句。
形态适配:
  - ctor(World,…) → Deps 注入(sequences + 地形高度查询;上游 world.Map
    的两依赖拆出 —— Map 随 Phase 5,默认高度面 = WDist 0);
  - tickFunc 闭包族 → TickMode 枚举 + after/fetch/direction 三回调(语义
    一一对应,Play* 时装配);
  - IRenderable[] 返回 → out 数组(≤ 2 项:shadow + image);
  - RandomOrDefault 的 r.Next 可负(上游 ElementAt 负索引抛
    ArgumentOutOfRangeException)—— 等价抛;.NET 消息依版本,取 .NET 10
    形态文本。
The animation state machine: the Play* family (five flavors — Repeating/
Then/BackwardsThen/FetchIndex/FetchDirection) assembles the tickFunc
delegate, with Tick(t)'s timeUntilNextFrame debt loop (the while <= 0
catch-up) and the tickAlways bypass copied statement by statement;
CurrentFrame's backwards index reversal; the Render family's shadow pair
(the height = map.DistanceAboveTerrain(pos).Length ground projection +
ShadowZOffset) and RenderUI's int2.FromVector/(int)-truncation arithmetic
verbatim. Shape adaptations:
  - ctor(World,…) → the Deps injection (sequences + the terrain-height
    query; the two dependencies upstream pulls from world.Map split out —
    Map arrives in Phase 5, the default height face = WDist 0);
  - the tickFunc closure family → the TickMode enum plus the three
    after/fetch/direction callbacks (one-to-one semantics, assembled at
    Play*);
  - the IRenderable[] returns → out arrays (at most 2 items: shadow +
    image);
  - RandomOrDefault's r.Next can go negative (upstream's ElementAt throws
    ArgumentOutOfRangeException on the negative index) — thrown
    equivalently; the .NET message is version-dependent, taking the .NET
    10 shape.

## src/gfx/chrome_provider.cpp

UPSTREAM: OpenRA.Game/Graphics/ChromeProvider.cs @1f594e320 L22-305
         (chrome_provider.hpp 的实现;头注的形态适配说明适用)
         Implementation of chrome_provider.hpp; the shape-adaptation notes
         of the hpp header apply.

## src/gfx/chrome_provider.hpp

UPSTREAM: OpenRA.Game/Graphics/ChromeProvider.cs @1f594e320 L22-305(全文逐语义)
UI 皮肤(chrome.yaml):Collection(Image/Image2x/Image3x + PanelRegion 八元 +
PanelSides 标志 + Regions 名→矩形表)。HiDPI 三档(dpiScale > 2 → 3x,> 1 →
2x,缺省原图);sheet 按图片名共享缓存,Sprite 按 density 缩放(1/density 的
scale + density × 矩形)。面板九宫格:PanelRegion 存在时按 3×3 切角/边/心
(PanelSides 缺失侧 → null 槽),否则九个具名 region 查询("corner-tl"…);
均无定义 = 空数组(上游 PERF 注释照抄)。
形态适配:
  - 上游 static 类 + Game.ModData 全局 → 实例类(测试/多实例可注入;Phase 5
    的 Ui.Initialize 持单例 —— 形态等价,COVERAGE 登记);
  - FrozenDictionary/Dictionary → 插入序 vector&lt;pair&gt;(错误文本与查询语
    义不变);
  - FieldLoader.Load&lt;Collection&gt; → meta 描述表(ORA_FIELD;Rectangle/Size
    解析随本批 meta 补齐);
  - 上游 Log.Write("debug") 的三条诊断 → stderr(形态偏离,D96 族)。
The UI skin (chrome.yaml): Collection (Image/Image2x/Image3x + the
eight-element PanelRegion + the PanelSides flags + the Regions name →
rectangle table). Three HiDPI tiers (dpiScale > 2 → 3x, > 1 → 2x, else the
base image); sheets share a per-image cache, Sprites scale by density
(a 1/density scale + density × the rectangle). The panel nine-slice: with a
PanelRegion, the 3×3 corners/edges/center (missing PanelSides → null
slots); otherwise nine named-region queries ("corner-tl"...); with no
definitions at all, the empty array (the upstream PERF comment kept).
Shape adaptations:
  - upstream's static class + the Game.ModData global → an instance class
    (injectable for tests/multiple instances; Phase 5's Ui.Initialize holds
    the singleton — shape-equivalent, registered in COVERAGE);
  - FrozenDictionary/Dictionary → insertion-ordered vector<pair> (the error
    texts and query semantics unchanged);
  - FieldLoader.Load<Collection> → the meta descriptor tables (ORA_FIELD;
    Rectangle/Size parsing landed with this batch's meta additions);
  - the three Log.Write("debug") diagnostics → stderr (a shape deviation,
    the D96 family).

## src/gfx/cursor_manager.cpp

UPSTREAM: OpenRA.Game/Graphics/CursorManager.cs @1f594e320 L20-317
         (cursor_manager.hpp 的实现;头注的形态适配说明适用)
         Implementation of cursor_manager.hpp; the shape-adaptation notes
         of the hpp header apply.

## src/gfx/cursor_manager.hpp

UPSTREAM: OpenRA.Game/Graphics/CursorManager.cs @1f594e320 L20-317(逐方法)+
          OpenRA.Game/ModData.cs @1f594e320 L135-149(ParseCursors 的装载面)
光标管理:cursors.yaml 的序列 → 帧切取(Start/Length 两上界检查,YamlException
文本逐字)→ 热点居中换算(hotspot = FromVector(f.Offset) - Hotspot -
size/2)→ Indexed8 经 ConvertIndexedToBgra 解色 → SheetBuilder.Add(热点为
偏移)→ Bounds 并集 → 8 倍数 PaddedSize(三平台怪癖的上游注释照抄)。
硬件光标:每帧 paddingTL/BR 对齐后 CreateHardwareCursor(软光标共享同 sheet);
Tick 的 3 tick 步进换帧 + CursorDouble 变更重建;Lock/Unlock 的相对鼠标
模式 + lockedPosition。
形态适配:
  - Game.Settings.Graphics → 两 live 查询注入;Game.Renderer/Window →
    Renderer*/Sdl2Window* 注入(空 = 上游 Utility 的 Game.Renderer == null
    路径:不建硬件光标);Viewport.LastMousePos → 查询注入(Phase 5);
  - ModData.Cursors 表 + IProvidesCursorPaletteInfo 调色板面 → 构造注入
    (光标序列表 + 名字 → ImmutablePalette 解析器;调色板缓存内置);
  - IHardwareCursor[] → optional&lt;Sdl2HardwareCursor&gt; 数组(上游 null 槽
    = 建败/禁用);
  - 上游 Dispose 的 GC 终结路径 → 析构。
The cursor manager: cursors.yaml sequences → frame slicing (the Start/
Length upper-bound checks with YamlException texts verbatim) → the
hotspot-centering conversion (hotspot = FromVector(f.Offset) - Hotspot -
size/2) → Indexed8 resolved through ConvertIndexedToBgra →
SheetBuilder.Add (the hotspot as the offset) → the Bounds union → the
multiple-of-8 PaddedSize (the upstream comment on the three platform
quirks kept verbatim). Hardware cursors: per-frame paddingTL/BR alignment
then CreateHardwareCursor (the software cursor shares the same sheet);
Tick's 3-tick frame stepping + the CursorDouble rebuild; Lock/Unlock's
relative mouse mode + lockedPosition. Shape adaptations: Game.Settings.
Graphics → two live-query injections; Game.Renderer/Window → Renderer*/
Sdl2Window* injections (null = upstream's Game.Renderer == null Utility
path: no hardware cursors); Viewport.LastMousePos → a query injection
(Phase 5); the ModData.Cursors table + the IProvidesCursorPaletteInfo
palette face → constructor injection (the cursor-sequence table + a name →
ImmutablePalette resolver; the palette cache built in); IHardwareCursor[]
→ an optional<Sdl2HardwareCursor> array (upstream's null slots =
build-failure/disabled); upstream's Dispose-GC-finalizer path → the
destructor.

## src/gfx/cursor_sequence.hpp

UPSTREAM: OpenRA.Game/Graphics/CursorSequence.cs @1f594e320 L14-52(全文逐语义)
cursors.yaml 的单条光标序列:Name/Src(来自文件级上下文)+ X/Y 热点
(TryParseInt32Invariant,失败静默保持 0 的上游怪癖)+ Start(必填,缺键 =
KeyNotFound 等价抛)+ Length 的三态怪癖照抄 —— Length 键存在且值 != "*" =
数值;值为 "*" = null(至序列尾);End 键存在且值恰为 "*" 时上游的三元
表达式恒取 null(死分支保真);其余 = 1。
The single cursor sequence of cursors.yaml: Name/Src (from the file-level
context) + the X/Y hotspot (TryParseInt32Invariant with upstream's
silently-keep-0-on-failure quirk) + Start (required; a missing key = the
KeyNotFound-equivalent throw) + Length's three kept states — a Length key
with a value != "*" parses; the value "*" means null (through the sequence
end); an End key whose value is exactly "*" takes upstream's always-null
ternary (the dead branch preserved); everything else = 1.

## src/gfx/frame_buffer.cpp

UPSTREAM: OpenRA.Platforms.Default/FrameBuffer.cs @1f594e320 L19-157(实现)
头文件携带完整 UPSTREAM 锚点与偏离登记。
Implementation of FrameBuffer.cs; the header carries the full UPSTREAM
anchors and deviation registrations.

## src/gfx/frame_buffer.hpp

UPSTREAM: OpenRA.Platforms.Default/FrameBuffer.cs @1f594e320 L19-157(逐方法)
语义面:颜色纹理 + 深度 renderbuffer 附件、Bind 的 viewport 保存/恢复与
clear(clearColor + COLOR|DEPTH)、Unbind 恢复 viewport、scissor 断言
("Attempting to unbind FrameBuffer with an active scissor region.")、
完整性校验(FRAMEBUFFER_COMPLETE,失败即错)。
偏离(已登记 COVERAGE):
  - OPT-B1(D37):上游 L32-33 的 IsPowerOf2 强制校验不复刻(NPOT 直建);
  - Bind/Unbind 的 glFlush 不复刻 —— 上游为多线程封送正确性而插,本引擎
    命令流天然全序(语义等价);
  - 完整性失败从 throw 改为 stderr + b_invalid 标记(异常面随渲染对拍
    关卡统一;消息文本保留)。
Method-by-method port of FrameBuffer.cs. Semantic plane: the color-texture
plus depth-renderbuffer attachments, Bind's viewport save/restore and clear
(clearColor + COLOR|DEPTH), Unbind's viewport restore, the scissor
assertion ("Attempting to unbind FrameBuffer with an active scissor
region."), and the completeness check (FRAMEBUFFER_COMPLETE or bust).
Deviations (registered in COVERAGE):
  - OPT-B1 (D37): upstream's L32-33 IsPowerOf2 enforcement is not
    replicated (NPOT built directly);
  - the glFlush of Bind/Unbind is not replicated — upstream inserted it for
    multithreaded-marshal correctness, while this engine's command stream
    is totally ordered by construction (semantically equivalent);
  - a completeness failure prints to stderr and flags b_invalid instead of
    throwing (the exception surface settles with the render-differential
    gate; the message text is kept).

## src/gfx/gfx_command.hpp

UPSTREAM: OpenRA.Platforms.Default/ThreadedGraphicsContext.cs @1f594e320 L188-505(命令封送段)
OPT-A5(docs/OPTIMIZATION_TRACKER.md)重设计:上游以 Post(Action<object>, object) 把每个
GL 调用装箱成消息 + 全局锁 + Monitor.Pulse(每帧几十到几百次堆分配),本实现改为
**定长值类型命令 + 内联载荷 blob** 的字节流,SPSC(Lamport)无锁环形缓冲传递:
  - 零装箱、零委托、零锁(仅空/满时的 atomic wait 唤醒);
  - 记录布局 [u32 record_size][对齐垫 4B][GfxCmd 定长 48B][payload…(align8)];
  - 单调递增字节偏移 + 2 的幂容量:写者只发布 head,读者只发布 tail;
  - 环尾放不下整条时写 4B pad 记录跳到环首;
  - 同步往返(读回/资源名)内嵌 binary_semaphore 指针于命令。
上游 Windows 窗口模式"渲染线程禁用"特例不复刻(统一线程模型)。
OPT-A5 (docs/OPTIMIZATION_TRACKER.md) redesign: upstream marshals every GL
call via Post(Action<object>, object) — a boxed message plus a global lock
plus Monitor.Pulse (tens to hundreds of heap allocations per frame); this
implementation replaces it with a byte stream of **fixed-size value-type
commands with inline payload blobs** carried over an SPSC (Lamport)
lock-free ring:
  - zero boxing, zero delegates, zero locks (atomic-wait wakeups only when
    empty/full);
  - record layout [u32 record_size][4-byte alignment pad][fixed 48-byte GfxCmd][payload… (align8)];
  - monotonically increasing byte offsets over a power-of-two capacity: the
    writer publishes only head, the reader only tail;
  - a 4-byte pad record jumps to the ring start when the tail lacks room;
  - synchronous round-trips (readbacks/resource names) embed a
    binary_semaphore pointer in the command.
The upstream Windows-windowed "render thread disabled" special case is not
replicated (unified threading model).

## src/gfx/gfx_util.cpp

UPSTREAM: OpenRA.Game/Graphics/Util.cs @1f594e320 L23-320(渲染域子集;头文件已注记范围)
The rendering-domain subset of Util.cs (scope noted in the header).

## src/gfx/gfx_util.hpp

UPSTREAM: OpenRA.Game/Graphics/Util.cs @1f594e320 L23-320(渲染域子集)+
          OpenRA.Game/Exts.cs @1f594e320 L270-273(NextPowerOf2)+
          OpenRA.Mods.Common/Util.cs @1f594e320 L79-104(面向索引族,
          第十四批随 mods 序列移植)+
          OpenRA.Game/Exts.cs @1f594e320 L77-99(WindingDirectionTest/
          PolygonContains,第十四批随 Viewport 移植)
渲染域工具:四边形索引表、FastCreateQuad(顶点生成 + combined.vert 的
aVertexAttributes 位域打包)、FastCopyIntoChannel(Indexed8→单通道 /
Bgr[a]/Rgb[a]→RGBA 预乘拷贝)、FastCopyIntoSprite(Png→sheet 的预乘
展开,ChromeProvider 的 Sheet(Stream) 路径,随第十三批)、
PremultiplyAlpha(uint 快速整数预乘)、RotateQuadInto(绕中心 x-y 旋转)、
BoundingRectangle、NextPowerOf2、IndexFacing/AngleDiffToStep/
GetInterpolatedFacingRotation(Mods.Common 的面向量化索引)、Lerp、
PolygonContains(回绕数点在多边形内测试)。
同时给出 core::Vector2/Vector3 的渲染运算(vector_n.hpp 注记的
"渲染运算 Phase 4 随 gfx 扩展" 落地点)。
Not ported here: RotateQuad(数组分配版)、PremultipliedColorLerp/FromAngle
(随 WorldRenderer 批次)。
Rendering-domain utilities: the quad-index table, FastCreateQuad (vertex
generation with the aVertexAttributes bitfield packing documented in
combined.vert), FastCopyIntoChannel (Indexed8 → single channel /
Bgr[a]/Rgb[a] → premultiplied RGBA copies), FastCopyIntoSprite (the
Png→sheet premultiplied expansion of ChromeProvider's Sheet(Stream) path,
landed with the thirteenth batch), PremultiplyAlpha (the fast integer
uint premultiply), RotateQuadInto (a rotation about the center in the x-y
plane), BoundingRectangle, NextPowerOf2, the IndexFacing/
AngleDiffToStep/GetInterpolatedFacingRotation family (the facing-quantized
indices of Mods.Common), Lerp, and PolygonContains (the winding-number
point-in-polygon test). Also extends core::Vector2/Vector3 with the
rendering arithmetic (the "rendering math extends with gfx in Phase 4"
note in vector_n.hpp lands here).
Not ported here: RotateQuad (the array-allocating overload) and
PremultipliedColorLerp/FromAngle (the WorldRenderer batch).

## src/gfx/hardware_palette.cpp

UPSTREAM: OpenRA.Game/Graphics/HardwarePalette.cs @1f594e320 L18-161(实现)
Implementations of HardwarePalette (the header carries the full UPSTREAM
anchors, the OPT-A7 dirty-row design, and the OPT-C5 equivalence notes).

## src/gfx/hardware_palette.hpp

UPSTREAM: OpenRA.Game/Graphics/HardwarePalette.cs @1f594e320 L18-161 +
          OpenRA.Game/Traits/IPaletteModifier.cs @1f594e320(接口)
硬件调色板:256×N 纹理(行 0 保留为"无色移占位"—— 上游 PERF 注释保留)+
ColorShifts 浮点纹理(HSV 色移参数,combined.frag 的 rgb2hsv 消费)。
可变调色板经 modifier 每次调整后写回缓冲并上传;ApplyModifiers 末尾把
可变调色板重置回原色(modifier 是无状态逐帧调整)。

OPT-A7(docs/OPTIMIZATION_TRACKER.md)落地点 —— 调色板 dirty 行:
  上游 HardwarePalette.cs L132-154 每次 ReplacePalette/ApplyModifiers/
  Initialize 无条件 CopyBufferToTexture 全量上传(256×H RGBA8 + float 表);
  本实现按调色板行跟踪脏位 —— 单行变化走 SetSubData 逐行增量,高度增长
  或脏行过半(启发式)退回全量 SetData。
  OPT-C5 语义论证:上传字节与落点完全相同(SetSubData 与 SetData 写同一
  mipmap level 0 的同一矩形),渲染结果逐像素一致 —— gfx_test 的 GL 集成
  断言"增量上传读回 == 全量上传读回"逐字节验证。
  ColorShifts 表(2×H float)保持全量上传(整表 ≤ 数百字节,逐行收益为零)。

形态适配:Game.Renderer.Context.CreateTexture() → RenderThread* 注入
(空 = 纯数据模式:脏位跟踪照常,上传跳过;Utility 路径);三 Dictionary
→ std::map(查询按名;遍历仅写互不相交的行,顺序不可观测);IPaletteModifier
的 IReadOnlyDictionary → map 引用直传。ReplacePalette/AddPalette 的
InvalidOperationException → std::runtime_error(消息文本逐字)。
The hardware palette: a 256×N texture (row 0 reserved as the "no color
shift" placeholder — the upstream PERF comment preserved) plus the
ColorShifts float texture (HSV shift parameters consumed by
combined.frag's rgb2hsv). Mutable palettes are adjusted by modifiers,
written back into the buffer, and uploaded; ApplyModifiers resets them
to their original colors afterwards (modifiers are stateless per-frame
adjustments).
OPT-A7 landing — palette dirty rows: upstream's
ReplacePalette/ApplyModifiers/Initialize all upload the full texture
unconditionally (HardwarePalette.cs L132-154); this implementation
tracks per-row dirty bits — single-row changes go through per-row
SetSubData, with height growth or a dirty-majority heuristic falling
back to a full SetData. OPT-C5 argument: the uploaded bytes and their
destinations are identical (SetSubData and SetData write the same rect
of the same mipmap level 0), so the render output is pixel-identical —
asserted byte-for-byte in gfx_test's GL integration ("incremental
upload readback == full upload readback"). The ColorShifts table
(2×H floats) stays a full upload (a few hundred bytes at most; per-row
gains are zero). Shape adaptations: Game.Renderer.Context.CreateTexture()
→ RenderThread* injection (null = the data-only mode: dirty tracking
continues, uploads skip; the Utility path); the three Dictionaries →
std::map (lookups by name; iterations write disjoint rows, so ordering
is unobservable); IPaletteModifier's IReadOnlyDictionary → a map
reference. The InvalidOperationExceptions of ReplacePalette/AddPalette
→ std::runtime_error (message texts verbatim).

## src/gfx/palette.cpp

UPSTREAM: OpenRA.Game/Graphics/Palette.cs @1f594e320 L20-160 +
          OpenRA.Game/Graphics/PaletteReference.cs @1f594e320 L14-31(实现)
Implementations of the palette family (headers in palette.hpp carry the
full UPSTREAM anchors and deviation notes).

## src/gfx/palette.hpp

UPSTREAM: OpenRA.Game/Graphics/Palette.cs @1f594e320 L20-160 +
          OpenRA.Game/Graphics/PaletteReference.cs @1f594e320 L14-31 +
          OpenRA.Game/Graphics/PlayerColorRemap.cs @1f594e320 L18-53
调色板值族:IPalette(uint32 ARGB × 256)、ImmutablePalette(不可变;
流构造的字节形态 = r<<2 | r>>6 高位复制 + remapTransparent→0 +
remapShadow→140<<24)、MutablePalette(可变 + ApplyRemap)、IPaletteRemap。
PaletteReference:名字 + 纹理行索引 + HasColorShift 查询(上游每精灵
查 HardwarePalette 字典;SpriteRenderer 批次的 OPT-A7 热点,届时改造)。
形态适配:
  - 上游 IEnumerable<uint> 构造 → span;
  - 流构造改为字节区间(调用方负责读流,.pal 768B RGB;formats 批接线);
  - GetColor/AsReadOnly 扩展方法 → core::Color::FromArgb / const 引用直返
    (C++ const& 天然只读,ReadOnlyPalette 包装类不需要 —— COVERAGE 登记)。
The palette value family: IPalette (uint32 ARGB × 256), ImmutablePalette
(immutable; the stream constructor's byte semantics = r<<2 | r>>6 high-bit
replication + remapTransparent→0 + remapShadow→140<<24), MutablePalette
(mutable + ApplyRemap), and IPaletteRemap. PaletteReference: name +
texture-row index + the HasColorShift query (upstream queries the
HardwarePalette dictionary per sprite — the OPT-A7 hot spot to be
reworked in the SpriteRenderer batch). Shape adaptations:
  - the IEnumerable<uint> constructor → a span;
  - the stream constructor takes a byte span instead (the caller reads
    the stream; the .pal 768-byte RGB layout wires up with formats);
  - the GetColor/AsReadOnly extension methods → core::Color::FromArgb /
    direct const& returns (a C++ const reference is read-only already —
    the ReadOnlyPalette wrapper is unneeded; registered in COVERAGE).

## src/gfx/projected_cell_region.hpp

UPSTREAM: OpenRA.Game/Map/ProjectedCellRegion.cs @1f594e320 L18-125(全文逐语义)
投影单元格区域:屏幕上的 (U,V) 矩形集合(TopLeft/BottomRight 含端点)。
构造期由 Map 的 MaximumTerrainHeight 与 Height 层换算出"可能投影进本
区域"的地图坐标包围区(mapTopLeft/mapBottomRight);枚举器逐列推进
(MoveNext 的溢列进行/溢行终止逐行)。
形态适配:
  - Map 依赖(Height.Clamp / Grid.MaximumTerrainHeight / Grid.Type)
    → 构造参数(heightOffset 与 heightClamp 由调用方给出 —— 完整 Map
    随 Phase 5,WorldRenderer.TerrainMapSurface 同族注入);
  - IEnumerable&lt;PPos&gt; → 前向迭代器对(begin/end),零分配枚举
    (上游枚举器逐 MoveNext 分配 PPos 装箱的等价值语义)。
The projected-cell region: a rectangular (U,V) collection on screen
(TopLeft/BottomRight inclusive). Construction derives the bounding map
region (mapTopLeft/mapBottomRight) that may project into this region from
the Map's MaximumTerrainHeight and Height layer; the enumerator advances
column by column (MoveNext's column-overflow row step and row-overflow
stop kept line by line). Shape adaptations:
  - the Map dependencies (Height.Clamp / Grid.MaximumTerrainHeight /
    Grid.Type) arrive as constructor parameters (heightOffset and
    heightClamp come from the caller — the full Map is Phase 5, the same
    injection family as WorldRenderer.TerrainMapSurface);
  - IEnumerable<PPos> → a forward iterator pair (begin/end), enumeration
    with zero allocation (the value semantics of upstream's per-MoveNext
    PPos production).

区域角点(UPSTREAM 字段名)| the region corners (UPSTREAM field names).

## src/gfx/render_thread.cpp

UPSTREAM: OpenRA.Platforms.Default/ThreadedGraphicsContext.cs @1f594e320 L188-820(线程循环与 GL 分发)+
          OpenRA.Platforms.Default/Sdl2GraphicsContext.cs @1f594e320 L159-269(DrawPrimitives/DrawElements/
          Clear/Enable-DisableDepthBuffer/SetBlendMode 的 GL 序列)
命令消费的 GL 分发 + 状态 diff(绑定缓存:program/fbo/vao/纹理单元/blend)。
SetBlendMode 的 9 模式映射逐上游(Sdl2GraphicsContext.cs L207-269);blend
状态机 diff(OPT-A6)只跳过"同模式重复",模式变化时始终发完整 GL 序列 ——
与上游每次全发语义等价(GL 状态机收敛到同一状态)。
The GL dispatch of command consumption plus the state diff (binding caches:
program/fbo/vao/texture units/blend). The nine blend-mode mappings follow
upstream one for one (Sdl2GraphicsContext.cs L207-269); the blend
state-machine diff (OPT-A6) skips only same-mode repeats — a mode change
always emits the full GL sequence, equivalent to upstream's unconditional
emission (the GL state machine converges to the same state).

## src/gfx/render_thread.hpp

UPSTREAM: OpenRA.Platforms.Default/ThreadedGraphicsContext.cs @1f594e320 L188-820(渲染线程宿主)
OPT-A5(docs/OPTIMIZATION_TRACKER.md):统一线程模型 —— 渲染线程永远存在并独占 GL 上下文
(上游 Windows 窗口模式禁用渲染线程的特例不复刻,Sdl2PlatformWindow.cs L345-353);
命令消费见 gfx_command.hpp。OPT-A6:消费侧对 Bind* 类命令做状态 diff(仅变化时发 GL
调用),替代上游 Shader.cs L147-172 每次 flush 重播绑定;Debug 构建注册 KHR_debug 回调,
全程零 glGetError 轮询。
The render-thread host. OPT-A5 (docs/OPTIMIZATION_TRACKER.md): a unified
threading model — the render thread always exists and solely owns the GL
context (the upstream Windows-windowed special case of
Sdl2PlatformWindow.cs L345-353 is not replicated); command consumption is
in gfx_command.hpp. OPT-A6: the consumer state-diffs Bind* commands (GL
calls only on change), replacing upstream's per-flush rebinding
(Shader.cs L147-172); Debug builds register a KHR_debug callback and no
glGetError polling happens anywhere.

## src/gfx/renderable.cpp

UPSTREAM: OpenRA.Game/Graphics/Renderable.cs @1f594e320 L18-60 +
          OpenRA.Game/Graphics/SpriteRenderable.cs @1f594e320 L18-131 +
          OpenRA.Game/Graphics/UISpriteRenderable.cs @1f594e320 L17-79 +
          OpenRA.Game/Graphics/TargetLineRenderable.cs @1f594e320 L19-77 +
          OpenRA.Game/Graphics/MarkerTileRenderable.cs @1f594e320 L17-55 +
          OpenRA.Game/Graphics/WorldRenderer.cs @1f594e320 L25-26/141-175/319-323
实现(头文件携带完整 UPSTREAM 锚点与 OPT-A7 论证)。
Implementations (the headers carry the full UPSTREAM anchors and the
OPT-A7 arguments).

## src/gfx/renderable.hpp

UPSTREAM: OpenRA.Game/Graphics/Renderable.cs @1f594e320 L18-60(接口族 + TintModifiers)+
          OpenRA.Game/Graphics/SpriteRenderable.cs @1f594e320 L18-131 +
          OpenRA.Game/Graphics/UISpriteRenderable.cs @1f594e320 L17-79 +
          OpenRA.Game/Graphics/TargetLineRenderable.cs @1f594e320 L19-77 +
          OpenRA.Game/Graphics/MarkerTileRenderable.cs @1f594e320 L17-55 +
          OpenRA.Game/Graphics/WorldRenderer.cs @1f594e320 L25-26/141-175(排序键公式)
可渲染物族。上游是接口 + 每次调用 With* 都 new 一个堆对象的不可变类;
C++ 侧按 OPT-A7(docs/OPTIMIZATION_TRACKER.md)以 **RenderItem POD(带 kind
判别)** 承载引擎内四种具体 renderable —— With* 返回按值拷贝,零分配;
自定义(mod 侧)renderable 走 IRenderable/IFinalizedRenderable 虚接口
(Phase 5 trait 面),经 Custom 槽非拥有指针进入收集流。
OPT-A7 落地对照(上游 WorldRenderer.cs L141-175/319-323):
  - 上游每帧:renderablesBuffer 装箱对象 + keys 数组 + span.Sort 双联动 +
    PrepareRender 再一次堆分配(多数实现返回 this);
  - C++:收集进复用 vector<RenderItem>(值语义),排序在 FrameArena 上做
    (key<<32|index 的 uint64 数组 + std::sort;键嵌入 index 后全序唯一,
    与上游"稳定排序防闪烁"的 (key,i) 序逐项一致),PrepareRender 对四种
    引擎 kind 为恒等(上游 SpriteRenderable/UISprite/TargetLine/MarkerTile
    的 PrepareRender 全部 `return this`);
  - 上游 Draw 的 overlay 段 LINQ GroupBy(GetType()) HACK("Keep old grouping
    behaviour")→ 计数分段:一遍按 kind 计数 + 记录 kind 首遇序,二遍稳定
    散进 FrameArena 分段 —— 段间序 = kind 首遇序,段内序 = 收集序,与
    GroupBy 的两阶稳定序逐项一致,零分配。
形态适配:
  - Game.Renderer 静态 → WorldRenderer 注入的 Renderer 引用(上游为进程
    级单例,等价);
  - TargetLine 的 IEnumerable<WPos>/LINQ → span(调用方保证生存期;上游
    委托枚举同为惰性引用);
  - MarkerTile 的 map.Grid.Ramps 查询 → WorldRenderer 的 TerrainMapSurface
    注入面(Map 随 Phase 5);
  - WithZOffset/OffsetBy/AsDecoration 在 UI/TargetLine/MarkerTile 上游本就
    返回 this(C# 引用相等)—— POD 按值拷贝,可观测行为一致。
The renderable family. Upstream models this as interfaces plus immutable
classes whose every With* call news a heap object; per OPT-A7
(docs/OPTIMIZATION_TRACKER.md) the C++ side carries the four engine
concrete renderables as a **RenderItem POD (with a kind discriminator)** —
With* returns by-value copies, zero allocation — while custom (mod-side)
renderables keep the IRenderable/IFinalizedRenderable virtual interfaces
(the Phase 5 trait surface) entering the collection flow as non-owning
pointers in the Custom slot. OPT-A7 mapping (upstream
WorldRenderer.cs L141-175/319-323): upstream allocates boxed objects into
renderablesBuffer per frame, sorts with a keys-array/span.Sort pairing,
then PrepareRender heap-allocates once more (most implementations return
this); C++ collects into a reused vector<RenderItem> (value semantics),
sorts on the FrameArena (a uint64 array of key<<32|index + std::sort; the
embedded index makes the order total and unique, matching upstream's
"(key, i) stable sort against flickering" item for item), and PrepareRender
is the identity for the four engine kinds (upstream
SpriteRenderable/UISprite/TargetLine/MarkerTile all `return this`). The
overlay stage's LINQ GroupBy(GetType()) HACK ("Keep old grouping
behaviour") becomes counting segmentation: one pass counts per kind and
records first-encounter kind order, a second pass stably scatters into
FrameArena segments — between segments the first-encounter order, within a
segment the collection order, item-for-item equal to GroupBy's two-level
stable order, with zero allocation. Shape adaptations: the Game.Renderer
static → the Renderer reference injected into WorldRenderer (upstream is a
process-wide singleton, equivalent); TargetLine's IEnumerable<WPos>/LINQ →
a span (the caller guarantees the lifetime; upstream's delegate enumeration
is equally a lazy reference); MarkerTile's map.Grid.Ramps lookup → the
TerrainMapSurface injection surface on WorldRenderer (Map arrives in Phase
5); WithZOffset/OffsetBy/AsDecoration return this upstream on
UI/TargetLine/MarkerTile (C# reference equality) — a POD by-value copy is
observably identical.

## src/gfx/renderer.cpp

UPSTREAM: OpenRA.Game/Renderer.cs @1f594e320 L24-585(实现)
头文件携带完整 UPSTREAM 锚点、OPT-B1 单级合成论证与形态适配清单。
Implementation of Renderer.cs; the header carries the full UPSTREAM
anchors, the OPT-B1 single-pass compositing argument, and the shape
adaptations.

## src/gfx/renderer.hpp

UPSTREAM: OpenRA.Game/Renderer.cs @1f594e320 L24-585(逐方法)
帧编排:BeginWorld(world FBO 渲染)→ BeginUI(合成 + UI)→ EndFrame(Present)。
OPT-B1(docs/OPTIMIZATION_TRACKER.md)单级合成 —— 上游三级 pow2 链
(world FBO → screen FBO → 默认帧缓冲;1920×1080 窗口被迫 2048² 双 FBO,
每帧 3 次全屏 clear + 2 次全屏拷贝)改为 NPOT + 单级:
  - world FBO 按视口精确尺寸(OPT-B1 去 pow2,D37 延续);
  - screen FBO 整级删除:BeginUI 把 worldSprite 直接画进默认帧缓冲
    (像素风放大保留),UI 随后同缓冲绘制;
  - BeginFrame 对默认帧缓冲的 Context.Clear() 删除(上游注释自认冗余 ——
    EndFrame 的全屏 quad 覆盖之;单级下由 BeginUI 的全屏 blit 覆盖);
    无 world 的帧(UI-only)在 BeginUI 的 else 分支显式清一次;
  - EndFrame 的最终全屏拷贝随之消失(默认帧缓冲已是最终画面)。
**NDC 数学等价论证**(渲染对拍关卡的先验论证):上游 UI 顶点坐标 c ∈
[0, bufferSize(pow2/scale)],p1 = 2/bufferSize → NDC = 2c/bufferSize - 1;
随后 screenSprite 把 pow2 缓冲的 [0,surface] 子矩形缩放到窗口,净效果
NDC = 2c/surface - 1。单级下 bufferSize = surface(NPOT)/scale,同一净映射,
像素落点相同(HiDPI scale 两版同除)。
形态适配:
  - 着色器源码构造注入(上游 ShaderBindings.GetShaderCode 读 EngineDir/
    glsl;引擎侧文件系统随 Game 批次接线,测试直接传仓库内 glsl/ 源码);
  - Fonts/InitializeFonts/SaveScreenshot/GetRenderBufferSnapshot 随字体与
    Png 批次(formats/);WorldRenderers(IRenderer 后处理族)随后处理批次;
  - window/scale 查询经 Sdl2Window 几何快照(OPT-B5);
  - GraphicSettings 的窗口模式解析(GetResolution)由调用方决定窗口尺寸,
    本类只接窗口与批次参数。
The frame orchestration: BeginWorld (render into the world FBO) → BeginUI
(composite + UI) → EndFrame (Present). OPT-B1
(docs/OPTIMIZATION_TRACKER.md) single-pass compositing — upstream's
three-stage pow2 chain (world FBO → screen FBO → default framebuffer; a
1920×1080 window forced dual 2048² FBOs with three fullscreen clears and
two fullscreen copies per frame) becomes NPOT + single-pass:
  - the world FBO at the exact viewport size (OPT-B1 drops pow2, extending
    D37);
  - the screen FBO stage deleted outright: BeginUI draws worldSprite
    straight into the default framebuffer (pixel-art scaling preserved),
    with the UI drawn into the same buffer afterwards;
  - BeginFrame's Context.Clear() of the default framebuffer is deleted
    (upstream's own comment calls it redundant — the EndFrame fullscreen
    quad overwrites it; under single-pass, BeginUI's fullscreen blit does)
    with UI-only frames cleared once explicitly in BeginUI's else branch;
  - EndFrame's final fullscreen copy disappears with it (the default
    framebuffer already holds the final image).
**NDC-math equivalence argument** (the a-priori argument for the
render-differential gate): upstream UI vertex coordinates c ∈ [0,
bufferSize (pow2/scale)] with p1 = 2/bufferSize → NDC = 2c/bufferSize - 1;
the screenSprite then maps the pow2 buffer's [0,surface] sub-rectangle onto
the window, netting NDC = 2c/surface - 1. Single-pass has bufferSize =
surface (NPOT)/scale — the same net mapping, identical pixel landing spots
(HiDPI scale divides identically in both). Shape adaptations: shader
sources injected at construction (upstream's
ShaderBindings.GetShaderCode reads EngineDir/glsl; the engine-side
filesystem wires up with the Game batch — tests pass the repo's glsl/
sources directly); Fonts/InitializeFonts/SaveScreenshot/
GetRenderBufferSnapshot await the font and Png batches (formats/);
WorldRenderers (the IRenderer post-process family) await the
post-process batch; window/scale queries go through the Sdl2Window
geometry snapshot (OPT-B5); GraphicSettings' window-mode resolution
(GetResolution) is the caller's business — this class takes the window
and batch parameters only.

## src/gfx/sequence_set.cpp

UPSTREAM: OpenRA.Game/Graphics/SequenceSet.cs @1f594e320 L19-119
         (sequence_set.hpp 的实现;头注的形态适配说明适用)
         Implementation of sequence_set.hpp; the shape-adaptation notes of
         the hpp header apply.

## src/gfx/sequence_set.hpp

UPSTREAM: OpenRA.Game/Graphics/SequenceSet.cs @1f594e320 L19-119(全文逐语义)
序列集:sequences.yaml 的引擎侧容器。ISpriteSequence = 单条序列的查询面
(mods 侧 ClassicSpriteSequence 家族实现,Phase 5 移植;本批留注入面);
ISpriteSequenceLoader = 解析器(modData.SpriteSequenceLoader,manifest 的
SpriteSequenceFormat 名分派 —— 同为 Phase 5 面)。
形态适配:
  - 构造依赖(上游自 ModData 取 Manifest.Sequences/SpriteLoaders/
    RendererConstants 两 sheet 尺寸)→ Deps 注入;
  - images 的嵌套 Dictionary(插入序)→ vector&lt;pair&gt; 两级保序(GetSequence
    的错误文本逐字;LoadSprites 的遍历序与上游一致);
  - PerfTimer 保留为名字注释(计时面 Phase 6);
  - ^ 前缀跳过 = ActorInfo.AbstractActorPrefix。
The sequence set: the engine-side container of sequences.yaml.
ISpriteSequence = the single-sequence query face (the mods-side
ClassicSpriteSequence family implements it, ported in Phase 5; this batch
lands the injection surface); ISpriteSequenceLoader = the parser
(modData.SpriteSequenceLoader, dispatched by the manifest's
SpriteSequenceFormat name — equally a Phase 5 face). Shape adaptations:
  - the construction dependencies (upstream pulls Manifest.Sequences/
    SpriteLoaders/the two RendererConstants sheet sizes from ModData) →
    the Deps injection;
  - the nested insertion-ordered Dictionary of images → a two-level
    order-preserving vector<pair> (GetSequence's error texts verbatim;
    LoadSprites' traversal order as upstream);
  - the PerfTimer stays as a name comment (the timing face is Phase 6);
  - the ^-prefix skip = ActorInfo.AbstractActorPrefix.

## src/gfx/shader.cpp

UPSTREAM: OpenRA.Platforms.Default/Shader.cs @1f594e320 L29-259(实现体)
实现体翻译:CompileShaderObject({VERSION} 替换 + 编译 + 状态检查)、
program 组装(属性循环/fragColor/attach/link)、active uniform 枚举、
Set*/PrepareRender/Bind。GL 访问全部经命令队列;同步往返仅构造期发生。
The implementation body translated: CompileShaderObject ({VERSION}
substitution + compile + status check), program assembly (the attribute
loop / fragColor / attach / link), active-uniform enumeration, and
Set*/PrepareRender/Bind. All GL access goes through the command queue;
synchronous round-trips happen only during construction.

## src/gfx/shader.hpp

UPSTREAM: OpenRA.Game/Graphics/ShaderBindings.cs @1f594e320 L17-34 +
OpenRA.Platforms.Default/Shader.cs @1f594e320 L20-260(逐方法)
语义面:{VERSION} 占位替换(Embedded→"300 es",否则→"140",全量替换)、
属性循环(EnableVertexAttribArray + BindAttribLocation)、Modern 档
BindFragDataLocation(program, 0, "fragColor")、链接后 active uniform 枚举
(uniformCache 建表 + sampler 依次分配纹理单元并 Uniform1i 固定)、
PrepareRender(use + 纹理表绑定)、Bind(属性指针重播)。
OPT-A6(docs/OPTIMIZATION_TRACKER.md)落地点:
  - uniform 位置构造期枚举为整数表(SetVec/SetBool 走整型 location 发射,
    字符串只在查表时出现 —— 与上游 Dictionary<string,int> 缓存等价,但
    Location() 整数 API 供热路径完全绕开字符串);
  - 每次 PrepareRender/Set* 前的 UseProgram 由消费端绑定 diff 吞掉
    (上游每次显式 glUseProgram,Shader.cs L147-172);
  - 上游 PrepareRender 的 glIsTexture 逐帧驱逐(OPT-A6 批判点)以生命
    周期契约替代:SetTexture 传入的 Texture 须存活至被替换或 Shader 析构
    (引擎内 Sheet 纹理由 SpriteCache 持有,天然满足)—— COVERAGE 登记。
  - VAO 缓存(每 (顶点格式, shader) 一 VAO)随 SpriteRenderer 批次落地,
    本批 Bind() 保持上游的全局 VAO 属性重播语义。
偏离:链接成功后即 DeleteShader 两个编译产物(上游不删;已链接 program
的行为不受影响,GL 规范允许)。
The semantic plane of Shader.cs plus the ShaderBindings.cs data plane:
the {VERSION} placeholder substitution (Embedded→"300 es", else→"140", all
occurrences), the attribute loop (EnableVertexAttribArray plus
BindAttribLocation), BindFragDataLocation(program, 0, "fragColor") on the
Modern profile, the post-link active-uniform enumeration (uniformCache
built, samplers assigned texture units one by one and pinned via
Uniform1i), PrepareRender (use + texture-table binding), and Bind
(attribute-pointer replay). OPT-A6 landing points:
  - uniform locations enumerate into an integer table at construction
    (SetVec/SetBool post with integer locations; strings appear only on
    table lookup — equivalent to upstream's Dictionary<string,int> cache,
    while the Location() integer API lets hot paths bypass strings
    entirely);
  - the UseProgram before each PrepareRender/Set* is absorbed by the
    consumer-side binding diff (upstream calls glUseProgram explicitly
    every time, Shader.cs L147-172);
  - upstream's per-flush glIsTexture eviction in PrepareRender (the OPT-A6
    critique) is replaced by a lifetime contract: a Texture passed to
    SetTexture must outlive its replacement or the Shader (Sheet textures
    are held by SpriteCache in-engine, satisfying this naturally) —
    registered in COVERAGE;
  - VAO caching (one VAO per (vertex format, shader)) lands with the
    SpriteRenderer batch; this batch's Bind() keeps upstream's global-VAO
    attribute-replay semantics.
Deviation: the two compiled shader objects are DeleteShader'd right after a
successful link (upstream keeps them; linked programs are unaffected, as
the GL spec allows).

## src/gfx/sheet.cpp

UPSTREAM: OpenRA.Game/Graphics/Sheet.cs @1f594e320 L19-196 +
          OpenRA.Game/Graphics/SheetBuilder.cs @1f594e320 L34-175(实现)
Implementations of Sheet/SheetBuilder (headers in sheet.hpp carry the
full UPSTREAM anchors and deviation notes).

## src/gfx/sheet.hpp

UPSTREAM: OpenRA.Game/Graphics/Sheet.cs @1f594e320 L19-196 +
          OpenRA.Game/Graphics/SheetBuilder.cs @1f594e320 L34-175
Sheet:CPU 侧像素缓冲 + 惰性 GL 纹理;dirty 全量/子区域上传自动切换
(dirtyRegion 且纹理尺寸匹配时 SetSubData,否则 SetData);缓冲可释放
(ReleaseBuffer 提交后丢弃,转移给下一张 sheet 复用)。
SheetBuilder:shelf 打包(行高推进、margin 间隔、Indexed 类型四通道
轮换 R→G→B→A,BGRA 类型直接换 sheet)。
形态适配(上游 Game.Renderer 全局访问 → RenderThread* 注入;render 为空
= 上游 Utility 的 Game.Renderer == null 纯数据路径,GetTexture 断言挡):
  - Sheet 纹理由 optional<Texture> RAII 持有(上游手动 Dispose);
  - 上游 Sheet(Stream)(Png 解码)随 formats 第二编(png/dds/tga);AsPng 同;
  - Add(ISpriteFrame) 已随 formats 第一编接线;Add(Png) 随第二编;
  - ReleaseBufferAndTryTransferTo 转移"清零缓冲"改为重新分配(上游
    转移原数组对象复用;内容同为全零,行为等价 —— COVERAGE 登记)。
Sheet: the CPU-side pixel buffer with a lazily created GL texture;
dirty full/sub-rectangle uploads switch automatically (SetSubData when a
dirtyRegion exists and the texture size matches, else SetData); the
buffer may be released (discarded after commit in ReleaseBuffer, or
transferred to the next sheet for reuse). SheetBuilder: shelf packing
(row-height advance, margin gaps, and the Indexed four-channel rotation
R→G→B→A with BGRA types going straight to a new sheet).
Shape adaptations (upstream's Game.Renderer global → RenderThread*
injection; a null render reproduces upstream's Game.Renderer == null
Utility data-only path, with GetTexture asserting):
  - the Sheet texture is owned by an optional<Texture> RAII handle
    (upstream Disposes manually);
  - Sheet(Stream) (Png decode) and AsPng await the second formats
    batch (png/dds/tga);
  - Add(ISpriteFrame) is wired with the first formats batch; Add(Png)
    awaits the second;
  - ReleaseBufferAndTryTransferTo re-allocates the transferred
    "cleared buffer" instead of reusing the original array object
    (upstream recycles the array; contents are equally all-zero —
    registered in COVERAGE).

## src/gfx/sprite.hpp

UPSTREAM: OpenRA.Game/Graphics/Sprite.cs @1f594e320 L18-82 +
          OpenRA.Game/Graphics/SheetBuilder.cs @1f594e320 L26-32(SheetType)+
          OpenRA.Game/Graphics/PlatformInterfaces.cs @1f594e320 L36-48(BlendMode)+
          OpenRA.Game/Graphics/SpriteLoader.cs @1f594e320 L24-59(SpriteFrameType)
精灵值类型:Bounds/Sheet/BlendMode/Channel/ZRamp/Size/Offset 全 readonly 逐字段;
归一化纹理坐标 Left/Top/Right/Bottom 在构造期预计算(含 1/128 像素 inset,
防 GPU 在非 1:1 帧缓冲上采样溢出采样到精灵矩形外 —— 上注释逐句保留)。
SpriteWithSecondaryData = 深度/辅助通道的第二纹理引用(无 inset)。
Sprite value type: Bounds/Sheet/BlendMode/Channel/ZRamp/Size/Offset are
all readonly, field for field; the normalized texture coordinates
Left/Top/Right/Bottom are precomputed at construction (with the 1/128-pixel
inset guarding GPUs that sample a texel row outside the sprite rect on
non-1:1 framebuffers — the upstream comment preserved verbatim).
SpriteWithSecondaryData carries the second (depth/auxiliary) texture
reference (no inset).

## src/gfx/sprite_cache.cpp

UPSTREAM: OpenRA.Game/Graphics/SpriteCache.cs @1f594e320 L21-183
         (sprite_cache.hpp 的实现;头注的形态适配说明适用)
         Implementation of sprite_cache.hpp; the shape-adaptation notes of
         the hpp header apply.

## src/gfx/sprite_cache.hpp

UPSTREAM: OpenRA.Game/Graphics/SpriteCache.cs @1f594e320 L21-183(全文逐语义)
精灵预留/物化两段流水:ReserveSprites 记账(token 制,同文件聚合),Load
Reservations 按文件加载帧 → (帧索引越界检查,消息逐字含 SourceLocation
的 "Name:Line" 形态)→ pendingResolve 按 **帧高稳定排序**(sheet 行高打包
的装箱优化,上游注释照抄)→ (文件名, 帧索引, 预乘, AdjustFrame) 四元组
去重的 spriteCache 逐项入 SheetBuilder。ResolveSprites 以 token 取回(已
取/未记 → InvalidOperationException 逐字;文件缺失 → FileNotFoundException
文本)。LoadScreen 进度回调为 Phase 6 面(no-op)。
形态适配:
  - Stream/ISpriteLoader[] → span + SpriteLoaderFn 链(sprite_loader.hpp);
  - Dictionary&lt;SheetType, SheetBuilder&gt; → 两成员(Indexed/BGRA;上游键集
    就是这两枚);
  - reservationsByFilename/spriteReservations 的 Dictionary 迭代序 = 插入
    序,以 vector&lt;pair&gt; 保序(等高稳定排序的平局结果与上游一致);
  - AdjustFrame 委托返回的帧对象由调用方保证存活至 LoadReservations 结束
    (上游靠 GC;消费点仅在 Add 读取 Size/Type/Data);
  - TrimExcess 的容量收缩无 C++ 对应物(行为等价,内存面不同)。
The two-stage sprite pipeline: ReserveSprites books (token-based, grouped
per file), then LoadReservations loads frames per file → (the frame-index
bounds check, message verbatim including SourceLocation's "Name:Line"
form) → pendingResolve stably sorted by **frame height** (the sheet-packing
optimization over the row height, the upstream comment verbatim) → the
spriteCache deduplicated by (filename, frame index, premultiplied,
AdjustFrame) feeding the SheetBuilder item by item. ResolveSprites hands
the sprites back per token (already-reserved/never-reserved → the
InvalidOperationException verbatim; a missing file → the
FileNotFoundException text). The LoadScreen progress callback is a Phase 6
face (no-op). Shape adaptations:
  - Stream/ISpriteLoader[] → the span + SpriteLoaderFn chain
    (sprite_loader.hpp);
  - Dictionary<SheetType, SheetBuilder> → two members (Indexed/BGRA; the
    upstream key set is exactly these two);
  - the Dictionary iteration order of reservationsByFilename/
    spriteReservations = insertion order, preserved as vector<pair> (the
    height-stable sort breaks ties as upstream does);
  - the frames returned by the AdjustFrame delegate stay alive until
    LoadReservations ends by caller guarantee (upstream leans on the GC;
    the consumption points only read Size/Type/Data at Add);
  - TrimExcess has no C++ counterpart (behavior-equal, memory differs).

## src/gfx/sprite_font.cpp

UPSTREAM: OpenRA.Game/Graphics/SpriteFont.cs @1f594e320(sprite_font.hpp 的
实现)
The implementation of sprite_font.hpp.

## src/gfx/sprite_font.hpp

UPSTREAM: OpenRA.Game/Graphics/SpriteFont.cs @1f594e320 L20-431(全文)+
PlatformInterfaces.cs L192-195(IFont)。字形缓存/对比度字形(灰度膨胀的
圆形权图)/阴影与对比度文本绘制/Measure 的行宽累加;deviceScale 的屏幕
像素取整面(0.5f 偏移 + (int) 截断)逐行保真。
形态映射:
  - Game.Renderer.RgbaSpriteRenderer 静态 → 两个绘制 sink 注入(平铺/四角
    形态;生产 = UIRgbaSpriteRenderer 的对应重载,测试可捕捕获;D 注册);
  - char(UTF-16 单元)迭代 → UTF-8 码点迭代(core/text.hpp DecodeUtf8;
    字形键 = 码点 —— 上游 >0xFFFF 以代理对半值查字形本就无字形,语义
    等价,登记 COVERAGE);
  - Cache<char,…>/Cache<(char,int),…>/Cache<int,…> → 插入序 vector 键表
    (C# Cache 的 GetValue 工厂等价;SetScale 的 Clear 语义照抄);
  - Platform.CreateFont(data) → IFont 抽象直传(Renderer.InitializeFonts
    经 FreeTypeFont 实现供应);
  - ctor 的 `glyphs[n] == null` 抛检查上游不可达(Cache 恒物化),省略。
Shape mappings: Game.Renderer.RgbaSpriteRenderer's static becomes two
injected draw sinks (the flat and four-corner shapes; production wires
UIRgbaSpriteRenderer's matching overloads, tests capture; registered);
the char (UTF-16 unit) iteration becomes a UTF-8 code-point iteration
(core/text.hpp DecodeUtf8; the glyph key = the code point — upstream's
>0xFFFF surrogates looked up half-value glyphs that never existed,
semantically equal, registered in COVERAGE); the
Cache<char,…>/Cache<(char,int),…>/Cache<int,…> become insertion-ordered
vector-keyed tables (the C# Cache's GetValue-factory equivalent;
SetScale's Clear kept); Platform.CreateFont(data) arrives as the IFont
abstraction passed in (Renderer.InitializeFonts supplies FreeTypeFont
instances); the ctor's `glyphs[n] == null` throw is unreachable upstream
(the Cache always materializes) and is dropped.

## src/gfx/sprite_frame.hpp

UPSTREAM: OpenRA.Game/Graphics/SpriteLoader.cs @1f594e320 L53-71
ISpriteFrame:格式加载器(SHP/TMP/PNG…)对渲染侧暴露的帧契约。
上游为 C# 属性(Type/Size/FrameSize/Offset/Data/DisableExportPadding);
C++ 侧为纯虚 getter —— 字段语义逐一对应:
  - Size = Data 的像素尺寸;FrameSize = 整帧画框尺寸(SHPTD 裁边帧
    FrameSize > Size;TMP 空帧两者皆 0);
  - Offset = 帧在画框内的偏移(半像素对齐语义,可能为 0.5 步进);
  - Data = 索引色(BGRA)字节流;上游 null 与 byte[0] 在消费路径
    (SheetBuilder.Add 的空尺寸早退)不可区分,C++ 统一为空 span。
ISpriteFrame: the frame contract that format loaders (SHP/TMP/PNG...)
expose to the render side. Upstream models these as C# properties; the
C++ side uses pure-virtual getters — field semantics map one to one:
  - Size is the pixel size of Data; FrameSize is the full frame canvas
    (SHPTD trimmed frames carry FrameSize > Size; TMP empty tiles zero
    both);
  - Offset is the frame's offset within the canvas (half-pixel aligned
    semantics, in 0.5 steps possible);
  - Data is the indexed-color (BGRA) byte stream; upstream's null vs
    byte[0] is indistinguishable on consumer paths (SheetBuilder.Add's
    empty-size early-out), unified here as an empty span.

## src/gfx/sprite_loader.cpp

UPSTREAM: OpenRA.Game/Graphics/SpriteLoader.cs @1f594e320 L53-114 +
         OpenRA.Game/ObjectCreator.cs @1f594e320 L155
         (sprite_loader.hpp 的实现)
         Implementation of sprite_loader.hpp.

## src/gfx/sprite_loader.hpp

UPSTREAM: OpenRA.Game/Graphics/SpriteLoader.cs @1f594e320 L53-114(ISpriteLoader/
         FrameCache/FrameLoader)+
         OpenRA.Game/ModData.cs @1f594e320 L107(ObjectCreator.GetLoaders&lt;ISpriteLoader&gt;)
         OpenRA.Game/ObjectCreator.cs @1f594e320 L155(loader 未找到的异常文本)
精灵加载链:按 Manifest.SpriteFormats 名序逐个尝试(首个命中即用)。
C++ 形态:
  - ISpriteLoader 接口 → 函数指针 bool(*)(span, vector<unique_ptr<
    ISpriteFrame>>&)(各格式的 TryParse* 已有,签名一致);
  - ObjectCreator.GetLoaders(名 → 实例)→ MakeSpriteLoaders 名字分派表
    (与 MakePackageLoaders 同形,D73 族);未知名消息逐字("Unable to find
    a sprite loader for type '{name}'.");
  - FrameLoader 的 TypeDictionary metadata 出参未接:引擎消费面
    (SpriteCache/FrameCache)对 metadata 仅透传不读,PngSheet 的
    EmbeddedSpritePalette 随 mods 序列批(COVERAGE 登记);
  - **帧与文件字节共持有**:上游帧可引用解码缓冲(ShpTD 的 TrimmedFrame
    借底层 byte[],靠 GC 存活);C++ 以 FramesSource 同时持有文件字节与
    帧表,调用方保证两者同寿命(形态适配,COVERAGE 登记)。
The sprite loader chain: try each loader in Manifest.SpriteFormats name
order, first hit wins. C++ shape:
  - the ISpriteLoader interface → the function-pointer signature
    bool(*)(span, vector<unique_ptr<ISpriteFrame>>&) (the existing
    per-format TryParse* functions already match it);
  - ObjectCreator.GetLoaders (name → instance) → the MakeSpriteLoaders
    name-dispatch table (the same shape as MakePackageLoaders, the D73
    family); the unknown-name message is verbatim ("Unable to find a
    sprite loader for type '{name}'.");
  - FrameLoader's TypeDictionary metadata output is not wired: the engine
    consumers (SpriteCache/FrameCache) only pass metadata through without
    reading it; PngSheet's EmbeddedSpritePalette lands with the mods
    sequences batch (registered in COVERAGE);
  - **frames co-own the file bytes**: upstream frames may reference the
    decode buffer (ShpTD's TrimmedFrame borrows the underlying byte[],
    kept alive by the GC); C++ carries the file bytes and the frame table
    together in FramesSource, with the caller keeping both alive (a shape
    adaptation, registered in COVERAGE).

## src/gfx/sprite_renderer.cpp

UPSTREAM: OpenRA.Game/Graphics/SpriteRenderer.cs @1f594e320 L20-305(实现)+
          OpenRA.Game/Graphics/RgbaSpriteRenderer.cs @1f594e320 L17-58 +
          OpenRA.Game/Graphics/RgbaColorRenderer.cs @1f594e320 L20-256
头文件携带完整 UPSTREAM 锚点与第四批优化挂接说明。
Implementations; the header carries the full UPSTREAM anchors and the
fourth-batch optimization wiring notes.

## src/gfx/sprite_renderer.hpp

UPSTREAM: OpenRA.Game/Graphics/SpriteRenderer.cs @1f594e320 L20-305(逐方法)+
          OpenRA.Game/Graphics/RgbaSpriteRenderer.cs @1f594e320 L17-58 +
          OpenRA.Game/Graphics/RgbaColorRenderer.cs @1f594e320 L20-256
SpriteRenderer:8 纹理槽批渲染 + BlendSpan 交错混合段 + SetRenderStateForSprite
的槽位映射(含 SpriteWithSecondaryData 的双 sheet 逻辑与满槽 flush 重试),
逐行复刻;Flush 的绘制路径挂接第四批的持久映射 VB(OPT-A5:顶点一次
memcpy 进命令 payload,渲染线程等槽 fence 后 memcpy 进映射区)+ per-program
VAO(OPT-A6:绑定/属性一次固化)+ blend 状态机 diff(消费端)。
OPT-A7:ResolveTextureIndex 的 PaletteReference::HasColorShift 每精灵字符串
字典查找,以 HardwarePalette 的色移 epoch 缓存消除(见 palette.hpp)。
形态适配:
  - renderer.CurrentBatchRenderer(上游属性)→ 构造注入 Renderer 的批槽指针
    (IBatchRenderer**),ActivateAsCurrent 复刻 setter 的"前批 Flush"语义;
  - 上游 internal DrawSprite(int paletteTextureIndex, …)族 → C++ 公开
    (上游 internal = 程序集内可见;引擎内 RgbaSpriteRenderer/调用方同权);
  - BlendSpan 列表提出为 BlendSpanTracker、RgbaColorRenderer 的落点提出为
    IRgbaQuadSink 注入面 —— 两者纯逻辑可测(上游私有直连);
  - PerfHistory.Increment("batches") 不移植(诊断面,Phase 9 统计);
  - 上游 vertices 数组 = renderer.Context.CreateVertices(size)(每渲染器一
    份);C++ 侧同构(成员 vector 复用,不逐帧分配)。
SpriteRenderer: the 8-texture-slot batch renderer with BlendSpan
interleaved blending and SetRenderStateForSprite's slot mapping (including
the SpriteWithSecondaryData dual-sheet logic and the full-slot flush
retry), ported line by line; Flush's draw path wires onto the fourth
batch's persistent-mapped VB (OPT-A5: vertices memcpy once into the command
payload, the render thread awaits the slot fence then memcpys into the
mapping) plus the per-program VAO (OPT-A6: bindings/attributes baked once)
plus the consumer-side blend state diff. OPT-A7: ResolveTextureIndex's
per-sprite string-dictionary HasColorShift lookup is eliminated via the
HardwarePalette color-shift epoch cache (see palette.hpp). Shape
adaptations: renderer.CurrentBatchRenderer (an upstream property) → the
Renderer's batch-slot pointer injected at construction (IBatchRenderer**),
with ActivateAsCurrent reproducing the setter's flush-the-previous-batch
semantics; the upstream internal DrawSprite(int paletteTextureIndex, …)
family → public in C++ (internal = assembly-visible; the in-engine callers
share the same reach); the BlendSpan list is extracted into
BlendSpanTracker and RgbaColorRenderer's sink into the injected
IRgbaQuadSink — both pure-logic testable (upstream kept them private
direct-wired); PerfHistory.Increment("batches") is not ported (a
diagnostics surface, settling with Phase 9); upstream's vertices array =
renderer.Context.CreateVertices(size) (one per renderer) — mirrored as a
reused member vector (no per-frame allocation).

## src/gfx/terrain_sprite_layer.cpp

UPSTREAM: OpenRA.Game/Graphics/TerrainSpriteLayer.cs @1f594e320 L21-269(实现)
头文件携带完整 UPSTREAM 锚点与 OPT-A7 分离数组论证。
Implementations of TerrainSpriteLayer.cs; the header carries the full
UPSTREAM anchors and the OPT-A7 split-array arguments.

## src/gfx/terrain_sprite_layer.hpp

UPSTREAM: OpenRA.Game/Graphics/TerrainSpriteLayer.cs @1f594e320 L21-269(逐方法)
地形精灵层:全图静态大 VB(每格 4 顶点),脏行级 SetData 增量上传 +
共享静态 IB(每 World 一份,引用计数)+ 调色板失效全图标脏 +
地形光照的四角采样 tint。
OPT-A7(docs/OPTIMIZATION_TRACKER.md)本批待办"地形层 palette_index/tint
独立数组"落地:
  - 上游 UpdatePaletteIndices(L76-88)在调色板行上移时**逐顶点重写整个
    Vertex 数组**(每顶点一次 48B 结构体构造);C++ 把调色板行索引分离为
    `vec_palette_`(每格 PaletteReference*)、把地形光照 tint 分离为
    `vec_corner_tint_`(每顶点 Vector3,FastCreateQuad 时置 (1,1,1)、
    UpdateTint 写四角采样权重)—— 两者都在**脏行上传点**组合进临时行
    (c = (p&lt;&lt;16)|(v.c&amp;0xFFFF),RGB = v.RGB × cornerTint),上传字节
    与上游逐顶点重写**逐字节一致**(OPT-C5 同型论证:写同一 GL_STATIC_DRAW
    缓冲的同一行区间);
  - 调色板失效 = 全行标脏(不再有任何逐顶点工作)。
形态适配:
  - map(map.MapSize/CenterOfCell/Grid.Ramps/Ramp/Tiles.Contains)→
    WorldRenderer 的 TerrainMapSurface 注入面(完整 Map 随 Phase 5;
    COVERAGE 登记);
  - Viewport 的可见行范围(Draw 的 CandidateMapCoords)→ first/last 行
    参数(完整 Viewport 随 Phase 5;Clamp 语义照抄 L215-216);
  - Update(CPos, ISpriteSequence, …) 重载随序列系统(Phase 5/6);
  - RenderThread* 可空 = 纯数据模式(脏行跟踪/组合照常,VB/IB 不建、
    上传与绘制跳过 —— Sheet/HardwarePalette 同族形态);
  - 上游 ConditionalWeakTable&lt;World, IndexBufferRc&gt; + Lock → 静态
    map&lt;World*, Rc&gt; + mutex(引擎单线程构造,锁照抄上游形态);
  - dirtyRows HashSet → 行标志数组(Remove 语义 = 标志清除,集合序不可
    观测)。
The terrain sprite layer: the whole-map static VB (4 vertices per cell)
with dirty-row SetData incremental uploads + a shared static IB (one per
World, reference counted) + palette-invalidation full re-dirty + the
terrain-lighting four-corner tint sampling. This batch lands OPT-A7's
outstanding "terrain-layer palette_index/tint as separate arrays"
(docs/OPTIMIZATION_TRACKER.md): upstream's UpdatePaletteIndices (L76-88)
rewrites the entire Vertex array vertex by vertex on every palette-row
shift (a 48-byte struct construction per vertex); C++ splits the palette
row index into `vec_palette_` (one PaletteReference* per cell) and the
terrain-lighting tint into `vec_corner_tint_` (one Vector3 per vertex —
(1,1,1) from FastCreateQuad, the four-corner sampling weights from
UpdateTint), composing both into a scratch row at the dirty-row upload
point (c = (p&lt;&lt;16)|(v.c&amp;0xFFFF), RGB = v.RGB × cornerTint) — the uploaded
bytes are byte-identical to upstream's vertex-by-vertex rewrite (the
OPT-C5-style argument: the same row range of the same GL_STATIC_DRAW
buffer), and palette invalidation becomes "mark all rows dirty" with zero
per-vertex work. Shape adaptations: the map (MapSize/CenterOfCell/
Grid.Ramps/Ramp/Tiles.Contains) → the TerrainMapSurface injection surface
on WorldRenderer (the full Map arrives in Phase 5; registered in COVERAGE);
the Viewport's visible-row range (Draw's CandidateMapCoords) → the
first/last row parameters (the full Viewport arrives in Phase 5; the Clamp
semantics of L215-216 kept verbatim); the Update(CPos, ISpriteSequence, …)
overload awaits the sequence system (Phase 5/6); a null RenderThread*
selects the data-only mode (dirty tracking/composition continue, the VB/IB
are never created, uploads and draws skip — the same family as Sheet/
HardwarePalette); upstream's ConditionalWeakTable&lt;World, IndexBufferRc&gt; +
Lock → a static map&lt;World*, Rc&gt; + mutex (single-threaded construction in
engine; the lock mirrors upstream's shape); dirtyRows' HashSet → a row
flag array (Remove = clearing the flag; set ordering is unobservable).

## src/gfx/texture.cpp

UPSTREAM: OpenRA.Platforms.Default/Texture.cs @1f594e320 L43-236(实现体)
实现体翻译:PrepareTexture 的 6 参数序列、BGRA 上传、UNPACK 行打包、
RGBA16F 浮点路径、glGetTexImage 读回。所有 GL 访问经命令队列(渲染线程
独占上下文);纹理操作绑 unit 0(与上游 glBindTexture 的当前单元语义一致
—— Shader::PrepareRender 才切换活动单元)。
The implementation body translated: PrepareTexture's six-parameter
sequence, BGRA uploads, UNPACK row packing, the RGBA16F float path, and the
glGetTexImage readback. All GL access goes through the command queue (the
render thread solely owns the context); texture *operations* bind unit 0
(matching the current-unit semantics of upstream's glBindTexture — only
Shader::PrepareRender switches the active unit).

## src/gfx/texture.hpp

UPSTREAM: OpenRA.Platforms.Default/Texture.cs @1f594e320 L18-237(逐方法)
语义面:BGRA 字节序上传(internal:桌面 RGBA8 / ES BGRA)、SetSubData 的
UNPACK_ROW_LENGTH/SKIP 行打包、RGBA16F 浮点路径(调色板)、读回走桌面
glGetTexImage(GLES FBO 回读分支随 ES 档位批次)、ScaleFilter 变更触发
PrepareTexture(filter/CLAMP_TO_EDGE/BASE_LEVEL 0/MAX_LEVEL 0)。
偏离(已登记 COVERAGE):
  - OPT-B1(D37):上游 SetData/SetFloatData/SetDataFromReadBuffer/SetEmpty 的
    IsPowerOf2 强制校验不复刻(本引擎按 NPOT 直建);
  - OPT-A6:纹理句柄 RAII(析构异步发 DeleteTextures,命令序保证晚于既有
    引用);上游手动 Dispose + Shader 侧 glIsTexture 逐帧驱逐;
  - GLES 分支(Texture.cs L161-199 的 FBO 回读 + BGRA/RGBA 交换)待 ES 档
    位批次;当前桌面路径先行(GetData 断言在 GL 集成测试)。
Method-by-method port of Texture.cs. Semantic plane: BGRA-byte uploads
(internal RGBA8 on desktop / BGRA on ES), the UNPACK_ROW_LENGTH/SKIP row
packing of SetSubData, the RGBA16F float path (palettes), and readback via
desktop glGetTexImage (the GLES FBO-readback branch lands with the ES
profile batch), with ScaleFilter changes re-running PrepareTexture
(filter/CLAMP_TO_EDGE/BASE_LEVEL 0/MAX_LEVEL 0). Deviations (registered in
COVERAGE):
  - OPT-B1 (D37): upstream's IsPowerOf2 enforcement in
    SetData/SetFloatData/SetDataFromReadBuffer/SetEmpty is not replicated
    (this engine builds NPOT directly);
  - OPT-A6: RAII texture handles (the destructor posts DeleteTextures
    asynchronously; command ordering keeps it after all prior references)
    replacing upstream's manual Dispose plus the per-flush glIsTexture
    eviction in Shader;
  - the GLES branch (the FBO readback and BGRA/RGBA swap of Texture.cs
    L161-199) awaits the ES-profile batch; the desktop path comes first.

## src/gfx/vertex.hpp

UPSTREAM: OpenRA.Game/Graphics/Vertex.cs @1f594e320 L17-64
批渲染顶点(48B 胖顶点,字段序 = 上游 StructLayout(LayoutKind.Sequential)):
  [x,y,z] 位置;[s,t] 主纹理坐标(或 RGBA 颜色);[u,v] 次纹理坐标;
  [c] 调色板/通道打包位域(布局注释见 combined.vert,gfx_util::FastCreateQuad);
  [r,g,b,a] 颜色 tint。
CombinedShaderBindings 的属性表(顶点格式契约)一并列出,第四批接
SpriteRenderer 的 VAO 缓存(OPT-A6)时按 (顶点格式, shader) 建 VAO。
The batch-render vertex (a 48-byte fat vertex; field order = upstream's
StructLayout(LayoutKind.Sequential)):
  [x,y,z] position; [s,t] primary texcoords (or an RGBA color); [u,v]
  secondary texcoords; [c] the packed palette/channel bitfield (layout
  documented in combined.vert; packed by gfx_util::FastCreateQuad);
  [r,g,b,a] color tint.
The CombinedShaderBindings attribute table (the vertex-format contract)
is listed alongside; the fourth batch wires it into the SpriteRenderer
VAO cache (OPT-A6) as one VAO per (vertex format, shader).

## src/gfx/vertex_buffer.cpp

UPSTREAM: OpenRA.Game/Graphics/PlatformInterfaces.cs @1f594e320(IVertexBuffer/IIndexBuffer 接口族)+
          OpenRA.Platforms.Default/VertexBuffer.cs @1f594e320 L17-125 +
          OpenRA.Platforms.Default/StaticIndexBuffer.cs @1f594e320
实现(头文件携带完整 UPSTREAM 锚点与 OPT-A5/A6 论证)。
Implementations (the headers carry the full UPSTREAM anchors and the
OPT-A5/A6 arguments).

## src/gfx/vertex_buffer.hpp

UPSTREAM: OpenRA.Game/Graphics/PlatformInterfaces.cs @1f594e320(IVertexBuffer/IIndexBuffer 接口族)+
          OpenRA.Platforms.Default/VertexBuffer.cs @1f594e320 L17-125 +
          OpenRA.Platforms.Default/StaticIndexBuffer.cs @1f594e320
顶点/索引缓冲封装(主线程侧命令发射器)。两种 VB 形态:
  - 持久映射(OPT-A5):glBufferStorage + MapBufferRange(PERSISTENT|WRITE|
    COHERENT)整块映射,N 槽轮换 + 槽级 fence;写入 = 命令 payload → 渲染
    线程 memcpy 进映射区(槽 fence 先行等待)。上游 Flush 路径 = "CPU 数组
    → 装箱消息(或 LOH 分支拷贝)→ glBufferSubData(驱动内再拷贝)",本实现
    免去驱动路径的额外拷贝与 glBufferData 的存储重分配;映射与 fence 全在
    渲染线程内闭环,主线程零等待(详证 gfx_command.hpp 的 OPT-A5 头注)。
  - 静态:一次 BufferData(GL_STATIC_DRAW)(上游 VertexBuffer(T[] data)
    构造;TerrainSpriteLayer 的静态大 VB 等)。
OPT-A6:每 (顶点格式, program) 一个 VAO —— VB/IB/属性指针一次固化进 VAO,
绘制路径只剩 BindVertexArray(消费端 diff);上游每 flush 重播 Shader.Bind
的属性指针序列 + indexBuffer.Bind(Shader.cs L134-145)。
Vertex/index buffer wrappers (main-thread command emitters). Two VB forms:
persistent-mapped (OPT-A5: glBufferStorage + whole-buffer
PERSISTENT|WRITE|COHERENT mapping, N rotating slots with per-slot fences;
writes ride the command payload and the render thread memcpys into the
mapping after awaiting the slot fence — upstream's flush path was "CPU
array → boxed message (or the LOH-branch copy) → glBufferSubData (yet
another copy inside the driver)", which this removes along with
glBufferData's storage reallocation; mapping and fences stay entirely
inside the render thread, so the main thread never waits — see the OPT-A5
header note in gfx_command.hpp) and static (one BufferData(GL_STATIC_DRAW),
upstream's VertexBuffer(T[] data) constructor; the big static
TerrainSpriteLayer VBs among others). OPT-A6: one VAO per (vertex format,
program) — VB/IB/attribute pointers bake into the VAO once, leaving only
BindVertexArray on the draw path (diffed by the consumer), replacing
upstream's per-flush attribute-pointer replay of Shader.Bind
(Shader.cs L134-145) plus indexBuffer.Bind.

## src/gfx/viewport.cpp

UPSTREAM: OpenRA.Game/Graphics/Viewport.cs @1f594e320 L21-447
视口:缩放/居中/滚动、世界-屏幕像素换算、可见投影格区的惰性缓存。
注入面(Map/Settings/Renderer,见 viewport.hpp 头注)随 Phase 5 接线。
The viewport: zoom/centering/scrolling, world-screen pixel conversion,
and the lazily cached visible projected-cell regions. The injection faces
(Map/Settings/Renderer — see the viewport.hpp header note) wire up in
Phase 5.

## src/gfx/viewport.hpp

UPSTREAM: OpenRA.Game/Graphics/Viewport.cs @1f594e320 L21-447(全文逐语义)
视口:缩放/居中/滚动/世界-屏幕坐标换算 + 可见投影格区(惰性缓存)。
形态适配(注入面,D89~D92 同族;完整 Map/Settings/Renderer 随 Phase 5):
  - Game.Renderer.NativeResolution / SetMaximumViewportSize →
    IViewportHostRenderer 注入(上游进程单例;测试/Phase 5 适配);
  - Game.Settings.Graphics(ViewportDistance/UIScale)→ GraphicSettingsFace
    值注入;Game.Settings.Sound.Repeat 的写入面在 Sound 门面侧;
  - WorldViewportSizes(IGlobalModData)→ 值结构 + GetSizeRange(随
    Phase 5 规则链加载);
  - Map 依赖:ProjectedTopLeft/BottomRight(WPos 角)、MapSize、
    Clamp(PPos)/Height.Clamp(MPos)/CellContaining(WPos)→ Deps 函数注入
    (默认 = 方格网格 / 界内恒真形态,与 TerrainMapSurface.MakeSquareDefault
    同族);CenterOfCell/TileScale/Ramp 查询复用 wr.TerrainSurface();
  - CandidateMouseoverCells 的 yield 惰性 → vector 物化(遍历序保持
    上游双重递减循环);
  - ViewportTick 事件 → token 订阅表(成对注销);
  - Center(IEnumerable&lt;Actor&gt;) 的 Average → 调用方求均值注入
    (Actor::CenterPosition 面 Phase 5)。
The viewport: zoom/centering/scrolling/world-screen conversion plus the
lazily cached visible projected-cell regions. Shape adaptations (the
injection faces, the D89-D92 family; the full Map/Settings/Renderer land
with Phase 5):
  - Game.Renderer.NativeResolution / SetMaximumViewportSize → the injected
    IViewportHostRenderer (an upstream process singleton; tests and Phase
    5 adapt it);
  - Game.Settings.Graphics (ViewportDistance/UIScale) → the injected
    GraphicSettingsFace value; Game.Settings.Sound.Repeat's write face
    lives on the Sound facade side;
  - WorldViewportSizes (IGlobalModData) → a value struct + GetSizeRange
    (loaded with the Phase 5 rules chain);
  - the Map dependencies: ProjectedTopLeft/BottomRight (the WPos corners),
    MapSize, Clamp(PPos)/Height.Clamp(MPos)/CellContaining(WPos) → Deps
    function injections (defaults = the square-grid / always-in-bounds
    shapes, the same family as TerrainMapSurface.MakeSquareDefault);
    CenterOfCell/TileScale/Ramp queries reuse wr.TerrainSurface();
  - CandidateMouseoverCells' lazy yield → a materialized vector (the
    traversal order keeps upstream's doubly-decreasing loops);
  - the ViewportTick event → a token subscription table (paired
    unsubscribe);
  - Center(IEnumerable<Actor>)'s Average → the caller supplies the mean
    (the Actor::CenterPosition face is Phase 5).

## src/gfx/world_renderer.cpp

UPSTREAM: OpenRA.Game/Graphics/WorldRenderer.cs @1f594e320 L23-483(实现)+
          OpenRA.Game/Map/MapGrid.cs @1f594e320 L48-57(方格默认 Ramp 角)
头文件携带完整 UPSTREAM 锚点、OPT-A7 收集流论证与注入面清单。
Implementations of WorldRenderer.cs; the header carries the full UPSTREAM
anchors, the OPT-A7 collection-flow arguments, and the injection list.

## src/gfx/world_renderer.hpp

UPSTREAM: OpenRA.Game/Graphics/WorldRenderer.cs @1f594e320 L23-483(逐方法)+
          OpenRA.Game/Traits/TraitsInterfaces.cs @1f594e320 L435-476(IRenderTerrain/
          ITerrainLighting/IRenderPostProcessPass/PostProcessPassType)+
          OpenRA.Game/Map/MapGrid.cs @1f594e320 L20-57(默认 Ramp 角的方格形态)
世界渲染器:调色板管理 + 世界坐标到屏幕坐标换算 + 渲染收集/排序/绘制序。
OPT-A7(docs/OPTIMIZATION_TRACKER.md)本批落地的"渲染收集 SOA + 帧 arena":
  - 上游 GenerateRenderables(L141-175)每帧向 renderablesBuffer 装入 IRenderable
    堆对象、PrepareRender 再分配 IFinalizedRenderable;C++ 收集进复用的
    vector<RenderItem>(POD 值语义,见 renderable.hpp),排序 key 数组与分段
    缓冲在 FrameArena 上(帧末 rewind);
  - 上游 Draw 的 overlay GroupBy(GetType()) HACK(L319-323)→ kind 计数分段
    (首遇序 + 段内收集序,两阶稳定序与 LINQ 逐项一致)。
注入面(上游构造期 L53-79 从 ModData/World 的 trait 反射链取的全部依赖,
Phase 5 随 Game/Map 接线;机制面完整移植):
  - TileSize/TileScale(上游 = map.Rules.TerrainInfo.TileSize / map.Grid.
    TileScale)→ Desc 构造参数;enableDepthBuffer(mapGrid.EnableDepthBuffer)
    → Desc;
  - Viewport(上游 new Viewport(this, map);Viewport.cs 依赖 Map 的
    ProjectedCellRegion/Ramp/CellContaining 全家)→ IViewportSurface 接口
    + SetViewport;完整 Viewport 随 Phase 5 的 Map 落地(COVERAGE 登记);
  - ILoadsPalettes/ILoadsPlayerPalettes trait 收集、ScreenMap 查询、
    OrderGenerator/Selection/Effects 渲染源、IRenderer/IRenderTerrain/
    IRenderAboveWorld/IRenderShroud trait 族、DebugVisualizations →
    Hooks/收集回调/接口表注入(默认 no-op);
  - Game.Renderer 静态 → Renderer* 注入(上游为进程单例,等价);
  - IPaletteModifier 收集(上游 World.WorldActor.TraitsImplementing)→
    RefreshPalette 的 span 参数。
形态适配:三 List<IFinalizedRenderable> → 三 vector<RenderItem>(复用,
帧内零分配);event → 订阅表(成对注销,token 制);上游 Dispose 的
"World 所有权 HACK"(L473-482)不移植 —— C++ 侧 World 生存期归 sim 调用方,
析构只释放本类资源(COVERAGE 登记)。
The world renderer: palette management + world-to-screen conversion + the
render collection/sorting/draw order. This batch lands OPT-A7's "render
collection SOA + frame arena" (docs/OPTIMIZATION_TRACKER.md): upstream's
GenerateRenderables (L141-175) heap-allocates an IRenderable per source
item and then an IFinalizedRenderable per PrepareRender; C++ collects into
a reused vector<RenderItem> (POD value semantics — see renderable.hpp)
with the sort-key and segmentation buffers on a FrameArena (rewound at
frame end), and the overlay GroupBy(GetType()) HACK of Draw (L319-323)
becomes per-kind counting segmentation (first-encounter order between
segments, collection order within — item-for-item equal to LINQ's
two-level stable order). Injection surfaces (everything upstream's
constructor L53-79 pulls from the ModData/World trait reflection chain,
wired with Game/Map in Phase 5; the mechanics are fully ported):
TileSize/TileScale (upstream = map.Rules.TerrainInfo.TileSize /
map.Grid.TileScale) and enableDepthBuffer arrive via Desc; the Viewport
(upstream new Viewport(this, map); Viewport.cs leans on the whole Map
family of ProjectedCellRegion/Ramp/CellContaining) becomes the
IViewportSurface interface + SetViewport, with the full Viewport landing
with Phase 5's Map (registered in COVERAGE); the
ILoadsPalettes/ILoadsPlayerPalettes trait collection, the ScreenMap
queries, the OrderGenerator/Selection/Effects render sources, the
IRenderer/IRenderTerrain/IRenderAboveWorld/IRenderShroud trait families,
and DebugVisualizations arrive as Hooks/collection callbacks/interface
tables (no-op by default); the Game.Renderer static becomes an injected
Renderer* (upstream is a process singleton, equivalent); the IPaletteModifier
collection (upstream World.WorldActor.TraitsImplementing) becomes the span
parameter of RefreshPalette. Shape adaptations: the three
List<IFinalizedRenderable> → three reused vector<RenderItem> (zero
in-frame allocation); events → token-managed subscription tables (paired
unsubscribe); upstream Dispose's "world ownership HACK" (L473-482) is not
ported — World's lifetime belongs to the sim caller on the C++ side, and
the destructor frees only this class's own resources (registered in
COVERAGE).

## src/map/cell_layer.hpp

UPSTREAM: OpenRA.Game/Map/CellLayerBase.cs @1f594e320 L19-78(逐语义重写)+
         OpenRA.Game/Map/CellLayer.cs L18-174 +
         OpenRA.Game/Map/ProjectedCellLayer.cs L106-151
         C# CellLayer<T> 事件(CellEntryChanged)→ 回调表(construct 序
         通知;CopyValuesFrom/Clear 的监听者检查 = 逐事件判空)
         The C# CellLayer<T> events (CellEntryChanged) become callback
         lists (notification in registration order; the listener checks
         of CopyValuesFrom/Clear test the list for emptiness).

机制对照 / Mechanism mapping:
 - Index(CPos)(L56-69):方格直乘;等距 (u,v) 换算 + Bounds 含端点检查 →
   IndexOutOfRangeException 等价抛(上游消息固定)
   Index (CPos) (L56-69): rectangular multiplies directly; isometric
   (u,v) conversion + inclusive Bounds check → the equivalent
   IndexOutOfRangeException throw (fixed upstream message).

## src/map/cell_region.hpp

UPSTREAM: OpenRA.Game/Map/CellRegion.cs @1f594e320 L18-247(逐语义重写)+
         OpenRA.Game/Map/CellCoordsRegion.cs L15-368 +
         OpenRA.Game/Map/MapCoordsRegion.cs L17-89
         C# IEnumerable/yield → 零分配枚举器结构体(begin/end 迭代对;
         CellRegion 的列主序枚举序 = 上游 CellRegionEnumerator 逐行推进)
         The C# IEnumerable/yield surfaces become zero-allocation
         enumerator structs (begin/end iteration pairs; CellRegion's
         column-major enumeration order = the upstream
         CellRegionEnumerator row-by-row advance).

## src/map/map.cpp

UPSTREAM: OpenRA.Game/Map/Map.cs @1f594e320(map.hpp 的实现)
         The implementation of map.hpp.

## src/map/map.hpp

UPSTREAM: OpenRA.Game/Map/Map.cs @1f594e320 L29-1482(逐语义重写)
         Verbatim-semantics rewrite.

机制对照 / Mechanism mapping:
 - MapField 反射表(YamlFields)→ 逐字段 lambda 表(键/required/ignoreIfValue
   + Deserialize/Serialize 闭包;类型分派 NodeList/MiniYaml/标量与上游
   MapField.Type 三态一致)
   The MapField reflection table (YamlFields) becomes a per-field lambda
   table (key/required/ignoreIfValue + Deserialize/Serialize closures; the
   NodeList/MiniYaml/scalar dispatch matches MapField.Type's three states).
 - CellEntryChanged 事件 → 回调表(construct 序;UpdateRamp/UpdateProjection/
   InvalidateTerrainIndex 的挂载序照抄 —— 事件序即上游委托序)
   CellEntryChanged events become callback lists in registration order
(the UpdateRamp/UpdateProjection/InvalidateTerrainIndex wiring order
   is upstream's delegate order).
 - IReadOnlyFileSystem 面(Open/TryOpen/Exists/TryGetPackageContaining/
   IsExternalFile)逐语义;Package 优先、modFiles 兜底
   The IReadOnlyFileSystem face (Open/TryOpen/Exists/
   TryGetPackageContaining/IsExternalFile) verbatim; the package wins and
   modFiles is the fallback.
 - 依赖面:SequenceSet 构造(上游直接 new SequenceSet(this, modData, ...))
   → Params.fn_sequences_factory 注入(渲染线程/加载器链的装配面随嵌入侧
   ModData;COVERAGE 登记)
   Dependency face: the SequenceSet construction (upstream news it
   directly) rides Params.fn_sequences_factory injection — registered in
   COVERAGE.

## src/map/map_players.cpp

UPSTREAM: OpenRA.Game/Map/PlayerReference.cs @1f594e320 + MapPlayers.cs
         (map_players.hpp 的实现) | The implementation of map_players.hpp.

## src/map/map_players.hpp

UPSTREAM: OpenRA.Game/Map/PlayerReference.cs @1f594e320 L15-65(逐语义重写)+
         OpenRA.Game/Map/MapPlayers.cs L12-82(全量;ToMiniYaml 属编辑面
         随编辑器批) | Verbatim-semantics rewrite; MapPlayers.ToMiniYaml
         is an editor-face item landing with the editor batch.

机制对照 / Mechanism mapping:
 - PlayerReference 字段加载(FieldLoader.Load)→ 手写逐键(全默认;
   Color 默认 = Game.ModData.GetOrCreate<DefaultPlayer>().Color —— C++
   侧经注入的默认色回调承载,缺省红警默认色 HSL(145±)…… 实际为
   DefaultPlayer 的 Color 字段;本批以注入面缺省中性色 + 登记偏离)
   The PlayerReference field loading (FieldLoader.Load) is hand-written
   per key (all defaulted); the Color default (Game.ModData's
   DefaultPlayer.Color) rides an injected default-color callback.

## src/meta/dump_format.cpp

UPSTREAM: OpenRA.Game/FieldLoader.cs @1f594e320(dump_format.hpp 的实现;
         .NET float/WDist/枚举格式化复刻,见 hpp 头注)
         Implementation of dump_format.hpp — the .NET float/WDist/enum
         formatting replicas; see the hpp header notes.

## src/meta/dump_format.hpp

UPSTREAM: OpenRA.Game/FieldLoader.cs @1f594e320 L996-1018(dump 协议的值
         格式化 —— FieldSaver.FormatValue 语义面;协议由 schema_dumper
         --dump 与 tests/golden_rules 共享,与 C# 侧 Program.cs DumpValue
         逐分支对齐)
         The dump-protocol value formatter (the output spec shared by
         schema_dumper --dump and tests/golden_rules; branch-aligned with
         DumpValue of the C# Program.cs).

协议要点 / Protocol notes:
 - float:复刻 .NET float.ToString(InvariantCulture) 的最短往返 + 定点/科学
   阈值(科学当且仅当十进制指数 < -4 或 > 6;"E±dd" 大写两位指数)
   float: replicates the shortest-round-trip + fixed/scientific threshold
   of .NET float.ToString(InvariantCulture) (scientific iff the decimal
   exponent < -4 or > 6; uppercase "E±dd" with two exponent digits).
 - 字典/集合:按稳定序输出(键升序)——.NET Frozen* 枚举序是内部哈希序,
   两侧统一排序以保证对拍确定性
   Dictionaries/sets: stable output order (keys ascending) — the .NET
   Frozen* enumeration order is an internal hash order; both sides sort
   for comparison determinism.
 - null 引用(字符串/表达式/可空)→ "(null)";空容器 → "(empty)";
   嵌套记录 → "{类型名:{字段=值,…}}"
   null references (strings/expressions/nullables) → "(null)"; empty
   containers → "(empty)"; nested records → "{TypeName:{Field=value,…}}".

## src/meta/field_desc.hpp

UPSTREAM: OpenRA.Game/FieldLoader.cs @1f594e320 L937-1032(FieldLoadInfo/SerializeAttribute 体系)
         + OpenRA.Game/ObjectCreator.cs L78-137(类型按名实例化)
         The FieldLoadInfo/SerializeAttribute system + string-named type
         instantiation.

C++26 反射缺失下的等价机制 / The C++26 equivalent in the absence of reflection:
 - 上游用反射枚举类型字段(BuildTypeLoadInfo);此处每个 Info 类自带
   constexpr 字段描述表(kMetaFields),由 tools/schema_dumper 从 C# 程序集
   反射导出对照(tools/schema_dumper --gen),零漂移
   Upstream enumerates type fields via reflection; here every Info class
   carries a constexpr field-descriptor table (kMetaFields), cross-checked
   against C# reflection output from tools/schema_dumper (--gen), zero drift.
 - 上游 ObjectCreator 按字符串类名实例化;此处 TypeRegistry 以
   std::string_view → 工厂(返回 std::unique_ptr<TraitRecordBase> 的
   std::function)等价
   Upstream ObjectCreator instantiates by class-name string; the TypeRegistry
   maps std::string_view → factory (a std::function returning
   std::unique_ptr<TraitRecordBase>) as the equivalent.
 - [FieldLoader.LoadUsing] 的自定义加载器:描述表带 loader 名,语义在
   FieldLoader 内分派(第一版覆盖通用模式,其余登记偏离)
   [FieldLoader.LoadUsing] custom loaders: the descriptor carries the loader
   name and dispatch happens inside FieldLoader (the first version covers the
   common patterns; the rest are registered deviations).

## src/meta/field_loader.cpp

UPSTREAM: OpenRA.Game/FieldLoader.cs @1f594e320 L27-1032(field_loader.hpp 的实现)
         Implementation of field_loader.hpp.
分派结构 / Dispatch structure:
 - Load(L754-796):描述表字段循环 + MissingFieldsException;GeneratedRecord
   走值袋路径(LoadValueIntoValue 直写槽),手写类走 值解析 → AssignToMemory
   的两段路径
   Load (L754-796): the descriptor-field loop + MissingFieldsException;
   GeneratedRecord takes the bag path (LoadValueIntoValue writes slots
   directly), hand-written classes take the two-stage value-parse →
   AssignToMemory path.
 - 具体解析(ParseXxx 家族)全部在 generic_record.cpp 的 LoadValueIntoValue
   (唯一实现;错误消息与上游 GetValue/ParseXxx 逐字一致)
   The concrete parsing (the ParseXxx family) lives entirely in
   LoadValueIntoValue of generic_record.cpp (the single implementation;
   error messages verbatim from upstream GetValue/ParseXxx).
 - InvalidValueAction/UnknownFieldAction 可替换委托 → 全局函数
   (默认实现消息逐字;lint 侧替换行为随后续阶段接入)
   The replaceable InvalidValueAction/UnknownFieldAction delegates →
   global functions (verbatim default messages; linter replacements
   arrive with later phases).

## src/meta/field_loader.hpp

UPSTREAM: OpenRA.Game/FieldLoader.cs @1f594e320 L27-1032(全文件逐语义重写)
         Full-file statement-by-statement rewrite of upstream FieldLoader.cs.

机制对照 / Mechanism mapping:
 - C# 反射(BuildTypeLoadInfo 收集 Public|NonPublic 字段 + SerializeAttribute)
   → RecordDesc::fields(schema_dumper 从 C# 反射导出,零漂移;加载沿 base 链
   拼接,顺序 = C# GetFields:最远基类字段在前)
   C# reflection (BuildTypeLoadInfo collecting Public|NonPublic fields +
   SerializeAttribute) → RecordDesc::fields (exported from C# reflection by
   schema_dumper, zero drift; loading concatenates along the base chain in
   C# GetFields order: base-most fields first).
 - TypeParsers/GenericType*Parsers 的 FrozenDictionary 分派 → FieldType switch
   The FrozenDictionary dispatch of TypeParsers/GenericType*Parsers → a
   FieldType switch.
 - InvalidValueAction/UnknownFieldAction 可替换委托 → 全局 std::function
   (默认实现消息逐字;lint 侧替换行为随后续阶段接入)
   The replaceable InvalidValueAction/UnknownFieldAction delegates → global
   std::function objects (verbatim default messages; linter replacements
   arrive with later phases).

已登记偏离(docs/COVERAGE.md)/ Registered deviations:
 - string 字段 null 与 "" 合流(C# string 可 null;dump/FormatValue 层两者
   均为 "",运行时 null 判断在 trait 移植时以 empty() 等价改写)
   string null and "" conflate (C# strings are nullable; both dump/format
   as "" — runtime null checks become empty() checks as traits are ported).
 - TypeConverter 兜底路径未实现(trait/weapon 加载链字段类型已被
   TypeParsers + 枚举全覆盖,兜底不可达)
   The TypeConverter fallback is not implemented (the trait/weapon loading
   surface is fully covered by TypeParsers + enums; the fallback is
   unreachable there).

## src/meta/field_saver.cpp

UPSTREAM: OpenRA.Game/FieldSaver.cs @1f594e320 L26-166(实现部分;头注见
         field_saver.hpp)
         The implementation half of FieldSaver.cs L26-166 (the header
         note lives in field_saver.hpp).

## src/meta/field_saver.hpp

UPSTREAM: OpenRA.Game/FieldSaver.cs @1f594e320 L26-166 全文(值袋承载形
         态:反射面 → RecordDesc 描述表;Settings 只存差异的序列化面)
         The whole of FieldSaver.cs L26-166 (the value-bag form: the
         reflection faces → the RecordDesc tables; the save-differences
         serialization face of Settings).

机制对照 / Mechanism mapping:
 - GetTypeLoadInfo 的反射字段枚举 → CollectFields(desc)(base 链序 = C#
   GetFields 序)
   GetTypeLoadInfo's reflective field enumeration → CollectFields(desc)
   (the base-chain order = C# GetFields order).
 - FormatValue 的 TypeConverter/ToString 兜底链 → desc.type 判别的值袋
   分支(null → "";BitSet → 注册标签名 ", " 连接;bool → True/False)
   FormatValue's TypeConverter/ToString fallback chain → the value-bag
   branches discriminating on desc.type (null → ""; BitSet → the
   registered tag names joined ", "; bool → True/False).
 - Save 的字典子节点形态保留(上游 settings 键段);DateTime/Vector2/3 与
   文档用字典串接面 = 值袋无此载荷,静态断言于 FieldType 域外
   Save's dictionary sub-node form is kept (the settings key sections);
   DateTime/Vector2/3 and the doc-only dictionary-concat faces have no
   value-bag payloads — outside the FieldType domain.

## src/meta/generic_record.cpp

UPSTREAM: OpenRA.Game/FieldLoader.cs @1f594e320(generic_record.hpp 的实现;
         解析分派结构承自 FieldLoader.cs)
         L146-752(GetValue + ParseXxx 家族),与 field_loader.cpp 的
         LoadValueInto 同源 —— 本文件是唯一解析实现(值形态),
         内存路径经 AssignToMemory 二次转换。
         Implementation of generic_record.hpp; the dispatch structure
         inherits FieldLoader.cs L146-752 (GetValue + the ParseXxx family)
         and shares its origin with LoadValueInto in field_loader.cpp —
         this file is the single parse implementation (value shape); the
         memory path converts a second time via AssignToMemory.

## src/meta/generic_record.hpp

UPSTREAM: OpenRA.Game/FieldLoader.cs @1f594e320 L937-975(schema_dumper --gen
         的运行时载体 —— C# 反射对象模型 FieldLoadInfo 的值形态等价)
         The runtime carrier of schema_dumper --gen output — the
         value-shaped equivalent of the C# reflection FieldLoadInfo model.

机制对照 / Mechanism mapping:
 - 601 个可加载类型(Trait/Warhead/Projectile 普查)不逐一手写 C++ 类:
   描述表 + 默认值表由 schema_dumper 从 C# 反射导出(gen/ 源),运行时以
   GeneratedRecord(按 CollectFields 序对齐的值袋)承载 —— dump/lint/
   resolved-rules 全走值袋;Phase 5 手写 Info 类(offset 路径)按需替换工厂
   The 601 loadable types (the Trait/Warhead/Projectile survey) are not
   hand-written one by one: descriptor + default tables are exported from C#
   reflection by schema_dumper (gen/ sources) and carried at runtime by
   GeneratedRecord (a value bag aligned to the CollectFields order) —
   dump/lint/resolved-rules all go through the bag; Phase 5 hand-written Info
   classes (the offset path) replace factories on demand.
 - Opaque 字段(上游无 TypeParser 的对象字段,如 WeaponInfo 引用/Lazy<T>):
   保留描述条目与值槽(恒 null),yaml 出现该键时走 DefaultUnknownField
   —— 与上游 GetValue 找不到解析器时的报错行为逐字一致
   Opaque fields (upstream object fields without a TypeParser, e.g.
   WeaponInfo references / Lazy<T>): keep the descriptor entry and a value
   slot (permanently null); a yaml key hitting them goes through
   DefaultUnknownField — verbatim identical to upstream GetValue failing to
   find a parser.

## src/meta/parse.cpp

UPSTREAM: OpenRA.Game/Exts.cs @1f594e320 L485-566 + OpenRA.Game/FieldLoader.cs L146-591
         (parse.hpp 的实现;BCL 语义注记见头文件)
         Implementation of parse.hpp (BCL semantics documented in the header).

## src/meta/parse.hpp

UPSTREAM: OpenRA.Game/Exts.cs @1f594e320 L485-566(Parse*/TryParse*Invariant 家族)
         + FieldLoader.cs L146-591(标量 ParseXxx 委托体)
         The Parse*/TryParse*Invariant family + the scalar ParseXxx delegate
         bodies of FieldLoader.

BCL 语义复刻要点 / BCL semantics being replicated:
 - NumberStyles.Integer:允许首尾 Unicode 空白 + 前导符号,十进制整串,溢出即失败
   NumberStyles.Integer: leading/trailing Unicode whitespace + leading sign,
   decimal, whole-string, overflow fails.
 - NumberStyles.Float:上述 + 小数点/指数;C# 仅接受字面 "Infinity"/"-Infinity"/"NaN"
   (区分大小写),from_chars 更宽(接受任意大小写 inf/nan)——结果非有限时按
   C# 符号集回验收紧(偏离表已登记的无穷角落不可达,双保险)
   NumberStyles.Float: the above + decimal point/exponent; C# only accepts
   the literal "Infinity"/"-Infinity"/"NaN" (case-sensitive), while
   from_chars is looser — non-finite results are re-validated against the C#
   symbol set (belt-and-braces for an unreachable corner).
 - float 结果与 C# 逐位一致:两侧均为最短往返正确舍入解析
   float results are bit-identical to C#: both sides parse with
   correctly-rounded shortest-round-trip semantics.

## src/meta/type_registry.cpp

UPSTREAM: OpenRA.Game/ObjectCreator.cs @1f594e320 L21-168(type_registry.hpp 的实现)
         Implementation of type_registry.hpp.

双键注册 / Dual-key registration:
 - 简单名键(ObjectCreator.FindType 的 Namespace+"."+className 匹配面;
   同名类后注册覆盖 —— 与上游"首个命名空间命中"近似,冲突清单在
   schema_dumper --gen 的警告输出)
   The simple-name key (the Namespace+"."+className match surface of
   ObjectCreator.FindType; same-named classes overwrite on later
   registration — approximating the upstream first-namespace hit, with the
   conflict list in the schema_dumper --gen warnings).
 - 全名键(str_full_name;嵌套同名类如两族 ResourceTypeInfo 的精确寻址,
   loader 侧使用)
   The full-name key (str_full_name; exact addressing of nested same-named
   classes like the two ResourceTypeInfo families, used by loaders).

## src/meta/type_registry.hpp

UPSTREAM: OpenRA.Game/ObjectCreator.cs @1f594e320 L21-168(按名实例化/类型查找)
         + OpenRA.Game/Traits/TraitsInterfaces.cs L342-351(TraitInfo 基类)
         Name-based instantiation/type lookup + the TraitInfo base class.

C# 侧程序集反射扫描(FindType/GetTypesImplementing)在此替换为静态注册表:
每个 Info 类在 gen/ 注册源中登记(RecordDesc + 工厂),名字即 yaml 中的
trait 名 + "Info" 后缀(ActorInfo.LoadTraitInfo 的拼名约定)。
The C# assembly-reflection scan (FindType/GetTypesImplementing) is replaced
by a static registry: every Info class registers (RecordDesc + factory) in
the gen/ registration sources, keyed by the yaml trait name + "Info" suffix
(the name-joining convention of ActorInfo.LoadTraitInfo).

## src/meta/variable_expression.cpp

UPSTREAM: OpenRA.Game/Support/VariableExpression.cs @1f594e320 L22-983
         (variable_expression.hpp 的实现;tokenizer/校验/后缀化/求值逐行对照)
         Implementation of variable_expression.hpp (tokenizer/validation/
         postfix/evaluation line-checked against upstream).
已登记偏离(docs/COVERAGE.md)/ Registered deviations:
 - 错误消息中的 index 按 UTF-8 字节计(C# 按 UTF-16 code unit);
   ASCII 输入下完全一致,mods 表达式均为 ASCII
   Indices in error messages count UTF-8 bytes (C# counts UTF-16 code
   units); identical for ASCII input, and mods expressions are ASCII.

## src/meta/variable_expression.hpp

UPSTREAM: OpenRA.Game/Support/VariableExpression.cs @1f594e320 L22-983(全文件逐语义重写)
         Full-file statement-by-statement rewrite of upstream
         VariableExpression.cs.

语义要点 / Semantic notes:
 - 构造即完成 tokenize + 语法校验 + 后缀化;所有构造期错误消息逐字一致
   (C# InvalidDataException 的 Message 在 FieldLoader 侧被包上
   "FieldLoader: Cannot parse ..." 前缀后重抛,此处保留同构异常链)
   Construction performs tokenizing + syntax validation + postfix
   conversion; every construction-time error message is verbatim (the C#
   InvalidDataException message gets the "FieldLoader: Cannot parse ..."
   prefix added on rethrow, and this side keeps the same exception chain).
 - 求值:C# 经 System.Linq.Expressions 编译为委托;此处以同构后缀栈机求值,
   语义逐条对应(非短路 And/Or、除零/模零得 0、int↔bool 隐式互转:
   int→bool 非 0 即真,bool→int 1/0)
   Evaluation: C# compiles to a delegate via System.Linq.Expressions; this
   side evaluates with an isomorphic postfix stack machine (non-short-circuit
   And/Or, divide/modulo by zero yield 0, implicit int↔bool conversions:
   int→bool tests non-zero, bool→int yields 1/0).
 - Variables 集合按首次出现序去重(上游 HashSet 插入序)
   The Variables set deduplicates in first-appearance order (the upstream
   HashSet insertion order).

## src/mods/actor_exts.hpp

UPSTREAM: OpenRA.Mods.Common/ActorExts.cs @1f594e320 L21-83(逐语义重写;
         AppearsHostileTo 已随第四批落地;ClosestCell 随消费批)+ WorldUtils.cs L26-45
         的 ClosestToIgnoringPath 位置面
         Verbatim-semantics rewrite (AppearsHostileTo/ClosestCell land
         with their consumers) + the ClosestToIgnoringPath position face
         of WorldUtils.cs L26-45.

## src/mods/affects_shroud.cpp

UPSTREAM: OpenRA.Mods.Common/Traits/AffectsShroud.cs + RevealsShroud.cs
         (实现部分) | The implementation half.

## src/mods/affects_shroud.hpp

UPSTREAM: OpenRA.Mods.Common/Traits/AffectsShroud.cs @1f594e320 L18-174 +
         RevealsShroud.cs L18-72(全文逐语义重写)
         The whole of AffectsShroud.cs L18-174 + RevealsShroud.cs
         L18-72.

机制对照 / Mechanism mapping:
 - abstract AffectsShroud + abstract Info → AffectsShroudBase<Derived>
   模板基(CRTP;ConditionalTraitCore 同构);Derived 提供
   AddCellsToPlayerShroud/RemoveCellsFromPlayerShroud 并可覆写 Range
   The abstract AffectsShroud + abstract Info → the AffectsShroudBase<
   Derived> template base (CRTP; the ConditionalTraitCore isomorph);
   the Derived class supplies AddCellsToPlayerShroud/
   RemoveCellsFromPlayerShroud and may override Range.
 - ProjectedCells 的 HashSet<PPos> 暂存(Footprint 型)→ vector 去重并
   (UnionWith 语义:无序并集;排序仅影响遍历序,AddSource 对格序不敏感)
   ProjectedCells' HashSet<PPos> staging (the Footprint form) → a
   dedup-merged vector (UnionWith semantics: an unordered union; the
   order affects traversal alone, and AddSource is cell-order
   insensitive).
 - VisibilityType 枚举:gen/enums_gen.cpp 已注册,int 槽承载
   The VisibilityType enum: registered in gen/enums_gen.cpp, carried by
   an int slot.

## src/mods/ai_utils.cpp

UPSTREAM: OpenRA.Mods.Common/AIUtils.cs @1f594e320(实现部分)
         The implementation half.

## src/mods/ai_utils.hpp

UPSTREAM: OpenRA.Mods.Common/AIUtils.cs @1f594e320 L63-95 的
         ClearBlockersOrders(AI 面的其余成员随 Phase 8 bot 模块批;
         Aircraft 分支未移植 = 无该 trait 世界的等价域 —— 落
         `continue` 的 else 同构)
         AIUtils.cs L63-95's ClearBlockersOrders (the rest of the AI
         face rides the Phase 8 bot-module batch; the Aircraft branch
         unported = the equivalent domain of a world without that trait
         — landing in the `continue` else-shape).

## src/mods/air_activities.cpp

UPSTREAM: OpenRA.Mods.Common/Activities/Air/Fly.cs @1f594e320 +
         TakeOff.cs + Land.cs + FlyOffMap.cs + FlyIdle.cs +
         FlyForward.cs + FlyFollow.cs + ReturnToBase.cs + FallToEarth.cs
         + ../Resupply.cs 的实现面

## src/mods/air_activities.hpp

UPSTREAM: OpenRA.Mods.Common/Activities/Air/Fly.cs @1f594e320 +
         TakeOff.cs + Land.cs + FlyOffMap.cs + FlyIdle.cs +
         FlyForward.cs + FlyFollow.cs + ReturnToBase.cs + FallToEarth.cs
         + ../Resupply.cs

## src/mods/air_support.cpp

UPSTREAM: OpenRA.Mods.Common/Traits/AmmoPool.cs + Rearmable.cs +
         Repairable.cs + RepairsUnits.cs + Air/FallsToEarth.cs 的实现面

## src/mods/air_support.hpp

UPSTREAM: OpenRA.Mods.Common/Traits/AmmoPool.cs + Rearmable.cs +
         Repairable.cs + RepairsUnits.cs + Air/FallsToEarth.cs
         @1f594e320

## src/mods/aircraft.cpp

UPSTREAM: OpenRA.Mods.Common/Traits/Air/Aircraft.cs @1f594e320 的实现面

## src/mods/aircraft.hpp

UPSTREAM: OpenRA.Mods.Common/Traits/Air/Aircraft.cs @1f594e320 L26-1381

## src/mods/airstrike_power.cpp

UPSTREAM: OpenRA.Mods.Common/Traits/SupportPowers/DirectionalSupportPower.cs
         + AirstrikePower.cs + SelectDirectionalTarget.cs @1f594e320
         的实现面

## src/mods/airstrike_power.hpp

UPSTREAM: OpenRA.Mods.Common/Traits/SupportPowers/DirectionalSupportPower.cs
         + AirstrikePower.cs + SelectDirectionalTarget.cs @1f594e320

## src/mods/armament.cpp

UPSTREAM: OpenRA.Mods.Common/Traits/Armament.cs @1f594e320 L22-431 实现部分
         (the implementation half)。

## src/mods/armament.hpp

UPSTREAM: OpenRA.Mods.Common/Traits/Armament.cs @1f594e320 L22-431 全文
         (逐语义重写)
         The whole of Armament.cs L22-431 (verbatim-semantics rewrite).

机制对照 / Mechanism mapping:
 - PausableConditionalTrait<ArmamentInfo> → sim::ConditionalTraitCore<
   Armament> 组合(同 Mobile 头注)
   PausableConditionalTrait<ArmamentInfo> → the
   sim::ConditionalTraitCore<Armament> composition (see Mobile's header).
 - OPT-A9 开火链零分配:上游每发 CheckFire 的 LINQ ToArray
   (rangeModifiers/damageModifiers/inaccuracyModifiers)→ Created 一次性
   解析 trait 指针 + 每发回填的成员定长缓冲(span 直传);
   delayedActions 的闭包 Action<int> → 成员函数 + 载荷,零闭包分配
   OPT-A9's zero-allocation firing chain: upstream's per-shot LINQ
   ToArray (rangeModifiers/damageModifiers/inaccuracyModifiers) → the
   trait pointers resolved once at Created + member fixed buffers
   refilled per shot (span-passed); delayedActions' Action<int> closures
   → member functions + payloads, zero closure allocation.
 - ArmamentInfo.RulesetLoaded 的武器解析/校验(L92-110)→ 工厂解析面
   (时点差异同 Health D 系;异常文本逐字;ModifiedRange 的
   IRangeModifierInfo 集空 = 无修正上游等价)
   ArmamentInfo.RulesetLoaded's weapon resolution/validation (L92-110) →
   the factory-parse face (the timing difference like Health's D series;
   exception texts verbatim; ModifiedRange's empty IRangeModifierInfo
   set = upstream's no-modifier equivalent).
 - Turreted/Hovers trait 面未移植:查询空集的上游等价分支(turret ==
   null → 体朝向;hovers == null → 无悬浮偏移;COVERAGE 登记)
   The Turreted/Hovers trait faces unported: the empty-query upstream-
   equivalent branches (turret == null → the body orientation;
   hovers == null → no hover offset; registered in COVERAGE).
 - Game.Sound.Play:声音门面注入面(Deps;Phase 6 接线前 = 空实现的
   上游不可观测面 —— 上游依赖 Sound 系统已初始化)
   Game.Sound.Play: the sound-facade injection face (Deps; before the
   Phase 6 wiring = the upstream-unobservable no-op face — upstream
   presumes the Sound system initialized).

## src/mods/armor.cpp

UPSTREAM: OpenRA.Mods.Common/Traits/Armor.cs 实现部分
         The implementation half.

## src/mods/armor.hpp

UPSTREAM: OpenRA.Mods.Common/Traits/Armor.cs @1f594e320 L17-30 全文
         (ArmorType 标签在 hit_shape.hpp —— BitSet 注册面的就近锚)
         The whole of Armor.cs L17-30 (the ArmorType tag lives in
         hit_shape.hpp — anchored next to the BitSet registration face).

## src/mods/asset_browser_logic.cpp

UPSTREAM: OpenRA.Mods.Common/Widgets/Logic/AssetBrowserLogic.cs
         @1f594e320(实现;头注见 asset_browser_logic.hpp)
         Implementation; the header notes live in asset_browser_logic.hpp.

## src/mods/asset_browser_logic.hpp

UPSTREAM: OpenRA.Mods.Common/AssetBrowser.cs @1f594e320 L16-22(IGlobal
         ModData 的四扩展名表;mod.yaml "AssetBrowser" 节点装载)+
         OpenRA.Mods.Common/Widgets/Logic/AssetBrowserLogic.cs L26-770
         全文(源包下拉/五类型过滤/帧滑条与播放三键/sprite-voxel-
         audio-video 四装载路径/调色板下拉/静音对)。
         AssetBrowser.cs L16-22 (the IGlobalModData four-extension table;
         the mod.yaml "AssetBrowser" node load) + the whole of
         AssetBrowserLogic.cs L26-770 (the source-package dropdown / the
         five type filters / the frame slider and the play trio / the
         sprite-voxel-audio-video load paths / the palette dropdown / the
         mute pair).

## src/mods/attack_activity.cpp

UPSTREAM: OpenRA.Mods.Common/Activities/Attack.cs 实现部分
         The implementation half.

## src/mods/attack_activity.hpp

UPSTREAM: OpenRA.Mods.Common/Activities/Attack.cs @1f594e320 L22-283 全文
         (逐语义重写;TargetLineNodes 渲染面随 Phase 6 —— 活动基类未引入)
         The whole of Attack.cs L22-283 (verbatim-semantics rewrite;
         TargetLineNodes' render face lands with Phase 6 — not carried
         by the Activity base).

机制对照 / Mechanism mapping:
 - attackTraits 的 IEnumerable<AttackBase>(非禁用过滤)→ 构造期物化
   向量;armaments 的 LINQ 物化(ToList)→ 复用调用点局部 vector
   attackTraits' IEnumerable<AttackBase> (the non-disabled filter) →
   the construction-time materialized vector; armaments' LINQ ToList →
   a reused call-site local vector.
 - StanceChanged 的 autoTarget.HasValidTargetPriority 消费 = AutoTarget
   具体类(本批挂点换实)
   StanceChanged's autoTarget.HasValidTargetPriority consumption = the
   AutoTarget concrete class (this batch's wiring).

## src/mods/attack_aircraft.cpp

UPSTREAM: OpenRA.Mods.Common/Traits/Air/AttackAircraft.cs +
         AttackBomber.cs @1f594e320 + Activities/Air/FlyAttack.cs 的实现面

## src/mods/attack_aircraft.hpp

UPSTREAM: OpenRA.Mods.Common/Traits/Air/AttackAircraft.cs +
         AttackBomber.cs @1f594e320 + Activities/Air/FlyAttack.cs

## src/mods/attack_base.cpp

UPSTREAM: OpenRA.Mods.Common/Traits/Attack/AttackBase.cs 实现部分
         + AttackFrontal.cs | The implementation half.

## src/mods/attack_base.hpp

UPSTREAM: OpenRA.Mods.Common/Traits/Attack/AttackBase.cs @1f594e320
         L23-526 全文(逐语义重写;AttackFrontal.cs L18-48 同文件承载)
         The whole of AttackBase.cs L23-526 + AttackFrontal.cs L18-48
         (verbatim-semantics rewrite).

机制对照 / Mechanism mapping:
 - abstract AttackBase + PausableConditionalTrait → AttackBaseCore<
   Derived> 模板基(CRTP;ConditionalTraitCore 同构 —— 与 AffectsShroud
   同法);具体类(AttackFrontal/后续 AttackFollow 族)挂 TypeId
   The abstract AttackBase + PausableConditionalTrait → the
   AttackBaseCore<Derived> template base (CRTP; the ConditionalTraitCore
   isomorph — the same device as AffectsShroud); the concrete classes
   (AttackFrontal / the later AttackFollow family) carry the TypeId.
 - getArmaments 的 Func<IEnumerable<Armament>> 闭包 → Created 一次性
   过滤物化(上游 InitializeGetArmaments 的 ToArray 缓存同值)
   getArmaments' Func<IEnumerable<Armament>> closure → the one-shot
   filtered materialization at Created (the same value as upstream's
   InitializeGetArmaments ToArray cache).
 - OrderBy(IsTraitPaused).ThenByDescending(MaxRange).First 的稳定序 →
   单遍择优(键 (paused 升序, range 降序);平键首见者胜 = 稳定序等价)
   OrderBy(IsTraitPaused).ThenByDescending(MaxRange).First's stable
   order → a single-pass pick (the key (paused ascending, range
   descending); the first-seen wins ties — the stable-order
   equivalent).
 - 上游 Cursor 的 string ?? 回退:空串 = null(游标名非空域)
   Upstream's string ?? cursor fallback: an empty string = null (the
   cursor-name domain is non-empty).
 - OnStopOrder 的 Resupply/ReturnToBase 活动判:两类活动未移植 → 恒 false
   的上游等价分支(COVERAGE 登记)
   OnStopOrder's Resupply/ReturnToBase activity checks: both activity
   classes are unported → upstream's constantly-false equivalent branch
   (registered in COVERAGE).

## src/mods/attack_follow.cpp

UPSTREAM: OpenRA.Mods.Common/Traits/Attack/AttackFollow.cs 实现部分
         + AttackTurreted.cs | The implementation half.

## src/mods/attack_follow.hpp

UPSTREAM: OpenRA.Mods.Common/Traits/Attack/AttackFollow.cs @1f594e320
         L22-466 全文 + AttackTurreted.cs L18-51 全文(逐语义重写;
         嵌套 AttackActivity 以 activities::AttackFollowActivity 承载)
         The whole of AttackFollow.cs L22-466 + the whole of
         AttackTurreted.cs L18-51 (verbatim-semantics rewrites; the
         nested AttackActivity rides activities::AttackFollowActivity).

机制对照 / Mechanism mapping:
 - C# 继承链 AttackFollow : AttackBase → AttackFollow :
   AttackBaseCore<AttackFollow>(CRTP;AttackTurreted 再继承 AttackFollow
   —— 基类接口集在 AttackTurreted 的 upcast 表重列,Classic 同法)
   C#'s AttackFollow : AttackBase chain → AttackFollow :
   AttackBaseCore<AttackFollow> (CRTP; AttackTurreted then derives from
   AttackFollow — the base's interface set is re-listed in
   AttackTurreted's upcast table, Classic's device).
 - AttackFollowInfo 的 `new` Info 隐藏 → 基类切片拷贝 + 派生字段成员
   (info 不可变;两份数据同值)
   AttackFollowInfo's `new` Info hiding → the base-slice copy + the
   derived-field member (info is immutable; both copies hold the same
   values).

## src/mods/auto_target.cpp

UPSTREAM: OpenRA.Mods.Common/Traits/AutoTarget.cs + AutoTargetPriority.cs
         (实现部分) | The implementation half.

## src/mods/auto_target.hpp

UPSTREAM: OpenRA.Mods.Common/Traits/AutoTarget.cs @1f594e320 L21-487 +
         AutoTargetPriority.cs L16-40(全文逐语义重写;IEditorActorOptions
         的编辑器 UI 面随 Phase 6)
         The whole of AutoTarget.cs L21-487 + AutoTargetPriority.cs
         L16-40 (verbatim-semantics rewrites; the IEditorActorOptions
         editor-UI face lands with Phase 6).

机制对照 / Mechanism mapping:
 - FrozenDictionary<UnitStance, string> ConditionByStance → std::map<
   UnitStance, string>(键序稳定;TryGetValue 语义不变)
   The FrozenDictionary<UnitStance, string> ConditionByStance →
   std::map<UnitStance, string> (a stable key order; TryGetValue
   semantics unchanged).
 - ChooseTarget 的 IEnumerable 惰性链(Select/Concat/Where)→ 两段物化
   循环(actor 域 + frozen 域;PERF 注释的 validPriorities 复用列表 →
   成员缓冲)
   ChooseTarget's lazy IEnumerable chains (Select/Concat/Where) → the
   two materialized loops (the actor domain + the frozen domain; the
   PERF-noted validPriorities reuse list → a member buffer).
 - Passenger trait 未移植:attacker 换乘 transport 面恒不触发(空集
   等价分支;COVERAGE 登记)
   The Passenger trait is unported: the attacker-swaps-to-transport face
   never triggers (the empty-set equivalent branch; registered in
   COVERAGE).

## src/mods/blocks_projectiles.cpp

UPSTREAM: OpenRA.Mods.Common/Traits/BlocksProjectiles.cs 实现部分
         The implementation half.

## src/mods/blocks_projectiles.hpp

UPSTREAM: OpenRA.Mods.Common/Traits/BlocksProjectiles.cs @1f594e320
         L17-75 全文(逐语义重写)
         The whole of BlocksProjectiles.cs L17-75.

## src/mods/body_orientation.cpp

UPSTREAM: OpenRA.Mods.Common/Traits/BodyOrientation.cs @1f594e320 L18-127
         实现部分(the implementation half)。

## src/mods/body_orientation.hpp

UPSTREAM: OpenRA.Mods.Common/Traits/BodyOrientation.cs @1f594e320 L18-127
         全文(逐语义重写)
         The whole of BodyOrientation.cs L18-127 (verbatim-semantics
         rewrite).

机制对照 / Mechanism mapping:
 - Info 侧(值袋无方法)→ BodyOrientationInfoData 解析值 + 数据方法;
   LocalToWorld/QuantizeOrientation/QuantizeFacing 的算式随数据对象
   The Info side (the bag is method-free) → the parsed
   BodyOrientationInfoData + its data methods; the LocalToWorld/
   QuantizeOrientation/QuantizeFacing formulas ride the data object.
 - QuantizedFacings 的 Lazy → 首查物化(同求值时点;IQuantizeBodyOrientation
   实现批未至时按上游 qboi==null 抛路径,异常文本逐字)
   The Lazy QuantizedFacings → first-query materialization (the same
   evaluation moment; until the IQuantizeBodyOrientation implementing
   batch arrives this follows upstream's qboi==null throw path, with
   verbatim exception texts).

## src/mods/browser_logics.cpp

UPSTREAM: OpenRA.Mods.Common/Widgets/Logic/ReplayBrowserLogic.cs +
         ReplayUtils.cs + GameSaveUtils.cs(see browser_logics.hpp)。

## src/mods/browser_logics.hpp

UPSTREAM: OpenRA.Mods.Common/Widgets/Logic/ReplayBrowserLogic.cs
         @1f594e320 L29-883 全文 +
         OpenRA.Mods.Common/Widgets/Logic/ReplayUtils.cs L17-79 全文 +
         GameSaveUtils.cs L19-82 全文 +
         GameSaveBrowserLogic.cs L26-599 全文 +
         MissionBrowserLogic.cs L25-631 全文 +
         MapChooserLogic.cs L23-660 全文 +
         MusicPlayerLogic.cs L19-170 全文 +
         Lobby/LobbyUtils.cs L195-205(SplitOnFirstToken)。

## src/mods/building.cpp

UPSTREAM: OpenRA.Mods.Common/Traits/Buildings/Building.cs 实现部分
         | The implementation half.

## src/mods/building.hpp

UPSTREAM: OpenRA.Mods.Common/Traits/Buildings/Building.cs @1f594e320
         L22-355 全文(LoadFootprint 的 yaml 面已由 game/loaders.cpp 的
         RegisterLoadFootprint 前批落地 —— 本文件消费值袋槽)
         The whole of Building.cs L22-355 (LoadFootprint's yaml face
         already landed with game/loaders.cpp's RegisterLoadFootprint in
         an earlier batch — this file consumes the value-bag slot).

机制对照 / Mechanism mapping:
 - Footprint 的 FrozenDictionary<CVec, FootprintCellType> → 插入序
   vector<pair<CVec, FootprintCellType>>(loader 的 dict 槽 = 插入序,
   y 外 x 内的行主序;FootprintTiles 的枚举序 = 上游字典首建序等价)
   Footprint's FrozenDictionary<CVec, FootprintCellType> → the
   insertion-ordered vector<pair<CVec, FootprintCellType>> (the
   loader's dict slot is insertion-ordered, row-major x-within-y;
   FootprintTiles' enumeration order is the equivalent of upstream's
   dictionary build order).
 - IsCloseEnoughToBase 的 RequiresBuildableAreaInfo/MapBuildRadius/
   BaseProvider/GivesBuildableArea 未移植:RequiresBuildableAreaInfo
   == null 的真分支(上游无该 trait 世界同值;空集等价面 —— 登记
   COVERAGE)
   IsCloseEnoughToBase's RequiresBuildableAreaInfo/MapBuildRadius/
   BaseProvider/GivesBuildableArea are unported: the true branch of
   RequiresBuildableAreaInfo == null (the same value upstream gives in
   a world without the trait; the empty-set equivalent face —
   registered in COVERAGE).
 - RemoveSmudges 走 SmudgeLayer 记账面(本批);BuildSounds/
   UndeploySounds 的 Game.Sound.PlayToPlayer 随声音注入面
   RemoveSmudges goes through SmudgeLayer's bookkeeping face (this
   batch); BuildSounds/UndeploySounds' Game.Sound.PlayToPlayer rides
   the sound injection face.

## src/mods/building_influence.cpp

UPSTREAM: OpenRA.Mods.Common/Traits/Buildings/BuildingInfluence.cs
         (实现部分) | The implementation half.

## src/mods/building_influence.hpp

UPSTREAM: OpenRA.Mods.Common/Traits/Buildings/BuildingInfluence.cs @1f594e320
         L20-92 全文(世界 actor 的建筑影响层链表)
         The whole of L20-92 (the world actor's building-influence
         linked layer).

## src/mods/building_utils.cpp

UPSTREAM: OpenRA.Mods.Common/Traits/Buildings/BuildingUtils.cs @1f594e320
         (实现部分)
         The implementation half.

## src/mods/building_utils.hpp

UPSTREAM: OpenRA.Mods.Common/Traits/Buildings/BuildingUtils.cs @1f594e320
         L21-139 全文(world 扩展形态)
         The whole of BuildingUtils.cs L21-139 (the world-extension
         form).

## src/mods/bulk_production_queue.cpp

UPSTREAM: OpenRA.Mods.Common/Traits/Player/BulkProductionQueue.cs @1f594e320
         (实现部分)
         The implementation half.

## src/mods/bulk_production_queue.hpp

UPSTREAM: OpenRA.Mods.Common/Traits/Player/BulkProductionQueue.cs @1f594e320
         L20-337 全文(同步面;ProductionBulkAirdrop 未移植 ——
         DeliveryHasArrived 的 producer 恒空集 = 无该 trait 世界的等价
         域,d2k 全链随 Phase 8;登记 COVERAGE)
         The whole synchronous face of BulkProductionQueue.cs L20-337
         (ProductionBulkAirdrop unported — DeliveryHasArrived's
         producer stays the empty set = the equivalent domain of a
         world without that trait; d2k's full chain rides Phase 8;
         registered in COVERAGE).

## src/mods/capture.cpp

UPSTREAM: OpenRA.Mods.Common/Traits/CaptureManager.cs @1f594e320
         L21-293 + Capturable.cs L15-50 + Captures.cs L15-173 +
         GivesCashOnCapture.cs L14-62 + Activities/Enter.cs L11-163 +
         Activities/CaptureActor.cs L11-135(逐语句重写;机制对照见
         capture.hpp 头注)
         Statement-by-statement; the mechanism mapping lives in
         capture.hpp's header note.

## src/mods/capture.hpp

UPSTREAM: OpenRA.Mods.Common/Traits/CaptureManager.cs @1f594e320
         L21-293 全文 + Capturable.cs L15-50 全文 + Captures.cs L15-173
         全文 + GivesCashOnCapture.cs L14-62 全文 +
         Activities/Enter.cs L11-163 全文 + Activities/CaptureActor.cs
         L11-135 全文
         The whole of CaptureManager.cs L21-293 + Capturable.cs L15-50 +
         Captures.cs L15-173 + GivesCashOnCapture.cs L14-62 +
         Activities/Enter.cs L11-163 + Activities/CaptureActor.cs
         L11-135.

机制对照 / Mechanism mapping:
 - CaptureProgressBar/CapturableProgressBar/CapturableProgressBlink =
   ICaptureProgressWatcher 的 UI 面(Phase 6);TransformOnCapture 依赖
   Transform 活动系统(未移植)—— 两族随宿主批
   CaptureProgressBar/CapturableProgressBar/CapturableProgressBlink are
   ICaptureProgressWatcher's UI faces (Phase 6); TransformOnCapture
   depends on the unported Transform system — both families ride their
   host batches.
 - Captures 的 CaptureOrderTargeter/voice/cursor/TargetLines = 输入批
   面(Phase 6);ResolveOrder 的 CaptureActor 活动链全量
   Captures' CaptureOrderTargeter/voice/cursor/TargetLines are the
   input-batch faces (Phase 6); ResolveOrder's CaptureActor activity
   chain is kept in full.
 - GivesCashOnCapture 的 FloatingText 面省略(Phase 6);ChangeCash 记账
   全量
   GivesCashOnCapture's FloatingText face is omitted (Phase 6); the
   ChangeCash bookkeeping is kept in full.
 - Enter 的 TargetLineNodes = Phase 6;MoveCooldownHelper/让行/入内
   状态机全量
   Enter's TargetLineNodes ride Phase 6; the MoveCooldownHelper/yield/
   enter state machine is kept in full.

## src/mods/cargo.cpp

UPSTREAM: OpenRA.Mods.Common/Traits/Cargo.cs + ParaDrop.cs @1f594e320

## src/mods/cargo.hpp

UPSTREAM: OpenRA.Mods.Common/Traits/Cargo.cs + ParaDrop.cs @1f594e320

## src/mods/cargo_activities.cpp

UPSTREAM: OpenRA.Mods.Common/Activities/RideTransport.cs +
         UnloadCargo.cs + Parachute.cs @1f594e320

## src/mods/cargo_activities.hpp

UPSTREAM: OpenRA.Mods.Common/Activities/RideTransport.cs +
         UnloadCargo.cs + Parachute.cs @1f594e320

## src/mods/carryall.cpp

UPSTREAM: OpenRA.Mods.Common/Traits/Carryall.cs + Carryable.cs +
         AutoCarryall.cs + AutoCarryable.cs + CarryableHarvester.cs @1f594e320

## src/mods/carryall.hpp

UPSTREAM: OpenRA.Mods.Common/Traits/Carryall.cs + Carryable.cs +
         AutoCarryall.cs + AutoCarryable.cs + CarryableHarvester.cs @1f594e320

## src/mods/carryall_activities.cpp

UPSTREAM: OpenRA.Mods.Common/Activities/PickupUnit.cs + DeliverUnit.cs +
         AutoCarryall.cs 的 FerryUnit 嵌套活动 @1f594e320

## src/mods/carryall_activities.hpp

UPSTREAM: OpenRA.Mods.Common/Activities/PickupUnit.cs + DeliverUnit.cs @1f594e320

## src/mods/cash_trickler.cpp

UPSTREAM: OpenRA.Mods.Common/Traits/CashTrickler.cs 实现部分
         The implementation half.

## src/mods/cash_trickler.hpp

UPSTREAM: OpenRA.Mods.Common/Traits/CashTrickler.cs @1f594e320 L21-72(Info
         字段面 + 运行时类的 ISync 面;第十二批 G2 黄金对拍补面 —
         oilb 的 ISync 条目缺失平移 SyncHash 的 n 计数。Tick 计费与
         金币浮字面随经济 UI 批)
         The Info field face + the runtime class's ISync face (L21-72;
         the batch-12 G2 golden differential's missing face — an absent
         oilb ISync entry shifts the SyncHash n counter. The tick
         crediting and the floating-cash face ride the economy-UI
         batch).

## src/mods/changes_health.cpp

UPSTREAM: OpenRA.Mods.Common/Traits/ChangesHealth.cs 实现部分
         The implementation half.

## src/mods/changes_health.hpp

UPSTREAM: OpenRA.Mods.Common/Traits/ChangesHealth.cs @1f594e320 L21-64(Info
         字段面 + 运行时类的 ISync 面;第十二批 G2 黄金对拍补面 —
         ^Infantry 的两实例 ChangesHealth 条目缺失平移 SyncHash 的 n
         计数。血量增减的 Tick 面随医疗 UI 批)
         The Info field face + the runtime class's ISync face (L21-64;
         the batch-12 G2 golden differential's missing face — ^Infantry's
         two-instance ChangesHealth entries missing shift the SyncHash n
         counter. The HP-adjustment Tick face rides the medical-UI
         batch).

## src/mods/classic_sprite_sequence.cpp

UPSTREAM: OpenRA.Mods.Cnc/Graphics/ClassicSpriteSequence.cs @1f594e320 L18-48 +
          OpenRA.Mods.Cnc/Util.cs @1f594e320 L18-67
         (classic_sprite_sequence.hpp 的实现)
         Implementation of classic_sprite_sequence.hpp.

## src/mods/classic_sprite_sequence.hpp

UPSTREAM: OpenRA.Mods.Cnc/Graphics/ClassicSpriteSequence.cs @1f594e320 L18-48 +
          OpenRA.Mods.Cnc/Util.cs @1f594e320 L18-67(SpriteRanges/SpriteFacings 表
          与 ClassicIndexFacing/ClassicQuantizeFacing)
一代 Westwood 作品怪癖的序列:UseClassicFacings(仅 32 面向合法)启用
非线性面向映射 ClassicIndexFacing。
The sequence carrying first-generation Westwood oddities: UseClassicFacings
(valid only with 32 facings) enables the non-linear facing map of
ClassicIndexFacing.

## src/mods/classic_tileset_specific_sprite_sequence.cpp

UPSTREAM: OpenRA.Mods.Cnc/Graphics/ClassicTilesetSpecificSpriteSequence.cs @1f594e320 L21-95
         (classic_tileset_specific_sprite_sequence.hpp 的实现)
         Implementation of classic_tileset_specific_sprite_sequence.hpp.
上游在 Classic 变体里重复了 TilesetSpecific 的解析体(非继承);此处同形
重复,保持与上游文件一一对应。
Upstream duplicates TilesetSpecific's parsing body inside the Classic
variant (no inheritance); duplicated here in the same shape, keeping the
one-to-one file correspondence.

## src/mods/classic_tileset_specific_sprite_sequence.hpp

UPSTREAM: OpenRA.Mods.Cnc/Graphics/ClassicTilesetSpecificSpriteSequence.cs @1f594e320 L21-95
Classic + TilesetSpecific 的组合:Classic 的面向怪癖叠加地形集文件名变体。
The Classic + TilesetSpecific combination: Classic's facing quirks stacked
with the tileset filename variants.

## src/mods/cloak.cpp

UPSTREAM: OpenRA.Mods.Common/Traits/Cloak.cs @1f594e320 L23-377 +
         DetectCloaked.cs L11-54 + IgnoresCloak.cs L11-19(逐语句重写;
         机制对照见 cloak.hpp 头注)
         Statement-by-statement; the mechanism mapping lives in
         cloak.hpp's header note.

## src/mods/cloak.hpp

UPSTREAM: OpenRA.Mods.Common/Traits/Cloak.cs @1f594e320 L23-377 全文 +
         DetectCloaked.cs L11-54 全文 + IgnoresCloak.cs L11-19 全文
         The whole of Cloak.cs L23-377 + DetectCloaked.cs L11-54 +
         IgnoresCloak.cs L11-19.

机制对照 / Mechanism mapping:
 - IRenderModifier/IRadarColorModifier 的渲染物化面(WithAlpha/
   WithTint/WithPalette/RadarColorOverride)随渲染批 —— Cloak 的同步
   面(remainingTime/Cloaked/Uncloak 族/可见性/条件授予)全量保留;
   隐匿时 ModifyRender 返回 SpriteRenderable.None 的分支由渲染批承载
   The IRenderModifier/IRadarColorModifier render materialization
   (WithAlpha/WithTint/WithPalette/RadarColorOverride) rides the render
   batch — Cloak's synchronous faces (remainingTime/Cloaked/the
   Uncloak family/visibility/condition granting) are kept in full;
   the ModifyRender-returns-None branch lands with the render batch.
 - 声音/SpriteEffect 注入面不构造(声音注入惯例;RNG 无涉 —— 声音走
   Game.Sound 无随机)
   The sound/SpriteEffect injection faces are not constructed (the
   sound-injection convention; no RNG involvement — sounds go through
   Game.Sound without randomness).
 - 上游 otherCloaks = 同 CloakType 的其它 Cloak 实例(声音抑制判据);
   C++ 侧同构缓存
   Upstream's otherCloaks = the other Cloak instances sharing the
   CloakType (the sound-suppression predicate); cached isomorphically.

## src/mods/concrete_support_powers.cpp

UPSTREAM: OpenRA.Mods.Common/Traits/SupportPowers/NukePower.cs +
         SpawnActorPower.cs + ProduceActorPower.cs +
         GrantExternalConditionPower.cs 的实现面

## src/mods/concrete_support_powers.hpp

UPSTREAM: OpenRA.Mods.Common/Traits/SupportPowers/NukePower.cs @1f594e320 L23-243 +
         SpawnActorPower.cs L22-163 + ProduceActorPower.cs L19-113 +
         GrantExternalConditionPower.cs L23-178

## src/mods/conditions.cpp

UPSTREAM: OpenRA.Mods.Common/Traits/Conditions/GrantCondition.cs @1f594e320(逐语句
         重写;文件清单见 conditions.hpp 头注)
         Statement-by-statement rewrite; the file list lives in
         conditions.hpp's header note.

## src/mods/conditions.hpp

UPSTREAM: OpenRA.Mods.Common/Traits/Conditions/GrantCondition.cs @1f594e320
         (第七批 31 件:GrantCondition L17-51 / GrantRandomCondition
         L23-46 / GrantConditionOnTileSet L19-48 / GrantConditionWhileAiming
         L19-46 / GrantConditionOnDamageState L19-82 / GrantConditionOnHealth
         L22-100 / GrantConditionOnTerrain L18-72 / GrantConditionOnFaction
         L26-65 / GrantConditionOnBotOwner L19-55 / GrantConditionOnCombatantOwner
         L19-56 / GrantConditionOnPlayerResources L19-67 / GrantConditionOnPowerState
         L21-95 / GrantConditionOnMovement L18-58 / GrantConditionOnAttack
         L18-167 / GrantConditionOnProduction L19-86 / GrantConditionOnPrerequisite
         L22-90 + Player/GrantConditionOnPrerequisiteManager.cs L17-85 /
         SpreadsCondition L19-69 / GrantExternalConditionToProduced L19-48 /
         GrantExternalConditionToCrusher L20-58 / ToggleConditionOnOrder L28-117 /
         GrantConditionOnClientDock L19-88 / GrantConditionOnHostDock L19-88 /
         GrantConditionOnDeploy L38-352 / GrantChargedConditionOnToggle L51-253 +
         Activities/DeployForGrantedCondition.cs 全文 + GrantConditionOnLayer
         L18-63(Subterranean/Tunnel 两子类))
         The batch-7 set of 31: every file listed above, in full.

机制对照 / Mechanism mapping:
 - GrantConditionOnLineBuildDirection/LineBuildSegmentExternalCondition/
   GrantConditionOnMinelaying 三件依赖 LineBuild/Minelayer 系统(未移植)
   —— 延后至其宿主批(COVERAGE 登记)
   The three files GrantConditionOnLineBuildDirection/
   LineBuildSegmentExternalCondition/GrantConditionOnMinelaying depend on
   the unported LineBuild/Minelayer systems — deferred to their host
   batch (registered in COVERAGE).
 - 声音/Fluent 通知面:上游 Game.Sound.Play/PlayNotification +
   TextNotificationsManager —— 本批不构造(与 CreateEffect 批同);
   声音数组的 CosmeticRandom(LocalRandom 域)消耗保留
   The sound/Fluent notification faces: upstream's Game.Sound.Play/
   PlayNotification + TextNotificationsManager — not constructed in this
   batch (same as the CreateEffect batch); the sound arrays'
   CosmeticRandom (the LocalRandom domain) consumption is kept.
 - GrantConditionOnDeploy 的 notify(INotifyDeployTriggered)与
   GrantChargedConditionOnToggle 的 DeployOrderTargeter/cursor:实现者
   是渲染 trait/输入批 —— 空集承载;Deploy/Undeploy 的动画等待分支
   (notify.Length == 0)即真分支
   GrantConditionOnDeploy's notify (INotifyDeployTriggered) and
   GrantChargedConditionOnToggle's DeployOrderTargeter/cursor: the
   implementors are render traits/the input batch — carried as empty
   sets; the no-animation branch of Deploy/Undeploy (notify.Length == 0)
   is therefore the live branch.
 - GrantConditionOnLayer 抽象基类保留虚 UpdateConditions;两子类在
   C++ 侧仅 Info 差异(ValidLayerType 注入),类体直通基类
   The GrantConditionOnLayer abstract base keeps the virtual
   UpdateConditions; the two subclasses differ only in Info (the
   ValidLayerType injection) on the C++ side, their bodies pass through
   to the base.

## src/mods/confirmation_dialogs.cpp

UPSTREAM: OpenRA.Mods.Common/Widgets/ConfirmationDialogs.cs @1f594e320
         L17-193 全文(实现部分)。

## src/mods/confirmation_dialogs.hpp

UPSTREAM: OpenRA.Mods.Common/Widgets/ConfirmationDialogs.cs @1f594e320
         L17-193 全文。

## src/mods/create_map_players.hpp

UPSTREAM: OpenRA.Mods.Common/Traits/World/CreateMapPlayers.cs @1f594e320
         L22-181 + OpenRA.Game/Traits/World/Faction.cs(逐语义重写)
         Verbatim-semantics rewrites.

机制对照 / Mechanism mapping:
 - CreateMapPlayersInfo.CreateServerPlayers(L30-82):服务器 GameInformation
   面 —— Phase 7(与 ICreatePlayersInfo 接口面一并;COVERAGE 登记)
   CreateMapPlayersInfo.CreateServerPlayers (L30-82): the server
   GameInformation face — Phase 7 (together with the ICreatePlayersInfo
   interface face; in COVERAGE).
 - SetupPlayerMasks 的 "HACK: Map players share a ClientID"(L159-161)
   注释语义照抄 | the SetupPlayerMasks HACK-comment semantics kept.

## src/mods/d2k_sprite_sequence.cpp

UPSTREAM: OpenRA.Mods.D2k/Graphics/D2kSpriteSequence.cs @1f594e320 L19-116
         (d2k_sprite_sequence.hpp 的实现)
         Implementation of d2k_sprite_sequence.hpp.

## src/mods/d2k_sprite_sequence.hpp

UPSTREAM: OpenRA.Mods.D2k/Graphics/D2kSpriteSequence.cs @1f594e320 L19-116
D2k 色重映射序列:Remap/UseShadow/ConvertShroudToFog 经 AdjustFrame 把
R8RemappableFrame 按序列标志重建。
The D2k color-remap sequence: Remap/UseShadow/ConvertShroudToFog rebuild
each R8RemappableFrame with the sequence flags through AdjustFrame.

## src/mods/default_sprite_sequence.cpp

UPSTREAM: OpenRA.Mods.Common/Graphics/DefaultSpriteSequence.cs @1f594e320 L24-605
         (default_sprite_sequence.hpp 的实现)
         Implementation of default_sprite_sequence.hpp.
形态偏离(COVERAGE 登记):resolved[f] 的越界 = 上游数组
IndexOutOfRangeException,C++ 以同文本 runtime_error 等价抛;ParseSequences
包装异常的内层 {e} 为 C# ToString(),此处取 e.what()(信息等价,少类型前
缀);PerfTimer 计时面不落地(名字保留)。
Registered shape deviations: resolved[f] out of range = upstream's array
IndexOutOfRangeException, thrown here as a runtime_error with the same
text; the inner {e} of ParseSequences' wrapper is C# ToString(), here
e.what() (information-equal, minus the type prefix); the PerfTimer timing
face is not ported (the name stays).

## src/mods/default_sprite_sequence.hpp

UPSTREAM: OpenRA.Mods.Common/Graphics/DefaultSpriteSequence.cs @1f594e320 L24-605
通用精灵序列:两段流水(ReserveSprites 预留记账 → ResolveSprites 物化重
索引),字段全走 FieldLoader.GetValue&lt;T&gt; 的 LoadField 查找(data 缺键
回退 defaults)。mods 侧 Classic/Tileset/D2k 变体继承此类。
The generic sprite sequence: the two-stage pipeline (ReserveSprites
booking → ResolveSprites materialization + reindexing), every field read
through the LoadField lookup of FieldLoader.GetValue<T> (a data miss falls
back to defaults). The mods-side Classic/Tileset/D2k variants derive from
this class.

## src/mods/deploy_order_targeter.hpp

UPSTREAM: OpenRA.Mods.Common/Orders/DeployOrderTargeter.cs @1f594e320
         L18-46 全文
         The whole of DeployOrderTargeter.cs L18-46.

## src/mods/dock_activities.cpp

UPSTREAM: OpenRA.Mods.Common/Activities/MoveToDock.cs 实现部分
         + GenericDockSequence.cs | The implementation half.

## src/mods/dock_activities.hpp

UPSTREAM: OpenRA.Mods.Common/Activities/MoveToDock.cs @1f594e320 L19-150
         全文 + GenericDockSequence.cs L22-216 全文(逐语义重写;
         WithDockingOverlay/IDockClientBody 的动画面 = 未移植空集:
         PlayThen 回调即置 Loop 态 —— 上游无该 trait 时的等价路径)
         The whole of MoveToDock.cs L19-150 + the whole of
         GenericDockSequence.cs L22-216 (verbatim-semantics rewrites;
         the WithDockingOverlay/IDockClientBody animation faces are the
         unported empty set: the PlayThen callback sets the Loop state
         at once — upstream's equivalent path without those traits).

机制对照 / Mechanism mapping:
 - 嵌套 DockingState 枚举照抄;PlayDockAnimations 的 Action after 闭包
   链 → std::function(无动画 trait 时直接调用 —— 上游 else 分支)
   The nested DockingState enum copied; PlayDockAnimations' Action-after
   closure chain → std::function (invoked directly without the
   animation traits — upstream's else branches).
 - INotifyDockClientMoving/INotifyDockClient/INotifyDockHost 的通知面
   = TraitsImplementing 直查(空集 = 上游无实现者时的等价域)
   The INotifyDockClientMoving/INotifyDockClient/INotifyDockHost
   notification faces query TraitsImplementing directly (the empty set
   = upstream's equivalent domain without implementors).

## src/mods/dock_client.cpp

UPSTREAM: OpenRA.Mods.Common/Traits/DockClientManager.cs 实现部分
         The implementation half.

## src/mods/dock_client.hpp

UPSTREAM: OpenRA.Mods.Common/Traits/DockClientBase.cs @1f594e320 L15-56
         全文 + DockClientManager.cs L22-438 全文(逐语义重写;游标/声音
         的 UI 参照面与 BooleanExpression 观察者链保留)
         The whole of DockClientBase.cs L15-56 + the whole of
         DockClientManager.cs L22-438 (verbatim-semantics rewrites; the
         cursor/sound UI references and the BooleanExpression observer
         chain kept).

机制对照 / Mechanism mapping:
 - abstract DockClientBase<InfoType> → DockClientBaseCore<ConcreteT>
   CRTP 组合核(条件 trait 四钩子;ConcreteT 承载 ORA_TRAIT_INTERFACES)
   The abstract DockClientBase<InfoType> → the DockClientBaseCore<
   ConcreteT> CRTP composition core (the condition-trait four hooks;
   ConcreteT carries ORA_TRAIT_INTERFACES).
 - DockActorTargeter 的 Func<CanTargetContext, CanTargetResult> 闭包 →
   std::function(求值体;Blocked/Allowed 记录结构体承载)
   DockActorTargeter's Func<CanTargetContext, CanTargetResult> closure →
   std::function (the evaluation body; the Blocked/Allowed records carry
   the payloads).
 - EnterCursorOverrides 的 FrozenDictionary<string, string> → 插入序
   vector<pair>(TryGetValue = 首见键)
   EnterCursorOverrides' FrozenDictionary<string, string> → the
   insertion-ordered vector<pair> (TryGetValue = the first-seen key).
 - DockExts.ClosestDock 的 AggregateBy(格子去重)→ map<CPos, TraitPair>
   (首值保留),路径搜索照抄(Mobile 无 → 距离+占用排序)
   DockExts.ClosestDock's AggregateBy (the per-cell dedup) → map<CPos,
   TraitPair> (the first value kept); the path search copied verbatim
   (no Mobile → the distance+occupancy ordering).

## src/mods/dock_host.cpp

UPSTREAM: OpenRA.Mods.Common/Traits/DockHost.cs 实现部分
         The implementation half.

## src/mods/dock_host.hpp

UPSTREAM: OpenRA.Mods.Common/Traits/DockHost.cs @1f594e320 L16-185 全文
         (逐语义重写;DockType 位标签在 sim/trait_interfaces.hpp)
         The whole of DockHost.cs L16-185 (a verbatim-semantics rewrite;
         the DockType bit tag lives in sim/trait_interfaces.hpp).

机制对照 / Mechanism mapping:
 - ReservedDockClients 的 List → vector;UnreserveAll = 逐个 Unreserve
   的上游循环
   ReservedDockClients' List → a vector; UnreserveAll keeps upstream's
   per-item Unreserve loop.
 - [VerifySync] {preventDock, dockedClientActor}:Actor 键 → HashActor
   (ActorID);哈希注册于 dock_host.cpp
   The [VerifySync] {preventDock, dockedClientActor}: the Actor key →
   HashActor (ActorID); the hash registers in dock_host.cpp.

## src/mods/experience.cpp

UPSTREAM: OpenRA.Mods.Common/Traits/GainsExperience.cs @1f594e320
         L21-172 + GivesExperience.cs L15-79 +
         Multipliers/GainsExperienceMultiplier.cs L11-29 +
         Player/PlayerExperience.cs L14-35(逐语句重写;机制对照见
         experience.hpp 头注)
         Statement-by-statement; the mechanism mapping lives in
         experience.hpp's header note.

## src/mods/experience.hpp

UPSTREAM: OpenRA.Mods.Common/Traits/GainsExperience.cs @1f594e320 L21-172
         全文 + GivesExperience.cs L15-79 全文 +
         Multipliers/GainsExperienceMultiplier.cs L11-29 全文 +
         Player/PlayerExperience.cs L14-35 全文
         The whole of GainsExperience.cs L21-172 + GivesExperience.cs
         L15-79 + Multipliers/GainsExperienceMultiplier.cs L11-29 +
         Player/PlayerExperience.cs L14-35.

机制对照 / Mechanism mapping:
 - Conditions 的 FrozenDictionary<int,string> → 插入序 pair vector
   (yaml 节点序 = 上游 FieldLoader 插入序;nextLevel 的等级序保真)
   Conditions' FrozenDictionary<int,string> → an insertion-ordered
   pair vector (the yaml node order = upstream's FieldLoader insert
   order; nextLevel's level order preserved).
 - 击杀方的 PlayerExperience 授予 / 升级声音+SpriteEffect / Fluent
   通知:PlayerExperience 全量;声音/特效/通知面省略(声音注入惯例,
   无 RNG)
   The killer's PlayerExperience grant / the level-up sound+SpriteEffect
   / Fluent notifications: PlayerExperience in full; the sound/effect/
   notification faces omitted (the sound-injection convention, no RNG).
 - DevLevelUp order 的 DeveloperMode 检查全量保留
   The DevLevelUp order's DeveloperMode check is kept in full.

## src/mods/external_condition.cpp

UPSTREAM: OpenRA.Mods.Common/Traits/Conditions/ExternalCondition.cs @1f594e320
         L14-190 + Traits/Conditions/ProximityExternalCondition.cs
         L18-194(逐语句重写;机制对照见 external_condition.hpp 头注)
         Statement-by-statement; the mechanism mapping lives in
         external_condition.hpp's header note.

## src/mods/external_condition.hpp

UPSTREAM: OpenRA.Mods.Common/Traits/Conditions/ExternalCondition.cs @1f594e320
         L14-190 全文 + Traits/Conditions/ProximityExternalCondition.cs
         L18-194 全文
         The whole of ExternalCondition.cs L14-190 + the whole of
         Conditions/ProximityExternalCondition.cs L18-194.

机制对照 / Mechanism mapping:
 - 上游 source(授予方标识)是 object(Actor/战头/支持力等);C++ 侧以
   const void* 源键承载(地址等价;上游按引用相等判同 —— Dictionary
   键语义一致)
   Upstream's source (the granting-side identity) is an object (an
   Actor/warhead/support power); the C++ side carries it as a const
   void* source key (address equality; identical to upstream's
   reference-equality Dictionary keys).
 - GrantExternalConditionByName:GrantExternalConditionToProduced/
   ToCrusher 经按名转发的注册表面(避免 conditions.cpp ↔ 本文件的头
   循环;上游是直接 Trait<ExternalCondition> 查询 + Info.Condition 匹配)
   GrantExternalConditionByName: the registry face the
   GrantExternalConditionToProduced/ToCrusher pair routes through
   (breaking the conditions.cpp ↔ this-file header cycle; upstream is
   the direct Trait<ExternalCondition> query + Info.Condition match).
 - ProximityExternalCondition 的 Enable/DisableSound 面省略(声音注入
   惯例);tokens 的 Actor 键 = WorldArena 生命周期内稳定指针
   ProximityExternalCondition's Enable/DisableSound faces are omitted
   (the sound-injection convention); the tokens' Actor keys are
   pointers stable over the WorldArena lifetime.

## src/mods/footprint_place_building_preview.cpp

UPSTREAM: OpenRA.Mods.Common/Traits/Buildings/FootprintPlaceBuildingPreview.cs
         @1f594e320(实现部分)
         The implementation half.

## src/mods/footprint_place_building_preview.hpp

UPSTREAM: OpenRA.Mods.Common/Traits/Buildings/FootprintPlaceBuildingPreview.cs
         @1f594e320 L21-135 全文(引擎 SpriteRenderable → 自有 custom
         renderable 承载:IOrderGenerator 的 IRenderable* 出参域)
         The whole of FootprintPlaceBuildingPreview.cs L21-135 (the
         engine SpriteRenderable → an own custom renderable: the
         IRenderable* out domain of IOrderGenerator).

## src/mods/frozen_under_fog.cpp

UPSTREAM: OpenRA.Mods.Common/Traits/Modifiers/FrozenUnderFog.cs +
         HiddenUnderShroud.cs + ShroudExts.cs L19-32(实现部分)
         The implementation half.

## src/mods/frozen_under_fog.hpp

UPSTREAM: OpenRA.Mods.Common/Traits/Modifiers/FrozenUnderFog.cs @1f594e320
         L17-190 全文 + HiddenUnderShroud.cs L15-72 全文 + FrozenUnderFog.cs
         L189-190 的 HiddenUnderFogInit(SpawnedByMapInit 随标记面入
         actor_init.hpp)
         The whole of FrozenUnderFog.cs L17-190 + the whole of
         HiddenUnderShroud.cs L15-72 + HiddenUnderFogInit of L189-190
         (SpawnedByMapInit's marker face lives in actor_init.hpp).

机制对照 / Mechanism mapping:
 - 上游 FrozenUnderFog = ICreatesFrozenActors + IRenderModifier +
   IDefaultVisibility + ITickRender + ISync + INotify{Created,
   OwnerChanged,ActorDisposing}:C++ 挂同步面四接口;IRenderModifier/
   ITickRender 的渲染物化(renderables/bounds/mouseBounds)随渲染批 ——
   TickRender 保留 NeedRenderables/ScreenMap.AddOrUpdateFrozen 的结构
   面,渲染物化空(登记 COVERAGE)
   Upstream's FrozenUnderFog = ICreatesFrozenActors + IRenderModifier +
   IDefaultVisibility + ITickRender + ISync + INotify{Created,
   OwnerChanged,ActorDisposing}: the C++ side mounts the four
   synchronous interfaces; the IRenderModifier/ITickRender render
   materialization (renderables/bounds/mouseBounds) rides the render
   batch — TickRender keeps the NeedRenderables/ScreenMap.
   AddOrUpdateFrozen structural face with an empty render materialization
   (registered in COVERAGE).
 - PlayerDictionary<FrozenState> → 玩家序 vector(World.Players() 的
   IndexOf;上游按 playerIndex 索引同构)
   PlayerDictionary<FrozenState> → the player-indexed vector
   (World.Players()' IndexOf; isomorphic to upstream's playerIndex
   indexing).
 - startsRevealed 的 ShroudInfo 读:上游 Map.Rules.Actors[Player].
   TraitInfo<ShroudInfo>() + LobbyInfo OptionOrDefault("explored",…)
   —— C++ 沿 Shroud::Created 的同形读(玩家 Shroud 的 checkbox 面)
   The startsRevealed ShroudInfo read: upstream's Map.Rules.Actors[
   Player].TraitInfo<ShroudInfo>() + LobbyInfo OptionOrDefault(
   "explored",…) — C++ keeps Shroud::Created's same-shaped read (the
   player Shroud's checkbox face).

## src/mods/game_info_logics.cpp

UPSTREAM: OpenRA.Mods.Common/Widgets/Logic/Ingame/GameInfoLogic.cs
         @1f594e320(实现;头注见 game_info_logics.hpp)
         Implementation; the header notes live in game_info_logics.hpp.

## src/mods/game_info_logics.hpp

UPSTREAM: OpenRA.Mods.Common/Widgets/Logic/Ingame/GameInfoLogic.cs
         @1f594e320 L23-195 全文(五面板字典 + AutoSelect 流 + Chat 焦点
         转发)+ GameInfoBriefingLogic.cs L17-40 全文 + GameInfo
         ObjectivesLogic.cs L19-92 全文 + GameInfoStatsLogic.cs L24-335
         全文(队伍分组的计分板 + 踢人/投票踢对话 + 观战者行)。
         The whole of GameInfoLogic.cs L23-195 (the five-panel dictionary
         + the AutoSelect flow + the chat focus forwarding) +
         GameInfoBriefingLogic.cs L17-40 + GameInfoObjectivesLogic.cs
         L19-92 + GameInfoStatsLogic.cs L24-335 (the team-grouped score
         board + the kick/vote-kick dialogs + the spectator rows).

## src/mods/gps_watcher.cpp

UPSTREAM: OpenRA.Mods.Cnc/Traits/GpsWatcher.cs 实现部分
         The implementation half.

## src/mods/gps_watcher.hpp

UPSTREAM: OpenRA.Mods.Cnc/Traits/GpsWatcher.cs @1f594e320 L21-30(Info 面)
         + L41-70(运行时类的 ISync 字段面;第十二批 G2 黄金对拍补面 —
         player actor 的 ISync 条目缺失平移 SyncHash 的 n 计数;
         GpsAdd/GpsRemove/ReachedOrbit 的刷新分发面随 support-power 批)
         The Info face of GpsWatcher.cs L21-30 + the runtime class's
         ISync field face (L41-70; the batch-12 G2 golden differential's
         missing face — an absent player-actor ISync entry shifts the
         SyncHash n counter; the GpsAdd/GpsRemove/ReachedOrbit refresh
         dispatch rides the support-power batch).

## src/mods/harvest_activities.cpp

UPSTREAM: OpenRA.Mods.Common/Activities/HarvestResource.cs 实现部分
         + FindAndDeliverResources.cs | The implementation half.

## src/mods/harvest_activities.hpp

UPSTREAM: OpenRA.Mods.Common/Activities/HarvestResource.cs @1f594e320
         L19-124 全文 + FindAndDeliverResources.cs L19-263 全文(逐语义
         重写)
         The whole of HarvestResource.cs L19-124 + the whole of
         FindAndDeliverResources.cs L19-263 (verbatim-semantics
         rewrites).

## src/mods/harvester.cpp

UPSTREAM: OpenRA.Mods.Common/Traits/Harvester.cs 实现部分
         The implementation half.

## src/mods/harvester.hpp

UPSTREAM: OpenRA.Mods.Common/Traits/Harvester.cs @1f594e320 L20-330 全文
         (逐语义重写;IResourceRenderer 的游标面 = 未移植空集:CanTarget
         恒 false —— 上游无渲染器时同域;COVERAGE 登记)
         The whole of Harvester.cs L20-330 (a verbatim-semantics rewrite;
         the IResourceRenderer cursor face is the unported empty set:
         CanTarget stays false — upstream's same domain without a
         renderer; registered in COVERAGE).

机制对照 / Mechanism mapping:
 - DockClientBase<HarvesterInfo> → DockClientBaseCore<Harvester>(CRTP)
   DockClientBase<HarvesterInfo> → DockClientBaseCore<Harvester> (CRTP).
 - IRulesetLoaded 的 Resources 校验(L80-88)→ 工厂解析时点(异常文本
   逐字;时点差异同 D 系登记)
   The IRulesetLoaded Resources validation (L80-88) → the factory-parse
   moment (the exception texts verbatim; the timing difference is
   registered like the D series).

## src/mods/health.cpp

UPSTREAM: OpenRA.Mods.Common/Traits/Health.cs @1f594e320(实现部分)
         The implementation half of Health.cs.

## src/mods/health.hpp

UPSTREAM: OpenRA.Mods.Common/Traits/Health.cs @1f594e320 L19-261(逐语义重写)
         Verbatim-semantics rewrite.

机制对照 / Mechanism mapping:
 - InflictDamage 的手内联 decimal 修正链(L167-187)→
   core/percent_modifiers.hpp 的 __int128 精确复刻(OPT-A1 已落地面;
   modifier ≠ 100 收集 + span 直传,零 LINQ)
   InflictDamage's hand-inlined decimal chain (L167-187) → the exact
   __int128 replica in core/percent_modifiers.hpp (the OPT-A1 landed
   face; non-100 modifiers collected + span-passed, zero LINQ).
 - DamageState 阶梯的 `HP * 100L < MaxHP * 25L` 长整升位照抄
   The DamageState ladder's `HP * 100L < MaxHP * 25L` long promotions
   kept.
 - RulesetLoaded(L33-36:无 HitShape 抛)→ 工厂面执行(值袋无
   IRulesetLoaded 生命周期;时点差异 = COVERAGE 登记偏离)
   RulesetLoaded (L33-36: the no-HitShape throw) → executed by the
   factory (the value bag has no IRulesetLoaded lifecycle; the timing
   difference is the registered COVERAGE deviation).

## src/mods/hit_shape.cpp

UPSTREAM: OpenRA.Mods.Common/Traits/HitShape.cs 实现部分
         The implementation half.

## src/mods/hit_shape.hpp

UPSTREAM: OpenRA.Mods.Common/Traits/HitShape.cs @1f594e320 L21-177 全文
         (逐语义重写;RenderDebug* 渲染面随 Phase 6)
         The whole of HitShape.cs L21-177 (verbatim-semantics rewrite;
         the RenderDebug* faces land with Phase 6).

机制对照 / Mechanism mapping:
 - ArmorType 类型标签(Armor.cs L18)权威注册在 gen/bitsets_gen.cpp
   (OpenRA.Mods.Common.Traits.ArmorType);标签类定义于此
   The ArmorType type tag (Armor.cs L18) is authoritatively registered
   in gen/bitsets_gen.cpp; the tag class is defined here.
 - Info.Type(IHitShape)的 LoadShape 产物 = 值袋嵌套记录 → 本批的
   mods::ParseHitShape(Initialize() 在解析尾 —— 上游 LoadShape L157
   同点;loaders.cpp 注记的"Phase 5 行为面"即此)
   Info.Type's (IHitShape) LoadShape product = the value-bag nested
   record → this batch's mods::ParseHitShape (Initialize() at the parse
   tail — the same point as upstream's LoadShape L157; loaders.cpp's
   "Phase 5 behaviour surface" note lands here).
 - Turreted trait 面未移植:turret == null 的上游分支(体朝向/体原点)
   (COVERAGE 登记,同 Armament 的 Turreted 空集面)
   The Turreted trait face is unported: upstream's turret == null
   branches (the body orientation/origin) (registered in COVERAGE, the
   same empty-set face as Armament's Turreted).
 - TargetablePositions 的缓存键(上游 (TargetableCells, CenterPosition,
   Orientation, TurretLocalOrientation, TurretOffset) 元组)→ 无 turret
   面下的三字段简化键(TargetableCells/CenterPosition/Orientation;
   turret 两元恒 null)
   TargetablePositions's cache key (upstream's (TargetableCells,
   CenterPosition, Orientation, TurretLocalOrientation, TurretOffset)
   tuple) → the three-field simplified key under the turret-less face
   (TargetableCells/CenterPosition/Orientation; the two turret members
   stay null).

## src/mods/hit_shapes.cpp

UPSTREAM: OpenRA.Mods.Common/HitShapes/IHitShape.cs + Circle.cs +
         Capsule.cs + Polygon.cs + Rectangle.cs 实现部分
         The implementation half.

## src/mods/hit_shapes.hpp

UPSTREAM: OpenRA.Mods.Common/HitShapes/IHitShape.cs @1f594e320 L17-28 +
         Circle.cs L17-67 + Capsule.cs L17-121 + Polygon.cs L21-130 +
         Rectangle.cs L20-146(逐语义重写;RenderDebugOverlay 渲染面随
         Phase 6)
         IHitShape.cs L17-28 + Circle.cs L17-67 + Capsule.cs L17-121 +
         Polygon.cs L21-130 + Rectangle.cs L20-146 (verbatim-semantics
         rewrites; the RenderDebugOverlay render face lands with Phase
         6).

机制对照 / Mechanism mapping:
 - 上游 Game.CreateObject<IHitShape>(shape + "Shape") + FieldLoader.Load
   → 值袋嵌套记录(loaders.cpp 的 LoadShape 已物化)+ 本文件的按名解析
   (Initialize() 在解析尾调用 —— 上游 LoadShape L157 同点)
   Upstream's Game.CreateObject<IHitShape>(shape + "Shape") +
   FieldLoader.Load → the value-bag nested record (materialized by
   loaders.cpp's LoadShape) + the by-name parse here (Initialize() runs
   at the parse tail — the same point as upstream's LoadShape L157).
 - Polygon 的 Points.PolygonContains(Exts)→ core/polygon.hpp 的同面
   Polygon's Points.PolygonContains (Exts) → the same face in
   core/polygon.hpp.

## src/mods/immobile.cpp

UPSTREAM: OpenRA.Mods.Common/Traits/Immobile.cs 实现部分
         The implementation half.

## src/mods/immobile.hpp

UPSTREAM: OpenRA.Mods.Common/Traits/Immobile.cs @1f594e320 L21-76 全文
         (第十二批 G2 黄金对拍补面:mpspawn 等编辑器摆位 actor 的
         ISync 条目 {TopLeft, CenterPosition} 缺失平移 SyncHash 的 n
         计数)
         The whole of Immobile.cs L21-76 (the batch-12 G2 golden
         differential's missing face: editor-placement actors like
         mpspawn lacked their ISync entries {TopLeft, CenterPosition},
         shifting the SyncHash n counter).

## src/mods/ingame_hud_logics.cpp

UPSTREAM: OpenRA.Mods.Common/Widgets/Logic/Ingame/ 的 HUD 第一组
         (实现部分;头注见 ingame_hud_logics.hpp)
         The HUD group one under Ingame/ (the implementation half; the
         header note lives in ingame_hud_logics.hpp).

## src/mods/ingame_hud_logics.hpp

UPSTREAM: OpenRA.Mods.Common/Widgets/Logic/Ingame/ 的 HUD 第一组
         @1f594e320:
         IngameRadarDisplayLogic.cs L18-52 全文 /
         ClassicProductionLogic.cs L13-186 全文 /
         ProductionTabsLogic.cs L13-97 全文 /
         IngameCashCounterLogic.cs L14-86 全文 /
         IngamePowerCounterLogic.cs L14-55 全文 /
         IngameSiloBarLogic.cs L14-49 全文 /
         GameTimerLogic.cs L14-95 全文。
         The HUD group one under Ingame/ @1f594e320: the whole of
         IngameRadarDisplayLogic.cs L18-52 / ClassicProductionLogic.cs
         L13-186 / ProductionTabsLogic.cs L13-97 / IngameCashCounterLogic.cs
         L14-86 / IngamePowerCounterLogic.cs L14-55 / IngameSiloBarLogic.cs
         L14-49 / GameTimerLogic.cs L14-95.

## src/mods/ingame_menu_logic.cpp

UPSTREAM: OpenRA.Mods.Common/Widgets/Logic/Ingame/IngameMenuLogic.cs
         @1f594e320 L23-647 全文(实现部分)。

## src/mods/ingame_menu_logic.hpp

UPSTREAM: OpenRA.Mods.Common/Widgets/Logic/Ingame/IngameMenuLogic.cs
         @1f594e320 L23-647 全文。

## src/mods/line_build.cpp

UPSTREAM: OpenRA.Mods.Common/Traits/Buildings/LineBuild.cs @1f594e320
         (实现部分)+ Traits/Buildings/LineBuildNode.cs
         The implementation half + LineBuildNode.cs.

## src/mods/line_build.hpp

UPSTREAM: OpenRA.Mods.Common/Traits/Buildings/LineBuild.cs @1f594e320
         L19-124 全文 + Traits/Buildings/LineBuildNode.cs L18-34
         The whole of LineBuild.cs L19-124 + LineBuildNode.cs L18-34.

## src/mods/load_screens.cpp

UPSTREAM: OpenRA.Mods.Common/LoadScreens/LogoStripeLoadScreen.cs @1f594e320

## src/mods/load_screens.hpp

UPSTREAM: OpenRA.Mods.Common/LoadScreens/LogoStripeLoadScreen.cs @1f594e320

## src/mods/lobby_logics.hpp

UPSTREAM: OpenRA.Mods.Common/Widgets/Logic/TabCompletionLogic.cs
         @1f594e320 全文 + OpenRA.Mods.Common/Widgets/Logic/Lobby/
         {LatencyTooltipLogic,SpawnSelectorTooltipLogic,KickClientLogic,
         KickSpectatorsLogic,MapPreviewLogic,LobbyOptionsLogic,LobbyLogic}
         .cs 全文(大厅面板族九件;ChromeLogic 注册锚之外全部以
         自由函数 + 状态结构承载)。
         The whole of TabCompletionLogic.cs + the Lobby/{LatencyTooltip
         Logic,SpawnSelectorTooltipLogic,KickClientLogic,KickSpectators
         Logic,MapPreviewLogic,LobbyOptionsLogic,LobbyLogic}.cs family
         (nine lobby-panel pieces; everything outside the ChromeLogic
         registration anchors rides free functions + state structs).

## src/mods/lobby_utils.cpp

UPSTREAM: OpenRA.Mods.Common/Widgets/Logic/Lobby/LobbyUtils.cs
         @1f594e320(实现;头注见 lobby_utils.hpp)
         Implementation; the header notes live in lobby_utils.hpp.

## src/mods/lobby_utils.hpp

UPSTREAM: OpenRA.Mods.Common/Widgets/Logic/Lobby/LobbyUtils.cs
         @1f594e320 全文(SetupProfileWidget 先于本批承载;本批补齐
         其余:下拉族/出生点选择/Setup* widget 族/延迟色面)
         The whole file (SetupProfileWidget landed before this batch;
         this batch completes the rest: the dropdown family / spawn
         selection / the Setup* widget family / the latency colors).

## src/mods/main_menu_logic.cpp

UPSTREAM: OpenRA.Mods.Common/Widgets/Logic/MainMenuLogic.cs @1f594e320
         L25-566 全文(实现部分)。

## src/mods/main_menu_logic.hpp

UPSTREAM: OpenRA.Mods.Common/Widgets/Logic/MainMenuLogic.cs @1f594e320
         L25-566 全文。

## src/mods/menu_logics.cpp

UPSTREAM: OpenRA.Mods.Common/Widgets/Logic/Ingame/AddFactionSuffixLogic.cs
         @1f594e320 L18-74 全文 + SupportPowerBinLogic.cs L16-79 全文 +
         OrderButtonsChromeLogic.cs L18-55 全文 +
         MenuButtonsChromeLogic.cs L18-124 全文(实现部分;头注见
         menu_logics.hpp)。

## src/mods/menu_logics.hpp

UPSTREAM: OpenRA.Mods.Common/Widgets/Logic/Ingame/AddFactionSuffixLogic.cs
         @1f594e320 L18-74 全文 +
         OpenRA.Mods.Common/Widgets/Logic/Ingame/SupportPowerBinLogic.cs
         @1f594e320 L16-79 全文 +
         OpenRA.Mods.Common/Widgets/Logic/Ingame/OrderButtonsChromeLogic.cs
         @1f594e320 L18-55 全文 +
         OpenRA.Mods.Common/Widgets/Logic/Ingame/MenuButtonsChromeLogic.cs
         @1f594e320 L18-124 全文 +
         GameInfoLogic.cs L21(IngameInfoPanel 枚举)。

## src/mods/missile_projectiles.cpp

UPSTREAM: OpenRA.Mods.Common/Projectiles/Missile.cs + GravityBomb.cs +
         OpenRA.Mods.Cnc/Projectiles/TeslaZap.cs(实现部分)
         The implementation half.

## src/mods/missile_projectiles.hpp

UPSTREAM: OpenRA.Mods.Common/Projectiles/Missile.cs @1f594e320 L26-982 +
         GravityBomb.cs L20-147 + OpenRA.Mods.Cnc/Projectiles/TeslaZap.cs
         L20-99(全文逐语义重写;Render/TeslaZapRenderable/Contrail 的
         视觉面随渲染批 —— RNG 消耗序保持)
         The whole of Missile.cs L26-982 + GravityBomb.cs L20-147 +
         OpenRA.Mods.Cnc/Projectiles/TeslaZap.cs L20-99
         (verbatim-semantics rewrites; the Render/TeslaZapRenderable/
         Contrail visual faces land with the render batch — the RNG
         consumption order is preserved).

机制对照 / Mechanism mapping:
 - 值袋 MissileInfo/GravityBombInfo/TeslaZapInfo 记录 → 本文件 Info 解析
   面;Create 经 ProjectileRegistry(记录名即键;同 Bullet 形)
   The value-bag MissileInfo/GravityBombInfo/TeslaZapInfo records →
   this file's Info parse faces; Create registers through the
   ProjectileRegistry (the record name is the key; Bullet's shape).
 - Animation 注入面:复用 projectiles.hpp 的 SetBulletAnimationFactory
   (缺省 = 不构造;Sequences.Random 的 SharedRandom 消耗保持 —— 上游
   Missile/GravityBomb 的 anim 构造序 = ctor 尾部,RNG 序不依赖视觉装配;
   GravityBomb 的 OpenSequence→PlayThen 链 = 注入面之上的渲染批语义,
   RNG 只消耗一次 Random,登记 COVERAGE)
   The Animation injection face: reuses projectiles.hpp's
   SetBulletAnimationFactory (the default constructs nothing; the
   Sequences.Random SharedRandom consumption is preserved — upstream's
   Missile/GravityBomb anim construction sits at the ctor tail, so the
   RNG order never depends on the visual assembly; GravityBomb's
   OpenSequence→PlayThen chain is a render-batch semantics atop the
   injection face, with exactly one Random consumed, registered in
   COVERAGE).
 - Contrail/ContrailFader/TeslaZapRenderable/SpriteEffect(trail):视觉
   效果不构造(渲染批;Missile 的 trail RNG 消耗保持;COVERAGE 登记)
   The Contrail/ContrailFader/TeslaZapRenderable/SpriteEffect (trail):
   the visual effects are not constructed (the render batch; Missile's
   trail RNG consumption is preserved; registered in COVERAGE).
 - JamsMissiles trait 未移植:world.ActorsWithTrait<JamsMissiles>() 的
   空集 = Any() 恒假(jammed 恒 false;上游无该 trait 世界同值;
   COVERAGE 登记)
   The JamsMissiles trait is unported: world.ActorsWithTrait<
   JamsMissiles>()'s empty set = Any() constantly false (jammed stays
   false; the same value upstream gives in a world without the trait;
   registered in COVERAGE).

## src/mods/mission_objectives.cpp

UPSTREAM: OpenRA.Mods.Common/Traits/Player/MissionObjectives.cs 实现部分
         The implementation half.

## src/mods/mission_objectives.hpp

UPSTREAM: OpenRA.Mods.Common/Traits/Player/MissionObjectives.cs @1f594e320
         L21-70(Info 字段面 + 运行时类的 ISync 面;第十二批 G2 黄金
         对拍补面 — player actor 的 ISync 条目缺失平移 SyncHash 的 n
         计数)+ L21-25(MissionObjective)+ L115-125(Add 与
         ObjectiveAdded 事件面;第十七批 MenuButtonsChrome 补面)。
         目标列表的胜负分发面随战役批。
         The Info field face + the runtime class's ISync face (L21-70;
         the batch-12 G2 golden differential's missing face) +
         L21-25 (MissionObjective) + L115-125 (Add and the
         ObjectiveAdded event; the batch-17 MenuButtonsChrome face).
         The win-state dispatch rides the campaign batch.

## src/mods/mobile.cpp

UPSTREAM: OpenRA.Mods.Common/Traits/Mobile.cs @1f594e320 L25-1091 实现部分
         (the implementation half)。

## src/mods/mobile.hpp

UPSTREAM: OpenRA.Mods.Common/Traits/Mobile.cs @1f594e320 L25-1091 全文
         (逐语义重写;MoveOrderTargeter/ReturnToCellActivity/
         LeaveProductionActivity 内嵌同上游)
         The whole of Mobile.cs L25-1091 (verbatim-semantics rewrite;
         MoveOrderTargeter/ReturnToCellActivity/LeaveProductionActivity
         nested as upstream).

机制对照 / Mechanism mapping:
 - PausableConditionalTrait<MobileInfo> → sim::ConditionalTraitCore<Mobile>
   组合(四钩子静态派发;见 sim/conditional_trait.hpp 头注)
   PausableConditionalTrait<MobileInfo> → the
   sim::ConditionalTraitCore<Mobile> composition (the four hooks
   statically dispatched; see sim/conditional_trait.hpp's header).
 - MobileInfo.RulesetLoaded 的 LocomotorInfo 解析(世界 actor 的 Info 表
   扫描)→ 工厂解析面(时点差异同 Health 的 D 系;异常文本逐字);
   Info.locomotor 运行时缓存随每实例解析物化(上游跨 world 重置的
   "reset between worlds" 语义由此免掉 —— 每次工厂解析即新实例)
   MobileInfo.RulesetLoaded's LocomotorInfo resolution (the world
   actor's Info-table scan) → the factory-parse face (the timing
   difference like Health's D series; exception texts verbatim); the
   Info.locomotor runtime cache materializes with each instance's parse
   (upstream's "reset between worlds" reset becomes unnecessary — every
   factory parse is a fresh instance).
 - Shroud.IsExplored(L948/L1077/L1081):Shroud 未移植 —— MoveIntoShroud
   默认 true 时短路不触;false 时按已探索论(COVERAGE 登记,Shroud 批回接)
   Shroud.IsExplored (L948/L1077/L1081): Shroud unported — short-
   circuited untouched when MoveIntoShroud keeps its default true;
   treated-as-explored when false (registered in COVERAGE; rewired with
   the Shroud batch).
 - Parachutable/RejectsOrders/Turreted/Hovers 未移植:TraitOrDefault 查询
   = 空集的上游等价面(不可达分支;登记于 COVERAGE)
   Parachutable/RejectsOrders/Turreted/Hovers unported: the
   TraitOrDefault queries are the upstream-equivalent empty set (the
   unreachable branches; registered in COVERAGE).
 - ShowTargetLines(L952):Settings 面 Phase 6(上游 UI 命令,仿真不可观测)
   ShowTargetLines (L952): the Settings face is Phase 6 (an upstream UI
   command, unobservable in the sim).

## src/mods/move_activities.cpp

UPSTREAM: OpenRA.Mods.Common/Activities/Wait.cs @1f594e320
         实现部分(Wait/Turn/Drag/Nudge/AttackMoveActivity/
         MoveCooldownHelper/MoveAdjacentTo/MoveWithinRange/MoveOnto/
         MoveOntoAndTurn/Follow/LocalMoveIntoTarget/Move +
         MovePart/MoveFirstHalf/MoveSecondHalf;锚文件 = Wait.cs)
         The implementation half of the whole Move activity family (the
         anchor file = Wait.cs).

## src/mods/move_activities.hpp

UPSTREAM: OpenRA.Mods.Common/Activities/Wait.cs @1f594e320
         全族逐语义重写:Activities/Wait.cs L17-30 | Activities/Turn.cs
         L17-41 | Move/Drag.cs L17-71 | Move/Nudge.cs L17-56 |
         Move/AttackMoveActivity.cs L17-106 | Move/MoveCooldownHelper.cs
         L15-106 | Move/MoveAdjacentTo.cs L17-157 |
         Move/MoveWithinRange.cs L17-76 | Move/MoveOnto.cs L17-55 |
         Move/MoveOntoAndTurn.cs L17-42 | Move/Follow.cs L17-86 |
         Move/LocalMoveIntoTarget.cs L17-84 | Move/Move.cs L22-655
         (锚文件 = Wait.cs;族内文件见各行内标注)
         Verbatim-semantics rewrites of the whole Move activity family
         (the anchor file = Wait.cs; the family members carry their own
         inline tags).

机制对照 / Mechanism mapping:
 - 活动对象分配:C# new(GC)→ WorldArena::Create<T>(每局世界区 + 析构登记,
   D26/D27 所有权面);活动相互构造(QueueChild)统一经 NewActivity 助手
   Activity allocation: C#'s new (GC) → WorldArena::Create<T> (the
   per-world arena + destructor records, the D26/D27 ownership face);
   activities constructing one another (QueueChild) uniformly go through
   the NewActivity helper.
 - GetTargets/TargetLineNodes:Activity 基类未引入渲染域面(见
   sim/activity.hpp 头注)—— 本族同形省略(Phase 4/6 随 Target 渲染面)
   GetTargets/TargetLineNodes: the Activity base pulls in no render
   face (see sim/activity.hpp's header) — omitted here the same way
   (with the Target render surface in Phase 4/6).
 - Nudge 的 Aircraft 分支 / AttackMoveActivity 的 AutoTarget·AttackMove
   trait 面:实现类未移植 → 查询空集的上游等价分支(不可达;COVERAGE 登记)
   Nudge's Aircraft branch / AttackMoveActivity's AutoTarget·AttackMove
   trait faces: the implementing classes are unported → the
   upstream-equivalent empty-query branches (unreachable; registered in
   COVERAGE).
 - Move.getPath 的闭包(Func<BlockedByActor,(bool,List)>;三构造变体)
   → std::function 成员(构造期绑定;求值体零分配 —— TakeWhile 物化进
   成员缓冲)
   Move.getPath's closure (Func<BlockedByActor,(bool,List)>; the three
   constructor variants) → a std::function member (bound at
   construction; the evaluation body materializes TakeWhile into a
   member buffer, zero allocation).

## src/mods/music_playlist.cpp

UPSTREAM: OpenRA.Mods.Common/Traits/World/MusicPlaylist.cs @1f594e320
         L22-254 全文。

## src/mods/music_playlist.hpp

UPSTREAM: OpenRA.Mods.Common/Traits/World/MusicPlaylist.cs @1f594e320
         L22-254 全文。

## src/mods/observer_widgets.cpp

UPSTREAM: OpenRA.Mods.Common/Widgets/ObserverArmyIconsWidget.cs @1f594e320
         L24-211 + ObserverProductionIconsWidget.cs L25-281 +
         ObserverSupportPowerIconsWidget.cs L23-199 的实现面
         The implementation face of the three observer icon strips.

## src/mods/observer_widgets.hpp

UPSTREAM: OpenRA.Mods.Common/Widgets/ObserverArmyIconsWidget.cs @1f594e320
         L24-211 + ObserverProductionIconsWidget.cs L25-281 +
         ObserverSupportPowerIconsWidget.cs L23-199(观战三图标条)
         The three observer icon strips (army composition, in-progress
         production, support powers).

## src/mods/order_generators.cpp

UPSTREAM: OpenRA.Mods.Common/Orders/OrderGenerator.cs @1f594e320
         L18-61 全文 + GlobalButtonOrderGenerator.cs @1f594e320 L19-95
         全文 + RepairOrderGenerator.cs @1f594e320 L20-87 全文 +
         BeaconOrderGenerator.cs @1f594e320 L18-40 全文(实现部分)。

## src/mods/order_generators.hpp

UPSTREAM: OpenRA.Mods.Common/Orders/OrderGenerator.cs @1f594e320
         L18-61 全文 + GlobalButtonOrderGenerator.cs @1f594e320 L19-95
         全文 + RepairOrderGenerator.cs @1f594e320 L20-87 全文 +
         BeaconOrderGenerator.cs @1f594e320 L18-40 全文。

## src/mods/parachutable.cpp

UPSTREAM: OpenRA.Mods.Common/Traits/Parachutable.cs @1f594e320

## src/mods/parachutable.hpp

UPSTREAM: OpenRA.Mods.Common/Traits/Parachutable.cs @1f594e320

## src/mods/paratroopers_power.cpp

UPSTREAM: OpenRA.Mods.Common/Traits/SupportPowers/ParatroopersPower.cs
         @1f594e320

## src/mods/paratroopers_power.hpp

UPSTREAM: OpenRA.Mods.Common/Traits/SupportPowers/ParatroopersPower.cs
         @1f594e320

## src/mods/passenger.cpp

UPSTREAM: OpenRA.Mods.Common/Traits/Passenger.cs +
         Orders/EnterAlliedActorTargeter.cs @1f594e320

## src/mods/passenger.hpp

UPSTREAM: OpenRA.Mods.Common/Traits/Passenger.cs +
         Orders/EnterAlliedActorTargeter.cs @1f594e320

## src/mods/pathfinding/cell_info_layer_pool.cpp

UPSTREAM: OpenRA.Mods.Common/Pathfinder/CellInfoLayerPool.cs @1f594e320
         (实现部分 + PathSearch.cs 的 LayerPoolForWorld 表)
         The implementation half of CellInfoLayerPool.cs + PathSearch.cs's
         LayerPoolForWorld table.

## src/mods/pathfinding/cell_info_layer_pool.hpp

UPSTREAM: OpenRA.Mods.Common/Pathfinder/CellInfoLayerPool.cs @1f594e320
         (逐语义重写 + OPT-A2)
         Verbatim-semantics rewrite + OPT-A2.

OPT-A2(docs/OPTIMIZATION_TRACKER.md):上游池层取出时 layer.Clear() 做
O(地图格数) 清零(CellInfo 28B/格,256² 图 ≈1.8MB/次搜索)。C++ 侧以
**世代标记**承载"默认 = Unvisited"语义:每层附 u32 epoch[],取出即
++curEpoch(写入路径盖戳),Get 的未达格 = 默认 CellInfo —— 免清零的
单项最大收益;语义面(读到的默认值)与上游逐字节一致
OPT-A2 (the optimization tracker): upstream zeroes a pooled layer on
checkout (an O(map-cells) Clear; 28B/cell ≈ 1.8MB per search on a
256² map). The C++ side carries the "default = Unvisited" semantics
with **generation stamps**: each layer gains a u32 epoch[] — checkout
is just ++curEpoch (the write path stamps), and Get on an unstamped
cell yields the default CellInfo — the single biggest win of the
no-clear family; the semantic face (the value read back) matches
upstream byte for byte.

池形态:MaxPoolSize=4 + Get/Return 的栈语义照抄;ConditionalWeakTable
<World,…> → 进程级 map 键(世界指针;GC 弱表 → 显式 Erase 面供世界
析构调用 —— COVERAGE 登记)
The pool shape: MaxPoolSize=4 + the Get/Return stack semantics kept;
the ConditionalWeakTable<World,…> → a process-level map keyed by the
world pointer (the GC weak table → an explicit Erase face called from
the world's teardown — registered in COVERAGE).

## src/mods/pathfinding/dense_path_graph.cpp

UPSTREAM: OpenRA.Mods.Common/Pathfinder/DensePathGraph.cs @1f594e320 +
         MapPathGraph.cs + GridPathGraph.cs + SparsePathGraph.cs
         (实现部分)
         The implementation halves.

## src/mods/pathfinding/dense_path_graph.hpp

UPSTREAM: OpenRA.Mods.Common/Pathfinder/DensePathGraph.cs @1f594e320 +
         MapPathGraph.cs + GridPathGraph.cs + SparsePathGraph.cs
         (逐语义重写 + OPT-A2)
         Verbatim-semantics rewrites + OPT-A2.

OPT-A2(docs/OPTIMIZATION_TRACKER.md):上游 GetConnections 每节点展开一次
`new List<GraphConnection>`(DensePathGraph.cs L123 —— 千节点搜索 = 千次
分配)。C++ 侧以**成员暂存缓冲**承载返回契约(每次展开先 Clear;消费
在单次 Expand 内闭合,无重入)—— 零分配等价;邻居表(DirectedNeighbors/
DirectedNeighborsConservative)逐项照抄
OPT-A2 (the optimization tracker): upstream allocates a
`new List<GraphConnection>` once per node expansion (DensePathGraph.cs
L123 — a thousand-node search = a thousand allocations). The C++ side
carries the return contract with a **member scratch buffer** (cleared per
expansion; consumed inside one Expand, no reentrancy) — the allocation-
free equivalent; the neighbor tables (DirectedNeighbors /
DirectedNeighborsConservative) are copied item for item.

## src/mods/pathfinding/hierarchical_path_finder.cpp

UPSTREAM: OpenRA.Mods.Common/Pathfinder/HierarchicalPathFinder.cs
         @1f594e320(实现部分)
         The implementation half of HierarchicalPathFinder.cs.

## src/mods/pathfinding/hierarchical_path_finder.hpp

UPSTREAM: OpenRA.Mods.Common/Pathfinder/HierarchicalPathFinder.cs
         @1f594e320 L97-1284(逐语义重写 + OPT-A2/A10 面)
         Verbatim-semantics rewrite + the OPT-A2/A10 faces.

机制对照 / Mechanism mapping:
 - dirtyGridIndexes(L106:HashSet<int> 稠密小整数)→ 插入序 vector + 位图
   判重(枚举序 = 上游 HashSet 实现序的无删除形态 = 插入序;A10 的位图
   只承担成员测试,不承担枚举序 —— 相邻格重建次序可观测,序保真优先)
   dirtyGridIndexes (L106) → an insertion-ordered vector + a membership
   bitmap (the enumeration order = upstream's no-deletion HashSet order
   = insertion order; A10's bitmap only carries membership — the
   neighbor-rebuild order is observable, order fidelity wins).
 - cellsWithBlockingActor(L107)→ unordered_set(消费面仅 Contains/Add/
   Remove,无序依赖)| cellsWithBlockingActor (L107) → an unordered_set
   (consumed via Contains/Add/Remove only — no order dependence).
 - PathFinderOverlay 形参(L734 等)随调试渲染批 —— 上游全部 `?.` 调用
   在无 overlay 时为空操作,删参即无 overlay 语义的等价形态(COVERAGE)
   The PathFinderOverlay parameters (L734 etc.) land with the debug-
   render batch — upstream's calls are all `?.` no-ops without an
   overlay, so dropping the parameter is the equivalent no-overlay form
   (COVERAGE).
 - ActorIsBlocking/ActorCellIsBlocking 的 Mobile/Building/ITemporaryBlocker
   挂点(L658-662/685)同 Locomotor 的部分覆盖装配面(COVERAGE)
   The ActorIsBlocking/ActorCellIsBlocking Mobile/Building/
   ITemporaryBlocker hooks (L658-662/685) share Locomotor's
   partial-coverage assembly face (COVERAGE).

## src/mods/pathfinding/locomotor.cpp

UPSTREAM: OpenRA.Mods.Common/Traits/World/Locomotor.cs @1f594e320(实现部分)
         The implementation half of Locomotor.cs.

## src/mods/pathfinding/locomotor.hpp

UPSTREAM: OpenRA.Mods.Common/Traits/World/Locomotor.cs @1f594e320 L24-526
         (逐语义重写 + Mobile 挂点的部分覆盖面)
         Verbatim-semantics rewrite + the partial-coverage Mobile hooks.

机制对照 / Mechanism mapping:
 - CellCache(L133:readonly record struct)→ POD;LongBitSet<PlayerBitMask>
   的 Immovable/Crushable 集合照抄(世界掩码语义随玩家创建链本批到位)
   The CellCache (L133) → a POD; the Immovable/Crushable
   LongBitSet<PlayerBitMask> sets kept (the world-mask semantics arrive
   with this batch's player-creation chain).
 - Mobile 挂点(L336-337/484-486:OccupiesSpace as Mobile 的可移动判定、
   IsBlockedBy 的 allied/moving 让行):Mobile trait 随下一批 —— 部分覆盖
   装配面下 `as Mobile` 恒 null → otherIsMovable/otherIsMoving 恒 false,
   ITemporaryBlocker(经 world.RulesContainTemporaryBlocker 门控)与
   Building 的 TransitOnlyCells 判定同面(COVERAGE 登记;代码形态保留
   挂点注释,Mobile 批即插即用)
   The Mobile hooks (L336-337/484-486): the Mobile trait lands next
   batch — under the partial-coverage assembly face `as Mobile` is always
   null → otherIsMovable/otherIsMoving stay false, and the same face
   applies to the ITemporaryBlocker (gated by
   world.RulesContainTemporaryBlocker) and the Building TransitOnlyCells
   checks (in COVERAGE; the code shapes keep hook comments so the Mobile
   batch is plug-in).
 - dirtyCells(L144:HashSet<CPos>)→ 插入序 vector + 位图判重(集面语义:
   Remove 探测即重算)
   The dirtyCells (L144) → an insertion-ordered vector + a bitmap for
   membership (the Remove-probe-recomputes semantics).
 - CellCostChanged 事件 → 回调表 | the CellCostChanged event → callbacks.

## src/mods/pathfinding/path_finder.cpp

UPSTREAM: OpenRA.Mods.Common/Traits/World/PathFinder.cs @1f594e320(实现部分)
         The implementation half of PathFinder.cs.

## src/mods/pathfinding/path_finder.hpp

UPSTREAM: OpenRA.Mods.Common/Traits/World/PathFinder.cs @1f594e320 L22-295
         + Mods.Common/TraitsInterfaces.cs L930-960 的 IPathFinder 接口面
         (逐语义重写 + Mobile 挂点)
         Verbatim-semantics rewrite + the Mobile hook face.

机制对照 / Mechanism mapping:
 - GetActorLocomotor(L286-291:`(Mobile)self.OccupiesSpace).Locomotor`):
   Mobile 随下一批 —— 部分覆盖装配面以注入的解析器承载(缺省 = 世界首
   个 Locomotor;Mobile 批换实 —— COVERAGE 登记)
   GetActorLocomotor (L286-291): Mobile lands next batch — the
   partial-coverage assembly face carries an injected resolver (the
   default = the world's first Locomotor; replaced in the Mobile batch —
   in COVERAGE).
 - pathFinderOverlay 的 `?.` 面随调试渲染批(全部为空操作等价)
   The pathFinderOverlay `?.` faces land with the debug-render batch
   (all equivalent no-ops).

## src/mods/pathfinding/path_graph.hpp

UPSTREAM: OpenRA.Mods.Common/Pathfinder/IPathGraph.cs @1f594e320 +
         Pathfinder/CellInfo.cs + Pathfinder/Grid.cs(逐语义重写)
         Verbatim-semantics rewrites.

机制对照 / Mechanism mapping:
 - GraphEdge/GraphConnection 的三处构造校验文本逐字(负代价/不可达代价/
   自环);CostComparer → 轻型比较结构(可内联,同零虚化)
   The three construction-check texts of GraphEdge/GraphConnection kept
   verbatim (negative cost / unreachable cost / self loop); CostComparer
   → a light comparer struct (inlinable, equally devirtualized).
 - CellInfo 的 Unvisited 单例校验文本逐字 | the CellInfo Unvisited-only
   default check text verbatim.
 - Grid 的 int2 线段相交走 core/polygon.hpp 的 Exts 复刻
   Grid's int2 line intersection goes through the core Exts replica.

## src/mods/pathfinding/path_search.cpp

UPSTREAM: OpenRA.Mods.Common/Pathfinder/PathSearch.cs @1f594e320(实现部分)
         The implementation half of PathSearch.cs.

## src/mods/pathfinding/path_search.hpp

UPSTREAM: OpenRA.Mods.Common/Pathfinder/PathSearch.cs @1f594e320(逐语义重写)
         Verbatim-semantics rewrite.

机制对照 / Mechanism mapping:
 - 开队列 = 上游 PriorityQueue<GraphConnection, GraphConnection.CostComparer>
   的逐语义移植(core/priority_queue.hpp;"层级加倍"堆序保留 —— 平局
   弹出序进入寻路结果,OPT-A2/C4 警示的确定性前提)
   The open queue = the verbatim port of upstream's PriorityQueue
   (core/priority_queue.hpp; the levels-doubled pop order is kept — the
   tie pop order enters the path result, the determinism precondition of
   the OPT-A2/C4 warning).
 - LayerPoolTable(静态 WeakTable)→ cell_info_layer_pool.hpp 的进程级表
   The static LayerPoolTable → the process-level table in
   cell_info_layer_pool.hpp.
 - PathFinderOverlay 的 recorder 面保留为可空接口(overlay 随调试渲染批)
   The PathFinderOverlay recorder face stays a nullable interface (the
   overlay lands with the debug-render batch).

## src/mods/place_building.cpp

UPSTREAM: OpenRA.Mods.Common/Traits/Player/PlaceBuilding.cs @1f594e320
         (实现部分)+ Traits/Buildings/PlaceBuildingVariants.cs +
         Traits/Replacement.cs + Traits/Replaceable.cs
         The implementation halves.

## src/mods/place_building.hpp

UPSTREAM: OpenRA.Mods.Common/Traits/Player/PlaceBuilding.cs @1f594e320
         L18-268 全文 + Traits/Buildings/PlaceBuildingVariants.cs +
         Traits/Replacement.cs + Traits/Replaceable.cs
         The whole of PlaceBuilding.cs + PlaceBuildingVariants.cs +
         Replacement.cs + Replaceable.cs.

## src/mods/place_building_order_generator.cpp

UPSTREAM: OpenRA.Mods.Common/Orders/PlaceBuildingOrderGenerator.cs @1f594e320
         (实现部分)
         The implementation half.

## src/mods/place_building_order_generator.hpp

UPSTREAM: OpenRA.Mods.Common/Orders/PlaceBuildingOrderGenerator.cs @1f594e320
         L24-337 全文
         The whole of PlaceBuildingOrderGenerator.cs L24-337.

## src/mods/player_resources.cpp

UPSTREAM: OpenRA.Mods.Common/Traits/Player/PlayerResources.cs 实现部分
         | The implementation half.

## src/mods/player_resources.hpp

UPSTREAM: OpenRA.Mods.Common/Traits/Player/PlayerResources.cs @1f594e320
         L17-264 全文(InsufficientFunds 的声音/文本通知随注入面;
         ILobbyOptions 的 startingcash 选项声明随 Phase 7 lobby)
         The whole of PlayerResources.cs L17-264 (the
         InsufficientFunds sound/text notifications ride the injection
         face; the ILobbyOptions startingcash declaration rides
         Phase 7's lobby).

机制对照 / Mechanism mapping:
 - GiveCash 的 C# checked 溢出捕获 → __builtin_add_overflow 的饱和
   语义(int.MaxValue 钳位;行为等价)
   GiveCash's C# checked-overflow catch → __builtin_add_overflow's
   saturating semantics (the int.MaxValue clamp; behaviorally
   equivalent).
 - startingcash 的 LobbyInfo 读 = Shroud::Created 同形(Session 最小
   承载面的 "True"/"False" 字典;int.TryParse 语义 = from_chars 全串
   解析,失败回落 DefaultCash)
   The startingcash LobbyInfo read takes Shroud::Created's same shape
   (the Session minimal carrier's dictionary; int.TryParse semantics =
   a full-string from_chars, falling back to DefaultCash).

## src/mods/player_statistics.cpp

UPSTREAM: OpenRA.Mods.Common/Traits/Player/PlayerStatistics.cs @1f594e320
         L22-311 的实现面
         The implementation face of L22-311.

## src/mods/player_statistics.hpp

UPSTREAM: OpenRA.Mods.Common/Traits/Player/PlayerStatistics.cs @1f594e320
         L22-311(观战统计账本 + ArmyUnit 的图标面 + 单位侧记账 trait)
         L22-311 (the observer statistics ledger, ArmyUnit's icon face,
         and the per-unit accounting trait).

## src/mods/plug.cpp

UPSTREAM: OpenRA.Mods.Common/Traits/Plug.cs + Traits/Pluggable.cs
         @1f594e320(实现部分)
         The implementation halves.

## src/mods/plug.hpp

UPSTREAM: OpenRA.Mods.Common/Traits/Plug.cs @1f594e320 L15-24 +
         Traits/Pluggable.cs L15-160
         Plug.cs L15-24 + Pluggable.cs L15-160.

## src/mods/power.cpp

UPSTREAM: OpenRA.Mods.Common/Traits/Power/Player/PowerManager.cs 实现部分
         + Power.cs + Power/AffectedByPowerOutage.cs
         The implementation half.

## src/mods/power.hpp

UPSTREAM: OpenRA.Mods.Common/Traits/Power/Player/PowerManager.cs @1f594e320
         L17-233 全文 + Power.cs L17-57 全文 + Power/AffectedByPowerOutage.cs
         L17-83 全文(逐语义重写;声音/文本通知面与 FluentReference 随
         Phase 6/7 注入面)
         The whole of Power/Player/PowerManager.cs L17-233 + the whole
         of Power.cs L17-57 + the whole of Power/AffectedByPowerOutage.cs
         L17-83 (verbatim-semantics rewrites; the sound/text
         notification faces and the FluentReference ride the Phase 6/7
         injection faces).

机制对照 / Mechanism mapping:
 - powerDrain 的 Dictionary<Actor, int> → std::map<Actor*, int>(键序
   稳定;Tick 的重建循环只做和,序不可观测)
   powerDrain's Dictionary<Actor, int> → std::map<Actor*, int> (a
   stable key order; the Tick rebuild loop only sums, so the order is
   unobservable).
 - 低电通知(Game.RunTime/Sound/TextNotificationsManager)未接:纯
   unsynced 通知面(COVERAGE 登记;同 PlayerResources 的低资通知)
   The low-power notification (Game.RunTime/Sound/
   TextNotificationsManager) is unwired: a purely unsynced notification
   face (registered in COVERAGE; the same as PlayerResources'
   low-funds notification).
 - AffectedByPowerOutage 的 ISelectionBar(UI 面)随 Phase 6
   AffectedByPowerOutage's ISelectionBar (a UI face) rides Phase 6.

## src/mods/primary_building.cpp

UPSTREAM: OpenRA.Mods.Common/Traits/Buildings/PrimaryBuilding.cs @1f594e320
         (实现部分)
         The implementation half.

## src/mods/primary_building.hpp

UPSTREAM: OpenRA.Mods.Common/Traits/Buildings/PrimaryBuilding.cs @1f594e320
         L19-134 全文
         The whole of PrimaryBuilding.cs L19-134.

## src/mods/production.cpp

UPSTREAM: OpenRA.Mods.Common/Traits/Production.cs + OpenRA.Mods.Common/Traits/Player/ProductionQueue.cs + OpenRA.Mods.Common/Traits/Buildings/Exit.cs 实现部分
         The implementation half.

## src/mods/production.hpp

UPSTREAM: OpenRA.Mods.Common/Traits/Production.cs @1f594e320 L15-156 +
         Traits/Player/ProductionQueue.cs L20-813(同步面全文;
         PowerManager 的电源判定/声音与文本通知/UI 只读查询面随
         各自批次)
         Production.cs L15-156 + ProductionQueue.cs L20-813 (the whole
         synchronous face; PowerManager's power predicates/the sound
         and text notifications/the UI read-only queries ride their
         own batches).

机制对照 / Mechanism mapping:
 - ProductionQueue 的 Producible 字典 → 插入序 vector<pair<const
   ActorInfo*, ProductionState>>(AllBuildables 的 rules 枚举序 = 上游
   Actors.Values 的字典序 —— ActorInfoDictionary 的插入序键序;线性
   查找,规模 = 可产项数)
   ProductionQueue's Producible dictionary → the insertion-ordered
   vector<pair<const ActorInfo*, ProductionState>> (AllBuildables'
   rules enumeration order = upstream's Actors.Values dictionary
   order — ActorInfoDictionary's sorted-key order; linear lookup, the
   scale = the producible count).
 - allProducibles/buildableProducibles 的上游 LINQ 惰性查询 → 每次
   消费时物化的函数面(上游每枚举重算;D 系同 AutoTarget 惰性先例)
   Upstream's lazily-evaluated allProducibles/buildableProducibles
   LINQ chains → per-consumption materializing function faces
   (upstream recomputes per enumeration; the D-series AutoTarget lazy
   precedent).
 - ProductionItem.OnComplete 的帧末闭包(捕获 notified 局部)→
   ProductionItem 成员承载闭包状态(同一 ProductionItem 的重复
   OnComplete 共享 notified —— 上游闭包变量提升的等价域;OPT-A9 的
   闭包 → 成员判别式先例)
   ProductionItem.OnComplete's frame-end closure (capturing the
   notified local) → the ProductionItem member carries the closure
   state (the same ProductionItem's repeated OnComplete invocations
   share notified — upstream's hoisted closure-variable equivalent
   domain; OPT-A9's closure→member-discriminator precedent).
 - PowerManager 未移植:ProductionItem 的 pm == null 路径(Remaining
   TimeActual = RemainingTime;Slowdown 跳过 —— 上游无电源 trait 的
   等价分支;登记 COVERAGE)
   PowerManager is unported: ProductionItem's pm == null path
   (RemainingTimeActual = RemainingTime; Slowdown skipped —
   upstream's equivalent branch without the power trait; registered
   in COVERAGE).
 - IProductionTimeModifierInfo/IProductionCostModifierInfo 的修正链
   为空集(GetBuildTime/GetProductionCost 的 modifiers 仅
   BuildDurationModifier 两级 —— 无该 trait 世界的等价域;登记)
   The IProductionTimeModifierInfo/IProductionCostModifierInfo
   modifier chains are empty sets (GetBuildTime/GetProductionCost's
   modifiers carry only the two BuildDurationModifier tiers — the
   equivalent domain of a world without those traits; registered).

## src/mods/production_bar.cpp

UPSTREAM: OpenRA.Mods.Common/Traits/Render/ProductionBar.cs @1f594e320
         L18-105

## src/mods/production_bar.hpp

UPSTREAM: OpenRA.Mods.Common/Traits/Render/ProductionBar.cs @1f594e320
         L18-105 生产进度条(ISelectionBar 的首个真消费面)。
         The production progress bar (ISelectionBar's first real
         consumer).

## src/mods/production_icon_overlay.cpp

UPSTREAM: OpenRA.Mods.Common/Traits/Render/WithProductionIconOverlay.cs +
         Traits/Render/ProductionIconOverlayManager.cs @1f594e320
         (实现部分)
         The implementation halves.

## src/mods/production_icon_overlay.hpp

UPSTREAM: OpenRA.Mods.Common/Traits/Render/WithProductionIconOverlay.cs
         @1f594e320 L17-40 + Traits/Render/ProductionIconOverlayManager.cs
         L19-126 全文
         WithProductionIconOverlay.cs L17-40 + the whole of
         ProductionIconOverlayManager.cs L19-126.

## src/mods/production_palette_widget.cpp

UPSTREAM: OpenRA.Mods.Common/Widgets/ProductionPaletteWidget.cs @1f594e320
         (实现部分)
         The implementation half.

## src/mods/production_palette_widget.hpp

UPSTREAM: OpenRA.Mods.Common/Widgets/ProductionPaletteWidget.cs @1f594e320
         L28-650 全文
         The whole of ProductionPaletteWidget.cs L28-650.

## src/mods/production_support.cpp

UPSTREAM: OpenRA.Mods.Common/Traits/Valued.cs + Buildable.cs +
         Buildings/Exit.cs + Buildings/Reservable.cs + Buildings/
         RallyPoint.cs + Player/ProvidesPrerequisite.cs + Player/
         TechTree.cs + Player/DeveloperMode.cs 实现部分
         The implementation half.

## src/mods/production_support.hpp

UPSTREAM: OpenRA.Mods.Common/Traits/Valued.cs @1f594e320 L17-25 +
         Traits/Buildable.cs L15-69 + Traits/Buildings/Exit.cs L19-95 +
         Traits/Buildings/Reservable.cs L19-106 + Traits/Buildings/
         RallyPoint.cs L19-199 + Traits/Player/ProvidesPrerequisite.cs
         L17-107 + Traits/Player/TechTree.cs L17-200 + Traits/Player/
         DeveloperMode.cs 的同步字段面(L17-60)
         Valued.cs L17-25 + Buildable.cs L15-69 + Buildings/Exit.cs
         L19-95 + Buildings/Reservable.cs L19-106 + Buildings/
         RallyPoint.cs L19-199 + Player/ProvidesPrerequisite.cs L17-107 +
         Player/TechTree.cs L17-200 + DeveloperMode.cs's synchronous
         field face (L17-60).

机制对照 / Mechanism mapping:
 - DeveloperMode:本批最小承载(FastBuild/AllTech 两 cheat 字段的同步
   面;IResolveOrder 的 cheat 命令族/ILobbyOptions 随 Phase 6/7;
   默认全 false = 上游 CheckboxEnabled=false 的等价域 —— 登记 COVERAGE)
   DeveloperMode: this batch's minimal carrier (the synchronous face
   of the FastBuild/AllTech cheat fields; the IResolveOrder cheat
   command family/ILobbyOptions ride Phase 6/7; the all-false default
   = upstream's CheckboxEnabled=false equivalent domain — registered
   in COVERAGE).
 - Reservable 的 Aircraft 面未移植:Reserve/UnReserve 的航空器调度
   (AttackMoveActivity/TakeOff 排队)为空集,IsReserved 走
   MayYieldReservation 恒假分支(IsReserved = reservedForAircraft !=
   null 的等价面;登记 COVERAGE)
   Reservable's Aircraft face is unported: Reserve/UnReserve's
   aircraft scheduling (the AttackMoveActivity/TakeOff queueing) is an
   empty set; IsReserved takes the MayYieldReservation-constantly-false
   branch (the equivalent face of IsReserved = reservedForAircraft !=
   null; registered in COVERAGE).
 - RallyPoint 的 RallyPointIndicator 效果/IIssueOrder 的声音与文本
   通知随渲染/声音注入面
   RallyPoint's RallyPointIndicator effect/IIssueOrder's sound and
   text notifications ride the render/sound injection faces.

## src/mods/production_tabs_widget.cpp

UPSTREAM: OpenRA.Mods.Common/Widgets/ProductionTabsWidget.cs @1f594e320
         (实现部分)
         The implementation half.

## src/mods/production_tabs_widget.hpp

UPSTREAM: OpenRA.Mods.Common/Widgets/ProductionTabsWidget.cs @1f594e320
         L24-371 全文
         The whole of ProductionTabsWidget.cs L24-371.

## src/mods/production_type_button_widget.hpp

UPSTREAM: OpenRA.Mods.Common/Widgets/ProductionTypeButtonWidget.cs @1f594e320
         L12-28 全文
         The whole of ProductionTypeButtonWidget.cs L12-28.

## src/mods/projectiles.cpp

UPSTREAM: OpenRA.Mods.Common/Projectiles/Bullet.cs + InstantHit.cs
         (实现部分) | The implementation half.

## src/mods/projectiles.hpp

UPSTREAM: OpenRA.Mods.Common/Projectiles/Bullet.cs @1f594e320 L26-399 +
         InstantHit.cs L20-96(全文逐语义重写;Render/ContrailRenderable/
         SpriteEffect 的视觉面随渲染批 —— RNG 消耗序保持)
         The whole of Bullet.cs L26-399 + InstantHit.cs L20-96
         (verbatim-semantics rewrites; the Render/ContrailRenderable/
         SpriteEffect visual faces land with the render batch — the RNG
         consumption order is preserved).

机制对照 / Mechanism mapping:
 - 值袋 BulletInfo/InstantHitInfo 记录 → 本文件 Info 解析面;Create 经
   ProjectileRegistry 注册(记录名即键)
   The value-bag BulletInfo/InstantHitInfo records → this file's Info
   parse faces; Create registers through the ProjectileRegistry (the
   record name is the key).
 - Animation(弹体图像)注入面:工厂函数注入(缺省 = 不构造;RNG 的
   Sequences.Random 消耗保持 —— 确定性不依赖视觉装配)
   The Animation (projectile image) face: an injected factory (the
   default constructs nothing; the Sequences.Random RNG consumption is
   preserved — determinism never depends on the visual assembly).
 - Trail 的 SpriteEffect / Contrail 的 ContrailFader/ContrailRenderable:
   视觉效果不构造(渲染批;RNG 消耗保持;COVERAGE 登记)
   Trail's SpriteEffect / Contrail's ContrailFader/ContrailRenderable:
   the visual effects are not constructed (the render batch; the RNG
   consumption is preserved; registered in COVERAGE).
 - Game.Sound.Play(BounceSound) → 声音注入面(同 Armament 形)
   Game.Sound.Play (BounceSound) → the sound injection face (Armament's
   shape).

## src/mods/proximity_capturable.cpp

UPSTREAM: OpenRA.Mods.Common/Traits/ProximityCapturableBase.cs @1f594e320
         L21-211 + ProximityCapturable.cs L18-57 + ProximityCaptor.cs
         L17-28(实现部分;头注见 proximity_capturable.hpp)
         The implementation halves (the header note lives in
         proximity_capturable.hpp).

## src/mods/proximity_capturable.hpp

UPSTREAM: OpenRA.Mods.Common/Traits/ProximityCapturableBase.cs @1f594e320
         L21-211 全文 + ProximityCapturable.cs L18-57 全文 +
         ProximityCaptor.cs L17-28 全文
         The whole of ProximityCapturableBase.cs L21-211 +
         ProximityCapturable.cs L18-57 + ProximityCaptor.cs L17-28.

机制对照 / Mechanism mapping:
 - RulesetLoaded 的 Player-actor ProximityCaptor 前置校验:工厂时点无
   ruleset 面 → 装配面断言由测试/装配处承载(COVERAGE 登记)
   RulesetLoaded's Player-actor ProximityCaptor precondition: the
   factory face has no ruleset — the assembly-side assertion rides the
   test/assembly site (registered in COVERAGE).
 - Game.RunAfterTick(OnOwnerChanged 的恢复)→ 注入面(缺省 = 帧末任务
   的近似一步;恢复时点差登记)
   Game.RunAfterTick (OnOwnerChanged's re-arm) → the injection face (the
   default approximates with the frame-end queue; the one-step timing
   difference is registered).
 - FlashTarget 视觉效果与 RangeCircle 注释圈随渲染装配批(空注释面)
   The FlashTarget visual and the RangeCircle annotation ride the
   render-assembly batch (an empty annotation face here).

## src/mods/radar.cpp

UPSTREAM: OpenRA.Mods.Common/Traits/World/RadarPings.cs @1f594e320
         L20-115 + PlayerRadarTerrain.cs L19-96 + Radar/ 三件的实现面
         L20-115, PlayerRadarTerrain.cs L19-96 and the three Radar/
         traits' implementation face.

## src/mods/radar.hpp

UPSTREAM: OpenRA.Mods.Common/Traits/World/RadarPings.cs @1f594e320
         L20-115 + Traits/Player/PlayerRadarTerrain.cs L19-96 +
         Traits/Radar/{AppearsOnRadar,ProvidesRadar,RadarColorFromTerrain}.cs
         (雷达链五件:ping 星箭头动画/玩家雷达地形缓存/minimap 签名三元
         组/纯开关/地形色覆写)
         L20-115 plus PlayerRadarTerrain.cs L19-96 and the three Radar/
         traits (the five radar-chain pieces: the ping star animation,
         the player radar-terrain cache, the minimap signature triple,
         the bare switch, the terrain-color override).

## src/mods/radar_widget.cpp

UPSTREAM: OpenRA.Mods.Common/Widgets/RadarWidget.cs @1f594e320
         L25-539 的实现面
         The implementation face of L25-539.

## src/mods/radar_widget.hpp

UPSTREAM: OpenRA.Mods.Common/Widgets/RadarWidget.cs @1f594e320
         L25-539 全文(四层 2x2 单 sheet 的雷达 minimap:地形/迷雾/actor
         三层 + 视口框与 ping 星;开关动画与 Classic 的 minimap 移动)
         The whole of RadarWidget.cs L25-539 (the radar minimap on a
         single 2x2 four-layer sheet: terrain/shroud/actor plus the
         viewport rect and ping stars; the toggle animation and Classic's
         minimap movement).

## src/mods/refinery.cpp

UPSTREAM: OpenRA.Mods.Common/Traits/Buildings/Refinery.cs 实现部分
         The implementation half.

## src/mods/refinery.hpp

UPSTREAM: OpenRA.Mods.Common/Traits/Buildings/Refinery.cs @1f594e320
         L19-110 全文(逐语义重写;Requires<WithSpriteBodyInfo> 的
         约束面 = WithSpriteBody 未移植的注册侧跳过 —— COVERAGE 登记;
         FloatingText 的 ShowTicks 显示面随 Phase 6)
         The whole of Buildings/Refinery.cs L19-110 (a verbatim-semantics
         rewrite; the Requires<WithSpriteBodyInfo> constraint face = the
         registration-side skip with WithSpriteBody unported — registered
         in COVERAGE; the ShowTicks FloatingText display face rides
         Phase 6).

## src/mods/render_sprites.cpp

UPSTREAM: OpenRA.Mods.Common/Traits/Render/RenderSprites.cs @1f594e320
         L24-302(实现部分;头注见 render_sprites.hpp)
         The implementation half of RenderSprites.cs L24-302 (the header
         note lives in render_sprites.hpp).

## src/mods/render_sprites.hpp

UPSTREAM: OpenRA.Mods.Common/Traits/Render/RenderSprites.cs @1f594e320
         L24-302 全文(RenderSprites 的仿真/装配语义面;RenderPreview 的
         IActorPreview 面随 Phase 6 预览装配 —— IRenderActorPreviewSpritesInfo
         的接口位保留空实现)
         The whole of RenderSprites.cs L24-302 (the sim/assembly semantic
         faces; RenderPreview's IActorPreview faces ride Phase 6 — the
         IRenderActorPreviewSpritesInfo slot keeps an empty
         implementation).

机制对照 / Mechanism mapping:
 - ctor 的 world.Map.Sequences → Animation::Deps 注入(SequenceSet 取自
   Map::Sequences 装配面;高度查询 = Map::DistanceAboveTerrain)
   The ctor's world.Map.Sequences → the Animation::Deps injection (the
   SequenceSet from Map::Sequences' assembly face; the height query =
   Map::DistanceAboveTerrain).
 - RenderAnimations 的 yield 生成器闭包(ctror 即挂、每次 Render 重放)
   → Render() 内的即时遍历(逐枚举求值等价 —— 生成器逐项读当前 anims)
   RenderAnimations's yield-generator closure (hung at the ctor, replayed
   per Render) → Render()'s on-the-spot iteration (equivalent
   per-enumeration evaluation — the generator reads the live anims).
 - Game.CosmeticRandom 域不涉及;StandSequences 随机在 WithInfantryBody
   (LocalRandom 域,见其头部)
   No Game.CosmeticRandom domain here; the StandSequences pick lives in
   WithInfantryBody (the LocalRandom domain, see its header).

## src/mods/render_utils.hpp

UPSTREAM: OpenRA.Mods.Common/Traits/Render/RenderUtils.cs @1f594e320
         L14-21 全文(ZOffsetFromCenter 单函数)
         The whole of RenderUtils.cs L14-21 (the single ZOffsetFromCenter).

## src/mods/repairable_building.cpp

UPSTREAM: OpenRA.Mods.Common/Traits/Buildings/RepairableBuilding.cs
         @1f594e320 L22-208 全文(实现部分)。

## src/mods/repairable_building.hpp

UPSTREAM: OpenRA.Mods.Common/Traits/Buildings/RepairableBuilding.cs
         @1f594e320 L22-208 全文。

## src/mods/repairable_near.cpp

UPSTREAM: OpenRA.Mods.Common/Traits/RepairableNear.cs @1f594e320
         L22-143 全文(实现部分)。

## src/mods/repairable_near.hpp

UPSTREAM: OpenRA.Mods.Common/Traits/RepairableNear.cs @1f594e320
         L22-143 全文。

## src/mods/resource_layer.cpp

UPSTREAM: OpenRA.Mods.Common/Traits/World/ResourceLayer.cs 实现部分
         + World/ResourceClaimLayer.cs | The implementation half.

## src/mods/resource_layer.hpp

UPSTREAM: OpenRA.Mods.Common/Traits/World/ResourceLayer.cs @1f594e320
         L20-302 全文 + World/ResourceClaimLayer.cs L17-74 全文(逐语义
         重写;CellChanged 事件 → 回调表)
         The whole of World/ResourceLayer.cs L20-302 + the whole of
         World/ResourceClaimLayer.cs L17-74 (verbatim-semantics
         rewrites; the CellChanged event → a callback list).

机制对照 / Mechanism mapping:
 - FrozenDictionary<string, ResourceTypeInfo> → 插入序 vector<pair> +
   线性查找(yaml 声明序;ResourceTypesByIndex 的构建序 = 声明序,键冲突
   = 上游 ToDictionary 抛的等价抛)
   FrozenDictionary<string, ResourceTypeInfo> → an insertion-ordered
   vector<pair> + linear lookup (the yaml declaration order;
   ResourceTypesByIndex builds in declaration order, a key clash throws
   as upstream's ToDictionary does).
 - AllowedTerrainTypes 的 FrozenSet<string> → vector<string>(去重集合
   语义;Contains 线性)
   AllowedTerrainTypes' FrozenSet<string> → vector<string> (the
   dedup-set semantics; a linear Contains).
 - CellChanged 事件(Action<CPos, string>)→ 回调表(construct 序;本批
   无订阅者 —— ResourceRenderer 随渲染批)
   The CellChanged event (Action<CPos, string>) → a callback list (in
   registration order; no subscribers this batch — ResourceRenderer
   rides the render batch).
 - claimByCell/claimByActor 字典 → 两个 map(键序稳定;List 清理序 =
   RemoveAll 的首见移除语义)
   The claimByCell/claimByActor dictionaries → two maps (stable key
   order; the List cleanup keeps RemoveAll's remove-all-matching
   semantics).

## src/mods/scaredy_cat.cpp

UPSTREAM: OpenRA.Mods.Common/Traits/Infantry/ScaredyCat.cs 实现部分
         The implementation half.

## src/mods/scaredy_cat.hpp

UPSTREAM: OpenRA.Mods.Common/Traits/Infantry/ScaredyCat.cs @1f594e320 L21-40(Info
         字段面)+ L42-58(运行时类的 ISync 面;第十二批 G2 黄金对拍
         补面 — ^CivInfantry 的 ScaredyCat 条目缺失平移 SyncHash 的 n
         计数。恐慌移动/序列前缀面随行为批)
         The Info field face (L21-40) + the runtime class's ISync face
         (L42-58; the batch-12 G2 golden differential's missing face —
         ^CivInfantry's absent ScaredyCat entry shifts the SyncHash n
         counter. The panic-move/sequence-prefix faces ride the
         behaviour batch).

## src/mods/seeds_resource.cpp

UPSTREAM: OpenRA.Mods.Common/Traits/SeedsResource.cs 实现部分
         The implementation half.

## src/mods/seeds_resource.hpp

UPSTREAM: OpenRA.Mods.Common/Traits/SeedsResource.cs @1f594e320 L21-81 全文
         (第十二批 G2 黄金对拍修出:mine 的 Tick 播种 RandomWalk 是
         SharedRandom 的每 tick 大宗消费者,缺失即 desync)
         The whole of SeedsResource.cs L21-81 (rooted out by the
         batch-12 G2 golden differential: the mines' tick-seeding
         RandomWalk is a bulk SharedRandom consumer per tick — its
         absence is an immediate desync).

## src/mods/selectable.cpp

UPSTREAM: OpenRA.Mods.Common/Traits/Selectable.cs @1f594e320 L15-55 +
         Interactable.cs L15-105(逐语句重写;机制对照见 selectable.hpp
         头注)
         Statement-by-statement; the mechanism mapping lives in
         selectable.hpp's header note.

## src/mods/selectable.hpp

UPSTREAM: OpenRA.Mods.Common/Traits/Selectable.cs @1f594e320 L15-55 全文 +
         Interactable.cs L15-105 全文
         The whole of Selectable.cs L15-55 + Interactable.cs L15-105.

机制对照 / Mechanism mapping:
 - Interactable 的 MouseoverBounds/PolygonBounds/DecorationBounds 依赖
   WorldRenderer/IAutoMouseBounds(渲染批):Bounds/DecorationBounds/
   Polygon 的值面与 polygonCenterOffset 预计算全量,屏幕换算面随渲染批
   Interactable's MouseoverBounds/PolygonBounds/DecorationBounds depend
   on WorldRenderer/IAutoMouseBounds (the render batch): the
   Bounds/DecorationBounds/Polygon value faces and the
   polygonCenterOffset precalculation are kept in full; the screen
   conversion rides the render batch.
 - 上游 SelectableInfo : InteractableInfo(单表继承);C++ 侧两 Info
   解析面分立 + Selectable 持 Interactable 基段(基类仅 mouse-bounds
   渲染面,数据面无字段 —— 直挂 TraitBase + 行为位)
   Upstream's SelectableInfo : InteractableInfo (single-table
   inheritance); the C++ side splits the two Info parse faces and
   Selectable carries no Interactable base segment (the base holds
   only the mouse-bounds render face — no data fields — so TraitBase
   + the behaviour bit mount directly).

## src/mods/selection_decorations.cpp

UPSTREAM: OpenRA.Mods.Common/Traits/Render/SelectionDecorationsBase.cs
         @1f594e320

## src/mods/selection_decorations.hpp

UPSTREAM: OpenRA.Mods.Common/Traits/Render/SelectionDecorationsBase.cs
         @1f594e320 L18-139 + SelectionDecorations.cs L18-93 +
         Graphics/SelectionBoxAnnotationRenderable.cs +
         SelectionBarsAnnotationRenderable.cs 选择框/血条/装饰注释族。
         StatusBars 读取 = 注入 provider(缺省 Standard;Settings 实体随
         Phase 6);注释 renderable 的每帧 new → trait 所有可能重配置对象
         经 Custom 槽入收集流。
         The selection-box/health-bar/decoration family. The StatusBars
         read = an injected provider (Standard by default; the Settings
         entity rides Phase 6); the annotation renderables' per-frame new
         → the trait-owned objects reconfigured per frame, entering the
         collection flow via the Custom slot.

## src/mods/sellable.cpp

UPSTREAM: OpenRA.Mods.Common/Traits/CustomSellValue.cs @1f594e320
         L18-38 全文 + OpenRA.Mods.Common/Traits/Sellable.cs @1f594e320
         L21-122 全文 + OpenRA.Mods.Common/Activities/Sell.cs @1f594e320
         L19-58 全文(实现部分)。

## src/mods/sellable.hpp

UPSTREAM: OpenRA.Mods.Common/Traits/CustomSellValue.cs @1f594e320
         L18-38 全文 + OpenRA.Mods.Common/Traits/Sellable.cs @1f594e320
         L21-122 全文 + OpenRA.Mods.Common/Activities/Sell.cs @1f594e320
         L19-58 全文。

## src/mods/sequence_loader_factory.cpp

UPSTREAM: OpenRA.Game/ModData.cs @1f594e320 L111 + OpenRA.Game/ObjectCreator.cs
         @1f594e320 L149-156(sequence_loader_factory.hpp 的实现)
         Implementation of sequence_loader_factory.hpp.

## src/mods/sequence_loader_factory.hpp

UPSTREAM: OpenRA.Game/ModData.cs @1f594e320 L111(ObjectCreator.GetLoader&lt;
         ISpriteSequenceLoader&gt;(Manifest.SpriteSequenceFormat, "sequence"))+
          OpenRA.Game/ObjectCreator.cs @1f594e320 L149-156
SpriteSequenceFormat 名 → 序列加载器实例(Manifest 名分派;未知名抛
InvalidOperationException 文本逐字)。
The SpriteSequenceFormat name → the sequence-loader instance (Manifest
name dispatch; an unknown name throws the InvalidOperationException text
verbatim).

## src/mods/server_creation_logic.cpp

UPSTREAM: OpenRA.Mods.Common/Widgets/Logic/ServerCreationLogic.cs
         @1f594e320(实现;头注见 server_creation_logic.hpp)
         Implementation; the header notes live in server_creation_logic.hpp.

## src/mods/server_creation_logic.hpp

UPSTREAM: OpenRA.Mods.Common/Widgets/Logic/ServerCreationLogic.cs
         @1f594e320 L20-256 全文(NAT 公告三标签方程 + CreateAndJoin 的
         设置存盘与 Game.CreateServer/SocketException 10048 分流)。
         The whole of ServerCreationLogic.cs L20-256 (the NAT-notice
         three-label layout equations + CreateAndJoin's settings save and
         the Game.CreateServer/SocketException-10048 split).

## src/mods/smudge_layer.cpp

UPSTREAM: OpenRA.Mods.Common/Traits/World/SmudgeLayer.cs 实现部分
         (同步记账面)| The implementation half (the synchronous
         bookkeeping face).

## src/mods/smudge_layer.hpp

UPSTREAM: OpenRA.Mods.Common/Traits/World/SmudgeLayer.cs @1f594e320
         L26-121(Info + LoadInitialSmudges)+ L123-133/163-186/188-196
         (SmudgeLayer 的同步记账面:AddSmudge/RemoveSmudge/tiles·dirty)
         The Info + LoadInitialSmudges of L26-121 + the synchronous
         bookkeeping face of L123-133/163-186/188-196 (AddSmudge/
         RemoveSmudge/tiles·dirty).

机制对照 / Mechanism mapping:
 - 渲染面(TerrainSpriteLayer/paletteReference/IRenderOverlay.Render/
   ITickRender 的可见性推进/WorldLoaded 的 InitialSmudges 上传)随
   渲染批;同步面 = tiles/dirty 两字典记账(Type/Depth;删除判据 =
   上游 Sequence==null 的 bool 化)
   The render faces (the TerrainSpriteLayer/paletteReference/
   IRenderOverlay.Render/ITickRender's visibility advance/WorldLoaded's
   InitialSmudges upload) land with the render batch; the synchronous
   face = the tiles/dirty dictionary bookkeeping (Type/Depth; the
   deletion marker = a bool of upstream's Sequence==null).
 - AddSmudge 的 CosmeticRandom 消耗(smoke 概率 + smudges.Keys.Random)
   与 depth 增殖(需序列帧长)未接:上游走 Game.CosmeticRandom(非同步
   RNG,不进 SyncHash/replay),烟效与深度增殖为纯视觉差异;smudge 类型
   退化为 Info.Type 单选(登记 COVERAGE)
   AddSmudge's CosmeticRandom consumptions (the smoke probability +
   smudges.Keys.Random) and the depth increment (which needs the
   sequence's frame count) are unwired: upstream rides
   Game.CosmeticRandom (a non-synced RNG that never enters SyncHash/
   replays), so the smoke effect and the depth growth are purely visual
   differences; the smudge type degrades to the single Info.Type pick
   (registered in COVERAGE).
 - CellEntryChanged 的四订阅(Tiles/CustomTerrain/Ramp/Height)随地图
   事件面(当前 Map 无变更写者;RemoveUnacceptableSmudgeOnCellChange
   的消费为零 —— 等价;登记)
   The four CellEntryChanged subscriptions (Tiles/CustomTerrain/Ramp/
   Height) ride the map-event face (the current Map carries no writers,
   so RemoveUnacceptableSmudgeOnCellChange has zero consumers — the
   equivalent; registered).

## src/mods/spawn_map_actors.cpp

UPSTREAM: OpenRA.Mods.Common/Traits/World/SpawnMapActors.cs @1f594e320
         L17-75 + Game/Map/ActorReference.cs L17-113(逐语句重写;机制
         对照见 spawn_map_actors.hpp 头注)
         Statement-by-statement; the mechanism mapping lives in
         spawn_map_actors.hpp's header note.

## src/mods/spawn_map_actors.hpp

UPSTREAM: OpenRA.Mods.Common/Traits/World/SpawnMapActors.cs @1f594e320
         L17-75 全文 + Game/Map/ActorReference.cs L17-113 的装载面
         (InitRegistry 的 from-yaml 工厂 = LoadInit 的 C++ 承载)
         The whole of SpawnMapActors.cs L17-75 + the loading face of
         Game/Map/ActorReference.cs L17-113 (InitRegistry's from-yaml
         factories = LoadInit's C++ carrier).

机制对照 / Mechanism mapping:
 - 上游 ActorReference 的 Lazy initDict → 装载帧存活的 unique_ptr
   vector(CreateActor 同步消费后释放 —— GC 的显式等价)
   Upstream's Lazy initDict → a load-frame-lived unique_ptr vector
   (released after CreateActor's synchronous consumption — the
   explicit GC equivalent).
 - IPreventMapSpawn 上游 mods 无实现者 → 空集直通
   IPreventMapSpawn has no upstream mod implementors → an empty-set
   pass-through.

## src/mods/sprite_effect.cpp

UPSTREAM: OpenRA.Mods.Common/Effects/SpriteEffect.cs @1f594e320 L18-86

## src/mods/sprite_effect.hpp

UPSTREAM: OpenRA.Mods.Common/Effects/SpriteEffect.cs @1f594e320 L18-86
         一次性序列特效:delay 帧后播完自移除(WithDeathAnimation 的
         效果载体)。posFunc/facingFunc 构造形态随消费批。
         The one-shot sequence effect: plays out after the delay frames
         and removes itself (WithDeathAnimation's carrier). The
         posFunc/facingFunc constructor forms ride their consumer
         batches.

## src/mods/stores_resources.cpp

UPSTREAM: OpenRA.Mods.Common/Traits/StoresResources.cs 实现部分
         + StoresPlayerResources.cs | The implementation half.

## src/mods/stores_resources.hpp

UPSTREAM: OpenRA.Mods.Common/Traits/StoresResources.cs @1f594e320 L18-107
         全文 + StoresPlayerResources.cs L14-67 全文(逐语义重写)
         The whole of StoresResources.cs L18-107 + the whole of
         StoresPlayerResources.cs L14-67 (verbatim-semantics rewrites).

机制对照 / Mechanism mapping:
 - contents 的 Dictionary<string, int> → 插入序 vector<pair>(构造序 =
   Info.Resources 声明序;ContentHash 的键长位移照抄)
   contents' Dictionary<string, int> → an insertion-ordered vector<pair>
   (the construct order = Info.Resources' declaration order; the
   ContentHash's key-length shift copied).

## src/mods/support_power_effects.cpp

UPSTREAM: OpenRA.Mods.Common/Effects/Beacon.cs + RevealShroudEffect.cs +
         Projectiles/NukeLaunch.cs + Graphics/RangeCircleAnnotationRenderable.cs
         的实现面
         The implementation face.

## src/mods/support_power_timer_widget.cpp

UPSTREAM: OpenRA.Mods.Common/Widgets/SupportPowerTimerWidget.cs @1f594e320
         L23-97 的实现面
         The implementation face of L23-97.

## src/mods/support_power_timer_widget.hpp

UPSTREAM: OpenRA.Mods.Common/Widgets/SupportPowerTimerWidget.cs @1f594e320
         L23-97 全文(观战的支持 power 充能倒计时列)
         The whole of SupportPowerTimerWidget.cs L23-97 (the observer's
         support-power charge countdown list).

## src/mods/support_powers.cpp

UPSTREAM: OpenRA.Mods.Common/Traits/SupportPowers/SupportPowerManager.cs
         @1f594e320 L21-320 + SupportPower.cs L18-260 的实现面
         The implementation face of L21-320 + SupportPower.cs L18-260.

## src/mods/support_powers.hpp

UPSTREAM: OpenRA.Mods.Common/Traits/SupportPowers/SupportPowerManager.cs
         @1f594e320 L21-320(SupportPowerManager/SupportPowerInstance/
         SelectGenericPowerTarget)+ SupportPower.cs L18-260(基类与
         Info 字段面;具体 power 子类随其批)
         L21-320 (SupportPowerManager/SupportPowerInstance/
         SelectGenericPowerTarget) plus SupportPower.cs L18-260 (the
         base class and Info field face; the concrete power subclasses
         ride their own batches).

## src/mods/support_powers_widget.cpp

UPSTREAM: OpenRA.Mods.Common/Widgets/SupportPowersWidget.cs @1f594e320
         L24-297 的实现面
         The implementation face of L24-297.

## src/mods/support_powers_widget.hpp

UPSTREAM: OpenRA.Mods.Common/Widgets/SupportPowersWidget.cs @1f594e320
         L24-297 全文(支持 power 图标列:充能钟面/READY·HOLD·剩余时
         间叠字/热键/tooltip 容器)
         The whole of SupportPowersWidget.cs L24-297 (the support-power
         icon column: charge clock, READY/HOLD/remaining-time overlay,
         hotkeys, the tooltip container).

## src/mods/targetable.cpp

UPSTREAM: OpenRA.Mods.Common/Traits/Targetable.cs 实现部分
         The implementation half.

## src/mods/targetable.hpp

UPSTREAM: OpenRA.Mods.Common/Traits/Targetable.cs @1f594e320 L18-55 全文
         (逐语义重写;Cloak trait 未移植 → cloaks 空集的等价面,
         COVERAGE 登记)
         The whole of Targetable.cs L18-55 (verbatim-semantics rewrite;
         the Cloak trait is unported → the empty-cloaks equivalent face,
         registered in COVERAGE).

## src/mods/tileset_specific_sprite_sequence.cpp

UPSTREAM: OpenRA.Mods.Common/Graphics/TilesetSpecificSpriteSequence.cs @1f594e320 L20-93
         (tileset_specific_sprite_sequence.hpp 的实现)
         Implementation of tileset_specific_sprite_sequence.hpp.

## src/mods/tileset_specific_sprite_sequence.hpp

UPSTREAM: OpenRA.Mods.Common/Graphics/TilesetSpecificSpriteSequence.cs @1f594e320 L20-93
带地形集变体的序列:TilesetFilenames/TilesetFilenamesPattern 命中当前
tileset 时覆盖 Filename/FilenamePattern,未命中回退 Default 的解析。
A sequence with tileset-specific variants: a TilesetFilenames/
TilesetFilenamesPattern hit for the active tileset overrides Filename/
FilenamePattern, and a miss falls back to the Default parsing.

## src/mods/tooltip.cpp

UPSTREAM: OpenRA.Mods.Common/Traits/Tooltip.cs @1f594e320
         L16-102(实现面)
         L16-102 (the implementation face).

## src/mods/tooltip.hpp

UPSTREAM: OpenRA.Mods.Common/Traits/Tooltip.cs @1f594e320
         L16-102(TooltipInfoBase/TooltipInfo 的字段面与
         TooltipForPlayerStance 的 Fluent 三前缀链 + Tooltip 运行时)
         L16-102 (the TooltipInfoBase/TooltipInfo field face with
         TooltipForPlayerStance's Fluent three-prefix chain, plus the
         Tooltip runtime).

## src/mods/trait_registry_batch10.cpp

UPSTREAM: OpenRA.Mods.Common/Traits 第十批 mods trait 注册表装配
         (PlaceBuilding/LineBuild/Plug/Pluggable/PrimaryBuilding/
         BulkProductionQueue/ProductionIconOverlayManager/
         WithProductionIconOverlay/FootprintPlaceBuildingPreview/
         Replacement/Replaceable/PlaceBuildingVariants;Info 名 → 工厂 →
         WorldArena)
         The batch-10 registry assembly (Info name → factory →
         WorldArena).

## src/mods/trait_registry_batch11.cpp

UPSTREAM: OpenRA.Mods.Common/Traits 第十一批 mods trait 注册表装配
         (RadarPings/PlayerRadarTerrain/AppearsOnRadar/ProvidesRadar/
         RadarColorFromTerrain/SupportPowerManager/Tooltip/
         PlayerStatistics/UpdatesPlayerStatistics;Info 名 → 工厂 →
         WorldArena)
         The batch-11 registry assembly (Info name → factory →
         WorldArena).

## src/mods/trait_registry_batch13.cpp

UPSTREAM: OpenRA.Mods.Common/Traits/SupportPowers/NukePower.cs +
         SpawnActorPower.cs + ProduceActorPower.cs +
         GrantExternalConditionPower.cs(NukePowerInfo/SpawnActorPowerInfo/
         ProduceActorPowerInfo/GrantExternalConditionPowerInfo 的注册装配)

## src/mods/trait_registry_batch14.cpp

UPSTREAM: OpenRA.Mods.Common/Traits/Air/Aircraft.cs + AmmoPool.cs +
         Rearmable.cs + Repairable.cs + RepairsUnits.cs +
         Air/FallsToEarth.cs + Air/AttackAircraft.cs + AttackBomber.cs +
         SupportPowers/AirstrikePower.cs(注册装配)

## src/mods/trait_registry_batch15.cpp

UPSTREAM: OpenRA.Mods.Common/Traits/Cargo.cs + Passenger.cs +
         ParaDrop.cs + Parachutable.cs + Render/WithParachute.cs +
         SupportPowers/ParatroopersPower.cs(注册装配)

## src/mods/trait_registry_batch16.cpp

UPSTREAM: OpenRA.Mods.Common/Traits/Carryall.cs + Carryable.cs +
         AutoCarryall.cs + AutoCarryable.cs + CarryableHarvester.cs
         (注册装配)

## src/mods/trait_registry_batch17.cpp

UPSTREAM: OpenRA.Mods.Common/Traits/CustomSellValue.cs + Sellable.cs +
         RepairableNear.cs + RepairableBuilding.cs @1f594e320
         (注册装配)

## src/mods/trait_registry_batch18.cpp

UPSTREAM: OpenRA.Mods.Common/Traits/World/MusicPlaylist.cs @1f594e320
         (注册装配)。

## src/mods/trait_registry_batch7.cpp

UPSTREAM: OpenRA.Mods.Common/Traits/Conditions/GrantCondition.cs @1f594e320(第七批 mods trait 的
         注册表装配:Conditions 31 件 + ExternalCondition/Proximity +
         Cloak/DetectCloaked/IgnoresCloak + GainsExperience/GivesExperience/
         GainsExperienceMultiplier/PlayerExperience + CaptureManager/
         Capturable/Captures/GivesCashOnCapture + Selectable/Interactable +
         SpawnMapActors;与 RegisterCommonTraits 同形 —— Info 名 → 工厂 →
         WorldArena)
         The batch-7 mods-trait registry assembly (every file listed in
         the batch's headers; the same shape as RegisterCommonTraits —
         the Info name → factory → WorldArena).

## src/mods/trait_registry_batch8.cpp

UPSTREAM: OpenRA.Mods.Common/Traits/Render/RenderSprites.cs @1f594e320(第八批
         mods trait 的注册表装配:RenderSprites/WithSpriteBody/
         WithFacingSpriteBody/WithInfantryBody/WithSpriteTurret/
         WithMakeAnimation/WithMakeOverlay + ProximityCapturable 族;
         与 RegisterCommonTraits 同形 —— Info 名 → 工厂 → WorldArena)
         The batch-8 mods-trait registry assembly: RenderSprites/
         WithSpriteBody/WithFacingSpriteBody/WithInfantryBody/
         WithSpriteTurret/WithMakeAnimation/WithMakeOverlay + the
         ProximityCapturable family; the same shape as
         RegisterCommonTraits — the Info name → factory → WorldArena.

## src/mods/trait_registry_batch9.cpp

UPSTREAM: OpenRA.Mods.Common/Traits/Render/SelectionDecorations.cs @1f594e320
         第九批 mods trait 注册表装配(SelectionDecorations/
         WithDecoration/WithDeathAnimation/WithDamageOverlay/
         ProductionBar + pips 装饰;Info 名 → 工厂 → WorldArena)。
         The batch-9 registry assembly (Info name → factory → WorldArena).

## src/mods/turreted.cpp

UPSTREAM: OpenRA.Mods.Common/Traits/Turreted.cs 实现部分
         The implementation half.

## src/mods/turreted.hpp

UPSTREAM: OpenRA.Mods.Common/Traits/Turreted.cs @1f594e320 L19-333(全文逐语
         义重写;IActorPreviewInitInfo 的预览面/IEditorActorOptions 的编辑
         器 UI 面随 Phase 6 —— ValueActorInit/TurretFacingInit 的 init 面
         本批全承载)
         The whole of Turreted.cs L19-333 (a verbatim-semantics rewrite;
         IActorPreviewInitInfo's preview face / IEditorActorOptions's
         editor-UI face land with Phase 6 — the ValueActorInit/
         TurretFacingInit init faces are carried in full this batch).

机制对照 / Mechanism mapping:
 - PausableConditionalTrait<TurretedInfo> → sim::ConditionalTraitCore<
   Turreted>(四钩子静态派发;同 Mobile 形)
   PausableConditionalTrait<TurretedInfo> → sim::ConditionalTraitCore<
   Turreted> (the four statically dispatched hooks; Mobile's shape).
 - WorldFacingFromInit 的 Func<WAngle> 闭包族 → std::function<WAngle()>
   (捕获 facing/dynamicInit 值副本;上游闭包语义保真)
   WorldFacingFromInit's Func<WAngle> closure family →
   std::function<WAngle()> (value-captured facing/dynamicInit; the
   upstream closure semantics kept).
 - Created 的 SingleOrDefault(AttackTurreted 匹配)→ 首匹配 + 多匹配抛
   "Sequence contains more than one matching element"(上游 Single 语义)
   Created's SingleOrDefault (the AttackTurreted match) → the first
   match + the multi-match throw of "Sequence contains more than one
   matching element" (upstream Single's semantics).
 - QuantizedFacings 的 ArgumentOutOfRangeException 消息文本逐字(.NET
   三段格式)
   QuantizedFacings' ArgumentOutOfRangeException keeps the verbatim
   .NET three-segment message text.

## src/mods/unit_order_generator.cpp

UPSTREAM: OpenRA.Mods.Common/Orders/UnitOrderGenerator.cs @1f594e320
         (实现部分)
         The implementation half of UnitOrderGenerator.cs.

## src/mods/unit_order_generator.hpp

UPSTREAM: OpenRA.Mods.Common/Orders/OrderGenerator.cs @1f594e320 L18-61 +
         Orders/UnitOrderGenerator.cs L21-227(逐语义重写 + 注入面)
         Verbatim-semantics rewrite + the injection faces.

机制对照 / Mechanism mapping:
 - OrderGenerator 基类的 ActionButton/CancelButton → GameSettingsFace 注入
   (D101 Settings 批换实体);classic 样式 ctor 清空选择(L26-28)照抄
   The base's ActionButton/CancelButton → the GameSettingsFace injection
   (replaced in the D101 Settings batch); the classic-style ctor's
   selection clear (L26-28) kept.
 - TextNotificationsManager.Debug(CheckSameOrder,L204-211)→ 注入钩子
   (通知管理器随 Phase 6)| the injected hook (the notification manager
   lands in Phase 6).

## src/mods/util.hpp

UPSTREAM: OpenRA.Mods.Common/Util.cs @1f594e320 L53-73(TickFacing)+
         L78-84(IndexFacing)+ L105-107(QuantizeFacing)+ L129-140
         (BetweenCells)+ L181-185(AreAdjacentCells)+ L197-201
         (AdjacentCells;ApplyPercentageModifiers 已在
         core/percent_modifiers.hpp = OPT-A1)
         Util.cs L53-73 (TickFacing) + L78-84 (IndexFacing) + L105-107
         (QuantizeFacing) + L129-140 (BetweenCells) + L181-185
         (AreAdjacentCells) + L197-201 (AdjacentCells;
         ApplyPercentageModifiers already lives in
         core/percent_modifiers.hpp = OPT-A1).

## src/mods/video_player_widget.cpp

UPSTREAM: OpenRA.Mods.Common/Widgets/VideoPlayerWidget.cs @1f594e320
         L22-313 的实现面
         The implementation face of L22-313.

## src/mods/video_player_widget.hpp

UPSTREAM: OpenRA.Mods.Common/Widgets/VideoPlayerWidget.cs @1f594e320
         L22-313 全文(VQA/WSA 过场播放器:逐帧 BGRA 上传 + 扫描线
         overlay + 音频时钟驱动 + ESC 跳过)
         The whole of VideoPlayerWidget.cs L22-313 (the VQA/WSA cutscene
         player: per-frame BGRA upload, the scan-line overlay, the
         audio-clock drive, ESC skipping).

## src/mods/wands.cpp

UPSTREAM: OpenRA.Mods.Common/Traits/Wanders.cs 实现部分
         The implementation half.

## src/mods/wands.hpp

UPSTREAM: OpenRA.Mods.Common/Traits/Wanders.cs @1f594e320 L21-118 全文
         (逐语义重写;第十二批随 G2 黄金对拍修出 —— 平民 7 人的
         SharedRandom 构造消耗缺失致帧 1 SyncHash 分歧)
         The whole of Wanders.cs L21-118 (verbatim-semantics rewrite;
         fixed into existence by the batch-12 G2 golden differential —
         the civilians' SharedRandom constructor draws were missing,
         diverging frame 1's SyncHash).

机制对照 / Mechanism mapping:
 - WandersInfo.Requires<IMoveInfo> 的装载期校验未设(C++ 侧全部宿主
   均挂 Mobile;空缺面与 Refinery 的 Requires 登记侧跳过同形)
   WandersInfo.Requires<IMoveInfo>'s load-time validation is not
   installed (every C++ host carries Mobile; the gap is the same shape
   as Refinery's registration-side Requires skip).
 - AvoidTerrainTypes 的 FrozenSet<string> → vector<string>
   (resource_layer 同形)
   AvoidTerrainTypes' FrozenSet<string> → vector<string> (the same
   shape as resource_layer's).

## src/mods/warheads.cpp

UPSTREAM: OpenRA.Mods.Common/Warheads/DamageWarhead.cs +
         SpreadDamageWarhead.cs 实现部分 | The implementation half.

## src/mods/warheads.hpp

UPSTREAM: OpenRA.Mods.Common/Warheads/DamageWarhead.cs @1f594e320 L20-94 +
         SpreadDamageWarhead.cs L20-143(全文逐语义重写;DebugVisualizations
         的 WarheadDebugOverlay 面随 Phase 6)
         The whole of DamageWarhead.cs L20-94 + SpreadDamageWarhead.cs
         L20-143 (verbatim-semantics rewrites; the DebugVisualizations/
         WarheadDebugOverlay faces land with Phase 6).

机制对照 / Mechanism mapping:
 - 上游 abstract DamageWarhead 继承链(Warhead 基类已在 sim/weapons.hpp)
   → C++ DamageWarhead 中间基类 + SpreadDamageWarhead 具体类;基字段
   (Warhead 七字段)由 sim::Warhead 承载,值袋解析经 CollectFields 的
   base 链扁平序
   Upstream's abstract DamageWarhead inheritance chain (the Warhead base
   already lives in sim/weapons.hpp) → the C++ DamageWarhead intermediate
   base + the SpreadDamageWarhead concrete class; the base fields (the
   Warhead seven) ride sim::Warhead, with the value-bag parse going
   through CollectFields' flattened base-chain order.
 - Versus 的 FrozenDictionary<string, int> → 插入序 vector<pair>(键查
   找线性;规模 = 装甲类型数)
   Versus's FrozenDictionary<string, int> → an insertion-ordered
   vector<pair> (linear key lookup; the scale = the armor-type count).
 - IRulesetLoaded<WeaponInfo>.RulesetLoaded 的校验 → 工厂解析时点
   (异常文本逐字;同 D 系时点偏离)
   The IRulesetLoaded<WeaponInfo>.RulesetLoaded validation → the
   factory-parse timing (exception texts verbatim; the same D-series
   timing deviation).

## src/mods/with_damage_overlay.cpp

UPSTREAM: OpenRA.Mods.Common/Traits/Render/WithDamageOverlay.cs @1f594e320
         L18-165

## src/mods/with_damage_overlay.hpp

UPSTREAM: OpenRA.Mods.Common/Traits/Render/WithDamageOverlay.cs @1f594e320
         L18-165 重损烟雾(start/loop/end 三段链 + 延迟触发)。
         The heavy-damage smoke (the start/loop/end chain + the delayed
         trigger).

## src/mods/with_death_animation.cpp

UPSTREAM: OpenRA.Mods.Common/Traits/Render/WithDeathAnimation.cs @1f594e320
         L18-127(statement-by-statement;机制对照见 with_death_animation.hpp)
         Statement-by-statement; the mechanism mapping lives in
         with_death_animation.hpp.

## src/mods/with_death_animation.hpp

UPSTREAM: OpenRA.Mods.Common/Traits/Render/WithDeathAnimation.cs @1f594e320
         L18-127 死亡/碾压序列(效果载体 = SpriteEffect;DeathTypes 首命中
         键 + SharedRandom 后缀择取)。
         The death/crushed sequences (the carrier = SpriteEffect; the
         first-hit DeathTypes key + the SharedRandom suffix pick).

## src/mods/with_decoration.cpp

UPSTREAM: OpenRA.Mods.Common/Traits/Render/WithDecorationBase.cs @1f594e320
         L18-146 + WithDecoration.cs L18-71

## src/mods/with_decoration.hpp

UPSTREAM: OpenRA.Mods.Common/Traits/Render/WithDecorationBase.cs @1f594e320
         L18-146 + WithDecoration.cs L18-71 装饰族基座:泛型基 → CRTP
         双参(DerivedT + InfoT);FrozenDictionary<BooleanExpression,…>
         → 插入序对(键序 = 首命中序)。
         The decoration-family base: the generic base → the two-parameter
         CRTP (DerivedT + InfoT); FrozenDictionary<BooleanExpression,…>
         → insertion-ordered pairs (the key order = the first-hit one).

## src/mods/with_decoration_pips.cpp

UPSTREAM: OpenRA.Mods.Common/Traits/Render/WithSpriteControlGroupDecoration.cs
         @1f594e320(statement-by-statement;机制对照见
         with_decoration_pips.hpp)
         Statement-by-statement; the mechanism mapping lives in
         with_decoration_pips.hpp.

## src/mods/with_decoration_pips.hpp

UPSTREAM: OpenRA.Mods.Common/Traits/Render/WithSpriteControlGroupDecoration.cs
         @1f594e320 L18-69 + WithResourceStoragePipsDecoration.cs L18-79 +
         WithStoresResourcesPipsDecoration.cs L18-101 pip/控制组装饰
         (AmmoPips/CargoPips 随宿主 trait 批)。
         The pip/control-group decorations (AmmoPips/CargoPips ride their
         host traits' batches).

## src/mods/with_infantry_body.cpp

UPSTREAM: OpenRA.Mods.Common/Traits/Render/WithInfantryBody.cs @1f594e320
         L21-225(实现部分;头注见 with_infantry_body.hpp)
         The implementation half of WithInfantryBody.cs L21-225 (the
         header note lives in with_infantry_body.hpp).

## src/mods/with_infantry_body.hpp

UPSTREAM: OpenRA.Mods.Common/Traits/Render/WithInfantryBody.cs @1f594e320
         L21-225 全文
         The whole of WithInfantryBody.cs L21-225.

机制对照 / Mechanism mapping:
 - Game.CosmeticRandom(PlayStandAnimation 的 stand 择一)→
   world.LocalRandom()(同 conditions 批的 LocalRandom 域形态 —— 非同步
   消耗不进 SyncHash;偏离面与 GrantConditionOnDamageState 同登记)
   Game.CosmeticRandom (PlayStandAnimation's stand pick) →
   world.LocalRandom() (the conditions batch's LocalRandom-domain shape —
   a non-synced consumption never entering SyncHash; the deviation rides
   GrantConditionOnDamageState's registry).
 - RenderPreviewSprites(IActorPreview 面)随 Phase 6 预览装配
   RenderPreviewSprites (the IActorPreview face) rides Phase 6's preview
   assembly.
 - AttackSequences 的 FrozenDictionary → 插入序 vector pair(遍历序 =
   键查找,无序依赖)
   AttackSequences' FrozenDictionary → an insertion-ordered pair vector
   (the traversal = key lookup, no order dependence).

## src/mods/with_make_animation.cpp

UPSTREAM: OpenRA.Mods.Common/Traits/Render/WithMakeAnimation.cs @1f594e320
         L20-203 + WithMakeOverlay.cs L17-64(实现部分)
         The implementation halves of WithMakeAnimation.cs L20-203 +
         WithMakeOverlay.cs L17-64.

## src/mods/with_make_animation.hpp

UPSTREAM: OpenRA.Mods.Common/Traits/Render/WithMakeAnimation.cs @1f594e320
         L20-203 全文 + WithMakeOverlay.cs L17-64 全文
         The whole of WithMakeAnimation.cs L20-203 + WithMakeOverlay.cs
         L17-64.

机制对照 / Mechanism mapping:
 - PlayCustomAnimation 的 onComplete 闭包族 → std::function(帧末任务闭包
   体逐字;token 的 grant/revoke 配对保真)
   PlayCustomAnimation's onComplete closures → std::function (the
   frame-end closure bodies verbatim; the token grant/revoke pairing
   kept).
 - FirstEnabledConditionalTraitOrDefault(Exts.cs)→ 首个未禁用元素的内联
   扫描
   FirstEnabledConditionalTraitOrDefault (Exts.cs) → the inline scan for
   the first enabled element.

## src/mods/with_parachute.cpp

UPSTREAM: OpenRA.Mods.Common/Traits/Render/WithParachute.cs @1f594e320

## src/mods/with_parachute.hpp

UPSTREAM: OpenRA.Mods.Common/Traits/Render/WithParachute.cs @1f594e320

## src/mods/with_sprite_body.cpp

UPSTREAM: OpenRA.Mods.Common/Traits/Render/WithSpriteBody.cs @1f594e320
         L21-151 + WithFacingSpriteBody.cs L19-39(实现部分)
         The implementation halves of WithSpriteBody.cs L21-151 +
         WithFacingSpriteBody.cs L19-39.

## src/mods/with_sprite_body.hpp

UPSTREAM: OpenRA.Mods.Common/Traits/Render/WithSpriteBody.cs @1f594e320
         L21-151 全文 + WithFacingSpriteBody.cs L19-39 全文
         The whole of WithSpriteBody.cs L21-151 + WithFacingSpriteBody.cs
         L19-39.

机制对照 / Mechanism mapping:
 - PausableConditionalTrait<WithSpriteBodyInfo> → CRTP 核 + IObservables
   (blocks_projectiles.hpp 同形)
   PausableConditionalTrait<WithSpriteBodyInfo> → the CRTP core +
   IObservesVariables (blocks_projectiles.hpp's shape).
 - PlayCustomAnimation 的 after 回调闭包 → std::function 成员链(Animation
   的 PlayThen after 面直接承载;闭包体逐字)
   PlayCustomAnimation's after-callback closure → the std::function
   (Animation's PlayThen-after face carries it; the body verbatim).
 - RenderPreviewSprites(IActorPreview 面)随 Phase 6 预览装配
   RenderPreviewSprites (the IActorPreview face) rides Phase 6's preview
   assembly.

## src/mods/with_sprite_turret.cpp

UPSTREAM: OpenRA.Mods.Common/Traits/Render/WithSpriteTurret.cs @1f594e320
         L21-144(实现部分;头注见 with_sprite_turret.hpp)
         The implementation half of WithSpriteTurret.cs L21-144 (the
         header note lives in with_sprite_turret.hpp).

## src/mods/with_sprite_turret.hpp

UPSTREAM: OpenRA.Mods.Common/Traits/Render/WithSpriteTurret.cs @1f594e320
         L21-144 全文
         The whole of WithSpriteTurret.cs L21-144.

机制对照 / Mechanism mapping:
 - RenderPreviewSprites(IActorPreview 面)随 Phase 6 预览装配
   RenderPreviewSprites (the IActorPreview face) rides Phase 6's preview
   assembly.
 - TurretOffset 的 recoil 常量折叠:WVec(new WDist(-recoilDist), Zero,
   Zero).Rotate(WorldOrientation) —— 运算逐字
   TurretOffset's recoil arithmetic kept verbatim (WVec(new
   WDist(-recoilDist), Zero, Zero).Rotate(WorldOrientation)).

## src/mods/world_exts.hpp

UPSTREAM: OpenRA.Mods.Common/WorldExtensions.cs @1f594e320 L~185-320
         (FindActorsOnLine/FindBlockingActorsOnLine/FindActorsOnCircle/
         MinimumPointLineProjection;逐语义重写)
         FindActorsOnLine/FindBlockingActorsOnLine/FindActorsOnCircle/
         MinimumPointLineProjection (verbatim-semantics rewrite).

## src/net/byte_io.hpp

UPSTREAM: OpenRA.Game/Network/Order.cs @1f594e320 L345-487(BinaryWriter/Reader 字节协议的 Order 序列化消费面)
         字节协议)—— 兼容层,行为对齐(MSDN 语义):
         - 整型/浮点:小端
         - string:7-bit 变长长度前缀(每字节低 7 位,高位继续)+ UTF-8 字节
         The .NET BinaryWriter/BinaryReader byte protocol — a
         behavior-aligned shim (MSDN semantics): little-endian scalars;
         strings as a 7-bit variable-length prefix + UTF-8 bytes.

## src/net/connection.cpp

UPSTREAM: OpenRA.Game/Network/Connection.cs @1f594e320 L73-96(EchoConnection
         .Receive)
         EchoConnection.Receive.

## src/net/connection.hpp

UPSTREAM: OpenRA.Game/Network/Connection.cs @1f594e320 L24-97(IConnection +
         EchoConnection 逐语义重写;NetworkConnection 随 Phase 7 网络批;
         IsReplay/ReplayTickCount = 上游 `is ReplayConnection` 类型判与
         TickCount 取值的虚面 —— World.IsReplay 与 OrderManager
         .SuggestedTimestep 的消费点,第十一批随 ReplayConnection 全文化)
         IConnection + EchoConnection, verbatim-semantics; Network
         Connection rides the Phase 7 network batch; IsReplay/
         ReplayTickCount are the virtual face of upstream's `is
         ReplayConnection` test + TickCount read (consumed by
         World.IsReplay and OrderManager.SuggestedTimestep, wired with
         batch 11's full ReplayConnection).

## src/net/connection_target.cpp

UPSTREAM: OpenRA.Game/Network/ConnectionTarget.cs @1f594e320 L19-62(实现;
         头文件同条目)
         The implementation; the header carries the same tag.

## src/net/connection_target.hpp

UPSTREAM: OpenRA.Game/Network/ConnectionTarget.cs @1f594e320 L19-62(逐语义
         重写;Dns.GetHostAddresses → getaddrinfo 全族解析,D204)
         Verbatim-semantics rewrite; Dns.GetHostAddresses → getaddrinfo
         all-family resolution, D204.

## src/net/fluent_message.cpp

UPSTREAM: OpenRA.Game/Network/FluentMessage.cs @1f594e320 L60-124
         (Serialize + LoadArguments 全文)
         (Serialize + the whole of LoadArguments.)

## src/net/fluent_message.hpp

UPSTREAM: OpenRA.Game/Network/FluentMessage.cs @1f594e320 L19-124(服务
         器 ↔ 客户端的通知消息线格式:Protocol/Key/Arguments 三段 +
         FluentArgument 的 Key/Value/Type;Serialize 的 ToLines +
         JoinWith("\n") 形态逐字;FromYaml = 客户端侧 FluentMessage(
         MiniYaml) 构造;上游位于 OpenRA.Game/Network —— C++ 随第十六
         批自 src/server 迁入 src/net 对齐上游层属)
         FluentMessage.cs L19-124 (the server→client notification wire
         format: the Protocol/Key/Arguments trio + FluentArgument's
         Key/Value/Type; Serialize's ToLines + JoinWith("\n") shape
         verbatim; Deserialize rides the Phase 6 Fluent batch).

## src/net/game_save.cpp

UPSTREAM: OpenRA.Game/Network/GameSave.cs @1f594e320 L21-333(见 game_save.hpp)。

## src/net/game_save.hpp

UPSTREAM: OpenRA.Game/Network/GameSave.cs @1f594e320 L21-333 全文
         (SlotClient 值面 + GameSave 的 orasav 读/写双态)。
         The whole of GameSave.cs L21-333 (the SlotClient value face +
         GameSave's orasav read/write states).

机制对照 / Mechanism mapping:
 - FieldLoader.Load<SlotClient>/FieldSaver.Save → 按名字段手写表(色 =
   Session 同一 hex TryParse/ToString 域)
   FieldLoader.Load<SlotClient>/FieldSaver.Save → the hand-written by-name
   table (the color = the same hex TryParse/ToString domain as Session).
 - ctor(path) 的 InvalidDataException → std::optional 返回(严禁异常协议;
   上游调用方 browser 以 try/catch 吞,等价面 = nullopt)
   The ctor(path)'s InvalidDataException → the std::optional return (the
   no-throw protocol; upstream's caller swallows via try/catch — the
   equivalent face is nullopt).
 - DispatchOrders(Connection,...) 的 conn.PlayerIndex → int 形参
   (net 层不依赖 server 的连接类;消费面 = 服务器存档运行时)
   DispatchOrders' conn.PlayerIndex → the int parameter (the net layer
   must not depend on the server's connection class; the consumer is the
   server's save runtime).
 - Dictionary<int, MiniYaml> TraitData → 插入序 vector(上游字典迭代 =
   插入序;ParseOrders 的包序保持)
   Dictionary<int, MiniYaml> TraitData → the insertion-order vector
   (upstream's dictionary iteration = insertion order; ParseOrders keeps
   the packet order).

## src/net/handshake.cpp

UPSTREAM: OpenRA.Game/Network/Handshake.cs @1f594e320 L17-90(字段装载/
         序列化全文;FieldLoader/FieldSaver → 按名手写表,与 Session 同形)
         Handshake.cs L17-90 (the whole load/serialize text; FieldLoader/
         FieldSaver → the by-name hand-written tables, the same shape as
         Session's).

## src/net/handshake.hpp

UPSTREAM: OpenRA.Game/Network/Handshake.cs @1f594e320 L17-90(握手请求/
         响应的 yaml 线格式:Handshake 节按名装载,Response 另挂 Client
         节 = FieldSaver.Save(Session.Client);Serialize 的显式字段序
         Mod/Version/Password/Fingerprint/AuthSignature/OrdersProtocol
         逐字)
         Handshake.cs L17-90 (the yaml wire format of the handshake
         request/response: the Handshake node loads by name, the Response
         additionally carries the Client node = FieldSaver.Save(
         Session.Client); Serialize's explicit field order Mod/Version/
         Password/Fingerprint/AuthSignature/OrdersProtocol verbatim).

## src/net/http_sync.cpp

UPSTREAM: OpenRA.Game/Support/HttpClientFactory.cs @1f594e320 +
OpenRA.Game/HttpExtension.cs L18-49 @1f594e320(实现半;契约半见头文件)
GET 的分块读复刻 ReadAsStreamWithProgress 的 8192 缓冲;Content-
Disposition 的 filename 解析 = .NET ContentDisposition.FileName 的
去引号参数值(参数名大小写不敏感;值两端的成对引号剥除)。
The implementation half (the contract half is in the header). The GET's
chunked read reproduces ReadAsStreamWithProgress's 8192 buffer; the
Content-Disposition filename parse = .NET ContentDisposition.FileName's
unquoted parameter value (a case-insensitive parameter name; a paired
quoting around the value is stripped).

## src/net/http_sync.hpp

UPSTREAM: OpenRA.Game/Support/HttpClientFactory.cs @1f594e320 +
OpenRA.Game/HttpExtension.cs L18-49 + System.Net.Http.HttpClient 的
GetAsync/PostAsync 语义面 @1f594e320(消费点直调的同步承载)
上游 HttpClient 的同步直调形态(boost::beast + boost::url;../cpp26.md
§二 Boost 补位 —— HTTP 为 std 未落地面):HttpGetSync = GetAsync +
HttpCompletionOption.ResponseHeadersRead + ReadAsStreamWithProgress(
Content-Length>0 时逐块回调,结尾恒一次 (total, totalRead, 100));POST =
PostAsync + ReadAsStringAsync。两形均为纯同步 —— 调用方(引擎侧)在专属
线程调用以保持上游 await 的非阻塞面。URI 非法/https(纯同步 beast 无
TLS 后端)/DNS/连接/读写任一失败 = 传输层失败;状态码不参与成败判断
(上游无 EnsureSuccessStatusCode 的面)。
The synchronous direct-call form of the upstream HttpClient (boost::beast
+ boost::url; ../cpp26.md §2's Boost fill-in — HTTP is a face std has
not landed): HttpGetSync = GetAsync +
HttpCompletionOption.ResponseHeadersRead + ReadAsStreamWithProgress
(per-chunk callbacks when Content-Length > 0, plus one final
(total, totalRead, 100)); the POST = PostAsync + ReadAsStringAsync. Both
are purely synchronous — the callers (engine-side) invoke on a dedicated
thread to keep upstream's non-blocking await face. An invalid URI / https
(plain synchronous beast carries no TLS backend) / any DNS/connect/
read/write failure = a transport failure; the status code takes no part
in success (upstream has no EnsureSuccessStatusCode on this face).

## src/net/launch_arguments.cpp

UPSTREAM: OpenRA.Game/Support/LaunchArguments.cs @1f594e320 L34-66

## src/net/launch_arguments.hpp

UPSTREAM: OpenRA.Game/Support/LaunchArguments.cs @1f594e320 L17-67

## src/net/nat.cpp

UPSTREAM: OpenRA.Game/Network/Nat.cs @1f594e320(实现;头注见 nat.hpp)
         Implementation of Nat.cs; the header notes live in nat.hpp.

## src/net/nat.hpp

UPSTREAM: OpenRA.Game/Network/Nat.cs @1f594e320 L18-101(全文;
         Mono.Nat 的 UPnP/NAT-PMP 发现链 → 极小承载:发现从不启动或
         从无设备两态 + 端口映射恒 false —— Mono.Nat 依赖的 C++ 侧
         等价域登记于 docs/COVERAGE.md)
         The whole of Nat.cs L18-101 (the Mono.Nat UPnP/NAT-PMP discovery
         chain becomes a minimal carrier: the never-started/never-found
         pair of states + port mappings that always fail — the C++-side
         equivalence domain of the Mono.Nat dependency is registered in
         docs/COVERAGE.md).

## src/net/network_connection.cpp

UPSTREAM: OpenRA.Game/Network/Connection.cs @1f594e320 L99-386(实现;头
         文件同条目)
         The implementation; the header carries the same tag.

## src/net/network_connection.hpp

UPSTREAM: OpenRA.Game/Network/Connection.cs @1f594e320 L99-386(Network
         Connection 全文:连接竞赛/读线程/发送链/Ack·Ping·TickScale 分发;
         线程与并发原语的形态偏离 D203~D206,见 docs/COVERAGE.md)+
         OpenRA.Game/Server/ProtocolVersion.cs L75(Handshake = 7)
         The whole of NetworkConnection (L99-386): the connect race/
         reader thread/send chain/Ack·Ping·TickScale dispatch; the
         thread-and-concurrency shape deviations D203-D206, see
         docs/COVERAGE.md + ProtocolVersion.cs L75 (Handshake = 7).

## src/net/order.cpp

UPSTREAM: OpenRA.Game/Network/Order.cs @1f594e320(实现部分:逐字节序列化)
         The implementation half of Order.cs (byte-exact serialization).

## src/net/order.hpp

UPSTREAM: OpenRA.Game/Network/Order.cs @1f594e320 L18-495(逐字节序列化协议
         逐语义重写;BinaryWriter/Reader → byte_io 兼容层)
         Verbatim-bytes rewrite of the serialization protocol via the
         byte_io shim.

机制对照 / Mechanism mapping:
 - C# null vs 空容器的 flags 位差(Subject/TargetString/ExtraActors/
   Grouped/ExtraData!=0/ExtraLocation!=Zero)→ std::optional / 显式判定,
   位掩码值与写入序逐字节一致(Order.cs L378-479)
   The C# null-vs-empty flags distinction → std::optional / explicit
   checks; bit values and write order are byte-identical (Order.cs
   L378-479).
 - Target 序列化:Actor(actorID+generation)/Terrain(cell 或 pos+位置数,
   单位置短路 -1)/FrozenActor(Viewer.PlayerActor.ActorID+ID);反序列化经
   当前世界解析(GetActorById;失败 → Invalid/null 语义保留)
   Target serialization: Actor (actorID+generation) / Terrain (cell, or
   pos+count with the single-position -1 shortcut) / FrozenActor
   (Viewer.PlayerActor.ActorID+ID); deserialization resolves through the
   live world (GetActorById; failures degrade to the upstream
   Invalid/null semantics).
 - Deserialize 的异常吞并(L235-245:catch → null)→ C++ try/catch(std::
   exception)等价;TextNotificationsManager 通知 Phase 6 接线
   Deserialize's exception swallowing (L235-245: catch → null) → the
   equivalent C++ try/catch(std::exception); TextNotificationsManager
   notifications wire up in Phase 6.
 - FromCell 分支的 CenterOfSubCell(Map 依赖 Phase 5):经 World 注入的
   子格中心换算器,未注入时按 square 网格公式(x*1024+512)桩换算 ——
   COVERAGE 登记
   The FromCell branch's CenterOfSubCell (Map dependency, Phase 5) goes
   through a World-injected sub-cell center converter, falling back to
   the square-grid formula (x*1024+512) — registered in COVERAGE.

## src/net/order_io.cpp

UPSTREAM: OpenRA.Game/Network/OrderIO.cs @1f594e320(实现部分)
         The implementation half of OrderIO.cs.

## src/net/order_io.hpp

UPSTREAM: OpenRA.Game/Network/OrderIO.cs @1f594e320 L18-212(逐字节重写)
         Byte-exact rewrite.

机制对照 / Mechanism mapping:
 - OrderPacket:MemoryStream 载荷 → vector<uint8_t>;GetOrders 延迟解析
   (依赖世界状态)→ 惰性反序列化迭代;Serialize(frame) = frame LE + 载荷
 - OrderPacket: the MemoryStream payload → vector<uint8_t>; the
   deferred GetOrders parsing (world-state dependent) → lazy
   deserialization; Serialize(frame) = frame LE + payload.
 - TryParse* 家族:长度/首字节魔数判定 + FromClient==0(仅服务器)门槛,
   全部逐字段对齐
   The TryParse* family: length/first-byte magic checks plus the
   FromClient==0 (server-only) gate, aligned field by field.

## src/net/order_manager.cpp

UPSTREAM: OpenRA.Game/Network/OrderManager.cs @1f594e320(实现部分:锁步核心)
         The implementation half of OrderManager.cs (the lockstep core).

## src/net/order_manager.hpp

UPSTREAM: OpenRA.Game/Network/OrderManager.cs @1f594e320 L22-334(逐语义重写;
         SyncReport/TextNotificationsManager/Game 静态面以桩/注入承载 ——
         Phase 5/6/7 接线)
         Verbatim-semantics rewrite; the SyncReport/TextNotifications
         Manager/Game statics ride stubs/injections — wired in Phase
         5/6/7.

机制对照 / Mechanism mapping:
 - pendingOrders: Dictionary<int, Queue<(int, OrderPacket)>> →
   std::map<int, std::deque<FramePacket>>(遍历序 = 客户端 ID 升序,
   上游 Dictionary 枚举序差异不影响:ProcessOrders 逐客户端独立处理)
   pendingOrders → std::map<int, std::deque<FramePacket>> (iteration
   order = ascending client id; upstream's Dictionary enumeration order
   does not matter: ProcessOrders handles each client independently).
 - Order 队列化:C# Order 对象 → OrderPacket 字节(与上游同策略:本地与
   远端一致化,见 OrderPacket 注)
 - SyncReport:desync 诊断面(Phase 7);generateSyncReport 恒 false 路径
   保真(EnableSyncReports 默认 false)
 - Game.RunTime:时钟注入(ClockFn;默认恒 0 —— 单机锁步不依赖墙钟)

## src/net/replay_recorder.cpp

UPSTREAM: OpenRA.Game/Network/ReplayRecorder.cs @1f594e320 L19-118 +
          OpenRA.Game/FileFormats/ReplayMetadata.cs L17-108 +
          OpenRA.Game/Network/ReplayConnection.cs L19-135(实现部分)
          The implementation halves (the header note lives in
          replay_recorder.hpp).

## src/net/replay_recorder.hpp

UPSTREAM: OpenRA.Game/Network/ReplayRecorder.cs @1f594e320 L19-118 全文 +
          OpenRA.Game/FileFormats/ReplayMetadata.cs L17-108 全文 +
          OpenRA.Game/Network/ReplayConnection.cs L19-135 全文(第十一批:
          回放驱动世界 = OrderManager 装配;orderLatency/FinalGameTick 为
          装配注入面)
          The whole of ReplayRecorder.cs L19-118 + ReplayMetadata.cs
          L17-108 + the whole of ReplayConnection.cs L19-135 (batch 11).

机制对照 / Mechanism mapping:
 - GameInformation 的完整序列化(ReplayMetadata 载荷)随 Phase 7 大厅
   协议 —— 承载面 = 版本/标记/长度前缀协议全保真,载荷串以注入面出入
   GameInformation's full serialization (ReplayMetadata's payload) rides
   Phase 7's lobby protocol — the carrier keeps the marker/version/
   length-prefix protocol fully faithful, the payload string passes
   through an injection face.
 - preStartBuffer(MemoryStream)→ vector<uint8_t> 追加缓冲(writer 的
   clientID/length/data 三段写 = 同一序)
   The preStartBuffer (a MemoryStream) → an append buffer (the writer's
   clientID/length/data three-part writes keep the same order).
 - Platform.SupportDir/ModData.Manifest 的目录解析 → chooseFilename
   回调 + 目录参数(引擎装配面注入)
   The Platform.SupportDir/ModData.Manifest directory resolution → the
   chooseFilename callback + a directory parameter (injected by the
   engine assembly).

## src/net/sanitize.cpp

UPSTREAM: OpenRA.Game/Settings.cs @1f594e320 L500-538(实现全文;
         第十六批自 src/server/server_settings.cpp 迁入)
         The whole implementation (migrated from src/server/
         server_settings.cpp with batch 16).

## src/net/sanitize.hpp

UPSTREAM: OpenRA.Game/Settings.cs @1f594e320 L500-538(SanitizedName/
         SanitizedPlayerName 全文;上游 Settings.cs 静态 —— C++ 依
         lib 层属(client/server 双消费)置于 net;SanitizedServerName
         仍随 server_settings)
         The whole of Settings.cs L500-538 (SanitizedName/
         SanitizedPlayerName; upstream Settings.cs statics — placed in
         net per the C++ lib layering (both the client and the server
         consume them); SanitizedServerName stays with server_settings).

## src/net/session.hpp

UPSTREAM: OpenRA.Game/Network/Session.cs @1f594e320 L1-286(锁步/大厅面;
         反序列化随第十一批回放驱动链落地 —— FieldLoader.Load 的
         Client/Slot/Global/LobbyOptionState 按名装载手写面,序列化面
         对称承载)
         The lockstep/lobby face of Session.cs; the deserialization lands
         with the batch-11 replay-driver chain (a hand-written by-name
         loading face of FieldLoader.Load for Client/Slot/Global/
         LobbyOptionState), with the symmetric serialization face.

机制对照 / Mechanism mapping:
 - OrderManager/UnitOrders 消费面:GlobalSettings(RandomSeed/
   NetFrameInterval/OptionOrDefault/EnableSyncReports)、Clients(Index/
   IsBot/State/Team)、ClientWithIndex/NonBotClients
   The OrderManager/UnitOrders consumption surface.
 - 玩家创建链消费面(Phase 5 第二批):Slots(PlayerReference/LockFaction)、
   ClientInSlot、Client 的 Bot(类型串)/Handicap/Fingerprint/Color/
   IsAdmin 面(C# nullable 串 → 空串承载)
   The player-creation-chain consumption faces (Phase 5 batch 2).
 - Deserialize 的任何字段/结构失败统一抛 YamlException("Session
   deserialized invalid MiniYaml:\n…")(上游两 catch 包装的等价文本)
   Any field/structure failure in Deserialize throws the unified
   YamlException ("Session deserialized invalid MiniYaml:\n…" — the
   equivalent text of upstream's two catch wrappers).
 - Serialize 的 FieldSaver.Save 反射 → 按名字段的双向手写表
   Serialize's reflected FieldSaver.Save becomes the by-name hand-written
   two-way tables.

## src/net/sync_report.cpp

UPSTREAM: OpenRA.Game/Network/SyncReport.cs @1f594e320 L23-344(实现部分;
         头注见 sync_report.hpp)
         The implementation half of SyncReport.cs L23-344 (the header
         note lives in sync_report.hpp).

## src/net/sync_report.hpp

UPSTREAM: OpenRA.Game/Network/SyncReport.cs @1f594e320 L23-344(起步承载
          面:环形 7 报告 + UpdateSyncReport 的哈希/帧/随机态记账 +
          DumpSyncReport 的文本面)
          The starter face of SyncReport.cs L23-344 (the 7-report ring +
          UpdateSyncReport's hash/frame/random bookkeeping + the
          DumpSyncReport text face).

机制对照 / Mechanism mapping:
 - TypeInfo 的 Reflection.Emit 成员值快照(DumpSyncTrait 的逐成员
   values)→ 可选 dump 注册表(手写 trait 按批注册;未注册类型 = 名字
   表空值 —— 起步偏离,D30 扩展)
   TypeInfo's Reflection.Emit member-value snapshots (DumpSyncTrait's
   per-member values) → an optional dump registry (hand-written traits
   register batch by batch; unregistered types carry the name table with
   empty values — the starter deviation, extending D30).
 - Log.AddChannel("sync")/Log.Write → 调用方注入的 sink(std::print 的
   后端随日志设施批)
   Log.AddChannel("sync")/Log.Write → the caller-injected sink (the
   std::print backend rides the logging-facility batch).
 - Values 的内联四槽优化 → vector(报告生成是罕见路径,分配无关紧要)
   Values' inline-four-slot optimization → a vector (report generation
   is a rare path; the allocation is immaterial).

## src/net/text_notifications.cpp

UPSTREAM: OpenRA.Game/TextNotificationsManager.cs @1f594e320 L26-117
         (实现全文)
         (the whole implementation).

## src/net/text_notifications.hpp

UPSTREAM: OpenRA.Game/TextNotification.cs @1f594e320 L19-54 +
         OpenRA.Game/TextNotificationsManager.cs L19-117(全文;C#
         static → 进程级单例;FluentProvider.GetMessage/
         ChromeMetrics["SystemMessageLabel"]/Ui.Send/Game.RunTime/
         Game.Settings.Game.TextNotificationPoolFilters 五静态面以注入
         + 上游字面量缺省承载 —— 见各 setter;线程面同上游:全部调用
         发生于主线程/OM 驱动线程,无内部锁)
         The whole of TextNotification.cs L19-54 +
         TextNotificationsManager.cs L19-117 (the C# static becomes a
         process-wide singleton; the five static faces —
         FluentProvider.GetMessage / ChromeMetrics["SystemMessageLabel"]
         / Ui.Send / Game.RunTime / Game.Settings.Game.
         TextNotificationPoolFilters — ride injections with upstream's
         literal defaults, see each setter; threading as upstream: every
         call lands on the main/OM-driving thread, no internal locks).

机制对照 / Mechanism mapping:
 - FluentProvider.GetMessage(key, args)(Phase 6)→ 注入;缺省 = key 回落
   (第十五批同登记)
   FluentProvider.GetMessage(key, args) (Phase 6) → injected; the
   default falls back to the key (registered the same as batch 15).
 - Ui.Send(notification)(Mediator 直连,Phase 6)→ sink 注入,缺省
   no-op
   Ui.Send(notification) (the Mediator direct call, Phase 6) → a sink
   injection defaulting to no-op.
 - DateTime.Now → system_clock::now()(UI 展示面专用,不入同步面)
   DateTime.Now → system_clock::now() (the UI display face only, never
   a synced face).

## src/net/tick_time.hpp

UPSTREAM: OpenRA.Game/Network/TickTime.cs @1f594e320 L16-60(逐语义重写;
         Game.TimestepJankThreshold 常量随值内联 = 250,Game.RunTime 时钟
         经回调注入 —— Game 静态面 Phase 5)
         Verbatim-semantics rewrite; the Game.TimestepJankThreshold
         constant inlined as 250 and the Game.RunTime clock injected via
         callback — the Game statics land in Phase 5.

## src/net/unit_orders.cpp

UPSTREAM: OpenRA.Game/Network/UnitOrders.cs @1f594e320 L48-431(第十六批
         客户端接线:ProcessOrder 全分支实现;上游 Game/Settings/
         ModData/MapCache/ExternalMods 静态面以注入族承载,见
         unit_orders.hpp;ExternalMods 注册表恒空 —— 跨进程 mod 切换
         面(ConnectionFailedLogic)随 Phase 6)
         The batch-16 client wiring: ProcessOrder's every branch; the
         upstream Game/Settings/ModData/MapCache/ExternalMods statics
         ride the injection family, see unit_orders.hpp; the
         ExternalMods registry stays empty — the cross-process mod
         switch (ConnectionFailedLogic) rides Phase 6.

## src/net/unit_orders.hpp

UPSTREAM: OpenRA.Game/Network/UnitOrders.cs @1f594e320 L48-431(第十六批
         客户端接线:ProcessOrder 全分支 —— Message/FluentMessage/
         DisableChatEntry/StartKickVote/EndKickVote/Chat/StartGame/
         SaveTraitData/GameSaved/PauseGame/HandshakeRequest/ServerError/
         AuthenticationError/SyncInfo/SyncLobbyClients/SyncLobbySlots/
         SyncLobbyGlobalSettings/SyncConnectionQuality/SyncMapPool/
         GenerateMap + default→ResolveOrder;TextNotificationsManager 全
         承载;Game/Settings/ModData/MapCache 的静态直调面以注入族承载)
         The batch-16 client wiring: ProcessOrder's every branch —
         Message/FluentMessage/DisableChatEntry/StartKickVote/EndKickVote/
         Chat/StartGame/SaveTraitData/GameSaved/PauseGame/HandshakeRequest/
         ServerError/AuthenticationError/SyncInfo/SyncLobbyClients/
         SyncLobbySlots/SyncLobbyGlobalSettings/SyncConnectionQuality/
         SyncMapPool/GenerateMap + default→ResolveOrder; the whole
         TextNotificationsManager carriage; the Game/Settings/ModData/
         MapCache static calls ride the injection family.

## src/platform/al_loader.cpp

UPSTREAM: OpenRA.Platforms.Default/OpenAlSoundEngine.cs @1f594e320(加载面 =
OpenRA-OpenAL-CS 的 DllImport 表;此处为其 C++ 直连等价物)
加载器实现:Windows LoadLibraryA + GetProcAddress 表驱动(gl_loader 同款;
DLL 句柄进程级常驻,与上下文生命周期解耦 —— OpenAL 规范允许 alcShutdown 后
复用函数指针,本项目不调用 alcShutdown)。
Loader implementation: table-driven Windows LoadLibraryA + GetProcAddress
(the same arrangement as gl_loader; the DLL handle stays process-lifetime,
decoupled from the context lifetime — the OpenAL spec allows reusing the
function pointers after context destruction, and we never call alcShutdown).

## src/platform/al_loader.hpp

UPSTREAM: OpenRA.Platforms.Default/OpenAlSoundEngine.cs @1f594e320 L20-669(经
OpenRA-OpenAL-CS 的 AL10/ALC10/ALC11/EFX 调用面)
OPT-A6 同形态(docs/OPTIMIZATION_TRACKER.md):函数指针直连(LoadLibraryA +
GetProcAddress 一次加载),上游 OpenRA-OpenAL-CS 的 P/Invoke 包装层消失。
常量与类型直接取自 third_party/OpenAL/include 的官方头(al.h/alc.h/efx.h),
不再手抄数值(与 GL 层的 gl_types 对照形态不同 —— OpenAL 有官方头可用)。
Function pointers loaded once via LoadLibraryA + GetProcAddress (the
OPT-A6 shape, docs/OPTIMIZATION_TRACKER.md); the upstream
OpenRA-OpenAL-CS P/Invoke wrapper disappears. Constants and types come
straight from the official headers in third_party/OpenAL/include
(al.h/alc.h/efx.h) — no hand-copied values (unlike the GL layer's
gl_types cross-check, OpenAL ships usable official headers).

## src/platform/dummy_sound_engine.hpp

UPSTREAM: OpenRA.Platforms.Default/DummySoundEngine.cs @1f594e320 L16-105(全文:
DummySoundEngine/NullSoundSource/NullSound;DefaultPlatform.CreateSound 的
OpenAL 初始化失败回落目标)
空实现逐语义移植(NullSound.Complete 恒假、Play2DStream 返回 null 照抄)。
The whole of DummySoundEngine.cs L16-105 (DummySoundEngine/NullSoundSource/
NullSound; the fallback target of DefaultPlatform.CreateSound when OpenAL
initialization fails). The no-op semantics ported verbatim (NullSound's
Complete stays false; Play2DStream returns null).

## src/platform/freetype_font.cpp

UPSTREAM: OpenRA.Platforms.Default/FreeTypeFont.cs @1f594e320 L20-143(实现)
OPT-B2:真结构体直取(third_party/FreeType/include 官方 2.14.1 头);入口经
LoadLibraryA 运行时直连。进程级 FT_Library 一次初始化(上游 static
library 字段的等价物)。

ABI 校正(随包 freetype6.dll 的实证布局):上游 FreeTypeFont.cs L27-36 的
手算偏移(glyph@152/metrics@48/bitmap@152/bitmap_left@192/bitmap_top@196,
64 位分支)与 LLP64 头(long=4,glyph@120)不符 —— 该 DLL 按 long=8 的
LP64 字长编译。真结构体路线下,以 `#define long long long` 宏窗把官方头
的 long 提升为 long long,布局即与 DLL 一致(等价于用与 DLL 同字长的编
译器编这份头),并以 static_assert 锁死上游六个偏移 + FT_Pos==8 —— 偏移
漂移在编译期即炸,上游 HACK 常量由此从"运行期魔法数"变为"编译期契约"。
系统头(stddef/limits/string/stdio/stdlib/setjmp/stdarg)先于宏窗经典引入
(include guard 已置),宏窗内只剩 FreeType 自有头。
OPT-B2: fields read straight through the real structs (the official 2.14.1
headers of third_party/FreeType/include); the entries connect at runtime
via LoadLibraryA. The process-wide FT_Library initializes once (the
equivalent of upstream's static library field).

ABI correction (the bundled freetype6.dll's empirically-observed layout):
the hand-computed offsets of upstream FreeTypeFont.cs L27-36 (glyph@152/
metrics@48/bitmap@152/bitmap_left@192/bitmap_top@196, the 64-bit branch)
do not match the LLP64 headers (long = 4, glyph@120) — the DLL was built
with LP64-sized longs (8 bytes). On the real-struct route, a
`#define long long long` macro window promotes the official headers' long
to long long, aligning the layout with the DLL (equivalent to compiling
these headers with the DLL's own long size); static_asserts then pin the
six upstream offsets plus FT_Pos == 8 — any layout drift explodes at
compile time, turning the upstream HACK constants from runtime magic
numbers into a compile-time contract. The system headers (stddef/limits/
string/stdio/stdlib/setjmp/stdarg) are classically included before the
macro window (their include guards are set), so only FreeType's own
headers are seen inside it.

## src/platform/freetype_font.hpp

UPSTREAM: OpenRA.Game/Graphics/PlatformInterfaces.cs @1f594e320 L192-203(IFont/
FontGlyph)+ OpenRA.Platforms.Default/FreeTypeFont.cs @1f594e320 L20-143(全文)
OPT-B2(docs/OPTIMIZATION_TRACKER.md):上游 FreeTypeFont.cs L27-36 的手算
结构体偏移 HACK(FaceRecGlyphOffset 152/GlyphSlotMetricsOffset 48/…)全部
删除 —— 直接 #include <ft2build.h> 用真结构体(FT_FaceRec/FT_GlyphSlotRec/
FT_Bitmap)取 face->glyph->metrics.width 等字段;FreeType 2.14.1 官方头
(third_party/FreeType/include)与运行时 freetype6.dll 同版本。
五个入口(FT_Init_FreeType/FT_New_Memory_Face/FT_Done_Face/
FT_Set_Pixel_Sizes/FT_Load_Char)经 LoadLibraryA("freetype6.dll") 运行时
直连(al_loader 同形态);字形位图行复制循环逐语句照抄(尺寸取
metrics 而非 bitmap 的上游怪癖保留 —— pitch < width 时两者可差)。
形态映射(D86,docs/COVERAGE.md):字节数组 GCHandle pin → 自持 vector
(FT_New_Memory_Face 生命期语义等价);char(charCode 为 UTF-16 码单元,
≤ 0xFFFF)→ std::uint32_t 码点参数(FT_Load_Char 的原生参数域)。
OPT-B2 (docs/OPTIMIZATION_TRACKER.md): the hand-computed struct-offset HACK
of FreeTypeFont.cs L27-36 (FaceRecGlyphOffset 152/GlyphSlotMetricsOffset
48/…) is deleted — real structs (FT_FaceRec/FT_GlyphSlotRec/FT_Bitmap) come
straight from #include <ft2build.h>, reading face->glyph->metrics.width and
friends; the official FreeType 2.14.1 headers
(third_party/FreeType/include) match the freetype6.dll runtime version.
The five entry points (FT_Init_FreeType/FT_New_Memory_Face/FT_Done_Face/
FT_Set_Pixel_Sizes/FT_Load_Char) connect at runtime via
LoadLibraryA("freetype6.dll") (the al_loader shape); the glyph bitmap row
copy loop is ported statement by statement (the upstream quirk of sizing by
metrics rather than the bitmap kept — the two can differ when pitch <
width). Shape mapping (D86, docs/COVERAGE.md): the pinned byte array
becomes an owned vector (the FT_New_Memory_Face lifetime semantics are
equivalent); char (a UTF-16 code unit, ≤ 0xFFFF) becomes a std::uint32_t
code-point parameter (FT_Load_Char's native domain).

## src/platform/gl_loader.cpp

UPSTREAM: OpenRA.Platforms.Default/OpenGL.cs @1f594e320 L506-669(Bind<T> 加载段)
表驱动单次加载;KHR_debug 双候选探测(无后缀 core 4.3 / KHR / ARB 后缀),
对应上游 OpenGL.cs L550-562 的后缀拼接探测。
Table-driven one-shot loading; KHR_debug probes two candidates (the
suffixless core-4.3 form and the KHR/ARB-suffixed forms), mirroring the
suffix concatenation of upstream OpenGL.cs L550-562.

## src/platform/gl_loader.hpp

UPSTREAM: OpenRA.Platforms.Default/OpenGL.cs @1f594e320 L240-669(801 行手写绑定段)
OPT-A6(docs/OPTIMIZATION_TRACKER.md):函数指针直连(经 SDL_GL_GetProcAddress 一次加载),
无委托双重装箱、无每次调用的 CheckGLError 轮询 —— Debug 构建注册 KHR_debug 回调,
Release 构建零错误检查。KHR_debug 为可选扩展(缺失不致命)。
Function pointers loaded once via SDL_GL_GetProcAddress — no double-boxed
delegates and no per-call CheckGLError polling (OPT-A6,
docs/OPTIMIZATION_TRACKER.md): Debug builds register a KHR_debug callback;
Release builds do no error checking at all. KHR_debug is optional (a missing
extension is not fatal).

## src/platform/gl_types.hpp

UPSTREAM: OpenRA.Platforms.Default/OpenGL.cs @1f594e320 L12-238(类型/常量手抄段)
GL 3.2 Core + GLES3 共用子集的类型与常量(上游 103 个常量全量收录,值与拼写逐一对照)。
OPT-A6(docs/OPTIMIZATION_TRACKER.md):不引入 glGetError 轮询 —— 错误检查由
KHR_debug 回调承担(Debug 构建),Release 构建零错误检查调用;此处仅定义类型与常量。
GL 3.2 Core + GLES3 shared-subset types and constants (all 103 upstream
constants, values and spellings cross-checked one by one).
OPT-A6 (docs/OPTIMIZATION_TRACKER.md): no glGetError polling — error
checking is carried by the KHR_debug callback (Debug builds) and is absent
in Release builds; this header defines types and constants only.

## src/platform/ifont.hpp

UPSTREAM: OpenRA.Game/Graphics/PlatformInterfaces.cs @1f594e320 L192-203
(FontGlyph + IFont;Dispose 随 C++ 析构)。FreeTypeFont(freetype_font.hpp)
实现该抽象;SpriteFont 经此消费(上游 Platform.CreateFont 的返回型)。
FontGlyph 自 freetype_font.hpp 上移至此(接口与其载荷同域;避免相互包含)。
FontGlyph + IFont (PlatformInterfaces.cs L192-203; Dispose arrives as the
C++ destructor). FreeTypeFont (freetype_font.hpp) implements the
abstraction; SpriteFont consumes it (the return type of upstream's
Platform.CreateFont). FontGlyph moves up here from freetype_font.hpp (the
interface and its payload share a home; avoids a mutual include).

## src/platform/keycode.hpp

UPSTREAM: OpenRA.Game/Input/Keycode.cs @1f594e320 L24-257(枚举体逐值照搬)
上游注释:"Duplicated from SDL 2.0.1, with the addition of MOUSE4 and
MOUSE5" —— 值域与 SDL_Keycode 完全一致(platform_test 以 SDL 头常量对照
断言)。Fluent 键名表(KeycodeExts.KeycodeFluentKeys)属 Phase 6 本地化,
随 Fluent 落地。KeycodeExts 部分未移植。
The enum body is reproduced value-for-value from upstream (whose own
comment reads "Duplicated from SDL 2.0.1, with the addition of MOUSE4 and
MOUSE5") — the values coincide with SDL_Keycode (platform_test asserts
against the SDL header constants). The fluent key-name table
(KeycodeExts.KeycodeFluentKeys) belongs to Phase 6 localization and lands
with Fluent. The KeycodeExts part is not ported.

## src/platform/openal_sound_engine.cpp

UPSTREAM: OpenRA.Platforms.Default/OpenAlSoundEngine.cs @1f594e320 L20-669(实现)
逐语句移植;浮点表达式(衰减公式、lengthInSecs)的运算序与字面量逐字照抄。
上游 Log.Write("sound",…) 形态偏离为 stderr 前缀打印(日志不入任何对拍面)。
Statement-by-statement port; the float expressions (the attenuation
formula, lengthInSecs) keep their evaluation order and literals verbatim.
Upstream's Log.Write("sound", …) deviates as prefixed stderr prints (the
logs never enter any differential surface).

## src/platform/openal_sound_engine.hpp

UPSTREAM: OpenRA.Platforms.Default/OpenAlSoundEngine.cs @1f594e320 L20-669(全文:
OpenAlSoundEngine/OpenAlSoundSource/OpenAlSound/OpenAlAsyncLoadSound)
形态映射(偏离登记 D83~D85,docs/COVERAGE.md):
 - OpenRA-OpenAL-CS 包装层消失,al_loader 直连(OPT-A6 形态);
 - sourcePool:Dictionary<uint,PoolSlot> → vector<PoolSlot>(保插入序 =
   C# Dictionary 无删除时的枚举序;alGenSources 名非单调时行为等价,槽等价);
 - Game.LocalTick → SetLocalTick 注入面(Phase 5/6 的 Game 接线点,上游为
   静态全局读取);
 - Play2D/Play2DStream 返回 unique_ptr<ISound>(所有权给调用方;对象析构经
   回指针把槽内裸指针清空 —— 上游 GC 生命期的等价物);流式后台任务
   Task.Run/Task.Delay → jthread + condition_variable(PORTING_PLAN §4.8);
 - 字节参数 → span 直传(README 有意优化表)。
纯逻辑提取(可无设备单测):MakeAlFormat / ParseAlDeviceList / EvaluatePlayGate。
[UPSTREAM continued] the whole of OpenAlSoundEngine.cs L20-669
(OpenAlSoundEngine/OpenAlSoundSource/OpenAlSound/OpenAlAsyncLoadSound).
Shape mapping (deviations D83–D85, docs/COVERAGE.md):
 - the OpenRA-OpenAL-CS wrapper disappears in favor of the al_loader direct
   connection (the OPT-A6 shape);
 - sourcePool: Dictionary<uint,PoolSlot> becomes vector<PoolSlot> (insertion
   order preserved = the enumeration order of a removal-free C# Dictionary;
   behavior equivalent when alGenSources names are non-monotonic — slots are
   interchangeable);
 - Game.LocalTick becomes the SetLocalTick injection point (wired by Game in
   Phase 5/6; upstream reads a static global);
 - Play2D/Play2DStream return unique_ptr<ISound> (ownership to the caller;
   the object's destructor clears its slot's raw pointer through a back-
   pointer — the equivalent of upstream's GC lifetime); the streaming
   background task's Task.Run/Task.Delay become a jthread + condition
   variable (PORTING_PLAN §4.8);
 - byte parameters pass as spans (the README deliberate-optimization table).
Pure-logic extractions (unit-testable without a device): MakeAlFormat /
ParseAlDeviceList / EvaluatePlayGate.

## src/platform/sdl2_hardware_cursor.cpp

UPSTREAM: OpenRA.Platforms.Default/Sdl2HardwareCursor.cs @1f594e320 L20-73(实现)
+ Sdl2PlatformWindow.cs L364-377(DoublePixelData 实现)
Implementation of the hardware cursor; the pixel masks
(0x00FF0000/0x0000FF00/0x000000FF/0xFF000000) and the 3-retry loop are
verbatim upstream.

## src/platform/sdl2_hardware_cursor.hpp

UPSTREAM: OpenRA.Platforms.Default/Sdl2HardwareCursor.cs @1f594e320 L20-73(全文)
+ Sdl2PlatformWindow.cs @1f594e320 L364-377(DoublePixelData)、L379-405
(CreateHardwareCursor)、L412-421(SetHardwareCursor)
形态映射(D87,docs/COVERAGE.md):
 - IHardwareCursor 接口面由具体类型承担(单平台;上游接口为跨实现门面);
 - Marshal.PtrToStructure<SDL_Surface> + Marshal.Copy → 真结构体字段直写
   (pixels 指针 memcpy,语义 = 上游的"写表面像素区");
 - 倍率判定的 NativeWindowScale 取 OPT-B5 几何快照的 scale
   (drawable/window HiDPI 比;> 1.5f 的上游阈值照抄);
 - 上游 try/catch(Exception)→ log + null 的容错面 = std::optional 返回;
   Cursor == 0 时上游泄漏 surface(靠 GC 终结器)—— C++ 侧析构释放(偏离,
   行为更紧;失败路径上游本就不可用该光标)。
The whole of Sdl2HardwareCursor.cs L20-73 plus the cursor sections of
Sdl2PlatformWindow.cs (L364-377 DoublePixelData, L379-405
CreateHardwareCursor, L412-421 SetHardwareCursor). Shape mapping (D87,
docs/COVERAGE.md): the IHardwareCursor interface face becomes the concrete
type (single platform; the upstream interface is a cross-implementation
facade); Marshal.PtrToStructure<SDL_Surface> + Marshal.Copy become a direct
real-struct field write (memcpy into the pixels region = upstream's
behavior); the NativeWindowScale of the doubling rule reads the OPT-B5
geometry snapshot's scale (the drawable/window HiDPI ratio; the > 1.5f
upstream threshold kept); upstream's try/catch(Exception)→ log + null
tolerance becomes a std::optional return, and where upstream leaks the
surface on a zero cursor handle (GC finalizer eventually frees it) the C++
side destroys it (a deviation, strictly tighter — the cursor is unusable on
that path upstream anyway).

## src/platform/sdl2_input.cpp

UPSTREAM: OpenRA.Platforms.Default/Sdl2Input.cs @1f594e320 L19-252 + MultiTapDetection.cs L17-83
事件泵逐事件移植:修饰符泵前一次(SDL_GetModState)、motion 合并
(pendingMotion 只保留最后一条,左/中/右键事件先 flush)、X1X2 转伪键盘
(MOUSE4/MOUSE5)、滚轮位置取 SDL_GetMouseState、文本输入 UTF-8、
Windows Alt+F4 与 macOS Cmd+Q 退出特例(退出以返回值上报,Phase 5 接 Game)。
X1X2 分支的 IsRepeat 在上游读 e.key.repeat(mouse 事件 union 覆盖读,落点
是 which 的第二字节,实际恒 0);此处取 false,详见 docs/COVERAGE.md 输入偏离条目。
The event pump ported event by event: modifiers sampled once before the
pump (SDL_GetModState), motion coalescing (pendingMotion keeps only the
last sample; left/middle/right button events flush it first), X1X2 as
pseudo-keyboard (MOUSE4/MOUSE5), wheel positions from SDL_GetMouseState,
UTF-8 text input, and the Windows Alt+F4 / macOS Cmd+Q exit special cases
(exit is reported via the return value; Game wiring is Phase 5). The IsRepeat
of the X1X2 branch reads e.key.repeat upstream (a union overlay on the mouse
event landing in the second byte of `which`, effectively always 0); false is
used here — see the input-deviation entry in docs/COVERAGE.md.

## src/platform/sdl2_input.hpp

UPSTREAM: OpenRA.Game/Input/IInputHandler.cs @1f594e320 L17-83(数据面)+
OpenRA.Platforms.Default/Sdl2Input.cs L19-252(事件泵)+ MultiTapDetection.cs L17-83(多击检测)
形态适配:上游 PumpInput 内对 Game.Exit() 的直接调用改为返回值 b_quit_requested
(Game 编排归 Phase 5);TapHistory/MultiTapDetection 的 DateTime.Now 改为
steady_clock 毫秒注入(单调时钟;上游 DateTime.Now 的偶发回退不是契约行为),
键盘缓存键打包为 u64(keycode | mods<<32)替代 (Keycode, Modifiers) 元组键。
The data plane of IInputHandler.cs plus the event pump of Sdl2Input.cs and
the multi-tap detection of MultiTapDetection.cs. Shape adaptations: the
direct Game.Exit() calls inside upstream's PumpInput become a returned
b_quit_requested (Game orchestration is Phase 5); TapHistory/MultiTapDetection
take steady-clock milliseconds injected instead of DateTime.Now (a monotonic
clock; upstream's incidental DateTime.Now setbacks are not contract
behavior), and the keyboard cache key packs into u64 (keycode | mods<<32)
in place of the (Keycode, Modifiers) tuple key.

## src/platform/sdl2_window.cpp

UPSTREAM: OpenRA.Platforms.Default/Sdl2PlatformWindow.cs @1f594e320 L124-215,270-353,538-587
实现:窗口创建/模式切换式样、GL 属性序列(DOUBLEBUFFER=1,RGB888,ALPHA=0;Modern=3.2
Core / Embedded=3.0 ES)、HiDPI scale = drawable/window(Sdl2PlatformWindow.cs L270-276)、
事件泵。几何快照为打包原子量(OPT-B5)。
Implementation: window creation/mode styles, the GL attribute sequence
(DOUBLEBUFFER=1, RGB888, ALPHA=0; Modern=3.2 Core / Embedded=3.0 ES), HiDPI
scale = drawable/window (Sdl2PlatformWindow.cs L270-276), and the event
pump. The geometry snapshot is a packed atomic (OPT-B5).

## src/platform/sdl2_window.hpp

UPSTREAM: OpenRA.Platforms.Default/Sdl2PlatformWindow.cs @1f594e320 L34-587(窗口创建/模式/GL 属性段)
OPT-B5(docs/OPTIMIZATION_TRACKER.md):窗口几何为打包 atomic<u64> 快照,getter 无锁 ——
替代上游每个属性 getter 各取一把 lock(Sdl2PlatformWindow.cs L42-120,scissor 等热路径
每帧多次读)。OPT-A5:GL 上下文由渲染线程创建并持有(统一线程模型,上游
Sdl2PlatformWindow.cs L345-353 的 Windows 窗口模式单线程特例不复刻)。
Window creation/modes/GL attributes per upstream; OPT-B5
(docs/OPTIMIZATION_TRACKER.md): window geometry is a packed atomic<u64>
snapshot with lock-free getters — replacing the per-getter locks of
upstream (Sdl2PlatformWindow.cs L42-120, read several times per frame on
hot paths like scissor). OPT-A5: the GL context is created and owned by the
render thread (a unified threading model; the upstream Windows-windowed
single-thread special case of Sdl2PlatformWindow.cs L345-353 is not
replicated).

## src/platform/sound_engine.hpp

UPSTREAM: OpenRA.Game/Sound/SoundDevice.cs @1f594e320 L20-46(ISoundEngine/
ISoundSource/ISound 接口族与 SoundDevice 记录;文件另含 ISoundLoader/
ISoundFormat/SoundType,归 Sound 门面批次 —— Phase 6)
接口族的 C++ 形态映射:byte[] 参数 → std::span 直传(README 有意优化表);
Stream(Play2DStream)→ IPcmStream 最小顺序读面;float.NaN 的 Volume 读值
语义保留(解绑源后的哨兵值,消费端 Sound.cs 不读该值)。
The C++ shape of the interface family: byte[] parameters pass as
std::span (the README deliberate-optimization table); the Stream of
Play2DStream becomes the minimal sequential-read IPcmStream; the
float.NaN Volume read of a sound whose source was unbound keeps verbatim
(a sentinel the Sound.cs consumer never reads).

## src/platform/tcp_socket.cpp

UPSTREAM: NONE —— 与 tcp_socket.hpp 同一条目(WinSock2 直连基础设施)
UPSTREAM: NONE — the same entry as tcp_socket.hpp (the WinSock2 direct
infrastructure).

## src/platform/tcp_socket.hpp

UPSTREAM: NONE —— PORTING_PLAN §7 自有网络基础设施:.NET TcpClient/Dns/
IPEndPoint 的 WinSock2 直连对应物(NetworkConnection 的连接/读线程与
测试侧伪服务器共用;上游无逐行对应源文件,语义对照 Connection.cs 的
TcpClient 用法面)。NONE = C++ 侧自有设计,见 tools/upstream_check.py
登记形式。
PORTING_PLAN §7 in-house network infrastructure: the WinSock2 direct
counterparts of .NET's TcpClient/Dns/IPEndPoint (shared by
NetworkConnection's connect/read threads and the test-side fake
server; no line-by-line upstream source exists — the semantic anchor
is Connection.cs's TcpClient usage). NONE = C++-native design, see
tools/upstream_check.py for the registration form.

## src/platform/udp_socket.cpp

UPSTREAM: NONE —— 自有基础设施(见 udp_socket.hpp 头注)
NONE = in-house infrastructure (see udp_socket.hpp's header note).

## src/platform/udp_socket.hpp

UPSTREAM: NONE —— PORTING_PLAN §7 自有网络基础设施:.NET UdpClient 的
WinSock2 直连对应物(BeaconLib 的 LAN 发现应答与测试侧共用;上游无逐
行对应源文件,语义对照 BeaconLib 1.0.2 (rix0rrr/beacon) 的 UdpClient
用法面 —— SO_REUSEADDR + Any:port 绑定 + BeginReceive 循环 + Send 回
应答)。NONE = C++ 侧自有设计,见 tools/upstream_check.py 登记形式。
PORTING_PLAN §7 in-house network infrastructure: the WinSock2 direct
counterpart of .NET's UdpClient (shared by BeaconLib's LAN-discovery
responder and the test side; no line-by-line upstream source exists — the
semantic anchor is BeaconLib 1.0.2 (rix0rrr/beacon)'s UdpClient usage:
SO_REUSEADDR + an Any:port bind + the BeginReceive loop + the Send
reply). NONE = C++-native design, see tools/upstream_check.py for the
registration form.

## src/server/color_picker_manager.cpp

UPSTREAM: OpenRA.Mods.Common/Traits/World/ColorPickerManager.cs
         @1f594e320 L66-186(实现体;IsInvalidColor/MakeValid/
         RandomPresetColor 的 RNG 消耗序逐字保真)
         The implementation body (L66-186; IsInvalidColor/MakeValid/
         RandomPresetColor with the RNG consumption order kept verbatim).

## src/server/color_picker_manager.hpp

UPSTREAM: OpenRA.Mods.Common/Traits/World/ColorPickerManager.cs
         @1f594e320 L28-186(服务器消费面全文:IsInvalidColor 的 compuphase
         感知度量 / MakeValid 的双色域钳制 + 双向扩幅搜索 + 400 次迭代
         上限 / RandomPresetColor 的 Fisher-Yates 洗牌遍历与随机回落;
         PresetColors 表 = 规则装载注入;ShowColorDropDown 的 UI 下拉
         面随 Phase 6;注册为 trait Info 的 World 构造面随 Phase 6 装配
         批 —— 本批承载值数据 + 三个纯函数)
         ColorPickerManager.cs L28-186 (the whole server-consumed face:
         IsInvalidColor's compuphase perceptual metric / MakeValid's
         dual-range clamping + the bidirectional expanding search with the
         400-iteration cap / RandomPresetColor's Fisher-Yates-shuffled
         walk and the random fallback; the PresetColors table = a
         rules-loading injection; ShowColorDropDown's UI face rides
         Phase 6, as does the trait-Info world-construction face — this
         batch carries the value data + the three pure functions).

## src/server/game_information.cpp

UPSTREAM: OpenRA.Game/GameInformation.cs @1f594e320 L71-121(双节 yaml
         序列化全文;FieldLoader/FieldSaver → 按名手写表)
         GameInformation.cs L71-121 (the whole two-node yaml
         serialization; FieldLoader/FieldSaver → the by-name hand-written
         tables).

## src/server/game_information.hpp

UPSTREAM: OpenRA.Game/GameInformation.cs @1f594e320 L21-237(Root/Player
         双节 yaml 形态:字段序 = 声明序;DateTime → "yyyy-MM-dd HH-mm-ss"
         不变量格式;WinState → 枚举名;玩家空位 = 上游 null 的
         std::optional 承载 —— 服务器 defeat 跟踪保持 world.Players
         的下标对齐)
         GameInformation.cs L21-237 (the Root/Player two-node yaml shape:
         field order = declaration order; DateTime → the invariant
         "yyyy-MM-dd HH-mm-ss" format; WinState → the enum name; the
         null player slots ride std::optional — the server's defeat
         tracking keeps the world.Players index alignment).

## src/server/lobby_commands.cpp

UPSTREAM: OpenRA.Mods.Common/ServerTraits/LobbyCommands.cs @1f594e320
         L29-1492 + LobbyUtils.cs L289-302(实现体;Dictionary 命令表 →
         静态 string_view/function 表,LINQ → 消费时物化)
         The implementation body (LobbyCommands.cs + LobbyUtils.cs
         L289-302; the Dictionary command table → a static
         string_view/function table, LINQ → consumption-time
         materialization).

## src/server/lobby_commands.hpp

UPSTREAM: OpenRA.Mods.Common/ServerTraits/LobbyCommands.cs @1f594e320
         L29-1492 全文(24 命令分发字典 + ValidateCommand 双前置门 +
         CheckAutoStart 的六段自动开局 + State/StartGame/Slot/
         AllowSpectators/Spectate/SlotClose/SlotOpen/SlotBot/Map/Option/
         ResetOptions/AssignTeams/Kick/VoteKick/MakeAdmin/MakeSpectator/
         Name/Faction/Team/Handicap/Spawn/ClearPlayerSpawn/PlayerColor/
         SyncLobby 二十命令全文 + InitializeMapPool/ChooseInitialMap/
         ServerStarted 的地图装配 + LoadMapSettings 的选项状态迁移 +
         SanitizePlayerColor/SanitizePlayerFaction + ClientJoined 的
         SyncMapPool 与色彩消毒 + ServerEmpty 的槽位重置;
         LobbyUtils.InsufficientEnabledSpawnPoints(LobbyUtils.cs L289-302)
         一并移植;ModData 大厅面(MapCache/色彩管理器/ILobbyOptions/
         IBotInfo/远程查询)经 ServerDeps 注入族承载 —— 逐 trait 的
         LobbyOptions(map) 装配随其宿主 trait 批,AI 的 IBotInfo 面
         随 Phase 8)
         The whole of LobbyCommands.cs L29-1492 (the 24-command dispatch
         dictionary + ValidateCommand's two preamble gates +
         CheckAutoStart's six-stage auto-start + the full twenty-command
         bodies State/StartGame/Slot/AllowSpectators/Spectate/SlotClose/
         SlotOpen/SlotBot/Map/Option/ResetOptions/AssignTeams/Kick/
         VoteKick/MakeAdmin/MakeSpectator/Name/Faction/Team/Handicap/
         Spawn/ClearPlayerSpawn/PlayerColor/SyncLobby + InitializeMapPool/
         ChooseInitialMap/ServerStarted's map assembly + LoadMapSettings'
         option-state migration + SanitizePlayerColor/SanitizePlayerFaction
         + ClientJoined's SyncMapPool and color sanitization +
         ServerEmpty's slot reset; LobbyUtils.InsufficientEnabledSpawnPoints
         (LobbyUtils.cs L289-302) ported along; the ModData lobby faces
         (MapCache/the color manager/ILobbyOptions/IBotInfo/the remote
         query) ride the ServerDeps injection family — the per-trait
         LobbyOptions(map) assembly rides its host trait's batch, and the
         AI's IBotInfo face rides Phase 8).

## src/server/lobby_option.cpp

UPSTREAM: OpenRA.Game/Traits/TraitsInterfaces.cs @1f594e320 L576-620
         (LobbyOption/LobbyBooleanOption 值面;实现体)
         LobbyOption/LobbyBooleanOption's value face (implementation).

## src/server/lobby_option.hpp

UPSTREAM: OpenRA.Game/Traits/TraitsInterfaces.cs @1f594e320 L571-612(
         ILobbyOptions 接口 + LobbyOption/LobbyBooleanOption 值面全
         文;map.GetMessage 的本地化名 = 上游直返 key 的回落面 ——
         FluentProvider 随 Phase 6,LobbyOptions(map) 的逐 trait 装配
         面(PlayerResources/MapOptions 等 11 宿主)随其宿主 trait 批,
         本批以 LobbyCommandsDeps.fn_lobby_options 注入)
         TraitsInterfaces.cs L571-612 (the ILobbyOptions interface + the
         whole LobbyOption/LobbyBooleanOption value face; map.GetMessage's
         localized names = the key-returning fallback face — the
         FluentProvider rides Phase 6, and the per-trait assembly of
         LobbyOptions(map) (the eleven hosts PlayerResources/MapOptions/
         and friends) rides those hosts' own batches, injected here via
         LobbyCommandsDeps.fn_lobby_options).

## src/server/map_status_cache.cpp

UPSTREAM: OpenRA.Game/Server/MapStatusCache.cs @1f594e320 L19-106
         (实现体)
         The implementation body.

## src/server/map_status_cache.hpp

UPSTREAM: OpenRA.Game/Server/MapStatusCache.cs @1f594e320 L19-106 全文
         (索引器的规则装载/UnsafeCustomRules 位/玩家数上限三段定级 +
         Validating 的后台 lint 线程 + RunLintTests 的失败定位级;
         ILintServerMapPass 的类型枚举 = ObjectCreator 反射 → 注入面
         (Mods.Common 的 lint pass 族未移植);ThreadPool → 自持
         std::thread(析构 join;登记 D224);Dictionary → mutex + map
         (worker 线程的并发写;上游裸字典竞态不设防,C++ 侧加锁 —— 行
         为等价、竞态面收紧)
         The whole of MapStatusCache.cs L19-106 (the indexer's three-stage
         grading — the rules load / the UnsafeCustomRules bit / the
         player-count cap — + Validating's background lint thread +
         RunLintTests's failure-to-grade flow; ILintServerMapPass's type
         enumeration = ObjectCreator reflection → an injection face (the
         Mods.Common lint-pass family is unported); ThreadPool → a
         self-held std::thread (joined at destruction; registered D224);
         Dictionary → a mutex + map (the worker thread's concurrent write;
         upstream's bare dictionary is race-unguarded, C++ locks —
         behavior-equivalent, the race window closed).

## src/server/master_server_pinger.cpp

UPSTREAM: OpenRA.Mods.Common/ServerTraits/MasterServerPinger.cs
         @1f594e320 L27-217 + BeaconLib 1.0.2(rix0rrr/beacon)的
         Beacon.cs(实现体;Task.Run 的闭包态 → shared_ptr 共享态,HTTP
         线程不触达 trait 生命周期 —— 上游 GC 常青语义的确定性等价)
         The implementation body (MasterServerPinger.cs + BeaconLib
         1.0.2's Beacon.cs; Task.Run's closure state → a shared_ptr state
         block, so the HTTP thread never touches the trait's lifetime —
         the deterministic equivalent of upstream's GC evergreen liveness).

## src/server/master_server_pinger.hpp

UPSTREAM: OpenRA.Mods.Common/ServerTraits/MasterServerPinger.cs
         @1f594e320 L27-217 全文(3min 广告刷新/1s 限速的 Tick 门 +
         isBusy 双闸 + GameServer.ToPOSTData 的在线/LAN 双投递 +
         首投的 [code]message 应答解析与错误码表 + 消息队列的服务器线
         程汇流;BeaconLib.Beacon → 自持 LanBeacon(UDP 35891 发现应
         答,RIX0RRR 编码:2 字节小端长度前缀 + UTF-8;LanAdvertisePort
         的 System.Random(ms) → MersenneTwister(ms),值域面等价);
         HttpClient → 注入面(缺省 = 立即失败 → Error 通知,与不可达
         主服务器同 observable);Game.RunTime → Server::RunTimeMs)
         The whole of MasterServerPinger.cs L27-217 (the 3min
         advertisement refresh / the 1s rate-limited Tick gate + the
         isBusy double gate + GameServer.ToPOSTData's online/LAN dual
         delivery + the first ping's [code]message response parsing with
         the error-code table + the message queue's server-thread drain;
         BeaconLib.Beacon → a self-held LanBeacon (the UDP 35891
         discovery responder, rix0rrr encoding: a 2-byte little-endian
         length prefix + UTF-8; LanAdvertisePort's System.Random(ms) →
         MersenneTwister(ms), value-domain equivalence); HttpClient → an
         injection face (default = immediate failure → the Error
         notification, the same observable as an unreachable master);
         Game.RunTime → Server::RunTimeMs).

## src/server/order_buffer.cpp

UPSTREAM: OpenRA.Game/Server/OrderBuffer.cs @1f594e320 L42-137 全文
         (the whole of OrderBuffer.cs L42-137)

## src/server/order_buffer.hpp

UPSTREAM: OpenRA.Game/Server/OrderBuffer.cs @1f594e320 L21-138 全文
         (TickScale 动态延迟的测量核:玩家订单时间戳 → 基线差值的中位
         数 → (timestep + deltaPerTick)/timestep,钳 [1, 1.1];20 帧
         窗口/1000ms 更新间隔;ConcurrentDictionary → 单线程服务器事
         件的插入序容器 —— 上游并发容器仅为防御形态,本侧单线程访问
         等价)
         The whole of OrderBuffer.cs L21-138 (the measurement core of
         the dynamic TickScale latency: player order timestamps → the
         median of the baseline deltas → (timestep + deltaPerTick) /
         timestep clamped to [1, 1.1]; the 20-frame window / 1000ms
         update interval; ConcurrentDictionary → the single-threaded
         server-event insertion-ordered container — upstream's
         concurrent containers are a defensive shape only, and this
         side's single-threaded access is equivalent).

## src/server/player_pinger.cpp

UPSTREAM: OpenRA.Mods.Common/ServerTraits/PlayerPinger.cs @1f594e320
         L18-104(实现体)
         The implementation body.

## src/server/player_pinger.hpp

UPSTREAM: OpenRA.Mods.Common/ServerTraits/PlayerPinger.cs @1f594e320
         L18-104 全文(5s ping 周期的无 client 踢/超时预警/60s 超时
         踢线 + 20s 周期的超时倒计汇报;单机 < 2 人且非专用服豁免;
         Game.RunTime → Server::RunTimeMs 注入面)
         The whole of PlayerPinger.cs L18-104 (the 5s-ping cycle's
         client-less kick / the stall warning / the 60s-timeout drop +
         the 20s cycle's timeout countdown report; the sub-2-player
         non-dedicated exemption; Game.RunTime → the Server::RunTimeMs
         injection face).

## src/server/protocol_version.hpp

UPSTREAM: OpenRA.Game/Server/ProtocolVersion.cs @1f594e320 L73-81(握手
         协议版本 = 7(同 net/network_connection.hpp 的
         kHandshakeProtocolVersion,双锚点合一处)+ 订单协议版本 = 21;
         常量语义照抄,注释块描述的线格式见各实现文件的字段序)
         ProtocolVersion.cs L73-81 (the handshake protocol version = 7,
         sharing net/network_connection.hpp's kHandshakeProtocolVersion,
         now unified here, + the orders protocol version = 21).

## src/server/server.cpp

UPSTREAM: OpenRA.Game/Server/Server.cs @1f594e320 L181-1601 + OpenRA.Game/
         Server/PlayerMessageTracker.cs L26-84(服务器实体与聊天闸的实
         现全文;事件闭包/注入面偏离见 docs/COVERAGE.md D216~)
         Server.cs L181-1601 + PlayerMessageTracker.cs L26-84 (the
         implementation whole of the server entity and the chat gate;
         the event-closure/injection-face deviations live in
         docs/COVERAGE.md D216-on).

## src/server/server.hpp

UPSTREAM: OpenRA.Game/Server/Server.cs @1f594e320 L33-1601(服务器实体
         全文:监听线程群 + 单线程事件循环 + AcceptConnection 的令牌
         握手 + ValidateClient 的六门校验 + CompleteConnection 的槽位
         装配 + ReceiveOrders 的 OrderLatency 前投影与 Ack 回执 +
         HandleSyncOrder 的服务器侧 desync 检测与 defeat 状态跟踪 +
         StartGame 的空帧注入(L1444-1464 注释语义)+ DropClient 的
         管理员重指派 + SyncLobby 族 + ReceivePing 的连接质量分级;
         BlockingCollection<IServerEvent> → mutex/cv 闭包队列,Lock →
         std::mutex;Map/PlayerDatabase/GeoIP/Nat/HTTP 鉴权/GameSave/
         FluentProvider 为注入面 —— 见 docs/COVERAGE.md D216~)
         The whole of the server entity, Server.cs L33-1601 (the
         listener threads + the single-threaded event loop +
         AcceptConnection's token handshake + ValidateClient's six
         gates + CompleteConnection's slot assembly + ReceiveOrders'
         OrderLatency forward projection with the Ack receipt +
         HandleSyncOrder's server-side desync detection and defeat-state
         tracking + StartGame's empty-frame injection (the L1444-1464
         comment semantics) + DropClient's admin reassignment + the
         SyncLobby family + ReceivePing's connection-quality grading;
         BlockingCollection<IServerEvent> → the mutex/cv closure queue,
         Lock → std::mutex; Map/PlayerDatabase/GeoIP/Nat/HTTP-auth/
         GameSave/FluentProvider ride injection faces — see
         docs/COVERAGE.md D216-on).

## src/server/server_connection.cpp

UPSTREAM: OpenRA.Game/Server/Connection.cs @1f594e320 L47-216(读循环/
         发送队列全文)
         Connection.cs L47-216 (the read loop/send queue's whole text).

## src/server/server_connection.hpp

UPSTREAM: OpenRA.Game/Server/Connection.cs @1f594e320 L24-220(服务器侧
         每连接线程全文:100ms Poll 读帧机(Header/Data 状态机 + 128kB
         超长闸)、Ping 的线程内自处理(历史 15 样本上限)、发送队列的
         WouldBlock 回落阻塞发送、空包断连语义;BlockingCollection →
         mutex/cv 集合(D203 同形),background Thread → 自持 shared
         引用的 std::thread(末路自分离 = GC 常青语义的确定性等价))
         The whole of the server-side per-connection thread, Connection.
         cs L24-220 (the 100ms-poll read frame machine with the Header/
         Data states + the 128kB oversize gate, the inline Ping handling
         with the 15-sample history cap, the send queue's WouldBlock
         fallback to a blocking send, the empty-packet disconnect
         semantics; BlockingCollection → the mutex/cv collection (the
         same shape as D203), the background Thread → a shared-self-
         holding std::thread (the final self-detach = the deterministic
         equivalent of GC evergreen liveness)).

## src/server/server_settings.cpp

UPSTREAM: OpenRA.Game/Settings.cs @1f594e320 L515-522(SanitizedServerName
         全文;SanitizedName/SanitizedPlayerName 已随第十六批迁入
         ora::net —— net/sanitize.cpp)
         The whole of SanitizedServerName (L515-522); SanitizedName/
         SanitizedPlayerName moved into ora::net with batch 16 —
         net/sanitize.cpp).

## src/server/server_settings.hpp

UPSTREAM: OpenRA.Game/Settings.cs @1f594e320 L92-154(ServerSettings 全
         字段 + 默认值逐字;L500-541 的 SanitizedName/SanitizedServerName/
         SanitizedPlayerName 全文 —— MiniYAML/JSON 保留字符剥离与 16 字
         截断;Ban/MapPool 的 FrozenSet → 插入序 vector,Whitelist/
         Blacklist 同形)
         Settings.cs L92-154 (ServerSettings' every field + default,
         verbatim) + L500-541 (the whole of SanitizedName/
         SanitizedServerName/SanitizedPlayerName — the MiniYAML/JSON
         reserved-character strip and the 16-char truncation; Ban's/
         MapPool's FrozenSet → insertion-ordered vectors, Whitelist/
         Blacklist the same shape).

## src/server/server_traits.cpp

UPSTREAM: OpenRA.Game/Server/TraitInterfaces.cs @1f594e320 L30-62
         (DebugServerTrait 全文)
         TraitInterfaces.cs L30-62 (the whole of DebugServerTrait).

## src/server/server_traits.hpp

UPSTREAM: OpenRA.Game/Server/TraitInterfaces.cs @1f594e320 L14-62(九个
         服务器 trait 接口 + ServerTrait 基类 + DebugServerTrait;接口
         分发面 = 服务器事件循环的 WithInterface<T> 查询 —— C++ 侧以
         注册期 dynamic_cast 装配的按接口向量承载,LobbyCommands 批
         接首个真实现)
         TraitInterfaces.cs L14-62 (the nine server-trait interfaces +
         the ServerTrait base + DebugServerTrait; the interface
         dispatch face = the event loop's WithInterface<T> queries —
         carried C++-side as per-interface vectors assembled by
         registration-time dynamic_cast, with the LobbyCommands batch
         supplying the first real implementation).

## src/server/vote_kick_tracker.cpp

UPSTREAM: OpenRA.Game/Server/VoteKickTracker.cs @1f594e320 L18-223
         (实现体)
         The implementation body.

## src/server/vote_kick_tracker.hpp

UPSTREAM: OpenRA.Game/Server/VoteKickTracker.cs @1f594e320 L18-223 全文
         (ClientHasPower 的管理员/存活双门/Tick 的离场结束与超时
         阻断/VoteKick 的七门 + 合法票数账本 + 起始广播与逐票进度 +
         EndKickVoteAndBlockKicker 的失败冷却/EndKickVote 的全清;
         (Client, Conn) 元组 = Client.Index + shared_ptr<ServerConnection>
         承载 —— 上游按对象引用恒等,C++ 以 Index 恒等(索引唯一,等价;
         登记 D223)
         The whole of VoteKickTracker.cs L18-223 (ClientHasPower's
         admin/alive double gate / Tick's departure-end and timeout-block /
         VoteKick's seven gates + the eligible-votes ledger + the start
         broadcast and the per-vote progress + EndKickVoteAndBlockKicker's
         failure cooldown / EndKickVote's full clear; the (Client, Conn)
         tuple = Client.Index + a shared_ptr<ServerConnection> — upstream
         keys by object identity, C++ keys by Index (indexes unique,
         equivalent; registered as D223).

## src/sim/activity.cpp

UPSTREAM: OpenRA.Game/Activities/Activity.cs @1f594e320 L95-226(实现部分)
         The implementation half of Activity.cs.

## src/sim/activity.hpp

UPSTREAM: OpenRA.Game/Activities/Activity.cs @1f594e320 L21-295(逐行重写)
         + OpenRA.Game/Traits/ActivityUtils.cs L17-38
         Line-by-line rewrite of Activity.cs + ActivityUtils.cs.

机制对照 / Mechanism mapping:
 - ActivityState/State/ChildActivity/NextActivity/SkipDoneActivities/
   TickOuter/TickChild/Cancel/Queue/QueueChild:控制流逐语句对应(含
   lastRun/finishing 的"child 刚入队免延迟 tick"分支)
   ActivityState/State/ChildActivity/NextActivity/SkipDoneActivities/
   TickOuter/TickChild/Cancel/Queue/QueueChild: statement-level control
   flow correspondence (including the lastRun/finishing "freshly queued
   child skips a tick delay" branch).
 - TargetLineNode/GetTargets/TargetLineNodes(PrintActivityTree):渲染域
   (Sprite/Color/Target 线),Phase 4/6 随 Target 渲染面落地 —— 本文件不引入
   TargetLineNode/GetTargets/TargetLineNodes (PrintActivityTree): render
   domain (Sprite/Color/Target lines), landing with the Target render
   surface in Phase 4/6 — not pulled into this file.
 - ActivitiesImplementing<T>:IActivityInterface 标记 + 动态转型的模板
   等价物(dynamic_cast;上游 is T + cast)
   ActivitiesImplementing<T>: the template equivalent of the
   IActivityInterface marker + runtime cast (dynamic_cast; upstream is
   `is T` + cast).

## src/sim/actor.cpp

UPSTREAM: OpenRA.Game/Actor.cs @1f594e320 L129-615(实现部分;渲染面除外)
         The implementation half of Actor.cs (render surface excluded).

## src/sim/actor.hpp

UPSTREAM: OpenRA.Game/Actor.cs @1f594e320 L27-651(逐语义重写;渲染/Lua 面
         随 Phase 4/8)
         Verbatim-semantics rewrite; the render/Lua surfaces land with
         Phase 4/8.

机制对照 / Mechanism mapping:
 - 条件系统(L89-109/558-615):Dictionary→std::map(键序稳定;ConditionState
   的 HashSet<int> Tokens 仅做成员测试与计数 → std::set<int>);
   IReadOnlyDictionary 只读视图 → const std::map&
   The condition system (L89-109/558-615): Dictionary→std::map (stable
   key order; ConditionState's HashSet<int> Tokens only does membership
   and counting → std::set<int>); the IReadOnlyDictionary read-only view
   → const std::map&.
 - Initialize 的初始通知去重(L221/241):C# HashSet<delegate> 按方法+目标
   去重;std::function 无身份 —— 改为逐 observer 条目顺序通知(与
   UpdateConditionState 的重复调用语义一致;受端幂等是 trait 契约)。
   COVERAGE 登记
   The Initialize initial-notification dedup (L221/241): C# dedups
   HashSet<delegate> by method+target; std::function has no identity —
   notify each observer entry in order instead (matching the
   repeat-call semantics of UpdateConditionState; idempotent receivers
   are the trait contract). Registered in COVERAGE.
 - trait 创建(L169-171):traitInfo.Create(init) 的工厂面 Phase 5 才有具体
   trait 类 —— 经 World 注入的 trait 工厂承载(返回构造序 trait 列表),
   Actor 侧"逐 trait AddTrait + 接口缓存"循环保持上游形态
   Trait creation (L169-171): the traitInfo.Create(init) factory surface
   has no concrete trait classes until Phase 5 — carried by a
   World-injected trait factory (returning the construct-order trait
   list); the Actor-side "AddTrait each + cache interfaces" loop keeps
   the upstream shape.
 - 规则表查询(L152-155):world.Map.Rules.Actors[name] 的规则面同样在工厂
   内闭合(name 小写/查表/异常文本 "No rules definition for unit X" 逐字)
   The rules lookup (L152-155): the world.Map.Rules.Actors[name] surface
   also closes inside the factory (name lower-casing, table lookup, and
   the verbatim "No rules definition for unit X" error).
 - 渲染缓存面(L71-127/292-349:Render/ScreenBounds/MouseBounds/renders/
   renderModifiers/mouseBounds/visibility...):Phase 4;CanBeViewedByPlayer
   的可见性判定依赖 Shroud(Phase 5),本文件保留接口位
   The render caches (L71-127/292-349) are Phase 4; CanBeViewedByPlayer's
   visibility check depends on Shroud (Phase 5) — interface slots kept.

## src/sim/actor_init.cpp

UPSTREAM: OpenRA.Game/Map/ActorInitializer.cs @1f594e320 L223-262(OwnerInit)
         The OwnerInit half of ActorInitializer.cs.

## src/sim/actor_init.hpp

UPSTREAM: OpenRA.Game/Map/ActorInitializer.cs @1f594e320 L21-262(逐语义重写)
         Verbatim-semantics rewrite.

机制对照 / Mechanism mapping:
 - IActorInitializer/ActorInitializer:模板查询转发 TypeDictionary;
   info.InstanceName 匹配语义(带名 init 优先同名,回落无名;重复取最后
   —— LastOrDefault)逐字保留
   IActorInitializer/ActorInitializer: template queries forwarding into
   the TypeDictionary; the info.InstanceName matching semantics (named
   inits prefer the same name, falling back to unnamed ones; duplicates
   take the last — LastOrDefault) kept verbatim.
 - ValueActorInit<T>.Initialize(yaml) 的反射写字段 → 构造注入
   (C++ 侧值不可变;Save/Initialize 面随 Phase 5 地图序列化落地)
   The reflection-backed field writes of ValueActorInit<T>.Initialize
   (yaml) become constructor injection (values are immutable on the C++
   side; the Save/Initialize surface lands with Phase 5 map
   serialization).
 - 注册面:上游 Add 注册 GetInterfaces()+BaseTypes()(含泛型基类 Type 键);
   C++ 侧经 ORA_INIT_TYPE 声明 {接口, 自身}。泛型基类键
   (ValueActorInit<T>)无查询使用点,省略 —— COVERAGE 登记
   Registration surface: upstream Add registers GetInterfaces() +
   BaseTypes() (generic base Type keys included); the C++ side declares
   {interfaces, self} via ORA_INIT_TYPE. Generic base keys
   (ValueActorInit<T>) have no query sites and are omitted — registered
   in COVERAGE.

## src/sim/actor_map.cpp

UPSTREAM: OpenRA.Mods.Common/Traits/World/ActorMap.cs @1f594e320(实现部分)
         The implementation half of ActorMap.cs.

## src/sim/actor_map.hpp

UPSTREAM: OpenRA.Mods.Common/Traits/World/ActorMap.cs @1f594e320 L21-688
         (逐语义重写)
         Verbatim-semantics rewrite.

机制对照 / Mechanism mapping:
 - InfluenceNode 链表节点(上游 GC 托管):C++ 侧 heap 分配、摘链即删
   (RemoveInfluenceInner 的 ref 重写不释放节点 —— GC 语义的显式等价:
   被摘节点 delete,链序保持)
   The InfluenceNode linked-list nodes (upstream GC-managed): heap
   allocation here, unlink = delete (upstream's ref-rewrite abandons the
   nodes to the GC — the explicit equivalent: unlinked nodes are deleted,
   list order preserved).
 - HashSet<Actor>(addActorPosition/removeActorPosition/触发器的 old/current
   集)→ 插入序 vector + ActorID 判重(D 系惯例;上游 HashSet 实现序在
   无删除时即插入序 —— tick 消费序一致)
   The HashSet<Actor> (add/remove position + trigger old/current sets) →
   insertion-ordered vectors + ActorID dedup (the D-series convention;
   upstream's HashSet order is insertion order absent removals — the
   tick consumption order matches).
 - LargestActorRadius(L207-209):各 actor 的 HitShapeInfo.Type.OuterRadius
   最大值 —— 值袋的嵌套形状记录(CircleShape/RectangleShape/CapsuleShape)
   按 OuterRadius 公式求值(Circle=Radius;Rectangle=corners 最大长度;
   Capsule=Radius+max(|A|,|B|));无 IBlocksProjectilesInfo actor 时的
   LargestBlockingActorRadius = WDist.Zero 分支照抄
   LargestActorRadius (L207-209): the max of every actor's
   HitShapeInfo.Type.OuterRadius — evaluated from the bag's nested shape
   records per the OuterRadius formulas; the no-blockers
   LargestBlockingActorRadius = WDist.Zero branch kept.
 - CellUpdated 事件 → construct 序回调表(与 Map/CellLayer 事件同型)
   The CellUpdated event → callbacks in registration order (the same
   shape as the Map/CellLayer events).
 - GetCustomMovementLayers 扩展方法(L674-688)→ World::CustomMovementLayers()
   The GetCustomMovementLayers extension (L674-688) →
   World::CustomMovementLayers().

## src/sim/conditional_trait.cpp

UPSTREAM: OpenRA.Mods.Common/Traits/Conditions/ConditionalTrait.cs @1f594e320
         L19-39(ConditionalTraitInfo.RulesetLoaded 的 NoVariables 求值面)
         The NoVariables-evaluation face of
         ConditionalTraitInfo.RulesetLoaded (L19-39).

## src/sim/conditional_trait.hpp

UPSTREAM: OpenRA.Mods.Common/Traits/Conditions/ConditionalTrait.cs @1f594e320
         L19-92 全文 + PausableConditionalTrait.cs L19-77 全文
         The whole of Conditions/ConditionalTrait.cs L19-92 + the whole of
         PausableConditionalTrait.cs L19-77.

机制对照 / Mechanism mapping:
 - C# 的 Info 侧基类(ConditionalTraitInfo.RulesetLoaded 的
   EnabledByDefault/PausedByDefault)在值袋侧无生命周期钩子 → 解析时点
   从"规则加载"移到"工厂解析"(NoVariables 求值同一表达式;时点差异 =
   COVERAGE 登记,同 Health 的 D 系)
   The C# Info-side base (ConditionalTraitInfo.RulesetLoaded's
   EnabledByDefault/PausedByDefault) has no lifecycle hook on the bag
   side → the evaluation moment moves from rules-load to
   factory-parse (the same NoVariables expression; the timing
   difference is registered in COVERAGE, like Health's D series).
 - 泛型基类 ConditionalTrait<InfoType> → CRTP 组合核(ConcreteT 静态派发
   TraitEnabled/TraitDisabled/TraitResumed/TraitPaused 四钩子,零虚表
   间接);具体 trait 类继承 TraitBase + IObservesVariables +
   INotifyCreated,转发到核
   The generic base ConditionalTrait<InfoType> → a CRTP composition core
   (ConcreteT statically dispatching the four hooks
   TraitEnabled/TraitDisabled/TraitResumed/TraitPaused, zero vtable
   indirection); the concrete trait class inherits TraitBase +
   IObservesVariables + INotifyCreated and forwards into the core.
 - 上游 GetVariableObservers 的 yield 序(base 先、子类追加)以
   "核收集 + 子类 append"保真
   Upstream's GetVariableObservers yield order (base first, the
   subclass's additions after) is kept by "the core collects + the
   subclass appends".

## src/sim/control_groups.cpp

UPSTREAM: OpenRA.Mods.Common/Traits/World/ControlGroups.cs @1f594e320
         (实现部分)
         The implementation half of ControlGroups.cs.

## src/sim/control_groups.hpp

UPSTREAM: OpenRA.Mods.Common/Traits/World/ControlGroups.cs @1f594e320 L19-150
         (逐语义重写)
         Verbatim-semantics rewrite.

机制对照 / Mechanism mapping:
 - List<Actor>[] 组表 + Contains/RemoveAll 的线性语义照抄(组内成员数以
   个位数为主,线性即上游形态);IsInWorld/Disposed/Owner 过滤面原样
   The List<Actor>[] group table with the linear Contains/RemoveAll
   semantics kept (groups hold single-digit member counts — linear is the
   upstream shape); the IsInWorld/Disposed/Owner filters verbatim.

## src/sim/debug_pause_state.cpp

UPSTREAM: OpenRA.Game/Traits/DebugPauseState.cs 实现部分
         The implementation half.

## src/sim/debug_pause_state.hpp

UPSTREAM: OpenRA.Game/Traits/DebugPauseState.cs @1f594e320 L18-39 全文
         (第十二批 G2 黄金对拍补面:world actor 的 ISync 条目缺失会
         平移 SyncHash 的 n 计数)
         The whole of DebugPauseState.cs L18-39 (the batch-12 G2 golden
         differential's missing face: an absent world-actor ISync entry
         shifts the SyncHash n counter).

## src/sim/effects.cpp

UPSTREAM: OpenRA.Game/Effects/DelayedAction.cs @1f594e320 L18-36(实现)
         DelayedAction implementation.

## src/sim/effects.hpp

UPSTREAM: OpenRA.Game/Effects/IEffect.cs @1f594e320 L16-28
         + OpenRA.Game/Effects/DelayedAction.cs L18-37
         + OpenRA.Game/Effects/DelayedImpact.cs(结构对照;渲染面 Phase 4)
         IEffect.cs + DelayedAction.cs + DelayedImpact.cs (structure
         carried; render surface Phase 4).

机制对照 / Mechanism mapping:
 - IEffect.Render(WorldRenderer) → 本期空实现占位(WorldRenderer 是 Phase 4
   类型;World.Remove 的 effect 存储与 ISync 收集不依赖渲染)
   IEffect.Render(WorldRenderer) → an empty placeholder this phase
   (WorldRenderer is a Phase 4 type; World's effect storage and ISync
   collection do not depend on rendering).
 - DelayedAction 的 world.AddFrameEndTask(w => { w.Remove(this); a(); }):
   捕获 this 的裸引用 —— effect 生命周期由 World 的 effects 容器托管,
   Remove 后由 World 统一析构(见 world.hpp 所有权注释)
   DelayedAction's world.AddFrameEndTask(w => { w.Remove(this); a(); }):
   a bare self-reference — effect lifetimes are owned by the World's
   effects containers and destroyed by the World after Remove (see the
   ownership notes in world.hpp).

## src/sim/frozen_actor_layer.cpp

UPSTREAM: OpenRA.Game/Traits/Player/FrozenActorLayer.cs @1f594e320 L19-386
         实现部分(the implementation half)。

## src/sim/frozen_actor_layer.hpp

UPSTREAM: OpenRA.Game/Traits/Player/FrozenActorLayer.cs @1f594e320 L19-386
         全文逐语义重写(FrozenActor + FrozenActorLayer)
         The whole of FrozenActorLayer.cs L19-386 (FrozenActor +
         FrozenActorLayer).

机制对照 / Mechanism mapping:
 - FrozenActor 的渲染缓存面(Renderables/ScreenBounds/MouseBounds 字段、
   Flash、Render/HasRenderables):字段保留(渲染收集批写入);本批无写
   入者 —— FrozenUnderFog/渲染收集随其后各批(COVERAGE 登记)
   FrozenActor's render caches (the Renderables/ScreenBounds/MouseBounds
   fields, Flash, Render/HasRenderables): fields kept (written by the
   render-collection batch); no writer exists yet — FrozenUnderFog and
   the render collection ride their later batches (registered in
   COVERAGE).
 - FrozenActorLayer 的 SpatiallyPartitioned<FrozenActor> → 通用模板的
   指针实例化(SpatiallyPartitioned<FrozenActor*>;FrozenActor 由
   FrozenActorLayer 的 world arena 构造并独占)
   FrozenActorLayer's SpatiallyPartitioned<FrozenActor> → the generic
   template's pointer instantiation (SpatiallyPartitioned<FrozenActor*>;
   FrozenActors are constructed inside the layer's world arena and owned
   there).
 - ScreenMap 的 Cache<Player,…> 惰性缓存 → unordered_map<const Player*,
   …> 惰性构造(screen_map.hpp 的冻结面本批落地)
   ScreenMap's Cache<Player,…> lazy caches → unordered_map<const
   Player*, …> lazily constructed (screen_map.hpp's frozen faces land
   this batch).
 - ITooltip/ITooltipInfo 的运行时面:接口 + 值袋承载(TooltipInfo 字段
   面随具体 Tooltip trait 批);FirstEnabledTraitOrDefault = 首个非禁用
   The ITooltip/ITooltipInfo runtime faces: the interfaces + the
   value-bag carrier (TooltipInfo's field face rides the concrete
   Tooltip trait batch); FirstEnabledTraitOrDefault = the first
   non-disabled.
 - Dictionary<uint, FrozenActor> → std::map<uint32_t, FrozenActor*>(
   Tick 的遍历序进入 VisibilityHash/FrozenHash 的加法序 —— 上游
   Dictionary 序在插入序稳定性下等价,而两哈希均为求和(交换律),
   遍历序差异不可观测;map 的键序为确定性锚点)
   Dictionary<uint, FrozenActor> → std::map<uint32_t, FrozenActor*>
   (Tick's iteration order enters the addition order of
   VisibilityHash/FrozenHash — upstream's Dictionary order equals
   insertion order in practice, both hashes are sums (commutative), so
   the traversal-order difference is unobservable; the map's key order
   is the determinism anchor).

## src/sim/player.cpp

UPSTREAM: OpenRA.Game/Player.cs @1f594e320 实现部分+ Faction.cs 的解析面
         The implementation half of Player.cs + the Faction.cs parse face.

## src/sim/player.hpp

UPSTREAM: OpenRA.Game/Player.cs @1f594e320 L27-337(逐语义重写;Shroud/
         FrozenActorLayer 面随该批、Fluent 本地化随 Phase 6、Lua 随 Phase 8)
         Verbatim-semantics rewrite (the Shroud/FrozenActorLayer faces land
         with those batches, Fluent localization with Phase 6, Lua with
         Phase 8).

机制对照 / Mechanism mapping:
 - PlayerBitMask 位身份:上游 LongBitSet<PlayerBitMask>(Player.cs L37/91-93)
   → core/long_bitset.hpp 的全量位集(分配器进程级;World ctor 的
   LongBitSet<PlayerBitMask>.Reset() 锚点在 World 构造序保留)
   The PlayerBitMask bit identity: upstream LongBitSet<PlayerBitMask> →
   the full bit-set in core/long_bitset.hpp (the process-level allocator;
   the World ctor's Reset() anchor keeps its place in the World
   construction order).
 - Faction/DisplayFaction(Info 类引用):Info 走值袋(无运行时类)→
   解析后的 FactionInfoData 值(字段面 Name/InternalName/RandomFactionMembers/
   Selectable/Side/Description;解析 = 生成记录的按名槽位读)
   Faction/DisplayFaction (Info-class references): the Info rides the
   value bag (no runtime class) → a parsed FactionInfoData value (the
   field face; parsing = by-name slot reads over the generated record).
 - PlayerActor 的构造怪癖保留(L204-211):先赋"未初始化 actor"再手动
   Initialize(true),使 INotifyCreated.Created 查询 Player traits 不崩
   The PlayerActor construction quirk kept (L204-211): assign the
   uninitialized actor first, then Initialize(true) by hand, so
   INotifyCreated.Created can query player traits without crashing.
 - Shroud(L213)/FrozenActorLayer(L214):Trait<Shroud>() 的解析面随
   Shroud 批接入 —— 部分覆盖装配面(COVERAGE 登记;上游缺 Shroud 时
   此处即抛,消费面本批为空)
   Shroud (L213) / FrozenActorLayer (L214): the resolution faces land
   with the Shroud batch — the partial-coverage assembly face (in
   COVERAGE; upstream throws here without Shroud, and this batch has no
   consumers yet).

## src/sim/screen_map.cpp

UPSTREAM: OpenRA.Game/Traits/World/ScreenMap.cs @1f594e320(screen_map.hpp
         的实现) | The implementation of screen_map.hpp.

## src/sim/screen_map.hpp

UPSTREAM: OpenRA.Game/Traits/World/ScreenMap.cs @1f594e320 L21-287(逐语义
         重写;FrozenActor 面随 FrozenActorLayer 批落地)
         Verbatim-semantics rewrite; the FrozenActor faces land with the
         FrozenActorLayer batch.

机制对照 / Mechanism mapping:
 - SpatiallyPartitioned<Actor> → OPT-A8 slab 数组化(spatially_partitioned.hpp)
   SpatiallyPartitioned<Actor> → the OPT-A8 slab form.
 - Cache<Player,...> 惰性缓存(FrozenActor 域)本批不落;addOrUpdate/remove
   单遍合批(L54-58 的注释语义)对 Actor/IEffect 面保留
   The Cache<Player,...> lazy caches (the FrozenActor domain) are not
   carried this batch; the one-pass addOrUpdate/remove batching note
   (L54-58) stays for the Actor/IEffect faces.
 - a.MouseBounds(worldRenderer)/a.ScreenBounds(worldRenderer) 的 Actor 渲染
   缓存面随渲染 trait 批接入 —— 本批以注入面承载(缺省 = 空界,即不注册)
   Actor.MouseBounds/ScreenBounds arrive with the render-trait batch —
   carried by injection faces here (the defaults are empty bounds, i.e.
   nothing registers).

## src/sim/selection.cpp

UPSTREAM: OpenRA.Mods.Common/Traits/World/Selection.cs @1f594e320
         (selection.hpp 的实现) | The implementation of selection.hpp.

## src/sim/selection.hpp

UPSTREAM: OpenRA.Mods.Common/Traits/World/Selection.cs @1f594e320 L18-198
         (逐语义重写)
         Verbatim-semantics rewrite.

机制对照 / Mechanism mapping:
 - HashSet<Actor>(插入序枚举 = Actors)→ 插入序 vector + ActorID 判重
   (RemoveWhere 的枚举序 = HashSet 实现序在无删除时为插入序 —— 与上游
   同为"插入序"消费面;同步哈希不含 Selection.Hash 本体,刷新序安全)
   The HashSet<Actor> (its enumeration feeds Actors) becomes an
   insertion-ordered vector with ActorID dedup.
 - world.OrderGenerator.SelectionChanged 经 IOrderGenerator 面分发
   world.OrderGenerator.SelectionChanged dispatches through the
   IOrderGenerator face.
 - 语音面(actor.HasVoice/PlayVoice,L138-144)随 VoiceInfo trait 批;
   本批保留注入钩子(缺省 no-op —— 首个可选 actor 直接 break)
   The voice face (L138-144) lands with the VoiceInfo trait batch; an
   injection hook stands in this batch (the default no-ops — the first
   selectable actor just breaks).

## src/sim/shroud.cpp

UPSTREAM: OpenRA.Game/Traits/Player/Shroud.cs @1f594e320 L19-514 实现部分
         (the implementation half)。

## src/sim/shroud.hpp

UPSTREAM: OpenRA.Game/Traits/Player/Shroud.cs @1f594e320 L19-514(全文逐语义
         重写)
         The whole of Shroud.cs L19-514 (verbatim-semantics rewrite).

机制对照 / Mechanism mapping:
 - ShroudInfo 的 ILobbyOptions 面(lobby UI)随 Phase 6/7;字段进值袋
   (gen 表),本文件只承载运行时语义。fogEnabled/ExploreMapEnabled 的
   OptionOrDefault 消费面在 Created(L146-157)
   ShroudInfo's ILobbyOptions face (the lobby UI) lands with Phase 6/7;
   the fields ride the value bag (the gen tables) while this file carries
   the runtime semantics alone. The OptionOrDefault consumption of
   fogEnabled/ExploreMapEnabled sits in Created (L146-157).
 - 事件 OnShroudChanged(Action<PPos>) → 回调 vector(FrozenActorLayer
   订阅;+= 的多播语义保持)
   The OnShroudChanged event (Action<PPos>) → a callback vector (the
   FrozenActorLayer subscription; += multicasting kept).
 - Dictionary<object, ShroudSource> → std::map<const void*, ShroudSource>
   (键 = trait 指针;上游 object 引用相等 → 指针相等,序仅影响遍历而非
   语义 —— sources 只做 TryAdd/Remove 单键操作)
   Dictionary<object, ShroudSource> → std::map<const void*, ShroudSource>
   (key = the trait pointer; upstream's reference equality → pointer
   equality; the order affects iteration only, and sources sees single-key
   TryAdd/Remove operations alone).
 - Tick 的 touched 向量化搜索(span.IndexOf(true))→ span 线性扫描
   (memchr 级;非 hot 域,行为等价)
   Tick's vectorized touched search (span.IndexOf(true)) → a linear span
   scan (memchr-grade; not a hot domain here, behaviour-equal).
 - 上游 resolvedType 构造默认 0 = Shroud:ProjectedCellLayer<ShroudCellType>
   零初始化即上游 new ProjectedCellLayer 的 default(T) 填充
   Upstream's resolvedType defaults to 0 = Shroud on construction: the
   zero-initialized ProjectedCellLayer<ShroudCellType> equals upstream's
   default(T)-filled new ProjectedCellLayer.

## src/sim/spatially_partitioned.cpp

UPSTREAM: OpenRA.Game/Primitives/SpatiallyPartitioned.cs @1f594e320
         (spatially_partitioned.hpp 的 Actor* 特化实现;OPT-A8 slab 形态)
         The SpatiallyPartitioned<Actor*> specialization implementation
         (the OPT-A8 slab form).

## src/sim/spatially_partitioned.hpp

UPSTREAM: OpenRA.Game/Primitives/SpatiallyPartitioned.cs @1f594e320 L19-168
         (逐语义重写 + OPT-A8 数组化)
         Verbatim-semantics rewrite + the OPT-A8 array-ization.

OPT-A8(docs/OPTIMIZATION_TRACKER.md):上游每 bin 的 Dictionary<T,Rectangle>
与全局 Dictionary<T,Rectangle)(哈希桶 + 装箱)→
  - Actor 键:ActorID 稠密 ⇒ slab 平行数组直引(bounds/present/查询戳按
    ActorID 索引),bin 内 = ActorID 列表;InBox 去重 = "最后查询标记"数组
    (per-query serial,零分配);
  - 其余键(IEffect* 等无稠密整数键):指针哈希 unordered_map + bin 内
    指针列表。
  语义等价:枚举序 = bin 列表插入序(上游 Dictionary 实现序在无删除时同
  为插入序;筛选后的消费序不进入同步哈希 —— 鼠标拾取/渲染收集域)。
  OPT-A8 (docs/OPTIMIZATION_TRACKER.md): upstream's per-bin
  Dictionary<T,Rectangle> plus the global Dictionary become, for dense
  Actor keys, ActorID-indexed slab arrays (bounds/presence/query-stamp by
  id) with per-bin id lists and a "last-query stamp" array for InBox dedup
  (per-query serial, zero allocation); for the remaining keys (IEffect*
  etc., no dense integer key), an unordered_map keyed by pointer plus
  per-bin pointer lists. Semantic equivalence: the enumeration order is
  the bin-list insertion order (upstream's Dictionary implementation order
  is insertion order absent removals; the filtered consumption order never
  enters the sync hash — the mouse-picking/render-collection domain).

## src/sim/sync_hash.cpp

UPSTREAM: OpenRA.Game/Sync.cs @1f594e320(实现部分:注册表 + AssertUnsynced)
         The implementation half of Sync.cs (registries +
         AssertUnsynced).

## src/sim/sync_hash.hpp

UPSTREAM: OpenRA.Game/Sync.cs @1f594e320 L23-211(逐语义重写;反射 Emit 的
         哈希生成协议 → 编译期哈希函数族 + gen/sync_gen.cpp 成员表)
         Verbatim-semantics rewrite; the Reflection.Emit hash generation
         protocol becomes the compile-time hash function family + the
         gen/sync_gen.cpp member table.

机制对照 / Mechanism mapping:
 - C# GenerateHashFunc(Sync.cs L78-107):Reflection.Emit 生成
   "0 XOR hash(f1) XOR hash(f2) …"(GetFields 序先、GetProperties 序后,
   Public|NonPublic|Instance)→ C++ 侧 CombineSyncHash 起始 0 的 XOR 链;
   成员集由 gen/sync_gen.cpp 注册表锚定(Phase 5 手写 trait 按表接线)
   C# GenerateHashFunc emits "0 XOR hash(f1) XOR hash(f2) …" (GetFields
   order first, GetProperties after, Public|NonPublic|Instance) → the
   C++ CombineSyncHash XOR chain seeded with 0; member sets are anchored
   by the gen/sync_gen.cpp registry (Phase 5 hand-written traits wire up
   against the table).
 - bool 字段哈希(EmitSyncOpcodes L63-71):Ldc_I4 0xAAA 是死常量 ——
   Brtrue 弹掉的是刚压入的非零常量(恒跳转),Pop/0x555 不可达;活语义
   = (b ? 1 : 0) → HashBool(第十二批 G2 黄金对拍修正:旧 ^0xAAA 读法
   把死常量当成了活操作数)
   bool member hash (EmitSyncOpcodes L63-71): the Ldc_I4 0xAAA is a dead
   constant — Brtrue pops the just-pushed non-zero constant (the branch
   is unconditional), leaving Pop/0x555 unreachable; the live semantics
   are (b ? 1 : 0) → HashBool (the batch-12 G2 golden differential fix:
   the old ^0xAAA reading mistook the dead constant for a live operand).
 - HashUsingHashCode(WDist/WPos/WVec/WAngle/WRot):C# GetHashCode 为值
   XOR 组合(int.GetHashCode = 原值)→ 下方 HashUsingHashCode 家族
   HashUsingHashCode (WDist/WPos/WVec/WAngle/WRot): the C# GetHashCode
   is a value XOR combination (int.GetHashCode is identity) → the
   HashUsingHashCode family below.
 - RunUnsynced/AssertUnsynced(L166-211):unsyncCount 重入门禁 + 世界哈希
   前后校验;世界指针空判定语义保留(disposing 世界跳过)
   RunUnsynced/AssertUnsynced (L166-211): the unsyncCount reentry gate +
   world-hash before/after check; the null-world semantics are kept
   (disposing worlds skip the check).

## src/sim/target.cpp

UPSTREAM: OpenRA.Game/Traits/Target.cs @1f594e320(实现部分:值语义核心)
         The implementation half of Target.cs (the value-semantics core).

## src/sim/target.hpp

UPSTREAM: OpenRA.Game/Traits/Target.cs @1f594e320 L18-293(仿真核心子集逐语义
         重写;FrozenActor 依赖 Shroud/FrozenActorLayer,Phase 5 落地前以
         前向指针承载 —— 上游 FrozenActorLayer 为 null 时同样落 Invalid 分支)
         Verbatim-semantics rewrite of the sim-core subset; FrozenActor
         depends on Shroud/FrozenActorLayer (Phase 5), carried here as a
         forward-declared pointer — upstream falls into the Invalid branch
         when FrozenActorLayer is null, which stays reachable.

机制对照 / Mechanism mapping:
 - readonly struct + in 传参 → 值类型按值/const 引用传(C++ 结构体自然形态)
   readonly struct + `in` parameters → a value type passed by value /
   const-ref (the natural C++ shape).
 - FromCell(World, CPos, SubCell):依赖 Map.CenterOfSubCell(Phase 5);
   本期不提供,Order 反序列化的 TargetIsCell 分支经 World 桩换算(见
   order.cpp,COVERAGE 登记)
   FromCell(World, CPos, SubCell): depends on Map.CenterOfSubCell
   (Phase 5); not provided this phase — the TargetIsCell branch of order
   deserialization goes through a World stub conversion (order.cpp,
   registered in COVERAGE).
 - IsValidFor/RequiresForceFire/IsInRange:依赖 trait 查询(ISelectable/
   ITargetable 的运行时面,Phase 5 trait 类);本期保留定义,Phase 5 接线
   IsValidFor/RequiresForceFire/IsInRange: depend on trait queries (the
   runtime surface of ISelectable/ITargetable, Phase 5); declared here,
   wired in Phase 5.

## src/sim/trait_dictionary.cpp

UPSTREAM: OpenRA.Game/TraitDictionary.cs @1f594e320(实现部分:容器算术与注册)
         The implementation half (container arithmetic + registration).

## src/sim/trait_dictionary.hpp

UPSTREAM: OpenRA.Game/TraitDictionary.cs @1f594e320 L21-329(逐语义重写)
         Verbatim-semantics rewrite.

机制对照 / Mechanism mapping:
 - Dictionary<Type, ITraitContainer> → std::map<gen::TypeId,
   std::unique_ptr<TraitContainer>>(查询热路径;容器按需懒建 —— GetOrAdd)
 - TraitContainer<T> 平行数组(List<Actor>/List<T> 按 ActorID 有序)+
   BinarySearchMany(actorID+1 插入 = 同 actor 追加尾部;查询起点二分)→
   单一擦除容器(Actor* + void* 平行数组;载荷为键类型子对象地址,由
   AddTrait 的 upcast 表保证 —— 与 C# object 载荷同构,模板查询处
   static_cast 还原)
   The TraitContainer<T> parallel arrays (List<Actor>/List<T> kept sorted
   by ActorID) + BinarySearchMany (an actorID+1 insertion appends after
   same-actor entries; queries bisect the start) → a single erased
   container (parallel Actor* + void* arrays; payloads are key-type
   sub-object addresses guaranteed by AddTrait's upcast table — the same
   shape as the C# object payload, restored via static_cast at the
   template queries).
 - AddTrait 的 GetInterfaces()+BaseTypes() 注册 → 手写 trait 的
   ORA_TRAIT_INTERFACES 上行转换表(静态;upcast lambda 承载多继承偏移)
 - yield 枚举器 → vector 物化(上游枚举器持有 List 引用,遍历中变更会
   反映;调用方约定 = 查询后立即消费,C++ 物化快照语义等价)
   yield enumerators → materialized vectors (upstream enumerators hold
   List references where mid-iteration mutations show through; callers
   consume immediately by convention, so the C++ snapshot is equivalent).

## src/sim/trait_interfaces.hpp

UPSTREAM: OpenRA.Game/Traits/TraitsInterfaces.cs @1f594e320 L29-666
         (仿真核心接口子集逐语义重写;渲染/调色板/UI 接口随 Phase 4/6 落地)
         Verbatim-semantics rewrite of the sim-core interface subset;
         render/palette/UI interfaces land with Phase 4/6.

机制对照 / Mechanism mapping:
 - C# interface 查询键(Type 对象)→ gen::TypeId(gen/interfaces_gen.h 集中
   分配,schema_dumper 导出,手写禁改;PORTING_PLAN §4.1)。每个接口绑定
   kTypeId;TraitDictionary/AddTrait 的 GetInterfaces()+BaseTypes() 注册面
   → 手写 trait 类的 ORA_TRAIT_INTERFACES 上行转换表(编译期生成,
   多继承子对象地址由 static_cast lambda 承载)
   The C# interface query key (a Type object) → gen::TypeId (centrally
   allocated in gen/interfaces_gen.h, exported by schema_dumper, hand
   edits forbidden; PORTING_PLAN §4.1). Every interface binds kTypeId;
   the GetInterfaces()+BaseTypes() registration surface of
   TraitDictionary/AddTrait → the ORA_TRAIT_INTERFACES upcast table of
   hand-written trait classes (generated at compile time; multiple-
   inheritance sub-object addresses carried by static_cast lambdas).
 - delegate VariableObserverNotifier → std::function(规范:严禁函数指针);
   IReadOnlyDictionary<string,int> → const std::map<std::string,int>&
   (SortedDictionary→std::map 的既有映射;上游此处是只读视图)
   delegate VariableObserverNotifier → std::function (spec: function
   pointers forbidden); IReadOnlyDictionary<string,int> →
   const std::map<std::string,int>& (the established
   SortedDictionary→std::map mapping; upstream passes a read-only view).

## src/sim/trait_registry.cpp

UPSTREAM: OpenRA.Game/ObjectCreator.cs @1f594e320(trait_registry.hpp 的实现)
         The implementation of trait_registry.hpp.

## src/sim/trait_registry.hpp

UPSTREAM: OpenRA.Game/ObjectCreator.cs @1f594e320 L78-137(按名实例化)+
         TraitInfo.Create(ActorInitializer) 的分发面(PORTING_PLAN D26/D27
         的 Phase 5 接线)
         The name-keyed instantiation of ObjectCreator + the dispatch face
         of TraitInfo.Create — the D26/D27 Phase 5 wiring.

机制对照 / Mechanism mapping:
 - 上游 traitInfo.Create(init) 是 Info 类的虚方法;C++ 侧 Info 走
   meta::GeneratedRecord 值袋(无方法),故以"注册表 + 登记函数"承载:
   Info 类型名 → 工厂(RecordObject&, ActorInitializer&, WorldArena&) →
   TraitBase*(World arena 内构造 —— 所有权随 D26/D27 移入 arena)
   Upstream's traitInfo.Create(init) is virtual on the Info class; the
   C++ Info rides the GeneratedRecord value bag (method-free), so a
   registry + a registration function carries it: the Info type name → a
   factory (RecordObject&, ActorInitializer&, WorldArena&) → TraitBase*
   (constructed inside the World arena — ownership moved there per
   D26/D27).
 - 未知名异常 = 上游 ObjectCreator.FindType null 的
   "Cannot locate type: {name}" 文本逐字
   An unknown name throws "Cannot locate type: {name}" verbatim.
 - 生成类型无行为 → 值袋没有 Create 方法体;工厂即该 Info 的 Create 语义
   (字段读取经按名槽位辅助)
   Generated types carry no behaviour → the bag has no Create body; the
   factory IS that Info's Create semantics (field reads through the
   by-name slot helpers).

## src/sim/type_dictionary.cpp

UPSTREAM: OpenRA.Game/Primitives/TypeDictionary.cs @1f594e320(实现部分)
         The implementation half of TypeDictionary.cs.

## src/sim/type_dictionary.hpp

UPSTREAM: OpenRA.Game/Primitives/TypeDictionary.cs @1f594e320 L19-183(逐语义
         重写;Type 键 → gen::TypeId,泛型容器 → TypeId 桶 + void* 载荷)
         Verbatim-semantics rewrite; the Type key becomes gen::TypeId and
         the generic container a TypeId-bucketed void* store.

机制对照 / Mechanism mapping:
 - Dictionary<Type, TypeContainer<T>> → std::map<gen::TypeId,
   std::vector<void*>>(插入序保持 C# List 语义)
 - Add(val):GetInterfaces()+BaseTypes() 全注册 → Add(val, type_ids)
   (调用方 —— ActorInit 子类 —— 经 ORA_INIT_TYPE 静态声明注册面)
 - Get<T>/GetOrDefault<T>/WithInterface<T>/Remove<T>:异常消息逐字
   (typeof(T) 形态 → T 的全名,经 gen::kTypeFullNames 反查)

## src/sim/weapons.cpp

UPSTREAM: OpenRA.Game/GameRules/WeaponInfo.cs @1f594e320 L178-268(仿真面)+
         OpenRA.Game/Effects/DelayedImpact.cs L15-43 +
         OpenRA.Mods.Common/Warheads/Warhead.cs L59-96(IsValidAgainst 面)
         The sim face of WeaponInfo.cs L178-268 + DelayedImpact.cs
         L15-43 + Warhead.cs L59-96 (the IsValidAgainst faces).

## src/sim/weapons.hpp

UPSTREAM: OpenRA.Game/GameRules/WeaponInfo.cs @1f594e320 L21-72(ProjectileArgs/
         WarheadArgs/IProjectile/IProjectileInfo 的承载面)+
         L178-268 的仿真面方法(IsValidTarget/IsValidAgainst/Impact)+
         OpenRA.Mods.Common/Warheads/Warhead.cs L22-106(Warhead 基类)+
         OpenRA.Game/Effects/DelayedImpact.cs L15-43
         The carrier faces of WeaponInfo.cs L21-72 (ProjectileArgs/
         WarheadArgs/IProjectile/IProjectileInfo) + the sim-domain
         methods of L178-268 (IsValidTarget/IsValidAgainst/Impact) +
         Warheads/Warhead.cs L22-106 (the Warhead base) +
         Effects/DelayedImpact.cs L15-43.

机制对照 / Mechanism mapping:
 - 上游 WeaponInfo 是引擎类,加载链字段已在 game/game_records.hpp 的值袋
   (ora::game::WeaponInfo);仿真面方法在此以自由函数承载(首参 = 值袋)
   —— 值袋不可加成员,分层即"加载面在 game、仿真面在 sim"
   Upstream's WeaponInfo is an engine class whose loading-chain fields
   already live in the value bag at game/game_records.hpp
   (ora::game::WeaponInfo); the sim-domain methods ride free functions
   here (first parameter = the bag) — the bag cannot grow members, so
   the layering is "loading face in game, sim face in sim".
 - IProjectileInfo/IWarhead:上游 Game.CreateObject("{值}Info"/"{值}
   Warhead") 工厂;C++ = 值袋记录 + 名字注册表(记录名 → 工厂 → 构造)。
   本批零注册(Bullet/Missile/DamageWarhead 等随武器批)—— 未注册 →
   nullptr(上游 ObjectCreator 未注册类型同为 null;部分覆盖装配面,
   COVERAGE 登记)。解析产物为无状态数据对象,进程级缓存按记录指针
   一记录一构造(确定性;非 world 生命周期 —— 与上游 WeaponInfo 的
   ruleset 生命周期同阶)
   IProjectileInfo/IWarhead: upstream's Game.CreateObject("{value}Info"/
   "{value}Warhead") factories; C++ = the value-bag record + the
   name-keyed registry (record name → factory → construct). Zero
   registrations this batch (Bullet/Missile/DamageWarhead & co. arrive
   with the weapons batch) — unregistered → nullptr (upstream's
   ObjectCreator is likewise null for unregistered types; the
   partial-coverage assembly is registered in COVERAGE). The resolved
   products are stateless data objects with a process-level
   one-record-one-construct cache keyed by the record pointer
   (deterministic; not world lifetime — the same order as upstream's
   ruleset-lifetime WeaponInfo).
 - WarheadArgs 的 int[] 快照 → std::vector<int> 拷贝(上游 FireBarrel 每
   发 ToArray() 快照,引用别名不可观测;OPT-A9 的零分配落在 CheckFire
   热循环的修正链收集(armament.hpp),延迟弹着面保持快照形态)
   WarheadArgs's int[] snapshot → a std::vector<int> copy (upstream's
   FireBarrel snapshots with ToArray() per shot, so the array aliasing
   is unobservable; OPT-A9's zero allocation lands in the CheckFire
   hot-loop modifier collection (armament.hpp), while the delayed-impact
   face keeps the snapshot shape).

## src/sim/world.cpp

UPSTREAM: OpenRA.Game/World.cs @1f594e320 L28-627(world.hpp 的实现;全量
         构造链 + trait arena + 系统 trait 面)
         The implementation of world.hpp — the full construction chain,
         the trait arena, and the system-trait faces.

## src/sim/world.hpp

UPSTREAM: OpenRA.Game/World.cs @1f594e320 L28-650(逐语义重写;gameInfo/
ReplayMetadata/存档包面随 Phase 7 批)
         Verbatim-semantics rewrite (gameInfo/ReplayMetadata/the save
         package faces land with the Phase 7 batches).

机制对照 / Mechanism mapping:
 - actors: SortedDictionary<uint, Actor> → std::map<uint32_t, Actor*>
   (遍历序 = ActorID 升序,确定性关键)
   actors: SortedDictionary → std::map (iteration order = ascending
   ActorID, determinism-critical).
 - trait 对象所有权:D26/D27 —— 上游 GC;C++ = WorldArena(§4.5 每局世界
   区;bump + 逆序析构登记)。actor 的 Dispose 只摘字典引用,对象内存随
   World 析构的 arena.Reset 统一蒸发;FrameArena 之外的字段(条件系统/
   std::function)由登记表逆序析构
   Trait object ownership (D26/D27): upstream GC; C++ = the WorldArena
   (§4.5; bump + reverse-order destructor registration). An actor's
   Dispose only drops dictionary references — the memory evaporates with
   arena.Reset when the World dies.
 - ScreenMap/Selection/ControlGroups/OrderValidators/notifyDisconnected:
   WorldActor.Trait<>() 的构造期解析(全量 ctor;测试 ctor 保持注入面)
   Resolved at construction from the WorldActor (the full ctor; the test
   ctor keeps the injection faces).
 - IActorMap:ActorMap trait(Mods.Common)随下一批 —— 本批解析面留空位
   (AddToMaps/UpdateMaps/RemoveFromMaps 的 ActorMap 半段以空判跳过;
   COVERAGE 登记)
   IActorMap: the ActorMap trait lands next batch — the resolution face
   is left empty this batch (the ActorMap half of AddToMaps/... skips on
   null; in COVERAGE).
 - SyncHash(L482-512):公式逐项(n 计数器跨段连续;乘法回绕 -fwrapv)
   SyncHash (L482-512): the formula term by term.

## src/sound/sound.cpp

UPSTREAM: OpenRA.Game/Sound/Sound.cs @1f594e320 L36-481(sound.hpp 的实现)
         Implementation of sound.hpp.
形态偏离(COVERAGE 登记):文件缺失日志(Log.Write "sound")不落地(同
D70 的日志面);sounds[name] 为 null 源时上游 Play2D(null) 即 NRE,此处
以同文本 runtime_error 等价抛;LoadSound 的 "not a valid sound file!"
逐字。
Registered shape deviations: the missing-file log (Log.Write "sound") is
not ported (D70's log face); a null sounds[name] hits upstream's
Play2D(null) NRE, thrown here as a runtime_error with the same text;
LoadSound's "not a valid sound file!" verbatim.

## src/sound/sound.hpp

UPSTREAM: OpenRA.Game/Sound/Sound.cs @1f594e320 L36-481(全文逐语义)
声音门面:格式链 + 源缓存 + 调度(Play 族/PlayPredefined 通知池)、音乐
与视频声道、音量面。Game.Settings/Game.CosmeticRandom/Selection 以注入
面承载(主循环 Phase 5 接线)。
The sound facade: the format chain + the source cache + scheduling (the
Play family / PlayPredefined's notification pools), the music and video
channels, and the volume faces. Game.Settings / Game.CosmeticRandom /
Selection arrive as injection faces (the main loop wires them in Phase 5).

## src/sound/sound_loader.cpp

UPSTREAM: OpenRA.Game/Sound/Sound.cs @1f594e320 L22-34 + OpenRA.Game/
         ObjectCreator.cs @1f594e320 L149-156 + 五个 AudioLoaders
         (sound_loader.hpp 的实现)
         Implementation of sound_loader.hpp.

## src/sound/sound_loader.hpp

UPSTREAM: OpenRA.Game/Sound/Sound.cs @1f594e320 L22-34(ISoundLoader/
         ISoundFormat)+
          OpenRA.Game/ObjectCreator.cs @1f594e320 L149-156(GetLoader 名分派)+
          OpenRA.Mods.{Cnc,Common}/AudioLoaders/*.cs(五格式名)
声音格式链:名字分派表(Manifest.SoundFormats,如 "Aud, Wav")+
TryParseSound 的统一签名(嗅探 + 元数据 + 整段 PCM 物化 = D74 形态)。
The sound-format chain: the name-dispatch table (Manifest.SoundFormats,
e.g. "Aud, Wav") plus the unified TryParseSound signature (sniff +
metadata + the whole-PCM materialization = the D74 shape).

## src/terrain/map_grid.cpp

UPSTREAM: OpenRA.Game/Map/MapGrid.cs @1f594e320(map_grid.hpp 的实现)
         The implementation of map_grid.hpp.

## src/terrain/map_grid.hpp

UPSTREAM: OpenRA.Game/Map/MapGrid.cs @1f594e320 L20-256(逐语义重写)
         Verbatim-semantics rewrite.

机制对照 / Mechanism mapping:
 - CellRamp 的 ImmutableArray → 固定 std::array;HeightOffset 的
   "do-while 多边形三角化"逐控制流(含未劈分时取前三顶点)
   CellRamp's ImmutableArray becomes fixed std::arrays; HeightOffset's
   "do-while polygon triangulation" keeps the control flow (including
   the unsplit three-first-vertices case).
 - MapGrid 字段加载(FieldLoader.Load)→ 手写字段表(Name/Type/MaximumTerrain
   Height/DefaultSubCell/MaximumTileSearchRange/EnableDepthBuffer;
   SubCellOffsets/Ramps 为常量,上游 [FieldLoader.Ignore] 语义)
   MapGrid's FieldLoader.Load becomes a hand-written field table
   (SubCellOffsets/Ramps are constants — the upstream [FieldLoader.Ignore]
   semantics).

## src/terrain/terrain_info.cpp

UPSTREAM: OpenRA.Game/Map/TerrainInfo.cs @1f594e320 + OpenRA.Mods.Common/
         Terrain/{TerrainInfo,DefaultTerrain}.cs @1f594e320(terrain_info.hpp
         的实现) | The implementation of terrain_info.hpp.

## src/terrain/terrain_info.hpp

UPSTREAM: OpenRA.Game/Map/TerrainInfo.cs @1f594e320 L22-198(逐语义重写)+
         OpenRA.Mods.Common/Terrain/TerrainInfo.cs L31-120(TerrainTemplateInfo)
         + OpenRA.Mods.Common/Terrain/DefaultTerrain.cs L26-262(DefaultTerrain)
         Verbatim-semantics rewrite.

已登记偏离(docs/COVERAGE.md,第十六批)/ Registered deviations (batch 16):
 - DefaultTerrain 的 MultiBrushCollections(地图生成域)与 DumpSheets
   (Utility 命令域)不解析 —— 本批无消费点
   DefaultTerrain's MultiBrushCollections (map-generation domain) and
   DumpSheets (the Utility-command domain) stay unparsed — no call sites
   in this batch.

## src/terrain/tile_reference.hpp

UPSTREAM: OpenRA.Game/Map/TileReference.cs @1f594e320 L18-51(逐语义重写)
         Verbatim-semantics rewrite.

## src/ui/chrome_metrics.cpp

UPSTREAM: OpenRA.Game/Widgets/ChromeMetrics.cs @1f594e320(chrome_metrics.hpp
的实现;Get<T> 的标量域经 meta GetValue,类型名进错误文本)
The implementation of chrome_metrics.hpp (Get<T>'s scalar domain through
meta GetValue; the type names enter the error texts).

## src/ui/chrome_metrics.hpp

UPSTREAM: OpenRA.Game/Widgets/ChromeMetrics.cs @1f594e320(chrome.yaml 指标表
逐语义:Merge 全部 ChromeMetrics 源 → 键值覆盖)。C# static → 实例类
(ChromeProvider 先例);Get<T> 的 T 域收敛为 string(引擎核心唯一消费
形态,泛型面随消费批次扩展);Dictionary → 插入序条目表(索引写覆盖原位)。
Verbatim-semantics rewrite of the chrome.yaml metrics table (merging every
ChromeMetrics source → key/value overwrite). C#'s static becomes an
instance class (the ChromeProvider precedent); Get<T>'s T domain narrows
to string (the engine core's only consumer shape; the generic face
extends with its consumer batches); the Dictionary becomes an
insertion-ordered entry list (indexer writes overwrite in place).

## src/ui/hotkey.cpp

UPSTREAM: OpenRA.Game/Input/Hotkey.cs + HotkeyReference.cs +
         HotkeyDefinition.cs + HotkeyManager.cs + Keycode.cs(KeycodeExts)+
         IInputHandler.cs(ModifiersExts) @1f594e320(hotkey.hpp 的实现;
         键名/fluent 名表 = platform/keycode.hpp 枚举体 × 上游
         KeycodeFluentKeys,238 项同源生成)
The implementation of hotkey.hpp (the key-name/fluent tables generated
from the platform/keycode.hpp enum body × the upstream KeycodeFluentKeys,
238 entries from the same source).

## src/ui/hotkey.hpp

UPSTREAM: OpenRA.Game/Input/Hotkey.cs @1f594e320 L20-101(全文)+
         HotkeyReference.cs L15-46(全文)+ HotkeyDefinition.cs L20-66(全文)
         + HotkeyManager.cs L20-116(核心面)+ Keycode.cs L259-511
         (KeycodeExts.KeycodeFluentKeys/DisplayString)+ IInputHandler.cs
         L53-77(ModifiersExts)。
Hotkey 值语义(UNKNOWN 永不相等/TryParse 的 ' ' 双段解析/DisplayString
的 Shift+Alt+Ctrl+Meta 前缀序)与 HotkeyReference 的惰性取值/IsActivatedBy
逐行照抄;HotkeyManager 的 definitions/keys 双表 + GetFirstDuplicate 的
Contexts 交集判 + Set 的重算面保真,settings 持久化节点(HotkeySettings)
为注入面(缺省空 = 全部默认键)。Enum.TryParse 的名字 ↔ 值经
Build/keycode_names.txt 生成的 238 项名表(与 keycode.hpp 枚举体同源)。
The Hotkey value semantics (UNKNOWN never equal / TryParse's two-segment
' ' parse / DisplayString's Shift+Alt+Ctrl+Meta prefix order) and
HotkeyReference's lazy get/IsActivatedBy copied line for line;
HotkeyManager's definitions/keys dual table + GetFirstDuplicate's
Contexts-intersection test + Set's recompute kept, the settings
persistence nodes (HotkeySettings) an injected face (defaulting empty =
all default keys). Enum.TryParse's name ↔ value goes through the
238-entry name table generated from Build/keycode_names.txt (same source
as the keycode.hpp enum body).

## src/ui/logic/button_tooltip_logic.cpp

UPSTREAM: OpenRA.Mods.Common/Widgets/Logic/ButtonTooltipLogic.cs @1f594e320 L17-68(全文)。

## src/ui/logic/button_tooltip_logic.hpp

UPSTREAM: OpenRA.Mods.Common/Widgets/Logic/ButtonTooltipLogic.cs @1f594e320 L17-68(全文)。

## src/ui/logic/color_picker_logic.cpp

UPSTREAM: OpenRA.Mods.Common/Widgets/Logic/ColorPickerLogic.cs @1f594e320
(color_picker_logic.hpp 的实现)
The implementation of color_picker_logic.hpp.

## src/ui/logic/color_picker_logic.hpp

UPSTREAM: OpenRA.Mods.Common/Widgets/Logic/ColorPickerLogic.cs @1f594e320 L22-191(全文)。

## src/ui/logic/credits_logic.cpp

UPSTREAM: OpenRA.Mods.Common/Widgets/Logic/CreditsLogic.cs @1f594e320(实现)。

## src/ui/logic/credits_logic.hpp

UPSTREAM: OpenRA.Mods.Common/Widgets/Logic/CreditsLogic.cs @1f594e320 L18-97(全文)。
形态适配:上游 Game.Renderer.Fonts 的局部 font 变量在 ctor 中未被消费,
不复刻;DefaultFileSystem(引擎级挂载面)以 ModFiles 代偿(登记 COVERAGE)。

## src/ui/logic/perf_debug_logic.cpp

UPSTREAM: OpenRA.Mods.Common/Widgets/Logic/PerfDebugLogic.cs @1f594e320(实现)。

## src/ui/logic/perf_debug_logic.hpp

UPSTREAM: OpenRA.Mods.Common/Widgets/Logic/PerfDebugLogic.cs @1f594e320 L21-81(全文)。
形态适配:Game.AfterGameStart 的静态事件 → Ui::Deps 注入的订阅面;
Game.RenderFrame → fn_render_frame;析构退订 = 上游 Dispose 的 C++ 域。

## src/ui/logic/simple_tooltip_logic.cpp

UPSTREAM: OpenRA.Mods.Common/Widgets/Logic/SimpleTooltipLogic.cs @1f594e320
(simple_tooltip_logic.hpp 的实现)
The implementation of simple_tooltip_logic.hpp.

## src/ui/logic/simple_tooltip_logic.hpp

UPSTREAM: OpenRA.Mods.Common/Widgets/Logic/SimpleTooltipLogic.cs @1f594e320
L15-61(全文)。LABEL 模板 + LINE_HEIGHT 间距数据节点 + BeforeRender 的
cachedText 门控重建:文本变更时逐行克隆 Label 定宽排布并自适应 widget
尺寸(horizontalPadding = LABEL 宽 - widget 宽,非正回落 2×LABEL.X)。
The whole of SimpleTooltipLogic.cs L15-61. The LABEL template + the
LINE_HEIGHT spacing data node + BeforeRender's cachedText-gated rebuild:
on a text change, Label clones are laid out one per line at the measured
width and the widget resizes (horizontalPadding = LABEL width - widget
width, falling back to 2×LABEL.X when non-positive).

## src/ui/logic/version_label_logic.cpp

UPSTREAM: OpenRA.Mods.Common/Widgets/Logic/VersionLabelLogic.cs @1f594e320 L17-25(全文)。

## src/ui/logic/version_label_logic.hpp

UPSTREAM: OpenRA.Mods.Common/Widgets/Logic/VersionLabelLogic.cs @1f594e320 L17-25(全文)。

## src/ui/widget.cpp

UPSTREAM: OpenRA.Game/Widgets/Widget.cs @1f594e320(widget.hpp 的实现,方法体
逐行对照)
The implementation of widget.hpp (method bodies line-checked).

## src/ui/widget_loader.cpp

UPSTREAM: OpenRA.Game/Widgets/WidgetLoader.cs @1f594e320(widget_loader.hpp 的
实现;基型 Container/Input 就地注册)
The implementation of widget_loader.hpp (the Container/Input base types
registered in place).

## src/ui/widget_loader.hpp

UPSTREAM: OpenRA.Game/Widgets/WidgetLoader.cs @1f594e320(chrome layout 加载
逐语义:首 '@' 后键索引/重复键抛/Id 取 Split('@')[1]/字段加载/Initialize/
Children 递归/logicArgs 出入参/PostInit)。ObjectCreator.CreateObject →
WidgetTypeRegistry 名字分派(键 = "{Type}Widget";未知名 "Cannot locate
type: {name}" 逐字);根 widget 之外的依赖(FileSystem/ChromeLayout)注入。
Verbatim-semantics rewrite of the chrome-layout loader (the after-first-
'@' key index / the duplicate-key throw / Id from Split('@')[1] / field
loading / Initialize / the Children recursion / the logicArgs set-remove
/ PostInit). ObjectCreator.CreateObject becomes the WidgetTypeRegistry
name dispatch (keyed "{Type}Widget"; unknown names throw "Cannot locate
type: {name}" verbatim); the FileSystem/ChromeLayout dependencies are
injected.

## src/ui/widget_utils.cpp

UPSTREAM: OpenRA.Mods.Common/Widgets/WidgetUtils.cs @1f594e320(widget_utils.hpp
的实现)
The implementation of widget_utils.hpp.

## src/ui/widget_utils.hpp

UPSTREAM: OpenRA.Mods.Common/Widgets/WidgetUtils.cs @1f594e320 L20-352(基础
渲染/格式化子集;WithSuffix/BindPlayerNameAndStatus/SetupTextNotification
的会话与通知面板绑定面随其消费批)。Game.Renderer/ChromeProvider 静态 →
Ui::Instance() 注入承载(D 注册);CachedTransform → core/cached_transform。
The base rendering/formatting subset of WidgetUtils.cs L20-352 (the
session and notification-panel binding faces — WithSuffix/
BindPlayerNameAndStatus/SetupTextNotification — await their consumer
batches). Game.Renderer/ChromeProvider statics become the Ui::Instance()
injected carrier (registered); CachedTransform becomes
core/cached_transform.

## src/ui/widgets/background_widget.cpp

UPSTREAM: OpenRA.Mods.Common/Widgets/BackgroundWidget.cs @1f594e320
(background_widget.hpp 的实现)
The implementation of background_widget.hpp.

## src/ui/widgets/background_widget.hpp

UPSTREAM: OpenRA.Mods.Common/Widgets/BackgroundWidget.cs @1f594e320 L18-43(全文)。
The whole of BackgroundWidget.cs L18-43.

## src/ui/widgets/button_widget.cpp

UPSTREAM: OpenRA.Mods.Common/Widgets/ButtonWidget.cs @1f594e320(button_widget.hpp
的实现)
The implementation of button_widget.hpp.

## src/ui/widgets/button_widget.hpp

UPSTREAM: OpenRA.Mods.Common/Widgets/ButtonWidget.cs @1f594e320 L20-298(全文)。
输入三态(Depressed/双击/键重复门)/键激活(HotkeyReference)/Tooltip 容器
懒查/GetTextPosition 三对齐/DrawBackground 静态九宫格。Game.Sound/
Game.Renderer.Fonts/Ui.MouseOverWidget → Ui::Instance() 注入承载;
ModData.DefaultRules → WidgetArgs "modData"。
The whole of ButtonWidget.cs L20-298. The input trio (Depressed /
double-click / the key-repeat gate) / key activation (HotkeyReference) /
the tooltip container's lazy lookup / GetTextPosition's three alignments /
the static nine-slice DrawBackground. Game.Sound and
Game.Renderer.Fonts and Ui.MouseOverWidget go through the Ui::Instance()
injected carrier; ModData.DefaultRules through WidgetArgs "modData".

## src/ui/widgets/checkbox_widget.cpp

UPSTREAM: OpenRA.Mods.Common/Widgets/CheckboxWidget.cs @1f594e320
(checkbox_widget.hpp 的实现)
The implementation of checkbox_widget.hpp.

## src/ui/widgets/checkbox_widget.hpp

UPSTREAM: OpenRA.Mods.Common/Widgets/CheckboxWidget.cs @1f594e320 L19-91(全文)。
Background 以新字段隐藏 ButtonWidget.Background(C# new);双层
CachedTransform((CheckType,Checked) → 状态五元 → Sprite);文本色对来自
chrome metrics 的 TextColor/TextDisabledColor。
The whole of CheckboxWidget.cs L19-91. Background hides
ButtonWidget.Background with a new field (C# new); the two-layer
CachedTransform ((CheckType,Checked) → the five-state tuple → Sprite); the
text-color pair comes from the chrome metrics' TextColor/TextDisabledColor.

## src/ui/widgets/client_tooltip_region_widget.cpp

UPSTREAM: OpenRA.Mods.Common/Widgets/ClientTooltipRegionWidget.cs @1f594e320 L20-76(全文)。

## src/ui/widgets/client_tooltip_region_widget.hpp

UPSTREAM: OpenRA.Mods.Common/Widgets/ClientTooltipRegionWidget.cs @1f594e320 L20-76(全文)。

## src/ui/widgets/color_block_widget.cpp

UPSTREAM: OpenRA.Mods.Common/Widgets/ColorBlockWidget.cs + Gradient-
ColorBlockWidget.cs @1f594e320(color_block_widget.hpp 的实现)
The implementation of color_block_widget.hpp.

## src/ui/widgets/color_block_widget.hpp

UPSTREAM: OpenRA.Mods.Common/Widgets/ColorBlockWidget.cs @1f594e320 L18-80(全文)
+ GradientColorBlockWidget.cs L18-57(全文)。ColorBlock 的输入三态
(Down 持焦/Up 让焦触发)与 ClickSound 通知;Gradient 的四角色 FillRect。
ModData.DefaultRules → WidgetArgs "modData";Game.Sound → Ui 注入。
The whole of ColorBlockWidget.cs L18-80 + GradientColorBlockWidget.cs
L18-57. ColorBlock's input trio (Down taking focus / Up yielding focus to
fire) with the ClickSound notification; Gradient's four-color FillRect.
ModData.DefaultRules through WidgetArgs "modData"; Game.Sound through the
Ui injection.

## src/ui/widgets/color_mixer_widget.cpp

UPSTREAM: OpenRA.Mods.Common/Widgets/ColorMixerWidget.cs @1f594e320
(color_mixer_widget.hpp 的实现)
The implementation of color_mixer_widget.hpp.

## src/ui/widgets/color_mixer_widget.hpp

UPSTREAM: OpenRA.Mods.Common/Widgets/ColorMixerWidget.cs @1f594e320 L18-189(全文)。
OnChange 多播 event → vector + 调用列表快照(D262 同族);Sheet 的 RAII
持有(上游手动 Dispose → Removed 释放);modData 经 WidgetArgs。
The whole of ColorMixerWidget.cs L18-189. The OnChange multicast event
becomes a vector + invocation-list snapshot (the D262 family); the Sheet
is RAII-owned (upstream's manual Dispose → Released in Removed());
modData through WidgetArgs.

## src/ui/widgets/drop_down_button_widget.cpp

UPSTREAM: OpenRA.Mods.Common/Widgets/DropDownButtonWidget.cs @1f594e320
(drop_down_button_widget.hpp 的实现)
The implementation of drop_down_button_widget.hpp.

## src/ui/widgets/drop_down_button_widget.hpp

UPSTREAM: OpenRA.Mods.Common/Widgets/DropDownButtonWidget.cs @1f594e320
L20-257(全文)。开面板三件套(AttachPanel 的全屏 MaskWidget + 边缘回退
定位/RemovePanel 的退焦点与 tooltip 重置/ShowDropDown 双模板 —— 平面与
分组头部形态);箭头与分隔条的状态图缓存(??= 惰性初建 → optional);
UsableWidth = Width - Height(上游 "This is crap" 注释照抄)。上游 clone
不拷 PanelAlign(保护拷贝 ctor 的遗漏 quirk)保真。Game.Renderer.
Resolution/Game.Sound/Ui.Root/Ui.LoadWidget → Ui::Instance() 注入承载。
The whole of DropDownButtonWidget.cs L20-257. The open-panel trio
(AttachPanel's fullscreen MaskWidget + the edge-fallback positioning /
RemovePanel's focus yield + tooltip reset / ShowDropDown's two templates —
the flat and the grouped-header shapes); the marker and separator
stateful-image caches (??= lazy init → optional); UsableWidth = Width -
Height (upstream's "This is crap" comment kept). Upstream's clone not
copying PanelAlign (the protected copy-ctor's omission quirk) is kept
faithful. Game.Renderer.Resolution/Game.Sound/Ui.Root/Ui.LoadWidget go
through the Ui::Instance() injected carrier.

## src/ui/widgets/exponential_slider_widget.cpp

UPSTREAM: OpenRA.Mods.Common/Widgets/ExponentialSliderWidget.cs @1f594e320 L17-50(全文)。

## src/ui/widgets/exponential_slider_widget.hpp

UPSTREAM: OpenRA.Mods.Common/Widgets/ExponentialSliderWidget.cs @1f594e320 L17-50(全文)。

## src/ui/widgets/generated_map_preview_widget.cpp

UPSTREAM: OpenRA.Mods.Common/Widgets/GeneratedMapPreviewWidget.cs @1f594e320
L24-168(全文;自有 Sheet 的预览位图 + 出生点标签对)。

## src/ui/widgets/generated_map_preview_widget.hpp

UPSTREAM: OpenRA.Mods.Common/Widgets/GeneratedMapPreviewWidget.cs @1f594e320
L24-168(全文;自有 Sheet 的预览位图 + 出生点标签对)。

## src/ui/widgets/hotkey_entry_widget.cpp

UPSTREAM: OpenRA.Mods.Common/Widgets/HotkeyEntryWidget.cs @1f594e320 L19-162(全文;
键捕获 + 15-tick blink + 三色态)。

## src/ui/widgets/hotkey_entry_widget.hpp

UPSTREAM: OpenRA.Mods.Common/Widgets/HotkeyEntryWidget.cs @1f594e320 L19-162(全文;
键捕获 + 15-tick blink + 三色态)。

## src/ui/widgets/hue_slider_widget.cpp

UPSTREAM: OpenRA.Mods.Common/Widgets/HueSliderWidget.cs @1f594e320
(hue_slider_widget.hpp 的实现)
The implementation of hue_slider_widget.hpp.

## src/ui/widgets/hue_slider_widget.hpp

UPSTREAM: OpenRA.Mods.Common/Widgets/HueSliderWidget.cs @1f594e320 L18-75(全文)。

## src/ui/widgets/image_widget.cpp

UPSTREAM: OpenRA.Mods.Common/Widgets/ImageWidget.cs + SpriteWidget.cs
@1f594e320(image_widget.hpp 的实现)
The implementation of image_widget.hpp.

## src/ui/widgets/image_widget.hpp

UPSTREAM: OpenRA.Mods.Common/Widgets/ImageWidget.cs @1f594e320 L20-98(全文)
+ SpriteWidget.cs L18-83(全文)。Image = ChromeProvider 图像缓存 + ClickThrough
输入门;Sprite = 调色板/缩放的 WorldRenderer 绘制(缓存三态 + 居中偏移)。
Game.Renderer/ChromeProvider/Ui.Root → Ui 注入;WorldRenderer ctor 形参 →
WidgetArgs "worldRenderer"。
The whole of ImageWidget.cs L20-98 + SpriteWidget.cs L18-83. Image = the
ChromeProvider image cache + the ClickThrough input gate; Sprite = the
palette/scaled WorldRenderer draw (the cached trio + the centered offset).
Game.Renderer/ChromeProvider/Ui.Root through the Ui injection; the
WorldRenderer ctor parameter through WidgetArgs "worldRenderer".

## src/ui/widgets/label_for_input_widget.cpp

UPSTREAM: OpenRA.Mods.Common/Widgets/LabelForInputWidget.cs @1f594e320 L20-48(全文)。

## src/ui/widgets/label_for_input_widget.hpp

UPSTREAM: OpenRA.Mods.Common/Widgets/LabelForInputWidget.cs @1f594e320 L20-48(全文)。

## src/ui/widgets/label_widget.cpp

UPSTREAM: OpenRA.Mods.Common/Widgets/LabelWidget.cs @1f594e320(label_widget.hpp
的实现)
The implementation of label_widget.hpp.

## src/ui/widgets/label_widget.hpp

UPSTREAM: OpenRA.Mods.Common/Widgets/LabelWidget.cs @1f594e320 L20-133(全文)。
Text 为 Fluent 键(CachedTransform 空串/空值直通);字体经 Game.Renderer.
Fonts 注入面(缺名抛 ArgumentException 文本逐字);WordWrap/三态对齐/
IncreaseHeightToFitCurrentText 保真;DrawInner 虚面供 LabelWithTooltip 等
子类(随其批)。ModData ctor 形参 → WidgetArgs 注入(ObjectCreator 等价)。
The whole of LabelWidget.cs L20-133. Text is a Fluent key (CachedTransform
passing empty through); the font goes through the Game.Renderer.Fonts
injected face (a miss throws the ArgumentException text verbatim);
WordWrap / the three-state alignment / IncreaseHeightToFitCurrentText kept;
the DrawInner virtual awaits the LabelWithTooltip subclasses. The ModData
ctor parameter becomes the WidgetArgs injection (the ObjectCreator
equivalent).

## src/ui/widgets/label_with_highlight_widget.cpp

UPSTREAM: OpenRA.Mods.Common/Widgets/LabelWithHighlightWidget.cs @1f594e320 L20-86(全文)。

## src/ui/widgets/label_with_highlight_widget.hpp

UPSTREAM: OpenRA.Mods.Common/Widgets/LabelWithHighlightWidget.cs @1f594e320 L20-86(全文)。

## src/ui/widgets/label_with_tooltip_widget.cpp

UPSTREAM: OpenRA.Mods.Common/Widgets/LabelWithTooltipWidget.cs @1f594e320
(label_with_tooltip_widget.hpp 的实现)
The implementation of label_with_tooltip_widget.hpp.

## src/ui/widgets/label_with_tooltip_widget.hpp

UPSTREAM: OpenRA.Mods.Common/Widgets/LabelWithTooltipWidget.cs @1f594e320
L20-64(全文)。Tooltip 容器的 Lazy 解析(Ui.Root.Get<TooltipContainer
Widget>)挂 MouseEntered/MouseExited 对;MouseExited 仅在容器已物化时才
RemoveTooltip(上游 IsValueCreated 门,防 widget 与容器同批移除的崩溃)。
C# Lazy → 指针 + 物化标志。
The whole of LabelWithTooltipWidget.cs L20-64. The tooltip container's
Lazy resolution (Ui.Root.Get<TooltipContainerWidget>) hangs on the
MouseEntered/MouseExited pair; MouseExited removes the tooltip only when
the container has materialized (upstream's IsValueCreated gate, guarding
the widget-and-container-removed-together crash). C#'s Lazy becomes a
pointer + a materialized flag.

## src/ui/widgets/logic_key_listener_widget.cpp

UPSTREAM: OpenRA.Mods.Common/Widgets/LogicKeyListenerWidget.cs @1f594e320 L19-36(全文)。

## src/ui/widgets/logic_key_listener_widget.hpp

UPSTREAM: OpenRA.Mods.Common/Widgets/LogicKeyListenerWidget.cs @1f594e320 L19-36(全文)。

## src/ui/widgets/logic_ticker_widget.hpp

UPSTREAM: OpenRA.Mods.Common/Widgets/LogicTickerWidget.cs @1f594e320 L19-22(全文)。

## src/ui/widgets/map_preview_widget.cpp

UPSTREAM: OpenRA.Mods.Common/Widgets/MapPreviewWidget.cs @1f594e320 L22-234(全文;
SpawnOccupant 三 ctor + 缩放布局/出生点覆盖/禁用面)。minimap 柄 =
map::MapPreview::GetMinimap 的 uintptr(Sprite* 重释;D 注册)。

## src/ui/widgets/map_preview_widget.hpp

UPSTREAM: OpenRA.Mods.Common/Widgets/MapPreviewWidget.cs @1f594e320 L22-234(全文;
SpawnOccupant 三 ctor + 缩放布局/出生点覆盖/禁用面)。minimap 柄 =
map::MapPreview::GetMinimap 的 uintptr(Sprite* 重释;D 注册)。

## src/ui/widgets/menu_button_widget.cpp

UPSTREAM: OpenRA.Mods.Common/Widgets/MenuButtonWidget.cs @1f594e320 L19-33(全文)。

## src/ui/widgets/menu_button_widget.hpp

UPSTREAM: OpenRA.Mods.Common/Widgets/MenuButtonWidget.cs @1f594e320 L19-33(全文)。

## src/ui/widgets/password_field_widget.cpp

UPSTREAM: OpenRA.Mods.Common/Widgets/PasswordFieldWidget.cs @1f594e320 L15-23(全文)。

## src/ui/widgets/password_field_widget.hpp

UPSTREAM: OpenRA.Mods.Common/Widgets/PasswordFieldWidget.cs @1f594e320 L15-23(全文)。

## src/ui/widgets/progress_bar_widget.cpp

UPSTREAM: OpenRA.Mods.Common/Widgets/ProgressBarWidget.cs @1f594e320
(progress_bar_widget.hpp 的实现)
The implementation of progress_bar_widget.hpp.

## src/ui/widgets/progress_bar_widget.hpp

UPSTREAM: OpenRA.Mods.Common/Widgets/ProgressBarWidget.cs @1f594e320 L19-85(全文)。
定态/不定态双面(Tick 的 offset 往返 + tickStep 翻转;wasIndeterminate 的
复位语义);ChromeProvider.GetMinimumPanelSize 的最小条宽。
The whole of ProgressBarWidget.cs L19-85. The determinate/indeterminate
pair (Tick's offset ping-pong + the tickStep flip; wasIndeterminate's
reset semantics); ChromeProvider.GetMinimumPanelSize's minimum bar width.

## src/ui/widgets/rgba_sprite_widget.cpp

UPSTREAM: OpenRA.Mods.Common/Widgets/RGBASpriteWidget.cs @1f594e320 L19-39(全文)。

## src/ui/widgets/rgba_sprite_widget.hpp

UPSTREAM: OpenRA.Mods.Common/Widgets/RGBASpriteWidget.cs @1f594e320 L19-39(全文)。

## src/ui/widgets/scroll_item_widget.cpp

UPSTREAM: OpenRA.Mods.Common/Widgets/ScrollItemWidget.cs @1f594e320
(scroll_item_widget.hpp 的实现)
The implementation of scroll_item_widget.hpp.

## src/ui/widgets/scroll_item_widget.hpp

UPSTREAM: OpenRA.Mods.Common/Widgets/ScrollItemWidget.cs @1f594e320 L19-97(全文)。
IsVisible 恒 false 模板 + VisualHeight 0 + Initialize 的
IgnoreChildMouseOver 翻转 HACK;Setup 三静态模板装配方;面板图缓存走
GetCachedStatefulPanelImages。
The whole of ScrollItemWidget.cs L19-97. The IsVisible-always-false
template + VisualHeight 0 + Initialize's IgnoreChildMouseOver flip HACK;
the three static Setup template binders; the panel-image cache through
GetCachedStatefulPanelImages.

## src/ui/widgets/scroll_panel_widget.cpp

UPSTREAM: OpenRA.Mods.Common/Widgets/ScrollPanelWidget.cs + ListLayout.cs
+ GridLayout.cs @1f594e320(scroll_panel_widget.hpp 的实现)
The implementation of scroll_panel_widget.hpp.

## src/ui/widgets/scroll_panel_widget.hpp

UPSTREAM: OpenRA.Mods.Common/Widgets/ScrollPanelWidget.cs @1f594e320 L20-528
(核心面;Bind/Unbind 的 IObservableCollection 绑定族随其消费批 ——
ObservableCollection 基建未移植,偏离登记)+ ListLayout.cs L15-47(全文)
+ GridLayout.cs L18-48(全文)。
平滑滚动(target/current 双 offset + SmoothScrollSpeed·dt/40 的衰减)与
滚轮/箭头/拖拽三输入面;九段 thumb 几何与三档 ScrollBar;DrawOuter 的
scissor 裁剪子绘制。Game.RunTime/Settings.UIScrollSpeed/Sound/Renderer.
EnableScissor/Viewport.LastMousePos/Ui.ResetTooltips → Ui::Instance() 注入
承载;Layout = ILayout 接口(ListLayout/GridLayout)。
The core of ScrollPanelWidget.cs L20-528 (the Bind/Unbind
IObservableCollection family awaits its consumer batch — the
ObservableCollection infrastructure is unported; registered) + the whole of
ListLayout.cs L15-47 + GridLayout.cs L18-48. The smooth scrolling (the
target/current offset pair + the SmoothScrollSpeed·dt/40 decay) and the
wheel/arrow/drag input trio; the nine-part thumb geometry and the three
ScrollBar tiers; DrawOuter's scissor-clipped child draw.
Game.RunTime/Settings.UIScrollSpeed/Sound/Renderer.EnableScissor/
Viewport.LastMousePos/Ui.ResetTooltips go through the Ui::Instance()
injected carrier; Layout = the ILayout interface (ListLayout/GridLayout).

## src/ui/widgets/slider_widget.cpp

UPSTREAM: OpenRA.Mods.Common/Widgets/SliderWidget.cs @1f594e320
(slider_widget.hpp 的实现)
The implementation of slider_widget.hpp.

## src/ui/widgets/slider_widget.hpp

UPSTREAM: OpenRA.Mods.Common/Widgets/SliderWidget.cs @1f594e320 L20-146(全文)。
左键拖拽取值(ValueFromPx/PxFromValue 的 thumb 半宽补偿)/UpdateValue 的
Clamp + OnChange 变更通知/thumb 命中即消费;Draw 的刻度条(GetImage 直
取)/轨道九宫格/ButtonWidget.DrawBackground 静态 thumb。ChromeProvider →
Ui::Instance() 注入承载。
The whole of SliderWidget.cs L20-146. The left-drag value taking
(ValueFromPx/PxFromValue's thumb half-width compensation) / UpdateValue's
Clamp + OnChange change notification / consuming while over the thumb;
Draw's tick marks (GetImage direct) / the nine-slice track /
ButtonWidget.DrawBackground's static thumb. ChromeProvider goes through
the Ui::Instance() injected carrier.

## src/ui/widgets/text_field_widget.cpp

UPSTREAM: OpenRA.Mods.Common/Widgets/TextFieldWidget.cs @1f594e320
(text_field_widget.hpp 的实现)
The implementation of text_field_widget.hpp.

## src/ui/widgets/text_field_widget.hpp

UPSTREAM: OpenRA.Mods.Common/Widgets/TextFieldWidget.cs @1f594e320 L20-618
(全文)。全编辑面:选择区双索引 + 鼠标拖选/键盘左移右移的词跳(Ctrl)/
Home/End/Ctrl+D·K·U 的直写 backing field 面/Ctrl+X·C·V·A 的剪贴板对/
删除族(DELETE/BACKSPACE 的词删)/MaxLength 截断/blink 光标(20 tick 相位);
三类型字符过滤(Filename 的 Path.GetInvalidFileNameChars / Integer 的
IsDigit)。Text/CursorPosition 的索引域 = UTF-8 字节(上游 UTF-16 单元;
ASCII 域恒等,登记 COVERAGE);isOSX = game::CurrentPlatformIsOSX。
The whole of TextFieldWidget.cs L20-618. The full editing face: the
selection index pair + mouse drag-select / the keyboard arrows'
Ctrl word-jumps / Home/End / Ctrl+D·K·U's direct-backing-field writes /
the Ctrl+X·C·V·A clipboard quartet / the delete family (DELETE/BACKSPACE
word-deletes) / MaxLength truncation / the blinking cursor (20-tick
phase); the three-type character filter (Filename's Path.
GetInvalidFileNameChars / Integer's IsDigit). Text/CursorPosition's
index domain is UTF-8 bytes (upstream: UTF-16 units; identical over
ASCII, registered); isOSX = game::CurrentPlatformIsOSX.

## src/ui/widgets/tooltip_container_widget.cpp

UPSTREAM: OpenRA.Mods.Common/Widgets/TooltipContainerWidget.cs @1f594e320
(tooltip_container_widget.hpp 的实现)
The implementation of tooltip_container_widget.hpp.

## src/ui/widgets/tooltip_container_widget.hpp

UPSTREAM: OpenRA.Mods.Common/Widgets/TooltipContainerWidget.cs @1f594e320
L18-114(全文)。令牌化的 SetTooltip/RemoveTooltip 对 + 懒装载的 tooltip
子树 + 光标偏移 ChildOrigin 的屏幕边缘回退。Game.Settings.Graphics.
CursorDouble / Game.RunTime / Game.Renderer.Resolution / Viewport 静态 →
Ui::Instance() 注入承载(CursorDouble 随 GraphicSettings 批,缺省 false)。
The whole of TooltipContainerWidget.cs L18-114. The tokenized
SetTooltip/RemoveTooltip pair + the lazily loaded tooltip subtree + the
cursor-offset ChildOrigin with its screen-edge fallbacks.
Game.Settings.Graphics.CursorDouble / Game.RunTime /
Game.Renderer.Resolution / the Viewport statics go through the
Ui::Instance() injected carrier (CursorDouble awaits the GraphicSettings
batch, defaulting false).

## src/ui/widgets/world_button_widget.cpp

UPSTREAM: OpenRA.Mods.Common/Widgets/WorldButtonWidget.cs @1f594e320 L19-49(全文)。

## src/ui/widgets/world_button_widget.hpp

UPSTREAM: OpenRA.Mods.Common/Widgets/WorldButtonWidget.cs @1f594e320 L19-49(全文)。

## src/ui/widgets/world_label_with_tooltip_widget.cpp

UPSTREAM: OpenRA.Mods.Common/Widgets/WorldLabelWithTooltipWidget.cs @1f594e320 L19-44(全文)。

## src/ui/widgets/world_label_with_tooltip_widget.hpp

UPSTREAM: OpenRA.Mods.Common/Widgets/WorldLabelWithTooltipWidget.cs @1f594e320 L19-44(全文)。

## src/yaml/mini_yaml.cpp

UPSTREAM: OpenRA.Game/MiniYaml.cs @1f594e320 L22-791(全文件逐语义重写)
依赖的 BCL 语义在对应位置标注:
 - StreamExts.cs L200-248 ReadAllLinesAsMemory(流式行切分,含 BOM/行尾 \r)
 - MiniYaml.cs L405 String.Split(["\r\n","\n"])(字符串行切分,保留末尾空行)
 - Exts.cs L414-467 IntoDictionaryWithConflictLog(合并冲突消息格式)
C# IEnumerable 惰性产出在此物化为 vector(FromStreamAsEnumerable 并发流测试
不移植,属实现细节;docs/COVERAGE.md 已登记)。
BCL semantics relied upon are annotated at the corresponding sites:
 - StreamExts.cs L200-248 ReadAllLinesAsMemory (streaming line splitting, incl. BOM/trailing \r)
 - MiniYaml.cs L405 String.Split(["\r\n","\n"]) (string line splitting, keeps the trailing empty line)
 - Exts.cs L414-467 IntoDictionaryWithConflictLog (merge conflict log message format)
C# IEnumerable lazy production is materialized into a vector here (the
FromStreamAsEnumerable concurrent-streaming test is not ported, being an
implementation detail; recorded in docs/COVERAGE.md).

## src/yaml/mini_yaml.hpp

UPSTREAM: OpenRA.Game/MiniYaml.cs @1f594e320 L22-791(全文件逐语义重写)
         Full-file statement-by-statement rewrite of upstream MiniYaml.cs.
已登记偏离(docs/COVERAGE.md)/ Registered deviations (docs/COVERAGE.md):
 1. 异常类型统一为 YamlException(C# 分散使用 YamlException/InvalidDataException/
    ArgumentException),消息文本逐字一致
    All error types are unified into YamlException (C# scatters
    YamlException/InvalidDataException/ArgumentException); message texts are verbatim.
 2. C# ResolveInherits 对 null 键节点会 NullReferenceException(上游潜在 bug),
    C++ 侧 null 键走 else 分支(与上游合法输入下的行为一致)
    C# ResolveInherits NullReferenceExceptions on null-key nodes (latent upstream
    bug); the C++ side routes null keys to the else branch (identical behaviour
    for all valid upstream inputs).
 3. ToDictionary/合并键集合中 null 键与 "" 键视为同键(上游解析器不会产生 "" 键)
    In ToDictionary/merge key-sets, a null key is conflated with "" (the upstream
    parser never produces "" keys).

字符串模型 / String model:
C# MiniYaml 的 string 字段可为 null 且引用池化;此处统一为 `const std::string*`
—— nullptr 即 C# null,非空指针指向 StringPool 内的池化字符串(地址稳定、内容即
等价)。参与 Merge 的节点必须来自同一 StringPool(上游 MiniYaml.Load 同样以单池
贯穿一次加载,见 MiniYaml.cs L692)。
C# MiniYaml string fields are nullable, pooled references; here they become
`const std::string*` — nullptr is C# null, a non-null pointer targets a pooled
string inside a StringPool (stable address, content equivalence). Nodes fed into
Merge must come from one StringPool (upstream MiniYaml.Load threads a single pool
through a load as well, see MiniYaml.cs L692).

## tests/attack_test.cpp

UPSTREAM: Phase 5 第四批验收:Shroud/FrozenActorLayer/AttackBase 族/
         AutoTarget/弹丸(Bullet/InstantHit)/战头(SpreadDamage)
         (真 ra 全链:迷雾可见性 → 攻击活动 → 落弹 → 减血至死亡 +
         AutoTarget 空闲扫描)
         The Phase 5 batch-4 acceptance: Shroud/FrozenActorLayer/the
         AttackBase family/AutoTarget/the projectiles
         (Bullet/InstantHit)/the warhead (SpreadDamage) (the real ra
         full chain: fog visibility → the attack activity → impact →
         HP drain to death + the AutoTarget idle scan).

## tests/condition_test.cpp

UPSTREAM: Phase 5 第七批验收:Conditions 31 件 + ExternalCondition/
         ProximityExternalCondition + Cloak/DetectCloaked/IgnoresCloak +
         GainsExperience/GivesExperience/GainsExperienceMultiplier/
         PlayerExperience + CaptureManager/Capturable/Captures/
         GivesCashOnCapture + Enter/CaptureActor + Selectable/Interactable +
         SpawnMapActors(真 ra 全链:地图摆位 → 升级授条件 → 隐身节拍 →
         伤害状态禁隐 → 工程师捕获换主)
         The Phase 5 batch-7 acceptance: the 31 conditions +
         ExternalCondition/ProximityExternalCondition +
         Cloak/DetectCloaked/IgnoresCloak + the experience family +
         the capture family + Enter/CaptureActor + Selectable/
         Interactable + SpawnMapActors (the real ra full chain: the
         map placement → the promotion grant → the cloak cadence →
         the damage-state cloak disable → the engineer capture's
         owner change).

## tests/fluent_test.cpp

UPSTREAM: OpenRA.Game/FluentProvider.cs + FluentBundle.cs @1f594e320
(Phase 6 第一批验收:golden_fluent 对拍门 + 单元面)。
重放器与 golden_gen FluentGoldenMode 同一行扫描/同一探针表;微用例集
逐字节同文嵌入。oracle = Linguini 0.8.1(上游 FluentBundle 的依赖)。
The Phase 6 batch-one acceptance: the golden_fluent differential gate +
the unit faces. The replayer shares the same line scan/probe table with
golden_gen's FluentGoldenMode; the micro-case set is embedded
byte-identically. The oracle = Linguini 0.8.1 (the upstream FluentBundle
dependency).

## tests/fs_test.cpp

UPSTREAM: OpenRA.Game/FileSystem/{FileSystem,Folder,ZipFile}.cs @7d57605
文件系统测试:挂载顺序=覆盖优先级、'|' 显式挂载、Folder 读写、zip 只读
(miniz,含子目录视图与嵌套 zip)、MiniYaml::Load 端到端。
Filesystem tests: mount order as override priority, the '|' explicit mount,
Folder read/write, read-only zip via miniz (with the subfolder view and
nested zips), and MiniYaml::Load end to end.
import std; 门禁:<miniz.h> 为白名单第三方头,其余标准库经 mini_yaml.hpp。
import std; gate: <miniz.h> is the whitelisted third-party header; every
other standard-library entity comes via mini_yaml.hpp.

## tests/game_test.cpp

UPSTREAM: 三代表 trait 端到端语义断言(PORTING_PLAN §Phase 2 R2 风险对冲;
         期望值全部人工对照上游源码/yaml 核对,注记出处行号)
         End-to-end semantic assertions of the three representative traits
         (the PORTING_PLAN §Phase 2 R2 hedge; every expected value
         hand-verified against upstream sources, with source line notes).

断言对象 / Assertion targets:
 - HealthInfo(v2rl):HP=20000(vehicles.yaml L18),NotifyAppliedDamage/
   EditorHealthDisplayOrder 为 C# 默认(Health.cs L30-38)
   HealthInfo (v2rl): HP=20000 (vehicles.yaml L18);
   NotifyAppliedDamage/EditorHealthDisplayOrder at C# defaults (Health.cs
   L30-38).
 - MobileInfo(v2rl,^Vehicle 继承链):Locomotor=wheeled/TurnSpeed=20
   (defaults.yaml L249-251),Speed=72(vehicles.yaml L19 自身覆盖),
   PauseOnCondition=being-captured(^Vehicle 的 BooleanExpression 文本)
   MobileInfo (v2rl, the ^Vehicle chain): Locomotor=wheeled/TurnSpeed=20
   (defaults.yaml L249-251), Speed=72 (the vehicles.yaml L19 override),
   PauseOnCondition=being-captured (the ^Vehicle BooleanExpression text).
 - ArmamentInfo(v2rl):Weapon=SCUD/ReloadingCondition=reloading
   (vehicles.yaml L34-36),Name/Turret 为默认 "primary",TargetRelationships
   =Enemy(1)/ForceTargetRelationships=Enemy|Neutral|Ally(7)
   (Armament.cs L52-53 + TraitsInterfaces.cs L66-71)
   ArmamentInfo (v2rl): Weapon=SCUD/ReloadingCondition=reloading
   (vehicles.yaml L34-36), Name/Turret at the "primary" defaults,
   TargetRelationships=Enemy (1) / ForceTargetRelationships=
   Enemy|Neutral|Ally (7) (Armament.cs L52-53 + TraitsInterfaces.cs
   L66-71).
 - WeaponInfo(SCUD):继承 ^AntiGroundMissile + -Projectile 弱删除 + Bullet
   覆盖(missiles.yaml L319-331 + L1-9 的继承链语义)
   WeaponInfo (SCUD): the ^AntiGroundMissile inheritance + the -Projectile
   weak removal + the Bullet override (missiles.yaml L319-331 + the L1-9
   chain semantics).

## tests/golden_rules.cpp

UPSTREAM: 三 mod 全规则深度解析 dump(PORTING_PLAN §Phase 2 验收关卡;
         黄金数据由 tools/schema_dumper --dump 生成,逐字节对拍)
         The full-rules deep-parse dump of the three mods (the PORTING_PLAN
         §Phase 2 acceptance gate; golden data generated by
         tools/schema_dumper --dump, byte-compared).

用法 / Usage: golden_rules <mod-id> <upstream-root> <golden-file>
协议(schema_dumper --dump 同步实现)/ Protocol (kept in lockstep with
schema_dumper --dump):
  == Actor <name> ==
  Trait <ClassName>[@<instance>]
  <FieldName>: <formatted>
  == Weapon <name> ==
  ...
  == VoiceSet <name> == / == Notification <name> ==
  ...
  == Music <name> ==
  Title: <v> / Filename: <v> / Hidden: <v> / VolumeModifier: <v>

## tests/golden_yaml.cpp

UPSTREAM: OpenRA.Game/MiniYaml.cs @7d57605(黄金对拍:mods/**/*.yaml 全量解析)
与 tests/golden_yaml.txt(C# oracle 生成)逐文件逐字节比对,帧格式见
tools/golden_gen/Program.cs 头注(\x01 帧控制符)。
用法: golden_yaml <golden_yaml.txt> <OpenRA 仓库根>
Per-file, byte-for-byte comparison against tests/golden_yaml.txt (generated by
the C# oracle); see the header comment of tools/golden_gen/Program.cs for the
frame format (\x01 frame control chars).
Usage: golden_yaml <golden_yaml.txt> <OpenRA repository root>
标准库一律经 mini_yaml.hpp 的 import std;(工程门禁:禁止 #include <标准头>)
All standard-library entities come via import std; in mini_yaml.hpp
(project gate: no #include <std headers>).

## tests/meta_test.cpp

UPSTREAM: OpenRA.Game/FieldLoader.cs + Support/VariableExpression.cs + Exts.cs
         的语义单测(PORTING_PLAN §Phase 2;断言文本与 C# 逐字)
         Semantics unit tests for the Phase 2 metadata framework (assertion
         texts verbatim against C#).

## tests/mini_yaml_test.cpp

UPSTREAM: OpenRA.Test/OpenRA.Game/MiniYamlTest.cs @7d57605 L26-997(全用例移植;
FromStreamAsEnumerable 并发流式用例不移植——IEnumerable 惰性属实现细节)
All upstream cases ported; the FromStreamAsEnumerable concurrent-streaming case is
not ported (IEnumerable laziness is an implementation detail).
断言文本与上游逐字一致(含异常消息),作为 Merge/继承/弱删除语义的回归契约。
Assertion texts match upstream verbatim (including exception messages), serving as
the regression contract for merge/inheritance/weak-removal semantics.
多行 yaml 字面量由 tools/fill_yaml_test_literals.py 从上游源码按字节注入
("函数.常量" 双花括号占位符),杜绝缩进层级的手抄偏差。
Multi-line yaml literals are injected byte-exactly from the upstream source by
tools/fill_yaml_test_literals.py (Func.Const double-brace placeholders),
eliminating hand-transcription indent errors.

## tests/move_test.cpp

UPSTREAM: Phase 5 第三批验收:Mobile/BodyOrientation/Move 活动族/Armament
         (纯逻辑矩阵 + 真实 ra 规则/地图全链:构造 → 移动到站 → 开火节拍)
         The Phase 5 batch-3 acceptance: Mobile/BodyOrientation/the move
         activity family/Armament (pure-logic matrices + the real ra
         rules/map full chain: construct → move to arrival → the firing
         cadence).

## tests/production_test.cpp

UPSTREAM: Phase 5 第五批验收:Missile/TeslaZap/GravityBomb 弹丸 +
         TargetDamage/LeaveSmudge/CreateEffect 战头 +
         FrozenUnderFog/HiddenUnderShroud + 建筑/生产链
         (真 ra 全链:玩家资源 → TechTree 前置 → TENT 建筑 →
         ClassicProductionQueue 造 e1 → 出兵;Dragon 导弹飞行链)
         The Phase 5 batch-5 acceptance: the Missile/TeslaZap/
         GravityBomb projectiles + the TargetDamage/LeaveSmudge/
         CreateEffect warheads + FrozenUnderFog/HiddenUnderShroud +
         the building/production chain (the real ra full chain: the
         player resources → the TechTree prerequisites → the TENT
         building → ClassicProductionQueue producing an e1 → the unit
         leaving; the Dragon missile flight chain).

## tests/render_sequences_fixture.hpp

UPSTREAM: NONE(测试设施:真 ra 序列表 + 合成资产副本的序列装配;
         Phase 5 第八批起,渲染 trait 构造依赖 Map::Sequences —— 各
         sim 测试共用此夹具装配)
         Test facility (the real ra sequence tables + synthetic asset
         copies; from Phase 5 batch 8 the render traits' construction
         depends on Map::Sequences — the sim tests share this fixture).

## tests/render_test.cpp

UPSTREAM: Phase 5 第八批验收:RenderSprites/WithSpriteBody(+Facing)/
         WithInfantryBody/WithSpriteTurret/WithMakeAnimation(+Overlay)+
         ProximityCapturable 族 + FieldSaver/Settings + ReplayRecorder/
         ReplayConnection/SyncReport 起步(真 ra 全链:真序列集装配 →
         e1 动画状态机 → 2tnk 炮塔面 → TENT make 动画 → 邻近捕获换主)
         The Phase 5 batch-8 acceptance: the RenderSprites/WithSpriteBody
         family + WithInfantryBody/WithSpriteTurret/WithMakeAnimation
         (+Overlay) + the ProximityCapturable family + FieldSaver/
         Settings + the ReplayRecorder/ReplayConnection/SyncReport
         start (the real ra full chain: the real sequence-set assembly →
         e1's animation state machine → the 2tnk turret face → TENT's
         make animation → the proximity capture's owner change).

## tests/resource_test.cpp

UPSTREAM: Phase 5 第六批验收:AttackFollow/Turreted 族 + PowerManager/
         Power/AffectedByPowerOutage + 资源链全量(ResourceLayer/
         ResourceClaimLayer/DockClientManager/DockHost/MoveToDock/
         GenericDockSequence/Harvester/HarvestResource/
         FindAndDeliverResources/StoresResources/StoresPlayerResources/
         Refinery)
         (真 ra 全链:电力账本 → 低电限速;2tnk 炮塔索敌开火致死;
         HARV 采 Ore → 满载 → 入坞卸货 → 玩家资源入账)
         The Phase 5 batch-6 acceptance: the AttackFollow/Turreted
         family + PowerManager/Power/AffectedByPowerOutage + the full
         resource chain (ResourceLayer/ResourceClaimLayer/
         DockClientManager/DockHost/MoveToDock/GenericDockSequence/
         Harvester/HarvestResource/FindAndDeliverResources/
         StoresResources/StoresPlayerResources/Refinery)
         (the real ra full chain: the power ledger → the low-power
         slowdown; the 2tnk turret acquiring and killing; the HARV
         harvesting ore → full → docking → unloading → the player
         account credited).

## tests/seq_test.cpp

UPSTREAM: OpenRA.Mods.Common/Graphics/DefaultSpriteSequence.cs +
          OpenRA.Mods.{Common,Cnc,D2k}/Graphics/*SpriteSequence.cs
          @b6fc03f 的验收测试(Phase 4 第十四批)
mods 序列族端到端:合成 shpTD + 临时 FileSystem → SequenceSet(真实
SpriteCache 流水)→ 解析/预留/物化/查询矩阵 + 异常文本逐字。
The acceptance test of the mods sequence family (Phase 4 batch 14):
synthetic shpTD files + a temp FileSystem → SequenceSet (the real
SpriteCache pipeline) → the parse/reserve/resolve/query matrix + the
exception texts verbatim.

## tests/server_test.cpp

UPSTREAM: 测试面(Phase 7 第十四批):回环真 socket 的服务器全链验收
 —— 握手(协议 7 + 令牌)/HandshakeResponse 六门/槽位装配与
 SyncInfo/Chat 广播与洪水闸/Ack 前投影/StartGame 空帧注入/服务器侧
 desync 检测(录像作废)/DropClient 断连包与 Fluent 通知/Ping 往返/
 OrderBuffer 的 TickScale 结算/专用服 RecordFakeHandshake/
 GameInformation 往返/FluentMessage 与 Sanitize 的逐字面。
 The test face (Phase 7 batch 14): the loopback real-socket full-chain
 acceptance of the server — the handshake (protocol 7 + the token)/the
 HandshakeResponse six gates/the slot assembly and SyncInfo/the Chat
 broadcast and the flood gate/the Ack forward projection/StartGame's
 empty-frame injection/the server-side desync detection (the recording
 invalidated)/DropClient's disconnect packet and Fluent notifications/
 the Ping round-trip/OrderBuffer's TickScale settlement/the dedicated
 RecordFakeHandshake/GameInformation's round-trip/FluentMessage's and
 the Sanitize family's verbatim faces.

## tests/sim_test.cpp

UPSTREAM: 仿真核心单测(PORTING_PLAN §Phase 3 验收:
         TraitDictionary/Activity/条件系统/Order 字节级序列化/OrderIO/
         EchoConnection+OrderManager 锁步 10⁶ tick ASan 无泄漏)
         The sim-core unit tests (the PORTING_PLAN §Phase 3 acceptance:
         TraitDictionary/Activity/condition system/byte-level Order
         serialization/OrderIO/EchoConnection+OrderManager lockstep with
         10⁶ ticks, ASan-clean).

断言期望值对照 / Expected values verified against:
 - 哈希函数族:Sync.cs L109-164 公式手算(bool 的 EmitSyncOpcodes
   L63-71:0xAAA 为死常量,Brtrue 恒跳转,活语义 = (b?1:0);第十二批
   G2 黄金对拍修正了旧的 ^0xAAA 误读)
   The hash family: hand-computed from Sync.cs L109-164 (bool's
   EmitSyncOpcodes L63-71: the 0xAAA is a dead constant, the Brtrue
   branch is unconditional, the live semantics are (b?1:0); the
   batch-12 G2 golden differential fixed the old ^0xAAA misreading).
 - Order 字节:Order.cs L345-487 序列化序逐字段人工推导(flags 位值/
   7-bit 前缀/小端)
   Order bytes: hand-derived field-by-field from the Order.cs L345-487
   serialization order (flag bit values / 7-bit prefixes / little-endian).
 - 锁步:OrderManager.cs L98-328 状态机(EchoConnection 帧投影 +1/
   IsNetFrame 节流/ProcessOrders 帧号校验)
   Lockstep: the OrderManager.cs L98-328 state machine (EchoConnection
   frame projection +1 / IsNetFrame throttling / ProcessOrders frame
   validation).

## tests/sound_test.cpp

UPSTREAM: OpenRA.Game/Sound/Sound.cs @b6fc03f L36-481 + SoundInfo.cs L56-86
          的验收测试(Phase 4 第十四批)
声音门面:格式链分派/源缓存/Play 族/通知池打断策略/音乐与视频面;
Dummy 引擎(空实现)+ 上游 mods 散装 .aud 实资产。
The acceptance test of the sound facade (Phase 4 batch 14): the
format-chain dispatch / the source cache / the Play family / the
notification-pool interrupt policies / the music and video faces; the
Dummy engine (no-op) + real loose .aud assets from upstream mods.
