#pragma once
import std;

#include "map/cell_layer.hpp"
#include "map/map.hpp"
#include "mods/pathfinding/path_graph.hpp"

namespace ora::sim {
class World;
}

namespace ora::mods::pathfinding {

class StampedCellInfoLayer {
 public:
  explicit StampedCellInfoLayer(const map::Map& map_world)
      : size_{map_world.MapSize()},
        grid_type_{map_world.Grid().Type},
        vec_infos_{static_cast<std::size_t>(size_.Width) * size_.Height},
        vec_epochs_(static_cast<std::size_t>(size_.Width) * size_.Height, 0) {}

  CellInfo Get(CPos cell) const {
    const std::size_t idx = Index(cell);
    return vec_epochs_[idx] == current_epoch_ ? vec_infos_[idx] : CellInfo{};
  }

  void Set(CPos cell, CellInfo info) {
    const std::size_t idx = Index(cell);
    vec_infos_[idx] = info;
    vec_epochs_[idx] = current_epoch_;
  }

  void InvalidateAll() { ++current_epoch_; }

 private:
  std::size_t Index(CPos cell) const {
    const MPos uv = cell.ToMPos(grid_type_);
    if (uv.U < 0 || uv.V < 0 || uv.U >= size_.Width || uv.V >= size_.Height)
      throw std::out_of_range("Index was outside the bounds of the array.");
    return static_cast<std::size_t>(uv.V) * size_.Width + uv.U;
  }

  Size size_;
  MapGridType grid_type_;
  std::vector<CellInfo> vec_infos_;
  std::vector<std::uint32_t> vec_epochs_;
  std::uint32_t current_epoch_ = 1;

};

class CellInfoLayerPool final {
 public:
  static constexpr int kMaxPoolSize = 4;

  explicit CellInfoLayerPool(const map::Map& map_world) : map_{map_world} {}

  class PooledCellInfoLayer final {
   public:
    explicit PooledCellInfoLayer(CellInfoLayerPool* layer_pool)
        : layer_pool_{layer_pool} {}

    PooledCellInfoLayer(const PooledCellInfoLayer&) = delete;
    PooledCellInfoLayer& operator=(const PooledCellInfoLayer&) = delete;
    PooledCellInfoLayer(PooledCellInfoLayer&&) = default;
    PooledCellInfoLayer& operator=(PooledCellInfoLayer&&) = default;

    StampedCellInfoLayer& GetLayer();

    void Dispose();
    ~PooledCellInfoLayer() { Dispose(); }

   private:
    CellInfoLayerPool* layer_pool_ = nullptr;
    std::vector<StampedCellInfoLayer*> vec_layers_;
    friend class CellInfoLayerPool;
  };

  PooledCellInfoLayer Get() {
    return PooledCellInfoLayer{this};
  }

  static CellInfoLayerPool& LayerPoolForWorld(const sim::World* world,
                                              const map::Map* map_world);

  static void ErasePoolForWorld(const sim::World* world);

 private:
  friend class PooledCellInfoLayer;

  StampedCellInfoLayer* GetLayer() {
    StampedCellInfoLayer* layer = nullptr;
    if (!pool_.empty()) {
      layer = pool_.back();
      pool_.pop_back();
    }
    if (layer == nullptr)
      layer = new StampedCellInfoLayer{map_};
    else
      layer->InvalidateAll();

    return layer;
  }

  void ReturnLayer(StampedCellInfoLayer* layer) {
    if (static_cast<int>(pool_.size()) < kMaxPoolSize)
      pool_.push_back(layer);
    else
      delete layer;
  }

  const map::Map& map_;
  std::vector<StampedCellInfoLayer*> pool_;
};

}
