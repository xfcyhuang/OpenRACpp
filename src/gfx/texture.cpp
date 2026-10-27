import std;
#include <cassert>

#include "gfx/texture.hpp"
#include "platform/gl_types.hpp"

namespace ora::gfx {

namespace {

void PostTexParameteri(GfxCommandQueue& queue, gl::GLenum pname, gl::GLint int4_param) {
  if (GfxCmd* ptr_cmd = queue.Reserve(GfxCmdKind::TexParameteri, 0); ptr_cmd != nullptr) {
    ptr_cmd->uint4_a = pname;
    ptr_cmd->uint4_b = static_cast<std::uint32_t>(int4_param);
    queue.CommitBare();
  }
}

void PostBindTextureUnit0(GfxCommandQueue& queue, std::uint32_t uint4_id) {
  if (GfxCmd* ptr_cmd = queue.Reserve(GfxCmdKind::BindTexture, 0); ptr_cmd != nullptr) {
    ptr_cmd->uint4_a = 0;
    ptr_cmd->uint4_b = gl::GL_TEXTURE_2D;
    ptr_cmd->uint4_c = uint4_id;
    queue.CommitBare();
  }
}

}

Texture::Texture(RenderThread& render) : render_(render) {
  render_.GenNames(GfxCmdKind::GenTextures, 1, &uint4_texture_);
}

Texture::~Texture() {
  if (uint4_texture_ != 0) {

    if (GfxCmd* ptr_cmd = render_.queue().Reserve(GfxCmdKind::DeleteTextures, sizeof(std::uint32_t));
        ptr_cmd != nullptr)
      render_.queue().Commit(&uint4_texture_);
    uint4_texture_ = 0;
  }
}

Texture::Texture(Texture&& other) noexcept
    : render_(other.render_),
      uint4_texture_(std::exchange(other.uint4_texture_, 0)),
      int4_width_(std::exchange(other.int4_width_, 0)),
      int4_height_(std::exchange(other.int4_height_, 0)),
      kind_filter_(other.kind_filter_) {}

void Texture::PrepareTexture() {
  GfxCommandQueue& queue = render_.queue();
  PostBindTextureUnit0(queue, uint4_texture_);

  const gl::GLenum filter = kind_filter_ == TextureScaleFilter::Linear ? gl::GL_LINEAR : gl::GL_NEAREST;
  PostTexParameteri(queue, gl::GL_TEXTURE_MAG_FILTER, filter);
  PostTexParameteri(queue, gl::GL_TEXTURE_MIN_FILTER, filter);
  PostTexParameteri(queue, gl::GL_TEXTURE_WRAP_S, gl::GL_CLAMP_TO_EDGE);
  PostTexParameteri(queue, gl::GL_TEXTURE_WRAP_T, gl::GL_CLAMP_TO_EDGE);
  PostTexParameteri(queue, gl::GL_TEXTURE_BASE_LEVEL, 0);
  PostTexParameteri(queue, gl::GL_TEXTURE_MAX_LEVEL, 0);
}

void Texture::SetData(std::span<const std::byte> vec_bgra_bytes, std::int32_t int4_width,
                      std::int32_t int4_height) {
  assert(vec_bgra_bytes.size() == std::size_t(4) * int4_width * int4_height);
  int4_width_ = int4_width;
  int4_height_ = int4_height;

  PrepareTexture();
  if (GfxCmd* ptr_cmd = render_.queue().Reserve(
          GfxCmdKind::TexImage2D, static_cast<std::uint32_t>(vec_bgra_bytes.size())); ptr_cmd != nullptr) {
    ptr_cmd->uint4_a = gl::GL_TEXTURE_2D;
    ptr_cmd->uint4_b = static_cast<std::uint32_t>(int4_width);
    ptr_cmd->uint4_c = static_cast<std::uint32_t>(int4_height);
    ptr_cmd->uint4_d = gl::GL_RGBA8;
    ptr_cmd->float_a = std::bit_cast<float>(static_cast<std::uint32_t>(gl::GL_BGRA));
    render_.queue().Commit(vec_bgra_bytes.data());
  }
}

void Texture::SetEmpty(std::int32_t int4_width, std::int32_t int4_height) {
  int4_width_ = int4_width;
  int4_height_ = int4_height;

  PrepareTexture();

  if (GfxCmd* ptr_cmd = render_.queue().Reserve(GfxCmdKind::TexImage2D, 0); ptr_cmd != nullptr) {
    ptr_cmd->uint4_a = gl::GL_TEXTURE_2D;
    ptr_cmd->uint4_b = static_cast<std::uint32_t>(int4_width);
    ptr_cmd->uint4_c = static_cast<std::uint32_t>(int4_height);
    ptr_cmd->uint4_d = gl::GL_RGBA8;
    ptr_cmd->float_a = std::bit_cast<float>(static_cast<std::uint32_t>(gl::GL_BGRA));
    render_.queue().CommitBare();
  }
}

void Texture::SetSubData(std::span<const std::byte> vec_bgra_bytes, std::int32_t int4_x, std::int32_t int4_y,
                         std::int32_t int4_width, std::int32_t int4_height) {

  assert(vec_bgra_bytes.size() >=
         std::size_t(4) * (std::size_t(int4_y) * std::size_t(int4_width_ < 0 ? 0 : int4_width_) +
                           std::size_t(int4_x < 0 ? 0 : int4_x) + std::size_t(int4_width)));
  assert(int4_x >= 0 && int4_y >= 0 && int4_x + int4_width <= int4_width_ &&
         int4_y + int4_height <= int4_height_);

  PrepareTexture();
  GfxCommandQueue& queue = render_.queue();
  auto PostPixelStore = [&](gl::GLenum pname, std::int32_t int4_param) {
    if (GfxCmd* ptr_cmd = queue.Reserve(GfxCmdKind::PixelStorei, 0); ptr_cmd != nullptr) {
      ptr_cmd->uint4_a = pname;
      ptr_cmd->uint4_b = static_cast<std::uint32_t>(int4_param);
      queue.CommitBare();
    }
  };
  PostPixelStore(gl::GL_UNPACK_ROW_LENGTH, int4_width_);
  PostPixelStore(gl::GL_UNPACK_SKIP_PIXELS, int4_x);
  PostPixelStore(gl::GL_UNPACK_SKIP_ROWS, int4_y);

  if (GfxCmd* ptr_cmd = queue.Reserve(GfxCmdKind::TexSubImage2D,
                                      static_cast<std::uint32_t>(vec_bgra_bytes.size())); ptr_cmd != nullptr) {
    ptr_cmd->uint4_a = gl::GL_TEXTURE_2D;
    ptr_cmd->uint4_b = static_cast<std::uint32_t>(int4_x);
    ptr_cmd->uint4_c = static_cast<std::uint32_t>(int4_y);
    ptr_cmd->uint4_d = static_cast<std::uint32_t>(int4_width);
    ptr_cmd->float_a = std::bit_cast<float>(static_cast<std::uint32_t>(int4_height));
    ptr_cmd->float_b = std::bit_cast<float>(static_cast<std::uint32_t>(gl::GL_BGRA));
    queue.Commit(vec_bgra_bytes.data());
  }

  PostPixelStore(gl::GL_UNPACK_ROW_LENGTH, 0);
  PostPixelStore(gl::GL_UNPACK_SKIP_PIXELS, 0);
  PostPixelStore(gl::GL_UNPACK_SKIP_ROWS, 0);
}

void Texture::SetFloatData(std::span<const float> vec_data, std::int32_t int4_width,
                           std::int32_t int4_height) {
  assert(vec_data.size() == std::size_t(4) * int4_width * int4_height);
  int4_width_ = int4_width;
  int4_height_ = int4_height;

  PrepareTexture();
  const std::size_t size_bytes = vec_data.size_bytes();
  assert(size_bytes <= 0xFFFF);
  if (GfxCmd* ptr_cmd = render_.queue().Reserve(GfxCmdKind::TexImage2D,
                                                static_cast<std::uint32_t>(size_bytes)); ptr_cmd != nullptr) {
    ptr_cmd->uint4_a = gl::GL_TEXTURE_2D;
    ptr_cmd->uint4_b = static_cast<std::uint32_t>(int4_width);
    ptr_cmd->uint4_c = static_cast<std::uint32_t>(int4_height);
    ptr_cmd->uint4_d = gl::GL_RGBA16F;
    ptr_cmd->float_a = std::bit_cast<float>(static_cast<std::uint32_t>(gl::GL_RGBA));
    ptr_cmd->float_b = std::bit_cast<float>(static_cast<std::uint32_t>(gl::GL_FLOAT));
    render_.queue().Commit(vec_data.data());
  }
}

void Texture::SetDataFromReadBuffer(std::int32_t int4_x, std::int32_t int4_y, std::int32_t int4_width,
                                    std::int32_t int4_height) {
  int4_width_ = int4_width;
  int4_height_ = int4_height;

  PrepareTexture();
  if (GfxCmd* ptr_cmd = render_.queue().Reserve(GfxCmdKind::CopyTexImage2D, 0); ptr_cmd != nullptr) {
    ptr_cmd->uint4_a = static_cast<std::uint32_t>(int4_x);
    ptr_cmd->uint4_b = static_cast<std::uint32_t>(int4_y);
    ptr_cmd->uint4_c = static_cast<std::uint32_t>(int4_width);
    ptr_cmd->uint4_d = static_cast<std::uint32_t>(int4_height);
    ptr_cmd->float_a = std::bit_cast<float>(static_cast<std::uint32_t>(gl::GL_RGBA8));
    render_.queue().CommitBare();
  }
}

std::vector<std::byte> Texture::GetData() {

  PostBindTextureUnit0(render_.queue(), uint4_texture_);
  std::vector<std::byte> vec_data(std::size_t(4) * int4_width_ * int4_height_);
  render_.GetTexImage(vec_data.size(), vec_data.data());
  return vec_data;
}

void Texture::SetScaleFilter(TextureScaleFilter kind_filter) {
  if (kind_filter_ == kind_filter)
    return;
  kind_filter_ = kind_filter;
  PrepareTexture();
}

}
