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
