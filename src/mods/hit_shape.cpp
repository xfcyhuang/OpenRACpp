// UPSTREAM: OpenRA.Mods.Common/Traits/HitShape.cs 实现部分
//          The implementation half.
#include "mods/hit_shape.hpp"

#include "meta/field_loader.hpp"
#include "meta/generic_record.hpp"
#include "sim/trait_registry.hpp"
#include "sim/world.hpp"

namespace ora::mods {

// ———— HitShapeInfoData::Parse(L25-70 的字段面)————

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
        // BitSet 原始位(gen dump 协议)
        // The BitSet raw bits (the gen dump protocol).
        if (auto* n = std::get_if<std::int64_t>(&v.val))
          data.bitset_armor_types =
              core::BitSet<ArmorType>::FromRawBits(
                  static_cast<std::uint64_t>(*n));
      } else if (name == "Type") {
        // LoadShape 的嵌套记录产物(loaders.cpp 已物化;缺省 =
        // CircleShape 记录)
        // LoadShape's nested-record product (materialized by loaders.cpp;
        // the default = the CircleShape record).
        if (auto* rec = std::get_if<std::shared_ptr<meta::RecordObject>>(
                &v.val)) {
          if (*rec != nullptr)
            data.ptr_type = ParseHitShape(**rec);
        }
      }
    }
  }

  // LoadShape 缺省 CircleShape(L154-155;空槽 = 上游 shapeNode == null)
  // LoadShape's CircleShape default (L154-155; an empty slot = upstream's
  // shapeNode == null).
  if (data.ptr_type == nullptr)
    data.ptr_type = std::make_unique<CircleShape>();

  data.conditional = sim::ConditionalTraitData::Parse(rec_info);
  return data;
}

// ———— HitShape(L72-177)————

HitShape::HitShape(ActorInitializer& init, HitShapeInfoData&& info)
    : sim::ConditionalTraitCore<HitShape>(info.conditional),
      info_{std::move(info)} {
  // L86-90:orientation = self.Trait<BodyOrientation>()
  coords_ = init.Self().Trait<BodyOrientation>();
}

void HitShape::Created(Actor& self) {
  // L92-98(turret 面:Turreted 未移植 → TraitsImplementing<Turreted>() 空
  // 集,FirstOrDefault == null —— 上游等价分支;COVERAGE 登记)
  // L92-98 (the turret face: Turreted unported → the
  // TraitsImplementing<Turreted>() empty set gives FirstOrDefault == null —
  // upstream's equivalent branch; registered in COVERAGE).
  targetable_cells_ = self.TraitOrDefault<sim::ITargetableCells>();

  // ConditionalTrait.Created
  CoreCreated(self);
}

std::vector<WPos> HitShape::TargetablePositions(Actor& self) {
  // L100-122
  if (IsTraitDisabled())
    return {};

  // Check for changes in inputs that affect the result of the
  // TargetablePositions method.(上游注释;缓存键 = 简化三字段,见头注)
  // Check for changes in inputs that affect the result of the
  // TargetablePositions method. (the upstream comment; the cache key =
  // the simplified three fields, see the header).
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
  // L124-135
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
  // L137-149(turret 面空集 → 体朝向单段)
  // L137-149 (the turret-less face → the single body-orientation leg).
  const WRot quantized_body_orientation =
      coords_->QuantizeOrientation(self.Orientation());
  return coords_->LocalToWorld(offset.Rotate(quantized_body_orientation));
}

WDist HitShape::DistanceFromEdge(Actor& self, const WPos& pos) {
  // L151-156(turret == null → 体原点/体朝向)
  // L151-156 (turret == null → the body origin/orientation).
  return info_.ptr_type->DistanceFromEdge(pos, self.CenterPosition(),
                                          self.Orientation());
}

}  // namespace ora::mods
