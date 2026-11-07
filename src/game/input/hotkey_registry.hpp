#pragma once

import std;

#include "game/input/hotkey.hpp"
#include "game/miniyaml.hpp"

namespace ora::game::input {

// 上游 HotkeyDefinition（HotkeyDefinition.cs）的注册表条目。
struct HotkeyDefinitionModel {
  std::string str_name;
  HotkeyModel hk_default{};  // Hotkey.Invalid = (UNKNOWN, None)
  std::string str_description;
  std::vector<std::string> vec_types;
  std::vector<std::string> vec_contexts;  // "hotkey-context." 前缀（上游 :51-53）
  bool b_readonly{};
  bool b_has_duplicates{};
};

// 上游 HotkeyManager（HotkeyManager.cs:18-116）。
// definitions/keys 平行数组保装载序（GetFirstDuplicate 的遍历序语义）。
struct HotkeyRegistryModel {
  std::vector<HotkeyDefinitionModel> vec_definitions;
  std::vector<HotkeyModel> vec_keys;
  // settings.yaml `Keys:` 段（shared SettingsModule）的重绑节点（name → "KEY Mod1, Mod2"），
  // 保序（上游 hotkeySettings.Yaml.Nodes 的 RemoveAll+追加语义）。持久化写盘归 T21。
  std::vector<std::pair<std::string, std::string>> vec_settings;
};

// 装载：文件节点序 = mod.yaml Hotkeys: 列表序；MiniYaml Merge 语义复用 T04。
// 定义值解析失败 = 报错拒绝装载（上游热键 yaml 路径 FieldLoader.GetValue 直接 throw）。
[[nodiscard]] std::expected<HotkeyRegistryModel, std::string> hotkey_registry_load(
    const std::vector<std::vector<MiniYamlNode>>& vec_files);

// settings `Keys:` 段覆盖（HotkeyManager ctor :36-39）：未知名跳过、Readonly 跳过、
// 坏值 = 报错拒绝装载（D059 修正：上游此解析在 InvalidValueAction 替换窗口外，
// 默认委托 throw YamlException → ModData ctor 炸 → 启动失败，t21 probe 6 实证）。
[[nodiscard]] std::expected<void, std::string> hotkey_registry_apply_settings(
    HotkeyRegistryModel& reg, std::span<const MiniYamlNode> vec_keys_nodes);

[[nodiscard]] std::optional<std::size_t> hotkey_registry_find(const HotkeyRegistryModel& reg,
                                                              std::string_view str_name);

// 上游 GetHotkeyReference（:50-61）：有定义 → 当前值活查询；否则按字面
// Hotkey.TryParse(name)；再失败 → Hotkey.Invalid。
[[nodiscard]] HotkeyModel hotkey_resolve(const HotkeyRegistryModel& reg, std::string_view str_name);

// 上游 Set（:63-89）：定义缺失/Readonly → false 不动；否则更新当前值 + settings
// 节点 RemoveAll + `value != Default`（hotkey_equal 的 UNKNOWN 永不等语义）时追加 +
// 全表冲突重算（上游为条件重算，见 COVERAGE.md 偏离登记）。
bool hotkey_set(HotkeyRegistryModel& reg, std::string_view str_name, const HotkeyModel& hk);

// 上游 GetFirstDuplicate（:91-106）：装载序遍历 keys，hotkey_equal 且 Contexts 相交。
[[nodiscard]] std::optional<std::size_t> hotkey_get_first_duplicate(const HotkeyRegistryModel& reg,
                                                                    std::size_t idx_definition);

void hotkey_registry_recompute_duplicates(HotkeyRegistryModel& reg);

}
