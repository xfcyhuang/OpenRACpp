#pragma once

import std;

#include "game/miniyaml.hpp"
#include "game/miniyaml_merge.hpp"

namespace ora::game {

// 上游 Manifest.Rules 提取（Manifest.cs:97-124 + YamlList:190-196）：
// FromFile(mod.yaml, discard=true) → Merge([nodes]) → 查 "Rules" 键 → 子节点键
// 序列（保 Nodes 顺序；区块缺失 → 空列表）。Include 内联展开（Manifest.cs:98-111）
// 不实现——ra 的 mod.yaml 无 Include，偏离登记 docs/COVERAGE.md。
[[nodiscard]] std::expected<std::vector<std::string>, std::string> manifest_rules_files(
    std::string_view str_mod_yaml_path);

// 上游 Ruleset.LoadDefaults（Ruleset.cs:118-164）最小面对象模型：
// Merge 之后过滤 '^' 抽象前缀 → 小写化冲突检测 → 每 actor 取子节点完整键序列。
// trait 完整键（含 @suffix）在合并树内唯一且顺序 = 继承展开序；@ 后缀多实例
// 是正常形态（上游 InstanceName 分裂 / FieldLoader 反射装配留给后续 trait 语义票）。
struct ActorRules {
  std::string str_name;                     // 原大小写
  std::vector<std::string> vec_trait_keys;  // 完整键（含 @suffix），合并树序
};

struct RulesetModel {
  std::vector<ActorRules> vec_actors;  // 非 ^ actor，首现序
};

// 逐文件解析 + 合并 + 装配。vec_manifest_entries 每项 =
// (Location 名如 "ra|rules/misc.yaml"，物理路径)；Location 名进 SourceLocation
//（与上游 MiniYaml.Load 的 FromStream(stream, manifest 条目) 一致）。
// 小写化冲突 → 错误（上游 ToDictionaryWithConflictLog 消息为 .NET 集合拼接，
// 近似复刻，ra 语料 0 触发，登记 docs/COVERAGE.md）。
[[nodiscard]] std::expected<RulesetModel, std::string> ruleset_load_minimal(
    const std::vector<std::pair<std::string, std::string>>& vec_manifest_entries);

}  // namespace ora::game
