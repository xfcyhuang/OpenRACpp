// UPSTREAM: OpenRA.Mods.Common/Traits/ProximityCapturableBase.cs @b6fc03f
//          L21-211 + ProximityCapturable.cs L18-57 + ProximityCaptor.cs
//          L17-28(实现部分;头注见 proximity_capturable.hpp)
//          The implementation halves (the header note lives in
//          proximity_capturable.hpp).
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

/// Game.RunAfterTick 的缺省承载:帧末任务近似(时点差一头 —— COVERAGE)
/// The default Game.RunAfterTick carrier: the frame-end approximation (a
/// one-step timing difference — COVERAGE).
std::function<void(std::function<void()>)>& RunAfterTickProvider() {
  static std::function<void(std::function<void()>)> fn_provider;
  return fn_provider;
}

}  // namespace

void ProximityCapturable::SetRunAfterTickProvider(
    std::function<void(std::function<void()>)> fn_provider) {
  RunAfterTickProvider() = std::move(fn_provider);
}

// ———— Info 解析面 ————

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

// ———— ProximityCapturable(基座 + 圆域具体化)————
// ———— ProximityCapturable (the base + the circle concretization) ————

ProximityCapturable::ProximityCapturable(
    const ActorInitializer& init,
    const ProximityCapturableBaseInfoData& info_base, WDist dist_range)
    : info_{info_base}, dist_range_{dist_range} {
  // L64-69
  ptr_self_ = &init.Self();
  ptr_original_owner_ = ptr_self_->Owner();
}

int ProximityCapturable::CreateTrigger(Actor& self) {
  // ProximityCapturable.cs L39
  return self.world().ActorMapFace()->AddProximityTrigger(
      self.CenterPosition(), dist_range_, WDist{0},
      [this](Actor& other) { ActorEntered(other); },
      [this](Actor& other) { ActorLeft(other); });
}

void ProximityCapturable::RemoveTrigger(Actor& /*self*/, int trigger) {
  // ProximityCapturable.cs L43
  ptr_self_->world().ActorMapFace()->RemoveProximityTrigger(trigger);
}

void ProximityCapturable::TickInner(Actor& self) {
  // ProximityCapturable.cs L48
  self.world().ActorMapFace()->UpdateProximityTrigger(
      int4_trigger_, self.CenterPosition(), dist_range_, WDist{0});
}

void ProximityCapturable::AddedToWorld(Actor& self) {
  // L76-82
  if (b_skip_trigger_update_)
    return;

  int4_trigger_ = CreateTrigger(self);
}

void ProximityCapturable::RemovedFromWorld(Actor& self) {
  // L84-91
  if (b_skip_trigger_update_)
    return;

  RemoveTrigger(self, int4_trigger_);
  vec_actors_in_range_.clear();
}

void ProximityCapturable::Tick(Actor& self) {
  // L93-100
  if (!self.IsInWorld() || self.CenterPosition() == wpos_prev_position_)
    return;

  TickInner(self);
  wpos_prev_position_ = self.CenterPosition();
}

void ProximityCapturable::ActorEntered(Actor& other) {
  // L102-109
  if (b_skip_trigger_update_ || !CanBeCapturedBy(other))
    return;

  vec_actors_in_range_.push_back(&other);
  UpdateOwnership();
}

void ProximityCapturable::ActorLeft(Actor& other) {
  // L111-118
  if (b_skip_trigger_update_ || !CanBeCapturedBy(other))
    return;

  std::erase(vec_actors_in_range_, &other);
  UpdateOwnership();
}

bool ProximityCapturable::CanBeCapturedBy(Actor& a) const {
  // L120-127:TraitInfoOrDefault<ProximityCaptorInfo> —— 具体 Info 类查询
  // (非接口;按 Info 短名过滤)
  // L120-127: TraitInfoOrDefault<ProximityCaptorInfo> — the concrete Info
  // query (not an interface; filtered by the Info short name).
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
  // L129-174
  if (Captured() && info_.b_permanent) {
    // 已永久捕获:拆触发器并阻止 AddedToWorld 重建
    // Permanently captured: drop the trigger and stop AddedToWorld from
    // recreating it.
    b_skip_trigger_update_ = true;
    RemoveTrigger(*ptr_self_, int4_trigger_);

    return;
  }

  // 在域内最久的 actor 成为 captor(邻近触发器只产生进出事件,最近者方案
  // 不可行 —— 上游注释)
  // The actor that has been in the area the longest becomes the captor
  // (proximity triggers emit enter/leave events only — the closest-one
  // scheme doesn't work; upstream's note).
  Actor* ptr_captor =
      vec_actors_in_range_.empty() ? nullptr : vec_actors_in_range_.front();

  if (ptr_captor == nullptr) {
    // 末位离场:非 Sticky 即还原原主
    // The last one left: revert to the original owner unless Sticky.
    if (Captured() && !info_.b_sticky)
      ChangeOwnership(*ptr_original_owner_->PlayerActor());
  } else {
    if (info_.b_must_be_clear) {
      bool b_is_clear = true;
      for (auto* a : vec_actors_in_range_)
        if (ptr_captor->Owner()->RelationshipWith(a->Owner()) !=
            sim::PlayerRelationship::Ally)
          b_is_clear = false;

      // 敌人进域:失守还原
      // An enemy wandered in: lost control, revert.
      if (Captured() && !b_is_clear)
        ChangeOwnership(*ptr_original_owner_->PlayerActor());
      // 尚未持有但域内已清:接管
      // Not ours yet but clear: take possession.
      else if (ptr_self_->Owner() != ptr_captor->Owner() && b_is_clear)
        ChangeOwnership(*ptr_captor);
    } else {
      // 其余情形:直接接管
      // Every other case: just take over.
      if (ptr_self_->Owner() != ptr_captor->Owner())
        ChangeOwnership(*ptr_captor);
    }
  }
}

void ProximityCapturable::ChangeOwnership(Actor& captor) {
  // L176-195
  Actor* ptr_self = ptr_self_;
  ptr_self->world().AddFrameEndTask(
      [this, ptr_self, &captor](sim::World&) {
        if (ptr_self->Disposed() || captor.Disposed())
          return;

        // 防 ChangeOwner 过程触发 (Added|Removed)FromWorld
        // Keep (Added|Removed)FromWorld from firing during ChangeOwner.
        b_skip_trigger_update_ = true;
        sim::Player* ptr_previous_owner = ptr_self->Owner();
        ptr_self->ChangeOwner(captor.Owner());

        // 本地玩家的失守闪白(FlashTarget 视觉随渲染装配批)
        // The local player's loss flash (the FlashTarget visual rides the
        // render-assembly batch).

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

void ProximityCapturable::OnOwnerChanged(Actor& /*self*/,
                                         sim::Player& /*old_owner*/,
                                         sim::Player& /*new_owner*/) {
  // L197-200
  if (auto& fn_provider = RunAfterTickProvider(); fn_provider)
    fn_provider([this]() { b_skip_trigger_update_ = false; });
  else
    // 缺省近似:帧末任务(一步早于上游的下一 tick —— COVERAGE 登记)
    // The default approximation: the frame-end queue (one step ahead of
    // upstream's next tick — registered in COVERAGE).
    ptr_self_->world().AddFrameEndTask([this](sim::World&) {
      b_skip_trigger_update_ = false;
    });
}

void ProximityCapturable::RenderAnnotations(
    Actor& self, gfx::WorldRenderer& /*wr*/,
    std::vector<gfx::RenderItem>& /*vec_out*/) {
  // L202-208:GetRenderable = RangeCircleAnnotationRenderable —— 注释圈
  // 的渲染物化随渲染装配批(空面)
  // L202-208: GetRenderable = the RangeCircle annotation — the annotation
  // materialization rides the render-assembly batch (an empty face).
  (void)self;
}

}  // namespace ora::mods
