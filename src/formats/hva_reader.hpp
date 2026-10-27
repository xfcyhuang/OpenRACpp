#pragma once
import std;

#include "formats/span_reader.hpp"

namespace ora::fmt {

class HvaReader {
 public:
  HvaReader(std::span<const std::byte> vec_file, std::string_view str_file_name);

  std::uint32_t uint4_frame_count = 0;
  std::uint32_t uint4_limb_count = 0;
  std::vector<float> vec_transforms;
};

std::optional<std::array<float, 16>> MatrixInverse(const std::array<float, 16>& arr_m);

}
