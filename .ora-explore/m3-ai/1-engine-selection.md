# Issue #46 §1：lua 引擎选型矩阵（2026-11-10）

口径：上游基线 `1f594e320`（见 UPSTREAM.baseline），全部行号锚对此基线。术语用 GLOSSARY（语义等价/黄金对拍/同步哈希/偏离登记/垂直切片/遭遇战）。
证据等级：**【实测】**=本机一手盘点（git show / NuGet 包 / dotnet 反射）；**【精读】**=上游源码逐行；**【推测】**=本票推断。

---

## 一、前置事实修正：上游 lua 引擎不是 MoonSharp，是 Eluant（原生 Lua 5.1.5）【实测】

这是本票最重要的证据，直接改写三候选的前提：

1. **上游引用的是 NuGet 包 `OpenRA-Eluant 1.0.23`**，非 MoonSharp：
   `OpenRA.Game/OpenRA.Game.csproj:8` —— `<PackageReference Include="OpenRA-Eluant" Version="1.0.23" />`。
   仓库全树（`git ls-tree -r 1f594e320`）grep `moonsharp` 零命中【实测】。
2. **Eluant = 原生 Lua 5.1.5（C 实现）+ C# P/Invoke 绑定层**，不是纯 C# 解释器：
   - 本机 NuGet 缓存 `~/.nuget/packages/openra-eluant/1.0.23/` 实测含 `native/win-x64/lua51.dll`、`linux-x64/lua51.so`、`osx-{x64,arm64}/lua51.dylib`（五平台原生库随包分发）。
   - nuspec 原文："The package includes the C# wrapper (Eluant) and the native binaries for Lua and targets version **5.1.5** of the native project"，版权行 "Copyright (c) 2013 Chris Howie；Copyright (c) The OpenRA Developers"，license MIT【实测，`openra-eluant.nuspec`】。
3. **Eluant.dll 的公共 API 面**（dotnet 10 反射，本机实测）为绑定形态而非解释器形态：
   `LuaRuntime`（ctor / DoBuffer / DoString / CompileString / CreateFunctionFromDelegate / CreateTable / Globals）、`MemoryConstrainedLuaRuntime`（+`MaxMemoryUse`/`MemoryUse` 两个 Int64 属性）、值类型族 `LuaValue/LuaNumber/LuaString/LuaBoolean/LuaNil/LuaTable/LuaFunction/LuaThread/LuaUserdata/LuaVararg`、`LuaException`、对象绑定族 `Eluant.ObjectBinding.*`（ILuaTableBinding/ILuaCallBinding 等 19 接口）。无 AST/解释器入口——语言语义全部在原生 lua51 里。
4. **消费面**：OpenRA 对 Eluant 的全部用法集中在 `OpenRA.Game/Scripting/`（7 文件）与 `OpenRA.Mods.Common/Scripting/`（Global 16 文件 + Properties 34 文件 + LuaScript/CallLuaFunc/ScriptTriggers/Media），`using Eluant` 的程序集外零引用【实测 git grep】。

**推论**：issue #46 三候选中「vendored Lua 5.4 + MoonSharp 差异垫片」的表述基于「上游=MoonSharp（纯 C# Lua）」的假设，该假设不成立。真实等价基准是**原生 Lua 5.1.5 参考实现（lua.org C 源码，MIT）**。三候选据此重述（见 §三），不预设结论。

---

## 二、M3 语境里 lua 引擎的真实权重（先解耦再选型）【精读+实测】

选型前必须钉死 lua 在「遭遇战完整形态」里的边界，否则矩阵会量错对象：

1. **上游 bot（M3 验收口径①②的对象）不依赖 lua**：
   - 上游已无 NullBot（仓库全树 grep `nullbot` 零命中【实测】）。bot = **C# trait 体系**：`OpenRA.Mods.Common/Traits/Player/ModularBot.cs:43`（`ModularBot : ITick, IBot, INotifyDamage`）+ 12 种 BotModule（`OpenRA.Mods.Common/Traits/BotModules/`，共 21 文件 6444 行【实测 wc】）。
   - bot 激活链、注入链、随机源证据见 §4 笔记（`OpenRA.Game/Player.cs:217` host-only 激活；`world.LocalRandom` 决策随机）。
   - **bot vs bot 逐帧 SyncHash 黄金对拍与 lua 引擎零相关**——对拍面是订单流 + 世界机制，bot 的 C# 逻辑在 oracle 宿主侧跑（或经回放重演注入）。
2. **lua 只在地图脚本面出现，ra 官方遭遇战图几乎不挂 lua**【实测】：
   - ra 全部 74 张地图目录中，含 `LuaScript` 的 rules.yaml 共 63 份，其 map.yaml Visibility 分布：57 张 MissionSelector（战役）、1 张 Shellmap、3 张 Lobby+MissionSelector、**纯 Lobby 仅 2 张**（`mods/ra/maps/fort-lonestar`、`mods/ra/maps/oil-spill`——合作生存/特殊玩法图）。
   - M1/M2 主验收图 ore-lord（`mods/ra/maps/ore-lord.oramap` 内 map.yaml）无 LuaScript，`Visibility: Lobby`【实测解包】。
   - 全 mod lua 文件 230 个（ra/cnc/d2k/ts 地图 + `mods/common/scripts/utils.lua` 34 行 + `mods/ra/scripts/campaign.lua` 81 行）【实测】。
3. **lua 进同步可观测面的唯一通道**：脚本经绑定 API 改世界（Trigger 回调、Actor 属性、生产订单）；lua 语言层的随机已被上游沙盒显式禁用（`OpenRA.Game/Scripting/ScriptContext.cs:174-183` 把 `math.random`/`math.randomseed` 置 null，注释 "not desync safe"），脚本随机统一走 `Utils.Random*` → `World.SharedRandom`（`OpenRA.Mods.Common/Scripting/Global/UtilsGlobal.cs:124,131,146`）——即 **lua 侧不存在引擎私有随机源，随机序全部并入同步随机面**【精读】。

**结论**：lua 引擎是「带脚本地图（战役 + fort-lonestar/oil-spill 类）」的支撑面，不是「遭遇战带 AI 对手可玩」（PORTING_PLAN Phase 8 验收原文）的必要面。两件事可解耦切票（§5 采纳）。

---

## 三、三候选选型矩阵

候选按 issue #46 固定为三路线；其中①③按 §一 证据做「实证修正」标注（原表述→重述），维度评分不预设结论，证据倾向以引注给出。

| 维度 | ① vendored Lua **5.4** + 「MoonSharp 差异垫片」→ 实证修正：vendored Lua（5.1.5 同版本为正档 / 5.4+垫片为偏档） | ② 自写 lua 子集解释器 | ③ 逐语义移植 MoonSharp → 实证修正：逐语义移植 Eluant 所依据的 Lua 5.1 语义面（纯 C++ 完整解释器） |
|---|---|---|---|
| **确定性等价潜力** | **5.1.5 正档：高**。与 oracle 同为 lua.org 参考实现源码，语言语义同源；剩余差异集中在 libm（sin/cos/pow 等跨库差异，沙盒未禁 math 三角函数，ScriptContext.cs:163-171 白名单保留 math 全表）与编译器浮点环境——见 §2 笔记量化。**5.4 偏档：中**。5.1→5.4 有整数子类型/`//`/位运算/`%g` 格式/`table.unpack` 等语义差（【精读】沙盒白名单 `unpack`（ScriptContext.cs:167）在 5.4 需垫 `table.unpack`），每处差异都是潜在 SyncHash 分叉点，垫片面=语言 diff 面，不可枚举封闭 | **低-中**。只保证「子集内」等价；地图脚本一旦用到子集外语义（战役脚本 230 文件的真实语料，含 metatable/pcall/vararg/string.format 等），要么扩子集要么拒载。等价性由自测定义，无独立基准 | **中**。语义面完整但等价性靠「逐条对拍参考实现」自证；解释器实现 bug 与语义偏差同形，定位成本最高 |
| **与同步哈希对拍的可达成性** | 5.1.5 正档：宿主沙盒 PoC 对拍可直接用 ore-lord+lua 图或 fort-lonestar（可复现步骤见 §2 笔记 §五）| 需先建「子集覆盖度」语料断言，否则对拍全绿也不能证明语义等价 | 同①（语义面全），但首次全绿前的调试期最长 |
| **工作量**（含绑定层，不含 Scripting API 面本体——API 面三路线等价） | lua 5.1.5 C 源 ~1.7 万行 vendored 入库 + CMake 静态库 + C 边界封装 TU（cpp26.md §二第三方边界豁免）+ OpenRA 侧绑定层改写（Eluant LuaValue/Dispose 语义 → C++ RAII）。**点估 1.5 人月**（区间 1-2.5，§5 详算） | 子集解释器本体（词法/语法/求值/GC 或 Arena）+ 语义差测试。语料驱动的子集仍需覆盖 230 文件实际语法面。**点估 4.5 人月**（区间 3-7） | 完整 5.1 语义解释器（含 string 库 %.14g 格式化、ltable 哈希遍历序复刻、协程或其裁剪论证）。**点估 6 人月**（区间 4-9） |
| **风险** | libm 跨平台差异（低概率高隐蔽：脚本数学进决策分支→订单分叉）；vendored C 代码维护面（上游 Eluant 升版时要跟） | **语料外语义静默偏离**（最危险：偏离登记无法穷举）；长期与上游语义漂移 | 工作量超支风险；遍历序/哈希复刻是逐 bit 工程，错一处全对拍崩 |
| **cpp26.md 合规** | 合规。vendored 第三方源码入库已裁定可接受（issue #46 Implementation Decisions）；构建期/链接期零外部依赖满足——lua 5.1.5 源码自带构建（无 libm 外依赖，Windows 自带 msvcrt），编译为 `ora::` 内静态库；C 头/宏照常 include（cpp26.md §二第三方库头豁免，D024/D056 先例）；C 边界 TU 免 `import std;`（同豁免） | 最合规（纯自有 C++，`import std;` 全覆盖） | 同② |
| **与上游同步（语义合并）成本** | 低：上游 Eluant 升版只动绑定层 | 高：上游脚本语料演化需跟读 | 高：同左 |
| **先例一致性** | stb_truetype（D063/D064 裁定单头入库）、SDL2/GL 头 include（D024）、OpenAL 运行时动态加载（D057）——vendored/边界封装是本项目常规手段 | 无先例（自写解释器超出 std→Boost→自研顺序的「自研仅限 std/Boost 均无」条款论证负担：cpp26.md §二依赖选型。lua C 源码存在且 MIT，自写解释器的「为何不适用」论证较弱） | 同② |

**矩阵读法**（证据倾向，非结论）：三路线的等价潜力排序 ①(5.1.5) > ③ ≈ ①(5.4+垫片) > ②；工作量排序 ① < ② < ③；风险性质不同——①是「外部同源风险」（libm/构建环境），②③是「自造基准风险」。若决策票接受 vendored 入库裁定（已裁定），①的 5.1.5 正档在四个维度同时占优或持平；②③的合理动机只剩「不接受 C 源入库」或「子集可显式封闭」两个前提，而前者已被 issue #46 裁定排除，后者被 230 文件语料的实际语法面削弱（战役脚本是 Phase 11/M4 面，但 fort-lonestar/oil-spill 已在遭遇战 Lobby 图集内）。

## 四、候选①的 5.1.5 正档与 5.4 偏档对比（决策票需要的细分证据）

| 点 | vendored Lua 5.1.5 | vendored Lua 5.4.x |
|---|---|---|
| 与 oracle 语言语义同源 | 同（同一参考实现谱系） | 否：5.2 _ENV/5.3 整数型与 `//`/5.4 语义微调（warning、闭包语义细节） |
| 沙盒白名单适配（ScriptContext.cs:163-171） | 白名单 `unpack`/`math`/`string`/`table` 原生匹配 | 需垫片：`unpack`→`table.unpack`；整数除法脚本若出现 `//` 则 5.1 报错而 5.4 出整数——错误路径分歧进脚本控制流 |
| 数值语义 | lua_Number=double 单一（脚本数值决策与 oracle 同构） | 双类型（integer/float），`tostring(3)` 语义不同（"3" 两边同但 `3.0` 在 5.4 整数化） |
| 上游跟版成本 | 上游钉 5.1.5（nuspec），Eluant 升版不影响语言面 | 上游任何升版都扩大 5.4↔上游差分面 |
| 结论语向 | **垫片≈0**（绑定层语义即全部差异面） | 垫片=语言 diff 面（未封闭） |

## 五、合规底线核验清单（三路线共用）

- 构建期零外部依赖：① lua 5.1.5 源码树自带 Makefile 语义收敛为 CMake 静态库目标（无外部包引用）；②③无依赖。
- 链接期零外部依赖：①静态链接入 ora 目标族（lua51.dll 形态不需要——上游 C# 是动态 P/Invoke，C++ 静态更简，且规避 DLL 分发）；②③无。
- 运行期：均无新动态库（对照 D057 OpenAL 的 LoadLibrary 形态不适用——lua 无需运行时探测）。
- cpp26.md §三禁异常：lua C API 是错误码返回（lua_pcall），天然适配；绑定层错误路径走 `std::expected`（D004 口径）。
- cpp26.md §四禁函数指针：lua C API 的 lua_CFunction 是 C 回调，属「对接 C 接口」豁免边界（收口单一绑定 TU，同 D024 函数指针垫层先例）。

## 六、留给决策票的三个问题（本票不裁）

1. 引擎路线三选一（本矩阵证据倾向 ①5.1.5 正档，见 §三读法）。
2. lua 面进 M3 的深度：仅绑定期基建（引擎+沙盒+context）vs 含最小地图脚本通路（fort-lonestar 或最小合成脚本图）——影响 §5 切票序列。
3. PoC 是否先做（宿主沙盒内最小 lua 语义对拍样本）：建议做，样本设计见 §2 笔记 §五（可复现步骤已给）。
