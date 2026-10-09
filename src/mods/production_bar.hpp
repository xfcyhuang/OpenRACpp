// UPSTREAM: OpenRA.Mods.Common/Traits/Render/ProductionBar.cs @b6fc03f
//          L18-105 生产进度条(ISelectionBar 的首个真消费面)。
//          The production progress bar (ISelectionBar's first real
//          consumer).
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

/// ProductionBarInfo(L22-46)的解析面
/// The parsed face of ProductionBarInfo (L22-46).
struct ProductionBarInfoData {
  sim::ConditionalTraitData conditional;
  std::string str_production_type;             // L27([FieldLoader.Require])
  core::Color color_color = core::Color::FromArgb(135, 206, 235);  // L31 SkyBlue

  static ProductionBarInfoData Parse(const meta::RecordObject& rec_info);
};

/// ProductionBar(L48-105):逐 tick 计值(Started 项的最小剩余时间占比)
/// ProductionBar (L48-105): the per-tick value (the started item with the
/// least remaining time, as a fraction).
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

  /// ISelectionBar(L86-93)
  float GetValue() override;
  core::Color GetColor() override { return info_.color_color; }
  bool DisplayWhenEmpty() const override { return false; }

  const ProductionBarInfoData& Info() const { return info_; }
  ProductionQueue* BoundQueueForTest() const { return ptr_queue_; }

 private:
  /// FindQueue(L59-68):本 actor 队列 → 玩家 actor 队列(含禁用队列)
  /// FindQueue (L59-68): this actor's queue → the player actor's (disabled
  /// queues included).
  void FindQueue(Actor& self);

  Actor* ptr_self_ = nullptr;
  ProductionBarInfoData info_;
  ProductionQueue* ptr_queue_ = nullptr;
  float float_value_ = 0.0f;
};

}  // namespace ora::mods
