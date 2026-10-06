// UPSTREAM: OpenRA.Mods.Common/Traits/World/ActorMap.cs @b6fc03f L21-688
//          (逐语义重写)
//          Verbatim-semantics rewrite.
//
// 机制对照 / Mechanism mapping:
//  - InfluenceNode 链表节点(上游 GC 托管):C++ 侧 heap 分配、摘链即删
//    (RemoveInfluenceInner 的 ref 重写不释放节点 —— GC 语义的显式等价:
//    被摘节点 delete,链序保持)
//    The InfluenceNode linked-list nodes (upstream GC-managed): heap
//    allocation here, unlink = delete (upstream's ref-rewrite abandons the
//    nodes to the GC — the explicit equivalent: unlinked nodes are deleted,
//    list order preserved).
//  - HashSet<Actor>(addActorPosition/removeActorPosition/触发器的 old/current
//    集)→ 插入序 vector + ActorID 判重(D 系惯例;上游 HashSet 实现序在
//    无删除时即插入序 —— tick 消费序一致)
//    The HashSet<Actor> (add/remove position + trigger old/current sets) →
//    insertion-ordered vectors + ActorID dedup (the D-series convention;
//    upstream's HashSet order is insertion order absent removals — the
//    tick consumption order matches).
//  - LargestActorRadius(L207-209):各 actor 的 HitShapeInfo.Type.OuterRadius
//    最大值 —— 值袋的嵌套形状记录(CircleShape/RectangleShape/CapsuleShape)
//    按 OuterRadius 公式求值(Circle=Radius;Rectangle=corners 最大长度;
//    Capsule=Radius+max(|A|,|B|));无 IBlocksProjectilesInfo actor 时的
//    LargestBlockingActorRadius = WDist.Zero 分支照抄
//    LargestActorRadius (L207-209): the max of every actor's
//    HitShapeInfo.Type.OuterRadius — evaluated from the bag's nested shape
//    records per the OuterRadius formulas; the no-blockers
//    LargestBlockingActorRadius = WDist.Zero branch kept.
//  - CellUpdated 事件 → construct 序回调表(与 Map/CellLayer 事件同型)
//    The CellUpdated event → callbacks in registration order (the same
//    shape as the Map/CellLayer events).
//  - GetCustomMovementLayers 扩展方法(L674-688)→ World::CustomMovementLayers()
//    The GetCustomMovementLayers extension (L674-688) →
//    World::CustomMovementLayers().
#pragma once
import std;

#include "core/wdist.hpp"
#include "map/cell_layer.hpp"
#include "map/map.hpp"
#include "sim/spatially_partitioned.hpp"
#include "sim/trait_interfaces.hpp"

namespace ora::meta {
class RecordObject;
}

namespace ora::map {
class Map;  // 完整类型由使用方(world.hpp 链)引入 | completed by the users
            // (the world.hpp chain).
}  // namespace ora::map

namespace ora::sim {

class Actor;
class CellTrigger;
class ProximityTrigger;
class World;

/// ActorMapInfo 的运行时承载(BinSize 字段;工厂面在 trait_registry)
/// The runtime carrier of ActorMapInfo (the BinSize field; the factory
/// face lives in trait_registry).
class ActorMap final : public TraitBase,
                       public IActorMap,
                       public ITick,
                       public INotifyCreated {
 public:
  ActorMap(World& world, int bin_size);
  ~ActorMap() override;

  ORA_TRAIT_INTERFACES(ActorMap, OpenRA_Mods_Common_Traits_ActorMap,
                       IActorMap, ITick, INotifyCreated)

  /// INotifyCreated.Created(L212-227):自定义移动层装配
  /// INotifyCreated.Created (L212-227): the custom-movement-layer assembly.
  void Created(Actor& self) override;
  void Tick(Actor& self) override;  // ITick.Tick(L473-509)

  // ———— 查询族(L265-399)————
  std::vector<Actor*> GetActorsAt(CPos a) override;
  /// 零分配枚举核(链表直走;GetActorsAt 的热路径承载 —— OPT-A8 同型的
  /// 回调面)| the zero-allocation enumeration core (a straight list walk;
  /// the hot-path carrier of GetActorsAt — the OPT-A8-style callback face).
  template <class F>
  void ForEachActorAt(CPos a, F&& fn) {
    const MPos uv = a.ToMPos(map_.Grid().Type);
    auto* layer = LayerAt(a.Layer());
    if (layer == nullptr || !layer->Contains(uv))
      return;
    for (InfluenceNode* i = layer->Get(uv); i != nullptr; i = i->next)
      fn(*i->actor);
  }
  std::vector<Actor*> GetActorsAt(CPos a, SubCell sub) override;
  bool HasFreeSubCell(CPos cell, bool check_transient = true) override;
  SubCell FreeSubCell(CPos cell, SubCell preferred_sub_cell = SubCell::Any,
                      bool check_transient = true) override;
  SubCell FreeSubCell(CPos cell, SubCell preferred_sub_cell,
                      const std::function<bool(Actor&)>& check_if_blocker)
      override;
  bool AnyActorsAt(CPos a) override;
  bool AnyActorsAt(CPos a, SubCell sub, bool check_transient = true) override;
  bool AnyActorsAt(CPos a, SubCell sub,
                   const std::function<bool(Actor&)>& with_condition) override;
  std::vector<Actor*> AllActors() override;

  // ———— 影响登记(L413-462)————
  void AddInfluence(Actor* self, IOccupySpace* ios) override;
  void RemoveInfluence(Actor* self, IOccupySpace* ios) override;

  /// UpdateOccupiedCells(L464-471) | UpdateOccupiedCells (L464-471).
  void UpdateOccupiedCells(IOccupySpace* ios) override;

  // ———— 触发器族(L511-586)————
  int AddCellTrigger(const std::vector<CPos>& cells,
                     std::function<void(Actor&)> on_entry,
                     std::function<void(Actor&)> on_exit) override;
  std::vector<CPos> TriggerPositions() override;
  void RemoveCellTrigger(int id) override;
  int AddProximityTrigger(const WPos& pos, const WDist& range,
                          const WDist& v_range,
                          std::function<void(Actor&)> on_entry,
                          std::function<void(Actor&)> on_exit) override;
  void RemoveProximityTrigger(int id) override;
  void UpdateProximityTrigger(int id, const WPos& new_pos,
                              const WDist& new_range,
                              const WDist& new_v_range) override;

  // ———— 位置缓存面(L588-602)————
  void AddPosition(Actor* a, IOccupySpace* ios) override;
  void RemovePosition(Actor* a, IOccupySpace* ios) override;
  void UpdatePosition(Actor* a, IOccupySpace* ios) override;

  // ———— 盒查询(L644-671)————
  std::vector<Actor*> ActorsInBox(const WPos& a, const WPos& b) override;

  WDist LargestActorRadius() const override { return wdist_largest_actor_radius_; }
  WDist LargestBlockingActorRadius() const override {
    return wdist_largest_blocking_actor_radius_;
  }

  void AddCellUpdatedListener(std::function<void(CPos)> fn) override {
    vec_cell_updated_.push_back(std::move(fn));
  }

  /// CustomMovementLayers 数组(L177;索引 0 恒 null —— 地面层)
  /// The CustomMovementLayers array (L177; index 0 stays null — the ground
  /// layer).
  std::span<ICustomMovementLayer* const> CustomMovementLayers() const override {
    return vec_custom_movement_layers_;
  }

 private:
  /// InfluenceNode(L32-37) | InfluenceNode (L32-37).
  struct InfluenceNode {
    InfluenceNode* next = nullptr;
    SubCell sub_cell = SubCell::FullCell;
    Actor* actor = nullptr;
  };
  using InfluenceLayer = ora::map::CellLayer<InfluenceNode*>;

  /// Bin(L39-43) | Bin (L39-43).
  struct Bin {
    std::vector<Actor*> actors;
    std::vector<class ProximityTrigger*> proximity_triggers;
  };

  InfluenceLayer* LayerAt(std::uint8_t layer) const {
    return layer < vec_influence_.size() ? vec_influence_[layer].get()
                                         : nullptr;
  }

  /// CellCoordToBinIndex/WorldCoordToBinIndex(L604-612)
  /// CellCoordToBinIndex / WorldCoordToBinIndex (L604-612).
  int CellCoordToBinIndex(int cell) const { return cell / bin_size_; }
  int WorldCoordToBinIndex(int world) const {
    return CellCoordToBinIndex(world / 1024);
  }

  /// BinRectangleCoveringWorldArea(L614-621) | (L614-621).
  Rectangle BinRectangleCoveringWorldArea(int world_left, int world_top,
                                          int world_right,
                                          int world_bottom) const;

  Bin& BinAt(int bin_row, int bin_col) {
    return vec_bins_[static_cast<std::size_t>(bin_row) * cols_ + bin_col];
  }

  /// RemoveInfluenceInner(L453-462):递归 ref 重写 → 迭代重建 + 摘除节点
  /// delete(GC 的显式等价;链序保持)
  /// RemoveInfluenceInner (L453-462): the recursive ref rewrite → an
  /// iterative rebuild with the removed nodes deleted (the explicit GC
  /// equivalent; list order preserved).
  static InfluenceNode* RemoveInfluenceInner(InfluenceNode* head,
                                             Actor* to_remove);

  /// AnyActorsAt 的静态内层(L348-365/L379-388) | the static AnyActorsAt
  /// inners (L348-365/L379-388).
  static bool AnyActorsAtInner(InfluenceNode* influence_node, CPos a,
                               SubCell sub, bool check_transient);
  static bool AnyActorsAtInner(
      InfluenceNode* influence_node, SubCell sub,
      const std::function<bool(Actor&)>& with_condition);

  /// BinsInBox(L628-642) | BinsInBox (L628-642).
  std::vector<Bin*> BinsInBox(const WPos& a, const WPos& b);

  int bin_size_;
  map::Map& map_;  // ctor 抓取的 world.Map(L194;World 完整类型经 world.hpp)
  World& world_;

  std::unordered_map<int, std::unique_ptr<class CellTrigger>> map_cell_triggers_;
  std::unordered_map<CPos, std::vector<CellTrigger*>> map_cell_trigger_influence_;
  std::unordered_map<int, std::unique_ptr<ProximityTrigger>>
      map_proximity_triggers_;
  int next_trigger_id_ = 0;

  /// influence 层数组(L174;0 层恒有,其余随 Created 装配)
  /// The influence-layer array (L174; layer 0 always present, the rest
  /// assembled by Created).
  std::vector<std::unique_ptr<InfluenceLayer>> vec_influence_;
  std::vector<ICustomMovementLayer*> vec_custom_movement_layers_{nullptr};  // L177

  std::vector<std::function<void(CPos)>> vec_cell_updated_;  // L178 事件
  std::vector<Bin> vec_bins_;                                // L179
  int rows_ = 0, cols_ = 0;                                  // L180

  // Position updates are done in one pass(上游注释)| (upstream comment).
  std::vector<Actor*> vec_add_actor_position_;       // L184(HashSet 承载)
  std::vector<Actor*> vec_remove_actor_position_;    // L185

  WDist wdist_largest_actor_radius_{0};              // L188
  WDist wdist_largest_blocking_actor_radius_{0};     // L189

  friend class ProximityTrigger;  // Tick 闭包面(ActorsInBox 消费)
};

}  // namespace ora::sim
