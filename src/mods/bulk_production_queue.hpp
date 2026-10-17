// UPSTREAM: OpenRA.Mods.Common/Traits/Player/BulkProductionQueue.cs @e180e3225
//          L20-337 全文(同步面;ProductionBulkAirdrop 未移植 ——
//          DeliveryHasArrived 的 producer 恒空集 = 无该 trait 世界的等价
//          域,d2k 全链随 Phase 8;登记 COVERAGE)
//          The whole synchronous face of BulkProductionQueue.cs L20-337
//          (ProductionBulkAirdrop unported — DeliveryHasArrived's
//          producer stays the empty set = the equivalent domain of a
//          world without that trait; d2k's full chain rides Phase 8;
//          registered in COVERAGE).
#pragma once
import std;

#include "game/actor_info.hpp"
#include "mods/production.hpp"

namespace ora::mods {

/// ActorsReadyForDelivery 的载荷(L58)
/// The ActorsReadyForDelivery payload (L58).
struct BulkReadyActor {
  const game::ActorInfo* actor_info = nullptr;
  int int4_resources = 0;
  int int4_cash = 0;
};

struct BulkProductionQueueInfoData {
  int int4_max_capacity = 6;          // L29
  int int4_delivery_delay = 1500;     // L32
  std::string str_start_delivery_notification;  // L35
  bool b_refund_undelivered_actors = true;      // L38
  std::string str_start_delivery_text_notification;  // L41
  std::vector<std::string> vec_delivery_progress_notifications;  // L46

  ProductionQueueInfoData queue_info;

  static BulkProductionQueueInfoData Parse(const meta::RecordObject& rec_info);
};

/// BulkProductionQueue(L51-336)
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

  void TickImpl(sim::Actor& self) override;  // L74-117
  std::vector<const game::ActorInfo*> AllItems() override;  // L119-122
  std::vector<const game::ActorInfo*> BuildableItems() override;  // L124-127
  Production* MostLikelyProducer() override;  // L129-140
  bool BuildUnit(const game::ActorInfo& unit) override;  // L142-176
  void ResolveOrder(sim::Actor& self,
                    const ora::net::Order& order) override;  // L178-250

  /// L252-258 | L252-258.
  void DeliverFinished();

  /// L260-263 | L260-263.
  bool HasDeliveryStarted() const { return b_delivery_process_started_; }

  /// L265-268 | L265-268.
  const std::vector<BulkReadyActor>& GetActorsReadyForDelivery() const {
    return vec_actors_ready_for_delivery_;
  }

  /// L294-317 | L294-317.
  void ReturnOrder(std::string_view str_item_name, std::uint32_t number_to_cancel);

 private:
  /// L270-279 | L270-279.
  void StartDeliveryProcess();

  /// L281-292 | L281-292.
  void DeliveryHasArrived();

  /// L319-335 | L319-335.
  void PlayDeliveryProgressNotifications();

  BulkProductionQueueInfoData bulk_info_;
  std::vector<BulkReadyActor> vec_actors_ready_for_delivery_;  // L58
  int int4_delivery_delay_ = 0;  // L59(DeliveryDelay 属性)
  int int4_notification_interval_ = 0;  // L60
  int int4_notification_index_ = 0;     // L61
  bool b_delivery_process_started_ = false;  // L63
};

}  // namespace ora::mods
