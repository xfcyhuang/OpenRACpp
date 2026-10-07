// UPSTREAM: OpenRA.Mods.Common/Traits/CaptureManager.cs @b6fc03f
//          L21-293 + Capturable.cs L15-50 + Captures.cs L15-173 +
//          GivesCashOnCapture.cs L14-62 + Activities/Enter.cs L11-163 +
//          Activities/CaptureActor.cs L11-135(逐语句重写;机制对照见
//          capture.hpp 头注)
//          Statement-by-statement; the mechanism mapping lives in
//          capture.hpp's header note.
#include "mods/capture.hpp"


#include "game/actor_info.hpp"
#include "mods/experience.hpp"
#include "mods/mobile.hpp"
#include "mods/player_resources.hpp"
#include "sim/frozen_actor_layer.hpp"
#include "sim/player.hpp"
#include "sim/world.hpp"

namespace ora::mods {

namespace {

const meta::RecordObject* FindTraitInfoByFullName(
    const game::ActorInfo& actor_info, std::string_view str_full_name) {
  for (const meta::RecordObject* rec_trait :
       actor_info.TraitsInConstructOrder())
    if (rec_trait->record_desc().str_full_name == str_full_name)
      return rec_trait;
  return nullptr;
}

}  // namespace

// ———— CaptureManager ————

CaptureManagerInfoData CaptureManagerInfoData::Parse(
    const meta::RecordObject& rec) {
  CaptureManagerInfoData data;
  if (const auto s = sim::RecordFieldString(rec, "CapturingCondition"))
    data.str_capturing_condition = std::string{*s};
  if (const auto s =
          sim::RecordFieldString(rec, "BeingCapturedCondition"))
    data.str_being_captured_condition = std::string{*s};
  if (const auto v = sim::RecordFieldInt(rec, "PreventsAutoTarget"))
    data.b_prevents_auto_target = *v != 0;
  return data;
}

CaptureManager::CaptureManager(sim::ActorInitializer& init,
                               CaptureManagerInfoData info)
    : info_{std::move(info)} {
  // L104-105
  ptr_self_ = &init.Self();
}

CaptureManager::~CaptureManager() = default;

void CaptureManager::Created(sim::Actor& self) {
  // L107-118:progressWatchers/move 解析 + 双 Refresh
  // L107-118: the progressWatchers/move resolution + the two Refreshes.
  ptr_self_ = &self;
  ptr_move_ = self.TraitOrDefault<sim::IMove>();
  for (auto* watcher :
       self.TraitsImplementing<sim::ICaptureProgressWatcher>())
    vec_progress_watchers_.push_back(watcher);

  RefreshCapturable();
  RefreshCaptures();
}

void CaptureManager::RefreshCapturable() {
  // L120-121
  vec_enabled_capturable_.clear();
  bitset_capturable_types_ = core::BitSet<sim::CaptureType>{};
  if (ptr_self_ == nullptr)
    return;
  for (Capturable* capturable :
       ptr_self_->TraitsImplementing<Capturable>())
    if (!capturable->IsTraitDisabled()) {
      vec_enabled_capturable_.push_back(capturable);
      bitset_capturable_types_ =
          bitset_capturable_types_.Union(capturable->InfoData().bitset_types);
    }
}

void CaptureManager::RefreshCaptures() {
  // L123-124
  vec_enabled_captures_.clear();
  bitset_ally_captures_types_ = core::BitSet<sim::CaptureType>{};
  bitset_neutral_captures_types_ = core::BitSet<sim::CaptureType>{};
  bitset_enemy_captures_types_ = core::BitSet<sim::CaptureType>{};
  if (ptr_self_ == nullptr)
    return;
  for (Captures* captures : ptr_self_->TraitsImplementing<Captures>())
    if (!captures->IsTraitDisabled()) {
      vec_enabled_captures_.push_back(captures);
      const sim::PlayerRelationship rel =
          captures->InfoData().valid_relationships;
      if (sim::HasRelationship(rel, sim::PlayerRelationship::Ally))
        bitset_ally_captures_types_ =
            bitset_ally_captures_types_.Union(
                captures->InfoData().bitset_capture_types);
      if (sim::HasRelationship(rel, sim::PlayerRelationship::Neutral))
        bitset_neutral_captures_types_ =
            bitset_neutral_captures_types_.Union(
                captures->InfoData().bitset_capture_types);
      if (sim::HasRelationship(rel, sim::PlayerRelationship::Enemy))
        bitset_enemy_captures_types_ =
            bitset_enemy_captures_types_.Union(
                captures->InfoData().bitset_capture_types);
    }
}

bool CaptureManager::CanTargetInternal(
    const sim::Player& target,
    const core::BitSet<sim::CaptureType>& capture_types) {
  // L126-147:关系三向判(敌/中立/盟的优先序 = 上游 if 链)
  // L126-147: the three-way relationship test (the enemy/neutral/ally
  // priority = upstream's if chain).
  const sim::PlayerRelationship relationship =
      ptr_self_->Owner()->RelationshipWith(&target);
  if (sim::HasRelationship(relationship, sim::PlayerRelationship::Enemy))
    return capture_types.Overlaps(bitset_enemy_captures_types_);

  if (sim::HasRelationship(relationship,
                           sim::PlayerRelationship::Neutral))
    return capture_types.Overlaps(bitset_neutral_captures_types_);

  if (sim::HasRelationship(relationship, sim::PlayerRelationship::Ally))
    return capture_types.Overlaps(bitset_ally_captures_types_);

  return false;
}

bool CaptureManager::CanTarget(const CaptureManager& target) {
  // L120 的 Manager 面
  return CanTargetInternal(*target.ptr_self_->Owner(),
                           target.bitset_capturable_types_);
}

bool CaptureManager::CanTargetFrozen(const sim::FrozenActor& target) {
  // L149-157:FrozenActorInfo 无条件缓存 → Capturable 恒启用假设
  // (上游 TODO 注释语义)
  // L149-157: FrozenActorInfo carries no condition cache → the
  // all-Capturable-enabled assumption (upstream's TODO semantics).
  const game::ActorInfo& info = target.Info();
  if (FindTraitInfoByFullName(
          info, "OpenRA.Mods.Common.Traits.CaptureManagerInfo") == nullptr)
    return false;

  core::BitSet<sim::CaptureType> target_types{};
  for (const meta::RecordObject* rec_trait :
       info.TraitsInConstructOrder())
    if (rec_trait->record_desc().str_full_name ==
        "OpenRA.Mods.Common.Traits.CapturableInfo") {
      const CapturableInfoData data = CapturableInfoData::Parse(*rec_trait);
      target_types = target_types.Union(data.bitset_types);
    }

  return CanTargetInternal(*target.Owner(), target_types);
}

Captures* CaptureManager::ValidCapturesWithLowestSabotageThreshold(
    const CaptureManager& target) {
  // L170-188:OrderBy(SabotageThreshold).ThenBy(CaptureDelay) 的稳定序
  // (插入序 = TraitsImplementing 的 ActorID 序;std::stable_sort 等价)
  // L170-188: the OrderBy(SabotageThreshold).ThenBy(CaptureDelay)
  // stable order (the insertion order = TraitsImplementing's ActorID
  // order; std::stable_sort is equivalent).
  if (target.ptr_self_->IsDead())
    return nullptr;

  const sim::PlayerRelationship relationship =
      ptr_self_->Owner()->RelationshipWith(target.ptr_self_->Owner());

  std::vector<Captures*> vec_sorted = vec_enabled_captures_;
  std::stable_sort(vec_sorted.begin(), vec_sorted.end(),
                   [](const Captures* a, const Captures* b) {
                     if (a->InfoData().int4_sabotage_threshold !=
                         b->InfoData().int4_sabotage_threshold)
                       return a->InfoData().int4_sabotage_threshold <
                              b->InfoData().int4_sabotage_threshold;
                     return a->InfoData().int4_capture_delay <
                            b->InfoData().int4_capture_delay;
                   });

  for (Captures* c : vec_sorted)
    if (sim::HasRelationship(c->InfoData().valid_relationships,
                             relationship) &&
        target.bitset_capturable_types_.Overlaps(
            c->InfoData().bitset_capture_types))
      return c;

  return nullptr;
}

bool CaptureManager::StartCapture(CaptureManager& target_manager,
                                  Captures*& captures_out) {
  // L198-249
  captures_out = nullptr;

  // 防处置期重启
  // Prevent a restart during disposal.
  if (ptr_self_->WillDispose())
    return false;

  sim::Actor* target = target_manager.ptr_self_;
  if (target != ptr_current_target_) {
    if (ptr_current_target_manager_ != nullptr)
      CancelCapture(ptr_current_target_, ptr_current_target_manager_);

    target_manager.vec_current_captors_.push_back(ptr_self_);
    ptr_current_target_ = target;
    ptr_current_target_manager_ = &target_manager;
    int4_current_target_delay_ = 0;
  } else
    int4_current_target_delay_++;

  if (int4_capturing_token_ == sim::Actor::InvalidConditionToken)
    int4_capturing_token_ =
        ptr_self_->GrantCondition(info_.str_capturing_condition);

  if (target_manager.int4_being_captured_token_ ==
      sim::Actor::InvalidConditionToken)
    target_manager.int4_being_captured_token_ = target->GrantCondition(
        target_manager.info_.str_being_captured_condition);

  captures_out = ValidCapturesWithLowestSabotageThreshold(target_manager);
  if (captures_out == nullptr)
    return false;

  // HACK:目标不得在格间移动(上游注释)
  // HACK: the target must not be between cells (upstream comment).
  Mobile* enter_mobile = target->TraitOrDefault<Mobile>();
  if (enter_mobile != nullptr && enter_mobile->IsMovingBetweenCells())
    return false;

  if (!vec_progress_watchers_.empty() ||
      !target_manager.vec_progress_watchers_.empty()) {
    int4_current_target_total_ = captures_out->InfoData().int4_capture_delay;
    if (ptr_move_ != nullptr &&
        captures_out->InfoData().b_consumed_by_capture) {
      // 目标位置集合中距自身最近者(Positions 的 ClosestToIgnoringPath)
      // The closest of the target's positions (Positions'
      // ClosestToIgnoringPath).
      WPos pos = ptr_self_->CenterPosition();
      std::int64_t best = -1;
      for (const WPos& candidate : target->GetTargetablePositions()) {
        const WVec delta = candidate - ptr_self_->CenterPosition();
        const std::int64_t dist = delta.HorizontalLengthSquared();
        if (best < 0 || dist < best) {
          best = dist;
          pos = candidate;
        }
      }
      int4_current_target_total_ +=
          ptr_move_->EstimatedMoveDuration(ptr_self_,
                                           ptr_self_->CenterPosition(), pos);
    }

    for (auto* w : vec_progress_watchers_)
      w->Update(*ptr_self_, *ptr_self_, *target,
                int4_current_target_delay_, int4_current_target_total_);

    for (auto* w : target_manager.vec_progress_watchers_)
      w->Update(*target, *ptr_self_, *target,
                int4_current_target_delay_, int4_current_target_total_);
  }

  b_entering_current_target_ =
      int4_current_target_delay_ >=
      captures_out->InfoData().int4_capture_delay;
  return b_entering_current_target_;
}

void CaptureManager::CancelCapture(sim::Actor* target,
                                   CaptureManager* target_manager) {
  // L213-249(签名 = 上游 CancelCapture(Actor, CaptureManager))
  // L213-249 (the signature = upstream's CancelCapture (Actor,
  // CaptureManager)).
  if (ptr_current_target_ == nullptr)
    return;

  for (auto* w : vec_progress_watchers_)
    w->Update(*ptr_self_, *ptr_self_, *target, 0, 0);

  if (target_manager != nullptr)
    for (auto* w : target_manager->vec_progress_watchers_)
      w->Update(*target, *ptr_self_, *target, 0, 0);

  if (int4_capturing_token_ != sim::Actor::InvalidConditionToken)
    int4_capturing_token_ =
        ptr_self_->RevokeCondition(int4_capturing_token_);

  if (target_manager != nullptr) {
    if (target_manager->int4_being_captured_token_ !=
        sim::Actor::InvalidConditionToken) {
      target_manager->int4_being_captured_token_ =
          target->RevokeCondition(
              target_manager->int4_being_captured_token_);
    }

    std::vector<sim::Actor*>& captors =
        target_manager->vec_current_captors_;
    captors.erase(std::remove(captors.begin(), captors.end(), ptr_self_),
                  captors.end());
  }

  ptr_current_target_ = nullptr;
  ptr_current_target_manager_ = nullptr;
  int4_current_target_delay_ = 0;
  b_entering_current_target_ = false;
}

void CaptureManager::OnCapture(sim::Actor& self, sim::Actor&,
                               sim::Player&, sim::Player&,
                               const core::BitSet<sim::CaptureType>&) {
  // L158-162:BeingCaptured 帧末复位
  // L158-162: the frame-end BeingCaptured reset.
  BeingCaptured = true;
  // 帧末复位(闭包捕 this 安全:帧末任务于本帧 drain,trait 仍活)
  // The frame-end reset (capturing this is safe: the task drains this
  // frame, the trait still lives).
  self.world().AddFrameEndTask([this](sim::World&) {
    BeingCaptured = false;
  });
}

void CaptureManager::Tick(sim::Actor& self) {
  // L251-264
  // 开始进入后 TryCapture 不再被调 → 自行推进观察者
  // TryCapture stops being called once entering begins → tick the
  // watchers ourselves.
  if (!b_entering_current_target_)
    return;

  if (int4_current_target_delay_ < int4_current_target_total_)
    int4_current_target_delay_++;

  for (auto* w : vec_progress_watchers_)
    w->Update(self, *ptr_self_, *ptr_current_target_,
              int4_current_target_delay_, int4_current_target_total_);

  if (ptr_current_target_manager_ != nullptr)
    for (auto* w : ptr_current_target_manager_->vec_progress_watchers_)
      w->Update(*ptr_current_target_, *ptr_self_, *ptr_current_target_,
                int4_current_target_delay_, int4_current_target_total_);
}

bool CaptureManager::DisableEnemyAutoTarget(sim::Actor&,
                                            sim::Actor& attacker) {
  // L266-268
  return info_.b_prevents_auto_target &&
         std::find(vec_current_captors_.begin(),
                   vec_current_captors_.end(),
                   &attacker) != vec_current_captors_.end();
}

// ———— Capturable ————

CapturableInfoData CapturableInfoData::Parse(const meta::RecordObject& rec) {
  CapturableInfoData data;
  if (const auto* gv = sim::RecordFieldValue(rec, "Types"))
    if (auto* bits = std::get_if<std::int64_t>(&gv->val))
      data.bitset_types = core::BitSet<sim::CaptureType>::FromRawBits(
          static_cast<std::uint64_t>(*bits));
  if (const auto v = sim::RecordFieldInt(rec, "CancelActivity"))
    data.b_cancel_activity = *v != 0;
  data.conditional = sim::ConditionalTraitData::Parse(rec);
  return data;
}

Capturable::Capturable(sim::ActorInitializer& init, CapturableInfoData info)
    : ConditionalTraitCore{info.conditional}, info_{std::move(info)} {
  // L41-42
  ptr_capture_manager_ = init.Self().Trait<CaptureManager>();
}

Capturable::~Capturable() = default;

void Capturable::OnCapture(sim::Actor& self, sim::Actor&, sim::Player&,
                           sim::Player&, const core::BitSet<sim::CaptureType>&) {
  // L44-47
  if (info_.b_cancel_activity)
    self.CancelActivity();
}

void Capturable::TraitEnabledHook(sim::Actor&) {
  // L49
  ptr_capture_manager_->RefreshCapturable();
}

void Capturable::TraitDisabledHook(sim::Actor&) {
  // L49
  ptr_capture_manager_->RefreshCapturable();
}

// ———— Captures ————

CapturesInfoData CapturesInfoData::Parse(const meta::RecordObject& rec) {
  CapturesInfoData data;
  if (const auto* gv = sim::RecordFieldValue(rec, "CaptureTypes"))
    if (auto* bits = std::get_if<std::int64_t>(&gv->val))
      data.bitset_capture_types =
          core::BitSet<sim::CaptureType>::FromRawBits(
              static_cast<std::uint64_t>(*bits));
  if (const auto v = sim::RecordFieldInt(rec, "SabotageThreshold"))
    data.int4_sabotage_threshold = static_cast<int>(*v);
  if (const auto v = sim::RecordFieldInt(rec, "SabotageHPRemoval"))
    data.int4_sabotage_hp_removal = static_cast<int>(*v);
  if (const auto* gv = sim::RecordFieldValue(rec, "SabotageDamageTypes"))
    if (auto* bits = std::get_if<std::int64_t>(&gv->val))
      data.bitset_sabotage_damage_types =
          core::BitSet<sim::DamageType>::FromRawBits(
              static_cast<std::uint64_t>(*bits));
  if (const auto v = sim::RecordFieldInt(rec, "CaptureDelay"))
    data.int4_capture_delay = static_cast<int>(*v);
  if (const auto v = sim::RecordFieldInt(rec, "ConsumedByCapture"))
    data.b_consumed_by_capture = *v != 0;
  if (const auto v = sim::RecordFieldInt(rec, "PlayerExperience"))
    data.int4_player_experience = static_cast<int>(*v);
  if (const auto* gv = sim::RecordFieldValue(rec, "ValidRelationships"))
    if (auto* bits = std::get_if<std::int64_t>(&gv->val))
      data.valid_relationships =
          static_cast<sim::PlayerRelationship>(*bits);
  if (const auto* gv =
          sim::RecordFieldValue(rec, "PlayerExperienceRelationships"))
    if (auto* bits = std::get_if<std::int64_t>(&gv->val))
      data.player_experience_relationships =
          static_cast<sim::PlayerRelationship>(*bits);
  data.conditional = sim::ConditionalTraitData::Parse(rec);
  return data;
}

Captures::Captures(sim::ActorInitializer& init, CapturesInfoData info)
    : ConditionalTraitCore{info.conditional}, info_{std::move(info)} {
  // L89-93
  ptr_capture_manager_ = init.Self().Trait<CaptureManager>();
}

Captures::~Captures() = default;

void Captures::ResolveOrder(sim::Actor& self, const net::Order& order) {
  // L143-151(ShowTargetLines = Phase 6 渲染面)
  // L143-151 (ShowTargetLines = the Phase 6 render face).
  if (order.str_order_string != "CaptureActor" || IsTraitDisabled())
    return;

  self.QueueActivity(
      order.b_queued,
      self.world().Arena().Create<CaptureActor>(
          self, order.target, info_.color_target_line));
}

void Captures::TraitEnabledHook(sim::Actor&) {
  // L153
  ptr_capture_manager_->RefreshCaptures();
}

void Captures::TraitDisabledHook(sim::Actor&) {
  // L154
  ptr_capture_manager_->RefreshCaptures();
}

// ———— GivesCashOnCapture ————

GivesCashOnCaptureInfoData GivesCashOnCaptureInfoData::Parse(
    const meta::RecordObject& rec) {
  GivesCashOnCaptureInfoData data;
  if (const auto v = sim::RecordFieldInt(rec, "Amount"))
    data.int4_amount = static_cast<int>(*v);
  if (const auto v = sim::RecordFieldInt(rec, "ShowTicks"))
    data.b_show_ticks = *v != 0;
  if (const auto* gv = sim::RecordFieldValue(rec, "CaptureTypes"))
    if (auto* bits = std::get_if<std::int64_t>(&gv->val))
      data.bitset_capture_types =
          core::BitSet<sim::CaptureType>::FromRawBits(
              static_cast<std::uint64_t>(*bits));
  if (const auto v = sim::RecordFieldInt(rec, "DisplayDuration"))
    data.int4_display_duration = static_cast<int>(*v);
  return data;
}

GivesCashOnCapture::GivesCashOnCapture(
    GivesCashOnCaptureInfoData info, sim::ConditionalTraitData conditional)
    : ConditionalTraitCore{conditional}, info_{std::move(info)} {}

GivesCashOnCapture::~GivesCashOnCapture() = default;

void GivesCashOnCapture::OnCapture(
    sim::Actor& self, sim::Actor& captor, sim::Player& old_owner,
    sim::Player& new_owner, const core::BitSet<sim::CaptureType>& capture_types) {
  // L40-58(FloatingText = Phase 6)
  // L40-58 (FloatingText = Phase 6).
  if (IsTraitDisabled())
    return;

  if (!info_.bitset_capture_types.IsEmpty() &&
      !info_.bitset_capture_types.Overlaps(capture_types))
    return;

  PlayerResources* resources =
      new_owner.PlayerActor()->Trait<PlayerResources>();
  const int amount = resources->ChangeCash(info_.int4_amount);
  if (!info_.b_show_ticks && amount != 0)
    return;

  // FloatingText 注入面省略
  // The FloatingText injection face omitted.
  (void)self;
  (void)captor;
  (void)old_owner;
}

// ———— Enter ————

Enter::Enter(sim::Actor& self, const sim::Target& target,
             std::optional<core::Color> target_line_color)
    : ptr_move_{self.Trait<sim::IMove>()},
      ptr_mobile_{self.TraitOrDefault<Mobile>()},
      opt_target_line_color_{target_line_color},
      move_cooldown_helper_{self.world(), ptr_mobile_},
      target_{target} {
  // L31-38
  b_child_has_priority_ = false;
  move_cooldown_helper_.SetRetryIfDestinationBlocked(true);
}

Enter::~Enter() = default;

bool Enter::Tick(sim::Actor& self) {
  // L52-158
  bool b_target_is_hidden_actor = false;
  target_ = target_.Recalculate(self.Owner(), b_target_is_hidden_actor);
  if (!b_target_is_hidden_actor &&
      target_.Type() == sim::TargetType::Actor)
    target_last_visible_ = sim::Target::FromTargetPositions(target_);

  b_use_last_visible_target_ =
      b_target_is_hidden_actor || !target_.IsValidFor(&self);

  // 进入中目标死亡立即取消
  // Cancel immediately if the target died while entering.
  if (!IsCanceling() && b_use_last_visible_target_ &&
      state_last_ == EnterState::Entering) {
    Cancel(self, true);
  }

  TickInner(self, target_, b_use_last_visible_target_);

  // 移动完成前不推进状态
  // The state machine waits for the movement to finish.
  if (!TickChild(self))
    return false;

  const std::optional<bool> result =
      move_cooldown_helper_.Tick(b_target_is_hidden_actor);
  if (result.has_value())
    return *result;

  switch (state_last_) {
    case EnterState::Approaching: {

      // 此时取消安全:任何进行中的移动活动已结束(上游注释)
      // Cancelling here is safe: any in-progress move has finished
      // (upstream comment).
      if (IsCanceling())
        return true;

      // 目标丢失
      // Lost track of the target.
      if (b_use_last_visible_target_ &&
          target_last_visible_.Type() == sim::TargetType::Invalid)
        return true;

      // 未邻接目标 —— 修复之(targetLineColor 不传:本 trait 自管)
      // Not next to the target — fix that (no targetLineColor: this
      // trait manages its own lines).
      if (target_.Type() != sim::TargetType::Invalid &&
          !ptr_move_->CanEnterTargetNow(&self, target_)) {
        move_cooldown_helper_.NotifyMoveQueued();
        const WPos initial_target_position =
            (b_use_last_visible_target_ ? target_last_visible_ : target_)
                .CenterPosition();
        // MoveToTarget(上游 IMove 的 Mobile 实现;邻接停)
        // MoveToTarget (upstream's IMove face via Mobile; stops when
        // adjacent).
        QueueChild(ptr_mobile_->MoveToTarget(&self, target_,
                                             initial_target_position,
                                             std::nullopt));
        return false;
      }

      // 在预想位置旁但目标不在 —— 无能为力
      // Next to where the target should be, but it isn't here.
      if (b_use_last_visible_target_ ||
          target_.Type() != sim::TargetType::Actor)
        return true;

      // 准备进入目标?
      // Ready to move into the target?
      if (TryStartEnter(self, *const_cast<sim::Actor*>(target_.ActorPtr))) {
        move_cooldown_helper_.NotifyMoveQueued();
        state_last_ = EnterState::Entering;
        QueueChild(ptr_move_->MoveIntoTarget(&self, target_));
        return false;
      }

      // TryStartEnter 中可取消;立即返回免一 tick 延迟
      // Subclasses may cancel during TryStartEnter; return at once to
      // avoid an extra tick's delay.
      if (IsCanceling())
        return true;

      return false;
    }

    case EnterState::Entering: {
      // 检查到达位置
      // Check that we reached the requested position.
      const std::vector<WPos>& vec_positions = target_.Positions();
      WPos target_pos = self.CenterPosition();
      std::int64_t best = -1;
      for (const WPos& candidate : vec_positions) {
        const WVec delta = candidate - self.CenterPosition();
        const std::int64_t dist = delta.HorizontalLengthSquared();
        if (best < 0 || dist < best) {
          best = dist;
          target_pos = candidate;
        }
      }

      if (!IsCanceling() && self.CenterPosition() == target_pos &&
          target_.Type() == sim::TargetType::Actor)
        OnEnterComplete(self, *const_cast<sim::Actor*>(target_.ActorPtr));

      state_last_ = EnterState::Exiting;
      return false;
    }

    case EnterState::Exiting: {
      move_cooldown_helper_.NotifyMoveQueued();
      QueueChild(ptr_move_->ReturnToCell(&self));
      state_last_ = EnterState::Finished;
      return false;
    }
  }

  return true;
}

// ———— CaptureActor ————

CaptureActor::CaptureActor(sim::Actor& self, const sim::Target& target,
                           std::optional<core::Color> target_line_color)
    : Enter{self, target, target_line_color} {
  // L24-29
  ptr_manager_ = self.Trait<CaptureManager>();
}

void CaptureActor::TickInner(sim::Actor& self, const sim::Target& target,
                             bool b_target_is_dead_or_hidden_actor) {
  // L34-46
  if (target.Type() == sim::TargetType::Actor &&
      ptr_enter_actor_ != target.ActorPtr) {
    ptr_enter_actor_ = const_cast<sim::Actor*>(target.ActorPtr);
    ptr_enter_capture_manager_ =
        ptr_enter_actor_->TraitOrDefault<CaptureManager>();
  }

  if (!b_target_is_dead_or_hidden_actor &&
      target.Type() != sim::TargetType::FrozenActor &&
      (ptr_enter_capture_manager_ == nullptr ||
       !ptr_manager_->CanTarget(*ptr_enter_capture_manager_)))
    Cancel(self, true);
}

bool CaptureActor::TryStartEnter(sim::Actor& self,
                                 sim::Actor& target_actor) {
  // L48-78
  if (ptr_enter_actor_ != &target_actor) {
    ptr_enter_actor_ = &target_actor;
    ptr_enter_capture_manager_ =
        target_actor.TraitOrDefault<CaptureManager>();
  }

  // 进入前再验(过早停 actor 于半路的规避 —— 上游注释)
  // Re-check before entering (avoids stopping the actor mid-nowhere —
  // upstream comment).
  if (ptr_enter_capture_manager_ == nullptr ||
      !ptr_manager_->CanTarget(*ptr_enter_capture_manager_)) {
    Cancel(self, true);
    return false;
  }

  // StartCapture 的 capture delay 为 false 时等待
  // StartCapture returning false (the capture delay) waits.
  Captures* captures = nullptr;
  if (!ptr_manager_->StartCapture(*ptr_enter_capture_manager_, captures))
    return false;

  if (!captures->InfoData().b_consumed_by_capture) {
    // 不进入不消耗 —— 立即捕获
    // No entering or disposing — capture immediately.
    DoCapture(self, *captures);
    Cancel(self, true);
    return false;
  }

  return true;
}

void CaptureActor::OnEnterComplete(sim::Actor& self,
                                   sim::Actor& target_actor) {
  // L80-104
  if (ptr_enter_actor_ != &target_actor)
    return;

  if (ptr_enter_capture_manager_->BeingCaptured ||
      !ptr_manager_->CanTarget(*ptr_enter_capture_manager_))
    return;

  // 捕获优先于破坏
  // Prioritize capturing over sabotaging.
  Captures* captures =
      ptr_manager_->ValidCapturesWithLowestSabotageThreshold(
          *ptr_enter_capture_manager_);
  if (captures == nullptr)
    return;

  DoCapture(self, *captures);
}

void CaptureActor::DoCapture(sim::Actor& self, Captures& captures) {
  // L125-133:帧末闭包的快照捕获(下游 dispose 悬垂防线)
  // L125-133: the frame-end closure's snapshot capture (the guard
  // against downstream dangling).
  const CapturesInfoData capture_info = captures.InfoData();
  sim::Actor* enter_actor = ptr_enter_actor_;
  sim::Player* old_owner = enter_actor->Owner();
  CaptureManager* enter_manager = ptr_enter_capture_manager_;
  CaptureManager* manager = ptr_manager_;
  sim::Player* captor_owner = self.Owner();
  sim::Actor* captor_self = &self;

  self.world().AddFrameEndTask(
      [enter_actor, old_owner, enter_manager, manager, captor_owner,
       captor_self, capture_info](sim::World& w) {
        (void)w;
        // 本 tick 内目标死亡或已被捕获
        // The target died or was already captured during this tick.
        if (enter_actor->IsDead() || old_owner != enter_actor->Owner())
          return;

        // 高于破坏阈值则破坏而非捕获
        // Sabotage instead of capture above the threshold.
        if (capture_info.int4_sabotage_threshold > 0 &&
            !enter_actor->Owner()->NonCombatant()) {
          const sim::IHealth* health = enter_actor->Trait<sim::IHealth>();

          // long 乘防溢出(上游显式 cast)
          // long products prevent overflow (upstream's explicit casts).
          if (100 * static_cast<std::int64_t>(health->HP()) >
              capture_info.int4_sabotage_threshold *
                  static_cast<std::int64_t>(health->MaxHP())) {
            const int damage = static_cast<int>(
                static_cast<std::int64_t>(health->MaxHP()) *
                capture_info.int4_sabotage_hp_removal / 100);
            enter_actor->Trait<sim::IHealth>()->InflictDamage(
                *enter_actor, captor_self,
                sim::Damage{damage,
                            capture_info.bitset_sabotage_damage_types},
                false);

            if (capture_info.b_consumed_by_capture)
              captor_self->Dispose();

            return;
          }
        }

        // 执行捕获
        // Do the capture.
        enter_actor->ChangeOwnerSync(captor_owner);

        for (auto* t :
             enter_actor->TraitsImplementing<sim::INotifyCapture>())
          t->OnCapture(*enter_actor, *captor_self, *old_owner,
                       *captor_owner, capture_info.bitset_capture_types);

        if (sim::HasRelationship(
                captor_owner->RelationshipWith(old_owner),
                capture_info.player_experience_relationships))
          if (PlayerExperience* player_experience =
                  captor_owner->PlayerActor()
                      ->TraitOrDefault<PlayerExperience>())
            player_experience->GiveExperience(
                capture_info.int4_player_experience);

        if (capture_info.b_consumed_by_capture)
          captor_self->Dispose();
      });

  // 快照后不再引用实时的 captures/manager 指针(帧末任务内已隔离)
  // The live captures/manager pointers are no longer referenced after
  // the snapshot (isolated inside the frame-end task).
  (void)manager;
  (void)enter_manager;
}

void CaptureActor::OnLastRun(sim::Actor& self) {
  // L106-110
  CancelCapture();
  Enter::OnLastRun(self);
}

void CaptureActor::OnActorDispose(sim::Actor& self) {
  // L112-116
  CancelCapture();
  Enter::OnActorDispose(self);
}

void CaptureActor::Cancel(sim::Actor& self, bool keep_queue) {
  // L118-123
  CancelCapture();
  Enter::Cancel(self, keep_queue);
}

}  // namespace ora::mods
