// UPSTREAM: OpenRA.Game/GameRules/WeaponInfo.cs @b6fc03f(game_records.hpp
//          的实现;WeaponInfo/SoundInfo/MusicInfo,见 hpp 头注)
//          Implementation of game_records.hpp — WeaponInfo/SoundInfo/
//          MusicInfo; see the hpp header notes.
import std;
#include "game/game_records.hpp"
#include "meta/field_loader.hpp"
#include "meta/parse.hpp"

namespace ora::game {

// ———— WeaponInfo ————

namespace {

/// ValidTargets 默认 "Ground,Water"(L107)的位分配在 Defaults 静态构造时
/// 与 C# 首个实例化同序
/// The ValidTargets default "Ground,Water" (L107) allocates bits at Defaults
/// construction, in the same order as the first C# instantiation.
std::int64_t DefaultValidTargetsBits() {
  static const std::int64_t int8_bits = static_cast<std::int64_t>(
      meta::BitsOf("OpenRA.Traits.TargetableType", std::vector<std::string>{"Ground", "Water"}));
  return int8_bits;
}

// 元素/容器子描述复用 / the shared element/container sub-descriptors.
constexpr meta::FieldDesc kElemString = meta::ElemOf(meta::FieldType::String);
constexpr meta::FieldDesc kElemInt32 = meta::ElemOf(meta::FieldType::Int32);
constexpr meta::FieldDesc kValStringArray{
    .str_name = {}, .type = meta::FieldType::ImmutableArray, .b_required = false,
    .str_loader = {}, .off_offset = 0, .elem = &kElemString, .key = nullptr,
    .value = nullptr,
    .str_type_name = "System.Collections.Immutable.ImmutableArray`1[System.String]"};

constexpr std::array<meta::FieldDesc, 17> kFields_WeaponInfo{{
    {.str_name = "Range", .type = meta::FieldType::WDist, .b_required = false, .str_loader = {}, .off_offset = __builtin_offsetof(WeaponInfo, int4_range), .elem = nullptr, .key = nullptr, .value = nullptr, .str_type_name = "OpenRA.WDist"},
    {.str_name = "FirstBurstTargetOffset", .type = meta::FieldType::WVec, .b_required = false, .str_loader = {}, .off_offset = __builtin_offsetof(WeaponInfo, tuple_firstBurstTargetOffset), .elem = nullptr, .key = nullptr, .value = nullptr, .str_type_name = "OpenRA.WVec"},
    {.str_name = "FollowingBurstTargetOffset", .type = meta::FieldType::WVec, .b_required = false, .str_loader = {}, .off_offset = __builtin_offsetof(WeaponInfo, tuple_followingBurstTargetOffset), .elem = nullptr, .key = nullptr, .value = nullptr, .str_type_name = "OpenRA.WVec"},
    {.str_name = "Report", .type = meta::FieldType::ImmutableArray, .b_required = false, .str_loader = {}, .off_offset = __builtin_offsetof(WeaponInfo, vec_report), .elem = &kElemString, .key = nullptr, .value = nullptr, .str_type_name = "System.Collections.Immutable.ImmutableArray`1[System.String]"},
    {.str_name = "StartBurstReport", .type = meta::FieldType::ImmutableArray, .b_required = false, .str_loader = {}, .off_offset = __builtin_offsetof(WeaponInfo, vec_startBurstReport), .elem = &kElemString, .key = nullptr, .value = nullptr, .str_type_name = "System.Collections.Immutable.ImmutableArray`1[System.String]"},
    {.str_name = "AfterFireSound", .type = meta::FieldType::ImmutableArray, .b_required = false, .str_loader = {}, .off_offset = __builtin_offsetof(WeaponInfo, vec_afterFireSound), .elem = &kElemString, .key = nullptr, .value = nullptr, .str_type_name = "System.Collections.Immutable.ImmutableArray`1[System.String]"},
    {.str_name = "AfterFireSoundDelay", .type = meta::FieldType::Int32, .b_required = false, .str_loader = {}, .off_offset = __builtin_offsetof(WeaponInfo, int4_afterFireSoundDelay), .elem = nullptr, .key = nullptr, .value = nullptr, .str_type_name = "System.Int32"},
    {.str_name = "ReloadDelay", .type = meta::FieldType::Int32, .b_required = false, .str_loader = {}, .off_offset = __builtin_offsetof(WeaponInfo, int4_reloadDelay), .elem = nullptr, .key = nullptr, .value = nullptr, .str_type_name = "System.Int32"},
    {.str_name = "Burst", .type = meta::FieldType::Int32, .b_required = false, .str_loader = {}, .off_offset = __builtin_offsetof(WeaponInfo, int4_burst), .elem = nullptr, .key = nullptr, .value = nullptr, .str_type_name = "System.Int32"},
    {.str_name = "CanTargetSelf", .type = meta::FieldType::Bool, .b_required = false, .str_loader = {}, .off_offset = __builtin_offsetof(WeaponInfo, b_canTargetSelf), .elem = nullptr, .key = nullptr, .value = nullptr, .str_type_name = "System.Boolean"},
    {.str_name = "ValidTargets", .type = meta::FieldType::BitSet, .b_required = false, .str_loader = {}, .off_offset = __builtin_offsetof(WeaponInfo, int8_validTargets), .elem = nullptr, .key = nullptr, .value = nullptr, .str_type_name = "OpenRA.Traits.TargetableType"},
    {.str_name = "InvalidTargets", .type = meta::FieldType::BitSet, .b_required = false, .str_loader = {}, .off_offset = __builtin_offsetof(WeaponInfo, int8_invalidTargets), .elem = nullptr, .key = nullptr, .value = nullptr, .str_type_name = "OpenRA.Traits.TargetableType"},
    {.str_name = "AirThreshold", .type = meta::FieldType::WDist, .b_required = false, .str_loader = {}, .off_offset = __builtin_offsetof(WeaponInfo, int4_airThreshold), .elem = nullptr, .key = nullptr, .value = nullptr, .str_type_name = "OpenRA.WDist"},
    {.str_name = "BurstDelays", .type = meta::FieldType::ImmutableArray, .b_required = false, .str_loader = {}, .off_offset = __builtin_offsetof(WeaponInfo, vec_burstDelays), .elem = &kElemInt32, .key = nullptr, .value = nullptr, .str_type_name = "System.Collections.Immutable.ImmutableArray`1[System.Int32]"},
    {.str_name = "MinRange", .type = meta::FieldType::WDist, .b_required = false, .str_loader = {}, .off_offset = __builtin_offsetof(WeaponInfo, int4_minRange), .elem = nullptr, .key = nullptr, .value = nullptr, .str_type_name = "OpenRA.WDist"},
    {.str_name = "TargetActorCenter", .type = meta::FieldType::Bool, .b_required = false, .str_loader = {}, .off_offset = __builtin_offsetof(WeaponInfo, b_targetActorCenter), .elem = nullptr, .key = nullptr, .value = nullptr, .str_type_name = "System.Boolean"},
    {.str_name = "Projectile", .type = meta::FieldType::Opaque, .b_required = false, .str_loader = "OpenRA.GameRules.WeaponInfo.LoadProjectile", .off_offset = __builtin_offsetof(WeaponInfo, rec_projectile), .elem = nullptr, .key = nullptr, .value = nullptr, .str_type_name = "LoadProjectile"},
}};

constexpr std::array<meta::FieldDesc, 1> kFields_WeaponInfoWarheads{{
    {.str_name = "Warheads", .type = meta::FieldType::Opaque, .b_required = false, .str_loader = "OpenRA.GameRules.WeaponInfo.LoadWarheads", .off_offset = __builtin_offsetof(WeaponInfo, vec_warheads), .elem = nullptr, .key = nullptr, .value = nullptr, .str_type_name = "LoadWarheads"},
}};

constexpr meta::RecordDesc kDesc_WeaponInfo{
    .str_name = "OpenRA.GameRules.WeaponInfo",
    .str_base = {},
    .fields = kFields_WeaponInfo,
    .requires_types = {},
    .not_before_types = {},
    .interfaces = {},
};

const meta::RecordDesc& WeaponInfoDescWithWarheads() {
  // Warheads 字段单列一个 RecordDesc(同名字段不与 Projectile 并表 ——
  // FieldDesc 数组须完整覆盖字段;两表合一更简洁,但战头加载器需要
  // off_offset 指向 vec_warheads,直接并入主表)
  // The Warheads field lives in its own RecordDesc table merged into the
  // main one below (a FieldDesc array must cover every field).
  static constexpr std::array<meta::FieldDesc, 18> kAll = [] {
    std::array<meta::FieldDesc, 18> arr{};
    for (std::size_t int4_i{}; int4_i < kFields_WeaponInfo.size(); int4_i++)
      arr[int4_i] = kFields_WeaponInfo[int4_i];
    arr[17] = kFields_WeaponInfoWarheads[0];
    return arr;
  }();
  static constexpr meta::RecordDesc kDesc{
      .str_name = "OpenRA.GameRules.WeaponInfo",
      .str_base = {},
      .fields = kAll,
      .requires_types = {},
      .not_before_types = {},
      .interfaces = {},
  };
  return kDesc;
}

}  // namespace

WeaponInfo::WeaponInfo(const yaml::MiniYaml& content) {
  int8_validTargets = DefaultValidTargetsBits();

  // L141-144:WithNodes(Merge([content.Nodes])) —— 武器级继承/删除解析
  // L141-144: WithNodes(Merge([content.Nodes])) — the weapon-level
  // inheritance/removal resolution.
  const yaml::MiniYaml yaml_resolved =
      content.WithNodes(yaml::MiniYaml::Merge({content.Nodes}));
  meta::Load(this, yaml_resolved);
}

const meta::RecordDesc& WeaponInfo::record_desc() const {
  return WeaponInfoDescWithWarheads();
}

// ———— SoundInfo ————

namespace {

constexpr std::array<meta::FieldDesc, 8> kFields_SoundInfo{{
    {.str_name = "Variants", .type = meta::FieldType::FrozenDictionary, .b_required = false, .str_loader = {}, .off_offset = __builtin_offsetof(SoundInfo, map_variants), .elem = nullptr, .key = &kElemString, .value = &kValStringArray, .str_type_name = "System.Collections.Frozen.FrozenDictionary`2[System.String,System.Collections.Immutable.ImmutableArray`1[System.String]]"},
    {.str_name = "Prefixes", .type = meta::FieldType::FrozenDictionary, .b_required = false, .str_loader = {}, .off_offset = __builtin_offsetof(SoundInfo, map_prefixes), .elem = nullptr, .key = &kElemString, .value = &kValStringArray, .str_type_name = "System.Collections.Frozen.FrozenDictionary`2[System.String,System.Collections.Immutable.ImmutableArray`1[System.String]]"},
    {.str_name = "Voices", .type = meta::FieldType::FrozenDictionary, .b_required = false, .str_loader = {}, .off_offset = __builtin_offsetof(SoundInfo, map_voices), .elem = nullptr, .key = &kElemString, .value = &kValStringArray, .str_type_name = "System.Collections.Frozen.FrozenDictionary`2[System.String,System.Collections.Immutable.ImmutableArray`1[System.String]]"},
    {.str_name = "Notifications", .type = meta::FieldType::FrozenDictionary, .b_required = false, .str_loader = {}, .off_offset = __builtin_offsetof(SoundInfo, map_notifications), .elem = nullptr, .key = &kElemString, .value = &kValStringArray, .str_type_name = "System.Collections.Frozen.FrozenDictionary`2[System.String,System.Collections.Immutable.ImmutableArray`1[System.String]]"},
    {.str_name = "DefaultVariant", .type = meta::FieldType::String, .b_required = false, .str_loader = {}, .off_offset = __builtin_offsetof(SoundInfo, str_defaultVariant), .elem = nullptr, .key = nullptr, .value = nullptr, .str_type_name = "System.String"},
    {.str_name = "DefaultPrefix", .type = meta::FieldType::String, .b_required = false, .str_loader = {}, .off_offset = __builtin_offsetof(SoundInfo, str_defaultPrefix), .elem = nullptr, .key = nullptr, .value = nullptr, .str_type_name = "System.String"},
    {.str_name = "DisableVariants", .type = meta::FieldType::FrozenSet, .b_required = false, .str_loader = {}, .off_offset = __builtin_offsetof(SoundInfo, vec_disableVariants), .elem = &kElemString, .key = nullptr, .value = nullptr, .str_type_name = "System.Collections.Frozen.FrozenSet`1[System.String]"},
    {.str_name = "DisablePrefixes", .type = meta::FieldType::FrozenSet, .b_required = false, .str_loader = {}, .off_offset = __builtin_offsetof(SoundInfo, vec_disablePrefixes), .elem = &kElemString, .key = nullptr, .value = nullptr, .str_type_name = "System.Collections.Frozen.FrozenSet`1[System.String]"},
}};

constexpr meta::RecordDesc kDesc_SoundInfo{
    .str_name = "OpenRA.GameRules.SoundInfo",
    .str_base = {},
    .fields = kFields_SoundInfo,
    .requires_types = {},
    .not_before_types = {},
    .interfaces = {},
};

}  // namespace

SoundInfo::SoundInfo(const yaml::MiniYaml& y) {
  meta::Load(this, y);
  // VoicePools/NotificationsPools(Lazy 音频池)Phase 4 随音频层 —— 上游亦无
  // yaml 解析器,加载行为等价
  // The Lazy audio pools land with the Phase 4 audio layer — upstream has no
  // yaml parser for them either, so loading behaves identically.
}

const meta::RecordDesc& SoundInfo::record_desc() const { return kDesc_SoundInfo; }

// ———— MusicInfo ————

MusicInfo::MusicInfo(std::string str_key, const yaml::MiniYaml& value) {
  // MusicInfo(MusicInfo.cs L30-45):手读字段(不走 FieldLoader)
  // MusicInfo (MusicInfo.cs L30-45): hand-read fields (no FieldLoader).
  str_title_ = value.Value != nullptr ? *value.Value : std::string{};

  const std::vector<std::pair<std::string_view, const yaml::MiniYaml*>> vec_nd =
      value.ToDictionary();
  const auto find = [&vec_nd](std::string_view sv_key) -> const yaml::MiniYaml* {
    for (const auto& [sv_k, yaml_v] : vec_nd)
      if (sv_k == sv_key)
        return yaml_v;
    return nullptr;
  };

  if (const yaml::MiniYaml* yaml_hidden = find("Hidden"))
    meta::TryParseBoolNet(yaml_hidden->Value != nullptr ? std::string_view{*yaml_hidden->Value}
                                                        : std::string_view{},
                          b_hidden_);

  std::string str_ext = "aud";
  std::string str_base = std::move(str_key);
  if (const yaml::MiniYaml* yaml_vm = find("VolumeModifier"))
    fp4_volumeModifier_ = meta::GetFloatValue(
        "VolumeModifier",
        yaml_vm->Value != nullptr ? std::string_view{*yaml_vm->Value} : std::string_view{});
  if (const yaml::MiniYaml* yaml_ext = find("Extension"))
    str_ext = yaml_ext->Value != nullptr ? *yaml_ext->Value : std::string{};
  if (const yaml::MiniYaml* yaml_fn = find("Filename"))
    str_base = yaml_fn->Value != nullptr ? *yaml_fn->Value : std::string{};

  str_filename_ = str_base + "." + str_ext;
}

}  // namespace ora::game
