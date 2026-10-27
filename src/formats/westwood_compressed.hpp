#pragma once
import std;

namespace ora::fmt::westwood_compressed {

void DecodeWestwoodCompressedSample(std::span<const std::byte> vec_input, std::span<std::byte> vec_output);

}
