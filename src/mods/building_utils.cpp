#include "mods/building_utils.hpp"

#include "game/ruleset.hpp"
#include "mods/building_influence.hpp"

#include "game/actor_info.hpp"
#include "meta/generic_record.hpp"
#include "mods/line_build.hpp"
#include "mods/place_building.hpp"
#include "mods/plug.hpp"
#include "sim/actor.hpp"
#include "sim/actor_map.hpp"
#include "sim/player.hpp"
#include "sim/trait_interfaces.hpp"
#include "sim/world.hpp"

namespace ora::mods {

std::vector<const meta::RecordObject*> TraitInfoRecords(
    const game::ActorInfo& info, std::string_view str_record_name) {
  std::vector<const meta::RecordObject*> vec_ret;
  for (const meta::RecordObject* rec :
       info.TraitInfosByInterface("OpenRA.Traits.ITraitInfoInterface"))
    if (rec->record_desc().str_name == str_record_name)
      vec_ret.push_back(rec);
  return vec_ret;
}

const meta::RecordObject* FindTraitInfoRecord(const game::ActorInfo& info,
                                               std::string_view str_record_name) {
  const std::vector<const meta::RecordObject*> vec =
      TraitInfoRecords(info, str_record_name);
  return vec.empty() ? nullptr : vec.front();
}

bool IsCellBuildable(sim::World& world, CPos cell, const game::ActorInfo* ai,
                     const BuildingInfoData& bi, sim::Actor* to_ignore) {
  if (!world.Map().Contains(cell))
    return false;

  if (!bi.b_allow_invalid_placement) {
    const bool b_check_replacements = ai != nullptr &&
                                      !TraitInfoRecords(*ai, "ReplacementInfo")
                                            .empty();
    std::set<std::string> set_accepted_replacements;

    bool b_found_actors = false;
    for (sim::Actor* a : world.ActorMapFace()->GetActorsAt(cell)) {
      if (a == to_ignore)
        continue;

      if (!b_check_replacements)
        return false;

      b_found_actors = true;
      for (Replaceable* r : a->TraitsImplementing<Replaceable>())
        if (!r->IsTraitDisabled())
          for (const std::string& str_type : r->Info().vec_types)
            set_accepted_replacements.insert(str_type);
    }

    const bool b_found_building =
        world.WorldActor()->TraitOrDefault<BuildingInfluence>() != nullptr &&
        world.WorldActor()->Trait<BuildingInfluence>()->AnyBuildingAt(cell);
    if (b_found_actors || b_found_building) {
      if (set_accepted_replacements.empty())
        return false;

      const bool b_found_replacement = [&] {
        for (const meta::RecordObject* rec :
             TraitInfoRecords(*ai, "ReplacementInfo")) {
          const ReplacementInfoData data = ReplacementInfoData::Parse(*rec);
          for (const std::string& str_type : data.vec_replaceable_types)
            if (set_accepted_replacements.count(str_type) != 0)
              return true;
        }
        return false;
      }();
      if (!b_found_replacement)
        return false;
    }
  } else {
    if (world.WorldActor()->TraitOrDefault<BuildingInfluence>() != nullptr &&
        world.WorldActor()->Trait<BuildingInfluence>()->AnyBuildingAt(cell))
      return false;
  }

  if (world.Map().Ramp().Get(cell) != 0)
    return false;

  const std::string& str_terrain = world.Map().GetTerrainInfo(cell).Type;
  for (const std::string& str_type : bi.vec_terrain_types)
    if (str_type == str_terrain)
      return true;
  return false;
}

bool CanPlaceBuilding(sim::World& world, CPos cell, const game::ActorInfo* ai,
                      const BuildingInfoData& bi, sim::Actor* to_ignore) {
  if (bi.b_allow_invalid_placement)
    return true;

  for (const CPos t : bi.Tiles(cell))
    if (!world.Map().Contains(t) ||
        !IsCellBuildable(world, t, ai, bi, to_ignore))
      return false;
  return true;
}

std::vector<std::pair<CPos, sim::Actor*>> GetLineBuildCells(
    sim::World& world, CPos cell, const game::ActorInfo& ai,
    const BuildingInfoData& bi, sim::Player& owner) {
  std::vector<std::pair<CPos, sim::Actor*>> vec_out;

  const meta::RecordObject* rec_line_build = FindTraitInfoRecord(ai, "LineBuildInfo");
  const LineBuildInfoData lbi =
      rec_line_build != nullptr ? LineBuildInfoData::Parse(*rec_line_build)
                                : LineBuildInfoData{};

  const CPos top_left = cell;

  if (IsCellBuildable(world, top_left, &ai, bi))
    vec_out.emplace_back(top_left, nullptr);

  const CVec vecs[]{CVec{1, 0}, CVec{0, 1}, CVec{-1, 0}, CVec{0, -1}};
  int dirs[4] = {0, 0, 0, 0};
  sim::Actor* connectors[4] = {nullptr, nullptr, nullptr, nullptr};

  for (int d = 0; d < 4; d++) {
    for (int i = 1; i < lbi.int4_range; i++) {
      if (dirs[d] != 0)
        continue;

      const game::ActorInfo* segment_info = &ai;
      const BuildingInfoData* segment_building_info = &bi;
      BuildingInfoData parsed_segment;
      if (!lbi.str_segment_type.empty()) {
        const game::ActorInfo* found =
            world.Map().Rules().FindActor(lbi.str_segment_type);
        if (found != nullptr) {
          segment_info = found;
          if (const meta::RecordObject* rec_b =
                  FindTraitInfoRecord(*segment_info, "BuildingInfo"))
            parsed_segment = BuildingInfoData::Parse(*rec_b);
          segment_building_info = &parsed_segment;
        }
      }

      const CPos c{top_left.X() + i * vecs[d].X, top_left.Y() + i * vecs[d].Y};
      if (IsCellBuildable(world, c, segment_info, *segment_building_info) ||
          owner.GetShroud() == nullptr || !owner.GetShroud()->IsExplored(c))
        continue;

      for (sim::Actor* a : world.ActorMapFace()->GetActorsAt(c)) {
        bool b_matches = false;
        for (const meta::RecordObject* rec_node :
             TraitInfoRecords(*a->Info(), "LineBuildNodeInfo")) {
          const LineBuildNodeInfoData node =
              LineBuildNodeInfoData::Parse(*rec_node);

          bool b_type_overlap = false;
          for (const std::string& str_t : node.vec_types)
            for (const std::string& str_n : lbi.vec_node_types)
              if (str_t == str_n)
                b_type_overlap = true;

          bool b_connection = false;
          for (const CVec& vec_connection : node.vec_connections)
            if (vec_connection.X == vecs[d].X &&
                vec_connection.Y == vecs[d].Y)
              b_connection = true;

          if (b_type_overlap && b_connection)
            b_matches = true;
        }
        if (b_matches) {
          connectors[d] = a;
          break;
        }
      }

      dirs[d] = connectors[d] != nullptr ? i : -1;
    }

    if (dirs[d] > 0)
      for (int i = 1; i < dirs[d]; i++)
        vec_out.emplace_back(
            CPos{top_left.X() + i * vecs[d].X, top_left.Y() + i * vecs[d].Y},
            connectors[d]);
  }

  return vec_out;
}

}
