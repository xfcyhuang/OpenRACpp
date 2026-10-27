#pragma once
import std;

#include "core/cell_pos.hpp"
#include "core/cvec.hpp"
#include "mods/building.hpp"

namespace ora::meta {
class RecordObject;
}

namespace ora::game {
class ActorInfo;
}

namespace ora::sim {
class Actor;
class Player;
class World;
}

namespace ora::mods {

std::vector<const meta::RecordObject*> TraitInfoRecords(
    const game::ActorInfo& info, std::string_view str_record_name);
const meta::RecordObject* FindTraitInfoRecord(const game::ActorInfo& info,
                                               std::string_view str_record_name);

bool IsCellBuildable(sim::World& world, CPos cell, const game::ActorInfo* ai,
                     const BuildingInfoData& bi, sim::Actor* to_ignore = nullptr);

bool CanPlaceBuilding(sim::World& world, CPos cell, const game::ActorInfo* ai,
                      const BuildingInfoData& bi, sim::Actor* to_ignore = nullptr);

std::vector<std::pair<CPos, sim::Actor*>> GetLineBuildCells(
    sim::World& world, CPos cell, const game::ActorInfo& ai,
    const BuildingInfoData& bi, sim::Player& owner);

}
