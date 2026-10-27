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
  int int4_new_options_notification_delay = 10;
  std::string str_new_options_notification;
  std::string str_new_options_text_notification;
  std::string str_cannot_place_notification;
  std::string str_cannot_place_text_notification;
  ui::HotkeyReference hk_toggle_variant_key;

  static PlaceBuildingInfoData Parse(const meta::RecordObject& rec_info);
};

class PlaceBuilding final : public sim::TraitBase,
                            public sim::IResolveOrder,
                            public sim::ITick {
 public:
  ORA_TRAIT_INTERFACES(PlaceBuilding, OpenRA_Mods_Common_Traits_PlaceBuilding,
                       sim::IResolveOrder, sim::ITick)

  PlaceBuilding(sim::ActorInitializer& init, PlaceBuildingInfoData info);

  void ResolveOrder(sim::Actor& self,
                    const ora::net::Order& order) override;
  void Tick(sim::Actor& self) override;

  const PlaceBuildingInfoData& Info() const { return info_; }
  const ui::HotkeyReference& ToggleVariantKey() const {
    return hk_toggle_variant_key;
  }

 private:

  void PlayNotification(sim::Actor& self);

  static int GetNumBuildables(sim::Player& p);

  PlaceBuildingInfoData info_;
  ui::HotkeyReference hk_toggle_variant_key;
  sim::Actor* p_self_ = nullptr;
  bool b_trigger_notification = false;
  int int4_tick_ = 0;
};

struct PlaceBuildingVariantsInfoData {
  std::vector<std::string> vec_actors;
  std::vector<WAngle> vec_facings;

  static PlaceBuildingVariantsInfoData Parse(const meta::RecordObject& rec_info);
};

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
  std::vector<std::string> vec_replaceable_types;

  static ReplacementInfoData Parse(const meta::RecordObject& rec_info);
};

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
  std::vector<std::string> vec_types;
  sim::ConditionalTraitData conditional;

  static ReplaceableInfoData Parse(const meta::RecordObject& rec_info);
};

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

}
