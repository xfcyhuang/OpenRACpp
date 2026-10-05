// UPSTREAM: OpenRA.Game/GameRules/WeaponInfo.cs @b6fc03f L74-175(加载链部分)+
//          SoundInfo.cs L19-77 + MusicInfo.cs L16-47
//          The loading-chain parts of WeaponInfo/SoundInfo/MusicInfo.
//
// 机制对照 / Mechanism mapping:
//  - WeaponInfo:手写 RecordObject(非 601 生成集;两处 LoadUsing 的
//    LoadProjectile/LoadWarheads 按上游逐语义注册 —— Projectile = 键 "Projectile"
//    值节点的 "{值}Info" 工厂产物;Warheads = 键前缀 "Warhead" 的 "{值}Warhead"
//    工厂产物,L147-175)
//    WeaponInfo: a hand-written RecordObject (outside the generated 601;
//    the two LoadUsing loaders LoadProjectile/LoadWarheads registered
//    verbatim — Projectile = the "{value}Info" factory product of the
//    "Projectile" key's value node; Warheads = the "{value}Warhead" factory
//    products of keys prefixed "Warhead", L147-175).
//  - SoundInfo:手写 RecordObject + ORA_FIELD 描述表(Lazy 音频池字段
//    [VoicePools/NotificationsPools] 上游无解析器,描述表省略 = 等价跳过)
//    SoundInfo: a hand-written RecordObject + ORA_FIELD descriptors (the
//    Lazy audio-pool fields [VoicePools/NotificationsPools] have no upstream
//    parser; omitting them from the table skips identically).
//  - MusicInfo:上游不走 FieldLoader(手写从 yaml 读 Title/Hidden/…),
//    同样手写
//    MusicInfo: upstream bypasses FieldLoader (hand-reads
//    Title/Hidden/…); likewise hand-written here.
#pragma once
import std;

#include "meta/field_desc.hpp"
#include "meta/generic_record.hpp"
#include "meta/type_registry.hpp"
#include "yaml/mini_yaml.hpp"

namespace ora::game {

/// WeaponInfo(WeaponInfo.cs L74;加载链字段全量,IsValid*/Impact 属仿真域 Phase 5)
class WeaponInfo final : public meta::RecordObject {
 public:
  /// WeaponInfo(content)(L139-145):先 Merge 解析武器级继承/删除,再 Load
  /// WeaponInfo(content) (L139-145): merge-resolve weapon-level
  /// inheritance/removals, then Load.
  explicit WeaponInfo(const yaml::MiniYaml& content);

  const meta::RecordDesc& record_desc() const override;

  // ———— yaml 字段(默认值 = 上游字段初始化器)————
  std::int32_t int4_range{0};                  // Range: WDist.Zero
  meta::GenericTuple tuple_firstBurstTargetOffset{{0, 0, 0, 0}, {}, 3};
  meta::GenericTuple tuple_followingBurstTargetOffset{{0, 0, 0, 0}, {}, 3};
  std::vector<std::string> vec_report{};
  std::vector<std::string> vec_startBurstReport{};
  std::vector<std::string> vec_afterFireSound{};
  std::int32_t int4_afterFireSoundDelay{0};
  std::int32_t int4_reloadDelay{1};
  std::int32_t int4_burst{1};
  bool b_canTargetSelf{false};
  std::int64_t int8_validTargets{0};           // BitSet<TargetableType>("Ground","Water")
  std::int64_t int8_invalidTargets{0};         // BitSet<TargetableType> 空
  std::int32_t int4_airThreshold{128};         // WDist(128)
  std::vector<std::int32_t> vec_burstDelays{5};
  std::int32_t int4_minRange{0};               // WDist.Zero
  bool b_targetActorCenter{false};
  std::shared_ptr<meta::RecordObject> rec_projectile{};      // LoadProjectile
  std::vector<std::shared_ptr<meta::RecordObject>> vec_warheads{};  // LoadWarheads

  static constexpr std::string_view kTypeName = "OpenRA.GameRules.WeaponInfo";
};

/// SoundInfo(SoundInfo.cs L19;音频池字段 Phase 4 随音频层)
class SoundInfo final : public meta::RecordObject {
 public:
  explicit SoundInfo(const yaml::MiniYaml& y);

  const meta::RecordDesc& record_desc() const override;

  std::vector<std::pair<std::string, std::vector<std::string>>> map_variants{};
  std::vector<std::pair<std::string, std::vector<std::string>>> map_prefixes{};
  std::vector<std::pair<std::string, std::vector<std::string>>> map_voices{};
  std::vector<std::pair<std::string, std::vector<std::string>>> map_notifications{};
  std::string str_defaultVariant{".aud"};
  std::string str_defaultPrefix{};
  std::vector<std::string> vec_disableVariants{};
  std::vector<std::string> vec_disablePrefixes{};

  static constexpr std::string_view kTypeName = "OpenRA.GameRules.SoundInfo";
};

/// MusicInfo(MusicInfo.cs L16;Length/Exists 随资产加载,Phase 4)
class MusicInfo final {
 public:
  MusicInfo(std::string str_key, const yaml::MiniYaml& value);

  const std::string& Filename() const { return str_filename_; }
  const std::string& Title() const { return str_title_; }
  bool Hidden() const { return b_hidden_; }
  float VolumeModifier() const { return fp4_volumeModifier_; }

 private:
  std::string str_filename_;
  std::string str_title_;
  bool b_hidden_{false};
  float fp4_volumeModifier_{1.0f};
};

/// 24 个 [FieldLoader.LoadUsing] 加载器注册(loaders.cpp;引擎/测试的加载链
/// 入口各调用一次)
/// Register the 24 [FieldLoader.LoadUsing] loaders (loaders.cpp; each
/// loading-chain entry — engine or tests — calls this once).
void RegisterGameLoaders();

}  // namespace ora::game
