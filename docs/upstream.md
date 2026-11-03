# upstream —— 上游注记表

对上游（基线见 [UPSTREAM.baseline](../UPSTREAM.baseline)）实现的调研结论、移植注意点与坑位登记于此，供后续阶段回查。

| 编号 | 日期 | 上游位置 | 注记 | 影响面（C++ 侧位置） |
|------|------|----------|------|----------------------|
| U001 | 2026-11-03 | OpenRA.Game/Support/MersenneTwister.cs:35-39 | `Next()` 的 XML 注释声称返回值介于 -0x7fffffff..0x7fffffff 且 0 双倍概率——**陈旧注释与代码不符**：`Last = (int)(y % int.MaxValue)` 中 C# `uint % int` 提升为 long 运算，结果恒非负 ∈ [0, 2147483646]；黄金数据 7 种子 × 200 值零负值实证 | src/game/support/mersenne_twister.cpp（`next_uint` 的 last 赋值，按代码而非注释移植）；tests/prng.cpp 的 next kind 对拍含非负断言 |
| U002 | 2026-11-03 | OpenRA.Game/Support/MersenneTwister.cs:77-79 | `Next(low, high)` 在 `diff <= 1` 时**不消耗任何随机数**直接返回 `low`（含 `Next(1)`、`Next(0)`、`Next(3,3)` 及 `diff` int 溢出回绕为负的病态情形）；上游大量 `Next(1)`/`Next(0, count)` 空列表调用依赖该短路保持序列对齐 | src/game/support/mersenne_twister.cpp（`next` 双参重载）；tests/prng.cpp 短路契约检查（total_count 零消耗断言，含 int.MinValue/int.MaxValue 回绕路径） |
| U003 | 2026-11-03 | OpenRA.Game/Support/MersenneTwister.cs:57-60 | `NextUlong()` 依赖 C# 左结合求值序：第一个 `NextUint()` 结果是**高 32 位**、第二个是低 32 位；C++ `|` 两操作数不定序，必须手工分步取值再拼接 | src/game/support/mersenne_twister.cpp（`next_ulong` 分步 hi/lo）；tests/prng.cpp 的 u64 kind 对拍 |
| U004 | 2026-11-03 | OpenRA.Game/MiniYaml.cs L342（FromLines） | 空注释三态依赖 `ReadOnlySpan<char>` 上 `==` 的引用比较怪癖：行尾空切片（`key:#`）与 `ReadOnlySpan<char>.Empty` 比较为 false，得到 `Comment==""`；未赋值 default 比较为 true，得到 null。即"有 `#` 无内容"是空串而非无注释 | src/game/miniyaml.cpp（opt_comment 三态，comment-forms 语料） |
| U005 | 2026-11-03 | OpenRA.Game/MiniYaml.cs L235-262（缩进扫描） | 4 个空格=1 层、1 个 TAB=1 层、不满 4 的零散空格不构成层（2 空格缩进=同层、5 空格=1 层）；首行任意层合法；无浅行领养的深层节点静默丢弃（孤儿）；空行（null key）重置缩进上下文，其后跳深 >1 同样 Bad indent | src/game/miniyaml.cpp（parse_line/build_completed_subnode，spaces-*/orphan-* 语料与单元断言） |
| U006 | 2026-11-03 | OpenRA.Game/MiniYaml.cs L264-318（键值提取） | 第一个未转义 `#` 优先于第一个 `:`：`#` 之后整行皆注释（内含冒号也不再切分）；只有值做 `\#`↔`#` 转义（键、注释不转义）；值守卫反斜杠（首/尾 `\ `）解析时剥掉但写出不恢复——值首尾有真实空白的输入二次解析失稳，oracle 产物仍恒等，勿"修复" | src/game/miniyaml.cpp（hash-before-colon/escaped-hash/guarded-whitespace 语料） |
| U007 | 2026-11-03 | OpenRA.Game/MiniYaml.cs L403-406 vs StreamExts.cs L200-244 | `FromString` 的 `Split(["\r\n","\n"])` 对尾部换行文本产生幽灵空行（level 0 null 节点，可把首行缩进内容包成其子树）；`FromFile` 的 `ReadAllLinesAsMemory` 无幽灵行且剥 `\n` 前紧邻 `\r`、剥离 BOM。生产路径 `MiniYaml.Load` 走 FromFile；C++ 统一移植 FromFile 语义（见 D008） | src/game/miniyaml.cpp（split_lines） |
| U008 | 2026-11-03 | OpenRA.Game/MiniYaml.cs L24-33 | `WriteToFile`（逐行 `TrimEnd()` + `File.WriteAllLines`，Windows 产出 CRLF）与 `WriteToString`（`JoinWith("\n").TrimEnd('\n')+"\n"`，LF、剥全部尾 `\n` 补一、不逐行 Trim）是两种口径；对拍一律用 WriteToString | src/game/miniyaml.cpp（miniyaml_write_to_string） |
| U009 | 2026-11-03 | OpenRA.Game/MiniYaml.cs L321-327 | `discardCommentsAndWhitespace=true`（默认）丢弃所有无 Key 行（空行/纯注释行）且不提取注释；false 时保留为 null-Key 节点（空行输出空串行、缩进注释行按其层缩进），空行/注释行照常计入行号 | src/game/miniyaml.cpp（miniyaml_from_string 的行取舍，单元 discard 断言） |
| U010 | 2026-11-03 | OpenRA.Game/MiniYaml.cs L360-390（BuildCompletedSubNode） | 两阶段折叠：深一层的已完成块挂到同层**最后**一行（非前一兄弟）；层结果列表折叠后清空复用（子块只被最近的同层前一行领养） | src/game/miniyaml.cpp（build_completed_subnode，upstream-roundtrip-test 语料） |
| U011 | 2026-11-03 | mods/ra/rules/*（21 文件） | ra 规则全集 parse(discard=false)→WriteToString 与原文件逐字节恒等（oracle check 21/21 IDENTICAL，FromString/FromFile 两路一致）——黄金期望即原文件本身；`mod.yaml`/`missions.yaml` 的裸条目（无冒号行）回写追写 `:`，是天然非恒等边界语料 | tests/golden/miniyaml/（ra/ 自期望、edge/ 带 .expected） |
