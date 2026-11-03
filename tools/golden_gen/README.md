# tools/golden_gen

dotnet 10 无头 oracle 宿主（工单 #3 / T02）。`ProjectReference` 指向上游基线源码
`../openra/OpenRA.Game/OpenRA.Game.csproj` 构建宿主，进程内驱动**上游真源**产出黄金
基准产物，供 C++ 侧移植黄金对拍（入库字节 = 判定真值）。产物落本仓库
`tests/golden/**`，上游侧只写其 git 忽略目录 `openra/bin`（不改上游 tracked 文件）。

## 用法（一条命令）

```
# 仓库 tools/golden_gen/ 下；cmd 或 Git Bash 均可
tools\golden_gen\run.cmd gen      # 重产黄金 + .sha256 旁车（评审后入库）
tools\golden_gen\run.cmd verify   # 重产并与入库黄金逐字节比对
tools\golden_gen\run.cmd check    # 验收复验：每产物独立重产两次断言字节恒等（SHA256 一致），再比对入库黄金
```

退出码：`0` 成功 / `1` 校验失败（含 check 双跑不恒等）/ `2` 基线漂移 /
`3` 宿主错误 / `77` 前置缺失（dotnet 或 ../openra 不在，ctest 可映射 SKIP）。

前置：dotnet 10；兄弟目录 `../openra` 检出钉在 `UPSTREAM.baseline` 首行 commit
（宿主每次动作前断言，防基准漂移）。首次全量冷构建上游约 10-12 分钟，之后增量秒级。

## 产物模式（Program.cs 的 artifacts 清单登记，随工单扩展）

- `prng`：上游 `OpenRA.Support.MersenneTwister` 序列 dump，7 种子 × 10 kind，
  `tests/golden/prng/seed_<n>.txt`（T03 引入；C++ 侧 `tests/prng.cpp` 消费）。
- `resolved-rules`：上游 `IUtilityCommand --resolved-rules ra:BADR.Bomber`，经
  `ModData.ObjectCreator` 自 `openra/bin` 的上游 mod 程序集加载命令体，覆盖 MiniYaml
  解析、Inherits/Inherits@ 继承展开与多文件合并语义，
  `tests/golden/resolved_rules/BADR.Bomber.yaml`。产物字节为命令 stdout 原样捕获
  （UTF-8 无 BOM；末尾行尾来自 `Console.WriteLine`，Windows 上为 CR LF）——锚定
  Windows 上游 stdout 语义，不做任何规范化。

## 布局与机制约束

- 宿主 exe 输出到 `openra/bin`：`Platform.BinDir` = 宿主 exe 所在目录，上游
  `ObjectCreator` 从该目录加载 manifest 声明的 `OpenRA.Mods.*.dll`——这是
  resolved-rules 模式能跑的前提。
- **构建顺序**：宿主与其加载的上游 DLL 同在 `openra/bin`，运行中的宿主锁文件，
  故宿主**不自行构建上游**（会自锁，MSB3026）；`run.cmd` 先整建上游 slnx、再建
  宿主、最后运行。直接跑 exe 时若 mod 程序集缺失，宿主报可操作错误（退出码 3）。
- 宿主收尾 `finally { Log.Dispose(); }`：上游 `Log` 静态构造启动的是**前台**线程，
  不 Dispose 进程永不退出（见 docs/upstream.md U004）。
- 黄金文件按字节入库，仓库根 `.gitattributes` 对 `tests/golden/**` 设 `-text`
  禁止 EOL 转换（`core.autocrlf=true` 环境下 checkout 改写字节会导致 verify 假失败）。

## 直接驱动 exe（run.cmd 内部即此）

```
<openra>/bin/golden-gen.exe gen|verify|check --repo-root <repo> --openra <openra>
```
