#pragma once
import std;

#include "sim/trait_interfaces.hpp"

namespace ora::sim {

class Actor;

template <class T>
struct TraitPair {
  Actor* actor = nullptr;
  T* trait = nullptr;
};

std::size_t BinarySearchMany(std::span<Actor* const> span,
                             std::uint32_t search_for);

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

class TraitDictionary {
 public:

  void AddTrait(Actor* actor, TraitBase* val);

  static void CheckDestroyed(Actor* actor);

  template <class T>
  T* Get(Actor* actor) {
    CheckDestroyed(actor);
    const auto name = FullNameOfTypeIdSafe(T::kTypeId);
    return static_cast<T*>(InnerGet(T::kTypeId).Get(actor, name));
  }

  template <class T>
  T* GetOrDefault(Actor* actor) {
    CheckDestroyed(actor);
    return static_cast<T*>(InnerGet(T::kTypeId).GetOrDefault(
        actor, FullNameOfTypeIdSafe(T::kTypeId)));
  }

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

  void RemoveActor(Actor* a);

  template <class T>
  void ApplyToActorsWithTrait(
      const std::function<void(Actor*, T*)>& action) {
    InnerGet(T::kTypeId).ApplyToAll(
        [&action](Actor* actor, void* p) { action(actor, static_cast<T*>(p)); });
  }

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

}
