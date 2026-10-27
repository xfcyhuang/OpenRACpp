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

struct ResourceTypeInfoData {
  std::uint8_t uint1_resource_index = 0;
  std::string str_terrain_type;
  std::vector<std::string> vec_allowed_terrain_types;
  std::uint8_t uint1_max_density = 10;

  static ResourceTypeInfoData Parse(const meta::RecordObject& rec_info);
};

class ResourceLayerInfoData : public sim::IResourceLayerInfo {
 public:

  std::vector<std::pair<std::string, ResourceTypeInfoData>>
      vec_resource_types;
  bool b_recalculate_resource_density = false;

  static ResourceLayerInfoData Parse(const meta::RecordObject& rec_info);

  const ResourceTypeInfoData* FindType(
      const std::string& resource_type) const {
    for (const auto& [name, info] : vec_resource_types)
      if (name == resource_type)
        return &info;
    return nullptr;
  }

  bool TryGetTerrainType(const std::string& resource_type,
                         std::string& terrain_type_out) override;
  bool TryGetResourceIndex(const std::string& resource_type,
                           std::uint8_t& index_out) override;
};

class ResourceLayer final : public sim::TraitBase,
                            public sim::IResourceLayer,
                            public sim::IWorldLoaded {
 public:
  ResourceLayer(Actor& self, const ResourceLayerInfoData& info);

  ORA_TRAIT_INTERFACES(ResourceLayer,
                       OpenRA_Mods_Common_Traits_ResourceLayer,
                       sim::IResourceLayer, sim::IWorldLoaded)

  void AddCellChangedListener(
      std::function<void(CPos, const std::string&)> fn_listener) {
    vec_cell_changed_.push_back(std::move(fn_listener));
  }

  void WorldLoaded(sim::World& w, gfx::WorldRenderer* wr) override;

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

  virtual bool AllowResourceAt(const std::string& resource_type, CPos cell);

 private:

  sim::ResourceLayerContents CreateResourceCell(
      const std::string& resource_type, CPos cell, int density);

  const ResourceLayerInfoData info_;
  sim::World& world_;
  map::Map& map_;
  BuildingInfluence& building_influence_;
  std::unique_ptr<map::CellLayer<sim::ResourceLayerContents>>
      ptr_content_;
  std::map<std::uint8_t, std::string> map_resource_types_by_index_;

  int int4_res_cells_ = 0;

  std::vector<std::function<void(CPos, const std::string&)>>
      vec_cell_changed_;
};

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

  bool TryClaimCell(Actor& claimer, CPos cell);

  bool CanClaimCell(Actor& claimer, CPos cell) const;

  void RemoveClaim(Actor& claimer);

 private:
  std::map<CPos, std::vector<Actor*>> map_claim_by_cell_;
  std::map<Actor*, CPos> map_claim_by_actor_;
};

}
