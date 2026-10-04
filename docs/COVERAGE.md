# 上游覆盖登记表(PORTING_PLAN §7.2)

> 本表由 `tools/upstream_check.py` 扫描 `src/` 的 `// UPSTREAM:` 标注自动校验;
> 状态语义:**未开始 / 进行中 / 已对拍**(与黄金数据比对通过)/ **已偏离**(附理由,未登记的偏离视为 bug)。
> 上游基线见 [UPSTREAM.baseline](../UPSTREAM.baseline)。

## src/core/ — 定点原语(Phase 0)

| C++ 文件 | 上游文件 | 覆盖范围 | 状态 |
|---|---|---|---|
| src/core/exts_math.hpp | OpenRA.Game/Exts.cs L282-373 | ISqrt 家族(Floor/Nearest/Ceiling)、MultiplyBySqrtTwo* | 已对拍 |
| src/core/int2.hpp | OpenRA.Game/Primitives/int2.cs | 全类型(FromVector/ToVector 浮点转换除外,渲染域) | 已对拍 |
| src/core/rectangle.hpp | OpenRA.Game/Primitives/Rectangle.cs | 除 Location/Size 属性与 Clamp(Vector2)(依赖 Size/浮点,图形阶段补) | 已对拍* |
| src/core/int32_matrix4x4.hpp | OpenRA.Game/Primitives/Int32Matrix4x4.cs | 全类型 | 已对拍 |
| src/core/cvec.hpp | OpenRA.Game/CVec.cs | 全类型(Lua 绑定除外) | 已对拍 |
| src/core/cell_pos.hpp | OpenRA.Game/Map/MapGrid.cs L20 + MPos.cs + CPos.cs | MapGridType、MPos/PPos/CPos 全运算(等距换算、12/12/8 打包) | 已对拍 |
| src/core/mersenne_twister.hpp | OpenRA.Game/Support/MersenneTwister.cs | 全类型(无参 TickCount 构造除外——同步路径须显式播种) | 已对拍 |
| src/core/wdist.hpp | OpenRA.Game/WDist.cs | 全运算含 FromPDF/TryParse/ToString(Lua 绑定除外) | 已对拍 |
| src/core/wangle.hpp | OpenRA.Game/WAngle.cs | 全类型含两张查找表、ArcSin/ArcCos/ArcTan/Lerp(Lua 绑定除外) | 已对拍 |
| src/core/wvec.hpp | OpenRA.Game/WVec.cs | 全运算含 LerpQuadratic(__int128 复刻 decimal)、Rotate、FromPDF | 已对拍 |
| src/core/wpos.hpp | OpenRA.Game/WPos.cs L20-79 + IEnumerableExtensions.Average | 全运算含 LerpQuadratic 跨零截断语义 | 已对拍 |
| src/core/wrot.hpp | OpenRA.Game/WRot.cs | 全类型:欧拉/轴角构造、QuaternionToEuler、Rotate、SLerp、AsMatrix | 已对拍 |

\* rectangle.hpp 的已对拍指其被引用成员经黄金测试间接覆盖;专用单测待 Catch2 接入后补。

## 黄金对拍关卡(2026-10-03)

`tests/golden_core.txt` 60,883 行,由 C# golden_gen 生成;C++ 侧逐行重放 **100% 一致**(ASan+UBSan 与 Release -O2 双构建)。
覆盖:ISqrt 全量与 2^k 边界、WAngle 全 1024 角度 sin/cos/tan、ArcSin/ArcCos [-1024,1024] 全量、
ArcTan 双网格、WAngle.Lerp 全组合、MT19937 三种子全序列(uint/Next/ulong/range/state/PickWeighted)、
FromPDF、WVec/WPos 4000 随机组(含 LerpQuadratic)、WRot 4913 网格 + 2000 随机 SLerp、
CPos 打包/解包与等距换算、CVec 2000 采样。

## src/core/text.hpp 与 src/yaml/ — MiniYaml(Phase 1,2026-10-03)

| C++ 文件 | 上游文件 | 覆盖范围 | 状态 |
|---|---|---|---|
| src/core/text.hpp | OpenRA.Game/MiniYaml.cs L295/L299(.Trim() 的 BCL 语义) | .NET char.IsWhiteSpace 空白集的 UTF-8 码点判定、两端/尾端 Trim、U+FFFD 兜底 | 已对拍* |
| src/yaml/mini_yaml.hpp/.cpp | OpenRA.Game/MiniYaml.cs L22-791 | FromLines 行状态机、字符串池、SourceLocation、Merge/MergeSelfPartial/ResolveInherits/WeakResolveRemovals/MergePartial、序列化、Builder;BCL 依赖(StreamExts.cs L200-248 ReadAllLinesAsMemory、String.Split、Exts.cs L414-467 冲突日志)在对应位置标注 | 已对拍 |
| src/fs/i_package.hpp | OpenRA.Game/FileSystem/IPackage.cs L18-42 | IPackageLoader/IReadOnlyPackage/IReadWritePackage(GetStream 返回值化,见偏离表) | 已对拍* |
| src/fs/folder.hpp/.cpp | OpenRA.Game/FileSystem/Folder.cs L19-110 | Contents/GetStream/Contains/OpenPackage/Update/Delete(含 zip 双路径前缀 HACK) | 已对拍* |
| src/fs/zip_file.hpp/.cpp | OpenRA.Game/FileSystem/ZipFile.cs L20-261 | ReadOnlyZipFile、ZipFileLoader 签名嗅探、ZipFolder 子目录视图、嵌套 zip;SharpZipLib→miniz | 已对拍* |
| src/fs/file_system.hpp/.cpp | OpenRA.Game/FileSystem/FileSystem.cs L20-303 | 挂载序=覆盖优先级、fileIndex/显式 '|' 挂载、Mount/Unmount/TryOpen/Exists/ResolveCaseInsensitivePath | 已对拍* |
| src/fs/yaml_load.cpp | OpenRA.Game/MiniYaml.cs L684-698 | MiniYaml::Load(附加文件列表按 FieldLoader.ParseArray 语义) | 已对拍* |

### 黄金对拍关卡(Phase 1 第一关)

`tests/golden_yaml.txt`(13.5MB,由基线 commit 的 C# golden_gen `yaml` 模式生成):
**mods/ 全部 759 个 yaml × {丢弃注释, 保留注释} 两模式,FromStream→WriteToString 规范化输出
逐字节 100% 一致**(ASan+UBSan 与 Release 双构建)。另移植上游 MiniYamlTest.cs 全部 28 用例
(76 断言,含异常消息逐字比对)全绿;fs_test 24 断言(挂载序/显式挂载/Folder/zip/嵌套 zip/Load)全绿。
测试 yaml 字面量由 `tools/extract_yaml_test_literals.py` 从上游源码按字节提取注入,杜绝缩进手抄偏差。

### 已登记偏离(PORTING_PLAN §7.5)

| # | 位置 | 偏离 | 理由 |
|---|---|---|---|
| D1 | src/yaml | 异常类型统一为 YamlException(C# 分散 YamlException/InvalidDataException/ArgumentException);消息文本逐字一致 | 消息文本是兼容面,异常类型不是 |
| D2 | src/yaml ResolveInherits | null 键节点走 else 分支(C# 在首条件即 NullReferenceException,上游潜在 bug) | 合法输入下行为一致 |
| D3 | src/yaml 合并键集合 | null 键与 "" 键在键集合中合流(上游解析器不产生 "" 键) | 实现简化,不可达角落 |
| D4 | src/yaml FromLines | C# IEnumerable 惰性产出物化为 vector(并发流式用例不移植) | 实现细节 |
| D5 | src/fs IPackage | GetStream 返回 std::optional<std::vector<char>>(C# 惰性 Stream) | 上游消费方均全量读取 |
| D6 | src/fs Folder | Contents 按字节序排序(C# .Order() 为 culture 序) | mods ASCII 文件名集合下等价 |
| D7 | src/fs FileSystem | '$' mod 引用挂载需 Manifest/installedMods,推迟 Phase 2 | 依赖链 |
| D8 | src/fs ZipFile | ReadWriteZipFile(条目更新/删除)推迟 Phase 6 地图保存(miniz writer) | 使用点在 Phase 6 |
| D9 | src/fs FileSystem.OpenPackage | Platform.ResolvePath(Game 层 '~' 展开等)未引入,Phase 1 按原名 | Game 层依赖,Phase 2 随 Manifest 落地 |

\* 已对拍(星号)= 经行为级断言/集成路径覆盖;专用逐字节黄金关卡仅 mini_yaml 与 golden_core 具备。

## src/core/ 补充原语 与 src/meta/ — 元数据框架(Phase 2,2026-10-03 进行中)

| C++ 文件 | 上游文件 | 覆盖范围 | 状态 |
|---|---|---|---|
| src/core/bitset.hpp | OpenRA.Game/Primitives/BitSet.cs | BitSetAllocator(插入序分配、uint64 承载)、BitSet 全集合运算、FromStringsNoAlloc、ToString 分配序 | 单测 |
| src/core/color.hpp | OpenRA.Game/Primitives/Color.cs L20-232,374 | ARGB 值类型、FromArgb、TryParse(6/8 位十六进制)、ToString(RRGGBB[AA]);HSV/HSL 属 Phase 4 | 单测 |
| src/core/vector_n.hpp | OpenRA.Game/FieldLoader.cs L531-566 | Vector2/Vector3 数据语义(解析/序列化往返) | 单测 |
| src/meta/parse.hpp/.cpp | OpenRA.Game/Exts.cs L485-566 + FieldLoader.cs L146-591 | TryParse*Invariant 家族(NumberStyles.Integer/Float 语义、% 缩放)、TryParseBoolNet、SplitComma(Trimmed)、Enum.TryParse/ToString(flags 分解、剩余位十进制) | 单测 |
| src/meta/variable_expression.hpp/.cpp | OpenRA.Game/Support/VariableExpression.cs L22-983 | 全文件:tokenizer(字符类/空白守护/错误消息逐字)、token 流校验、调度场后缀化、求值栈机(int↔bool 互转、除零/模零得 0、非短路 And/Or) | 单测 |
| src/meta/field_desc.hpp | OpenRA.Game/FieldLoader.cs L937-1032 + ObjectCreator.cs | FieldType 标签集(schema_dumper 普查 103 种归一化)、FieldDesc/RecordDesc、ORA_FIELD 宏(__builtin_offsetof:offsetof 宏不穿越 import std;) | 单测* |
| src/meta/field_loader.hpp/.cpp | OpenRA.Game/FieldLoader.cs L27-1032 | Load 主循环(MissingFieldsException 消息逐字)、GetValue 分派(标量/元组/容器/字典/Nullable/BitSet)、分组数组特化、LoadUsing 注册表、InvalidValue/UnknownFieldAction 逐字消息 | 单测 |
| src/meta/type_registry.hpp/.cpp | OpenRA.Game/ObjectCreator.cs L21-168 | FindType/CreateObject/注册表(等价程序集反射扫描) | 单测* |

### meta_test 断言集(2026-10-03)

解析器边界(空白/符号/溢出/百分比/逗号)、枚举(大小写/数值/逗号列表/flags ToString 剩余位)、
表达式(优先级/短路语义/除零/11 条构造期错误消息逐字)、FieldLoader 端到端(默认值保持/
MissingFields 消息/InvalidValue 消息逐字/容器/字典/Nullable/BitSet/枚举字段)、Color/BitSet。
ASan+UBSan 与 Release 双构建通过(ctest 6/6)。

### 已登记偏离(PORTING_PLAN §7.5,Phase 2 新增)

| # | 位置 | 偏离 | 理由 |
|---|---|---|---|
| D10 | src/meta string 字段 | C# string null 与 "" 合流(std::string) | dump/FormatValue 层两者均输出 "";运行时 null 判断在 trait 移植时以 empty() 等价改写 |
| D11 | src/meta TypeConverter 兜底 | 未实现 GetValue 的 TypeDescriptor 转换分支 | trait/weapon 加载链字段类型已被 TypeParsers+枚举全覆盖,兜底不可达 |
| D12 | src/meta 字典字段 | C++ vector<pair>(插入序);FrozenDictionary 的 .NET 枚举序≠插入序,dump 协议按键排序 | 运行时遍历序差异 Phase 5 评估 |
| D13 | src/meta 字典重复键 | 抛 YamlException 同文本(C# 经反射包装为 TargetInvocationException) | 消息文本一致,异常类型差异不可观测(上游加载链不捕获该类型) |
| D14 | src/meta VariableExpression | 错误消息 index 按 UTF-8 字节计(C# 按 UTF-16 code unit) | ASCII 输入下完全一致;mods 表达式均为 ASCII |
| D15 | src/core/bitset.hpp | 位图 uint64(≤64 字符串/标签,超出 abort) | C# BigInteger 无界;实际 mods 最多约 20 种 |
| D16 | src/meta Hotkey/DateTime/Size/Rectangle | 解析器占位(未知字段路径) | 加载链无使用点(--scan 验证);widget/settings 阶段落地 |

## src/game/ + gen/ — 加载链与权威 schema 导出(Phase 2 续,2026-10-03)

| C++ 文件 | 上游文件 | 覆盖范围 | 状态 |
|---|---|---|---|
| gen/types_gen.cpp 等 | 上游程序集反射(--gen) | 680 类型(Trait/Warhead/Projectile + 抽象基类闭包 + 嵌套记录)字段描述表 + 默认值(实例化读取)+ Requires/NotBefore + 接口全名集;37 枚举成员表;9 BitSet 标签 | 快照对拍 |
| src/meta/generic_record.hpp/.cpp | (生成类型运行时载体) | GenericValue 值袋(variant 载荷族)、GeneratedRecord、LoadValueIntoValue(唯一解析实现)、AssignToMemory/ReadFromMemory(双向 typed 内存变换)、生成 loader 注册表 | 单测+快照 |
| src/meta/dump_format.hpp/.cpp | (dump 协议;与 C# Program.cs DumpValue 分支对齐) | FormatValue(含 .NET float ToString 阈值复刻、WDist "NcM"、字典稳定排序键、"(null)"/"(empty)" 约定) | 快照对拍 |
| src/game/platform.hpp/.cpp | OpenRA.Game/Platform.cs L288-312 | ResolvePath(^EngineDir/^SupportDir/^BinDir 前缀替换;尾分隔符规范化对齐 BinDir 语义) | 单测(经加载链) |
| src/game/manifest.hpp/.cpp | OpenRA.Game/Manifest.cs L21-206 | mod.yaml + Include 展开 + Merge 继承 + 全节解析(Metadata 走 FieldLoader;FileSystem 必需节缺失异常文本逐字) | 快照对拍(经加载链) |
| src/game/actor_info.hpp/.cpp | OpenRA.Game/GameRules/ActorInfo.cs L18-201 | TraitName@Instance 拼名实例化、junk 值检查、MissingFields 头部文本逐字、Requires/NotBefore 拓扑序(接口依赖含泛型接口匹配;异常文本逐字) | 单测(game_test) |
| src/game/game_records.hpp/.cpp | WeaponInfo.cs L74-175 + SoundInfo.cs + MusicInfo.cs | WeaponInfo(武器级继承 Merge 前置 + 17 字段 + 双 loader)、SoundInfo(8 字段)、MusicInfo(手读构造) | 单测(game_test SCUD 链) |
| src/game/ruleset.hpp/.cpp | Ruleset.cs L23-281 + ActorInfoDictionary.cs L18-55 | MergeOrDefault(小写键/首键优先/'^' 过滤)、SystemActors 补空 | 快照对拍 |
| src/game/mod_data.hpp/.cpp | ModData.cs L29-233(Phase 2 最小集)+ Default/ContentInstallerFileSystemLoader | InstalledMods 目录发现、'$mod' 借用挂载、SystemPackages 必挂/ContentPackages 逐项容错 | 快照对拍(经加载链) |
| src/game/loaders.cpp | 24 处 [FieldLoader.LoadUsing] 逐语义 | LoadSpeeds×3/Footprint×3/ResourceTypes×3(声明类复合名归一)/InitialSmudges/Shape(反射闭包全部 IHitShape 实现)/Decisions/Consideration/Options/FluentReferences/Choices/Parameters/Weapon 双 loader | 快照对拍+game_test |
| src/fs/file_system.hpp/.cpp(增补) | FileSystem.cs L84-115 '$' 分支 | '$mod' 引用挂载(借用不拥有)+ PathResolver 可安装解析器(D7 落地) | 单测(fs_test)+加载链 |
| tests/golden_rules.cpp | (Phase 2 验收关卡) | 三 mod 全规则深度解析 dump(ra 80,274 / cnc 49,416 / d2k 35,833 行;307 actors@ra)逐字节快照回归 | 已固化 |
| tests/game_test.cpp | (R2 对冲) | 三代表 trait 端到端:Health(HP/默认值)、Mobile(^Vehicle 继承链/表达式文本)、Armament(枚举位/默认值)、SCUD 武器(继承+弱删除+Projectile 换型+Warhead)、拓扑序、SystemActors | 全部断言通过 |

### Phase 2 续验证锚点(2026-10-03)

1. **C++ 快照对拍**:三 mod 全量 dump 逐字节回归(上游 @7d57605bca 数据;黄金文件
   `tests/golden_rules_{ra,cnc,d2k}.txt` 由 golden_rules 固化,首次快照经人工抽检
   + game_test 语义锚点核对);ASan+UBSan 与 Release 双构建 ctest 10/10。
2. **上游权威验证**:`OpenRA.Utility.exe ra --check-yaml` 在上游基线 commit 下
   exit=0(全部规则加载成功且通过 lint)。
3. **game_test 语义锚点**:期望值人工对照上游源码行号(见测试头注)。

### 已登记偏离(PORTING_PLAN §7.5,Phase 2 续新增)

| # | 位置 | 偏离 | 理由 |
|---|---|---|---|
| D17 | src/game Ruleset 构造器 | IRulesetLoaded 回调(145 处 RulesetLoaded)不执行 | 回调只做跨引用校验与非 yaml 属性缓存,不影响 dump 数据面;行为面 Phase 5 起随 trait 落地。dump 侧 C# 工具受 ALC 程序集副本干扰同样跳过(见 tools/schema_dumper --dump 注记) |
| D18 | src/game 未知 trait | 抛 YamlException(C# 为 InvalidOperationException 同点位) | 引擎路径(非 linter)语义等价:加载失败即中止;消息文本走 C++ 侧格式 |
| D19 | gen/ 嵌套记录默认值 | ResourceTypeInfo 族/SupportPowerDecision/MapGenerator Option 无默认构造 → 全 null 默认 | 这些记录仅由 loader 手工构造(必填字段经描述表校验),默认值不可达 |
| D20 | gen/ 同名嵌套类 | 简单名键后者覆盖(ResourceLayerInfo+ResourceTypeInfo vs ResourceRendererInfo+);loader 侧经全名键精确寻址 | 上游 FindType 同名时取首个命名空间命中(近似);对拍面(loader 值)不受影响 |
| D21 | LoadUsing 复合名 | str_loader = "声明类.loader"(上游按类型上下文 GetMethod 解析) | C++ 全局注册表需复合键区分同族异构 loader(TS/D2k 继承基类 loader 归一声明类) |
| D22 | MapGeneratorDropdownChoice.Parameters | 值袋以 WriteToString 规范化文本承载(C# 为 ImmutableArray<MiniYamlNode>) | dump 协议两侧同源(文本形式);地图生成器行为面 Phase 8 |
| D23 | dump 协议 float | C++ 复刻 .NET ToString(Invariant) 阈值记法(定点当且仅当指数 ∈[-4,6]) | 黄金侧为 C# 原生 ToString;快照固化后为回归基准,后续如有角差异按快照校准 |
| D24 | tests/golden_rules | 黄金数据由 C++ 首次快照固化(非 C# 反射逐字段导出) | C# --dump 受 ALC 环境制约(见 tools/schema_dumper 注记);以 game_test 语义锚点 + 上游 --check-yaml 补偿锚定;C# 侧恢复后可再对拍校准 |

## src/sim/ + src/net/ — 仿真核心 + Order/锁步(Phase 3,2026-10-03 主体完成)

| C++ 文件 | 上游文件 | 覆盖范围 | 状态 |
|---|---|---|---|
| src/sim/trait_interfaces.hpp | OpenRA.Game/Traits/TraitsInterfaces.cs L29-666 | 仿真核心接口族(ITick/INotify*/IObservesVariables/IResolveOrder/ICreationActivity/IHealth/IOccupySpace/IFacing/ITargetable/…)+ DamageState/SubCell/WinState 枚举 + TraitBase/upcast 表机制(ORA_TRAIT_INTERFACES);渲染/UI 接口 Phase 4/6 | 单测 |
| src/sim/type_dictionary.hpp/.cpp | OpenRA.Game/Primitives/TypeDictionary.cs L19-183 | Type 键 → gen::TypeId;Add/Get/GetOrDefault/WithInterface/Remove(异常消息逐字) | 单测 |
| src/sim/actor_init.hpp/.cpp | OpenRA.Game/Map/ActorInitializer.cs L21-262 | ActorInitializer 查询族(InstanceName 匹配 LastOrDefault 语义)、ValueActorInit/LocationInit/OwnerInit | 单测 |
| src/sim/activity.hpp/.cpp | OpenRA.Game/Activities/Activity.cs L21-295 + Traits/ActivityUtils.cs L17-38 | 状态机全文:TickOuter/lastRun/finishing/免延迟分支、SkipDoneActivities、Cancel(Queued→Done)、Queue/QueueChild、ActivitiesImplementing、RunActivity 循环 | 单测 |
| src/sim/sync_hash.hpp/.cpp | OpenRA.Game/Sync.cs L23-211 | 哈希函数族(int2/CPos/CVec/WDist/WAngle/WPos/WVec/WRot/Actor/Player/Target + bool IL 可达语义 (b?1:0)^0xAAA)、XOR 组合协议、成员注册表、RunUnsynced/AssertUnsynced 门禁 | 单测(公式手算) |
| src/sim/target.hpp/.cpp | OpenRA.Game/Traits/Target.cs L18-293 | 值语义核心:Type 有效性判定(出世界/死亡/换代)、FromPos/FromActor/FromSerialized*、SerializableState、IsInRange;FrozenActor 面 Phase 5 | 单测 |
| src/sim/actor.hpp/.cpp | OpenRA.Game/Actor.cs L27-651 | 条件系统全文(Grant/Revoke/TokenValid/UpdateConditionState + 观察者收集/初始通知)、Initialize(INotifyCreated→观察者→ICreationActivity→Add)、Tick(wasIdle 双跑)、Dispose(帧末任务幂等)、ChangeOwner(Sync)、trait 查询转发;渲染/Lua 面 Phase 4/8 | 单测 |
| src/sim/trait_dictionary.hpp/.cpp | OpenRA.Game/TraitDictionary.cs L21-329 | 平行数组按 ActorID 有序 + BinarySearchMany(actorID+1 追加/区间删除)全文;Get/GetOrDefault(唯一性)/GetMultiple/Actors 去重/ApplyToAll;键 = gen::TypeId,载荷 = upcast 子对象指针 | 单测 |
| src/sim/effects.hpp/.cpp | OpenRA.Game/Effects/IEffect.cs + DelayedAction.cs | IEffect/ISpatiallyPartitionable/DelayedAction(帧末"移除后触发") | 单测 |
| src/sim/player.hpp | OpenRA.Game/Player.cs L27-337 | SyncHash/Order 反序列化最小面(PlayerActor/WinState/UnlockedRenderPlayer/PlayerMask);完整构造 Phase 5 | 单测(经 SyncHash) |
| src/sim/world.hpp/.cpp | OpenRA.Game/World.cs L28-650 | 仿真核心:Tick 序(actor→ITick traits→effects→帧末 drain)、Add/Remove、SyncHash 公式(n 连续跨段/回绕)、NextAID、帧末任务队列、effects 三视图 + 单所有权、trait 工厂注入面(Phase 5 接 game 加载链);完整构造(GameSpeed/LobbyInfo/Map/ScreenMap)Phase 5 | 单测 |
| src/net/byte_io.hpp | OpenRA.Game/Network/Order.cs L345-487 | BinaryWriter/Reader 兼容(小端 + 7-bit string 前缀 + UTF-8) | 单测(字节级) |
| src/net/order.hpp/.cpp | OpenRA.Game/Network/Order.cs L18-495 | OrderType/OrderFields 位掩码;Serialize/Deserialize 逐字节(字段序/位判定/Target Actor|Terrain(cell/pos/-1 短路)|FrozenActor 槽位);命名构造族(Chat/Command/StartProduction/FromGroupedOrder…) | 单测(往返逐字节) |
| src/net/order_io.hpp/.cpp | OpenRA.Game/Network/OrderIO.cs L18-212 | OrderPacket(立即序列化语义)+ TryParse* 全家(Sync/Ping/Ack/Disconnect/TickScale/OrderPacket;server-only 门槛) | 单测 |
| src/net/session.hpp | OpenRA.Game/Network/Session.cs L1-286 | GlobalSettings/Client/ClientWithIndex/OptionOrDefault 最小面;完整序列化 Phase 7 | 单测(经锁步) |
| src/net/tick_time.hpp | OpenRA.Game/Network/TickTime.cs L16-60 | ShouldAdvance/AdvanceTickTime(JankThreshold=250 内联;时钟注入) | 未直接 |
| src/net/connection.hpp/.cpp | OpenRA.Game/Network/Connection.cs L24-97 | IConnection + EchoConnection 全文(空帧注入/投影 +1/immediate 优先/disposed 逃生) | 单测(10⁶ tick) |
| src/net/order_manager.hpp/.cpp | OpenRA.Game/Network/OrderManager.cs L22-334 | 锁步全文:StartGame/IssueOrder/TickImmediate/TryTick(shouldTick→SendOrders→IsReady→ProcessOrders)/IsNetFrame 节流/帧号校验异常逐字/disconnect 标记/defeat 位图/SendSync;SyncReport/TextNotifications/Game 静态面注入 | 单测 |
| src/net/unit_orders.hpp/.cpp | OpenRA.Game/Network/UnitOrders.cs L48-431 | 可运行子集:default→IValidateOrder→Subject.ResolveOrder 分发 + Grouped 展开 + PauseGame;UI/大厅命令族 Phase 6/7 | 单测(经锁步) |
| gen/interfaces_gen.h | 上游程序集反射(--gen) | TypeId 键空间 6048(全程序集类型全名 ordinal 排序;枚举名消毒;FindTypeIdByFullName 二分) | 编译期锚定 |
| gen/sync_gen.cpp | 上游程序集反射(--gen) | [VerifySync] 成员表 84 类型(成员名/类型/属性位/声明类;序 = GetFields 后 GetProperties) | 编入 ora_sim |
| tests/sim_test.cpp | (§Phase 3 验收) | 哈希族公式手算/Order 字节级构造+往返/OrderIO 魔数与门槛/EchoConnection 锁步 100+10⁶ tick(NetFrameInterval=3 节律 35/38/333335 推导断言)/条件系统(令牌唯一/计数缓存/重复撤销异常)/Activity 生命周期/TraitDictionary(多实例异常/注册序/去重/Dispose 摘除)/DelayedAction/空世界 SyncHash==RNG.Last | 全部通过 |

### Phase 3 验证锚点(2026-10-03)

1. **EchoConnection 单机 10⁶ tick**(OrderManager+World 全链,TickImmediate→TryTick→world.Tick 主循环节律):
   ASan+UBSan 与 Release 双构建通过,无泄漏无 desync(NetFrame 333335 与
   NetFrameInterval=3 节律推导一致)。
2. **字节级对拍面**:Order 序列化构造期望字节逐位核对(flags 位值/7-bit
   前缀/小端/-1 短路),serialize→deserialize→serialize 往返逐字节恒等。
3. **异常文本逐字**:No rules definition for unit X / TypeDictionary
   does not contain / has multiple traits / Attempted to process orders
   from client …(消息形态差异见偏离表)。

### 已登记偏离(PORTING_PLAN §7.5,Phase 3 新增)

| # | 位置 | 偏离 | 理由 |
|---|---|---|---|
| D25 | src/sim Initialize 初始通知 | C# HashSet<delegate> 去重(方法+目标)不可在 std::function 上复刻 → 逐 observer 条目序通知(与 UpdateConditionState 的重复调用语义一致) | 受端幂等是 trait 契约;Phase 5 trait 对拍复核 |
| D26 | src/sim trait 创建 | traitInfo.Create(init) 工厂面经 World 注入(Phase 5 接 game::ActorInfo + gen 工厂);name 小写/查表/异常文本在工厂闭合 | 运行时 trait 类 Phase 5 起手;Actor 侧循环形态保持 |
| D27 | src/sim trait 所有权 | 计划 §4.5 World arena 未落地,trait 对象由 World 级 unique_ptr 表持有,actor Dispose 只摘字典引用 | Phase 5 arena 替换;行为面等价(GC 语义) |
| D28 | src/net Target.FromCell | Map.CenterOfSubCell 依赖 Phase 5;默认 square 网格公式(x*1024+512)桩换算,可注入精确实现 | Order 反序列化 TargetIsCell 分支占位;Phase 5 接 Map |
| D29 | src/net UnitOrders | 仅 default→ResolveOrder + PauseGame 落地;其余 15 命令(Message/Chat/StartGame/SyncLobby*/Handshake…)为 UI/大厅面,静默吞并 | Phase 6/7 接线;上游多数分支本就是 UI 通知 |
| D30 | src/net OrderManager | SyncReport(dump/Update)与 TextNotificationsManager 通知未接;generateSyncReport 恒 false(上游默认) | Phase 6/7;默认路径行为一致 |
| D31 | src/sim LocalRandom | C# 无参构造用 Environment.TickCount;C++ 显式种子 0(UI 域 RNG,不进同步面) | 同步路径禁隐式播种(Phase 0 既定) |
| D32 | src/sim Player | 最小面(见覆盖表);UnlockedRenderPlayer 恒走 WinState 分支(IUnlocksRenderPlayer trait 族 Phase 5) | SyncHash 消费字段集完整;其余 Phase 5 |
| D33 | gen/interfaces_gen.h | 编译器合成类型(<>f__AnonymousType 等)收编为消毒枚举名;枚举名仅锚定注释用(查询走 FindTypeIdByFullName) | 键空间完整性优先;名字无语义载荷 |
| D34 | src/net Order 异常 | Deserialize 的 catch(std::exception) 面不含 TextNotificationsManager 提示;unknown order 的 Log 走 stderr | 日志通道 Phase 5;返回 null 语义一致 |

## src/platform/ + src/gfx/ — Phase 4 第一批:SDL2 窗口 + GL 加载 + 渲染命令缓冲(2026-10-04)

| C++ 文件 | 上游锚点 | 状态 |
|---|---|---|
| src/platform/gl_types.hpp | OpenGL.cs L12-238(常量/类型,103 个常量全量) | 已对拍(值逐一对照) |
| src/platform/gl_loader.hpp/.cpp | OpenGL.cs L240-669(78 入口表驱动加载) | 已落地(直连 + KHR_debug,OPT-A6) |
| src/platform/sdl2_window.hpp/.cpp | Sdl2PlatformWindow.cs L34-587(窗口/模式/GL 属性/事件) | 主体落地(输入映射待输入批次) |
| src/gfx/gfx_command.hpp | ThreadedGraphicsContext.cs L188-505(重设计,OPT-A5) | 已落地(SPSC + 值类型命令) |
| src/gfx/render_thread.hpp/.cpp | ThreadedGraphicsContext.cs L188-820(统一线程模型) | 已落地(状态 diff,OPT-A6) |

### 验收(2026-10-04)

- platform_test:SPSC 单线程 200 条(pad/环绕)+ 双线程 200,000 条序号完整性;桌面 GL 集成 —— NPOT FBO(333×257,OPT-B1)FRAMEBUFFER_COMPLETE 直过、清屏与着色器三角形 glReadPixels 像素断言、KHR_debug 回调工作;无桌面环境自动 SKIP(ORA_SKIP_GL 亦可显式跳过);
- 双构建(ASan+UBSan / Release)ctest 13/13;import std 门禁(105 文件,白名单 + SDL2/SDL.h)与 UPSTREAM 标注门禁(95 条)PASS。

### 已登记偏离(PORTING_PLAN §7.5,Phase 4 新增)

| # | 位置 | 偏离内容 | 理由 |
|---|---|---|---|
| D35 | src/gfx 命令封送 | ThreadedGraphicsContext 的 Post(Action<object>,object) 装箱消息队列 → 值类型命令 + 内联载荷 + SPSC 无锁字节环(OPT-A5) | 行为等价(命令序保持);消除每帧装箱/锁/Pulse;上游 1.1/1.2/1.4 全部问题点(见 UPSTREAM_CPP_REVIEW 主题一) |
| D36 | src/gfx 线程模型 | 渲染线程永远存在并独占 GL 上下文;上游 Windows 窗口模式禁用渲染线程的特例(Sdl2PlatformWindow.cs L345-353)不复刻 | 统一模型;渲染结果走离屏 FBO 对拍(Phase 4 渲染关卡),不依赖上游线程拓扑 |
| D37 | src/platform 纹理/FBO | 不强制 2 的幂纹理/帧缓冲(OPT-B1);FBO 精确匹配视口 | GL3.2 core 完整支持 NPOT;上游 Texture.cs L84-85 的 GLES2 时代约束不移植;渲染对拍关卡验证 |
| D38 | src/platform/sdl2_window | 窗口几何 getter 为打包 atomic<u64> 快照(OPT-B5),非上游每 getter 一把 lock | 单写(事件线程)多读;快照语义与上游 lock 读等价(几何只在窗口事件变化) |
| D39 | src/platform/gl_loader | 无每次调用的 CheckGLError 轮询;Debug 构建注册 KHR_debug 回调,Release 零错误检查 | 上游 OpenGL.cs L740-767 的轮询在 KHR_debug 时代冗余;错误发现面不减(回调含全部 HIGH/通知) |
| D40 | src/platform/sdl2_window | GL 上下文由渲染线程创建/持有(D36 的组成部分);窗口事件泵在主线程 | 上游上下文在主线程创建后移交;SDL2 允许创建线程即持有,省一次 MakeCurrent 迁移 |
