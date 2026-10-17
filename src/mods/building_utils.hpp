// UPSTREAM: OpenRA.Mods.Common/Traits/Buildings/BuildingUtils.cs @e180e3225
//          L21-139 全文(world 扩展形态)
//          The whole of BuildingUtils.cs L21-139 (the world-extension
//          form).
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

/// 按具体 Info 记录名取 trait 记录(上游 TraitInfo<T> 的具体类键等价:
/// ITraitInfoInterface 全集 + str_name 过滤)
/// The trait record by its concrete Info record name (the concrete-class-
/// key equivalent of upstream TraitInfo<T>: the full ITraitInfoInterface
/// set filtered by str_name).
std::vector<const meta::RecordObject*> TraitInfoRecords(
    const game::ActorInfo& info, std::string_view str_record_name);
const meta::RecordObject* FindTraitInfoRecord(const game::ActorInfo& info,
                                               std::string_view str_record_name);

/// BuildingUtils.cs L22-80:IsCellBuildable(world 扩展)
/// BuildingUtils.cs L22-80: IsCellBuildable (the world extension).
bool IsCellBuildable(sim::World& world, CPos cell, const game::ActorInfo* ai,
                     const BuildingInfoData& bi, sim::Actor* to_ignore = nullptr);

/// BuildingUtils.cs L82-88:CanPlaceBuilding(world 扩展)
/// BuildingUtils.cs L82-88: CanPlaceBuilding (the world extension).
bool CanPlaceBuilding(sim::World& world, CPos cell, const game::ActorInfo* ai,
                      const BuildingInfoData& bi, sim::Actor* to_ignore = nullptr);

/// BuildingUtils.cs L90-137:GetLineBuildCells((Cell, Actor) 对;1x1 假设)
/// BuildingUtils.cs L90-137: GetLineBuildCells (the (Cell, Actor) pairs;
/// the 1x1 assumption).
std::vector<std::pair<CPos, sim::Actor*>> GetLineBuildCells(
    sim::World& world, CPos cell, const game::ActorInfo& ai,
    const BuildingInfoData& bi, sim::Player& owner);

}  // namespace ora::mods
