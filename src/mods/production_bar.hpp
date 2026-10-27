#pragma once
import std;

#include "core/color.hpp"
#include "meta/generic_record.hpp"
#include "mods/production.hpp"
#include "sim/actor.hpp"
#include "sim/conditional_trait.hpp"
#include "sim/trait_interfaces.hpp"

namespace ora::mods {

using sim::Actor;
using sim::ActorInitializer;
using sim::TraitBase;

struct ProductionBarInfoData {
  sim::ConditionalTraitData conditional;
  std::string str_production_type;
  core::Color color_color = core::Color::FromArgb(135, 206, 235);

  static ProductionBarInfoData Parse(const meta::RecordObject& rec_info);
};

class ProductionBar final : public TraitBase,
                            public sim::ConditionalTraitCore<ProductionBar>,
                            public sim::IObservesVariables,
                            public sim::INotifyCreated,
                            public sim::ISelectionBar,
                            public sim::ITick,
                            public sim::INotifyOwnerChanged {
 public:
  ProductionBar(const ActorInitializer& init, ProductionBarInfoData info);

  ORA_TRAIT_INTERFACES(ProductionBar,
                       OpenRA_Mods_Common_Traits_Render_ProductionBar,
                       sim::IObservesVariables, sim::INotifyCreated,
                       sim::ISelectionBar, sim::ITick,
                       sim::INotifyOwnerChanged)

  bool IsTraitEnabled() const override {
    return !sim::ConditionalTraitCore<ProductionBar>::IsTraitDisabled();
  }
  bool IsTraitDisabled() const override {
    return sim::ConditionalTraitCore<ProductionBar>::IsTraitDisabled();
  }
  std::vector<sim::VariableObserver> GetVariableObservers() override {
    return CollectObservers();
  }

  void Created(Actor& self) override;
  void Tick(Actor& self) override;
  void OnOwnerChanged(Actor& self, sim::Player& old_owner,
                      sim::Player& new_owner) override;

  float GetValue() override;
  core::Color GetColor() override { return info_.color_color; }
  bool DisplayWhenEmpty() const override { return false; }

  const ProductionBarInfoData& Info() const { return info_; }
  ProductionQueue* BoundQueueForTest() const { return ptr_queue_; }

 private:

  void FindQueue(Actor& self);

  Actor* ptr_self_ = nullptr;
  ProductionBarInfoData info_;
  ProductionQueue* ptr_queue_ = nullptr;
  float float_value_ = 0.0f;
};

}
