import std;

#include "mods/unit_order_generator.hpp"

#include "game/actor_info.hpp"
#include "net/order.hpp"
#include "sim/actor.hpp"
#include "sim/frozen_actor_layer.hpp"
#include "sim/screen_map.hpp"
#include "sim/selectable_exts.hpp"
#include "sim/selection.hpp"
#include "sim/world.hpp"

namespace ora::mods {

using sim::ActorBoundsPair;
using sim::ISelection;
using sim::TargetModifiers;

UnitOrderGenerator::UnitOrderGenerator(sim::World& world,
                                       const GameSettingsFace& game_settings)
    : game_settings_{game_settings} {

  (void)world;

  b_has_issued_queued_command_ = false;
}

sim::Target UnitOrderGenerator::TargetForInput(sim::World& world, CPos cell,
                                               int2 world_pixel,
                                               const MouseInput& mi) {
  std::vector<sim::ActorBoundsPair> eligible;
  for (const sim::ActorBoundsPair& a : world.ScreenMapFace()->ActorsAtMouse(
           int2{mi.Location.X, mi.Location.Y})) {
    if (a.ptr_actor->IsDead() || a.ptr_actor->Info() == nullptr ||
        !a.ptr_actor->Info()->HasTraitInfoOfInterface(
            "OpenRA.Traits.ITargetableInfo") ||
        world.FogObscures(*a.ptr_actor))
      continue;
    eligible.push_back(a);
  }

  sim::Actor* best =
      sim::WithHighestSelectionPriority(eligible, world_pixel,
                                         mi.ModifierFlags);

  if (best != nullptr)
    return sim::Target::FromActor(best);

  std::vector<sim::FrozenActor*> frozen_eligible;
  for (sim::FrozenActor* a : world.ScreenMapFace()->FrozenActorsAtMouse(
           world.RenderPlayer(), int2{mi.Location.X, mi.Location.Y})) {
    if (!a->Info().HasTraitInfoOfInterface("OpenRA.Traits.ITargetableInfo") ||
        !a->Visible() || !a->HasRenderables())
      continue;
    frozen_eligible.push_back(a);
  }

  sim::FrozenActor* frozen = sim::WithHighestSelectionPriority(
      frozen_eligible, world_pixel, mi.ModifierFlags);

  if (frozen != nullptr)
    return sim::Target::FromFrozenActor(frozen);

  return world.TargetFromCell(cell, sim::SubCell::Any);
}

MouseButton UnitOrderGenerator::ActionButton() const {
  return game_settings_.ResolveActionButton(ActionType());
}

MouseButton UnitOrderGenerator::CancelButton() const {
  return game_settings_.ResolveCancelButton(ActionType());
}

std::vector<std::unique_ptr<net::Order>> UnitOrderGenerator::Order(
    sim::World& world, CPos cell,
    int2 world_pixel,
    const MouseInput& mi) {
  if (mi.Button == game_settings_.ResolveActionButton(
                       MouseActionType::Contextual))
    return OrderInner(world, cell, world_pixel, mi);
  if (mi.Button == game_settings_.ResolveCancelButton(
                       MouseActionType::Contextual))
    world.CancelInputMode();

  return {};
}

std::vector<std::unique_ptr<net::Order>> UnitOrderGenerator::OrderInner(
    sim::World& world, CPos cell, int2 world_pixel, const MouseInput& mi) {
  const sim::Target target = TargetForInput(world, cell, world_pixel, mi);

  std::vector<UnitOrderResult> orders;
  if (ISelection* selection = world.Selection()) {
    for (Actor* a : selection->Actors())
      if (auto o = OrderForUnit(a, target, cell, mi))
        orders.push_back(std::move(*o));
  }

  std::vector<Actor*> actors_involved;
  for (const UnitOrderResult& o : orders) {
    const std::uint32_t id = o.actor->ActorID();
    if (!std::any_of(actors_involved.begin(), actors_involved.end(),
                     [&](Actor* x) { return x->ActorID() == id; }))
      actors_involved.push_back(o.actor);
  }

  if (actors_involved.empty())
    return {};

  std::vector<std::unique_ptr<net::Order>> out;

  out.push_back(std::make_unique<net::Order>(
      "CreateGroup", actors_involved[0]->Owner()->PlayerActor(),
      false, actors_involved));

  const bool queued = HasModifier(mi.ModifierFlags, Modifiers::Shift);
  for (UnitOrderResult& o : orders)
    out.push_back(CheckSameOrder(
        o.order,
        o.trait->IssueOrder(*o.actor, o.order, o.target, queued)));

  return out;
}

void UnitOrderGenerator::Tick(sim::World&) {}
std::vector<gfx::IRenderable*> UnitOrderGenerator::Render(gfx::WorldRenderer&,
                                                            sim::World&) {
  return {};
}
std::vector<gfx::IRenderable*> UnitOrderGenerator::RenderAboveShroud(
    gfx::WorldRenderer&, sim::World&) {
  return {};
}
std::vector<gfx::IRenderable*> UnitOrderGenerator::RenderAnnotations(
    gfx::WorldRenderer&, sim::World&) {
  return {};
}
void UnitOrderGenerator::Deactivate() {}
void UnitOrderGenerator::SelectionChanged(sim::World&,
                                          std::span<sim::Actor* const>) {}

std::string UnitOrderGenerator::GetCursor(sim::World& world, CPos cell,
                                          int2 world_pixel,
                                          const MouseInput& mi) {
  const sim::Target target = TargetForInput(world, cell, world_pixel, mi);

  bool use_select;
  if (game_settings_.mouse_control_style == MouseControlStyle::Classic &&
      !InputOverridesSelection(world, world_pixel, mi)) {
    use_select = target.Type() == sim::TargetType::Actor &&
                 target.ActorPtr != nullptr &&
                 const_cast<sim::Actor*>(target.ActorPtr)
                     ->Info()
                     ->HasTraitInfoOfInterface(
                         "OpenRA.Traits.ISelectableInfo");
  } else {
    std::optional<UnitOrderResult> cursor_order;
    int cursor_priority = std::numeric_limits<int>::min();
    if (ISelection* selection = world.Selection()) {
      for (Actor* a : selection->Actors()) {
        auto o = OrderForUnit(a, target, cell, mi);

        if (!o.has_value() || o->cursor.empty())
          continue;

        const int priority = o->order->OrderPriority();
        if (!cursor_order.has_value() || priority > cursor_priority) {
          cursor_order = std::move(o);
          cursor_priority = priority;
        }
      }
    }

    use_select = target.Type() == sim::TargetType::Actor &&
                 target.ActorPtr != nullptr &&
                 const_cast<sim::Actor*>(target.ActorPtr)
                     ->Info()
                     ->HasTraitInfoOfInterface(
                         "OpenRA.Traits.ISelectableInfo") &&
                 (!cursor_order.has_value() ||
                  world.Selection() == nullptr ||
                  world.Selection()->Actors().empty() ||
                  !InputOverridesSelection(world, world_pixel, mi));

    if (!use_select && cursor_order.has_value())
      return cursor_order->cursor;
  }

  return use_select ? world_select_cursor_ : world_default_cursor_;
}

bool UnitOrderGenerator::InputOverridesSelection(sim::World& world, int2 xy,
                                                 const MouseInput& mi) {
  std::vector<sim::ActorBoundsPair> eligible;
  for (const sim::ActorBoundsPair& a :
       world.ScreenMapFace()->ActorsAtMouse(xy)) {
    if (a.ptr_actor->IsDead() || a.ptr_actor->Info() == nullptr ||
        !a.ptr_actor->Info()->HasTraitInfoOfInterface(
            "OpenRA.Traits.ISelectableInfo"))
      continue;
    if (!(a.ptr_actor->Owner()->IsAlliedWith(world.RenderPlayer()) ||
          !world.FogObscures(*a.ptr_actor)))
      continue;
    eligible.push_back(a);
  }
  sim::Actor* actor =
      sim::WithHighestSelectionPriority(eligible, xy, mi.ModifierFlags);

  if (actor == nullptr)
    return true;

  const sim::Target target = sim::Target::FromActor(actor);
  const CPos cell = world.Map().CellContaining(target.CenterPosition());
  const std::vector<Actor*> actors_at = world.ActorMapFace()->GetActorsAt(cell);

  TargetModifiers modifiers = TargetModifiers::None;
  if (HasModifier(mi.ModifierFlags, Modifiers::Ctrl))
    modifiers = modifiers | TargetModifiers::ForceAttack;
  if (HasModifier(mi.ModifierFlags, Modifiers::Shift))
    modifiers = modifiers | TargetModifiers::ForceQueue;
  if (HasModifier(mi.ModifierFlags, Modifiers::Alt))
    modifiers = modifiers | TargetModifiers::ForceMove;

  if (ISelection* selection = world.Selection()) {
    for (Actor* a : selection->Actors()) {
      auto o = OrderForUnit(a, target, cell, mi);
      if (o.has_value() &&
          o->order->TargetOverridesSelection(*a, target, actors_at, cell,
                                             modifiers))
        return true;
    }
  }

  return false;
}

std::optional<UnitOrderResult> UnitOrderGenerator::OrderForUnit(
    sim::Actor* self, const sim::Target& target_in, CPos xy,
    const MouseInput& mi) {
  if (self->Owner() != self->world().LocalPlayer())
    return std::nullopt;

  if (self->world().IsGameOver())
    return std::nullopt;

  sim::Target target = target_in;

  if (self->Disposed() || target.Type() == sim::TargetType::Invalid)
    return std::nullopt;

  TargetModifiers modifiers = TargetModifiers::None;
  if (HasModifier(mi.ModifierFlags, Modifiers::Ctrl))
    modifiers = modifiers | TargetModifiers::ForceAttack;
  if (HasModifier(mi.ModifierFlags, Modifiers::Shift))
    modifiers = modifiers | TargetModifiers::ForceQueue;
  if (HasModifier(mi.ModifierFlags, Modifiers::Alt))
    modifiers = modifiers | TargetModifiers::ForceMove;

  struct TraitOrder {
    sim::IIssueOrder* trait;
    sim::IOrderTargeter* order;
  };
  std::vector<TraitOrder> orders;
  for (auto* trait : self->TraitsImplementing<sim::IIssueOrder>())
    for (auto* order : trait->Orders())
      orders.push_back(TraitOrder{trait, order});
  std::stable_sort(orders.begin(), orders.end(),
                   [](const TraitOrder& a, const TraitOrder& b) {
                     return a.order->OrderPriority() > b.order->OrderPriority();
                   });

  for (int i = 0; i < 2; i++) {
    for (const TraitOrder& o : orders) {
      TargetModifiers local_modifiers = modifiers;
      std::string cursor;
      if (o.order->CanTarget(*self, target, local_modifiers, cursor))
        return UnitOrderResult{self, o.order, o.trait, cursor, target};
    }

    target = self->world().TargetFromCell(xy, sim::SubCell::Any);
  }

  return std::nullopt;
}

std::unique_ptr<net::Order> UnitOrderGenerator::CheckSameOrder(
    sim::IOrderTargeter* iot, std::unique_ptr<net::Order> order) {

  if (order == nullptr && !iot->OrderID().empty())
    ;
  else if (order != nullptr &&
           (!order->str_target_string.has_value() ||
            iot->OrderID() != *order->str_target_string))
    ;
  return order;
}

}
