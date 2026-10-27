#include "mods/hit_shape.hpp"

#include "meta/field_loader.hpp"
#include "meta/generic_record.hpp"
#include "sim/trait_registry.hpp"
#include "sim/world.hpp"

namespace ora::mods {

HitShapeInfoData HitShapeInfoData::Parse(const meta::RecordObject& rec_info) {
  HitShapeInfoData data;

  if (const auto v = sim::RecordFieldString(rec_info, "Turret"))
    data.str_turret = std::string{*v};
  if (const auto v = sim::RecordFieldInt(rec_info, "UseTargetableCellsOffsets"))
    data.b_use_targetable_cells_offsets = *v != 0;

  if (const auto* generated =
          dynamic_cast<const meta::GeneratedRecord*>(&rec_info)) {
    const std::vector<const meta::FieldDesc*> fields =
        meta::CollectFields(generated->record_desc());
    for (std::size_t i = 0; i < fields.size(); i++) {
      const std::string_view name = fields[i]->str_name;
      const meta::GenericValue& v = generated->Slot(i);
      if (name == "TargetableOffsets") {
        if (auto* list = std::get_if<std::vector<meta::GenericValue>>(&v.val)) {
          data.vec_targetable_offsets.clear();
          for (const auto& element : *list)
            if (auto* tuple = std::get_if<meta::GenericTuple>(&element.val))
              data.vec_targetable_offsets.push_back(
                  WVec{static_cast<int>(tuple->arr_ints[0]),
                       static_cast<int>(tuple->arr_ints[1]),
                       static_cast<int>(tuple->arr_ints[2])});
        }
      } else if (name == "ArmorTypes") {

        if (auto* n = std::get_if<std::int64_t>(&v.val))
          data.bitset_armor_types =
              core::BitSet<ArmorType>::FromRawBits(
                  static_cast<std::uint64_t>(*n));
      } else if (name == "Type") {

        if (auto* rec = std::get_if<std::shared_ptr<meta::RecordObject>>(
                &v.val)) {
          if (*rec != nullptr)
            data.ptr_type = ParseHitShape(**rec);
        }
      }
    }
  }

  if (data.ptr_type == nullptr)
    data.ptr_type = std::make_unique<CircleShape>();

  data.conditional = sim::ConditionalTraitData::Parse(rec_info);
  return data;
}

HitShape::HitShape(ActorInitializer& init, HitShapeInfoData&& info)
    : sim::ConditionalTraitCore<HitShape>(info.conditional),
      info_{std::move(info)} {

  coords_ = init.Self().Trait<BodyOrientation>();
}

void HitShape::Created(Actor& self) {

  targetable_cells_ = self.TraitOrDefault<sim::ITargetableCells>();

  CoreCreated(self);
}

std::vector<WPos> HitShape::TargetablePositions(Actor& self) {

  if (IsTraitDisabled())
    return {};

  CacheKey new_cache_input;
  if (info_.b_use_targetable_cells_offsets && targetable_cells_ != nullptr) {
    new_cache_input.b_has_cells = true;
    new_cache_input.vec_targetable_cells = targetable_cells_->TargetableCells();
  }
  if (!info_.vec_targetable_offsets.empty()) {
    new_cache_input.b_has_center = true;
    new_cache_input.center = self.CenterPosition();
    new_cache_input.b_has_orientation = true;
    new_cache_input.orientation = self.Orientation();
  }

  if (!b_cache_valid_ || !(cache_input_ == new_cache_input)) {
    vec_cached_targetable_positions_ = MaterializeTargetablePositions(self);
    cache_input_ = std::move(new_cache_input);
    b_cache_valid_ = true;
  }

  return vec_cached_targetable_positions_;
}

std::vector<WPos> HitShape::MaterializeTargetablePositions(Actor& self) {

  std::vector<WPos> vec_out;
  if (info_.b_use_targetable_cells_offsets && targetable_cells_ != nullptr)
    for (const auto& [cell, sub] : targetable_cells_->TargetableCells())
      vec_out.push_back(self.world().Map().CenterOfCell(cell));

  for (const WVec& o : info_.vec_targetable_offsets) {
    const WVec offset = CalculateTargetableOffset(self, o);
    vec_out.push_back(self.CenterPosition() + offset);
  }
  return vec_out;
}

WVec HitShape::CalculateTargetableOffset(Actor& self, const WVec& offset) {

  const WRot quantized_body_orientation =
      coords_->QuantizeOrientation(self.Orientation());
  return coords_->LocalToWorld(offset.Rotate(quantized_body_orientation));
}

WDist HitShape::DistanceFromEdge(Actor& self, const WPos& pos) {

  return info_.ptr_type->DistanceFromEdge(pos, self.CenterPosition(),
                                          self.Orientation());
}

}
