#pragma once
import std;

#include "formats/span_reader.hpp"

namespace ora::fs {

enum class PackageHashType { Classic, CRC32 };

struct PackageEntry {
  static constexpr int int4_size = 12;

  std::uint32_t uint4_hash{};
  std::uint32_t uint4_offset{};
  std::uint32_t uint4_length{};

  PackageEntry() = default;
  PackageEntry(std::uint32_t uint4_hash_in, std::uint32_t uint4_offset_in, std::uint32_t uint4_length_in)
      : uint4_hash{uint4_hash_in}, uint4_offset{uint4_offset_in}, uint4_length{uint4_length_in} {}

  explicit PackageEntry(fmt::SpanReader& reader)
      : uint4_hash{reader.ReadUInt32()}, uint4_offset{reader.ReadUInt32()},
        uint4_length{reader.ReadUInt32()} {}

  static std::uint32_t HashFilename(std::string_view str_name, PackageHashType enum_type);
};

}
