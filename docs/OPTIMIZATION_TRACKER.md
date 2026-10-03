# OPTIMIZATION_TRACKER — 上游优化项执行追踪表

> 来源:[UPSTREAM_CPP_REVIEW.md](UPSTREAM_CPP_REVIEW.md)(2026-10-04 上游 @7d57605bca 审阅,证据锚点齐全)。
> 本表是后续 Phase 4-9 的**执行依据**:每个优化项一条,带 ID、归属 Phase、验收要求与状态。
> 移植一个模块前先查本表该 Phase 的条目,**优化随移植一起落地**,不做事后返工。
>
> **状态流转**:`未开始` → `进行中` → `已落地`(代码合入,单测过)→ `已验证`(对拍/验收关卡过)。
> 特殊状态:`已吸收` = Phase 0-3 现有架构(gen/ 描述表、TypeId、sync_gen)已覆盖,无需单独立项。
>
> **纪律**(与 PORTING_PLAN §7.5 对齐):
> 1. 凡进入同步路径的优化,必须先给出语义等价论证,再过对拍(见 C 类警示);
> 2. 凡改变可观察行为(非透明优化,如 B11 的 HACK 修正),合入时在 docs/COVERAGE.md 登记偏离;
> 3. 状态更新与代码同提交;本表 ID 同时用作提交信息与 COVERAGE 的引用键。

---

## 总览

| ID | 标题 | Phase | 状态 | 验收 |
|---|---|---|---|---|
| OPT-A1 | 百分比修正链 decimal→__int128 精确复刻 + span 化 | 5(核心已提前落地) | **已落地**(黄金对拍待 Phase 5) | core_test + Phase 5 黄金矩阵 |
| OPT-A2 | 寻路内环:世代标记 + 邻居回调化 + 格点 u32 索引 | 5 | 未开始 | 寻路单测 + replay 对拍 |
| OPT-A3 | Actor 构造期接口物化扩到全部热 INotify* | 5 | 未开始 | sim_test + replay 对拍 |
| OPT-A4 | 条件系统 intern 化(u32 ConditionId) | 5 | 未开始 | 语义论证 + replay 对拍 |
| OPT-A5 | 渲染命令缓冲(值类型命令 + 无锁 SPSC + 持久映射 VB) | 4 | 未开始 | 渲染对拍(截图容差) |
| OPT-A6 | GL 层现代化(glad 直连/VAO/UBO uniform/去 glGetError 轮询) | 4 | 未开始 | 渲染对拍 |
| OPT-A7 | 渲染收集 SOA + 帧 arena;调色板 dirty 行;顶点瘦身 | 4 | 未开始 | 渲染对拍 + 帧分配统计 |
| OPT-A8 | 空间索引 ActorID 数组化(SpatiallyPartitioned/ScreenMap) | 5 | 未开始 | sim_test + replay 对拍 |
| OPT-A9 | 武器/攻击链零分配(modifier 失效重算/struct 按值/候选缓冲复用) | 5 | 未开始 | replay 对拍 |
| OPT-A10 | 资源层 u8 索引/HPF 位图 dirty/CSR 抽象图/采金位掩码 | 5 | 未开始 | replay 对拍 |
| OPT-A11 | 数据加载散点(排序索引匹配/装箱缓存消灭/% 零分配/键规范形) | 2-3 已吸收 / 5 补遗 | 部分已吸收 | resolved-rules 快照不变 |
| OPT-B1 | NPOT FBO + 单级合成 | 4 | 未开始 | 渲染对拍 |
| OPT-B2 | FreeType 真结构体 | 4 | 未开始 | 字形渲染对拍 |
| OPT-B4 | 平台 DPI/窗口 API 现代化 | 4 | 未开始 | 冒烟 |
| OPT-B5 | 窗口属性原子快照(去锁) | 4 | 未开始 | 冒烟 |
| OPT-B6~B10 | 解析框架去 TypeConverter/magic string/静态委托/Task 轮询/程序集哈希 | 已吸收(gen/ 架构)/5 | 已吸收 | — |
| OPT-B11 | sim 自认 HACK 修正(Mobile/ResourceLayer lerp-9 等) | 5 | 未开始 | **COVERAGE 偏离登记** |
| OPT-B12 | 阻塞判定三处镜像收敛 | 5 | 未开始 | replay 对拍 |
| OPT-B13 | 热路径 assert 化校验 + 采样编译期开关 | 5 | 未开始 | sim lint |
| OPT-B14 | Server 事件 variant + OrderManager/SyncHash 手写循环 | 7 | 未开始 | 锁步对拍 |
| OPT-B17 | UI 散点(IsVisible 闭包/布局变量 intern) | 6 | 未开始 | UI 冒烟 |
| OPT-B18 | Lua 绑定编译期表 | 8 | 未开始 | --lua-docs 对拍 |
| OPT-INFRA | arena/帧分配器落地(Phase 0 遗留) | 0(补) | **已落地** | core_test + ASan |
| OPT-C1~C5 | 确定性对拍警示(见下) | 随关联项 | — | 各自关卡 |

---

## A 类:语言特性优化(详情)

### OPT-A1 百分比修正链(Phase 5;核心已落地)

- 上游:`Mods.Common/Util.cs:203-211`(decimal 链)+ 22 个调用文件;参数 `IEnumerable<int>`(LINQ/ToArray 分配)。
- **已落地(2026-10-04)**:`src/core/percent_modifiers.hpp`
  - `ApplyPercentageModifiers(int32, span<const int32>)`:`__int128` 分子 × 分母 `100^k`,整除 = C# `(int)decimal` 向零截断;
  - 等价域论证写在文件头(100=2²×5² ⇒ p/100 有限小数;实践值域永不触发 decimal 28 位舍入);
  - `ORA_CASSERT`:`if !consteval` 包裹的常量求值安全断言(溢出/超界 = 上游 OverflowException 域);
  - 测试:`tests/core_test.cpp` 含语义底线用例 `Apm(3,{60,60})==1`(早截断会得 0,decimal 全精度得 1.08→1)。
- **待办(Phase 5)**:22 个调用点移植时改用 `inplace_vector<int,8>` 传参;golden_gen 增补程序化边界矩阵(链长 0..4 × 正负 × 截断边界)与 C# oracle 对拍;Armament/Mobile/Health 等 modifier 缓存按 OPT-A9 失效重算方案接线。

### OPT-A2 寻路内环(Phase 5)

上游:DensePathGraph.cs:123 每节点展开 new List;CellInfoLayerPool.cs:35-51 池 4 层 + 取层全图 memset(28B/格);CellLayer.cs:56-90 索引器分支+除法+事件检查;PriorityQueue.cs 层级加倍堆。
方案:世代标记(`u32 epoch[]`,免清零)、`for_each_neighbor` 回调模板(邻居 ≤10,栈缓冲)、状态 `{u32 costSoFar; u32 costTotal:30,status:2; u32 prevIndex}` 格点索引、网格类型编译期化、平坦 4 叉堆 + `clear()` 只重置 size。
验收:寻路单测(平局路径用例,见 OPT-C4)+ Phase 5 replay 对拍。

### OPT-A3 接口物化扩展(Phase 5)

上游:Actor.cs:179-195 已物化 17 接口(16 连 `is` 测试);C++ 侧 gen/ 上行转换表已具备零成本等价物。Phase 5 移植 mods trait 时把受击(INotifyAppliedDamage/Damage)、所有者变更、队列等**热 INotify\* 全部纳入物化清单**(物化清单由 schema_dumper 导出的接口实现关系生成,不手工维护)。
验收:sim_test 接口物化断言 + replay 对拍。

### OPT-A4 条件系统 intern 化(Phase 5,观察者大面移植前)

上游:Actor.cs:89-109/560-615 三个 string 字典;C++ 现状:`src/sim/actor.hpp` 照抄 `std::map<std::string,...>`。
方案:Ruleset 加载期 intern 为全局 `u32 ConditionId`(条件名全量在规则文件,封闭集);每 Actor 按**其涉及的 condition 局部索引**开小数组(活跃计数 + 观察者位集);token = `{u16 slot, u16 serial}`。
**语义红线(OPT-C2)**:观察者目前收全字典 `IReadOnlyDictionary<string,int>` —— C++ 侧保留"按 id 索引的只读数组视图"语义,通知顺序 = 登记顺序,不得改增量(增量 span 是二期可选,需逐观察者审计)。
验收:条件系统单测(通知序断言)+ replay 对拍;COVERAGE 登记(数据结构偏离,行为等价)。

### OPT-A5 渲染命令缓冲(Phase 4,最大重设计)

上游:ThreadedGraphicsContext.cs 477-505(每 GL 调用装箱+锁+Pulse)、599-630(顶点三段拷贝/数组换所有权)、719-755(LOH 阈值分支拷贝)、210/298-309(每 Message 常驻内核句柄)、103-123(反射构造泛型 VB);Sdl2PlatformWindow.cs:345-353(Windows 窗口模式整体禁渲染线程)。
方案:`struct GfxCmd`(variant/union 值类型)双缓冲命令表 + SPSC 无锁环 + `atomic::wait/notify`;`glBufferStorage` 持久映射 + fence 顶点直写;PBO staging 纹理上传;模板化 `create_vb<V>`;**统一线程模型**(Windows 特例删除或以单线程+命令缓冲模型替代,Phase 4 开工前定案)。
验收:渲染截图对拍 + 帧分配/锁行为统计;COVERAGE 登记(线程模型偏离)。

### OPT-A6 GL/着色器层(Phase 4)

上游:OpenGL.cs 801 行手写绑定 + 每次 CheckGLError;Shader.cs:134-145 单 VAO 重播属性指针、184-218 字符串 uniform、147-172 每次 flush glIsTexture;blend 无缓存;FBO Bind 内嵌 glFlush。
方案:glad2 直连(Release 剔除错误轮询,Debug 用 KHR_debug);每 (顶点格式,shader) 一 VAO;uniform 枚举化 + UBO;纹理句柄 RAII + 绑定 diff;blend 状态机 diff。
验收:渲染对拍。

### OPT-A7 渲染收集 SOA + 调色板(Phase 4)

上游:WorldRenderer.cs:141-175 每帧对象分配、319-323 GroupBy(GetType()) HACK;Vertex.cs 48B 胖顶点;TerrainSpriteLayer 全图 AOS + 调色板失效全图重写;HardwarePalette.cs:132-154 每帧无条件全量上传;PaletteReference 每精灵字符串哈希。
方案:`std::vector<RenderItem>`(POD)帧 arena(FRAME ARENA **已落地**,src/core/arena.hpp);type_id 计数分段;instanced quad(~36-40B/精灵)+ u16 索引;地形层 palette_index(u16)/tint(u8x4) 独立数组;调色板指纹比较 + dirty 行。
验收:渲染对拍(逐像素容差)+ 帧峰值分配统计。

### OPT-A8 空间索引 ActorID 数组化(Phase 5)

上游:SpatiallyPartitioned.cs(bins=Dictionary<Actor,Rect>,跨 bin new HashSet 去重);ScreenMap.cs LINQ 链;ActorMap yield 迭代器。
方案:ActorID 稠密 ⇒ slab 数组直接索引;`for_each_actor_in_circle(pos,r,f)` 模板回调;去重用"最后查询标记"数组。
验收:sim_test + replay 对拍。

### OPT-A9 武器/攻击链零分配(Phase 5)

上游:Armament.cs:167-170/322-362;DamageWarhead.cs:78-88;SpreadDamageWarhead.cs:106-122;AutoTarget.cs:355-470;Activities/Attack.cs:192-207。
方案:modifier 在条件授予/撤销时失效重算(`inplace_vector<int,8>` 缓存);`AttackInfo`/`WarheadArgs` struct 按值;AutoTarget 单层手写循环 + 复用候选缓冲;armament 选择结果按 target 缓存。
验收:replay 对拍(伤害/AI 决策序列一致)。

### OPT-A10 类型紧化散点(Phase 5)

ResourceLayer string→u8 资源索引({u8 type;u8 density} 2B/格);Harvester Resources 256 位掩码;HPF dirtyGridIndexes/cellsWithBlockingActor 位图化、localCellToAbstractCell→CellLayer<u32>、抽象图 CSR;Locomotor.dirtyCells 位平面;PerfSample/GraphConnection 校验编译期化。
验收:replay 对拍。

### OPT-A11 数据加载散点(部分已吸收)

已吸收:反射链主体(gen/ 描述表 + TypeId)、`%` 解析零分配(meta/parse.cpp 的 from_chars 路线)、规则键(加载链已走 intern)。
Phase 5 补遗:Ruleset 键小写化统一为大小写不敏感比较(若快照对拍显示仍有分配点);MiniYaml MergePartial 的 scratch 字典查重改排序+adjacent_find(快照输出必须逐字节不变)。
验收:golden_yaml / golden_rules_* 快照零差异。

---

## B 类:陈旧模块改造(详情摘录)

- **OPT-B1 NPOT FBO**(Phase 4):Texture.cs:84-85/FrameBuffer.cs:32-33 强制 pow2;Renderer.cs:164-360 三级 pow2 合成(1920×1080 → 2048² 双 FBO、每帧 3 次全屏 clear + 2 次全屏拷贝)。改 NPOT + 单级 blit;`BeginFrame` 对默认 FB 的冗余 Clear 一并删除。验收:渲染对拍。
- **OPT-B2 FreeType 真结构体**(Phase 4):FreeTypeFont.cs:27-36 手算偏移 HACK 删除,直接 `#include <ft2build.h>`。
- **OPT-B11 sim HACK 修正**(Phase 5):Mobile.cs:261/770-772/865(NearestMoveableCell 换正规寻路)、ResourceLayer.cs:159-161(lerp-9 修正为 8)。**每项登记 COVERAGE 偏离**(行为可观察变化,仅单机可感知处可不登,以"replay 不受影响"论证)。
- **OPT-B12 阻塞判定收敛**(Phase 5):Locomotor.cs:328-331/460-462 与 HPF.cs:656-690 三处镜像收敛为单一函数。
- **OPT-B14 Server/OrderManager 散点**(Phase 7):Server.cs 事件对象 variant 化、`Conns.SingleOrDefault` 手写;OrderManager.cs:202/308 `pendingOrders.All` 手写;World.cs SyncHash 惰性枚举改直接遍历。
- **OPT-B17 UI 散点**(Phase 6):Widget IsVisible 闭包→成员函数指针/位标志;IntegerExpression 变量 intern;Logic 实例化走 gen/ 表。
- **OPT-B18 Lua 绑定**(Phase 8):ScriptObjectWrapper 反射绑定 → gen/lua_api.cpp 编译期表(PORTING_PLAN 已计划,此条确认热路径 = 每成员访问的字符串字典 + 委托双跳)。
- B3(自研 PNG)/B15(Settings diff)/B16(PriorityArray 不移植)/B6-B10(解析框架):见 UPSTREAM_CPP_REVIEW 二节,B6-B10 已被 gen/ 架构吸收。

---

## C 类:确定性对拍警示(执行关联项时必须重读本条)

- **OPT-C1(OPT-A1)**:黄金矩阵覆盖链长>2、负百分比、|商| 恰在整数边界、INT32 极值;C# oracle 由 golden_gen 生成(Tools 在 .NET 上跑,非运行时依赖)。
- **OPT-C2(OPT-A4)**:条件通知序 = 登记序;观察者读全量视图语义保留;禁在未审计前改增量。
- **OPT-C3(OPT-A3/A8)**:TraitDictionary 列式化后 `ActorsWithTrait<ISync>` 遍历序必须仍为 ActorID 升序;`Actor.SyncHashes` 数组序 = 拓扑构造序,不得重排。
- **OPT-C4(OPT-A2)**:上游堆在相等代价下的弹出顺序需复刻或证明不影响选路(平局路径用例入对拍矩阵)。
- **OPT-C5(OPT-A7)**:调色板 dirty 行方案渲染结果须与全量上传逐像素一致(渲染关卡)。

---

## 已落地记录

| 日期 | 项 | 内容 | 验证 |
|---|---|---|---|
| 2026-10-04 | OPT-A1(核心) | `src/core/percent_modifiers.hpp`:`__int128` 精确复刻 decimal 链 + `ORA_CASSERT` 常量安全断言;`tests/core_test.cpp` 语义断言(含早截断分歧底线用例 + constexpr static_assert) | core_test PASS;双构建 ctest 12/12;黄金矩阵留 Phase 5 |
| 2026-10-04 | OPT-INFRA | `src/core/arena.hpp`:FrameArena(帧临时,平凡可析构,Mark/Rewind/Reset)+ WorldArena(整局,析构登记逆序,Destroy 幂等,Reset 复用/Release 归还);`tools/upstream_check.py` 增 `UPSTREAM: NONE` 自有设施登记形式 | core_test 析构序/幂等/复用断言全过;ASan+UBSan 干净;87 条 UPSTREAM 标注 PASS |

> Phase 4/5 各模块开工时:先读本表对应条目 → 移植与优化同批合入 → 更新状态列与"已落地记录" → 关联 C 类警示走对拍。
