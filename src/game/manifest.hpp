// UPSTREAM: OpenRA.Game/Manifest.cs @b6fc03f L21-206(逐语义重写)
//          Full statement-by-statement rewrite of upstream Manifest.cs.
//
// 机制对照 / Mechanism mapping:
//  - Include 展开(L98-111):倒序就地替换为文件内容
//    The Include expansion (L98-111): in-place replacement by file contents
//    in reverse order.
//  - Merge + ToDictionary(L114):继承覆盖合并后的节点字典
//    Merge + ToDictionary (L114): the node dictionary after inheritance
//    merging.
//  - ModMetadata(RendererConstants)经 FieldLoader 加载(手写类 + ORA_FIELD
//    描述表;ModMetadata.cs L21-38 / L40-48)
//    ModMetadata (RendererConstants) loads through FieldLoader (hand-written
//    classes + ORA_FIELD descriptor tables).
//  - 持有性:Manifest 拥有整个 MiniYaml 节点树(视图在其存活期内稳定;
//    上游 string 池化等价物 = 节点树内 const std::string*)
//    Ownership: the Manifest owns the whole MiniYaml tree (views stay valid
//    for its lifetime; the upstream pooled strings = the in-tree
//    const std::string*).
#pragma once
import std;

#include "fs/i_package.hpp"
#include "meta/field_desc.hpp"
#include "meta/type_registry.hpp"
#include "yaml/mini_yaml.hpp"

namespace ora::game {

/// ModMetadata(Manifest.cs L21-38;FieldLoader 按 yaml 键加载)
/// ModMetadata (Manifest.cs L21-38; loaded by FieldLoader per yaml keys).
class ModMetadata final : public meta::RecordObject {
 public:
  std::string str_title{};        // Title(FluentReference,Phase 6 翻译)
  std::string str_version{};
  std::string str_website{};
  std::string str_webIcon32{};
  std::string str_windowTitle{};  // 可选 / optional
  bool b_hidden{false};

  const meta::RecordDesc& record_desc() const override;

  static constexpr std::string_view kTypeName = "OpenRA.ModMetadata";
};

/// RendererConstants(Manifest.cs L40-48)
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

/// Manifest(Manifest.cs L51-206):mod.yaml 的解析产物
/// Manifest (Manifest.cs L51-206): the parsed product of mod.yaml.
class Manifest final {
 public:
  /// modId + mod 包(包不归 Manifest 所有;mod.yaml 从包读出)
  /// modId + the mod package (not owned; mod.yaml is read out of it).
  Manifest(std::string str_mod_id, fs::IReadOnlyPackage& package);

  const std::string& Id() const { return str_id_; }
  fs::IReadOnlyPackage& Package() const { return *pkg_package_; }
  const ModMetadata& Metadata() const { return rec_metadata_; }

  // YamlList 产物(节点 Key 拷贝;Manifest.cs L190-196)/ YamlList products
  // (node-key copies; Manifest.cs L190-196).
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
  const std::vector<std::string>& TileSets() const { return vec_tileSets_; }
  const std::vector<std::string>& ChromeMetrics() const { return vec_chromeMetrics_; }
  const std::vector<std::string>& ServerTraits() const { return vec_serverTraits_; }

  /// FileSystem 节点(必需;缺省抛 invalid-data 语义异常)
  /// The FileSystem node (required; a missing one raises the
  /// invalid-data-semantics exception).
  const yaml::MiniYaml& FileSystemNode() const { return *yaml_fileSystem_; }
  /// FileSystem 加载器名(节点 Value)
  /// The FileSystem loader name (the node Value).
  const std::string& FileSystemLoaderName() const { return str_fileSystemLoader_; }

  /// PackageFormats(L159-161):包格式加载器名列表(标量 = 单元素;
  /// mod.yaml 事实形态)。消费方:ModData → IPackageLoader 集(Mix 等)。
  /// PackageFormats (L159-161): the package-format loader names (a scalar
  /// is a one-element list — the de-facto mod.yaml shape). Consumer: ModData
  /// → the IPackageLoader set (Mix, ...).
  const std::vector<std::string>& PackageFormats() const { return vec_packageFormats_; }

  /// SoundFormats(L162-164):声音格式加载器名(逗号分隔串;空 = 空)。
  /// 消费方:声音格式链(Aud→Wav→Voc→Ogg→Mp3…)。
  /// SoundFormats (L162-164): the sound-format loader names (a
  /// comma-separated string; empty when absent). Consumer: the sound-format
  /// chain (Aud→Wav→Voc→Ogg→Mp3...).
  const std::vector<std::string>& SoundFormats() const { return vec_soundFormats_; }

  /// SpriteFormats(L165-167):精灵格式加载器名(逗号分隔串;空 = 空)。
  /// 消费方:SpriteCache/FrameCache 的加载器链(ShpTD/PngSheet/…)。
  /// SpriteFormats (L165-167): the sprite-format loader names (a
  /// comma-separated string; empty when absent). Consumer: the loader chain
  /// of SpriteCache/FrameCache (ShpTD/PngSheet/...).
  const std::vector<std::string>& SpriteFormats() const { return vec_spriteFormats_; }

  /// SpriteSequenceFormat(L171-172):序列加载器名(单标量)。消费方:
  /// MakeSequenceLoader(Default/Classic/TilesetSpecific/D2k…)。
  /// SpriteSequenceFormat (L171-172): the sequence-loader name (a single
  /// scalar). Consumer: MakeSequenceLoader (Default/Classic/
  /// TilesetSpecific/D2k...).
  const std::string& SpriteSequenceFormat() const { return str_spriteSequenceFormat_; }

  /// MapFolders(my.Value 字典;L119 + L198-204)
  /// MapFolders (the my.Value dictionary; L119 + L198-204).
  const std::vector<std::pair<std::string, std::string>>& MapFolders() const {
    return vec_mapFolders_;
  }

 private:
  std::string str_id_;
  fs::IReadOnlyPackage* pkg_package_;
  yaml::StringPool pool_stringPool_;           // 整树的字符串池 / the pool of the whole tree
  std::vector<yaml::MiniYamlNode> vec_nodes_;  // Include 展开后的原始节点 / raw nodes after Include expansion
  yaml::MiniYaml yaml_merged_{};               // Merge([nodes]) 的树 / the Merge([nodes]) tree

  ModMetadata rec_metadata_;
  std::vector<std::string> vec_rules_, vec_sequences_, vec_modelSequences_,
      vec_cursors_, vec_chrome_, vec_chromeLayout_, vec_weapons_, vec_voices_,
      vec_notifications_, vec_music_, vec_tileSets_, vec_chromeMetrics_,
      vec_serverTraits_;
  std::vector<std::pair<std::string, std::string>> vec_mapFolders_;
  const yaml::MiniYaml* yaml_fileSystem_{nullptr};
  std::string str_fileSystemLoader_;
  std::vector<std::string> vec_packageFormats_;
  std::vector<std::string> vec_soundFormats_;
  std::vector<std::string> vec_spriteFormats_;
  std::string str_spriteSequenceFormat_;
};

}  // namespace ora::game
