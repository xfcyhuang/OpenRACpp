// UPSTREAM: OpenRA.Mods.Common/Traits/Player/BulkProductionQueue.cs @1f594e320
//          (实现部分)
//          The implementation half.
#include "mods/bulk_production_queue.hpp"

#include "game/actor_info.hpp"
#include "game/ruleset.hpp"
#include "mods/player_resources.hpp"
#include "meta/generic_record.hpp"
#include "mods/building_utils.hpp"
#include "mods/primary_building.hpp"
#include "mods/production_support.hpp"
#include "net/order.hpp"
#include "net/text_notifications.hpp"
#include "sim/actor.hpp"
#include "sim/actor_init.hpp"
#include "sim/trait_registry.hpp"
#include "sim/world.hpp"

namespace ora::mods {

BulkProductionQueueInfoData BulkProductionQueueInfoData::Parse(
    const meta::RecordObject& rec_info) {
  BulkProductionQueueInfoData data;
  if (const auto v = sim::RecordFieldInt(rec_info, "MaxCapacity"))
    data.int4_max_capacity = static_cast<int>(*v);
  if (const auto v = sim::RecordFieldInt(rec_info, "DeliveryDelay"))
    data.int4_delivery_delay = static_cast<int>(*v);
  if (const auto v = sim::RecordFieldString(rec_info, "StartDeliveryNotification"))
    data.str_start_delivery_notification = std::string{*v};
  if (const auto v = sim::RecordFieldInt(rec_info, "RefundUndeliveredActors"))
    data.b_refund_undelivered_actors = *v != 0;
  if (const auto v = sim::RecordFieldString(rec_info, "StartDeliveryTextNotification"))
    data.str_start_delivery_text_notification = std::string{*v};
  if (const auto* gv =
          sim::RecordFieldValue(rec_info, "DeliveryProgressNotifications"))
    if (const auto* items = std::get_if<std::vector<meta::GenericValue>>(&gv->val))
      for (const auto& item : *items)
        if (const auto* s = std::get_if<std::string>(&item.val))
          data.vec_delivery_progress_notifications.push_back(*s);
  data.queue_info = ProductionQueueInfoData::Parse(rec_info);
  return data;
}

BulkProductionQueue::BulkProductionQueue(sim::ActorInitializer& init,
                                         BulkProductionQueueInfoData info)
    : ProductionQueue{init, info.queue_info},
      bulk_info_{std::move(info)} {
  if (!bulk_info_.vec_delivery_progress_notifications.empty())
    int4_notification_interval_ =
        bulk_info_.int4_delivery_delay /
        static_cast<int>(bulk_info_.vec_delivery_progress_notifications.size());
}

void BulkProductionQueue::TickImpl(sim::Actor& self) {
  b_enabled_ = false;
  bool b_is_active = false;
  for (const auto& pair : self.world().ActorsWithTrait<Production>()) {
    if (pair.trait->IsTraitDisabled())
      continue;

    bool b_produces_type = false;
    for (const std::string& str_produce : pair.trait->Info().vec_produces)
      if (str_produce == Info().str_type)
        b_produces_type = true;
    if (pair.actor->Owner() != self.Owner() || !b_produces_type)
      continue;

    b_enabled_ = b_enabled_ | b_is_valid_faction_;
    b_is_active = b_is_active | !pair.trait->IsTraitPaused();
  }

  if (!b_enabled_) {
    DeliverFinished();
    ClearQueue();
  } else {
    if (HasDeliveryStarted() && int4_delivery_delay_ > 0) {
      if (p_developer_mode_ != nullptr && p_developer_mode_->FastBuild)
        int4_delivery_delay_ = 0;
      else {
        int4_delivery_delay_--;
        PlayDeliveryProgressNotifications();
      }
    } else if (HasDeliveryStarted() && int4_delivery_delay_ == 0) {
      PlayDeliveryProgressNotifications();
      DeliveryHasArrived();
      int4_delivery_delay_--;
    }
  }

  TickInner(self, !b_is_active);
}

std::vector<const game::ActorInfo*> BulkProductionQueue::AllItems() {
  if (!b_enabled_)
    return {};
  return ProductionQueue::AllItems();
}

std::vector<const game::ActorInfo*> BulkProductionQueue::BuildableItems() {
  if (b_enabled_ &&
      vec_actors_ready_for_delivery_.size() !=
          static_cast<std::size_t>(bulk_info_.int4_max_capacity) &&
      !b_delivery_process_started_)
    return ProductionQueue::BuildableItems();
  return {};
}

Production* BulkProductionQueue::MostLikelyProducer() {
  struct ProducerEntry {
    Production* trait;
    PrimaryBuilding* primary;
    bool b_paused;
    std::uint32_t uint4_actor_id;
  };
  std::vector<ProducerEntry> vec_producers;
  for (const auto& pair : Actor()->world().ActorsWithTrait<Production>()) {
    if (pair.actor->Owner() != Actor()->Owner() ||
        pair.trait->IsTraitDisabled())
      continue;

    bool b_produces_type = false;
    for (const std::string& str_produce : pair.trait->Info().vec_produces)
      if (str_produce == Info().str_type)
        b_produces_type = true;
    if (b_produces_type)
      vec_producers.push_back(ProducerEntry{
          pair.trait,
          pair.actor->TraitOrDefault<PrimaryBuilding>(),
          pair.trait->IsTraitPaused(), pair.actor->ActorID()});
  }

  std::stable_sort(vec_producers.begin(), vec_producers.end(),
                   [](const ProducerEntry& a, const ProducerEntry& b) {
                     if (a.b_paused != b.b_paused)
                       return !a.b_paused;
                     const bool a_primary = a.primary != nullptr && a.primary->IsPrimary();
                     const bool b_primary = b.primary != nullptr && b.primary->IsPrimary();
                     if (a_primary != b_primary)
                       return a_primary;
                     return a.uint4_actor_id > b.uint4_actor_id;
                   });

  return vec_producers.empty() ? nullptr : vec_producers.front().trait;
}

bool BulkProductionQueue::BuildUnit(const game::ActorInfo& unit) {
  std::string str_type = Info().str_type;
  if (const meta::RecordObject* rec_buildable =
          FindTraitInfoRecord(unit, "BuildableInfo")) {
    const BuildableInfoData bi = BuildableInfoData::Parse(*rec_buildable);
    if (!bi.str_build_at_production_type.empty() &&
        !(p_developer_mode_ != nullptr && p_developer_mode_->AllTech))
      str_type = bi.str_build_at_production_type;
  }

  struct ProducerEntry {
    Production* trait;
    PrimaryBuilding* primary;
    std::uint32_t uint4_actor_id;
  };
  std::vector<ProducerEntry> vec_producers;
  for (const auto& pair : Actor()->world().ActorsWithTrait<Production>()) {
    if (pair.actor->Owner() != Actor()->Owner() ||
        pair.trait->IsTraitDisabled())
      continue;

    bool b_produces_type = false;
    for (const std::string& str_produce : pair.trait->Info().vec_produces)
      if (str_produce == str_type)
        b_produces_type = true;
    if (b_produces_type)
      vec_producers.push_back(ProducerEntry{
          pair.trait, pair.actor->TraitOrDefault<PrimaryBuilding>(),
          pair.actor->ActorID()});
  }

  std::stable_sort(vec_producers.begin(), vec_producers.end(),
                   [](const ProducerEntry& a, const ProducerEntry& b) {
                     const bool a_primary = a.primary != nullptr && a.primary->IsPrimary();
                     const bool b_primary = b.primary != nullptr && b.primary->IsPrimary();
                     if (a_primary != b_primary)
                       return a_primary;
                     return a.uint4_actor_id > b.uint4_actor_id;
                   });

  bool b_any_producers = false;
  for (const ProducerEntry& p : vec_producers) {
    b_any_producers = true;
    if (p.trait->IsTraitPaused())
      continue;
    if (vec_actors_ready_for_delivery_.size() <=
        static_cast<std::size_t>(bulk_info_.int4_max_capacity)) {
      ProductionItemLike* item = nullptr;
      for (const auto& queued : vec_queue_)
        if (queued->Done() && queued->Item == unit.Name()) {
          item = queued.get();
          break;
        }
      if (item == nullptr)
        continue;
      vec_actors_ready_for_delivery_.push_back(
          BulkReadyActor{&unit, item->ResourcesPaid,
                         item->TotalCost - item->ResourcesPaid});
      EndProduction(*item);
      return true;
    }
  }

  if (!b_any_producers)
    CancelProduction(unit.Name(), 1);

  return false;
}

void BulkProductionQueue::ResolveOrder(sim::Actor& self,
                                       const ora::net::Order& order) {
  if (!b_enabled_)
    return;

  if (order.str_order_string == "StartProduction") {
    const game::ActorInfo* unit =
        self.world().Map().Rules().FindActor(*order.str_target_string);
    if (unit == nullptr)
      return;

    BuildableInfoData bi;
    if (const meta::RecordObject* rec_buildable =
            FindTraitInfoRecord(*unit, "BuildableInfo"))
      bi = BuildableInfoData::Parse(*rec_buildable);

    bool b_in_queue = false;
    for (const std::string& str_q : bi.vec_queue)
      if (str_q == Info().str_type)
        b_in_queue = true;
    if (!b_in_queue)
      return;

    bool b_buildable = false;
    for (const game::ActorInfo* buildable : BuildableItems())
      if (buildable->Name() == *order.str_target_string)
        b_buildable = true;
    if (!b_buildable)
      return;

    int from_limit = std::numeric_limits<int>::max();
    if (!(p_developer_mode_ != nullptr && p_developer_mode_->AllTech)) {
      if (Info().int4_queue_limit > 0)
        from_limit = Info().int4_queue_limit -
                     static_cast<int>(vec_queue_.size());

      if (Info().int4_item_limit > 0) {
        int in_queue = 0;
        for (const auto& queued : vec_queue_)
          if (queued->Item == *order.str_target_string)
            in_queue++;
        from_limit = std::min(from_limit,
                              Info().int4_item_limit - in_queue);
      }

      if (bi.int4_build_limit > 0) {
        int in_queue = 0;
        for (const auto& queued : vec_queue_)
          if (queued->Item == *order.str_target_string)
            in_queue++;
        int owned = 0;
        for (const auto& pair :
             self.world().ActorsWithTrait<Buildable>())
          if (pair.actor->Info()->Name() == *order.str_target_string &&
              pair.actor->Owner() == self.Owner())
            owned++;
        from_limit =
            std::min(from_limit, bi.int4_build_limit - (in_queue + owned));
      }

      if (from_limit <= 0)
        return;
    }

    const int cost = GetProductionCost(*unit);
    const int time = GetBuildTime(*unit);
    const std::uint32_t amount_to_build =
        std::min(static_cast<std::uint32_t>(from_limit), order.uint4_extra_data);
    for (std::uint32_t n = 0; n < amount_to_build; n++) {
      if (Info().b_pay_up_front &&
          (p_player_resources_ == nullptr ||
           cost > p_player_resources_->GetCashAndResources()))
        return;

      const std::string item_name = *order.str_target_string;
      ProductionQueue* queue_this = this;
      BeginProduction(
          std::make_unique<ProductionItemLike>(
              *queue_this, item_name, cost, p_player_power_,
              [queue_this, unit](ProductionItemLike* item) {
                queue_this->Actor()->world().AddFrameEndTask(
                    [queue_this, unit](sim::World&) {
                      bool b_done = false;
                      for (const auto& queued :
                           queue_this->AllQueued())
                        if (queued->Done() &&
                            queued->Item == unit->Name())
                          b_done = true;
                      if (!b_done)
                        return;
                      queue_this->BuildUnit(*unit);
                    });
              }),
          !order.b_queued);
    }
  } else if (order.str_order_string == "PauseProduction") {
    PauseProduction(*order.str_target_string, order.uint4_extra_data != 0);
  } else if (order.str_order_string == "CancelProduction") {
    CancelProduction(*order.str_target_string, order.uint4_extra_data);
  } else if (order.str_order_string == "ReturnOrder") {
    ReturnOrder(*order.str_target_string, order.uint4_extra_data);
  } else if (order.str_order_string == "PurchaseOrder") {
    if (!b_delivery_process_started_ && *order.str_target_string == Info().str_type)
      StartDeliveryProcess();
  }
}

void BulkProductionQueue::DeliverFinished() {
  if (bulk_info_.b_refund_undelivered_actors || !b_delivery_process_started_)
    ReturnOrder("", 1);
  vec_actors_ready_for_delivery_.clear();
  b_delivery_process_started_ = false;
}

void BulkProductionQueue::StartDeliveryProcess() {
  ClearQueue();
  b_delivery_process_started_ = true;
  int4_delivery_delay_ = bulk_info_.int4_delivery_delay;

  net::TextNotificationsManager::Instance().AddTransientLine(
      Actor()->Owner(), bulk_info_.str_start_delivery_text_notification);
}

void BulkProductionQueue::DeliveryHasArrived() {
  // ProductionBulkAirdrop 未移植:producers 恒空集(DeliveryHasArrived 的
  // L283-291 无 trait 世界等价域;d2k 链随 Phase 8)
  // ProductionBulkAirdrop unported: the producers stay the empty set
  // (DeliveryHasArrived's L283-291 equivalent domain without the trait;
  // the d2k chain rides Phase 8).
}

void BulkProductionQueue::ReturnOrder(std::string_view str_item_name,
                                      std::uint32_t number_to_cancel) {
  if (str_item_name.empty()) {
    for (const BulkReadyActor& actor_data : vec_actors_ready_for_delivery_) {
      if (p_player_resources_ != nullptr) {
        p_player_resources_->RefundResources(actor_data.int4_resources);
        p_player_resources_->RefundCash(actor_data.int4_cash);
      }
    }

    vec_actors_ready_for_delivery_.clear();
    return;
  }

  for (std::uint32_t i = 0; i < number_to_cancel; i++) {
    std::optional<std::size_t> opt_index;
    for (std::size_t idx = 0; idx < vec_actors_ready_for_delivery_.size();
         idx++)
      if (vec_actors_ready_for_delivery_[idx].actor_info != nullptr &&
          vec_actors_ready_for_delivery_[idx].actor_info->Name() ==
              str_item_name)
        opt_index = idx;
    if (!opt_index.has_value() ||
        vec_actors_ready_for_delivery_[*opt_index].actor_info == nullptr)
      break;

    const BulkReadyActor actor_data = vec_actors_ready_for_delivery_[*opt_index];
    if (p_player_resources_ != nullptr) {
      p_player_resources_->RefundResources(actor_data.int4_resources);
      p_player_resources_->RefundCash(actor_data.int4_cash);
    }
    vec_actors_ready_for_delivery_.erase(
        vec_actors_ready_for_delivery_.begin() +
        static_cast<std::ptrdiff_t>(*opt_index));
  }
}

void BulkProductionQueue::PlayDeliveryProgressNotifications() {
  if (int4_notification_interval_ == 0) {
    int4_notification_interval_ =
        bulk_info_.int4_delivery_delay /
        static_cast<int>(bulk_info_.vec_delivery_progress_notifications.size());
    int4_notification_index_++;
    if (bulk_info_.vec_delivery_progress_notifications.size() ==
        static_cast<std::size_t>(int4_notification_index_)) {
      int4_notification_index_ = 0;
      return;
    }
  }

  int4_notification_interval_--;
}

}  // namespace ora::mods
