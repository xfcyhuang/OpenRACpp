#pragma once
import std;

#include "formats/span_reader.hpp"

namespace ora::fmt {

struct IdxEntry {
  std::string str_filename;
  std::uint32_t uint4_offset = 0;
  std::uint32_t uint4_length = 0;
  std::uint32_t uint4_sample_rate = 0;
  std::uint32_t uint4_flags = 0;
  std::uint32_t uint4_chunk_size = 0;

  [[nodiscard]] std::string ToString() const;
};

class IdxReader {
 public:
  explicit IdxReader(std::span<const std::byte> vec_file);

  std::int32_t int4_sound_count = 0;
  std::vector<IdxEntry> vec_entries;
};

}
