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

#include "core/mersenne_twister.hpp"
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

/// SoundPool(SoundInfo.cs L56-86;GetNext 的 Game.CosmeticRandom → 随机器
/// 注入,Phase 5 主循环接全局 —— COVERAGE 登记)。
/// SoundPool (SoundInfo.cs L56-86; GetNext's Game.CosmeticRandom becomes an
/// injected random source, the Phase 5 main loop wiring the global —
/// registered in COVERAGE).
class SoundPool final {
 public:
  /// InterruptType(L57)。
  enum class InterruptType : std::uint8_t { DoNotPlay, Interrupt, Overlap };
  static constexpr InterruptType kDefaultInterruptType = InterruptType::DoNotPlay;

  SoundPool(float fp4_volume_modifier, InterruptType kind_interrupt_type,
            std::vector<std::string> vec_clips)
      : fp4_volume_modifier_(fp4_volume_modifier),
        kind_type_(kind_interrupt_type),
        vec_clips_(std::move(vec_clips)) {}

  float VolumeModifier() const { return fp4_volume_modifier_; }
  InterruptType Type() const { return kind_type_; }

  /// GetNext(L73-84):liveclips 耗尽回填;零 clips → null(空串承载)。
  /// GetNext (L73-84): refills liveclips when drained; zero clips → null
  /// (carried as the empty string).
  std::string GetNext(MersenneTwister& mt_random) const;

 private:
  float fp4_volume_modifier_ = 1.0f;
  InterruptType kind_type_ = kDefaultInterruptType;
  std::vector<std::string> vec_clips_;
  mutable std::vector<std::string> vec_live_clips_;  // 抽干即回填 | refilled when drained
};

/// SoundInfo(SoundInfo.cs L19;音频池字段随第十四批声音链落地)
/// SoundInfo (SoundInfo.cs L19; the audio-pool fields land with the
/// fourteenth batch's sound chain)
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

  // 字典查找(上游 FrozenDictionary;线性保插入序)
  // Dictionary lookups (upstream's FrozenDictionary; linear, insertion
  // order kept).
  const std::vector<std::string>* FindVariants(std::string_view str_key) const;
  const std::vector<std::string>* FindPrefixes(std::string_view str_key) const;
  bool DisableVariantsContains(std::string_view str_key) const;
  bool DisablePrefixesContains(std::string_view str_key) const;

  /// VoicePools(Lazy;Voices 字典 → 1 音量/默认打断型池)。首查物化。
  /// VoicePools (Lazy; the Voices dictionary → 1-volume/default-interrupt
  /// pools). Materialized on the first query.
  const std::vector<std::pair<std::string, SoundPool>>& VoicePools() const;

  /// NotificationsPools(Lazy;ParseSoundPool(y,"Notifications") 重读原
  /// yaml —— 缺键 = NodeWithKey 的等价抛)。首查物化。
  /// NotificationsPools (Lazy; ParseSoundPool(y, "Notifications") re-reads
  /// the raw yaml — a missing key = NodeWithKey's equivalent throw).
  /// Materialized on the first query.
  const std::vector<std::pair<std::string, SoundPool>>& NotificationsPools() const;

  static constexpr std::string_view kTypeName = "OpenRA.GameRules.SoundInfo";

 private:
  // ParseSoundPool 的重读源(上游闭包捕获 y)
  // The re-read source of ParseSoundPool (upstream's closure captures y).
  std::optional<yaml::MiniYaml> yaml_source_{};
  mutable std::optional<std::vector<std::pair<std::string, SoundPool>>> opt_vec_voice_pools_;
  mutable std::optional<std::vector<std::pair<std::string, SoundPool>>> opt_vec_notification_pools_;
};

/// MusicInfo(MusicInfo.cs L16;Load/Exists/Length 随第十四批声音链)
/// MusicInfo (MusicInfo.cs L16; Load/Exists/Length arrive with the
/// fourteenth batch's sound chain)
class MusicInfo final {
 public:
  MusicInfo(std::string str_key, const yaml::MiniYaml& value);

  const std::string& Filename() const { return str_filename_; }
  const std::string& Title() const { return str_title_; }
  bool Hidden() const { return b_hidden_; }
  float VolumeModifier() const { return fp4_volumeModifier_; }
  std::int32_t Length() const { return int4_length_; }  // 秒 | seconds
  bool Exists() const { return b_exists_; }

  /// Load(L49-72):TryOpen 失败静默;命中 loader 记 Exists + Length。
  /// loader 链 = SoundLoaderFn(声音门面侧的形态)。
  /// Load (L49-72): a TryOpen failure stays silent; a loader hit records
  /// Exists + Length. The loader chain takes the SoundLoaderFn shape (the
  /// sound-facade side).
  template <class TryOpen, class TryParse>
  void Load(TryOpen&& fn_try_open, TryParse&& fn_try_parse) {
    // upstream: fileSystem.TryOpen 失败即返回(Exists 不置位)
    // upstream: a fileSystem.TryOpen failure returns at once (Exists stays
    // unset).
    if (!fn_try_open(str_filename_))
      return;

    b_exists_ = true;
    fn_try_parse(*this);
  }

  void SetLength(std::int32_t int4_seconds) { int4_length_ = int4_seconds; }

 private:
  std::string str_filename_;
  std::string str_title_;
  bool b_hidden_{false};
  float fp4_volumeModifier_{1.0f};
  std::int32_t int4_length_{0};
  bool b_exists_{false};
};

/// 24 个 [FieldLoader.LoadUsing] 加载器注册(loaders.cpp;引擎/测试的加载链
/// 入口各调用一次)
/// Register the 24 [FieldLoader.LoadUsing] loaders (loaders.cpp; each
/// loading-chain entry — engine or tests — calls this once).
void RegisterGameLoaders();

}  // namespace ora::game
