# porting-notes —— 代码注释迁移档案

cpp26.md 规定 `.hpp` / `.cpp` 内一律不写注释（P1-2 整改）。本文件按源文件索引，收纳删除前注释中有移植价值的内容：上游对照锚点（cs 文件:行号）、实测口径、坑位与偏离说明；复述代码的废话注释未迁移、直接删除。

行号 `Lnnn` 锚定**注释删除前**的提交 `78df842c`（P1-1 后、P1-2 前），可用
`git show 78df842c:<path>` 对照原始上下文。此后代码演化以函数/符号名为准。

## src
## src/game/combat/armament.cpp
- L10 `namespace {`
  Util.IndexFacing/QuantizeFacing（Util.cs）：量化朝向到最近整步

## src/game/combat/armament.hpp
- L12 `struct CombatUnit;`
  Armament trait 状态机最小面（OpenRA.Mods.Common/Traits/Armament.cs，T12/issue #13）。

## src/game/combat/combat_sim.cpp
- L29 `}`
  Util.FacingWithinTolerance（Util.cs:120-127）
- L37 `}`
  Target.IsInRange（Target.cs:196-203）：Positions 任一 2D 水平距离 <= range（含等号）。
- L46 `}`
  Weapon.IsValidAgainst（WeaponInfo.cs:209-221）：CanTargetSelf + TargetTypes Overlap
- L66 `}`
  —— MoveWithinRange 子活动装配（MoveWithinRange.cs + PathFinder.FindPathToTargetCells）——
- L146 `}`
  —— Activity 段：Attack 活动（AttackFrontal 型，Activities/Attack.cs）——
- L223 `queue_move_within_range(arena, unit, target);`
  Activity.TickOuter（Activity.cs:130-136）："Avoid a single tick delay if the
- L265 `const CombatUnit& target{*ptr_target};`
  CanAimAtTarget（AttackFollow.cs:86-103）：in range && min 外 && firing arc（512 容忍恒过）
- L485 `std::int32_t arena_sync_hash(const Arena& arena) {`
  World.SyncHash（World.cs:482-510）+ Sync.EmitSyncOpcodes 聚合语义（int32 wrap）。
- L486 `// World.SyncHash（World.cs:482-510）+ Sync.EmitSyncOpcodes 聚合语义（int32 wrap）。`
  ISync 槽序（探针实证 t12-sync 诊断）：p0/p1 Shroud(0) → 每 unit
- L488 `// Health(HP) → BodyOrientation(qf) → Mobile(Facing ^ FromCell.Bits ^ ToCell.Bits ^`
  CenterPosition——Sync.cs 属性序，from==to 时 Bits 抵消) → [Turreted(0)] →
- L506 `if (unit.b_in_world) actor_term(unit.u4_id);`
  ISync 段（player Shroud 占位 ×2）
- L520 `sync_term(unit.int4_quantized_facings);`
  Mobile（Sync.cs GetProperties 声明序：Facing/FromCell/ToCell/CenterPosition）

## src/game/combat/combat_sim.hpp
- L18 `// 剧本世界（T12/issue #13）：双玩家敌对 + 最小合成规则集三单位型`
  （rifleman/tank/arty，探针 t12-scratch 同配方）+ effects + 帧尾任务 +
- L20 `// SharedRandom(MersenneTwister(0)) + 段序 tick（Activity → ITick → effects → 帧尾排干，`
  对齐上游 World.Tick）。dump 行/sync_hash 供逐 tick 对拍。
- L66 `bool b_is_aiming{};`
  AttackBase.IsAiming（ISync）
- L89 `kBullet,`
  弹道飞行（ISync）
- L137 `[[nodiscard]] const std::map<std::string, UnitTemplate, std::less<>>& unit_templates();`
  探针剧本等价：spawn（FacingInit=0）+ Attack 订单（AttackTarget，queued=false allowMove=true）
- L150 `[[nodiscard]] const CombatUnit* arena_unit(const Arena& arena, std::uint32_t u4_id);`
  World.SyncHash() 语义复刻（int32 wrap；actors 段 → ISync 段[Shroud×2 占位 →
- L154 `[[nodiscard]] std::int32_t arena_sync_hash(const Arena& arena);`
  dump tick 行（探针格式逐字段一致：tick/a/b/eff/rng/sync）

## src/game/combat/health.cpp
- L16 `if (!b_ignore_modifiers && int4_damage > 0) {`
  链 3：decimal 逐修饰符精确乘积（*= m/100m 全精度），链末一次截断（Health.cs:176-187）

## src/game/combat/health.hpp
- L7 `namespace ora::game::combat {`
  Health trait 最小面（Traits/Health.cs，T12/issue #13）。

## src/game/combat/projectile.cpp
- L10 `std::int32_t wdist_from_pdf(support::MersenneTwister& rng, std::int32_t int4_samples) {`
  WDist.FromPDF（WDist.cs:56-61）：N 样本 Next(-1024,1024) 求和除 N（int 向零截断）
- L18 `pathfinding::WVec wvec_from_pdf2(support::MersenneTwister& rng) {`
  WVec.FromPDF（WVec.cs:99-102）：X/Y 各 WDist.FromPDF(r,2) → 共 4 次 Next
- L35 `std::int32_t int4_div) {`
  WPos.LerpQuadratic（WPos.cs:58-72）：linear lerp + decimal 高度项。
- L37 `// offset = (decimal)(b-a).Length * pitch.Tan() * mul * (div-mul) / (1024*div*div)`
  —— decimal 精确商链末 (int) 截断 → int64 乘积一次除（值域内等价）。
- L79 `if (weapon.projectile.int4_inaccuracy > 0) {`
  Util.GetProjectileInaccuracy（Util.cs:369-384，InaccuracyType.Maximum 默认）：
- L104 `if (wh.kind == WarheadKind::kLeaveSmudge) return;`
  不同步、无伤害（upstream 登记）
- L221 `void effects_tick(Arena& arena) {`
  列表序枚举（含本帧新增尾插项——与上游 List 枚举器一致）

## src/game/combat/projectile.hpp
- L14 `// DelayedImpact / SpreadDamageWarhead / DamageWarhead / CreateEffectWarhead，T12）。`
  效果动画（SpriteEffect）不入对拍面：RNG 消耗照做（CreateEffect 的 Inaccuracy
- L15 `// 效果动画（SpriteEffect）不入对拍面：RNG 消耗照做（CreateEffect 的 Inaccuracy`
  在效果创建之前），动画生命周期登记 upstream.md。
- L35 `// 外扩面] → falloff 距离 → 链 2/链 3）/ CreateEffect（ActorTypeAtImpact 门闸 + Inaccuracy`
  RNG 4）/ LeaveSmudge（不同步，跳过——登记 upstream）
- L44 `void effects_tick(Arena& arena);`
  WPos.LerpQuadratic（WPos.cs:58-72）：linear lerp + decimal 高度项

## src/game/combat/warhead_damage.cpp
- L10 `// decimal 链：a = number; foreach p: a *= p/100m; return (int)a`
  （p/100m 是精确两位小数 → 全链等价于 number × ∏p / 100^n 的精确商，

## src/game/combat/warhead_damage.hpp
- L15 `// SpreadDamageWarhead / Health 最小面，T12/issue #13）。链内 __int128 精确乘积、`
  链末一次向零截断——与 C# decimal（96 位尾数）在 int 输入域内逐值等价。
- L18 `// 三条链绝不合并：链间是截断后的 int 传递（23000×70%×60% 型值合并计算会差 1）。`
  Util.ApplyPercentageModifiers（Util.cs:203-211）：number × ∏(p/100)，链末 (int) 截断
- L24 `std::int32_t int4_number, const std::vector<std::int32_t>& vec_percentages);`
  int2.Lerp（Primitives/int2.cs:77-80）：a + (b-a)*mul/div（int 乘除向零截断）
- L30 `// Lerp(Falloff[i-1], Falloff[i], distance-inner, outer-inner)；超界 0。`
  vec_falloff/spread 语义与上游一致（Range 空表 → effectiveRange[i] = i*Spread）。
- L45 `std::string_view str_armor_type);`
  Circle HitShape.DistanceFromEdge（HitShapes/Circle.cs）：max(0, |pos-origin| - radius)

## src/game/combat/weapon.cpp
- L47 `if (*opt_cell < 0) int4_sub = -int4_sub;`
  上游：符号传播到小数部分
- L51 `}`
  逗号分隔列表（FieldLoader 数组语义；空白容许）
- L85 `}`
  TargetTypes / DamageTypes 剥离：对拍面用 ValidTargets 字符串序列（保留原序）

## src/game/combat/weapon.hpp
- L10 `// WeaponInfo/ProjectileInfo/Warhead 的 yaml 最小解析面（T12/issue #13）。`
  语义：上游 WeaponInfo(MiniYaml) ctor（FieldLoader：缺键 → 字段默认值保留）
- L12 `// + Projectile/Warhead 子节点（值 = 类型名 + "Info"/"Warhead" 反射构造）。`
  对拍面 = tests/golden/combat/duels.txt 头部（weapons loaded/weapon/warheads 行）。
- L22 `std::string str_kind_name;`
  反射类名（"SpreadDamageWarhead" 等，dump 对拍）
- L64 `std::vector<WeaponDef> vec_weapons;`
  合并树序（含 ^ 抽象节点，与 oracle 同口径）
- L73 `};`
  上游 MiniYaml.Load(modFiles, Manifest.Weapons, null) 等价：逐文件解析（保清单序，
- L80 `const std::vector<std::string>& vec_file_names);`
  WDist yaml 字面量（"5c0"/"171"/"1c138"）；失败 nullopt（上游 TryParse 语义）

## src/game/economy/economy.cpp
- L120 `}`
  经济面 actor id 视图：对齐上游剧本 ActorID 分配序——world=0、非观察玩家按
- L121 `// 经济面 actor id 视图：对齐上游剧本 ActorID 分配序——world=0、非观察玩家按`
  vec_players 序、之后按创建序；观察者玩家（Everyone）不参与经济面（上游剧本
- L122 `// vec_players 序、之后按创建序；观察者玩家（Everyone）不参与经济面（上游剧本`
  不创建该 actor）。观测行/A 行一律经此归一，保证与 oracle 黄金同一编号空间。
- L442 `const char ch_cell{str_row[static_cast<std::size_t>(int4_dx)]};`
  '_' 格不属于建筑；'='（bib/TransitOnly）语义是可穿行不可停留——各
- L443 `// '_' 格不属于建筑；'='（bib/TransitOnly）语义是可穿行不可停留——各`
  BlockedByActor 档均不挡路径（探针实测 All 档回程路径穿 proc bib）。
- L633 `}`
  上游 GetResource 返回值拷贝（struct），RemoveResource 归零清格会改写原格
- L747 `// 载重减速（Harvester ISpeedModifier：100-(100-FullyLoadedSpeed)×Fullness/100，`
  int 截断；Fullness=ContentsSum×100/Capacity 整除）。速度链对齐上游
- L986 `const world::ActorModel& actor{w.vec_actors_all[u4_actor_id]};`
  FreeActor 附送单位是独立的帧尾任务（上游 FreeActor.TraitEnabled），与
- L1199 `// 采矿车的 Locomotor 按名匹配（规则可同时声明 foot 等其它 Locomotor——`
  上游 Move 的 PathSearchOrder 语义里 harv 只用 heavywheeled）。

## src/game/economy/economy.hpp
- L58 `kTurn = 7,`
  上游 MoveOnto（MoveAdjacentTo 子类）的移动：FindPathToTargetCells 反向搜索，

## src/game/miniyaml.hpp
- L26 `std::string_view str_path, bool b_discard_comments_and_whitespace = true);`
  同上，但节点 SourceLocation 名用 str_name 而非物理路径（上游
- L27 `// 同上，但节点 SourceLocation 名用 str_name 而非物理路径（上游`
  MiniYaml.Load 的 FromStream(stream, manifest 条目字符串) 语义——Location

## src/game/miniyaml_merge.cpp
- L3 `#include "game/miniyaml_merge.hpp"`
  上游 OpenRA.Game/MiniYaml.cs Merge 管线（L408-698）语义等价移植。
- L4 `// 上游 OpenRA.Game/MiniYaml.cs Merge 管线（L408-698）语义等价移植。`
  行号注释均指上游文件。C++ 单层节点模型下，上游 MiniYaml(Value, Nodes)
- L5 `// 行号注释均指上游文件。C++ 单层节点模型下，上游 MiniYaml(Value, Nodes)`
  二层结构对应节点的 (str_value, vec_children)；上游 null MiniYaml 对应
- L6 `// 二层结构对应节点的 (str_value, vec_children)；上游 null MiniYaml 对应`
  "节点不存在"（空指针），其 Value 的 null 与空串等价（T04 惯例）。
- L8 `//`
  保序生命线：tree/resolved/结果列表一律 vector 首现序（上游 .NET Dictionary
- L9 `// 保序生命线：tree/resolved/结果列表一律 vector 首现序（上游 .NET Dictionary`
  插入序的事实等价），线性查找；绝不用 map 遍历序决定输出。
- L30 `}`
  List.RemoveAll(r => r.Key == key) 等价（返回删除数）
- L46 `}`
  上游 SourceLocation.ToString（MiniYaml.cs:68）= $"{Name}:{Line}"；
- L52 `}`
  上游 tree：Dictionary<string, MiniYaml>（键 → 顶层节点的 value/children 载体）。
- L65 `}`
  循环检测沿递归传递的集合（上游 ImmutableDictionary，值语义：兄弟分支不共享）
- L75 `// -K 标记经集合级 MergePartial 透传、由 ResolveInherits 强删除消费。`
  返回 nullopt = 无删除标记（视图与原列表相同，上游 ret ?? nodes 短路）。
- L96 `}`
  —— IntoDictionaryWithConflictLog（Exts.cs:414-465）消息复刻 ——
- L112 `if (node.str_key.empty()) continue;`
  上游丢弃 null key
- L143 `}`
  上游 MiniYaml（Value + Nodes）的值语义载体
- L154 `// existing/override 为 null（节点不存在）时空指针。null 侧跳过视图检查；`
  existing == null 返回 override 原样（其子层未做弱删除——上游返回原始对象）。
- L178 `if (!res_children.has_value()) return std::unexpected(res_children.error());`
  override 非空值胜出（上游 overrideNodes.Value ?? existingNodes.Value；
- L200 `if (node.str_key.empty()) return {};`
  上游 null key 跳过
- L238 `// 首现保留位置；后现节点经 MiniYaml 级 MergePartial 合并进首现（壳用首现）。`
  不解析继承、不处理 '-'（上游 L505 注释明言）。
- L268 `// —— MergeIntoResolved（L437-457）：解析列表内同键合并 + 合并后立即递归解析继承 ——`
  上游 existingNodeKeys 为 HashSet（纯存在性）；existingNodes 索引取正序首个。
- L309 `if (vec_nodes.empty()) return vec_nodes;`
  上游空数组短路
- L359 `const std::vector<std::vector<MiniYamlNode>>& vec_sources) {`
  上游 Merge（L408-435）总控
- L363 `// 每源 MergeSelfPartial → 按清单序 Aggregate(集合级 MergePartial)`
  （单元素序列不调 func，直接作种子——LINQ Aggregate 语义）
- L377 `}`
  null key 过滤 → tree（上游 ToDictionary：重复键报错；K,-K,K 顶层形态会走到
- L391 `std::string str_value;`
  顶层节点自身 Value 保留（上游 kv.Value.Value）
- L405 `// 顶层删除解析：节点键/值来自 resolved，Location/Comment 为 default`
  （上游 new MiniYamlNode(kv.Key, kv.Value)）；inherited 为空（无种子）

## src/game/miniyaml_merge.hpp
- L9 `namespace ora::game {`
  上游 MiniYaml.Merge（OpenRA.Game/MiniYaml.cs:408-435）语义等价移植。
- L12 `// （保序是字节稳定生命线：输出顺序 = 清单序 + 首现序，禁 map 遍历序）。`
  五组件语义：
- L22 `// 数据错误（同侧重复键 / 父缺失 / 循环继承 / 无元素可删 / 顶层重复键）→`
  expected 错误串，消息复刻上游 YamlException 格式（D004 口径：异常→expected）。

## src/game/network/order.cpp
- L106 `if (u4_shift >= 32) return std::nullopt;`
  长度溢出（上游 int 界）

## src/game/network/order_manager.cpp
- L134 `}`
  返回 true = 与参照不匹配（OutOfSync 语义：syncForFrame 首条为参照，同帧后续比对）
- L152 `if (om.ptr_replay_feed != nullptr) {`
  重演（ReplayConnection.Receive 语义）：本地 sync 先排干成参照，再按
- L206 `if (om.ptr_recorder != nullptr)`
  记录「应用帧」= 发出帧 + 投影（NetworkConnection 走服务器 Ack 帧，Connection.cs:344-345）
- L235 `if (om.ptr_recorder != nullptr)`
  SendSync → 17 字节 sync 记录（Connection.cs:302）
- L259 `if (om.ptr_recorder != nullptr)`
  StartGame → [1] pacing 空包（服务器开局注入，Connection.cs:52-56 + Server.cs:1449-1466）
- L326 `world::WorldModel& w) {`
  1. 本地 sync 先排干（重演中 ProcessOrders 产生的 SendSync 入队，首到者成参照）
- L346 `om.vec_echo_sync.clear();`
  2. chunk 按 Frame <= NetFrameNumber + OrderLatency 投喂（ReplayConnection.cs:111）
- L358 `if (b_mismatch) {`
  local 侧值：本地已算出则用本地；否则用参照（首到者=先到的另一份录中包）

## src/game/network/order_manager.hpp
- L38 `std::int32_t int4_sent_orders_frame{};`
  syncForFrame 字典（OrderManager.cs:161-170）：每帧首份 (hash, defeat) 为参照，后续比对
- L43 `std::string str_fatal;`
  录制挂接（ReplayRecorder 语义，非拥有；null=不录制）
- L45 `replay::ReplayRecorderModel* ptr_recorder{};`
  重演挂接（ReplayConnection 泵语义，非拥有；非空时本地发单忽略、订单按帧从文件灌回）
- L61 `// 供 replay::replay_feed_pump（重演投喂）复用 receive_orders/receive_sync`
  （T16 内部面；返回 true = 与 syncForFrame 参照不匹配）

## src/game/pathfinding/astar.cpp
- L53 `bool b_lane_bias;`
  完整反向模式（FindPathToTargetCells 语义：initial=target、goal=from、lane 翻转、
- L82 `std::int32_t path_cost_to_node(SearchState& state, CPos cell_dest, CellVec vec_dir) {`
  上游 GetPathCostToNode：反向搜索时允许进入不可达的 source 格（被 actor 自己
- L83 `// 上游 GetPathCostToNode：反向搜索时允许进入不可达的 source 格（被 actor 自己`
  占据，MovementCostToEnterCell 判 Unreachable），代价按 0（PathSearch.cs:186-190）。
- L97 `if (state.b_lane_bias) {`
  上游 DensePathGraph.GetConnections（:215-216）：ux = neighbor.X + (inReverse ? 1 : 0)
- L203 `state.vec_cells.assign(static_cast<std::size_t>(map.int4_width) * map.int4_height, CellInfoNode{});`
  上游 PathFinder.FindPathToTargetCells：多目标反向搜索——以 targets 为初始源、
- L225 `}`
  反向搜索的回溯序 [goal..initial] 与正向相反；上游 FindPathToTargetCells 在
- L237 `namespace {`
  Grid.TilesByDistance（MapGrid.cs）：圈 = ISqrt(i²+j², Ceiling)，圈序 (len², hash, x, y)
- L303 `state.vec_cells.assign(static_cast<std::size_t>(map.int4_width) * map.int4_height, CellInfoNode{});`
  AddInitialCells（PathSearch.cs:98-110）：多源各 cost 0 加入（可达过滤在调用方）
- L319 `return vec_path;`
  target→source（上游 ToTargetCell 返回序）

## src/game/pathfinding/astar.hpp
- L27 `// b_lane_bias_reverse：DensePathGraph 的反向 lane 偏置奇偶翻转（x+1 / y+1）。`
  返回路径 target→source（调用方 reverse 换回正向，与上游语义一致）。
- L32 `std::int32_t int4_weight, bool b_lane_bias, bool b_lane_bias_reverse);`
  MapGrid.TilesByDistance 序（len² → hash(x^y) → x → y，MapGrid.cs CreateTilesByDistance）
- L33 `// MapGrid.TilesByDistance 序（len² → hash(x^y) → x → y，MapGrid.cs CreateTilesByDistance）`
  的 annulus 偏移表照抄 + Map.Contains 过滤（FindTilesInAnnulus，Map.cs）。

## src/game/pathfinding/move_sim.hpp
- L56 `std::int32_t int4_speed{};`
  速度修饰链（上游 Util.ApplyPercentageModifiers 是 decimal 累乘后单次截断）：
- L62 `const CellBlockSet* ptr_set_blocked{};`
  上游 MoveOnto/MoveAdjacentTo 走 FindPathToTargetCells 反向搜索（targets 起搜）；
- L63 `// 上游 MoveOnto/MoveAdjacentTo 走 FindPathToTargetCells 反向搜索（targets 起搜）；`
  普通 MoveTo 是正向。两类搜索的 lane bias 与 tie-break 不同（PathFinder.cs:124）。

## src/game/pathfinding/wangle.cpp
- L85 `std::int32_t wangle_tan(std::int32_t int4_angle_raw) {`
  WAngle.Tan（WAngle.cs:89-107）：branchless 三角折叠 + 符号（表与 CosineTable 同源）

## src/game/production/production.cpp
- L532 `if (!res_spawn.has_value()) return false;`
  出厂帧 rng 账本（与上游逐字节对齐，positive/negative 黄金实测）：
- L535 `// ② 带 WithInfantryBody（IdleSequences 非空）的步兵出生同帧首次 idle 抽样`
  （WithInfantryBody.cs:207-208，INotifyIdle 的 Waiting→Idle 转换）：
- L537 `//    IdleSequences.Random → Next(序列数) + idleDelay → Next(Min,Max)。`
  渲染面账本——T14 动画面接管后应移位至彼处（偏离登记见 docs/COVERAGE.md）。
- L829 `if (actor.u4_owner >= w.prod.vec_players.size()) return;`
  上游 powerDrain 字典仅由 Power trait 的 AddedToWorld 事件驱动——无 Power trait

## src/game/replay/replay_format.hpp
- L9 `namespace ora::game::replay {`
  .orarep 容器层（ReplayRecorder.cs / ReplayMetadata.cs / ReplayConnection.cs 的读写面）。
- L33 `std::vector<MiniYamlNode> vec_meta_nodes;`
  元数据 yaml 节点树（保持上游字段序）
- L34 `std::int32_t int4_final_game_tick{};`
  Root.FinalGameTick 便捷抽取（回放计时口径）

## src/game/replay/replay_player.hpp
- L22 `constexpr std::int32_t kReplayOrderLatency{3};`
  chunk 聚合（ReplayConnection.cs:44-93 语义）：自上一常规帧包后的全部包
- L31 `std::deque<ReplayChunk> deque_chunks;`
  录中 sync 封包（frame → (hash, defeatState)）——逐帧对拍参照
- L33 `ReplaySessionInfo session;`
  SyncInfo 单解析（seed/帧间隔/client 表）
- L34 `bool b_valid{};`
  上游 IsValid：流内见过 StartGame 引导单
- L35 `std::int32_t int4_tick_count{};`
  上游 TickCount = 最大 frame（尾帧投影可比仿真帧多 1）
- L36 `std::int32_t int4_tick_count{}; // 上游 TickCount = 最大 frame（尾帧投影可比仿真帧多 1）`
  逐帧对拍累计：本地 ProcessOrders 点 (frame → (hash, defeat))（探针 SentSyncs 口径）
- L41 `std::vector<std::pair<std::int32_t, std::vector<std::uint8_t>>> vec_replayed_orders;`
  每帧 mismatch 行（探针 desync 报告格式复刻）：
- L46 `};`
  从 .orarep 构建重演 feed（ReplayConnection ctor 聚合语义；frame-0 包不投喂不建世界——
- L47 `// 从 .orarep 构建重演 feed（ReplayConnection ctor 聚合语义；frame-0 包不投喂不建世界——`
  无头宿主世界由调用方按 session 重建，与探针 ReplayPumpConnection 同构）。
- L51 `const ReplayFile& file);`
  每帧投喂（ReplayConnection.Receive 语义，OrderManager.connection_receive 的重演分支）：
- L52 `// 每帧投喂（ReplayConnection.Receive 语义，OrderManager.connection_receive 的重演分支）：`
  先排干本地 sync 队列（首到者成 syncForFrame 参照），再按 chunk.Frame <= NetFrameNumber +
- L53 `// 先排干本地 sync 队列（首到者成 syncForFrame 参照），再按 chunk.Frame <= NetFrameNumber +`
  kReplayOrderLatency 投喂——订单包入 per-client pending、sync 包与参照比对（不等 → desync

## src/game/replay/replay_record.cpp
- L25 `}`
  上游 FieldSaver 的 DateTime 格式（FieldSaver.cs:131-137）："yyyy-MM-dd HH-mm-ss" invariant。
- L37 `}`
  上游 IsGameStart（ReplayRecorder.cs:29-35）：frame-0 包的订单串里含 "StartGame"。

## src/game/replay/replay_record.hpp
- L12 `// —— Session（SyncInfo 引导包内容）最小面 ——`
  重演世界重建所需字段：seed / 地图 uid / 帧间隔 / client 列表。
- L13 `// 重演世界重建所需字段：seed / 地图 uid / 帧间隔 / client 列表。`
  上游默认 NetFrameInterval=3（Session.cs:222）——黄金与本录制器统一 1（t07 帧 1:1 模型）。
- L23 `};`
  上游 Session.Serialize yaml 格式的核心子集（字段序对齐探针实测输出）。
- L26 `[[nodiscard]] std::string replay_session_yaml(const ReplaySessionInfo& info);`
  解析（兼容上游全字段 Session yaml——黄金文件的 SyncInfo 单）：GlobalSettings 与 Client@N。
- L30 `std::string_view str_yaml);`
  —— GameInformation 元数据（ReplayMetadata.cs 写侧 / GameInformation.cs 字段序）——
- L38 `std::string str_start_time_utc;`
  "yyyy-MM-dd HH-mm-ss"（invariant，上游 FieldSaver.cs:131-137）
- L50 `[[nodiscard]] std::vector<MiniYamlNode> replay_metadata_nodes(const ReplayGameInfo& info);`
  —— 录制器（ReplayRecorder.cs 语义 + RecordingEchoConnection 挂接形态，探针 §3.2）——
- L55 `//   immediate → [0]；SendSync → 17 字节 sync 封包；引导包两枚由宿主手工注入（from=0 frame=0，`
  SyncInfo 在 StartGame 之前——后者触发上游 pre-start 落盘）。
- L59 `bool b_game_start_seen{};`
  上游 IsGameStart：收到含 frame-0 StartGame 单的包才落盘
- L67 `std::vector<std::uint8_t> vec_data);`
  元数据挂接（上游 World.LoadComplete 尾部 nc.Recorder.Metadata = ... 的等价物）。
- L70 `void recorder_attach_metadata(ReplayRecorderModel& rec, std::vector<MiniYamlNode> vec_nodes);`
  Dispose（ReplayRecorder.cs:102-117 等价）：EndTimeUtc = 当前 UTC 墙钟写入 Root 节点，
- L75 `ReplayRecorderModel& rec);`
  服务器引导包（Server.cs:1288 SyncInfo / :1428 StartGame，from=0 frame=0 的 immediate 单）。

## src/game/replay/replay_world.cpp
- L20 `// 类型面表:oracle isync-init.txt / isync-born.txt(反射 dump,全量 ra 规则 ore-lord)`
  每 ISync trait 构造序 + 成员初值参数。player/mpspawn/world 面在装配处手写。
- L123 `}`
  —— shroud 置位几何(Shroud.ProjectedCellsInRange 平地口径:ore-lord 本场景
- L166 `}`
  注入表(golden_gen ReplayNewModData 同源;t17 探针 t17 前缀键名不入 ActorID)
- L227 `// 单 trait 哈希(成员 XOR;初值/运行期状态按 kind 分派)。player 面由调用点展开`
  (每队列实例独立取 b_queue_enabled[idx])。int4_instance = 同型 ISync trait 的
- L247 `actor.int4_ch_damage[std::size_t(int4_instance)];`
  f7 实测 damageTicks=125
- L318 `// 地图序在 replay_oracle_create 时由 oramap 读出;此清单仅注入+mcv 尾段(完整清单`
  由测试用黄金 init 行断言)。
- L365 `if (entry.str_type == "mpspawn" && !b_mpspawn_seen) {`
  mcv 出生点 = 首个 mpspawn(黄金 init 148:mcv Mobile FromCell 15,15)
- L414 `PlayerInit{"Multi0", 2000, 50},`
  proc 在场→容量 2000;装配期 drained 50(黄金 init)
- L441 `return std::unexpected(res.error());`
  mcv(SpawnStartingUnits 等价:BaseActorFacing=512)
- L445 `return std::unexpected(res.error());`
  装配期 SharedRandom 消耗 4(SpawnMapActors 内,来源未逐 trait 定位;oracle 实测
- L446 `// 装配期 SharedRandom 消耗 4(SpawnMapActors 内,来源未逐 trait 定位;oracle 实测`
  rng last=1411461475 count=4 —— 确定性常量预推进,登记 COVERAGE)
- L501 `e1.b_wib_quirk = true;`
  首帧 CurrentMovementTypes 误含 Horizontal(f1 B 弹回 Waiting 的实测路径)
- L516 `// —— B 段(ITick;矿 SeedsResource RandomWalk ×28 = 全量世界确定性常量,`
  no-inject 26 + 注入建筑足迹扰动 +2;RandomWalk 真复刻 = 子面 4)——
- L684 `actor.u4_owner = 2;  // Multi0(barr 生产者)`
  出生面 = 黄金 dump 数据常量:pos (22358,11050)(f189 首变化 old)、facing 619
- L696 `w.vec_actors.push_back(std::move(actor));`
  WIB Created ×2(主 + @PARACHUTE:各 1 消耗 idleDelay=Next(30,110);f187 实测)
- L714 `actor.int4_frozen_vis = 12;`
  黄金 f552 反推(出生即置位:Neutral/Creeps 视角位包)
- L764 `std::from_chars(tok_frame.data(), tok_frame.data() + tok_frame.size(), int4_frame);`
  146 移动窗末帧(末次 Mobile 变化;攻击排队同帧链接的锚点)——须在 move 前
- L808 `std::vector<FireDamage> vec_fires;`
  帧尾结算(实测伤害落点在 B 段后)
- L810 `std::vector<FireDamage> vec_fires;  // 帧尾结算(实测伤害落点在 B 段后)`
  —— 数据面:Mobile 预应用(上游 actor 序:146 的本帧位移先于 147 重瞄)——
- L820 `// Move 活动(146,f16-末帧):busy——无扫描无 WIB;末帧与排队 Attack 同帧链接`
  (上游活动链语义:f77 移动末步 + 转向+IsAiming+开火实测)。
- L849 `} else {`
  攻击活动 tick(f7 实测:转向+IsAiming+开火同一活动 tick;开火消耗 4 =
- L857 `actor.b_activity = false;`
  目标亡:活动终止(f108 实测:147 归 idle,f109 扫描)
- L858 `actor.b_aiming = false;`
  IsAiming 随活动终止复位(f108 黄金实测 True -> False)
- L869 `actor.int4_reload = 20;`
  含开火帧在内的 20 次 B 段递减 → 恰 +20 帧再开火(f7→f27 实测)
- L885 `if (actor.int4_reload > 0) --actor.int4_reload;`
  WIB ITick 状态迁移(f2 实测:Moving → !Horizontal → PlayStand → Waiting)
- L889 `if (actor.str_type == "harv" && int4_frame >= 2 && int4_frame < 15 && actor.int4_facing < 512) {`
  采矿转身(f2-f14 实测 +20/帧,目标朝向 512=矿区正南;上限 512)
- L894 `}`
  —— 经典生产队列分期扣款(ProductionItem.Tick + 低电桶逐字复刻)——
- L907 `replay_oracle_spawn_e1(w);`
  步兵 OnComplete:帧尾出生(黄金 actor+ f187)
- L931 `// —— 帧尾:伤害结算(B 段后;1500 站立/750 卧倒已定,TriggerProne → 50)——`
  击杀:HP ≤ 0 → 移除(f107 实测;经验/玩家击杀计数据面)。
- L935 `if (victim.int4_ch_damage[0] == 0) {`
  ChangesHealth damageTicks 首伤置位(黄金实测仅 f7/f8)
- L967 `if (order.str_order == "StartProduction" && order.opt_str_target.has_value()) {`
  成本/工期 = 黄金 prod-trace 实测(GetBuildTime 产物;低电产能不减工期面)
- L987 `if (order.str_order == "Move" && order.b_has_subject && order.u4_subject == 146) {`
  146 移动(黄金 f16 应用):攻击活动中断 → Move 活动(busy;轨迹数据面)
- L996 `order.target.kind == network::TargetKind::kActor) {`
  146 攻击排队(黄金 f26 应用):移动完成后链接(末帧同帧转向+IsAiming+开火)
- L1004 `order.target.kind == network::TargetKind::kTerrain && order.target.b_cell) {`
  powr 放置(黄金 f551 应用):帧尾出生(位置 = 订单格真机制;frozen 面 = 黄金 12)
- L1023 `}`
  段2:ISync trait((ActorID, 构造序);world/player 面展开)

## src/game/replay/replay_world.hpp
- L10 `namespace ora::game::replay {`
  —— ore-lord 黄金世界的重演面(T17/issue #18)——
- L12 `//`
  口径:golden_gen replay 模式的 oracle 世界 = ore-lord 官方图 + 全量 ra 规则(21 渲染
- L13 `// 口径:golden_gen replay 模式的 oracle 世界 = ore-lord 官方图 + 全量 ra 规则(21 渲染`
  trait 剥离)+ 注入 5 actor(fact/proc/barr/e1/e1)+ mcv(SpawnStartingUnits),seed 20261103,
- L15 `// NetFrameInterval=1,580 帧(调研 t16-t17 §3.2 配方)。本模块按同一装配序重建世界的`
  ISync 面(SyncHash 五段式的段2 贡献面),oracle 侧字段级规格见
- L16 `// ISync 面(SyncHash 五段式的段2 贡献面),oracle 侧字段级规格见`
  .ora-explore/t17-analyze/out/{isync-init,isync-born,pertrail-*}.txt(探针反射 dump)。
- L18 `//`
  段2 逐 trait 哈希规则(Sync.cs GenerateHashFunc,行为级复刻):
- L23 `// 段5:单人局 DeveloperMode.Enabled=True → 全玩家 UnlockedRenderPlayer`
  (DeveloperMode.cs:159-161 NonBotPlayers.Count()==1)→ Σ HashPlayer 恒定。
- L25 `//   (DeveloperMode.cs:159-161 NonBotPlayers.Count()==1)→ Σ HashPlayer 恒定。`
  ISync trait 面(上游 trait 类型名的重演种类;枚举序即各类型内的构造序标记)
- L63 `std::vector<FaceTraitKind> vec_traits;`
  ISync trait 构造序
- L82 `bool b_disposed{};`
  —— 运行期 ISync 成员(变化字段;39 个变化字段的载体)——
- L105 `std::int32_t int4_activity_target{-1};`
  活动目标(非同步;Aggressor 仅受击还击才置,见 AutoTarget.cs)
- L123 `};`
  经典生产队列项目(ProductionItem + 低电桶逐字复刻;黄金 prod-trace 实测:
- L144 `std::vector<std::set<std::pair<std::int32_t, std::int32_t>>> vec_sight;`
  延迟开火特效日程(命中帧 +2/+4 各耗 4;值不入对拍面,时点入 seg4 账本)
- L148 `std::vector<OracleProdItem> vec_prod_items;`
  段3:ISync 效果(Bullet 弹道)——战斗期非空
- L150 `std::int32_t int4_hash{};`
  Bullet 的 ISync 哈希(弹道状态)
- L155 `};`
  —— oracle 数据面(黄金 isync-changes 重演)——
- L156 `// —— oracle 数据面(黄金 isync-changes 重演)——`
  机制未移植面(移动轨迹/采矿资源/经验/玩家聚合)由上游探针黄金 dump 逐帧权威应用,
- L158 `// 真机制面(AutoTarget/TakeCover/Health/AttackFrontal/ChangesHealth/现金扣款/队列波)`
  数据跳过——每字段单一驱动源,登记 docs/COVERAGE.md 偏离。
- L167 `struct OracleReplayContext {`
  frame → ISync 变化(黄金 ore-lord.isync-changes.txt 同构解析)
- L169 `std::map<std::int32_t, std::vector<OracleChange>> map_changes;`
  frame → SharedRandom 累计消耗(黄金 frames.txt rng 列;真机制消耗 + 差额闭合,
- L173 `// 146 Move 活动窗末帧(数据推导:146 末次 Mobile 变化帧 = f77;末帧移动完成与`
  排队 Attack 同帧链接——上游活动链语义,f77 转向+IsAiming+开火实测)
- L177 `};`
  从黄金 isync-changes + frames 文本构建重演上下文。
- L184 `[[nodiscard]] const std::vector<std::string>& replay_oracle_init_manifest();`
  装配(oracle BuildWorld + LoadComplete 子集的行为等价):
- L188 `//   RNG:装配期 4 个 SharedRandom 消耗(SpawnMapActors 内,来源未逐 trait 定位——`
  按确定性常量预推进,登记 docs/COVERAGE.md)。
- L192 `std::int32_t int4_seed, std::span<const std::uint8_t> span_oramap);`
  f1 行为(LoadComplete 后首帧的确定性事件;oracle isync-changes/栈级插桩实测):
- L195 `//   (1 消耗,shroud 未探索 → 无目标)、e1 147 同构;B 段(ITick):矿 SeedsResource`
  RandomWalk ×28(全量世界;no-inject 26,建筑足迹 +2——RandomWalk 复刻为子面 4)
- L198 `//   (frozen 集可见判定)+ FrozenActorLayer 4 玩家聚合 + 队列 Enabled 波。`
  重置值口径:扫描 raw = 3 + out%5,**同帧 B 段 ITick 再递减一次**(终值 = raw-1)。
- L201 `void replay_oracle_frame1(ReplayOracleWorld& w);`
  通用单帧(n ≥ 2,ctx 驱动):Mobile 数据预应用(上游 actor 序)→ A 段(活动:e1 攻击链
- L203 `// /146 移动窗;idle TickIdle:WIB/AutoTarget 扫描)→ B 段(ITick 递减)→ 生产队列分期`
  扣款 → 其余数据面应用 → 帧尾伤害结算 → RNG 账本闭合。逐帧与录包 sync(N+1) 对拍
- L204 `// 扣款 → 其余数据面应用 → 帧尾伤害结算 → RNG 账本闭合。逐帧与录包 sync(N+1) 对拍`
  (录包口径:sync(N) = N-1 帧末状态)。
- L208 `std::int32_t int4_frame);`
  订单应用(录包标签 = 应用帧:echo 下一帧投影;黄金实测 StartProduction powr@5
- L214 `// 生产出生链(e1 OnComplete@f187 / powr PlaceBuilding@f551,帧尾入世界):`
  位置/朝向 = 黄金 dump 数据常量(frozen 面 = f552 反推);订单格 = 真机制。
- L218 `void replay_oracle_spawn_powr(ReplayOracleWorld& w, std::int32_t int4_cell_bits);`
  World.SyncHash() 五段式(int 环绕;n 跨段连续;段2 按 (ActorID,构造序))。
- L221 `[[nodiscard]] std::int32_t replay_oracle_sync_hash(const ReplayOracleWorld& w);`
  段2 调试 dump:每 (actor, trait) 的 hash 与贡献(探针 pertrait-*.txt 同格式)。

## src/game/ruleset.cpp
- L9 `constexpr char kAbstractActorPrefix{'^'};`
  ToLowerInvariant 的 ASCII 面（语料全 ASCII；与 D005 同口径）
- L25 `if (!res_nodes.has_value()) return std::unexpected(res_nodes.error());`
  上游 new MiniYaml(null, MiniYaml.Merge([nodes])).ToDictionary()——
- L26 `// 上游 new MiniYaml(null, MiniYaml.Merge([nodes])).ToDictionary()——`
  mod.yaml 单文件也走完整 Merge（跨文件继承/删除语义），输出顶层键唯一，
- L27 `// mod.yaml 单文件也走完整 Merge（跨文件继承/删除语义），输出顶层键唯一，`
  线性查首个 "Rules" 与字典查找等价
- L40 `return std::vector<std::string>{};`
  上游 YamlList：区块缺失 → 空列表

## src/game/ruleset.hpp
- L10 `namespace ora::game {`
  上游 Manifest.Rules 提取（Manifest.cs:97-124 + YamlList:190-196）：
- L12 `// FromFile(mod.yaml, discard=true) → Merge([nodes]) → 查 "Rules" 键 → 子节点键`
  序列（保 Nodes 顺序；区块缺失 → 空列表）。Include 内联展开（Manifest.cs:98-111）
- L13 `// 序列（保 Nodes 顺序；区块缺失 → 空列表）。Include 内联展开（Manifest.cs:98-111）`
  不实现——ra 的 mod.yaml 无 Include，偏离登记 docs/COVERAGE.md。
- L17 `std::string_view str_mod_yaml_path);`
  上游 Ruleset.LoadDefaults（Ruleset.cs:118-164）最小面对象模型：
- L20 `// trait 完整键（含 @suffix）在合并树内唯一且顺序 = 继承展开序；@ 后缀多实例`
  是正常形态（上游 InstanceName 分裂 / FieldLoader 反射装配留给后续 trait 语义票）。
- L32 `// (Location 名如 "ra|rules/misc.yaml"，物理路径)；Location 名进 SourceLocation`
  （与上游 MiniYaml.Load 的 FromStream(stream, manifest 条目) 一致）。
- L33 `//（与上游 MiniYaml.Load 的 FromStream(stream, manifest 条目) 一致）。`
  小写化冲突 → 错误（上游 ToDictionaryWithConflictLog 消息为 .NET 集合拼接，
- L34 `// 小写化冲突 → 错误（上游 ToDictionaryWithConflictLog 消息为 .NET 集合拼接，`
  近似复刻，ra 语料 0 触发，登记 docs/COVERAGE.md）。

## src/game/world/world.cpp
- L332 `}`
  SpawnStartingUnits 等价（mcvonly）：MCV 落 mpspawn。rng 账本 0 耗——
- L336 `// 规则集未定义 mcv（经济等无 SpawnStartingUnits 的最小规则集）时跳过——`
  上游无该 trait 的规则不生成起始单位。

## src/launcher/main.cpp
- L282 `}`
  T16 --record 引导段：SyncInfo/StartGame 两枚 from=0 引导包（Server.cs:1288/:1428 同型）
- L283 `// T16 --record 引导段：SyncInfo/StartGame 两枚 from=0 引导包（Server.cs:1288/:1428 同型）`
  + [1] pacing 空包（session_setup 已 start_game，此处补记探针 StartGame 挂接点的等价物）
- L940 `int run_headless(const lch::CliArgs& args, std::ofstream* ptr_log, char* sz_argv0) {`
  T16 --replay：先读 .orarep 构建重演 feed（世界按文件 Session 的 seed 重建）
- L984 `// T16 --record：录制器挂接（session_setup 已 start_game——[1] pacing 空包在此补记，`
  与探针 RecordingEchoConnection.StartGame 的记录点等价）
- L1023 `std::println("{}", lch::session_hud_line(s));`
  T16 --record：Dispose 落盘（EndTimeUtc=当前 UTC 墙钟——记录流恒等口径不受影响）
- L1037 `}`
  T16 --replay：desync 检出面（逐帧对拍报告 / om 标志）


## tests
## tests/cli_game.cpp
- L14 `// T15（issue #16）：CLI 直进沙盒对局的无头可测内核。`
  seam 1：GNU 风格参数解析（--mod/--map/--seed/--tick-ms/...，上游 Key=Value 语法
- L15 `// seam 1：GNU 风格参数解析（--mod/--map/--seed/--tick-ms/...，上游 Key=Value 语法`
  偏离登记 COVERAGE）；seam 2：内置沙盒图组装时序（玩家/actor/资源层/队列）；
- L16 `// 偏离登记 COVERAGE）；seam 2：内置沙盒图组装时序（玩家/actor/资源层/队列）；`
  seam 3：同 seed 双跑 N tick digest 恒等 + 建造/生产/移动/攻击/经济闭环锚点断言。
- L126 `// 按 owner 视角取 e1：b_own=true 己方（viewer）第 idx_skip 个，false 敌方第 idx_skip 个。`
  vec_in_world 按 id 序：地图 e1own/e1foe 在生产 e1 之前，idx 语义必须走 owner。
- L385 `env.int4_seed = res_feed->session.int4_seed;`
  文件 Session 的 seed

## tests/combat.cpp
- L24 `// T12（issue #13）：攻击/战斗/死亡闭环。`
  单元部分：武器表解析（黄金 weapons yaml → WeaponDef）、三条独立 decimal
- L26 `//   取整链（DamageVersus 100 基数链 → InflictDamage 链 → Health victim 修饰链，`
  每链末一次截断）锚点单测。
- L27 `//   每链末一次截断）锚点单测。`
  剧本部分：四剧本交战至死亡逐 tick 对拍 tests/golden/combat/duels.txt +
- L77 `}`
  —— 武器表装载：manifest 清单 + 逐文件（保序）解析合并（上游 MiniYaml.Load 同路径）——
- L183 `using namespace ora::game::combat;`
  链语义锚点 1：M1Carbine 命中 None 甲（Versus None=150）——
- L190 `check(apply_percentage_modifiers(1000, {100, 150}) == 1500, "chain2 1000*100%*150%");`
  链语义锚点 2：25mm 命中 None 甲（Versus None=32）——
- L194 `check(apply_percentage_modifiers(2500, {100, 32}) == 800, "chain2 25mm 800");`
  链语义锚点 3：155mm 命中 None 甲（Versus None=60）带 falloff 70 ——
- L195 `// 链语义锚点 3：155mm 命中 None 甲（Versus None=60）带 falloff 70 ——`
  23000 × 70 × 60 / 10000 = 9660 —— 三链独立 vs 一次合并的差异锚点：
- L199 `check(apply_percentage_modifiers(23000, {70, 60}) == 9660, "chain2 155mm 9660");`
  取整方向锚点：decimal 向零截断（非四舍五入、非向下取整——负数亦向零）
- L202 `check(apply_percentage_modifiers(10, {33, 33}) == 1, "10*33%*33% -> 1 (chain, no per-step)");`
  若逐步取整：10*33%=3（截），3*33%=0 —— 链内不逐步截断得 1。这是链语义的关键差异。
- L205 `check(apply_percentage_modifiers(100, {7}) == 7, "100*7% -> 7");`
  DamageVersus 链独立截断后传 int 的锚点（DamageWarhead.cs:72-90）：
- L211 `check(apply_percentage_modifiers(100, {33, 33}) == 10, "chain1 versus 100*33%*33% -> 10");`
  Health victim 修饰链（Health.cs:172-187）：appliedDamage decimal 链末截断。
- L245 `}`
  Falloff 链锚点（SpreadDamageWarhead.GetDamageFalloff + int2.Lerp 向零截断）：
- L266 `namespace pfg = ora::game::pathfinding;`
  duelD 接近寻路：ore-lord 地形 + foot locomotor（探针 Locomotor@FOOT 同参）
- L332 `}`
  —— 剧本对拍（探针 RunScenario 等价驱动）——
- L343 `};`
  生成与探针 dump 同序的行（spawn + 逐 tick hit/death/removed/tick/proj）
- L448 `};`
  黄金按 scenario 切片（spawn 行起）

## tests/economy.cpp
- L640 `}`
  —— 剧本锚点断言（黄金语义抽查；逐字节对拍见 test_golden_compare）——

## tests/input_logic.cpp
- L14 `// T14（issue #15）：输入交互与基础 HUD 侧栏的无头测试。`
  合成世界 + 合成 MouseInput → 断言选择集/订单（T08 serialize 字节对拍）/面板状态机。

## tests/pathfind.cpp
- L437 `// movepair（(35,17)→(12,12)）刻意不在交叉表：该远距对上 oracle HPF 走抽象层，`
  路径与局部 A* w100/w125 均不同（黄金 hpf movepair 行即实证），等价性断言
- L438 `// 路径与局部 A* w100/w125 均不同（黄金 hpf movepair 行即实证），等价性断言`
  不成立——HPF 复刻是 T12+ 面，登记 docs/COVERAGE.md D025。

## tests/pathfind_move.cpp
- L333 `std::format("move: consumed {} of {} golden tick rows", idx_gold, vec_gold_tick.size()));`
  第二段 (35,17)→(28,10)：近距西北对角（= paths.txt 的 moveturn 对，实测
- L335 `// oracle A100==A125==HPF 逐格一致）→ 135° Turn + FirstHalf 弧线链。原定远距对`
  (12,12) 上 oracle HPF 走抽象层、与局部 A* 不同路径（D025），逐 tick 对拍不可行。

## tests/production.cpp
- L10 `// T10（issue #11）：建造/生产闭环含电力门槛。`
  口径修正（调研实证）：上游 ra 语义是电力不足生产 ×LowPowerModifier/100 减速
- L12 `// （LowPowerModifier=300 → 每 3 tick 推进 1），非停摆；真停摆仅缺钱/出口堵/`
  生产建筑被禁用。ProductionItem.Tick 的令牌桶照上游实码（Slowdown-100，

## tests/replay.cpp
- L18 `// T16（issue #17）：回放记录与读放、字节恒等。`
  恒等口径三层（调研 t16-t17 §0.5/§5.1）：
- L21 `//   ②读→写整文件恒等（replay_write(replay_read(bytes)) == bytes，记录流透传 + 元数据 yaml 节点级往返）`
  ③规范化恒等（钉死时间戳）——oracle golden_gen 黄金生成侧口径
- L22 `//   ③规范化恒等（钉死时间戳）——oracle golden_gen 黄金生成侧口径`
  .orarep 格式（ReplayRecorder.cs:87-89 / ReplayMetadata.cs:53-70）：
- L140 `using namespace ora::game::replay;`
  无元数据块（服务器 desync 自毁形态——上游 ReplayMetadata.Read 返 null 即不可用文件）
- L161 `}`
  —— slice 1c：oracle 黄金 .orarep（上游真录制文件，探针 scenario-normalized）的
- L202 `"golden: first pacing packet [1][]");`
  sync 封包形态（17B、0x65）：黄金内首条 sync 在 frame 1
- L207 `"golden: sync packet 17B/0x65");`
  读→写整文件字节恒等（真实上游元数据 yaml 走 C++ MiniYaml 节点级往返）
- L228 `//`
  世界：fact/barr@Multi0 + e1 攻击对（Move/Attack 形态单）+ RngProbe/SyncProbe（SyncHash 演化）。
- L449 `ora::game::replay::ReplayFeedModel feed;`
  重演侧：逐帧对拍数据（录制侧为空）
- L514 `net::run_frame(om, w);`
  本帧 SentSync（探针口径）：ProcessOrders 尾部产的 sync 包恰在 echo 队尾（下一拍才排干）
- L659 `}`
  —— slice 3：自录自放逐帧 SyncHash 自洽 + 双跑记录流恒等 ——
- L683 `check(res_a->str_dump == res_b->str_dump, "record-dump-identical (double run)");`
  ②自放：重演逐帧 SyncHash 与录中 sync 封包比对（feed 机制 + 显式对拍）
- L804 `} else {`
  上游机制双验：om.b_out_of_sync（syncForFrame 比对）或显式逐帧对拍报告

## tests/replay_oracle.cpp
- L18 `#endif`
  T17(issue #18,M1 收口):oracle 真回放逐帧 SyncHash 对拍。
- L19 `// T17(issue #18,M1 收口):oracle 真回放逐帧 SyncHash 对拍。`
  黄金 ore-lord.orarep = golden_gen replay 模式(ore-lord + 全量 ra 规则 + 注入剧本 5 actor
- L20 `// 黄金 ore-lord.orarep = golden_gen replay 模式(ore-lord + 全量 ra 规则 + 注入剧本 5 actor`
  + mcv,seed 20261103,NetFrameInterval=1,580 帧)。C++ 侧重演 = replay_world 面重建世界
- L21 `// + mcv,seed 20261103,NetFrameInterval=1,580 帧)。C++ 侧重演 = replay_world 面重建世界`
  (ISync 段2 逐 trait 字段级)+ 逐帧 SyncHash 与录中 sync 封包对拍 + desync 负向检出。
- L51 `}`
  黄金 frames.txt 的 init actors 行(装配序 oracle 实测;装配序对拍锚点)
- L60 `}`
  全帧重演对拍(desync 负向重跑):返回首分歧记录 sync 帧(0 = 全对)。
- L61 `// 全帧重演对拍(desync 负向重跑):返回首分歧记录 sync 帧(0 = 全对)。`
  数据面 ctx 共享(黄金 dump 不受录包篡改影响);订单流从(可能篡改的)记录重解析。
- L96 `}`
  —— slice 1:装配序与 f1 静态对拍(sync(1) = 装配后世界五段式)——
- L126 `const ReplayOracleWorld& w{*res_world};`
  装配序:149 个初始 actor 与黄金 init 行逐项一致(id:type:owner)
- L152 `if (b_order_ok) check(true, "assembly order matches golden init");`
  f1 静态对拍:sync(1) = 装配后世界(ProcessOrders@f1,harv 未生、置位波未发)
- L155 `// 调试 dump 门控(ORA_PERTRAIT_DUMP_PREFIX=<路径前缀>:逐帧写 <前缀>-f{n}.txt,`
  与探针 pertrait-*.txt 同格式 diff 用;不设则不写,不落仓库垃圾文件)
- L174 `std::format("frame-1 sync hash: got {} want {}", int4_hash, it_sync->second.first));`
  slice 3:f1-f580 逐帧对拍(录包口径 sync(N) = N-1 帧末状态;frame1 = f1 末事件)。
- L194 `}`
  oracle 数据面上下文(黄金 isync-changes + frames 账本)
- L260 `"all 580 recorded syncs compared (init f1 + frame ends f1-f579; f580 end = 未录 sync581)");`
  —— desync 负向(当前实现面):篡改录中 frame-2 sync 封包 hash 字节 → 与本地
- L261 `// —— desync 负向(当前实现面):篡改录中 frame-2 sync 封包 hash 字节 → 与本地`
  重演 hash 对拍必然分叉(检出)。订单 subject/物品名负向 = 全帧重演重跑(下方)。
- L292 `}`
  —— desync 负向全帧重演(三态):订单 subject 篡改 / prod-name 篡改 ——

## tests/rules_load.cpp
- L15 `// T05（issue #6）：ra 规则加载最小面。单元用例驱动 miniyaml_merge 的`
  Merge 五组件语义（上游 OpenRA.Game/MiniYaml.cs Merge/MergeSelfPartial/
- L17 `// MergePartial/ResolveInherits/MergeIntoResolved/WeakResolveRemovals），`
  黄金部分对拍 oracle 产物（tests/golden/rules/）。
- L30 `}`
  以固定 Location 名 "m" 解析（discard=true，与上游 MiniYaml.Load 同参）
- L36 `}`
  二进制整读（黄金文件字节保真，防 CRLF 翻译）
- L49 `}`
  黄金 manifest 清单按行切（LF 结尾）
- L192 `{`
  强删除无命中：错误消息复刻上游格式（{name}:{line}: There are no elements ...）
- L201 `{`
  父不存在：错误消息复刻
- L256 `{`
  同侧重复键（删除标记之后两个同名）：错误消息复刻
- L312 `{`
  manifest_rules_files：mod.yaml Rules 区块 → 有序清单，与 oracle 黄金逐行比对
- L329 `{`
  缺 Rules 区块 → 空列表（上游 YamlList 语义）；文件缺失 → 错误
- L339 `{`
  ===== ra 主线全量加载（验收锚点：307 actor / 13850 trait / 68 抽象）=====
- L343 `const std::string str_ra_dir{std::string{ORA_MINIYAML_GOLDEN_DIR} + "/ra"};`
  manifest 条目 → (Location 名, 物理路径)：黄金目录平铺，剥 ra|rules/ 取 basename
- L358 `}`
  计数断言 + 逐 actor 与 oracle stats 黄金对拍（顺序 = Merge 输出序）
- L399 `}`
  全量合并树对拍：顶层 375（含 ^68）、Inherits/- 残留 0、1MB 字节恒等
- L427 `check(sz_removals == 0, "no removal markers survive the merge");`
  与上游 WriteToString 黄金 1MB 字节对拍（报首差偏移）

## tests/world_tick.cpp
- L8 `// T07（issue #8）：World/Tick/Actor/Trait 最小面与无头自洽。`
  确定性三支柱（上游 World.cs:33 / TraitDictionary.cs:152-158 /
- L9 `// 确定性三支柱（上游 World.cs:33 / TraitDictionary.cs:152-158 /`
  ActorInfo.cs:104-167）：Actor 遍历序 = ActorID 升序；同 actor trait

## src/mods/common/wav_loader.cpp
- `wav_try_parse`
  上游 TryParseSound 全路径吞异常（WavLoader.cs:41-44），C++ 出口统一 nullopt。两处
  退化路径与上游不等价（零语料、不可对拍，登记于此备查）：①fmt 在但无 data chunk
  ——上游 LoadSound 仍返回 true（GetPCMInputStream 读时才炸），C++ 判 nullopt；
  ②IMA 的 blockAlign<=0——上游 numBlocks 除零炸在惰性工厂里，C++ 判 nullopt。
- `lengthInSeconds` 分母为 0（channels/rate/bits 全 0 的畸形头）时上游 float 除零得
  Infinity，C++ 保守跳过除法保持 -1（UBSan -fsanitize=undefined 含
  float-divide-by-zero，不能照抄；该值只进 MusicInfo.Length 截断，退化路径不可观测）。
- 块跳转 `s.Position += chunkSize - 16` 的 uint32 环绕语义照抄（-fwrapv 下 size_t
  加法放大后循环条件自然终止，与上游 Position 越界停循环等价）。

## src/mods/common/ima_adpcm.cpp
- `decode_ima_adpcm_sample`
  StepTable 89 项 / IndexAdjust[-1,-1,-1,-1,2,4,6,8] 逐值照抄
  （ImaAdpcmReader.cs:18-31）；delta = StepTable[index]*b/4 + StepTable[index]/8
  为**整型截断除法**；整字节传参（低半字节符号位看 bit3，高位忽略——上游
  `b & 8` / `b &= 7` 掩码行为）。
- `load_ima_adpcm_block`
  每输入字节产 4 输出字节（低半字节先）；高半字节传 `(byte)(b >> 4)`。

## src/game/sound_info.cpp
- `sound_info_from_yaml`
  上游 Voices/Notifications 池字典重复键在 ParseDictionary 的 Dictionary.Add 抛
  ArgumentException；C++ 归一为 expected 错误（D016 FieldLoader 异常→错误串口径），
  消息为近似串（.NET 运行时消息不可逐字符移植）。
- `parse_interrupt_type`
  上游 Enum.TryParse 接受任意可装下底层类型的数字（含未定义值如 3）；C++ 只收
  0/1/2（语料仅出现具名值），其余拒绝——与上游"接受后永不等于具名值"的观测差异
  只在畸形 yaml 出现。
- `string_to_lower_ascii`
  上游 ToLowerInvariant 是 Unicode 文化感知；C++ ASCII 小写化（D011 tileset 先例，
  语料全 ASCII）。

## src/game/music_info.cpp
- `music_info_from_yaml`
  Hidden 走 bool.TryParse 语义（仅 true/false 全大小写，非法静默 false 不报错）；
  Title 取节点标量值原样不 trim；Filename 的 key 用**原始大小写**（字典键才小写化，
  U071）。

## src/platforms/openal_loader.cpp
- `load_openal`
  探测序：`ORA_OPENAL_DLL` 环境变量（调研建议位，测试经 openal_probe_override
  注入等价路径）→ exe 目录 soft_oal.dll → exe 目录 openal32.dll → LoadLibrary 默认
  搜索（soft_oal/openal32）；符号缺失整库作废继续下一候选（上游 DllImport 缺符号
  是 EntryPointNotFoundException 崩溃，D057①）。
- windows.h 边界 TU（D056 豁免）：std 声明经 openal_loader.hpp 的 `import std;`
  进入，仅 <windows.h> 走 #include。

## src/platforms/sound_engine.cpp
- AL 常量**实证修正**：AL_POSITION=0x1004、AL_VELOCITY=0x1006、AL_GAIN=0x100A
  （soft_oal.dll alGetError 实测 0x1006 报 AL_INVALID_ENUM(0xA002)、0x100A 设读
  恒等）——调研笔记 t18-audio.md §A.5 常量表此三项有误，以其为准者须改。
  AL_ORIENTATION=0x100F、EFX AL_METERS_PER_UNIT=0x20004、ALC_DEVICE_SPECIFIER=
  0x1005、ALC_ALL_DEVICES_SPECIFIER=0x1013 实测无误。
- `try_get_source_from_pool`
  收割路径改写 slot.snd（shared_ptr 置空）+ 缓冲 id 归零，等价上游
  slot.SoundSource=null/slot.Sound=null；被收割 ISound 的 done=true 经共享
  sound_state 传导给调用方持有副本（上游引用语义的值语义等价物）。
- `engine_play_2d`
  atten 公式 `(256 - activeCount*0.5f)/256` 用 float 精确常量 256.0f 展开保持
  IEEE 序；同源限 3 判定用池槽 buffer id 代替上游 SoundSource 对象引用恒等
  （每个 AddSoundSourceFromMemory 恰生成一个 buffer）。
- `engine_dispose`
  alDeleteSources 传实际池大小（上游传 PoolSize=256 常量与数组实长脱钩，U070①）。

## src/game/sound/sound.cpp
- `sound_facade_tick`
  上游 Sound.Tick 的 MusicPlaying&&Complete→StopMusic+onMusicComplete 轮询形
  （Sound.cs:217-225）；M2 无音乐（music nullopt 空转），.aud 票接 PlayMusic 时
  补 onMusicComplete 回调位。

## src/game/input/hotkey.cpp
- kKeycodeNames 与 Keycode 枚举由上游 OpenRA.Game/Input/Keycode.cs 生成脚本对齐
  （238 项，含 MOUSE4/MOUSE5 与扩展键 N|(1<<30)；SDL 2.0.1 表）——再生成时两处必须
  同步，枚举序即表序。
- C# Enum.TryParse 对 flags 的逗号分段逐 token trim：`"  Ctrl"`（首段带空白）可解析，
  `"B C"`（无逗号整体一段）不可——modifiers_from_string 的 trim 位置照此。

## src/game/input/hotkey_registry.cpp
- GetFirstDuplicate 遍历序 = 装载序（上游 .NET Dictionary 无删除保插入序）；C++ 用
  平行 vector 保序，黄金 dump 行序即装载序，勿改按名排序输出。
- hotkey_set 的 `value != Default` 用 hotkey_equal（左操作数 UNKNOWN 否决）——
  Set(name, Invalid) 对 Default=Invalid 的定义恒"不等"→ 写 `UNKNOWN None` 节点，
  上游 quirk 照抄（U074）。

## src/game/input/selection_logic.cpp
- BaseSelectionPriority 两条 if 顺序无害（Ctrl&&!Alt 与 Alt&&!Ctrl 互斥）；越权置
  MaxValue 后关系扣减照减（敌方越权 = MaxValue-90）——测试勿假设"越权=全选一锅"。
- 双击选同类的 eligible 过滤用玩家集（观战者=全体），不做 owner==viewer 硬过滤；
  空地双击（无命中/owner 不 eligible）不改选区。

## src/game/input/order_generator.cpp
- targeter_accepts 按 str_order_id=="Attack" 分支是 AttackBase targeter 特有的
  ForceMove 否决/ForceAttack 打地形公式；Move targeter 不拒 ForceMove（requireForceMove
  ra 无用法）。新增 force 敏感 targeter 时在此扩展，勿改 TraitSpec 公共面。

## src/game/input/viewport_controller.cpp
- 滚动键置位条件 `Down && (isHotkey || 绑定无修饰键)` 是上游 quirk（U079）：Alt+UP
  既置滚动位又（不吃事件地）命中 MapJumpToTopEdge——viewport_handle_key 返回值只看
  isHotkey，与上游 `HandleMapScrollKey` 返回语义一致。

## tests/hotkeys.cpp
- 黄金对拍分两文件：ra-bindings.txt（默认表 174 定义）+ ra-override.txt（合成 Keys
  段走构造覆盖路径）；合成段本身从 override 黄金头注释解析（单一事实源）。
- canonical 修饰键序 = C# 声明序 Shift, Alt, Ctrl, Meta（Modifiers:F），**不是** yaml
  源序——ControlGroupAddTo01 源写 `Ctrl, Shift`，dump 为 `Shift, Ctrl`。
