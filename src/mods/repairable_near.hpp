#pragma once
import std;

#include "core/wdist.hpp"
#include "meta/generic_record.hpp"
#include "meta/variable_expression.hpp"
#include "net/order.hpp"
#include "sim/actor.hpp"
#include "sim/actor_init.hpp"
#include "sim/trait_interfaces.hpp"

namespace ora::mods {

using sim::Actor;
using sim::TraitBase;

struct RepairableNearInfoData {
  std::vector<std::string> vec_repair_actors;
  WDist dist_close_enough{4 * 1024};
  std::string str_voice{"Action"};
  std::optional<expr::BooleanExpression> expr_require_force_move;
  std::string str_enter_cursor{"enter"};
  std::string str_enter_blocked_cursor{"enter-blocked"};

  static RepairableNearInfoData Parse(const meta::RecordObject& rec_info);
};

class RepairableNear final : public TraitBase,
                             public sim::IIssueOrder,
                             public sim::IResolveOrder,
                             public sim::IOrderVoice,
                             public sim::IObservesVariables {
 public:
  RepairableNear(Actor& self, RepairableNearInfoData info);

  ORA_TRAIT_INTERFACES(RepairableNear,
                       OpenRA_Mods_Common_Traits_RepairableNear,
                       sim::IIssueOrder, sim::IResolveOrder, sim::IOrderVoice,
                       sim::IObservesVariables)

  const RepairableNearInfoData& Info() const { return info_; }

  std::vector<sim::IOrderTargeter*> Orders() override;
  std::unique_ptr<net::Order> IssueOrder(
      Actor& self, sim::IOrderTargeter* order,
      const sim::Target& target, bool queued) override;
  std::string VoicePhraseForOrder(Actor& self,
                                  const net::Order& order) override;
  void ResolveOrder(Actor& self, const net::Order& order) override;

  Actor* FindRepairBuilding(Actor& self);

  bool CanRepairAt(Actor& target);
  bool CanRepairAt(Actor& target, sim::TargetModifiers modifiers);
  bool ShouldRepair();

  std::vector<sim::VariableObserver> GetVariableObservers() override;

  class RepairNearEnterTargeter final : public sim::IOrderTargeter {
   public:
    RepairNearEnterTargeter(const RepairableNearInfoData& info)
        : info_{info} {}

    std::string OrderID() const override { return "RepairNear"; }
    int OrderPriority() const override { return 5; }
    bool TargetOverridesSelection(
        Actor& self, const sim::Target& target,
        std::span<Actor* const> actors_at, CPos xy,
        sim::TargetModifiers modifiers) override;
    bool CanTarget(Actor& self, const sim::Target& target,
                   sim::TargetModifiers& modifiers,
                   std::string& cursor) override;
    bool IsQueued() const override { return b_is_queued_; }

   private:
    const RepairableNearInfoData& info_;
    bool b_is_queued_ = false;
  };

 private:
  void RequireForceMoveConditionChanged(Actor& self,
                                        sim::ConditionCacheView conditions);

  RepairableNearInfoData info_;
  Actor* ptr_self_ = nullptr;
  bool b_require_force_move_ = false;
  std::unique_ptr<RepairNearEnterTargeter> ptr_order_targeter_;
};

}
