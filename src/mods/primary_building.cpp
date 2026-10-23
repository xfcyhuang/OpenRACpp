// UPSTREAM: OpenRA.Mods.Common/Traits/Buildings/PrimaryBuilding.cs @e180e3225
//          (实现部分)
//          The implementation half.
#include "mods/primary_building.hpp"

#include "game/actor_info.hpp"
#include "meta/generic_record.hpp"
#include "mods/deploy_order_targeter.hpp"
#include "mods/production.hpp"
#include "net/order.hpp"
#include "net/text_notifications.hpp"
#include "sim/actor_init.hpp"
#include "sim/trait_registry.hpp"
#include "sim/world.hpp"

namespace ora::mods {

namespace {
constexpr std::string_view kOrderId = "PrimaryProducer";
}

PrimaryBuildingInfoData PrimaryBuildingInfoData::Parse(
    const meta::RecordObject& rec_info) {
  PrimaryBuildingInfoData data;
  if (const auto v = sim::RecordFieldString(rec_info, "PrimaryCondition"))
    data.str_primary_condition = std::string{*v};
  if (const auto v = sim::RecordFieldString(rec_info, "SelectionNotification"))
    data.str_selection_notification = std::string{*v};
  if (const auto v = sim::RecordFieldString(rec_info, "SelectionTextNotification"))
    data.str_selection_text_notification = std::string{*v};
  if (const auto* gv = sim::RecordFieldValue(rec_info, "ProductionQueues"))
    if (const auto* items = std::get_if<std::vector<meta::GenericValue>>(&gv->val))
      for (const auto& item : *items)
        if (const auto* s = std::get_if<std::string>(&item.val))
          data.vec_production_queues.push_back(*s);
  if (const auto v = sim::RecordFieldString(rec_info, "Cursor"))
    if (!v->empty())
      data.str_cursor = std::string{*v};
  data.conditional = sim::ConditionalTraitData::Parse(rec_info);
  return data;
}

PrimaryBuilding::PrimaryBuilding(sim::ActorInitializer& init,
                                 PrimaryBuildingInfoData info)
    : ConditionalTraitCore{info.conditional}, info_{std::move(info)} {
  p_self_ = &init.Self();
}

std::vector<sim::IOrderTargeter*> PrimaryBuilding::Orders() {
  std::vector<sim::IOrderTargeter*> orders;
  if (IsTraitDisabled())
    return orders;

  const std::string str_cursor = info_.str_cursor;
  orders.push_back(p_self_->world().Arena().Create<DeployOrderTargeter>(
      std::string{kOrderId}, 1,
      [str_cursor] { return str_cursor; }));
  return orders;
}

std::unique_ptr<net::Order> PrimaryBuilding::IssueOrder(
    sim::Actor& self, sim::IOrderTargeter* order,
    const sim::Target&, bool) {
  if (order->OrderID() == kOrderId)
    return std::make_unique<net::Order>(std::string{kOrderId}, &self, false);

  return nullptr;
}

void PrimaryBuilding::ResolveOrder(sim::Actor& self,
                                   const ora::net::Order& order) {
  if (order.str_order_string == kOrderId)
    SetPrimaryProducer(self, !b_is_primary_);

  if (RallyPoint::IsForceSet(order) && !b_is_primary_)
    SetPrimaryProducer(self, true);
}

void PrimaryBuilding::SetPrimaryProducer(sim::Actor& self,
                                         bool b_is_primary) {
  b_is_primary_ = b_is_primary;

  if (b_is_primary) {
    std::vector<std::string> vec_queues;
    if (info_.vec_production_queues.empty())
      for (Production* production :
           self.TraitsImplementing<Production>())
        if (!production->IsTraitDisabled())
          for (const std::string& str_produce : production->Info().vec_produces)
            vec_queues.push_back(str_produce);
    else
      vec_queues = info_.vec_production_queues;

    for (const std::string& q : vec_queues) {
      for (const auto& pair :
           self.world().ActorsWithTrait<PrimaryBuilding>()) {
        if (pair.actor == &self || pair.actor->Owner() != self.Owner() ||
            !pair.trait->IsPrimary())
          continue;

        bool b_matches_queue = false;
        for (Production* production :
             pair.actor->TraitsImplementing<Production>())
          if (!production->IsTraitDisabled())
            for (const std::string& str_produce :
                 production->Info().vec_produces)
              if (str_produce == q)
                b_matches_queue = true;
        if (b_matches_queue)
          pair.trait->SetPrimaryProducer(*pair.actor, false);
      }
    }

    if (int4_primary_token_ == sim::Actor::InvalidConditionToken)
      int4_primary_token_ = self.GrantCondition(info_.str_primary_condition);

    net::TextNotificationsManager::Instance().AddTransientLine(
        self.Owner(), info_.str_selection_text_notification);
  } else if (int4_primary_token_ != sim::Actor::InvalidConditionToken) {
    int4_primary_token_ = self.RevokeCondition(int4_primary_token_);
  }
}

void PrimaryBuilding::TraitDisabledHook(sim::Actor& self) {
  if (b_is_primary_)
    SetPrimaryProducer(self, !b_is_primary_);
}

}  // namespace ora::mods
