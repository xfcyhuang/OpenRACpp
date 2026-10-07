// UPSTREAM: OpenRA.Mods.Common/Traits/Conditions/GrantCondition.cs @b6fc03f(第七批 mods trait 的
//          注册表装配:Conditions 31 件 + ExternalCondition/Proximity +
//          Cloak/DetectCloaked/IgnoresCloak + GainsExperience/GivesExperience/
//          GainsExperienceMultiplier/PlayerExperience + CaptureManager/
//          Capturable/Captures/GivesCashOnCapture + Selectable/Interactable +
//          SpawnMapActors;与 RegisterCommonTraits 同形 —— Info 名 → 工厂 →
//          WorldArena)
//          The batch-7 mods-trait registry assembly (every file listed in
//          the batch's headers; the same shape as RegisterCommonTraits —
//          the Info name → factory → WorldArena).

#include "core/color.hpp"
#include "game/actor_info.hpp"
#include "meta/generic_record.hpp"
#include "mods/capture.hpp"
#include "mods/cloak.hpp"
#include "mods/conditions.hpp"
#include "mods/experience.hpp"
#include "mods/external_condition.hpp"
#include "mods/selectable.hpp"
#include "mods/spawn_map_actors.hpp"
#include "sim/actor.hpp"
#include "sim/actor_init.hpp"
#include "sim/sync_hash.hpp"
#include "sim/trait_registry.hpp"

namespace ora::meta {
struct GenericTuple;
}

namespace ora::mods {

namespace {

/// string 数组槽读取(ImmutableArray<string> / FrozenSet<string>)
/// The string-array slot read (ImmutableArray<string> /
/// FrozenSet<string>).
std::vector<std::string> RecStringArray(const meta::RecordObject& rec,
                                        std::string_view str_name) {
  std::vector<std::string> out;
  if (const auto* gv = sim::RecordFieldValue(rec, str_name))
    if (const auto* arr =
            std::get_if<std::vector<meta::GenericValue>>(&gv->val))
      for (const auto& element : *arr)
        if (const auto* str = std::get_if<std::string>(&element.val))
          out.push_back(*str);
  return out;
}

std::optional<std::string> RecString(const meta::RecordObject& rec,
                                     std::string_view str_name) {
  if (const auto s = sim::RecordFieldString(rec, str_name))
    return std::string{*s};
  return std::nullopt;
}

std::optional<int> RecInt(const meta::RecordObject& rec,
                          std::string_view str_name) {
  if (const auto v = sim::RecordFieldInt(rec, str_name))
    return static_cast<int>(*v);
  return std::nullopt;
}

std::optional<std::int64_t> RecBits(const meta::RecordObject& rec,
                                    std::string_view str_name) {
  if (const auto* gv = sim::RecordFieldValue(rec, str_name))
    if (auto* bits = std::get_if<std::int64_t>(&gv->val))
      return *bits;
  return std::nullopt;
}

/// WDist 元组槽读取 | the WDist tuple-slot read.
std::optional<WDist> RecWDist(const meta::RecordObject& rec,
                              std::string_view str_name) {
  if (const auto* gv = sim::RecordFieldValue(rec, str_name))
    if (const auto* tuple = std::get_if<meta::GenericTuple>(&gv->val))
      if (tuple->uint1_count >= 1)
        return WDist{static_cast<int>(tuple->arr_ints[0])};
  return std::nullopt;
}

std::optional<core::Color> RecColor(const meta::RecordObject& rec,
                                    std::string_view str_name) {
  if (const auto bits = RecBits(rec, str_name))
    return core::Color::FromArgbRaw(static_cast<std::uint32_t>(*bits));
  return std::nullopt;
}

}  // namespace

void RegisterCommonTraitsBatch7() {
  static const bool b_registered = [] {
    sim::TraitRegistry& registry = sim::TraitRegistry::Instance();

    // ———— Conditions 简单授予面 ————

    registry.Register(
        "GrantConditionInfo",
        [](const meta::RecordObject& rec_info, ActorInitializer&,
           ora::WorldArena& arena) -> TraitBase* {
          GrantConditionInfoData data =
              GrantConditionInfoData::Parse(rec_info);
          return arena.Create<GrantCondition>(std::move(data));
        });

    registry.Register(
        "GrantConditionOnTileSetInfo",
        [](const meta::RecordObject& rec_info, ActorInitializer&,
           ora::WorldArena& arena) -> TraitBase* {
          return arena.Create<GrantConditionOnTileSet>(
              RecString(rec_info, "Condition").value_or(""),
              RecStringArray(rec_info, "TileSets"));
        });

    registry.Register(
        "GrantRandomConditionInfo",
        [](const meta::RecordObject& rec_info, ActorInitializer&,
           ora::WorldArena& arena) -> TraitBase* {
          return arena.Create<GrantRandomCondition>(
              RecStringArray(rec_info, "Conditions"));
        });

    registry.Register(
        "GrantConditionWhileAimingInfo",
        [](const meta::RecordObject& rec_info, ActorInitializer&,
           ora::WorldArena& arena) -> TraitBase* {
          return arena.Create<GrantConditionWhileAiming>(
              RecString(rec_info, "Condition").value_or(""));
        });

    // ———— Conditions 伤害/生命/地形面 ————

    registry.Register(
        "GrantConditionOnDamageStateInfo",
        [](const meta::RecordObject& rec_info, ActorInitializer&,
           ora::WorldArena& arena) -> TraitBase* {
          sim::DamageState valid_states =
              static_cast<sim::DamageState>(
                  static_cast<std::int32_t>(sim::DamageState::Heavy) |
                  static_cast<std::int32_t>(sim::DamageState::Critical));
          if (const auto bits =
                  RecBits(rec_info, "ValidDamageStates"))
            valid_states = static_cast<sim::DamageState>(*bits);
          return arena.Create<GrantConditionOnDamageState>(
              RecString(rec_info, "Condition").value_or(""),
              RecStringArray(rec_info, "EnabledSounds"),
              RecStringArray(rec_info, "DisabledSounds"), valid_states,
              RecInt(rec_info, "GrantPermanently").value_or(0) != 0);
        });

    registry.Register(
        "GrantConditionOnHealthInfo",
        [](const meta::RecordObject& rec_info, ActorInitializer& init,
           ora::WorldArena& arena) -> TraitBase* {
          auto* trait = arena.Create<GrantConditionOnHealth>(
              RecString(rec_info, "Condition").value_or(""),
              RecStringArray(rec_info, "EnabledSounds"),
              RecStringArray(rec_info, "DisabledSounds"),
              RecInt(rec_info, "MinHP").value_or(0),
              RecInt(rec_info, "MaxHP").value_or(0),
              RecInt(rec_info, "GrantPermanently").value_or(0) != 0);
          // RulesetLoaded 的 MinHP 校验 = 工厂时点(HealthInfo.MaxHP)
          // The RulesetLoaded MinHP validation at factory time
          // (HealthInfo.MaxHP).
          if (init.Self().Info() != nullptr)
            for (const meta::RecordObject* rec_trait :
                 init.Self().Info()->TraitsInConstructOrder())
              if (rec_trait->record_desc().str_full_name ==
                  std::string_view{
                      "OpenRA.Mods.Common.Traits.HealthInfo"}) {
                trait->Validate(static_cast<int>(
                    sim::RecordFieldInt(*rec_trait, "MaxHP")
                        .value_or(0)));
                break;
              }
          return trait;
        });

    registry.Register(
        "GrantConditionOnTerrainInfo",
        [](const meta::RecordObject& rec_info, ActorInitializer&,
           ora::WorldArena& arena) -> TraitBase* {
          return arena.Create<GrantConditionOnTerrain>(
              RecString(rec_info, "Condition").value_or(""),
              RecStringArray(rec_info, "TerrainTypes"));
        });

    // ———— Conditions 玩家/阵营面 ————

    registry.Register(
        "GrantConditionOnBotOwnerInfo",
        [](const meta::RecordObject& rec_info, ActorInitializer&,
           ora::WorldArena& arena) -> TraitBase* {
          return arena.Create<GrantConditionOnBotOwner>(
              RecString(rec_info, "Condition").value_or(""),
              RecStringArray(rec_info, "Bots"));
        });

    registry.Register(
        "GrantConditionOnCombatantOwnerInfo",
        [](const meta::RecordObject& rec_info, ActorInitializer&,
           ora::WorldArena& arena) -> TraitBase* {
          return arena.Create<GrantConditionOnCombatantOwner>(
              RecString(rec_info, "Condition").value_or(""));
        });

    registry.Register(
        "GrantConditionOnPlayerResourcesInfo",
        [](const meta::RecordObject& rec_info, ActorInitializer&,
           ora::WorldArena& arena) -> TraitBase* {
          return arena.Create<GrantConditionOnPlayerResources>(
              RecString(rec_info, "Condition").value_or(""),
              RecInt(rec_info, "Threshold").value_or(0));
        });

    registry.Register(
        "GrantConditionOnFactionInfo",
        [](const meta::RecordObject& rec_info, ActorInitializer& init,
           ora::WorldArena& arena) -> TraitBase* {
          return arena.Create<GrantConditionOnFaction>(
              init, RecString(rec_info, "Condition").value_or(""),
              RecStringArray(rec_info, "Factions"),
              RecInt(rec_info, "ResetOnOwnerChange").value_or(0) != 0,
              sim::ConditionalTraitData::Parse(rec_info));
        });

    registry.Register(
        "GrantConditionOnPowerStateInfo",
        [](const meta::RecordObject& rec_info, ActorInitializer&,
           ora::WorldArena& arena) -> TraitBase* {
          std::int32_t valid_states = 2 | 4;  // Low | Critical
          if (const auto bits =
                  RecBits(rec_info, "ValidPowerStates"))
            valid_states = static_cast<std::int32_t>(*bits);
          return arena.Create<GrantConditionOnPowerState>(
              RecString(rec_info, "Condition").value_or(""),
              valid_states,
              sim::ConditionalTraitData::Parse(rec_info));
        });

    // ———— Conditions 移动/攻击/生产面 ————

    registry.Register(
        "GrantConditionOnMovementInfo",
        [](const meta::RecordObject& rec_info, ActorInitializer& init,
           ora::WorldArena& arena) -> TraitBase* {
          sim::MovementType valid_types =
              sim::MovementType::Horizontal;
          if (const auto bits =
                  RecBits(rec_info, "ValidMovementTypes"))
            valid_types = static_cast<sim::MovementType>(*bits);
          return arena.Create<GrantConditionOnMovement>(
              init.Self(),
              RecString(rec_info, "Condition").value_or(""),
              valid_types,
              sim::ConditionalTraitData::Parse(rec_info));
        });

    registry.Register(
        "GrantConditionOnAttackInfo",
        [](const meta::RecordObject& rec_info, ActorInitializer&,
           ora::WorldArena& arena) -> TraitBase* {
          GrantConditionOnAttack::InfoData data;
          data.str_condition =
              RecString(rec_info, "Condition").value_or("");
          if (const auto names =
                  RecStringArray(rec_info, "ArmamentNames");
              !names.empty())
            data.vec_armament_names = names;
          if (const auto* gv = sim::RecordFieldValue(
                  rec_info, "RequiredShotsPerInstance"))
            if (const auto* arr =
                    std::get_if<std::vector<meta::GenericValue>>(
                        &gv->val)) {
              data.vec_required_shots_per_instance.clear();
              for (const auto& element : *arr)
                if (const auto* num =
                        std::get_if<std::int64_t>(&element.val))
                  data.vec_required_shots_per_instance.push_back(
                      static_cast<int>(*num));
            }
          data.int4_maximum_instances =
              RecInt(rec_info, "MaximumInstances").value_or(1);
          data.b_is_cyclic =
              RecInt(rec_info, "IsCyclic").value_or(0) != 0;
          data.int4_revoke_delay =
              RecInt(rec_info, "RevokeDelay").value_or(15);
          data.b_revoke_on_new_target =
              RecInt(rec_info, "RevokeOnNewTarget").value_or(0) != 0;
          data.b_revoke_all =
              RecInt(rec_info, "RevokeAll").value_or(0) != 0;
          data.conditional =
              sim::ConditionalTraitData::Parse(rec_info);
          return arena.Create<GrantConditionOnAttack>(std::move(data));
        });

    registry.Register(
        "GrantConditionOnProductionInfo",
        [](const meta::RecordObject& rec_info, ActorInitializer&,
           ora::WorldArena& arena) -> TraitBase* {
          // Color.Magenta 的上游常量值 | Color.Magenta's upstream value.
          core::Color bar_color =
              core::Color::FromArgb(0xFFFF00FFu);
          if (const auto color =
                  RecColor(rec_info, "SelectionBarColor"))
            bar_color = *color;
          return arena.Create<GrantConditionOnProduction>(
              RecString(rec_info, "Condition").value_or(""),
              RecStringArray(rec_info, "Actors"),
              RecInt(rec_info, "Duration").value_or(-1),
              RecInt(rec_info, "ShowSelectionBar").value_or(1) != 0,
              bar_color);
        });

    registry.Register(
        "GrantConditionOnPrerequisiteInfo",
        [](const meta::RecordObject& rec_info, ActorInitializer&,
           ora::WorldArena& arena) -> TraitBase* {
          return arena.Create<GrantConditionOnPrerequisite>(
              RecString(rec_info, "Condition").value_or(""),
              RecStringArray(rec_info, "Prerequisites"));
        });

    registry.Register(
        "GrantConditionOnPrerequisiteManagerInfo",
        [](const meta::RecordObject&, ActorInitializer& init,
           ora::WorldArena& arena) -> TraitBase* {
          return arena.Create<GrantConditionOnPrerequisiteManager>(
              init);
        });

    // ———— Conditions 扩散/外部授予/开关面 ————

    registry.Register(
        "SpreadsConditionInfo",
        [](const meta::RecordObject& rec_info, ActorInitializer&,
           ora::WorldArena& arena) -> TraitBase* {
          return arena.Create<SpreadsCondition>(
              RecInt(rec_info, "Probability").value_or(5),
              RecWDist(rec_info, "Range").value_or(
                  WDist{WDist::FromCells(3)}),
              RecString(rec_info, "SpreadCondition")
                  .value_or("spreading"),
              RecInt(rec_info, "Delay").value_or(5),
              sim::ConditionalTraitData::Parse(rec_info));
        });

    registry.Register(
        "GrantExternalConditionToProducedInfo",
        [](const meta::RecordObject& rec_info, ActorInitializer&,
           ora::WorldArena& arena) -> TraitBase* {
          return arena.Create<GrantExternalConditionToProduced>(
              RecString(rec_info, "Condition").value_or(""),
              RecInt(rec_info, "Duration").value_or(0),
              sim::ConditionalTraitData::Parse(rec_info));
        });

    registry.Register(
        "GrantExternalConditionToCrusherInfo",
        [](const meta::RecordObject& rec_info, ActorInitializer&,
           ora::WorldArena& arena) -> TraitBase* {
          return arena.Create<GrantExternalConditionToCrusher>(
              RecString(rec_info, "Condition").value_or(""),
              RecInt(rec_info, "Duration").value_or(0),
              sim::ConditionalTraitData::Parse(rec_info));
        });

    registry.Register(
        "ToggleConditionOnOrderInfo",
        [](const meta::RecordObject& rec_info, ActorInitializer&,
           ora::WorldArena& arena) -> TraitBase* {
          return arena.Create<ToggleConditionOnOrder>(
              RecString(rec_info, "Condition").value_or(""),
              RecString(rec_info, "OrderName").value_or(""),
              sim::ConditionalTraitData::Parse(rec_info));
        });

    registry.Register(
        "GrantConditionOnClientDockInfo",
        [](const meta::RecordObject& rec_info, ActorInitializer&,
           ora::WorldArena& arena) -> TraitBase* {
          return arena.Create<GrantConditionOnClientDock>(
              RecString(rec_info, "Condition").value_or(""),
              RecStringArray(rec_info, "DockHostNames"),
              RecInt(rec_info, "AfterDockDuration").value_or(0));
        });

    registry.Register(
        "GrantConditionOnHostDockInfo",
        [](const meta::RecordObject& rec_info, ActorInitializer&,
           ora::WorldArena& arena) -> TraitBase* {
          return arena.Create<GrantConditionOnHostDock>(
              RecString(rec_info, "Condition").value_or(""),
              RecStringArray(rec_info, "DockClientNames"),
              RecInt(rec_info, "AfterDockDuration").value_or(0));
        });

    // BodyOrientationInfo 已在第一批注册(minv 等地图 actor 的裸
    // BodyOrientation 名直接命中);此处不重复 —— 注释锚定
    // BodyOrientationInfo is registered since batch 1 (the bare
    // BodyOrientation name of map actors like minv hits it directly);
    // no duplicate here — the comment anchors the fact.

    registry.Register(
        "GrantConditionOnDeployInfo",
        [](const meta::RecordObject& rec_info, ActorInitializer& init,
           ora::WorldArena& arena) -> TraitBase* {
          GrantConditionOnDeploy::InfoData data;
          data.str_undeployed_condition =
              RecString(rec_info, "UndeployedCondition").value_or("");
          data.str_deployed_condition =
              RecString(rec_info, "DeployedCondition").value_or("");
          data.vec_allowed_terrain_types =
              RecStringArray(rec_info, "AllowedTerrainTypes");
          data.b_can_deploy_on_ramps =
              RecInt(rec_info, "CanDeployOnRamps").value_or(0) != 0;
          data.b_smart_deploy =
              RecInt(rec_info, "SmartDeploy").value_or(0) != 0;
          if (const auto* gv =
                  sim::RecordFieldValue(rec_info, "Facing"))
            if (const auto* tuple =
                    std::get_if<meta::GenericTuple>(&gv->val))
              if (tuple->uint1_count >= 1)
                data.opt_facing =
                    WAngle{static_cast<int>(tuple->arr_ints[0])};
          data.b_skip_make_animation =
              RecInt(rec_info, "SkipMakeAnimation").value_or(0) != 0;
          data.b_undeploy_on_move =
              RecInt(rec_info, "UndeployOnMove").value_or(0) != 0;
          data.b_undeploy_on_pickup =
              RecInt(rec_info, "UndeployOnPickup").value_or(0) != 0;
          data.conditional =
              sim::ConditionalTraitData::Parse(rec_info);
          return arena.Create<GrantConditionOnDeploy>(init,
                                                      std::move(data));
        });

    registry.Register(
        "GrantChargedConditionOnToggleInfo",
        [](const meta::RecordObject& rec_info, ActorInitializer&,
           ora::WorldArena& arena) -> TraitBase* {
          GrantChargedConditionOnToggle::InfoData data;
          data.str_activated_condition =
              RecString(rec_info, "ActivatedCondition").value_or("");
          data.str_charged_condition =
              RecString(rec_info, "ChargedCondition").value_or("");
          data.int4_initial_charge =
              RecInt(rec_info, "InitialCharge").value_or(-1);
          data.int4_charge_duration =
              RecInt(rec_info, "ChargeDuration").value_or(500);
          data.int4_charge_threshhold =
              RecInt(rec_info, "ChargeThreshhold").value_or(-1);
          data.int4_condition_duration =
              RecInt(rec_info, "ConditionDuration").value_or(1);
          data.b_can_cancel_condition =
              RecInt(rec_info, "CanCancelCondition").value_or(0) != 0;
          data.b_cancels_current_activity =
              RecInt(rec_info, "CancelsCurrentActivity").value_or(0) !=
              0;
          data.color_deactivated =
              RecColor(rec_info, "DeactivatedColor")
                  .value_or(core::Color::FromArgb(0xFFFF00FFu));
          data.color_activated =
              RecColor(rec_info, "ActivatedColor")
                  .value_or(core::Color::FromArgb(0xFF8B008Bu));
          data.b_display_bar_when_empty =
              RecInt(rec_info, "DisplayBarWhenEmpty").value_or(1) != 0;
          data.conditional =
              sim::ConditionalTraitData::Parse(rec_info);
          data.Validate();
          return arena.Create<GrantChargedConditionOnToggle>(
              std::move(data));
        });

    // ———— Conditions 自定义层触发面(ValidLayerType 注入恒 0 = 默认层域,
    //      Subterranean/Tunnel 层 trait 随其宿主批)————
    registry.Register(
        "GrantConditionOnSubterraneanLayerInfo",
        [](const meta::RecordObject& rec_info, ActorInitializer&,
           ora::WorldArena& arena) -> TraitBase* {
          return arena.Create<GrantConditionOnSubterraneanLayer>(
              RecString(rec_info, "Condition").value_or(""), 0,
              sim::ConditionalTraitData::Parse(rec_info));
        });

    registry.Register(
        "GrantConditionOnTunnelLayerInfo",
        [](const meta::RecordObject& rec_info, ActorInitializer&,
           ora::WorldArena& arena) -> TraitBase* {
          return arena.Create<GrantConditionOnTunnelLayer>(
              RecString(rec_info, "Condition").value_or(""), 0,
              sim::ConditionalTraitData::Parse(rec_info));
        });

    // ———— ExternalCondition 双件 ————

    registry.Register(
        "ExternalConditionInfo",
        [](const meta::RecordObject& rec_info, ActorInitializer&,
           ora::WorldArena& arena) -> TraitBase* {
          return arena.Create<ExternalCondition>(
              RecString(rec_info, "Condition").value_or(""),
              RecInt(rec_info, "SourceCap").value_or(0),
              RecInt(rec_info, "TotalCap").value_or(0));
        });

    registry.Register(
        "ProximityExternalConditionInfo",
        [](const meta::RecordObject& rec_info, ActorInitializer& init,
           ora::WorldArena& arena) -> TraitBase* {
          sim::PlayerRelationship valid =
              sim::PlayerRelationship::Ally;
          if (const auto bits =
                  RecBits(rec_info, "ValidRelationships"))
            valid = static_cast<sim::PlayerRelationship>(*bits);
          return arena.Create<ProximityExternalCondition>(
              init.Self(),
              RecString(rec_info, "Condition").value_or(""),
              RecWDist(rec_info, "Range").value_or(
                  WDist{WDist::FromCells(3)}),
              RecWDist(rec_info, "MaximumVerticalOffset")
                  .value_or(WDist{0}),
              valid,
              RecInt(rec_info, "AffectsParent").value_or(0) != 0,
              sim::ConditionalTraitData::Parse(rec_info));
        });

    // ———— Cloak 三件 ————

    registry.Register(
        "CloakInfo",
        [](const meta::RecordObject& rec_info, ActorInitializer&,
           ora::WorldArena& arena) -> TraitBase* {
          return arena.Create<Cloak>(CloakInfoData::Parse(rec_info));
        });

    registry.Register(
        "DetectCloakedInfo",
        [](const meta::RecordObject& rec_info, ActorInitializer&,
           ora::WorldArena& arena) -> TraitBase* {
          // 值袋默认槽 = BitSet("Cloak") 的 raw bits(schema 导出已物化)
          // The bag's default slot = BitSet("Cloak")'s raw bits
          // (materialized at schema export).
          core::BitSet<DetectionType> detection_types{};
          if (const auto bits =
                  RecBits(rec_info, "DetectionTypes"))
            detection_types =
                core::BitSet<DetectionType>::FromRawBits(
                    static_cast<std::uint64_t>(*bits));
          return arena.Create<DetectCloaked>(
              std::move(detection_types),
              RecWDist(rec_info, "Range").value_or(
                  WDist{WDist::FromCells(5)}),
              sim::ConditionalTraitData::Parse(rec_info));
        });

    registry.Register(
        "IgnoresCloakInfo",
        [](const meta::RecordObject&, ActorInitializer&,
           ora::WorldArena& arena) -> TraitBase* {
          return arena.Create<IgnoresCloak>();
        });

    // ———— Experience 四件 ————

    registry.Register(
        "GainsExperienceInfo",
        [](const meta::RecordObject& rec_info, ActorInitializer& init,
           ora::WorldArena& arena) -> TraitBase* {
          return arena.Create<GainsExperience>(
              init, GainsExperienceInfoData::Parse(rec_info));
        });

    registry.Register(
        "GivesExperienceInfo",
        [](const meta::RecordObject& rec_info, ActorInitializer&,
           ora::WorldArena& arena) -> TraitBase* {
          return arena.Create<GivesExperience>(
              GivesExperienceInfoData::Parse(rec_info));
        });

    registry.Register(
        "GainsExperienceMultiplierInfo",
        [](const meta::RecordObject& rec_info, ActorInitializer&,
           ora::WorldArena& arena) -> TraitBase* {
          return arena.Create<GainsExperienceMultiplier>(
              RecInt(rec_info, "Modifier").value_or(100),
              sim::ConditionalTraitData::Parse(rec_info));
        });

    registry.Register(
        "PlayerExperienceInfo",
        [](const meta::RecordObject&, ActorInitializer&,
           ora::WorldArena& arena) -> TraitBase* {
          return arena.Create<PlayerExperience>();
        });

    // ———— Capture 四件 ————

    registry.Register(
        "CaptureManagerInfo",
        [](const meta::RecordObject& rec_info, ActorInitializer& init,
           ora::WorldArena& arena) -> TraitBase* {
          return arena.Create<CaptureManager>(
              init, CaptureManagerInfoData::Parse(rec_info));
        });

    registry.Register(
        "CapturableInfo",
        [](const meta::RecordObject& rec_info, ActorInitializer& init,
           ora::WorldArena& arena) -> TraitBase* {
          return arena.Create<Capturable>(
              init, CapturableInfoData::Parse(rec_info));
        });

    registry.Register(
        "CapturesInfo",
        [](const meta::RecordObject& rec_info, ActorInitializer& init,
           ora::WorldArena& arena) -> TraitBase* {
          return arena.Create<Captures>(
              init, CapturesInfoData::Parse(rec_info));
        });

    registry.Register(
        "GivesCashOnCaptureInfo",
        [](const meta::RecordObject& rec_info, ActorInitializer&,
           ora::WorldArena& arena) -> TraitBase* {
          return arena.Create<GivesCashOnCapture>(
              GivesCashOnCaptureInfoData::Parse(rec_info),
              sim::ConditionalTraitData::Parse(rec_info));
        });

    // ———— Selectable/Interactable/SpawnMapActors ————

    registry.Register(
        "SelectableInfo",
        [](const meta::RecordObject& rec_info, ActorInitializer& init,
           ora::WorldArena& arena) -> TraitBase* {
          return arena.Create<Selectable>(
              init.Self(), SelectableInfoData::Parse(rec_info));
        });

    registry.Register(
        "InteractableInfo",
        [](const meta::RecordObject& rec_info, ActorInitializer&,
           ora::WorldArena& arena) -> TraitBase* {
          return arena.Create<Interactable>(
              InteractableInfoData::Parse(rec_info));
        });

    registry.Register(
        "SpawnMapActorsInfo",
        [](const meta::RecordObject&, ActorInitializer&,
           ora::WorldArena& arena) -> TraitBase* {
          return arena.Create<SpawnMapActors>();
        });

    // InitRegistry 装配(from-yaml 工厂;ActorReference.LoadInit 面)
    // InitRegistry's assembly (the from-yaml factories; the
    // ActorReference.LoadInit face).
    RegisterCommonActorInits();

    return true;
  }();
  (void)b_registered;
}

// ———— 第七批 [VerifySync] 哈希注册(gen/sync_gen.cpp 成员表:
//      Cloak {remainingTime} / GainsExperience {Experience,Level} /
//      GrantChargedConditionOnToggle {chargeTick} /
//      GrantConditionOnClientDock {Duration} /
//      GrantConditionOnHostDock {Duration} /
//      GrantConditionOnProduction {ticks} /
//      ToggleConditionOnOrder {enabled} /
//      PlayerExperience {Experience})————
// ———— The batch-7 [VerifySync] hash registrations (the gen/sync_gen.cpp
//      member tables: Cloak {remainingTime} / GainsExperience
//      {Experience,Level} / GrantChargedConditionOnToggle {chargeTick} /
//      GrantConditionOnClientDock {Duration} / GrantConditionOnHostDock
//      {Duration} / GrantConditionOnProduction {ticks} /
//      ToggleConditionOnOrder {enabled} / PlayerExperience
//      {Experience}) ————

namespace {

int CloakSyncHash(const sim::ISync* s) {
  const auto* cloak = static_cast<const Cloak*>(s);
  return sim::sync::CombineSyncHash(0, cloak->RemainingTime);
}

int GainsExperienceSyncHash(const sim::ISync* s) {
  const auto* gains = static_cast<const GainsExperience*>(s);
  return sim::sync::CombineSyncHash(
      sim::sync::CombineSyncHash(0, gains->Experience), gains->Level);
}

int GrantChargedConditionOnToggleSyncHash(const sim::ISync* s) {
  const auto* toggle =
      static_cast<const GrantChargedConditionOnToggle*>(s);
  return sim::sync::CombineSyncHash(0, toggle->ChargeTick);
}

int GrantConditionOnClientDockSyncHash(const sim::ISync* s) {
  const auto* dock = static_cast<const GrantConditionOnClientDock*>(s);
  return sim::sync::CombineSyncHash(0, dock->Duration);
}

int GrantConditionOnHostDockSyncHash(const sim::ISync* s) {
  const auto* dock = static_cast<const GrantConditionOnHostDock*>(s);
  return sim::sync::CombineSyncHash(0, dock->Duration);
}

int GrantConditionOnProductionSyncHash(const sim::ISync* s) {
  const auto* production =
      static_cast<const GrantConditionOnProduction*>(s);
  return sim::sync::CombineSyncHash(0, production->Ticks);
}

int ToggleConditionOnOrderSyncHash(const sim::ISync* s) {
  const auto* toggle = static_cast<const ToggleConditionOnOrder*>(s);
  return sim::sync::CombineSyncHash(0, sim::sync::HashBool(toggle->Enabled));
}

int PlayerExperienceSyncHash(const sim::ISync* s) {
  const auto* experience = static_cast<const PlayerExperience*>(s);
  return sim::sync::CombineSyncHash(0, experience->Experience);
}

const bool b_registered_batch7_sync_hash = [] {
  sim::RegisterSyncHashFunction("OpenRA.Mods.Common.Traits.Cloak",
                                &CloakSyncHash);
  sim::RegisterSyncHashFunction(
      "OpenRA.Mods.Common.Traits.GainsExperience",
      &GainsExperienceSyncHash);
  sim::RegisterSyncHashFunction(
      "OpenRA.Mods.Common.Traits.GrantChargedConditionOnToggle",
      &GrantChargedConditionOnToggleSyncHash);
  sim::RegisterSyncHashFunction(
      "OpenRA.Mods.Common.Traits.GrantConditionOnClientDock",
      &GrantConditionOnClientDockSyncHash);
  sim::RegisterSyncHashFunction(
      "OpenRA.Mods.Common.Traits.GrantConditionOnHostDock",
      &GrantConditionOnHostDockSyncHash);
  sim::RegisterSyncHashFunction(
      "OpenRA.Mods.Common.Traits.GrantConditionOnProduction",
      &GrantConditionOnProductionSyncHash);
  sim::RegisterSyncHashFunction(
      "OpenRA.Mods.Common.Traits.ToggleConditionOnOrder",
      &ToggleConditionOnOrderSyncHash);
  sim::RegisterSyncHashFunction(
      "OpenRA.Mods.Common.Traits.PlayerExperience",
      &PlayerExperienceSyncHash);
  return true;
}();

}  // namespace

}  // namespace ora::mods
