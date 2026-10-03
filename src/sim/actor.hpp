// UPSTREAM: OpenRA.Game/Actor.cs @7d57605 L27-651(逐语义重写;渲染/Lua 面
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
  bool b_created_ = false;
};

}  // namespace ora::sim

// 模板查询的实现体依赖 trait_dictionary.hpp —— 由使用方翻译单元包含
// (sim/world.hpp 统一引入)。此处不循环包含。
// The template-query bodies depend on trait_dictionary.hpp — included by
// the consuming translation units (sim/world.hpp pulls it in). No
// circular include here.
#include "sim/trait_dictionary.hpp"
