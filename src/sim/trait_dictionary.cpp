// UPSTREAM: OpenRA.Game/TraitDictionary.cs @7d57605(实现部分:容器算术与注册)
//          The implementation half (container arithmetic + registration).
#include "sim/actor.hpp"
#include "sim/trait_dictionary.hpp"

namespace ora::sim {

std::size_t BinarySearchMany(std::span<Actor* const> span,
                             std::uint32_t search_for) {
  // L23-37
  std::size_t start = 0;
  std::size_t end = span.size();
  while (start != end) {
    const auto mid = (start + end) / 2;
    if (span[mid]->ActorID() < search_for)
      start = mid + 1;
    else
      end = mid;
  }
  return start;
}

// ———— TraitContainer(TraitDictionary.cs L145-327 的擦除载荷版)————
// ———— TraitContainer (the erased-payload edition of TraitDictionary.cs
//      L145-327) ————

void TraitContainer::Add(Actor* actor, void* trait) {
  // L152-158:按 ActorID+1 定位插入点(同 actor 追加尾部)
  const auto insert_index = BinarySearchMany(vec_actors_, actor->ActorID() + 1);
  vec_actors_.insert(vec_actors_.begin() + insert_index, actor);
  vec_traits_.insert(vec_traits_.begin() + insert_index, trait);
}

void* TraitContainer::Get(Actor* actor, std::string_view key_name) {
  // L160-167
  auto* result = GetOrDefault(actor, key_name);
  if (result == nullptr)
    throw std::runtime_error("Actor " + actor->InfoName() +
                             " does not have trait of type `" +
                             std::string(key_name) + "`");

  return result;
}

void* TraitContainer::GetOrDefault(Actor* actor, std::string_view key_name) {
  // L169-181
  const auto index = BinarySearchMany(vec_actors_, actor->ActorID());
  if (index >= vec_actors_.size() || vec_actors_[index] != actor)
    return nullptr;

  if (index + 1 < vec_actors_.size() && vec_actors_[index + 1] == actor)
    throw std::runtime_error("Actor " + actor->InfoName() +
                             " has multiple traits of type `" +
                             std::string(key_name) + "`");

  return vec_traits_[index];
}

std::vector<void*> TraitContainer::GetMultiple(std::uint32_t actor) {
  // L183-219(MultipleEnumerator 物化)
  std::vector<void*> out;
  auto index = BinarySearchMany(vec_actors_, actor);
  while (index < vec_actors_.size() &&
         vec_actors_[index]->ActorID() == actor) {
    out.push_back(vec_traits_[index]);
    ++index;
  }
  return out;
}

std::vector<Actor*> TraitContainer::Actors() {
  // L228-241
  std::vector<Actor*> out;
  Actor* last = nullptr;
  for (Actor* current : vec_actors_) {
    if (current == last)
      continue;
    out.push_back(current);
    last = current;
  }
  return out;
}

std::vector<Actor*> TraitContainer::Actors(
    const std::function<bool(void*)>& predicate) {
  // L243-257
  std::vector<Actor*> out;
  Actor* last = nullptr;
  for (std::size_t i = 0; i < vec_actors_.size(); ++i) {
    Actor* current = vec_actors_[i];
    if (current == last || !predicate(vec_traits_[i]))
      continue;
    out.push_back(current);
    last = current;
  }
  return out;
}

void TraitContainer::RemoveActor(std::uint32_t actor) {
  // L287-301:同 actor 区间整体移除
  const auto start_index = BinarySearchMany(vec_actors_, actor);
  if (start_index >= vec_actors_.size() ||
      vec_actors_[start_index]->ActorID() != actor)
    return;

  auto end_index = start_index + 1;
  while (end_index < vec_actors_.size() &&
         vec_actors_[end_index]->ActorID() == actor)
    ++end_index;

  vec_actors_.erase(vec_actors_.begin() + start_index,
                    vec_actors_.begin() + end_index);
  vec_traits_.erase(vec_traits_.begin() + start_index,
                    vec_traits_.begin() + end_index);
}

void TraitContainer::ApplyToAll(
    const std::function<void(Actor*, void*)>& action) {
  // L303-310
  for (std::size_t i = 0; i < vec_actors_.size(); ++i)
    action(vec_actors_[i], vec_traits_[i]);
}

// ———— TraitDictionary ————

void TraitDictionary::AddTrait(Actor* actor, TraitBase* val) {
  // L67-75:GetInterfaces()+BaseTypes() 注册面 = trait 静态 upcast 表;
  // 每键经 upcast 取对应子对象指针(等价 C# 容器内的运行时 cast)
  for (const auto& entry : val->TraitUpcasts()) {
    auto& container = InnerGet(entry.type_id);
    container.Add(actor, entry.upcast(val));
  }
}

void TraitDictionary::CheckDestroyed(Actor* actor) {
  // L82-86
  if (actor->Disposed())
    throw std::runtime_error("Attempted to get trait from destroyed object (" +
                             actor->DebugName() + ")");
}

void TraitDictionary::RemoveActor(Actor* a) {
  // L121-125
  for (auto& [id, container] : map_containers_)
    container->RemoveActor(a->ActorID());
}

}  // namespace ora::sim
