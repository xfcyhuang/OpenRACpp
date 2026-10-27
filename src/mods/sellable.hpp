#pragma once
import std;

#include "meta/generic_record.hpp"
#include "mods/player_resources.hpp"
#include "net/order.hpp"
#include "sim/actor.hpp"
#include "sim/actor_init.hpp"
#include "sim/activity.hpp"
#include "sim/conditional_trait.hpp"
#include "sim/trait_interfaces.hpp"

namespace ora::mods {

using sim::Actor;
using sim::TraitBase;

class PlayerResources;

struct CustomSellValueInfoData {
  int int4_value = 0;

  static CustomSellValueInfoData Parse(const meta::RecordObject& rec_info);
};

class CustomSellValue final : public TraitBase {
 public:
  explicit CustomSellValue(CustomSellValueInfoData info);

  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_CustomSellValue;
  gen::TypeId GetTraitTypeId() const override { return kTypeId; }
  static constexpr auto kTraitUpcasts =
      sim::MakeTraitUpcasts<CustomSellValue>();
  std::span<const sim::TraitUpcastEntry> TraitUpcasts() const override {
    return kTraitUpcasts;
  }

  const CustomSellValueInfoData& Info() const { return info_; }

 private:
  CustomSellValueInfoData info_;
};

int GetSellValue(sim::Actor& self);

struct SellableInfoData {
  sim::ConditionalTraitData conditional;
  int int4_refund_percent = 50;
  std::vector<std::string> vec_sell_sounds;
  std::string str_notification;
  std::string str_text_notification;
  bool b_show_ticks = true;
  bool b_show_tooltip_text = true;
  bool b_skip_make_animation = false;
  std::string str_cursor{"sell"};

  static SellableInfoData Parse(const meta::RecordObject& rec_info);
};

class Sellable final : public TraitBase,
                       public sim::ConditionalTraitCore<Sellable>,
                       public sim::IObservesVariables,
                       public sim::INotifyCreated,
                       public sim::IResolveOrder,
                       public sim::IProvideTooltipInfo {
 public:
  Sellable(Actor& self, SellableInfoData info);

  ORA_TRAIT_INTERFACES(Sellable, OpenRA_Mods_Common_Traits_Sellable,
                       sim::IObservesVariables, sim::INotifyCreated,
                       sim::IResolveOrder, sim::IProvideTooltipInfo)

  void Created(Actor& self) override {
    sim::ConditionalTraitCore<Sellable>::CoreCreated(self);
  }
  std::vector<sim::VariableObserver> GetVariableObservers() override {
    return CollectObservers();
  }

  bool IsTraitEnabled() const override {
    return !sim::ConditionalTraitCore<Sellable>::IsTraitDisabled();
  }
  bool IsTraitDisabled() const override {
    return sim::ConditionalTraitCore<Sellable>::IsTraitDisabled();
  }

  void ResolveOrder(Actor& self, const net::Order& order) override;

  const SellableInfoData& Info() const { return info_; }

  void Sell(Actor& self);

  bool IsTooltipVisible(sim::Player* ptr_for_player) override;
  std::string_view TooltipText() const override;

 private:
  Actor* ptr_self_ = nullptr;
  SellableInfoData info_;
  mutable std::string str_tooltip_cache_;
};

class SellActivity final : public sim::Activity {
 public:
  SellActivity(Actor& self, bool b_show_ticks);

  bool Tick(Actor& self) override;

 private:
  sim::IHealth* ptr_health_ = nullptr;
  SellableInfoData sellable_info_;
  PlayerResources* ptr_player_resources_ = nullptr;
  bool b_show_ticks_ = false;
};

}
