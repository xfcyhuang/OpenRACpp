#include "mods/affects_shroud.hpp"

#include "core/percent_modifiers.hpp"
#include "meta/generic_record.hpp"
#include "meta/field_loader.hpp"
#include "sim/world.hpp"
#include "sim/sync_hash.hpp"
#include "sim/trait_registry.hpp"

namespace ora::mods {

AffectsShroudInfoData AffectsShroudInfoData::Parse(
    const meta::RecordObject& rec_info) {
  AffectsShroudInfoData data;
  if (const auto v = sim::RecordFieldInt(rec_info, "MinRange"))
    data.dist_min_range = WDist{static_cast<std::int32_t>(*v)};
  if (const auto v = sim::RecordFieldInt(rec_info, "Range"))
    data.dist_range = WDist{static_cast<std::int32_t>(*v)};
  if (const auto v = sim::RecordFieldInt(rec_info, "MaxHeightDelta"))
    data.int4_max_height_delta = static_cast<int>(*v);
  if (const auto v =
          sim::RecordFieldInt(rec_info, "MoveRecalculationThreshold"))
    data.dist_move_recalculation_threshold =
        WDist{static_cast<std::int32_t>(*v)};
  if (const auto v = sim::RecordFieldInt(rec_info, "Type"))
    data.type = static_cast<VisibilityType>(static_cast<std::int32_t>(*v));
  data.conditional = sim::ConditionalTraitData::Parse(rec_info);
  return data;
}

template <class Derived>
std::vector<PPos> AffectsShroudBase<Derived>::ProjectedCells(Actor& self) {

  static const std::vector<PPos> no_cells;
  const map::Map& map = self.world().Map();
  const WDist min_range = info_.dist_min_range;
  const WDist max_range = static_cast<Derived*>(this)->Range();
  if (max_range <= min_range)
    return no_cells;

  if (info_.type == VisibilityType::Footprint) {

    std::vector<PPos> vec_footprint;
    for (const auto& kv : self.OccupiesSpace()->OccupiedCells())
      for (const PPos& puv : sim::Shroud::ProjectedCellsInRange(
               map, map.CenterOfCell(kv.first), min_range, max_range,
               info_.int4_max_height_delta)) {
        if (std::find(vec_footprint.begin(), vec_footprint.end(), puv) ==
            vec_footprint.end())
          vec_footprint.push_back(puv);
      }
    return vec_footprint;
  }

  WPos pos = self.CenterPosition();
  if (info_.type == VisibilityType::GroundPosition)
    pos = pos - WVec{0, 0, map.DistanceAboveTerrain(pos).Length};

  return sim::Shroud::ProjectedCellsInRange(map, pos, min_range, max_range,
                                            info_.int4_max_height_delta);
}

template <class Derived>
void AffectsShroudBase<Derived>::CenterPositionChanged(
    Actor& self, std::uint8_t , std::uint8_t ) {

  if (!self.IsInWorld())
    return;

  const WPos center_position = self.CenterPosition();
  const WPos projected_pos =
      center_position - WVec{0, center_position.Z, center_position.Z};
  const CPos projected_location = self.world().Map().CellContaining(
      projected_pos);
  const WPos pos = self.CenterPosition();

  const bool b_dirty =
      info_.dist_move_recalculation_threshold.Length > 0 &&
      (pos - pos_cached_).LengthSquared() >
          info_.dist_move_recalculation_threshold.LengthSquared();
  if (!b_dirty && cached_location_ == projected_location)
    return;

  cached_location_ = projected_location;
  pos_cached_ = pos;

  UpdateShroudCells(self);
}

template <class Derived>
void AffectsShroudBase<Derived>::Tick(Actor& self) {

  if (!self.IsInWorld())
    return;

  const bool b_trait_disabled =
      sim::ConditionalTraitCore<Derived>::IsTraitDisabled();
  const WDist range = static_cast<Derived*>(this)->Range();

  if (cached_range_ == range && b_trait_disabled == cached_trait_disabled_)
    return;

  cached_range_ = range;
  cached_trait_disabled_ = b_trait_disabled;

  UpdateShroudCells(self);
}

template <class Derived>
void AffectsShroudBase<Derived>::UpdateShroudCells(Actor& self) {

  const std::vector<PPos> cells = ProjectedCells(self);
  for (Player* p : self.world().Players()) {
    CallRemoveCellsFromPlayerShroud(self, *p);
    CallAddCellsToPlayerShroud(self, *p, cells);
  }
}

template <class Derived>
void AffectsShroudBase<Derived>::AddedToWorld(Actor& self) {

  const WPos center_position = self.CenterPosition();
  const WPos projected_pos =
      center_position - WVec{0, center_position.Z, center_position.Z};
  cached_location_ = self.world().Map().CellContaining(projected_pos);
  pos_cached_ = center_position;
  cached_trait_disabled_ =
      sim::ConditionalTraitCore<Derived>::IsTraitDisabled();
  const std::vector<PPos> cells = ProjectedCells(self);

  for (Player* p : self.world().Players())
    CallAddCellsToPlayerShroud(self, *p, cells);
}

template <class Derived>
void AffectsShroudBase<Derived>::RemovedFromWorld(Actor& self) {

  for (Player* p : self.world().Players())
    CallRemoveCellsFromPlayerShroud(self, *p);
}

template <class Derived>
void AffectsShroudBase<Derived>::MovementTypeChanged(
    Actor& self, sim::MovementType type) {

  if (type == sim::MovementType::None && self.IsInWorld()) {
    const WPos center_position = self.CenterPosition();
    const WPos projected_pos =
        center_position - WVec{0, center_position.Z, center_position.Z};
    cached_location_ = self.world().Map().CellContaining(projected_pos);
    pos_cached_ = center_position;

    UpdateShroudCells(self);
  }
}

RevealsShroudInfoData RevealsShroudInfoData::Parse(
    const meta::RecordObject& rec_info) {
  RevealsShroudInfoData data;
  static_cast<AffectsShroudInfoData&>(data) =
      AffectsShroudInfoData::Parse(rec_info);

  if (const auto* generated =
          dynamic_cast<const meta::GeneratedRecord*>(&rec_info)) {
    const std::vector<const meta::FieldDesc*> fields =
        meta::CollectFields(generated->record_desc());
    for (std::size_t i = 0; i < fields.size(); i++) {
      const std::string_view name = fields[i]->str_name;
      const meta::GenericValue& v = generated->Slot(i);
      if (name == "ValidRelationships") {
        if (auto* n = std::get_if<std::int64_t>(&v.val))
          data.valid_relationships =
              static_cast<sim::PlayerRelationship>(
                  static_cast<std::int32_t>(*n));
      } else if (name == "RevealGeneratedShroud") {
        if (auto* b = std::get_if<bool>(&v.val))
          data.b_reveal_generated_shroud = *b;
      }
    }
  }
  return data;
}

RevealsShroud::RevealsShroud(const RevealsShroudInfoData& info)
    : AffectsShroudBase<RevealsShroud>(info), reveals_info_{info} {

  source_type_ = reveals_info_.b_reveal_generated_shroud
                     ? sim::ShroudSourceType::Visibility
                     : sim::ShroudSourceType::PassiveVisibility;
}

void RevealsShroud::Created(Actor& self) {

  vec_range_modifier_ptrs_ =
      self.TraitsImplementing<sim::IRevealsShroudModifier>();

  CoreCreated(self);
}

void RevealsShroud::AddCellsToPlayerShroud(Actor& self, Player& p,
                                           const std::vector<PPos>& vec_uv) {

  if (!sim::HasRelationship(reveals_info_.valid_relationships,
                            self.Owner()->RelationshipWith(&p)))
    return;

  p.GetShroud()->AddSource(this, source_type_, vec_uv);
}

void RevealsShroud::RemoveCellsFromPlayerShroud(Actor& , Player& p) {

  p.GetShroud()->RemoveSource(this);
}

WDist RevealsShroud::Range() const {

  if (cached_trait_disabled_)
    return WDist{0};

  thread_local std::vector<std::int32_t> vec_modifiers;
  vec_modifiers.clear();
  for (const auto* modifier : vec_range_modifier_ptrs_)
    vec_modifiers.push_back(modifier->GetRevealsShroudModifier());
  return WDist{ApplyPercentageModifiers(BaseInfo().dist_range.Length,
                                        vec_modifiers)};
}

int RevealsShroudSyncHash(const sim::ISync* s) {
  const auto* reveals = static_cast<const RevealsShroud*>(s);
  return sim::sync::CombineSyncHash(
      0, sim::sync::HashBool(reveals->cached_trait_disabled_));
}

const bool b_reveals_shroud_sync_registered = [] {
  sim::RegisterSyncHashFunction("OpenRA.Mods.Common.Traits.RevealsShroud",
                                &RevealsShroudSyncHash);
  return true;
}();
[[maybe_unused]] const bool* b_reveals_shroud_sync_registered_anchor =
    &b_reveals_shroud_sync_registered;

template class AffectsShroudBase<RevealsShroud>;

}
