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

### Phase 4 第二批(2026-10-05):输入层 + Shader/Texture 封装

**移植面**:Keycode 枚举(238 条逐值照搬,SDL 头对照断言);Sdl2Input 事件泵(修饰符/按钮换算、motion 合并、X1X2 伪键盘、滚轮、文本输入、退出上报)+ MultiTapDetection/TapHistory(三槽 250ms/位移 4);Sdl2Window 增焦点/挂起原子状态(HandleWindowEvent 共用);gfx_command 增 13 命令(uniform 直连/PixelStorei/TexParameteri/CopyTexImage2D/GetTexImage/active-uniform 往返/Delete* 等;TexImage2D 带 fmt/type 字段);Shader 封装({VERSION} 替换、属性循环、fragColor、链接后 active-uniform 枚举 + sampler 单元分配、Bind/PrepareRender/Set* 族);Texture 封装(BGRA 上传/UNPACK 行打包/RGBA16F/读回/ScaleFilter,RAII)。

**验收(2026-10-05)**

- platform_test 纯逻辑:TapHistory 时序/距离边界(ISqrt 语义)、(键,修饰符) 独立缓存、MakeButton/MakeModifiers 位组合、ScaleAwayFromZero 截断边界(含 ±0.5 前置的向零截断用例)、Keycode × SDL 头 34 项对照;
- 合成事件泵(SDL_PushEvent):双击 MultiTap 1→2、三条 motion 合并为一(位置 = 末事件、delta = 末相对量)、滚轮 Delta=(0,1)、文本输入、SDL_QUIT 上报、ModifierKeys 泵前一次;
- GL 封装集成:NPOT(5×3)BGRA 全量上传 glGetTexImage 逐字节往返、SetSubData 行距位图子窗(其余像素不变量)、过滤切换幂等、{VERSION} 占位编译、active uniform 枚举(Location 三态断言)、sampler 采样链(白纹理 × 红 uColor → 像素级红/绿断言)、RAII 析构后管线照常;
- 双构建 ctest 13/13;门禁 102 标注 / 112 文件 PASS。

**实现过程修出三个真 bug(全部由测试暴露)**:SPSC 记录 4B header 致 GfxCmd 落点失配(Ubsan 实证 UB,Release 向量化后段错误 —— header 扩 8B 记录恒 8 对齐);CreateGlContext 漏存成员致 GL 上下文泄漏悬挂(驱动崩于后续上下文创建);Shader::SetVecAt 的 Uniform4fv payload 只申请 4B(渲染线程越界读)。

### 已登记偏离(PORTING_PLAN §7.5,第二批新增)

| # | 位置 | 偏离内容 | 理由 |
|---|---|---|---|
| D41 | src/platform/sdl2_input | PumpInput 内 Game.Exit() 改为返回 b_quit_requested(退出编排归 Phase 5 调用方);TapHistory/MultiTapDetection 的 DateTime.Now 改 steady_clock 毫秒注入;键盘缓存键 (Keycode, Modifiers) 元组打包为 u64;X1X2 伪键的 IsRepeat 恒 false(上游读 e.key.repeat 于鼠标事件 = which 第二字节的 union 覆盖,实际恒 0) | 形态适配不可观测;steady 单调优于上游 DateTime.Now 的可回退;IsRepeat 对伪键无可观察消费方(热键系统不读它) |
| D42 | src/gfx/shader | Shader::SetTexture 的纹理生命周期契约:Texture 须存活至被替换或 Shader 析构(引擎内 Sheet 纹理由 SpriteCache 持有,天然满足) | 替代上游 PrepareRender 每帧 glIsTexture 逐纹理驱逐(Shader.cs L159-165,O(绑定数) 的 GL 查询/flush);命令队列模式下该驱逐需同步往返,代价不可接受 |
| D43 | src/gfx/shader | 链接成功后即刻 DeleteShader 两个编译对象(上游保留) | 已链接 program 内嵌产物,GL 规范允许;省显存与驱动对象数,无行为差异 |
| D44 | src/gfx/texture | Texture RAII:析构异步发 DeleteTextures(命令序保证晚于既有引用);SetData/SetEmpty/SetFloatData 无 pow2 校验(D37 的组成部分,上表已登记此处为封装层落点) | 上游手动 Dispose + 泄漏容忍;OPT-B1 的引擎级 NPOT 决策 |
| D45 | src/gfx/shader | Shader::Bind() 重播属性指针时同时 EnableVertexAttribArray(上游构造期一次 enable,全局单 VAO 模型) | C++ 侧 VAO 化后 enable 状态属各 VAO;Bind 语义 = "该 VAO 上属性状态完备",与上游可观察行为等价(VAO 缓存随 SpriteRenderer 批次,tracker OPT-A6 注) |

### Phase 4 第三批(2026-10-05):Sheet/SheetBuilder/Sprite + Palette 家族 + HardwarePalette(OPT-A7)

- `src/gfx/sprite.hpp`:TextureChannel/BlendMode/SpriteFrameType/SheetType 四枚举 + Sprite(bounds/预计算 1/128 inset 归一化坐标)+ SpriteWithSecondaryData(无 inset 二级坐标;b_secondary 判别位 = 上游 `is` 测试的等价物);
- `src/gfx/vertex.hpp`:Vertex 48B 逐字段 + combined 属性表 constexpr(static_assert 布局契约);
- `src/gfx/gfx_util.hpp/.cpp`:CreateQuadIndices、FastCreateQuad 两形态(aVertexAttributes 位域打包逐位 = combined.vert 注释契约)、FastCopyIntoChannel(单通道 ChannelMasks "nuts" 序 + Bgra32 逐行 memcpy 快路径 + Bgr/Rgb 慢路径 + PremultiplyAlpha uint32 快速整数预乘)、RotateQuadInto(Matrix3x2.CreateRotation(-r) 的 2D 变换)、BoundingRectangle((int) 向零截断)、NextPowerOf2;core::Vector2/3 渲染运算扩展(vector_n.hpp 的 Phase 4 注记落地);
- `src/gfx/sheet.hpp/.cpp`:Sheet(CPU 缓冲 + 惰性纹理、dirty 全量/子区域自动切换、ReleaseBuffer 提交后释放、转移复用)+ SheetBuilder(shelf 打包/行高推进/margin、Indexed 四通道轮换 R→G→B→A→换 sheet、BGRA 直接换 sheet、空 sprite 不占位、FrameTypeToSheetType);
- `src/gfx/palette.hpp/.cpp`:IPalette/ImmutablePalette(768B 字节流构造的 <<2|>>6 高位复制 + remapTransparent/remapShadow)/MutablePalette(SetColor/ApplyRemap/SetFromPalette)/IPaletteRemap/PaletteReference;
- `src/gfx/hardware_palette.hpp/.cpp`:HardwarePalette(行 0 保留、索引 = 已有数+1、高度 NextPowerOf2(index+1) 扩容**保留旧行**、ReplacePalette 重建、SetColorShift 延迟上传、ApplyModifiers 调整→上传→重置);IPaletteModifier 接口。

**验收(2026-10-05,gfx_test)**

- 纯逻辑:SheetBuilder shelf 几何(换行/行高/margin/空 sprite/顶行高承接)、通道轮换 R→G→B→A→换 sheet 与 BGRA 直换、Sheet dirtyRegion 并集(含无参 Commit 仍并集不重置)、palette 字节流构造边界(255→252|3=255、64→0、65→4)与重映射、CopyToArray 字节序/目标偏移、HardwarePalette 索引/高度增长/ApplyModifiers 重置/异常路径消息逐字、PremultiplyAlpha 边界(a=255 恒等/0 归零/128 精确/1 最低位)、FastCreateQuad 位域全位断言(主/次通道、双 sampler、调色板行、inset UV、旋转四角)、FastCopyIntoChannel 全路径(Indexed8 单通道掩码/Bgra32 快慢路径/Bgr24/Rgba32)、CreateQuadIndices/NextPowerOf2/BoundingRectangle/RotateQuadInto;
- GL 集成:SheetBuilder 拼装 → GetTexture 全量上传读回(行距/序号完整)、子区域 CommitBufferedData → SetSubData 路径(脏行更新、行外不变)、缓冲转移 GL 路径(src 提交后释放、dst 接收清零缓冲并照常上传)、HardwarePalette Initialize 全量 + ReplacePalette 单行**增量** + ApplyModifiers 增量;**OPT-C5 断言:每次上传后调色板纹理读回与裸纹理全量 SetData 同 CPU 缓冲的参考读回逐字节一致**;
- 双构建 ctest 14/14;门禁 112 标注 / 123 文件 PASS。

**实现过程修出两个真 bug(全部由测试暴露)**:HardwarePalette 高度增长误用 assign 清零缓冲(上游 Array.Resize 保留旧行 —— 行内容断言实证,OPT-C5 纹理==缓冲对比测不出此类"一致地错");Texture::GetData 未绑定自身即调 glGetTexImage(读回的是活动单元上最后绑定的任意纹理 —— palette 双纹理场景读成 ColorShifts 的 RGBA16F 全零,且两帧 GetData 同读一纹理使对比恒真)。

### 已登记偏离(PORTING_PLAN §7.5,第三批新增)

| # | 位置 | 偏离内容 | 理由 |
|---|---|---|---|
| D46 | src/gfx/sheet | Game.Renderer 全局访问 → RenderThread* 注入(空 = 上游 Game.Renderer==null 的 Utility 纯数据路径,GetTexture 断言挡);ReleaseBufferAndTryTransferTo 转移"清零缓冲"改为重新分配(上游转移原数组对象复用);Sheet(Stream)(Png 解码)与 AsPng 随 formats 批次 | 去全局化必经;数组对象复用仅 GC 分配语义,内容同为全零、行为等价;Png 依赖 formats 加载链 |
| D47 | src/gfx/sheet | SheetBuilder::Add 的 ISpriteFrame/Png 重载随 formats 批次,当前为字节区间重载;构造函数增 RenderThread* 尾参(默认空) | 调用面等价;ISpriteFrame 接口随 shp/tmp 加载器落地 |
| D48 | src/gfx/palette | IEnumerable<uint> 构造 → span;流构造改为 768B 字节区间(调用方负责读流);GetColor/AsReadOnly 扩展方法 → core::Color::FromArgb / const 引用(ReadOnlyPalette 包装类不需要);InvalidOperationException → std::runtime_error(消息文本逐字) | 合法输入下行为等价;C++ const 引用天然只读 |
| D49 | src/gfx/hardware_palette | 三 Dictionary → std::map(遍历仅写互不相交行,顺序不可观测);OPT-A7 调色板 dirty 行:单行 ReplacePalette/ApplyModifiers 走逐行 SetSubData 增量(脏行过半启发式退回全量 SetData;ColorShifts 表保持全量);纯数据模式(render 空)路径决策与脏位推进照常、仅 GL 发射跳过 | 上游 L132-154 每次无条件全量上传;OPT-C5:上传字节与落点相同,渲染结果逐像素一致(gfx_test 读回断言);map 顺序不可观测论证见左 |
| D50 | src/gfx/texture | Texture::GetData() 读回前先绑定自身到单元 0(glGetTexImage 作用于活动单元当前绑定;绑定与读回同队列保序) | 上游单线程同上下文内 GetData 前调用方必有 Bind 语义;命令队列模式下不绑定会读到任意残留绑定(第三批实证);消费端绑定 diff 与 GL 状态同步,无副作用 |

### Phase 4 第四批(2026-10-05):SpriteRenderer + 单级合成 Renderer(OPT-A5 持久 VB/A6 VAO+blend diff/A7 palette epoch/B1 单级)

- `src/gfx/vertex_buffer.hpp/.cpp`:VertexBuffer(持久映射形态:glBufferStorage MAP_WRITE|PERSISTENT|COHERENT 整块映射 + 三槽轮换 + 槽级 fence,槽轮换归 VB 所有(共享 VB 的 world/UI 两渲染器自然错开);静态形态:一次 BufferData(STATIC_DRAW))+ IndexBuffer(静态);**OPT-A6 VAO 缓存**:每 (顶点格式, program) 一 VAO,VB/IB/属性指针一次固化,绘制路径只剩 BindVertexArray(消费端 diff);MakeCombinedAttributes 共用转换;
- `src/gfx/frame_buffer.hpp/.cpp`:FrameBuffer(颜色纹理 + 深度 renderbuffer 附件、Bind 的 viewport 保存/恢复与 clear、完整性校验、scissor 断言文本逐字;**NPOT 直建**);
- `src/gfx/sprite_renderer.hpp/.cpp`:SpriteRenderer 逐行(8 纹理槽映射含 SpriteWithSecondaryData 双 sheet 与满槽 flush 重试、BlendSpan 交错段、Flush 的"纹理绑定→PrepareRender→顶点写入→VAO→逐段 SetBlend+DrawElements→复位 None→槽 fence"、DrawSprite 全形态、DrawVertexBuffer、SetPalette/SetViewportParams(SetViewportParams 的 depth 注释逐句)/SetDepthPreview/EnablePixelArtScaling,uniform 位置构造期缓存)+ RgbaSpriteRenderer(四形态,Channel 校验消息逐字)+ RgbaColorRenderer(DrawLine 双色/单色、DrawConnectedLine 闭合段交点、DrawRect/FillRect 四形态、FillEllipse;落点 IRgbaQuadSink 注入);BlendSpanTracker/IRgbaQuadSink 提出为可测纯逻辑件;
- `src/gfx/renderer.hpp/.cpp`:Renderer 单级合成(SetMaximumViewportSize 去 pow2、BeginWorld 的 worldSprite 几何(ComputeWorldSpriteParams 纯函数:downscale/+1 滚动补偿/整数倍 renderScale 的 offset 就近偶舍入)、BeginUI 把 worldSprite 直接画进默认帧缓冲(像素风放大保留)+ 无 world 分支显式清屏、EndFrame 直接呈现、SetPalette 绑定缓存、scissor 栈(World 降采样换算 + HiDPI scale 换算)、深度开关);
- `src/platform/gl_types.hpp/gl_loader`:BufferStorage/MapBufferRange/FenceSync/ClientWaitSync/DeleteSync/DeleteVertexArrays/DrawElementsBaseVertex 入口与常量;
- `glsl/combined.vert|frag`:自上游原样复制(GL-3.0 同源许可);
- OPT-A7:HardwarePalette 色移 epoch(HasColorShift 可观察结果翻转时递增)+ PaletteReference (epoch,value) 缓存 —— 每精灵的字符串字典查找归零。

**验收(2026-10-05,gfx_test)**

- 纯逻辑:BlendSpanTracker 段合并/分段/Clear;ResolveTextureIndex 三态(null→0/RGBA 无色移→0/带色移→TextureIndex)与 epoch 失效链(翻转→失效→回读);ComputeWorldSpriteParams(downscale=2、s=(1025,513) 的 +1 补偿、整数倍 renderScale=2 的 (-0.4,-0.7)→(-0.5,-0.5) 就近偶舍入、非整数倍保分数);RgbaColorRenderer 几何(捕获 sink:水平线 corner/偏移/预乘、FillRect 四角、半透明预乘 a=0x80、闭合三角 3 段、单点不成线);
- GL 集成:NPOT FrameBuffer(130×70)创建/清屏读回;SpriteRenderer 端到端(调色板采样链:Indexed8 索引 200 → Palette 行 1 → 像素 = 调色板色,OPT-C5 渲染级)× **五轮 Flush 覆盖三槽回绕与 fence 等待**;BlendSpan 三段交错(None 全屏白 / Alpha 半透明蓝 tint 得 (128,128,255) / None 段首索引偏移的红)单次 Flush;双 shader 共享 VB/IB 的 per-program VAO 往返切换(红/蓝/绿三连);单级合成 Renderer 全流程(BeginWorld → world 精灵 → BeginUI 合成(2× 像素风放大)→ UI 精灵 → 默认帧缓冲读回:UI 蓝区/world 红区/黑底三断言,HiDPI scale 感知)→ EndFrame;
- 双构建(ASan+UBSan / Release)ctest 14/14;门禁 std_import(131 文件)/upstream_check(120 标注)PASS。

**实现过程修出四个真 bug(全部由测试暴露)**:GL_SYNC_GPU_COMMANDS_COMPLETE 常量误写 0x911D(实为 GL_WAIT_FAILED 的值,正确 0x9117 —— glFenceSync 恒 INVALID_ENUM,fence 机制全灭);RenderState 纹理绑定缓存仅 8 单元而 sampler 分配用到 unit 8/9(Palette/ColorShifts 从未绑上,调色板采样恒黑 —— 第一批遗留,本批采样链断言暴露);DeleteTextures/DeleteFramebuffers 后 GL 名字复用撞上消费端绑定 diff 的 (unit,id) 键(旧值恰同 → "看似未变"跳过绑定 → 上传打到已删对象,palette 纹理行内容全空 —— 第一批 diff 与第二批 RAII 删除的组合缺陷);持久 VB 写入槽 N 而索引寻址自 buffer 首(仅写槽 0 的帧正确,槽回绕帧读到旧帧顶点 —— 修为 glDrawElementsBaseVertex 携带槽基址,五轮回绕压测暴露)。

### 已登记偏离(PORTING_PLAN §7.5,第四批新增)

| # | 位置 | 偏离内容 | 理由 |
|---|---|---|---|
| D51 | src/gfx/gfx_command | GfxCmd.size_payload u16→u32(单次 Flush 顶点载荷 8192×48B≈393KB 超 u16;48B 定长重排不变);新增 14 命令(SetBlendMode/深度三件/FBO depth 四件/持久 VB 四件/ConfigureVao/DeleteVertexArrays/GetViewport/Present) | OPT-A5/A6/B1 的命令面;内部布局无上游对应物 |
| D52 | src/gfx/render_thread | SetBlendMode 消费端状态机 diff:同模式跳过,模式变化必发全序列(上游每调用全发);DeleteTextures/DeleteFramebuffers/DeleteVertexArrays 消费时清除对应绑定缓存;渲染线程启动即建常驻全局 VAO(上游 InitializeOpenGL L46-49 同语义) | GL 状态机收敛等价(diff 只省重复);绑定缓存与 GL 名字生命周期同步(名字复用防护,上游单线程直调无此问题) |
| D53 | src/gfx/vertex_buffer | OPT-A5:持久映射 VB = glBufferStorage(PERSISTENT\|WRITE\|COHERENT)三槽 + 槽级 fence + DrawElementsBaseVertex 槽基址(上游 CreateEmptyVertexBuffer 的 GL_DYNAMIC_DRAW + 每次 BufferSubData);写入 = 命令 payload → 渲染线程等槽 fence 后 memcpy 进映射区(上游:CPU 数组 → 装箱消息 → 驱动内拷贝);OPT-A6:per-(格式,program) VAO 固化 VB/IB/属性(上游每 flush 重播属性指针 + indexBuffer.Bind) | 顶点数据少一次驱动路径拷贝、免存储重分配;映射与 fence 全在渲染线程内闭环,主线程零等待;GL 3.2 core |
| D54 | src/gfx/frame_buffer | NPOT 直建(pow2 校验不复刻,D37 组成部分);Bind/Unbind 的 glFlush 不复刻(上游为多线程封送正确性而插,命令流全序等价);完整性失败 stderr+b_valid 标记(上游 throw,消息保留) | OPT-B1;异常面随渲染对拍关卡统一 |
| D55 | src/gfx/sprite_renderer | CurrentBatchRenderer 属性 → 批槽指针注入(ActivateAsCurrent 复刻"切批 flush 前批");上游 internal DrawSprite 族公开;BlendSpanTracker/IRgbaQuadSink 注入面(上游私有直连);PerfHistory.Increment 不移植;OPT-A7:PaletteReference::HasColorShift 走 (epoch,value) 缓存(HardwarePalette 仅在结果翻转时递增 epoch) | 形态适配不可观测;纯逻辑可测性;epoch 缓存与逐次字典查找语义等价(单线程渲染路径,翻转点外值恒同);诊断面 Phase 9 统计 |
| D56 | src/gfx/renderer | OPT-B1 单级合成:world FBO NPOT、screen FBO 整级删除(BeginUI 把 worldSprite 直接画进默认帧缓冲,UI 同缓冲)、BeginFrame 默认 FB Clear 删除(上游自认冗余;无 world 帧在 else 分支显式清)、EndFrame 最终拷贝删除;NDC 净映射等价论证见文件头(UI 坐标 c 经 p1 与 blit 缩放的复合 = 2c/surface−1,单级直接同式);Sheet 增非拥有外部纹理构造(worldSheet 引用 FBO 附件 = 上游共享引用);着色器源码构造注入(上游 GetShaderCode 读 EngineDir,文件系统随 Game 批);Fonts/SaveScreenshot/GetRenderBufferSnapshot 随字体与 Png 批;WorldRenderers 后处理族随后处理批;Present 走命令(上下文归渲染线程) | OPT-B1(1920×1080 由 2048² 双 FBO/3 clear/2 拷贝每帧 → 1 clear/1 blit);渲染对拍关卡的先验论证;依赖批次接线 |
| D57 | glsl/combined.vert|.frag | 自上游 @7d57605 原样复制(仅 combined 一对;其余 11 个着色器随后处理/模型批次) | GPL-3.0 同源;按需落地 |

### Phase 4 第五批(2026-10-05):Westwood 文件格式第一编(编解码器 + SHP/TMP 图像链)

- `src/formats/`:fast_byte_reader(FastByteReader.cs 逐语义;ReadWord 的 int 提升)、span_reader(UPSTREAM: NONE,Stream 的 span 形态适配:long 位置/小端读取/越界等价抛点)、lcw(Format80 五 case + ReplicatePrevious 的 dist=1 展开 + CountSame/WriteCopyBlocks/Encode)、xor_delta(Format40 六 case)、rle_zeros(Format2,零段与字面量两分支均有界)、lzo(LZOCompression.cs 的 minilzo 2.06 移植逐控制流照抄:gtFirstLiteralRun/gtMatchDone 布尔 + 四标签;MatchNext/CopyMatch;未对齐 32/16 位读写按小端位拼 = memcpy 等价)、crc32(256 表逐值 + Calculate(poly)/Update/Finish;static_assert 标准向量)、shp_td(ImageHeader 头表 + XORPrev/XORLCW 引用链 + LCW + TrimmedFrame 收边(偶数行列调整/半像素偏移防御)+ recurseDepth 无限循环防线 + offsets 重复键等价抛)、shp_d2(2/4 字节偏移两型 + PaletteTable/VariableLengthTable/默认 256 项恒等表改四项 + LCW 预解压 + RLE0)、shp_ts(奇宽高取偶 + Format3/2/1-0 扫描线 + 伪帧 do-while 判定)、tmp_td/tmp_ra(魔数 @16+@20 / @20+@26;单字节索引 255=空 tile)、tmp_ts(菱形 UnpackTileData(行宽 4 起 ±4)+ flags&1 的 bounds union/Offset 半像素 + 悬崖 extra 两层主/深回填(<32 有效)+ DepthFrame 第二帧组);
- `src/gfx/sprite_frame.hpp`(ISpriteFrame:Type/Size/FrameSize/Offset/Data/DisableExportPadding)+ SheetBuilder::Add(ISpriteFrame)(上游 L85-87 直通,Offset.AsVector3 = (x,y,0))。

**验收(2026-10-05,formats_test)**

- oracle:`tools/golden_gen -- fmt`(csproj 增上游三 DLL 的 extern alias 引用;链序 = 各 mod mod.yaml 的 SpriteFormats 事实序,R8/PngSheet 未移植剔除)输出 `tests/golden_formats.txt`(6096 行):213 个 mods .shp(186 ShpTD + 27 ShpTS)逐帧 Type/Size/FrameSize/Offset/数据 CRC32 + 前 32 字节 hex、6 个 .pal 的 ImmutablePalette(stream,[0],[]) 全 256 项、合成 tmpTD/RA/TS 夹具、4 个 LZO 向量(字面量/EOF/M2 大匹配 32×'e'/MatchNext 吞字节路径)、LCW 编码向量;oracle 双跑 diff 确认确定性(V4 向量初版偏移字节错位致越界读堆残留,已重造);
- 纯逻辑:FastByteReader(Done/ReadWord/CopyTo/Remaining)、RLE0(字面量/零段交错 + 两分支越界抛)、XOR 六 case 手工向量(含 word 字节序)、LCW 五 case + Encode→Decode 往返 + 0xFE RLE 形态抽查 + case1 越界抛、CRC32("123456789"→0xCBF43926 + 分段链式恒等)、合成 shpTD(LCW 单帧,TrimmedFrame 8×8 裁剪/Offset (0,0)/全零帧 Size(0,0) 空数据)、判定负例(避开上游截断流同抛 EndOfStream 的形态)、SheetBuilder::Add(ISpriteFrame)(bounds 8×8、Red 通道 = 像素内字节 2(kChannelMasks "nuts" 序)、全零帧空 Sprite 不占位);
- 双构建(ASan+UBSan / Release)ctest 15/15;门禁 std_import(156 文件)/upstream_check(131 标注)PASS。

**实现过程修出两个真 bug**:CRC32 查找表 idx194 一位抄错(0x757AA39C,上游 0x756AA39C)——"123456789" 标准向量仅触及 9 个表项而漏检,4608 字节真实资产 CRC 全偏后以 zlib 交叉验证定位(资产前 32 字节逐位相同排除解码嫌疑,锁定 CRC 实现);RLE0 字面量分支无边界检查(测试期望的等价抛未生效,ASan heap-buffer-overflow 实证)。

### 已登记偏离(PORTING_PLAN §7.5,第五批新增)

| # | 位置 | 偏离内容 | 理由 |
|---|---|---|---|
| D58 | src/formats/span_reader、fast_byte_reader | Stream → SpanReader(span + long 位置;区间读取零拷贝返回 subspan);越界读取/Seek 抛 std::runtime_error,为上游 EndOfStreamException/IndexOutOfRangeException 的等价抛点(消息文本非逐字,仅坏数据可达) | 形态适配;C++ 无需流抽象的分配语义;合法资产零触发 |
| D59 | src/formats/ 各 loader | 上游 FrameLoader.GetFrames 链式共享同一 Stream(IsShpTS 中途拒绝路径不复位流位置);C++ 每判定从文件头独立构造 SpanReader。ISpriteLoader 接口暂无 metadata 出参(六个已移植 loader 上游均恒 null;随 png/remastered 批补全) | 格式魔数互斥且 oracle 链序 = 上游事实链序,位移歧义文件不存在;形态收敛 |
| D60 | src/gfx/sprite_frame.hpp | ISpriteFrame::Data 的上游 null 与 byte[0] 统一为空 span(消费路径 SheetBuilder.Add 的空尺寸早退不可区分);ShpTD 帧数据以 shared_ptr<const vector> 共享(上游 TrimmedFrame 直通分支的数组引用语义),帧数组可独立于 ShpTDSprite 移动 | 数据不可变,行为等价;解耦生命周期使 TryParse 直出帧数组 |
| D61 | src/formats/ lcw/xor_delta/rle_zeros/tmp_ts | 坏数据的越界写/负索引:上游为 IndexOutOfRangeException/Array.Copy 抛/C# 数组负索引抛;C++ 以显式守卫(CheckDestBounds/字面量界检查/tmp_ts 的 .at())抛 std::runtime_error,消息文本统一非逐字;ShpTD 的 ToDictionary 重复键 ArgumentException 同为等价抛点 | 语义面等价(异常类型统一为本项目先例 D48 的延续);仅坏数据可达,合法资产零触发 |
| D62 | src/formats/ lcw/shp_td | LCWCompression.Encode 落地(运行时消费者 ShpTDSprite.Write 属 Utility 面,随 Phase 8);ShpTDSprite::Write 同批不移植 | Encode 为纯字节函数且 oracle 已对拍;Write 依赖 Png/BinaryWriter 工具面,批次对齐 |

### Phase 4 第六批(2026-10-05):Westwood 文件格式第二编(Png 自研 OPT-B3 + Tga/Dds 的 Pfim 逐语义 + ShpRemastered + EmbeddedSpritePalette)

- `src/formats/` 五件:png(Png.cs 592 行逐语义:块循环/IHDR 校验/PLTE/tRNS 部分 alpha/tEXt ASCIIZ 与 Dictionary 更新保位/IDAT 链缝合(PngIdatStream 的非 IDAT 回退语义)/zlib 解压 + 五滤波反解 + 位深 1/2/4 解包/原始像素构造的 BGR→RGB 大端交换/Save 的块序 + CRC32 链式写 + 行滤波 0)、targa(Pfim v0.11.3 targa/ 四件逐语义:18 字节头/四向原点(BottomLeft/Right、TopRight 归一自下而上,TopLeft 自上而下)/非压缩与 RLE(TopLeft-RLE 的行紧排怪癖照抄)/4 字节对齐 stride 零衬垫/色图应用含 newLen=depthBytes×DataLen 怪癖)、dds(Pfim dds/ 子集:124 字节头 + 像素格式、非压缩 8/16/24/32bpp + 位掩码 R/B 交换 + Rgba16 半字节交换 + mip 链、DXT1/3/5 块解码(RGB565 float 插值 + (byte)(x+0.5f) 截断逐字、3 位 alpha 梯度两分支)、mip 尺寸 double 截断)、shp_remastered(zip 容器:惰性前缀正则的最左匹配手写复刻、帧数 = max(帧号+1)、缺号空白帧、meta JSON 手写全串匹配、前缀不一致逐字抛)、embedded_sprite_palette(帧级/文件级协商,含帧字典 null 值条目语义注记)。
- oracle:`tools/golden_gen -- fmt` 增四段合成夹具(两语言同构造算法,stored-deflate zlib 与手工 zip 保证字节恒等):7 个 PNG(五滤波全扫、位深 1/2/4/8、PLTE+tRNS 部分、双 IDAT 切分、tEXt 重复键、未知块跳过 + Save 结构对拍)、5 个 TGA(24/32bpp × 三向 + RLE 32/24)、7 个 DDS(非压 32/24 掩码交换/三 mip/DXT1 插值+透穿/DXT3/DXT5 两梯度)、1 个 ShpRemastered 合成 zip(4 条目:meta 裁剪帧/缺号空白帧/无 meta 帧/无关条目)—— golden_formats.txt 6097 → 6183 行,逐行对拍一致;oracle 双跑确定性验证。
- 验收(formats_test):纯逻辑新增(PNG 九负例 + Save 往返/块序/CRC/滤波 0 行流断言、TGA IsTga 四负例 + R5g5b5/Rgb8/负宽、DDS 魔数/头尺寸/${ 怪癖}/DXT2/BC5(D66)/Rgb8+交换、ShpRemastered 前缀不一致/空容器/坏 meta、EmbeddedSpritePalette 四态);双构建 ctest 15/15;门禁 std_import(165 文件)/upstream_check(156 标注)PASS。

### 已登记偏离(PORTING_PLAN §7.5,第六批新增)

| # | 位置 | 偏离内容 | 理由 |
|---|---|---|---|
| D63 | src/formats/png | zlib 层 .NET ZLibStream → miniz(mz_uncompress/mz_compress2 MZ_BEST_COMPRESSION):解压输出逐字节等价,坏压缩流的异常消息/时机非逐字;IDAT 链缝合由流式改为整段物化后单次解压(输出等价);FileStream.Seek 越端合法(Position 可超 Length,后续读 0 字节)以 SpanReader 钳位复刻;行数据短读的 EndOfStream 消息为等价抛点;IHDR 负宽/溢出的 new byte[] 异常面等价抛 | 合法文件解压输出与行级抛点等价;压缩器字节依实现(D64);仅坏数据可达 |
| D64 | src/formats/png | Save() 的 IDAT deflate 字节依实现(miniz vs .NET ZLibStream 的 SmallestSize);黄金以"块类型序 CRC + IDAT 解压 CRC"(GS 行)替代字节对拍,CRC/块序/滤波全一致 | deflate 输出非格式语义;两实现解压互认(测试双向往返断言) |
| D65 | src/formats/targa、dds | Pfim v0.11.3 逐语义移植的形态适配:Allocator.Rent → vector(零初始化语义保留);FileStream 缓冲路径与 MemoryStream 快路径在合法输入下输出一致,C++ 统一走内存路径(CompressedDds 取 InMemoryDecode 控制流);截断输入的异常时机/类型差异(上游 BlockCopy ArgumentOutOfRange / 指针越界)为等价抛点 | 合法资产零触发;上游双路径本就同输出;oracle 六向量 + 合成夹具覆盖已移植路径 |
| D66 | src/formats/dds | FourCC BC4/BC4s/BC5/BC5s/ATI1/ATI2/DX10(含 BC6h/BC7)未移植,显式抛 "FourCC: X not supported."(上游 BC5/BC5s 可解为 Rgb24 且 DdsLoader 接受;BC4 系/16bpp 系 loader 层亦抛 Unhandled) | mods 零 dds 资产;已移植路径(非压 RGB/DXT1/3/5)全覆盖对拍;需求出现时按 Pfim 同法补 |
| D67 | src/formats/shp_remastered | 两条正则(FilenameRegex/MetaRegex)以等价手写最左匹配/全串匹配复刻(非正则引擎);meta 失配时上游 ParseGroup 空捕获 FormatException → 等价抛点(消息非逐字);SharpZipLib ZipFile → ora::fs::ReadOnlyZipFile(miniz;条目枚举序 = 中央目录序,同 SharpZipLib 事实序);Pfim 的 TgaSprite/TgaFrame 直接复用 targa.cpp | 合法 meta/帧名下手写解析与正则同语言集;容器读取面一致;夹具 zip 手工构造两语言字节恒等 |

### Phase 4 第七批(2026-10-05):mix 包 + Blowfish(mix/Blowfish/Xcc/D2kSoundResources)

- `src/formats/` 三件:blowfish(Blowfish.cs 全文逐语义:P/S 盒 18+4×256 逐值照搬、密钥扩展的 j 循环取字节与 a<<24|b<<16|c<<8|d 组装、16 轮 Feistel 的 x 交错标志与 Encrypt/Decrypt 镜像、RunCipher 的 SwapBytes 大端装卸与奇数尾元素落 0)、blowfish_key_provider(BlowfishKeyProvider.cs 全文逐语义:固定 base64 公钥 DER 解包、小端 uint32 大数 + 16 位 limb 乘减、InvertBigNum 倒数、GetMulWord 的 Knuth-D 商估计 —— **全部中间量按 64 位累积**(C# int*uint 提升为 long,C++ int*uint32 中途回绕不等价)、CalcKey 平方乘、ProcessPredata 的 (55/a+1) 块分组)、xcc_database(XccLocalDatabase:48 字节头 + count + NUL 串解析 + Data() 写出器(Size = 字符数+条目数+52);XccGlobalDatabase:[int32 count + (name\0 comment\0)*] 块至流尾)。
- `src/fs/` 三件:package_entry(PackageEntry + PackageHashType:Classic 的 rotate-1 累加与 CRC32 的 (len%4) 字节值填充 + 尾对齐字节复制;大写化 + 4 字节对齐;ToString 的 Names 反查表仅调试用,未移植)、mix_file(MixFile:三格式检测(C&C 首 u16≠0 / RA-TS 标志 / 加密头 = 80B 密钥块 RSA 解密 + 首块探长 + (13+n*12)/8 整块 Blowfish)+ ToDictionaryWithConflictLog 首遇胜出 + ParseIndex(local mix database.dat 内嵌库双哈希命中 + Classic/CRC32 计数择优 + unknown 计数);MixLoader:".mix" 后缀嗅探 + global mix database.dat 惰性单次加载 + ToHashSet 去重;Index 绝对偏移化)、d2k_sound_resources(D2kSoundResources.cs 全文逐语义:u32 头长界定目录区、ASCIIZ + u32 偏移(文件绝对偏移)+ u32 长度、OpenPackage 上游即 "Not implemented")。
- 接线:Manifest.PackageFormats 解析(标量 = 单元素;ra/cnc/ts = "Mix",d2k = "D2kSoundResources")+ ModData 构造器按名装载(ObjectCreator.GetLoaders 的名字分派等价,未知名逐字抛)→ FileSystem 以包格式加载器集构造(ModData.cs L63-65)。
- oracle:`tools/golden_gen -- fmt` 增五段(Q/KB/BE/M/X/G):14 个 HashFilename 双哈希向量(上游 PackageEntry 权威)、BlowfishKeyProvider 密钥派生 hex + Blowfish 加密向量(**构造侧走 Program.cs 文末内嵌的 Blowfish/BlowfishKeyProvider 逐字副本 —— 上游 DLL 内为 internal;解析侧走 upstream 别名的 MixLoader.MixFile,副本产密文、上游解密还原 = 跨语言闭环**)、三个合成 mix 夹具(cnc/ra 明文 Classic 哈希 + ts 加密 CRC32 哈希;内嵌 local mix database.dat + global 名集 + unknown.file 未解析条目;ME 行 = 名/绝对偏移/长/CRC32)、XccLocal 往返、XccGlobal 块解析 —— golden_formats.txt 6183 → **6227 行逐行对拍一致**,oracle 双跑确定性验证。
- 验收(formats_test):纯逻辑新增(Blowfish 公开标准向量 + 往返 + 奇数尾落 0、Xcc 两库往返/截断负例、HashFilename 填充边界、MixFile C&C 最小夹具(dataStart/Contents/GetStream)+ 垃圾密钥块负例 + MixLoader 大小写嗅探、D2kSoundResources 绝对偏移读取 + 重键负例 + 嗅探);双构建 ctest 15/15;门禁 std_import(177 文件)/upstream_check(168 标注)PASS。

### 已登记偏离(PORTING_PLAN §7.5,第七批新增)

| # | 位置 | 偏离内容 | 理由 |
|---|---|---|---|
| D68 | src/fs/package_entry | HashFilename 的哈希域为 ASCII 字节(UTF-8 逐字节):≥0x80 折为 '?'(等价 .NET Encoding.ASCII 的替换),长度按字节数(上游按 UTF-16 字符数);ToUpperInvariant 的非 ASCII 特例(如 ſ→S)不复刻 | 真实 mix 文件名全为 ASCII,两域等价;黄金 Q 向量 14 名全绿 |
| D69 | src/fs/mix_file | 字节全量驻留 + GetStream 整读返回(i_package 契约);Contents/Index 顺序非契约(上游 Dictionary/HashSet 序本就进程随机化,消费方不迭代序依赖);global 库 TryOpen 未命中缓存空集不再重试(上游保持 null 每 mix 重试) | 形态适配;真实挂载序下 global 库先于 mix 挂载,重试差异不可观测 |
| D70 | src/fs/mix_file | ParseIndex 的未解析哈希日志(Log.Write "debug")丢弃为 no-op;ReadBlocks 沿用上游宽松界检查(offset+count*2 > Length,实际读 8*count 字节),越界兜底 = SpanReader 等价抛点(消息非逐字);local 库扫描的 unordered_map 迭代序 vs Dictionary 序(仅当同 mix 内同时存在 classic/crc 两键的 local 库条目时选择可异 —— 实际不存在的形态) | 日志非数据面;宽松检查为上游原样;极端角落不可达 |
| D71 | src/formats/xcc_database | ReadChar 逐字符 = 逐字节(ASCII 域);截断流越界 = SpanReader 等价抛点;Data() 的 ASCII 写出与上游逐字节等价(MakeData 双语对拍) | 名表数据全 ASCII;黄金 X 段对拍 |
| D72 | src/fs/d2k_sound_resources | Dictionary.Add 重键 ArgumentException → runtime_error(消息含键名,非逐字);流形式 → 全量驻留切片 | 仅坏数据可达;D48 先例延续 |
| D73 | src/game/ mod_data/manifest | ObjectCreator 反射装载 IPackageLoader → 编译期名字分派表(Mix/D2kSoundResources);未知名抛 "Unable to find a package loader for type 'X'."(逐字);PackageFormats 标量 = 单元素列表(上游 ImmutableArray<string> 字段加载语义) | 无运行时反射(项目优化主轴);mod.yaml 事实形态全为标量 |

### Phase 4 第八批(2026-10-05):aud/wav 声音格式(IMA ADPCM + MS ADPCM + Westwood WS)

- `src/formats/` 四件:ima_adpcm(ImaAdpcmReader.cs 全文:IndexAdjust 8 项/StepTable 89 项逐值、DecodeImaAdpcmSample 的双整除向零截断与 current/index 双饱和、LoadImaAdpcmSound 的 4 字节组两样点/低半字节先行)、westwood_compressed(WestwoodCompressedReader.cs 全文:五分支 = 2 位差分/4 位差分/字面/count&0x20 单样点跳变((sbyte)(count<<3)>>3 双截断符号扩展,byte 域回环**非饱和**)/填充;等长直通;跳变后 sample 每调用重置 0x80)、aud_reader(AudReader.cs + AudLoader.cs 嗅探面:12 字节头 + AudChunk 8 字节块头(0xdeaf 逐字抛);IMA 流的 index/currentSample 跨块持久与 outputSize 奇数半字节截断;WS 流的 EnsureArraySize **增长 = 全新零数组**语义与不增长时短写残留)、wav_reader(WavReader.cs + WavLoader.cs 嗅探面:RIFF 块循环(奇地址衬垫/fmt 的 Enum.IsDefined 检查与 lengthInSeconds 用整流长度的怪癖/fact/data 缺陷尺寸按余量钳制/LIST·cue·未知块跳过);三解码路径 = PCM 切片直通、WavStreamImaAdpcm(块级 predictor/index + 半字节交错、outputSize = uncompressedSize×channels×2、fact 缺失时 -1 的首样点即截怪癖)、WavStreamMsAdpcm(bpred/idelta/s1/s2 块头、高半字节恒左声道、idelta 下限 16、7 项 AdaptCoeff 越界等价抛))。
- oracle:`tools/golden_gen -- fmt` 增五段(A/B/SA/SV/IV·WV):mods 全部 46 个 .aud + 4 个 .wav 实资产(经 SoundFormats 链 Aud→Wav = 各 mod.yaml 事实序;PCM 经 CopyTo 整读,IMA/MS 流的 Length 抛 NotSupportedException 故不可用)、4 个合成 aud 夹具(ima 双块跨块状态/ima 奇 outputSize 半字节截/ws 五 case 全扫 + 等长直通/ws 三连残留观测:增长清零、不增长残留、每块 sample 重置)、9 个合成 wav 夹具(PCM 单声道/缺陷 data 尺寸吞尾随垃圾/LIST 奇尺寸衬垫 + cue/IMA 单声道 66 字节/无 fact 2 字节怪癖/IMA 立体双块交错/MS 单声道/MS 立体)、IMA 32 字节解码向量 + Westwood 四向量 —— golden_formats.txt 6227 → **6547 行逐行对拍一致**,oracle 双跑确定性验证(实现在写完输出后因 WavLoader 静态面残留前台线程不退出,文件完整后强杀,非黄金契约面)。
- 验收(formats_test):纯逻辑新增(IMA 饱和链 0/88 与 ±32768、双整除截断、4 字节组契约违约抛;WS 五 case 手工向量 + 跳变 byte 回环 0x02-16=0xF2 + 溢出等价抛;IsAud 短文件不抛/未知 format/魔数逐字抛/TryParse 吞面;IsWave 负例/压缩类型与 channels 两条 NotSupportedException 逐字/MS bpred 越界);双构建 ctest 15/15;门禁 std_import(185 文件)/upstream_check(176 标注)PASS。

**实现过程修出一个真 bug**:WavStreamImaAdpcm 的交错缓冲定长 32 字节,单声道时 toCopy = min(remaining, 32) 把上一组残留也拷入(输出 ~2×,d2k 三个 IMA wav 实资产 + 合成 mono 夹具全偏)—— 黄金首跑即暴露,修为 channels × 16(上游 interleaveBuffer.Length 语义)。

### 已登记偏离(PORTING_PLAN §7.5,第八批新增)

| # | 位置 | 偏离内容 | 理由 |
|---|---|---|---|
| D74 | src/formats/ aud_reader、wav_reader | 上游 LoadSound 返回惰性工厂(Func&lt;Stream&gt; + ReadOnlyAdapterStream 逐块 Queue&lt;byte&gt; Enqueue);C++ 物化为整段 PCM vector(Queue 追加 → vector 追加,return true → 循环退出,字节面等价 —— 黄金 A/B/SA/SV 全对拍);TryParse 内的解码失败折入 false(上游在 GetPCMInputStream 消费时才抛,仅坏数据可达);IsAud 短文件返回 false 不抛(上游 Stream.ReadByte 的 -1);IMA/MS 流的 Length 属性不存在(消费端整读) | 消费端(Sound/OpenAL 批次)本就整读;SoundFormats 链的 catch 面照抄 |
| D75 | src/formats/wav_reader | 两条 NotSupportedException 以 runtime_error 抛,消息带 "System.NotSupportedException: " 前缀逐字;fmt/fact 块的 Skip(chunkSize−n) 越端:上游 FileStream 允许 Position&gt;Length(循环自然出),C++ SpanReader 等价抛 —— 异常时机差异仅坏数据;blockAlign=0 的除零与负 blockDataSize 的 new byte[] 溢出 → runtime_error 等价抛(上游抛于工厂调用点);IsWave 短文件 false(上游 ReadASCII 抛 EndOfStream 被 TryParse 吞) | 语义面等价(D48/D58 先例);合法 wav 逐字节对拍 |
| D76 | src/formats/westwood_compressed | 输出越界写 = 上游 IndexOutOfRange 的 runtime_error 等价抛(D61 先例);输入读越界走 .at();跳变分支的 byte 回环((byte)(sample+=delta))保留(上游显式行为,非饱和) | 仅坏数据可达;回环为上游可观测语义(纯逻辑断言) |
| D77 | src/formats/ima_adpcm | LoadImaAdpcmSound 的尺寸契约违约抛 runtime_error,消息 "output must be 4 times the length of raw."(上游 ArgumentException 的 nameof 内插结果逐字);C# int 数学(乘除截断/取负)以 int32 自然等价,无额外守卫 | 契约面等价;合法输入零触发 |

### Phase 4 第九批(2026-10-05):vqa/wsa 视频格式(VqaVideo/WsaVideo + IVideo 链)

- `src/formats/` 五件:video(OpenRA.Game/Graphics/Video.cs 的 IVideo 十三成员 + VideoLoader.cs 的 IVideoLoader/GetVideo 链:属性面 → const 方法面、byte[] 返回 → span 直传、WsaVideo 的 null AudioData → 空 span)、vqa_video(VqaVideo.cs 全文逐语义:FORM/WVQA/VQHD 头与 FINF 帧偏移表(0x40000000 标志位 + <<1)、type[3]=='F' 文件级跳转、CollectAudioData 的 SND0/SND2 收集(SND2 全量走 IMA、立体声左右道独立解后 4 字节组交错、奇长度的 Position+=2 怪癖)、DecodeVQFR 子块循环(CPL0 的 <<2 调色板、CBFZ 的 Peek()==0 反向模式 + HQ 的 RGB555 拆三字节、CBF0 新读、CBP0/CBPZ 分块码本于 chunkBufferParts 集齐后"下一帧"应用 —— Clone 或 LCW 解入、VPTZ/VPRZ/VPTR 三指令路径(VPRZ 首字节 0 的反向分支不写 vtprSize)、VQFL 父块的 CBFZ 提前返回)、非 HQ 的 (mod==0x0f)?px:cbf[(mod*256+px)*8+…] 8 位展开与 HQ 的五 case 指令流 + WriteBlock 回绕)、wsa_video(WsaVideo.cs 全文逐语义:10 字节头 + frames+2 偏移表(flags==1 先 768 字节调色板且偏移 +768、调色板 <<2 后高位复制到低两位)、每帧 LCW 解中间帧 + XOR delta 作用于上一帧索引图、totalFrameWidth 行距重算;WsaLoader.cs 的 IsWsa 嗅探(frames&gt;1 + 长度核对;`width &lt;= 0` 对 ushort 恒假的上游怪癖照抄)与 VqaLoader.cs 的 IsWestwoodVqa(FORM + 非零长度 + WVQA))。
- SpanReader 增 Peek()(StreamExts.Peek 的 EOF -1 语义,偶对齐 `Peek()==0` 消费依赖)。
- oracle:`tools/golden_gen -- fmt` 增第 18 段 VQ 协议(VideoFormats 链 Vqa→Wsa = cnc/ra mod.yaml 事实序):六个合成夹具两语言同构造 —— vqa_nonhq(CBFZ/CPL0/VPTZ + SND0 + SN2J→SND2 跳径 + 尾置奇长 SND2 + \0VQF + VQFL,双填充模式)、vqa_stereo(奇长 SND2 立体声)、vqa_cbp(CBP0+CBP0 clone 应用 / CBP0+CBPZ LCW 解码应用两轮,帧间以调色板与码本分辨)、vqa_hq(0x10:CBFZ 16 位码本 + VPRZ/VPTR/反向 VPRZ 三路径,帧间以块号 seed 与第二码本分辨)、wsa_basic(手写 XOR delta 编码器 + LCW 中间帧,盘上偏移相对调色板前,30×20 双填充模式)、wsa_shp_like(frames=1 嗅探负例;上游 IsWsa 对 frames&gt;1 短文件抛 EndOfStream 故负例必须 frames≤1)—— golden_formats.txt 6547 → **6606 行逐行对拍一致**,oracle 双跑确定性验证。
- 验收(formats_test):纯逻辑新增(IsWestwoodVqa 三态、VqaVideo 构造负例消息逐字、IsWsa frames/长度负例、无调色板 WSA 的 NRE 等价抛);双构建 ctest 15/15;门禁 std_import(190 文件)/upstream_check(181 标注)PASS。

**实现过程修出一个真 bug**:WsaVideo::AdvanceFrame 漏置 has_previous 标志,第二帧对全零基线异或(黄金 wsa_basic 帧 1 首跑即暴露,CRC + 首像素双偏)。

### 已登记偏离(PORTING_PLAN §7.5,第九批新增)

| # | 位置 | 偏离内容 | 理由 |
|---|---|---|---|
| D78 | src/formats/ vqa_video、video | Stream → SpanReader(Seek 越端即抛 vs 上游 FileStream 允许 Position&gt;Length 后续读抛 —— 异常时机差异仅坏数据);cbf 字段的多重重指(ctor 数组/cbp.Clone/cbfBuffer/CBF0 新读)以 heap 存储 + span 别名复刻(CBF0 拷入 heap 同上游 ReadBytes 新数组);异常面:参数-less 的 NotSupportedException/IndexOutOfRangeException 以 .NET 全名 + 默认消息等价抛,带消息的 InvalidData/NotSupportedException 消息逐字;HQ 下 CBP0/CBPZ 触及不存在的 cbp(上游 null NRE)显式等价抛;HQ 的 CBFZ 展开固定写 ctor 存储(上游写"当前 cbf 引用"—— 仅 CBF0 重指后的坏数据可达差异);IVideo 属性面 → 方法面、byte[]/null → span(空 span 表 null) | 形态适配;合法输入逐字节对拍(vqa_nonhq/stereo/cbp/hq 全段);视频消费端(VideoPlayer,后续批次)走接口不变 |
| D79 | src/formats/wsa_video | LoadFrame 每帧的两处 new byte[](intermediate/current)以成员 vector 每帧重置零代替(上游中间帧恒新零数组,字节面等价,仅复用存储);flags≠1 无调色板时上游 paletteBytes null → 展开即 NRE,C++ 空向量等价抛(仅坏数据可达);IsWsa 的 `width &lt;= 0` 对 ushort 恒假(上游怪癖照抄,非行为差异) | 存储复用不减语义;合法 wsa 逐字节对拍(wsa_basic 双填充模式) |

### Phase 4 第十批(2026-10-05):vxl/hva/idx 体素/骨骼/声音索引格式

- `src/formats/` 三件:vxl_reader(VxlReader.cs 全文逐语义:16 字节头 "Voxel Animation" 前缀检查、LimbCount/bodySize 尺寸域 + 770 保留、每肢体 28 字节头(Name 16 + 12 保留)、足注区绝对寻址 802 + 28*LimbCount + bodySize(limbDataOffset u32 + 8 保留 + Scale f32 + 48 保留 + Bounds 6×f32 + Size 3 字节 + NormalType 1 字节)、体素数据按 limbDataOffset 回跳、每肢体两遍列扫描 —— 先逐列数出 VoxelCount 再建图;列 = baseSize 个 i32 偏移 + 等长跳过表;列内 do-while 游程(skip 字节 + count 字节 + count×(color,normal) + 重复 count),空列 = -1;byte z 的模 256 回环保留)、hva_reader(HvaReader.cs 全文逐语义:16 字节名 + FrameCount/LimbCount u32 + 16×LimbCount 肢体名跳过 + 每帧每肢体 12 个 f32 经转置表 ids 写入 16 元列主序矩阵(末行 0,0,0,1),逐矩阵 MatrixInverse 可逆性校验;含 Util.cs L131-255 MatrixInverse 子集逐字照抄 —— 伴随矩阵法 + det 除法,float 运算序不变,Util 余部随 ModelRenderer 批次移植)、idx_reader(IdxReader.cs + IdxEntry.cs 全文逐语义:GABA 魔数 + u32==2 校验 + SoundCount 条目;IdxEntry 的 NUL 截断怪癖两分支照抄 —— pos==0 不截断(16 个 NUL 原样入名 + ".wav")、pos==-1(无 NUL 满名)上游 name[..-1] 抛 ArgumentOutOfRangeException(.NET 10 双行默认消息,已探针锚定);ToString 的 0x{x8} 小写十六进制逐字)。
- SpanReader 增 ReadSingle()(StreamExts.ReadSingle 的 BitConverter.ToSingle 小端位模式)。
- oracle:`tools/golden_gen -- fmt` 增第 19 段 X/H/I 协议:vxl_basic(两肢体 TS/RA2 双型:空列 -1、count 0 游程的空 Dictionary 列、首游程 skip>0、整空行、恰 16 字符满肢体名、足注前 'G' 间隙验证 bodySize 寻址;XCM/XM 逐体素含 z/color/normal)、vxl_badhead(恰 16 字节使前缀检查本身失败 —— 15 字节会先触发 ReadASCII(16) 的 EndOfStream,上游真实行为经探针区分)、hva_basic(2 帧 × 2 肢体的对角 + 平移矩阵,逐浮点 .NET 格式化)、hva_singular(全零 12 浮点 = det 0,异常消息逐字含文件名/节号/帧号)、idx_basic(普通名/NUL 首字节名 = 16 NUL 保留/边界值条目 + ToString 行)、idx_badmagic/idx_badtwo(两头部负例消息逐字)—— golden_formats.txt 6606 → **6658 行逐行对拍一致**,oracle 双跑字节恒等验证。
- 验收(formats_test):纯逻辑新增(坏头消息逐字、MatrixInverse 单位阵自逆 + 零阵 nullopt、HVA 不可逆消息逐字、IDX 双头部负例消息逐字、无 NUL 满名的双行 ArgumentOutOfRangeException 逐字、pos==0 怪癖的 Filename/ToString 逐字节);双构建 ctest 15/15;门禁 std_import(196 文件)/upstream_check(187 标注)PASS。
- 基线前进善后(2104ec8 遗留):src/ 全部 176 个文件的 UPSTREAM 标注自 @7d57605 重打为 @b6fc03f(三黄金已在新基线再生成逐字节不变、双构建对拍全绿 —— 2104ec8 已完成再验证,仅标签滞后;upstream_check 169 失配清零)。

### 已登记偏离(PORTING_PLAN §7.5,第十批新增)

| # | 位置 | 偏离内容 | 理由 |
|---|---|---|---|
| D80 | src/formats/ vxl_reader | Stream → SpanReader(越界 = EndOfStream 等价抛点);VoxelMap 的 Dictionary&lt;byte,VxlElement&gt;[,] 以行主序 vector + 逐槽 presence 标志复刻(null 槽 = colStart -1,空 Dictionary = count 0 游程列,两态可分辨);pair 序 = 上游 Dictionary 的 Add 序(无删除,插入序);EnsureCapacity 丢弃(纯容量提示);ReadASCII 的 Encoding.ASCII 解码(≥0x80 → '?')以逐字节映射复刻 | 形态适配;合成 vxl 夹具逐体素对拍(含空列/空 Dictionary 列/满名肢体) |
| D81 | src/formats/hva_reader | Stream → SpanReader;Util.MatrixInverse 的 null 返回 → std::optional;非法矩阵的 InvalidDataException 消息逐字(含文件名/节号/帧号内插);float 全等价 IEEE(伴随矩阵展开式逐项照抄,det==0 精确比较) | 子集移植(Util 余部随 ModelRenderer 批次);hva_basic 逐浮点 + hva_singular 消息逐字对拍 |
| D82 | src/formats/idx_reader | IdxEntry 无 NUL 的 16 字符名:上游 name[..-1] = Substring(0,-1) 抛 ArgumentOutOfRangeException("length ('-1') must be a non-negative value. (Parameter 'length')\nActual value was -1.",.NET 10 探针锚定)以 runtime_error 双行消息等价抛;pos==0 不截断怪癖照抄(Filename 含 16 个 NUL + ".wav");ToString 逐字(NUL 以转义入黄金) | 消息面等价;真实 idx 名 ≤15 字符 + NUL 填充,两怪癖分支经黄金/纯逻辑双锚定 |

### Phase 4 第十一批(2026-10-06):OpenAL 引擎 + FreeType 字体(OPT-B2)+ SDL2 硬件光标

- third_party 两件:OpenAL(官方 openal-soft 1.24.3 发行包的 al.h/alc.h/efx.h + Win64 soft_oal.dll 更名 OpenAL32.dll,LGPL,COPYING 随包)+ FreeType(官方 2.14.1 源码包 include/ 全树 + 上游 bin 的 freetype6.dll 副本 —— **与 C# oracle 加载同一二进制**,字形黄金逐字节恒等的前提;FTL 许可)。
- `src/platform/` 六件:al_loader(25 入口表驱动 LoadLibraryA 直连,OPT-A6 形态;OpenRA-OpenAL-CS P/Invoke 包装层消失)、sound_engine.hpp(SoundDevice.cs L20-46 的 ISoundEngine/ISoundSource/ISound/SoundDevice 接口族:byte[] → span 直传、Stream → IPcmStream 最小顺序读面 + unique_ptr 转移(= 上游 using(stream) 的消费语义)、NaN 哨兵语义保留)、openal_sound_engine(OpenAlSoundEngine.cs 全文逐语义:池 256/同源实例限 3(GroupDistance 2730 分组、5 tick 窗、衰减公式 0.66×((256−0.5·active)/256) 运算序逐字)、TryGetSourceFromPool 两遍回收、OpenAlSound 家族(PITCH/REF_DISTANCE 6826/MAX_DISTANCE 136533、done 哨兵全 no-op)、OpenAlAsyncLoadSound 的 Task.Run/Delay → jthread + condition_variable(PORTING_PLAN §4.8 既定映射;静音 buffer → 后台填充 → 换 buffer → 60Hz 轮询,锁序照抄)、DummySoundEngine 全文);纯逻辑提取三件可离线单测(MakeALFormat/ParseAlDeviceList/EvaluatePlayGate)。**设备表怪癖照抄**:QueryDevices 的 do-while continue 跳条件判定,读完名字末字节即于其 NUL 前退出 —— 上游恒返回 [](AvailableDevices 实际只剩 Default Output),C# 语义同构复刻(bug 兼容)、freetype_font(FreeTypeFont.cs 全文:FT_Init 一次/FT_New_Memory_Face/FT_Set_Pixel_Sizes/FT_Load_Char(FT_LOAD_RENDER)/26.6 右移/位图行复制循环逐语句;**OPT-B2 真结构体** —— 上游 L27-36 手算偏移 HACK 删除,直接 ft2build.h 真结构体字段)、sdl2_hardware_cursor(Sdl2HardwareCursor.cs 全文:32bpp 表面四掩码 + CreateColorCursor 3 次重试;DoublePixelData 纯函数提取)+ sdl2_window 增 CreateHardwareCursor/SetHardwareCursor(scale>1.5 与 pixelDouble 两倍增路径,hotspot 同倍)。
- **LP64 ABI 发现与手术**(本批最重要考古):随包 freetype6.dll 的实际结构布局 = long 8 字节(上游手算偏移 glyph@152/metrics@48/bitmap@152/bitmap_left@192/bitmap_top@196 即其 64 位实证;而 LLP64 本机头的 long=4 给出 glyph@120 —— 首版直接崩溃于此差)。修复:freetype_font.cpp 以 `FT_SIZEOF_LONG 8` + `#define long long long` 宏窗编官方头(系统头先经典引入占住 include guard),字长即与 DLL 一致 = 等价于用 DLL 的编译字长编这份头;六个上游偏移 + FT_Pos==8 以 static_assert 锁死 —— HACK 魔法数升格为编译期契约,布局漂移即编译失败。
- oracle:`tools/golden_gen -- fonts` 新模式(引用上游 OpenRA.Platforms.Default 的 FreeTypeFont;freetype6.dll 从上游 bin 复制到 oracle 进程目录使 DllImport 与 C++ 加载同一二进制):mods 全部 4 个 .ttf(FreeSans/FreeSansBold/Dune2k/ZoodRangmah)× 尺寸 {7,12,16} × scale {1,1.25} × 9 码点(常规/' '/~/'é'/'中'/0xD800 孤代理 —— FreeSans 无 CJK,后两者同渲染 .notdef 框,恰证失败路径归一)的 GL 行(Advance .NET float 格式化/offset/尺寸/全量位图 hex)→ `tests/golden_fonts.txt` **220 行 / 216 字形逐行对拍一致**,oracle 双跑字节恒等。
- 验收(platform_test):纯逻辑(MakeAlFormat 四组合、设备表双 NUL 解析含恒空怪癖、门控五分支含衰减公式精确相等、DoublePixelData 2×2→4×4 邻域)+ OpenAL 活测(openal-soft 随包 DLL:设备表/音量/停止即 Complete/门控活测第 4 拒(实证上游不查 Complete —— 已停声槽仍计数)/池耗尽与回收/流式 Stop join 后 Complete/监听者与音量族;DLL 或设备不可用整段 SKIP)+ Dummy 引擎空面 + FreeType(垃圾字体负例消息逐字 + 黄金对拍)+ 光标(直建/窗口集成两倍增路径);双构建 ctest 15/15;门禁 std_import(206 文件,白名单增 OpenAL/FreeType 头与宏窗前置系统头)/upstream_check(197 标注)PASS。

### 已登记偏离(PORTING_PLAN §7.5,第十一批新增)

| # | 位置 | 偏离内容 | 理由 |
|---|---|---|---|
| D83 | src/platform/ al_loader | OpenRA-OpenAL-CS 的 P/Invoke 包装层消失,25 入口(含 ALC11 扩展与 EFX 的 AL_METERS_PER_UNIT)经 LoadLibraryA("OpenAL32.dll") 表驱动直连(随包 openal-soft 1.24.3,可执行文件旁优先于系统路由);AL/ALC 常量直接取官方头(与 GL 层 gl_types 手抄对照形态不同 —— OpenAL 有官方头可用);上游 Log.Write("sound") → stderr 前缀打印 | 形态适配(OPT-A6 同族);调用序列与字面量逐字,平台测试活测全路径 |
| D84 | src/platform/ openal_sound_engine | sourcePool:Dictionary&lt;uint,PoolSlot&gt; → vector(插入序 = 无删除 Dictionary 的枚举序,alGenSources 名非单调时槽等价);Game.LocalTick 静态读 → SetLocalTick 注入面(Phase 5/6 Game 接线,默认 0 = 初值);Play2D/Play2DStream 返回 unique_ptr&lt;ISound&gt;(调用方所有;对象析构经回指清槽 = GC 生命期等价物,引擎先亡时回指置空);SetSoundVolume 的 LINQ Where → 手写循环 | 形态适配;池策略/门控以纯函数提取单测,活测覆盖耗尽/回收/流式停止 |
| D85 | src/platform/ openal_sound_engine(OpenAlAsyncLoadSound) | Task.Run/Task.Delay/CancellationToken → jthread + condition_variable + atomic cancel(PORTING_PLAN §4.8 既定映射);Play2DStream 的 Stream 参数 → unique_ptr&lt;IPcmStream&gt;(转移 = 上游 using(stream) 消费语义);stream.Length 的 NotSupportedException 回退分支 → Length() 返回 optional;lengthInSecs/delay 的浮点运算序逐字照抄 | 并发原语映射;流式 Stop join 后 Complete 断言 |
| D86 | src/platform/ freetype_font | FontGlyph(byte[] Data) → vector&lt;uint8_t&gt;;字节数组 GCHandle pin → 自持 vector 拷贝(FT_New_Memory_Face 生命期语义等价);char(UTF-16 码单元)→ uint32 码点参数(FT_Load_Char 原生域);char[] 大小写 *8f/除法的浮点表达式逐字 | 形态适配;216 字形黄金逐字节对拍 |
| D87 | src/platform/ sdl2_hardware_cursor + sdl2_window | IHardwareCursor 接口面由具体类型承担(单平台);Marshal.PtrToStructure + Marshal.Copy → SDL_Surface 真结构体直写;倍增判定的 NativeWindowScale 取 OPT-B5 快照 scale(drawable/window 比;>1.5 阈值照抄);上游 try/catch(Exception)→ log+null 的容错 → optional 返回;Cursor==0 时上游泄漏 surface 委托 GC 终结器 —— C++ 侧即析构释放(更紧;该路径上游本不可用此光标) | 形态适配;DoublePixelData 纯函数 + SDL 活测双锚定 |
| D88 | third_party/ FreeType 头的编入方式 | 官方 2.14.1 头以 `FT_SIZEOF_LONG 8` + `#define long long long` 宏窗编入(系统头先经典引入占 guard):随包 freetype6.dll 为 LP64 字长构建(上游 glyph@152 等手算偏移实证),LLP64 本机头(long=4)会错读 face→glyph;宏窗后真结构体布局与 DLL 一致,上游六个偏移 + FT_Pos==8 以 static_assert 锁为编译期契约 | ABI 对齐(不改头一字节);字形黄金逐字节恒等为证 |

### Phase 4 第十二批(2026-10-06):WorldRenderer/渲染收集(OPT-A7 SOA + 帧 arena)+ TerrainSpriteLayer

- `src/gfx/` 三件新 + vertex_buffer 增量:renderable(Renderable.cs 接口族 + SpriteRenderable/UISpriteRenderable/TargetLineRenderable/MarkerTileRenderable 四引擎件;**OPT-A7 RenderItem POD** —— 四引擎 renderable 并入带 kind 判别的 POD,With* 按值拷贝 = 上游每次 new 的零分配等价,PrepareRender 对引擎 kind 恒等(上游四类全部 `return this`);IRenderable/IPalettedRenderable/IModifyableRenderable/IFinalizedRenderable 虚接口保留为 mod 侧自定义 renderable 的承载(Phase 5 trait 面);排序 = (int64(zkey)<<32)|i 全序唯一键 + std::sort(与上游 keys.Sort 内嵌收集序的稳定结果逐项一致;负键走带符号序 = 上游 (long)key 装载前 int 加法);overlay 的 GroupBy(GetType()) HACK → kind 计数分段(首遇序 + 段内收集序 = LINQ 两阶稳定序,零分配)、world_renderer(WorldRenderer.cs 逐方法:坐标换算族 L399-471 逐行含 Math.Round 默认档 = 就近偶舍入(std::nearbyint);调色板面 Palette() 空名 null/GetOrAdd 缓存、AddPalette 高度增长才触发 PaletteInvalidated、ReplacePalette 缓存引用同步;收集三列表复用 + 排序/分段在 FrameArena(Mark/Rewind 帧内、DrawAnnotations 末 Reset);PrepareRenderables 排序→Prepare(Custom 虚接口)→清缓冲;Draw 全序(scissor→depth→terrain→prepared→AfterActors→aboveWorld→AfterWorld→shroud→overlay 分组→AfterShroud→Flush)与 DrawAnnotations(抗锯齿档→annotation→调试钩子→AfterAnnotations→清三列表);ITerrainLighting/IRenderPostProcessPass/IRendererTrait/IViewportSurface/TerrainMapSurface 注入面)、terrain_sprite_layer(TerrainSpriteLayer.cs 逐方法;**OPT-A7 分离数组** —— 上游 UpdatePaletteIndices 调色板行上移时逐顶点重写整个 Vertex 数组,C++ 把调色板行索引(vec_palette_ 每格 PaletteReference*)与地形光照 tint(vec_corner_tint_ 每顶点,FastCreateQuad 置 One/UpdateTint 写四角 TintAt 采样)分离,脏行上传点组合 c=(p<<16)|(v.c&0xFFFF)、RGB=v.RGB×cornerTint —— 与上游 AOS 逐顶点重写逐字节一致;调色板失效 = 全行标脏,零逐顶点工作;共享静态 IB 引用计数(每 World 一份);Update 的 samplers/palette-null/界外早退语句序照抄)、vertex_buffer 增 UpdateStaticSubData(上游 IVertexBuffer.SetData 的静态子区形态,GL_STATIC_DRAW 上经 BufferSubData 命令)。
- 验收(gfx_test):纯逻辑(排序键回绕/同键收集序/空 TargetLine 的 First() 以 "Sequence contains no elements" 等价抛/计数分段两阶稳定序与 counts/starts;坐标族含 0.5→0、1.5→2 的就近偶舍入与 Screen3DPxPosition 只整 xy;调色板事件/缓存/ReplacePalette 同步/ApplyModifiers 重置回原色;地形层 **ComposeRow == 上游 AOS 参考逐字段对拍**(无光照与四角采样两态)、ignoreTint 复位 One、Clear、blend 不匹配与第 9 张 sheet 的 "Sheet overflow" 负例、界外早退不标脏、调色板失效全行标脏)+ GL 集成(地形绿 tile + 红 SpriteRenderable 经 WorldRenderer 的收集/排序/绘制全链,重叠区 prepared 压 terrain,默认帧缓冲三区像素断言;Draw 消费脏行、DrawAnnotations 清列);双构建 ctest 15/15;门禁 std_import(212 文件)/upstream_check(203 标注)PASS。
- 实现过程修出一个真 bug:脏行循环含端(上游 lastRow = (V+1).Clamp(…,H) 可达 H),行标志数组按行号直索引越界一行 —— 上游 HashSet.Remove 对未登记行是 no-op,以界检查等价复刻(ASan 实证定位)。

### 已登记偏离(PORTING_PLAN §7.5,第十二批新增)

| # | 位置 | 偏离内容 | 理由 |
|---|---|---|---|
| D89 | src/gfx/ renderable | 引擎四 renderable = RenderItem POD(带 kind 判别),With* 按值拷贝替代上游 new 不可变对象(可观测行为一致:字段同值);IRenderable 虚接口仅承载 mod 侧自定义件(Phase 5 trait 面,经 Custom 槽非拥有指针入流,PrepareRender 后走 IFinalizedRenderable);TargetLine 的 IEnumerable&lt;WPos&gt;/惰性 Select → span(调用方帧内保证生存期;OffsetBy 物化进调用方存储);UI/TargetLine/MarkerTile 的 With* 返回 this(引用相等)→ 按值拷贝;Game.Renderer 静态 → WorldRenderer 注入的 Renderer*(上游进程单例,等价);空 TargetLine 的 Pos=First() 抛 InvalidOperationException("Sequence contains no elements")以 runtime_error 等价 | 形态适配(OPT-A7 落地件);排序/分段以纯函数提取,AOS 参考逐字节锚定 |
| D90 | src/gfx/ world_renderer | TileSize/TileScale/enableDepthBuffer/Renderer 经 Desc 注入(上游 map.Rules.TerrainInfo/map.Grid/modData.GetOrCreate&lt;MapGrid&gt;/Game.Renderer 反射链,Phase 5 随 Game/Map 接线);Viewport → IViewportSurface 最小面(WorldToViewPx/GetScissorBounds/TopLeft/BottomRight;完整 Viewport 随 Phase 5 Map —— Viewport.cs 依赖 ProjectedCellRegion/Ramp/CellContaining 全家);渲染源(onScreenActors/WorldActor/RenderPlayer/OrderGenerator/effects/ScreenMap)与 ILoadsPalettes/ILoadsPlayerPalettes/IRenderer/IRenderTerrain/IRenderAboveWorld/IRenderShroud/DebugVisualizations → Hooks/回调/接口表注入(默认 no-op);IPaletteModifier 收集 → RefreshPalette 的 span 参数;event → token 订阅表(成对注销);Dispose 兼带 World.Dispose() 的所有权 HACK 不移植(World 生存期归 sim 调用方);WorldActor 未注入时 Draw/Prepare 视作未就绪跳过(= 上游 Disposed 早退的等价域) | 注入面形态(与 sim/net 批次同族);坐标/调色板/收集/绘制序机制面完整,纯逻辑 + GL 集成双锚定 |
| D91 | src/gfx/ terrain_sprite_layer | map.MapSize/CenterOfCell/Tiles.Contains/Grid.Ramps/Ramp/TileScale 经 TerrainMapSurface 注入(默认 = 方格桩:cell*1024+512 中心、矩形包含、平地 Ramp ±512 角 —— 与 World::TargetFromCell 默认桩同族;完整 Map 随 Phase 5);Draw 的可见行范围参数化(上游 viewport.VisibleCellsInsideBounds/AllVisibleCells 的 CandidateMapCoords;restrictToBounds 归调用方决定,值存证;Clamp 与"行循环含端、绘制长度不含端"两上游形态照抄);Update(CPos, ISpriteSequence, …) 重载随序列系统(Phase 5/6);RenderThread* 空 = 纯数据模式(VB/IB 不建、上传/绘制跳过,脏行跟踪与组合照常 —— Sheet/HardwarePalette 同族);dirtyRows HashSet → 行标志数组(端行可达 H 的 Remove no-op 以界检查等价);ConditionalWeakTable&lt;World,IndexBufferRc&gt;+Lock → 静态 map + mutex + 壳内存归属键;**OPT-A7 分离数组**:palette 行(vec_palette_ 每格)与地形 tint(vec_corner_tint_ 每顶点)独立,上传点组合 —— 上传字节与上游 AOS 逐顶点重写逐字节一致(c 组合式 = UpdatePaletteIndices L82 的现值等价重写;RGB=v.RGB×cornerTint 与 UpdateTint L145 的 v.A×weights 在 FastCreateQuad 已写 RGB=alpha 的域上重合) | OPT-A7 待办项落地 + 注入面;ComposeRow 与 AOS 参考逐字段对拍(无光照/四角采样/ignoreTint 三态) |
| D92 | src/gfx/ renderable + world_renderer(收集流) | 上游 keys 数组 + span.Sort 双联动 → FrameArena 上的 (key<<32\|i) 键数组 + std::sort + 排序副本散布(键全序唯一,序恒同);GroupBy(GetType()) → 计数分段(两遍:计数+首遇序,稳定散布);三 List&lt;IFinalizedRenderable&gt; → 三 vector&lt;RenderItem&gt; 复用(帧内零分配;PrepareRender 引擎 kind 恒等在收集层短路);renderablesBuffer 复用语义照抄(帧末清) | OPT-A7 收集流落地;排序/分段纯函数单测(含负键/同键/空输入),GL 集成全链像素断言 |

### Phase 4 第十三批(2026-10-06):SpriteCache/SequenceSet/Animation + CursorManager/ChromeProvider + 声音解码收尾(voc/ogg/mp3)

- `src/gfx/` 七件新 + 若干增量:sprite_loader(ISpriteLoader 接口 → 函数指针链 + MakeSpriteLoaders 名字分派表(与 MakePackageLoaders 同形);FrameLoader/FrameCache 的 memo 语义,合法无命中 → "X is not a valid sprite file!" 逐字)、sprite_cache(SpriteCache.cs 全文:ReserveSprites 的 token 记账、LoadReservations 的越界帧检查(消息含 SourceLocation 的 "Name:Line")、**按帧高稳定排序的 sheet 打包优化**、(文件名,帧索引,预乘,AdjustFrame) 四元组去重、ResolveSprites 的二次取/未记/缺文件三抛点文本逐字)、sequence_set(ISpriteSequence/ISpriteSequenceLoader 接口 + 容器:GetSequence 两级缺失文本逐字、^ 前缀跳过、LoadSprites = 物化 + 逐序列 ResolveSprites;mods 侧解析器留注入面)、animation(Animation.cs 全文状态机:Play 五族 + Tick(t) 的 timeUntilNextFrame 债务循环与 tickAlways 旁路、backwards 反转索引、Render 的 shadow 双件套(高度下投 + ShadowZOffset)/RenderUI 的 (int) 截断算式/ScreenBounds;AnimationWithOffset 三回调)、cursor_sequence(X/Y 的 TryParse 静默怪癖 + Length 三态含 End="*" 死三元)、cursor_manager(CursorManager.cs 逐方法:Start/Length 上界检查 YamlException 逐字、热点居中换算、Indexed8 解色、8 倍数 PaddedSize、硬件光标重建/CursorDouble 变更、3 tick 换帧、Lock/Unlock 相对鼠标模式)、chrome_provider(ChromeProvider.cs 全文:HiDPI 三档选图、sheet 按图名共享、density × 矩形 + 1/density scale、PanelRegion 九宫格(PanelSides 缺失侧 null 槽)/具名九查询/空表三态、GetMinimumPanelSize、SetDPIScale 的缓存清空/sheet 保留取舍)。增量:Sheet 增 Png 字节构造(L57-66)、gfx_util 增 FastCopyIntoSprite(L203-241 预乘展开)、sdl2_window 增 SetRelativeMouseMode + NativeWindowScale、palette 增 PlayerColorRemap(头内联)、core::Color 增 HSV/线性 gamma 转换族(L84-160)、meta 补 Size/Rectangle 解析器(上游 TypeParsers 本有,Phase 2 裁剪遗漏)与字典 Rectangle/int2 值形态。
- `src/formats/` 五件:voc_loader(VocLoader.cs 全文:26 字节头四重校验(消息逐字含十六进制)、块序列、code-8 对后续块的采样率覆盖 + 按值 Remove、静默/循环块入表不产样、混合采样率抛)、r8_loader(R8Loader.cs 全文:RGB555 帧/调色板展开、type==2 借调色板、RemappableFrame 的 shroud→fog 位运算半调/索引 1 影色/PlayerColorRemap 玩家色段)、png_sheet_loader(PngSheetLoader.cs 全文:Frame[i] 手工区优先、FrameSize/FrameAmount/Offset 切片、行距拷贝)、ogg_loader(OggLoader.cs 形态;后端 stb_vorbis)、mp3_loader(Mp3Loader.cs 形态含 IsMp3 双嗅探;后端 minimp3)。third_party 增 StbVorbis(stb_vorbis.c 逐字节,MIT/公有领域双许可)与 Minimp3(minimp3.h 逐字节,CC0)。
- oracle:golden_gen 增第 20 段(VO voc 四夹具 + R 段 r8 两夹具/pngsheet 三夹具,两语言同构造;csproj 增 OpenRA.Mods.D2k 引用),黄金 6658 → **6693 行逐行对拍一致**。
- 验收:formats_test 纯逻辑增 39 断言(voc 全路径/四头负例/混合率/未用块 8、IsMp3 三态与 ogg/mp3 垃圾负例、IsR8 三态、r8 双帧含影色/展开/借调色板、pngsheet 整图/切片/手工区/签名负例);gfx_test 增六测(CursorSequence 怪癖矩阵、Animation 状态机全族(Repeating 回绕/Then 钳制与 after/Backwards 反转/FetchIndex 旁路/FetchDirection 双向/paused/ReplaceAnim/GetRandomExistingSequence 的 MT 决定性/Render shadow 双件套/AnimationWithOffset)、SpriteCache 预留流水(缺文件文本/二次取/越界消息/去重共享槽位/AdjustFrame)、SequenceSet(^ 跳过/错误文本/LoadSprites)、ChromeProvider(Regions 查询/九宫格 PanelSides 子集/SetDPIScale 换 2x 图与缓存)、FastCopyIntoSprite 预乘 + Sheet(png) 构造);双构建 ctest 15/15。
- 实现过程修出一个真 bug:core/rectangle.hpp 的 Contains(Rectangle) 误写为 `this == Intersect(this, rect)`(上游为 `rect == Intersect(this, rect)`)—— 上半平面矩形恒判不含,PngSheet 的 Frame[i] 手工区路径触发定位。

### 已登记偏离(PORTING_PLAN §7.5,第十三批新增)

| # | 位置 | 偏离内容 | 理由 |
|---|---|---|---|
| D93 | src/formats/ voc_loader | Stream → SpanReader;VocStream 惰性流 → 整段 PCM 物化(D74 同族);块尾净结束(上游 EndOfStreamException break)= Position 检查;List.Remove(结构体) 的按值首例删除以 find+erase 复刻;头十六进制消息以 std::format("{:X}") 对齐 C# ToStringInvariant("X") | 形态适配;黄金 VO 段四夹具逐行对拍 |
| D94 | src/formats/ ogg_loader | 解码后端 NVorbis(C#)→ stb_vorbis(third_party/StbVorbis,逐字节未改):Ogg/Vorbis 样点值非位精确规范,PCM 字节不保证与上游一致(声道/位深/采样率/总样点数元数据等价;ClipSamples 的 [-1,1] 夹取与 (short)(x*32767) 截断换算逐句);上游 mods 无 ogg 资产,黄金不含 ogg 段 | 解码后端替换(单文件公有领域库);元数据面 + 嗅探/负例纯逻辑锚定 |
| D95 | src/formats/ mp3_loader | 解码后端 MP3Sharp(JavaLayer 系)→ minimp3(third_party/Minimp3,逐字节未改):PCM 字节不保证与上游一致;LengthInSeconds 上游经 TagLib 精确解析(首猜 = 文件大小×8/(2·ch·rate)),C++ 以解码样点数/rate 计(更接近 TagLib 精确值;仅 UI 时长展示可观察);IsMp3 的越界读异常逃出 catch 面 = 长度前置检查归 false | 解码后端替换;IsMp3 三态 + 垃圾负例锚定 |
| D96 | src/gfx/ sprite_loader + sprite_cache + r8/png_sheet | ISpriteLoader[] → 函数指针链;ObjectCreator.GetLoaders → MakeSpriteLoaders 名字分派表(未知名消息逐字;D73 族);FrameLoader 的 TypeDictionary metadata 出参未接(引擎消费面仅透传不读;PngSheet 的 PngSheetMetadata/EmbeddedSpritePalette 随 mods 序列批);**帧与文件字节共持有**(ShpTD TrimmedFrame 等借底层缓冲,上游靠 GC —— FramesSource 同持字节与帧,调用方保证同寿命);SpriteCache 的 Dictionary → 保序 vector(等高稳定排序的平局序一致)、TrimExcess 无对应、LoadScreen 进度 no-op(Phase 6)、FileNotFoundException/InvalidOperationException 以 runtime_error 文本等价;R8 的 uint[] Palette → optional、RemappableFrame 惰性 Data 缓存、IsR8 越界读 = 长度前置检查;PngSheet 的 coords[1] 缺分号 = IndexOutOfRangeException 等价抛、Size.ToString("W,H") 入消息 | 形态适配;黄金 R 段五夹具逐行对拍 + 预留流水单测 |
| D97 | src/gfx/ sequence_set + animation + cursor_sequence + cursor_manager + chrome_provider | 构造依赖注入族:SequenceSet(上游自 ModData 取 Manifest.Sequences/SpriteLoaders/rc 两尺寸)→ Deps;ISpriteSequenceLoader 为 mods 解析器注入面(SpriteSequenceFormat 批,Phase 5);PerfTimer 计时面略(名字注释);images 嵌套字典 → 两级保序 vector;Animation 的 World 依赖 → Deps(sequences + 高度查询;Map 随 Phase 5,默认平地 0);tickFunc 闭包族 → TickMode 枚举 + after/fetch/direction 回调;IRenderable[] 返回 → out 数组(≤2);RandomOrDefault 的 r.Next 负值(上游 ElementAt 抛 ArgumentOutOfRangeException)以 .NET 10 文本等价抛、空集 → 空串(上游 null);CursorSequence 的 KeyNotFoundException/FormatException 以 .NET 10 文本等价抛;CursorManager 的 GraphicSettings → live 查询注入、Game.Renderer/Window → 指针注入(空 = Utility 面不建硬件光标;Update 无窗口静默跳过)、Viewport.LastMousePos → 查询注入、IHardwareCursor[] → unique_ptr 槽、诊断双打印(stdout+stderr);ChromeProvider 静态类 → 实例类(Phase 5 Ui.Initialize 持单例,形态等价)、FrozenDictionary → 保序 vector、FieldLoader.Load 经 meta 描述表(ORA_FIELD/手写条目 + PanelSides 枚举注册)、三条 Log.Write("debug") → stderr | 注入面形态(与 D90/D91 同族);怪癖矩阵 + 九宫格/HiDPI/缓存行为单测锚定 |
| D98 | src/gfx/ viewport + projected_cell_region + gfx_util 增量 | Viewport 的 Game.Renderer → IViewportHostRenderer 注入、Game.Settings.Graphics → GraphicSettingsFace 值、WorldViewportSizes(IGlobalModData)→ 值结构(Phase 5 规则链加载)、Map 依赖(ProjectedTopLeft/BottomRight 角、MapSize、Clamp(PPos)/Height.Clamp/CellContaining)→ Deps 函数注入(默认 = 方格/界内恒真,与 TerrainMapSurface 同族);CandidateMouseoverCells 的 yield 惰性 → vector 物化(双重递减序照抄);ViewportTick event → token 订阅表;CalculateMinimumZoom 的 Game.Renderer 静态读 → 宿主分辨率参数;Center(IEnumerable&lt;Actor&gt;) 的 Average → 调用方注入(Actor 面随 Phase 5);ProjectedCellRegion 的 Map 构造依赖 → Make(heightOffset/heightClamp)注入、IEnumerable → 前向迭代器;Rectangle::Clamp(Vector2)/IndexFacing/AngleDiffToStep/GetInterpolatedFacingRotation/Lerp/PolygonContains 随用例落地(Rectangle.cs L92-95 / Mods.Common Util.cs L79-102 / Util.cs L368 / Exts.cs L77-99) | 注入面形态(与 D90/D91 同族);缩放矩阵/滚动夹取/坐标往返/格区缓存单测锚定 |
| D99 | src/mods/ default/tileset_specific/classic/classic_tileset_specific/d2k_sprite_sequence + sequence_loader_factory | ReserveSprites/ParseFilenames 的 (modData, tileset) 形参 → tileset 成员化(每轮 ReserveSprites 更新;modData 上游未用);resolved[f] 越界 = 上游数组 IndexOutOfRangeException → 同文本 runtime_error;ParseSequences 包装异常的内层 {e} = C# ToString(),C++ 取 e.what()(信息等价,少类型前缀 —— 黄金段 21 以归一化协议锚定:头|内层,oracle 剥 "Type: " 前缀与栈帧);PerfTimer 计时面略;FormatInvariant("{0}") → std::vformat;D2k 的 RemapFrame 闭包捕获 → this 捕获 + deque 包装帧存活区(LoadReservations 结束前 = D96 调用方保证);GetLoader&lt;ISpriteSequenceLoader&gt;(SpriteSequenceFormat) → MakeSequenceLoader 五名分派(未知名消息逐字;D73/D96 族);Classic 变体重复 TilesetSpecific 解析体 = 上游同构重复(非继承) | 形态适配;黄金段 21(14 案例字段面 + 校验文本)逐行对拍 + seq_test 全流水单测(合成 shpTD 真实 SpriteCache) |
| D100 | src/sound/ sound + sound_loader + game_records(SoundPool/NotificationsPools/MusicInfo.Load)+ manifest(SpriteSequenceFormat) | ISoundLoader[] → 函数指针链(五格式 Info 同构转发,Wav 的 short Channels 直转);GetLoaders("sound") → MakeSoundLoaders 名字分派(未知名逐字);Game.Settings.Sound → SoundSettingsFace 引用(活读写);Game.CosmeticRandom(new MersenneTwister() = TickCount 种子)→ 成员 MersenneTwister(steady_clock 计数种子);上游 sounds[name] null 源进 Play2D(null) 的 NRE → 同文本 runtime_error 等价抛;Log.Write("sound") 缺文件日志不落地(D70 族);player.World.LocalPlayer → SetLocalPlayer 指针、World.Selection.Contains → SetSelectionContains 回调(默认恒假 = 空选区);ISound 的 GC 所有权 → vec_playing_ 所有权表(替换条目析构 = D84 析构回指同族);SoundPool.GetNext 的 CosmeticRandom → 参数注入;NotificationsPools 的 Exts.Lazy → mutable 首查物化、ParseSoundPool 重读原 yaml(缺键 = NodeWithKey 等价抛)、InterruptType 枚举按需注册(C# 嵌套全名 SoundPool+InterruptType);MusicInfo.Load 的 (fileSystem, Game.ModData.SoundLoaders) → 双回调模板形态;PlayMusic 的 Play2DStream 可空返回 = 上游 music=null 分支(与"not a valid sound file!"两态分离);Play2DStream 的 Stream → IPcmStream 内存适配器 | 形态适配;sound_test(上游 mods 散装 .aud 实资产 + 通知池打断策略矩阵)锚定 |

### Phase 5 第一批(2026-10-06):Map 全量 + ScreenMap(OPT-A8)+ Selection + World 全量接线 + trait 工厂入 World arena + MapCache/StartGame

- 新库 `ora_terrain`(src/terrain/ 三件):map_grid(MapGrid.cs 全文:CellRamp 的 do-while 三角化 HeightOffset 逐控制流、21 斜坡常量表、TilesByDistance 的 ISqrt(Ceiling) 分桶 + LengthSquared/Hash/X/Y 四键排序、DefaultSubCell 归中与越界校验文本逐字;MapGridType/MapVisibility 两枚举补注册 —— IGlobalModData 域枚举不在 601 trait 字段树内)、terrain_info(TerrainInfo.cs + Mods.Common 的 TerrainTemplateInfo/DefaultTerrain 全量:Riser 长式 8 段/短式 "LU=6" 双格式与 U/R/D/L 缺侧掩码、TerrainTileInfo 的 TerrainType 名换索引 + MinColor/MaxColor 地形色回落、模板 Tiles 的 PickAny 双路径与非法帧号三消息逐字、TerrainTypes 按 Type 排序 + 重键抛、defaultWalkableTerrainIndex="Clear")、tile_reference(TerrainTile/ResourceTile + TryParse 双段不变式)。
- 新库 `ora_map`(src/map/ 五件):cell_region(CellRegion/CellCoordsRegion/MapCoordsRegion 三区族:行主序零分配枚举器、Expand/BoundingRegion 的空输入 ArgumentException 逐字、Contains(CellRegion) 四端点/CPos 的 MPos 域判定)、cell_layer(CellLayerBase/CellLayer/ProjectedCellLayer:方格直乘索引 + 等距 (u,v) 换算 + 越界 IndexOutOfRange 等价抛、CellEntryChanged 监听表 + CopyValuesFrom/Clear 监听者守卫文本逐字、TryGetValue/Contains 的等距 X<Y 预滤怪癖照抄、Resize 交集拷贝)、map_players(PlayerReference 全 22 字段逐键 + MapPlayers 键=Name + 重键 Add 等价抛)、map(Map.cs 全量:MapField 逐字段表(必填缺 "Required field `{key}` not found in map.yaml" 逐字)、BinaryDataHeader 两格式 + 宽高校验、map.bin 三段读(含 index==255 的 i%4+j%4*4 怪癖;heights clamp)、ComputeUID 的 SHA-1 内容寻址、GetMapFormat 的逐行 "^MapFormat:\s*(\d*)\s*$" 手写匹配、PostInit 全序(Ruleset.Load 失败回退 + 异常快照、AllCells/SetBounds/projectionSafeBounds、CustomTerrain=255、无效 tile 替换表 + Ramp 缓存、UpdateEdgeCells、缓存失效事件挂载序)、SaveBinaryData 偏移公式 + 往返恒等、投影族(悬崖向上传播 while、ProjectedCellHeightInner 的 MaxBy(V)、ProjectCellInner 奇高四候选)、坐标换算族逐行(CenterOfCell/CenterOfSubCell/DistanceAboveTerrain/TerrainOrientation/CellContaining/CellHeightStep)、ContainsAllProjectedCellsCovering 三态、Clamp(MPos) 的 dv=±x/2 搜索、ChooseRandomCell/ChooseClosestEdgeCell/DistanceToEdge/FindTilesInAnnulus 两校验文本逐字、IReadOnlyFileSystem 面(Package 优先/modFiles 兜底)、Dispose=Sequences 释放)、map_cache(MapPreview 同步核心:UpdateFromMap 全字段链 + ToMap 惰性重开;MapCache:MapFolders 枚举(~ 前缀/分类解析)、LoadMapInternal 的 UID→Update→LastModifiedMap/mapUpdates、ChooseInitialMap 的全五条件 + 双回落、GetUpdatedMap 追链、PickLastModifiedMap)。
- `src/sim/` 增量五件:spatially_partitioned(SpatiallyPartitioned.cs 逐语义 + **OPT-A8**:Actor* 特化 = ActorID slab 三平行数组 + id→Actor 反查,InBox 去重 = "最后查询戳"数组零分配(HashSet 三态免跟踪语义保留);通用模板承载 IEffect* 等指针键;空界 "Bounds of {item} are empty." 逐字)、screen_map(ScreenMap.cs Actor/IEffect 面:bin 尺寸 = MapSize×TileSize、单遍合批、Add(effect) 的居中矩形 + ValidBounds、TickRender 双界注册/摘除、ActorsAtMouse 四段链、ActorsInMouseBox、RenderBounds/MouseBounds;Actor 的 MouseBounds/ScreenBounds 经注入函数承载)、selection(Selection.cs 全量:三链通知(RunUnsynced 门禁)、Combine 双态(Take(1)+SymmetricExceptWith/UnionWith)、OnOwnerChanged 本地专删、Tick 的 RemoveWhere、存档序列化双向)、order_generator(IOrderGenerator 接口面)、trait_registry(**D26/D27 接线**:Info 名→工厂(RecordObject&,ActorInitializer&,WorldArena&)→TraitBase*;RegisterWorldTraits 注册 ScreenMapInfo/SelectionInfo;未注册 → nullptr 部分覆盖装配面;按名槽位读值辅助)。
- `src/game/` 增量:game_speed(GameSpeed 三 Required + LoadSpeeds 的包装异常头面逐字 + 字典键 = yaml 节点键)、manifest(TerrainFormat + MapCompatibility = [Id]+SupportsMapsFrom + MergedNode)、mod_data(GetOrCreateMapGrid/GetOrCreateGameSpeeds/GetTerrainInfo = TerrainFormat 名分派 + TileSets 解析 + t.Id 缓存)、ruleset(**按表借用视图**(MergeOrDefault 的 defaults 复用等价);Load 的 mini_yaml_load = MiniYaml.Load 的 map 面等价(additional.Value 逗号文件列表追加 + 单池合并))、game(Game::StartGame 双入口 + MapCacheFace 惰性;Run 清理序补 world/map)。
- `src/sim/ world` 全量重写:全量构造(GameSpeeds 链/系统 actor 解析(ScreenMap/ISelection 缺失等价抛 + 两通知族收集)/ICreatePlayers/RulesContainTemporaryBlocker/CenterOfSubCell 精确注入(D28 面解除));**trait 对象所有权 D26/D27:WorldArena(§4.5)**;AddToMaps/UpdateMaps/RemoveFromMaps(ActorMap 半段空位)、SetRenderPlayer/SetOrderGenerator/SetPauseState/EndGame/LoadComplete(ScreenMap 先行)/PostLoadComplete/OnClientDisconnected/存档回灌段/IssueOrder sink(分层倒置)/Tick 的 PauseShellmap 值面/Dispose 全序。
- `src/core/` 增量三件:sha1(FIPS 180-1 自给自足 + HexOf;流式/一次性恒等)、size(Size.cs 值类型)、polygon(Polygon.cs + Exts 的 PolygonContains/LinesIntersect/WindingDirectionTest)。
- 验收:新 map_test(SHA-1 标准向量、CellRegion 三区、CellLayer 索引/越界/预滤/Resize、MapGrid 21 斜坡/距离桶、OPT-A8 空间索引矩阵、真 ra mod 链:DefaultTerrain(12 类型 308 模板)→ 合成 8×6 地图 → SaveBinaryData 尺寸公式 → 目录包读回(MapFormat/UID/坐标族/Contains 双态/二进制往返恒等)→ MapCache.LoadMaps → 真实地图 ToMap → **World 全量构造(WorldActor/ScreenMap/Selection/GameSpeeds 节拍/首 tick/arena 存活)**);ctest **18 → 19** 双构建全绿;门禁 std_import(296 文件)/upstream_check(285 标注)PASS。
- 实现过程修出三个真问题:DefaultTerrain 的模板表 emplace 时 std::move 掏空定义序 vector(首模板 nullptr → DefaultTerrainTile 解引用崩溃 —— 探针定位,改为字典持裸指针);GameSpeeds 字典键误用 Name 字段(上游 = yaml 节点键 → "default" 查询恒 KeyNotFound);GameSpeeds 漏读 DefaultSpeed。另修 Ruleset::Load 的 MergeOrDefault 语义(初版忽略 defaults 复用态与 additional.Value 的文件列表追加)。

### 已登记偏离(PORTING_PLAN §7.5,第十六批新增)

| # | 位置 | 偏离内容 | 理由 |
|---|---|---|---|
| D107 | src/terrain/ terrain_info | 模板双视图:上游 FrozenDictionary 共享引用 → 定义序 vector<unique_ptr> 持所有权 + id→裸指针字典(修正初版 move 掏空 bug 的形态);MultiBrushCollections/DumpSheets 不解析(无消费点);General 缺 Palette 时 "terrain" 默认 = 常量同值 | 形态适配 + 域裁剪;map_test 308 模板链锚定 |
| D108 | src/map/ map + cell_layer + cell_region | 事件 → construct 序回调表(多播委托序 = 挂载序;缓存失效 handler 先挂的序依赖保留);MapField 反射表 → 逐字段 lambda;Save(yaml 全量)/SavePreview/GetCellSpaceBounds 随编辑器批;GetMapFormat 的 Regex → 手写行匹配;ComputeUID 的 MergedStream → 逐流 Update(摘要恒等);IsExternalFile 恒 false(Phase 6);序列化字段的 C# null = unique_ptr 可空;CellLayer 的 Map 构造路径以 (GridType, MapSize) 直构 | 形态适配;map_test 全链锚定 |
| D109 | src/map/ map_cache | MapPreview 异步面(预览线程/minimap/远程查询/ModifiedDate/SetCustomRules/map.png)与 MapDirectoryTracker/SupportDir 建目录随 Phase 6;mpspawn 直读 Location(ActorReference 机制随 SpawnMapActors 批);PlayerReference.Color 默认 = 注入回退;ToMap 包重开经 MapCache 装配的 modFiles 语境 | 域裁剪 + 形态适配;map_test LoadMaps/ToMap 锚定 |
| D110 | src/sim/ spatially_partitioned + screen_map | **OPT-A8 有意优化**:Actor 键 = ActorID slab(bounds/present/query-stamp)+ id→Actor 反查(上游全局/每 bin Dictionary 的哈希装箱消失);InBox 去重 = per-query 串行号戳(HashSet 三态免跟踪保留);枚举序 = bin 插入序(上游同;消费域 = 拾取/渲染收集,不入同步哈希);ScreenMap 的 FrozenActor 面随 FrozenActorLayer 批;Actor.MouseBounds/ScreenBounds 经注入函数(缺省 = 空界不注册);MouseInput 重载随 UI 输入装配批 | OPT-A8 语义论证;map_test 空间索引矩阵锚定 |
| D111 | src/sim/ selection + order_generator | HashSet<Actor> → 插入序 vector(枚举序 = 上游无删除时插入序;Hash 哨兵保留);Mods.Common OrderGenerator 基类 = C++ 抽象基(GameSettings/MouseActionType 随 Settings 批);语音面经注入钩子;FogObscures 迷雾条件随 Shroud 批;IssueTraitData 的 uint[] = 逗号列表;ISelectableInfo.Voice 经接口名过滤 + 槽位读值 | 形态适配 + 域裁剪 |
| D112 | src/sim/ trait_registry + world | **D26/D27 落地**:traitInfo.Create 反射虚方法 → Info 名注册表工厂(arena 内构造,所有权 = WorldArena);未注册 Info → nullptr 跳过(**部分覆盖装配面**;Cannot locate 逐字抛点保留于 CreateOrThrow;随 trait 批补齐);Actor 对象亦入 arena;WorldActor 系统面缺失 = "TypeDictionary does not contain instance of type" 等价抛(ActorMap/IControlGroups 解析面空位随各批);DefaultOrderGenerator 校验面随该 trait 批;gameInfo/ReplayMetadata/GameOver 事件随 Phase 7;OrderManager 直调 → IssueOrder sink(ora_sim→ora_net 分层);SetPauseState 的 order 工厂随订单族批;PauseShellmap/声音钩子 = 注入值面 | 分层/所有权形态;map_test World 全量构造 + arena 存活锚定 |
| D113 | src/game/ ruleset + mod_data + manifest + game_speed + game | MergeOrDefault defaults 复用 → **按表借用视图**(全表/按 additional 双态;上游引用共享等价);Ruleset::Load 文件打开经 MapFileSystemFace 三函数面(分层倒置);GameSpeeds 字典键 = yaml 节点键;MapGridType/MapVisibility 补注册(IGlobalModData 域);GetOrCreate<T> 的 ObjectCreator.FindType → MergedNode 键命中;Game::StartGame 的 WorldRenderer = 注入钩子(无头测试面)、Map/MapCache 所有权归 Game(D105 族);PrepareMap/LoadScreen/RefreshPalette/SetDepthMargin no-op 锚定 | 分层/所有权/枚举域形态;map_test GameSpeeds/MapCache/StartGame 链锚定 |
### Phase 4 第十五批(2026-10-06):UI 框架起步(Widget 树/Ui/ChromeMetrics/WidgetLoader)+ Game 主循环骨架(ActionQueue)

- `src/core/` 一件新:action_queue(ActionQueue.cs 全文:时间升序稳定插入、锁内切片/锁外执行、重入 Add 不死锁;DelayedAction 时间键)。
- `src/ui/` 六件(新库 ora_ui):widget(Widget.cs 全文:WidgetArgs 载荷闭包/插入序条目表、Mediator 的 type_index 桶 + INotificationHandler/NotificationHandlerSet 混入(SubscribeAll = TypeDictionary.Add 反射接口枚举的编译期等价)、ChromeLogicRegistry 名字分派(未知名 "Cannot locate type: {name}" 逐字 —— ObjectCreator.cs L92)、WidgetBounds/Widget 全方法(几何/焦点三态/GetCursorOuter 逆序/HandleMouseInputOuter 冒泡与 IgnoreMouseOver 恢复/Hidden·Removed 焦点强制让出/GetOrNull·Get 异常文本/TickOuter 的 Logic 推进;Initialize 的 WINDOW_*/PARENT_* substitution Dictionary.Add 撞键抛;LoadFieldOrProperty 虚链:string/bool/IntegerExpression/Logic 逗号拆分 Trim+RemoveEmptyEntries)、ContainerWidget(ClickThrough/IgnoreMouseOver 恒真)、InputWidget(IsDisabled 克隆读源怪癖照抄;上游拷贝构造搬走源委托))、ui(Ui.cs L24-194 静态门面 → 进程级单例:窗口栈 Open/Close/Current(可见栈顶 Root 持有、隐藏条目栈持有;CloseWindow 的 Pop→RemoveChild→BecameHidden 序照抄,含对已 Dispose Logic 仍通知的上游怪癖)、HandleInput 的悬停三态迁移 + Viewport.LastMousePos/LastMoveRunTime 静态直写、HandleKeyPress/HandleTextInput 焦点路由、ResetAll/ResetTooltips、Tick/PrepareRenderables/Draw、Mediator 转发;WidgetLoader/Renderer.Resolution/RunTime/ModData → Deps 注入)、chrome_metrics(ChromeMetrics.cs 全文:Merge 全部 ChromeMetrics 源 → 键值覆盖,实例化形态同 D97 族)、widget_loader(WidgetLoader.cs 全文:ChromeLayout 全解析 + 首 '@' 后键索引(无 '@' = 全键)、重复键 "Widget has duplicate Key" 逐字、Id = Split('@')[1]、字段加载/Initialize/Children 递归/logicArgs 出入参(Add 抛语义保留)、PostInit;WidgetTypeRegistry 名字分派(键 = "{Type}Widget";基型 Container/Input 就地注册;ORA_REGISTER_WIDGET 自注册宏))。
- `src/game/` 一件(新库 ora_engine):game(Game.cs 主循环骨架切片:RunTime 时钟(Stopwatch → 注入/缺省 steady)、RenderFrame、CosmeticRandom、modifiers 键面、ActionQueue 延迟动作(RunAfterTick/RunAfterDelay/PerformDelayedActions)、JoinInner 的非 shellmap OM 先 Dispose 序、JoinLocal 的 EchoConnection + 观战客户端、InnerLogicTick 的 Ui/OM 双 TickTime 节拍 + Sound.Tick + TickImmediate + TryTick→world.Tick + 首帧后 TickRender(全部经 Sync.RunUnsynced 门禁)、LogicTick 的连接态事件、RenderTick 的 prepare→world→UI→flip 全序(BeginWorld/SetListenerPosition/Draw/BeginUI/DrawAnnotations/Ui.Draw/光标 SetCursor ?? "default"/Render/EndFrame)、Loop 的逻辑-渲染双时间表(logicInterval 步长来源链/renderInterval 1..1000 clamp/存档加载 1+200 提速/MaxLogicTicksBehind=250 截断/renderBeforeNextTick/MinReplayFps=10 强制帧/挂起窗口泵)、Run 的 finally 清理 + OnQuit、IsCurrentWorld、Exit/state)。
- 增量:session.hpp 的 SessionClient 补 Faction/SpawnPoint(JoinLocal 装配面);world.hpp 的 RunUnsynced 支持 void 可调用体(上游 Action 面)。
- 验收:新 ui_test(18 测试族:ActionQueue 时序/重入/空抛、WidgetArgs 语义、Mediator 订阅分发、Widget 树表达式定界/RenderOrigin/Get 异常文本/Logic 拆分、焦点让出链与 Hidden 强制、悬停迁移、Ui 窗口栈隐藏/还原/通知计数/ResetAll、PostInit 订阅与 Removed 退订、Clone 深拷贝与 IsDisabled 读源怪癖、ChromeMetrics 覆盖/缺键、WidgetLoader 全流水 + 异常文本矩阵 + MULTI@EXTRA 双 '@' 语义、Game 双 TickTime 节拍/无世界早退/世界挂接 StartGame→TryTick 帧推进/JoinLocal 观战客户端/RunAfterDelay→Exit 的 Loop 收敛/Run 清理与 OnQuit);双构建 ctest **17 → 18**;门禁 std_import(268 文件)/upstream_check(256 标注)PASS。
- 实现过程修出两个真问题:Widget::Removed() 误清 LogicObjects(上游 Removed 后数组保持可观察 —— CloseWindow 的 BecameHidden 快照怪癖依赖此);Ui::ResetAll 沿上游序先清 Root 在 C++ 所有权下析构可见栈顶致悬垂(ASan 实证;改为先排水窗口栈再清 Root,终态等价)。Initialize 的 WINDOW_* 初版误取父界(上游恒取 Renderer.Resolution)。

### 已登记偏离(PORTING_PLAN §7.5,第十五批新增)

| # | 位置 | 偏离内容 | 理由 |
|---|---|---|---|
| D101 | src/core/ action_queue | C# List+BinarySearch+while 推进的插入位 → std::upper_bound(等值段时间插段末,稳定序同语义);DelayedAction.ToString 的 Action 呈现以 "<action>/<null>" 占位(C# 委托 ToString 非契约);Add 的 ArgumentNullException → invalid_argument 同文本 | 形态适配;ui_test 时序/重入/空抛矩阵锚定 |
| D102 | src/ui/ widget | C# static 类族的静态访问 → WidgetArgs 注入(chromeMetrics/windowSize/mediator;Widget::Initialize 的 Renderer.Resolution 与 ChromeMetrics.Get 经 args);子 widget 所有权 GC → 父 unique_ptr(RemoveChild/HideChild 转移出树,Children() 原始指针视图;无 parent 的 loader 产物锚定到加载器);Mediator 的 TypeDictionary 反射接口索引 → type_index 桶 + std::function 通知器(Subscribe 追加/首匹配摘除/Send 插入序快照分发)、INotificationHandler<T> 以通知类型 type_index 自键、NotificationHandlerSet 混入经自身继承 ChromeLogic 保证虚分派;ObjectCreator.CreateObject<ChromeLogic> → ChromeLogicRegistry 名字分派(std::function 工厂 + 自注册宏,严禁函数指针规范);FieldLoader.LoadFieldOrProperty → LoadFieldOrProperty 虚链(string 直赋/bool.Parse 等价抛/IntegerExpression 构造/Logic 逗号拆分;未知字段 = UnknownFieldAction 默认文本 "FieldLoader: Missing field `{key}`" 等价抛;null 字段值按空串承载 —— 有效 chrome yaml 不可达);ChromeLogic 的 Dispose 语义 = 随 widget 析构、Removed() 只退订不清 LogicObjects(上游 Removed 后数组保持可观察,BecameHidden 快照怪癖保留);WidgetArgs 字典 → 插入序条目表(KeyNotFoundException/Add 重复键等价);InputWidget 克隆 IsDisabled 读源怪癖照抄;Clone 未实现类型的 InvalidOperationException 文本逐字 | 所有权/注入面形态(与 D96/D97 同族);ui_test 全族单测锚定 |
| D103 | src/ui/ ui | 窗口栈所有权:可见栈顶 Root 持有、隐藏条目由栈条目持有(HideChild 转移)—— 上游 GC 双引用的显式等价;CloseWindow 的 Pop→RemoveChild→BecameHidden 序照抄(含对已 Dispose Logic 仍通知的上游怪癖,快照指针在析构前逐个调用);**ResetAll 的 RemoveChildren→CloseWindow 序反转为先排水窗口栈再清 Root**(C++ 所有权下先清 Root 会析构可见栈顶致悬垂 —— ASan 实证;上游 GC 下两序终态等价);WidgetLoader/Renderer.Resolution/RunTime/ModData → Deps 注入(无 loader 抛 runtime_error(上游 NRE)、时钟缺省恒 0);LastMousePos/LastMoveRunTime 写 gfx::Viewport 同名静态(Viewport.cs L134-135) | 所有权/注入面形态;ui_test 窗口栈/悬停/键盘路由单测锚定 |
| D104 | src/ui/ chrome_metrics + widget_loader | ChromeMetrics static → 实例类(D97 族)、Get<T> 的 T 域收敛 string(引擎核心唯一消费形态;泛型面随消费批扩展)、缺键 KeyNotFoundException 等价文本;WidgetLoader:ObjectCreator → WidgetTypeRegistry 名字分派(键 = "{Type}Widget";基型就地注册 + ORA_REGISTER_WIDGET 自注册宏;未知名 "Cannot locate type: {name}" 逐字);ModData.WidgetLoader 集成未接(Phase 5;构造面 = FileSystem + ChromeLayout 注入);重复键/缺 Id 消息逐字(SourceLocation.ToString 复用);Id = Split('@')[1](首二 '@' 之间段)逐字;logicArgs 的 args.Add 抛语义保留(子级递归在外层 Add 前已 Remove,同构上游) | 形态适配;ui_test 加载全流水 + 异常文本矩阵锚定 |
| D105 | src/game/ game | 主循环骨架切片,Phase 5-8 依赖面 → Deps/钩子:Settings.Graphics → Deps 值面(Phase 5 换实体,Run 的 MaxFramerate<1 sanitize 以 Loop 内 clamp 等价);NetworkConnection 的连接态观测 → fn_connection_state 钩子(缺省 = EchoConnection 恒跳过 —— 上游 `is NetworkConnection` 模式匹配语义);world.OrderGenerator.Tick → 钩子 no-op(Phase 5);PerfHistory/PerfSample/Benchmark/截图/TextNotificationsManager.Clear/UnitOrders.Clear(无静态态)→ no-op 注释锚定;Renderer 空指针 = 无头测试面(上游恒有);EndFrame 的 DefaultInputHandler 未接(Phase 7 输入装配);WorldRenderer+Viewport → SetWorldRenderer 注入(unique_ptr 所有权;上游 Game 持引用 + GC);JoinInner 的 shellmap OM 保活依赖 WorldRenderer 所有权(Phase 5),当前非 shellmap 即 Dispose;JoinLocal 色面缺(Faction/SpawnPoint 已补,HSLColor 随 Phase 7);Run 的 finally → try/catch + 显式清理重抛;Sound/Renderer/ModData 所有权归嵌入侧(C++ RAII;上游 Game.Dispose 面);NetFrameNumber/LocalTick 缺 OM = NRE 等价抛;worldRenderer.World.OrderManager 加空判跳过(上游 NRE;shellmap 前装配窗口);World.IsReplay → fn_is_replay 钩子(Phase 7 前恒 false);World.IsLoadingGameSave 以 OM 面直算(World.cs L116 公式,非注入) | Phase 5-8 依赖面切片;ui_test 节拍/JoinLocal/延迟动作/Run 清理锚定 |
| D106 | src/net/ session.hpp + src/sim/ world.hpp | SessionClient 补 Faction/SpawnPoint 两字段(Game.JoinLocal 装配面;Color/PreferredColor 的 HSLColor 面随 Phase 7 大厅批);RunUnsynced 增 void 可调用体分支(上游 Sync.RunUnsynced 收 Action;原 C++ 模板假定带返回值,if constexpr 分派,门禁语义不变) | 接线增量;ui_test 的 Game 节拍测试锚定 |
