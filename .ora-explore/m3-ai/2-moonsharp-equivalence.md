# Issue #46 §2：MoonSharp 确定性等价面 → 实证改写：Eluant（原生 Lua 5.1.5）确定性等价面（2026-11-10）

口径与证据等级同 §1。本节任务原文是「MoonSharp 确定性等价面」；经 §一 实证，上游依赖是 `OpenRA-Eluant 1.0.23`（原生 Lua 5.1.5 + C# 绑定），MoonSharp 不在上游依赖闭包内。本笔记按真实基准改写，原题的四个量化维度（数值/字符串/表序/随机/错误路径）保留。

---

## 一、上游集成方式【实测】

| 项 | 事实 | 锚 |
|---|---|---|
| 引用形态 | NuGet 包引用，仓库内无源码（非 submodule、非 vendored fork） | `OpenRA.Game/OpenRA.Game.csproj:8` |
| 版本 | OpenRA-Eluant 1.0.23（nuspec：MIT，"targets version 5.1.5 of the native project"，2013 Chris Howie 原作 + OpenRA 维护） | `~/.nuget/packages/openra-eluant/1.0.23/openra-eluant.nuspec` |
| 原生库分发 | 包内 `native/{win-x64,win-arm64,linux-x64,linux-arm64,osx-x64,osx-arm64}/lua51.{dll,so,dylib}` 六份预编译二进制 | 同上目录【实测】 |
| 是否 fork | 是（Eluant 是 Chris Howie 2013 年 LuaInterface 后继，OpenRA 团队改版打包）；**fork 的是绑定层，语言实现是原生 lua.org C 源** | nuspec 版权行 + 反射 API 形态【实测】 |
| C# 侧 API 面 | `LuaRuntime`/`MemoryConstrainedLuaRuntime`/`LuaValue` 族/`ObjectBinding.*` 19 接口（反射清单见 §1 笔记 §一·3） | dotnet 10 反射【实测】 |
| OpenRA 消费点 | ScriptContext（运行时/沙盒/装载）、ScriptObjectWrapper/ScriptMemberWrapper（成员绑定）、ScriptTypes（值转换）、LuaScript trait（生命周期）、Global/Properties（API 实现） | `OpenRA.Game/Scripting/`（7 文件）+ `OpenRA.Mods.Common/Scripting/`（52 文件）【实测 ls-tree】 |

## 二、Eluant 与「标准 Lua 5.1」的语义差异面

关键结论先行：**Eluant 不重实现语言语义**（与 MoonSharp 纯 C# 解释器本质不同），语言层=原生 Lua 5.1.5 逐语义同源；可动差异面全部在**绑定层**。逐维度：

### 2.1 数值
- 语言层：lua_Number=double 单一类型（5.1 无整数型）。C++ 侧若 vendored 同版 C 源（§1 正档），`+ - * /`、比较、`math.floor/sqrt/fmod`（IEEE 精确或 libm）同源；**理论残余差异=libm 的 `sin/cos/exp/pow/tanh`**（沙盒保留 math 全表，ScriptContext.cs:163-171 白名单 + 174-183 只禁 random/randomseed【精读】）。**地图语料实证（230 个 lua 文件全量 grep）：`math.` 调用仅 12 次，全部为精确运算**——`math.abs` 8、`math.floor` 2、`math.max` 1、`math.ceil` 1，三角/指数零调用。**风险定级：语料内为零（libm 差异面在现有语料不触发）；未来脚本引入三角函数时需补 PoC 对拍断言**。
- 绑定层（进同步面的真实差异点）：`OpenRA.Game/Scripting/ScriptTypes.cs:61-88` `TryGetClrValue` 把 lua double 转 C# 参数时的 `(int)/(short)/(byte)` **C# 截断转换**（向零取整、不检查范围）——C++ 侧必须复刻同款 `static_cast` 截断语义，否则脚本传 3.7 给 int 参数时行为分叉。返回向（C# → lua）：int/short/byte → double 无损，零风险【精读】。

### 2.2 字符串
- 语言层：Lua 5.1 字符串不可变化 + 驻留；`tostring(number)` 用 `"%.14g"`（lua 5.1 源 `LOCALE_FMT` 族）——vendored 同版同源。**跨编译器差异点：locale 敏感的小数点**（lua 5.1 的 `lua_Number2str` 用系统 locale 的 `%g`？——lua 5.1.5 源码实际固定 `"%.14g"` 且 setlocale(LC_NUMERIC) 默认 C locale；风险=宿主进程别处调 setlocale。C++ 侧进程内无人改 LC_NUMERIC，风险低【精读+推测】）。
- 绑定层：`LuaString` ↔ `System.String`（UTF-16）经 P/Invoke 转 UTF-8——脚本内字符串只作标识符/消息用，不进数值决策；错误消息含 lua 字符串拼接（表现层）。**风险定级：低**。
- `string.format`/`string.rep` 等 string 库由原生 lua 提供（白名单保留 `string`），同源零差异。

### 2.3 表序（pairs/next 遍历序）——**本面最高风险项**
- Lua 语言规范不定义遍历序；实际序由 ltable 哈希（lua 5.1 字符串哈希 `luaS_hash` 是**确定性算法**）+ 冲突链 + rehash 历史决定。**同一份 lua 5.1.5 C 源码、同样的插入/删除序列 → 遍历序一致**（【精读】lua 5.1 ltable 语义；这也是 vendored 同版的核心论据）。
- 进同步面的路径：脚本 `pairs`/ipairs 遍历表后在循环体内发绑定调用（决策序 → 世界变更序）。**若 C++ 选非同源引擎（§1 候选②③），表序必须逐 bit 复刻 lua 5.1 的 ltable——这是自写解释器路线最难的单点**（rehash 时机、数组段/哈希段分界、字符串哈希种子常量）。上游语料实证：`pairs(` 出现于多数战役脚本（【实测】`Utils.Do`（1001 次）内部即 ipairs 型遍历）；`UtilsGlobal.Do/Where/_shuffle` 族把遍历序直接带进脚本控制流（`OpenRA.Mods.Common/Scripting/Global/UtilsGlobal.cs`【精读】）。
- **风险定级：vendored 同版=零差异（同二进制谱系）；非同源=高风险**。

### 2.4 随机
- **上游架构已把引擎随机从同步面剔除**：`math.random`/`math.randomseed` 沙盒置 null（ScriptContext.cs:174-183，注释 "not desync safe, unsuitable"）；脚本随机唯一正路 = `Utils.Random/Shuffle/RandomInteger` → **`World.SharedRandom`**（UtilsGlobal.cs:124,131,146【精读】）——并入同步随机消耗序，与 C# 世界机制共用同一 MersenneTwister（本项目 T03 已逐语义移植）。
- **lua 引擎选型对随机面零影响**（三候选同分）：随机等价负担在 `SharedRandom` 消耗序（黄金对拍已有整套账本机制，D029/D036/D055 先例）。
- 墙钟面：`DateTimeGlobal.IsHalloween`（`DateTimeGlobal.cs:36`，`DateTime.Today` 真实墙钟，标注 Obsolete）与 `CurrentMonth/CurrentDay` 同族是脚本可触达的墙钟读数；语料实证仅 `mods/ra/maps/desert-shellmap/desert-shellmap.lua:11` 使用（菜单背景 shellmap，Visibility: Shellmap，非遭遇战图集）【实测 git grep】；`GameTime => World.WorldTick`（DateTimeGlobal.cs:39）确定。fort-lonestar/oil-spill 语料零墙钟调用 → 遭遇战图集内零墙钟风险。

### 2.5 错误路径
- 脚本错误 → `LuaException` → `ScriptContext.FatalError`（ScriptContext.cs:238-265）→ **`World.AddFrameEndTask(w => World.EndGame())`**——lua 错误终结对局（可观测：对局提前结束、SyncHash 停止），不是静默吞掉【精读】。C++ 侧等价形态 = fatal 通道 + 帧尾 EndGame（str_fatal 先例 D018）。
- 装载期错误（DoBuffer 抛异常）同样走 FatalError 后 context 不可用（ScriptContext.cs:217-226）。
- pcall/xpcall 在白名单（ScriptContext.cs:167）——脚本可自捕获错误，错误串（`LuaException.Message` = lua 原生 error 串透传）进脚本控制流 → **错误消息字符串本身是同步可观测面**（脚本据 error 串分支）。lua 5.1 原生错误串（"attempt to index a nil value"）与绑定层错误串（ScriptObjectWrapper.MemberNotFoundError 的 `Actor 'X' does not define a property 'Y'`，ScriptObjectWrapper.cs:80 等）都需要逐字符复刻（D013/D016 消息复刻先例）。
- 内存/指令预算：`MaxUserScriptMemory=50MB`（ScriptContext.cs:122,215 `MaxMemoryUse = MemoryUse + 50MB`，Eluant `MemoryConstrainedLuaRuntime` 超限抛异常→FatalError）；`MaxUserScriptInstructions=1_000_000`（ScriptContext.cs:125）**只是注入 lua 的全局变量，全仓库无消费方**（【实测】git grep 零命中 mods/OpenRA.Mods.Common）——死接口，C++ 可不实现（登记即可）。

## 三、同步可观测面 vs 表现层差异清单（总表）

| 差异源 | 进 SyncHash（对拍风险） | 仅表现层 |
|---|---|---|
| Utils.Random* → SharedRandom 消耗序 | **是**（核心对拍点，与 C# 世界机制共享账本） | — |
| 表序 pairs/next（非同源引擎时） | **是**（决策序） | — |
| TryGetClrValue double→int 截断 | **是**（参数语义） | — |
| 绑定层错误消息串（pcall 分支） | **是**（控制流输入） | console/log 输出 |
| libm 三角/指数 | 是（若脚本决策用；语料占比低） | — |
| tostring(number) %.14g | 间接（若串进决策/错误串） | Media 消息文本 |
| Media.PlaySound/Speech、Camera、Lighting、Beacon、UserInterface 提示 | — | **是**（音视频/UI 面，sfx 先例 D071 表现层随机隔离） |
| EngineDir 全局（ScriptContext.cs:186） | — | 是（路径串） |
| print → console/log（ScriptContext.cs:193-194） | — | 是 |
| 脚本耗时（PerfSample "tick_lua"，ScriptContext.cs:298-305） | — | 是 |

## 四、SyncHash 风险量化结论

1. **上游基准是原生 Lua 5.1.5**， vendored 同版 C 源路线下语言层差异≈0，残余风险三件：libm 非精确函数、绑定层截断语义复刻、错误消息串复刻——三件都有既定工程手段（黄金对拍 + 消息逐字符断言 + D013/D023 先例）。
2. **MoonSharp 风险模型不适用**：不存在「纯 C# 重实现的语义偏差面」。issue #46 预告的「最大技术风险：MoonSharp 与标准 Lua 5.x 语义差异」经实证**降级**为「绑定层转换语义 + libm」两个窄面。
3. 非同源路线（②③）的等价负担重排序：表序 ltable 复刻 > 错误串 > %.14g > 数值细节——表序是单点否决项（对拍一帧即崩，且崩因难定位）。

## 五、实证样本与可复现步骤（对拍验证者用，User Story 11）

PoC 设计（宿主沙盒内最小 lua 语义对拍，代码不入 src/，issue #46 裁定允许）：

1. **oracle 侧产样**：golden_gen（tools/golden_gen，dotnet 10 无头宿主，M1/M2 已有 125 产物先例）加 dump 模式：装载一张含 LuaScript 的地图（建议 `mods/ra/maps/fort-lonestar`——遭遇战可见图，或最小合成：ore-lord rules.yaml 追加 `LuaScript: Scripts: probe.lua` + 自写 probe.lua），逐帧 dump SyncHash + `World.SharedRandom` 消耗账本（T17 frames.txt 格式沿用）+ `Lua debug:` 日志。probe.lua 覆盖面：pairs 遍历序落日志、Utils.Random 消耗、tostring(数值) 串、双精度边界（0.1+0.2、2^53、-0.0）、pcall 错误串、TryGetClrValue 传 3.7/3.5/-3.5 给 int 参数的截断回显。
2. **C++ PoC 侧**：vendored lua 5.1.5（或候选②子集解释器）接同一 probe.lua + 最小绑定桩（Utils.Random→rng_shared、print→log），同种子跑同帧数，逐行比对 dump。
3. 判定：SyncHash 恒等 + rng 账本恒等 + 遍历序/串/截断回显逐行恒等 → 候选获得「宿主沙盒对拍实证」证据；任一分叉即定位差异面。
4. 复现命令形态（既有设施）：`tools/golden_gen/run.cmd ... produce <mode>` / `check`（README.md 同目录），模式名进 golden 产物清单（tests/golden/ 下新目录），check 双跑 SHA256 恒等沿用 D040 口径。

## 六、给 §1 矩阵的反馈

等价面证据整体支持「上游同源优先」：Eluant 架构（原生解释器 + 绑定层）使得「与上游语义同源」在 vendored Lua 5.1.5 路线上是**结构性成立**而非逐项追赶；MoonSharp 式的「解释器语义偏差面」在本上游不存在。
