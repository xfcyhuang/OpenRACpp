// UPSTREAM: OpenRA.Platforms.Default/FrameBuffer.cs @b6fc03f L19-157(实现)
// 头文件携带完整 UPSTREAM 锚点与偏离登记。
// Implementation of FrameBuffer.cs; the header carries the full UPSTREAM
// anchors and deviation registrations.
#include "gfx/frame_buffer.hpp"

#include <cstdio>  // stderr 为宏,不穿越 import 边界(规范允许的并用正形态) | stderr is a macro; the spec-sanctioned mixed form

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

  // Depth(FrameBuffer.cs L46-58;桌面 GL_DEPTH_COMPONENT,ES 的 16 位档随 ES 批次)。
  // Depth (FrameBuffer.cs L46-58; desktop GL_DEPTH_COMPONENT, with the ES
  // 16-bit flavor deferred to the ES batch).
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

  // Test for completeness(FrameBuffer.cs L60-67;失败消息保留上游文本)。
  // Test for completeness (FrameBuffer.cs L60-67; failures keep the
  // upstream text).
  b_valid_ = render_.QueryFboStatus() == gl::GL_FRAMEBUFFER_COMPLETE;
  if (!b_valid_)
    std::println(stderr, "Error creating framebuffer: incomplete ({}x{})", int2_size.X, int2_size.Y);

  // Restore default buffer(FrameBuffer.cs L69-71)。
  // Restore the default buffer (FrameBuffer.cs L69-71).
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
  // FrameBuffer.cs L116-125(负宽高 clamp 到 0)。
  // FrameBuffer.cs L116-125 (negative widths/heights clamp to 0).
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

}  // namespace ora::gfx
