#!/usr/bin/env python3
"""extract_yaml_test_literals — 按"函数名.常量名"从上游 MiniYamlTest.cs 提取字节精确的字面量。

Usage:
  python tools/extract_yaml_test_literals.py <MiniYamlTest.cs> <输出.json>
  (可再配合 --cpp 生成 C++ 转义字面量,供 tests/mini_yaml_test.cpp 模板替换)
"""
import json
import re
import sys


def unescape_cs(lit: str) -> str:
    out = []
    i = 0
    while i < len(lit):
        c = lit[i]
        if c == '\\' and i + 1 < len(lit):
            n = lit[i + 1]
            if n == 'n':
                out.append('\n')
            elif n == 't':
                out.append('\t')
            elif n == '\\':
                out.append('\\')
            elif n == '"':
                out.append('"')
            else:
                out.append(n)
            i += 2
        else:
            out.append(c)
            i += 1
    return ''.join(out)


def main() -> int:
    src = open(sys.argv[1], encoding='utf-8').read().replace('\r\n', '\n')
    result = {}
    for m in re.finditer(r'public void (\w+)\(\)(.*?)\n\t\t\}', src, re.S):
        fname, body = m.group(1), m.group(2)
        for sm in re.finditer(
                r'(?:const string|var) (\w+)\s*=\s*(@?"(?:[^"\\]|\\.)*")'
                r'(?:\.Replace\("\\r\\n",\s*"\\n"\))?\s*;', body):
            name, lit = sm.group(1), sm.group(2)
            val = lit[2:-1] if lit.startswith('@') else unescape_cs(lit[1:-1])
            result[f'{fname}.{name}'] = val
    json.dump(result, open(sys.argv[2], 'w', encoding='utf-8'), ensure_ascii=False)
    print(f'extracted {len(result)} literals -> {sys.argv[2]}')
    return 0


if __name__ == '__main__':
    sys.exit(main())
