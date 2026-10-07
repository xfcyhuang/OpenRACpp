// UPSTREAM: OpenRA.Mods.Common/Traits/Buildings/Building.cs 实现部分
//          | The implementation half.
#include "mods/building.hpp"

#include "map/map.hpp"
#include "meta/field_loader.hpp"
#include "meta/generic_record.hpp"
#include "mods/building_influence.hpp"
#include "mods/smudge_layer.hpp"
#include "sim/actor_init.hpp"
#include "sim/actor_map.hpp"
#include "sim/world.hpp"

namespace ora::mods {

namespace {

std::optional<std::int64_t> RecInt(const meta::RecordObject& rec,
                                   std::string_view str_name) {
  if (const auto* generated =
          dynamic_cast<const meta::GeneratedRecord*>(&rec)) {
    const std::vector<const meta::FieldDesc*> fields =
        meta::CollectFields(generated->record_desc());
    for (std::size_t i = 0; i < fields.size(); i++)
      if (fields[i]->str_name == str_name) {
        const meta::GenericValue& v = generated->Slot(i);
        if (auto* n = std::get_if<std::int64_t>(&v.val))
          return *n;
      }
  }
  return std::nullopt;
}

std::optional<std::string> RecString(const meta::RecordObject& rec,
                                     std::string_view str_name) {
  if (const auto* generated =
          dynamic_cast<const meta::GeneratedRecord*>(&rec)) {
    const std::vector<const meta::FieldDesc*> fields =
        meta::CollectFields(generated->record_desc());
    for (std::size_t i = 0; i < fields.size(); i++)
      if (fields[i]->str_name == str_name) {
        const meta::GenericValue& v = generated->Slot(i);
        if (auto* s = std::get_if<std::string>(&v.val))
          return *s;
      }
  }
  return std::nullopt;
}

std::vector<std::string> RecStringArray(const meta::RecordObject& rec,
                                        std::string_view str_name) {
  std::vector<std::string> vec_out;
  if (const auto* generated =
          dynamic_cast<const meta::GeneratedRecord*>(&rec)) {
    const std::vector<const meta::FieldDesc*> fields =
        meta::CollectFields(generated->record_desc());
    for (std::size_t i = 0; i < fields.size(); i++)
      if (fields[i]->str_name == str_name) {
        const meta::GenericValue& v = generated->Slot(i);
        if (auto* list = std::get_if<std::vector<meta::GenericValue>>(
                &v.val))
          for (const auto& element : *list)
            if (auto* s = std::get_if<std::string>(&element.val))
              vec_out.push_back(*s);
      }
  }
  return vec_out;
}

}  // namespace

BuildingInfoData BuildingInfoData::Parse(const meta::RecordObject& rec_info) {
  BuildingInfoData data;
  data.vec_terrain_types = RecStringArray(rec_info, "TerrainTypes");

  // Footprint 槽(loaders.cpp RegisterLoadFootprint 产物:dict{tuple(x,y)
  // → int(char 码点)};行主序 y 外 x 内 —— 上游字典同建序)
  // The Footprint slot (loaders.cpp's RegisterLoadFootprint product:
  // dict{tuple(x,y) → int(char code)}; row-major x-within-y — the same
  // build order as upstream's dictionary).
  if (const auto* generated =
          dynamic_cast<const meta::GeneratedRecord*>(&rec_info)) {
    const std::vector<const meta::FieldDesc*> fields =
        meta::CollectFields(generated->record_desc());
    for (std::size_t i = 0; i < fields.size(); i++)
      if (fields[i]->str_name == "Footprint") {
        const meta::GenericValue& v = generated->Slot(i);
        if (auto* dict = std::get_if<meta::GenericDict>(&v.val)) {
          if (!dict->empty()) {
            data.vec_footprint.clear();
            for (const auto& [key, value] : *dict) {
              const auto* key_tuple =
                  std::get_if<meta::GenericTuple>(&key.val);
              auto* code = std::get_if<std::int64_t>(&value.val);
              if (key_tuple == nullptr || code == nullptr)
                continue;
              data.vec_footprint.emplace_back(
                  CVec{static_cast<std::int32_t>((*key_tuple).arr_ints[0]),
                       static_cast<std::int32_t>(
                           (*key_tuple).arr_ints[1])},
                  static_cast<FootprintCellType>(
                      static_cast<std::int32_t>(*code)));
            }
          }
        }
      }
  }

  // Dimensions 的 CVec 槽(GenericTuple 双整型分量)
  // The Dimensions CVec slot (GenericTuple's two integer components).
  if (const auto* generated =
          dynamic_cast<const meta::GeneratedRecord*>(&rec_info)) {
    const std::vector<const meta::FieldDesc*> fields =
        meta::CollectFields(generated->record_desc());
    for (std::size_t i = 0; i < fields.size(); i++)
      if (fields[i]->str_name == "Dimensions") {
        const meta::GenericValue& v = generated->Slot(i);
        if (auto* tuple = std::get_if<meta::GenericTuple>(&v.val))
          data.dimensions =
              CVec{static_cast<std::int32_t>((*tuple).arr_ints[0]),
                   static_cast<std::int32_t>((*tuple).arr_ints[1])};
      }
  }

  if (const auto v = RecInt(rec_info, "RequiresBaseProvider"))
    data.b_requires_base_provider = *v != 0;
  if (const auto v = RecInt(rec_info, "AllowInvalidPlacement"))
    data.b_allow_invalid_placement = *v != 0;
  if (const auto v = RecInt(rec_info, "RemoveSmudgesOnBuild"))
    data.b_remove_smudges_on_build = *v != 0;
  if (const auto v = RecInt(rec_info, "RemoveSmudgesOnSell"))
    data.b_remove_smudges_on_sell = *v != 0;
  if (const auto v = RecInt(rec_info, "RemoveSmudgesOnTransform"))
    data.b_remove_smudges_on_transform = *v != 0;
  data.vec_build_sounds = RecStringArray(rec_info, "BuildSounds");
  data.vec_undeploy_sounds = RecStringArray(rec_info, "UndeploySounds");
  return data;
}

std::vector<CPos> BuildingInfoData::FootprintTiles(
    CPos location, FootprintCellType type) const {
  // L97-100
  std::vector<CPos> vec_out;
  for (const auto& [offset, cell_type] : vec_footprint)
    if (cell_type == type)
      vec_out.push_back(location + offset);
  return vec_out;
}

std::vector<CPos> BuildingInfoData::Tiles(CPos location) const {
  // L102-115
  std::vector<CPos> vec_out =
      FootprintTiles(location, FootprintCellType::OccupiedPassable);
  const std::vector<CPos> vec_occupied =
      FootprintTiles(location, FootprintCellType::Occupied);
  vec_out.insert(vec_out.end(), vec_occupied.begin(), vec_occupied.end());
  const std::vector<CPos> vec_untargetable =
      FootprintTiles(location, FootprintCellType::OccupiedUntargetable);
  vec_out.insert(vec_out.end(), vec_untargetable.begin(),
                 vec_untargetable.end());
  const std::vector<CPos> vec_transit = FootprintTiles(
      location, FootprintCellType::OccupiedPassableTransitOnly);
  vec_out.insert(vec_out.end(), vec_transit.begin(), vec_transit.end());
  return vec_out;
}

std::vector<CPos> BuildingInfoData::FrozenUnderFogTiles(CPos location) const {
  // L117-124
  std::vector<CPos> vec_out = FootprintTiles(location, FootprintCellType::Empty);
  const std::vector<CPos> vec_tiles = Tiles(location);
  vec_out.insert(vec_out.end(), vec_tiles.begin(), vec_tiles.end());
  return vec_out;
}

std::vector<CPos> BuildingInfoData::OccupiedTiles(CPos location) const {
  // L126-136
  std::vector<CPos> vec_out =
      FootprintTiles(location, FootprintCellType::Occupied);
  const std::vector<CPos> vec_untargetable =
      FootprintTiles(location, FootprintCellType::OccupiedUntargetable);
  vec_out.insert(vec_out.end(), vec_untargetable.begin(),
                 vec_untargetable.end());
  const std::vector<CPos> vec_transit = FootprintTiles(
      location, FootprintCellType::OccupiedPassableTransitOnly);
  vec_out.insert(vec_out.end(), vec_transit.begin(), vec_transit.end());
  return vec_out;
}

std::vector<CPos> BuildingInfoData::PathableTiles(CPos location) const {
  // L138-145
  std::vector<CPos> vec_out = FootprintTiles(location, FootprintCellType::Empty);
  const std::vector<CPos> vec_passable =
      FootprintTiles(location, FootprintCellType::OccupiedPassable);
  vec_out.insert(vec_out.end(), vec_passable.begin(), vec_passable.end());
  return vec_out;
}

std::vector<CPos> BuildingInfoData::TransitOnlyTiles(CPos location) const {
  // L147-151
  return FootprintTiles(location,
                        FootprintCellType::OccupiedPassableTransitOnly);
}

WVec BuildingInfoData::CenterOffset(sim::World& w) const {
  // L153-157
  const WVec off = w.Map().CenterOfCell(CPos{dimensions.X, dimensions.Y}) -
                   w.Map().CenterOfCell(CPos{1, 1});
  return off / 2 - WVec{0, 0, off.Z / 2} + local_center_offset;
}

std::vector<std::pair<CPos, SubCell>> BuildingInfoData::OccupiedCellsInfo(
    CPos top_left) const {
  // L249-253
  std::vector<std::pair<CPos, SubCell>> vec_out;
  for (const CPos c : OccupiedTiles(top_left))
    vec_out.emplace_back(c, SubCell::FullCell);
  return vec_out;
}

// ———— Building(L282-355)————
// ———— Building (L282-355) ————

Building::Building(sim::ActorInitializer& init, BuildingInfoData info)
    : info_{std::move(info)} {
  // L282-298
  sim::Actor& self = init.Self();
  top_left_ = init.GetValue<sim::LocationInit>();
  for (const CPos cell : info_.OccupiedTiles(top_left_))
    vec_occupied_cells_.emplace_back(cell, SubCell::FullCell);

  for (const CPos cell :
       info_.FootprintTiles(top_left_, FootprintCellType::Occupied))
    vec_targetable_cells_.emplace_back(cell, SubCell::FullCell);

  vec_transit_only_cells_ = info_.TransitOnlyTiles(top_left_);

  center_position_ =
      init.Self().world().Map().CenterOfCell(top_left_) +
      info_.CenterOffset(init.Self().world());
  (void)self;
}

void Building::AddedToWorld(sim::Actor& self) {
  // L306-318
  if (info_.b_remove_smudges_on_build)
    RemoveSmudges(self);

  self.world().AddToMaps(&self, this);

  // BuildingInfluence.AddInfluence(self, Info.Tiles(Location))
  // BuildingInfluence.AddInfluence(self, Info.Tiles(Location)).
  if (auto* influence =
          self.world().WorldActor()->TraitOrDefault<BuildingInfluence>())
    influence->AddInfluence(&self, info_.Tiles(self.Location()));
}

void Building::RemovedFromWorld(sim::Actor& self) {
  // L320-324
  self.world().RemoveFromMaps(&self, this);
  if (auto* influence =
          self.world().WorldActor()->TraitOrDefault<BuildingInfluence>())
    influence->RemoveInfluence(&self, info_.Tiles(self.Location()));
}

void Building::Selling(sim::Actor& self) {
  // L326-329
  if (info_.b_remove_smudges_on_sell)
    RemoveSmudges(self);
}

void Building::BeforeTransform(sim::Actor& self) {
  // L334-340(UndeploySounds 的 PlayToPlayer 随声音注入面)
  // (UndeploySounds' PlayToPlayer rides the sound injection face.)
  if (info_.b_remove_smudges_on_transform)
    RemoveSmudges(self);
}

void Building::RemoveSmudges(sim::Actor& self) {
  // L346-353
  for (auto* smudge_layer :
       self.world().WorldActor()->TraitsImplementing<SmudgeLayer>())
    for (const CPos footprint_tile : info_.Tiles(self.Location()))
      smudge_layer->RemoveSmudge(footprint_tile);
}

}  // namespace ora::mods
