#pragma once
import std;

#include "gfx/render_thread.hpp"

namespace ora::gfx {

enum class TextureScaleFilter : std::uint8_t { Nearest, Linear };

class Texture {
 public:

  explicit Texture(RenderThread& render);

  ~Texture();

  Texture(const Texture&) = delete;
  Texture& operator=(const Texture&) = delete;

  Texture(Texture&& other) noexcept;
  Texture& operator=(Texture&&) = delete;

  void SetData(std::span<const std::byte> vec_bgra_bytes, std::int32_t int4_width, std::int32_t int4_height);

  void SetEmpty(std::int32_t int4_width, std::int32_t int4_height);

  void SetSubData(std::span<const std::byte> vec_bgra_bytes, std::int32_t int4_x, std::int32_t int4_y,
                  std::int32_t int4_width, std::int32_t int4_height);

  void SetFloatData(std::span<const float> vec_data, std::int32_t int4_width, std::int32_t int4_height);

  void SetDataFromReadBuffer(std::int32_t int4_x, std::int32_t int4_y, std::int32_t int4_width,
                             std::int32_t int4_height);

  [[nodiscard]] std::vector<std::byte> GetData();

  TextureScaleFilter ScaleFilter() const { return kind_filter_; }
  void SetScaleFilter(TextureScaleFilter kind_filter);

  std::int32_t Width() const { return int4_width_; }
  std::int32_t Height() const { return int4_height_; }

  std::uint32_t GlId() const { return uint4_texture_; }

 private:

  void PrepareTexture();

  RenderThread& render_;
  std::uint32_t uint4_texture_ = 0;
  std::int32_t int4_width_ = 0;
  std::int32_t int4_height_ = 0;
  TextureScaleFilter kind_filter_ = TextureScaleFilter::Nearest;
};

}
