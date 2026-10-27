#pragma once
import std;

#include "core/cell_pos.hpp"
#include "core/wangle.hpp"
#include "mods/production_support.hpp"
#include "sim/actor.hpp"
#include "sim/conditional_trait.hpp"
#include "sim/trait_interfaces.hpp"
#include "sim/type_dictionary.hpp"

namespace ora::meta {
class RecordObject;
}

namespace ora::sim {
class ActorInitializer;
}

namespace ora::game {
class ActorInfo;
}

namespace ora::mods {

class Production;
class ProductionQueue;
class PlayerResources;
class ProductionItemLike;
class PowerManager;

struct ProductionInfoData {
  std::vector<std::string> vec_produces;
  bool b_update_faction_on_owner_change = false;
  sim::ConditionalTraitData conditional;

  static ProductionInfoData Parse(const meta::RecordObject& rec_info);
};

class Production : public sim::TraitBase,
                   public sim::ConditionalTraitCore<Production>,
                   public sim::INotifyCreated,
                   public sim::INotifyOwnerChanged,
                   public sim::IObservesVariables {
 public:
  ORA_TRAIT_INTERFACES(
      Production, OpenRA_Mods_Common_Traits_Production, sim::INotifyCreated,
      sim::INotifyOwnerChanged, sim::IObservesVariables)

  Production(sim::ActorInitializer& init, ProductionInfoData info);

  bool IsTraitDisabled() const {
    return sim::ConditionalTraitCore<Production>::IsTraitDisabled();
  }
  bool IsTraitPaused() const {
    return sim::ConditionalTraitCore<Production>::IsTraitPaused();
  }

  std::vector<sim::VariableObserver> GetVariableObservers() {
    return sim::ConditionalTraitCore<Production>::CollectObservers();
  }

  void Created(sim::Actor& self) override;
  void OnOwnerChanged(sim::Actor& self, sim::Player& old_owner,
                      sim::Player& new_owner) override;

  const ProductionInfoData& Info() const { return info_; }
  const std::string& Faction() const { return str_faction_; }
  sim::Actor& HostActor() const { return *p_self_; }

  virtual void DoProduction(sim::Actor& self,
                            const game::ActorInfo& producee,
                            const ExitInfoData* exit_info,
                            const std::string& production_type,
                            sim::TypeDictionary& inits);

  Exit* SelectExit(sim::Actor& self, const game::ActorInfo& producee,
                   const std::string& production_type);

  virtual bool Produce(sim::Actor& self,
                       const game::ActorInfo& producee,
                       const std::string& production_type,
                       sim::TypeDictionary& inits, int refundable_value);

 private:

  Exit* SelectExitImpl(sim::Actor& self,
                       const game::ActorInfo& producee,
                       const std::string& production_type,
                       const std::function<bool(Exit*)>& p);

  static bool CanUseExit(sim::Actor& self,
                         const game::ActorInfo& producee,
                         const ExitInfoData& s);

  ProductionInfoData info_;
  std::string str_faction_;
  RallyPoint* p_rally_point_ = nullptr;
  sim::Actor* p_self_ = nullptr;
};

struct ProductionQueueInfoData {
  std::string str_type;
  int int4_display_order = 0;
  std::string str_group;
  std::vector<std::string> vec_factions;
  bool b_sticky = true;
  bool b_pay_up_front = false;
  bool b_disallow_paused = false;
  int int4_build_duration_modifier = 100;
  int int4_item_limit = 999;
  int int4_queue_limit = 0;
  int int4_low_power_modifier = 100;
  int int4_infinite_build_limit = -1;
  std::string str_ready_audio;
  std::string str_blocked_audio;
  std::string str_limited_audio;
  std::string str_cannot_place_audio;
  std::string str_queued_audio;
  std::string str_on_hold_audio;
  std::string str_cancelled_audio;
  std::string str_ready_text_notification;
  std::string str_blocked_text_notification;
  std::string str_limited_text_notification;
  std::string str_queued_text_notification;
  std::string str_on_hold_text_notification;
  std::string str_cancelled_text_notification;

  static ProductionQueueInfoData Parse(const meta::RecordObject& rec_info);
};

class ProductionItemLike {
 public:
  std::string Item;
  int TotalCost = 0;
  int RemainingCost = 0;
  int ResourcesPaid = 0;
  bool Infinite = false;

  ProductionItemLike(ProductionQueue& queue, std::string item, int cost,
                     PowerManager* pm,
                     std::function<void(ProductionItemLike*)> on_complete);

  int TotalTime() const { return int4_total_time_; }
  int RemainingTime() const { return int4_remaining_time_; }
  int RemainingTimeActual() const;
  bool Paused() const { return b_paused_; }
  bool Done() const { return b_done_; }
  bool Started() const { return b_started_; }
  int Slowdown() const { return int4_slowdown_; }
  int BuildPaletteOrder() const { return int4_build_palette_order_; }

  void Tick(PlayerResources& pr);

  void Pause(bool paused) { b_paused_ = paused; }

  ProductionQueue* Queue = nullptr;
  std::function<void(ProductionItemLike*)> OnComplete;

  bool b_notified = false;

 private:
  int int4_total_time_ = 1;
  int int4_remaining_time_ = 1;
  bool b_paused_ = false;
  bool b_done_ = false;
  bool b_started_ = false;
  int int4_slowdown_ = 0;
  int int4_build_palette_order_ = 0;
  const game::ActorInfo* p_ai_ = nullptr;
  PowerManager* p_pm_ = nullptr;
};

class ProductionQueue : public sim::TraitBase,
                        public sim::IResolveOrder,
                        public sim::ITick,
                        public sim::ITechTreeElement,
                        public sim::INotifyOwnerChanged,
                        public sim::INotifyKilled,
                        public sim::INotifySold,
                        public sim::ISync,
                        public sim::INotifyTransform,
                        public sim::INotifyCreated {
 public:
  ORA_TRAIT_INTERFACES(
      ProductionQueue, OpenRA_Mods_Common_Traits_ProductionQueue,
      sim::IResolveOrder, sim::ITick, sim::ITechTreeElement,
      sim::INotifyOwnerChanged, sim::INotifyKilled, sim::INotifySold,
      sim::ISync, sim::INotifyTransform, sim::INotifyCreated)

  ProductionQueue(sim::ActorInitializer& init,
                  ProductionQueueInfoData info);

  void Created(sim::Actor& self) override;
  void Tick(sim::Actor& self) override;

  void PrerequisitesAvailable(const std::string& key) override;
  void PrerequisitesUnavailable(const std::string& key) override;
  void PrerequisitesItemHidden(const std::string& key) override;
  void PrerequisitesItemVisible(const std::string& key) override;

  void OnOwnerChanged(sim::Actor& self, sim::Player& old_owner,
                      sim::Player& new_owner) override;
  void Killed(sim::Actor& killed,
              const sim::AttackInfo& e) override;
  void Selling(sim::Actor& self) override;
  void Sold(sim::Actor& self) override {}
  void BeforeTransform(sim::Actor& self) override;
  void OnTransform(sim::Actor& self) override {}
  void AfterTransform(sim::Actor& to_actor) override {}

  void ResolveOrder(sim::Actor& self,
                    const ora::net::Order& order) override;

  virtual void TickImpl(sim::Actor& self);

  bool IsProducing(const ProductionItemLike& item) const;
  bool IsInQueue(const game::ActorInfo& actor) const;
  ProductionItemLike* CurrentItem();
  const std::vector<std::unique_ptr<ProductionItemLike>>& AllQueued()
      const {
    return vec_queue_;
  }
  virtual std::vector<const game::ActorInfo*> AllItems();
  virtual std::vector<const game::ActorInfo*> BuildableItems();
  bool AnyItemsToBuild();
  bool CanBuild(const game::ActorInfo& actor);

  bool CanQueue(const game::ActorInfo& actor,
                std::string& notification_audio,
                std::string& notification_text);

  virtual int GetBuildTime(const game::ActorInfo& unit);

  virtual int GetProductionCost(const game::ActorInfo& unit);

  void PauseProduction(const std::string& item_name, bool paused);
  void CancelProduction(const std::string& item_name,
                        std::uint32_t number_to_cancel);

  bool CancelProductionInner(const std::string& item_name);

  void EndProduction(ProductionItemLike& item);

  virtual void BeginProduction(std::unique_ptr<ProductionItemLike> item,
                               bool has_priority);

  int RemainingTimeActual(const ProductionItemLike& item) const;
  virtual Production* MostLikelyProducer();

  virtual bool BuildUnit(const game::ActorInfo& unit);

  const ProductionQueueInfoData& Info() const { return info_; }
  sim::Actor* Actor() const { return p_actor_; }
  bool Enabled() const { return b_enabled_; }
  bool IsValidFaction() const { return b_is_valid_faction_; }
  const std::string& Faction() const { return str_faction_; }

 protected:

  void ClearQueue();
  void CancelUnbuildableItems();

  void CacheProducibles();
  std::vector<const game::ActorInfo*> AllBuildables(
      const std::string& category) const;

  virtual void TickInner(sim::Actor& self, bool all_production_paused);

  struct ProducibleEntry {
    const game::ActorInfo* actor_info = nullptr;
    bool b_visible = true;
    bool b_buildable = false;
  };

  ProducibleEntry* FindProducible(const game::ActorInfo* actor);
  ProducibleEntry* FindProducibleByName(const std::string& name);

  ProductionQueueInfoData info_;
  sim::Actor* p_actor_ = nullptr;
  std::vector<ProducibleEntry> vec_producible_;
  std::vector<std::unique_ptr<ProductionItemLike>> vec_queue_;
  PowerManager* p_player_power_ = nullptr;
  std::vector<Production*> vec_production_traits_;
  PlayerResources* p_player_resources_ = nullptr;
  DeveloperMode* p_developer_mode_ = nullptr;
  TechTree* p_tech_tree_ = nullptr;
  bool b_enabled_ = false;
  std::string str_faction_;
  bool b_is_valid_faction_ = false;
};

class ClassicProductionQueue final : public ProductionQueue {
 public:

  ORA_TRAIT_INTERFACES(
      ClassicProductionQueue,
      OpenRA_Mods_Common_Traits_ClassicProductionQueue, ProductionQueue,
      sim::IResolveOrder, sim::ITick, sim::ITechTreeElement,
      sim::INotifyOwnerChanged, sim::INotifyKilled, sim::INotifySold,
      sim::ISync, sim::INotifyTransform, sim::INotifyCreated)

  ClassicProductionQueue(sim::ActorInitializer& init,
                         ProductionQueueInfoData info)
      : ProductionQueue{init, std::move(info)} {}

  void TickImpl(sim::Actor& self) override;
  std::vector<const game::ActorInfo*> AllItems() override;
  std::vector<const game::ActorInfo*> BuildableItems() override;
  Production* MostLikelyProducer() override;
  bool BuildUnit(const game::ActorInfo& unit) override;

};

Exit* NearestExitOrDefault(sim::Actor& actor, const WPos& pos,
                           const std::string& production_type,
                           const std::function<bool(Exit*)>& p = nullptr);

std::vector<Exit*> ActorExits(sim::Actor& actor,
                              const std::string& production_type);
Exit* RandomExitOrDefault(sim::Actor& actor, sim::World& world,
                          const std::string& production_type,
                          const std::function<bool(Exit*)>& p = nullptr);

template <class T>
void ShuffleSpan(std::vector<T>& vec, MersenneTwister& rng) {
  for (std::size_t i = vec.size(); i > 1; i--) {

    const std::size_t j = static_cast<std::size_t>(
        rng.Next(static_cast<std::int32_t>(i)));
    std::swap(vec[j], vec[i - 1]);
  }
}

}
