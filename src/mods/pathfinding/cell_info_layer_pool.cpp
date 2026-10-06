// UPSTREAM: OpenRA.Mods.Common/Pathfinder/CellInfoLayerPool.cs @b6fc03f
//          (实现部分 + PathSearch.cs 的 LayerPoolForWorld 表)
//          The implementation half of CellInfoLayerPool.cs + PathSearch.cs's
//          LayerPoolForWorld table.
import std;

#include "mods/pathfinding/cell_info_layer_pool.hpp"

namespace ora::mods::pathfinding {

StampedCellInfoLayer& CellInfoLayerPool::PooledCellInfoLayer::GetLayer() {
  StampedCellInfoLayer* layer = layer_pool_->GetLayer();
  vec_layers_.push_back(layer);
  return *layer;
}

void CellInfoLayerPool::PooledCellInfoLayer::Dispose() {
  // L77-85:layerPool 空判 = 移动后的句柄(重复归还防护)
  // L77-85: the layerPool null check = a moved-out handle (the double-
  // return guard).
  if (layer_pool_ != nullptr) {
    for (StampedCellInfoLayer* layer : vec_layers_)
      layer_pool_->ReturnLayer(layer);
    vec_layers_.clear();
    layer_pool_ = nullptr;
  }
}

namespace {

/// ConditionalWeakTable<World, CellInfoLayerPool>(PathSearch.cs L30-31)
/// 的进程级承载 | the process-level carrier of the ConditionalWeakTable.
std::unordered_map<const sim::World*, std::unique_ptr<CellInfoLayerPool>>&
LayerPoolTable() {
  static std::unordered_map<const sim::World*,
                            std::unique_ptr<CellInfoLayerPool>>
      table;
  return table;
}

}  // namespace

CellInfoLayerPool& CellInfoLayerPool::LayerPoolForWorld(
    const sim::World* world, const map::Map* map_world) {
  auto& table = LayerPoolTable();
  auto it = table.find(world);
  if (it == table.end()) {
    // CreateValueCallback:world => new CellInfoLayerPool(world.Map)
    // (PathSearch.cs L31)
    it = table
             .emplace(world,
                      std::unique_ptr<CellInfoLayerPool>(
                          new CellInfoLayerPool{*map_world}))
             .first;
  }
  return *it->second;
}

void CellInfoLayerPool::ErasePoolForWorld(const sim::World* world) {
  LayerPoolTable().erase(world);
}

}  // namespace ora::mods::pathfinding
