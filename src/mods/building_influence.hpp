#pragma once
import std;

#include "core/cell_pos.hpp"
#include "map/cell_layer.hpp"
#include "sim/actor.hpp"
#include "sim/trait_interfaces.hpp"

namespace ora::mods {

class BuildingInfluence final : public sim::TraitBase {
 public:

  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_BuildingInfluence;
  gen::TypeId GetTraitTypeId() const override { return kTypeId; }
  static constexpr auto kTraitUpcasts =
      ora::sim::MakeTraitUpcasts<BuildingInfluence>();
  std::span<const ora::sim::TraitUpcastEntry> TraitUpcasts() const override {
    return kTraitUpcasts;
  }

  explicit BuildingInfluence(sim::World& world);

  void AddInfluence(sim::Actor* a, const std::vector<CPos>& vec_cells);

  void RemoveInfluence(sim::Actor* a, const std::vector<CPos>& vec_cells);

  std::vector<sim::Actor*> GetBuildingsAt(CPos cell) const;

  bool AnyBuildingAt(CPos cell) const;

 private:
  struct InfluenceNode {
    InfluenceNode* next = nullptr;
    sim::Actor* actor = nullptr;
  };

  sim::World& world_;
  map::CellLayer<InfluenceNode*> layer_influence_;
  std::vector<std::unique_ptr<InfluenceNode>> vec_nodes_;
};

}
