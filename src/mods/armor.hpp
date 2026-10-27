#pragma once
import std;

#include "meta/generic_record.hpp"
#include "sim/actor.hpp"
#include "sim/conditional_trait.hpp"
#include "sim/trait_registry.hpp"

namespace ora::mods {

using sim::Actor;
using sim::ActorInitializer;
using sim::TraitBase;

struct ArmorInfoData {
  std::string str_type;

  sim::ConditionalTraitData conditional;

  static ArmorInfoData Parse(const meta::RecordObject& rec_info);
};

class Armor final : public TraitBase,
                    public sim::ConditionalTraitCore<Armor> {
 public:
  Armor(const ArmorInfoData& info)
      : sim::ConditionalTraitCore<Armor>(info.conditional), info_{info} {}

  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_Armor;
  gen::TypeId GetTraitTypeId() const override { return kTypeId; }
  static constexpr auto kTraitUpcasts =
      ora::sim::MakeTraitUpcasts<Armor>();
  std::span<const ora::sim::TraitUpcastEntry> TraitUpcasts() const override {
    return kTraitUpcasts;
  }

  bool IsTraitEnabled() const override {
    return !ConditionalTraitCore<Armor>::IsTraitDisabled();
  }
  bool IsTraitDisabled() const override {
    return ConditionalTraitCore<Armor>::IsTraitDisabled();
  }

  const ArmorInfoData& Info() const { return info_; }

  void TraitEnabledHook(Actor& ) {}
  void TraitDisabledHook(Actor& ) {}
  void TraitResumedHook(Actor& ) {}
  void TraitPausedHook(Actor& ) {}

 private:
  friend class sim::ConditionalTraitCore<Armor>;
  ArmorInfoData info_;
};

}
