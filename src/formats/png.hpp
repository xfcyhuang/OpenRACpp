// UPSTREAM: OpenRA.Game/FileFormats/Png.cs @b6fc03f L29-592(全文逐语义)
// 自研 PNG 编解码器(OPT-B3:上游自研 592 行,PORTING_PLAN 决定照抄保行为
// 一致,含 Indexed8 读写):块循环(IHDR/PLTE/tRNS/IDAT/tEXt/IEND/未知块
// 跳过)、PngIdatStream 的 IDAT 链缝合、zlib 解压 + 五滤波反解(None/Sub/
// Up/Average/Paeth)+ 位深 1/2/4 解包、tEXt 键值收集、Save 的块序(IHDR→
// PLTE→tRNS(任一 alpha>0)→IDAT(行滤波 0)→tEXt→IEND)与 CRC32 链式写。
// 形态适配(COVERAGE 登记):
//   - zlib 层 .NET ZLibStream → miniz(mz_uncompress/mz_compress):解压
//     输出逐字节等价;坏压缩流的异常消息/时机不逐字(D63);Save 的 deflate
//     字节依实现(D64)——黄金以"结构 CRC + IDAT 解压 CRC"替代字节对拍;
//   - IDAT 链缝合流式 → 整段物化后单次解压(输出等价,D63);
//   - Color[](GDI)→ PngColor(RGBA 字节对);Dictionary<string,string> →
//     插入序 vector(键更新保位,同 Dictionary 语义);
//   - Encoding.ASCII 的 >0x7F → '?' 替换语义保留。
// The in-house PNG codec (OPT-B3: upstream's own 592 lines, kept verbatim in
// behavior per PORTING_PLAN, Indexed8 read/write included): the chunk loop
// (IHDR/PLTE/tRNS/IDAT/tEXt/IEND + unknown-chunk skipping), PngIdatStream's
// IDAT-chain stitching, zlib decompression + the five unfilters (None/Sub/
// Up/Average/Paeth) + the 1/2/4-bit unpacking, tEXt collection, and Save's
// chunk order (IHDR→PLTE→tRNS when any alpha>0→IDAT with filter-0 rows→
// tEXt→IEND) with chained CRC32 writes. Shape adaptations (registered in
// COVERAGE): .NET ZLibStream → miniz (decode output byte-equal; corrupt-
// stream exception texts/timing differ, D63; Save's deflate bytes are
// implementation-defined, D64 — the golden uses a structural CRC + an
// IDAT-decompressed CRC instead of byte comparison); the streamed IDAT
// stitching → materialize-then-inflate once (output-equivalent, D63);
// Color[] (GDI) → PngColor (RGBA bytes); Dictionary → an insertion-ordered
// vector (key updates keep position, same as Dictionary); the ASCII
// >0x7F→'?' replacement semantics kept.
#pragma once
import std;

#include "gfx/sprite.hpp"

namespace ora::fmt {

/// 上游 System.Drawing.Color 的 RGBA 字节对替身(仅 Png 家族内部使用;
/// At 语义 = (A<<24)|(R<<16)|(G<<8)|B 的 uint32 形态在消费方换算)。
/// The RGBA byte quadruple standing in for System.Drawing.Color (internal
/// to the Png family; consumers convert to the uint32 (A<<24)|(R<<16)|
/// (G<<8)|B form as needed).
struct PngColor {
  std::uint8_t uint1_r{0};
  std::uint8_t uint1_g{0};
  std::uint8_t uint1_b{0};
  std::uint8_t uint1_a{255};

  bool operator==(const PngColor&) const = default;

  /// Color.FromArgb(r,g,b)(alpha=255)。| Color.FromArgb(r,g,b) (alpha 255).
  static constexpr PngColor FromRgb(std::uint8_t uint1_r, std::uint8_t uint1_g, std::uint8_t uint1_b) {
    return PngColor{uint1_r, uint1_g, uint1_b, 255};
  }

  /// Color.FromArgb(alpha, color)(保 RGB 换 A)。| Color.FromArgb(alpha,
  /// color) (keep RGB, replace A).
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

  /// Png(Stream)(L115-286):块循环解析。文件字节 → Png。
  /// Png(Stream) (L115-286): the chunk-loop parse. File bytes → Png.
  explicit Png(std::span<const std::byte> vec_file);

  /// Png(data, type, width, height, palette, embeddedData)(L389-455):
  /// 原始像素构造(Bgra32/Bgr24 → 大端字节交换拷贝)。
  /// Png(data, type, width, height, palette, embeddedData) (L389-455):
  /// the raw-pixel constructor (Bgra32/Bgr24 → the big-endian swap copy).
  Png(std::vector<std::byte> vec_data, gfx::SpriteFrameType type_frame, int int4_width, int int4_height,
      std::optional<std::vector<PngColor>> vec_palette = std::nullopt,
      std::vector<std::pair<std::string, std::string>> vec_embedded_data = {});

  /// Verify(s)(L457-464):8 字节签名探测。
  /// Verify(s) (L457-464): the 8-byte signature probe.
  static bool Verify(std::span<const std::byte> vec_file);

  /// Save(L506-585):块序写回 + CRC32(压缩档 CompressionLevel.SmallestSize
  /// → miniz MZ_BEST_COMPRESSION,字节依实现 D64)。
  /// Save (L506-585): the ordered chunk write-back with CRC32 (the
  /// SmallestSize level → miniz MZ_BEST_COMPRESSION; bytes are
  /// implementation-defined, D64).
  std::vector<std::byte> Save() const;

  int Width() const { return int4_width_; }
  int Height() const { return int4_height_; }
  gfx::SpriteFrameType Type() const { return type_frame_; }

  /// PixelStride(L113):Indexed8=1 / Rgb24=3 / 其余 4。
  /// PixelStride (L113): Indexed8=1 / Rgb24=3 / else 4.
  int PixelStride() const {
    return type_frame_ == gfx::SpriteFrameType::Indexed8 ? 1 : type_frame_ == gfx::SpriteFrameType::Rgb24 ? 3 : 4;
  }

  /// 上游 Palette 属性:null → nullopt。
  /// The upstream Palette property: null → nullopt.
  const std::optional<std::vector<PngColor>>& Palette() const { return vec_palette_; }
  const std::vector<std::byte>& Data() const { return vec_data_; }

  /// 上游 EmbeddedData = [](字段,非属性;默认空字典)。插入序遍历 = C#
  /// Dictionary 事实序;键已存在时更新值保位(同索引器语义)。
  /// The upstream EmbeddedData = [] (a field, not a property; an empty
  /// dictionary by default). Insertion-order iteration = C# Dictionary's
  /// de-facto order; updating an existing key keeps its position (same as
  /// the indexer).
  const std::vector<std::pair<std::string, std::string>>& EmbeddedData() const { return vec_embedded_data_; }

 private:
  /// ProcessIDAT(L288-387):IDAT 缝合 + 解压 + 反滤波 + 位深解包。
  /// ProcessIDAT (L288-387): IDAT stitching + inflate + unfilter + the
  /// bit-depth unpack.
  void ProcessIDAT(class SpanReader& reader_source, std::int32_t int4_first_chunk_length, std::uint8_t uint1_bit_depth);

  int int4_width_ = 0;
  int int4_height_ = 0;
  std::optional<std::vector<PngColor>> vec_palette_;
  std::vector<std::byte> vec_data_;
  gfx::SpriteFrameType type_frame_ = gfx::SpriteFrameType::Rgba32;
  std::vector<std::pair<std::string, std::string>> vec_embedded_data_;
};

}  // namespace ora::fmt
