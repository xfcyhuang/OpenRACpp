// UPSTREAM: OpenRA.Mods.Common/Traits/Player/PlaceBuilding.cs @1f594e320
//          L18-268 全文 + Traits/Buildings/PlaceBuildingVariants.cs +
//          Traits/Replacement.cs + Traits/Replaceable.cs
//          The whole of PlaceBuilding.cs + PlaceBuildingVariants.cs +
//          Replacement.cs + Replaceable.cs.
#pragma once
import std;

#include "core/wangle.hpp"
#include "sim/actor.hpp"
#include "ui/hotkey.hpp"
#include "sim/conditional_trait.hpp"
#include "sim/trait_interfaces.hpp"

namespace ora::meta {
class RecordObject;
}

namespace ora::mods {

/// PlaceBuildingInit(L18;RuntimeFlagInit 标记 —— 第三方 mod 检测面)
/// PlaceBuildingInit (L18; the RuntimeFlagInit marker — the third-party
/// mod detection face).
class PlaceBuildingInit final : public sim::ActorInit,
                                public sim::ISingleInstanceInit {
 public:
  ORA_INIT_TYPE(PlaceBuildingInit, OpenRA_Mods_Common_Traits_PlaceBuildingInit,
                gen::TypeId::OpenRA_ISingleInstanceInit)
  PlaceBuildingInit() = default;

  std::span<const ora::sim::TraitUpcastEntry> TraitUpcasts() const override {
    return {};
  }
};

struct PlaceBuildingInfoData {
  int int4_new_options_notification_delay = 10;  // L25
  std::string str_new_options_notification;      // L29
  std::string str_new_options_text_notification;  // L33
  std::string str_cannot_place_notification;     // L37
  std::string str_cannot_place_text_notification;  // L41
  ui::HotkeyReference hk_toggle_variant_key;     // L44

  static PlaceBuildingInfoData Parse(const meta::RecordObject& rec_info);
};

/// PlaceBuilding(L49-266;player actor 挂载)
/// PlaceBuilding (L49-266; mounted on the player actor).
class PlaceBuilding final : public sim::TraitBase,
                            public sim::IResolveOrder,
                            public sim::ITick {
 public:
  ORA_TRAIT_INTERFACES(PlaceBuilding, OpenRA_Mods_Common_Traits_PlaceBuilding,
                       sim::IResolveOrder, sim::ITick)

  PlaceBuilding(sim::ActorInitializer& init, PlaceBuildingInfoData info);

  void ResolveOrder(sim::Actor& self,
                    const ora::net::Order& order) override;  // L60-237
  void Tick(sim::Actor& self) override;                      // L239-246

  const PlaceBuildingInfoData& Info() const { return info_; }
  const ui::HotkeyReference& ToggleVariantKey() const {
    return hk_toggle_variant_key;
  }

 private:
  /// L248-255 | L248-255.
  void PlayNotification(sim::Actor& self);

  /// L257-266 | L257-266.
  static int GetNumBuildables(sim::Player& p);

  PlaceBuildingInfoData info_;
  ui::HotkeyReference hk_toggle_variant_key;
  sim::Actor* p_self_ = nullptr;
  bool b_trigger_notification = false;  // L52
  int int4_tick_ = 0;                   // L53
};

struct PlaceBuildingVariantsInfoData {
  std::vector<std::string> vec_actors;  // L25
  std::vector<WAngle> vec_facings;      // L28

  static PlaceBuildingVariantsInfoData Parse(const meta::RecordObject& rec_info);
};

/// PlaceBuildingVariants(L30;空运行时类)
/// PlaceBuildingVariants (L30; the empty runtime class).
class PlaceBuildingVariants final : public sim::TraitBase {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_PlaceBuildingVariants;
  gen::TypeId GetTraitTypeId() const override { return kTypeId; }
  static constexpr auto kTraitUpcasts =
      ora::sim::MakeTraitUpcasts<PlaceBuildingVariants>();
  std::span<const ora::sim::TraitUpcastEntry> TraitUpcasts() const override {
    return kTraitUpcasts;
  }
};

struct ReplacementInfoData {
  std::vector<std::string> vec_replaceable_types;  // L23

  static ReplacementInfoData Parse(const meta::RecordObject& rec_info);
};

/// Replacement(L24;空运行时类)
/// Replacement (L24; the empty runtime class).
class Replacement final : public sim::TraitBase {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_Replacement;
  gen::TypeId GetTraitTypeId() const override { return kTypeId; }
  static constexpr auto kTraitUpcasts =
      ora::sim::MakeTraitUpcasts<Replacement>();
  std::span<const ora::sim::TraitUpcastEntry> TraitUpcasts() const override {
    return kTraitUpcasts;
  }
};

struct ReplaceableInfoData {
  std::vector<std::string> vec_types;  // L23
  sim::ConditionalTraitData conditional;

  static ReplaceableInfoData Parse(const meta::RecordObject& rec_info);
};

/// Replaceable(L27;ConditionalTrait 空实现)
/// Replaceable (L27; ConditionalTrait with an empty body).
class Replaceable final : public sim::TraitBase,
                          public sim::IObservesVariables,
                          public sim::INotifyCreated,
                          public sim::ConditionalTraitCore<Replaceable> {
 public:
  ORA_TRAIT_INTERFACES(Replaceable, OpenRA_Mods_Common_Traits_Replaceable,
                       sim::IObservesVariables, sim::INotifyCreated)

  Replaceable(sim::ActorInitializer& init, ReplaceableInfoData info);

  bool IsTraitDisabled() const {
    return sim::ConditionalTraitCore<Replaceable>::IsTraitDisabled();
  }
  bool IsTraitPaused() const {
    return sim::ConditionalTraitCore<Replaceable>::IsTraitPaused();
  }

  void Created(sim::Actor& self) override {
    sim::ConditionalTraitCore<Replaceable>::CoreCreated(self);
  }
  std::vector<sim::VariableObserver> GetVariableObservers() override {
    return sim::ConditionalTraitCore<Replaceable>::CollectObservers();
  }

  const ReplaceableInfoData& Info() const { return info_; }

 private:
  ReplaceableInfoData info_;
};

}  // namespace ora::mods
