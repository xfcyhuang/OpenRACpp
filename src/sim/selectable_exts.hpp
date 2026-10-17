#pragma once
import std;

#include "core/int2.hpp"
#include "core/polygon.hpp"
#include "platform/sdl2_input.hpp"

namespace ora::game {
class ActorInfo;
}

namespace ora::sim {

class Actor;
class FrozenActor;
struct ActorBoundsPair;

int SelectionPriority(const game::ActorInfo& info, Modifiers modifiers);
int SelectionPriority(const Actor& actor, Modifiers modifiers);

Actor* WithHighestSelectionPriority(std::span<const ActorBoundsPair> actors,
                                    int2 selection_pixel, Modifiers modifiers);
FrozenActor* WithHighestSelectionPriority(
    std::span<FrozenActor* const> actors, int2 selection_pixel,
    Modifiers modifiers);

std::vector<Actor*> SubsetWithHighestSelectionPriority(
    std::span<Actor* const> actors, Modifiers modifiers);

}  // namespace ora::sim
