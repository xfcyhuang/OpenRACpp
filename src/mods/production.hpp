// UPSTREAM: OpenRA.Mods.Common/Traits/Production.cs @b6fc03f L15-156 +
//          Traits/Player/ProductionQueue.cs L20-813(同步面全文;
//          PowerManager 的电源判定/声音与文本通知/UI 只读查询面随
//          各自批次)
//          Production.cs L15-156 + ProductionQueue.cs L20-813 (the whole
//          synchronous face; PowerManager's power predicates/the sound
//          and text notifications/the UI read-only queries ride their
//          own batches).
//
// 机制对照 / Mechanism mapping:
//  - ProductionQueue 的 Producible 字典 → 插入序 vector<pair<const
//    ActorInfo*, ProductionState>>(AllBuildables 的 rules 枚举序 = 上游
//    Actors.Values 的字典序 —— ActorInfoDictionary 的插入序键序;线性
//    查找,规模 = 可产项数)
//    ProductionQueue's Producible dictionary → the insertion-ordered
//    vector<pair<const ActorInfo*, ProductionState>> (AllBuildables'
//    rules enumeration order = upstream's Actors.Values dictionary
//    order — ActorInfoDictionary's sorted-key order; linear lookup, the
//    scale = the producible count).
//  - allProducibles/buildableProducibles 的上游 LINQ 惰性查询 → 每次
//    消费时物化的函数面(上游每枚举重算;D 系同 AutoTarget 惰性先例)
//    Upstream's lazily-evaluated allProducibles/buildableProducibles
//    LINQ chains → per-consumption materializing function faces
//    (upstream recomputes per enumeration; the D-series AutoTarget lazy
//    precedent).
//  - ProductionItem.OnComplete 的帧末闭包(捕获 notified 局部)→
//    ProductionItem 成员承载闭包状态(同一 ProductionItem 的重复
//    OnComplete 共享 notified —— 上游闭包变量提升的等价域;OPT-A9 的
//    闭包 → 成员判别式先例)
//    ProductionItem.OnComplete's frame-end closure (capturing the
//    notified local) → the ProductionItem member carries the closure
//    state (the same ProductionItem's repeated OnComplete invocations
//    share notified — upstream's hoisted closure-variable equivalent
//    domain; OPT-A9's closure→member-discriminator precedent).
//  - PowerManager 未移植:ProductionItem 的 pm == null 路径(Remaining
//    TimeActual = RemainingTime;Slowdown 跳过 —— 上游无电源 trait 的
//    等价分支;登记 COVERAGE)
//    PowerManager is unported: ProductionItem's pm == null path
//    (RemainingTimeActual = RemainingTime; Slowdown skipped —
//    upstream's equivalent branch without the power trait; registered
//    in COVERAGE).
//  - IProductionTimeModifierInfo/IProductionCostModifierInfo 的修正链
//    为空集(GetBuildTime/GetProductionCost 的 modifiers 仅
//    BuildDurationModifier 两级 —— 无该 trait 世界的等价域;登记)
//    The IProductionTimeModifierInfo/IProductionCostModifierInfo
//    modifier chains are empty sets (GetBuildTime/GetProductionCost's
//    modifiers carry only the two BuildDurationModifier tiers — the
//    equivalent domain of a world without those traits; registered).
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

// ———— Production(L15-156)————
// ———— Production (L15-156) ————

struct ProductionInfoData {
  std::vector<std::string> vec_produces;  // L21([FieldLoader.Require])
  bool b_update_faction_on_owner_change = false;  // L24
  sim::ConditionalTraitData conditional;          // PausableConditional 基

  static ProductionInfoData Parse(const meta::RecordObject& rec_info);
};

/// Production(L32-156)
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

  void Created(sim::Actor& self) override;  // L54-59
  void OnOwnerChanged(sim::Actor& self, sim::Player& old_owner,
                      sim::Player& new_owner) override;  // L151-155

  const ProductionInfoData& Info() const { return info_; }
  const std::string& Faction() const { return str_faction_; }  // L50
  sim::Actor& HostActor() const { return *p_self_; }  // Classic 序的宿主引用


  /// L61-108:DoProduction(帧末 CreateActor + 双通知)
  /// L61-108: DoProduction (the frame-end CreateActor + the two
  /// notifications).
  virtual void DoProduction(sim::Actor& self,
                            const game::ActorInfo& producee,
                            const ExitInfoData* exit_info,
                            const std::string& production_type,
                            sim::TypeDictionary& inits);

  /// L110-122:SelectExit(谓词形)
  /// L110-122: SelectExit (the predicate form).
  Exit* SelectExit(sim::Actor& self, const game::ActorInfo& producee,
                   const std::string& production_type);

  /// L124-140:Produce
  /// L124-140: Produce.
  virtual bool Produce(sim::Actor& self,
                       const game::ActorInfo& producee,
                       const std::string& production_type,
                       sim::TypeDictionary& inits, int refundable_value);

 private:
  /// L142-146:SelectExit 虚核(谓词)
  /// L142-146: SelectExit's virtual core (the predicate).
  Exit* SelectExitImpl(sim::Actor& self,
                       const game::ActorInfo& producee,
                       const std::string& production_type,
                       const std::function<bool(Exit*)>& p);

  /// L148-150:static CanUseExit
  /// L148-150: static CanUseExit.
  static bool CanUseExit(sim::Actor& self,
                         const game::ActorInfo& producee,
                         const ExitInfoData& s);

  ProductionInfoData info_;
  std::string str_faction_;
  RallyPoint* p_rally_point_ = nullptr;
  sim::Actor* p_self_ = nullptr;
};

// ———— ProductionQueue(L136-813)————
// ———— ProductionQueue (L136-813) ————

struct ProductionQueueInfoData {
  std::string str_type;                 // L27([FieldLoader.Require])
  int int4_display_order = 0;           // L30
  std::string str_group;                // L33
  std::vector<std::string> vec_factions;  // L36
  bool b_sticky = true;                 // L39
  bool b_pay_up_front = false;          // L42
  bool b_disallow_paused = false;       // L45
  int int4_build_duration_modifier = 100;  // L48
  int int4_item_limit = 999;            // L51
  int int4_queue_limit = 0;             // L54
  int int4_low_power_modifier = 100;    // L57
  int int4_infinite_build_limit = -1;   // L60
  std::string str_ready_audio;          // L63
  std::string str_blocked_audio;        // L75
  std::string str_limited_audio;        // L86
  std::string str_cannot_place_audio;   // L96
  std::string str_queued_audio;         // L100
  std::string str_on_hold_audio;        // L110
  std::string str_cancelled_audio;      // L120

  static ProductionQueueInfoData Parse(const meta::RecordObject& rec_info);
};

/// ProductionItem(L718-810;OnComplete 闭包状态内嵌)
class ProductionItemLike {
 public:
  std::string Item;
  int TotalCost = 0;
  int RemainingCost = 0;
  int ResourcesPaid = 0;
  bool Infinite = false;

  ProductionItemLike(ProductionQueue& queue, std::string item, int cost,
                     std::function<void(ProductionItemLike*)> on_complete);

  int TotalTime() const { return int4_total_time_; }
  int RemainingTime() const { return int4_remaining_time_; }
  int RemainingTimeActual() const;  // L728-731
  bool Paused() const { return b_paused_; }
  bool Done() const { return b_done_; }
  bool Started() const { return b_started_; }
  int Slowdown() const { return int4_slowdown_; }
  int BuildPaletteOrder() const { return int4_build_palette_order_; }

  /// L758-809:Tick(pr)
  /// L758-809: Tick (pr).
  void Tick(PlayerResources& pr);

  void Pause(bool paused) { b_paused_ = paused; }  // L811

  ProductionQueue* Queue = nullptr;
  std::function<void(ProductionItemLike*)> OnComplete;

  /// 帧末闭包的 notified 提升(上游闭包捕获局部;同一 item 的重复
  /// 调用共享 —— 等价域)
  /// The frame-end closure's notified hoist (upstream captures the
  /// local; the same item's repeated invocations share it — the
  /// equivalent domain).
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
};


/// ProductionQueue(L136-710;同步面)
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

  void Created(sim::Actor& self) override;  // L177-186
  void Tick(sim::Actor& self) override;     // L338-359(ITick 面 → 虚核)

  // ———— ITechTreeElement(L258-276)————
  void PrerequisitesAvailable(const std::string& key) override;
  void PrerequisitesUnavailable(const std::string& key) override;
  void PrerequisitesItemHidden(const std::string& key) override;
  void PrerequisitesItemVisible(const std::string& key) override;

  void OnOwnerChanged(sim::Actor& self, sim::Player& old_owner,
                      sim::Player& new_owner) override;  // L205-224
  void Killed(sim::Actor& killed,
              const sim::AttackInfo& e) override;  // L226
  void Selling(sim::Actor& self) override;         // L227
  void Sold(sim::Actor& self) override {}          // L228
  void BeforeTransform(sim::Actor& self) override;  // L230
  void OnTransform(sim::Actor& self) override {}    // L231
  void AfterTransform(sim::Actor& to_actor) override {}  // L232

  void ResolveOrder(sim::Actor& self,
                    const ora::net::Order& order) override;  // L447-539

  /// L343-359:protected virtual Tick(基实现;Classic 覆写)
  /// L343-359: the protected virtual Tick (the base implementation;
  /// Classic overrides).
  virtual void TickImpl(sim::Actor& self);

  // ———— 查询面(Classic 覆写者 virtual)————
  // ———— The query faces (Classic's overriders virtual). ————
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

  /// L404-445:CanQueue(通知双出参 → 双引用)
  /// L404-445: CanQueue (the two notification out-parameters → the two
  /// references).
  bool CanQueue(const game::ActorInfo& actor,
                std::string& notification_audio,
                std::string& notification_text);

  /// L541-556:GetBuildTime
  /// L541-556: GetBuildTime.
  virtual int GetBuildTime(const game::ActorInfo& unit);

  /// L558-568:GetProductionCost
  /// L558-568: GetProductionCost.
  virtual int GetProductionCost(const game::ActorInfo& unit);

  /// L570-572:PauseProduction | L574-580:CancelProduction
  void PauseProduction(const std::string& item_name, bool paused);
  void CancelProduction(const std::string& item_name,
                        std::uint32_t number_to_cancel);

  /// L582-611:CancelProductionInner
  /// L582-611: CancelProductionInner.
  bool CancelProductionInner(const std::string& item_name);

  /// L613-619:EndProduction
  /// L613-619: EndProduction.
  void EndProduction(ProductionItemLike& item);

  /// L621-663:BeginProduction
  /// L621-663: BeginProduction.
  virtual void BeginProduction(std::unique_ptr<ProductionItemLike> item,
                               bool has_priority);

  /// L665-668:RemainingTimeActual | L671-678:MostLikelyProducer
  int RemainingTimeActual(const ProductionItemLike& item) const;
  virtual Production* MostLikelyProducer();

  /// L680-709:BuildUnit
  /// L680-709: BuildUnit.
  virtual bool BuildUnit(const game::ActorInfo& unit);

  const ProductionQueueInfoData& Info() const { return info_; }
  sim::Actor* Actor() const { return p_actor_; }
  bool Enabled() const { return b_enabled_; }
  bool IsValidFaction() const { return b_is_valid_faction_; }  // [VerifySync]
  const std::string& Faction() const { return str_faction_; }

 protected:
  /// L188-203:ClearQueue | L369-402:CancelUnbuildableItems
  void ClearQueue();
  void CancelUnbuildableItems();

  /// L234-247:CacheProducibles | L249-255:AllBuildables
  void CacheProducibles();
  std::vector<const game::ActorInfo*> AllBuildables(
      const std::string& category) const;

  /// L361-367:TickInner
  /// L361-367: TickInner.
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
  std::vector<ProducibleEntry> vec_producible_;  // 插入序 = rules 枚举序
  std::vector<std::unique_ptr<ProductionItemLike>> vec_queue_;
  std::vector<Production*> vec_production_traits_;
  PlayerResources* p_player_resources_ = nullptr;
  DeveloperMode* p_developer_mode_ = nullptr;
  TechTree* p_tech_tree_ = nullptr;
  bool b_enabled_ = false;  // [VerifySync] Enabled
  std::string str_faction_;
  bool b_is_valid_faction_ = false;  // [VerifySync]
};

/// ClassicProductionQueue(ClassicProductionQueue.cs L85-139;player
/// actor 挂载的共享队列 —— 世界级 producer 查找)
/// ClassicProductionQueue (ClassicProductionQueue.cs L85-139; the
/// player-actor-mounted shared queue — the world-scope producer lookup).
class ClassicProductionQueue final : public ProductionQueue {
 public:
  // upcast 表含基类 ProductionQueue 的全接口集(上游 GetInterfaces()
  // 含继承接口;漏列 = TypeDictionary 键缺失 → Created/Tick 分发断链)
  // The upcast table carries the base ProductionQueue's full interface
  // set (upstream's GetInterfaces() includes inherited interfaces;
  // omitting them = missing TypeDictionary keys → the Created/Tick
  // dispatch chain breaks).
  ORA_TRAIT_INTERFACES(
      ClassicProductionQueue,
      OpenRA_Mods_Common_Traits_ClassicProductionQueue, ProductionQueue,
      sim::IResolveOrder, sim::ITick, sim::ITechTreeElement,
      sim::INotifyOwnerChanged, sim::INotifyKilled, sim::INotifySold,
      sim::ISync, sim::INotifyTransform, sim::INotifyCreated)

  ClassicProductionQueue(sim::ActorInitializer& init,
                         ProductionQueueInfoData info)
      : ProductionQueue{init, std::move(info)} {}

  void TickImpl(sim::Actor& self) override;  // L58-83
  std::vector<const game::ActorInfo*> AllItems() override;  // L85-88
  std::vector<const game::ActorInfo*> BuildableItems() override;  // L80-83
  Production* MostLikelyProducer() override;  // L85-99
  bool BuildUnit(const game::ActorInfo& unit) override;  // L101-139

  /// SpeedUp/BuildTimeSpeedReduction 的加速面随批登记(缺省 false
  /// [100] = 无加速的等价域)
  /// The SpeedUp/BuildTimeSpeedReduction acceleration face is
  /// registered for a later batch (the default false/[100] = the
  /// no-acceleration equivalent domain).
};

/// ProductionState(L712-716)的承载(ProducibleEntry 内联;独立类省)
/// ProductionState's carrier (inlined into ProducibleEntry; the
/// standalone class omitted).

// ———— Exit 扩展(ExitExts.cs L44-93)————
// ———— The Exit extensions (ExitExts.cs L44-93) ————

/// ExitExts.cs L44-59:NearestExitOrDefault(OrderByDescending(Priority)
/// .ThenBy(dist²) 的稳定序;谓词首匹配)
/// ExitExts.cs L44-59: NearestExitOrDefault (OrderByDescending(Priority)
/// .ThenBy(dist²)'s stable order; the first predicate match).
Exit* NearestExitOrDefault(sim::Actor& actor, const WPos& pos,
                           const std::string& production_type,
                           const std::function<bool(Exit*)>& p = nullptr);

/// ExitExts.cs L61-73:Exits | L75-93:RandomExitOrDefault(GroupBy
/// (Priority) 首遇组序 + Shuffle(SharedRandom) 的 Fisher-Yates)
/// ExitExts.cs L61-73: Exits | L75-93: RandomExitOrDefault (GroupBy
/// (Priority)'s first-encounter order + Shuffle(SharedRandom)'s
/// Fisher-Yates).
std::vector<Exit*> ActorExits(sim::Actor& actor,
                              const std::string& production_type);
Exit* RandomExitOrDefault(sim::Actor& actor, sim::World& world,
                          const std::string& production_type,
                          const std::function<bool(Exit*)>& p = nullptr);

/// Exts.cs 的 Shuffle(Fisher-Yates;SharedRandom 序保真)
/// Exts.cs's Shuffle (Fisher-Yates; the SharedRandom order preserved).
template <class T>
void ShuffleSpan(std::vector<T>& vec, MersenneTwister& rng) {
  for (std::size_t i = vec.size(); i > 1; i--) {
    // Pick random element to swap into position i-1(上游注释)
    const std::size_t j = static_cast<std::size_t>(
        rng.Next(static_cast<std::int32_t>(i)));
    std::swap(vec[j], vec[i - 1]);
  }
}

}  // namespace ora::mods
