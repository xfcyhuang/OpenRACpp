// UPSTREAM: OpenRA.Mods.Common/Traits/World/ResourceLayer.cs @b6fc03f
//          L20-302 全文 + World/ResourceClaimLayer.cs L17-74 全文(逐语义
//          重写;CellChanged 事件 → 回调表)
//          The whole of World/ResourceLayer.cs L20-302 + the whole of
//          World/ResourceClaimLayer.cs L17-74 (verbatim-semantics
//          rewrites; the CellChanged event → a callback list).
//
// 机制对照 / Mechanism mapping:
//  - FrozenDictionary<string, ResourceTypeInfo> → 插入序 vector<pair> +
//    线性查找(yaml 声明序;ResourceTypesByIndex 的构建序 = 声明序,键冲突
//    = 上游 ToDictionary 抛的等价抛)
//    FrozenDictionary<string, ResourceTypeInfo> → an insertion-ordered
//    vector<pair> + linear lookup (the yaml declaration order;
//    ResourceTypesByIndex builds in declaration order, a key clash throws
//    as upstream's ToDictionary does).
//  - AllowedTerrainTypes 的 FrozenSet<string> → vector<string>(去重集合
//    语义;Contains 线性)
//    AllowedTerrainTypes' FrozenSet<string> → vector<string> (the
//    dedup-set semantics; a linear Contains).
//  - CellChanged 事件(Action<CPos, string>)→ 回调表(construct 序;本批
//    无订阅者 —— ResourceRenderer 随渲染批)
//    The CellChanged event (Action<CPos, string>) → a callback list (in
//    registration order; no subscribers this batch — ResourceRenderer
//    rides the render batch).
//  - claimByCell/claimByActor 字典 → 两个 map(键序稳定;List 清理序 =
//    RemoveAll 的首见移除语义)
//    The claimByCell/claimByActor dictionaries → two maps (stable key
//    order; the List cleanup keeps RemoveAll's remove-all-matching
//    semantics).
#pragma once
import std;

#include "map/cell_layer.hpp"
#include "map/map.hpp"
#include "mods/building_influence.hpp"
#include "sim/trait_interfaces.hpp"

namespace ora::meta {
class RecordObject;
}

namespace ora::mods {

using sim::Actor;

/// ResourceLayerInfo.ResourceTypeInfo(L32-53)的解析面
/// The parsed face of ResourceLayerInfo.ResourceTypeInfo (L32-53).
struct ResourceTypeInfoData {
  std::uint8_t uint1_resource_index = 0;  // L36([FieldLoader.Require])
  std::string str_terrain_type;           // L40([FieldLoader.Require])
  std::vector<std::string> vec_allowed_terrain_types;  // L44
  std::uint8_t uint1_max_density = 10;    // L47

  static ResourceTypeInfoData Parse(const meta::RecordObject& rec_info);
};

/// ResourceLayerInfo(L29-98)的解析面
/// The parsed face of ResourceLayerInfo (L29-98).
class ResourceLayerInfoData : public sim::IResourceLayerInfo {
 public:
  /// 声明序资源类型表(yaml 键序)
  /// The declaration-ordered resource-type table (the yaml key order).
  std::vector<std::pair<std::string, ResourceTypeInfoData>>
      vec_resource_types;
  bool b_recalculate_resource_density = false;  // L59

  static ResourceLayerInfoData Parse(const meta::RecordObject& rec_info);

  /// 资源类型查找(线性;未找到 = nullptr)
  /// The resource-type lookup (linear; nullptr when absent).
  const ResourceTypeInfoData* FindType(
      const std::string& resource_type) const {
    for (const auto& [name, info] : vec_resource_types)
      if (name == resource_type)
        return &info;
    return nullptr;
  }

  // ———— IResourceLayerInfo(L73-96)————
  // ———— IResourceLayerInfo (L73-96) ————
  bool TryGetTerrainType(const std::string& resource_type,
                         std::string& terrain_type_out) override;
  bool TryGetResourceIndex(const std::string& resource_type,
                           std::uint8_t& index_out) override;
};

/// ResourceLayer(L100-301)
class ResourceLayer final : public sim::TraitBase,
                            public sim::IResourceLayer,
                            public sim::IWorldLoaded {
 public:
  ResourceLayer(Actor& self, const ResourceLayerInfoData& info);

  ORA_TRAIT_INTERFACES(ResourceLayer,
                       OpenRA_Mods_Common_Traits_ResourceLayer,
                       sim::IResourceLayer, sim::IWorldLoaded)

  /// CellChanged 事件面(L111;订阅序通知)
  /// The CellChanged event face (L111; notified in subscription order).
  void AddCellChangedListener(
      std::function<void(CPos, const std::string&)> fn_listener) {
    vec_cell_changed_.push_back(std::move(fn_listener));
  }

  // ———— IWorldLoaded(L125-166)————
  // ———— IWorldLoaded (L125-166) ————
  void WorldLoaded(sim::World& w, gfx::WorldRenderer* wr) override;

  // ———— IResourceLayer(L285-300)————
  // ———— IResourceLayer (L285-300) ————
  sim::ResourceLayerContents GetResource(CPos cell) override;
  std::uint8_t GetMaxDensity(
      const std::string& resource_type) override;
  bool CanAddResource(const std::string& resource_type, CPos cell,
                      std::uint8_t amount = 1) override;
  int AddResource(const std::string& resource_type, CPos cell,
                  std::uint8_t amount = 1) override;
  int RemoveResource(const std::string& resource_type, CPos cell,
                     std::uint8_t amount = 1) override;
  void ClearResources(CPos cell) override;
  bool IsVisible(CPos cell) override;
  bool IsEmpty() override { return int4_res_cells_ < 1; }
  const sim::IResourceLayerInfo& ResourceLayerInfo() const override {
    return info_;
  }

  const ResourceLayerInfoData& Info() const { return info_; }

 protected:
  /// L168-181:AllowResourceAt(virtual)
  /// L168-181: AllowResourceAt (virtual).
  virtual bool AllowResourceAt(const std::string& resource_type, CPos cell);

 private:
  /// L183-195:CreateResourceCell | L183-195: CreateResourceCell.
  sim::ResourceLayerContents CreateResourceCell(
      const std::string& resource_type, CPos cell, int density);

  const ResourceLayerInfoData info_;
  sim::World& world_;
  map::Map& map_;
  BuildingInfluence& building_influence_;
  std::unique_ptr<map::CellLayer<sim::ResourceLayerContents>>
      ptr_content_;
  std::map<std::uint8_t, std::string> map_resource_types_by_index_;

  int int4_res_cells_ = 0;  // L109

  std::vector<std::function<void(CPos, const std::string&)>>
      vec_cell_changed_;  // L111
};

/// ResourceClaimLayer(ResourceClaimLayer.cs L22-73;world actor)
/// ResourceClaimLayer (ResourceClaimLayer.cs L22-73; the world actor).
class ResourceClaimLayer final : public sim::TraitBase {
 public:
  static constexpr gen::TypeId kTypeId =
      gen::TypeId::OpenRA_Mods_Common_Traits_ResourceClaimLayer;
  gen::TypeId GetTraitTypeId() const override { return kTypeId; }
  static constexpr auto kTraitUpcasts =
      ora::sim::MakeTraitUpcasts<ResourceClaimLayer>();
  std::span<const ora::sim::TraitUpcastEntry> TraitUpcasts() const override {
    return kTraitUpcasts;
  }

  /// L30-51:TryClaimCell(预定资源格)
  /// L30-51: TryClaimCell (reserving a resource cell).
  bool TryClaimCell(Actor& claimer, CPos cell);

  /// L53-60:CanClaimCell(盟友占用判定)
  /// L53-60: CanClaimCell (the allied-occupancy predicate).
  bool CanClaimCell(Actor& claimer, CPos cell) const;

  /// L62-72:RemoveClaim | L62-72: RemoveClaim.
  void RemoveClaim(Actor& claimer);

 private:
  std::map<CPos, std::vector<Actor*>> map_claim_by_cell_;
  std::map<Actor*, CPos> map_claim_by_actor_;
};

}  // namespace ora::mods
