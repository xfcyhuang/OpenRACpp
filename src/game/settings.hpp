// UPSTREAM: OpenRA.Game/Settings.cs @b6fc03f L52-340 的起步承载面(
//          SettingsModule 的 Commit 差异保存协议 + PlayerSettings/
//          GameSettings 两段手写字段表;其余段与 UI 绑定面随 Phase 6/7)
//          The starter face of Settings.cs L52-340 (SettingsModule's
//          save-differences Commit protocol + the hand-written field
//          tables of PlayerSettings/GameSettings; the remaining sections
//          and the UI bindings ride Phase 6/7).
//
// 机制对照 / Mechanism mapping:
//  - Settings 具体类的反射字段 → 手写 RecordDesc 描述表(值袋承载,
//    GeneratedRecord;schema_dumper 的 Settings 扩展随需再生成 —— 登记)
//    The concrete Settings classes' reflective fields → the hand-written
//    RecordDesc tables (a value-bag GeneratedRecord; schema_dumper's
//    Settings export regenerates on demand — registered).
//  - Commit 的"默认值不存 + 擦旧再增"(L69-80)→ 差异节点重建(等价:
//    节点集 = 差异集;键序 = 字段序)
//    Commit's "defaults unsaved + remove-then-add" (L69-80) → the
//    difference-node rebuild (equivalent: the node set = the difference
//    set; the key order = the field order).
#pragma once
import std;

#include "meta/generic_record.hpp"
#include "yaml/mini_yaml.hpp"

namespace ora::game {

/// 一个设置段(PlayerSettings/GameSettings/…):键 + 值袋 + 默认快照
/// One settings section (PlayerSettings/GameSettings/...): the key + the
/// value bag + the default snapshot.
class SettingsModule {
 public:
  SettingsModule(const meta::RecordDesc& desc, std::string str_key,
                 bool b_shared);

  /// FieldLoader.Load(module, yaml)的值袋等价:差异节点逐字段写槽
  /// The value-bag equivalent of FieldLoader.Load (the section nodes write
  /// slots field by field).
  void Load(const yaml::MiniYaml& yaml_section);

  /// Commit(L69-80):默认值字段不存;自定义值重建节点(键序 = 字段序)
  /// Commit (L69-80): default-valued fields stay unsaved; custom values
  /// rebuild the nodes (the key order = the field order).
  std::vector<yaml::MiniYamlNode> Commit(yaml::StringPool& pool) const;

  const meta::GeneratedRecord& Record() const { return *up_record_; }
  meta::GeneratedRecord& Record() { return *up_record_; }
  const std::string& Key() const { return str_key_; }
  bool Shared() const { return b_shared_; }

 private:
  std::string str_key_;
  bool b_shared_ = true;
  std::unique_ptr<meta::GeneratedRecord> up_record_;
  std::unique_ptr<meta::GeneratedRecord> up_defaults_;
};

/// PlayerSettings/GameSettings 的手写描述表(Settings.cs L309-340/L319-…)
/// The hand-written descriptors of PlayerSettings/GameSettings (Settings.cs
/// L309-340/L319-...).
const meta::RecordDesc* SettingsDescOf(std::string_view str_section);

/// 设置承载(段表 + Save 协议;文件 IO 面随 Game 装配)
/// The settings carrier (the section table + the Save protocol; the file
/// IO face rides the Game assembly).
class Settings {
 public:
  /// 装配 PlayerSettings/GameSettings 两段(其余段随 Phase 6/7 扩展)
  /// Assembles the PlayerSettings/GameSettings sections (the rest extend
  /// with Phase 6/7).
  Settings();

  /// 上游 Settings.Load 的段合并面(节点 → 段记录)
  /// Upstream Settings.Load's section-merge face (the nodes → the section
  /// records).
  void Load(const std::vector<yaml::MiniYamlNode>& vec_nodes,
            yaml::StringPool& pool);

  /// 上游 Settings.Save(false):全部段 Commit → 节点表(共享段在前)
  /// Upstream Settings.Save(false): every section's Commit → the node
  /// table (the shared sections first).
  std::vector<yaml::MiniYamlNode> Save(yaml::StringPool& pool) const;

  SettingsModule* Module(std::string_view str_key);
  const SettingsModule* Module(std::string_view str_key) const;

 private:
  std::vector<std::unique_ptr<SettingsModule>> vec_modules_;
};

}  // namespace ora::game
