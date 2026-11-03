#pragma once

import std;

namespace ora::game::render {

[[nodiscard]] std::expected<std::size_t, std::string> lcw_decode_into(
    std::span<const std::uint8_t> span1u_src, std::size_t sz_src_offset,
    std::vector<std::uint8_t>& vec1u_dest);

}
