// UPSTREAM: OpenRA.Mods.Common/Traits/Mobile.cs @b6fc03f L25-1091 实现部分
//          (the implementation half)。
#include "mods/mobile.hpp"

#include "mods/actor_exts.hpp"
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

// ———— MobileInfoData(工厂解析面:L26-171)————
// ———— MobileInfoData (the factory-parse face: L26-171) ————

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
  // 空串 = 上游 null 载荷(同 ConditionalTraitData::Parse)
  // An empty string = upstream's null payload (same as
  // ConditionalTraitData::Parse).
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

  // TerrainCursors 字典(插入序对)
  // The TerrainCursors dictionary (insertion-order pairs).
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

/// Info 面的 locomotor 运行时解析缓存(world 级;上游 Info 实例字段)
/// The Info face's locomotor runtime-resolution cache (world-scoped;
/// upstream's Info-instance field).
Locomotor* ResolveLocomotor(sim::World& world,
                            const std::string& str_name) {
  // SingleOrDefault:0 → null;>1 → 上游同样取首(SingleOrDefault 的多命中
  // 抛只发生在 RulesetLoaded 面的 Info 表扫描)
  // SingleOrDefault: 0 → null; >1 → the first (SingleOrDefault's
  // multiple-hit throw lives only in the RulesetLoaded Info-table scan).
  Locomotor* found = nullptr;
  for (auto* l : world.WorldActor()->TraitsImplementing<Locomotor>())
    if (l->Info().Name() == str_name) {
      found = l;
      break;
    }
  return found;
}

/// RulesetLoaded(L106-121)的 LocomotorInfo 解析:世界 actor 的 Info 表按名
/// 过滤(0/多命中异常文本逐字)
/// The RulesetLoaded (L106-121) LocomotorInfo resolution: the world
/// actor's Info table filtered by name (the 0/multiple-hit exception
/// texts verbatim).
const LocomotorInfo* ResolveLocomotorInfo(sim::World& world,
                                          const std::string& str_name) {
  // 上游扫 rules.Actors[SystemActors.World].TraitInfos<LocomotorInfo>()
  // —— C++ 侧世界 actor 的运行时 Locomotor trait 即同表产物(工厂构造序)
  // Upstream scans rules.Actors[SystemActors.World].TraitInfos<
  // LocomotorInfo>() — on the C++ side the world actor's runtime
  // Locomotor traits are the same table's products (factory construct
  // order).
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

}  // namespace

const LocomotorInfo* ResolveMobileLocomotorInfo(sim::World& world,
                                                const std::string& str_name) {
  // RulesetLoaded 解析面的工厂出口 | the factory outlet of the
  // RulesetLoaded resolution face.
  return ResolveLocomotorInfo(world, str_name);
}

bool MobileInfoData::CanEnterCell(sim::World& world, Actor* self, CPos cell,
                                  sim::SubCell sub_cell, Actor* ignore_actor,
                                  sim::BlockedByActor check) const {
  // L131-140(PERF:避免热路径重复 trait 查询 → 运行时解析)
  // L131-140 (PERF: avoiding repeated hot-path trait queries → the
  // runtime resolution).
  Locomotor* locomotor = ResolveLocomotor(world, str_locomotor);

  return locomotor->MovementCostToEnterCell(
             self, cell, check, ignore_actor, false, sub_cell) !=
         pathfinding::kMovementCostForUnreachableCell;
}

bool MobileInfoData::CanStayInCell(sim::World& world, CPos cell) const {
  // L142-152
  if (cell.Layer() == pathfinding::CustomMovementLayerType::kTunnel)
    return false;

  Locomotor* locomotor = ResolveLocomotor(world, str_locomotor);
  return locomotor->CanStayInCell(cell);
}

// ———— Mobile(L173-1090)————

Mobile::Mobile(ActorInitializer& init, const MobileInfoData& info)
    : ConditionalTraitCore<Mobile>(info.conditional),
      info_(info),
      p_self_(&init.Self()),
      orientation_(WRot::None()) {
  // L269-306
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

  // Sets the initial center position
  // Unit will move into the cell grid (defined by LocationInit) as its
  // initial activity
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
  // L308-320
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

  // ConditionalTrait.Created(条件面;无条件 → TraitEnabled)
  // ConditionalTrait.Created (the conditional face; unconditional →
  // TraitEnabled).
  CoreCreated(self);
}

std::vector<sim::VariableObserver> Mobile::GetVariableObservers() {
  // L894-904:base(requires/pause)+ RequireForceMove/Immovable(yield 序)
  // L894-904: the base (requires/pause) + RequireForceMove/Immovable (the
  // yield order).
  std::vector<sim::VariableObserver> observers = CollectObservers();

  if (info_.expr_require_force_move.has_value()) {
    // RequireForceMoveConditionChanged(L906-909)
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
    // ImmovableConditionChanged(L911-917)
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
  // L355-373 共体(TraitEnabled/Disabled/Resumed/Paused)
  // The L355-373 shared body (TraitEnabled/Disabled/Resumed/Paused).
  p_self_->world().ActorMapFace()->UpdateOccupiedCells(
      p_self_->OccupiesSpace());
}

void Mobile::SetCurrentMovementTypes(sim::MovementType value) {
  // L186-201
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
  // L327-343
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
  // L345-348
  self.world().AddToMaps(&self, this);
}

void Mobile::RemovedFromWorld(Actor& self) {
  // L350-353
  self.world().RemoveFromMaps(&self, this);
}

std::vector<std::pair<CPos, sim::SubCell>> Mobile::OccupiedCells() const {
  // L256-266
  if (FromCell() == ToCell())
    return {{FromCell(), FromSubCell}};

  // HACK: Should be fixed properly, see
  // https://github.com/OpenRA/OpenRA/pull/17292 for an explanation
  if (info_.locomotor_info->SharesCell())
    return {{ToCell(), ToSubCell}};

  return {{FromCell(), FromSubCell}, {ToCell(), ToSubCell}};
}

// ———— 局部杂项(L375-453)————

std::optional<CPos> Mobile::GetAdjacentEnterableCell(
    CPos next_cell, const std::function<bool(CPos)>& prefer_to_avoid) {
  // L377-392
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
    // Random(sharedRandom):uniform 上界含端点 → Next(0, count)
    // Random(sharedRandom): the uniform inclusive upper bound →
    // Next(0, count).
    auto& random = p_self_->world().SharedRandom();
    new_cell = vec_avail_cells[static_cast<std::size_t>(
        random.Next(0, static_cast<std::int32_t>(vec_avail_cells.size())))];
  }

  return new_cell;
}

std::optional<CPos> Mobile::GetAdjacentCell(
    CPos next_cell, const std::function<bool(CPos)>& prefer_to_avoid) {
  // L394-416
  std::optional<CPos> new_cell = GetAdjacentEnterableCell(next_cell,
                                                          prefer_to_avoid);
  if (!new_cell.has_value()) {
    std::vector<CPos> vec_not_stupid_cells;
    for (const CVec& direction : CVec::Directions()) {
      const CPos p = ToCell() + direction;
      if (p != next_cell && p != ToCell())
        vec_not_stupid_cells.push_back(p);
    }

    // notStupidCells.SelectMany(GetActorsAt().Where(IsMovable),
    // (c,a)=>(c,a)).RandomOrDefault(sharedRandom)
    struct CellActor {
      CPos cell;
      Actor* actor;
    };
    std::vector<CellActor> vec_pairs;
    for (const CPos c : vec_not_stupid_cells)
      for (Actor* a :
           p_self_->world().ActorMapFace()->GetActorsAt(c)) {
        // IsMovable(L418-428)
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
  // L430-439
  if (sim::HasMovementType(movement_types_, sim::MovementType::Horizontal))
    return true;

  if (sim::HasMovementType(movement_types_, sim::MovementType::Turn))
    return TurnToMove;

  return false;
}

bool Mobile::CanInteractWithGroundLayer(Actor* self) {
  // L441-451
  if (ToCell().Layer() == 0)
    return true;

  auto layers = self->world().ActorMapFace()->CustomMovementLayers();
  return layers[ToCell().Layer()] == nullptr ||
         layers[ToCell().Layer()]->InteractsWithDefaultLayer();
}

// ———— IPositionable(L455-553)————

sim::SubCell Mobile::GetValidSubCell(sim::SubCell preferred) {
  // L458-477
  // Try same sub-cell
  if (preferred == sim::SubCell::Any)
    preferred = FromSubCell;

  // Fix sub-cell assignment
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
  // L480-493
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
  // L496-502
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
  // L505-519
  pos_center_ = pos;
  self->world().UpdateMaps(self, this);

  auto& map = self->world().Map();
  SetTerrainRampOrientation(
      map.TerrainOrientation(map.CellContaining(pos)));

  // The first time SetCenterPosition is called is in the constructor
  // before creation, so we need a null check here as well
  for (auto* n : vec_notify_center_position_changed_)
    n->CenterPositionChanged(*self, static_cast<std::uint8_t>(FromCell().Layer()),
                             static_cast<std::uint8_t>(ToCell().Layer()));
}

void Mobile::SetTerrainRampOrientation(WRot orientation) {
  // L521-525
  if (info_.dist_terrain_orientation_adjustment_margin.Length >= 0)
    rot_terrain_ramp_ = orientation;
}

bool Mobile::IsLeavingCell(CPos location, sim::SubCell sub_cell) {
  // L527-531
  return ToCell() != location && FromCell() == location &&
         (sub_cell == sim::SubCell::Any || FromSubCell == sub_cell ||
          sub_cell == sim::SubCell::FullCell ||
          FromSubCell == sim::SubCell::FullCell);
}

sim::SubCell Mobile::GetAvailableSubCell(CPos a, sim::SubCell preferred_sub_cell,
                                         Actor* ignore_actor,
                                         sim::BlockedByActor check) {
  // L533-536
  return p_locomotor_->GetAvailableSubCell(p_self_, a, check,
                                           preferred_sub_cell, ignore_actor);
}

bool Mobile::CanExistInCell(CPos cell) {
  // L538-541
  return p_locomotor_->MovementCostForCell(cell) !=
         pathfinding::kMovementCostForUnreachableCell;
}

bool Mobile::CanEnterCell(CPos cell, Actor* ignore_actor,
                          sim::BlockedByActor check) {
  // L543-546
  return info_.CanEnterCell(p_self_->world(), p_self_, cell, ToSubCell,
                            ignore_actor, check);
}

bool Mobile::CanStayInCell(CPos cell) {
  // L548-551
  return info_.CanStayInCell(p_self_->world(), cell);
}

// ———— 局部 IPositionable 相关(L555-615)————

void Mobile::SetLocation(CPos from, sim::SubCell from_sub, CPos to,
                         sim::SubCell to_sub) {
  // L558-575
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

  // Most custom layer conditions are added/removed when starting the
  // transition between layers.
  if (ToCell().Layer() != FromCell().Layer())
    for (auto* n : vec_notify_custom_layer_changed_)
      n->CustomLayerChanged(*p_self_,
                            static_cast<std::uint8_t>(FromCell().Layer()),
                            static_cast<std::uint8_t>(ToCell().Layer()));
}

void Mobile::FinishedMoving(Actor* self) {
  // L577-589
  // Need to check both fromCell and toCell because FinishedMoving is
  // called multiple times during the move
  if (FromCell().Layer() == ToCell().Layer())
    for (auto* n : vec_notify_finished_moving_)
      n->FinishedMoving(*self, static_cast<std::uint8_t>(FromCell().Layer()),
                        static_cast<std::uint8_t>(ToCell().Layer()));

  // Only crush actors on having landed
  if (!IsAtGroundLevel(*self))
    return;

  CrushAction(self, &sim::INotifyCrushed::OnCrush);
}

void Mobile::CrushAction(
    Actor* self,
    void (sim::INotifyCrushed::*method)(Actor&, Actor&,
                                        const core::BitSet<sim::CrushClass>&)) {
  // L591-601
  // crushables = GetActorsAt(ToCell, ToSubCell).Where(a => a != self)
  //              .SelectMany(a => a.Crushables.Select(t => (a, t)))
  // (上游 Func<INotifyCrushed, Action<…>> 的方法组实参 → 成员指针)
  // (upstream's Func<INotifyCrushed, Action<…>> method-group argument → a
  // member pointer).
  for (Actor* a :
       self->world().ActorMapFace()->GetActorsAt(ToCell(), ToSubCell)) {
    if (a == self)
      continue;

    // Only crush actors that are on the ground level
    for (auto* crushable : a->Crushables()) {
      if (!crushable->CrushableBy(*a, *self,
                                  info_.locomotor_info->Crushes()))
        continue;

      // Only crush actors that are on the ground level
      if (!IsAtGroundLevel(*a))
        continue;

      for (auto* notify_crushed :
           a->TraitsImplementing<sim::INotifyCrushed>())
        (notify_crushed->*method)(*a, *self, info_.locomotor_info->Crushes());
    }
  }
}

void Mobile::AddInfluence() {
  // L603-607
  if (p_self_->IsInWorld())
    p_self_->world().ActorMapFace()->AddInfluence(p_self_, this);
}

void Mobile::RemoveInfluence() {
  // L609-613
  if (p_self_->IsInWorld())
    p_self_->world().ActorMapFace()->RemoveInfluence(p_self_, this);
}

// ———— IMove(L617-756)————

sim::Activity* Mobile::WrapMove(sim::Activity* inner) {
  // L619-626:FirstEnabledTraitOrDefault(IWrapMove 使能面)
  // L619-626: FirstEnabledTraitOrDefault (the IWrapMove enablement face).
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
  // L628-632
  return WrapMove(p_self_->world().Arena().Create<activities::Move>(
      *p_self_, cell, WDist::FromCells(near_enough), ignore_actor,
      evaluate_nearest_movable_cell, target_line_color));
}

sim::Activity* Mobile::MoveWithinRange(
    const sim::Target& target, WDist range,
    std::optional<WPos> initial_target_position,
    std::optional<core::Color> target_line_color) {
  // L634-638
  return WrapMove(p_self_->world().Arena().Create<activities::MoveWithinRange>(
      *p_self_, target, WDist{0}, range, initial_target_position,
      target_line_color));
}

sim::Activity* Mobile::MoveWithinRange(
    const sim::Target& target, WDist min_range, WDist max_range,
    std::optional<WPos> initial_target_position,
    std::optional<core::Color> target_line_color) {
  // L640-644
  return WrapMove(p_self_->world().Arena().Create<activities::MoveWithinRange>(
      *p_self_, target, min_range, max_range, initial_target_position,
      target_line_color));
}

sim::Activity* Mobile::MoveFollow(
    Actor* self, const sim::Target& target, WDist min_range, WDist max_range,
    std::optional<WPos> initial_target_position,
    std::optional<core::Color> target_line_color) {
  // L646-650
  return WrapMove(self->world().Arena().Create<activities::Follow>(
      *self, target, min_range, max_range, initial_target_position,
      target_line_color));
}

sim::Activity* Mobile::ReturnToCell(Actor* self) {
  // L652-655
  return self->world().Arena().Create<ReturnToCellActivity>(*self, 0, false);
}

sim::Activity* Mobile::MoveToTarget(
    Actor* self, const sim::Target& target,
    std::optional<WPos> /*initial_target_position*/,
    std::optional<core::Color> target_line_color) {
  // L707-714
  if (target.Type() == sim::TargetType::Invalid)
    return nullptr;

  return WrapMove(self->world().Arena().Create<activities::MoveAdjacentTo>(
      *self, target, std::nullopt, target_line_color));
}

sim::Activity* Mobile::MoveIntoTarget(Actor* self, const sim::Target& target) {
  // L716-724
  if (target.Type() == sim::TargetType::Invalid)
    return nullptr;

  // Activity cancels if the target moves by more than half a cell
  // to avoid problems with the cell grid
  return WrapMove(self->world().Arena().Create<activities::LocalMoveIntoTarget>(
      *self, target, WDist{512}));
}

sim::Activity* Mobile::MoveOntoTarget(
    Actor* self, const sim::Target& target, const WVec& offset,
    std::optional<WAngle> facing,
    std::optional<core::Color> target_line_color) {
  // L726-729
  return WrapMove(self->world().Arena().Create<activities::MoveOntoAndTurn>(
      *self, target, offset, facing, target_line_color));
}

sim::Activity* Mobile::LocalMove(Actor* self, WPos from_pos, WPos to_pos) {
  // L731-734
  return WrapMove(LocalMoveImpl(self, from_pos, to_pos, self->Location()));
}

int Mobile::EstimatedMoveDuration(Actor* self, WPos from_pos, WPos to_pos) {
  // L736-740
  const int speed = MovementSpeedForCell(self->Location());
  return speed > 0 ? (to_pos - from_pos).Length() / speed : 0;
}

CPos Mobile::NearestMoveableCell(CPos target) {
  // L742-746
  // Limit search to a radius of 10 tiles
  return NearestMoveableCell(target, 1, 10);
}

bool Mobile::CanEnterTargetNow(Actor* self, const sim::Target& target) {
  // L748-754
  if (target.Type() == sim::TargetType::FrozenActor)
    return false;  // FrozenActor.IsValid 面随 Shroud 批(无实例 → 假)
                   // (the FrozenActor.IsValid face rides the Shroud batch —
                   // no instances → false)

  if (self->Location() ==
      self->world().Map().CellContaining(target.CenterPosition()))
    return true;

  for (const CPos c : AdjacentCells(self->world(), target))
    if (c == self->Location())
      return true;

  return false;
}

// ———— 局部 IMove 相关(L758-843)————

int Mobile::MovementSpeedForCell(CPos cell) {
  // L760-766(speedModifiers Lazy → 首次物化;terrainSpeed 逐次求值)
  // L760-766 (the speedModifiers Lazy → first-use materialization;
  // terrainSpeed evaluated per call).
  if (!b_speed_modifiers_resolved_) {
    vec_speed_modifiers_.clear();
    for (auto* modifier : p_self_->TraitsImplementing<sim::ISpeedModifier>())
      vec_speed_modifiers_.push_back(modifier->GetSpeedModifier());
    b_speed_modifiers_resolved_ = true;
  }

  const int terrain_speed = p_locomotor_->MovementSpeedForCell(cell);

  // modifiers.Append(terrainSpeed)(尾部拼接 — 取 span 求值后弹出;
  // OPT-A1 的零 LINQ 形态)
  // modifiers.Append(terrainSpeed) (the tail append — the span evaluates
  // then pops; OPT-A1's zero-LINQ shape).
  vec_speed_modifiers_.push_back(terrain_speed);
  const int result = ApplyPercentageModifiers(
      info_.int4_speed, std::span<const int>{vec_speed_modifiers_});
  vec_speed_modifiers_.pop_back();
  return result;
}

CPos Mobile::NearestMoveableCell(CPos target, int min_range, int max_range) {
  // L768-790
  // HACK: This entire method is a hack, and needs to be replaced with
  // a proper path search that can account for movement layer transitions.
  // HACK: Work around code that blindly tries to move to cells in invalid
  // movement layers.
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

  // Couldn't find a cell
  return target;
}

CPos Mobile::NearestCell(CPos target,
                         const std::function<bool(CPos)>& check,
                         int min_range, int max_range) {
  // L792-803
  if (check(target))
    return target;

  for (const CPos tile :
       p_self_->world().Map().FindTilesInAnnulus(target, min_range, max_range))
    if (check(tile))
      return tile;

  // Couldn't find a cell
  return target;
}

void Mobile::EnteringCell(Actor* self) {
  // L805-812
  // Only crush actors on having landed
  if (!IsAtGroundLevel(*self))
    return;

  CrushAction(self, &sim::INotifyCrushed::WarnCrush);
}

sim::Activity* Mobile::MoveTo(
    const std::function<std::pair<bool, std::vector<CPos>>(
        sim::BlockedByActor)>& path_func) {
  // L814
  return p_self_->world().Arena().Create<activities::Move>(*p_self_,
                                                           path_func);
}

sim::Activity* Mobile::LocalMoveImpl(Actor* self, WPos from_pos, WPos to_pos,
                                     CPos cell) {
  // L816-825
  const int speed = MovementSpeedForCell(cell);
  const int length = speed > 0 ? (to_pos - from_pos).Length() / speed : 0;

  const WVec delta = to_pos - from_pos;
  const WAngle facing =
      delta.HorizontalLengthSquared() != 0 ? delta.Yaw() : Facing();

  return self->world().Arena().Create<activities::Drag>(*self, from_pos,
                                                        to_pos, length, facing);
}

std::optional<CPos> Mobile::ClosestGroundCell() {
  // L827-841
  // Creating a new CPos serves to reset a potential custom layer
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

// ———— init 修改面/闲置/阻挡通知(L845-892)————

void Mobile::ModifyActorPreviewInit(sim::Actor& self,
                                    sim::TypeDictionary& inits) {
  // L845-849
  (void)self;
  if (!inits.Contains<sim::DynamicFacingInit>() &&
      !inits.Contains<sim::FacingInit>())
    inits.Add(p_self_->world().Arena().Create<sim::DynamicFacingInit>(
        [this]() { return Facing(); }));
}

void Mobile::ModifyDeathActorInit(sim::Actor& self,
                                  sim::TypeDictionary& init) {
  // L851-858
  init.Add(self.world().Arena().Create<sim::FacingInit>(Facing()));

  // Allows the husk to drag to its final position
  if (CanEnterCell(self.Location(), &self, sim::BlockedByActor::Stationary))
    init.Add(self.world().Arena().Create<sim::HuskSpeedInit>(
        MovementSpeedForCell(self.Location())));
}

void Mobile::OnBecomingIdle(Actor& self) {
  // L860-878
  if (self.Location().Layer() == 0) {
    // Make sure that units aren't left idling in a transit-only cell
    // HACK: activities should be making sure that this can't happen in the
    // first place!
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
  // L880-892
  if (!AppearsFriendlyTo(self, blocking))
    return;

  if (self.IsIdle()) {
    self.QueueActivity(
        false, self.world().Arena().Create<activities::Nudge>(&blocking));
    return;
  }

  IsBlocking = true;
}

// ———— order 面(L919-987)————

std::vector<sim::IOrderTargeter*> Mobile::Orders() {
  // L919-926
  std::vector<sim::IOrderTargeter*> orders;
  if (!IsTraitDisabled())
    orders.push_back(
        p_self_->world().Arena().Create<MoveOrderTargeter>(*p_self_, this));
  return orders;
}

net::Order* Mobile::IssueOrder(Actor& self, sim::IOrderTargeter* order,
                               const sim::Target& target, bool queued) {
  // L929-935
  (void)self;
  if (dynamic_cast<MoveOrderTargeter*>(order) != nullptr)
    return p_self_->world().Arena().Create<net::Order>("Move", p_self_,
                                                       target, queued);

  return nullptr;
}

void Mobile::ResolveOrder(Actor& self, const net::Order& order) {
  // L937-963
  if (IsTraitDisabled())
    return;

  if (order.str_order_string == "Move") {
    if (!order.target.IsValidFor(&self))
      return;

    const CPos cell = self.world().Map().Clamp(
        self.world().Map().CellContaining(order.target.CenterPosition()));
    // MoveIntoShroud 短路面:Shroud 未移植 —— MoveIntoShroud 默认 true 时
    // 不触;false 时按已探索论(COVERAGE 登记,Shroud 批回接)
    // The MoveIntoShroud short circuit: Shroud unported — untouched when
    // MoveIntoShroud keeps its default true; treated-as-explored when
    // false (registered in COVERAGE; rewired with the Shroud batch).
    if (!info_.locomotor_info->MoveIntoShroud())
      return;  // IsExplored 的已探索等价面(the explored-equivalent face)

    self.QueueActivity(order.b_queued,
                       WrapMove(self.world().Arena().Create<activities::Move>(
                           self, cell, WDist::FromCells(8), nullptr, true,
                           info_.color_target_line)));
    // ShowTargetLines:Settings 面 Phase 6(UI 命令,仿真不可观测)
    // ShowTargetLines: the Settings face is Phase 6 (a UI command,
    // unobservable in the sim).
  }
  // TODO: This should only cancel activities queued by this trait
  else if (order.str_order_string == "Stop")
    self.CancelActivity();
  else if (order.str_order_string == "Scatter") {
    self.QueueActivity(order.b_queued,
                       self.world().Arena().Create<activities::Nudge>(&self));
    // ShowTargetLines(同上 | same as above)
  }
}

std::string Mobile::VoicePhraseForOrder(Actor& self, const net::Order& order) {
  // L965-987
  (void)self;
  if (IsTraitDisabled())
    return {};

  // "Move" 的 Shroud.IsExplored 前置 = 已探索等价面(同 ResolveOrder;
  // COVERAGE 登记)
  // The "Move" Shroud.IsExplored precondition = the explored-equivalent
  // face (same as ResolveOrder; registered in COVERAGE).
  if (order.str_order_string == "Move")
    return info_.str_voice;
  if (order.str_order_string == "Scatter" || order.str_order_string == "Stop")
    return info_.str_voice;

  return {};
}

// ———— Mobile::ReturnToCellActivity(L657-705)————

Mobile::ReturnToCellActivity::ReturnToCellActivity(sim::Actor& self,
                                                   int delay,
                                                   bool recalculate_sub_cell)
    : mobile_(self.Trait<Mobile>()),
      b_recalculate_sub_cell_(recalculate_sub_cell),
      int4_delay_(delay) {
  // L667-673
  b_is_interruptible_ = false;
}

void Mobile::ReturnToCellActivity::OnFirstRun(sim::Actor& self) {
  // L675-680
  pos_ = self.CenterPosition();
  // Parachutable trait 面未移植:TraitOrDefault<Parachutable> 恒 null →
  // 跳过 Parachute 子活动(空集等价;COVERAGE 登记)
  // The Parachutable trait face is unported: TraitOrDefault<Parachutable>
  // stays null → the Parachute child activity skips (the empty-set
  // equivalent; registered in COVERAGE).
}

bool Mobile::ReturnToCellActivity::Tick(sim::Actor& self) {
  // L682-704
  pos_ = self.CenterPosition();
  cell_ = mobile_->ToCell();
  sub_cell_ = mobile_->ToSubCell;

  if (b_recalculate_sub_cell_)
    sub_cell_ =
        mobile_->Info().locomotor_info->SharesCell()
            ? self.world().ActorMapFace()->FreeSubCell(
                  cell_, sub_cell_, [&self](Actor& a) { return &a != &self; })
            : sim::SubCell::FullCell;

  // TODO: solve/reduce cell is full problem
  if (sub_cell_ == sim::SubCell::Invalid)
    sub_cell_ = static_cast<sim::SubCell>(
        self.world().Map().Grid().DefaultSubCell());

  // Reserve the exit cell
  mobile_->SetPosition(&self, cell_, sub_cell_);
  mobile_->SetCenterPosition(&self, pos_);

  if (int4_delay_ > 0)
    QueueChild(self.world().Arena().Create<activities::Wait>(int4_delay_));

  QueueChild(mobile_->LocalMove(
      &self, pos_, self.world().Map().CenterOfSubCell(cell_, sub_cell_)));
  return true;
}

// ———— Mobile::LeaveProductionActivity(L989-1033)————

Mobile::LeaveProductionActivity::LeaveProductionActivity(
    sim::Actor& self, int delay, std::vector<CPos> rally_point,
    Mobile::ReturnToCellActivity* return_to_cell)
    : mobile_(self.Trait<Mobile>()),
      int4_delay_(delay),
      vec_rally_point_(std::move(rally_point)),
      p_return_to_cell_(return_to_cell) {}

void Mobile::LeaveProductionActivity::OnFirstRun(sim::Actor& self) {
  // L1004-1015
  // It is vital that ReturnToCell is queued first as it needs the power to
  // intercept a possible cancellation of this activity.
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

// ———— Mobile::MoveOrderTargeter(L1044-1089)————

Mobile::MoveOrderTargeter::MoveOrderTargeter(sim::Actor& self, Mobile* unit)
    : mobile_(unit),
      locomotor_info_(unit->Info().locomotor_info),
      b_reject_move_(!self.AcceptsOrder("Move")) {}

bool Mobile::MoveOrderTargeter::TargetOverridesSelection(
    sim::Actor& self, const sim::Target& target,
    std::span<Actor* const> actors_at, CPos xy,
    sim::TargetModifiers modifiers) {
  // L1050-1056
  (void)actors_at;
  (void)xy;
  // Always prioritise orders over selecting other peoples actors or own
  // actors that are already selected
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
  // L1069-1088
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

  // Shroud.IsExplored:Shroud 未移植 —— 恒已探索等价面(COVERAGE 登记;
  // Shroud 批回接)
  // Shroud.IsExplored: Shroud unported — the always-explored equivalent
  // face (registered in COVERAGE; rewired with the Shroud batch).
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

// ———— ICreationActivity(L1035-1042)————

sim::Activity* Mobile::GetCreationActivity() {
  // L1035-1042
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

}  // namespace ora::mods
