// UPSTREAM: OpenRA.Game/Graphics/PlatformInterfaces.cs @b6fc03f L192-203(IFont/
// FontGlyph)+ OpenRA.Platforms.Default/FreeTypeFont.cs @b6fc03f L20-143(全文)
// OPT-B2(docs/OPTIMIZATION_TRACKER.md):上游 FreeTypeFont.cs L27-36 的手算
// 结构体偏移 HACK(FaceRecGlyphOffset 152/GlyphSlotMetricsOffset 48/…)全部
// 删除 —— 直接 #include <ft2build.h> 用真结构体(FT_FaceRec/FT_GlyphSlotRec/
// FT_Bitmap)取 face->glyph->metrics.width 等字段;FreeType 2.14.1 官方头
// (third_party/FreeType/include)与运行时 freetype6.dll 同版本。
// 五个入口(FT_Init_FreeType/FT_New_Memory_Face/FT_Done_Face/
// FT_Set_Pixel_Sizes/FT_Load_Char)经 LoadLibraryA("freetype6.dll") 运行时
// 直连(al_loader 同形态);字形位图行复制循环逐语句照抄(尺寸取
// metrics 而非 bitmap 的上游怪癖保留 —— pitch < width 时两者可差)。
// 形态映射(D86,docs/COVERAGE.md):字节数组 GCHandle pin → 自持 vector
// (FT_New_Memory_Face 生命期语义等价);char(charCode 为 UTF-16 码单元,
// ≤ 0xFFFF)→ std::uint32_t 码点参数(FT_Load_Char 的原生参数域)。
// OPT-B2 (docs/OPTIMIZATION_TRACKER.md): the hand-computed struct-offset HACK
// of FreeTypeFont.cs L27-36 (FaceRecGlyphOffset 152/GlyphSlotMetricsOffset
// 48/…) is deleted — real structs (FT_FaceRec/FT_GlyphSlotRec/FT_Bitmap) come
// straight from #include <ft2build.h>, reading face->glyph->metrics.width and
// friends; the official FreeType 2.14.1 headers
// (third_party/FreeType/include) match the freetype6.dll runtime version.
// The five entry points (FT_Init_FreeType/FT_New_Memory_Face/FT_Done_Face/
// FT_Set_Pixel_Sizes/FT_Load_Char) connect at runtime via
// LoadLibraryA("freetype6.dll") (the al_loader shape); the glyph bitmap row
// copy loop is ported statement by statement (the upstream quirk of sizing by
// metrics rather than the bitmap kept — the two can differ when pitch <
// width). Shape mapping (D86, docs/COVERAGE.md): the pinned byte array
// becomes an owned vector (the FT_New_Memory_Face lifetime semantics are
// equivalent); char (a UTF-16 code unit, ≤ 0xFFFF) becomes a std::uint32_t
// code-point parameter (FT_Load_Char's native domain).
#pragma once
import std;

#include "core/int2.hpp"

namespace ora::platform {

/// 字形(FontGlyph,PlatformInterfaces.cs L197-203;Advance 上游为 float ——
/// 赋值源是 26.6 右移后的整数)。
/// A glyph (FontGlyph, PlatformInterfaces.cs L197-203; Advance is a float
/// upstream — its assignment source is the post-26.6-shift integer).
struct FontGlyph {
  int2 offset{};                // bitmap_left / -bitmap_top
  std::int32_t int4_width = 0;  // metrics.width >> 6
  std::int32_t int4_height = 0; // metrics.height >> 6
  float fp4_advance = 0.0f;     // metrics.horiAdvance >> 6
  std::vector<std::uint8_t> vec_data;  // 行主序位图(每行 width 字节)| row-major bitmap (width bytes per row)

  /// 上游 EmptyGlyph 的等价判定(Data == null / 全零)。
  /// The equivalent of upstream's EmptyGlyph test (Data == null / all-zero).
  [[nodiscard]] bool Empty() const {
    return int4_width == 0 && int4_height == 0 && fp4_advance == 0.0f && vec_data.empty();
  }
};

/// FreeType 字体(FreeTypeFont.cs 的 IFont 面)。
/// A FreeType font (the IFont face of FreeTypeFont.cs).
class FreeTypeFont {
 public:
  /// 数据拷入自持缓冲(D86);FT_New_Memory_Face 失败抛
  /// "Failed to initialize font"(InvalidDataException 消息逐字)。
  /// The data is copied into an owned buffer (D86); an FT_New_Memory_Face
  /// failure throws "Failed to initialize font" (the InvalidDataException
  /// message verbatim).
  explicit FreeTypeFont(std::span<const std::uint8_t> span_data);

  FreeTypeFont(const FreeTypeFont&) = delete;
  FreeTypeFont& operator=(const FreeTypeFont&) = delete;
  FreeTypeFont(FreeTypeFont&&) noexcept;
  FreeTypeFont& operator=(FreeTypeFont&&) = delete;
  ~FreeTypeFont();

  /// CreateGlyph(char c, int size, float deviceScale):Set_Pixel_Sizes 或
  /// Load_Char 失败返回空字形;成功则 26.6 右移 + 行复制。非 const:上游
  /// 同名方法对 face 的内部槽位有写效应。
  /// CreateGlyph(char c, int size, float deviceScale): a Set_Pixel_Sizes or
  /// Load_Char failure returns the empty glyph; success applies the 26.6
  /// shift and the row copy. Not const: the upstream method likewise writes
  /// the face's internal glyph slot.
  [[nodiscard]] FontGlyph CreateGlyph(std::uint32_t uint4_char_code, int int4_size,
                                      float fp4_device_scale);

 private:
  std::vector<std::uint8_t> vec_data_;  // 自持字体字节(D86)| owned font bytes (D86)
  void* ptr_face_ = nullptr;            // FT_Face(不透明指针经真结构体头解引用)| FT_Face (deref'd through the real-struct headers)
};

/// freetype6.dll 加载结果(ok = 五入口齐备)。
/// The freetype6.dll load result (ok = all five entries present).
struct FreetypeLoadResult {
  bool b_ok;
  const char* str_missing;
};

/// 经通用加载函数取五入口(与 al_loader 同款;测试可注入缺失符号)。
/// Fetches the five entries through the generic loader (the al_loader
/// arrangement; tests may inject missing symbols).
FreetypeLoadResult LoadFreetype(void* (*fn_load)(const char* name));

/// 便捷入口:LoadLibraryA("freetype6.dll")(可执行文件旁随包副本优先)。
/// Convenience: LoadLibraryA("freetype6.dll") (the bundled copy next to the
/// executable wins).
FreetypeLoadResult LoadFreetypeDefault();

/// 五入口是否已就位(创建字体前查询)。
/// Whether the five entries are in place (query before creating fonts).
[[nodiscard]] bool FreetypeReady();

}  // namespace ora::platform
