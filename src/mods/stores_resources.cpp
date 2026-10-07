// UPSTREAM: OpenRA.Mods.Common/Traits/StoresResources.cs 实现部分
//          + StoresPlayerResources.cs | The implementation half.
#include "mods/stores_resources.hpp"

#include "meta/field_loader.hpp"
#include "meta/generic_record.hpp"
#include "mods/player_resources.hpp"
#include "sim/sync_hash.hpp"
#include "sim/trait_registry.hpp"
#include "sim/world.hpp"

namespace ora::mods {

// ———— StoresResourcesInfoData/StoresResources(L18-106)————

StoresResourcesInfoData StoresResourcesInfoData::Parse(
    const meta::RecordObject& rec_info) {
  StoresResourcesInfoData data;

  if (const auto* generated =
          dynamic_cast<const meta::GeneratedRecord*>(&rec_info)) {
    const std::vector<const meta::FieldDesc*> fields =
        meta::CollectFields(generated->record_desc());
    for (std::size_t i = 0; i < fields.size(); i++) {
      const std::string_view name = fields[i]->str_name;
      const meta::GenericValue& v = generated->Slot(i);
      if (name == "Capacity") {
        if (auto* n = std::get_if<std::int64_t>(&v.val))
          data.int4_capacity = static_cast<int>(*n);
      } else if (name == "Resources") {
        if (auto* list = std::get_if<std::vector<meta::GenericValue>>(
                &v.val)) {
          data.vec_resources.clear();
          for (const auto& element : *list)
            if (auto* s = std::get_if<std::string>(&element.val))
              data.vec_resources.push_back(*s);
        }
      }
    }
  }

  return data;
}

StoresResources::StoresResources(ActorInitializer& /*init*/,
                                 const StoresResourcesInfoData& info)
    : info_{info} {
  // L56-64
  for (const std::string& r : info_.vec_resources)
    vec_contents_.emplace_back(r, 0);
}

int StoresResources::ContentHash() const {
  // L39-50(哈希取值域;调用侧 CombineSyncHash)
  // L39-50 (the value domain; the caller CombineSyncHash's it).
  int value = 0;
  for (const auto& [key, count] : vec_contents_)
    value += count << static_cast<int>(key.size());

  return value;
}

bool StoresResources::HasType(const std::string& resource_type) {
  // L66-69
  return std::find(info_.vec_resources.begin(), info_.vec_resources.end(),
                   resource_type) != info_.vec_resources.end();
}

int* StoresResources::Bucket(const std::string& resource_type) {
  for (auto& [key, count] : vec_contents_)
    if (key == resource_type)
      return &count;
  return nullptr;
}

const int* StoresResources::Bucket(
    const std::string& resource_type) const {
  for (const auto& [key, count] : vec_contents_)
    if (key == resource_type)
      return &count;
  return nullptr;
}

int StoresResources::AddResource(const std::string& resource_type,
                                 int value) {
  // L71-87
  if (!HasType(resource_type))
    return value;

  if (int4_contents_sum_ + value > info_.int4_capacity) {
    const int added = info_.int4_capacity - int4_contents_sum_;
    *Bucket(resource_type) += added;
    int4_contents_sum_ = info_.int4_capacity;
    return value - added;
  }

  *Bucket(resource_type) += value;
  int4_contents_sum_ += value;
  return 0;
}

int StoresResources::RemoveResource(const std::string& resource_type,
                                    int value) {
  // L89-106
  if (!HasType(resource_type))
    return value;

  const int count = *Bucket(resource_type);
  if (count < value) {
    const int leftover = value - count;
    int4_contents_sum_ -= count;
    *Bucket(resource_type) = 0;
    return leftover;
  }

  *Bucket(resource_type) -= value;
  int4_contents_sum_ -= value;
  return 0;
}

// ———— StoresPlayerResourcesInfoData/StoresPlayerResources(L14-66)————

StoresPlayerResourcesInfoData StoresPlayerResourcesInfoData::Parse(
    const meta::RecordObject& rec_info) {
  StoresPlayerResourcesInfoData data;
  if (const auto* generated =
          dynamic_cast<const meta::GeneratedRecord*>(&rec_info)) {
    const std::vector<const meta::FieldDesc*> fields =
        meta::CollectFields(generated->record_desc());
    for (std::size_t i = 0; i < fields.size(); i++) {
      if (fields[i]->str_name != "Capacity")
        continue;
      const meta::GenericValue& v = generated->Slot(i);
      if (auto* n = std::get_if<std::int64_t>(&v.val))
        data.int4_capacity = static_cast<int>(*n);
    }
  }
  return data;
}

StoresPlayerResources::StoresPlayerResources(
    ActorInitializer& init, const StoresPlayerResourcesInfoData& info)
    : info_{info} {
  // L33-37
  p_player_ = init.Self().Owner()->PlayerActor()->Trait<PlayerResources>();
}

int StoresPlayerResources::Stored() const {
  // L30
  return p_player_->ResourceCapacity == 0
             ? 0
             : static_cast<int>(static_cast<long long>(info_.int4_capacity) *
                                p_player_->Resources /
                                p_player_->ResourceCapacity);
}

void StoresPlayerResources::OnOwnerChanged(Actor& /*self*/,
                                           Player& /*old_owner*/,
                                           Player& new_owner) {
  // L39-42
  p_player_ = new_owner.PlayerActor()->Trait<PlayerResources>();
}

void StoresPlayerResources::OnCapture(
    Actor& /*self*/, Actor& /*captor*/, Player& old_owner,
    Player& new_owner, const core::BitSet<sim::CaptureType>&
        /*capture_types*/) {
  // L44-49
  const int resources = Stored();
  old_owner.PlayerActor()->Trait<PlayerResources>()->GiveResources(
      resources);
  new_owner.PlayerActor()->Trait<PlayerResources>()->GiveResources(
      resources);
}

void StoresPlayerResources::Killed(Actor& /*self*/,
                                   const sim::AttackInfo& /*e*/) {
  // L51-55
  // Lose the stored resources.(上游注释)
  p_player_->TakeResources(Stored());
}

void StoresPlayerResources::AddedToWorld(Actor& /*self*/) {
  // L57-60
  p_player_->AddStorageCapacity(info_.int4_capacity);
}

void StoresPlayerResources::RemovedFromWorld(Actor& /*self*/) {
  // L62-65
  p_player_->RemoveStorageCapacity(info_.int4_capacity);
}

// ———— [VerifySync] 哈希注册(gen/sync_gen.cpp:StoresResources
//      {ContentHash})————
// ———— The [VerifySync] hash registration (gen/sync_gen.cpp:
//      StoresResources {ContentHash}) ————

namespace {

int StoresResourcesSyncHash(const sim::ISync* s) {
  const auto* stores = static_cast<const StoresResources*>(s);
  return sim::sync::CombineSyncHash(0, stores->ContentHash());
}

const bool b_stores_resources_sync_registered = [] {
  sim::RegisterSyncHashFunction(
      "OpenRA.Mods.Common.Traits.StoresResources",
      &StoresResourcesSyncHash);
  return true;
}();
[[maybe_unused]] const bool* b_stores_resources_sync_registered_anchor =
    &b_stores_resources_sync_registered;

}  // namespace

}  // namespace ora::mods
