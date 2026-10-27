import std;

#include "ui/selection_utils.hpp"

#include "game/actor_info.hpp"
#include "sim/actor.hpp"
#include "sim/screen_map.hpp"
#include "sim/selectable_exts.hpp"
#include "sim/selection.hpp"
#include "sim/trait_interfaces.hpp"
#include "sim/world.hpp"
#include "ui/ui.hpp"
#include "gfx/world_renderer.hpp"

namespace ora::ui {
namespace {

bool OwnerInPlayers(sim::Actor& a, std::span<sim::Player* const> vec_owners) {
  for (const sim::Player* p : vec_owners)
    if (p == a.Owner())
      return true;
  return false;
}

bool ClassInSelectionClasses(const std::string& str_class,
                             const std::vector<std::string>* vec_selection_classes) {
  if (vec_selection_classes == nullptr)
    return true;
  for (const std::string& c : *vec_selection_classes)
    if (c == str_class)
      return true;
  return false;
}

}

std::vector<sim::Actor*> SelectActorsByOwnerAndSelectionClass(
    std::span<sim::Actor* const> vec_actors, std::span<sim::Player* const> vec_owners,
    const std::vector<std::string>* vec_selection_classes) {
  std::vector<sim::Actor*> result;
  for (sim::Actor* a : vec_actors) {
    if (!OwnerInPlayers(*a, vec_owners))
      continue;

    sim::ISelectable* s = a->TraitOrDefault<sim::ISelectable>();
    if (s != nullptr && ClassInSelectionClasses(std::string(s->Class()),
                                                vec_selection_classes))
      result.push_back(a);
  }
  return result;
}

std::vector<sim::Actor*> SelectActorsOnScreen(
    sim::World& world, gfx::WorldRenderer& wr,
    const std::vector<std::string>* vec_selection_classes,
    std::span<sim::Player* const> vec_players) {
  gfx::IViewportSurface* viewport = wr.Viewport();
  std::vector<sim::Actor*> actors;
  if (viewport != nullptr)
    for (const sim::ActorBoundsPair& pair :
         world.ScreenMapFace()->ActorsInMouseBox(viewport->TopLeft(),
                                                 viewport->BottomRight()))
      actors.push_back(pair.ptr_actor);
  return SelectActorsByOwnerAndSelectionClass(actors, vec_players,
                                              vec_selection_classes);
}

std::vector<sim::Actor*> SelectActorsInWorld(
    sim::World& world, const std::vector<std::string>* vec_selection_classes,
    std::span<sim::Player* const> vec_players) {
  std::vector<sim::Actor*> actors;
  for (sim::Actor* a : world.Actors())
    if (a->IsInWorld())
      actors.push_back(a);
  return SelectActorsByOwnerAndSelectionClass(actors, vec_players,
                                              vec_selection_classes);
}

std::vector<sim::Actor*> SelectHighestPriorityActorAtPoint(sim::World& world, int2 a,
                                                           Modifiers modifiers) {
  std::vector<sim::ActorBoundsPair> at_mouse =
      world.ScreenMapFace()->ActorsAtMouse(a);

  std::vector<sim::ActorBoundsPair> eligible;
  for (const sim::ActorBoundsPair& x : at_mouse) {
    if (x.ptr_actor->Info() == nullptr ||
        !x.ptr_actor->Info()->HasTraitInfoOfInterface("OpenRA.Traits.ISelectableInfo"))
      continue;
    if (!(x.ptr_actor->Owner()->IsAlliedWith(world.RenderPlayer()) ||
          !world.FogObscures(*x.ptr_actor)))
      continue;
    eligible.push_back(x);
  }

  sim::Actor* selected =
      WithHighestSelectionPriority(eligible, a, modifiers);
  if (selected == nullptr)
    return {};

  return {selected};
}

std::vector<sim::Actor*> SelectActorsInBoxWithDeadzone(sim::World& world, int2 a, int2 b,
                                                       Modifiers modifiers) {
  const std::int32_t deadzone = Ui::Instance().SelectionDeadzone();
  if ((a - b).Length() <= deadzone)
    a = b;

  if (a == b)
    return SelectHighestPriorityActorAtPoint(world, a, modifiers);

  std::vector<sim::Actor*> actors;
  for (const sim::ActorBoundsPair& x :
       world.ScreenMapFace()->ActorsInMouseBox(a, b)) {
    if (x.ptr_actor->Info() == nullptr ||
        !x.ptr_actor->Info()->HasTraitInfoOfInterface("OpenRA.Traits.ISelectableInfo"))
      continue;
    if (!(x.ptr_actor->Owner()->IsAlliedWith(world.RenderPlayer()) ||
          !world.FogObscures(*x.ptr_actor)))
      continue;
    actors.push_back(x.ptr_actor);
  }
  return SubsetWithHighestSelectionPriority(actors, modifiers);
}

std::vector<sim::Player*> GetPlayersToIncludeInSelection(sim::World& world) {
  const sim::Player* viewer =
      world.RenderPlayer() != nullptr ? world.RenderPlayer() : world.LocalPlayer();
  const bool is_shroud_disabled =
      viewer == nullptr || (world.RenderPlayer() == nullptr &&
                            world.LocalPlayer()->Spectating());
  const bool is_everyone =
      viewer != nullptr && viewer->NonCombatant() && viewer->Spectating();

  if (is_shroud_disabled || is_everyone)
    return world.Players();
  return {const_cast<sim::Player*>(viewer)};
}

}
