// UPSTREAM: OpenRA.Mods.Cnc/FileFormats/HvaReader.cs @b6fc03f + Util.cs
// L131-255(MatrixInverse 子集)—— 逐句照抄(Stream → SpanReader)。
// [UPSTREAM continued] HvaReader.cs + the Util.cs L131-255 MatrixInverse
// subset, copied statement by statement (Stream → SpanReader).
#include "formats/hva_reader.hpp"

namespace ora::fmt {

std::optional<std::array<float, 16>> MatrixInverse(const std::array<float, 16>& arr_m) {
  const auto& m = arr_m;
  auto arr_mtx = std::array<float, 16>{};

  arr_mtx[0] = m[5] * m[10] * m[15] -
      m[5] * m[11] * m[14] -
      m[9] * m[6] * m[15] +
      m[9] * m[7] * m[14] +
      m[13] * m[6] * m[11] -
      m[13] * m[7] * m[10];

  arr_mtx[4] = -m[4] * m[10] * m[15] +
      m[4] * m[11] * m[14] +
      m[8] * m[6] * m[15] -
      m[8] * m[7] * m[14] -
      m[12] * m[6] * m[11] +
      m[12] * m[7] * m[10];

  arr_mtx[8] = m[4] * m[9] * m[15] -
      m[4] * m[11] * m[13] -
      m[8] * m[5] * m[15] +
      m[8] * m[7] * m[13] +
      m[12] * m[5] * m[11] -
      m[12] * m[7] * m[9];

  arr_mtx[12] = -m[4] * m[9] * m[14] +
      m[4] * m[10] * m[13] +
      m[8] * m[5] * m[14] -
      m[8] * m[6] * m[13] -
      m[12] * m[5] * m[10] +
      m[12] * m[6] * m[9];

  arr_mtx[1] = -m[1] * m[10] * m[15] +
      m[1] * m[11] * m[14] +
      m[9] * m[2] * m[15] -
      m[9] * m[3] * m[14] -
      m[13] * m[2] * m[11] +
      m[13] * m[3] * m[10];

  arr_mtx[5] = m[0] * m[10] * m[15] -
      m[0] * m[11] * m[14] -
      m[8] * m[2] * m[15] +
      m[8] * m[3] * m[14] +
      m[12] * m[2] * m[11] -
      m[12] * m[3] * m[10];

  arr_mtx[9] = -m[0] * m[9] * m[15] +
      m[0] * m[11] * m[13] +
      m[8] * m[1] * m[15] -
      m[8] * m[3] * m[13] -
      m[12] * m[1] * m[11] +
      m[12] * m[3] * m[9];

  arr_mtx[13] = m[0] * m[9] * m[14] -
      m[0] * m[10] * m[13] -
      m[8] * m[1] * m[14] +
      m[8] * m[2] * m[13] +
      m[12] * m[1] * m[10] -
      m[12] * m[2] * m[9];

  arr_mtx[2] = m[1] * m[6] * m[15] -
      m[1] * m[7] * m[14] -
      m[5] * m[2] * m[15] +
      m[5] * m[3] * m[14] +
      m[13] * m[2] * m[7] -
      m[13] * m[3] * m[6];

  arr_mtx[6] = -m[0] * m[6] * m[15] +
      m[0] * m[7] * m[14] +
      m[4] * m[2] * m[15] -
      m[4] * m[3] * m[14] -
      m[12] * m[2] * m[7] +
      m[12] * m[3] * m[6];

  arr_mtx[10] = m[0] * m[5] * m[15] -
      m[0] * m[7] * m[13] -
      m[4] * m[1] * m[15] +
      m[4] * m[3] * m[13] +
      m[12] * m[1] * m[7] -
      m[12] * m[3] * m[5];

  arr_mtx[14] = -m[0] * m[5] * m[14] +
      m[0] * m[6] * m[13] +
      m[4] * m[1] * m[14] -
      m[4] * m[2] * m[13] -
      m[12] * m[1] * m[6] +
      m[12] * m[2] * m[5];

  arr_mtx[3] = -m[1] * m[6] * m[11] +
      m[1] * m[7] * m[10] +
      m[5] * m[2] * m[11] -
      m[5] * m[3] * m[10] -
      m[9] * m[2] * m[7] +
      m[9] * m[3] * m[6];

  arr_mtx[7] = m[0] * m[6] * m[11] -
      m[0] * m[7] * m[10] -
      m[4] * m[2] * m[11] +
      m[4] * m[3] * m[10] +
      m[8] * m[2] * m[7] -
      m[8] * m[3] * m[6];

  arr_mtx[11] = -m[0] * m[5] * m[11] +
      m[0] * m[7] * m[9] +
      m[4] * m[1] * m[11] -
      m[4] * m[3] * m[9] -
      m[8] * m[1] * m[7] +
      m[8] * m[3] * m[5];

  arr_mtx[15] = m[0] * m[5] * m[10] -
      m[0] * m[6] * m[9] -
      m[4] * m[1] * m[10] +
      m[4] * m[2] * m[9] +
      m[8] * m[1] * m[6] -
      m[8] * m[2] * m[5];

  const auto fp4_det = m[0] * arr_mtx[0] + m[1] * arr_mtx[4] + m[2] * arr_mtx[8] + m[3] * arr_mtx[12];
  if (fp4_det == 0)
    return std::nullopt;

  for (auto& fp4_v : arr_mtx)
    fp4_v *= 1 / fp4_det;

  return arr_mtx;
}

HvaReader::HvaReader(std::span<const std::byte> vec_file, std::string_view str_file_name) {
  // Index swaps for transposing a matrix
  constexpr std::uint8_t arr_ids[12] = {0, 4, 8, 12, 1, 5, 9, 13, 2, 6, 10, 14};

  auto reader = SpanReader{vec_file};
  reader.Seek(16);
  uint4_frame_count = reader.ReadUInt32();
  uint4_limb_count = reader.ReadUInt32();

  // Skip limb names
  reader.Skip(16 * static_cast<std::int64_t>(uint4_limb_count));
  vec_transforms.assign(16ull * uint4_frame_count * uint4_limb_count, 0.0f);

  auto arr_test_matrix = std::array<float, 16>{};
  for (auto uint4_j = 0ull; uint4_j < uint4_frame_count; uint4_j++)
    for (auto uint4_i = 0ull; uint4_i < uint4_limb_count; uint4_i++) {
      // Convert to column-major matrices and add the final matrix row
      const auto st_c = 16ull * (uint4_limb_count * uint4_j + uint4_i);
      vec_transforms[st_c + 3] = 0;
      vec_transforms[st_c + 7] = 0;
      vec_transforms[st_c + 11] = 0;
      vec_transforms[st_c + 15] = 1;

      for (auto int4_k = 0; int4_k < 12; int4_k++)
        vec_transforms[st_c + arr_ids[int4_k]] = reader.ReadSingle();

      std::ranges::copy_n(vec_transforms.begin() + static_cast<std::ptrdiff_t>(
                                                     16ull * (uint4_limb_count * uint4_j + uint4_i)),
                          16, arr_test_matrix.begin());
      if (!MatrixInverse(arr_test_matrix).has_value())
        throw std::runtime_error(std::format(
            "System.IO.InvalidDataException: The transformation matrix for HVA file `{}` section {} frame {} is "
            "invalid because it is not invertible!",
            str_file_name, uint4_i, uint4_j));
    }
}

}  // namespace ora::fmt
