# OpenRACpp

**中文** | [English](#english)

## 简介

[OpenRA](https://github.com/OpenRA/OpenRA) 游戏引擎的 **C++26 语义重写**。

按 OpenRA 上游源码（基线 commit `b6fc03fcfa`）的代码语义逐行重写，数据、地图与回放格式兼容上游。以**语义等价**为第一原则（黄金对拍验证），在此基础上做 C++ 侧有意优化；无法逐语义等价处登记于 [docs/COVERAGE.md](docs/COVERAGE.md)（未登记的偏离视为 bug）。

配套文档:[PORTING_PLAN.md](PORTING_PLAN.md)（分阶段移植计划与状态）、[docs/COVERAGE.md](docs/COVERAGE.md)（偏离登记全文）、[docs/UPSTREAM_CPP_REVIEW.md](docs/UPSTREAM_CPP_REVIEW.md)（优化点审阅证据）、[UPSTREAM.baseline](UPSTREAM.baseline)（上游基线与同步流程）。

## 进度总览

**整体完成度约 58%**（按 PORTING_PLAN 各阶段工作量加权估算）。数据/引擎/渲染/平台层（前半程）已完成并经黄金对拍锁定；当前处于 **Phase 5 的 gameplay 大面**——攻击/迷雾/建筑/生产/电力/资源全链（AttackBase+Follow+Turreted/AutoTarget/全部主力弹丸战头/Shroud/FrozenActorLayer/Building/Production/PlayerResources/TechTree/PowerManager/Harvester+ResourceLayer+Dock 子系统/Refinery）已打通，**控制台发 order 造兵→出厂→战斗→采矿→卸货入账的 skirmish 链路已在测试中全程跑通**；Cloak/GainsExperience/Capturable 族/Conditions 34 件/Selectable/SpawnMapActors 已随第七批打通；Render·WithSpriteBody 渲染族 + ProximityCapturable + Settings/FieldSaver + SyncReport/录像录制起步已随第八批打通（渲染批修出子类 upcast 漏接口集真问题）；注释·装饰·死亡·损伤·进度条渲染族（SelectionDecorations/WithDecoration/WithDeathAnimation/WithDamageOverlay/ProductionBar/pips 装饰）已随第九批打通（修出 BitSet 双表分裂与 Selectable 单表继承缺失两真问题）；下一步 MapPreview 异步面 + replay 对拍器后，即开启 replay SyncHash 对拍（确定性移植总关卡）。

| 阶段 | 内容 | 状态 | 完成度 |
|---|---|---|---|
| Phase 0 | C++26 工程骨架 + 定点原语 | ✅ 完成（2026-10-03） | 100% |
| Phase 1 | MiniYaml + 文件系统 | ✅ 完成（2026-10-03） | 100% |
| Phase 2 | 元数据框架 + 数据加载链 | ✅ 完成（2026-10-03） | 100% |
| Phase 3 | 仿真核心 + Order/锁步 | ✅ 完成（2026-10-03） | 100% |
| Phase 4 | 平台层 + 渲染 + 文件格式 + 音频 + UI 框架起步 + Game 主循环骨架（15 批） | ✅ 完成（2026-10-06） | 100% |
| Phase 5 | 主循环整合 + 核心 gameplay（已完成 Map/World/寻路/Mobile+Move/Armament/攻击链+炮塔/迷雾链/弹丸战头全量/建筑生产链/电力链/资源链/条件·隐身·经验·捕获·摆位链/渲染 trait·邻近捕获·Settings·录像起步/注释·装饰·死亡·损伤·进度条渲染族 9 批） | 🔨 进行中（2026-10-07 起） | ~95% |
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
- **持续门禁**：ctest **26/26** 三构建（ASan+UBSan / Release / **GCC -fhardened 旗级等价硬化**）全绿；`import std;` 严控（**462 文件**）；偏离登记 **D1~D186**。
- **双编译排错编排（2026-10-08 起）**：**Clang 23**（llvm-mingw/UCRT，基线工具链）× {ASan+UBSan、Release} + **GCC `-fhardened` 旗级等价第二编排**（`-D_FORTIFY_SOURCE=3 -fstack-protector-strong -fPIE -pie`，CMake `-DORA_HARDENED=ON`；std.pcm 同享 `-fstack-protector-strong` 防配置失配拒载）。实测注记：本机 `gcc.exe` 为 llvm-mingw 的 clang 壳，原旗标 `-fhardened` 不可执行 —— 等价集按 GCC 文档逐项展开；真 GCC 15+ 原旗标验证列为 CI 待办（见 ../cpp26.md 工具链表）。

### 当前焦点（Phase 5 第十批）

MapPreview 异步面 + tools/replaydiff 对拍器。关键里程碑：**C# 录制 replay 逐帧 SyncHash 对拍（确定性移植总关卡）**。

第九批（2026-10-09）已落地：SelectionDecorations(+Base+两注释 renderable)/WithDecoration(+Base)/WithDeathAnimation/WithDamageOverlay/SpriteEffect/ProductionBar + 三个 pips 装饰（ControlGroup/ResourceStorage/StoresResources）+ Selectable 恢复 Interactable 单表继承 + BitSet 分配器桥（修出双表分裂真问题）；render_test 批九节（e1 选择注释框+条+控制组 pip → spy 条件装饰 → e1 死亡 SpriteEffect(die1/2/3) → jeep 重损烟雾 → spen 生产条 → harv 存量 pips）。

第八批（2026-10-08）已落地：RenderSprites/WithSpriteBody(+Facing)/WithInfantryBody/WithSpriteTurret/WithMakeAnimation(+Overlay)/RenderSpritesEditorOnly + ProximityCapturable 族 + FieldSaver/Settings 起步 + SyncReport/ReplayRecorder/ReplayConnection 起步；render_test 真 ra 全链（e1 动画状态机 → 2tnk 炮塔钳定 → TENT make 动画 → 邻近捕获换主/还原 → 设置差异往返 → 录像三段包序回读）。

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

### Phase 5 — 主循环整合 + 核心 gameplay 🔨（~90%）

**第一批（2026-10-06）：Map 全量 + World 接线**

- `ora_terrain`（CellRamp 三角化/TilesByDistance/DefaultTerrain 双格式）+ `ora_map`（cell_region 零分配枚举器、cell_layer 双坐标索引 + 等距预滤怪癖、map_players 全 22 字段、**Map.cs 全量**：map.bin 三段读/SHA-1 UID 内容寻址/投影族/坐标换算族、MapCache 装载链）。
- **OPT-A8 落地**：SpatiallyPartitioned 的 Actor 键 slab 数组化（查询戳去重零分配）。
- Selection 全量；**D26/D27**：TraitRegistry（Info 名 → 工厂 → WorldArena 内构造）+ **World 全量重写**（系统 actor 解析/trait+actor 全入 arena/LoadComplete 与 Dispose 全序）；Game::StartGame 双入口装配。

**第二批（2026-10-07）：ActorMap/ControlGroups + Player 创建链 + UnitOrderGenerator + 寻路全套 + Health/Locomotor**

- core 增 priority_queue（"层级加倍"堆**原样保留**——平局弹出序进入寻路结果，对拍前提）+ long_bitset（全量位集）。
- sim 增 **ActorMap 全文**（影响层链表/位置缓存/Cell+Proximity 触发器）、ControlGroups 全文、**Player 全量重写**（ResolveFaction 随机阵营展开/关系掩码/FactionInfo 值袋解析）、trait_interfaces 批接口（IActorMap/INotifyDamage 族/IIssueOrder/BlockedByActor 等）、World 接线补全（DefaultOrderGenerator 注册表校验逐字/AddToMaps 全序/FogObscures 注入）。
- `src/mods/` 新族：create_map_players 全文、health 全文（**decimal 链走 OPT-A1 + 栈上定长缓冲 OPT-A9**；[VerifySync] HP 哈希注册）、unit_order_generator 全文（两轮目标器回退/InputOverridesSelection）、**寻路九件**：图类型校验逐字 + **OPT-A2 世代标记层池**（取出即 ++epoch 免 O(地图) 清零）+ 稠密图成员暂存缓冲 + PathSearch 全文 + **Locomotor 全文**（CellCache 三集合/CanMoveFreelyInto 阶梯/代价表 + 阻挡缓存）+ **HPF 全文**（BuildGrid 泛洪/AbstractGraphWithInsertedEdges/单源双向 + 多源单向/AbstractNodeForCost 高速公路延迟汇入）+ PathFinder 全文。
- 验收：新 path_test **41 检查全绿**（纯逻辑矩阵 + **真实 ra 地形全链**：代价表 → 全高墙 NoPath → 开口绕行 → 邻近快路径 → HPF 域查询）。

**第三批（2026-10-07）：Mobile/IPositionable 族 + BodyOrientation + Move 活动族 13 件 + Weapons/Warheads 起步（OPT-A9）**

- sim 增量：条件 trait 基建（**ConditionalTrait + PausableConditionalTrait 全文的 CRTP 组合核**）、weapons 基础（ProjectileArgs/WarheadArgs/IProjectile(IInfo)/**Warhead 基类全文** + WeaponInfo 仿真面自由函数 + Projectile/Warhead 名字注册表 + DelayedImpact）、批接口 24 件（IPositionable/IMove/MoveResult/INotify 族/修正 modifier 族/ITemporaryBlocker 等）、Actor 接口缓存面补全（Orientation/Targetables/CanBeViewedByPlayer/AcceptsOrder）、Target 补 Recalculate/IsValidFor、World 补 ContainsTemporaryBlocker。
- mods 五件新：**Mobile 全文**（MobileInfo 21 字段 + LocomotorInfo 工厂解析异常逐字；IPositionable/IMove 全族；order 四面 + MoveOrderTargeter/ReturnToCell/LeaveProduction 内嵌；[VerifySync] 四成员哈希）、**Armament 全文**（武器解析三校验逐字/CheckFire burst 循环/双 TargetOffset 换轴/UpdateBurst 分档；**OPT-A9 开火链零分配**：修正 trait 一次解析 + 成员缓冲回填 span 直传 + 延迟动作闭包 → kind 判别式）、body_orientation 全文（Lazy → 首查物化 + qboi 解析钩子）、util/actor_exts（TickFacing/QuantizeFacing/BetweenCells/AdjacentCells + IsAtGroundLevel/NotifyBlocker 等）。
- **Move 活动族 13 件全文**：Wait/Turn/Drag/Nudge/AttackMoveActivity/MoveCooldownHelper/MoveAdjacentTo（虚方法族）/MoveWithinRange/MoveOnto(+AndTurn)/Follow/LocalMoveIntoTarget/**Move**（三构造闭包/PopPath 让行四判 + 等待 + 重寻路 + 阻挡退让/MovePart 椭圆弧插值 + 坡度 SLerp + carryoverProgress/MoveFirstHalf 急弯判定）；活动对象 WorldArena 分配。
- 验收：新 move_test（BodyOrientation 纯逻辑矩阵 + **真实 ra 规则/地图全链**：e1 构造（^Infantry 继承链验证）→ **MoveTo 三格 58 tick 到站** → **M1Carbine 继承链解析 + 开火节拍**（面外拒/面内发/装填拒/复燃））；ctest 20→21。修出四个真问题（Actor::info_ 全量路径未赋值、RecordFieldInt 读 bool 恒 0、TypeDictionary 漏 self 键 + 查询键错配致跨类型桶错配 —— ASan 实证、Health 工厂 HitShape 误查接口名）。

**第四批（2026-10-07）：Shroud/FrozenActorLayer + AttackBase 族 + AutoTarget + 弹丸/战头（Bullet/InstantHit/SpreadDamage）+ HitShape/Armor/Targetable**

- sim 三件新：shroud（**Shroud.cs 全文**：五 ProjectedCellLayer/sources 表/Tick 的 touched 推进与 disabledChanged 重建/ProjectedCellsInRange 双形态/Explore 族/IsExplored·IsVisible×4/GetVisibility 四象限）、frozen_actor_layer（**全文**：FrozenActor 可见性重算 + 层的 OnShroudChanged 订阅/双哈希/InRegion·InCircle）、screen_map 冻结面（惰性 Cache<Player,…> + TickRender 半段 + viewer 重载）；接线：player 补 **L155 InternalName**（漏置真问题）、Target 的 FromCell + FrozenActor 全分支、Actor 的 EnabledTargetablePositions 族、World::FindActorsInCircle、Map 悬垂 span 修复。
- mods 十三件新：hit_shapes 四形状全文、hit_shape/armor/targetable、affects_shroud+reveals_shroud 全文、attack_base（AttackBase+Frontal 全文；AttackOrderTargeter）、attack_activity（Activities/Attack.cs 全文）、auto_target（+Priority 全文；**优先级禁用过滤 = 消费时求值，对齐上游惰性 Where**）、world_exts（圆/线查询 + 线投影）、blocks_projectiles、projectiles（Bullet+InstantHit 全文；**RNG 消耗序保真**，视觉面注入/不构造）、warheads（Damage+Spread 全文）。
- 验收：新 attack_test（真实 ra 全链：迷雾可见性矩阵 → 空闲扫描排攻击 → **落弹致死 61 tick（4×1500）** → FrozenActorLayer 面 → **Bullet 4 tick 落地 1800 伤害**）；ctest 21→22。修出四个真问题（Player 漏 InternalName、Map 悬垂 span、LocationInit 抽象缺口、AutoTarget 惰性过滤时点）。

**第五批（2026-10-07）：Missile/TeslaZap/GravityBomb + TargetDamage/LeaveSmudge/CreateEffect + FrozenUnderFog/HiddenUnderShroud + 建筑/生产链**

- 弹丸三件：**Missile 全文**（DetermineLaunchSpeedAndAngle 五函数族/InclineLookahead/IncreaseAltitude/HomingInnerTick 全分支/(sbyte) 符号截断语义逐处保真；ctor 的 LockOn→FromPDF→DetermineLaunch→Sequences.Random 的 RNG 序保真；[VerifySync] {pos,hFacing,vFacing}）、**TeslaZap 全文**（逐 tick Impact；{target}）、**GravityBomb 全文**（换轴旋转变换；OpenSequence→PlayThen 的 RNG 零消耗链；{pos,lastPos}）；InstallCommonSyncEffectHasher 扩四弹丸。
- 战头三件：**TargetDamage**（Spread 圆域最近 HitShape）、**CreateEffect**（三段 RNG 消耗序逐字/IsValidAgainstTerrain 的 "Air" 运行时位）、**LeaveSmudge**（FindTilesInAnnulus 环 + AcceptsSmudgeType 首遇序 + 层字典未含即抛）；SmudgeLayer 同步记账面（tiles/dirty 双字典；CosmeticRandom 烟效纯视觉未接）。
- 迷雾两件：**FrozenUnderFog 全文**（逐玩家 FrozenActor 入层/初始可见性帧末重算/OnVisibilityChanged/OwnerChanged 强制刷/Disposing Invalidate；{VisibilityHash}）+ **HiddenUnderShroud 全文** + ShroudExts 的 AnyExplored 双形态。
- 建筑/生产链：**Building 全文**（Footprint 五类型序/CenterOffset/IOccupySpace/ITargetableCells/{TopLeft}）、BuildingInfluence 全文、**Production 全文**（DoProduction 帧末出厂 + 双通知/ExitExts 的 Shuffle(SharedRandom) Fisher-Yates RNG 序保真）、**ProductionQueue 全文**（StartProduction/Pause/Cancel 三 order/逐 tick 计费/OnComplete 帧末闭包→item 成员/{Enabled,IsValidFaction}）、**ClassicProductionQueue 全文**（player actor 挂载的世界级 producer 查找）、**PlayerResources 全文**（ore-first 序/checked 溢出→饱和/{Cash,Resources,ResourceCapacity}）、Valued/Buildable/Exit/Reservable/RallyPoint/ProvidesPrerequisite/**TechTree 全文**（Watcher 双状态机/'!''~' 标记语义）/DeveloperMode 最小承载。
- 验收：新 production_test（**真实 ra 全链**：startingcash 5000 → TENT 建筑（occupied 4 格/CenterOffset/BuildingInfluence/FrozenUnderFog 入层）→ TechTree 汇 'barracks' → **e1 下单 → 60 tick 建造 → Exit@Soldier 出口 → 出厂入世界（逐 tick 扣费恰 100）** → Cancel 退款 → Dragon 导弹（Speed 213/Arm 2/80 tick 飞行））；ctest 22→23。修出六个真问题（Classic 子类 upcast 表漏基类接口集致 Created/Tick 分发断链、Player ctor 的 player-actor 构造序（Created 回调早于 PlayerActor 赋值 + 双 Initialize）、EndProduction 的 erase 后 UAF、Faction().Name 误代 InternalName、Exit/Production 族漏挂 IObservesVariables 致条件 trait 恒禁用、DelayedImpact 帧末闭包悬垂捕获）。

**第六批（2026-10-07）：AttackFollow/AttackTurreted/Turreted + PowerManager/Power/AffectedByPowerOutage + 资源链全量**

- 攻击面接面：**AttackFollow 全文**（Requested/Opportunity 双目标/PersistentTargeting 迁移/机会射击/OnResolveAttackOrder 预置/嵌套 AttackActivity 的 RangeMargin 钳制与 lastVisible 三段快照）+ **AttackTurreted 全文**（全炮塔就位判定）；上游以具体类 AttackBase 充当接口 → **AttackBaseFace**（TypeId=上游 AttackBase 键）接入，AutoTarget/Activities·Attack 全面换脸。
- 炮塔：**Turreted 全文**（WorldFacingFromInit 闭包族/realignTick 回正/DesiredLocalFacing 的垂直轴直差 vs 旋转变换/FaceTarget 五门/双 init 修饰/{QuantizedFacings}）+ TurretFacingInit/DynamicTurretFacingInit（按 info 实例名匹配）。
- 电力：**PowerManager 全文**（正负分流账本/UnlimitedPower 重建/PowerState 三档/TriggerPowerOutage 分发/{PowerProvided,PowerDrained}）+ **Power 全文**（IPowerModifier=OPT-A1 修正链/账本联动）+ **AffectedByPowerOutage 全文**；ProductionQueue 接线（RemainingTimeActual 的 LowPowerModifier 折算 + Slowdown 节流）；DeveloperMode 增 Enabled/UnlimitedPower。
- 资源链：**ResourceLayer 全文**（map.bin 装载/RecalculateResourceDensity 的 Lerp HACK/AllowResourceAt 五门/密度记账）+ **ResourceClaimLayer 全文**、**DockClientManager/DockHost/MoveToDock/GenericDockSequence 全文**（预约-移动-入坞-卸载状态机/DockExts.ClosestDock 的占用代价寻路）、**Harvester 全文**（载荷/卸载节拍/速度修正/{currentUnloadTicks}）+ **HarvestResource/FindAndDeliverResources 全文**（余弦定理代价的谓词寻路/备援搜索/厂口让位）、**StoresResources/StoresPlayerResources/Refinery 全文**（ContentHash/容量占比/修正链入账）；ClassicFacingBodyOrientation 注册（HARV/e1 全域）。
- 验收：新 resource_test（**真实 ra 全链**：POWR +100/三·六 TENT 的 60/120 drain → Normal→Low → UnlimitedPower 清零重建 → 低电 e1 建造 ×3 → **2tnk 炮塔对敌 e1 的 ForceAttack 追踪-开火-击杀-回正** → **HARV 邻 PROC 出生 → 500 tick 采满 20 → ClosestDock 寻路入坞 → 卸货 → PlayerResources 入账**）；ctest 23→24。修出三个真问题（AttackBase 接口面缺位、TestWorld 的 Map 逆序析构 UAF（ASan 实证）、ClassicFacingBodyOrientation 未注册致 HARV 构造抛）。

**第七批（2026-10-07）：Conditions 31 件 + Cloak 族 + 经验族 + 捕获族 + Selectable + SpawnMapActors**

- 条件系统：**Conditions 目录 31 件全文**（GrantCondition/GrantRandomCondition 的 SharedRandom 择一/OnTileSet·WhileAiming·OnDamageState·OnHealth(工厂时点校验)·OnTerrain(自定义层索引)·OnBotOwner·OnCombatantOwner·OnPlayerResources·OnFaction·OnPowerState·OnMovement·OnAttack(射击计数栈+冷却+TargetChanged 六判)·OnProduction·OnPrerequisite+**Manager 汇流**/SpreadsCondition/GrantExternalConditionToProduced·ToCrusher/ToggleConditionOnOrder(pause 状态保持)/**GrantConditionOnDeploy**(DeployForGrantedCondition+DeployInner 活动对/SmartDeploy)/**GrantChargedConditionOnToggle**(ToggleChargedCondition 活动)/OnLayer 抽象+Subterranean/Tunnel)+ **ExternalCondition 全文**（timed/permanent 双账+双上限淘汰+升序 Expires 插入）+ **ProximityExternalCondition 全文**（proximity 触发域/域内出厂 workaround/换主重评）。
- 隐身：**Cloak 全文**（Cloaked 三判/UncloakOn 十一位/Damage 三分/移动断隐/CloakType 抑制/DetectCloaked 圆域可见性/{remainingTime}）+ **DetectCloaked 全文**（修正链）+ IgnoresCloak。
- 经验：**GainsExperience 全文**（nextLevel=key×Cost/多级连授/DevLevelUp/变身续传/{Experience,Level}）+ **GivesExperience 全文**（双修正链+PlayerExperience）+ GainsExperienceMultiplier + **PlayerExperience**。
- 捕获：**CaptureManager 全文**（三向关系并集/StartCapture 的 captors 记账+progress 通知/CancelCapture）+ **Capturable/Captures 全文** + GivesCashOnCapture + **Enter 活动基类全文**（四态机/三虚钩/隐藏快照/冷却让行）+ **CaptureActor 全文**（帧末 DoCapture：破坏阈值 long 判+换主+INotifyCapture 分发+消耗）。
- 选择/摆位：**Selectable/Interactable**（Class 缺省名/Bounds·Polygon 值面+居中预计算）+ **SpawnMapActors 全文**（ActorReference 的 from-yaml 装载：InitRegistry 六 init 覆盖 ra 地图 18k 摆位实测/无效 owner 中立转移/SkipMakeAnims+SpawnedByMap 标记）。
- 验收：新 condition_test（**真实 ra 全链**：地图 actor 全量出生 → E1 的 Class="E1"/升级 20000=200%×100 → rank-veteran 授予 → 击杀链的 ^Infantry multiplier-0 上游怪癖 + PlayerExperience 记分 → DOG 的 aiming 边沿授撤 → **THF 隐身 250 tick/移动破隐/CloakDelay 复隐/Critical 伤害 → PauseOnCondition 阻隐** → **E6 工程师捕获 TENT 换主（reusable 路径）**）；ctest 24→25。修出两个真问题（**变量表达式 Token::str_symbol 的 string_view 悬垂** —— 表达式对象迁移后 postfix 读旧存储，全部条件表达式求值随机错（第三批起潜伏）；CapturesInfoData 漏填 conditional 致条件观察者不注册）。偏离 D152~D163 登记。

**第八批（2026-10-08）：RenderSprites/WithSpriteBody 渲染族 + ProximityCapturable 族 + FieldSaver/Settings 起步 + SyncReport/录像录制起步**

- sim 增量：第八批接口（IRender/IMouseBounds/IAutoMouseBounds/IRenderAnnotations/IRenderInfantrySequenceModifier）+ **Actor 渲染缓存面**（renders/mouseBounds 构造期物化 + Render/ScreenBounds/MouseBounds 三查询，Actor.cs L292-346;IRenderModifier 链空集随 Phase 6）+ ActorSyncHashEntry 的 type_id 报告键 + World::SyncEffectHash 公开面。
- mods 八件新：**RenderSprites 全文**（AnimationWrapper 变化检测/Add·Remove/伤害前缀四档/AutoRenderSize 的 float 截断/ModifyActorPreviewInit 补 FactionInit）+ **RenderSpritesEditorOnly 全文**（mpspawn 等编辑器摆位 actor 的隐形承载）、render_utils（ZOffsetFromCenter/GetDamageState）、**WithSpriteBody 全文**（Pausable CRTP/StartSequence 播种/PlayCustomAnimation 族/boundsAnimation 防闪烁/ForceToGround 下投 + **WithFacingSpriteBody 全文**）、**WithInfantryBody 全文**（五态机/stand 择一（LocalRandom 域）/barrel 索引快照/移动位驱转 run/TickIdle 的 SharedRandom 掷值）、**WithSpriteTurret 全文**（炮塔名匹配/recoil 求和 + LocalToWorld/QuantizedFacings 钳到精灵）、**WithMakeAnimation 全文**（Forward/Reverse 双态 + 第 0 帧钉住 quirk/Deploy·Undeploy 闩锁 + **WithMakeOverlay 全文**）、**ProximityCapturableBase/ProximityCapturable/ProximityCaptor 三件全文**（邻近触发器四抽象/最久在域者胜/MustBeClear/Permanent 拆触发器/非 Sticky 离域还原）。
- meta/game/net:（FieldSaver.cs 全文值袋面）（Save/SaveDifferences/SaveField + FormatValue 的 null/BitSet/bool/枚举/集合分支）+ Settings.cs 起步承载（SettingsModule 的 Commit 差异协议 + PlayerSettings/GameSettings 手写表）；net 增 replay_recorder（**ReplayRecorder/ReplayMetadata 全文内存盘形态** + ReplayConnection 起步面）+ sync_report（**SyncReport.cs 起步面**：环形 7 报告 + 哈希/帧/随机态记账 + DumpSyncReport 文本 + 可选 dumper 注册表）。
- 验收：新 render_test（真 ra 全链：真 Classic 序列装载 + 合成资产副本 → **e1 动画状态机**（stand 择一 → Horizontal 转 run → 回落）→ RenderSprites 真 sprite 项 + ScreenBounds → **2tnk 炮塔**（turret 序列/facings=32 钳定/零后坐 offset）→ **TENT make→idle** → **ProximityCapturable 真触发器链**（步入 5c 换主 B/离域还原 A）→ **Settings 差异保存往返** → **ReplayRecorder/Connection 三段包序逐字节回读** → **SyncReport 环形记账**）；ctest 25→26 **三构建**（ASan+UBSan / Release / **-fhardened 旗级等价硬化**）全绿；门禁 std_import（**447**）/upstream_check（**428**）PASS。修出四个真问题（**WithFacingSpriteBody 子类 upcast 漏基类接口集**（第五批教训重演，ASan 实证）、ReplayRecorder preStart 缓冲自追加、ctest 并行夹具目录互踩（0xc0000409）、e1 双 WithInfantryBody 为上游合法形态）。偏离 D164~D176 登记。

**第九批（2026-10-09）：SelectionDecorations 注释族 + WithDecoration 装饰族 + WithDeathAnimation/WithDamageOverlay + ProductionBar + pips 装饰 + BitSet 分配器桥**

- sim/gfx 增量：第九批接口（**ISelectionDecorations/IDecoration**）+ Activity 的 **GetTargets 虚面**（基类默认空）+ World 的 **FogObscures(WPos) 重载**（注入面第三参）+ IViewportSurface 的 Zoom/MinZoom 缺省虚 + DeveloperMode 的 PathDebug 同步面。
- **core/meta:BitSetAllocator 分配器桥**（InstallBridge 四面 + meta 的 no-alloc/contains 查询;七 tag 桥接）—— 修出 **BitSet 双表分裂真问题**（meta::BitsOf 的解析位与 core::BitSet 查询位两表互不相通,DamageTypes/ArmorTypes 的按名 Contains 恒 false,战头 Versus 装甲过滤自第五批起走空过滤路径）。
- mods 十件新:**SelectionDecorationsBase/SelectionDecorations 全文**（IDecoration 双集物化/迷雾空枚举/DrawDecorations 三态+rollover+PathDebug 目标线+缩出隐藏）+ **SelectionBox/Bars 两注释 renderable 全文**（四角短线/血条三层底三层值/DisplayHP 差值段/ISelectionBar 附加条）、**WithDecorationBase 全文**（CRTP 双参 + blink 取模 + 关系过滤 + Offsets/BlinkPatterns 变量观察者）+ **WithDecoration 全文**、**WithDeathAnimation 全文**（DeathTypes 首命中键 + SharedRandom 后缀 + 帧末 SpriteEffect）、**WithDamageOverlay 全文**（延迟掷值/start-loop-end 三段链）、**SpriteEffect 全文**、**ProductionBar 全文**（双跳队列绑定 + 最小剩余占比）、三个 pips 装饰全文（ControlGroup 的 PlayFetchIndex/ResourceStorage 的整数交叉乘法/StoresResources 的前缀和桶定位）。
- 结构修正:**Selectable 恢复承 Interactable**（上游单表继承 —— 第七批拆分使 Selectable-only actor 查不到界;Interactable 补齐 Interactable.cs L56-112 屏幕界全文:AutoBounds/PolygonBounds/Bounds/DecorationBounds 的 WDist→像素换算）。
- 验收:render_test 批九节（真 ra 全链:**e1 未选中零注释项 → 选中出选择框+血条两 Custom 项 → 编组出控制组 pip → spy 的 disguise 装饰条件禁用 → e1 被 DefaultDeath 击杀帧末出 SpriteEffect(image=e1,序列 die1/2/3) → jeep 60% 重损即时起烟雾链 → spen 生产条绑经典 Ship 队列空队值 0 → harv 存量 pips 的 RequiresSelection 语义**）;ctest 26/26 三构建全绿;门禁 std_import(462)/upstream_check(443)PASS。修出三个真问题（**IRender 空 Render 的共享出参 clear 抹除**、**BitSet 双表分裂**、**Selectable 单表继承缺失**;另 AutoTarget.Damaged 直读 e.Attacker 为上游同形 —— 测试对齐契约）。偏离 D177~D186 登记。

**剩余（~5%）**：MapPreview 异步面；tools/replaydiff 对拍器。

### Phase 6 — UI + 本地化 🌱（~10%）

已完成：Widget 框架内核（树/焦点/冒泡/注册表）、Ui 门面、ChromeMetrics、WidgetLoader、光标管理器、声音门面。**未完成**：62 个 chrome widget + 131 个 Logic、Fluent 本地化（FluentProvider/FluentMessage）、WorldInteractionController/DefaultInputHandler 输入装配、chrome.yaml 资产接线。里程碑：主菜单 → 大厅 → skirmish 全 UI 流程可玩。

### Phase 7 — 网络与服务器 🌱（~20%）

已完成：Order 线协议/OrderManager 锁步/UnitOrders 分发/EchoConnection、Session 最小面。**未完成**：NetworkConnection 实体（读线程/Ack/TickScale）、Session 完整序列化与大厅协议、ReplayRecorder/ReplayConnection、SyncReport、Server（单线程事件循环/LobbyCommands 20 命令/Pinger 族）。验收：C++ 回放 C# 联机 replay；力争 C++ 客户端连 C# 服务器完成一局。

### Phase 8 — Lua 脚本、AI、战役、Utility ⬜

Lua 宿主 + 沙箱（内存/指令上限）+ gen/ 编译期绑定表；AI（ModularBot/11 模块/小队状态机/Mamdani 模糊推理）；战役流程；d2k/ts 内容补全；Utility 子集（`--map-hash --check-yaml --resolved-rules --extract --docs`）。验收：官方战役可通关；`--lua-docs` 对拍；AI 对战 replay SyncHash 对拍。

### Phase 9 — 打磨与发布 ⬜

性能调优、打包发布、剩余平台兼容。

## 工程门禁

- **双构建**：ASan+UBSan 与 Release，ctest 25/25 全绿。
- **`import std;` 严控**：禁传统 std 头引入（`tools/std_import_check.py`，423 文件 PASS；白名单仅第三方 C 头）。
- **函数级裁剪**：`-ffunction-sections -fdata-sections` + `--gc-sections`。
- **黄金对拍体系**：yaml（759 文件）/定点原语（60,883 行）/规则深解析（三 mod）/资产解码（6,737 行）/字形（216 个），oracle 双跑确定性验证。
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

当前登记 **D1~D176**（全文见 [docs/COVERAGE.md](docs/COVERAGE.md)），按模块：

- **yaml/fs（D1~D9）**：异常类型统一 YamlException（消息逐字）、惰性枚举物化 vector、null/"" 键合流——合法输入下行为等价或不可观测；
- **meta/加载链（D10~D24）**：TypeConverter 兜底未实现（字段类型已全覆盖）、字典字段插入序 vector、三 mod 解析快照 C++ 侧固化（D24，工具恢复后可再对拍）；
- **sim/net（D25~D34）**：Initialize 观察者去重形态（受端幂等）、trait 工厂 + WorldArena 所有权（**D26/D27 已落地**）、Target.FromCell 换算（D28 已随 Map 解除）、UI 命令族静默（D29，Phase 6/7）、SyncReport 未接（D30，上游默认关闭）；
- **平台/渲染（D35~D57）**：命令缓冲替代装箱队列、统一线程模型、NPOT、KHR_debug、持久映射 VB/VAO 缓存/blend diff/单级合成；输入层形态适配；
- **文件格式族（D58~D95）**：Stream→SpanReader、zlib miniz、Save deflate 依实现、Pfim 双路径统一、BC4/5/DX10 拒绝、正则手写复刻、哈希域 ASCII、声音惰性工厂物化、ogg/mp3 后端替换（样点值非位精确规范，元数据等价）、vxl 行主序 + presence 标志、FreeType LP64 宏窗编入等；
- **精灵/UI/声音门面（D96~D100）**：帧与文件字节共持有（GC 显式等价）、名字分派注册表、Deps 注入面、.NET 异常文本等价抛；
- **主循环/UI（D101~D106）**：upper_bound 插入位（同稳定序）、子 widget unique_ptr 所有权、Mediator type_index 桶、ChromeLogic/WidgetTypeRegistry 双注册表、Ui::ResetAll 排水序反转（ASan 实证防悬垂）、Game 骨架 Deps/钩子面；
- **Map/World（D107~D113）**：DefaultTerrain 模板双视图、事件 → 回调表、MapPreview 异步面随 Phase 6、OPT-A8、TraitRegistry 工厂 + Ruleset 按表借用视图 + StartGame 装配；
- **第二批（D114~D119）**：priority_queue 三态 Compare 静态协议 + LongBitSet 进程级分配域、ActorMap 的 GC/集合/形状求值形态、Player 的 Shroud/FrozenActorLayer 解析面随其批、OPT-A2 语义论证（世代标记/暂存缓冲/插入序保真）、Health 工厂时点 + Session/input 最小承载面；
- **第三批（D120~D125）**：条件 trait CRTP 组合核（RulesetLoaded 时点工厂化）、WeaponInfo 仿真面自由函数分层 + Projectile/Warhead 注册表（零注册 = null 路径）、Armament 的 OPT-A9 零分配形态 + Turreted/Hovers/声音注入面、Mobile 的 Shroud 已探索等价面 + Parachutable/RejectsOrders 空集、BodyOrientation 的 qboi 解析钩子、Move 活动族 WorldArena 分配 + 空 trait 分支、TypeDictionary self 键 + RecordFieldInt bool 修复；
- **第四批（D126~D132）**：Shroud 的 lobby/Session 承载面 + 事件回调表 + touched 线性推进（vector<bool> 特化）、FrozenActorLayer 的渲染缓存无写入者 + ITooltip 最小值面 + ScreenMap 惰性冻结分区、Target::FromCell 落地 + Actor 目标位物化点（Initialize 头 = 上游 ctor 末）、四 HitShape 的记录全名分派 + Turreted/Cloak 空集、AttackBase/AutoTarget 的 CRTP + 稳定序单遍择优 + **优先级禁用过滤消费时求值（上游惰性 Where）** + Passenger/IOverride 空集、弹丸的 Animation/SpriteEffect/Contrail 注入或不构造但 **RNG 消耗序保真** + 同步效果哈希装配器、Damage/SpreadWarhead 的工厂时点校验 + Versus 插入序对 + AffectsShroud 模板基。
- **第五批（D133~D142）**：Missile/TeslaZap/GravityBomb 的视觉注入面 + **RNG 消耗序逐字保真**（GravityBomb 的 OpenSequence 延迟链 = 零消耗）+ JamsMissiles 空集、CreateEffect/LeaveSmudge 的三段 RNG 序 + SpriteEffect/声音注入、SmudgeLayer 记账面（CosmeticRandom 烟效纯视觉未接）、FrozenUnderFog 的渲染物化空 + PlayerDictionary→玩家序 vector、DeveloperMode/PowerManager/IProductionTime 修正链等空集承载、ProductionQueue 的 LINQ 惰性→消费时物化 + OnComplete 闭包→item 成员 + Classic 的 IsPrimaryBuilding 序退化、World 事件回调表 + CPos 全序（自有设施）、**Player 的 player-actor 构造序与 DelayedImpact 悬垂捕获两处语义修复**、ExitExts 的 Shuffle Fisher-Yates RNG 序保真、记录全名查询族（BuildingInfo/MobileInfo/FacingInfo 等）。

- **第六批（D143~D151）**：AttackBaseFace 接入（上游以具体类 AttackBase 充当接口；AutoTarget/attack_activity 换脸）+ AttackFollow 的 `new` Info 隐藏→切片副本 + Rearmable/Aircraft 空集、Turreted 的 TurretFacingInit 族（ORA_INIT_TYPE_SELF_ONLY 按实例名匹配）+ 预览/编辑器面随 Phase 6、PowerManager 的通知注入面缺省 + AffectedByPowerOutage 的 ISelectionBar 随 Phase 6、ResourceLayer 的 FrozenDictionary→声明序 vector + CellChanged 回调、DockClientManager 的游标覆盖表/闭包 targeter + WithDockingOverlay/IDockClientBody 空集直通 + DockClientManager 属性名冲突→GetDockClientManager、Harvester 的 Resources 校验工厂时点 + IResourceRenderer 空集恒 false 域、Refinery 的 Requires<WithSpriteBodyInfo> 注册侧跳过 + FloatingText 随 Phase 6、**Target 的 operator==/!= 补齐**（此前无消费点）、ClassicFacingBodyOrientation 的 facings==32 表域随渲染批。
- **第七批（D152~D163）**：Conditions 三件（LineBuild/Minelayer 系宿主）延后、声音/Fluent/SpriteEffect/FloatingText/TargetLines/cursor/ISelectionBar 的 UI 消费随 Phase 6（声音数组 LocalRandom 域消耗保留）、GrantConditionOnDeploy 的 notify 空集 = 动画分支即完成分支（UndeployStarted 置 Deploying 的上游 quirk 保真）、ExternalCondition 的 source→const void* 源键 + 按名转发面、Cloak 的渲染物化随渲染批 + DockClient/Host 同签名合并 override、Selectable 的单表继承拆分 + 屏幕换算随渲染批、捕获族的 UI/Transform/ProximityCapturable 随批、Enter 的 MoveToTarget 经 Mobile 具体型、SpawnMapActors 的 IPreventMapSpawn 空集 + InitRegistry 六 init + Replace 面、GrantConditionOnLayer 泛型基类不作注册键（ValidLayerType 恒 0）、GainsExperience 的 FrozenDictionary→插入序 vector（^Infantry multiplier-0 上游怪癖验证）、**变量表达式 Token 悬垂真问题修复**（string_view→深拷贝;第三批起潜伏）。
- **第八批（D164~D176）**：WithInfantryBody 的 stand 择一移入 LocalRandom 域（上游 not-synced 的 Game.CosmeticRandom）；IActorPreview 面随 Phase 6 + FactionInit/AnimationWithOffset 所有权形态；WithInfantryBody 的帧末 barrel 索引快照；WithMakeAnimation 的闩锁成员；ProximityCapturable 的装配侧前置校验 + RunAfterTick 注入面 + 按 Info 名的 captor 查询；Actor 的 IRenderModifier 空链 + ScreenMap 的 bounds 源注入；Map::SetSequences 后置装配口；FieldSaver 的值袋域；Settings 两段手写表；ReplayRecorder 的 GameInformation 注入载荷 + 内存盘；SyncReport 的可选 dumper 注册表（D30 扩展）；测试夹具的合成资产副本 + SkipMakeAnims 语义；**ORA_HARDENED 旗级等价硬化构建（本机 gcc.exe 为 llvm-mingw 的 clang 壳，原旗标 -fhardened 不可执行 —— 等价集按 GCC 文档逐项展开;真 GCC 验证列 CI 待办）**。
**尚未移植（Phase 5-8 范围）**：MapPreview 异步面、tools/replaydiff 对拍器、62 个 chrome widget + 131 Logic、Fluent、WorldInteractionController、NetworkConnection 实体、Session 完整协议、Replay 对拍、Server、Lua、AI、战役、Utility 子命令——详见上方各阶段“剩余”节。

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

**Overall completion ≈ 58%** (weighted by the PORTING_PLAN phase budgets). The data/engine/rendering/platform layers (the first half) are complete and locked in by golden differentials; the project is currently in **Phase 5's gameplay surface** — the attack/fog/building/production/power/resource chains (AttackBase+Follow+Turreted/AutoTarget/all the mainline projectiles & warheads/Shroud/FrozenActorLayer/Building/Production/PlayerResources/TechTree/PowerManager/Harvester+ResourceLayer+the dock subsystem/Refinery) are through end to end, and **the full skirmish chain — issuing build orders by console, units leaving the factory, combat, harvesting, and docked unloading credited — runs through in tests**; Cloak/GainsExperience/the Capturable family/the 34 Conditions traits/Selectable/SpawnMapActors landed with the seventh installment; the Render·WithSpriteBody family + ProximityCapturable + Settings/FieldSaver + the SyncReport/replay-recording start landed with the eighth (the render batch surfacing a real subclass-upcast interface-omission bug); the annotation/decoration/death/damage/progress-bar render family (SelectionDecorations/WithDecoration/WithDeathAnimation/WithDamageOverlay/ProductionBar/the pip decorations) landed with the ninth (surfacing the real BitSet split-table and Selectable-inheritance bugs); once the MapPreview async face + the replay differ land, the replay SyncHash differential (the determinism capstone) opens.

| Phase | Scope | Status | Done |
|---|---|---|---|
| Phase 0 | C++26 skeleton + fixed-point primitives | ✅ complete (2026-10-03) | 100% |
| Phase 1 | MiniYaml + file system | ✅ complete (2026-10-03) | 100% |
| Phase 2 | Metadata framework + data-loading chain | ✅ complete (2026-10-03) | 100% |
| Phase 3 | Simulation core + orders/lockstep | ✅ complete (2026-10-03) | 100% |
| Phase 4 | Platform + rendering + file formats + audio + the UI-framework start + the game main loop (15 batches) | ✅ complete (2026-10-06) | 100% |
| Phase 5 | Main-loop integration + core gameplay (Map/World/pathfinding/Mobile+move/Armament/the attack+fog+turret chains/the full projectile+warhead set/the building+production+power+resource chains/the conditions+cloak+experience+capture+placement chains/the render-traits+proximity-capture+Settings+replay start/the annotation+decoration+death+damage+progress-bar render family, 9 batches) | 🔨 in progress (since 2026-10-07) | ~95% |
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
- **Standing gates**: ctest **26/26** green on three builds (ASan+UBSan, Release, and the **GCC -fhardened flag-equivalent hardening**); strict `import std;` (**462 files**); the deviation registry **D1–D186**.
- **The dual-compile debugging rig (since 2026-10-08)**: **Clang 23** (llvm-mingw/UCRT, the baseline toolchain) x {ASan+UBSan, Release} plus a **second leg reproducing GCC `-fhardened` at flag level** (`-D_FORTIFY_SOURCE=3 -fstack-protector-strong -fPIE -pie`, CMake `-DORA_HARDENED=ON`; the std.pcm shares `-fstack-protector-strong` to avoid the configuration-mismatch load refusal). Measured note: this machine's gcc.exe is llvm-mingw's clang shim, so the literal -fhardened is unrunnable — the equivalent set expands the GCC docs item by item; validating the literal flag on real GCC 15+ is a CI TODO (see the toolchain table in ../cpp26.md).

### Current focus (Phase 5, tenth installment)

The MapPreview async face + the tools/replaydiff differ. The milestone after that: **the frame-by-frame replay SyncHash differential against a C#-recorded replay (the determinism capstone)**.

The ninth installment (2026-10-09) landed: SelectionDecorations(+Base+the two annotation renderables)/WithDecoration(+Base)/WithDeathAnimation/WithDamageOverlay/SpriteEffect/ProductionBar + the three pip decorations + Selectable restored onto Interactable (the single-table inheritance) + the BitSet allocator bridge (the real split-table fix); render_test's batch-9 section (e1's selection box+bars+control-group pip → spy's conditional decoration → e1's death SpriteEffect (die1/2/3) → jeep's heavy-damage smoke → spen's production bar → harv's stores pips).

The eighth installment (2026-10-08) landed: RenderSprites/WithSpriteBody(+Facing)/WithInfantryBody/WithSpriteTurret/WithMakeAnimation(+Overlay)/RenderSpritesEditorOnly + the ProximityCapturable family + the FieldSaver/Settings start + the SyncReport/ReplayRecorder/ReplayConnection start; render_test's real ra full chain (e1's animation state machine → the 2tnk turret clamp → TENT's make animation → the proximity capture's owner flip and revert → the settings diff round-trip → the replay's three-part packet read-back).

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

### Phase 5 — main-loop integration + core gameplay 🔨 (~90%)

**First installment (2026-10-06): the full Map + the World wiring**

- `ora_terrain` (CellRamp triangulation / TilesByDistance / DefaultTerrain's dual formats) + `ora_map` (zero-allocation cell_region enumerators, cell_layer's dual indexing + the isometric pre-filter quirk, all 22 map_players fields, **the whole of Map.cs**: the three-segment map.bin read / the SHA-1 content-addressed UID / the projection family / the coordinate conversions, the MapCache loading chain).
- **OPT-A8 landed**: SpatiallyPartitioned's Actor key as slab arrays (query-stamp dedup, zero allocation).
- Selection in full; **D26/D27**: TraitRegistry (an Info name → factory → constructed inside the WorldArena) + **the World rewritten in full** (system-actor resolution / every trait and actor in the arena / the LoadComplete and Dispose orders); Game::StartGame's dual-entry assembly.

**Second installment (2026-10-07): ActorMap/ControlGroups + the player-creation chain + UnitOrderGenerator + the full pathfinding suite + Health/Locomotor**

- core gains priority_queue (the "levels-doubled" heap **kept verbatim** — the tie pop order enters path results, the differential precondition) + long_bitset (the full bit-set).
- sim gains the **whole of ActorMap** (influence linked lists / position caches / Cell+Proximity triggers), the whole of ControlGroups, a **full Player rewrite** (ResolveFaction's random-faction expansion / the relationship masks / FactionInfo parsed from the bag), the batch trait_interfaces (IActorMap / the INotifyDamage family / IIssueOrder / BlockedByActor and more), and the completed World wiring (the DefaultOrderGenerator registry checks verbatim / the full AddToMaps order / the FogObscures injection).
- A new `src/mods/` family: the whole of create_map_players, the whole of health (**the decimal chain via OPT-A1 + a stack fixed buffer OPT-A9**; the [VerifySync] HP hash registration), the whole of unit_order_generator (the two-round targeter fallback / InputOverridesSelection), and the **nine pathfinding files**: the graph-type checks verbatim + the **OPT-A2 generation-stamped layer pool** (checkout is ++epoch with no O(map) clear) + the dense graph's member scratch buffer + the whole of PathSearch + the whole of **Locomotor** (the CellCache trio / CanMoveFreelyInto's ladder / the cost table + blocking cache) + the whole of **HPF** (BuildGrid flood fill / AbstractGraphWithInsertedEdges / single-source bidirectional + multi-source unidirectional / AbstractNodeForCost's highway-merge delay) + the whole of PathFinder.
- Acceptance: the new path_test **all 41 checks green** (pure-logic matrices + the **real-ra-terrain full chain**: the cost table → the full-height wall NoPath → the gap detour → the adjacency fast path → the HPF domain queries).

**Third installment (2026-10-07): the Mobile/IPositionable family + BodyOrientation + the 13-file move activity family + the Weapons/Warheads start (OPT-A9)**

- sim gains: the conditional-trait infrastructure (the whole of ConditionalTrait + PausableConditionalTrait as a CRTP composition core), the weapons base (ProjectileArgs/WarheadArgs/IProjectile(IInfo)/the whole Warhead base + WeaponInfo's sim face as free functions + the Projectile/Warhead name registries + DelayedImpact), 24 batch interfaces (IPositionable/IMove/MoveResult/the INotify families/the modifier families/ITemporaryBlocker etc.), the Actor interface-cache completion (Orientation/Targetables/CanBeViewedByPlayer/AcceptsOrder), Target's Recalculate/IsValidFor, World's ContainsTemporaryBlocker.
- Five new mods files: **the whole of Mobile** (MobileInfo's 21 fields + the LocomotorInfo factory resolution with verbatim exceptions; the full IPositionable/IMove families; the four order faces + the nested MoveOrderTargeter/ReturnToCell/LeaveProduction; the [VerifySync] four-member hash), **the whole of Armament** (the weapon resolution's three verbatim validations / the CheckFire burst loop / the dual TargetOffset axis swap / UpdateBurst's tiering; **OPT-A9's zero-allocation firing chain**: the modifier traits resolved once + the member buffers refilled with span passing + the delayed-action closures becoming a kind discriminator), the whole of body_orientation (the Lazy → first-query materialization + the qboi resolution hook), util/actor_exts (TickFacing/QuantizeFacing/BetweenCells/AdjacentCells + IsAtGroundLevel/NotifyBlocker etc.).
- **The 13-file move activity family in full**: Wait/Turn/Drag/Nudge/AttackMoveActivity/MoveCooldownHelper/MoveAdjacentTo (the virtual family)/MoveWithinRange/MoveOnto(+AndTurn)/Follow/LocalMoveIntoTarget/**Move** (the three constructor closures / PopPath's four concession checks + waiting + repathing + blocking back-off / MovePart's elliptic-arc interpolation + slope SLerp + carryoverProgress / MoveFirstHalf's tight-turn check); activities allocated in the WorldArena.
- Acceptance: the new move_test (BodyOrientation pure-logic matrices + the **real ra rules/map full chain**: the e1 construction (the ^Infantry inheritance chain verified) → **MoveTo across three cells arriving in 58 ticks** → **the M1Carbine inheritance resolution + the firing cadence** (out-of-range rejected / in-range fired / reloading rejected / refire after reload)); ctest 20→21. Four real problems fixed (Actor::info_ never assigned on the full path, RecordFieldInt reading bool as constant 0, TypeDictionary missing the self key + the query key misresolving into cross-type buckets — ASan-proven, the Health factory's HitShape precondition querying the wrong name).

**Fourth installment (2026-10-07): Shroud/FrozenActorLayer + the AttackBase family + AutoTarget + the projectiles/warheads (Bullet/InstantHit/SpreadDamage) + HitShape/Armor/Targetable**

- Three new sim files: shroud (**the whole of Shroud.cs**: the five ProjectedCellLayers/sources table/Tick's touched advance and disabledChanged rebuild/both ProjectedCellsInRange forms/the Explore family/IsExplored·IsVisible×4/GetVisibility's four quadrants), frozen_actor_layer (the whole: FrozenActor's visibility recompute + the layer's OnShroudChanged subscription/dual hashes/InRegion·InCircle), screen_map's frozen faces (the lazy Cache<Player,…> + the TickRender half + viewer overloads); wiring: player gains **L155 InternalName** (a real missed-assignment fix), Target's FromCell + every FrozenActor branch, Actor's EnabledTargetablePositions family, World::FindActorsInCircle, the Map dangling-span fix.
- Thirteen new mods files: hit_shapes (the four shapes in full), hit_shape/armor/targetable, affects_shroud+reveals_shroud in full, attack_base (AttackBase+Frontal in full; AttackOrderTargeter), attack_activity (the whole of Activities/Attack.cs), auto_target (+Priority in full; **the priority disabled-filter evaluates per consumption, matching upstream's lazy Where**), world_exts (circle/line queries + line projection), blocks_projectiles, projectiles (Bullet+InstantHit in full; **the RNG consumption order preserved verbatim**, visual faces injected or not constructed), warheads (Damage+Spread in full).
- Acceptance: the new attack_test (the real ra full chain: the fog-visibility matrix → the idle scan queues the attack → **death by impact in 61 ticks (4×1500)** → the FrozenActorLayer faces → **Bullet lands in 4 ticks for 1800 damage**); ctest 21→22. Four real problems fixed (Player's missing InternalName, the Map dangling span, LocationInit's abstract-class gap, AutoTarget's lazy-filter timing).

**Fifth installment (2026-10-07): Missile/TeslaZap/GravityBomb + TargetDamage/LeaveSmudge/CreateEffect + FrozenUnderFog/HiddenUnderShroud + the building/production chain**

- The three projectiles: **the whole of Missile** (the five-function DetermineLaunchSpeedAndAngle family/InclineLookahead/IncreaseAltitude/HomingInnerTick's every branch/the (sbyte) sign-truncation semantics kept verbatim at each site; the ctor's LockOn→FromPDF→DetermineLaunch→Sequences.Random RNG order preserved; [VerifySync] {pos,hFacing,vFacing}), **the whole of TeslaZap** (the per-tick Impact; {target}), **the whole of GravityBomb** (the axis-swap rotation transform; the OpenSequence→PlayThen zero-RNG delayed chain; {pos,lastPos}); InstallCommonSyncEffectHasher extended to the four projectiles.
- The three warheads: **TargetDamage** (the Spread circle's nearest HitShape), **CreateEffect** (the three-stage RNG consumption order verbatim/IsValidAgainstTerrain's runtime "Air" bit), **LeaveSmudge** (the FindTilesInAnnulus ring + AcceptsSmudgeType's first-encounter order + the throw on an unknown layer); SmudgeLayer's synchronous bookkeeping face (the tiles/dirty double dictionary; the CosmeticRandom smoke effect — purely visual — unwired).
- The two fog traits: **the whole of FrozenUnderFog** (the per-player FrozenActor ingestion/the frame-end initial-visibility recompute/OnVisibilityChanged/OwnerChanged's forced refresh/Disposing Invalidate; {VisibilityHash}) + **the whole of HiddenUnderShroud** + ShroudExts' two AnyExplored forms.
- The building/production chain: **the whole of Building** (the Footprint five-type order/CenterOffset/IOccupySpace/ITargetableCells/{TopLeft}), the whole of BuildingInfluence, **the whole of Production** (DoProduction's frame-end spawn + the two notifications/ExitExts' Shuffle(SharedRandom) Fisher-Yates RNG order preserved), **the whole of ProductionQueue** (the StartProduction/Pause/Cancel orders/per-tick payments/the OnComplete frame-end closure→item member/{Enabled,IsValidFaction}), **the whole of ClassicProductionQueue** (the player-actor-mounted world-scope producer lookup), **the whole of PlayerResources** (the ore-first order/checked overflow→saturation/{Cash,Resources,ResourceCapacity}), Valued/Buildable/Exit/Reservable/RallyPoint/ProvidesPrerequisite/**the whole of TechTree** (the Watcher two-state machine/the '!''~' marker semantics)/DeveloperMode's minimal carrier.
- Acceptance: the new production_test (the real ra full chain: startingcash 5000 → the TENT building (4 occupied cells/CenterOffset/BuildingInfluence/FrozenUnderFog ingested) → TechTree gathering 'barracks' → **an e1 ordered → built over 60 ticks → leaving via the Exit@Soldier exit → produced into the world (exactly 100 paid per-tick)** → the Cancel refund → the Dragon missile (Speed 213/Arm 2/an 80-tick flight)); ctest 22→23. Six real problems fixed (the Classic subclass's upcast table omitting the base's interface set, breaking the Created/Tick dispatch; the Player ctor's player-actor construction order (the Created callbacks running before PlayerActor's assignment + a double Initialize); EndProduction's post-erase UAF; Faction().Name misused for InternalName; the Exit/Production family missing IObservesVariables, leaving conditional traits permanently disabled; the DelayedImpact frame-end closure's dangling capture).

**Sixth installment (2026-10-07): AttackFollow/AttackTurreted/Turreted + PowerManager/Power/AffectedByPowerOutage + the full resource chain**

- The attack face's interface: **the whole of AttackFollow** (the Requested/Opportunity target pair/PersistentTargeting migration/opportunity fire/OnResolveAttackOrder's preemption/the nested AttackActivity's RangeMargin clamp and three-stage lastVisible snapshots) + **the whole of AttackTurreted** (the all-turrets-brought-to-bear check); upstream's concrete AttackBase doubles as the interface → **AttackBaseFace** (the TypeId = upstream's AttackBase key) wired in, with AutoTarget/Activities·Attack swapped over.
- Turrets: **the whole of Turreted** (the WorldFacingFromInit closure family/the realignTick realignment/DesiredLocalFacing's vertical-axis difference vs rotation transform/FaceTarget's five gates/both init modifiers/{QuantizedFacings}) + TurretFacingInit/DynamicTurretFacingInit (matched by info instance name).
- Power: **the whole of PowerManager** (the signed split ledger/the UnlimitedPower rebuild/the three PowerState tiers/the TriggerPowerOutage dispatch/{PowerProvided,PowerDrained}) + **the whole of Power** (IPowerModifier = the OPT-A1 modifier chain/ledger interplay) + **the whole of AffectedByPowerOutage**; the ProductionQueue wiring (RemainingTimeActual's LowPowerModifier scaling + the Slowdown throttle); DeveloperMode gains Enabled/UnlimitedPower.
- The resource chain: **the whole of ResourceLayer** (the map.bin load/the RecalculateResourceDensity Lerp HACK/AllowResourceAt's five gates/density bookkeeping) + **the whole of ResourceClaimLayer**, **the whole of DockClientManager/DockHost/MoveToDock/GenericDockSequence** (the reserve-move-dock-unload state machine/DockExts.ClosestDock's occupancy-cost pathfinding), **the whole of Harvester** (cargo/the unload cadence/the speed modifier/{currentUnloadTicks}) + **the whole of HarvestResource/FindAndDeliverResources** (the cosine-rule-cost predicate pathfinding/the fallback search/unblocking the refinery entrance), **the whole of StoresResources/StoresPlayerResources/Refinery** (ContentHash/the capacity share/the modifier-chained crediting); ClassicFacingBodyOrientation registered (the HARV/e1 domain).
- Acceptance: the new resource_test (the real ra full chain: POWR +100/three·six TENTs' 60/120 drain → Normal→Low → the UnlimitedPower zero-and-rebuild → the low-power e1 build ×3 → **the 2tnk turret's ForceAttack track-fire-kill-realign on an enemy e1** → **a HARV born beside the PROC → 20 harvested within 500 ticks → ClosestDock pathing → docking → unloading → PlayerResources credited**); ctest 23→24. Three real problems fixed (the missing AttackBase interface face, TestWorld's Map reverse-destruction UAF (ASan-proven), ClassicFacingBodyOrientation unregistered throwing HARV construction).

**Seventh installment (2026-10-07): the 31 Conditions + the Cloak family + the experience family + the capture family + Selectable + SpawnMapActors**

- Conditions: **31 files of the Conditions directory in full** (GrantCondition/GrantRandomCondition's SharedRandom pick/OnTileSet·WhileAiming·OnDamageState·OnHealth (factory-time validation)·OnTerrain (the custom-layer index)·OnBotOwner·OnCombatantOwner·OnPlayerResources·OnFaction·OnPowerState·OnMovement·OnAttack (the shot-count stack + cooldown + TargetChanged's six checks)·OnProduction·OnPrerequisite + the **Manager funnel**/SpreadsCondition/GrantExternalConditionToProduced·ToCrusher/ToggleConditionOnOrder (paused-state retention)/**GrantConditionOnDeploy** (the DeployForGrantedCondition+DeployInner activity pair/SmartDeploy)/**GrantChargedConditionOnToggle** (the ToggleChargedCondition activity)/the OnLayer abstract + Subterranean/Tunnel) + **ExternalCondition in full** (the timed/permanent ledgers + the two-cap eviction + the ascending-Expires insert) + **ProximityExternalCondition in full** (the proximity trigger domain/the in-range production workaround/the owner-change re-evaluation).
- Cloak: **Cloak in full** (the Cloaked triple/UncloakOn's eleven bits/Damage's three-way/movement break/the CloakType suppression/the DetectCloaked-circle visibility/{remainingTime}) + **DetectCloaked in full** (the modifier chain) + IgnoresCloak.
- Experience: **GainsExperience in full** (nextLevel = key×Cost/the multi-level grants/DevLevelUp/transform carry-over/{Experience,Level}) + **GivesExperience in full** (the dual modifier chains + PlayerExperience) + GainsExperienceMultiplier + **PlayerExperience**.
- Capture: **CaptureManager in full** (the three-way relationship union/StartCapture's captor bookkeeping + progress notifications/CancelCapture) + **Capturable/Captures in full** + GivesCashOnCapture + **the Enter activity base in full** (the four-state machine/three virtual hooks/the hidden snapshot/the cooldown yield) + **CaptureActor in full** (the frame-end DoCapture: the sabotage-threshold long test + owner change + the INotifyCapture dispatch + consumption).
- Selection/placement: **Selectable/Interactable** (the Class default/Bounds·Polygon value faces + the centering precalculation) + **SpawnMapActors in full** (ActorReference's from-yaml loading: InitRegistry's six inits cover the ra maps' 18k measured placements/the invalid-owner neutral transfer/the SkipMakeAnims+SpawnedByMap markers).
- Acceptance: the new condition_test (the real ra full chain: every map actor spawned → E1's Class="E1"/promotion 20000 = 200%×100 → rank-veteran granted → the kill chain's ^Infantry multiplier-0 upstream quirk + the PlayerExperience score → DOG's aiming-edge grants → **THF: the 250-tick cloak/movement break/CloakDelay re-cloak/Critical damage → PauseOnCondition blocks** → **the E6 engineer captures the TENT (the reusable path)**); ctest 24→25. Two real problems fixed (**the variable-expression Token::str_symbol string_view dangling** — the postfix stream reads the old storage after the expression object migrates, randomly breaking every condition-expression evaluation (latent since batch 3); CapturesInfoData's missing conditional fill leaving the condition observers unregistered). Deviations D152–D163 registered.

**The eighth installment (2026-10-08): the RenderSprites/WithSpriteBody family + the ProximityCapturable family + the FieldSaver/Settings start + the SyncReport/replay-recording start**

- sim gains: the batch-8 interfaces (IRender/IMouseBounds/IAutoMouseBounds/IRenderAnnotations/IRenderInfantrySequenceModifier) + **Actor's render caches** (the renders/mouseBounds construction-time materialization + the Render/ScreenBounds/MouseBounds queries, Actor.cs L292-346; the IRenderModifier chain stays empty for Phase 6) + ActorSyncHashEntry's type_id report key + World's public SyncEffectHash.
- Eight new mods files: **the whole of RenderSprites** (the AnimationWrapper change detection, Add/Remove, the four damage-prefix tiers, AutoRenderSize's float truncation, ModifyActorPreviewInit's FactionInit seeding) + **the whole of RenderSpritesEditorOnly** (the invisible carrier of the editor-placement actors like mpspawn), render_utils (ZOffsetFromCenter/GetDamageState), **the whole of WithSpriteBody** (the Pausable CRTP, the StartSequence seeding, the PlayCustomAnimation family, the anti-flicker boundsAnimation, the ForceToGround ground projection + **the whole of WithFacingSpriteBody**), **the whole of WithInfantryBody** (the five-state machine, the stand pick in the LocalRandom domain, the barrel-index snapshot, the movement-bit run/stand switching, TickIdle's SharedRandom rolls), **the whole of WithSpriteTurret** (the turret-name match, the recoil sum + LocalToWorld, the QuantizedFacings clamp to the sprite), **the whole of WithMakeAnimation** (the Forward/Reverse pair + the frame-0 pinning quirk, the Deploy/Undeploy latches + **the whole of WithMakeOverlay**), and **all three of ProximityCapturableBase/ProximityCapturable/ProximityCaptor** (the four proximity-trigger abstracts, the longest-in-area captor, MustBeClear, Permanent's trigger drop, the non-Sticky revert).
- meta/game/net: the value-bag face of the whole of FieldSaver.cs (Save/SaveDifferences/SaveField + FormatValue's null/BitSet/bool/enum/collection branches) + Settings.cs's starter carrier (SettingsModule's Commit diff protocol + the hand-written PlayerSettings/GameSettings tables); net gains replay_recorder (the whole of ReplayRecorder/ReplayMetadata in the in-memory-sink form + ReplayConnection's starter face) + sync_report (SyncReport.cs's starter face: the 7-report ring + the hash/frame/random bookkeeping + the DumpSyncReport text + the optional dumper registry).
- Acceptance: the new render_test (the real ra full chain: the real Classic sequence loading + synthetic asset copies → **e1's animation state machine** (the stand pick → the Horizontal switch to run → the fallback) → RenderSprites' real sprite items + ScreenBounds → **the 2tnk turret** (the turret sequence/the facings=32 clamp/the zero-recoil offset) → **TENT's make→idle** → **the ProximityCapturable real-trigger chain** (walking into 5c flips the owner to B/leaving reverts to A) → **the Settings save-differences round-trip** → **the ReplayRecorder/Connection three-part packet byte-exact read-back** → **SyncReport's ring bookkeeping**); ctest 25→26 green on **three builds** (ASan+UBSan / Release / the **-fhardened flag-equivalent hardening**); the std_import (**447**) / upstream_check (**428**) gates PASS. Four real problems fixed (the **WithFacingSpriteBody subclass's upcast table omitting the base interface set** (the batch-5 lesson repeating, ASan-proven), ReplayRecorder's pre-start self-append, the parallel-ctest fixture directory race (0xc0000409), and e1's double WithInfantryBody being upstream's legitimate shape). Deviations D164–D176 registered.

**The ninth installment (2026-10-09): the SelectionDecorations annotation family + the WithDecoration family + WithDeathAnimation/WithDamageOverlay + ProductionBar + the pip decorations + the BitSet allocator bridge**

- sim/gfx gains: the batch-9 interfaces (**ISelectionDecorations/IDecoration**) + Activity's **GetTargets virtual face** (empty base default) + World's **FogObscures(WPos) overload** + IViewportSurface's Zoom/MinZoom defaults + DeveloperMode's PathDebug.
- **core/meta: the BitSetAllocator bridge** (InstallBridge's four faces + the meta no-alloc/contains queries; seven bridged tags) — fixing the **real split-table bug** (the parse side's meta::BitsOf bits and the core::BitSet query bits lived in two unconnected tables, so the by-name Contains of DamageTypes/ArmorTypes was constantly false, and the warhead Versus armor filter had run an empty filter since batch 5).
- Ten new mods files: **the whole of SelectionDecorationsBase/SelectionDecorations** + **the two annotation renderables**, **the whole of WithDecorationBase** (the two-parameter CRTP + the blink modulo + the relationship filter + the Offsets/BlinkPatterns observers) + **WithDecoration**, **the whole of WithDeathAnimation** (the first-hit DeathTypes key + the SharedRandom suffix + the frame-end SpriteEffect), **the whole of WithDamageOverlay** (the delayed roll + the start-loop-end chain), **the whole of SpriteEffect**, **the whole of ProductionBar** (the two-hop queue binding + the least-remaining fraction), and the three pip decorations.
- Structural fix: **Selectable inherits Interactable again** (upstream's single-table shape — the batch-7 split left Selectable-only actors without a findable bounds face; Interactable gains the whole of Interactable.cs L56-112's screen faces).
- Acceptance: render_test's batch-9 section (the real ra full chain: **e1's zero unselected annotations → the selected box+bars Custom pair → the grouped control-group pip → spy's disguise decoration disabled → e1's DefaultDeath death spawning the frame-end SpriteEffect (image e1, die1/2/3) → jeep's 60% heavy damage starting the smoke chain at once → spen's bar bound to the classic Ship queue reading 0 → harv's stores pips' RequiresSelection semantics**); ctest 26/26 on three builds; the std_import (462)/upstream_check (443) gates PASS. Three real problems fixed (**the IRender empty-Render shared-out-vector clear**, **the BitSet split tables**, **the missing Selectable single-table inheritance**; AutoTarget.Damaged's unguarded e.Attacker is upstream's own shape — the test aligns with the contract). Deviations D177–D186 registered.

**Remaining (~5%)**: the MapPreview async face; the tools/replaydiff differ.

### Phase 6 — UI + localization 🌱 (~10%)

Done: the Widget-framework core (tree/focus/bubbling/registries), the Ui facade, ChromeMetrics, WidgetLoader, the cursor manager, the sound facade. **Remaining**: the 62 chrome widgets + 131 Logics, Fluent localization (FluentProvider/FluentMessage), WorldInteractionController/the input assembly, the chrome.yaml asset wiring. Milestone: main menu → lobby → a playable skirmish UI flow.

### Phase 7 — network + server 🌱 (~20%)

Done: the Order wire protocol / the OrderManager lockstep / the UnitOrders dispatch / EchoConnection / the minimal Session face. **Remaining**: the real NetworkConnection (the read thread / Acks / TickScale), full Session serialization + the lobby protocol, ReplayRecorder/ReplayConnection, SyncReport, the Server (a single-threaded event loop / the 20 LobbyCommands / the pinger family). Acceptance: a C++ replay of a C# multiplayer game; the stretch goal — a C++ client finishing a match on a C# server.

### Phase 8 — Lua scripting, AI, campaigns, Utility ⬜

The Lua host + sandbox (memory/instruction caps) + the gen/ compile-time binding tables; the AI (ModularBot / 11 modules / squad state machines / Mamdani fuzzy inference); campaign flow; d2k/ts content completion; the Utility subset (`--map-hash --check-yaml --resolved-rules --extract --docs`). Acceptance: official campaigns completable; the `--lua-docs` differential; an AI-vs-AI replay SyncHash differential.

### Phase 9 — polish + release ⬜

Performance tuning, packaging, remaining platform compatibility.

## Project Gates

- **Dual builds**: ASan+UBSan and Release; ctest 25/25 green on both.
- **Strict `import std;`**: no classic std-header includes (`tools/std_import_check.py`; 423 files PASS; the whitelist covers only third-party C headers).
- **Function-level dead-code elimination**: `-ffunction-sections -fdata-sections` + `--gc-sections`.
- **The golden-differential system**: yaml (759 files) / the fixed-point primitives (60,883 lines) / the deep rules parse (three mods) / asset decoding (6,737 lines) / glyphs (216), with the oracle's cross-run determinism verified.
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

Currently **D1–D176** (full texts in [docs/COVERAGE.md](docs/COVERAGE.md)), by module:

- **yaml/fs (D1–D9)**: exceptions unified into YamlException (texts verbatim), lazy enumerations materialized, null/"" key coalescing — behavior-equivalent for valid inputs;
- **meta/loading chain (D10–D24)**: the TypeConverter fallback not implemented (field types fully covered), insertion-ordered dictionary fields, the three-mod parse snapshots frozen on the C++ side (D24; re-differential once the tool is restored);
- **sim/net (D25–D34)**: the Initialize observer-dedup shape (idempotent receivers), the trait factory + WorldArena ownership (**D26/D27 landed**), Target.FromCell's conversion (D28 unlocked with Map), the UI command families silenced (D29, Phase 6/7), SyncReport unwired (D30, off upstream by default);
- **platform/render (D35–D57)**: the command buffer, the unified threading model, NPOT, KHR_debug, persistent-mapped VBs / the VAO cache / blend diff / single-pass compositing; the input-layer shape adaptations;
- **file formats (D58–D95)**: Stream→SpanReader, zlib via miniz, Save's implementation-defined deflate, the Pfim dual-path unification, the BC4/5/DX10 rejection, the hand-written regex reproductions, the ASCII hash domains, the materialized sound lazy factories, the ogg/mp3 backend swaps (sample values are not a bit-exact specification; metadata equivalent), vxl's row-major + presence flags, the FreeType LP64 macro window, and more;
- **sprite/UI/sound facades (D96–D100)**: frames co-owning the file bytes (the explicit GC equivalent), the name-dispatch registries, the Deps injection faces, the .NET exception-text equivalent throws;
- **main loop/UI (D101–D106)**: the upper_bound insertion point (the same stable order), child widgets as unique_ptr ownership, the Mediator's type_index buckets, the ChromeLogic/WidgetTypeRegistry dual registries, Ui::ResetAll's drain-order reversal (ASan-proven dangling prevention), the Game skeleton's Deps/hook faces;
- **map/world (D107–D113)**: DefaultTerrain's template dual views, events → callback lists, the MapPreview async faces deferred to Phase 6, OPT-A8, the TraitRegistry factory + Ruleset's per-table borrowed views + the StartGame assembly;
- **the second installment (D114–D119)**: priority_queue's three-way Compare static protocol + LongBitSet's process-level allocation domain, ActorMap's GC/collection/shape-evaluation forms, Player's Shroud/FrozenActorLayer resolution faces riding their batches, the OPT-A2 semantic argument (generation stamps / scratch buffers / order fidelity), Health's factory timing + the Session/input minimal carriers;
- **the third installment (D120–D125)**: the conditional trait's CRTP composition core (the RulesetLoaded timing moved to the factory), WeaponInfo's sim face as free functions + the Projectile/Warhead registries (zero registrations = the null path), Armament's OPT-A9 zero-allocation shape + the Turreted/Hovers/sound injection faces, Mobile's Shroud explored-equivalent face + the Parachutable/RejectsOrders empty sets, BodyOrientation's qboi resolution hook, the move family's WorldArena allocation + empty-trait branches, TypeDictionary's self key + the RecordFieldInt bool fix;
- **the fourth installment (D126–D132)**: Shroud's lobby/Session carrier face + the event callback list + the touched linear advance (the vector<bool> specialization), FrozenActorLayer's render caches without writers + ITooltip's minimal value face + ScreenMap's lazy frozen partitions, Target::FromCell landed + Actor's targetable-positions materialization point (Initialize's head = upstream's ctor tail), the four HitShapes' record-full-name dispatch + the Turreted/Cloak empty sets, AttackBase/AutoTarget's CRTP + the stable-order single-pass pick + **the priority disabled-filter evaluating per consumption (upstream's lazy Where)** + the Passenger/IOverride empty sets, the projectiles' Animation/SpriteEffect/Contrail injected or not constructed with **the RNG consumption order preserved** + the synced-effect hash assembler, Damage/SpreadWarhead's factory-time validations + Versus as insertion-ordered pairs + the AffectsShroud template base.
- **the fifth installment (D133–D142)**: Missile/TeslaZap/GravityBomb's visual injection faces + **the RNG consumption order kept verbatim** (GravityBomb's OpenSequence delayed chain = zero consumption) + the JamsMissiles empty set, CreateEffect/LeaveSmudge's three-stage RNG order + the SpriteEffect/sound injection, SmudgeLayer's bookkeeping face (the CosmeticRandom smoke effect — purely visual — unwired), FrozenUnderFog's empty render materialization + PlayerDictionary→the player-indexed vector, the DeveloperMode/PowerManager/IProductionTime modifier chains and other empty-set carriers, ProductionQueue's LINQ laziness→per-consumption materialization + the OnComplete closure→item member + Classic's IsPrimaryBuilding order degradation, World's event callback tables + CPos's total order (self-owned facility), **the two semantic fixes of Player's player-actor construction order and DelayedImpact's dangling capture**, ExitExts' Shuffle Fisher-Yates RNG order preserved, the record-full-name query family (BuildingInfo/MobileInfo/FacingInfo etc.).

- **the sixth installment (D143–D151)**: the AttackBaseFace wiring (upstream's concrete AttackBase doubles as the interface; AutoTarget/attack_activity swapped over) + AttackFollow's `new` Info hiding→the slice copy + the Rearmable/Aircraft empty sets, Turreted's TurretFacingInit family (ORA_INIT_TYPE_SELF_ONLY matched by instance name) + the preview/editor faces ride Phase 6, PowerManager's notification injection face defaults off + AffectedByPowerOutage's ISelectionBar rides Phase 6, ResourceLayer's FrozenDictionary→the declaration-ordered vector + the CellChanged callback list, DockClientManager's cursor-override table/closure targeter + the WithDockingOverlay/IDockClientBody empty-set pass-through + the DockClientManager property-name clash→GetDockClientManager, Harvester's Resources validation at factory time + the IResourceRenderer empty-set constantly-false domain, Refinery's Requires<WithSpriteBodyInfo> registration-side skip + FloatingText rides Phase 6, **Target's operator==/!= filled in** (no consumer until now), ClassicFacingBodyOrientation's facings==32 table domain rides the render batch.

- **the seventh installment (D152–D163)**: the three Conditions files (LineBuild/Minelayer hosts) deferred; the sound/Fluent/SpriteEffect/FloatingText/TargetLines/cursor/ISelectionBar UI consumptions ride Phase 6 (the sound arrays' LocalRandom-domain consumption kept); GrantConditionOnDeploy's empty notify set = the animation branch is the completion branch (the UndeployStarted-sets-Deploying upstream quirk kept verbatim); ExternalCondition's source → the const void* source key + the by-name forwarding face; Cloak's render materialization rides the render batch + the DockClient/Host same-signature merged override; Selectable's single-table-inheritance split + the screen conversion rides the render batch; the capture family's UI/Transform/ProximityCapturable ride their batches; Enter's MoveToTarget via the Mobile concrete type; SpawnMapActors's empty IPreventMapSpawn + InitRegistry's six inits + the Replace face; GrantConditionOnLayer's generic base serving as no registration key (ValidLayerType constantly 0); GainsExperience's FrozenDictionary → the insertion-ordered vector (the ^Infantry multiplier-0 upstream quirk verified); **the variable-expression Token dangling real-bug fix** (string_view → deep copy; latent since batch 3).
- **the eighth installment (D164–D176)**: WithInfantryBody's stand pick moves to the LocalRandom domain (upstream's not-synced Game.CosmeticRandom); the IActorPreview faces ride Phase 6 + the FactionInit/AnimationWithOffset ownership shapes; WithInfantryBody's frame-end barrel snapshot; WithMakeAnimation's latch members; ProximityCapturable's assembly-side precondition + the RunAfterTick injection + the by-Info-name captor lookup; Actor's empty IRenderModifier chain + ScreenMap's injected bounds sources; Map::SetSequences as the after-the-fact assembly port; FieldSaver's value-bag domain; Settings' two hand-written tables; ReplayRecorder's injected GameInformation payload + the in-memory sink; SyncReport's optional dumper registry (extending D30); the test fixture's synthetic asset copies + SkipMakeAnims semantics; **the ORA_HARDENED flag-equivalent build (this machine's gcc.exe being llvm-mingw's clang shim, the literal -fhardened is unrunnable — the equivalent set expands the GCC docs; real-GCC validation is a CI TODO)**.
**Not yet ported (the Phase 5-8 scope)**: the MapPreview async face, the tools/replaydiff differ, the 62 chrome widgets + 131 Logics, Fluent, WorldInteractionController, the real NetworkConnection, the full Session protocol, the replay differ, the server, Lua, AI, campaigns, and the Utility subcommands — see each phase's remaining section above.

## License & Attribution

This project is a derivative work of OpenRA and is released under the **GPL-3.0**, the same license as upstream. See [LICENSE](LICENSE).

- Upstream copyright: Copyright (c) OpenRA Developers and Contributors
- Rewrite code in this project: Copyright (c) 2026 xfcyhuang

OpenRA, Command & Conquer, Red Alert, and Dune 2000 are trademarks of their respective owners; this project is not affiliated with them.
