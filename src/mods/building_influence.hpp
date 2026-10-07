// UPSTREAM: OpenRA.Mods.Common/Traits/Buildings/BuildingInfluence.cs @b6fc03f
//          L20-92 全文(世界 actor 的建筑影响层链表)
//          The whole of L20-92 (the world actor's building-influence
//          linked layer).
#pragma once
import std;

#include "core/cell_pos.hpp"
#include "map/cell_layer.hpp"
#include "sim/actor.hpp"
#include "sim/trait_interfaces.hpp"

namespace ora::mods {

/// BuildingInfluence(L28-92;影响节点链表 = 上游 InfluenceNode 形态的
/// slab 化:节点 = (Actor* next, Actor* actor) 对,arena 无回收 → 删除
/// 以链重建承载)
/// BuildingInfluence (L28-92; the influence node chain = the slabbed
/// form of upstream's InfluenceNode: nodes are (Actor* next, Actor*
/// actor) pairs; with no arena reclamation removal rides the chain
/// rebuild).
class BuildingInfluence final : public sim::TraitBase {
 public:
  // 上游 BuildingInfluence 无接口(具体类直查)—— 空接口注册面(宏的
  // __VA_ARGS__ 至少一形,故手展开;self 键 = MakeTraitUpcasts 的空包形态)
  // Upstream's BuildingInfluence has no interfaces (queried by the
  // concrete class) — the empty-interface registration face (the macro
  // needs at least one variadic form, hence the manual expansion; the
  // self key = MakeTraitUpcasts's empty-pack form).
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_BuildingInfluence;
  gen::TypeId GetTraitTypeId() const override { return kTypeId; }
  static constexpr auto kTraitUpcasts =
      ora::sim::MakeTraitUpcasts<BuildingInfluence>();
  std::span<const ora::sim::TraitUpcastEntry> TraitUpcasts() const override {
    return kTraitUpcasts;
  }

  explicit BuildingInfluence(sim::World& world);

  /// L48-62:AddInfluence(头插)
  /// L48-62: AddInfluence (head insertion).
  void AddInfluence(sim::Actor* a, const std::vector<CPos>& vec_cells);

  /// L64-78:RemoveInfluence(递归链重建 = 上游 RemoveInfluenceInner)
  /// L64-78: RemoveInfluence (the recursive chain rebuild = upstream's
  /// RemoveInfluenceInner).
  void RemoveInfluence(sim::Actor* a, const std::vector<CPos>& vec_cells);

  /// L80-90:GetBuildingsAt
  /// L80-90: GetBuildingsAt.
  std::vector<sim::Actor*> GetBuildingsAt(CPos cell) const;

  /// L92-93:AnyBuildingAt
  /// L92-93: AnyBuildingAt.
  bool AnyBuildingAt(CPos cell) const;

 private:
  struct InfluenceNode {
    InfluenceNode* next = nullptr;
    sim::Actor* actor = nullptr;
  };

  sim::World& world_;
  map::CellLayer<InfluenceNode*> layer_influence_;
  std::vector<std::unique_ptr<InfluenceNode>> vec_nodes_;  // 节点所有权
};

}  // namespace ora::mods
