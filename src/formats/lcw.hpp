#pragma once
import std;

namespace ora::fmt {

namespace lcw {

std::int32_t DecodeInto(std::span<const std::byte> vec_src, std::span<std::byte> vec_dest,
                        std::int32_t int4_src_offset = 0, bool b_reverse = false);

std::vector<std::byte> Encode(std::span<const std::byte> vec_src);

}

}
