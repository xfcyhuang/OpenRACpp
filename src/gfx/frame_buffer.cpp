#include "gfx/frame_buffer.hpp"

#include <cstdio>

namespace ora::gfx {

FrameBuffer::FrameBuffer(RenderThread& render, int2 int2_size, float float_r, float float_g,
                         float float_float_b, float float_a)
    : render_{render},
      texture_{render},
      int2_size_{int2_size},
      float_clear_{float_r, float_g, float_float_b, float_a} {
  texture_.SetEmpty(int2_size.X, int2_size.Y);

  render_.GenNames(GfxCmdKind::GenFramebuffers, 1, &uint4_framebuffer_);
  if (GfxCmd* ptr_cmd = render_.queue().Reserve(GfxCmdKind::BindFramebuffer, 0); ptr_cmd != nullptr) {
    ptr_cmd->uint4_a = uint4_framebuffer_;
    render_.queue().CommitBare();
  }
  if (GfxCmd* ptr_cmd = render_.queue().Reserve(GfxCmdKind::FramebufferTexture2D, 0); ptr_cmd != nullptr) {
    ptr_cmd->uint4_a = uint4_framebuffer_;
    ptr_cmd->uint4_b = gl::GL_COLOR_ATTACHMENT0;
    ptr_cmd->uint4_c = texture_.GlId();
    render_.queue().CommitBare();
  }

  render_.GenNames(GfxCmdKind::GenRenderbuffers, 1, &uint4_depth_);
  if (GfxCmd* ptr_cmd = render_.queue().Reserve(GfxCmdKind::RenderbufferStorage, 0); ptr_cmd != nullptr) {
    ptr_cmd->uint4_a = gl::GL_DEPTH_COMPONENT;
    ptr_cmd->uint4_b = static_cast<std::uint32_t>(int2_size.X);
    ptr_cmd->uint4_c = static_cast<std::uint32_t>(int2_size.Y);
    ptr_cmd->uint4_d = uint4_depth_;
    render_.queue().CommitBare();
  }
  if (GfxCmd* ptr_cmd = render_.queue().Reserve(GfxCmdKind::FramebufferRenderbuffer, 0); ptr_cmd != nullptr) {
    ptr_cmd->uint4_a = uint4_framebuffer_;
    ptr_cmd->uint4_b = gl::GL_DEPTH_ATTACHMENT;
    ptr_cmd->uint4_c = uint4_depth_;
    render_.queue().CommitBare();
  }

  b_valid_ = render_.QueryFboStatus() == gl::GL_FRAMEBUFFER_COMPLETE;
  if (!b_valid_)
    std::println(stderr, "Error creating framebuffer: incomplete ({}x{})", int2_size.X, int2_size.Y);

  if (GfxCmd* ptr_cmd = render_.queue().Reserve(GfxCmdKind::BindFramebuffer, 0); ptr_cmd != nullptr) {
    ptr_cmd->uint4_a = 0;
    render_.queue().CommitBare();
  }
}

FrameBuffer::~FrameBuffer() {
  if (uint4_framebuffer_ != 0) {
    if (GfxCmd* ptr_cmd = render_.queue().Reserve(GfxCmdKind::DeleteFramebuffers, 4); ptr_cmd != nullptr)
      render_.queue().Commit(&uint4_framebuffer_);
  }
  if (uint4_depth_ != 0) {
    if (GfxCmd* ptr_cmd = render_.queue().Reserve(GfxCmdKind::DeleteRenderbuffers, 4); ptr_cmd != nullptr)
      render_.queue().Commit(&uint4_depth_);
  }
}

void FrameBuffer::Bind() {
  render_.ReadViewport(arr_viewport_prev_);

  if (GfxCmd* ptr_cmd = render_.queue().Reserve(GfxCmdKind::BindFramebuffer, 0); ptr_cmd != nullptr) {
    ptr_cmd->uint4_a = uint4_framebuffer_;
    render_.queue().CommitBare();
  }
  if (GfxCmd* ptr_cmd = render_.queue().Reserve(GfxCmdKind::SetViewport, 0); ptr_cmd != nullptr) {
    ptr_cmd->uint4_a = 0;
    ptr_cmd->uint4_b = 0;
    ptr_cmd->uint4_c = static_cast<std::uint32_t>(int2_size_.X);
    ptr_cmd->uint4_d = static_cast<std::uint32_t>(int2_size_.Y);
    render_.queue().CommitBare();
  }
  if (GfxCmd* ptr_cmd = render_.queue().Reserve(GfxCmdKind::Clear, 0); ptr_cmd != nullptr) {
    ptr_cmd->float_a = float_clear_[0];
    ptr_cmd->float_b = float_clear_[1];
    ptr_cmd->float_c = float_clear_[2];
    ptr_cmd->float_d = float_clear_[3];
    ptr_cmd->b_depth = 1;
    render_.queue().CommitBare();
  }
}

void FrameBuffer::Unbind() {
  assert(!b_scissored_ && "Attempting to unbind FrameBuffer with an active scissor region.");

  if (GfxCmd* ptr_cmd = render_.queue().Reserve(GfxCmdKind::BindFramebuffer, 0); ptr_cmd != nullptr) {
    ptr_cmd->uint4_a = 0;
    render_.queue().CommitBare();
  }
  if (GfxCmd* ptr_cmd = render_.queue().Reserve(GfxCmdKind::SetViewport, 0); ptr_cmd != nullptr) {
    ptr_cmd->uint4_a = static_cast<std::uint32_t>(arr_viewport_prev_[0]);
    ptr_cmd->uint4_b = static_cast<std::uint32_t>(arr_viewport_prev_[1]);
    ptr_cmd->uint4_c = static_cast<std::uint32_t>(arr_viewport_prev_[2]);
    ptr_cmd->uint4_d = static_cast<std::uint32_t>(arr_viewport_prev_[3]);
    render_.queue().CommitBare();
  }
}

void FrameBuffer::EnableScissor(Rectangle rect_region) {

  if (GfxCmd* ptr_cmd = render_.queue().Reserve(GfxCmdKind::SetScissor, 0); ptr_cmd != nullptr) {
    ptr_cmd->uint4_a = static_cast<std::uint32_t>(rect_region.X);
    ptr_cmd->uint4_b = static_cast<std::uint32_t>(rect_region.Y);
    ptr_cmd->uint4_c = static_cast<std::uint32_t>(std::max(0, rect_region.Width));
    ptr_cmd->uint4_d = static_cast<std::uint32_t>(std::max(0, rect_region.Height));
    render_.queue().CommitBare();
  }
  b_scissored_ = true;
}

void FrameBuffer::DisableScissor() {
  if (GfxCmd* ptr_cmd = render_.queue().Reserve(GfxCmdKind::DisableScissor, 0); ptr_cmd != nullptr)
    render_.queue().CommitBare();
  b_scissored_ = false;
}

}
