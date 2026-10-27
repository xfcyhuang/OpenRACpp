#pragma once
import std;

#include "core/color.hpp"
#include "meta/variable_expression.hpp"
#include "mods/mobile.hpp"
#include "mods/move_activities.hpp"
#include "sim/actor.hpp"
#include "sim/actor_init.hpp"
#include "sim/conditional_trait.hpp"
#include "sim/trait_interfaces.hpp"

namespace ora::meta {
class RecordObject;
}

namespace ora::mods {

using sim::Actor;
using sim::ActorInitializer;
using sim::TraitBase;

struct DockClientBaseInfoData {
  sim::ConditionalTraitData conditional;
};

struct DockClientManagerInfoData {
  int int4_search_for_dock_delay = 125;
  int int4_occupancy_cost_modifier = 12;
  std::optional<expr::BooleanExpression>
      expr_require_force_move;
  std::string str_enter_cursor{"enter"};
  std::vector<std::pair<std::string, std::string>>
      vec_enter_cursor_overrides;
  std::string str_enter_blocked_cursor{"enter-blocked"};
  std::string str_voice{"Action"};
  core::Color color_dock_line =
      core::Color::FromArgb(0, 128, 0);
  sim::ConditionalTraitData conditional;

  static DockClientManagerInfoData Parse(const meta::RecordObject& rec_info);
};

template <class ConcreteT>
class DockClientBaseCore : public TraitBase,
                           public sim::ConditionalTraitCore<ConcreteT>,
                           public sim::IDockClient {
 public:
  DockClientBaseCore(Actor& self, const sim::ConditionalTraitData& conditional)
      : sim::ConditionalTraitCore<ConcreteT>(conditional),
        p_self_{&self} {

    p_dock_client_manager_ = self.TraitOrDefault<DockClientManager>();
  }

  core::BitSet<sim::DockType> GetDockType() override = 0;
  DockClientManager* GetDockClientManager() override {
    return p_dock_client_manager_;
  }

  bool CanDock(const core::BitSet<sim::DockType>& type,
               bool force_enter = false) override {
    return !sim::ConditionalTraitCore<ConcreteT>::IsTraitDisabled() &&
           GetDockType().Overlaps(type);
  }

  bool CanDockAt(Actor& host_actor, sim::IDockHost* host,
                 bool force_enter = false,
                 bool ignore_occupancy = false) override {
    return CanDock(host->GetDockType(), force_enter) &&
           host->IsDockingPossible(*p_self_, this, ignore_occupancy);
  }

  bool CanQueueDockAt(Actor& host_actor, sim::IDockHost* host,
                      bool force_enter, bool is_queued) override {
    return CanDock(host->GetDockType(), true) &&
           host->IsDockingPossible(*p_self_, this, true);
  }

  void OnDockStarted(Actor& self, Actor& host_actor,
                     sim::IDockHost* host) override {
    [[maybe_unused]] auto _ =
        std::tie(self, host_actor, host);
  }
  bool OnDockTick(Actor& self, Actor& host_actor,
                  sim::IDockHost* host) override {
    [[maybe_unused]] auto _ = std::tie(self, host_actor, host);
    return false;
  }
  void OnDockCompleted(Actor& self, Actor& host_actor,
                       sim::IDockHost* host) override {
    [[maybe_unused]] auto _ = std::tie(self, host_actor, host);
  }

  Actor* Self() const { return p_self_; }

 protected:
  Actor* p_self_ = nullptr;
  DockClientManager* p_dock_client_manager_ = nullptr;
};

class DockActorTargeter;

class DockClientManager final : public TraitBase,
                                public sim::ConditionalTraitCore<
                                    DockClientManager>,
                                public sim::IObservesVariables,
                                public sim::INotifyCreated,
                                public sim::IResolveOrder,
                                public sim::IOrderVoice,
                                public sim::IIssueOrder,
                                public sim::INotifyKilled,
                                public sim::INotifyActorDisposing {
 public:
  DockClientManager(ActorInitializer& init,
                    const DockClientManagerInfoData& info);

  ORA_TRAIT_INTERFACES(
      DockClientManager, OpenRA_Mods_Common_Traits_DockClientManager,
      sim::IObservesVariables, sim::INotifyCreated, sim::IResolveOrder,
      sim::IOrderVoice, sim::IIssueOrder, sim::INotifyKilled,
      sim::INotifyActorDisposing)

  bool IsTraitEnabled() const override {
    return !sim::ConditionalTraitCore<
        DockClientManager>::IsTraitDisabled();
  }
  bool IsTraitDisabled() const override {
    return sim::ConditionalTraitCore<
        DockClientManager>::IsTraitDisabled();
  }

  void Created(Actor& self) override;
  std::vector<sim::VariableObserver> GetVariableObservers() override;

  core::Color DockLineColor() const {
    return info_.color_dock_line;
  }
  int OccupancyCostModifier() const {
    return info_.int4_occupancy_cost_modifier;
  }

  Actor* ReservedHostActor() const { return p_reserved_host_actor_; }
  sim::IDockHost* ReservedHost() const { return p_reserved_host_; }
  sim::IDockHost* LastReservedHost();

  void UnreserveHost();

  bool ReserveHost(Actor* host_actor, sim::IDockHost* host);

  void OnDockStarted(Actor& self, Actor& host_actor, sim::IDockHost* host);
  bool OnDockTick(Actor& self, Actor& host_actor, sim::IDockHost* host);
  void OnDockCompleted(Actor& self, Actor& host_actor, sim::IDockHost* host);

  std::vector<sim::IOrderTargeter*> Orders() override;
  std::unique_ptr<net::Order> IssueOrder(
      Actor& self, sim::IOrderTargeter* order,
      const sim::Target& target, bool queued) override;
  void ResolveOrder(Actor& self, const net::Order& order) override;
  std::string VoicePhraseForOrder(Actor& self,
                                  const net::Order& order) override;

  bool CanDock(const core::BitSet<sim::DockType>& type,
               bool force_enter = false);
  bool CanDock(Actor& target, bool force_enter = false);
  bool CanDockAt(Actor& host_actor, sim::IDockHost* host,
                 bool force_enter = false,
                 bool ignore_occupancy = false);
  bool CanDockAt(Actor& target, bool force_enter = false,
                 bool ignore_occupancy = false);
  bool CanQueueDockAt(Actor& target, bool force_enter, bool is_queued);

  std::optional<sim::TraitPair<sim::IDockHost>> ClosestDock(
      sim::IDockHost* ignore,
      core::BitSet<sim::DockType> type = {},
      bool force_enter = false, bool ignore_occupancy = false) const;

  std::vector<sim::TraitPair<sim::IDockHost>> AvailableDockHosts(
      Actor& target, core::BitSet<sim::DockType> type = {},
      bool force_enter = false, bool ignore_occupancy = false) const;

  std::vector<sim::IDockClient*> AvailableDockClients(
      const core::BitSet<sim::DockType>& type,
      bool force_enter = false) const;

  void Killed(Actor& self, const sim::AttackInfo& e) override;
  void Disposing(Actor& self) override;

  void TraitEnabledHook(Actor& ) {}
  void TraitDisabledHook(Actor& ) {}
  void TraitResumedHook(Actor& ) {}
  void TraitPausedHook(Actor& ) {}

  const DockClientManagerInfoData& Info() const { return info_; }
  std::span<sim::IDockClient* const> DockClients() const {
    return vec_dock_clients_;
  }

 private:
  friend class sim::ConditionalTraitCore<DockClientManager>;

  std::vector<sim::IDockHost*> GetDockableHosts(
      Actor& target, bool force_enter, bool is_queued) const;

  void RequireForceMoveConditionChanged(
      Actor& self, sim::ConditionCacheView conditions);

  const DockClientManagerInfoData info_;
  Actor* p_self_ = nullptr;
  std::vector<sim::IDockClient*> vec_dock_clients_;
  bool b_require_force_move_ = false;
  std::vector<std::unique_ptr<DockActorTargeter>>
      vec_order_targeters_;

  Actor* p_reserved_host_actor_ = nullptr;
  sim::IDockHost* p_reserved_host_ = nullptr;
  sim::IDockHost* p_last_reserved_dock_host_ = nullptr;
};

class DockActorTargeter final : public sim::IOrderTargeter {
 public:

  struct CanTargetContext {
    sim::Target target;
    bool b_force_enter = false;
    bool b_is_queued = false;
  };

  struct CanTargetResult {
    std::string str_cursor;
    bool b_can_target = false;

    static CanTargetResult Blocked(std::string str_cursor = "") {
      return CanTargetResult{std::move(str_cursor), false};
    }
    static CanTargetResult Allowed(std::string str_cursor) {
      return CanTargetResult{std::move(str_cursor), true};
    }
  };

  DockActorTargeter(
      int priority,
      std::function<CanTargetResult(const CanTargetContext&)>
          fn_can_target);

  std::string OrderID() const override { return str_order_id_; }
  int OrderPriority() const override { return int4_priority_; }
  bool IsQueued() const override { return b_is_queued_; }
  bool TargetOverridesSelection(
      Actor& self, const sim::Target& target,
      std::span<Actor* const> actors_at, CPos xy,
      sim::TargetModifiers modifiers) override;
  bool CanTarget(Actor& self, const sim::Target& target,
                 sim::TargetModifiers& modifiers,
                 std::string& cursor) override;

 private:
  int int4_priority_;
  std::function<CanTargetResult(const CanTargetContext&)> fn_can_target_;
  std::string str_order_id_{"Dock"};
  bool b_is_queued_ = false;
};

std::optional<sim::TraitPair<sim::IDockHost>> ClosestDock(
    std::span<const sim::TraitPair<sim::IDockHost>> vec_docks,
    Actor& client_actor, const DockClientManager& client);

}
