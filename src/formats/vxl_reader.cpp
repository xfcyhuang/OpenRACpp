// UPSTREAM: OpenRA.Mods.Cnc/FileFormats/VxlReader.cs @b6fc03f —— 逐句照抄
// (Stream → SpanReader;两遍列扫描保留:先计数后建图)。
// [UPSTREAM continued] VxlReader.cs copied statement by statement (Stream →
// SpanReader; the two column passes kept: count first, then build the map).
#include "formats/vxl_reader.hpp"

namespace ora::fmt {

std::string ReadAscii(std::span<const std::byte> vec_bytes) {
  auto str_out = std::string{};
  str_out.reserve(vec_bytes.size());
  for (const auto byte_b : vec_bytes)
    str_out.push_back(static_cast<std::uint8_t>(byte_b) <= 0x7F ? static_cast<char>(byte_b) : '?');
  return str_out;
}

namespace {

/// VxlReader.ReadVoxelData(L40-102):baseSize = Size[0]×Size[1] 个 i32 列
/// 偏移 + 等长跳过;先扫全部列数出 VoxelCount,再逐列建 Dictionary。
/// VxlReader.ReadVoxelData (L40-102): baseSize = Size[0]×Size[1] i32 column
/// offsets + an equal-length skip; first scan every column counting
/// VoxelCount, then build the dictionaries column by column.
void ReadVoxelData(SpanReader& reader, VxlLimb& limb) {
  const auto int4_base_size = static_cast<std::int32_t>(limb.arr_size[0]) * limb.arr_size[1];
  auto vec_col_start = std::vector<std::int32_t>(static_cast<std::size_t>(int4_base_size));
  for (auto& int4_col : vec_col_start)
    int4_col = reader.ReadInt32();
  reader.Skip(4 * static_cast<std::int64_t>(int4_base_size));
  const auto int8_data_start = reader.Position();

  // Count the voxels in this limb
  limb.uint4_voxel_count = 0;
  for (auto int4_i = 0; int4_i < int4_base_size; int4_i++) {
    // Empty column
    if (vec_col_start[static_cast<std::size_t>(int4_i)] == -1)
      continue;

    reader.Seek(int8_data_start + vec_col_start[static_cast<std::size_t>(int4_i)]);
    auto int4_z = 0;
    do {
      int4_z += reader.ReadUInt8();
      const auto uint1_count = reader.ReadUInt8();
      int4_z += uint1_count;
      limb.uint4_voxel_count += uint1_count;
      reader.Skip(2 * uint1_count + 1);
    } while (int4_z < limb.arr_size[2]);
  }

  // Read the data
  limb.vec_voxel_map.assign(static_cast<std::size_t>(limb.arr_size[0]) * limb.arr_size[1], {});
  limb.vec_column_present.assign(static_cast<std::size_t>(limb.arr_size[0]) * limb.arr_size[1], 0);
  for (auto int4_i = 0; int4_i < int4_base_size; int4_i++) {
    // Empty column
    if (vec_col_start[static_cast<std::size_t>(int4_i)] == -1)
      continue;

    reader.Seek(int8_data_start + vec_col_start[static_cast<std::size_t>(int4_i)]);

    const auto uint1_x = static_cast<std::uint8_t>(int4_i % limb.arr_size[0]);
    const auto uint1_y = static_cast<std::uint8_t>(int4_i / limb.arr_size[0]);
    auto uint1_z = std::uint8_t{0};
    const auto st_slot = static_cast<std::size_t>(uint1_x) + static_cast<std::size_t>(uint1_y) * limb.arr_size[0];
    auto& vec_column = limb.vec_voxel_map[st_slot];
    limb.vec_column_present[st_slot] = 1;
    vec_column.reserve(vec_column.size() + 8);  // 上游 EnsureCapacity 仅容量提示,无语义
    do {
      uint1_z += reader.ReadUInt8();
      const auto uint1_count = reader.ReadUInt8();
      for (auto uint1_j = 0; uint1_j < uint1_count; uint1_j++) {
        const auto element = VxlElement{reader.ReadUInt8(), reader.ReadUInt8()};
        vec_column.emplace_back(uint1_z, element);
        uint1_z++;
      }

      // Skip duplicate count
      reader.ReadUInt8();
    } while (uint1_z < limb.arr_size[2]);
  }
}

}  // namespace

VxlReader::VxlReader(std::span<const std::byte> vec_file) {
  auto reader = SpanReader{vec_file};

  if (!ReadAscii(reader.ReadBytes(16)).starts_with("Voxel Animation"))
    throw std::runtime_error("System.IO.InvalidDataException: Invalid vxl header");

  reader.ReadUInt32();
  uint4_limb_count = reader.ReadUInt32();
  reader.ReadUInt32();
  const auto uint4_body_size = reader.ReadUInt32();
  reader.Skip(770);

  // Read Limb headers
  vec_limbs.resize(uint4_limb_count);
  for (auto& limb : vec_limbs) {
    limb.str_name = ReadAscii(reader.ReadBytes(16));

    reader.Skip(12);
  }

  // Skip to the Limb footers(802 = 16 头 + 16 尺寸域 + 770 保留;uint 域
  // 照抄)| Skip to the Limb footers (802 = the 16-byte header + the 16-byte
  // size fields + 770 reserved; kept over the uint domain)
  reader.Seek(802u + 28u * uint4_limb_count + uint4_body_size);

  auto vec_limb_data_offset = std::vector<std::uint32_t>(uint4_limb_count);
  for (std::size_t st_i = 0; st_i < uint4_limb_count; st_i++) {
    vec_limb_data_offset[st_i] = reader.ReadUInt32();
    reader.Skip(8);
    vec_limbs[st_i].fp4_scale = reader.ReadSingle();
    reader.Skip(48);

    for (auto& fp4_bound : vec_limbs[st_i].arr_bounds)
      fp4_bound = reader.ReadSingle();
    const auto vec_size = reader.ReadBytes(3);
    for (auto int4_j = 0; int4_j < 3; int4_j++)
      vec_limbs[st_i].arr_size[static_cast<std::size_t>(int4_j)] =
          static_cast<std::uint8_t>(vec_size[static_cast<std::size_t>(int4_j)]);
    vec_limbs[st_i].enum_type = static_cast<NormalType>(reader.ReadUInt8());
  }

  for (std::size_t st_i = 0; st_i < uint4_limb_count; st_i++) {
    reader.Seek(802u + 28u * uint4_limb_count + vec_limb_data_offset[st_i]);
    ReadVoxelData(reader, vec_limbs[st_i]);
  }
}

}  // namespace ora::fmt
