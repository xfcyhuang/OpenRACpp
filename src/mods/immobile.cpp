// UPSTREAM: OpenRA.Mods.Common/Traits/Immobile.cs 实现部分
//          The implementation half.
#include "mods/immobile.hpp"

#include "map/map.hpp"
#include "meta/field_loader.hpp"
#include "meta/generic_record.hpp"
#include "sim/sync_hash.hpp"
#include "sim/trait_registry.hpp"
#include "sim/world.hpp"

namespace ora::mods {

ImmobileInfoData ImmobileInfoData::Parse(const meta::RecordObject& rec_info) {
  ImmobileInfoData data;

  if (const auto v = sim::RecordFieldInt(rec_info, "OccupiesSpace"))
    data.b_occupies_space = *v != 0;

  return data;
}

Immobile::Immobile(ActorInitializer& init, ImmobileInfoData&& info)
    : info_{std::move(info)} {
  pos_top_left_ = init.GetValue<sim::LocationInit>();
  pos_center_ = init.Self().world().Map().CenterOfCell(pos_top_left_);

  if (info_.b_occupies_space)
    vec_occupied_.emplace_back(pos_top_left_, sim::SubCell::FullCell);
}

void Immobile::AddedToWorld(Actor& self) {
  self.world().AddToMaps(&self, this);
}

void Immobile::RemovedFromWorld(Actor& self) {
  self.world().RemoveFromMaps(&self, this);
}

// ———— [VerifySync] 哈希注册({TopLeft, CenterPosition};第十二批 G2
//      补面)————
// ———— The [VerifySync] hash registration ({TopLeft, CenterPosition};
//      the batch-12 G2 missing face) ————

int ImmobileSyncHash(const sim::ISync* s) {
  const auto* immobile = static_cast<const Immobile*>(s);
  return sim::sync::CombineSyncHash(
      sim::sync::CombineSyncHash(0,
                                 sim::sync::HashCPos(
                                     immobile->TopLeft())),
      sim::sync::HashWPos(immobile->CenterPosition()));
}

const bool b_immobile_sync_registered = [] {
  sim::RegisterSyncHashFunction("OpenRA.Mods.Common.Traits.Immobile",
                                &ImmobileSyncHash);
  return true;
}();
[[maybe_unused]] const bool* b_immobile_sync_registered_anchor =
    &b_immobile_sync_registered;

}  // namespace ora::mods
