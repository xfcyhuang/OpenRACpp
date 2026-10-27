#pragma once
import std;

namespace ora::fmt {

namespace xor_delta {

std::int32_t DecodeInto(std::span<const std::byte> vec_src, std::span<std::byte> vec_dest,
                        std::int32_t int4_src_offset);

}

}
