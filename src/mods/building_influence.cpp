// UPSTREAM: OpenRA.Mods.Common/Traits/Buildings/BuildingInfluence.cs
//          (实现部分) | The implementation half.
#include "mods/building_influence.hpp"

#include "map/map.hpp"
#include "sim/world.hpp"

namespace ora::mods {

BuildingInfluence::BuildingInfluence(sim::World& world)
    : world_{world}, layer_influence_{world.Map()} {
  // L34-43
}

void BuildingInfluence::AddInfluence(sim::Actor* a,
                                     const std::vector<CPos>& vec_cells) {
  // L48-62
  for (const CPos& c : vec_cells) {
    const MPos uv = c.ToMPos(world_.Map().Grid().Type);
    if (layer_influence_.Contains(uv))
      layer_influence_.Set(uv,
                           vec_nodes_
                               .emplace_back(std::make_unique<InfluenceNode>(
                                   InfluenceNode{
                                       layer_influence_.Get(uv), a}))
                               .get());
  }
}

void BuildingInfluence::RemoveInfluence(
    sim::Actor* a, const std::vector<CPos>& vec_cells) {
  // L64-78
  for (const CPos& c : vec_cells) {
    const MPos uv = c.ToMPos(world_.Map().Grid().Type);
    if (!layer_influence_.Contains(uv))
      continue;

    // RemoveInfluenceInner 的递归链重建(上游同形;尾递归 → 循环)
    // RemoveInfluenceInner's recursive chain rebuild (upstream's shape;
    // tail recursion → the loop).
    InfluenceNode** link = &layer_influence_.GetRef(uv);
    while (*link != nullptr) {
      if ((*link)->actor == a)
        *link = (*link)->next;
      else
        link = &(*link)->next;
    }
  }
}

std::vector<sim::Actor*> BuildingInfluence::GetBuildingsAt(CPos cell) const {
  // L80-90
  std::vector<sim::Actor*> vec_out;
  const MPos uv = cell.ToMPos(world_.Map().Grid().Type);
  if (!layer_influence_.Contains(uv))
    return vec_out;

  const InfluenceNode* node = layer_influence_.Get(uv);
  while (node != nullptr) {
    vec_out.push_back(node->actor);
    node = node->next;
  }
  return vec_out;
}

bool BuildingInfluence::AnyBuildingAt(CPos cell) const {
  // L92-93
  return layer_influence_.Contains(cell) &&
         layer_influence_.Get(cell) != nullptr;
}

}  // namespace ora::mods
