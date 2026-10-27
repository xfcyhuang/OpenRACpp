import std;

#include "mods/pathfinding/cell_info_layer_pool.hpp"

namespace ora::mods::pathfinding {

StampedCellInfoLayer& CellInfoLayerPool::PooledCellInfoLayer::GetLayer() {
  StampedCellInfoLayer* layer = layer_pool_->GetLayer();
  vec_layers_.push_back(layer);
  return *layer;
}

void CellInfoLayerPool::PooledCellInfoLayer::Dispose() {

  if (layer_pool_ != nullptr) {
    for (StampedCellInfoLayer* layer : vec_layers_)
      layer_pool_->ReturnLayer(layer);
    vec_layers_.clear();
    layer_pool_ = nullptr;
  }
}

namespace {

std::unordered_map<const sim::World*, std::unique_ptr<CellInfoLayerPool>>&
LayerPoolTable() {
  static std::unordered_map<const sim::World*,
                            std::unique_ptr<CellInfoLayerPool>>
      table;
  return table;
}

}

CellInfoLayerPool& CellInfoLayerPool::LayerPoolForWorld(
    const sim::World* world, const map::Map* map_world) {
  auto& table = LayerPoolTable();
  auto it = table.find(world);
  if (it == table.end()) {

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

}
