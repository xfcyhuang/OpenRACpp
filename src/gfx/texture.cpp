// UPSTREAM: OpenRA.Platforms.Default/Texture.cs @7d57605 L43-236(实现体)
// 实现体翻译:PrepareTexture 的 6 参数序列、BGRA 上传、UNPACK 行打包、
// RGBA16F 浮点路径、glGetTexImage 读回。所有 GL 访问经命令队列(渲染线程
// 独占上下文);纹理操作绑 unit 0(与上游 glBindTexture 的当前单元语义一致
// —— Shader::PrepareRender 才切换活动单元)。
// The implementation body translated: PrepareTexture's six-parameter
// sequence, BGRA uploads, UNPACK row packing, the RGBA16F float path, and the
// glGetTexImage readback. All GL access goes through the command queue (the
// render thread solely owns the context); texture *operations* bind unit 0
// (matching the current-unit semantics of upstream's glBindTexture — only
// Shader::PrepareRender switches the active unit).
import std;
#include <cassert>  // assert 为宏,规范允许的 import/#include 并用正形态 | assert is a macro; the spec-sanctioned mixed form

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
    ptr_cmd->uint4_a = 0;                      // unit 0(操作通道)| unit 0 (the operation channel)
    ptr_cmd->uint4_b = gl::GL_TEXTURE_2D;
    ptr_cmd->uint4_c = uint4_id;
    queue.CommitBare();
  }
}

}  // namespace

Texture::Texture(RenderThread& render) : render_(render) {
  render_.GenNames(GfxCmdKind::GenTextures, 1, &uint4_texture_);
}

Texture::~Texture() {
  if (uint4_texture_ != 0) {
    // 异步删除:命令序保证晚于队列中既有引用(与上游显式 Dispose 等价的
    // RAII 化;见文件头 OPT-A6 注)。
    // Asynchronous delete: command ordering keeps this after the queue's
    // existing references (the RAII-ized equivalent of upstream's explicit
    // Dispose; see the OPT-A6 note in the header).
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
    ptr_cmd->uint4_d = gl::GL_RGBA8;  // 桌面 internal(ES→BGRA 随 ES 批次)| desktop internal (ES→BGRA with the ES batch)
    ptr_cmd->float_a = std::bit_cast<float>(static_cast<std::uint32_t>(gl::GL_BGRA));
    render_.queue().Commit(vec_bgra_bytes.data());
  }
}

void Texture::SetEmpty(std::int32_t int4_width, std::int32_t int4_height) {
  int4_width_ = int4_width;
  int4_height_ = int4_height;

  PrepareTexture();
  // 空 payload = data null(分配不初始化);OPT-B1:无 pow2 校验。
  // An empty payload means a null data pointer (allocate uninitialized);
  // OPT-B1: no pow2 enforcement.
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
  // 上游契约:源 = 行距整行宽(UNPACK_ROW_LENGTH = Size.Width)的位图,
  // SKIP 定位子窗 —— 覆盖性下界 = 最后一取出行末像素。
  // The upstream contract: the source is a bitmap pitched at the full width
  // (UNPACK_ROW_LENGTH = Size.Width) with SKIP locating the sub-window — the
  // coverage floor is the last fetched row's final pixel.
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
  // glGetTexImage 读取的是**当前活动单元**上绑定到 GL_TEXTURE_2D 的纹理;
  // 必须先绑定自己(第三批 gfx_test 实证:palette 双纹理场景下不绑定会
  // 读到最后一次 PrepareTexture 留下的其它纹理 —— ColorShifts 的 RGBA16F
  // 被 BGRA/UB 读成全零;消费端绑定 diff 与 GL 状态同步更新,无副作用)。
  // glGetTexImage reads whichever texture is bound to GL_TEXTURE_2D on the
  // *active* unit; bind ourselves first (proven by the third-batch
  // gfx_test: with two textures in play, the unbound read returned
  // whatever the last PrepareTexture left bound — ColorShifts' RGBA16F
  // read as all-zero under BGRA/UB; the consumer-side binding diff and
  // the GL state stay in sync, so this has no side effects).
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

}  // namespace ora::gfx
