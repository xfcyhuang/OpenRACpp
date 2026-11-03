# COVERAGE —— 偏离登记表

有意不与上游语义等价之处登记于此；**未登记的偏离视为 bug**（见 [ADR-0001](adr/0001-semantic-equivalence-first.md)）。对 `cpp26.md` 的规范例外（如 [ADR-0003](adr/0003-bare-ctest-no-framework.md) 裸 ctest）亦在此留痕。

| 编号 | 日期 | 位置（程序集/文件） | 偏离内容 | 原因 | 验证方式 | 恢复/回收计划 |
|------|------|---------------------|----------|------|----------|----------------|
| D001 | 2026-11-03 | CMakeLists.txt（hardened 编排） | `-pie` 链接旗标换为 MinGW 等价集 `-Wl,--dynamicbase -Wl,--nxcompat -Wl,--high-entropy-va` | clang 23 对 mingw PE 目标报 `-pie` unused 告警且不传导给 ld.lld；cpp26.md §一·五注记指定该等价集 | hardened 编排 ctest 全绿 + 链接行旗标核验 | 工具链支持 `-pie` 无告警时回收 |
| D002 | 2026-11-03 | src/CMakeLists.txt（ora_std 目标） | std.cppm 目标单独加 `-Wno-reserved-module-identifier` | 用户侧 `export module std;` 触发 clang 23 保留名告警；std 垫片是该保留名的唯一合法用例，零告警验收所必需 | 三编排构建零告警 | 换用 libc++ 官方 std 模块或告警语义变化时回收 |
| D003 | 2026-11-03 | src/game/miniyaml.{hpp,cpp} | 解析错误 `YamlException("Bad indent in miniyaml at {name}:{line}")` 改为 `std::expected<T, std::string>` 错误返回（错误串逐字符复刻上游消息）；文件打不开同样走 expected 而非异常 | cpp26.md §三禁异常；数据错误需可恢复传播给调用方 | tests/miniyaml_unit.cpp 断言错误串与 `Bad indent in miniyaml at test-filename:2` 逐字符一致 | 无（expected 即终态） |
| D004 | 2026-11-03 | src/game/miniyaml.cpp（trim_ascii） | 键/值 trim 按 ASCII 空白集 {0x09-0x0D, 0x20} 实现，C# `string.Trim()` 是 Unicode 空白集 | 黄金语料全为纯 ASCII（含 UTF-8 合法多字节但两侧无 Unicode 空白），两者零偏差；字节级透传避免引入 Unicode 解码 | 黄金对拍 49 文件字节恒等（含 unicode-value.yaml） | 语料引入 U+00A0/NEL 等 Unicode 空白时按需扩展 |
| D005 | 2026-11-03 | src/game/miniyaml.cpp（miniyaml_from_file） | 非 UTF-8 字节按字节透传；上游 StreamReader 以 U+FFFD 替换非法字节 | 黄金语料全为合法 UTF-8/ASCII，行为零偏差；解析按字节扫描与 UTF-8 自同步性等价 | 黄金对拍全绿 | 语料引入非法编码时再评估替换语义 |
| D006 | 2026-11-03 | src/game/miniyaml.hpp | `MiniYamlNode{key,value,comment,location,children}` 单层聚合 struct，替代上游 `MiniYamlNode`+`MiniYaml(Value,Nodes)` 二层不可变模型；comment 用 `optional<string>`（nullopt=无 `#`、""=裸 `#`）三态对应上游 null/""/span 怪癖 | 上游二层模型服务于不可变性与 stringPool 内存优化，与往返语义无关（U003）；单层聚合符合 cpp26.md §二自由函数+聚合 | 黄金对拍 49 文件字节恒等 | 后续工单需要 Builder/不可变镜像时再分层 |
| D007 | 2026-11-03 | src/game/miniyaml.cpp（split_lines） | `miniyaml_from_string` 采用 FromFile/ReadAllLinesAsMemory 行切分语义（无尾部幽灵空行），不移植 `FromString` 的 `Split(["
","
"])` 尾部空串分歧 | 工单 T04 钉死 FromFile 语义（生产路径 `MiniYaml.Load` 同此）；对首行无缩进的文本两者输出一致 | 黄金对拍（edge 语料全部经 oracle FromFile 路径生成 .expected）与单元测试 orphan 用例双重验证 | 无（有意选择） |
