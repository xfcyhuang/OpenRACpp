#include "sim/shroud.hpp"

#include "sim/actor.hpp"
#include "sim/player.hpp"
#include "net/session.hpp"
#include "sim/sync_hash.hpp"
#include "sim/world.hpp"

namespace ora::sim {

Shroud::Shroud(Actor& self, const ShroudInfoData& info)
    : info_{info},
      map_{self.world().Map()},
      layer_passive_visible_count_{map_},
      layer_visible_count_{map_},
      layer_generated_shroud_count_{map_},
      layer_explored_{map_},
      layer_touched_{map_},
      layer_resolved_type_{map_} {

  b_any_cell_touched_ = true;

}

void Shroud::Created(Actor& self) {

  const auto bool_option_or_default =
      [](const World& world, std::string_view key, bool def) {
        return const_cast<World&>(world).LobbyInfo().global_settings
            .OptionOrDefault(key, def);
      };

  b_fog_enabled_ = bool_option_or_default(
      self.world(), "fog", info_.b_fog_checkbox_enabled);

  b_explore_map_enabled_ = bool_option_or_default(
      self.world(), "explored", info_.b_explored_map_checkbox_enabled);
  if (b_explore_map_enabled_)
    ExploreAll();

  if (!b_fog_enabled_ && b_explore_map_enabled_)
    int4_revealed_cells_ = static_cast<int>(map_.ProjectedCells().size());
}

void Shroud::SetDisabled(bool value) {

  if (b_disabled_ == value)
    return;

  b_disabled_ = value;
  b_disabled_changed_ = true;
}

void Shroud::Tick(Actor& self) {

  if (!b_any_cell_touched_ && !b_disabled_changed_)
    return;

  b_any_cell_touched_ = false;

  if (vec_on_shroud_changed_.empty()) {
    b_disabled_changed_ = false;
    return;
  }

  if (b_disabled_changed_) {
    layer_touched_.SetAll(false);
    const int max_index = layer_touched_.MaxIndex();
    for (int index = 0; index < max_index; index++)
      UpdateCell(index, self);
  } else {

    const int max_index = layer_touched_.MaxIndex();
    int index = 0;
    while (index < max_index) {

      while (index < max_index && !layer_touched_.Get(index))
        ++index;
      if (index >= max_index)
        break;

      layer_touched_.Set(index, false);
      UpdateCell(index, self);
      ++index;
    }
  }

  int4_hash_ = sync::HashPlayer(self.Owner()) + self.world().WorldTick();
  b_disabled_changed_ = false;
}

void Shroud::UpdateCell(int index, Actor& self) {

  auto type = ShroudCellType::Shroud;

  if (layer_explored_.Get(index)) {
    auto count = layer_visible_count_.Get(index);
    if (!b_shroud_generation_enabled_ || count > 0 ||
        layer_generated_shroud_count_.Get(index) == 0) {
      if (b_passive_visibility_enabled_)
        count += layer_passive_visible_count_.Get(index);

      type = count > 0 ? ShroudCellType::Visible : ShroudCellType::Fog;
    }
  }

  const auto old_resolved_type = layer_resolved_type_.Get(index);
  if (type != old_resolved_type || b_disabled_changed_) {
    layer_resolved_type_.Set(index, type);
    const PPos puv = layer_touched_.PPosFromIndex(index);
    if (map_.Contains(puv))
      for (const auto& fn : vec_on_shroud_changed_)
        if (fn)
          fn(puv);

    if (!b_disabled_changed_ && (b_fog_enabled_ || !b_explore_map_enabled_)) {
      if (type == ShroudCellType::Visible)
        int4_revealed_cells_++;
      else if (b_fog_enabled_ && old_resolved_type == ShroudCellType::Visible)
        int4_revealed_cells_--;
    }

    if (self.Owner()->PlayerWinState() == WinState::Lost)
      int4_revealed_cells_ = 0;
  }
}

std::vector<PPos> Shroud::ProjectedCellsInRange(const map::Map& map,
                                                const WPos& pos,
                                                const WDist& min_range,
                                                const WDist& max_range,
                                                int max_height_delta) {

  const int r = (max_range.Length + 1023 + 512) / 1024;
  const int min_limit = min_range.LengthSquared();
  const int max_limit = max_range.LengthSquared();

  const WPos projected_pos = pos - WVec{0, pos.Z, pos.Z};
  const CPos projected_cell = map.CellContaining(projected_pos);
  const int projected_height = pos.Z / 512;

  std::vector<PPos> vec_result;
  for (const CPos& c :
       map.FindTilesInAnnulus(projected_cell, min_range.Length / 1024, r,
                              true)) {
    const int dist =
        (map.CenterOfCell(c) - projected_pos).HorizontalLengthSquared();
    if (dist <= max_limit && (dist == 0 || dist > min_limit)) {
      const PPos puv = ToPPos(c.ToMPos(map.Grid().Type));
      if (max_height_delta < 0 ||
          map.ProjectedHeight(puv) < projected_height + max_height_delta)
        vec_result.push_back(puv);
    }
  }

  return vec_result;
}

std::vector<PPos> Shroud::ProjectedCellsInRange(const map::Map& map,
                                                const CPos& cell,
                                                const WDist& range,
                                                int max_height_delta) {

  return ProjectedCellsInRange(map, map.CenterOfCell(cell), WDist{0}, range,
                               max_height_delta);
}

void Shroud::AddSource(const void* key, ShroudSourceType type,
                       std::vector<PPos> vec_projected_cells) {

  if (map_sources_.contains(key))
    throw std::runtime_error("Attempting to add duplicate shroud source");
  map_sources_.emplace(key,
                       ShroudSource{type, std::move(vec_projected_cells)});

  for (const PPos& puv : map_sources_[key].vec_projected_cells) {

    if (!map_.Contains(puv))
      continue;

    const int index = layer_touched_.Index(puv);
    layer_touched_.Set(index, true);
    b_any_cell_touched_ = true;
    switch (type) {
      case ShroudSourceType::PassiveVisibility:
        b_passive_visibility_enabled_ = true;
        layer_passive_visible_count_.Set(
            index, layer_passive_visible_count_.Get(index) + 1);
        layer_explored_.Set(index, true);
        break;
      case ShroudSourceType::Visibility:
        layer_visible_count_.Set(index, layer_visible_count_.Get(index) + 1);
        layer_explored_.Set(index, true);
        break;
      case ShroudSourceType::Shroud:
        b_shroud_generation_enabled_ = true;
        layer_generated_shroud_count_.Set(
            index, layer_generated_shroud_count_.Get(index) + 1);
        break;
    }
  }
}

void Shroud::RemoveSource(const void* key) {

  const auto it = map_sources_.find(key);
  if (it == map_sources_.end())
    return;

  for (const PPos& puv : it->second.vec_projected_cells) {

    if (map_.Contains(puv)) {
      const int index = layer_touched_.Index(puv);
      layer_touched_.Set(index, true);
      b_any_cell_touched_ = true;
      switch (it->second.type) {
        case ShroudSourceType::PassiveVisibility:
          layer_passive_visible_count_.Set(
              index, layer_passive_visible_count_.Get(index) - 1);
          break;
        case ShroudSourceType::Visibility:
          layer_visible_count_.Set(index,
                                   layer_visible_count_.Get(index) - 1);
          break;
        case ShroudSourceType::Shroud:
          layer_generated_shroud_count_.Set(
              index, layer_generated_shroud_count_.Get(index) - 1);
          break;
      }
    }
  }

  map_sources_.erase(it);
}

void Shroud::ExploreProjectedCells(const std::vector<PPos>& vec_cells) {

  for (const PPos& puv : vec_cells) {
    if (map_.Contains(puv)) {
      const int index = layer_touched_.Index(puv);
      if (!layer_explored_.Get(index)) {
        layer_touched_.Set(index, true);
        b_any_cell_touched_ = true;
        layer_explored_.Set(index, true);
      }
    }
  }
}

void Shroud::Explore(const Shroud& s) {

  if (map_.Bounds() != s.map_.Bounds())
    throw std::invalid_argument(
        "The map bounds of these shrouds do not match.");

  for (const PPos& puv : map_.ProjectedCells()) {
    const int index = layer_touched_.Index(puv);
    if (!layer_explored_.Get(index) && s.layer_explored_.Get(index)) {
      layer_touched_.Set(index, true);
      b_any_cell_touched_ = true;
      layer_explored_.Set(index, true);
    }
  }
}

void Shroud::ExploreAll() {

  for (const PPos& puv : map_.ProjectedCells()) {
    const int index = layer_touched_.Index(puv);
    if (!layer_explored_.Get(index)) {
      layer_touched_.Set(index, true);
      b_any_cell_touched_ = true;
      layer_explored_.Set(index, true);
    }
  }
}

void Shroud::ResetExploration() {

  for (const PPos& puv : map_.ProjectedCells()) {
    const int index = layer_touched_.Index(puv);
    layer_touched_.Set(index, true);
    layer_explored_.Set(index, layer_visible_count_.Get(index) +
                                   layer_passive_visible_count_.Get(index) >
                               0);
  }

  b_any_cell_touched_ = true;
}

bool Shroud::IsExplored(const WPos& pos) const {

  return IsExplored(map_.ProjectedCellCovering(pos));
}

bool Shroud::IsExplored(const CPos& cell) const {

  return IsExplored(cell.ToMPos(map_.Grid().Type));
}

bool Shroud::IsExplored(const MPos& uv) const {

  if (!map_.Contains(uv))
    return false;

  for (const PPos& puv : map_.ProjectedCellsCovering(uv))
    if (IsExplored(puv))
      return true;

  return false;
}

bool Shroud::IsExplored(const PPos& puv) const {

  if (Disabled())
    return map_.Contains(puv);

  return layer_resolved_type_.Contains(puv) &&
         layer_resolved_type_.Get(puv) > ShroudCellType::Shroud;
}

bool Shroud::IsVisible(const WPos& pos) const {

  return IsVisible(map_.ProjectedCellCovering(pos));
}

bool Shroud::IsVisible(const CPos& cell) const {

  return IsVisible(cell.ToMPos(map_.Grid().Type));
}

bool Shroud::IsVisible(const MPos& uv) const {

  for (const PPos& puv : map_.ProjectedCellsCovering(uv))
    if (IsVisible(puv))
      return true;

  return false;
}

bool Shroud::IsVisible(const PPos& puv) const {

  if (!FogEnabled())
    return map_.Contains(puv);

  return layer_resolved_type_.Contains(puv) &&
         layer_resolved_type_.Get(puv) == ShroudCellType::Visible;
}

bool Shroud::Contains(const PPos& uv) const {

  return layer_explored_.Contains(uv);
}

CellVisibility Shroud::GetVisibility(const WPos& pos) const {

  return GetVisibility(map_.ProjectedCellCovering(pos));
}

CellVisibility Shroud::GetVisibility(const PPos& puv) const {

  auto state = CellVisibility::Hidden;

  if (Disabled()) {
    if (b_fog_enabled_) {

      if (layer_resolved_type_.Contains(puv)) {
        state = static_cast<CellVisibility>(
            static_cast<std::uint8_t>(state) |
            static_cast<std::uint8_t>(CellVisibility::Explored));

        if (layer_resolved_type_.Get(puv) == ShroudCellType::Visible)
          state = static_cast<CellVisibility>(
              static_cast<std::uint8_t>(state) |
              static_cast<std::uint8_t>(CellVisibility::Visible));
      }
    } else if (map_.Contains(puv)) {
      state = static_cast<CellVisibility>(
          static_cast<std::uint8_t>(state) |
          static_cast<std::uint8_t>(CellVisibility::Explored) |
          static_cast<std::uint8_t>(CellVisibility::Visible));
    }
  } else {
    if (b_fog_enabled_) {

      if (layer_resolved_type_.Contains(puv)) {
        const auto rt = layer_resolved_type_.Get(puv);
        if (rt == ShroudCellType::Visible)
          state = static_cast<CellVisibility>(
              static_cast<std::uint8_t>(state) |
              static_cast<std::uint8_t>(CellVisibility::Explored) |
              static_cast<std::uint8_t>(CellVisibility::Visible));
        else if (rt > ShroudCellType::Shroud)
          state = static_cast<CellVisibility>(
              static_cast<std::uint8_t>(state) |
              static_cast<std::uint8_t>(CellVisibility::Explored));
      }
    } else if (layer_resolved_type_.Contains(puv)) {

      state = static_cast<CellVisibility>(
          static_cast<std::uint8_t>(state) |
          static_cast<std::uint8_t>(CellVisibility::Visible));

      if (layer_resolved_type_.Get(puv) > ShroudCellType::Shroud)
        state = static_cast<CellVisibility>(
            static_cast<std::uint8_t>(state) |
            static_cast<std::uint8_t>(CellVisibility::Explored));
    }
  }

  return state;
}

}

namespace ora::sim {

static_assert(gen::TypeId::OpenRA_Traits_Shroud != gen::TypeId{0});

int ShroudSyncHash(const ISync* s) {
  const auto* shroud = static_cast<const Shroud*>(s);
  return sync::CombineSyncHash(0, sync::HashBool(shroud->Disabled()));
}

const bool b_shroud_sync_registered = [] {
  RegisterSyncHashFunction("OpenRA.Traits.Shroud", &ShroudSyncHash);
  return true;
}();
[[maybe_unused]] const bool* b_shroud_sync_registered_anchor =
    &b_shroud_sync_registered;

}
