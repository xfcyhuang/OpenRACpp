// UPSTREAM: OpenRA.Game/Actor.cs @b6fc03f L27-651(逐语义重写;渲染/Lua 面
//          随 Phase 4/8)
//          Verbatim-semantics rewrite; the render/Lua surfaces land with
//          Phase 4/8.
//
// 机制对照 / Mechanism mapping:
//  - 条件系统(L89-109/558-615):Dictionary→std::map(键序稳定;ConditionState
//    的 HashSet<int> Tokens 仅做成员测试与计数 → std::set<int>);
//    IReadOnlyDictionary 只读视图 → const std::map&
//    The condition system (L89-109/558-615): Dictionary→std::map (stable
//    key order; ConditionState's HashSet<int> Tokens only does membership
//    and counting → std::set<int>); the IReadOnlyDictionary read-only view
//    → const std::map&.
//  - Initialize 的初始通知去重(L221/241):C# HashSet<delegate> 按方法+目标
//    去重;std::function 无身份 —— 改为逐 observer 条目顺序通知(与
//    UpdateConditionState 的重复调用语义一致;受端幂等是 trait 契约)。
//    COVERAGE 登记
//    The Initialize initial-notification dedup (L221/241): C# dedups
//    HashSet<delegate> by method+target; std::function has no identity —
//    notify each observer entry in order instead (matching the
//    repeat-call semantics of UpdateConditionState; idempotent receivers
//    are the trait contract). Registered in COVERAGE.
//  - trait 创建(L169-171):traitInfo.Create(init) 的工厂面 Phase 5 才有具体
//    trait 类 —— 经 World 注入的 trait 工厂承载(返回构造序 trait 列表),
//    Actor 侧"逐 trait AddTrait + 接口缓存"循环保持上游形态
//    Trait creation (L169-171): the traitInfo.Create(init) factory surface
//    has no concrete trait classes until Phase 5 — carried by a
//    World-injected trait factory (returning the construct-order trait
//    list); the Actor-side "AddTrait each + cache interfaces" loop keeps
//    the upstream shape.
//  - 规则表查询(L152-155):world.Map.Rules.Actors[name] 的规则面同样在工厂
//    内闭合(name 小写/查表/异常文本 "No rules definition for unit X" 逐字)
//    The rules lookup (L152-155): the world.Map.Rules.Actors[name] surface
//    also closes inside the factory (name lower-casing, table lookup, and
//    the verbatim "No rules definition for unit X" error).
//  - 渲染缓存面(L71-127/292-349:Render/ScreenBounds/MouseBounds/renders/
//    renderModifiers/mouseBounds/visibility...):Phase 4;CanBeViewedByPlayer
//    的可见性判定依赖 Shroud(Phase 5),本文件保留接口位
//    The render caches (L71-127/292-349) are Phase 4; CanBeViewedByPlayer's
//    visibility check depends on Shroud (Phase 5) — interface slots kept.
#pragma once
import std;

#include "sim/activity.hpp"
#include "sim/type_dictionary.hpp"
#include "sim/sync_hash.hpp"
#include "sim/trait_interfaces.hpp"

namespace ora::game {
class ActorInfo;  // Phase 2 加载链产物;Phase 5 与 sim 运行时接线(the Phase 2 loading-chain product; wired to the sim runtime in Phase 5)
}

namespace ora::net {
struct Order;
}

namespace ora::sim {

class World;
class Player;

/// SystemActors 枚举(Actor.cs L27-34)
/// The SystemActors enum (Actor.cs L27-34).
enum class SystemActors : std::int32_t {
  Player = 0,
  EditorPlayer = 1,
  World = 2,
  EditorWorld = 4,
};

/// Actor.SyncHash(L41-47):ISync trait + 其哈希函数
/// Actor.SyncHash (L41-47): an ISync trait plus its hash function.
struct ActorSyncHashEntry {
  ISync* trait = nullptr;
  int (*hash_function)(const ISync*) = nullptr;
  int Hash() const { return hash_function != nullptr ? hash_function(trait) : 0; }
};

/// Actor(Actor.cs L36-651)
class Actor final {
 public:
  /// Actor.cs L39
  static constexpr int InvalidConditionToken = -1;

  /// Actor.cs L129-211:name 可为空(空 = 无规则 actor,上游 Info 为 null 的
  /// 事实路径);trait 工厂由 World 注入(见上)
  /// Actor.cs L129-211: the name may be empty (an empty name is the
  /// upstream fact path where Info stays null); the trait factory is
  /// injected by the World (see above).
  Actor(World& world, std::string str_name, TypeDictionary& init_dict);
  ~Actor();

  Actor(const Actor&) = delete;
  Actor& operator=(const Actor&) = delete;

  // ———— 身份与状态(L49-68)————
  const game::ActorInfo* Info() const { return info_; }
  const std::string& InfoName() const { return str_info_name_; }
  std::string DebugName() const;  // ToString(L390-397)
  World& world() const { return world_; }
  std::uint32_t ActorID() const { return uint4_actor_id_; }
  Player* Owner() const { return p_owner_; }
  void SetOwnerInternal(Player* p) { p_owner_ = p; }  // ChangeOwnerSync 用
  bool IsInWorld() const { return b_is_in_world_; }
  void SetIsInWorld(bool b) { b_is_in_world_ = b; }  // World.Add/Remove 用

  /// Actor.cs L157-159:Info 在 trait 构造循环之前赋值(CreateTraitsForActor
  /// 的全量路径;测试注入路径不置)
  /// Actor.cs L157-159: Info is assigned before the trait-construction
  /// loop (CreateTraitsForActor's full path; the test-injection path leaves
  /// it unset).
  void SetInfo(const game::ActorInfo* info) { info_ = info; }
  bool WillDispose() const { return b_will_dispose_; }
  bool Disposed() const { return b_disposed_; }

  Activity* CurrentActivity() {
    return Activity::SkipDoneActivities(current_activity_);
  }
  void SetCurrentActivity(Activity* a) { current_activity_ = a; }

  int Generation() const { return int4_generation_; }
  void BumpGeneration() { ++int4_generation_; }
  Actor* ReplacedByActor() const { return p_replaced_by_; }
  void SetReplacedByActor(Actor* a) { p_replaced_by_ = a; }

  bool IsIdle() const {
    return const_cast<Actor*>(this)->CurrentActivity() == nullptr;
  }

  // ———— 位置/朝向/目标面(经缓存 trait;Phase 3 为空 trait 时不可调)————
  CPos Location() const;       // L84
  WPos CenterPosition() const; // L85
  bool IsDead() const;               // L82

  /// OccupiesSpace(L85 的单值语义:最后写入者) | OccupiesSpace (the L85
  /// single-value semantics: last writer wins).
  IOccupySpace* OccupiesSpace() const {
    return vec_occupy_space_.empty() ? nullptr : vec_occupy_space_.back();
  }

  // ———— 第三批缓存面 + 扩展(Actor.cs L169-195 的接口缓存;
  //      Orientation/Targetables/Crushables/CanBeViewedByPlayer)————
  // ———— The batch-3 cached faces + extensions (Actor.cs L169-195's
  //      interface caches; Orientation/Targetables/Crushables/
  //      CanBeViewedByPlayer) ————

  /// Actor.cs L87:Orientation => facing?.Orientation ?? WRot.None
  WRot Orientation() const {
    return p_facing_ != nullptr ? p_facing_->Orientation() : WRot::None();
  }

  /// Actor.cs L517-525:GetAllTargetTypes(构造序并集)
  /// Actor.cs L517-525: GetAllTargetTypes (the construct-order union).
  core::BitSet<TargetableType> GetAllTargetTypes() const;

  /// Actor.cs L530-538:GetEnabledTargetTypes(启用者并集)
  /// Actor.cs L530-538: GetEnabledTargetTypes (the enabled union).
  core::BitSet<TargetableType> GetEnabledTargetTypes() const;

  /// Actor.cs L540-548:IsTargetableBy(任一 ITargetable.TargetableBy)
  /// Actor.cs L540-548: IsTargetableBy (any ITargetable.TargetableBy).
  bool IsTargetableBy(Actor& by_actor) const;

  /// Actor.cs L511-515:CanBeViewedByPlayer —— 可见性修饰面(IVisibility
  /// 修饰/默认可见)未移植前恒走 defaultVisibility == null 的真分支
  /// (上游无修饰 trait 时同值;Shroud 批接线 —— COVERAGE 登记)
  /// Actor.cs L511-515: CanBeViewedByPlayer — until the visibility-modifier
  /// faces (IVisibility/default visibility) are ported this stays on the
  /// defaultVisibility == null true branch (the same value upstream gives
  /// without modifier traits; wired with the Shroud batch — registered in
  /// COVERAGE).
  bool CanBeViewedByPlayer(Player* player) const {
    for (auto* modifier : vec_visibility_modifiers_)
      if (!modifier->IsVisible(const_cast<Actor&>(*this), player))
        return false;

    return p_default_visibility_ != nullptr
               ? p_default_visibility_->IsVisible(const_cast<Actor&>(*this),
                                                  player)
               : true;
  }

  /// Actor.cs L399-417 缓存面:Targetables/Crushables(构造序)
  /// The Actor.cs L399-417 cached faces: Targetables/Crushables
  /// (construct order).
  std::span<ITargetable* const> Targetables() const { return vec_targetables_; }
  std::span<ICrushable* const> Crushables() const { return vec_crushables_; }

  /// RejectsOrdersExts.cs L50-72:AcceptsOrder —— RejectsOrders trait 未
  /// 移植:无该 trait 时上游恒真(rejectsOrdersTraits.Length == 0 分支),
  /// 空集等价面(trait 批接线;COVERAGE 登记)
  /// RejectsOrdersExts.cs L50-72: AcceptsOrder — the RejectsOrders trait
  /// is unported: upstream is constantly true without the trait (the
  /// rejectsOrdersTraits.Length == 0 branch), the empty-set equivalent
  /// face (wired with the trait batch; registered in COVERAGE).
  bool AcceptsOrder(std::string_view /*order_string*/) const { return true; }

  // ———— Tick(L272-290)————
  void Tick();

  // ———— 活动队列(L351-373)————
  void QueueActivity(bool queued, Activity* next_activity);
  void QueueActivity(Activity* next_activity);
  void CancelActivity();

  // ———— trait 查询转发(L399-417;World.TraitDict 承载)。模板成员定义
  //      在 world.hpp 尾部 —— 函数体需要 World 完整类型(actor.hpp 无法
  //      反向包含 world.hpp,故由统一引入点承载)
  //      The trait-query forwarding (L399-417; carried by World.TraitDict).
  //      The template-member definitions sit at the end of world.hpp —
  //      their bodies need the complete World type (actor.hpp cannot
  //      include world.hpp back, so the unified inclusion point carries
  //      them) ————
  template <class T>
  T* Trait();
  template <class T>
  T* TraitOrDefault();
  template <class T>
  std::vector<T*> TraitsImplementing();
  void AddTrait(TraitBase* trait);

  // ———— 生命周期(L213-270/419-444)————
  void Initialize(bool add_to_world = true);
  void Dispose();

  // ———— order 分发(L446-450)————
  void ResolveOrder(const net::Order& order);

  // ———— 换主(L453-485)————
  void ChangeOwner(Player* new_owner);
  void ChangeOwnerSync(Player* new_owner);

  // ———— 条件系统(L558-615)————
  int GrantCondition(std::string_view condition);
  int RevokeCondition(int token);
  bool TokenValid(int token) const;

  /// 条件计数缓存只读视图(readOnlyConditionCache)
  const std::map<std::string, int>& ConditionCache() const {
    return map_condition_cache_;
  }

  // ———— SyncHash 面(L111)————
  const std::vector<ActorSyncHashEntry>& SyncHashes() const {
    return vec_sync_hashes_;
  }
  std::vector<ActorSyncHashEntry>& MutableSyncHashes() {
    return vec_sync_hashes_;  // World::CreateTraitsForActor 收集用
  }

  // ———— 相等性(L375-388):ActorID 即身份 ————
  bool Equals(const Actor& other) const {
    return uint4_actor_id_ == other.uint4_actor_id_;
  }

 private:
  /// UpdateConditionState(L560-576)
  void UpdateConditionState(std::string_view condition, int token,
                            bool is_revoke);

  World& world_;
  std::uint32_t uint4_actor_id_;
  const game::ActorInfo* info_ = nullptr;
  std::string str_info_name_;  // Info.Name 副本(异常消息/DebugName)
  Player* p_owner_ = nullptr;
  bool b_is_in_world_ = false;
  bool b_will_dispose_ = false;
  bool b_disposed_ = false;
  Activity* current_activity_ = nullptr;
  int int4_generation_ = 0;
  Actor* p_replaced_by_ = nullptr;

  /// 条件系统数据面(L89-109)
  struct ConditionState {
    /// 已注册本条件变更通知的观察者(可重复 —— 每变量一次,上游 List 语义)
    /// Observers registered for this condition's changes (duplicates
    /// allowed — one per variable, the upstream List semantics).
    std::vector<VariableObserverNotifier> vec_notifiers;
    /// 已授条件实例的唯一令牌集 / the unique token set of granted instances.
    std::set<int> set_tokens;
  };
  std::map<std::string, ConditionState> map_condition_states_;
  std::map<int, std::string> map_condition_tokens_;
  int next_condition_token_ = 1;
  std::map<std::string, int> map_condition_cache_;

  /// 构造期接口缓存(L71-127;渲染缓存组 Phase 4 接入,此处为仿真查询面)
  std::vector<ActorSyncHashEntry> vec_sync_hashes_;
  std::vector<IOccupySpace*> vec_occupy_space_;  // OccupiesSpace(单值语义:
                                                 // 上游最后写入者生效)
  IFacing* p_facing_ = nullptr;            // L85 facing(单值,后者覆写)
                                           // (the single value; later writes win)
  std::vector<ITargetable*> vec_targetables_;      // Targetables(构造序)
  std::vector<ICrushable*> vec_crushables_;        // crushables(构造序)
  std::vector<IVisibilityModifier*> vec_visibility_modifiers_;  // 构造序
  IDefaultVisibility* p_default_visibility_ = nullptr;  // 单值,后者覆写
  bool b_created_ = false;
};

}  // namespace ora::sim

// 模板查询的实现体依赖 trait_dictionary.hpp —— 由使用方翻译单元包含
// (sim/world.hpp 统一引入)。此处不循环包含。
// The template-query bodies depend on trait_dictionary.hpp — included by
// the consuming translation units (sim/world.hpp pulls it in). No
// circular include here.
#include "sim/trait_dictionary.hpp"
