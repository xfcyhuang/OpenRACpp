#pragma once
import std;

#include "formats/span_reader.hpp"

namespace ora::fmt {

enum class NormalType : std::uint8_t {
  TiberianSun = 2,
  RedAlert2 = 4,
};

struct VxlElement {
  std::uint8_t uint1_color;
  std::uint8_t uint1_normal;
};

struct VxlLimb {
  std::string str_name;
  float fp4_scale = 0.0f;
  std::array<float, 6> arr_bounds{};
  std::array<std::uint8_t, 3> arr_size{};
  NormalType enum_type{};

  std::uint32_t uint4_voxel_count = 0;
  std::vector<std::vector<std::pair<std::uint8_t, VxlElement>>> vec_voxel_map;
  std::vector<std::uint8_t> vec_column_present;
};

class VxlReader {
 public:
  explicit VxlReader(std::span<const std::byte> vec_file);

  std::uint32_t uint4_limb_count = 0;
  std::vector<VxlLimb> vec_limbs;
};

std::string ReadAscii(std::span<const std::byte> vec_bytes);

}
