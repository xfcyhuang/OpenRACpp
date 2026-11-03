#pragma once

import std;

#include "game/miniyaml.hpp"

namespace ora::game {

// 上游 MiniYaml.Merge（OpenRA.Game/MiniYaml.cs:408-435）语义等价移植。
// sources = 逐文件（discard=true）解析出的顶层节点列表，顺序 = 加载清单序
// （保序是字节稳定生命线：输出顺序 = 清单序 + 首现序，禁 map 遍历序）。
// 五组件语义：
//   1. MergeSelfPartial：文件内同层重复键合并（子层走 MergePartial，含弱删除）
//   2. MergePartial（集合级）：按清单序跨文件覆盖（`-K` 透传、removal 更近时
//      append 不合并、同键原位深层合并）
//   3. MergePartial（MiniYaml 级）：两侧 WeakResolveRemovals（同侧弱删除，
//      删除标记本身被消费不透传）+ 同侧重复键检查 + override 非空值胜出
//   4. ResolveInherits：Inherits/Inherits@x 继承展开（父名 = 节点值）+ `-Key`
//      强删除（完整键匹配，含 @ 后缀）+ 循环检测；与合并递归交织
//   5. 顶层删除解析（顶层 `-Key` 键最后一步消费）
// 数据错误（同侧重复键 / 父缺失 / 循环继承 / 无元素可删 / 顶层重复键）→
// expected 错误串，消息复刻上游 YamlException 格式（D004 口径：异常→expected）。
[[nodiscard]] std::expected<std::vector<MiniYamlNode>, std::string> miniyaml_merge(
    const std::vector<std::vector<MiniYamlNode>>& vec_sources);

}  // namespace ora::game
