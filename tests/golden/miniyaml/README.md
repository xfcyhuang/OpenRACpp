# tests/golden/miniyaml —— MiniYaml 往返黄金语料

来源与用途见工单 #5（T04）。本目录受根 `.gitattributes` 的
`tests/golden/** -text` 保护（禁止任何换行转换，字节保真；
其中 `edge/crlf.yaml` 为刻意的 CRLF 输入）。

## ra/ —— 主线 mod 规则文件（21 个）

逐字节拷贝自上游 `mods/ra/rules/`（基线 `1f594e32065615ed1847962c80ecfe6e868a11c3`，
见根 `UPSTREAM.baseline`）。文件为 OpenRA 项目内容，GPL-3.0（本目录 `COPYING`）。

上游实测（2026-11-03，oracle 宿主）：对这 21 个文件执行
`MiniYaml.FromFile(path, discardCommentsAndWhitespace: false)` → `WriteToString()`，
产物与原文件逐字节恒等（21/21 IDENTICAL）。
**因此黄金基准即原文件本身**：C++ 侧 parse → write 产物必须与之逐字节相同，
无需 `.expected` 文件。

## tilesets/ —— 大文件压力（1 个）

`temperat.yaml` 拷自上游 `mods/ra/tilesets/`（77KB、深层嵌套），同为恒等自期望。

## edge/ —— 边界语料（27 对 `.yaml` + `.expected`）

ra 语料语法面窄（仅 TAB + key:value + 空行 + 单注释行），转义/守卫/空格缩进等
分支以 `edge/*.yaml` 输入补测。输入**不保证**往返恒等（如 4 空格缩进写出为 TAB、
`mod.yaml` 裸条目追写冒号），期望产物存于同名 `.expected`——由上游 oracle 宿主
`MiniYaml.FromFile(path, discardCommentsAndWhitespace: false)` → `WriteToString()`
（FromFile/ReadAllLinesAsMemory 路径，即 C++ 侧移植的同一语义）生成，非手写。

- `mod.yaml`、`missions.yaml`、`map-soviet-07.yaml`：拷自上游（裸条目→补 `:`、
  双空格值→单空格，非恒等）。
- `spaces-4`、`spaces-2`、`spaces-5-mixed`：空格缩进怪癖（4 空格=1 层、
  2 空格不构成层、5 空格混合）。
- `colon-no-space`、`comment-forms`、`hash-before-colon`、`escaped-hash`、
  `guarded-whitespace`：键值/注释三态/`#` 优先于 `:`/转义/守卫分支。
- `orphan-deep-first`、`orphan-level1-first`、`blank-steals-subtree`：首行深缩进
  孤儿丢弃、空行偷子树（FromFile 语义，与 FromString 幽灵行路径输出不同）。
- `crlf`、`no-final-newline`、`trailing-blanks`、`empty`、`only-newline`、
  `spaces-only-line`、`isolated-cr`、`bom`：行尾/BOM/孤立 `\r` 形态。
- `unicode-value`、`duplicate-keys`、`prefix-keys`、`comment-trailing-space`、
  `upstream-roundtrip-test`：UTF-8 透传、重复键、`-`/`@`/`^` 前缀键、
  注释尾随空格、上游 MiniYamlTest 往返用例。

错误路径（Bad indent 等）不入语料，在 `tests/miniyaml_unit.cpp` 以内联用例断言
（含消息格式 `Bad indent in miniyaml at {name}:{line}` 逐字符一致）。

## 对拍口径

C++ 侧驱动 `tests/miniyaml_roundtrip.cpp`：遍历本目录全部 `*.yaml`，
`miniyaml_from_file(path, false)` → `miniyaml_write_to_string`，与
`.expected`（存在时）或输入本身做**字节比较**（非文本行比较）。
