#pragma once
import std;

namespace ora::fmt {

namespace rle_zeros {

void DecodeInto(std::span<const std::byte> vec_src, std::span<std::byte> vec_dest, std::size_t st_dest_index);

}

}
