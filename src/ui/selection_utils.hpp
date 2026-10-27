#pragma once
import std;

#include "core/int2.hpp"
#include "platform/sdl2_input.hpp"

namespace ora::sim {
class Actor;
class Player;
class World;
}

namespace ora::gfx {
class WorldRenderer;
}

namespace ora::ui {

std::vector<sim::Actor*> SelectActorsOnScreen(
    sim::World& world, gfx::WorldRenderer& wr,
    const std::vector<std::string>* vec_selection_classes,
    std::span<sim::Player* const> vec_players);

std::vector<sim::Actor*> SelectActorsInWorld(
    sim::World& world, const std::vector<std::string>* vec_selection_classes,
    std::span<sim::Player* const> vec_players);

std::vector<sim::Actor*> SelectActorsByOwnerAndSelectionClass(
    std::span<sim::Actor* const> vec_actors, std::span<sim::Player* const> vec_owners,
    const std::vector<std::string>* vec_selection_classes);

std::vector<sim::Actor*> SelectHighestPriorityActorAtPoint(sim::World& world, int2 a,
                                                           Modifiers modifiers);

std::vector<sim::Actor*> SelectActorsInBoxWithDeadzone(sim::World& world, int2 a, int2 b,
                                                       Modifiers modifiers);

std::vector<sim::Player*> GetPlayersToIncludeInSelection(sim::World& world);

}
