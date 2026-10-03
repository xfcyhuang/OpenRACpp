# OpenRACpp

**中文** | [English](#english)

## 简介

[OpenRA](https://github.com/OpenRA/OpenRA) 游戏引擎的 **C++26 语义重写**。

按 OpenRA 上游源码（基线 commit `7d57605bca`）的代码语义完全重写，数据、地图与回放格式兼容上游。

## 状态

**Phase 0 完成**（C++26 工程骨架 + 定点原语）：

- 工程基线：clang `-std=c++26` + `import std;`（std.cppm 预编译 PCM）+ CMake/Ninja + ctest；确定性选项 `-fwrapv -fno-strict-aliasing`
- `src/core/` 12 个头文件：定点原语（WPos/WVec/WAngle/WRot/WDist/CPos/CVec/MPos/int2/Rectangle/Int32Matrix4x4/ISqrt）与 MersenneTwister，全部按上游源码逐语义重写，`constexpr` 全面化，文件头带 `// UPSTREAM:` 溯源标注
- 验收：与 C# 版黄金数据 **60,883 行逐行对拍 100% 一致**（覆盖全角度三角学、ArcSin/ArcCos 全量、MT19937 全序列、LerpQuadratic 的 decimal 截断语义等），ASan+UBSan 与 Release 双构建全绿
- 上游同步机制就位：`UPSTREAM.baseline`（基线 commit `7d57605bca`）+ `docs/COVERAGE.md` 覆盖登记 + `tools/upstream_check.py` 校验器

下一阶段（Phase 1）：MiniYaml 解析与文件系统——759 个上游 yaml 逐文件对拍的第一关卡。

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

**Phase 0 complete** (C++26 skeleton + fixed-point primitives):

- Toolchain baseline: clang `-std=c++26` + `import std;` (precompiled std.cppm PCM) + CMake/Ninja + ctest; determinism flags `-fwrapv -fno-strict-aliasing`
- 12 headers in `src/core/`: fixed-point primitives (WPos/WVec/WAngle/WRot/WDist/CPos/CVec/MPos/int2/Rectangle/Int32Matrix4x4/ISqrt) and MersenneTwister, rewritten statement-by-statement from upstream sources, fully `constexpr`, each file carrying a `// UPSTREAM:` provenance tag
- Acceptance: **60,883 lines of golden differential testing match the C# output 100%** (covering full-circle trigonometry, exhaustive ArcSin/ArcCos, full MT19937 sequences, and the decimal truncation semantics of LerpQuadratic); clean under both ASan+UBSan and Release builds
- Upstream sync mechanism in place: `UPSTREAM.baseline` (commit `7d57605bca`), coverage registry in `docs/COVERAGE.md`, and the `tools/upstream_check.py` validator

Next up (Phase 1): MiniYaml parser and file system — the first milestone gated by byte-exact comparison across all 759 upstream yaml files.

## License & Attribution

This project is a derivative work of OpenRA and is released under the **GPL-3.0**, the same license as upstream. See [LICENSE](LICENSE).

- Upstream copyright: Copyright (c) OpenRA Developers and Contributors
- Rewrite code in this project: Copyright (c) 2026 xfcyhuang

OpenRA, Command & Conquer, Red Alert, and Dune 2000 are trademarks of their respective owners; this project is not affiliated with them.
