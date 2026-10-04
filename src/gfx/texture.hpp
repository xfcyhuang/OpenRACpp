// UPSTREAM: OpenRA.Platforms.Default/Texture.cs @7d57605 L18-237(逐方法)
// 语义面:BGRA 字节序上传(internal:桌面 RGBA8 / ES BGRA)、SetSubData 的
// UNPACK_ROW_LENGTH/SKIP 行打包、RGBA16F 浮点路径(调色板)、读回走桌面
// glGetTexImage(GLES FBO 回读分支随 ES 档位批次)、ScaleFilter 变更触发
// PrepareTexture(filter/CLAMP_TO_EDGE/BASE_LEVEL 0/MAX_LEVEL 0)。
// 偏离(已登记 COVERAGE):
//   - OPT-B1(D37):上游 SetData/SetFloatData/SetDataFromReadBuffer/SetEmpty 的
//     IsPowerOf2 强制校验不复刻(本引擎按 NPOT 直建);
//   - OPT-A6:纹理句柄 RAII(析构异步发 DeleteTextures,命令序保证晚于既有
//     引用);上游手动 Dispose + Shader 侧 glIsTexture 逐帧驱逐;
//   - GLES 分支(Texture.cs L161-199 的 FBO 回读 + BGRA/RGBA 交换)待 ES 档
//     位批次;当前桌面路径先行(GetData 断言在 GL 集成测试)。
// Method-by-method port of Texture.cs. Semantic plane: BGRA-byte uploads
// (internal RGBA8 on desktop / BGRA on ES), the UNPACK_ROW_LENGTH/SKIP row
// packing of SetSubData, the RGBA16F float path (palettes), and readback via
// desktop glGetTexImage (the GLES FBO-readback branch lands with the ES
// profile batch), with ScaleFilter changes re-running PrepareTexture
// (filter/CLAMP_TO_EDGE/BASE_LEVEL 0/MAX_LEVEL 0). Deviations (registered in
// COVERAGE):
//   - OPT-B1 (D37): upstream's IsPowerOf2 enforcement in
//     SetData/SetFloatData/SetDataFromReadBuffer/SetEmpty is not replicated
//     (this engine builds NPOT directly);
//   - OPT-A6: RAII texture handles (the destructor posts DeleteTextures
//     asynchronously; command ordering keeps it after all prior references)
//     replacing upstream's manual Dispose plus the per-flush glIsTexture
//     eviction in Shader;
//   - the GLES branch (the FBO readback and BGRA/RGBA swap of Texture.cs
//     L161-199) awaits the ES-profile batch; the desktop path comes first.
#pragma once
import std;

#include "gfx/render_thread.hpp"

namespace ora::gfx {

/// 上采样过滤(PlatformInterfaces.cs L156)。
/// The scaling filter (PlatformInterfaces.cs L156).
enum class TextureScaleFilter : std::uint8_t { Nearest, Linear };

/// 2D 纹理封装(值语义句柄;命令发射器,主线程侧)。
/// The 2D-texture wrapper (a value-semantics handle; a command emitter on the
/// main-thread side).
class Texture {
 public:
  /// glGenTextures(同步往返拿名)。
  /// glGenTextures (name acquired via a synchronous round-trip).
  explicit Texture(RenderThread& render);

  ~Texture();

  Texture(const Texture&) = delete;
  Texture& operator=(const Texture&) = delete;

  /// 所有权移交(源对象退化为 id 0,不再删除)。
  /// Ownership transfer (the source degrades to id 0 and no longer deletes).
  Texture(Texture&& other) noexcept;
  Texture& operator=(Texture&&) = delete;

  /// 全量上传(BGRA 字节序;Texture.cs L81-93)。
  /// Full upload (BGRA byte order; Texture.cs L81-93).
  void SetData(std::span<const std::byte> vec_bgra_bytes, std::int32_t int4_width, std::int32_t int4_height);

  /// 分配不初始化(Texture.cs L219-227)。
  /// Allocates without initialization (Texture.cs L219-227).
  void SetEmpty(std::int32_t int4_width, std::int32_t int4_height);

  /// 子区域上传(行距 = 纹理宽;Texture.cs L113-121 的 UNPACK 序列)。
  /// Sub-rectangle upload (row pitch = texture width; the UNPACK sequence of
  /// Texture.cs L113-121).
  void SetSubData(std::span<const std::byte> vec_bgra_bytes, std::int32_t int4_x, std::int32_t int4_y,
                  std::int32_t int4_width, std::int32_t int4_height);

  /// 浮点全量上传,RGBA16F(调色板路径;Texture.cs L123-140)。
  /// Float full upload, RGBA16F (the palette path; Texture.cs L123-140).
  void SetFloatData(std::span<const float> vec_data, std::int32_t int4_width, std::int32_t int4_height);

  /// 从当前读缓冲拷贝建纹(Texture.cs L142-153)。
  /// Builds the texture by copying from the current read buffer
  /// (Texture.cs L142-153).
  void SetDataFromReadBuffer(std::int32_t int4_x, std::int32_t int4_y, std::int32_t int4_width,
                             std::int32_t int4_height);

  /// 读回(BGRA 字节序;桌面 glGetTexImage 同步往返)。
  /// Readback (BGRA byte order; a synchronous desktop glGetTexImage
  /// round-trip).
  [[nodiscard]] std::vector<std::byte> GetData();

  TextureScaleFilter ScaleFilter() const { return kind_filter_; }
  void SetScaleFilter(TextureScaleFilter kind_filter);

  std::int32_t Width() const { return int4_width_; }
  std::int32_t Height() const { return int4_height_; }

  /// GL 纹理名(Shader/渲染器侧绑定用;0 = 已移动出/未创建)。
  /// The GL texture name (for Shader/renderer binding; 0 = moved-from or
  /// uncreated).
  std::uint32_t GlId() const { return uint4_texture_; }

 private:
  /// PrepareTexture(Texture.cs L49-70):bind + 过滤/包裹/层级参数。
  /// PrepareTexture (Texture.cs L49-70): bind + filter/wrap/level parameters.
  void PrepareTexture();

  RenderThread& render_;
  std::uint32_t uint4_texture_ = 0;
  std::int32_t int4_width_ = 0;
  std::int32_t int4_height_ = 0;
  TextureScaleFilter kind_filter_ = TextureScaleFilter::Nearest;
};

}  // namespace ora::gfx
