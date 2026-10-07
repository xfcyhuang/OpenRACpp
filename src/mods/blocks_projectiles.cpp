// UPSTREAM: OpenRA.Mods.Common/Traits/BlocksProjectiles.cs 实现部分
//          The implementation half.
#include "mods/blocks_projectiles.hpp"

#include "meta/field_loader.hpp"
#include "meta/generic_record.hpp"
#include "sim/trait_registry.hpp"
#include "sim/world.hpp"

namespace ora::mods {

BlocksProjectilesInfoData BlocksProjectilesInfoData::Parse(
    const meta::RecordObject& rec_info) {
  BlocksProjectilesInfoData data;
  if (const auto v = sim::RecordFieldInt(rec_info, "Height"))
    data.dist_height = WDist{static_cast<std::int32_t>(*v)};

  if (const auto* generated =
          dynamic_cast<const meta::GeneratedRecord*>(&rec_info)) {
    const std::vector<const meta::FieldDesc*> fields =
        meta::CollectFields(generated->record_desc());
    for (std::size_t i = 0; i < fields.size(); i++)
      if (fields[i]->str_name == "ValidRelationships") {
        const meta::GenericValue& v = generated->Slot(i);
        if (auto* n = std::get_if<std::int64_t>(&v.val))
          data.valid_relationships =
              static_cast<sim::PlayerRelationship>(
                  static_cast<std::int32_t>(*n));
      }
  }

  data.conditional = sim::ConditionalTraitData::Parse(rec_info);
  return data;
}

bool BlocksProjectiles::AnyBlockingActorAt(sim::World& world,
                                           const WPos& pos) {
  // L37-45
  const WDist dat = world.Map().DistanceAboveTerrain(pos);

  for (Actor* a :
       world.ActorMapFace()->GetActorsAt(world.Map().CellContaining(pos)))
    for (sim::IBlocksProjectiles* t :
         a->TraitsImplementing<sim::IBlocksProjectiles>()) {
      auto* base = dynamic_cast<TraitBase*>(t);
      if (t->BlockingHeight() > dat &&
          (base == nullptr || base->IsTraitEnabled()))
        return true;
    }

  return false;
}

bool BlocksProjectiles::AnyBlockingActorsBetween(
    sim::World& world, sim::Player* owner, const WPos& start,
    const WPos& end, const WDist& width, WPos& hit) {
  // L47-72
  const std::vector<Actor*> vec_actors =
      FindBlockingActorsOnLine(world, start, end, width);
  const std::int32_t length = (end - start).Length();

  for (Actor* a : vec_actors) {
    std::vector<sim::IBlocksProjectiles*> vec_blockers;
    for (sim::IBlocksProjectiles* t :
         a->TraitsImplementing<sim::IBlocksProjectiles>()) {
      auto* base = dynamic_cast<TraitBase*>(t);
      if (base != nullptr && !base->IsTraitEnabled())
        continue;
      if (!sim::HasRelationship(
              t->ValidRelationships(),
              a->Owner()->RelationshipWith(owner)))
        continue;
      vec_blockers.push_back(t);
    }

    if (vec_blockers.empty())
      continue;

    const WPos hit_pos =
        MinimumPointLineProjection(start, end, a->CenterPosition());
    const WDist dat = world.Map().DistanceAboveTerrain(hit_pos);
    if ((hit_pos - start).Length() < length) {
      for (sim::IBlocksProjectiles* t : vec_blockers)
        if (t->BlockingHeight() > dat) {
          hit = hit_pos;
          return true;
        }
    }
  }

  hit = WPos::Zero();
  return false;
}

}  // namespace ora::mods
