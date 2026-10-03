// UPSTREAM: OpenRA.Game/TraitDictionary.cs @7d57605 L21-329(逐语义重写)
//          Verbatim-semantics rewrite.
//
// 机制对照 / Mechanism mapping:
//  - Dictionary<Type, ITraitContainer> → std::map<gen::TypeId,
//    std::unique_ptr<TraitContainer>>(查询热路径;容器按需懒建 —— GetOrAdd)
//  - TraitContainer<T> 平行数组(List<Actor>/List<T> 按 ActorID 有序)+
//    BinarySearchMany(actorID+1 插入 = 同 actor 追加尾部;查询起点二分)→
//    单一擦除容器(Actor* + void* 平行数组;载荷为键类型子对象地址,由
//    AddTrait 的 upcast 表保证 —— 与 C# object 载荷同构,模板查询处
//    static_cast 还原)
//    The TraitContainer<T> parallel arrays (List<Actor>/List<T> kept sorted
//    by ActorID) + BinarySearchMany (an actorID+1 insertion appends after
//    same-actor entries; queries bisect the start) → a single erased
//    container (parallel Actor* + void* arrays; payloads are key-type
//    sub-object addresses guaranteed by AddTrait's upcast table — the same
//    shape as the C# object payload, restored via static_cast at the
//    template queries).
//  - AddTrait 的 GetInterfaces()+BaseTypes() 注册 → 手写 trait 的
//    ORA_TRAIT_INTERFACES 上行转换表(静态;upcast lambda 承载多继承偏移)
//  - yield 枚举器 → vector 物化(上游枚举器持有 List 引用,遍历中变更会
//    反映;调用方约定 = 查询后立即消费,C++ 物化快照语义等价)
//    yield enumerators → materialized vectors (upstream enumerators hold
//    List references where mid-iteration mutations show through; callers
//    consume immediately by convention, so the C++ snapshot is equivalent).
#pragma once
import std;

#include "sim/trait_interfaces.hpp"

namespace ora::sim {

class Actor;

/// TraitPair<T>(World.cs L635-649)
template <class T>
struct TraitPair {
  Actor* actor = nullptr;
  T* trait = nullptr;
};

/// SpanExts.BinarySearchMany(TraitDictionary.cs L23-37):首个
/// ActorID >= searchFor 的下标
/// SpanExts.BinarySearchMany (TraitDictionary.cs L23-37): the index of the
/// first entry with ActorID >= searchFor.
std::size_t BinarySearchMany(std::span<Actor* const> span,
                             std::uint32_t search_for);

/// 擦除载荷容器(TraitContainer<T> 的 object 载荷同构;见头注)
/// The erased-payload container (the object-payload counterpart of
/// TraitContainer<T>; see the header note).
class TraitContainer final {
 public:
  void Add(Actor* actor, void* trait);
  void RemoveActor(std::uint32_t actor);

  void* Get(Actor* actor, std::string_view key_name);
  void* GetOrDefault(Actor* actor, std::string_view key_name);
  std::vector<void*> GetMultiple(std::uint32_t actor);

  std::vector<Actor*> Actors();
  std::vector<Actor*> Actors(const std::function<bool(void*)>& predicate);

  void ApplyToAll(const std::function<void(Actor*, void*)>& action);

  std::span<Actor* const> actors_span() const { return vec_actors_; }
  std::span<void* const> traits_span() const { return vec_traits_; }

 private:
  std::vector<Actor*> vec_actors_;
  std::vector<void*> vec_traits_;
};

/// TraitDictionary(L43-335)
class TraitDictionary {
 public:
  /// AddTrait(L67-75):按注册表(接口+基类+自身)逐键登记
  /// AddTrait (L67-75): registers under every key of the upcast table
  /// (interfaces + bases + self).
  void AddTrait(Actor* actor, TraitBase* val);

  /// CheckDestroyed(L82-86)
  static void CheckDestroyed(Actor* actor);

  /// Get<T>(L88-92):缺失/多实例异常逐字(TraitContainer.Get L160-167)
  /// Get<T> (L88-92): missing/multiple exceptions verbatim
  /// (TraitContainer.Get L160-167).
  template <class T>
  T* Get(Actor* actor) {
    CheckDestroyed(actor);
    const auto name = FullNameOfTypeIdSafe(T::kTypeId);
    return static_cast<T*>(InnerGet(T::kTypeId).Get(actor, name));
  }

  /// GetOrDefault<T>(L94-98)
  template <class T>
  T* GetOrDefault(Actor* actor) {
    CheckDestroyed(actor);
    return static_cast<T*>(InnerGet(T::kTypeId).GetOrDefault(
        actor, FullNameOfTypeIdSafe(T::kTypeId)));
  }

  /// WithInterface<T>(L100-104;同 actor 全部 T*,注册序)
  /// WithInterface<T> (L100-104; every T* of the actor, registration
  /// order).
  template <class T>
  std::vector<T*> WithInterface(Actor* actor) {
    CheckDestroyed(actor);
    auto payloads = InnerGet(T::kTypeId).GetMultiple(actor->ActorID());
    std::vector<T*> out;
    out.reserve(payloads.size());
    for (void* p : payloads)
      out.push_back(static_cast<T*>(p));
    return out;
  }

  /// ActorsWithTrait<T>(L106-109)
  template <class T>
  std::vector<TraitPair<T>> ActorsWithTrait() {
    auto& container = InnerGet(T::kTypeId);
    std::vector<TraitPair<T>> out;
    out.reserve(container.actors_span().size());
    for (std::size_t i = 0; i < container.actors_span().size(); ++i)
      out.push_back(TraitPair<T>{
          container.actors_span()[i],
          static_cast<T*>(container.traits_span()[i])});
    return out;
  }

  /// ActorsHavingTrait<T>(L111-119;去重 actor 序)
  /// ActorsHavingTrait<T> (L111-119; the deduplicated actor sequence).
  template <class T>
  std::vector<Actor*> ActorsHavingTrait() {
    return InnerGet(T::kTypeId).Actors();
  }

  template <class T>
  std::vector<Actor*> ActorsHavingTrait(
      const std::function<bool(T*)>& predicate) {
    return InnerGet(T::kTypeId).Actors(
        [&predicate](void* p) { return predicate(static_cast<T*>(p)); });
  }

  /// RemoveActor(L121-125)
  void RemoveActor(Actor* a);

  /// ApplyToActorsWithTrait<T>(L127-130)
  template <class T>
  void ApplyToActorsWithTrait(
      const std::function<void(Actor*, T*)>& action) {
    InnerGet(T::kTypeId).ApplyToAll(
        [&action](Actor* actor, void* p) { action(actor, static_cast<T*>(p)); });
  }

  /// ApplyToActorsWithTraitTimed(L132-135;计时面省略 —— PerfTickLogger 属
  /// 支持/诊断层,Phase 5 起随 Log 通道落地;分发语义与 ApplyToAll 一致)
  /// ApplyToActorsWithTraitTimed (L132-135; timing elided — PerfTickLogger
  /// belongs to the support/diagnostics layer landing with the Log channels
  /// from Phase 5; dispatch semantics identical to ApplyToAll).
  template <class T>
  void ApplyToActorsWithTraitTimed(
      const std::function<void(Actor*, T*)>& action,
      [[maybe_unused]] std::string_view text) {
    ApplyToActorsWithTrait<T>(action);
  }

 private:
  static std::string_view FullNameOfTypeIdSafe(gen::TypeId id) {
    const auto v = static_cast<std::size_t>(id);
    if (v < gen::kTypeIdCount)
      return gen::kTypeFullNames[v];
    return "(unknown)";
  }

  TraitContainer& InnerGet(gen::TypeId id) {
    auto it = map_containers_.find(id);
    if (it == map_containers_.end())
      it = map_containers_.emplace(id, std::make_unique<TraitContainer>())
               .first;
    return *it->second;
  }

  std::map<gen::TypeId, std::unique_ptr<TraitContainer>> map_containers_;
};

}  // namespace ora::sim
