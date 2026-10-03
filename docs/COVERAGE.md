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
