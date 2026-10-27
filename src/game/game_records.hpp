#pragma once
import std;

#include "core/mersenne_twister.hpp"
#include "meta/field_desc.hpp"
#include "meta/generic_record.hpp"
#include "meta/type_registry.hpp"
#include "yaml/mini_yaml.hpp"

namespace ora::game {

class WeaponInfo final : public meta::RecordObject {
 public:

  explicit WeaponInfo(const yaml::MiniYaml& content);

  const meta::RecordDesc& record_desc() const override;

  std::int32_t int4_range{0};
  meta::GenericTuple tuple_firstBurstTargetOffset{{0, 0, 0, 0}, {}, 3};
  meta::GenericTuple tuple_followingBurstTargetOffset{{0, 0, 0, 0}, {}, 3};
  std::vector<std::string> vec_report{};
  std::vector<std::string> vec_startBurstReport{};
  std::vector<std::string> vec_afterFireSound{};
  std::int32_t int4_afterFireSoundDelay{0};
  std::int32_t int4_reloadDelay{1};
  std::int32_t int4_burst{1};
  bool b_canTargetSelf{false};
  std::int64_t int8_validTargets{0};
  std::int64_t int8_invalidTargets{0};
  std::int32_t int4_airThreshold{128};
  std::vector<std::int32_t> vec_burstDelays{5};
  std::int32_t int4_minRange{0};
  bool b_targetActorCenter{false};
  std::shared_ptr<meta::RecordObject> rec_projectile{};
  std::vector<std::shared_ptr<meta::RecordObject>> vec_warheads{};

  static constexpr std::string_view kTypeName = "OpenRA.GameRules.WeaponInfo";
};

class SoundPool final {
 public:

  enum class InterruptType : std::uint8_t { DoNotPlay, Interrupt, Overlap };
  static constexpr InterruptType kDefaultInterruptType = InterruptType::DoNotPlay;

  SoundPool(float fp4_volume_modifier, InterruptType kind_interrupt_type,
            std::vector<std::string> vec_clips)
      : fp4_volume_modifier_(fp4_volume_modifier),
        kind_type_(kind_interrupt_type),
        vec_clips_(std::move(vec_clips)) {}

  float VolumeModifier() const { return fp4_volume_modifier_; }
  InterruptType Type() const { return kind_type_; }

  std::string GetNext(MersenneTwister& mt_random) const;

 private:
  float fp4_volume_modifier_ = 1.0f;
  InterruptType kind_type_ = kDefaultInterruptType;
  std::vector<std::string> vec_clips_;
  mutable std::vector<std::string> vec_live_clips_;
};

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

  const std::vector<std::string>* FindVariants(std::string_view str_key) const;
  const std::vector<std::string>* FindPrefixes(std::string_view str_key) const;
  bool DisableVariantsContains(std::string_view str_key) const;
  bool DisablePrefixesContains(std::string_view str_key) const;

  const std::vector<std::pair<std::string, SoundPool>>& VoicePools() const;

  const std::vector<std::pair<std::string, SoundPool>>& NotificationsPools() const;

  static constexpr std::string_view kTypeName = "OpenRA.GameRules.SoundInfo";

 private:

  std::optional<yaml::MiniYaml> yaml_source_{};
  mutable std::optional<std::vector<std::pair<std::string, SoundPool>>> opt_vec_voice_pools_;
  mutable std::optional<std::vector<std::pair<std::string, SoundPool>>> opt_vec_notification_pools_;
};

class MusicInfo final {
 public:
  MusicInfo(std::string str_key, const yaml::MiniYaml& value);

  const std::string& Filename() const { return str_filename_; }
  const std::string& Title() const { return str_title_; }
  bool Hidden() const { return b_hidden_; }
  float VolumeModifier() const { return fp4_volumeModifier_; }
  std::int32_t Length() const { return int4_length_; }
  bool Exists() const { return b_exists_; }

  template <class TryOpen, class TryParse>
  void Load(TryOpen&& fn_try_open, TryParse&& fn_try_parse) {

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

void RegisterGameLoaders();

}
