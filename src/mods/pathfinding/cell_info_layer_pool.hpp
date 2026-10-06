// UPSTREAM: OpenRA.Mods.Common/Pathfinder/CellInfoLayerPool.cs @b6fc03f
//          (逐语义重写 + OPT-A2)
//          Verbatim-semantics rewrite + OPT-A2.
//
// OPT-A2(docs/OPTIMIZATION_TRACKER.md):上游池层取出时 layer.Clear() 做
// O(地图格数) 清零(CellInfo 28B/格,256² 图 ≈1.8MB/次搜索)。C++ 侧以
// **世代标记**承载"默认 = Unvisited"语义:每层附 u32 epoch[],取出即
// ++curEpoch(写入路径盖戳),Get 的未达格 = 默认 CellInfo —— 免清零的
// 单项最大收益;语义面(读到的默认值)与上游逐字节一致
// OPT-A2 (the optimization tracker): upstream zeroes a pooled layer on
// checkout (an O(map-cells) Clear; 28B/cell ≈ 1.8MB per search on a
// 256² map). The C++ side carries the "default = Unvisited" semantics
// with **generation stamps**: each layer gains a u32 epoch[] — checkout
// is just ++curEpoch (the write path stamps), and Get on an unstamped
// cell yields the default CellInfo — the single biggest win of the
// no-clear family; the semantic face (the value read back) matches
// upstream byte for byte.
//
// 池形态:MaxPoolSize=4 + Get/Return 的栈语义照抄;ConditionalWeakTable
// <World,…> → 进程级 map 键(世界指针;GC 弱表 → 显式 Erase 面供世界
// 析构调用 —— COVERAGE 登记)
// The pool shape: MaxPoolSize=4 + the Get/Return stack semantics kept;
// the ConditionalWeakTable<World,…> → a process-level map keyed by the
// world pointer (the GC weak table → an explicit Erase face called from
// the world's teardown — registered in COVERAGE).
#pragma once
import std;

#include "map/cell_layer.hpp"
#include "map/map.hpp"
#include "mods/pathfinding/path_graph.hpp"

namespace ora::sim {
class World;
}

namespace ora::mods::pathfinding {

/// 世代标记的 CellInfo 层(OPT-A2;语义面 = CellLayer<CellInfo>)
/// The generation-stamped CellInfo layer (OPT-A2; the semantic face =
/// CellLayer<CellInfo>).
class StampedCellInfoLayer {
 public:
  explicit StampedCellInfoLayer(const map::Map& map_world)
      : size_{map_world.MapSize()},
        grid_type_{map_world.Grid().Type},
        vec_infos_{static_cast<std::size_t>(size_.Width) * size_.Height},
        vec_epochs_(static_cast<std::size_t>(size_.Width) * size_.Height, 0) {}

  /// 取值:未达格 = 默认 CellInfo(Unvisited)| the read: an unstamped cell
  /// reads the default CellInfo (Unvisited).
  CellInfo Get(CPos cell) const {
    const std::size_t idx = Index(cell);
    return vec_epochs_[idx] == current_epoch_ ? vec_infos_[idx] : CellInfo{};
  }

  /// 写值:盖当前世代戳 | the write: stamps with the current epoch.
  void Set(CPos cell, CellInfo info) {
    const std::size_t idx = Index(cell);
    vec_infos_[idx] = info;
    vec_epochs_[idx] = current_epoch_;
  }

  /// Clear 语义(OPT-A2):世代推进,O(1)| the Clear semantics (OPT-A2):
  /// the epoch advances, O(1).
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
  std::uint32_t current_epoch_ = 1;  // 0 = 未达域(默认值)| 0 = the
                                     // unstamped domain (the default).
};

/// CellInfoLayerPool(CellInfoLayerPool.cs L19-58) | CellInfoLayerPool
/// (CellInfoLayerPool.cs L19-58).
class CellInfoLayerPool final {
 public:
  static constexpr int kMaxPoolSize = 4;  // L20

  explicit CellInfoLayerPool(const map::Map& map_world) : map_{map_world} {}

  /// Get(L30-33):句柄对象(Dispose 归还 → RAII)| Get (L30-33): the
  /// handle (Dispose returns → RAII).
  class PooledCellInfoLayer final {
   public:
    explicit PooledCellInfoLayer(CellInfoLayerPool* layer_pool)
        : layer_pool_{layer_pool} {}

    PooledCellInfoLayer(const PooledCellInfoLayer&) = delete;
    PooledCellInfoLayer& operator=(const PooledCellInfoLayer&) = delete;
    PooledCellInfoLayer(PooledCellInfoLayer&&) = default;
    PooledCellInfoLayer& operator=(PooledCellInfoLayer&&) = default;

    /// GetLayer(L70-75):取层(世代推进)| GetLayer (L70-75): takes a
    /// layer (the epoch advances).
    StampedCellInfoLayer& GetLayer();

    /// Dispose(L77-85)/RAII:归还池 | Dispose (L77-85)/RAII: returns the
    /// layers to the pool.
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

  /// LayerPoolForWorld(PathSearch.cs L30-36):进程级表(见头注)
  /// LayerPoolForWorld (PathSearch.cs L30-36): the process-level table
  /// (see the header).
  static CellInfoLayerPool& LayerPoolForWorld(const sim::World* world,
                                              const map::Map* map_world);
  /// 世界析构面 | the world-teardown face.
  static void ErasePoolForWorld(const sim::World* world);

 private:
  friend class PooledCellInfoLayer;

  /// GetLayer(L35-51):栈借出;OPT-A2 世代推进替代 Clear
  /// GetLayer (L35-51): the stack loan; OPT-A2's epoch advance replaces
  /// the Clear.
  StampedCellInfoLayer* GetLayer() {
    StampedCellInfoLayer* layer = nullptr;
    if (!pool_.empty()) {
      layer = pool_.back();
      pool_.pop_back();
    }
    if (layer == nullptr)
      layer = new StampedCellInfoLayer{map_};
    else
      layer->InvalidateAll();  // 上游 layer.Clear() 的世代化形态
                               // the epoch form of upstream's layer.Clear().
    return layer;
  }

  /// ReturnLayer(L53-58) | ReturnLayer (L53-58).
  void ReturnLayer(StampedCellInfoLayer* layer) {
    if (static_cast<int>(pool_.size()) < kMaxPoolSize)
      pool_.push_back(layer);
    else
      delete layer;
  }

  const map::Map& map_;
  std::vector<StampedCellInfoLayer*> pool_;
};

}  // namespace ora::mods::pathfinding
