#include "mods/proximity_capturable.hpp"

#include "game/actor_info.hpp"
#include "sim/actor_init.hpp"
#include "sim/actor_map.hpp"
#include "sim/player.hpp"
#include "sim/trait_registry.hpp"
#include "meta/field_loader.hpp"
#include "sim/world.hpp"

namespace ora::mods {

namespace {

std::function<void(std::function<void()>)>& RunAfterTickProvider() {
  static std::function<void(std::function<void()>)> fn_provider;
  return fn_provider;
}

}

void ProximityCapturable::SetRunAfterTickProvider(
    std::function<void(std::function<void()>)> fn_provider) {
  RunAfterTickProvider() = std::move(fn_provider);
}

ProximityCaptorInfoData ProximityCaptorInfoData::Parse(
    const meta::RecordObject& rec_info) {
  ProximityCaptorInfoData data;
  if (const auto v = sim::RecordFieldInt(rec_info, "Types"))
    data.bitset_types =
        core::BitSet<sim::CaptureType>::FromRawBits(static_cast<std::uint64_t>(*v));
  return data;
}

core::BitSet<sim::CaptureType>
ProximityCapturableBaseInfoData::DefaultCaptorTypes() {
  const std::vector<std::string> vec_default_tags{"Player", "Vehicle",
                                                  "Tank", "Infantry"};
  return core::BitSet<sim::CaptureType>::FromRawBits(
      meta::BitsOf("OpenRA.Mods.Common.Traits.CaptureType",
                   vec_default_tags));
}

ProximityCapturableBaseInfoData ProximityCapturableBaseInfoData::Parse(
    const meta::RecordObject& rec_info) {
  ProximityCapturableBaseInfoData data;
  data.bitset_captor_types = DefaultCaptorTypes();
  if (const auto v = sim::RecordFieldInt(rec_info, "CaptorTypes"))
    data.bitset_captor_types =
        core::BitSet<sim::CaptureType>::FromRawBits(static_cast<std::uint64_t>(*v));
  if (const auto v = sim::RecordFieldInt(rec_info, "MustBeClear"))
    data.b_must_be_clear = *v != 0;
  if (const auto v = sim::RecordFieldInt(rec_info, "Sticky"))
    data.b_sticky = *v != 0;
  if (const auto v = sim::RecordFieldInt(rec_info, "Permanent"))
    data.b_permanent = *v != 0;
  if (const auto v = sim::RecordFieldInt(rec_info, "DrawDecoration"))
    data.b_draw_decoration = *v != 0;
  return data;
}

ProximityCapturable::ProximityCapturable(
    const ActorInitializer& init,
    const ProximityCapturableBaseInfoData& info_base, WDist dist_range)
    : info_{info_base}, dist_range_{dist_range} {

  ptr_self_ = &init.Self();
  ptr_original_owner_ = ptr_self_->Owner();
}

int ProximityCapturable::CreateTrigger(Actor& self) {

  return self.world().ActorMapFace()->AddProximityTrigger(
      self.CenterPosition(), dist_range_, WDist{0},
      [this](Actor& other) { ActorEntered(other); },
      [this](Actor& other) { ActorLeft(other); });
}

void ProximityCapturable::RemoveTrigger(Actor& , int trigger) {

  ptr_self_->world().ActorMapFace()->RemoveProximityTrigger(trigger);
}

void ProximityCapturable::TickInner(Actor& self) {

  self.world().ActorMapFace()->UpdateProximityTrigger(
      int4_trigger_, self.CenterPosition(), dist_range_, WDist{0});
}

void ProximityCapturable::AddedToWorld(Actor& self) {

  if (b_skip_trigger_update_)
    return;

  int4_trigger_ = CreateTrigger(self);
}

void ProximityCapturable::RemovedFromWorld(Actor& self) {

  if (b_skip_trigger_update_)
    return;

  RemoveTrigger(self, int4_trigger_);
  vec_actors_in_range_.clear();
}

void ProximityCapturable::Tick(Actor& self) {

  if (!self.IsInWorld() || self.CenterPosition() == wpos_prev_position_)
    return;

  TickInner(self);
  wpos_prev_position_ = self.CenterPosition();
}

void ProximityCapturable::ActorEntered(Actor& other) {

  if (b_skip_trigger_update_ || !CanBeCapturedBy(other))
    return;

  vec_actors_in_range_.push_back(&other);
  UpdateOwnership();
}

void ProximityCapturable::ActorLeft(Actor& other) {

  if (b_skip_trigger_update_ || !CanBeCapturedBy(other))
    return;

  std::erase(vec_actors_in_range_, &other);
  UpdateOwnership();
}

bool ProximityCapturable::CanBeCapturedBy(Actor& a) const {

  if (&a == ptr_self_)
    return false;

  const meta::RecordObject* rec_pc = nullptr;
  if (a.Info() != nullptr)
    for (const meta::RecordObject* rec : a.Info()->TraitsInConstructOrder())
      if (rec->record_desc().str_name == "ProximityCaptorInfo") {
        rec_pc = rec;
        break;
      }
  if (rec_pc == nullptr)
    return false;

  const ProximityCaptorInfoData data_pc = ProximityCaptorInfoData::Parse(*rec_pc);
  return data_pc.bitset_types.Overlaps(info_.bitset_captor_types);
}

void ProximityCapturable::UpdateOwnership() {

  if (Captured() && info_.b_permanent) {

    b_skip_trigger_update_ = true;
    RemoveTrigger(*ptr_self_, int4_trigger_);

    return;
  }

  Actor* ptr_captor =
      vec_actors_in_range_.empty() ? nullptr : vec_actors_in_range_.front();

  if (ptr_captor == nullptr) {

    if (Captured() && !info_.b_sticky)
      ChangeOwnership(*ptr_original_owner_->PlayerActor());
  } else {
    if (info_.b_must_be_clear) {
      bool b_is_clear = true;
      for (auto* a : vec_actors_in_range_)
        if (ptr_captor->Owner()->RelationshipWith(a->Owner()) !=
            sim::PlayerRelationship::Ally)
          b_is_clear = false;

      if (Captured() && !b_is_clear)
        ChangeOwnership(*ptr_original_owner_->PlayerActor());

      else if (ptr_self_->Owner() != ptr_captor->Owner() && b_is_clear)
        ChangeOwnership(*ptr_captor);
    } else {

      if (ptr_self_->Owner() != ptr_captor->Owner())
        ChangeOwnership(*ptr_captor);
    }
  }
}

void ProximityCapturable::ChangeOwnership(Actor& captor) {

  Actor* ptr_self = ptr_self_;
  ptr_self->world().AddFrameEndTask(
      [this, ptr_self, &captor](sim::World&) {
        if (ptr_self->Disposed() || captor.Disposed())
          return;

        b_skip_trigger_update_ = true;
        sim::Player* ptr_previous_owner = ptr_self->Owner();
        ptr_self->ChangeOwner(captor.Owner());

        const meta::RecordObject* rec_pc = nullptr;
        if (captor.Info() != nullptr)
          for (const meta::RecordObject* rec :
               captor.Info()->TraitsInConstructOrder())
            if (rec->record_desc().str_name == "ProximityCaptorInfo") {
              rec_pc = rec;
              break;
            }
        const core::BitSet<sim::CaptureType> bitset_types =
            rec_pc != nullptr
                ? ProximityCaptorInfoData::Parse(*rec_pc).bitset_types
                : core::BitSet<sim::CaptureType>{};
        for (auto* t :
             ptr_self->TraitsImplementing<sim::INotifyCapture>())
          t->OnCapture(*ptr_self, captor, *ptr_previous_owner,
                       *captor.Owner(), bitset_types);
      });
}

void ProximityCapturable::OnOwnerChanged(Actor& ,
                                         sim::Player& ,
                                         sim::Player& ) {

  if (auto& fn_provider = RunAfterTickProvider(); fn_provider)
    fn_provider([this]() { b_skip_trigger_update_ = false; });
  else

    ptr_self_->world().AddFrameEndTask([this](sim::World&) {
      b_skip_trigger_update_ = false;
    });
}

void ProximityCapturable::RenderAnnotations(
    Actor& self, gfx::WorldRenderer& ,
    std::vector<gfx::RenderItem>& ) {

  (void)self;
}

}
