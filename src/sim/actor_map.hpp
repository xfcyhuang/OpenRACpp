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
class Map;

}

namespace ora::sim {

class Actor;
class CellTrigger;
class ProximityTrigger;
class World;

class ActorMap final : public TraitBase,
                       public IActorMap,
                       public ITick,
                       public INotifyCreated {
 public:
  ActorMap(World& world, int bin_size);
  ~ActorMap() override;

  ORA_TRAIT_INTERFACES(ActorMap, OpenRA_Mods_Common_Traits_ActorMap,
                       IActorMap, ITick, INotifyCreated)

  void Created(Actor& self) override;
  void Tick(Actor& self) override;

  std::vector<Actor*> GetActorsAt(CPos a) override;

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

  void AddInfluence(Actor* self, IOccupySpace* ios) override;
  void RemoveInfluence(Actor* self, IOccupySpace* ios) override;

  void UpdateOccupiedCells(IOccupySpace* ios) override;

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

  void AddPosition(Actor* a, IOccupySpace* ios) override;
  void RemovePosition(Actor* a, IOccupySpace* ios) override;
  void UpdatePosition(Actor* a, IOccupySpace* ios) override;

  std::vector<Actor*> ActorsInBox(const WPos& a, const WPos& b) override;

  WDist LargestActorRadius() const override { return wdist_largest_actor_radius_; }
  WDist LargestBlockingActorRadius() const override {
    return wdist_largest_blocking_actor_radius_;
  }

  void AddCellUpdatedListener(std::function<void(CPos)> fn) override {
    vec_cell_updated_.push_back(std::move(fn));
  }

  std::span<ICustomMovementLayer* const> CustomMovementLayers() const override {
    return vec_custom_movement_layers_;
  }

 private:

  struct InfluenceNode {
    InfluenceNode* next = nullptr;
    SubCell sub_cell = SubCell::FullCell;
    Actor* actor = nullptr;
  };
  using InfluenceLayer = ora::map::CellLayer<InfluenceNode*>;

  struct Bin {
    std::vector<Actor*> actors;
    std::vector<class ProximityTrigger*> proximity_triggers;
  };

  InfluenceLayer* LayerAt(std::uint8_t layer) const {
    return layer < vec_influence_.size() ? vec_influence_[layer].get()
                                         : nullptr;
  }

  int CellCoordToBinIndex(int cell) const { return cell / bin_size_; }
  int WorldCoordToBinIndex(int world) const {
    return CellCoordToBinIndex(world / 1024);
  }

  Rectangle BinRectangleCoveringWorldArea(int world_left, int world_top,
                                          int world_right,
                                          int world_bottom) const;

  Bin& BinAt(int bin_row, int bin_col) {
    return vec_bins_[static_cast<std::size_t>(bin_row) * cols_ + bin_col];
  }

  static InfluenceNode* RemoveInfluenceInner(InfluenceNode* head,
                                             Actor* to_remove);

  static bool AnyActorsAtInner(InfluenceNode* influence_node, CPos a,
                               SubCell sub, bool check_transient);
  static bool AnyActorsAtInner(
      InfluenceNode* influence_node, SubCell sub,
      const std::function<bool(Actor&)>& with_condition);

  std::vector<Bin*> BinsInBox(const WPos& a, const WPos& b);

  int bin_size_;
  map::Map& map_;
  World& world_;

  std::unordered_map<int, std::unique_ptr<class CellTrigger>> map_cell_triggers_;
  std::unordered_map<CPos, std::vector<CellTrigger*>> map_cell_trigger_influence_;
  std::unordered_map<int, std::unique_ptr<ProximityTrigger>>
      map_proximity_triggers_;
  int next_trigger_id_ = 0;

  std::vector<std::unique_ptr<InfluenceLayer>> vec_influence_;
  std::vector<ICustomMovementLayer*> vec_custom_movement_layers_{nullptr};

  std::vector<std::function<void(CPos)>> vec_cell_updated_;
  std::vector<Bin> vec_bins_;
  int rows_ = 0, cols_ = 0;

  std::vector<Actor*> vec_add_actor_position_;
  std::vector<Actor*> vec_remove_actor_position_;

  WDist wdist_largest_actor_radius_{0};
  WDist wdist_largest_blocking_actor_radius_{0};

  friend class ProximityTrigger;
};

}
