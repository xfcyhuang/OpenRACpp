#pragma once
import std;

#include "core/int2.hpp"
#include "meta/generic_record.hpp"
#include "yaml/mini_yaml.hpp"

namespace ora::game {

class SettingsModule {
 public:
  SettingsModule(const meta::RecordDesc& desc, std::string str_key,
                 bool b_shared);

  void Load(const yaml::MiniYaml& yaml_section);

  std::vector<yaml::MiniYamlNode> Commit(yaml::StringPool& pool) const;

  const meta::GeneratedRecord& Record() const { return *up_record_; }
  meta::GeneratedRecord& Record() { return *up_record_; }
  const std::string& Key() const { return str_key_; }
  bool Shared() const { return b_shared_; }

  std::optional<std::size_t> SlotIndexOf(std::string_view str_field) const;

  meta::FieldType SlotType(std::size_t sz_slot) const;

  bool GetBool(std::size_t sz_slot) const;
  void SetBool(std::size_t sz_slot, bool b_value);
  float GetFloat(std::size_t sz_slot) const;
  void SetFloat(std::size_t sz_slot, float fp4_value);
  std::int64_t GetInt(std::size_t sz_slot) const;
  void SetInt(std::size_t sz_slot, std::int64_t int8_value);
  const std::string& GetString(std::size_t sz_slot) const;
  void SetString(std::size_t sz_slot, std::string str_value);
  int2 GetInt2(std::size_t sz_slot) const;
  void SetInt2(std::size_t sz_slot, int2 int2_value);

  const meta::GenericValue& DefaultSlot(std::size_t sz_slot) const;
  bool DefaultBool(std::size_t sz_slot) const;
  float DefaultFloat(std::size_t sz_slot) const;
  std::int64_t DefaultInt(std::size_t sz_slot) const;
  const std::string& DefaultString(std::size_t sz_slot) const;

  meta::GeneratedRecord CloneRecord() const;

 private:
  std::string str_key_;
  bool b_shared_ = true;
  std::unique_ptr<meta::GeneratedRecord> up_record_;
  std::unique_ptr<meta::GeneratedRecord> up_defaults_;
};

class RawSettingsSection {
 public:
  RawSettingsSection(std::string str_key) : str_key_{std::move(str_key)} {}

  void Load(const yaml::MiniYaml& yaml_section);
  std::vector<yaml::MiniYamlNode> Commit(yaml::StringPool& pool) const;

  const std::string& Key() const { return str_key_; }

  void RemoveNodesNamed(std::string_view str_name);

  void AddNode(const std::string& str_name, const std::string& str_value);

  const std::vector<yaml::MiniYamlNode>& Nodes() const { return vec_nodes_; }

 private:
  std::string str_key_;
  std::vector<yaml::MiniYamlNode> vec_nodes_;
  std::deque<std::string> deque_strings_;
};

const meta::RecordDesc* SettingsDescOf(std::string_view str_section);

class Settings {
 public:

  Settings();

  static Settings& Instance();
  static void ResetInstanceForTest();

  void Load(const std::vector<yaml::MiniYamlNode>& vec_nodes,
            yaml::StringPool& pool);

  std::vector<yaml::MiniYamlNode> Save(yaml::StringPool& pool) const;

  SettingsModule* Module(std::string_view str_key);
  const SettingsModule* Module(std::string_view str_key) const;

  SettingsModule* ModuleOrCreate(std::string_view str_key);

  RawSettingsSection& RawSection(std::string_view str_key);

 private:
  std::vector<std::unique_ptr<SettingsModule>> vec_modules_;
  std::vector<std::unique_ptr<RawSettingsSection>> vec_raw_sections_;
  std::vector<yaml::MiniYamlNode> vec_pending_nodes_;
};

}
