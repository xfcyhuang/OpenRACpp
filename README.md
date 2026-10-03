# OpenRACpp

**中文** | [English](#english)

## 简介

[OpenRA](https://github.com/OpenRA/OpenRA) 游戏引擎的 **C++26 语义重写**。

按 OpenRA 上游源码（基线 commit `7d57605bca`）的代码语义完全重写，数据、地图与回放格式兼容上游。

## 状态

**Phase 1 完成**(MiniYaml + 文件系统;2026-10-03):

- `src/yaml/`:上游 MiniYaml.cs(791 行)逐语义重写——行状态机(4 空格/1 tab 层级、`\#` 转义、`\ ` 空白守护)、字符串池 interning、`Merge`/继承解析/`-Key` 弱删除、规范化序列化、可变 Builder;`src/core/text.hpp` 复刻 .NET `char.IsWhiteSpace`/`Trim` 的 UTF-8 语义
- `src/fs/`:FileSystem(挂载顺序 = 覆盖优先级、`'|'` 显式挂载、大小写规整解析)、Folder(读写)、ZipFile(只读,SharpZipLib→**miniz**,含子目录视图与嵌套 zip)、`MiniYaml::Load`
- **第一关卡验收:上游 mods/ 全部 759 个 yaml × 双模式(丢弃/保留注释),`FromStream→WriteToString` 输出与 C# oracle 逐字节 100% 一致**(13.5MB 黄金数据,`tests/golden_yaml.txt`);上游 MiniYamlTest.cs 28 用例断言文本逐字移植全绿(测试字面量从上游源码按字节程序化提取);fs_test 24 断言全绿
- 工程门禁:`import std;` 严控(禁传统 std 头引入,`tools/std_import_check.py` 强制)、函数级裁剪(`-ffunction-sections -fdata-sections` + `--gc-sections`)、UPSTREAM 溯源标注(`tools/upstream_check.py`,23 条)、全代码中英双语注释
- 双构建(ASan+UBSan 与 Release)ctest 5/5 全绿;偏离 9 项登记于 docs/COVERAGE.md

**Phase 0 完成**(C++26 工程骨架 + 定点原语):

- 工程基线:clang `-std=c++26` + `import std;`(std.cppm 预编译 PCM)+ CMake/Ninja + ctest;确定性选项 `-fwrapv -fno-strict-aliasing`
- `src/core/` 12 个头文件:定点原语(WPos/WVec/WAngle/WRot/WDist/CPos/CVec/MPos/int2/Rectangle/Int32Matrix4x4/ISqrt)与 MersenneTwister,全部按上游源码逐语义重写,`constexpr` 全面化,文件头带 `// UPSTREAM:` 溯源标注
- 验收:与 C# 版黄金数据 **60,883 行逐行对拍 100% 一致**(覆盖全角度三角学、ArcSin/ArcCos 全量、MT19937 全序列、LerpQuadratic 的 decimal 截断语义等),ASan+UBSan 与 Release 双构建全绿
- 上游同步机制就位:`UPSTREAM.baseline`(基线 commit `7d57605bca`)+ `docs/COVERAGE.md` 覆盖登记 + `tools/upstream_check.py` 校验器

下一阶段(Phase 2):元数据框架 + 数据加载链(schema_dumper → gen/ 描述表、FieldLoader、Manifest/Ruleset 加载,三 mod resolved-rules 对拍)。

## 许可证与归属

本项目是 OpenRA 的衍生作品，按上游许可证以 **GPL-3.0** 发布，见 [LICENSE](LICENSE)。

- 上游版权所有：Copyright (c) OpenRA Developers and Contributors
- 本项目的重写代码：Copyright (c) 2026 xfcyhuang

OpenRA、Command & Conquer、Red Alert 及 Dune 2000 相关商标归各自权利人所有，本项目与这些权利人无从属关系。

---

<a name="english"></a>

**[中文](#opencpp-1)** | English

## Overview

A **C++26 semantic rewrite** of the [OpenRA](https://github.com/OpenRA/OpenRA) game engine.

The engine is rewritten from scratch to match the exact semantics of the upstream OpenRA source code (baseline commit `7d57605bca`), while keeping data, map, and replay formats compatible with upstream.

## Status

**Phase 1 complete** (MiniYaml + file system; 2026-10-03):

- `src/yaml/`: a statement-by-statement rewrite of upstream MiniYaml.cs (791 lines) — the line state machine (4-space/1-tab levels, `\#` escaping, `\ ` whitespace guards), string-pool interning, merge/inheritance resolution/`-Key` weak removals, normalized serialization, and the mutable builders; `src/core/text.hpp` replicates the .NET `char.IsWhiteSpace`/`Trim` semantics over UTF-8
- `src/fs/`: FileSystem (mount order as override priority, `'|'` explicit mounts, case-insensitive path resolution), Folder (read/write), ZipFile (read-only, SharpZipLib→**miniz**, with the subfolder view and nested zips), and `MiniYaml::Load`
- **First-gate acceptance: all 759 upstream mods yaml files × both modes (discard/keep comments) match the C# oracle byte-for-byte on `FromStream→WriteToString` output** (13.5 MB golden data in `tests/golden_yaml.txt`); all 28 upstream MiniYamlTest.cs cases ported with verbatim assertion texts (test literals extracted byte-exactly from the upstream source); fs_test's 24 assertions all green
- Project gates: strict `import std;` enforcement (no classic std-header includes, forced by `tools/std_import_check.py`), function-level dead-code elimination (`-ffunction-sections -fdata-sections` + `--gc-sections`), UPSTREAM provenance tags (`tools/upstream_check.py`, 23 tags), bilingual (Chinese/English) comments throughout
- Both build flavors (ASan+UBSan and Release) pass ctest 5/5; 9 registered deviations in docs/COVERAGE.md

**Phase 0 complete** (C++26 skeleton + fixed-point primitives):

- Toolchain baseline: clang `-std=c++26` + `import std;` (precompiled std.cppm PCM) + CMake/Ninja + ctest; determinism flags `-fwrapv -fno-strict-aliasing`
- 12 headers in `src/core/`: fixed-point primitives (WPos/WVec/WAngle/WRot/WDist/CPos/CVec/MPos/int2/Rectangle/Int32Matrix4x4/ISqrt) and MersenneTwister, rewritten statement-by-statement from upstream sources, fully `constexpr`, each file carrying a `// UPSTREAM:` provenance tag
- Acceptance: **60,883 lines of golden differential testing match the C# output 100%** (covering full-circle trigonometry, exhaustive ArcSin/ArcCos, full MT19937 sequences, and the decimal truncation semantics of LerpQuadratic); clean under both ASan+UBSan and Release builds
- Upstream sync mechanism in place: `UPSTREAM.baseline` (commit `7d57605bca`), coverage registry in `docs/COVERAGE.md`, and the `tools/upstream_check.py` validator

Next up (Phase 2): the metadata framework + data-loading chain (schema_dumper → gen/ descriptor tables, FieldLoader, Manifest/Ruleset loading, and the three-mod resolved-rules comparison).

## License & Attribution

This project is a derivative work of OpenRA and is released under the **GPL-3.0**, the same license as upstream. See [LICENSE](LICENSE).

- Upstream copyright: Copyright (c) OpenRA Developers and Contributors
- Rewrite code in this project: Copyright (c) 2026 xfcyhuang

OpenRA, Command & Conquer, Red Alert, and Dune 2000 are trademarks of their respective owners; this project is not affiliated with them.
