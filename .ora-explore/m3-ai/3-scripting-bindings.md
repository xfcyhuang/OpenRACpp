# Issue #46 §3：OpenRA Scripting 绑定面盘点（2026-11-10）

口径与证据等级同 §1。任务原文含「NullBot 实际用到的子集清单」——经 §1/§4 实证上游无 NullBot 且 bot 不依赖 lua，该子集问题改写为两层：①上游暴露给 lua 的 API 全景（本节主体）；②地图脚本语料（230 文件）实际消费的 API 子集（§四）。bot 面的 API 需求见 §4 笔记（bot 走 C# trait 面，不走 lua 绑定）。

---

## 一、Lua context 与生命周期【精读】

| 环节 | 机制 | 锚 |
|---|---|---|
| 装配 | 地图 rules 声明 `LuaScript:` trait（`Scripts:` 文件集），TraitLocation=World、Requires SpawnMapActors、NotBefore SpawnStartingUnits | `OpenRA.Mods.Common/Scripting/LuaScript.cs:21-28` |
| 创建 | WorldLoaded 时 `new ScriptContext(world, worldRenderer, scripts)`；构造内建 `MemoryConstrainedLuaRuntime`（Eluant）→ 沙盒清洗 → 注册全局 → 逐脚本 `DoBuffer` → 抓取 `Tick` 函数 | `LuaScript.cs:42-46`；`OpenRA.Game/Scripting/ScriptContext.cs:144-228` |
| 沙盒 | 全局白名单 `{ipairs,next,pairs,pcall,select,tonumber,tostring,type,unpack,xpcall,math,string,table}`，白名单外全置 null；math 内 `random/randomseed` 置 null | `ScriptContext.cs:163-183` |
| 注入 | `EngineDir`、`FatalError`（委托）、`MaxUserScriptInstructions`（死变量，见 §2 笔记 2.5）、`print`→日志 | `ScriptContext.cs:186-194` |
| 全局表 | 反射扫全部 `ScriptGlobal` 子类（单参 ctor），按 `[ScriptGlobal("名")]` 挂全局表 | `ScriptContext.cs:196-211`（bindings 注册循环），属性定义 `ScriptContext.cs:69-77` |
| 内存限额 | 系统函数装载后 `MaxMemoryUse = MemoryUse + 50MB`，超限 Eluant 抛异常 | `ScriptContext.cs:122,215` |
| 回调 | `WorldLoaded()`（脚本顶层 WorldLoaded 函数一次）、`Tick()`（每帧调脚本 Tick） | `ScriptContext.cs:277-293,296-311` |
| 地图 actor 注入 | `RegisterMapActor(name, actor)` 把地图命名 actor 挂为 lua 全局（与保留名冲突抛 LuaException） | `ScriptContext.cs:268-275` |
| 错误终结 | FatalError → 日志 + `World.AddFrameEndTask(EndGame)`（对局终止，可观测） | `ScriptContext.cs:238-265` |
| 挂点 | LuaScript trait ITick 每帧 `Context.Tick()`； disposing 时 Dispose | `LuaScript.cs:49-51,52-60` |

## 二、成员暴露机制（对象包装与绑定协议）【精读】

核心三件（C++ 绑定层要逐语义复刻的协议）：

1. **包装规则**（`ScriptMemberWrapper.WrappableMembers`，`ScriptMemberWrapper.cs:125-142`）：只暴露「public 实例 + 声明于本类型（DeclaredOnly）」的**属性（恒可包）与方法（非泛型、非 specialname）**；字段不包。继承的成员不包（每 Properties 类自带表面）。
2. **调用协议**（`ScriptMemberWrapper.Invoke`，`ScriptMemberWrapper.cs:47-98`）：缺参用 C# 默认值（`pi[i].DefaultValue`）；多余实参丢弃；参数经 `TryGetClrValue` 转换（double→int/short/byte **截断**、LuaNil→null 引用、LuaTable→数组，`ScriptTypes.cs:34-143`）；返回值经 `ToLuaValue`。
3. **值包装**（`ToLuaValue`，`ScriptTypes.cs:146-190`）：double/int/bool/string 直值；`IScriptBindable` 对象包成 `LuaCustomClrObject`（表语义，经 `ILuaTableBinding`）；数组→LuaTable（1 起下标）；**其余类型抛 InvalidOperationException**（绑定面封闭集合，非任意 CLR 对象可过界）。
4. **表语义**（`ScriptObjectWrapper`，`ScriptObjectWrapper.cs:47-93`）：包装对象以 lua 表暴露——get/set 走成员字典，未知成员抛 `LuaException(MemberNotFoundError)`（消息含 actor 名/player 名，进 pcall 可观测面，见 §2 笔记 2.5）。
5. **actor/player 包装**（`ScriptActorInterface.cs:30-47`）：按 actor 的 ActorInfo 过滤可用命令类；**disposed actor 只留 `[ExposedForDestroyedActors]` 标注类**（`ScriptActorInterface.cs:42-44`），销毁时 Unbind 其余（`ScriptActorInterface.cs:49-55`）。player 侧同构（`ScriptPlayerInterface.cs:23-28`）。
6. **Activity 桥**：`[ScriptActorPropertyActivity]`（`ScriptContext.cs:41-42`）标注「该属性/方法会入队 Activity」的成员（Aircraft/Carryall/Combat/Mobile 等 Properties 广泛使用【实测 grep 20+ 处】）——**全仓库无运行时消费方，是文档性标注**（反射 grep 仅定义+标注两处）。脚本函数入队执行走 `CallLuaFunc` Activity（`OpenRA.Mods.Common/Scripting/CallLuaFunc.cs:19-59`，`function.CopyReference()` 持有 + Tick 时 Call + 异常转 FatalError）。

## 三、API 全景清单【实测 ls-tree + grep 计数】

量级：**Global 16 文件 → 20 张全局表；Properties 34 类（14 个 ScriptPropertyGroup 分组）；公共可绑定成员合计 ~366 个**（`^\t\tpublic` 计数，含少量非绑定辅助成员，作上界口径）。

### 3.1 全局表（Global，按 `ScriptGlobalAttribute`）

| lua 表 | 文件 | 可绑定成员数 | 备注 |
|---|---|---|---|
| Trigger | Global/TriggerGlobal.cs | 36 | 最大表：OnKilled/OnDamaged/AfterDelay/OnIdle/OnEnteredFootprint/... 全回调注册 |
| Map | Global/MapGrid?（MapGlobal.cs） | 14 | ActorsInWorld/ClosestEdgeCell/Rulesheet 等 |
| Media | Global/MediaGlobal.cs | 13 | 表现层：DisplayMessage/PlaySoundNotification/... |
| DateTime | Global/DateTimeGlobal.cs | 12 | GameTime=WorldTick（确定）；IsHalloween/CurrentMonth/CurrentDay=墙钟（§2 笔记 2.4） |
| CPos/CVec/WPos/WVec/WDist | Global/CoordinateGlobals.cs:18,56,69,82,95 | 11（合计） | 坐标构造/量纲 |
| Utils | Global/UtilsGlobal.cs | 9 | Do/Where/Random/Shuffle/Take/Skip/...；Random 族→SharedRandom |
| Angle | Global/AngleGlobal.cs | 9 | WAngle 量纲 |
| Actor | Global/ActorGlobal.cs | 4 | Create/... |
| Player | Global/PlayerGlobal.cs | n | GetPlayer/... |
| Beacon/Camera/HSLColor/Lighting/Radar/Reinforcements/UserInterface | 其余 Global 文件 | 小表 | 多为表现层 |

（精确逐成员清单建议在实现票内以「反射 dump → lua-api.txt 黄金」生成，oracle 侧一行 `GetTypesImplementing<ScriptGlobal>()` 即可产真源；本票给全景与量级。）

### 3.2 actor/player 属性表（Properties，按 ScriptPropertyGroup 分组 → 挂到对应 actor 的 lua 包装上）

分组（14 个）：General/Movement/Combat/Ability/Support Powers/AmmoPool/Cloak/Condition/Diplomacy/Experience/MissionObjectives/Player/Power/Production/Resources/Transports。34 类清单（类名 | 分组 | trait 依赖）【实测提取】：

- General：GeneralProperties(IOccupySpace)、HealthProperties(IHealth)、SellableProperties(Sellable)、TransformProperties(Transforms)、RepairableBuildingProperties、MissionObjectiveProperties、ConditionProperties(ExternalCondition)
- Movement：MobileProperties(Mobile)、AircraftProperties(Aircraft)、HarvesterProperties(Harvester)、ScaredCatProperties(ScaredyCat)
- Combat：CombatProperties(AttackBase)、GuardProperties(Guard)、DemolitionProperties(IMove)
- Ability：CaptureProperties(CaptureManager)、CarryallProperties、DeliveryProperties(IMove)、InstantlyRepairsProperties(IMove)
- Support Powers：AirstrikeProperties、NukeProperties、ParatroopersProperties
- Transports：ParadropProperties(Cargo)、TransportProperties(Cargo)
- 其余：AmmoPool/Cloak/Diplomacy(无 Requires)/Experience(GainsExperience)/Player(PlayerCondition/PlayerExperience/PlayerProperties/PlayerStats)/Power(PowerManager)/Production(Production)/Resources(PlayerResources)

**绑定启用协议**：Properties 类不按 trait 装配逐 actor 实例化，而是 `ScriptContext.ActorCommands` 按 ActorInfo 的 trait 依赖（`RequiredTraitNames`，`ScriptMemberWrapper.cs:144-155`）过滤——actor 有 Mobile 才挂 Movement 表成员。**C++ 现状对接**：D015 合成 trait 注册表以 TraitSpec 声明面表达同类信息（str_kind/str_order_id/str_param 面），绑定层可加「lua 成员声明」面（声明表驱动，反射→静态注册表，与 WidgetTypeSpec 先例同构 D076）。

### 3.3 数值/枚举过界规则（对拍敏感子集，详 §2 笔记 2.1-2.2）

double↔int 截断、数组↔LuaTable（1 起下标）、nil↔null——三条是绑定层仅有的数值语义转换点。

## 四、地图脚本实际用集（230 lua 文件全量统计）【实测】

全局 API 调用频次 top（`git show` 全语料 grep 聚合）：

| 调用 | 次数 | | 调用 | 次数 |
|---|---|---|---|---|
| CPos.New | 2841 | | Trigger.OnKilled | 374 |
| DateTime.Seconds | 1830 | | Media.PlaySpeechNotification | 355 |
| Trigger.AfterDelay | 1695 | | Media.DisplayMessage | 272 |
| Utils.Do | 1001 | | DateTime.GameTime | 243 |
| DateTime.Minutes | 713 | | Trigger.OnEnteredFootprint | 212 |
| Actor.Create | 555 | | Utils.Random | 424 |
| UserInterface.GetFluentMessage | 457 | | Utils.RandomInteger | 201 |
| Player.GetPlayer | 452 | | Reinforcements.ReinforceWithTransport | 192 |
| Reinforcements.Reinforce | 393 | | Utils.Where | 154 |

读法：
1. **量纲构造（CPos/WDist/DateTime.Seconds）与延迟调度（Trigger.AfterDelay）占绝对大头**——绑定面的第一批必需件。
2. **随机族（Utils.Random 424 + RandomInteger 201）全部走 SharedRandom**（§2 笔记 2.4）——绑定层随机桥是对拍硬点。
3. 表现层族（Media/UserInterface/Camera）占可视大面但不进 SyncHash。
4. Actor.Create（555）会造 actor（进同步面：ActorID 序/构造序，D015 三支柱）。
5. 战役脚本库：`mods/common/scripts/utils.lua`（34 行：IdleHunt/AddPrimaryObjective/AddSecondaryObjective）与 `mods/ra/scripts/campaign.lua`（81 行）是全部战役图的共享前缀【实测】。

## 五、绑定层移植的工作量画像（给 §5 供数）

- 机制面（context/沙盒/包装协议/错误路径）：约等于上游 7+3 文件语义（ScriptContext 346 行 + Wrapper 族 + CallLuaFunc + LuaScript），C++ 预估 1.5-2.5k 行（声明表驱动，无反射，参照 D076 WidgetTypeSpec）。
- API 面本体：遭遇战最小通路（fort-lonestar/oil-spill 级）所需子集 ≈ 上表前 25 API + Trigger 全表 + 数值过界规则 ≈ Properties 5-8 类 + Global 8 表；战役全量（Phase 11/M4）才是 366 成员全量。**建议切票按「最小通路子集→战役全量」两档**（§5 采纳）。
- 黄金设施：oracle 反射 dump API 清单（一票产物）+ probe.lua 语义对拍（§2 笔记 §五）。
