#pragma once
import std;

namespace ora::fmt {

namespace lzo {

std::int32_t DecodeInto(std::span<const std::byte> vec_src, std::size_t st_src_offset, std::size_t st_src_length,
                        std::span<std::byte> vec_dest, std::size_t st_dest_offset, std::uint32_t& uint4_dest_length);

}

}
