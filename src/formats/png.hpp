#pragma once
import std;

#include "gfx/sprite.hpp"

namespace ora::fmt {

struct PngColor {
  std::uint8_t uint1_r{0};
  std::uint8_t uint1_g{0};
  std::uint8_t uint1_b{0};
  std::uint8_t uint1_a{255};

  bool operator==(const PngColor&) const = default;

  static constexpr PngColor FromRgb(std::uint8_t uint1_r, std::uint8_t uint1_g, std::uint8_t uint1_b) {
    return PngColor{uint1_r, uint1_g, uint1_b, 255};
  }

  [[nodiscard]] constexpr PngColor WithAlpha(std::uint8_t uint1_a) const {
    return PngColor{uint1_r, uint1_g, uint1_b, uint1_a};
  }
};

class Png {
 public:
  static constexpr std::uint32_t ChunkIHDR = 0x49484452;
  static constexpr std::uint32_t ChunkPLTE = 0x504C5445;
  static constexpr std::uint32_t ChunkIDAT = 0x49444154;
  static constexpr std::uint32_t ChunkIEND = 0x49454E44;
  static constexpr std::uint32_t ChunkTRNS = 0x74524E53;
  static constexpr std::uint32_t ChunkTEXT = 0x74455874;

  explicit Png(std::span<const std::byte> vec_file);

  Png(std::vector<std::byte> vec_data, gfx::SpriteFrameType type_frame, int int4_width, int int4_height,
      std::optional<std::vector<PngColor>> vec_palette = std::nullopt,
      std::vector<std::pair<std::string, std::string>> vec_embedded_data = {});

  static bool Verify(std::span<const std::byte> vec_file);

  std::vector<std::byte> Save() const;

  int Width() const { return int4_width_; }
  int Height() const { return int4_height_; }
  gfx::SpriteFrameType Type() const { return type_frame_; }

  int PixelStride() const {
    return type_frame_ == gfx::SpriteFrameType::Indexed8 ? 1 : type_frame_ == gfx::SpriteFrameType::Rgb24 ? 3 : 4;
  }

  const std::optional<std::vector<PngColor>>& Palette() const { return vec_palette_; }
  const std::vector<std::byte>& Data() const { return vec_data_; }

  const std::vector<std::pair<std::string, std::string>>& EmbeddedData() const { return vec_embedded_data_; }

 private:

  void ProcessIDAT(class SpanReader& reader_source, std::int32_t int4_first_chunk_length, std::uint8_t uint1_bit_depth);

  int int4_width_ = 0;
  int int4_height_ = 0;
  std::optional<std::vector<PngColor>> vec_palette_;
  std::vector<std::byte> vec_data_;
  gfx::SpriteFrameType type_frame_ = gfx::SpriteFrameType::Rgba32;
  std::vector<std::pair<std::string, std::string>> vec_embedded_data_;
};

}
