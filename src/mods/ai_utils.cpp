// UPSTREAM: OpenRA.Mods.Common/AIUtils.cs @1f594e320(实现部分)
//          The implementation half.
#include "mods/ai_utils.hpp"

#include "mods/actor_exts.hpp"
#include "mods/mobile.hpp"
#include "net/order.hpp"
#include "sim/actor.hpp"
#include "sim/actor_map.hpp"
#include "sim/player.hpp"
#include "sim/target.hpp"
#include "sim/trait_interfaces.hpp"
#include "sim/world.hpp"

namespace ora::mods {
namespace {

/// Util.cs L192-195:ExpandFootprint(cells, allowDiagonal)
/// Util.cs L192-195: ExpandFootprint (cells, allowDiagonal).
void AppendNeighbours(std::vector<CPos>& vec_out, CPos cell,
                      bool b_allow_diagonal) {
  const int d = b_allow_diagonal ? 1 : 0;
  for (int dy = -d; dy <= d; dy++)
    for (int dx = -d; dx <= d; dx++) {
      if (dx == 0 && dy == 0)
        continue;
      vec_out.push_back(CPos{cell.X() + dx, cell.Y() + dy});
    }
}

}  // namespace

namespace util {

std::vector<CPos> ExpandFootprintCells(std::span<const CPos> vec_cells,
                                       bool b_allow_diagonal) {
  std::vector<CPos> vec_expanded;
  for (const CPos cell : vec_cells)
    AppendNeighbours(vec_expanded, cell, b_allow_diagonal);

  std::vector<CPos> vec_distinct;
  for (const CPos cell : vec_expanded)
    if (std::find(vec_distinct.begin(), vec_distinct.end(), cell) ==
        vec_distinct.end())
      vec_distinct.push_back(cell);
  return vec_distinct;
}

}  // namespace util

std::vector<std::unique_ptr<net::Order>> ClearBlockersOrders(
    std::span<const CPos> vec_tiles,
    sim::Player& owner,
    sim::Actor* ignore_actor) {
  sim::World& world = owner.GetWorld();
  std::vector<CPos> vec_adjacent;
  for (const CPos cell : util::ExpandFootprintCells(vec_tiles, true))
    if (std::find(vec_tiles.begin(), vec_tiles.end(), cell) ==
            vec_tiles.end() &&
        world.Map().Contains(cell))
      vec_adjacent.push_back(cell);

  std::vector<sim::Actor*> vec_blocker_actors;
  for (const CPos tile : vec_tiles)
    for (sim::Actor* a : world.ActorMapFace()->GetActorsAt(tile))
      if (a->Owner() == &owner && a->IsIdle() &&
          (ignore_actor == nullptr || a != ignore_actor))
        if (std::find(vec_blocker_actors.begin(), vec_blocker_actors.end(),
                      a) == vec_blocker_actors.end())
          vec_blocker_actors.push_back(a);

  std::vector<std::unique_ptr<net::Order>> vec_out;
  for (sim::Actor* blocker : vec_blocker_actors) {
    Mobile* mobile = blocker->TraitOrDefault<Mobile>();

    CPos move_cell{};
    if (mobile != nullptr) {
      std::vector<CPos> vec_available;
      for (const CPos t : vec_adjacent)
        if (mobile->CanEnterCell(t, nullptr, sim::BlockedByActor::All))
          vec_available.push_back(t);
      if (vec_available.empty())
        continue;

      move_cell = ClosestCell(*blocker, vec_available);
    } else {
      continue;
    }

    auto up_order = std::make_unique<net::Order>(
        "Move", blocker, sim::Target::FromCell(world, move_cell), false);
    up_order->b_suppress_visual_feedback = true;
    vec_out.push_back(std::move(up_order));
  }

  return vec_out;
}

}  // namespace ora::mods
