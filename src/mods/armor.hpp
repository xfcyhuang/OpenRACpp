// UPSTREAM: OpenRA.Mods.Common/Traits/Armor.cs @b6fc03f L17-30 全文
//          (ArmorType 标签在 hit_shape.hpp —— BitSet 注册面的就近锚)
//          The whole of Armor.cs L17-30 (the ArmorType tag lives in
//          hit_shape.hpp — anchored next to the BitSet registration face).
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

/// ArmorInfo 的解析面(L21-25) | The parsed face of ArmorInfo (L21-25).
struct ArmorInfoData {
  std::string str_type;  // L22 Type(null 承载为空串;DamageWarhead 的
                         // ContainsKey 面 = 空串不入表)
  sim::ConditionalTraitData conditional;

  static ArmorInfoData Parse(const meta::RecordObject& rec_info);
};

/// Armor(L26-30)
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

  void TraitEnabledHook(Actor& /*self*/) {}
  void TraitDisabledHook(Actor& /*self*/) {}
  void TraitResumedHook(Actor& /*self*/) {}
  void TraitPausedHook(Actor& /*self*/) {}

 private:
  friend class sim::ConditionalTraitCore<Armor>;
  ArmorInfoData info_;
};

}  // namespace ora::mods
