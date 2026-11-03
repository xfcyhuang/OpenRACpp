#pragma once

import std;

namespace ora::game::filesystem {

[[nodiscard]] std::expected<std::vector<std::uint8_t>, std::string> inflate_raw(
    std::span<const std::uint8_t> span1u_input);

}
