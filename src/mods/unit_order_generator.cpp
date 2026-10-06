// UPSTREAM: OpenRA.Mods.Common/Orders/UnitOrderGenerator.cs @b6fc03f
//          (实现部分)
//          The implementation half of UnitOrderGenerator.cs.
import std;

#include "mods/unit_order_generator.hpp"

#include "game/actor_info.hpp"
#include "net/order.hpp"
#include "sim/actor.hpp"
#include "sim/screen_map.hpp"
#include "sim/selection.hpp"
#include "sim/world.hpp"

namespace ora::mods {

using sim::ActorBoundsPair;
using sim::ISelection;
using sim::TargetModifiers;

UnitOrderGenerator::UnitOrderGenerator(sim::World& world,
                                       const GameSettingsFace& game_settings)
    : game_settings_{game_settings} {
  // ChromeMetrics.Get<string>("WorldSelectCursor"/"WorldDefaultCursor")
  // (L23-24):Chrome 装配批接入前取缺省键值(部分覆盖装配面)
  // (L23-24): the default key values stand in before the Chrome assembly
  // batch (the partial-coverage assembly face).
  (void)world;
  // HasIssuedQueuedCommand = false(L34)| (L34).
  b_has_issued_queued_command_ = false;
}

// ———— TargetForInput(L37-54)————
sim::Target UnitOrderGenerator::TargetForInput(sim::World& world, CPos cell,
                                               int2 world_pixel,
                                               const MouseInput& mi) {
  sim::Actor* best = nullptr;
  for (const sim::ActorBoundsPair& a : world.ScreenMapFace()->ActorsAtMouse(
           int2{mi.Location.X, mi.Location.Y})) {
    // Where 谓词(L40)| the Where predicate (L40).
    if (a.ptr_actor->IsDead() || a.ptr_actor->Info() == nullptr ||
        !a.ptr_actor->Info()->HasTraitInfoOfInterface(
            "OpenRA.Traits.ITargetableInfo") ||
        world.FogObscures(*a.ptr_actor))
      continue;

    // WithHighestSelectionPriority(worldPixel, mi.Modifiers)(L41):
    // 选择优先级 trait 面随 Selection 批 —— 空优先级面下的首达形态
    // (头注)| (the header note).
    best = a.ptr_actor;
    break;
  }

  if (best != nullptr)
    return sim::Target::FromActor(best);

  // FrozenActorsAtMouse 分支(L46-51):FrozenActorLayer 随其批 —— 部分覆盖
  // 装配面下恒空(头注)| (the header note).

  return world.TargetFromCell(cell, sim::SubCell::Any);
}

// ———— Order(L56-64)————
std::vector<net::Order*> UnitOrderGenerator::Order(sim::World& world, CPos cell,
                                                   int2 world_pixel,
                                                   const MouseInput& mi) {
  if (mi.Button == game_settings_.ResolveActionButton(
                       MouseActionType::Contextual))  // ActionType(L27)
    return OrderInner(world, cell, world_pixel, mi);
  if (mi.Button == game_settings_.ResolveCancelButton(
                       MouseActionType::Contextual))
    world.CancelInputMode();

  return {};
}

// ———— OrderInner(L66-84)————
std::vector<net::Order*> UnitOrderGenerator::OrderInner(
    sim::World& world, CPos cell, int2 world_pixel, const MouseInput& mi) {
  const sim::Target target = TargetForInput(world, cell, world_pixel, mi);

  std::vector<UnitOrderResult> orders;
  if (ISelection* selection = world.Selection()) {
    for (Actor* a : selection->Actors())
      if (auto o = OrderForUnit(a, target, cell, mi))
        orders.push_back(std::move(*o));
  }

  // actorsInvolved = orders.Select(o => o.Actor).Distinct()(L74)
  std::vector<Actor*> actors_involved;
  for (const UnitOrderResult& o : orders) {
    const std::uint32_t id = o.actor->ActorID();
    if (!std::any_of(actors_involved.begin(), actors_involved.end(),
                     [&](Actor* x) { return x->ActorID() == id; }))
      actors_involved.push_back(o.actor);
  }

  if (actors_involved.empty())
    return {};  // yield break(L76)| (L76).

  std::vector<net::Order*> out;

  // HACK: This is required by the hacky player actions-per-minute
  // calculation(上游注释)| (upstream comment)
  out.push_back(new net::Order("CreateGroup",
                               actors_involved[0]->Owner()->PlayerActor(),
                               false, actors_involved));

  const bool queued = HasModifier(mi.Modifiers, Modifiers::Shift);
  for (UnitOrderResult& o : orders)
    out.push_back(CheckSameOrder(
        o.order,
        o.trait->IssueOrder(*o.actor, o.order, o.target, queued)));

  return out;
}

// ———— Tick/Render 族(L86-90/L117/L156)————
void UnitOrderGenerator::Tick(sim::World&) {}
std::vector<gfx::IRenderable*> UnitOrderGenerator::Render(sim::World&) {
  return {};
}
std::vector<gfx::IRenderable*> UnitOrderGenerator::RenderAboveShroud(sim::World&) {
  return {};
}
std::vector<gfx::IRenderable*> UnitOrderGenerator::RenderAnnotations(sim::World&) {
  return {};
}
void UnitOrderGenerator::Deactivate() {}
void UnitOrderGenerator::SelectionChanged(sim::World&,
                                          std::span<sim::Actor* const>) {}

// ———— GetCursor(L92-115)————
std::string UnitOrderGenerator::GetCursor(sim::World& world, CPos cell,
                                          int2 world_pixel) {
  // GetCursor(world, cell, worldPixel, mi) 的 UI 输入重载随输入装配批;
  // 无 mi 的接口面以默认 MouseInput 承载(部分覆盖装配面)
  // The mi-carrying overload lands with the input-assembly batch; the
  // mi-free interface face stands in with a default MouseInput (the
  // partial-coverage assembly face).
  const MouseInput mi{};
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
        // Where(o != null && o.Cursor != null)(L103)| (L103).
        if (!o.has_value() || o->cursor.empty())
          continue;
        // MaxByOrDefault(o => o.Order.OrderPriority)(L105)
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

// ———— InputOverridesSelection(L122-154)————
bool UnitOrderGenerator::InputOverridesSelection(sim::World& world, int2 xy,
                                                 const MouseInput& mi) {
  sim::Actor* actor = nullptr;
  for (const sim::ActorBoundsPair& a :
       world.ScreenMapFace()->ActorsAtMouse(xy)) {
    // Where(L124-129)| (L124-129).
    if (a.ptr_actor->IsDead() || a.ptr_actor->Info() == nullptr ||
        !a.ptr_actor->Info()->HasTraitInfoOfInterface(
            "OpenRA.Traits.ISelectableInfo"))
      continue;
    // (a.Actor.Owner.IsAlliedWith(world.RenderPlayer) ||
    //  !world.FogObscures(a.Actor))(L128)
    if (!(a.ptr_actor->Owner()->IsAlliedWith(world.RenderPlayer()) ||
          !world.FogObscures(*a.ptr_actor)))
      continue;
    actor = a.ptr_actor;
    break;  // WithHighestSelectionPriority 的首达形态(头注)
  }

  if (actor == nullptr)
    return true;

  const sim::Target target = sim::Target::FromActor(actor);
  const CPos cell = world.Map().CellContaining(target.CenterPosition());
  const std::vector<Actor*> actors_at = world.ActorMapFace()->GetActorsAt(cell);

  TargetModifiers modifiers = TargetModifiers::None;
  if (HasModifier(mi.Modifiers, Modifiers::Ctrl))
    modifiers = modifiers | TargetModifiers::ForceAttack;
  if (HasModifier(mi.Modifiers, Modifiers::Shift))
    modifiers = modifiers | TargetModifiers::ForceQueue;
  if (HasModifier(mi.Modifiers, Modifiers::Alt))
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

// ———— OrderForUnit(L163-202)————
std::optional<UnitOrderResult> UnitOrderGenerator::OrderForUnit(
    sim::Actor* self, const sim::Target& target_in, CPos xy,
    const MouseInput& mi) {
  if (self->Owner() != self->world().LocalPlayer())  // L165
    return std::nullopt;

  if (self->world().IsGameOver())  // L168
    return std::nullopt;

  sim::Target target = target_in;
  // IsValidFor(L171)的 C++ 面:Type() 的 Actor 失效退化
  if (self->Disposed() || target.Type() == sim::TargetType::Invalid)  // L171
    return std::nullopt;

  TargetModifiers modifiers = TargetModifiers::None;
  if (HasModifier(mi.Modifiers, Modifiers::Ctrl))
    modifiers = modifiers | TargetModifiers::ForceAttack;
  if (HasModifier(mi.Modifiers, Modifiers::Shift))
    modifiers = modifiers | TargetModifiers::ForceQueue;
  if (HasModifier(mi.Modifiers, Modifiers::Alt))
    modifiers = modifiers | TargetModifiers::ForceMove;

  // orders = TraitsImplementing<IIssueOrder>().SelectMany(Orders) 按
  // OrderPriority 降序(L182-185)| (L182-185).
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

    // No valid orders, so check for orders against the cell(上游注释)
    target = self->world().TargetFromCell(xy, sim::SubCell::Any);
  }

  return std::nullopt;
}

// ———— CheckSameOrder(L204-211)————
net::Order* UnitOrderGenerator::CheckSameOrder(sim::IOrderTargeter* iot,
                                               net::Order* order) {
  // TextNotificationsManager.Debug → 注入钩子面(通知管理器随 Phase 6;
  // 静默 = D29 系)| the injected-hook face (the notification manager lands
  // in Phase 6; silence = the D29 family).
  if (order == nullptr && !iot->OrderID().empty())
    ;  // Debug("BUG: in order targeter - decided on {0} but then didn't order", ...)
  else if (order != nullptr &&
           (!order->str_target_string.has_value() ||
            iot->OrderID() != *order->str_target_string))
    ;  // Debug("BUG: in order targeter - decided on {0} but ordered {1}", ...)
  return order;
}

}  // namespace ora::mods
