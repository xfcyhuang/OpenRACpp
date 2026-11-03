# 研究：全量手写 x64 汇编移植的工程量与可维护性证据

**工单**: [#21](https://github.com/xfcyhuang/OpenRACpp/issues/21)（母工单 #19 wayfinder）
**日期**: 2026-11-03
**方法**: 网络检索（一手来源优先）+ 本地仓库测量。所有关键数据注明来源；推断与证据缺位明确标注。

---

## 0. 基线事实（本地测量）

上游七程序集（基线 1f594e3206，2026-10-15）逐目录统计 `.cs`：

| 程序集 | 文件数 | 行数 |
|---|---|---|
| OpenRA.Game | 226 | 44,066 |
| OpenRA.Mods.Common | 1,087 | 159,338 |
| OpenRA.Mods.Cnc | 142 | 20,811 |
| OpenRA.Mods.D2k | 25 | 3,578 |
| OpenRA.Platforms.Default | 20 | 4,841 |
| OpenRA.Server | 3 | 151 |
| OpenRA.Utility | 3 | 205 |
| **合计** | **1,506** | **233,990** |

与工单描述一致（~233KLOC / 1506 文件）。另：上游近 12 个月（2025-11 至今）共 **355 个提交**（日均约 1 个）——「语义合并」不是一次性动作，而是每年数百次的持续性负载。

---

## 1. HLL→汇编的代码量倍率与生产率数据

### 1.1 功能点语言表：同功能 LOC 倍率 ≈ 5~6 倍

Capers Jones / QSM 的功能点换算表（gearing factor，每功能点所需源代码行数 SLOC/FP）是业界标准参考：

| 语言 | SLOC/FP |
|---|---|
| 汇编（basic assembler） | ~320 |
| C | ~128 |
| C++ | ~55 |
| C# | ~54–55 |

- 数据来源：Jones《Software Economics and Function Point Metrics》（[IFPUG PDF](https://www.ifpug.org/wp-content/uploads/2017/04/IYSM.-Thirty-years-of-IFPUG.-Software-Economics-and-Function-Point-Metrics-Capers-Jones.pdf)）；QSM 功能点语言表（[qsm.com](https://www.qsm.com/resources/function-point-languages-table)，直接抓取超时，数值经二手镜像交叉确认：[Haxe 社区引 Jones 数据](https://community.haxe.org/t/capers-jones-function-point-metrics-analysis/1426)、[Jones 换算表镜像](https://www.scribd.com/document/363902698)）。
- **换算到本项目**：233,990 行 C# ≈ 233,990 / 55 ≈ **4,254 功能点**；实现同等功能需要汇编约 4,254 × 320 ≈ **136 万行**，即 C# 代码量的 **~5.8 倍**（对 C++26 路线同为 55 SLOC/FP，倍率相同）。

### 1.2 生产率：每人月交付行数与语言基本无关 → 功能交付差 5~6 倍

- Brooks《人月神话》（1975）：职业开发者平均每人日交付约 **10 行**有效代码，且该行数生产率**与语言无关**（[NDepend 解读](https://blog.ndepend.com/mythical-man-month-10-lines-per-developer-day)、[Skeptics SE 考证](https://skeptics.stackexchange.com/questions/17224/do-professional-software-developers-write-an-average-of-10-lines-of-code-per-day)）。
- COCOMO（Boehm 1981）同样以「交付源指令」为工作量计量单位，其校准数据本身来自汇编尚属主流的年代；含义是：**每行成本近似恒定时，实现同等功能的总人月按 LOC 倍率放大**（[COCOMO 模型综述](https://www.geeksforgeeks.org/software-engineering/software-engineering-cocomo-model)、[COCOMO II 手册](https://www.rose-hulman.edu/class/cs/csse372/201310/Homework/CII_modelman2000.pdf)）。
- **适用性判断**：这两条是「汇编路线人月 ≈ C++ 路线人月 × (320/55) ≈ ×5.8」这一线性换算的理论依据。注意该换算**偏乐观**：它未计入汇编特有的协作/评审/调试放大（见 §3、§4）。

---

## 2. 可比项目案例：现代仍在用大量手写汇编的项目

| 项目 | 规模/人数/年限 | 手写 asm 范围 | 教训 |
|---|---|---|---|
| **RollerCoaster Tycoon**（1999） | 单人（Chris Sawyer），1996–1999 | 全游戏 ~99% x86 汇编（[Sawyer 官方 FAQ](https://www.chrissawyergames.com/faq3.htm)、[Wikipedia](https://en.wikipedia.org/wiki/RollerCoaster_Tycoon_(video_game))） | 存在性证明：单人全汇编大游戏**可行**；但它是「一名 1980 年代起写机器码的专家 + 封闭需求 + 无上游同步」的孤例 |
| **OpenRCT2**（2014–） | 社区团队，多年 | 反向工程：把 RCT2 的汇编**反编译回 C++** | 因汇编「不可读、不可改、无法扩展（多人联机/跨平台/新特性）」，社区逐函数反编译重写，耗时多年（[r/Games 通告](https://www.reddit.com/r/Games/comments/241krb/)、[HN 讨论](https://news.ycombinator.com/item?id=13898812)、[GitHub org](https://github.com/orgs/OpenRCT2/repositories)）——**读懂既有 asm 的单位成本**的实证 |
| **LuaJIT** | 单人（Mike Pall，匿名） | 解释器整体手写 asm（x86/x64/ARM） | 极致性能的正面案例，但 bus factor = 1，社区公开抱怨修 bug「只能等 Mike Pall」（["My love-hate relationship with LuaJIT"](https://news.ycombinator.com/item?id=2588696) 相关讨论）；2022 年业界出现以「自动生成解释器替代手写 asm」的工作，动机即维护难度（sillycross.github.io, 2022） |
| **dav1d / libjpeg-turbo** | 专业团队（VideoLAN 等） | 仅 SIMD 热点函数，每 ISA 一份 | 维护者公开承认成本：dav1d 维护者称「在这么多手写汇编的项目里支持（新平台要求）**不是免费的**」（jbkempf.com，dav1d 1.5.4 发布说明）；libjpeg-turbo 周期性重写/砍掉过时的 NEON/MIPS SIMD 路径（[SIMD 覆盖页](https://libjpeg-turbo.org/About/SIMDCoverage)）。**现代项目的共识：asm 只用于热点，且严格限制面积** |
| **MenuetOS / KolibriOS** | 极小社区，2000– | 全汇编 OS（FASM） | 全汇编可行但社区规模始终极小（[menuetos.net](https://menuetos.net)、[KolibriOS GitHub](https://github.com/kolibrios-nextgen/kolibrios-nextgen)）；其规模（软盘级）与 233KLOC 应用逻辑不可比 |

**适用性判断**：找不到任何现代先例是「把一个仍在活跃开发的 233KLOC 高层语言应用**全量**移植到手写汇编」。最接近的案例全部指向反方向：RCT 的汇编被 OpenRCT2 花多年换回 C++；LuaJIT 的 asm 由单人维护且社区视其为风险；现代高性能库把 asm 限制在热点并持续为「面积过大」付出代价。

---

## 3. 缺陷率、评审与调试代价

### 3.1 缺陷密度直接实证：证据缺位（这本身是发现）

- 未找到汇编 vs 高层语言缺陷密度（defects/KLOC）的严格定量实证研究。可用的最接近证据：
  - UC Davis FSE 2014 对 GitHub 729 项目（~63M 行）的研究：静态类型适度降低缺陷密度，语言选择与缺陷相关（[论文 PDF](https://web.cs.ucdavis.edu/~filkov/papers/lang_github.pdf)）；
  - ICSE 2011 对 C/C++ 大项目的研究：C++ 化改善生产率、缺陷密度大体持平（[论文 PDF](https://www.cs.ucr.edu/~neamtiu/pubs/icse11bhattacharya.pdf)）。
- **结构性论证**（COCOMO 口径）：即便每行缺陷密度相同，汇编需要 ~5.8 倍行数 → **每功能缺陷数 ~5.8 倍**。而实际上汇编每行更易错（手动调用约定、无类型检查、手动寄存器分配），方向只会更差（[SE 讨论串](https://softwareengineering.stackexchange.com/questions/380587/)）。

### 3.2 工具生态对照

| 能力 | C#（上游） | C++26（主线） | 手写 x64 汇编 |
|---|---|---|---|
| 静态分析 | Roslyn 分析器生态 | clang-tidy / cppcheck / Semgrep / CodeQL | **基本缺位**：无 clang-tidy 等价物，生态仅限格式化与指令集 linter（[SO: Assembly analysis tools](https://stackoverflow.com/questions/4110914/assembly-analysis-tools)） |
| 内存安全插桩 | — | ASan/UBSan（编译器 IR 级） | **不覆盖**：ASan 是编译器插桩，手写 asm 不被插桩（[google/sanitizers wiki](https://github.com/google/sanitizers/wiki/AddressSanitizer)） |
| 类型/重构支持 | 完整 | 完整 | 无（符号级 diff/评审） |

**适用性判断**：本项目验证策略（ctest 三编排 base/asan/hardened + SyncHash 对拍）中，ASan 编排在汇编路线下对 asm 代码本体失效，等于失去一条整编排的护栏；代码评审从「审 1 行 C#」变成「审 ~6 行 asm」，评审吞吐直接按倍率劣化。

---

## 4. 长期可维护性与人员供给

- **意愿面**：Stack Overflow 开发者调查中汇编长期位居「最可怕/最不受推崇（dreaded / least admired）」语言前列，2024 年其 admired 比例 ~57%，属主流语言中最低一档（[survey.stackoverflow.co/2024](https://survey.stackoverflow.co/2024/technology)）。
- **供给面**：无公开统计称「会 x64 汇编的工程师占比」，但可由课程面（大学仅在体系结构课接触）与招聘面（asm 几乎不作为主语言岗位）间接判断为**极稀缺**；本项目即使单人执行不存在招聘问题，仍存在**知识传递/bus factor** 问题——LuaJIT 即为 bus factor = 1 的标准反面教材（§2）。
- **适用性判断**：OpenRACpp 若走汇编路线，等于把整个项目的可维护性押在「唯一作者永久在岗」上，且无法从社区获得增量贡献者。

---

## 5. 「语义合并」的长期代价

- **上游压力（事实）**：上游近 12 个月 355 提交（§0）。C++26 主线下每次语义合并已需人工语义理解 + 对拍回归护栏；汇编路线额外承受：
  1. 无类型系统/无重构工具 → 上游一个 trait 语义变更要在 asm 里人工定位所有受影响过程；
  2. 上游重构（改名、移动类、拆方法）在 C++ 侧是机械操作（语义等价原则下仍可借 IDE/编译器核查），在 asm 侧全部退化为符号级考古；
  3. ASan/clang-tidy 护栏对 asm 失效（§3.2），回归只能靠 SyncHash 对拍兜底。
- **直接案例——LuaJIT 拒绝跟随上游**：LuaJIT 永久停留在 Lua 5.1 语义，官方原因是 API/ABI 兼容与内部实现深度绑定；社区讨论明确指出「从用户看 5.1→5.2 只是少量改动，但内部差异巨大，这正是 LuaJIT 不升级的原因」，5.2 的 `_ENV` 语义变更会击穿其手写 asm 解释器的全局解析优化（[LuaJIT extensions 官方页](https://luajit.org/extensions.html)、[HN 讨论](https://news.ycombinator.com/item?id=26740662)、[r/lua 讨论](https://www.reddit.com/r/lua/comments/39sq8f/is_it_worthwhile_to_stick_to_lua_51_for)）。**这是「语义合并进手写 asm 代码库代价高到宁可放弃同步」的直接证据。**
- **证据缺位声明**：未找到任何公开案例是「长期把活跃上游 HLL 项目语义合并进**全量**手写 asm 镜像」。最接近的两例（LuaJIT=放弃跟随；OpenRCT2=把 asm 换回 HLL）都否定该模式。**每次同步成本相对 C++ 路线的倍增系数无文献可引**，只能给推断区间 **×2~×5**（依据：§1 倍率、§3 工具缺位、LuaJIT 案例），标注为推断。

---

## 6. C++26 主线路线：粗基线人月估计（方法透明）

> 全部为**粗估**，仅供建立换算基线，不作为排期承诺。

**方法 A：COCOMO（以 233 KLOC 从零开发为口径）**
- Organic（小团队熟悉项目）：E = 2.4 × 233^1.05 ≈ **734 人月**
- Semi-detached（该规模更现实）：E = 3.0 × 233^1.12 ≈ **1,347 人月**
- 移植口径调整：规格已定（上游即规格）省去需求/设计探索，但增加行为对拍、验证基础设施与上游理解成本，二者部分相消，取 ×0.5~0.8 折扣。

**方法 B：功能点交叉验证**
- 4,254 FP（§1.1）× 行业典型生产率 7~15 FP/人月（Jones/ISBSG 量级）≈ **280~610 人月**，再计对拍验证开销 ×1.3 ≈ **365~790 人月**。

**合成（两法交叉）**：

| 档位 | 区间（粗估） | 说明 |
|---|---|---|
| 到 M1（Phase 0–5，沙盒对局 + 确定性关卡） | **~100–300 人月** | M1 功能面约占全量 15–25%，但 golden_gen/SyncHash/三编排等固定基础设施前置，人月占比高于功能面占比；另按 COCOMO 对 M1 触及的 ~50–70 KLOC 直接估算得 ~130–350 人月，两口径吻合 |
| 全量（七程序集 + ra，不含 cnc/d2k/ts 增量与 Launcher） | **~400–1,000 人月** | 方法 A×折扣 与 方法 B 区间的并集，量级「数百人月」 |

---

## 7. 证据支持的全量手写汇编路线工程量量级推断

**换算**：汇编人月 ≈ C++26 人月 × 5.8（Jones 功能点倍率 320/55，依据 §1；线性换算，偏乐观——未计入 §3 工具缺位与 §5 语义合并倍增）。

| 档位 | 汇编路线人月（推断） | 直觉换算 |
|---|---|---|
| **到 M1** | **~600–1,800 人月** | 单人 50–150 年；10 人团队 5–15 年 |
| **全量** | **~2,300–5,800 人月** | 单人 190–480 年；10 人团队 19–48 年 |

**结论要点**：

1. **量级判断**：全量手写 x64 汇编移植 233KLOC 托管代码，到 M1 即需「千余人月」量级、全量需「数千人月」量级——超出任何现实的单人/小团队工程承载。C++26 主线（全量数百人月）已属重型项目，汇编路线再放大 ~5.8 倍且偏乐观。
2. **可行性对照**：唯一的单人全汇编大游戏先例（RCT）依赖「顶级专家 + 无上游 + 封闭需求」三前提，本项目三条全不满足（需对拍验证、上游年 355 提交持续演进）。
3. **可维护性**：bus factor = 1（LuaJIT 教训）、ASan/静态分析护栏失效、人员供给稀缺且意愿最低，长期可维护性风险极高。
4. **语义合并是决定性否决项**：每年 355 次上游变更落到手写 asm 上，最接近的先例（LuaJIT）选择了**放弃跟随**；不存在「全量 asm 镜像持续语义合并」的成功案例（证据缺位，非仅证据不利）。
5. **对 #19 的建议**：汇编路线仅在「热点函数内联优化」意义上与主线兼容（dav1d/libjpeg-turbo 模式：C++26 为主体、asm 面积严格受限），「全量手写」应判为不可行。

---

## 来源清单

**本地测量**：`Z:\source_code\OpenRA`（基线 1f594e3206）`find + wc` 统计；`git log --since=2025-11-01` 计数。

**文献/数据**
1. Capers Jones, *Software Economics and Function Point Metrics* — https://www.ifpug.org/wp-content/uploads/2017/04/IYSM.-Thirty-years-of-IFPUG.-Software-Economics-and-Function-Point-Metrics-Capers-Jones.pdf
2. QSM Function Point Languages Table — https://www.qsm.com/resources/function-point-languages-table（数值经二手镜像交叉确认）
3. Jones 换算表镜像（Assembly 320 / C 128 / C++ 55） — https://www.scribd.com/document/363902698
4. Haxe 社区对 Jones 数据的引用 — https://community.haxe.org/t/capers-jones-function-point-metrics-analysis/1426
5. Brooks 10 行/人日 — https://blog.ndepend.com/mythical-man-month-10-lines-per-developer-day ；https://skeptics.stackexchange.com/questions/17224/
6. COCOMO 模型与系数 — https://www.geeksforgeeks.org/software-engineering/software-engineering-cocomo-model ；COCOMO II 手册 https://www.rose-hulman.edu/class/cs/csse372/201310/Homework/CII_modelman2000.pdf
7. UC Davis FSE 2014 语言与代码质量 — https://web.cs.ucdavis.edu/~filkov/papers/lang_github.pdf
8. ICSE 2011 C/C++ 缺陷与生产率 — https://www.cs.ucr.edu/~neamtiu/pubs/icse11bhattacharya.pdf

**案例**
9. Chris Sawyer 官方 FAQ（RCT 99% 汇编） — https://www.chrissawyergames.com/faq3.htm
10. Wikipedia: RollerCoaster Tycoon — https://en.wikipedia.org/wiki/RollerCoaster_Tycoon_(video_game)
11. OpenRCT2 通告与讨论 — https://www.reddit.com/r/Games/comments/241krb/ ；https://news.ycombinator.com/item?id=13898812 ；https://github.com/orgs/OpenRCT2/repositories
12. Mike Pall 论手写汇编解释器 — https://news.ycombinator.com/item?id=2588696 ；https://luajit.org/luajit.html
13. LuaJIT 停留 5.1 与扩展策略 — https://luajit.org/extensions.html ；https://news.ycombinator.com/item?id=26740662 ；https://www.reddit.com/r/lua/comments/39sq8f/
14. 自动生成解释器（维护动机） — sillycross.github.io（2022，经 Web 检索摘要）
15. dav1d 维护者谈手写 asm 成本 — jbkempf.com（dav1d 1.5.4 发布说明，经 Web 检索摘要）
16. libjpeg-turbo SIMD 覆盖 — https://libjpeg-turbo.org/About/SIMDCoverage
17. MenuetOS / KolibriOS — https://menuetos.net ；https://en.wikipedia.org/wiki/MenuetOS ；https://github.com/kolibrios-nextgen/kolibrios-nextgen

**工具与人员**
18. ASan 插桩边界 — https://github.com/google/sanitizers/wiki/AddressSanitizer
19. 汇编静态分析工具缺位 — https://stackoverflow.com/questions/4110914/assembly-analysis-tools
20. Stack Overflow Developer Survey 2024 — https://survey.stackoverflow.co/2024/technology
21. 汇编规避的一般工程讨论 — https://softwareengineering.stackexchange.com/questions/380587/
