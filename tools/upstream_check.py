#!/usr/bin/env python3
"""upstream_check — 上游覆盖登记校验器(PORTING_PLAN §7.2/§7.5)

扫描 src/ 全部 .hpp/.cpp 的 `// UPSTREAM: <上游路径> [@commit] [L行区间]` 标注,核对:
  1. 标注里的 commit 与 UPSTREAM.baseline 一致(防止跨基线混写);
  2. 标注的上游文件在 ../OpenRA 中真实存在;
  3. 输出 上游文件 → C++ 文件 映射矩阵(覆盖登记表的数据源)。

用法: python tools/upstream_check.py [--upstream <OpenRA 路径>]
退出码: 0 = 校验通过;1 = 存在未标注/失配项。
"""
from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

# 标注语法:// UPSTREAM: OpenRA.Game/WPos.cs @7d57605 L20-79(说明)
TAG_RE = re.compile(
    r"^\s*//\s*UPSTREAM:\s*(?P<path>\S+)\s*(?:@(?P<commit>[0-9a-f]{7,40}))?\s*(?P<rest>.*)$"
)
LINE_RE = re.compile(r"L(?P<lo>\d+)(?:-(?P<hi>\d+))?")


def read_baseline(repo_root: Path) -> str:
    """读 UPSTREAM.baseline 的完整 commit。"""
    baseline = repo_root / "UPSTREAM.baseline"
    if not baseline.exists():
        raise SystemExit(f"错误: 缺少 {baseline}")
    for line in baseline.read_text(encoding="utf-8").splitlines():
        line = line.strip()
        if line.startswith("#") or "=" not in line:
            continue
        key, value = line.split("=", 1)
        if key.strip() == "commit":
            return value.strip()
    raise SystemExit("错误: UPSTREAM.baseline 中无 commit= 记录")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--upstream", default=None, help="OpenRA 上游仓库路径(默认: <本仓库>/../OpenRA)")
    args = parser.parse_args()

    repo_root = Path(__file__).resolve().parent.parent
    upstream = Path(args.upstream) if args.upstream else repo_root.parent / "OpenRA"
    baseline_commit = read_baseline(repo_root)

    src_dir = repo_root / "src"
    if not src_dir.exists():
        raise SystemExit("错误: src/ 不存在")

    mapping: list[tuple[str, str, str]] = []  # (上游文件, 标注行, C++ 文件)
    untagged: list[str] = []
    errors: list[str] = []

    for cpp_file in sorted(src_dir.rglob("*")):
        if cpp_file.suffix not in (".hpp", ".cpp"):
            continue
        rel = cpp_file.relative_to(repo_root).as_posix()
        found = False
        for lineno, line in enumerate(cpp_file.read_text(encoding="utf-8").splitlines(), 1):
            m = TAG_RE.match(line)
            if not m:
                continue
            found = True
            up_path, commit, rest = m.group("path"), m.group("commit"), m.group("rest")
            if commit and not baseline_commit.startswith(commit):
                errors.append(f"{rel}:{lineno} 标注 commit {commit} ≠ 基线 {baseline_commit[:10]}")
            target = upstream / up_path
            if not target.exists():
                errors.append(f"{rel}:{lineno} 上游文件不存在: {up_path}")
            mapping.append((up_path, rest.strip(), rel))
        if not found:
            untagged.append(rel)

    print(f"基线 commit: {baseline_commit[:10]}  上游仓库: {upstream}")
    print(f"\n{'上游文件':<58} C++ 文件")
    print("-" * 100)
    for up_path, _rest, rel in mapping:
        print(f"{up_path:<58} {rel}")

    if untagged:
        print("\n未标注 UPSTREAM 的文件(违反 §7.2 强制规则):")
        for rel in untagged:
            print(f"  {rel}")
    if errors:
        print("\n校验失败项:")
        for err in errors:
            print(f"  {err}")

    if errors or untagged:
        print(f"\n结果: FAIL({len(errors)} 失配,{len(untagged)} 未标注)")
        return 1
    print(f"\n结果: PASS({len(mapping)} 条标注全部有效)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
