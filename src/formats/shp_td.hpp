#pragma once
import std;

#include "core/int2.hpp"
#include "core/vector_n.hpp"
#include "gfx/sprite_frame.hpp"
#include "formats/span_reader.hpp"

namespace ora::fmt {

bool IsShpTD(std::span<const std::byte> vec_file);

bool TryParseShpTD(std::span<const std::byte> vec_file, std::vector<std::unique_ptr<gfx::ISpriteFrame>>& vec_frames);

class ShpTDSprite {
 public:

  explicit ShpTDSprite(std::span<const std::byte> vec_file);

  const std::vector<std::unique_ptr<gfx::ISpriteFrame>>& Frames() const { return vec_frames_; }
  int2 Size() const { return int2_size_; }

 private:
  struct ImageHeader;
  struct TrimmedFrame;

  friend bool TryParseShpTD(std::span<const std::byte> vec_file,
                            std::vector<std::unique_ptr<gfx::ISpriteFrame>>& vec_frames);

  void Decompress(ImageHeader& header_h);

  std::vector<std::byte> CopyImageData(const std::vector<std::byte>& vec_base_image) const;

  int int4_recurse_depth_ = 0;
  std::int32_t int4_image_count_ = 0;
  std::int64_t int8_shp_bytes_file_offset_ = 0;
  std::span<const std::byte> vec_shp_bytes_;
  int2 int2_size_{};
  std::vector<std::unique_ptr<gfx::ISpriteFrame>> vec_frames_;
};

}
