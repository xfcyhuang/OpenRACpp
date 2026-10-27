#include "mods/mobile.hpp"

#include "mods/actor_exts.hpp"
#include "mods/cargo_activities.hpp"
#include "mods/parachutable.hpp"
#include "mods/pathfinding/path_finder.hpp"
#include "net/order.hpp"

#include "meta/field_loader.hpp"
#include "meta/generic_record.hpp"
#include "core/percent_modifiers.hpp"
#include "mods/move_activities.hpp"
#include "mods/util.hpp"
#include "sim/trait_registry.hpp"
#include "sim/world.hpp"

namespace ora::mods {

using pathfinding::Locomotor;
using pathfinding::LocomotorInfo;

MobileInfoData MobileInfoData::Parse(const meta::RecordObject& rec_info) {
  MobileInfoData data;

  if (const auto v = sim::RecordFieldString(rec_info, "Locomotor"))
    data.str_locomotor = std::string{*v};
  if (const auto v = sim::RecordFieldInt(rec_info, "InitialFacing"))
    data.angle_initial_facing = WAngle{static_cast<std::int32_t>(*v)};
  if (const auto v = sim::RecordFieldInt(rec_info, "TurnSpeed"))
    data.angle_turn_speed = WAngle{static_cast<std::int32_t>(*v)};
  if (const auto v = sim::RecordFieldInt(rec_info, "Speed"))
    data.int4_speed = static_cast<int>(*v);
  if (const auto v = sim::RecordFieldInt(rec_info, "AlwaysTurnInPlace"))
    data.b_always_turn_in_place = *v != 0;
  if (const auto v = sim::RecordFieldInt(rec_info, "TurnsWhileMoving"))
    data.b_turns_while_moving = *v != 0;
  if (const auto v = sim::RecordFieldString(rec_info, "Cursor"))
    data.str_cursor = std::string{*v};
  if (const auto v = sim::RecordFieldString(rec_info, "BlockedCursor"))
    data.str_blocked_cursor = std::string{*v};
  if (const auto v = sim::RecordFieldString(rec_info, "Voice"))
    data.str_voice = std::string{*v};
  if (const auto v = sim::RecordFieldInt(rec_info, "TargetLineColor"))
    data.color_target_line = core::Color::FromArgb(
        static_cast<std::uint32_t>(static_cast<std::int64_t>(*v)));
  if (const auto v = sim::RecordFieldInt(rec_info, "PreviewFacing"))
    data.angle_preview_facing = WAngle{static_cast<std::int32_t>(*v)};
  if (const auto v =
          sim::RecordFieldInt(rec_info, "EditorFacingDisplayOrder"))
    data.int4_editor_facing_display_order = static_cast<int>(*v);
  if (const auto v = sim::RecordFieldInt(rec_info, "CanMoveBackward"))
    data.b_can_move_backward = *v != 0;
  if (const auto v = sim::RecordFieldInt(rec_info, "BackwardDuration"))
    data.int4_backward_duration = static_cast<int>(*v);
  if (const auto v = sim::RecordFieldInt(rec_info, "MaxBackwardCells"))
    data.int4_max_backward_cells = static_cast<int>(*v);

  if (const auto v = sim::RecordFieldString(
          rec_info, "RequireForceMoveCondition");
      v.has_value() && !v->empty())
    data.expr_require_force_move =
        expr::BooleanExpression{std::string{*v}};
  if (const auto v =
          sim::RecordFieldString(rec_info, "ImmovableCondition");
      v.has_value() && !v->empty())
    data.expr_immovable = expr::BooleanExpression{std::string{*v}};
  if (const auto v = sim::RecordFieldInt(
          rec_info, "TerrainOrientationAdjustmentMargin"))
    data.dist_terrain_orientation_adjustment_margin =
        WDist{static_cast<std::int32_t>(*v)};

  if (const auto* generated =
          dynamic_cast<const meta::GeneratedRecord*>(&rec_info)) {
    const std::vector<const meta::FieldDesc*> fields =
        meta::CollectFields(generated->record_desc());
    for (std::size_t i = 0; i < fields.size(); i++) {
      if (fields[i]->str_name != "TerrainCursors")
        continue;
      const meta::GenericValue& v = generated->Slot(i);
      if (auto* dict = std::get_if<meta::GenericDict>(&v.val)) {
        for (const auto& [k, val] : *dict) {
          const std::string* key = std::get_if<std::string>(&k.val);
          const std::string* value = std::get_if<std::string>(&val.val);
          if (key != nullptr && value != nullptr)
            data.vec_terrain_cursors.emplace_back(*key, *value);
        }
      }
    }
  }

  data.conditional = sim::ConditionalTraitData::Parse(rec_info);
  return data;
}

namespace {

Locomotor* ResolveLocomotor(sim::World& world,
                            const std::string& str_name) {

  Locomotor* found = nullptr;
  for (auto* l : world.WorldActor()->TraitsImplementing<Locomotor>())
    if (l->Info().Name() == str_name) {
      found = l;
      break;
    }
  return found;
}

const LocomotorInfo* ResolveLocomotorInfo(sim::World& world,
                                          const std::string& str_name) {

  const LocomotorInfo* first = nullptr;
  int count = 0;
  for (auto* l : world.WorldActor()->TraitsImplementing<Locomotor>()) {
    if (l->Info().Name() == str_name) {
      if (first == nullptr)
        first = &l->Info();
      ++count;
    }
  }

  if (count == 0)
    throw yaml::YamlException(
        "A locomotor named '" + str_name + "' doesn't exist.");
  if (count > 1)
    throw yaml::YamlException(
        "There is more than one locomotor named '" + str_name + "'.");

  return first;
}

}

const LocomotorInfo* ResolveMobileLocomotorInfo(sim::World& world,
                                                const std::string& str_name) {

  return ResolveLocomotorInfo(world, str_name);
}

bool MobileInfoData::CanEnterCell(sim::World& world, Actor* self, CPos cell,
                                  sim::SubCell sub_cell, Actor* ignore_actor,
                                  sim::BlockedByActor check) const {

  Locomotor* locomotor = ResolveLocomotor(world, str_locomotor);

  return locomotor->MovementCostToEnterCell(
             self, cell, check, ignore_actor, false, sub_cell) !=
         pathfinding::kMovementCostForUnreachableCell;
}

bool MobileInfoData::CanStayInCell(sim::World& world, CPos cell) const {

  if (cell.Layer() == pathfinding::CustomMovementLayerType::kTunnel)
    return false;

  Locomotor* locomotor = ResolveLocomotor(world, str_locomotor);
  return locomotor->CanStayInCell(cell);
}

Mobile::Mobile(ActorInitializer& init, const MobileInfoData& info)
    : ConditionalTraitCore<Mobile>(info.conditional),
      info_(info),
      p_self_(&init.Self()),
      orientation_(WRot::None()) {

  ToSubCell = FromSubCell =
      info_.locomotor_info->SharesCell()
          ? static_cast<sim::SubCell>(
                init.Self().world().Map().Grid().DefaultSubCell())
          : sim::SubCell::FullCell;

  auto* sub_cell_init = init.GetOrDefault<sim::SubCellInit>();
  if (sub_cell_init != nullptr) {
    FromSubCell = ToSubCell = sub_cell_init->Value();
    b_return_to_cell_on_creation_recalculate_sub_cell = false;
  }

  auto* location_init = init.GetOrDefault<sim::LocationInit>();
  if (location_init != nullptr) {
    cell_from_ = cell_to_ = location_init->Value();
    SetCenterPosition(&init.Self(), init.Self().world().Map().CenterOfSubCell(
                                        FromCell(), FromSubCell));
  }

  SetFacing(init.GetValue<sim::FacingInit, WAngle>(info_.angle_initial_facing));
  angle_old_facing_ = Facing();

  auto* center_position_init = init.GetOrDefault<sim::CenterPositionInit>();
  if (center_position_init != nullptr) {
    pos_old_ = center_position_init->Value();
    SetCenterPosition(&init.Self(), pos_old_);
    b_return_to_cell_on_creation = true;
  }

  int4_creation_activity_delay =
      init.GetValue<sim::CreationActivityDelayInit, int>(0);
  if (auto* rally_init = init.GetOrDefault<sim::RallyPointInit>())
    vec_creation_rallypoint = rally_init->Value();
}

void Mobile::Created(Actor& self) {

  vec_notify_custom_layer_changed_ =
      self.TraitsImplementing<sim::INotifyCustomLayerChanged>();
  vec_notify_center_position_changed_ =
      self.TraitsImplementing<sim::INotifyCenterPositionChanged>();
  vec_notify_moving_ = self.TraitsImplementing<sim::INotifyMoving>();
  vec_notify_finished_moving_ =
      self.TraitsImplementing<sim::INotifyFinishedMoving>();
  vec_move_wrappers_ = self.TraitsImplementing<sim::IWrapMove>();
  p_path_finder_ = self.world().WorldActor()->Trait<pathfinding::IPathFinder>();
  p_locomotor_ = ResolveLocomotor(self.world(), info_.str_locomotor);

  CoreCreated(self);
}

std::vector<sim::VariableObserver> Mobile::GetVariableObservers() {

  std::vector<sim::VariableObserver> observers = CollectObservers();

  if (info_.expr_require_force_move.has_value()) {

    observers.push_back(sim::VariableObserver{
        [this](Actor& self, sim::ConditionCacheView conditions) {
          b_require_force_move = info_.expr_require_force_move->Evaluate(
              sim::ConditionSymbols(conditions));
          (void)self;
        },
        {}});
    observers.back().vec_variables.assign(
        info_.expr_require_force_move->Variables().begin(),
        info_.expr_require_force_move->Variables().end());
  }

  if (info_.expr_immovable.has_value()) {

    observers.push_back(sim::VariableObserver{
        [this](Actor& self, sim::ConditionCacheView conditions) {
          const bool was_immovable = IsImmovable;
          IsImmovable = info_.expr_immovable->Evaluate(
              sim::ConditionSymbols(conditions));
          if (was_immovable != IsImmovable)
            self.world().ActorMapFace()->UpdateOccupiedCells(
                self.OccupiesSpace());
        },
        {}});
    observers.back().vec_variables.assign(
        info_.expr_immovable->Variables().begin(),
        info_.expr_immovable->Variables().end());
  }

  return observers;
}

void Mobile::UpdateOccupiedCellsForCondition() {

  p_self_->world().ActorMapFace()->UpdateOccupiedCells(
      p_self_->OccupiesSpace());
}

void Mobile::SetCurrentMovementTypes(sim::MovementType value) {

  const sim::MovementType old_value = movement_types_;
  movement_types_ = value;
  if (value != old_value) {
    p_self_->world().ActorMapFace()->UpdateOccupiedCells(
        p_self_->OccupiesSpace());
    for (auto* n : vec_notify_moving_)
      n->MovementTypeChanged(*p_self_, value);
  }
}

void Mobile::UpdateMovement() {

  sim::MovementType new_movement_types = sim::MovementType::None;
  if ((pos_old_ - CenterPosition()).HorizontalLengthSquared() != 0)
    new_movement_types = new_movement_types | sim::MovementType::Horizontal;

  if (pos_old_.Z != CenterPosition().Z)
    new_movement_types = new_movement_types | sim::MovementType::Vertical;

  if (angle_old_facing_ != Facing())
    new_movement_types = new_movement_types | sim::MovementType::Turn;

  SetCurrentMovementTypes(new_movement_types);

  pos_old_ = CenterPosition();
  angle_old_facing_ = Facing();
}

void Mobile::AddedToWorld(Actor& self) {

  self.world().AddToMaps(&self, this);
}

void Mobile::RemovedFromWorld(Actor& self) {

  self.world().RemoveFromMaps(&self, this);
}

std::vector<std::pair<CPos, sim::SubCell>> Mobile::OccupiedCells() const {

  if (FromCell() == ToCell())
    return {{FromCell(), FromSubCell}};

  if (info_.locomotor_info->SharesCell())
    return {{ToCell(), ToSubCell}};

  return {{FromCell(), FromSubCell}, {ToCell(), ToSubCell}};
}

std::optional<CPos> Mobile::GetAdjacentEnterableCell(
    CPos next_cell, const std::function<bool(CPos)>& prefer_to_avoid) {

  std::vector<CPos> vec_avail_cells;
  for (const CVec& direction : CVec::Directions()) {
    const CPos p = ToCell() + direction;
    if (CanEnterCell(p, nullptr, sim::BlockedByActor::All) &&
        CanStayInCell(p) &&
        (!prefer_to_avoid || !prefer_to_avoid(p)))
      vec_avail_cells.push_back(p);
  }

  std::optional<CPos> new_cell;
  if (!vec_avail_cells.empty()) {

    auto& random = p_self_->world().SharedRandom();
    new_cell = vec_avail_cells[static_cast<std::size_t>(
        random.Next(0, static_cast<std::int32_t>(vec_avail_cells.size())))];
  }

  return new_cell;
}

std::optional<CPos> Mobile::GetAdjacentCell(
    CPos next_cell, const std::function<bool(CPos)>& prefer_to_avoid) {

  std::optional<CPos> new_cell = GetAdjacentEnterableCell(next_cell,
                                                          prefer_to_avoid);
  if (!new_cell.has_value()) {
    std::vector<CPos> vec_not_stupid_cells;
    for (const CVec& direction : CVec::Directions()) {
      const CPos p = ToCell() + direction;
      if (p != next_cell && p != ToCell())
        vec_not_stupid_cells.push_back(p);
    }

    struct CellActor {
      CPos cell;
      Actor* actor;
    };
    std::vector<CellActor> vec_pairs;
    for (const CPos c : vec_not_stupid_cells)
      for (Actor* a :
           p_self_->world().ActorMapFace()->GetActorsAt(c)) {

        if (!a->IsIdle())
          continue;
        auto* mobile = a->TraitOrDefault<Mobile>();
        if (mobile == nullptr || mobile->IsTraitDisabled() ||
            mobile->ConditionalTraitCore<Mobile>::IsTraitPaused() ||
            mobile->IsImmovable)
          continue;
        vec_pairs.push_back(CellActor{c, a});
      }

    if (!vec_pairs.empty()) {
      auto& random = p_self_->world().SharedRandom();
      new_cell =
          vec_pairs[static_cast<std::size_t>(random.Next(
                        0, static_cast<std::int32_t>(vec_pairs.size())))]
              .cell;
    }
  }

  return new_cell;
}

bool Mobile::IsLeaving() {

  if (sim::HasMovementType(movement_types_, sim::MovementType::Horizontal))
    return true;

  if (sim::HasMovementType(movement_types_, sim::MovementType::Turn))
    return TurnToMove;

  return false;
}

bool Mobile::CanInteractWithGroundLayer(Actor* self) {

  if (ToCell().Layer() == 0)
    return true;

  auto layers = self->world().ActorMapFace()->CustomMovementLayers();
  return layers[ToCell().Layer()] == nullptr ||
         layers[ToCell().Layer()]->InteractsWithDefaultLayer();
}

sim::SubCell Mobile::GetValidSubCell(sim::SubCell preferred) {

  if (preferred == sim::SubCell::Any)
    preferred = FromSubCell;

  if (info_.locomotor_info->SharesCell()) {
    if (preferred <= sim::SubCell::FullCell)
      return static_cast<sim::SubCell>(
          p_self_->world().Map().Grid().DefaultSubCell());
  } else {
    if (preferred != sim::SubCell::FullCell)
      return sim::SubCell::FullCell;
  }

  return preferred;
}

void Mobile::SetPosition(Actor* self, CPos cell, sim::SubCell sub_cell) {

  sub_cell = GetValidSubCell(sub_cell);
  SetLocation(cell, sub_cell, cell, sub_cell);

  WPos position =
      cell.Layer() == 0
          ? self->world().Map().CenterOfCell(cell)
          : self->world().ActorMapFace()->CustomMovementLayers()[cell.Layer()]
                ->CenterOfCell(cell);

  position = position +
             self->world().Map().Grid().OffsetOfSubCell(sub_cell);
  position = position -
             WVec{0, 0,
                  self->world().Map().DistanceAboveTerrain(position).Length};

  SetCenterPosition(self, position);
  FinishedMoving(self);
}

void Mobile::SetPosition(Actor* self, WPos pos) {

  const CPos cell = self->world().Map().CellContaining(pos);
  SetLocation(cell, FromSubCell, cell, FromSubCell);
  SetCenterPosition(
      self,
      self->world().Map().CenterOfSubCell(cell, FromSubCell) +
          WVec{0, 0,
               self->world().Map().DistanceAboveTerrain(pos).Length});
  FinishedMoving(self);
}

void Mobile::SetCenterPosition(Actor* self, WPos pos) {

  pos_center_ = pos;
  self->world().UpdateMaps(self, this);

  auto& map = self->world().Map();
  SetTerrainRampOrientation(
      map.TerrainOrientation(map.CellContaining(pos)));

  for (auto* n : vec_notify_center_position_changed_)
    n->CenterPositionChanged(*self, static_cast<std::uint8_t>(FromCell().Layer()),
                             static_cast<std::uint8_t>(ToCell().Layer()));
}

void Mobile::SetTerrainRampOrientation(WRot orientation) {

  if (info_.dist_terrain_orientation_adjustment_margin.Length >= 0)
    rot_terrain_ramp_ = orientation;
}

bool Mobile::IsLeavingCell(CPos location, sim::SubCell sub_cell) {

  return ToCell() != location && FromCell() == location &&
         (sub_cell == sim::SubCell::Any || FromSubCell == sub_cell ||
          sub_cell == sim::SubCell::FullCell ||
          FromSubCell == sim::SubCell::FullCell);
}

sim::SubCell Mobile::GetAvailableSubCell(CPos a, sim::SubCell preferred_sub_cell,
                                         Actor* ignore_actor,
                                         sim::BlockedByActor check) {

  return p_locomotor_->GetAvailableSubCell(p_self_, a, check,
                                           preferred_sub_cell, ignore_actor);
}

bool Mobile::CanExistInCell(CPos cell) {

  return p_locomotor_->MovementCostForCell(cell) !=
         pathfinding::kMovementCostForUnreachableCell;
}

bool Mobile::CanEnterCell(CPos cell, Actor* ignore_actor,
                          sim::BlockedByActor check) {

  return info_.CanEnterCell(p_self_->world(), p_self_, cell, ToSubCell,
                            ignore_actor, check);
}

bool Mobile::CanStayInCell(CPos cell) {

  return info_.CanStayInCell(p_self_->world(), cell);
}

void Mobile::SetLocation(CPos from, sim::SubCell from_sub, CPos to,
                         sim::SubCell to_sub) {

  if (FromCell() == from && ToCell() == to && FromSubCell == from_sub &&
      ToSubCell == to_sub)
    return;

  RemoveInfluence();
  cell_from_ = from;
  cell_to_ = to;
  FromSubCell = from_sub;
  ToSubCell = to_sub;
  AddInfluence();
  IsBlocking = false;

  if (ToCell().Layer() != FromCell().Layer())
    for (auto* n : vec_notify_custom_layer_changed_)
      n->CustomLayerChanged(*p_self_,
                            static_cast<std::uint8_t>(FromCell().Layer()),
                            static_cast<std::uint8_t>(ToCell().Layer()));
}

void Mobile::FinishedMoving(Actor* self) {

  if (FromCell().Layer() == ToCell().Layer())
    for (auto* n : vec_notify_finished_moving_)
      n->FinishedMoving(*self, static_cast<std::uint8_t>(FromCell().Layer()),
                        static_cast<std::uint8_t>(ToCell().Layer()));

  if (!IsAtGroundLevel(*self))
    return;

  CrushAction(self, &sim::INotifyCrushed::OnCrush);
}

void Mobile::CrushAction(
    Actor* self,
    void (sim::INotifyCrushed::*method)(Actor&, Actor&,
                                        const core::BitSet<sim::CrushClass>&)) {

  for (Actor* a :
       self->world().ActorMapFace()->GetActorsAt(ToCell(), ToSubCell)) {
    if (a == self)
      continue;

    for (auto* crushable : a->Crushables()) {
      if (!crushable->CrushableBy(*a, *self,
                                  info_.locomotor_info->Crushes()))
        continue;

      if (!IsAtGroundLevel(*a))
        continue;

      for (auto* notify_crushed :
           a->TraitsImplementing<sim::INotifyCrushed>())
        (notify_crushed->*method)(*a, *self, info_.locomotor_info->Crushes());
    }
  }
}

void Mobile::AddInfluence() {

  if (p_self_->IsInWorld())
    p_self_->world().ActorMapFace()->AddInfluence(p_self_, this);
}

void Mobile::RemoveInfluence() {

  if (p_self_->IsInWorld())
    p_self_->world().ActorMapFace()->RemoveInfluence(p_self_, this);
}

sim::Activity* Mobile::WrapMove(sim::Activity* inner) {

  for (sim::IWrapMove* move_wrapper : vec_move_wrappers_) {
    auto* base = dynamic_cast<TraitBase*>(move_wrapper);
    if (base != nullptr && base->IsTraitEnabled())
      return move_wrapper->WrapMove(inner);
  }
  return inner;
}

sim::Activity* Mobile::MoveTo(
    CPos cell, int near_enough, Actor* ignore_actor,
    bool evaluate_nearest_movable_cell,
    std::optional<core::Color> target_line_color) {

  return WrapMove(p_self_->world().Arena().Create<activities::Move>(
      *p_self_, cell, WDist::FromCells(near_enough), ignore_actor,
      evaluate_nearest_movable_cell, target_line_color));
}

sim::Activity* Mobile::MoveWithinRange(
    const sim::Target& target, WDist range,
    std::optional<WPos> initial_target_position,
    std::optional<core::Color> target_line_color) {

  return WrapMove(p_self_->world().Arena().Create<activities::MoveWithinRange>(
      *p_self_, target, WDist{0}, range, initial_target_position,
      target_line_color));
}

sim::Activity* Mobile::MoveWithinRange(
    const sim::Target& target, WDist min_range, WDist max_range,
    std::optional<WPos> initial_target_position,
    std::optional<core::Color> target_line_color) {

  return WrapMove(p_self_->world().Arena().Create<activities::MoveWithinRange>(
      *p_self_, target, min_range, max_range, initial_target_position,
      target_line_color));
}

sim::Activity* Mobile::MoveFollow(
    Actor* self, const sim::Target& target, WDist min_range, WDist max_range,
    std::optional<WPos> initial_target_position,
    std::optional<core::Color> target_line_color) {

  return WrapMove(self->world().Arena().Create<activities::Follow>(
      *self, target, min_range, max_range, initial_target_position,
      target_line_color));
}

sim::Activity* Mobile::ReturnToCell(Actor* self) {

  return self->world().Arena().Create<ReturnToCellActivity>(*self, 0, false);
}

sim::Activity* Mobile::MoveToTarget(
    Actor* self, const sim::Target& target,
    std::optional<WPos> ,
    std::optional<core::Color> target_line_color) {

  if (target.Type() == sim::TargetType::Invalid)
    return nullptr;

  return WrapMove(self->world().Arena().Create<activities::MoveAdjacentTo>(
      *self, target, std::nullopt, target_line_color));
}

sim::Activity* Mobile::MoveIntoTarget(Actor* self, const sim::Target& target) {

  if (target.Type() == sim::TargetType::Invalid)
    return nullptr;

  return WrapMove(self->world().Arena().Create<activities::LocalMoveIntoTarget>(
      *self, target, WDist{512}));
}

sim::Activity* Mobile::MoveOntoTarget(
    Actor* self, const sim::Target& target, const WVec& offset,
    std::optional<WAngle> facing,
    std::optional<core::Color> target_line_color) {

  return WrapMove(self->world().Arena().Create<activities::MoveOntoAndTurn>(
      *self, target, offset, facing, target_line_color));
}

sim::Activity* Mobile::LocalMove(Actor* self, WPos from_pos, WPos to_pos) {

  return WrapMove(LocalMoveImpl(self, from_pos, to_pos, self->Location()));
}

int Mobile::EstimatedMoveDuration(Actor* self, WPos from_pos, WPos to_pos) {

  const int speed = MovementSpeedForCell(self->Location());
  return speed > 0 ? (to_pos - from_pos).Length() / speed : 0;
}

CPos Mobile::NearestMoveableCell(CPos target) {

  return NearestMoveableCell(target, 1, 10);
}

bool Mobile::CanEnterTargetNow(Actor* self, const sim::Target& target) {

  if (target.Type() == sim::TargetType::FrozenActor)
    return false;

  if (self->Location() ==
      self->world().Map().CellContaining(target.CenterPosition()))
    return true;

  for (const CPos c : AdjacentCells(self->world(), target))
    if (c == self->Location())
      return true;

  return false;
}

int Mobile::MovementSpeedForCell(CPos cell) {

  if (!b_speed_modifiers_resolved_) {
    vec_speed_modifiers_.clear();
    for (auto* modifier : p_self_->TraitsImplementing<sim::ISpeedModifier>())
      vec_speed_modifiers_.push_back(modifier->GetSpeedModifier());
    b_speed_modifiers_resolved_ = true;
  }

  const int terrain_speed = p_locomotor_->MovementSpeedForCell(cell);

  vec_speed_modifiers_.push_back(terrain_speed);
  const int result = ApplyPercentageModifiers(
      info_.int4_speed, std::span<const int>{vec_speed_modifiers_});
  vec_speed_modifiers_.pop_back();
  return result;
}

CPos Mobile::NearestMoveableCell(CPos target, int min_range, int max_range) {

  if (target.Layer() != 0)
    target = CPos{target.X(), target.Y()};

  if (target == p_self_->Location() && CanStayInCell(target))
    return target;

  if (CanEnterCell(target, nullptr, sim::BlockedByActor::Immovable) &&
      CanStayInCell(target))
    return target;

  for (const CPos tile :
       p_self_->world().Map().FindTilesInAnnulus(target, min_range, max_range))
    if (CanEnterCell(tile, nullptr, sim::BlockedByActor::Immovable) &&
        CanStayInCell(tile))
      return tile;

  return target;
}

CPos Mobile::NearestCell(CPos target,
                         const std::function<bool(CPos)>& check,
                         int min_range, int max_range) {

  if (check(target))
    return target;

  for (const CPos tile :
       p_self_->world().Map().FindTilesInAnnulus(target, min_range, max_range))
    if (check(tile))
      return tile;

  return target;
}

void Mobile::EnteringCell(Actor* self) {

  if (!IsAtGroundLevel(*self))
    return;

  CrushAction(self, &sim::INotifyCrushed::WarnCrush);
}

sim::Activity* Mobile::MoveTo(
    const std::function<std::pair<bool, std::vector<CPos>>(
        sim::BlockedByActor)>& path_func) {

  return p_self_->world().Arena().Create<activities::Move>(*p_self_,
                                                           path_func);
}

sim::Activity* Mobile::LocalMoveImpl(Actor* self, WPos from_pos, WPos to_pos,
                                     CPos cell) {

  const int speed = MovementSpeedForCell(cell);
  const int length = speed > 0 ? (to_pos - from_pos).Length() / speed : 0;

  const WVec delta = to_pos - from_pos;
  const WAngle facing =
      delta.HorizontalLengthSquared() != 0 ? delta.Yaw() : Facing();

  return self->world().Arena().Create<activities::Drag>(*self, from_pos,
                                                        to_pos, length, facing);
}

std::optional<CPos> Mobile::ClosestGroundCell() {

  const CPos above{TopLeft().X(), TopLeft().Y()};
  if (CanEnterCell(above, nullptr, sim::BlockedByActor::All))
    return above;

  const std::vector<CPos> sources{p_self_->Location()};
  const std::vector<CPos> path =
      p_path_finder_->FindPathToTargetCellByPredicate(
          p_self_, sources,
          [this](CPos loc) {
            return loc.Layer() == 0 &&
                   CanEnterCell(loc, nullptr, sim::BlockedByActor::All);
          },
          sim::BlockedByActor::All);

  if (!path.empty())
    return path[0];

  return std::nullopt;
}

void Mobile::ModifyActorPreviewInit(sim::Actor& self,
                                    sim::TypeDictionary& inits) {

  (void)self;
  if (!inits.Contains<sim::DynamicFacingInit>() &&
      !inits.Contains<sim::FacingInit>())
    inits.Add(p_self_->world().Arena().Create<sim::DynamicFacingInit>(
        [this]() { return Facing(); }));
}

void Mobile::ModifyDeathActorInit(sim::Actor& self,
                                  sim::TypeDictionary& init) {

  init.Add(self.world().Arena().Create<sim::FacingInit>(Facing()));

  if (CanEnterCell(self.Location(), &self, sim::BlockedByActor::Stationary))
    init.Add(self.world().Arena().Create<sim::HuskSpeedInit>(
        MovementSpeedForCell(self.Location())));
}

void Mobile::OnBecomingIdle(Actor& self) {

  if (self.Location().Layer() == 0) {

    if (!p_locomotor_->CanStayInCell(self.Location()))
      self.QueueActivity(
          MoveTo(self.Location(), 0, nullptr, true, std::nullopt));
    return;
  }

  auto layers = self.world().ActorMapFace()->CustomMovementLayers();
  if (!layers[self.Location().Layer()]->ReturnToGroundLayerOnIdle())
    return;

  const std::optional<CPos> move_to = ClosestGroundCell();
  if (move_to.has_value())
    self.QueueActivity(MoveTo(*move_to, 0, nullptr, false, std::nullopt));
}

void Mobile::OnNotifyBlockingMove(Actor& self, Actor& blocking) {

  if (!AppearsFriendlyTo(self, blocking))
    return;

  if (self.IsIdle()) {
    self.QueueActivity(
        false, self.world().Arena().Create<activities::Nudge>(&blocking));
    return;
  }

  IsBlocking = true;
}

std::vector<sim::IOrderTargeter*> Mobile::Orders() {

  std::vector<sim::IOrderTargeter*> orders;
  if (!IsTraitDisabled())
    orders.push_back(
        p_self_->world().Arena().Create<MoveOrderTargeter>(*p_self_, this));
  return orders;
}

std::unique_ptr<net::Order> Mobile::IssueOrder(
    Actor& self, sim::IOrderTargeter* order,
    const sim::Target& target, bool queued) {

  (void)self;
  if (dynamic_cast<MoveOrderTargeter*>(order) != nullptr)
    return std::make_unique<net::Order>("Move", p_self_, target, queued);

  return nullptr;
}

void Mobile::ResolveOrder(Actor& self, const net::Order& order) {

  if (IsTraitDisabled())
    return;

  if (order.str_order_string == "Move") {
    if (!order.target.IsValidFor(&self))
      return;

    const CPos cell = self.world().Map().Clamp(
        self.world().Map().CellContaining(order.target.CenterPosition()));

    if (!info_.locomotor_info->MoveIntoShroud())
      return;

    self.QueueActivity(order.b_queued,
                       WrapMove(self.world().Arena().Create<activities::Move>(
                           self, cell, WDist::FromCells(8), nullptr, true,
                           info_.color_target_line)));

  }

  else if (order.str_order_string == "Stop")
    self.CancelActivity();
  else if (order.str_order_string == "Scatter") {
    self.QueueActivity(order.b_queued,
                       self.world().Arena().Create<activities::Nudge>(&self));

  }
}

std::string Mobile::VoicePhraseForOrder(Actor& self, const net::Order& order) {

  (void)self;
  if (IsTraitDisabled())
    return {};

  if (order.str_order_string == "Move")
    return info_.str_voice;
  if (order.str_order_string == "Scatter" || order.str_order_string == "Stop")
    return info_.str_voice;

  return {};
}

Mobile::ReturnToCellActivity::ReturnToCellActivity(sim::Actor& self,
                                                   int delay,
                                                   bool recalculate_sub_cell)
    : mobile_(self.Trait<Mobile>()),
      b_recalculate_sub_cell_(recalculate_sub_cell),
      int4_delay_(delay) {

  b_is_interruptible_ = false;
}

void Mobile::ReturnToCellActivity::OnFirstRun(sim::Actor& self) {

  pos_ = self.CenterPosition();
  if (self.world().Map().DistanceAboveTerrain(pos_) > WDist{0} &&
      self.TraitOrDefault<Parachutable>() != nullptr)
    QueueChild(self.world().Arena().Create<activities::Parachute>(self));
}

bool Mobile::ReturnToCellActivity::Tick(sim::Actor& self) {

  pos_ = self.CenterPosition();
  cell_ = mobile_->ToCell();
  sub_cell_ = mobile_->ToSubCell;

  if (b_recalculate_sub_cell_)
    sub_cell_ =
        mobile_->Info().locomotor_info->SharesCell()
            ? self.world().ActorMapFace()->FreeSubCell(
                  cell_, sub_cell_, [&self](Actor& a) { return &a != &self; })
            : sim::SubCell::FullCell;

  if (sub_cell_ == sim::SubCell::Invalid)
    sub_cell_ = static_cast<sim::SubCell>(
        self.world().Map().Grid().DefaultSubCell());

  mobile_->SetPosition(&self, cell_, sub_cell_);
  mobile_->SetCenterPosition(&self, pos_);

  if (int4_delay_ > 0)
    QueueChild(self.world().Arena().Create<activities::Wait>(int4_delay_));

  QueueChild(mobile_->LocalMove(
      &self, pos_, self.world().Map().CenterOfSubCell(cell_, sub_cell_)));
  return true;
}

Mobile::LeaveProductionActivity::LeaveProductionActivity(
    sim::Actor& self, int delay, std::vector<CPos> rally_point,
    Mobile::ReturnToCellActivity* return_to_cell)
    : mobile_(self.Trait<Mobile>()),
      int4_delay_(delay),
      vec_rally_point_(std::move(rally_point)),
      p_return_to_cell_(return_to_cell) {}

void Mobile::LeaveProductionActivity::OnFirstRun(sim::Actor& self) {

  if (p_return_to_cell_ != nullptr)
    QueueChild(p_return_to_cell_);
  else if (int4_delay_ > 0)
    QueueChild(self.world().Arena().Create<activities::Wait>(int4_delay_));

  if (!vec_rally_point_.empty())
    for (const CPos cell : vec_rally_point_)
      QueueChild(self.world().Arena().Create<activities::AttackMoveActivity>(
          self,
          [this, cell]() {
            return mobile_->MoveTo(cell, 1, nullptr, true,
                                   core::Color::FromArgb(255, 69, 0));
          },
          false));
}

Mobile::MoveOrderTargeter::MoveOrderTargeter(sim::Actor& self, Mobile* unit)
    : mobile_(unit),
      locomotor_info_(unit->Info().locomotor_info),
      b_reject_move_(!self.AcceptsOrder("Move")) {}

bool Mobile::MoveOrderTargeter::TargetOverridesSelection(
    sim::Actor& self, const sim::Target& target,
    std::span<Actor* const> actors_at, CPos xy,
    sim::TargetModifiers modifiers) {

  (void)actors_at;
  (void)xy;

  if (target.type == sim::TargetType::Actor &&
      (target.ActorPtr->Owner() != self.Owner() ||
       self.world().Selection()->Contains(
          const_cast<Actor*>(target.ActorPtr))))
    return true;

  return (static_cast<std::int32_t>(modifiers) &
          static_cast<std::int32_t>(sim::TargetModifiers::ForceMove)) != 0;
}

bool Mobile::MoveOrderTargeter::CanTarget(sim::Actor& self,
                                          const sim::Target& target,
                                          sim::TargetModifiers& modifiers,
                                          std::string& cursor) {

  if (b_reject_move_ || target.type != sim::TargetType::Terrain ||
      (mobile_->b_require_force_move &&
       (static_cast<std::int32_t>(modifiers) &
        static_cast<std::int32_t>(sim::TargetModifiers::ForceMove)) == 0))
    return false;

  const CPos location =
      self.world().Map().CellContaining(target.CenterPosition());
  b_is_queued_ = (static_cast<std::int32_t>(modifiers) &
                  static_cast<std::int32_t>(sim::TargetModifiers::ForceQueue)) !=
                 0;

  const bool explored = true;

  const bool mobile_paused =
      mobile_->ConditionalTraitCore<Mobile>::IsTraitPaused();

  bool terrain_cursor_found = false;
  std::string terrain_cursor;
  if (explored) {
    const std::string& terrain_type =
        self.world().Map().GetTerrainInfo(location).Type;
    for (const auto& [key, value] : mobile_->Info().vec_terrain_cursors)
      if (key == terrain_type) {
        terrain_cursor = value;
        terrain_cursor_found = true;
        break;
      }
  }

  if (mobile_paused || !self.world().Map().Contains(location) ||
      (!explored && !locomotor_info_->MoveIntoShroud()) ||
      (explored &&
       mobile_->Locomotor()->MovementCostForCell(location) ==
           pathfinding::kMovementCostForUnreachableCell))
    cursor = mobile_->Info().str_blocked_cursor;
  else if (!explored || !terrain_cursor_found)
    cursor = mobile_->Info().str_cursor;
  else
    cursor = terrain_cursor;

  return true;
}

sim::Activity* Mobile::GetCreationActivity() {

  if (b_return_to_cell_on_creation || !vec_creation_rallypoint.empty() ||
      int4_creation_activity_delay > 0)
    return p_self_->world().Arena().Create<Mobile::LeaveProductionActivity>(
        *p_self_, int4_creation_activity_delay, vec_creation_rallypoint,
        b_return_to_cell_on_creation
            ? p_self_->world().Arena().Create<Mobile::ReturnToCellActivity>(
                  *p_self_, int4_creation_activity_delay,
                  b_return_to_cell_on_creation_recalculate_sub_cell)
            : nullptr);

  return nullptr;
}

}
