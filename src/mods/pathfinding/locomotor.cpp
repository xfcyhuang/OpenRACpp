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

  std::vector<LocomotorInfo::TerrainInfo> terrain_infos;
  const game::Ruleset* rules = nullptr;
  if (rules != nullptr) {}

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

Locomotor::Locomotor(Actor& self, LocomotorInfo info)
    : info_{std::move(info)}, world_{&self.world()} {
  shares_cell_ = info_.SharesCell();

  const auto& terrain_info = world_->Map().Rules().TerrainInfo();
  const auto& terrain_types = terrain_info.TerrainTypes();
  vec_terrain_infos_.resize(terrain_types.size(),
                            LocomotorInfo::TerrainInfo::Impassable());
  for (std::size_t i = 0; i < terrain_types.size(); i++) {

    for (const auto& [key, ti] : info_.TerrainSpeeds())
      if (key == terrain_types[i].Type) {
        vec_terrain_infos_[i] = ti;
        break;
      }
  }
}

short Locomotor::MovementCostForCell(CPos cell, const CPos* from_cell) const {
  if (!world_->Map().Contains(cell))
    return kMovementCostForUnreachableCell;

  if (from_cell != nullptr && cell.Layer() == 0 && from_cell->Layer() == 0 &&
      world_->Map().Grid().MaximumTerrainHeight() > 0) {
    const auto& height_layer = world_->Map().Height();
    if (std::abs(static_cast<int>(height_layer.Get(cell)) -
                 static_cast<int>(height_layer.Get(*from_cell))) > 1)
      return kMovementCostForUnreachableCell;
  }

  return vec_cells_cost_[cell.Layer()]->Get(cell);
}

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

bool Locomotor::CanMoveFreelyInto(Actor* actor, CPos cell, SubCell sub_cell,
                                  BlockedByActor check, Actor* ignore_actor,
                                  bool ignore_self) {

  if (check == BlockedByActor::None)
    return true;

  const CellCache cell_cache = GetCache(cell);
  const CellFlag cell_flag = cell_cache.cell_flag;

  if (cell_flag == CellFlag::HasFreeSpace)
    return true;

  if (actor == nullptr)
    return false;

  if (cell_cache.crushable.Overlaps(actor->Owner()->PlayerMask))
    return true;

  if (check <= BlockedByActor::Stationary &&
      !HasCellFlag(cell_flag, CellFlag::HasStationaryActor))
    return true;

  if (check <= BlockedByActor::Immovable &&
      !cell_cache.immovable.Overlaps(actor->Owner()->PlayerMask))
    return true;

  if (ignore_actor == nullptr && !ignore_self &&
      sub_cell == SubCell::FullCell &&
      !HasCellFlag(cell_flag, CellFlag::HasTemporaryBlocker) &&
      !HasCellFlag(cell_flag, CellFlag::HasTransitOnlyActor)) {

    if (check == BlockedByActor::All)
      return false;

    if (!HasCellFlag(cell_flag, CellFlag::HasCrushableActor))
      return false;

    if (!HasCellFlag(cell_flag, CellFlag::HasMovableActor))
      return false;

    if (check == BlockedByActor::Stationary &&
        !HasCellFlag(cell_flag, CellFlag::HasMovingActor))
      return false;
  }

  const std::vector<Actor*> other_actors =
      sub_cell == SubCell::FullCell
          ? actor_map_->GetActorsAt(cell)
          : actor_map_->GetActorsAt(cell, sub_cell);

  if (ignore_self) {

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

  for (Actor* other_actor : other_actors)
    if (IsBlockedBy(actor, other_actor, ignore_actor, cell, check, cell_flag))
      return false;

  return true;
}

bool Locomotor::CanStayInCell(CPos cell) {
  if (!world_->Map().Contains(cell))
    return false;

  return !HasCellFlag(GetCache(cell).cell_flag,
                      CellFlag::HasTransitOnlyActor);
}

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

bool Locomotor::IsBlockedBy(Actor* actor, Actor* other_actor,
                            Actor* ignore_actor, CPos cell,
                            BlockedByActor check, CellFlag cell_flag) {
  if (other_actor == ignore_actor)
    return false;

  const bool other_is_movable = false;
  const bool other_is_moving = false;
  (void)other_is_moving;

  if (check <= BlockedByActor::Immovable &&
      HasCellFlag(cell_flag, CellFlag::HasMovableActor) && other_is_movable &&
      actor->Owner()->RelationshipWith(other_actor->Owner()) ==
          PlayerRelationship::Ally)
    return false;

  if (check <= BlockedByActor::Stationary &&
      HasCellFlag(cell_flag, CellFlag::HasMovingActor) && other_is_moving)
    return false;

  if (HasCellFlag(cell_flag, CellFlag::HasTemporaryBlocker)) {

  }

  if (HasCellFlag(cell_flag, CellFlag::HasTransitOnlyActor)) {

  }

  if (!HasCellFlag(cell_flag, CellFlag::HasCrushableActor) ||
      info_.Crushes().IsEmpty())
    return true;

  for (auto* crushable : other_actor->TraitsImplementing<ICrushable>())
    if (crushable->CrushableBy(*other_actor, *actor, info_.Crushes()))
      return false;

  return true;
}

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

Locomotor::CellCache Locomotor::GetCache(CPos cell) {

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

    const CPos dirty_cell = cell;
    std::erase_if(vec_dirty_cells_, [&](const CPos& c) { return c == dirty_cell; });
    UpdateCellBlocking(cell);
  }

  return vec_blocking_cache_[cell.Layer()]->Get(cell);
}

void Locomotor::CellUpdated(CPos cell) {
  if (cell.Layer() != 0)
    return;

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

void Locomotor::UpdateCellBlocking(CPos cell) {

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

      actor_immovable_players =
          actor_immovable_players.Except(actor->Owner()->AlliedPlayersMask);
    }

    if (world_->RulesContainTemporaryBlocker()) {

    }

    cell_crushable_players =
        cell_crushable_players.Intersect(actor_crushable_players);
    cell_immovable_players =
        cell_immovable_players.Union(actor_immovable_players);
  }

  cache.Set(cell,
            CellCache{cell_immovable_players, cell_flag, cell_crushable_players});
}

}
