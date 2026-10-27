#pragma once
import std;

#include "fs/i_package.hpp"
#include "meta/field_desc.hpp"
#include "meta/type_registry.hpp"
#include "yaml/mini_yaml.hpp"

namespace ora::game {

class ModMetadata final : public meta::RecordObject {
 public:
  std::string str_title{};
  std::string str_version{};
  std::string str_website{};
  std::string str_webIcon32{};
  std::string str_windowTitle{};
  bool b_hidden{false};

  const meta::RecordDesc& record_desc() const override;

  static constexpr std::string_view kTypeName = "OpenRA.ModMetadata";
};

class RendererConstants final : public meta::RecordObject {
 public:
  std::int32_t int4_fontSheetSize{512};
  std::int32_t int4_cursorSheetSize{512};
  std::int32_t int4_mapPreviewSheetSize{2048};
  std::int32_t int4_sequenceBgraSheetSize{2048};
  std::int32_t int4_sequenceIndexedSheetSize{2048};
  std::int32_t int4_vertexBatchSize{8192};

  const meta::RecordDesc& record_desc() const override;

  static constexpr std::string_view kTypeName = "OpenRA.RendererConstants";
};

class Manifest final {
 public:

  Manifest(std::string str_mod_id, fs::IReadOnlyPackage& package);

  const std::string& Id() const { return str_id_; }
  fs::IReadOnlyPackage& Package() const { return *pkg_package_; }
  const ModMetadata& Metadata() const { return rec_metadata_; }

  const RendererConstants& RendererConstantsRef() const {
    return rec_rendererConstants_;
  }

  const std::vector<std::string>& Rules() const { return vec_rules_; }
  const std::vector<std::string>& Sequences() const { return vec_sequences_; }
  const std::vector<std::string>& ModelSequences() const { return vec_modelSequences_; }
  const std::vector<std::string>& Cursors() const { return vec_cursors_; }
  const std::vector<std::string>& Chrome() const { return vec_chrome_; }
  const std::vector<std::string>& ChromeLayout() const { return vec_chromeLayout_; }
  const std::vector<std::string>& Weapons() const { return vec_weapons_; }
  const std::vector<std::string>& Voices() const { return vec_voices_; }
  const std::vector<std::string>& Notifications() const { return vec_notifications_; }
  const std::vector<std::string>& Music() const { return vec_music_; }
  const std::vector<std::string>& FluentMessages() const { return vec_fluentMessages_; }

  const std::string& FluentCulture() const { return str_fluentCulture_; }
  const std::vector<std::string>& TileSets() const { return vec_tileSets_; }
  const std::vector<std::string>& ChromeMetrics() const { return vec_chromeMetrics_; }
  const std::vector<std::string>& ServerTraits() const { return vec_serverTraits_; }
  const std::vector<std::string>& Hotkeys() const { return vec_hotkeys_; }
  const std::vector<std::string>& Missions() const { return vec_missions_; }

  const yaml::MiniYaml& FileSystemNode() const { return *yaml_fileSystem_; }

  const std::string& FileSystemLoaderName() const { return str_fileSystemLoader_; }

  const yaml::MiniYaml* MergedNode(std::string_view key) const {
    if (const yaml::MiniYamlNode* n = yaml_merged_.NodeWithKeyOrDefault(key))
      return &n->Value;
    return nullptr;
  }

  const yaml::MiniYaml* GlobalModDataNode(std::string_view key) const;

  const std::vector<std::string>& PackageFormats() const { return vec_packageFormats_; }

  const std::vector<std::string>& SoundFormats() const { return vec_soundFormats_; }

  const std::vector<std::string>& SpriteFormats() const { return vec_spriteFormats_; }

  const std::string& SpriteSequenceFormat() const { return str_spriteSequenceFormat_; }

  const std::string& TerrainFormat() const { return str_terrainFormat_; }

  const std::vector<std::string>& MapCompatibility() const {
    return vec_mapCompatibility_;
  }

  const std::vector<std::pair<std::string, std::string>>& MapFolders() const {
    return vec_mapFolders_;
  }

  const std::string& DefaultOrderGenerator() const {
    return str_defaultOrderGenerator_;
  }

 private:
  std::string str_id_;
  fs::IReadOnlyPackage* pkg_package_;
  yaml::StringPool pool_stringPool_;
  std::vector<yaml::MiniYamlNode> vec_nodes_;
  yaml::MiniYaml yaml_merged_{};

  ModMetadata rec_metadata_;
  RendererConstants rec_rendererConstants_{};
  std::vector<std::string> vec_rules_, vec_sequences_, vec_modelSequences_,
      vec_cursors_, vec_chrome_, vec_chromeLayout_, vec_weapons_, vec_voices_,
      vec_notifications_, vec_music_, vec_tileSets_, vec_chromeMetrics_,
      vec_serverTraits_, vec_hotkeys_, vec_missions_;
  std::vector<std::pair<std::string, std::string>> vec_mapFolders_;
  std::string str_defaultOrderGenerator_;
  const yaml::MiniYaml* yaml_fileSystem_{nullptr};
  std::string str_fileSystemLoader_;
  std::vector<std::string> vec_packageFormats_;
  std::vector<std::string> vec_soundFormats_;
  std::vector<std::string> vec_spriteFormats_;
  std::string str_spriteSequenceFormat_;
  std::string str_terrainFormat_;
  std::vector<std::string> vec_mapCompatibility_;
  std::vector<std::string> vec_fluentMessages_;
  std::string str_fluentCulture_{"en"};
};

}
