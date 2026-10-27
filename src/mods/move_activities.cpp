#include "mods/move_activities.hpp"

#include "mods/actor_exts.hpp"
#include "mods/pathfinding/path_finder.hpp"
#include "sim/world.hpp"

namespace ora::mods::activities {

bool Wait::Tick(sim::Actor& ) {

  if (IsCanceling())
    return true;

  return int4_remaining_ticks_-- == 0;
}

bool WaitFor::Tick(sim::Actor& ) {
  if (IsCanceling())
    return true;

  return fn_ == nullptr || fn_();
}

Turn::Turn(sim::Actor& self, WAngle desired_facing)
    : mobile_(self.TraitOrDefault<Mobile>()),
      facing_(self.Trait<sim::IFacing>()),
      angle_desired_facing_(desired_facing) {}

bool Turn::Tick(sim::Actor& ) {

  if (IsCanceling())
    return true;

  if (mobile_ != nullptr && (mobile_->IsTraitDisabled() ||
                             mobile_->ConditionalTraitCore<
                                 Mobile>::IsTraitPaused()))
    return false;

  if (angle_desired_facing_ == facing_->Facing())
    return true;

  facing_->SetFacing(
      TickFacing(facing_->Facing(), angle_desired_facing_,
                 facing_->TurnSpeed()));

  return false;
}

Drag::Drag(sim::Actor& self, WPos start, WPos end, int length,
           std::optional<WAngle> facing)
    : positionable_(self.Trait<sim::IPositionable>()),
      pos_start_(start),
      pos_end_(end),
      int4_length_(length),
      opt_desired_facing_(facing) {

  disableable_ = dynamic_cast<TraitBase*>(self.TraitOrDefault<sim::IMove>());
  b_is_interruptible_ = false;
}

void Drag::OnFirstRun(sim::Actor& self) {

  if (opt_desired_facing_.has_value())
    QueueChild(self.world().Arena().Create<Turn>(self, *opt_desired_facing_));
}

bool Drag::Tick(sim::Actor& self) {

  if (disableable_ != nullptr && disableable_->IsTraitDisabled())
    return false;

  const WPos pos =
      int4_length_ > 1 ? WPos::Lerp(pos_start_, pos_end_, int4_ticks_,
                                    int4_length_ - 1)
                       : pos_end_;

  positionable_->SetCenterPosition(&self, pos);
  if (++int4_ticks_ >= int4_length_)
    return true;

  return false;
}

void Nudge::OnFirstRun(sim::Actor& self) {

  auto* mobile = dynamic_cast<Mobile*>(self.Trait<sim::IMove>());
  if (mobile != nullptr) {
    if (mobile->IsTraitDisabled() ||
        mobile->ConditionalTraitCore<Mobile>::IsTraitPaused() ||
        mobile->IsImmovable)
      return;

    const std::optional<CPos> cell =
        mobile->GetAdjacentCell(p_nudger_->Location());
    if (cell.has_value())
      QueueChild(mobile->MoveTo(*cell, 0, nullptr, false,
                                mobile->Info().color_target_line));
  }
}

AttackMoveActivity::AttackMoveActivity(
    sim::Actor& self, std::function<sim::Activity*()> fn_get_move,
    bool assault_moving)
    : fn_get_move_(std::move(fn_get_move)),
      b_is_assault_move_(assault_moving) {

  b_child_has_priority_ = false;
}

void AttackMoveActivity::OnFirstRun(sim::Actor& self) {

  QueueChild(fn_get_move_());
}

bool AttackMoveActivity::Tick(sim::Actor& self) {

  return TickChild(self);
}

void AttackMoveActivity::OnLastRun(sim::Actor& ) {

}

std::optional<bool> MoveCooldownHelper::Tick(bool b_target_is_hidden_actor) {

  if (!b_was_moving_)
    return std::nullopt;

  if (!b_has_run_cooldown_) {

    if (b_target_is_hidden_actor)
      return true;

    if (p_mobile_ == nullptr ||
        p_mobile_->MoveResult == sim::MoveResult::CompleteCanceled ||
        p_mobile_->MoveResult ==
            sim::MoveResult::CompleteDestinationReached) {
      b_was_moving_ = false;
      return std::nullopt;
    }

    if (!b_retry_if_destination_blocked_ &&
        p_mobile_->MoveResult ==
            sim::MoveResult::CompleteDestinationBlocked)
      return true;

    b_has_run_cooldown_ = true;
    int4_cooldown_ticks_ =
        world_.SharedRandom().Next(int4_cooldown_min_, int4_cooldown_max_);
    return false;
  }
  if (p_mobile_ != nullptr && p_mobile_->IsBlocking) {

    return std::nullopt;
  }

  if (int4_cooldown_ticks_ > 0)
    int4_cooldown_ticks_--;

  if (int4_cooldown_ticks_ <= 0) {
    b_has_run_cooldown_ = false;
    b_was_moving_ = false;
  }

  return false;
}

MoveAdjacentTo::MoveAdjacentTo(sim::Actor& self, const sim::Target& target,
                               std::optional<WPos> initial_target_position,
                               std::optional<core::Color> target_line_color)
    : opt_target_line_color_(target_line_color),
      target_(target) {

  mobile_ = self.Trait<Mobile>();
  b_child_has_priority_ = false;

  if ((target.type == sim::TargetType::Actor &&
       target.ActorPtr->CanBeViewedByPlayer(self.Owner())) ||
      target.type == sim::TargetType::FrozenActor ||
      target.type == sim::TargetType::Terrain) {
    last_visible_target_ = sim::Target::FromPos(target.CenterPosition());
    SetVisibleTargetLocation(self, target);
  } else if (initial_target_position.has_value()) {
    last_visible_target_ = sim::Target::FromPos(*initial_target_position);
    last_visible_target_location_ =
        self.world().Map().CellContaining(*initial_target_position);
  }
}

bool MoveAdjacentTo::ShouldStop(sim::Actor& ) {

  return false;
}

bool MoveAdjacentTo::ShouldRepath(sim::Actor& ,
                                  CPos target_location) {

  return last_visible_target_location_ != target_location;
}

void MoveAdjacentTo::SetVisibleTargetLocation(sim::Actor& self,
                                              const sim::Target& target) {

  last_visible_target_location_ =
      self.world().Map().CellContaining(target.CenterPosition());
}

sim::Target MoveAdjacentTo::CurrentTarget() const {

  return b_use_last_visible_target_ ? last_visible_target_ : target_;
}

void MoveAdjacentTo::OnFirstRun(sim::Actor& self) {

  QueueChild(mobile_->MoveTo([this, &self](sim::BlockedByActor check)
                                 -> std::pair<bool, std::vector<CPos>> {
    PathEval result = CalculatePathToTarget(self, check);
    return {result.b_already_at_destination, std::move(result.vec_path)};
  }));
}

bool MoveAdjacentTo::Tick(sim::Actor& self) {

  const CPos old_target_location = last_visible_target_location_;
  bool target_is_hidden_actor = false;
  target_ = target_.Recalculate(self.Owner(), target_is_hidden_actor);
  if (!target_is_hidden_actor && target_.type == sim::TargetType::Actor) {
    last_visible_target_ = sim::Target::FromTargetPositions(target_);
    SetVisibleTargetLocation(self, target_);
  }

  const bool target_is_valid = target_.IsValidFor(&self);
  b_use_last_visible_target_ = target_is_hidden_actor || !target_is_valid;

  const bool no_target =
      b_use_last_visible_target_ && !last_visible_target_.IsValidFor(&self);

  if (ShouldStop(self) || no_target)
    Cancel(self, true);
  else if (!IsCanceling() && target_is_valid &&
           ShouldRepath(self, old_target_location)) {

    if (sim::Activity* child = ChildActivity())
      child->Cancel(self);
    QueueChild(mobile_->MoveTo([this, &self](sim::BlockedByActor check)
                                   -> std::pair<bool, std::vector<CPos>> {
      PathEval result = CalculatePathToTarget(self, check);
      return {result.b_already_at_destination, std::move(result.vec_path)};
    }));
  }

  if (!TickChild(self))
    return false;

  if (mobile_->MoveResult == sim::MoveResult::CompleteDestinationReached)
    return true;

  Cancel(self, true);
  return true;
}

MoveAdjacentTo::PathEval MoveAdjacentTo::CalculatePathToTarget(
    sim::Actor& self, sim::BlockedByActor check) {

  if (int4_search_cells_tick_ != self.world().WorldTick()) {
    vec_search_cells_.clear();
    int4_search_cells_tick_ = self.world().WorldTick();
    for (const CPos cell : AdjacentCells(self.world(), CurrentTarget())) {
      if (mobile_->CanStayInCell(cell) &&
          mobile_->CanEnterCell(cell, nullptr, sim::BlockedByActor::All)) {
        if (cell == self.Location())
          return PathEval{true, {}};

        vec_search_cells_.push_back(cell);
      }
    }
  }

  if (vec_search_cells_.empty())
    return PathEval{false, {}};

  return PathEval{
      false,
      mobile_->PathFinder()->FindPathToTargetCells(
          &self, self.Location(), vec_search_cells_, check)};
}

MoveWithinRange::MoveWithinRange(sim::Actor& self, const sim::Target& target,
                                 WDist min_range, WDist max_range,
                                 std::optional<WPos> initial_target_position,
                                 std::optional<core::Color> target_line_color)
    : MoveAdjacentTo(self, target, initial_target_position, target_line_color),
      dist_min_range_(min_range),
      dist_max_range_(max_range),
      int4_max_cells_((max_range.Length + 1023) / 1024),
      int4_min_cells_(min_range.Length / 1024) {}

bool MoveWithinRange::ShouldStop(sim::Actor& self) {

  return AtCorrectRange(self.CenterPosition()) &&
         mobile_->CanInteractWithGroundLayer(&self) &&
         mobile_->CanStayInCell(self.Location());
}

bool MoveWithinRange::ShouldRepath(sim::Actor& self, CPos target_location) {

  return last_visible_target_location_ != target_location &&
         (!AtCorrectRange(self.CenterPosition()) ||
          !mobile_->CanInteractWithGroundLayer(&self) ||
          !mobile_->CanStayInCell(self.Location()));
}

bool MoveWithinRange::AtCorrectRange(const WPos& origin) {

  return CurrentTarget().IsInRange(origin, dist_max_range_) &&
         !CurrentTarget().IsInRange(origin, dist_min_range_);
}

MoveAdjacentTo::PathEval MoveWithinRange::CalculatePathToTarget(
    sim::Actor& self, sim::BlockedByActor check) {

  if (last_visible_target_location_ == self.Location())
    return PathEval{true, {}};

  if (int4_search_cells_tick_ != self.world().WorldTick()) {
    vec_search_cells_.clear();
    int4_search_cells_tick_ = self.world().WorldTick();
    for (const CPos cell : self.world().Map().FindTilesInAnnulus(
             last_visible_target_location_, int4_min_cells_,
             int4_max_cells_)) {
      if (mobile_->CanStayInCell(cell) &&
          mobile_->CanEnterCell(cell, nullptr, sim::BlockedByActor::All) &&
          AtCorrectRange(
              self.world().Map().CenterOfSubCell(cell, mobile_->FromSubCell)))
        vec_search_cells_.push_back(cell);
    }
  }

  if (vec_search_cells_.empty())
    return PathEval{false, {}};

  return PathEval{
      false,
      mobile_->PathFinder()->FindPathToTargetCells(
          &self, self.Location(), vec_search_cells_, check)};
}

MoveOnto::MoveOnto(sim::Actor& self, const sim::Target& target,
                   std::optional<WVec> offset,
                   std::optional<WPos> initial_target_position,
                   std::optional<core::Color> target_line_color)
    : MoveAdjacentTo(self, target, initial_target_position, target_line_color),
      vec_offset_(offset.value_or(WVec{})) {}

void MoveOnto::SetVisibleTargetLocation(sim::Actor& self,
                                        const sim::Target& target) {

  last_visible_target_location_ = self.world().Map().CellContaining(
      CurrentTarget().CenterPosition() + vec_offset_);
}

bool MoveOnto::ShouldStop(sim::Actor& ) {

  return CurrentTarget().type == sim::TargetType::Terrain;
}

MoveAdjacentTo::PathEval MoveOnto::CalculatePathToTarget(
    sim::Actor& self, sim::BlockedByActor check) {

  if (last_visible_target_location_ == self.Location())
    return PathEval{true, {}};

  if (vec_search_cells_.empty())
    vec_search_cells_.push_back(last_visible_target_location_);
  else if (vec_search_cells_[0] != last_visible_target_location_)
    vec_search_cells_[0] = last_visible_target_location_;

  return PathEval{
      false,
      mobile_->PathFinder()->FindPathToTargetCells(
          &self, self.Location(), vec_search_cells_, check)};
}

MoveOntoAndTurn::MoveOntoAndTurn(sim::Actor& self,
                                 const sim::Target& target, const WVec& offset,
                                 std::optional<WAngle> desired_facing,
                                 std::optional<core::Color> target_line_color)
    : MoveOnto(self, target, offset, std::nullopt, target_line_color),
      opt_desired_facing_(desired_facing) {}

bool MoveOntoAndTurn::Tick(sim::Actor& self) {

  if (MoveOnto::Tick(self)) {
    if (!IsCanceling() && opt_desired_facing_.has_value() &&
        *opt_desired_facing_ != mobile_->Facing()) {
      QueueChild(self.world().Arena().Create<Turn>(self,
                                                   *opt_desired_facing_));
      return false;
    }

    return true;
  }

  return false;
}

Follow::Follow(sim::Actor& self, const sim::Target& target, WDist min_range,
               WDist max_range, std::optional<WPos> initial_target_position,
               std::optional<core::Color> target_line_color)
    : dist_max_range_(max_range),
      dist_min_range_(min_range),
      move_(self.Trait<sim::IMove>()),
      opt_target_line_color_(target_line_color),
      move_cooldown_helper_(self.world(), self.TraitOrDefault<Mobile>()),
      target_(target) {

  move_cooldown_helper_.SetRetryIfDestinationBlocked(true);

  if ((target.type == sim::TargetType::Actor &&
       target.ActorPtr->CanBeViewedByPlayer(self.Owner())) ||
      target.type == sim::TargetType::FrozenActor ||
      target.type == sim::TargetType::Terrain)
    last_visible_target_ = sim::Target::FromPos(target.CenterPosition());
  else if (initial_target_position.has_value())
    last_visible_target_ = sim::Target::FromPos(*initial_target_position);
}

bool Follow::Tick(sim::Actor& self) {

  if (IsCanceling())
    return true;

  bool target_is_hidden_actor = false;
  target_ = target_.Recalculate(self.Owner(), target_is_hidden_actor);
  if (!target_is_hidden_actor && target_.type == sim::TargetType::Actor)
    last_visible_target_ = sim::Target::FromTargetPositions(target_);

  b_use_last_visible_target_ =
      target_is_hidden_actor || !target_.IsValidFor(&self);

  const std::optional<bool> result =
      move_cooldown_helper_.Tick(target_is_hidden_actor);
  if (result.has_value())
    return *result;

  if (b_use_last_visible_target_ && !last_visible_target_.IsValidFor(&self))
    return true;

  const WPos pos = self.CenterPosition();
  const sim::Target& check_target =
      b_use_last_visible_target_ ? last_visible_target_ : target_;

  if (check_target.IsInRange(pos, dist_max_range_) &&
      !check_target.IsInRange(pos, dist_min_range_))
    return b_use_last_visible_target_;

  move_cooldown_helper_.NotifyMoveQueued();
  QueueChild(move_->MoveWithinRange(target_, dist_min_range_, dist_max_range_,
                                    check_target.CenterPosition(),
                                    opt_target_line_color_));
  return false;
}

LocalMoveIntoTarget::LocalMoveIntoTarget(
    sim::Actor& self, const sim::Target& target, WDist target_movement_threshold,
    std::optional<core::Color> target_line_color)
    : mobile_(self.Trait<Mobile>()),
      target_(target),
      opt_target_line_color_(target_line_color),
      dist_target_movement_threshold_(target_movement_threshold) {}

void LocalMoveIntoTarget::OnFirstRun(sim::Actor& self) {

  pos_target_start_ =
      ClosestToIgnoringPath(target_.Positions(), self.CenterPosition());
}

bool LocalMoveIntoTarget::Tick(sim::Actor& self) {

  if (IsCanceling() || target_.Type() == sim::TargetType::Invalid)
    return true;

  if (mobile_->IsTraitDisabled() ||
      mobile_->ConditionalTraitCore<Mobile>::IsTraitPaused())
    return false;

  const WPos current_pos = self.CenterPosition();
  const WPos target_pos =
      ClosestToIgnoringPath(target_.Positions(), current_pos);

  if (dist_target_movement_threshold_ > WDist{0} &&
      (target_pos - pos_target_start_).LengthSquared() >
          dist_target_movement_threshold_.LengthSquared())
    return true;

  const WVec delta = target_pos - current_pos;
  const WAngle facing =
      delta.HorizontalLengthSquared() != 0 ? delta.Yaw() : mobile_->Facing();
  if (facing != mobile_->Facing()) {
    mobile_->SetFacing(
        TickFacing(mobile_->Facing(), facing, mobile_->TurnSpeed()));
    return false;
  }

  const int speed = mobile_->MovementSpeedForCell(self.Location());
  if (delta.LengthSquared() <= static_cast<std::int64_t>(speed) * speed) {
    mobile_->SetCenterPosition(&self, target_pos);
    return true;
  }

  mobile_->SetCenterPosition(&self, current_pos + delta * speed /
                                                     delta.Length());
  return false;
}

namespace {

constexpr std::array<sim::BlockedByActor, 4> kPathSearchOrder{
    sim::BlockedByActor::All, sim::BlockedByActor::Stationary,
    sim::BlockedByActor::Immovable, sim::BlockedByActor::None};

}

Move::Move(sim::Actor& self, CPos destination,
           std::optional<core::Color> target_line_color)
    : angle_actor_facing_modifier_{},
      opt_target_line_color_(target_line_color) {

  mobile_ = static_cast<Mobile*>(self.OccupiesSpace());

  Actor* actor = &self;
  fn_get_path_ = [this, actor,
                  destination](sim::BlockedByActor check)
      -> std::pair<bool, std::vector<CPos>> {
    if (mobile_->ToCell() == destination)
      return {true, {}};

    const std::vector<CPos> sources{mobile_->ToCell()};
    return {false, mobile_->PathFinder()->FindPathToTargetCell(
                       actor, sources, destination, check, {}, nullptr,
                       false)};
  };

  opt_destination_ = destination;
  dist_near_enough_ = WDist{0};
}

Move::Move(sim::Actor& self, CPos destination, WDist near_enough,
           Actor* ignore_actor, bool evaluate_nearest_movable_cell,
           std::optional<core::Color> target_line_color)
    : angle_actor_facing_modifier_{},
      dist_near_enough_(near_enough),
      p_ignore_actor_(ignore_actor),
      opt_target_line_color_(target_line_color),
      b_evaluate_nearest_movable_cell_(evaluate_nearest_movable_cell) {

  mobile_ = static_cast<Mobile*>(self.OccupiesSpace());

  Actor* actor = &self;
  fn_get_path_ = [this, actor](sim::BlockedByActor check)
      -> std::pair<bool, std::vector<CPos>> {
    if (!opt_destination_.has_value())
      return {false, {}};

    if (mobile_->ToCell() == *opt_destination_)
      return {true, {}};

    const std::vector<CPos> sources{mobile_->ToCell()};
    return {false, mobile_->PathFinder()->FindPathToTargetCell(
                       actor, sources, *opt_destination_, check, {},
                       p_ignore_actor_, true)};
  };

  opt_destination_ = destination;
}

Move::Move(
    sim::Actor& self,
    const std::function<std::pair<bool, std::vector<CPos>>(
        sim::BlockedByActor)>& fn_get_path,
    std::optional<core::Color> target_line_color)
    : angle_actor_facing_modifier_{},
      fn_get_path_(fn_get_path),
      opt_target_line_color_(target_line_color) {

  mobile_ = static_cast<Mobile*>(self.OccupiesSpace());
  dist_near_enough_ = WDist{0};
}

std::pair<bool, std::vector<CPos>> Move::EvalPath(
    sim::BlockedByActor check) {

  auto [already_at_destination, path] = fn_get_path_(check);
  std::vector<CPos> trimmed;
  for (const CPos a : path) {
    if (a == mobile_->ToCell())
      break;
    trimmed.push_back(a);
  }
  return {already_at_destination, std::move(trimmed)};
}

class Move::MovePart : public sim::Activity {
 public:
  MovePart(Move& move, WPos from, WPos to, WAngle from_facing,
           WAngle to_facing, std::optional<WRot> from_terrain_orientation,
           std::optional<WRot> to_terrain_orientation,
           int terrain_orientation_margin, int carryover_progress,
           bool should_arc, bool moving_on_ground_layer)
      : ref_move_(move),
        pos_from_(from),
        pos_to_(to),
        angle_from_facing_(from_facing),
        angle_to_facing_(to_facing),
        opt_from_terrain_orientation_(from_terrain_orientation),
        opt_to_terrain_orientation_(to_terrain_orientation),
        int4_progress_(carryover_progress),
        int4_distance_((to - from).Length()),
        int4_terrain_orientation_margin_(std::min(
            terrain_orientation_margin, int4_distance_ / 2)),
        b_moving_on_ground_layer_(moving_on_ground_layer) {

    b_is_interruptible_ = false;

    b_turns_while_moving_ = ref_move_.mobile_->Info().b_turns_while_moving;

    if (should_arc) {

      const WVec u = WVec{1024, 0, 0}.Rotate(WRot::FromYaw(angle_from_facing_));
      const WVec v = WVec{1024, 0, 0}.Rotate(WRot::FromYaw(angle_to_facing_));

      if (v.X * u.Y != v.Y * u.X) {
        const WVec w = pos_from_ - pos_to_;
        const int s = (v.Y * w.X - v.X * w.Y) * 1024 / (v.X * u.Y - v.Y * u.X);
        const int x = pos_from_.X + s * u.X / 1024;
        const int y = pos_from_.Y + s * u.Y / 1024;

        pos_arc_center_ = WPos{x, y, 0};
        int4_arc_from_length_ = (pos_arc_center_ - pos_from_).HorizontalLength();
        angle_arc_from_angle_ = (pos_arc_center_ - pos_from_).Yaw();
        int4_arc_to_length_ = (pos_arc_center_ - pos_to_).HorizontalLength();
        angle_arc_to_angle_ = (pos_arc_center_ - pos_to_).Yaw();
        b_enable_arc_ = true;
      }
    }
  }

  bool Tick(sim::Actor& self) override;

 protected:
  virtual MovePart* OnComplete(sim::Actor& self, Mobile* mobile,
                               Move& parent) = 0;

  Move& ref_move_;
  WPos pos_from_, pos_to_;
  WAngle angle_from_facing_, angle_to_facing_;
  std::optional<WRot> opt_from_terrain_orientation_,
      opt_to_terrain_orientation_;
  bool b_enable_arc_ = false;
  WPos pos_arc_center_;
  int int4_arc_from_length_ = 0;
  WAngle angle_arc_from_angle_;
  int int4_arc_to_length_ = 0;
  WAngle angle_arc_to_angle_;
  int int4_distance_;
  bool b_moving_on_ground_layer_;
  bool b_turns_while_moving_ = false;
  int int4_terrain_orientation_margin_;
  int int4_progress_;

 private:
  friend class Move;
};

bool Move::MovePart::Tick(sim::Actor& self) {

  Mobile* mobile = ref_move_.mobile_;

  if (ref_move_.int4_last_move_part_completed_tick_ < self.world().WorldTick())
    int4_progress_ += mobile->MovementSpeedForCell(mobile->ToCell());

  if (int4_progress_ >= int4_distance_) {
    WPos to_pos = pos_to_;

    if (b_moving_on_ground_layer_)
      to_pos = to_pos - WVec{0, 0, self.world().Map()
                                         .DistanceAboveTerrain(to_pos)
                                         .Length};
    mobile->SetCenterPosition(&self, to_pos);

    mobile->SetFacing(b_turns_while_moving_
                          ? TickFacing(mobile->Facing(), angle_to_facing_,
                                       mobile->TurnSpeed())
                          : angle_to_facing_);

    ref_move_.int4_last_move_part_completed_tick_ = self.world().WorldTick();
    MovePart* next = OnComplete(self, mobile, ref_move_);
    if (next != nullptr)
      Queue(next);
    return true;
  }

  WPos pos;
  if (b_enable_arc_) {
    const WAngle angle = WAngle::Lerp(angle_arc_from_angle_,
                                      angle_arc_to_angle_, int4_progress_,
                                      int4_distance_);
    const int length = int2::Lerp(int4_arc_from_length_, int4_arc_to_length_,
                                  int4_progress_, int4_distance_);
    const int height = int2::Lerp(pos_from_.Z, pos_to_.Z, int4_progress_,
                                  int4_distance_);
    pos = pos_arc_center_ +
          WVec{0, length, height}.Rotate(WRot::FromYaw(angle));
  } else {
    pos = WPos::Lerp(pos_from_, pos_to_, int4_progress_, int4_distance_);
  }

  if (b_moving_on_ground_layer_)
    pos = pos - WVec{0, 0,
                     self.world().Map().DistanceAboveTerrain(pos).Length};

  mobile->SetCenterPosition(&self, pos);

  if (opt_from_terrain_orientation_.has_value() &&
      int4_progress_ < int4_terrain_orientation_margin_) {
    const WRot current_cell_orientation =
        self.world().Map().TerrainOrientation(mobile->FromCell());
    const WRot orientation =
        WRot::SLerp(*opt_from_terrain_orientation_, current_cell_orientation,
                    int4_progress_, int4_terrain_orientation_margin_);
    mobile->SetTerrainRampOrientation(orientation);
  } else if (opt_to_terrain_orientation_.has_value() &&
             int4_distance_ - int4_progress_ <
                 int4_terrain_orientation_margin_) {
    const WRot current_cell_orientation =
        self.world().Map().TerrainOrientation(mobile->FromCell());
    const WRot orientation =
        WRot::SLerp(*opt_to_terrain_orientation_, current_cell_orientation,
                    int4_distance_ - int4_progress_,
                    int4_terrain_orientation_margin_);
    mobile->SetTerrainRampOrientation(orientation);
  }

  mobile->SetFacing(b_turns_while_moving_
                        ? TickFacing(mobile->Facing(), angle_to_facing_,
                                     mobile->TurnSpeed())
                        : WAngle::Lerp(angle_from_facing_, angle_to_facing_,
                                       int4_progress_, int4_distance_));

  return false;
}

class Move::MoveFirstHalf final : public Move::MovePart {
 public:
  MoveFirstHalf(Move& move, WPos from, WPos to, WAngle from_facing,
                WAngle to_facing, std::optional<WRot> from_terrain_orientation,
                std::optional<WRot> to_terrain_orientation,
                int terrain_orientation_margin, int carryover_progress,
                bool should_arc, bool moving_on_ground_layer)
      : MovePart(move, from, to, from_facing, to_facing,
                 from_terrain_orientation, to_terrain_orientation,
                 terrain_orientation_margin, carryover_progress, should_arc,
                 moving_on_ground_layer) {}

 private:
  bool IsTurn(sim::Actor& self, Mobile* mobile, CPos next_cell);
  MovePart* OnComplete(sim::Actor& self, Mobile* mobile, Move& parent)
      override;
};

bool Move::MoveFirstHalf::IsTurn(sim::Actor& self, Mobile* mobile,
                                 CPos next_cell) {

  auto& map = self.world().Map();

  if (mobile->Info().b_always_turn_in_place || b_turns_while_moving_)
    return false;

  if (ref_move_.angle_actor_facing_modifier_ != WAngle{0} &&
      self.world().WorldTick() - ref_move_.int4_start_ticks_ >=
          mobile->Info().int4_backward_duration)
    return false;

  const WAngle next_facing =
      map.FacingBetween(next_cell, mobile->ToCell(), mobile->Facing());
  const WAngle current_facing =
      map.FacingBetween(mobile->ToCell(), mobile->FromCell(), mobile->Facing());
  const int delta = (next_facing - current_facing).Angle;
  return delta != 0 && (delta < 384 || delta > 640);
}

class Move::MoveSecondHalf final : public Move::MovePart {
 public:
  MoveSecondHalf(Move& move, WPos from, WPos to, WAngle from_facing,
                 WAngle to_facing, std::optional<WRot> from_terrain_orientation,
                 std::optional<WRot> to_terrain_orientation,
                 int terrain_orientation_margin, int carryover_progress,
                 bool should_arc, bool moving_on_ground_layer)
      : MovePart(move, from, to, from_facing, to_facing,
                 from_terrain_orientation, to_terrain_orientation,
                 terrain_orientation_margin, carryover_progress, should_arc,
                 moving_on_ground_layer) {}

 private:
  MovePart* OnComplete(sim::Actor& self, Mobile* mobile, Move& parent)
      override;
};

Move::MovePart* Move::MoveSecondHalf::OnComplete(sim::Actor& self,
                                                 Mobile* mobile,
                                                 Move& parent) {

  mobile->SetPosition(&self, mobile->ToCell(), mobile->ToSubCell);

  parent.int4_carryover_progress_ = int4_progress_ - int4_distance_;
  return nullptr;
}

Move::MovePart* Move::MoveFirstHalf::OnComplete(sim::Actor& self,
                                                Mobile* mobile,
                                                Move& parent) {

  auto& map = self.world().Map();
  const WVec from_subcell_offset =
      map.Grid().OffsetOfSubCell(mobile->FromSubCell);
  const WVec to_subcell_offset = map.Grid().OffsetOfSubCell(mobile->ToSubCell);

  const NextCellResult popped = parent.PopPath(self);
  if (popped.opt_next.has_value()) {
    const auto [next_cell, next_sub_cell] = *popped.opt_next;
    if (!mobile->ConditionalTraitCore<Mobile>::IsTraitPaused() &&
        !mobile->IsTraitDisabled() &&
        IsTurn(self, mobile, next_cell)) {
      const WVec next_subcell_offset =
          map.Grid().OffsetOfSubCell(next_sub_cell);
      std::optional<WRot> next_to_terrain_orientation;
      const int margin =
          mobile->Info().dist_terrain_orientation_adjustment_margin.Length;
      if (margin >= 0)
        next_to_terrain_orientation = WRot::SLerp(
            map.TerrainOrientation(mobile->ToCell()),
            map.TerrainOrientation(next_cell), 1, 2);

      auto* ret = self.world().Arena().Create<MoveFirstHalf>(
          parent,
          BetweenCells(self.world(), mobile->FromCell(), mobile->ToCell()) +
              (from_subcell_offset + to_subcell_offset) / 2,
          BetweenCells(self.world(), mobile->ToCell(), next_cell) +
              (to_subcell_offset + next_subcell_offset) / 2,
          mobile->Facing(),
          map.FacingBetween(mobile->ToCell(), next_cell, mobile->Facing()) +
              parent.angle_actor_facing_modifier_,
          opt_to_terrain_orientation_, next_to_terrain_orientation, margin,
          int4_progress_ - int4_distance_, true,
          mobile->ToCell().Layer() == 0 && next_cell.Layer() == 0);

      mobile->FinishedMoving(&self);
      mobile->SetLocation(mobile->ToCell(), mobile->ToSubCell, next_cell,
                          next_sub_cell);
      return ret;
    }

    parent.vec_path_.push_back(next_cell);
  }

  const WPos to_pos =
      mobile->ToCell().Layer() == 0
          ? map.CenterOfCell(mobile->ToCell())
          : self.world().ActorMapFace()
                ->CustomMovementLayers()[mobile->ToCell().Layer()]
                ->CenterOfCell(mobile->ToCell());

  auto* ret2 = self.world().Arena().Create<MoveSecondHalf>(
      parent,
      BetweenCells(self.world(), mobile->FromCell(), mobile->ToCell()) +
          (from_subcell_offset + to_subcell_offset) / 2,
      to_pos + to_subcell_offset, mobile->Facing(),
      b_turns_while_moving_
          ? map.FacingBetween(mobile->FromCell(), mobile->ToCell(),
                              mobile->Facing()) +
                parent.angle_actor_facing_modifier_
          : mobile->Facing(),
      opt_to_terrain_orientation_, std::nullopt,
      mobile->Info().dist_terrain_orientation_adjustment_margin.Length,
      int4_progress_ - int4_distance_, false, b_moving_on_ground_layer_);

  mobile->EnteringCell(&self);
  mobile->SetLocation(mobile->ToCell(), mobile->ToSubCell, mobile->ToCell(),
                      mobile->ToSubCell);
  return ret2;
}

void Move::OnFirstRun(sim::Actor& self) {

  int4_start_ticks_ = self.world().WorldTick();
  mobile_->MoveResult = sim::MoveResult::InProgress;

  if (b_evaluate_nearest_movable_cell_ && opt_destination_.has_value()) {
    const CPos movable_destination =
        mobile_->NearestMoveableCell(*opt_destination_);
    opt_destination_ =
        mobile_->CanEnterCell(movable_destination, nullptr,
                              sim::BlockedByActor::Immovable)
            ? std::optional<CPos>{movable_destination}
            : std::nullopt;
  }

  for (const sim::BlockedByActor check : kPathSearchOrder) {
    auto [already, path] = EvalPath(check);
    b_already_at_destination_ = already;
    vec_path_ = std::move(path);
    b_has_path_ = true;
    if (b_already_at_destination_ || !vec_path_.empty())
      return;
  }
}

bool Move::Tick(sim::Actor& self) {

  mobile_->TurnToMove = false;

  if (IsCanceling() && mobile_->CanStayInCell(mobile_->ToCell())) {
    if (b_has_path_)
      vec_path_.clear();

    mobile_->MoveResult = sim::MoveResult::CompleteCanceled;
    return true;
  }

  if (mobile_->IsTraitDisabled() ||
      mobile_->ConditionalTraitCore<Mobile>::IsTraitPaused())
    return false;

  if (b_already_at_destination_) {
    mobile_->MoveResult = sim::MoveResult::CompleteDestinationReached;
    return true;
  }

  if (opt_destination_.has_value() && *opt_destination_ == mobile_->ToCell()) {
    if (b_had_no_path_)
      mobile_->MoveResult = sim::MoveResult::CompleteDestinationBlocked;
    else
      mobile_->MoveResult = sim::MoveResult::CompleteDestinationReached;

    return true;
  }

  std::optional<std::pair<CPos, sim::SubCell>> next_cell;
  bool should_try_again = false;
  if (!vec_path_.empty() && b_has_path_) {

    opt_destination_ = vec_path_[0];
    NextCellResult popped = PopPath(self);
    next_cell = std::move(popped.opt_next);
    should_try_again = popped.b_should_try_again;
  } else if (mobile_->IsBlocking) {

    const auto unblock_destination = UnblockDestination(self);
    if (unblock_destination.has_value()) {
      next_cell = unblock_destination;
      should_try_again = true;
    }
  } else {

    b_had_no_path_ = true;
    opt_destination_ = mobile_->ToCell();
    return false;
  }

  if (!next_cell.has_value()) {
    if (!should_try_again) {
      mobile_->MoveResult = sim::MoveResult::CompleteDestinationBlocked;
      return true;
    }

    return false;
  }

  WAngle first_facing = self.world().Map().FacingBetween(
      mobile_->FromCell(), next_cell->first, mobile_->Facing());

  if (mobile_->Info().b_can_move_backward &&
      (mobile_->Info().int4_max_backward_cells < 0 ||
       static_cast<int>(vec_path_.size()) <
           mobile_->Info().int4_max_backward_cells) &&
      (mobile_->Info().int4_backward_duration < 0 ||
       self.world().WorldTick() - int4_start_ticks_ <
           mobile_->Info().int4_backward_duration) &&
      std::abs(first_facing.Angle - mobile_->Facing().Angle) > 256) {
    angle_actor_facing_modifier_ = WAngle{512};
    first_facing = first_facing + angle_actor_facing_modifier_;
  } else {
    angle_actor_facing_modifier_ = WAngle{0};
  }

  if (!mobile_->Info().b_turns_while_moving &&
      first_facing != mobile_->Facing()) {
    vec_path_.push_back(next_cell->first);
    QueueChild(self.world().Arena().Create<Turn>(self, first_facing));
    mobile_->TurnToMove = true;
    return false;
  }

  mobile_->SetLocation(mobile_->FromCell(), mobile_->FromSubCell,
                       next_cell->first, next_cell->second);

  auto& map = self.world().Map();
  const WPos from =
      (mobile_->FromCell().Layer() == 0
           ? map.CenterOfCell(mobile_->FromCell())
           : self.world().ActorMapFace()
                 ->CustomMovementLayers()[mobile_->FromCell().Layer()]
                 ->CenterOfCell(mobile_->FromCell())) +
      map.Grid().OffsetOfSubCell(mobile_->FromSubCell);

  const WPos to =
      BetweenCells(self.world(), mobile_->FromCell(), mobile_->ToCell()) +
      (map.Grid().OffsetOfSubCell(mobile_->FromSubCell) +
       map.Grid().OffsetOfSubCell(mobile_->ToSubCell)) /
          2;

  std::optional<WRot> to_terrain_orientation;
  const int margin =
      mobile_->Info().dist_terrain_orientation_adjustment_margin.Length;
  if (margin >= 0)
    to_terrain_orientation = WRot::SLerp(
        map.TerrainOrientation(mobile_->FromCell()),
        map.TerrainOrientation(mobile_->ToCell()), 1, 2);

  const bool moving_on_ground_layer =
      mobile_->FromCell().Layer() == 0 && mobile_->ToCell().Layer() == 0;
  QueueChild(self.world().Arena().Create<MoveFirstHalf>(
      *this, from, to, mobile_->Facing(), first_facing, std::nullopt,
      to_terrain_orientation, margin, int4_carryover_progress_, false,
      moving_on_ground_layer));
  int4_carryover_progress_ = 0;
  return false;
}

Move::NextCellResult Move::PopPath(sim::Actor& self) {

  if (vec_path_.empty() || !b_has_path_)
    return NextCellResult{std::nullopt, false};

  const CPos next_cell = vec_path_.back();

  if (!AreAdjacentCells(mobile_->ToCell(), next_cell)) {
    auto [already, path] = EvalPath(sim::BlockedByActor::Immovable);
    b_already_at_destination_ = already;
    vec_path_ = std::move(path);
    return NextCellResult{std::nullopt, false};
  }

  const bool contains_temporary_blocker =
      self.world().ContainsTemporaryBlocker(next_cell, &self);

  if (contains_temporary_blocker ||
      !mobile_->CanEnterCell(next_cell, p_ignore_actor_,
                             sim::BlockedByActor::All)) {

    const int cell_range = dist_near_enough_.Length / 1024;
    if (!contains_temporary_blocker &&
        (mobile_->ToCell() - *opt_destination_).LengthSquared() <=
            cell_range * cell_range &&
        mobile_->CanStayInCell(mobile_->ToCell())) {

      if (vec_path_.size() < 2) {
        vec_path_.clear();
        return NextCellResult{std::nullopt, false};
      }

      const int blocker_dist_sq =
          (next_cell - *opt_destination_).LengthSquared();
      bool nudge_or_repath = false;
      for (const CVec& d : CVec::Directions()) {
        const CPos c = next_cell + d;
        if (c != self.Location() &&
            (c - *opt_destination_).LengthSquared() <= blocker_dist_sq &&
            mobile_->CanEnterCell(c, p_ignore_actor_,
                                  sim::BlockedByActor::All)) {
          nudge_or_repath = true;
          break;
        }
      }

      if (!nudge_or_repath) {
        vec_path_.clear();
        return NextCellResult{std::nullopt, false};
      }
    }

    if (!mobile_->CanEnterCell(next_cell, p_ignore_actor_,
                                sim::BlockedByActor::Immovable)) {
      auto [already, path] = EvalPath(sim::BlockedByActor::Immovable);
      b_already_at_destination_ = already;
      vec_path_ = std::move(path);
      return NextCellResult{std::nullopt, false};
    }

    NotifyBlocker(self, next_cell);

    if (!b_has_waited_) {
      int4_wait_ticks_remaining_ =
          mobile_->Info().locomotor_info->WaitAverage();
      b_has_waited_ = true;
      return NextCellResult{std::nullopt, true};
    }

    if (--int4_wait_ticks_remaining_ >= 0)
      return NextCellResult{std::nullopt, true};

    b_has_waited_ = false;

    if (CellIsEvacuating(self, next_cell))
      return NextCellResult{std::nullopt, true};

    mobile_->RemoveInfluence();
    auto [already, new_path] = EvalPath(sim::BlockedByActor::All);
    mobile_->AddInfluence();
    b_already_at_destination_ = already;

    if (!new_path.empty()) {
      vec_path_ = std::move(new_path);
      const CPos new_cell = vec_path_.back();
      vec_path_.pop_back();

      return NextCellResult{
          std::pair<CPos, sim::SubCell>{
              new_cell,
              mobile_->GetAvailableSubCell(next_cell, mobile_->FromSubCell,
                                           p_ignore_actor_,
                                           sim::BlockedByActor::All)},
          true};
    }
    if (mobile_->IsBlocking) {

      const auto new_cell = UnblockDestination(self);
      if (new_cell.has_value())
        return NextCellResult{new_cell, true};
    }

    return NextCellResult{std::nullopt, false};
  }

  b_has_waited_ = false;
  vec_path_.pop_back();

  return NextCellResult{
      std::pair<CPos, sim::SubCell>{
          next_cell,
          mobile_->GetAvailableSubCell(next_cell, mobile_->FromSubCell,
                                       p_ignore_actor_,
                                       sim::BlockedByActor::All)},
      true};
}

std::optional<std::pair<CPos, sim::SubCell>> Move::UnblockDestination(
    sim::Actor& self) {

  const std::optional<CPos> adjacent =
      mobile_->GetAdjacentEnterableCell(self.Location());
  if (!adjacent.has_value())
    return std::nullopt;
  return std::pair<CPos, sim::SubCell>{
      *adjacent,
      mobile_->GetAvailableSubCell(*adjacent, mobile_->FromSubCell,
                                   p_ignore_actor_,
                                   sim::BlockedByActor::All)};
}

void Move::OnLastRun(sim::Actor& ) {

  vec_path_.clear();
  b_has_path_ = false;
}

bool Move::CellIsEvacuating(sim::Actor& self, CPos cell) {

  for (sim::Actor* actor : self.world().ActorMapFace()->GetActorsAt(cell)) {
    auto* move = dynamic_cast<Mobile*>(actor->OccupiesSpace());
    if (move == nullptr || move->IsTraitDisabled() || !move->IsLeaving())
      return false;
  }

  return true;
}

void Move::Cancel(sim::Actor& self, bool keep_queue) {

  Cancel(self, keep_queue, false);
}

void Move::Cancel(sim::Actor& self, bool keep_queue, bool force_clear_path) {

  if (b_has_path_ &&
      (force_clear_path || mobile_->CanStayInCell(mobile_->ToCell())))
    vec_path_.clear();

  sim::Activity::Cancel(self, keep_queue);
}

}
