#pragma once
import std;

#include "core/bitset.hpp"
#include "core/color.hpp"
#include "core/wangle.hpp"
#include "core/wdist.hpp"
#include "meta/generic_record.hpp"
#include "sim/actor.hpp"
#include "sim/conditional_trait.hpp"
#include "sim/trait_interfaces.hpp"

namespace ora::mods {

class Armament;
struct Barrel;

enum class UncloakType : std::int32_t {
  None = 0,
  Attack = 1,
  Move = 2,
  Load = 4,
  Unload = 8,
  Infiltrate = 16,
  Demolish = 32,
  Damage = 64,
  Heal = 128,
  SelfHeal = 256,
  Dock = 512,
  SupportPower = 1024,
};

inline bool HasUncloakFlag(UncloakType set, UncloakType flag) {
  return (static_cast<std::int32_t>(set) & static_cast<std::int32_t>(flag)) ==
         static_cast<std::int32_t>(flag);
}

class DetectionType {};

enum class CloakStyle : std::int32_t { None, Alpha, Color, Palette };

struct CloakInfoData {
  int int4_initial_delay = 10;
  int int4_cloak_delay = 30;
  std::int32_t int4_uncloak_on =
      static_cast<std::int32_t>(UncloakType::Attack) |
      static_cast<std::int32_t>(UncloakType::Unload) |
      static_cast<std::int32_t>(UncloakType::Infiltrate) |
      static_cast<std::int32_t>(UncloakType::Demolish) |
      static_cast<std::int32_t>(UncloakType::Dock);
  std::string str_cloak_sound;
  std::string str_uncloak_sound;
  core::BitSet<DetectionType> bitset_detection_types;
  std::string str_cloaked_condition;
  std::string str_cloak_type;
  sim::ConditionalTraitData conditional;

  static CloakInfoData Parse(const meta::RecordObject& rec);
};

class Cloak final : public sim::TraitBase,
                    public sim::IObservesVariables,
                    public sim::INotifyCreated,
                    public sim::ITick,
                    public sim::ISync,
                    public sim::INotifyDamage,
                    public sim::INotifyAttack,
                    public sim::IVisibilityModifier,
                    public sim::INotifyDockClient,
                    public sim::INotifyDockHost,
                    public sim::INotifyLoadCargo,
                    public sim::INotifyUnloadCargo,
                    public sim::INotifyDemolition,
                    public sim::INotifyInfiltration,
                    public sim::INotifySupportPower,
                    private sim::ConditionalTraitCore<Cloak> {
 public:
  ORA_TRAIT_INTERFACES(Cloak, OpenRA_Mods_Common_Traits_Cloak,
                       sim::IObservesVariables, sim::INotifyCreated,
                       sim::ITick, sim::ISync, sim::INotifyDamage,
                       sim::INotifyAttack, sim::IVisibilityModifier,
                       sim::INotifyDockClient, sim::INotifyDockHost,
                       sim::INotifyLoadCargo, sim::INotifyUnloadCargo,
                       sim::INotifyDemolition, sim::INotifyInfiltration,
                       sim::INotifySupportPower)

  explicit Cloak(CloakInfoData info);
  ~Cloak() override;

  bool IsTraitEnabled() const override { return !ConditionalTraitCore::IsTraitDisabled(); }
  bool IsTraitDisabled() const override { return ConditionalTraitCore::IsTraitDisabled(); }
  bool IsTraitPaused() const { return ConditionalTraitCore::IsTraitPaused(); }

  bool Cloaked() const {
    return !ConditionalTraitCore::IsTraitDisabled() &&
           !ConditionalTraitCore::IsTraitPaused() && RemainingTime <= 0;
  }

  void Uncloak() { UncloakFor(info_.int4_cloak_delay); }
  void UncloakFor(int time) {
    RemainingTime = std::max(RemainingTime, time);
  }

  const CloakInfoData& InfoData() const { return info_; }

  void Created(sim::Actor& self) override;
  std::vector<sim::VariableObserver> GetVariableObservers() override {
    return CollectObservers();
  }
  void Tick(sim::Actor& self) override;
  void Damaged(sim::Actor& self, const sim::AttackInfo& e) override;
  void Attacking(sim::Actor& self, const sim::Target& target,
                 Armament& armament, const Barrel& barrel) override;
  void PreparingAttack(sim::Actor&, const sim::Target&,
                       Armament&, const Barrel&) override {}
  bool IsVisible(sim::Actor& self, sim::Player* viewer) override;

  void Docked(sim::Actor& self, sim::Actor& host) override;
  void Undocked(sim::Actor& self, sim::Actor& host) override;
  void Loading(sim::Actor& self) override;
  void Unloading(sim::Actor& self) override;
  void Demolishing(sim::Actor& self) override;
  void Infiltrating(sim::Actor& self) override;
  void Charged(sim::Actor& self) override {}
  void Activated(sim::Actor& self,
                 const std::string& order_name) override;

  int RemainingTime = 0;

 private:
  friend class sim::ConditionalTraitCore<Cloak>;
  void TraitEnabledHook(sim::Actor& self);
  void TraitDisabledHook(sim::Actor& self);

  CloakInfoData info_;
  bool b_is_docking_ = false;
  std::vector<Cloak*> vec_other_cloaks_;
  std::optional<CPos> opt_last_pos_;
  bool b_was_cloaked_ = false;
  bool b_first_tick_ = true;
  int int4_cloaked_token_ = sim::Actor::InvalidConditionToken;
};

class DetectCloaked final : public sim::TraitBase,
                            public sim::IObservesVariables,
                            public sim::INotifyCreated,
                            private sim::ConditionalTraitCore<DetectCloaked> {
 public:
  ORA_TRAIT_INTERFACES(DetectCloaked, OpenRA_Mods_Common_Traits_DetectCloaked,
                       sim::IObservesVariables, sim::INotifyCreated)

  DetectCloaked(core::BitSet<DetectionType> detection_types, WDist range,
                sim::ConditionalTraitData conditional);
  ~DetectCloaked() override;

  bool IsTraitEnabled() const override { return !ConditionalTraitCore::IsTraitDisabled(); }
  bool IsTraitDisabled() const override { return ConditionalTraitCore::IsTraitDisabled(); }

  void Created(sim::Actor& self) override;
  std::vector<sim::VariableObserver> GetVariableObservers() override {
    return CollectObservers();
  }

  WDist Range() const;

  const core::BitSet<DetectionType>& DetectionTypes() const {
    return bitset_detection_types_;
  }

 private:
  friend class sim::ConditionalTraitCore<DetectCloaked>;
  core::BitSet<DetectionType> bitset_detection_types_;
  WDist range_;
  std::vector<sim::IDetectCloakedModifier*> vec_range_modifiers_;
};

class IgnoresCloak final : public sim::TraitBase {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_IgnoresCloak;
  gen::TypeId GetTraitTypeId() const override { return kTypeId; }
  static constexpr auto kTraitUpcasts =
      ora::sim::MakeTraitUpcasts<IgnoresCloak>();
  std::span<const ora::sim::TraitUpcastEntry> TraitUpcasts() const override {
    return kTraitUpcasts;
  }
};

}
