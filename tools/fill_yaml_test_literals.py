#!/usr/bin/env python3
"""fill_yaml_test_literals — 将上游 MiniYamlTest.cs 提取的字面量填入
tests/mini_yaml_test.cpp 的 {{函数.常量}} 占位符(生成 C++ 转义形式)。

Usage:
  python tools/fill_yaml_test_literals.py <literals.json> <mini_yaml_test.cpp>
"""
import json
import sys


def cpp_escape(s: str) -> str:
    out = []
    for c in s:
        if c == '\n':
            out.append('\\n')
        elif c == '\t':
            out.append('\\t')
        elif c == '"':
            out.append('\\"')
        elif c == '\\':
            out.append('\\\\')
        else:
            out.append(c)
    return '"' + ''.join(out) + '"'


def main() -> int:
    literals = json.load(open(sys.argv[1], encoding='utf-8'))
    path = sys.argv[2]
    src = open(path, encoding='utf-8').read()

    import re
    def sub(m):
        key = m.group(1)
        if key not in literals:
            raise SystemExit(f'占位符无对应字面量: {key}')
        return cpp_escape(literals[key])

    # 仅匹配 ASCII 标识符占位符(文件头注释中的 {{函数.常量}} 说明不参与替换)
    filled = re.sub(r'\{\{([A-Za-z0-9_.]+)\}\}', sub, src)
    open(path, 'w', encoding='utf-8', newline='\n').write(filled)
    print(f'filled {path}')
    return 0


if __name__ == '__main__':
    sys.exit(main())
