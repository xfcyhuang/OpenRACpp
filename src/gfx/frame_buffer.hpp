#pragma once
import std;
#include <cassert>

#include "core/int2.hpp"
#include "core/rectangle.hpp"
#include "gfx/render_thread.hpp"
#include "gfx/texture.hpp"
#include "platform/gl_types.hpp"

namespace ora::gfx {

class FrameBuffer {
 public:

  FrameBuffer(RenderThread& render, int2 int2_size, float float_r, float float_g, float float_float_b,
              float float_a);

  ~FrameBuffer();

  FrameBuffer(const FrameBuffer&) = delete;
  FrameBuffer& operator=(const FrameBuffer&) = delete;

  void Bind();

  void Unbind();

  void EnableScissor(Rectangle rect_region);
  void DisableScissor();

  Texture& GetTexture() { return texture_; }
  int2 Size() const { return int2_size_; }
  bool b_valid() const { return b_valid_; }

 private:
  RenderThread& render_;
  Texture texture_;
  std::uint32_t uint4_framebuffer_ = 0;
  std::uint32_t uint4_depth_ = 0;
  int2 int2_size_{};
  float float_clear_[4] = {};
  std::int32_t arr_viewport_prev_[4] = {};
  bool b_scissored_ = false;
  bool b_valid_ = false;
};

}
