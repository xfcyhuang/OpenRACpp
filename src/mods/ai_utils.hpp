// UPSTREAM: OpenRA.Mods.Common/AIUtils.cs @e180e3225 L63-95 的
//          ClearBlockersOrders(AI 面的其余成员随 Phase 8 bot 模块批;
//          Aircraft 分支未移植 = 无该 trait 世界的等价域 —— 落
//          `continue` 的 else 同构)
//          AIUtils.cs L63-95's ClearBlockersOrders (the rest of the AI
//          face rides the Phase 8 bot-module batch; the Aircraft branch
//          unported = the equivalent domain of a world without that trait
//          — landing in the `continue` else-shape).
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

/// AIUtils.cs L63-95:ClearBlockersOrders(返回 new 分配的 Order;调用方
/// 持有)
/// AIUtils.cs L63-95: ClearBlockersOrders (returns newly allocated
/// Orders; caller owns).
std::vector<std::unique_ptr<net::Order>> ClearBlockersOrders(
    std::span<const CPos> vec_tiles,
    sim::Player& owner,
    sim::Actor* ignore_actor = nullptr);

}  // namespace ora::mods
