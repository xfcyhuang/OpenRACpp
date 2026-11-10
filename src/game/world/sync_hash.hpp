#pragma once

import std;

#include "game/world/world.hpp"

namespace ora::game::world {

[[nodiscard]] std::int32_t sync_hash_upstream(const WorldModel& w);

[[nodiscard]] std::string state_digest(const WorldModel& w);

}
