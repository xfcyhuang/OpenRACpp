import std;

#include "sim/selectable_exts.hpp"

#include "game/actor_info.hpp"
#include "sim/actor.hpp"
#include "sim/frozen_actor_layer.hpp"
#include "sim/player.hpp"
#include "sim/screen_map.hpp"
#include "sim/trait_registry.hpp"
#include "sim/world.hpp"

namespace ora::sim {
namespace {

constexpr int kPriorityRange = 30;

int BaseSelectionPriority(std::int64_t int8_priority,
                          std::int64_t int8_priority_modifiers,
                          Modifiers modifiers) {
  std::int64_t priority = int8_priority;
  const auto ctrl = static_cast<std::int64_t>(SelectionPriorityModifiers::Ctrl);
  const auto alt = static_cast<std::int64_t>(SelectionPriorityModifiers::Alt);

  if (HasModifier(modifiers, Modifiers::Ctrl) &&
      !HasModifier(modifiers, Modifiers::Alt) && (int8_priority_modifiers & ctrl) != 0)
    priority = std::numeric_limits<int>::max();

  if (HasModifier(modifiers, Modifiers::Alt) &&
      !HasModifier(modifiers, Modifiers::Ctrl) && (int8_priority_modifiers & alt) != 0)
    priority = std::numeric_limits<int>::max();

  return static_cast<int>(priority);
}

std::int64_t CalculateActorSelectionPriority(const game::ActorInfo& info,
                                             const Polygon& bounds,
                                             int2 selection_pixel,
                                             Modifiers modifiers) {
  if (bounds.IsEmpty())
    return SelectionPriority(info, modifiers);

  const Rectangle& br = bounds.BoundingRect;
  const int2 center_pixel{br.Left() + br.Width / 2, br.Top() + br.Height / 2};

  const std::int64_t pixel_distance = (center_pixel - selection_pixel).Length();
  return static_cast<std::int64_t>(SelectionPriority(info, modifiers)) -
         (pixel_distance << 16);
}

}  // namespace

int SelectionPriority(const game::ActorInfo& info, Modifiers modifiers) {
  const meta::RecordObject* rec =
      FindTraitInfoOfInterface(info, "OpenRA.Traits.ISelectableInfo");
  if (rec == nullptr)
    return std::numeric_limits<int>::min();

  const std::int64_t priority = RecordFieldInt(*rec, "Priority").value_or(10);
  const std::int64_t priority_modifiers =
      RecordFieldInt(*rec, "PriorityModifiers").value_or(0);
  return BaseSelectionPriority(priority, priority_modifiers, modifiers);
}

int SelectionPriority(const Actor& actor, Modifiers modifiers) {
  const game::ActorInfo* info = actor.Info();
  if (info == nullptr)
    return std::numeric_limits<int>::min();

  const meta::RecordObject* rec =
      FindTraitInfoOfInterface(*info, "OpenRA.Traits.ISelectableInfo");
  if (rec == nullptr)
    return std::numeric_limits<int>::min();

  const int base_priority =
      BaseSelectionPriority(RecordFieldInt(*rec, "Priority").value_or(10),
                            RecordFieldInt(*rec, "PriorityModifiers").value_or(0),
                            modifiers);

  const World& world = actor.world();
  const Player* viewer =
      (world.LocalPlayer() == nullptr || world.LocalPlayer()->Spectating())
          ? world.RenderPlayer()
          : world.LocalPlayer();

  if (actor.Owner() == viewer || viewer == nullptr)
    return base_priority;

  switch (viewer->RelationshipWith(actor.Owner())) {
    case PlayerRelationship::Ally:
      return base_priority - kPriorityRange;
    case PlayerRelationship::Neutral:
      return base_priority - 2 * kPriorityRange;
    case PlayerRelationship::Enemy:
      return base_priority - 3 * kPriorityRange;
  }
  return base_priority;
}

Actor* WithHighestSelectionPriority(std::span<const ActorBoundsPair> actors,
                                    int2 selection_pixel, Modifiers modifiers) {
  const ActorBoundsPair* best = nullptr;
  std::int64_t best_key = std::numeric_limits<std::int64_t>::min();
  for (const ActorBoundsPair& a : actors) {
    const game::ActorInfo* info =
        a.ptr_actor != nullptr ? a.ptr_actor->Info() : nullptr;
    if (info == nullptr)
      continue;
    const std::int64_t key =
        CalculateActorSelectionPriority(*info, a.bounds, selection_pixel, modifiers);
    if (best == nullptr || key > best_key) {
      best = &a;
      best_key = key;
    }
  }
  return best != nullptr ? best->ptr_actor : nullptr;
}

FrozenActor* WithHighestSelectionPriority(
    std::span<FrozenActor* const> actors, int2 selection_pixel,
    Modifiers modifiers) {
  FrozenActor* best = nullptr;
  std::int64_t best_key = std::numeric_limits<std::int64_t>::min();
  for (FrozenActor* a : actors) {
    if (a == nullptr)
      continue;
    const std::int64_t key = CalculateActorSelectionPriority(
        a->Info(), a->MouseBounds, selection_pixel, modifiers);
    if (best == nullptr || key > best_key) {
      best = a;
      best_key = key;
    }
  }
  return best;
}

std::vector<Actor*> SubsetWithHighestSelectionPriority(
    std::span<Actor* const> actors, Modifiers modifiers) {
  std::vector<Actor*> result;
  if (actors.empty())
    return result;

  std::vector<int> keys;
  keys.reserve(actors.size());
  for (Actor* a : actors)
    keys.push_back(SelectionPriority(*a, modifiers));

  int best_key = keys[0];
  for (int key : keys)
    best_key = std::max(best_key, key);

  for (std::size_t i = 0; i < actors.size(); i++)
    if (keys[i] == best_key)
      result.push_back(actors[i]);
  return result;
}

}  // namespace ora::sim
