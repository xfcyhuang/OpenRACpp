#!/usr/bin/env python3
"""std_import_check — `import std;` 严控门禁(PORTING_PLAN §4.4)

扫描 src/ 与 tests/ 的 .hpp/.cpp:标准库引入只允许 `import std;`,
禁止 `#include <标准头>`。白名单仅放行宏无法穿越 import 边界的 C 头与
第三方 C 头(规范:平台/第三方差异只允许出现在包装头文件)。

白名单(可扩展,须注明理由):
  <cassert>   assert 宏(规范允许的 import/#include 并用正形态)
  <cstdio>    stderr 宏(同上)
  <miniz.h>   第三方 C 库(third_party/miniz)

用法: python tools/std_import_check.py [--dirs src tests]
退出码: 0 = 通过;1 = 存在违规。
"""
from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

ALLOWED = {
    "cassert": "assert 宏 / assert macro",
    "cstdio": "stderr 宏 / stderr macro",
    "miniz.h": "第三方 C 库 / third-party C library",
}

INCLUDE_RE = re.compile(r'^\s*#\s*include\s*<([^>]+)>')


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--dirs", nargs="+", default=["src", "tests"])
    args = parser.parse_args()

    repo_root = Path(__file__).resolve().parent.parent
    violations: list[str] = []
    scanned = 0
    for d in args.dirs:
        for f in sorted((repo_root / d).rglob("*")):
            if f.suffix not in (".hpp", ".cpp"):
                continue
            scanned += 1
            rel = f.relative_to(repo_root).as_posix()
            for lineno, line in enumerate(f.read_text(encoding="utf-8").splitlines(), 1):
                m = INCLUDE_RE.match(line)
                if not m:
                    continue
                header = m.group(1)
                if header not in ALLOWED:
                    violations.append(f"{rel}:{lineno}: #include <{header}>")

    if violations:
        print("import std; 门禁违规(标准头必须经 import std 引入):")
        for v in violations:
            print(f"  {v}")
        print(f"\n结果: FAIL({len(violations)} 处,{scanned} 个文件已扫描)")
        return 1
    print(f"结果: PASS({scanned} 个文件,标准库引入全部走 import std;白名单:"
          f"{', '.join(sorted(ALLOWED))})")
    return 0


if __name__ == "__main__":
    sys.exit(main())
