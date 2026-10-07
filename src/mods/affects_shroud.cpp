// UPSTREAM: OpenRA.Mods.Common/Traits/AffectsShroud.cs + RevealsShroud.cs
//          (实现部分) | The implementation half.
#include "mods/affects_shroud.hpp"

#include "core/percent_modifiers.hpp"
#include "meta/generic_record.hpp"
#include "meta/field_loader.hpp"
#include "sim/world.hpp"
#include "sim/sync_hash.hpp"
#include "sim/trait_registry.hpp"

namespace ora::mods {

// ———— AffectsShroudInfoData::Parse ————

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

// ———— AffectsShroudBase(模板基实现)————
// ———— AffectsShroudBase (the template-base implementations) ————

template <class Derived>
std::vector<PPos> AffectsShroudBase<Derived>::ProjectedCells(Actor& self) {
  // L63-87
  static const std::vector<PPos> no_cells;
  const map::Map& map = self.world().Map();
  const WDist min_range = info_.dist_min_range;
  const WDist max_range = static_cast<Derived*>(this)->Range();
  if (max_range <= min_range)
    return no_cells;

  if (info_.type == VisibilityType::Footprint) {
    // PERF: Reuse collection to avoid allocations.(上游注释;HashSet 暂存
    // → 去重并 —— UnionWith 语义)
    // PERF: Reuse collection to avoid allocations. (the upstream comment;
    // the HashSet staging → a dedup merge — UnionWith semantics).
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
    Actor& self, std::uint8_t /*old_layer*/, std::uint8_t /*new_layer*/) {
  // L89-107
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
  // L109-124
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
  // L126-134
  const std::vector<PPos> cells = ProjectedCells(self);
  for (Player* p : self.world().Players()) {
    CallRemoveCellsFromPlayerShroud(self, *p);
    CallAddCellsToPlayerShroud(self, *p, cells);
  }
}

template <class Derived>
void AffectsShroudBase<Derived>::AddedToWorld(Actor& self) {
  // L136-147
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
  // L149-153
  for (Player* p : self.world().Players())
    CallRemoveCellsFromPlayerShroud(self, *p);
}

template <class Derived>
void AffectsShroudBase<Derived>::MovementTypeChanged(
    Actor& self, sim::MovementType type) {
  // L157-172
  // Recalculate the visibility at our final stop position
  // (上游注释)
  if (type == sim::MovementType::None && self.IsInWorld()) {
    const WPos center_position = self.CenterPosition();
    const WPos projected_pos =
        center_position - WVec{0, center_position.Z, center_position.Z};
    cached_location_ = self.world().Map().CellContaining(projected_pos);
    pos_cached_ = center_position;

    UpdateShroudCells(self);
  }
}

// ———— RevealsShroudInfoData::Parse ————

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

// ———— RevealsShroud ————

RevealsShroud::RevealsShroud(const RevealsShroudInfoData& info)
    : AffectsShroudBase<RevealsShroud>(info), reveals_info_{info} {
  // L35-41
  source_type_ = reveals_info_.b_reveal_generated_shroud
                     ? sim::ShroudSourceType::Visibility
                     : sim::ShroudSourceType::PassiveVisibility;
}

void RevealsShroud::Created(Actor& self) {
  // L43-48(rangeModifiers 一次解析 —— 上游 Lazy IEnumerable 的 ToArray 缓存)
  // L43-48 (rangeModifiers resolved once — upstream's lazy IEnumerable's
  // ToArray cache).
  vec_range_modifier_ptrs_ =
      self.TraitsImplementing<sim::IRevealsShroudModifier>();

  // base.Created(self) → ConditionalTrait.Created
  CoreCreated(self);
}

void RevealsShroud::AddCellsToPlayerShroud(Actor& self, Player& p,
                                           const std::vector<PPos>& vec_uv) {
  // L50-56
  if (!sim::HasRelationship(reveals_info_.valid_relationships,
                            self.Owner()->RelationshipWith(&p)))
    return;

  p.GetShroud()->AddSource(this, source_type_, vec_uv);
}

void RevealsShroud::RemoveCellsFromPlayerShroud(Actor& /*self*/, Player& p) {
  // L58
  p.GetShroud()->RemoveSource(this);
}

WDist RevealsShroud::Range() const {
  // L60-70
  if (cached_trait_disabled_)
    return WDist{0};

  // OPT-A9 同形:成员缓冲回填修正链(span 直传)
  // OPT-A9's shape: the member buffer refilled with the modifier chain
  // (span-passed).
  thread_local std::vector<std::int32_t> vec_modifiers;
  vec_modifiers.clear();
  for (const auto* modifier : vec_range_modifier_ptrs_)
    vec_modifiers.push_back(modifier->GetRevealsShroudModifier());
  return WDist{ApplyPercentageModifiers(BaseInfo().dist_range.Length,
                                        vec_modifiers)};
}

// ———— [VerifySync] 哈希注册(gen/sync_gen.cpp 成员表:RevealsShroud
//      {CachedTraitDisabled})————
// ———— The [VerifySync] hash registration (the gen/sync_gen.cpp member
//      table: RevealsShroud {CachedTraitDisabled}) ————
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

// 模板显式实例化(单一 Derived;头文件消费者仅本文件)
// The explicit template instantiation (the single Derived; the header's
// consumers live in this file alone).
template class AffectsShroudBase<RevealsShroud>;

}  // namespace ora::mods
