// UPSTREAM: OpenRA.Mods.Common/Orders/OrderGenerator.cs @e180e3225
//          L18-61 全文 + GlobalButtonOrderGenerator.cs @e180e3225 L19-95
//          全文 + RepairOrderGenerator.cs @e180e3225 L20-87 全文 +
//          BeaconOrderGenerator.cs @e180e3225 L18-40 全文(实现部分)。
#include "mods/order_generators.hpp"

#include "mods/actor_exts.hpp"
#include "gfx/world_renderer.hpp"
#include "mods/conditions.hpp"
#include "mods/render_utils.hpp"
#include "mods/air_support.hpp"
#include "mods/repairable_building.hpp"
#include "mods/repairable_near.hpp"
#include "mods/sellable.hpp"
#include "sim/world.hpp"

namespace ora::mods {

ModsOrderGeneratorBase::ModsOrderGeneratorBase(sim::World& world) {
  if (game_settings_.mouse_control_style == MouseControlStyle::Classic)
    if (sim::ISelection* selection = world.Selection())
      selection->Clear();
}

MouseButton ModsOrderGeneratorBase::ActionButton() const {
  return game_settings_.ResolveActionButton(ActionType());
}

MouseButton ModsOrderGeneratorBase::CancelButton() const {
  return game_settings_.ResolveCancelButton(ActionType());
}

std::vector<std::unique_ptr<net::Order>> ModsOrderGeneratorBase::Order(
    sim::World& world, CPos cell, int2 world_pixel,
    const MouseInput& mi) {
  if (mi.Button == ActionButton() && mi.Event == MouseInputEvent::Down)
    return OrderInner(world, cell, world_pixel, mi);

  if (mi.Button == CancelButton() && mi.Event == MouseInputEvent::Up)
    world.CancelInputMode();

  return {};
}

template <class TraitT>
bool GlobalButtonOrderGenerator<TraitT>::IsValidTrait(
    const TraitT& trait_t) const {
  return trait_t.IsTraitEnabled();
}

template <class TraitT>
std::vector<std::unique_ptr<net::Order>>
GlobalButtonOrderGenerator<TraitT>::OrderInner(
    sim::World& world, CPos, int2, const MouseInput& mi) {
  for (const sim::ActorBoundsPair& pair :
       world.ScreenMapFace()->ActorsAtMouse(int2{mi.Location.X, mi.Location.Y})) {
    sim::Actor* actor = pair.ptr_actor;
    if (actor->Owner() != world.LocalPlayer())
      continue;

    bool b_valid = false;
    for (TraitT* trait_t : actor->TraitsImplementing<TraitT>())
      if (IsValidTrait(*trait_t)) {
        b_valid = true;
        break;
      }
    if (!b_valid)
      continue;

    std::vector<std::unique_ptr<net::Order>> vec_out;
    vec_out.push_back(std::make_unique<net::Order>(str_order_, actor, false));
    return vec_out;
  }

  return {};
}

template <class TraitT>
void GlobalButtonOrderGenerator<TraitT>::Tick(sim::World& world) {
  if (world.LocalPlayer() != nullptr &&
      world.LocalPlayer()->PlayerWinState() != sim::WinState::Undefined)
    world.CancelInputMode();
}

SellOrderGenerator::SellOrderGenerator(sim::World& world)
    : GlobalButtonOrderGenerator<Sellable>{world, "Sell"} {}

std::string SellOrderGenerator::GetCursor(sim::World& world, CPos cell,
                                          int2 world_pixel,
                                          const MouseInput& mi) {
  for (const auto& up_order :
       OrderInner(world, cell, world_pixel, mi)) {
    for (Sellable* sellable :
         up_order->p_subject->TraitsImplementing<Sellable>()) {
      if (sellable->IsTraitDisabled())
        continue;
      return sellable->Info().str_cursor;
    }
  }

  return "sell-blocked";
}

PowerDownOrderGenerator::PowerDownOrderGenerator(sim::World& world)
    : GlobalButtonOrderGenerator<ToggleConditionOnOrder>{world, "PowerDown"} {}

bool PowerDownOrderGenerator::IsValidTrait(
    const ToggleConditionOnOrder& trait_t) const {
  return !trait_t.IsTraitDisabled() && !trait_t.IsTraitPaused();
}

std::string PowerDownOrderGenerator::GetCursor(sim::World& world, CPos cell,
                                               int2 world_pixel,
                                               const MouseInput& mi) {
  return !OrderInner(world, cell, world_pixel, mi).empty()
             ? "powerdown"
             : "powerdown-blocked";
}

RepairOrderGenerator::RepairOrderGenerator(sim::World& world)
    : ModsOrderGeneratorBase{world} {}

void RepairOrderGenerator::Tick(sim::World& world) {
  if (world.LocalPlayer() != nullptr &&
      world.LocalPlayer()->PlayerWinState() != sim::WinState::Undefined)
    world.CancelInputMode();
}

std::vector<std::unique_ptr<net::Order>>
RepairOrderGenerator::OrderInner(sim::World& world,
                                 CPos, int2,
                                 const MouseInput& mi) {
  std::vector<std::unique_ptr<net::Order>> vec_orders;

  sim::Actor* under_cursor = nullptr;
  for (const sim::ActorBoundsPair& pair :
       world.ScreenMapFace()->ActorsAtMouse(int2{mi.Location.X, mi.Location.Y})) {
    sim::Actor* actor = pair.ptr_actor;
    if (AppearsFriendlyTo(*actor, *world.LocalPlayer()->PlayerActor()) &&
        !world.FogObscures(*actor)) {
      under_cursor = actor;
      break;
    }
  }

  if (under_cursor == nullptr)
    return vec_orders;

  if (render::GetDamageState(*under_cursor) == sim::DamageState::Undamaged)
    return vec_orders;

  if (under_cursor->Info()->HasTraitInfoOfInterface(
          "OpenRA.Mods.Common.Traits.RepairableBuildingInfo"))
    vec_orders.push_back(std::make_unique<net::Order>(
        "RepairBuilding", world.LocalPlayer()->PlayerActor(),
        sim::Target::FromActor(under_cursor), false));

  if (under_cursor->Owner() != world.LocalPlayer())
    return vec_orders;

  sim::Actor* repair_building = nullptr;
  std::string str_order_id{"Repair"};

  if (Repairable* repairable = under_cursor->TraitOrDefault<Repairable>()) {
    repair_building = repairable->FindRepairBuilding(*under_cursor);
  } else if (RepairableNear* repairable_near =
                 under_cursor->TraitOrDefault<RepairableNear>()) {
    str_order_id = "RepairNear";
    repair_building = repairable_near->FindRepairBuilding(*under_cursor);
  }

  if (repair_building == nullptr)
    return vec_orders;

  vec_orders.push_back(std::make_unique<net::Order>(
      str_order_id, under_cursor, sim::Target::FromActor(repair_building),
      sim::Target::FromActor(under_cursor),
      HasModifier(mi.ModifierFlags, Modifiers::Shift)));
  return vec_orders;
}

std::string RepairOrderGenerator::GetCursor(sim::World& world, CPos cell,
                                            int2 world_pixel,
                                            const MouseInput& mi) {
  return !OrderInner(world, cell, world_pixel, mi).empty()
             ? "repair"
             : "repair-blocked";
}

BeaconOrderGenerator::BeaconOrderGenerator(sim::World& world)
    : ModsOrderGeneratorBase{world} {}

std::string BeaconOrderGenerator::GetCursor(sim::World&, CPos, int2,
                                            const MouseInput&) {
  return "ability";
}

std::vector<std::unique_ptr<net::Order>>
BeaconOrderGenerator::OrderInner(sim::World& world,
                                 CPos cell, int2,
                                 const MouseInput&) {
  world.CancelInputMode();

  auto up_order = std::make_unique<net::Order>(
      "PlaceBeacon", world.LocalPlayer()->PlayerActor(),
      sim::Target::FromCell(world, cell), false);
  up_order->b_suppress_visual_feedback = true;
  std::vector<std::unique_ptr<net::Order>> vec_out;
  vec_out.push_back(std::move(up_order));
  return vec_out;
}

}  // namespace ora::mods
