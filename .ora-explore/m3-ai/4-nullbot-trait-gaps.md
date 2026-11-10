# Issue #46 §4：NullBot 依赖 trait/机制对照 C++ 现状缺口（2026-11-10）

口径与证据等级同 §1。任务要求「先核实上游 NullBot 形态」——核实结果是**否定了票据预设**，本节以实证形态展开。

---

## 一、上游 NullBot 形态核实【实测】

1. **上游基线（1f594e320）无 NullBot**：全树 `git grep -i nullbot` 零命中；`mods/*/bots/` 目录不存在；`*.ai.yaml` 不存在。
2. **现行 bot = C# trait 体系（ModularBot）**：
   - 类型声明：`mods/ra/rules/ai.yaml:1-13`——Player actor 挂 `ModularBot@RushAI/NormalAI/TurtleAI/NavalAI`（Type: rush/normal/turtle/naval），共 4 bot 类型。
   - 门控：`GrantConditionOnBotOwner`（ai.yaml:14-25）按 bot 类型给 Player 发条件（enable-rush-ai 等），各 BotModule 以 `RequiresCondition` 挂门（如 ai.yaml:26-27）。
   - 模块：12 种 BotModule 数据驱动装配（ai.yaml:26-542）：ResourceMapBotModule/SupportPowerBotModule/HarvesterBotModule×2/BaseBuilderBotModule×4/PowerDownBotModule/BuildingRepairBotModule/SquadManagerBotModule×4/McvManagerBotModule/McvExpansionManagerBotModule×3/UnitBuilderBotModule×4/MinelayerBotModule。代码面 `OpenRA.Mods.Common/Traits/BotModules/` 21 文件 6444 行【实测 wc】。
   - **lua 与 bot 零交集**：BotModules 目录无 Eluant 引用；bot 不创建 ScriptContext。
3. **bot 注入链（任务问的 LobbyCommands→Server）**【精读】：
   - 大堂命令 `slot_bot`：`OpenRA.Mods.Common/ServerTraits/LobbyCommands.cs:479-559`——槽位校验 → 查 `Map.PlayerActorInfo.TraitInfos<IBotInfo>()`（:509-516，bot 类型来自**地图规则的 Player 定义**——mod 规则 ai.yaml 默认给全部地图提供）→ 建 `Session.Client{ Bot=botType, Index=ChooseFreePlayerIndex, Faction="Random", Color=RandomPresetColor(server.Random,...) }`（:521-544，**颜色用 server.Random 服务器随机——表现层**）→ `SyncClientToPlayerReference` → `SyncLobbyClients/Slots` 广播（:553-555）。
   - 命令载体：客户端发 `Order.Command("slot_bot ...")`，Server `InterpretOrder` case "Command"（`OpenRA.Game/Server/Server.cs:1016`）转 LobbyCommands。
   - 开局激活：Player ctor `IsBot = BotType != null`（`OpenRA.Game/Player.cs:202`）；**`if (IsBot && Game.IsHost)`（Player.cs:217）→ `logic.Activate(this)`（:223）——bot 逻辑只在 host 客户端跑**；`Game.IsHost` = 本地 client 是 admin（`OpenRA.Game/Game.cs:316-324`）。
4. **确定性架构（M3 对拍的根基）**【精读】：
   - ModularBot.Activate 注释原文："**Bot logic is not allowed to affect world state, and can only act by issuing orders**. These orders are recorded in the replay"（`ModularBot.cs:68-69`）。
   - 每帧：bot 决策跑在 `Sync.RunUnsynced(...)`（`ModularBot.cs:93`，调试开关 SyncCheckBotModuleCode 校验决策代码不改 SyncHash，`OpenRA.Game/Sync.cs:183-203`）→ 积压订单按 `MinOrderQuotientPerTick=5` 节流出队（`ModularBot.cs:101-103`）→ `world.IssueOrder` 进订单管道 → **订单经服务器广播/回放记录，是 bot 影响世界的唯一通道**。
   - **决策随机源 = `world.LocalRandom`**（`OpenRA.Game/World.cs:200` `new MersenneTwister()` 无种子——本地非同步流）：BaseBuilderBotModule.cs:174,252-254,439、BaseBuilderQueueManager.cs:149,202,257,259,290,334,387,510,515,518,583、MinelayerBotModule.cs:98,148、SupportPowerDecision.cs:120、CaptureManagerBotModule.cs:80,123、HarvesterBotModule.cs:140-141,176,295,313、McvExpansionManagerBotModule.cs:167-169,441【实测 grep，bot 目录 LocalRandom 30+ 消耗点、SharedRandom 零点】。
   - **推论（对拍口径的架构性结论）**：bot 决策非同步（LocalRandom + host-only），订单同步（广播/回放）→ ①回放重演形态下 bot 逻辑无需跑（订单在录包里，C++ T17 架构直接吃）；②live bot 对拍（C++ 自跑 bot 产订单）需 oracle 宿主与 C++ 双侧**把 LocalRandom 改为定种子**（golden_gen 宿主改造点，M1 已有 21 项无头剥离先例；C++ 侧 D041 --seed 先例）+ MersenneTwister 逐字节等价（T03 已达成）+ bot 逻辑 LocalRandom 消耗序逐点对齐。
5. **bot 订单类型全集**【实测 grep】：直接构造 12 种——AttackMove(5)/Move(4)/DeployTransform(4)/Harvest(3)/Sell(2)/ReturnToBase(2)/RepairBuilding(2)/Stop/SetRallyPoint/Dock/CaptureActor/Attack（各 1，`new Order("` 计数）；经工厂 3 种——`Order.StartProduction`（BaseBuilderQueueManager.cs:170 等）、`Order.CancelProduction`（:216）、`"PlaceBuilding"`（:182,229）。

## 二、C++ 现状对照与缺口分级

分级：**0=已有可复用**；**1=部分有/合成面需扩展**；**2=全缺**。D 编号指 docs/COVERAGE.md 偏离登记。

### 2.1 bot 框架层

| 依赖（上游锚） | C++ 现状 | 级 | 缺口说明 |
|---|---|---|---|
| ModularBot 本体（ModularBot.cs:43-121）：IBot 激活/订单队列/节流/ITick | 无 | **2** | PlayerModel 无 bot 字段（src/game/world/world.hpp:50-57 只有 faction/owns_world 等；str_bot 仅在大堂 Session 侧 lobby_session.hpp:35-37,108） |
| host-only 激活（Player.cs:217，Game.IsHost=ClientIsAdmin） | 大堂有 admin 概念雏形（D082 startgame 校验序） | **2** | world 创建链无「本地 client 是 admin」的等价判定位（单机形态下恒真即可） |
| IBotTick/IBotRespondToAttack 接口族（TraitsInterfaces.cs:408-420） | TraitSpec 有 fn_tick 钩子（trait.hpp:65-68）但 Player actor 的模块化 tick 收集面无 | **1** | 合成 trait 注册表（D015）可扩展 player-level tick 收集，机制同构 |
| Sync.RunUnsynced 包装（ModularBot.cs:93） | 无（C++ 无 unsynced 边界概念——rng_shared 账本即守卫） | 1 | 语义=C++ 侧断言 rng_shared 计数/末值在 bot tick 前后不变（D071 sfx 同款断言先例），非移植 RunUnsynced 本体 |
| LocalRandom（World.cs:200，无种子 MT） | WorldModel 只有 rng_shared（world.hpp:76） | **1** | 加 rng_local（MersenneTwister 已有 T03）；D071 音效已建「表现层随机隔离」先例 |
| 订单节流 MinOrderQuotientPerTick（ModularBot.cs:34,101-103） | 无 | 2 | 纯逻辑 ~20 行 |
| GrantConditionOnBotOwner 条件门控 | 条件系统缺（D031 GrantConditionOnPowerState 未做） | **2** | bot 面可先用「BotType 直接判空门控」等价替代（登记偏离），条件系统另票 |

### 2.2 bot 订单通道（订单类型 × C++ resolve 面）

| 订单 | C++ 现状 | 级 |
|---|---|---|
| StartProduction/CancelProduction/PlaceBuilding | 有（D028 生产链 + D044 放置桥；CancelProduction 解析有、黄金无覆盖 D028） | 0-1 |
| Move/Attack | 有桥形态（D044 launcher 桥 MoverState/FighterState，**不在世界 Activity 内**） | 1（行为面等价但架构欠账 D050） |
| AttackMove | 无 | 2 |
| Harvest（订单形态） | economy FDA 自治链有（T13），但「订单触发收割」的 resolve 无 | 1 |
| Dock | 无（economy 的 dock 预留是内部链路 D038） | 2 |
| Sell/RepairBuilding/ReturnToBase/Stop/SetRallyPoint/DeployTransform/CaptureActor | 无（Sellable/RepairableBuilding/RallyPoint/Transforms/CaptureManager trait 全缺） | 2 |

### 2.3 bot 依赖的世界机制（BotModule 运行时查询面）

| 机制（上游消费点） | C++ 现状 | 级 | 备注 |
|---|---|---|---|
| PlayerResources | 有（T13 economy，D038 最小面） | 0 | 多 harv 争用/再生不在最小验收面 |
| PowerManager 三档 | 有（D031；IPowerModifier 缺） | 0-1 | bot 的 MinimumExcessPower 决策可用 |
| ProductionQueue/BuildableItems | 有（D028） | 0 | IsPrimaryBuilding 降序缺（D028 已登记） |
| 局部 A* 寻路 | 有（T11，正向 laneBias） | 0 | 反向/双向已随 D036 部分兑现 |
| HPF（远距路径） | 缺（D025：远距>20 格 oracle HPF≠局部 A*） | **1** | bot 单位远距移动会踩 D025 已实证的分叉——live bot 对拍的前置 |
| 建筑放置判定（CanPlaceBuilding footprint + IsCloseEnoughToBase） | 有最小面（D028/D044） | 1 | PlaceBuildingVariants/LineBuild 缺（D028） |
| ActorIndex 索引扫描（BaseBuilderBotModule.cs:214-228） | 缺（vec_units 线性扫 D036④） | 1 | 线性扫在小规模等价，>2 单位结算序 D036④ 已预警 |
| AutoTarget/TakeCover/Veterancy（战斗随机面） | 缺（D036①：AutoTarget 扫描耗 SharedRandom） | **1** | **live bot 对拍硬前置**——bot 单位交战必触发 AutoTarget 扫描序（T17 已实证 AutoTarget 首扫抽样位，D054） |
| Move Activity 化（世界内移动重演） | 缺（D044 桥承载，D050 已列退役计划） | **1** | bot 的 Move/AttackMove 订单必须在世界内 resolve（订单管道等价），桥形态无法进 SyncHash 全量对拍（D054/D055 数据面即为此打的补丁） |
| SupportPowerManager（SupportPowerBotModule Requires，SupportPowerBotModule.cs 声明面） | 缺 | 2 | 可从最小 bot 配置（naval 无支持力决策？不——ai.yaml:33 全类型挂 SupportPowerBotModule）——**最小验收若含 rush/normal 则必做**；或首票裁掉该模块（登记偏离） |
| Shroud（bot 扫敌是否受战争迷雾过滤） | 缺（D032 无迷雾） | 1 | 上游 SquadManager 扫描用 world.FindActors + IsVisible 判定？——首票需精读 SquadManagerBotModule 确认迷雾依赖度；遭遇战默认开迷雾（ra 默认），live bot 对拍含迷雾面 |
| RallyPoint/SetRallyPoint | 缺 | 2 | BaseBuilderBotModule.cs:418 使用 |
| Sellable/RepairableBuilding/CaptureManager/Transforms（DeployTransform） | 缺 | 2 | 对应订单通道缺口 |

### 2.4 大堂/Session 面

| 依赖 | C++ 现状 | 级 |
|---|---|---|
| Session.Client.Bot/BotControllerClientIndex/AllowBots 字段与序列化 | **有**（lobby_session.hpp:35-37,49,99,108 + lobby_session.cpp:179-180,193,264-287——字段面+Serialize 对齐上游） | 0 |
| slot_bot 命令受理（LobbyCommands.cs:479） | 无（D082 命令子集 state/map/faction/team/startgame） | 2 |
| 大堂 UI bot 槽操作（人类 vs bot 开局入口） | 无（D083 手写 GL 大堂） | 2 |
| 开局玩家链 BotType→Player（Player.cs:202-223） | 无（PlayerModel 无 bot 位） | 2 |
| bot 颜色 RandomPresetColor（server.Random） | 无 ColorPickerManager | 2（表现层，对拍无关） |

## 三、缺口总结与最小验收面建议

1. **框架层全新**（ModularBot/激活链/LocalRandom/节流）：量小（~0.5-1k 行），机制简单，但牵 PlayerModel 扩位与 lobby_session 接线。
2. **真正的成本重心是三个「1 级」架构欠账**，全部已有偏离登记与兑现计划：
   - Move/Attack 世界内 Activity 化（D044/D050 退役计划）——bot 订单必须走订单管道进 SyncHash；
   - AutoTarget 等 SharedRandom 消耗 trait（D036① 兑现计划）——bot 交战面的 rng 账本；
   - HPF 远距路径（D025 兑现计划）——bot 单位远距机动面。
   这三件即使不做 bot 也列在恢复计划里，M3 是自然兑现点。
3. **最小 bot 配置建议**：首票以 `ModularBot@NormalAI`（normal 类型）为目标——ai.yaml 中 normal 的模块集 = ResourceMap+SupportPower+Harvester@normal-turtle+BaseBuilder@normal+PowerDown+BuildingRepair+SquadManager@normal+McvExpansion@normal+UnitBuilder@normal（ai.yaml:17-19,33-34,89-93,150-216,330-334,357-362,398-446）。SupportPowerBotModule 可作为唯一建议裁剪项（需要 SupportPowerManager 基建）——但注意 normal 的 UnitsToBuild 不含支持力建筑联动，裁剪影响面=决策延迟与 Cash 消耗差异，**裁剪即 live bot 对拍分叉**（对拍口径下不可裁，除非 oracle 侧同裁）。**结论：SupportPowerBotModule 的 SupportPowerManager 依赖是最小验收的隐藏大头，工作量票必须显式计入**。
4. **验收双口径的可行性结论**（User Story 10）：
   - 口径①（无头 bot vs bot 逐帧 SyncHash 黄金全等）：**可达**。路线 A（回放重演）：golden_gen 宿主以 admin 身份开局 bot vs bot（宿主改造点=本地 client admin 化 + Session bot client 注入，GoldenGen.Headless.cs 已有 IConnection.StartGame 无头链），录 .orarep → C++ T17 重演架构吃录包逐帧对拍——**此路线不依赖 C++ bot 实现**；路线 B（live bot）：双侧定种子 LocalRandom + bot 逻辑逐语义移植 + rng 账本对齐——完整验收（建议 M3 收口形态=先 A 后 B）。
   - 口径②（人类 vs bot 大堂开局可玩）：需 slot_bot 命令 + 大堂 UI bot 槽 + ModularBot 激活链 + 2.2/2.3 订单与机制面——工程面大但无不确定性（全部有上游源语义可抄）。
