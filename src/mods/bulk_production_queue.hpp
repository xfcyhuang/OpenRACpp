#pragma once
import std;

#include "game/actor_info.hpp"
#include "mods/production.hpp"

namespace ora::mods {

struct BulkReadyActor {
  const game::ActorInfo* actor_info = nullptr;
  int int4_resources = 0;
  int int4_cash = 0;
};

struct BulkProductionQueueInfoData {
  int int4_max_capacity = 6;
  int int4_delivery_delay = 1500;
  std::string str_start_delivery_notification;
  bool b_refund_undelivered_actors = true;
  std::string str_start_delivery_text_notification;
  std::vector<std::string> vec_delivery_progress_notifications;

  ProductionQueueInfoData queue_info;

  static BulkProductionQueueInfoData Parse(const meta::RecordObject& rec_info);
};

class BulkProductionQueue final : public ProductionQueue {
 public:
  ORA_TRAIT_INTERFACES(
      BulkProductionQueue,
      OpenRA_Mods_Common_Traits_BulkProductionQueue, ProductionQueue,
      sim::IResolveOrder, sim::ITick, sim::ITechTreeElement,
      sim::INotifyOwnerChanged, sim::INotifyKilled, sim::INotifySold,
      sim::ISync, sim::INotifyTransform, sim::INotifyCreated)

  BulkProductionQueue(sim::ActorInitializer& init,
                      BulkProductionQueueInfoData info);

  void TickImpl(sim::Actor& self) override;
  std::vector<const game::ActorInfo*> AllItems() override;
  std::vector<const game::ActorInfo*> BuildableItems() override;
  Production* MostLikelyProducer() override;
  bool BuildUnit(const game::ActorInfo& unit) override;
  void ResolveOrder(sim::Actor& self,
                    const ora::net::Order& order) override;

  void DeliverFinished();

  bool HasDeliveryStarted() const { return b_delivery_process_started_; }

  const std::vector<BulkReadyActor>& GetActorsReadyForDelivery() const {
    return vec_actors_ready_for_delivery_;
  }

  void ReturnOrder(std::string_view str_item_name, std::uint32_t number_to_cancel);

 private:

  void StartDeliveryProcess();

  void DeliveryHasArrived();

  void PlayDeliveryProgressNotifications();

  BulkProductionQueueInfoData bulk_info_;
  std::vector<BulkReadyActor> vec_actors_ready_for_delivery_;
  int int4_delivery_delay_ = 0;
  int int4_notification_interval_ = 0;
  int int4_notification_index_ = 0;
  bool b_delivery_process_started_ = false;
};

}
