// UPSTREAM: OpenRA.Mods.Common/Traits/DockClientBase.cs @b6fc03f L15-56
//          全文 + DockClientManager.cs L22-438 全文(逐语义重写;游标/声音
//          的 UI 参照面与 BooleanExpression 观察者链保留)
//          The whole of DockClientBase.cs L15-56 + the whole of
//          DockClientManager.cs L22-438 (verbatim-semantics rewrites; the
//          cursor/sound UI references and the BooleanExpression observer
//          chain kept).
//
// 机制对照 / Mechanism mapping:
//  - abstract DockClientBase<InfoType> → DockClientBaseCore<ConcreteT>
//    CRTP 组合核(条件 trait 四钩子;ConcreteT 承载 ORA_TRAIT_INTERFACES)
//    The abstract DockClientBase<InfoType> → the DockClientBaseCore<
//    ConcreteT> CRTP composition core (the condition-trait four hooks;
//    ConcreteT carries ORA_TRAIT_INTERFACES).
//  - DockActorTargeter 的 Func<CanTargetContext, CanTargetResult> 闭包 →
//    std::function(求值体;Blocked/Allowed 记录结构体承载)
//    DockActorTargeter's Func<CanTargetContext, CanTargetResult> closure →
//    std::function (the evaluation body; the Blocked/Allowed records carry
//    the payloads).
//  - EnterCursorOverrides 的 FrozenDictionary<string, string> → 插入序
//    vector<pair>(TryGetValue = 首见键)
//    EnterCursorOverrides' FrozenDictionary<string, string> → the
//    insertion-ordered vector<pair> (TryGetValue = the first-seen key).
//  - DockExts.ClosestDock 的 AggregateBy(格子去重)→ map<CPos, TraitPair>
//    (首值保留),路径搜索照抄(Mobile 无 → 距离+占用排序)
//    DockExts.ClosestDock's AggregateBy (the per-cell dedup) → map<CPos,
//    TraitPair> (the first value kept); the path search copied verbatim
//    (no Mobile → the distance+occupancy ordering).
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

/// DockClientBaseInfo(DockClientBase.cs L17)的解析面(空体 + 条件面)
/// The parsed face of DockClientBaseInfo (L17) (an empty body + the
/// conditional face).
struct DockClientBaseInfoData {
  sim::ConditionalTraitData conditional;
};

/// DockClientManagerInfo(L25-58)的解析面
/// The parsed face of DockClientManagerInfo (L25-58).
struct DockClientManagerInfoData {
  int int4_search_for_dock_delay = 125;  // L28
  int int4_occupancy_cost_modifier = 12;  // L31
  std::optional<expr::BooleanExpression>
      expr_require_force_move;           // L35
  std::string str_enter_cursor{"enter"};  // L39
  std::vector<std::pair<std::string, std::string>>
      vec_enter_cursor_overrides;         // L44
  std::string str_enter_blocked_cursor{"enter-blocked"};  // L48
  std::string str_voice{"Action"};        // L52
  core::Color color_dock_line =
      core::Color::FromArgb(0, 128, 0);   // L55 Green
  sim::ConditionalTraitData conditional;

  static DockClientManagerInfoData Parse(const meta::RecordObject& rec_info);
};

/// DockClientBase<InfoType>(DockClientBase.cs L19-55)的 CRTP 组合核
/// The CRTP composition core of DockClientBase<InfoType> (L19-55).
template <class ConcreteT>
class DockClientBaseCore : public TraitBase,
                           public sim::ConditionalTraitCore<ConcreteT>,
                           public sim::IDockClient {
 public:
  DockClientBaseCore(Actor& self, const sim::ConditionalTraitData& conditional)
      : sim::ConditionalTraitCore<ConcreteT>(conditional),
        p_self_{&self} {
    // L26-31
    p_dock_client_manager_ = self.TraitOrDefault<DockClientManager>();
  }

  // ———— IDockClient(L44 的 abstract GetDockType)————
  // ———— IDockClient (L44's abstract GetDockType) ————
  core::BitSet<sim::DockType> GetDockType() override = 0;
  DockClientManager* GetDockClientManager() override {
    return p_dock_client_manager_;
  }

  /// L33-36:CanDock | L33-36: CanDock.
  bool CanDock(const core::BitSet<sim::DockType>& type,
               bool force_enter = false) override {
    return !sim::ConditionalTraitCore<ConcreteT>::IsTraitDisabled() &&
           GetDockType().Overlaps(type);
  }

  /// L38-42:CanDockAt | L38-42: CanDockAt.
  bool CanDockAt(Actor& host_actor, sim::IDockHost* host,
                 bool force_enter = false,
                 bool ignore_occupancy = false) override {
    return CanDock(host->GetDockType(), force_enter) &&
           host->IsDockingPossible(*p_self_, this, ignore_occupancy);
  }

  /// L44-48:CanQueueDockAt | L44-48: CanQueueDockAt.
  bool CanQueueDockAt(Actor& host_actor, sim::IDockHost* host,
                      bool force_enter, bool is_queued) override {
    return CanDock(host->GetDockType(), true) &&
           host->IsDockingPossible(*p_self_, this, true);
  }

  /// L50-54:OnDock 族(空体;子类覆写)
  /// The OnDock family (empty bodies; subclass overrides).
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
  Actor* p_self_ = nullptr;                        // L21
  DockClientManager* p_dock_client_manager_ = nullptr;  // L24
};

class DockActorTargeter;  // 本文件下方定义(Orders 载体先行)
                          // (defined below in this file; the Orders
                          // carrier's forward).

/// DockClientManager(L60-335)
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

  /// L80-98:ReservedHostActor/ReservedHost/LastReservedHost
  /// L80-98: ReservedHostActor/ReservedHost/LastReservedHost.
  Actor* ReservedHostActor() const { return p_reserved_host_actor_; }
  sim::IDockHost* ReservedHost() const { return p_reserved_host_; }
  sim::IDockHost* LastReservedHost();

  /// L100-109:UnreserveHost | L100-109: UnreserveHost.
  void UnreserveHost();

  /// L111-132:ReserveHost | L111-132: ReserveHost.
  bool ReserveHost(Actor* host_actor, sim::IDockHost* host);

  /// L134-138/140-151/153-159:OnDock 分发
  /// The OnDock dispatches (L134-138/140-151/153-159).
  void OnDockStarted(Actor& self, Actor& host_actor, sim::IDockHost* host);
  bool OnDockTick(Actor& self, Actor& host_actor, sim::IDockHost* host);
  void OnDockCompleted(Actor& self, Actor& host_actor, sim::IDockHost* host);

  // ———— order 面(L161-244)————
  // ———— The order faces (L161-244) ————
  std::vector<sim::IOrderTargeter*> Orders() override;
  net::Order* IssueOrder(Actor& self, sim::IOrderTargeter* order,
                         const sim::Target& target, bool queued) override;
  void ResolveOrder(Actor& self, const net::Order& order) override;
  std::string VoicePhraseForOrder(Actor& self,
                                  const net::Order& order) override;

  /// L261-294:CanDock 族 | L261-294: the CanDock family.
  bool CanDock(const core::BitSet<sim::DockType>& type,
               bool force_enter = false);
  bool CanDock(Actor& target, bool force_enter = false);
  bool CanDockAt(Actor& host_actor, sim::IDockHost* host,
                 bool force_enter = false,
                 bool ignore_occupancy = false);
  bool CanDockAt(Actor& target, bool force_enter = false,
                 bool ignore_occupancy = false);
  bool CanQueueDockAt(Actor& target, bool force_enter, bool is_queued);

  /// L304-312:ClosestDock(最近可用 DockHost;移动面 = DockExts.
  /// ClosestDock 的 C++ 自由函数)
  /// L304-312: ClosestDock (the nearest viable DockHost; the movement
  /// face = the C++ free function of DockExts.ClosestDock).
  std::optional<sim::TraitPair<sim::IDockHost>> ClosestDock(
      sim::IDockHost* ignore,
      core::BitSet<sim::DockType> type = {},
      bool force_enter = false, bool ignore_occupancy = false) const;

  /// L316-323:AvailableDockHosts(target 上的可用 DockHost 集)
  /// L316-323: AvailableDockHosts (the viable DockHosts on the target).
  std::vector<sim::TraitPair<sim::IDockHost>> AvailableDockHosts(
      Actor& target, core::BitSet<sim::DockType> type = {},
      bool force_enter = false, bool ignore_occupancy = false) const;

  /// L327-330:AvailableDockClients(类型匹配的客户端集)
  /// L327-330: AvailableDockClients (the type-matching clients).
  std::vector<sim::IDockClient*> AvailableDockClients(
      const core::BitSet<sim::DockType>& type,
      bool force_enter = false) const;

  void Killed(Actor& self, const sim::AttackInfo& e) override;
  void Disposing(Actor& self) override;

  void TraitEnabledHook(Actor& /*self*/) {}
  void TraitDisabledHook(Actor& /*self*/) {}
  void TraitResumedHook(Actor& /*self*/) {}
  void TraitPausedHook(Actor& /*self*/) {}

  const DockClientManagerInfoData& Info() const { return info_; }
  std::span<sim::IDockClient* const> DockClients() const {
    return vec_dock_clients_;
  }

 private:
  friend class sim::ConditionalTraitCore<DockClientManager>;

  /// L296-300:GetDockableHosts
  /// L296-300: GetDockableHosts.
  std::vector<sim::IDockHost*> GetDockableHosts(
      Actor& target, bool force_enter, bool is_queued) const;

  /// L255-258:RequireForceMoveConditionChanged
  /// L255-258: RequireForceMoveConditionChanged.
  void RequireForceMoveConditionChanged(
      Actor& self, sim::ConditionCacheView conditions);

  const DockClientManagerInfoData info_;
  Actor* p_self_ = nullptr;  // L62
  std::vector<sim::IDockClient*> vec_dock_clients_;  // L63 的物化
  bool b_require_force_move_ = false;                // L66
  std::vector<std::unique_ptr<DockActorTargeter>>
      vec_order_targeters_;  // Orders() 的载体(生命周期随 trait)

  Actor* p_reserved_host_actor_ = nullptr;  // L80
  sim::IDockHost* p_reserved_host_ = nullptr;  // L81
  sim::IDockHost* p_last_reserved_dock_host_ = nullptr;  // L83
};

/// DockActorTargeter(DockClientManager.cs L337-394;嵌套 → 独立类)
/// DockActorTargeter (DockClientManager.cs L337-394; nested → standalone).
class DockActorTargeter final : public sim::IOrderTargeter {
 public:
  /// L370-377:CanTargetContext(record struct)
  /// L370-377: CanTargetContext (the record struct).
  struct CanTargetContext {
    sim::Target target;
    bool b_force_enter = false;
    bool b_is_queued = false;
  };

  /// L379-394:CanTargetResult(record struct)
  /// L379-394: CanTargetResult (the record struct).
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

/// DockExts.ClosestDock(DockClientManager.cs L397-437;扩展方法 → 自由函数)
/// DockExts.ClosestDock (DockClientManager.cs L397-437; the extension
/// method → a free function).
std::optional<sim::TraitPair<sim::IDockHost>> ClosestDock(
    std::span<const sim::TraitPair<sim::IDockHost>> vec_docks,
    Actor& client_actor, const DockClientManager& client);

}  // namespace ora::mods
