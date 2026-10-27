#include "mods/resource_layer.hpp"

#include "map/cell_region.hpp"
#include "game/ruleset.hpp"
#include "terrain/terrain_info.hpp"
#include "meta/field_loader.hpp"
#include "meta/generic_record.hpp"
#include "mods/building.hpp"
#include "sim/trait_registry.hpp"
#include "sim/world.hpp"

namespace ora::mods {

ResourceTypeInfoData ResourceTypeInfoData::Parse(
    const meta::RecordObject& rec_info) {
  ResourceTypeInfoData data;

  if (const auto* generated =
          dynamic_cast<const meta::GeneratedRecord*>(&rec_info)) {
    const std::vector<const meta::FieldDesc*> fields =
        meta::CollectFields(generated->record_desc());
    for (std::size_t i = 0; i < fields.size(); i++) {
      const std::string_view name = fields[i]->str_name;
      const meta::GenericValue& v = generated->Slot(i);
      if (name == "ResourceIndex") {
        if (auto* n = std::get_if<std::int64_t>(&v.val))
          data.uint1_resource_index =
              static_cast<std::uint8_t>(*n);
      } else if (name == "TerrainType") {
        if (auto* s = std::get_if<std::string>(&v.val))
          data.str_terrain_type = *s;
      } else if (name == "AllowedTerrainTypes") {

        if (auto* s = std::get_if<std::string>(&v.val)) {
          std::string str_current;
          for (const char ch : *s + ",") {
            if (ch == ',') {

              const std::size_t begin =
                  str_current.find_first_not_of(' ');
              const std::size_t end =
                  str_current.find_last_not_of(' ');
              if (begin != std::string::npos)
                data.vec_allowed_terrain_types.push_back(
                    str_current.substr(begin, end - begin + 1));
              str_current.clear();
            } else {
              str_current.push_back(ch);
            }
          }
        } else if (auto* list =
                       std::get_if<std::vector<meta::GenericValue>>(
                           &v.val)) {
          for (const auto& element : *list)
            if (auto* s2 = std::get_if<std::string>(&element.val))
              data.vec_allowed_terrain_types.push_back(*s2);
        }
      } else if (name == "MaxDensity") {
        if (auto* n = std::get_if<std::int64_t>(&v.val))
          data.uint1_max_density = static_cast<std::uint8_t>(*n);
      }
    }
  }

  return data;
}

ResourceLayerInfoData ResourceLayerInfoData::Parse(
    const meta::RecordObject& rec_info) {
  ResourceLayerInfoData data;

  if (const auto* generated =
          dynamic_cast<const meta::GeneratedRecord*>(&rec_info)) {
    const std::vector<const meta::FieldDesc*> fields =
        meta::CollectFields(generated->record_desc());
    for (std::size_t i = 0; i < fields.size(); i++) {
      const std::string_view name = fields[i]->str_name;
      const meta::GenericValue& v = generated->Slot(i);
      if (name == "ResourceTypes") {

        if (auto* dict = std::get_if<meta::GenericDict>(&v.val)) {
          for (const auto& [key, value] : *dict) {
            const std::string* str_key =
                std::get_if<std::string>(&key.val);
            const auto* rec_nested =
                std::get_if<std::shared_ptr<meta::RecordObject>>(
                    &value.val);
            if (str_key == nullptr || rec_nested == nullptr ||
                *rec_nested == nullptr)
              continue;

            data.vec_resource_types.emplace_back(
                *str_key, ResourceTypeInfoData::Parse(**rec_nested));
          }
        }
      } else if (name == "RecalculateResourceDensity") {
        if (auto* b = std::get_if<bool>(&v.val))
          data.b_recalculate_resource_density = *b;
      }
    }
  }

  return data;
}

bool ResourceLayerInfoData::TryGetTerrainType(
    const std::string& resource_type, std::string& terrain_type_out) {

  const ResourceTypeInfoData* resource_info = FindType(resource_type);
  if (resource_info == nullptr) {
    terrain_type_out.clear();
    return false;
  }

  terrain_type_out = resource_info->str_terrain_type;
  return true;
}

bool ResourceLayerInfoData::TryGetResourceIndex(
    const std::string& resource_type, std::uint8_t& index_out) {

  const ResourceTypeInfoData* resource_info = FindType(resource_type);
  if (resource_info == nullptr) {
    index_out = 0;
    return false;
  }

  index_out = resource_info->uint1_resource_index;
  return true;
}

ResourceLayer::ResourceLayer(Actor& self, const ResourceLayerInfoData& info)
    : info_{info},
      world_{self.world()},
      map_{self.world().Map()},
      building_influence_{*self.Trait<BuildingInfluence>()} {

  ptr_content_ =
      std::make_unique<map::CellLayer<sim::ResourceLayerContents>>(map_);
  for (const auto& [name, type_info] : info_.vec_resource_types) {
    const auto it_inserted = map_resource_types_by_index_.emplace(
        type_info.uint1_resource_index, name);
    if (!it_inserted.second)
      throw std::runtime_error(
          "An item with the same key has already been added.");
  }
}

void ResourceLayer::WorldLoaded(sim::World& w,
                                gfx::WorldRenderer* ) {

  for (const CPos& cell : map_.AllCells()) {
    const map::ResourceTile resource = map_.Resources().Get(cell);
    const auto it_find = map_resource_types_by_index_.find(resource.Type);
    if (it_find == map_resource_types_by_index_.end())
      continue;

    if (!AllowResourceAt(it_find->second, cell))
      continue;

    ptr_content_->Set(cell,
                      CreateResourceCell(it_find->second, cell,
                                         resource.Index));
  }

  if (!info_.b_recalculate_resource_density)
    return;

  for (const CPos& cell : map_.AllCells()) {
    const sim::ResourceLayerContents resource = ptr_content_->Get(cell);
    if (resource.str_type.empty())
      continue;
    const ResourceTypeInfoData* resource_info =
        info_.FindType(resource.str_type);
    if (resource_info == nullptr)
      continue;

    int adjacent = 0;
    const auto directions = CVec::Directions();
    for (std::size_t i = 0; i < directions.size(); i++) {
      const CPos c = cell + directions[i];
      if (map_.Contains(c) &&
          ptr_content_->Get(c).str_type == resource.str_type)
        ++adjacent;
    }

    const int density = std::max(
        int2::Lerp(0, resource_info->uint1_max_density, adjacent, 9), 1);
    ptr_content_->Set(
        cell,
        sim::ResourceLayerContents{resource.str_type,
                                   static_cast<std::uint8_t>(density)});
  }

  [[maybe_unused]] sim::World& unused_w = w;
}

bool ResourceLayer::AllowResourceAt(const std::string& resource_type,
                                    CPos cell) {

  if (!map_.Contains(cell) || map_.Ramp().Get(cell) != 0)
    return false;

  const ResourceTypeInfoData* resource_info = info_.FindType(resource_type);
  if (resource_info == nullptr)
    return false;

  const std::string& cell_terrain_type = map_.GetTerrainInfo(cell).Type;
  if (std::find(resource_info->vec_allowed_terrain_types.begin(),
                resource_info->vec_allowed_terrain_types.end(),
                cell_terrain_type) ==
      resource_info->vec_allowed_terrain_types.end())
    return false;

  for (Actor* a : building_influence_.GetBuildingsAt(cell)) {
    const BuildingInfoData& building_info = a->Trait<Building>()->Info();
    if (std::find(building_info.vec_terrain_types.begin(),
                  building_info.vec_terrain_types.end(),
                  resource_info->str_terrain_type) ==
        building_info.vec_terrain_types.end())
      return false;
  }
  return true;
}

sim::ResourceLayerContents ResourceLayer::CreateResourceCell(
    const std::string& resource_type, CPos cell, int density) {

  const ResourceTypeInfoData* resource_info = info_.FindType(resource_type);
  if (resource_info == nullptr) {
    map_.CustomTerrainRef().Set(cell, 255);
    return sim::ResourceLayerContents::Empty();
  }

  map_.CustomTerrainRef().Set(
      cell, map_.Rules().TerrainInfo().GetTerrainIndex(
                resource_info->str_terrain_type));
  ++int4_res_cells_;

  const int clamped =
      std::clamp(density, 1, static_cast<int>(resource_info->uint1_max_density));
  return sim::ResourceLayerContents{resource_type,
                                    static_cast<std::uint8_t>(clamped)};
}

bool ResourceLayer::CanAddResource(const std::string& resource_type,
                                   CPos cell, std::uint8_t amount) {

  if (!map_.Contains(cell))
    return false;

  const ResourceTypeInfoData* resource_info = info_.FindType(resource_type);
  if (resource_info == nullptr)
    return false;

  const sim::ResourceLayerContents content = ptr_content_->Get(cell);
  if (content.str_type.empty())
    return amount <= resource_info->uint1_max_density &&
           AllowResourceAt(resource_type, cell);

  if (content.str_type != resource_type)
    return false;

  return content.uint1_density + amount <=
         resource_info->uint1_max_density;
}

int ResourceLayer::AddResource(const std::string& resource_type, CPos cell,
                               std::uint8_t amount) {

  if (!map_.Contains(cell))
    return 0;

  const ResourceTypeInfoData* resource_info = info_.FindType(resource_type);
  if (resource_info == nullptr)
    return 0;

  sim::ResourceLayerContents content = ptr_content_->Get(cell);
  if (content.str_type.empty())
    content = CreateResourceCell(resource_type, cell, 0);

  if (content.str_type != resource_type)
    return 0;

  const int old_density = content.uint1_density;
  const int density = std::min(
      static_cast<int>(resource_info->uint1_max_density),
      old_density + static_cast<int>(amount));
  ptr_content_->Set(
      cell, sim::ResourceLayerContents{content.str_type,
                                       static_cast<std::uint8_t>(density)});

  for (const auto& fn : vec_cell_changed_)
    fn(cell, content.str_type);

  return density - old_density;
}

int ResourceLayer::RemoveResource(const std::string& resource_type,
                                  CPos cell, std::uint8_t amount) {

  if (!map_.Contains(cell))
    return 0;

  const sim::ResourceLayerContents content = ptr_content_->Get(cell);
  if (content.str_type.empty() || content.str_type != resource_type)
    return 0;

  const int old_density = content.uint1_density;
  const int density =
      std::max(0, old_density - static_cast<int>(amount));

  if (density == 0) {
    ptr_content_->Set(cell, sim::ResourceLayerContents::Empty());
    map_.CustomTerrainRef().Set(cell, 255);
    --int4_res_cells_;

    for (const auto& fn : vec_cell_changed_)
      fn(cell, std::string{});
  } else {
    ptr_content_->Set(
        cell, sim::ResourceLayerContents{
                  content.str_type,
                  static_cast<std::uint8_t>(density)});
    for (const auto& fn : vec_cell_changed_)
      fn(cell, content.str_type);
  }

  return old_density - density;
}

void ResourceLayer::ClearResources(CPos cell) {

  if (!map_.Contains(cell))
    return;

  const sim::ResourceLayerContents content = ptr_content_->Get(cell);
  if (content.str_type.empty())
    return;

  ptr_content_->Set(cell, sim::ResourceLayerContents::Empty());
  map_.CustomTerrainRef().Set(cell, 255);
  --int4_res_cells_;

  for (const auto& fn : vec_cell_changed_)
    fn(cell, std::string{});
}

sim::ResourceLayerContents ResourceLayer::GetResource(CPos cell) {

  return map_.Contains(cell) ? ptr_content_->Get(cell)
                             : sim::ResourceLayerContents::Empty();
}

std::uint8_t ResourceLayer::GetMaxDensity(
    const std::string& resource_type) {

  const ResourceTypeInfoData* resource_info = info_.FindType(resource_type);
  if (resource_info == nullptr)
    return 0;
  return resource_info->uint1_max_density;
}

bool ResourceLayer::IsVisible(CPos cell) {

  return !world_.FogObscures(cell);
}

bool ResourceClaimLayer::TryClaimCell(Actor& claimer, CPos cell) {

  std::vector<Actor*>* vec_claimers = nullptr;
  if (const auto it_find = map_claim_by_cell_.find(cell);
      it_find != map_claim_by_cell_.end()) {
    vec_claimers = &it_find->second;

    std::erase_if(*vec_claimers,
                  [](const Actor* a) { return a->IsDead(); });

    for (const Actor* c : *vec_claimers)
      if (c != &claimer &&
          claimer.Owner()->IsAlliedWith(c->Owner()))
        return false;
  }

  if (const auto it_actor = map_claim_by_actor_.find(&claimer);
      it_actor != map_claim_by_actor_.end()) {
    if (const auto it_last = map_claim_by_cell_.find(it_actor->second);
        it_last != map_claim_by_cell_.end()) {
      auto& last_claimers = it_last->second;
      const auto it_remove = std::find(last_claimers.begin(),
                                       last_claimers.end(), &claimer);
      if (it_remove != last_claimers.end())
        last_claimers.erase(it_remove);
    }
  }

  if (vec_claimers == nullptr)
    vec_claimers =
        &map_claim_by_cell_.emplace(cell, std::vector<Actor*>{}).first->second;
  map_claim_by_actor_[&claimer] = cell;
  return true;
}

bool ResourceClaimLayer::CanClaimCell(Actor& claimer, CPos cell) const {

  const auto it_find = map_claim_by_cell_.find(cell);
  if (it_find == map_claim_by_cell_.end())
    return true;
  for (const Actor* c : it_find->second)
    if (c != &claimer && !c->IsDead() &&
        claimer.Owner()->IsAlliedWith(c->Owner()))
      return false;
  return true;
}

void ResourceClaimLayer::RemoveClaim(Actor& claimer) {

  if (const auto it_actor = map_claim_by_actor_.find(&claimer);
      it_actor != map_claim_by_actor_.end()) {
    if (const auto it_last = map_claim_by_cell_.find(it_actor->second);
        it_last != map_claim_by_cell_.end()) {
      auto& last_claimers = it_last->second;
      const auto it_remove = std::find(last_claimers.begin(),
                                       last_claimers.end(), &claimer);
      if (it_remove != last_claimers.end())
        last_claimers.erase(it_remove);
    }
  }

  map_claim_by_actor_.erase(&claimer);
}

}
