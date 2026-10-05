// UPSTREAM: OpenRA.Mods.D2k/SpriteLoaders/R8Loader.cs @b6fc03f L24-229
//          (r8_loader.hpp 的实现;头注的形态适配说明适用)
//          Implementation of r8_loader.hpp; the shape-adaptation notes of
//          the hpp header apply.
import std;
#include "formats/r8_loader.hpp"
#include "gfx/palette.hpp"

namespace ora::fmt {

namespace {

/// RGB555 u16 → ARGB u32(R8Loader.cs L150/L172 的同一展开式)。
/// An RGB555 u16 → ARGB u32 (the identical expansion of R8Loader.cs
/// L150/L172).
constexpr std::uint32_t ExpandRgb555(std::uint32_t uint2_packed) {
  return (0xFFu << 24) | ((uint2_packed & 0x7C00u) << 9) | ((uint2_packed & 0x3E0u) << 6) |
         ((uint2_packed & 0x1Fu) << 3);
}

/// uint32 的小端四字节写入。
/// Little-endian four-byte store of a uint32.
void StoreU32(std::span<std::byte> vec_dest, std::size_t st_index, std::uint32_t uint4_v) {
  vec_dest[st_index] = static_cast<std::byte>(uint4_v & 0xFF);
  vec_dest[st_index + 1] = static_cast<std::byte>((uint4_v >> 8) & 0xFF);
  vec_dest[st_index + 2] = static_cast<std::byte>((uint4_v >> 16) & 0xFF);
  vec_dest[st_index + 3] = static_cast<std::byte>((uint4_v >> 24) & 0xFF);
}

}  // namespace

// ———— R8Frame ————

R8Frame::R8Frame(SpanReader& reader_source, const std::vector<std::uint32_t>* ptr_last_palette) {
  // 前导 0 跳过(L114-117)
  // Skip the leading zeros (L114-117).
  auto int4_type = reader_source.ReadUInt8();
  while (int4_type == 0)
    int4_type = reader_source.ReadUInt8();

  const auto int4_width = reader_source.ReadInt32();
  const auto int4_height = reader_source.ReadInt32();
  const auto int4_x = reader_source.ReadInt32();
  const auto int4_y = reader_source.ReadInt32();

  int2_size_ = int2{int4_width, int4_height};
  // Offset = (w/2 - x, h/2 - y):C# 整除后入 float | C# integer divisions
  // before the float lift.
  vec2_offset_ = core::Vector2{static_cast<float>(int4_width / 2 - int4_x),
                               static_cast<float>(int4_height / 2 - int4_y)};

  reader_source.ReadUInt32();  // imageHandle
  const auto int4_palette_handle = reader_source.ReadInt32();
  const auto uint1_bpp = reader_source.ReadUInt8();
  if (uint1_bpp != 8 && uint1_bpp != 16)
    throw std::runtime_error(std::format("Error: {} bits per pixel are not supported.",
                                         static_cast<int>(uint1_bpp)));

  const auto uint1_frame_height = reader_source.ReadUInt8();
  const auto uint1_frame_width = reader_source.ReadUInt8();
  int2_frame_size_ = int2{uint1_frame_width, uint1_frame_height};

  reader_source.ReadUInt8();  // 对齐字节 | the alignment byte

  if (uint1_bpp == 16) {
    // L140-152:RGB555 反序展开(自尾向头;源/目标分区不重叠,循环序即上
    // 游的就地循环序)
    // L140-152: the reverse RGB555 expansion (tail to head; the source and
    // destination partitions stay disjoint — the loop order equals
    // upstream's in-place loop).
    const auto st_pixels = static_cast<std::size_t>(int4_width) * static_cast<std::size_t>(int4_height);
    vec_data_.assign(st_pixels * 4, std::byte{0});
    type_frame_ = gfx::SpriteFrameType::Bgra32;

    const std::span<const std::byte> vec_raw =
        reader_source.ReadBytes(static_cast<std::int64_t>(st_pixels * 2));
    for (auto int4_i = static_cast<std::int64_t>(st_pixels) - 1; int4_i >= 0; int4_i--) {
      const auto uint1_lo = static_cast<std::uint8_t>(vec_raw[static_cast<std::size_t>(int4_i) * 2]);
      const auto uint1_hi =
          static_cast<std::uint8_t>(vec_raw[static_cast<std::size_t>(int4_i) * 2 + 1]);
      StoreU32(vec_data_, static_cast<std::size_t>(int4_i) * 4,
               ExpandRgb555(static_cast<std::uint32_t>(uint1_hi << 8 | uint1_lo)));
    }
  } else {
    const std::span<const std::byte> vec_indexed =
        reader_source.ReadBytes(static_cast<std::int64_t>(int4_width) * int4_height);
    vec_data_.assign(vec_indexed.begin(), vec_indexed.end());
    type_frame_ = gfx::SpriteFrameType::Indexed8;
  }

  // 调色板(L159-180)
  // The palette (L159-180).
  if (int4_type == 1 && int4_palette_handle != 0) {
    reader_source.ReadUInt32();  // 头 8 字节 | the 8 header bytes
    reader_source.ReadUInt32();

    std::array<std::uint32_t, 256> arr_palette{};
    const std::span<const std::byte> vec_pal_bytes = reader_source.ReadBytes(512);
    for (std::int32_t int4_i{255}; int4_i >= 0; int4_i--) {
      const auto uint1_lo = static_cast<std::uint8_t>(vec_pal_bytes[static_cast<std::size_t>(int4_i) * 2]);
      const auto uint1_hi =
          static_cast<std::uint8_t>(vec_pal_bytes[static_cast<std::size_t>(int4_i) * 2 + 1]);
      arr_palette[static_cast<std::size_t>(int4_i)] =
          ExpandRgb555(static_cast<std::uint32_t>(uint1_hi << 8 | uint1_lo));
    }

    // 索引 0 重映射为透明 | Remap index 0 to transparent.
    arr_palette[0] = 0;
    opt_palette_ = std::vector<std::uint32_t>{arr_palette.begin(), arr_palette.end()};
  } else if (int4_type == 2) {
    if (ptr_last_palette != nullptr)
      opt_palette_ = *ptr_last_palette;
  }
}

// ———— R8RemappableFrame ————

std::span<const std::byte> R8RemappableFrame::Data() const {
  if (!b_data_materialized_) {
    const int2 int2_size = frame_inner_.Size();
    const auto st_pixel_count = static_cast<std::size_t>(int2_size.X) * static_cast<std::size_t>(int2_size.Y);
    vec_data_cached_.assign(st_pixel_count * 4, std::byte{0});

    std::array<std::uint32_t, 256> arr_palette{};
    const std::optional<std::vector<std::uint32_t>>& opt_inner = frame_inner_.Palette();
    if (opt_inner.has_value())
      std::ranges::copy(*opt_inner, arr_palette.begin());

    // 位运算 = RGB 通道减半后取反(shroud → fog)
    // The bit twiddle = the RGB channels halved then inverted (shroud → fog).
    if (b_convert_shroud_to_fog_)
      for (std::size_t st_i{}; st_i < arr_palette.size(); st_i++)
        arr_palette[st_i] = ~((arr_palette[st_i] >> 1) & 0x007F7F7Fu);

    // 索引 1 → 影色 | Index 1 → the shadow color.
    if (b_use_shadow_)
      arr_palette[1] = 140u << 24;

    if (color_remap_.ToArgb() != 0) {
      static constexpr std::array<std::int32_t, 16> kRemapIndices{
          240, 241, 242, 243, 244, 245, 246, 247, 248, 249, 250, 251, 252, 253, 254, 255};
      const gfx::PlayerColorRemap remap_r{kRemapIndices, color_remap_};
      for (std::int32_t int4_i{240}; int4_i < 256; int4_i++)
        arr_palette[static_cast<std::size_t>(int4_i)] =
            remap_r.GetRemappedColor(core::Color::FromArgb(arr_palette[static_cast<std::size_t>(int4_i)]),
                                     int4_i)
                .ToArgb();
    }

    const std::span<const std::byte> vec_inner = frame_inner_.Data();
    for (std::size_t st_i{}; st_i < st_pixel_count; st_i++)
      StoreU32(vec_data_cached_, st_i * 4,
               arr_palette[static_cast<std::uint8_t>(vec_inner[st_i])]);

    b_data_materialized_ = true;
  }
  return vec_data_cached_;
}

// ———— 探测与解析 ————

bool IsR8(std::span<const std::byte> vec_file) {
  if (vec_file.size() < 26)
    return false;  // 上游越界读异常逃出 catch 面 | upstream's OOB read escapes the catch surface

  // 首字节非零 | The first byte is nonzero.
  if (static_cast<std::uint8_t>(vec_file[0]) == 0)
    return false;

  // 首帧 bpp 字段(@25)∈ {8,16}
  // The first frame's bpp field (@25) in {8,16}.
  const auto uint1_d = static_cast<std::uint8_t>(vec_file[25]);
  return uint1_d == 8 || uint1_d == 16;
}

bool TryParseR8(std::span<const std::byte> vec_file,
                std::vector<std::unique_ptr<gfx::ISpriteFrame>>& vec_frames) {
  if (!IsR8(vec_file)) {
    vec_frames.clear();
    return false;
  }

  SpanReader reader_source{vec_file};
  std::optional<std::vector<std::uint32_t>> opt_last_palette;

  while (reader_source.Position() < reader_source.Length()) {
    auto frame_next = std::make_unique<R8Frame>(reader_source,
                                                opt_last_palette.has_value()
                                                    ? &*opt_last_palette
                                                    : nullptr);
    if (frame_next->Palette().has_value())
      opt_last_palette = frame_next->Palette();

    if (frame_next->Palette().has_value()) {
      // 带调色板帧以 RemappableFrame(默认参)包装 —— 上游 Select 的等价
      // A palette-carrying frame wraps as RemappableFrame (default
      // arguments) — the Select equivalent.
      vec_frames.push_back(std::make_unique<R8RemappableFrame>(std::move(*frame_next)));
    } else {
      vec_frames.push_back(std::move(frame_next));
    }
  }

  return true;
}

}  // namespace ora::fmt
