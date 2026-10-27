#include "mods/dock_host.hpp"

#include "meta/field_loader.hpp"
#include "meta/generic_record.hpp"
#include "mods/dock_activities.hpp"
#include "sim/sync_hash.hpp"
#include "sim/target.hpp"
#include "sim/trait_registry.hpp"
#include "sim/world.hpp"

namespace ora::mods {

DockHostInfoData DockHostInfoData::Parse(
    const meta::RecordObject& rec_info) {
  DockHostInfoData data;

  if (const auto* generated =
          dynamic_cast<const meta::GeneratedRecord*>(&rec_info)) {
    const std::vector<const meta::FieldDesc*> fields =
        meta::CollectFields(generated->record_desc());
    for (std::size_t i = 0; i < fields.size(); i++) {
      const std::string_view name = fields[i]->str_name;
      const meta::GenericValue& v = generated->Slot(i);
      if (name == "Type") {

        if (auto* n = std::get_if<std::int64_t>(&v.val)) {
          data.bitset_type =
              core::BitSet<sim::DockType>::FromRawBits(
                  static_cast<std::uint64_t>(*n));
        } else if (auto* s = std::get_if<std::string>(&v.val)) {
          const std::vector<std::string> vec_one{*s};
          data.bitset_type =
              core::BitSet<sim::DockType>::FromStringsNoAlloc(vec_one);
        } else if (auto* list =
                       std::get_if<std::vector<meta::GenericValue>>(
                           &v.val)) {
          std::vector<std::string> vec_types;
          for (const auto& element : *list)
            if (auto* s2 = std::get_if<std::string>(&element.val))
              vec_types.push_back(*s2);
          data.bitset_type =
              core::BitSet<sim::DockType>(vec_types);
        }
      } else if (name == "MaxQueueLength") {
        if (auto* n = std::get_if<std::int64_t>(&v.val))
          data.int4_max_queue_length = static_cast<int>(*n);
      } else if (name == "DockWait") {
        if (auto* n = std::get_if<std::int64_t>(&v.val))
          data.int4_dock_wait = static_cast<int>(*n);
      } else if (name == "DockAngle") {
        if (auto* n = std::get_if<std::int64_t>(&v.val))
          data.angle_dock_angle = WAngle{static_cast<std::int32_t>(*n)};
      } else if (name == "DockOffset") {
        if (auto* list = std::get_if<std::vector<meta::GenericValue>>(
                &v.val)) {
          std::vector<int> vec_xyz;
          for (const auto& element : *list)
            if (auto* n = std::get_if<std::int64_t>(&element.val))
              vec_xyz.push_back(static_cast<int>(*n));
          if (vec_xyz.size() == 3)
            data.vec_dock_offset =
                WVec{vec_xyz[0], vec_xyz[1], vec_xyz[2]};
        }
      } else if (name == "IsDragRequired") {
        if (auto* b = std::get_if<bool>(&v.val))
          data.b_is_drag_required = *b;
      } else if (name == "DragOffset") {
        if (auto* list = std::get_if<std::vector<meta::GenericValue>>(
                &v.val)) {
          std::vector<int> vec_xyz;
          for (const auto& element : *list)
            if (auto* n = std::get_if<std::int64_t>(&element.val))
              vec_xyz.push_back(static_cast<int>(*n));
          if (vec_xyz.size() == 3)
            data.vec_drag_offset =
                WVec{vec_xyz[0], vec_xyz[1], vec_xyz[2]};
        }
      } else if (name == "DragLength") {
        if (auto* n = std::get_if<std::int64_t>(&v.val))
          data.int4_drag_length = static_cast<int>(*n);
      }
    }
  }

  data.conditional = sim::ConditionalTraitData::Parse(rec_info);
  return data;
}

DockHost::DockHost(ActorInitializer& init, const DockHostInfoData& info)
    : sim::ConditionalTraitCore<DockHost>(info.conditional), info_{info} {
  p_self_ = &init.Self();
}

void DockHost::Created(Actor& self) {
  CoreCreated(self);
}

bool DockHost::IsDockingPossible(Actor& ,
                                 sim::IDockClient* client,
                                 bool ignore_reservations) {

  return !IsTraitDisabled() &&
         (ignore_reservations || CanBeReserved() ||
          std::find(vec_reserved_dock_clients_.begin(),
                    vec_reserved_dock_clients_.end(),
                    client->GetDockClientManager()) !=
              vec_reserved_dock_clients_.end());
}

bool DockHost::Reserve(Actor& self, DockClientManager* client) {

  if (CanBeReserved() &&
      std::find(vec_reserved_dock_clients_.begin(),
                vec_reserved_dock_clients_.end(),
                client) == vec_reserved_dock_clients_.end()) {
    vec_reserved_dock_clients_.push_back(client);
    client->ReserveHost(&self, this);
    return true;
  }

  return false;
}

void DockHost::UnreserveAll() {

  while (!vec_reserved_dock_clients_.empty())
    Unreserve(vec_reserved_dock_clients_[0]);
}

void DockHost::Unreserve(DockClientManager* client) {

  const auto it_remove = std::find(vec_reserved_dock_clients_.begin(),
                                   vec_reserved_dock_clients_.end(),
                                   client);
  if (it_remove != vec_reserved_dock_clients_.end()) {
    vec_reserved_dock_clients_.erase(it_remove);
    client->UnreserveHost();
  }
}

void DockHost::OnDockStarted(Actor& , Actor& client_actor,
                             DockClientManager* client) {

  p_docked_client_actor_ = &client_actor;
  p_docked_client_ = client;
}

void DockHost::OnDockCompleted(Actor& , Actor& ,
                               DockClientManager* ) {

  p_docked_client_actor_ = nullptr;
  p_docked_client_ = nullptr;
}

void DockHost::Tick(Actor& self) {

  if (p_docked_client_actor_ != nullptr &&
      (p_docked_client_actor_->IsDead() ||
       !p_docked_client_actor_->IsInWorld()))
    OnDockCompleted(self, *p_docked_client_actor_, p_docked_client_);
}

bool DockHost::QueueMoveActivity(
    sim::Activity* move_to_dock_activity, Actor& ,
    Actor& client_actor, DockClientManager* ,
    activities::MoveCooldownHelper& move_cooldown_helper) {

  sim::IMove* move = client_actor.Trait<sim::IMove>();

  const bool b_wrong_cell =
      dynamic_cast<Mobile*>(move) != nullptr
          ? client_actor.Location() !=
                client_actor.world().Map().CellContaining(DockPosition())
          : client_actor.CenterPosition() != DockPosition();
  const bool b_wrong_facing =
      dynamic_cast<sim::IFacing*>(move) == nullptr ||
      dynamic_cast<sim::IFacing*>(move)->Facing() != info_.angle_dock_angle;
  if (b_wrong_cell || b_wrong_facing) {
    move_cooldown_helper.NotifyMoveQueued();
    move_to_dock_activity->QueueChild(move->MoveOntoTarget(
        &client_actor, sim::Target::FromActor(p_self_),
        DockPosition() - p_self_->CenterPosition(), info_.angle_dock_angle,
        std::optional<core::Color>{}));
    return true;
  }

  return false;
}

void DockHost::QueueDockActivity(sim::Activity* move_to_dock_activity,
                                 Actor& , Actor& client_actor,
                                 DockClientManager* client) {

  move_to_dock_activity->QueueChild(
      activities::NewActivity<activities::GenericDockSequence>(
          client_actor, *client, *p_self_, *this, info_.int4_dock_wait,
          info_.b_is_drag_required, info_.vec_drag_offset,
          info_.int4_drag_length));
}

void DockHost::OnOwnerChanged(Actor& , Player& ,
                              Player& ) {

  UnreserveAll();
}

void DockHost::OnCapture(Actor& self, Actor& ,
                         Player& , Player& new_owner,
                         const core::BitSet<sim::CaptureType>&
                             ) {

  if (p_docked_client_actor_ != nullptr &&
      !p_docked_client_actor_->IsDead() &&
      p_docked_client_actor_->IsInWorld()) {
    p_docked_client_actor_->ChangeOwner(&new_owner);

    p_docked_client_->ReserveHost(&self, this);
  }
}

void DockHost::Selling(Actor& ) {

  b_prevent_dock = true;
}

void DockHost::Sold(Actor& ) {

  UnreserveAll();
}

void DockHost::Killed(Actor& , const sim::AttackInfo& ) {

  UnreserveAll();
}

void DockHost::Disposing(Actor& ) {

  b_prevent_dock = true;
  UnreserveAll();
}

int DockHostSyncHashImpl(const sim::ISync* s) {
  const auto* host = static_cast<const DockHost*>(s);
  int hash = sim::sync::CombineSyncHash(
      0, sim::sync::HashBool(host->b_prevent_dock));
  return sim::sync::CombineSyncHash(
      hash, sim::sync::HashActor(host->p_docked_client_actor_));
}

namespace {

const bool b_dock_host_sync_registered = [] {
  sim::RegisterSyncHashFunction("OpenRA.Mods.Common.Traits.DockHost",
                                &DockHostSyncHashImpl);
  return true;
}();
[[maybe_unused]] const bool* b_dock_host_sync_registered_anchor =
    &b_dock_host_sync_registered;

}

}
