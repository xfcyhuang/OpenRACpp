// UPSTREAM: OpenRA.Mods.Common/Traits/World/Locomotor.cs @b6fc03f(实现部分)
//          The implementation half of Locomotor.cs.
import std;

#include "mods/pathfinding/locomotor.hpp"

#include "game/ruleset.hpp"
#include "map/map.hpp"
#include "meta/generic_record.hpp"
#include "meta/field_loader.hpp"
#include "sim/trait_registry.hpp"
#include "sim/actor.hpp"
#include "sim/trait_registry.hpp"
#include "sim/world.hpp"

namespace ora::mods::pathfinding {

using sim::BlockedByActor;
using sim::ICrushable;
using sim::PlayerRelationship;

// ———— ParseLocomotorInfo(TraitRegistry 工厂面;值袋槽位 → 运行时 Info)
// ———— ParseLocomotorInfo (the TraitRegistry factory face; bag slots → the
// runtime Info).
LocomotorInfo ParseLocomotorInfo(const meta::RecordObject& rec_info) {
  std::string name{"default"};
  int wait_average = 40;
  int wait_spread = 10;
  bool shares_cell = false;
  bool move_into_shroud = true;
  core::BitSet<sim::CrushClass> crushes;
  core::BitSet<sim::DamageType> crush_damage_types;
  std::vector<std::pair<std::string, LocomotorInfo::TerrainInfo>> speeds;
  bool disable_domain_passability_check = false;

  if (const auto* generated =
          dynamic_cast<const meta::GeneratedRecord*>(&rec_info)) {
    const std::vector<const meta::FieldDesc*> fields =
        meta::CollectFields(generated->record_desc());
    for (std::size_t i = 0; i < fields.size(); i++) {
      const std::string_view field_name = fields[i]->str_name;
      const meta::GenericValue& v = generated->Slot(i);
      if (field_name == "Name") {
        if (auto* sv = std::get_if<std::string>(&v.val))
          name = *sv;
      } else if (field_name == "WaitAverage") {
        if (auto* n = std::get_if<std::int64_t>(&v.val))
          wait_average = static_cast<int>(*n);
      } else if (field_name == "WaitSpread") {
        if (auto* n = std::get_if<std::int64_t>(&v.val))
          wait_spread = static_cast<int>(*n);
      } else if (field_name == "SharesCell") {
        if (auto* b = std::get_if<bool>(&v.val))
          shares_cell = *b;
      } else if (field_name == "MoveIntoShroud") {
        if (auto* b = std::get_if<bool>(&v.val))
          move_into_shroud = *b;
      } else if (field_name == "Crushes") {
        if (auto* n = std::get_if<std::int64_t>(&v.val))
          crushes = core::BitSet<sim::CrushClass>::FromRawBits(
              static_cast<std::uint64_t>(*n));
      } else if (field_name == "CrushDamageTypes") {
        if (auto* n = std::get_if<std::int64_t>(&v.val))
          crush_damage_types = core::BitSet<sim::DamageType>::FromRawBits(
              static_cast<std::uint64_t>(*n));
      } else if (field_name == "TerrainSpeeds") {
        if (auto* dict = std::get_if<meta::GenericDict>(&v.val))
          for (const auto& [key, value] : *dict) {
            const auto* ks = std::get_if<std::string>(&key.val);
            const auto* t = std::get_if<meta::GenericTuple>(&value.val);
            if (ks == nullptr || t == nullptr)
              continue;
            speeds.emplace_back(
                *ks, LocomotorInfo::TerrainInfo{
                         static_cast<int>(t->arr_ints[0]),
                         static_cast<short>(t->arr_ints[1])});
          }
      } else if (field_name == "DisableDomainPassabilityCheck") {
        if (auto* b = std::get_if<bool>(&v.val))
          disable_domain_passability_check = *b;
      }
    }
  }

  // ctor 的地形索引展开(L158-162):按规则表 TerrainTypes 序对齐,
  // 未命中 = Impassable
  // The ctor's terrain-index expansion (L158-162): aligned to the rules
  // table's TerrainTypes order, misses = Impassable.
  std::vector<LocomotorInfo::TerrainInfo> terrain_infos;
  const game::Ruleset* rules = nullptr;
  if (rules != nullptr) {}
  // (地形索引在 Locomotor ctor 内展开 —— Parse 仅承载字段面)
  // (the terrain-index expansion happens inside the Locomotor ctor —
  // Parse only carries the field face.)
  return LocomotorInfo{std::move(name),
                       wait_average,
                       wait_spread,
                       shares_cell,
                       move_into_shroud,
                       crushes,
                       crush_damage_types,
                       std::move(terrain_infos),
                       std::move(speeds),
                       disable_domain_passability_check};
}

// ———— ctor(L152-163)————
Locomotor::Locomotor(Actor& self, LocomotorInfo info)
    : info_{std::move(info)}, world_{&self.world()} {
  shares_cell_ = info_.SharesCell();

  const auto& terrain_info = world_->Map().Rules().TerrainInfo();
  const auto& terrain_types = terrain_info.TerrainTypes();
  vec_terrain_infos_.resize(terrain_types.size(),
                            LocomotorInfo::TerrainInfo::Impassable());
  for (std::size_t i = 0; i < terrain_types.size(); i++) {
    // !info.TerrainSpeeds.TryGetValue(terrainType.Type, out terrainInfos[i])
    // 的命中覆盖(L161)| the hit override (L161).
    for (const auto& [key, ti] : info_.TerrainSpeeds())
      if (key == terrain_types[i].Type) {
        vec_terrain_infos_[i] = ti;
        break;
      }
  }
}

// ———— MovementCostForCell(L165-184)————
short Locomotor::MovementCostForCell(CPos cell, const CPos* from_cell) const {
  if (!world_->Map().Contains(cell))
    return kMovementCostForUnreachableCell;

  // Prevent units from jumping over height discontinuities.(上游注释)
  if (from_cell != nullptr && cell.Layer() == 0 && from_cell->Layer() == 0 &&
      world_->Map().Grid().MaximumTerrainHeight() > 0) {
    const auto& height_layer = world_->Map().Height();
    if (std::abs(static_cast<int>(height_layer.Get(cell)) -
                 static_cast<int>(height_layer.Get(*from_cell))) > 1)
      return kMovementCostForUnreachableCell;
  }

  return vec_cells_cost_[cell.Layer()]->Get(cell);
}

// ———— MovementSpeedForCell(L186-192)————
int Locomotor::MovementSpeedForCell(CPos cell) const {
  std::uint8_t index;
  if (cell.Layer() == 0) {
    index = world_->Map().GetTerrainIndex(cell);
  } else {
    const auto cmls = world_->CustomMovementLayers();
    index = cmls[cell.Layer()]->GetTerrainIndex(cell);
  }

  return vec_terrain_infos_[index].Speed();
}

// ———— MovementCostToEnterCell(L194-216)————
short Locomotor::MovementCostToEnterCell(Actor* actor, CPos dest_node,
                                         BlockedByActor check,
                                         Actor* ignore_actor, bool ignore_self,
                                         SubCell sub_cell) {
  const short cell_cost = MovementCostForCell(dest_node);

  if (cell_cost == kMovementCostForUnreachableCell ||
      !CanMoveFreelyInto(actor, dest_node, sub_cell, check, ignore_actor,
                         ignore_self))
    return kMovementCostForUnreachableCell;

  return cell_cost;
}

short Locomotor::MovementCostToEnterCell(Actor* actor, CPos src_node,
                                         CPos dest_node, BlockedByActor check,
                                         Actor* ignore_actor,
                                         bool ignore_self) {
  const short cell_cost = MovementCostForCell(dest_node, &src_node);

  if (cell_cost == kMovementCostForUnreachableCell ||
      !CanMoveFreelyInto(actor, dest_node, SubCell::FullCell, check,
                         ignore_actor, ignore_self))
    return kMovementCostForUnreachableCell;

  return cell_cost;
}

// ———— CanMoveFreelyInto(L219-297)————
bool Locomotor::CanMoveFreelyInto(Actor* actor, CPos cell, SubCell sub_cell,
                                  BlockedByActor check, Actor* ignore_actor,
                                  bool ignore_self) {
  // If the check allows: We are not blocked by other actors.(上游注释)
  if (check == BlockedByActor::None)
    return true;

  const CellCache cell_cache = GetCache(cell);
  const CellFlag cell_flag = cell_cache.cell_flag;

  // No actor in the cell or free SubCell.(上游注释)
  if (cell_flag == CellFlag::HasFreeSpace)
    return true;

  // If actor is null we're just checking what would happen theoretically.
  // (上游注释段)| (upstream comment)
  if (actor == nullptr)
    return false;

  // All actors that may be in the cell can be crushed.(上游注释)
  if (cell_cache.crushable.Overlaps(actor->Owner()->PlayerMask))
    return true;

  // If the check allows: We are not blocked by moving units.(上游注释)
  if (check <= BlockedByActor::Stationary &&
      !HasCellFlag(cell_flag, CellFlag::HasStationaryActor))
    return true;

  // If the check allows: We are not blocked by units that we can force to
  // move out of the way.(上游注释)| (upstream comment)
  if (check <= BlockedByActor::Immovable &&
      !cell_cache.immovable.Overlaps(actor->Owner()->PlayerMask))
    return true;

  // Cache doesn't account for ignored actors, subcells, temporary blockers
  // or transit only actors.(上游注释段)| (upstream comment)
  if (ignore_actor == nullptr && !ignore_self &&
      sub_cell == SubCell::FullCell &&
      !HasCellFlag(cell_flag, CellFlag::HasTemporaryBlocker) &&
      !HasCellFlag(cell_flag, CellFlag::HasTransitOnlyActor)) {
    // We already know there are uncrushable actors in the cell so we are
    // always blocked.(上游注释)| (upstream comment)
    if (check == BlockedByActor::All)
      return false;

    // We already know there are either immovable or stationary actors
    // which the check does not allow.(上游注释)| (upstream comment)
    if (!HasCellFlag(cell_flag, CellFlag::HasCrushableActor))
      return false;

    // All actors in the cell are immovable and some cannot be crushed.
    // (上游注释)| (upstream comment)
    if (!HasCellFlag(cell_flag, CellFlag::HasMovableActor))
      return false;

    // All actors in the cell are stationary and some cannot be crushed.
    // (上游注释)| (upstream comment)
    if (check == BlockedByActor::Stationary &&
        !HasCellFlag(cell_flag, CellFlag::HasMovingActor))
      return false;
  }

  const std::vector<Actor*> other_actors =
      sub_cell == SubCell::FullCell
          ? actor_map_->GetActorsAt(cell)
          : actor_map_->GetActorsAt(cell, sub_cell);

  if (ignore_self) {
    // Any actor blocking us will prevent our movement, *unless* we are one
    // of those actors.(上游注释)| (upstream comment)
    bool is_blocked = false;
    for (Actor* other_actor : other_actors) {
      if (actor == other_actor)
        return true;

      is_blocked = is_blocked ||
                   IsBlockedBy(actor, other_actor, ignore_actor, cell, check,
                               cell_flag);
    }

    return !is_blocked;
  }

  // Any actor blocking us will prevent our movement.(上游注释)
  for (Actor* other_actor : other_actors)
    if (IsBlockedBy(actor, other_actor, ignore_actor, cell, check, cell_flag))
      return false;

  return true;
}

// ———— CanStayInCell(L299-305)————
bool Locomotor::CanStayInCell(CPos cell) {
  if (!world_->Map().Contains(cell))
    return false;

  return !HasCellFlag(GetCache(cell).cell_flag,
                      CellFlag::HasTransitOnlyActor);
}

// ———— GetAvailableSubCell(L307-326)————
SubCell Locomotor::GetAvailableSubCell(Actor* self, CPos cell,
                                       BlockedByActor check,
                                       SubCell preferred_sub_cell,
                                       Actor* ignore_actor) {
  if (MovementCostForCell(cell) == kMovementCostForUnreachableCell)
    return SubCell::Invalid;

  if (check > BlockedByActor::None) {
    auto check_transient = [&](Actor& other_actor) {
      return IsBlockedBy(self, &other_actor, ignore_actor, cell, check,
                         GetCache(cell).cell_flag);
    };

    if (!shares_cell_)
      return actor_map_->AnyActorsAt(cell, SubCell::FullCell, check_transient)
                 ? SubCell::Invalid
                 : SubCell::FullCell;

    return actor_map_->FreeSubCell(cell, preferred_sub_cell, check_transient);
  }

  if (!shares_cell_)
    return actor_map_->AnyActorsAt(cell, SubCell::FullCell) ? SubCell::Invalid
                                                            : SubCell::FullCell;

  return actor_map_->FreeSubCell(cell, preferred_sub_cell);
}

// ———— IsBlockedBy(L331-376)————
bool Locomotor::IsBlockedBy(Actor* actor, Actor* other_actor,
                            Actor* ignore_actor, CPos cell,
                            BlockedByActor check, CellFlag cell_flag) {
  if (other_actor == ignore_actor)
    return false;

  // Mobile 挂点(L336-337):Mobile 批落地前 `as Mobile` 恒 null →
  // otherIsMovable/otherIsMoving 恒 false(部分覆盖装配面;COVERAGE)
  // The Mobile hooks (L336-337): `as Mobile` stays null before that batch
  // → otherIsMovable/otherIsMoving stay false (the partial-coverage
  // assembly face; COVERAGE).
  const bool other_is_movable = false;
  const bool other_is_moving = false;
  (void)other_is_moving;

  // If the check allows: We are not blocked by allied units that we can
  // force to move out of the way.(上游注释)| (upstream comment)
  if (check <= BlockedByActor::Immovable &&
      HasCellFlag(cell_flag, CellFlag::HasMovableActor) && other_is_movable &&
      actor->Owner()->RelationshipWith(other_actor->Owner()) ==
          PlayerRelationship::Ally)
    return false;

  // If the check allows: we are not blocked by moving units.(上游注释)
  if (check <= BlockedByActor::Stationary &&
      HasCellFlag(cell_flag, CellFlag::HasMovingActor) && other_is_moving)
    return false;

  // ITemporaryBlocker 挂点(L349-355):门控面下无注册实现 → 不触发
  // (world.RulesContainTemporaryBlocker 为 false;同 COVERAGE 面)
  // The ITemporaryBlocker hook (L349-355): no registered implementation
  // under the gated face → never fires (RulesContainTemporaryBlocker is
  // false; the same COVERAGE face).
  if (HasCellFlag(cell_flag, CellFlag::HasTemporaryBlocker)) {
    // If there is a temporary blocker in our path, but we can remove it,
    // we are not blocked.(上游注释)| (upstream comment)
  }

  // Building 的 TransitOnlyCells 挂点(L357-362):Building trait 随
  // Building 批 —— 同部分覆盖面
  // The Building TransitOnlyCells hook (L357-362): the Building trait
  // lands with that batch — the same partial face.
  if (HasCellFlag(cell_flag, CellFlag::HasTransitOnlyActor)) {
    // Transit only tiles should not block movement(上游注释)
  }

  // If we cannot crush the other actor in our way, we are blocked.
  // (上游注释)| (upstream comment)
  if (!HasCellFlag(cell_flag, CellFlag::HasCrushableActor) ||
      info_.Crushes().IsEmpty())
    return true;

  // If the other actor in our way cannot be crushed, we are blocked.
  // (上游注释;PERF: Avoid LINQ)| (upstream comment)
  for (auto* crushable : other_actor->TraitsImplementing<ICrushable>())
    if (crushable->CrushableBy(*other_actor, *actor, info_.Crushes()))
      return false;

  return true;
}

// ———— WorldLoaded(L378-420)————
void Locomotor::WorldLoaded(sim::World& w, gfx::WorldRenderer*) {
  map::Map& map = w.Map();
  actor_map_ = w.ActorMapFace();
  map.CustomTerrainRef().AddCellEntryChangedListener(
      [this](CPos cell) { UpdateCellCost(cell); });
  map.TilesRef().AddCellEntryChangedListener(
      [this](CPos cell) { UpdateCellCost(cell); });
  actor_map_->AddCellUpdatedListener(
      [this](CPos cell) { CellUpdated(cell); });

  vec_cells_cost_.push_back(std::make_unique<CellCostLayer>(map));
  vec_blocking_cache_.push_back(std::make_unique<BlockingCacheLayer>(map));

  for (const CPos cell : map.AllCells()) {
    UpdateCellCost(cell);
    UpdateCellBlocking(cell);
  }

  // NotBefore<> ensures all custom movement layers have been initialized.
  // (上游注释)| (upstream comment)
  const auto custom_movement_layers = w.CustomMovementLayers();
  vec_cells_cost_.resize(custom_movement_layers.size());
  vec_blocking_cache_.resize(custom_movement_layers.size());
  for (auto* cml : custom_movement_layers) {
    if (cml == nullptr)
      continue;

    auto cell_layer = std::make_unique<CellCostLayer>(map);
    for (const CPos cell : map.AllCells()) {
      const std::uint8_t index = cml->GetTerrainIndex(cell);

      short cost = kMovementCostForUnreachableCell;

      if (index != 0xFF)
        cost = vec_terrain_infos_[index].Cost();

      cell_layer->Set(cell, cost);
    }
    vec_cells_cost_[cml->Index()] = std::move(cell_layer);
    vec_blocking_cache_[cml->Index()] =
        std::make_unique<BlockingCacheLayer>(map);
  }
}

// ———— GetCache(L422-430)————
Locomotor::CellCache Locomotor::GetCache(CPos cell) {
  // dirtyCells.Remove(cell) 的位图判重形态 | the bitmap form of the
  // dirtyCells.Remove(cell) probe.
  std::size_t dirty_index = SIZE_MAX;
  if (cell.Layer() == 0) {
    const MPos uv = cell.ToMPos(world_->Map().Grid().Type);
    const Size size = world_->Map().MapSize();
    if (uv.U >= 0 && uv.V >= 0 && uv.U < size.Width && uv.V < size.Height) {
      const std::size_t flat =
          static_cast<std::size_t>(uv.V) * size.Width + uv.U;
      if (flat < vec_dirty_bitmap_.size() && vec_dirty_bitmap_[flat]) {
        vec_dirty_bitmap_[flat] = 0;
        dirty_index = flat;
      }
    }
  }
  if (dirty_index != SIZE_MAX) {
    // 移除插入序表中的该格(O(1) 位图 + O(n) 表清位 —— 一次性)
    // Erase the cell from the insertion-ordered vector (the O(1) bitmap +
    // a one-time O(n) vector sweep).
    const CPos dirty_cell = cell;
    std::erase_if(vec_dirty_cells_, [&](const CPos& c) { return c == dirty_cell; });
    UpdateCellBlocking(cell);
  }

  return vec_blocking_cache_[cell.Layer()]->Get(cell);
}

// ———— CellUpdated(L432-435)————
void Locomotor::CellUpdated(CPos cell) {
  if (cell.Layer() != 0)
    return;  // 位图面只覆盖地面层;高层阻挡缓存走整层重建面(随 CML 批)
             // The bitmap face covers the ground layer; upper layers ride
             // the whole-layer rebuild face (with the CML batch).
  const MPos uv = cell.ToMPos(world_->Map().Grid().Type);
  const Size size = world_->Map().MapSize();
  if (uv.U < 0 || uv.V < 0 || uv.U >= size.Width || uv.V >= size.Height)
    return;
  const std::size_t flat = static_cast<std::size_t>(uv.V) * size.Width + uv.U;
  if (flat >= vec_dirty_bitmap_.size()) {
    vec_dirty_bitmap_.resize(flat + 1, 0);
  }
  if (!vec_dirty_bitmap_[flat]) {
    vec_dirty_bitmap_[flat] = 1;
    vec_dirty_cells_.push_back(cell);
  }
}

// ———— UpdateCellCost(L437-458)————
void Locomotor::UpdateCellCost(CPos cell) {
  std::uint8_t index;
  if (cell.Layer() == 0) {
    index = world_->Map().GetTerrainIndex(cell);
  } else {
    const auto cmls = world_->CustomMovementLayers();
    if (cell.Layer() >= cmls.size() || cmls[cell.Layer()] == nullptr)
      return;
    index = cmls[cell.Layer()]->GetTerrainIndex(cell);
  }

  short cost = kMovementCostForUnreachableCell;

  if (index != 0xFF)
    cost = vec_terrain_infos_[index].Cost();

  if (cell.Layer() >= vec_cells_cost_.size() ||
      vec_cells_cost_[cell.Layer()] == nullptr)
    return;
  auto& cache = *vec_cells_cost_[cell.Layer()];
  if (vec_cell_cost_changed_.empty())
    cache.Set(cell, cost);
  else {
    const MPos uv = cell.ToMPos(world_->Map().Grid().Type);
    const short old_cost = cache.Get(uv);
    cache.Set(uv, cost);
    for (const auto& fn : vec_cell_cost_changed_)
      fn(cell, old_cost, cost);
  }
}

// ———— UpdateCellBlocking(L463-524)————
void Locomotor::UpdateCellBlocking(CPos cell) {
  // using (new PerfSample("locomotor_cache"))(B13:采样编译期开关面)
  auto& cache = *vec_blocking_cache_[cell.Layer()];
  CellFlag cell_flag = CellFlag::HasFreeSpace;
  PlayerMaskSet cell_immovable_players{};
  PlayerMaskSet cell_crushable_players = world_->AllPlayersMask;

  if (shares_cell_ && actor_map_->HasFreeSubCell(cell)) {
    cache.Set(cell, CellCache{cell_immovable_players, cell_flag,
                              cell_crushable_players});
    return;
  }

  for (Actor* actor : actor_map_->GetActorsAt(cell)) {
    PlayerMaskSet actor_immovable_players = world_->AllPlayersMask;
    PlayerMaskSet actor_crushable_players = world_->NoPlayersMask;

    // Mobile/Building 挂点(L484-488):同 IsBlockedBy 的部分覆盖面
    // The Mobile/Building hooks (L484-488): the same partial face as
    // IsBlockedBy.
    const bool is_movable = false;
    const bool is_moving = false;
    const bool is_transit_only = false;
    (void)is_moving;

    if (is_transit_only)
      cell_flag = static_cast<CellFlag>(
          static_cast<std::uint8_t>(cell_flag) |
          static_cast<std::uint8_t>(CellFlag::HasTransitOnlyActor));

    for (auto* crushable : actor->TraitsImplementing<ICrushable>()) {
      cell_flag = static_cast<CellFlag>(
          static_cast<std::uint8_t>(cell_flag) |
          static_cast<std::uint8_t>(CellFlag::HasCrushableActor));
      actor_crushable_players = actor_crushable_players.Union(
          crushable->CrushableByMask(*actor, info_.Crushes()));
    }

    if (is_moving)
      cell_flag = static_cast<CellFlag>(
          static_cast<std::uint8_t>(cell_flag) |
          static_cast<std::uint8_t>(CellFlag::HasMovingActor));
    else
      cell_flag = static_cast<CellFlag>(
          static_cast<std::uint8_t>(cell_flag) |
          static_cast<std::uint8_t>(CellFlag::HasStationaryActor));

    if (is_movable) {
      cell_flag = static_cast<CellFlag>(
          static_cast<std::uint8_t>(cell_flag) |
          static_cast<std::uint8_t>(CellFlag::HasMovableActor));
      // actorImmovablePlayers.Except(actor.Owner.AlliedPlayersMask)(L507)
      actor_immovable_players =
          actor_immovable_players.Except(actor->Owner()->AlliedPlayersMask);
    }

    // PERF: Only perform ITemporaryBlocker trait look-up if mod/map rules
    // contain any actors that are temporary blockers(上游注释)
    // (门控面下恒 false —— 同 COVERAGE)| (always false under the gated
    // face — the same COVERAGE face).
    if (world_->RulesContainTemporaryBlocker()) {
      // if (actor.TraitOrDefault<ITemporaryBlocker>() != null) ...
    }

    cell_crushable_players =
        cell_crushable_players.Intersect(actor_crushable_players);
    cell_immovable_players =
        cell_immovable_players.Union(actor_immovable_players);
  }

  cache.Set(cell,
            CellCache{cell_immovable_players, cell_flag, cell_crushable_players});
}

}  // namespace ora::mods::pathfinding
