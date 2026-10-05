// UPSTREAM: OpenRA.Mods.Common/FileFormats/WestwoodCompressedReader.cs
// @b6fc03f L20-84:逐句照抄;byte[]/Span → span;output 越界守卫 = 上游
// IndexOutOfRange 的等价抛点(D61 先例)。Clamp(byte.MinValue,
// byte.MaxValue) 对 int 差分恒在 0..255 内截,无需分支。
// [UPSTREAM continued] WestwoodCompressedReader.cs L20-84, copied statement by
// statement; byte[]/Span → span; the output-bounds guard throws at
// upstream's IndexOutOfRangeException points (the D61 precedent). Clamp
// over the byte domain always lands in 0..255 for these int deltas, no
// branch needed.
#include "formats/westwood_compressed.hpp"

namespace ora::fmt::westwood_compressed {
namespace {

constexpr int kAudWsStepTable2[] = {-2, -1, 0, 1};
constexpr int kAudWsStepTable4[] = {-9, -8, -6, -5, -4, -3, -2, -1, 0, 1, 2, 3, 4, 5, 6, 8};

std::uint8_t SaturatingAdd(int int4_sample, int int4_delta) {
  // (sample + delta).Clamp(byte.MinValue, byte.MaxValue) 的直译。
  // A literal rendering of (sample + delta).Clamp(byte.MinValue,
  // byte.MaxValue).
  const auto int4_sum = int4_sample + int4_delta;
  if (int4_sum < 0)
    return 0;
  if (int4_sum > 255)
    return 255;
  return static_cast<std::uint8_t>(int4_sum);
}

}  // namespace

void DecodeWestwoodCompressedSample(std::span<const std::byte> vec_input, std::span<std::byte> vec_output) {
  if (vec_input.size() == vec_output.size()) {
    std::ranges::copy(vec_input, vec_output.begin());
    return;
  }

  const auto fn_check_w = [vec_output](std::size_t st_w) {
    if (st_w >= vec_output.size()) [[unlikely]]
      throw std::runtime_error("Index was outside the bounds of the array.");
  };

  auto int4_sample = 0x80;
  auto st_r = std::size_t{0};
  auto st_w = std::size_t{0};

  while (st_r < vec_input.size()) {
    const auto uint1_cmd = static_cast<std::uint8_t>(vec_input[st_r++]);
    auto int4_count = uint1_cmd & 0x3f;

    switch (uint1_cmd >> 6) {
      case 0:
        for (++int4_count; int4_count > 0; int4_count--) {
          const auto uint1_code = static_cast<std::uint8_t>(vec_input.at(st_r++));
          fn_check_w(st_w);
          vec_output[st_w++] = static_cast<std::byte>(int4_sample = SaturatingAdd(int4_sample, kAudWsStepTable2[(uint1_code >> 0) & 0x03]));
          fn_check_w(st_w);
          vec_output[st_w++] = static_cast<std::byte>(int4_sample = SaturatingAdd(int4_sample, kAudWsStepTable2[(uint1_code >> 2) & 0x03]));
          fn_check_w(st_w);
          vec_output[st_w++] = static_cast<std::byte>(int4_sample = SaturatingAdd(int4_sample, kAudWsStepTable2[(uint1_code >> 4) & 0x03]));
          fn_check_w(st_w);
          vec_output[st_w++] = static_cast<std::byte>(int4_sample = SaturatingAdd(int4_sample, kAudWsStepTable2[(uint1_code >> 6) & 0x03]));
        }

        break;

      case 1:
        for (++int4_count; int4_count > 0; int4_count--) {
          const auto uint1_code = static_cast<std::uint8_t>(vec_input.at(st_r++));
          fn_check_w(st_w);
          vec_output[st_w++] = static_cast<std::byte>(int4_sample = SaturatingAdd(int4_sample, kAudWsStepTable4[(uint1_code >> 0) & 0x0f]));
          fn_check_w(st_w);
          vec_output[st_w++] = static_cast<std::byte>(int4_sample = SaturatingAdd(int4_sample, kAudWsStepTable4[(uint1_code >> 4) & 0xff]));
        }

        break;

      case 2:
        if ((int4_count & 0x20) != 0) {
          // sample += (sbyte)((sbyte)count << 3) >> 3 —— 两次 sbyte 截断
          // 与算术右移的符号扩展逐字保留。
          // sample += (sbyte)((sbyte)count << 3) >> 3 — both sbyte
          // truncations and the arithmetic-shift sign extension kept
          // verbatim.
          fn_check_w(st_w);
          const auto int2_jump = static_cast<std::int8_t>(static_cast<std::int8_t>(int4_count) << 3) >> 3;
          vec_output[st_w++] = static_cast<std::byte>(int4_sample += int2_jump);
          break;
        }

        for (++int4_count; int4_count > 0; int4_count--) {
          fn_check_w(st_w);
          vec_output[st_w++] = vec_input.at(st_r++);
        }

        int4_sample = static_cast<std::uint8_t>(vec_input[st_r - 1]);

        break;

      default:
        for (++int4_count; int4_count > 0; int4_count--) {
          fn_check_w(st_w);
          vec_output[st_w++] = static_cast<std::byte>(int4_sample);
        }

        break;
    }
  }
}

}  // namespace ora::fmt::westwood_compressed
