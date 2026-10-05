// UPSTREAM: OpenRA.Mods.Cnc/SpriteLoaders/ShpTDLoader.cs @7d57605 L17-331(全文)
// Command & Conquer TD/RA 的 SHP 图像:帧头表(偏移低 24 位 + 格式高
// 8 位)+ 三压缩格式(XORPrev/XORLCW/LCW)+ 引用解压链(XOR 帧引用
// 兄弟帧数据)+ TrimmedFrame 收边(非零像素包围盒裁剪,偶数行列对齐,
// 半像素偏移防御)。递归深度防线(Format20/40 infinite loop)与异常
// 消息逐字。
// 形态适配:Stream → SpanReader;Frames 的 IReadOnlyList → vector<
// unique_ptr<ISpriteFrame>>;ShpTDSprite.Write(Utility 面)随 Phase 8;
// headers.ToDictionary 的重复键 ArgumentException → runtime_error 等价
// 抛点(合法文件无重复 —— COVERAGE 登记)。
// The Command & Conquer TD/RA SHP image: a frame-header table (offset in
// the low 24 bits + format in the high 8) with three compression formats
// (XORPrev/XORLCW/LCW), a reference decompression chain (XOR frames borrow
// sibling frame data), and the TrimmedFrame bounds-trimming wrapper
// (non-zero-pixel bounding box, even row/column alignment, half-pixel
// offset guard). The recursion-depth guard ("Format20/40 headers contain
// infinite loop") and exception messages are verbatim. Shape adaptation:
// Stream → SpanReader; the IReadOnlyList Frames → vector<unique_ptr<
// ISpriteFrame>>; ShpTDSprite.Write (the Utility face) lands with Phase 8;
// headers.ToDictionary's duplicate-key ArgumentException → an equivalent
// std::runtime_error throw (valid files carry no duplicates — registered
// in COVERAGE).
#pragma once
import std;

#include "core/int2.hpp"
#include "core/vector_n.hpp"
#include "gfx/sprite_frame.hpp"
#include "formats/span_reader.hpp"

namespace ora::fmt {

/// IsShpTD(L19-47):首 word 帧数、末偏移 == 文件长、首帧格式标志
/// 0x20/0x40/0x80 三重判定。
/// IsShpTD (L19-47): the three-part probe of the leading frame-count
/// word, the final offset == file length, and the first frame's format
/// flag 0x20/0x40/0x80.
bool IsShpTD(std::span<const std::byte> vec_file);

/// ShpTDLoader.TryParseSprite(L49-63)形态:判定 + 解析一体。
/// The ShpTDLoader.TryParseSprite (L49-63) form: probe + parse in one.
bool TryParseShpTD(std::span<const std::byte> vec_file, std::vector<std::unique_ptr<gfx::ISpriteFrame>>& vec_frames);

class ShpTDSprite {
 public:
  /// 逐句照抄 L124-172 的构造(头表/引用链/解压/收边)。
  /// The L124-172 construction copied statement by statement (the header
  /// table / reference chain / decompression / trimming).
  explicit ShpTDSprite(std::span<const std::byte> vec_file);

  const std::vector<std::unique_ptr<gfx::ISpriteFrame>>& Frames() const { return vec_frames_; }
  int2 Size() const { return int2_size_; }

 private:
  struct ImageHeader;
  struct TrimmedFrame;

  friend bool TryParseShpTD(std::span<const std::byte> vec_file,
                            std::vector<std::unique_ptr<gfx::ISpriteFrame>>& vec_frames);

  /// Decompress(L190-218):格式分派 + XOR 引用递归(recurseDepth 防线)。
  /// Decompress (L190-218): format dispatch + the XOR reference
  /// recursion (the recurseDepth guard).
  void Decompress(ImageHeader& header_h);

  /// CopyImageData(L220-225;返回可变 vector,XOR 解码需要写目标)。
  /// CopyImageData (L220-225; returns a mutable vector — the XOR decode
  /// writes its destination).
  std::vector<std::byte> CopyImageData(const std::vector<std::byte>& vec_base_image) const;

  int int4_recurse_depth_ = 0;
  std::int32_t int4_image_count_ = 0;
  std::int64_t int8_shp_bytes_file_offset_ = 0;
  std::span<const std::byte> vec_shp_bytes_;
  int2 int2_size_{};
  std::vector<std::unique_ptr<gfx::ISpriteFrame>> vec_frames_;
};

}  // namespace ora::fmt
