#pragma once
import std;

#include "core/cell_pos.hpp"
#include "net/order.hpp"
#include "sim/trait_interfaces.hpp"

namespace ora::sim {
class Actor;
class Player;
class World;
}

namespace ora::mods {

namespace util {
std::vector<CPos> ExpandFootprintCells(std::span<const CPos> vec_cells,
                                       bool b_allow_diagonal);
}

std::vector<std::unique_ptr<net::Order>> ClearBlockersOrders(
    std::span<const CPos> vec_tiles,
    sim::Player& owner,
    sim::Actor* ignore_actor = nullptr);

}
