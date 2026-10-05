// UPSTREAM: OpenRA.Platforms.Default/FrameBuffer.cs @b6fc03f L19-157(逐方法)
// 语义面:颜色纹理 + 深度 renderbuffer 附件、Bind 的 viewport 保存/恢复与
// clear(clearColor + COLOR|DEPTH)、Unbind 恢复 viewport、scissor 断言
// ("Attempting to unbind FrameBuffer with an active scissor region.")、
// 完整性校验(FRAMEBUFFER_COMPLETE,失败即错)。
// 偏离(已登记 COVERAGE):
//   - OPT-B1(D37):上游 L32-33 的 IsPowerOf2 强制校验不复刻(NPOT 直建);
//   - Bind/Unbind 的 glFlush 不复刻 —— 上游为多线程封送正确性而插,本引擎
//     命令流天然全序(语义等价);
//   - 完整性失败从 throw 改为 stderr + b_invalid 标记(异常面随渲染对拍
//     关卡统一;消息文本保留)。
// Method-by-method port of FrameBuffer.cs. Semantic plane: the color-texture
// plus depth-renderbuffer attachments, Bind's viewport save/restore and clear
// (clearColor + COLOR|DEPTH), Unbind's viewport restore, the scissor
// assertion ("Attempting to unbind FrameBuffer with an active scissor
// region."), and the completeness check (FRAMEBUFFER_COMPLETE or bust).
// Deviations (registered in COVERAGE):
//   - OPT-B1 (D37): upstream's L32-33 IsPowerOf2 enforcement is not
//     replicated (NPOT built directly);
//   - the glFlush of Bind/Unbind is not replicated — upstream inserted it for
//     multithreaded-marshal correctness, while this engine's command stream
//     is totally ordered by construction (semantically equivalent);
//   - a completeness failure prints to stderr and flags b_invalid instead of
//     throwing (the exception surface settles with the render-differential
//     gate; the message text is kept).
#pragma once
import std;
#include <cassert>

#include "core/int2.hpp"
#include "core/rectangle.hpp"
#include "gfx/render_thread.hpp"
#include "gfx/texture.hpp"
#include "platform/gl_types.hpp"

namespace ora::gfx {

/// 离屏帧缓冲(FrameBuffer.cs;不可拷贝移动 —— 持纹理 RAII)。
/// An offscreen framebuffer (FrameBuffer.cs; non-copyable and non-movable —
/// holds the RAII texture).
class FrameBuffer {
 public:
  /// 构造即建颜色纹理 + 深度 renderbuffer 附件并校验完整性。
  /// Construction builds the color texture + depth renderbuffer attachments
  /// and validates completeness.
  FrameBuffer(RenderThread& render, int2 int2_size, float float_r, float float_g, float float_float_b,
              float float_a);

  ~FrameBuffer();

  FrameBuffer(const FrameBuffer&) = delete;
  FrameBuffer& operator=(const FrameBuffer&) = delete;

  /// 绑定 + viewport + 清屏(FrameBuffer.cs L83-100;保存当前 viewport 供
  /// Unbind 恢复)。
  /// Binds + viewport + clear (FrameBuffer.cs L83-100; the current viewport
  /// is saved for Unbind's restore).
  void Bind();

  /// 解绑回默认帧缓冲并恢复 viewport(FrameBuffer.cs L102-114)。
  /// Unbinds back to the default framebuffer and restores the viewport
  /// (FrameBuffer.cs L102-114).
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

}  // namespace ora::gfx
